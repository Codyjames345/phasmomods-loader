#include "body.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "journal.h"
#include "log.h"
#include "outcome.h"
#include "reward.h"

namespace pg::body {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;

// GameAssembly build 25811160 (verified at load).
constexpr uintptr_t kExitRva = 0x1205880;    // ExitLevel.Exit(PhotonMessageInfo), RPC
constexpr uintptr_t kReviveRva = 0xD2EA50;   // Player.RevivePlayer(bool)

constexpr size_t kNetworkPlayer = 0x28;      // Network: this client's networked Player
constexpr size_t kNetworkRig = 0x30;         // Network: this client's LocalPlayer (LocalPCPlayer)
constexpr size_t kNetworkSpots = 0x38;       // Network: List<PlayerSpot>
constexpr size_t kRigPlayer = 0x20;          // LocalPlayer: its networked Player
constexpr size_t kRigCamera = 0x30;          // LocalPlayer: its Camera
constexpr size_t kSpotPhotonPlayer = 0x18;   // PlayerSpot: Photon.Realtime.Player
constexpr size_t kSpotPlayer = 0x38;         // PlayerSpot: networked Player
constexpr size_t kPlayerDead = 0x28;
constexpr size_t kGameAllLoaded = 0x100;     // GameController: every player has loaded
constexpr size_t kLevelValuesExited = 0x20;  // LevelValues: set by the native exit
constexpr ULONGLONG kSweepMs = 500;          // anything the body switches on again is hidden within this

struct Api {
  il2cpp::Class* gameController = nullptr;
  il2cpp::Class* levelValues = nullptr;
  il2cpp::Object* rendererType = nullptr;
  il2cpp::Object* colliderType = nullptr;
  il2cpp::Object* lightType = nullptr;
  il2cpp::Object* audioType = nullptr;
  void (*loadBackToMenu)(void*, const void*) = nullptr;
} api;

using Exit_t = void (*)(void*, void*, const void*);
using Revive_t = void (*)(void*, bool, const void*);
Exit_t g_exit = nullptr;
Revive_t g_revive = nullptr;

// The ghost player's own client: their investigator rig, switched off while they play the ghost.
struct SteppedOut {
  bool active = false;
  il2cpp::Held player;        // their networked Player (Network's and the rig's reference cleared)
  il2cpp::Held rig;           // LocalPCPlayer, switched off for the contract
  il2cpp::Held camera;        // its camera, detached to be the ghost's eyes
  il2cpp::Held cameraParent;  // where the camera goes back to
  Vector3 cameraLocalPosition{};
  Quaternion cameraLocalRotation{0, 0, 0, 1};
} g;

// Every client: the ghost player's body, parked where it stood and kept out of play.
struct OutOfPlay {
  il2cpp::Held player;
  bool markedDead = false;              // the dead flag was set here (cleared again on release)
  std::vector<il2cpp::Held> renderers;  // switched off here
  std::vector<il2cpp::Held> colliders;
  std::vector<il2cpp::Held> lights;  // a torch left switched on in the body's hand, say
  std::vector<il2cpp::Held> sounds;  // anything held that keeps sounding
  ULONGLONG nextSweep = 0;
} g_parked;

bool g_inLevel = false;
// The ghost player stepped out once this contract. Giving the body back (leaving, quitting) is for
// good: the level outlives a leave until the next scene loads, and must not step them out again.
bool g_steppedOutThisContract = false;
bool g_stepOutFailLogged = false;

// Live instances only: a static instance can outlive its scene (destroyed but still referenced).
void* static_instance(il2cpp::Class* klass, const char* typeName) {
  void* instance = klass ? il2cpp::static_field_by_type(klass, typeName) : nullptr;
  return game::alive(instance) ? instance : nullptr;
}

il2cpp::List* spots() {
  void* net = game::network();
  return net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
}

// The networked Player in an actor's player slot, or null.
void* player_of(int actor) {
  il2cpp::List* list = spots();
  for (int i = 0; actor > 0 && list && list->items && i < list->size; ++i) {
    void* spot = list->items->data<void*>()[i];
    if (!spot || game::actor_number(il2cpp::field<void*>(spot, kSpotPhotonPlayer)) != actor) continue;
    void* player = il2cpp::field<void*>(spot, kSpotPlayer);
    return game::alive(player) ? player : nullptr;
  }
  return nullptr;
}

// Every investigator still in the contract is dead. The ghost player's parked body is not one.
bool every_investigator_dead() {
  il2cpp::List* list = spots();
  int investigators = 0;
  for (int i = 0; list && list->items && i < list->size; ++i) {
    void* spot = list->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    if (!game::alive(player) || player == g_parked.player) continue;
    ++investigators;
    if (!il2cpp::field<bool>(player, kPlayerDead)) return false;
  }
  return investigators > 0;
}

template <typename F>
void each_object(void* transform, const F& fn) {
  if (!transform) return;
  fn(game::game_object_of(transform));
  for (int i = 0; i < game::child_count(transform); ++i) each_object(game::child(transform, i), fn);
}

template <typename IsOn, typename SetOn>
void switch_off(void* gameObject, il2cpp::Object* type, std::vector<il2cpp::Held>& off, IsOn isOn, SetOn setOn) {
  il2cpp::Array* found = game::components(gameObject, type);
  for (uintptr_t i = 0; found && i < found->max_length; ++i) {
    void* c = found->data<void*>()[i];
    if (!c || !game::alive(c) || !isOn(c)) continue;
    setOn(c, false);
    if (std::find(off.begin(), off.end(), c) == off.end()) off.push_back(c);
  }
}

// Nobody sees or touches the parked body. On the ghost player's own client their journal, which
// stays on the body, is left to src/journal.cpp.
void hide(void* player, void* keep) {
  each_object(game::transform_of(player), [&](void* go) {
    if (keep && game::is_child_of(game::object_transform(go), keep)) return;
    switch_off(go, api.rendererType, g_parked.renderers, game::renderer_enabled, game::set_renderer_enabled);
    switch_off(go, api.colliderType, g_parked.colliders, game::collider_enabled, game::set_collider_enabled);
    switch_off(go, api.lightType, g_parked.lights, game::enabled, game::set_enabled);
    switch_off(go, api.audioType, g_parked.sounds, game::enabled, game::set_enabled);
  });
}

// The parked body is an investigator's again: seen, solid and alive.
void release_parked() {
  if (!g_parked.player) return;
  for (void* r : g_parked.renderers)
    if (game::alive(r)) game::set_renderer_enabled(r, true);
  for (void* c : g_parked.colliders)
    if (game::alive(c)) game::set_collider_enabled(c, true);
  for (void* l : g_parked.lights)
    if (game::alive(l)) game::set_enabled(l, true);
  for (void* a : g_parked.sounds)
    if (game::alive(a)) game::set_enabled(a, true);
  if (g_parked.markedDead && game::alive(g_parked.player)) il2cpp::set_field<bool>(g_parked.player, kPlayerDead, false);
  if (game::alive(g_parked.player)) PG_LOG("body: the ghost player's body is back in play");
  g_parked = OutOfPlay{};
}

// Keeps the ghost player's networked body in every player slot, so each client's load wait
// (GameController's, which waits for a live Player in every slot) and everything else that expects
// one sees it, but out of play: dead to the game's rules, unseen and not solid. The ghost player's
// own client does this once they have stepped out of the body.
void park(void* player, bool own) {
  if (player != g_parked.player) {
    release_parked();
    g_parked.player = player;
    PG_LOG("body: the ghost player's body is parked out of play%s", own ? " (their own client)" : "");
  }
  if (!il2cpp::field<bool>(player, kPlayerDead)) {
    il2cpp::set_field<bool>(player, kPlayerDead, true);
    g_parked.markedDead = true;
  }
  const ULONGLONG now = GetTickCount64();
  if (now < g_parked.nextSweep) return;
  g_parked.nextSweep = now + kSweepMs;
  hide(player, own ? journal::root() : nullptr);
}

// The ghost player's own client: the camera becomes the ghost's eyes and the investigator rig
// (controls, hands, HUD) is switched off. The networked body stays where it stood.
void step_out() {
  void* net = game::network();
  void* player = net ? il2cpp::field<void*>(net, kNetworkPlayer) : nullptr;
  void* rig = net ? il2cpp::field<void*>(net, kNetworkRig) : nullptr;
  void* camera = rig && game::alive(rig) ? il2cpp::field<void*>(rig, kRigCamera) : nullptr;
  if (!game::alive(player) || !game::alive(camera)) {
    // Nobody parks the body until this works: the player stays an ordinary investigator meanwhile.
    if (!g_stepOutFailLogged) PG_LOG("body: cannot step out yet: no %s", game::alive(player) ? "rig camera" : "local Player");
    g_stepOutFailLogged = true;
    return;
  }
  try {
    journal::keep(player);  // the ghost player's own journal, still on the body
  } catch (...) {
    PG_LOG("body: the journal could not be kept; the ghost player has none");
    journal::release();
  }
  void* cameraTransform = game::transform_of(camera);
  g.player = player;
  g.rig = rig;
  g.camera = camera;
  g.cameraParent = game::parent(cameraTransform);
  g.cameraLocalPosition = game::local_position(cameraTransform);
  g.cameraLocalRotation = game::local_rotation(cameraTransform);
  game::set_parent(cameraTransform, nullptr, true);
  game::set_active(game::game_object_of(rig), false);
  // This client plays no investigator: the game's local-player code finds none, as it did when the
  // body was first left behind.
  il2cpp::set_ref_field(net, kNetworkPlayer, nullptr);
  il2cpp::set_ref_field(rig, kRigPlayer, nullptr);
  g.active = true;
  g_steppedOutThisContract = true;
  PG_LOG("body: the ghost player stepped out of their investigator");
}

// The ghost player's own client: their rig, camera and journal are an investigator's again.
void step_in() {
  if (!g.active) return;
  journal::release();
  if (game::alive(g.camera) && game::alive(g.cameraParent)) {
    void* t = game::transform_of(g.camera);
    game::set_parent(t, g.cameraParent, false);
    game::set_local_position(t, g.cameraLocalPosition);
    game::set_local_rotation(t, g.cameraLocalRotation);
  }
  if (game::alive(g.rig)) {
    game::set_active(game::game_object_of(g.rig), true);
    if (game::alive(g.player)) {
      if (void* net = game::network()) il2cpp::set_ref_field(net, kNetworkPlayer, g.player);
      il2cpp::set_ref_field(g.rig, kRigPlayer, g.player);
    }
    PG_LOG("body: the ghost player is an investigator again");
  }
  g = SteppedOut{};
}

void give_back() {
  step_in();
  release_parked();
}

// ExitLevel.Exit runs on every client when the contract ends (the truck leaves, or everyone has
// died). Its native body needs the local investigator, so the ghost player's client settles the
// ghost's payout and goes back to the lobby the game's own way.
void detour_exit(void* exitLevel, void* info, const void* method) {
  hooks::Guard guard;
  if (!g.active) {
    g_exit(exitLevel, info, method);
    return;
  }
  reward::contract_ended(every_investigator_dead());
  outcome::contract_over();
  if (void* values = static_instance(api.levelValues, "LevelValues")) il2cpp::set_field<bool>(values, kLevelValuesExited, true);
  if (void* gc = static_instance(api.gameController, "GameController")) api.loadBackToMenu(gc, nullptr);
}

// The parked body only looks dead to the game's rules: a revive (the Monkey Paw's wish walks every
// dead player) would put the ghost player's investigator back in the contract.
void detour_revive(void* player, bool flag, const void* method) {
  hooks::Guard guard;
  if (player && player == g_parked.player) {
    PG_LOG("body: a revive skipped the ghost player's parked body");
    return;
  }
  g_revive(player, flag, method);
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  api.gameController = il2cpp::cls(kGame, "", "GameController");
  api.levelValues = il2cpp::cls(kGame, "", "LevelValues");
  api.rendererType = il2cpp::type_object(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer"));
  api.colliderType = il2cpp::type_object(il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Collider"));
  api.lightType = il2cpp::type_object(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Light"));
  api.audioType = il2cpp::type_object(il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource"));
  api.loadBackToMenu = reinterpret_cast<decltype(api.loadBackToMenu)>(
      il2cpp::method_ptr(api.gameController, "LoadBackToMenu", 0));
  const bool ok = api.gameController && api.levelValues && api.rendererType && api.colliderType && api.lightType && api.audioType &&
                  api.loadBackToMenu;
  PG_LOG("body: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kExitRva), reinterpret_cast<void*>(&detour_exit),
                       reinterpret_cast<void**>(&g_exit), "ExitLevel.Exit") &&
         hooks::create(game::game_assembly_address(kReviveRva), reinterpret_cast<void*>(&detour_revive),
                       reinterpret_cast<void**>(&g_revive), "Player.RevivePlayer");
}

void tick(session::Session& s) {
  if (!game::level_controller()) {
    g_inLevel = false;
    g_steppedOutThisContract = g_stepOutFailLogged = false;
    give_back();  // back in the lobby: the body went with the contract
    s.set_stepped_out(false);
    return;
  }
  if (!g_inLevel) {
    g_inLevel = true;
    reward::contract_started();
  }
  const int ghostActor = game::private_room() ? s.ghost_actor() : 0;
  if (s.local_is_ghost() && ghostActor > 0 && !g.active && !g_steppedOutThisContract) {
    void* gc = static_instance(api.gameController, "GameController");
    if (gc && il2cpp::field<bool>(gc, kGameAllLoaded)) step_out();
  }
  // Every client parks the ghost player's body once the ghost player has stepped out of it: their own
  // client knows, the others hear it from them (session HELLO). Until then it is their investigator.
  // Having stepped out, they stay out for the rest of the contract as far as the others know: a
  // leave gives their own client its rig back for the menu, but the body must not rejoin the
  // contract on everyone else's screen before Photon takes the player out of the room.
  s.set_stepped_out(s.local_is_ghost() && g_steppedOutThisContract);
  const bool parked = s.local_is_ghost() ? g.active : s.ghost_stepped_out();
  void* body = ghostActor > 0 && parked ? player_of(ghostActor) : nullptr;
  if (body) park(body, s.local_is_ghost());
  else release_parked();
}

void teardown() { give_back(); }

void* camera() { return g.active && game::alive(g.camera) ? g.camera.get() : nullptr; }

}  // namespace pg::body
