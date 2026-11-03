#include "body.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "GameController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "game.h"
#include "held.h"
#include "Transform.h"
#include "journal.h"
#include "log.h"
#include "outlist.h"
#include "reward.h"
#include "runtime.h"

namespace pm::body {
namespace {

using phasmo::Quaternion;
using phasmo::Vector3;

using phasmo::LocalPlayer;
using phasmo::Network;
using phasmo::PlayerSpot;

constexpr ULONGLONG kSweepMs = 500;  // anything the body switches on again is hidden within this

// The ghost player's own client: their investigator rig, switched off while they play the ghost.
struct SteppedOut {
  bool active = false;
  api::Held player;        // their networked Player (Network's and the rig's reference cleared)
  api::Held rig;           // LocalPCPlayer, switched off for the contract
  api::Held camera;        // its camera, detached to be the ghost's eyes
  api::Held cameraParent;  // where the camera goes back to
  Vector3 cameraLocalPosition{};
  Quaternion cameraLocalRotation{0, 0, 0, 1};
} g;

// Every client: the ghost player's body, parked where it stood and kept out of play.
struct OutOfPlay {
  api::Held player;
  std::vector<api::Held> renderers;  // switched off here
  std::vector<api::Held> colliders;
  std::vector<api::Held> lights;  // a torch left switched on in the body's hand, say
  std::vector<api::Held> sounds;  // anything held that keeps sounding
  ULONGLONG nextSweep = 0;
} g_parked;

bool g_inLevel = false;
// The ghost player stepped out once this contract. Giving the body back (leaving, quitting) is for
// good: the level outlives a leave until the next scene loads, and must not step them out again.
bool g_steppedOutThisContract = false;
bool g_stepOutFailLogged = false;

// Live instances only: a static instance can outlive its scene (destroyed but still referenced).
void* live(void* instance) { return game::alive(instance) ? instance : nullptr; }

// The room's player spots, read as a list; empty without a Network.
api::List spots() {
  void* net = game::network();
  return api::List{net ? Network::playerSpots.get(net) : nullptr};
}

// The networked Player in an actor's player slot, or null.
void* player_of(int actor) {
  const api::List list = spots();
  for (size_t i = 0, n = list.size(); actor > 0 && i < n; ++i) {
    void* spot = list[i];
    if (!spot || game::actor_number(PlayerSpot::photonPlayer.get(spot)) != actor) continue;
    return live(PlayerSpot::player.get(spot));
  }
  return nullptr;
}

template <typename F>
void each_object(void* transform, const F& fn) {
  if (!transform) return;
  fn(game::game_object_of(transform));
  for (int i = 0; i < game::child_count(transform); ++i) each_object(game::child(transform, i), fn);
}

template <typename IsOn, typename SetOn>
void switch_off(void* gameObject, void* type, std::vector<api::Held>& off, IsOn isOn, SetOn setOn) {
  const api::RefArray found{gameObject && type ? phasmo::GameObject::GetComponents(gameObject, type) : nullptr};
  for (size_t i = 0, n = found.size(); i < n; ++i) {
    void* c = found[i];
    if (!c || !game::alive(c) || !isOn(c)) continue;
    setOn(c, false);
    if (std::find(off.begin(), off.end(), c) == off.end()) off.push_back(c);
  }
}

// Nobody sees or touches the parked body. On the ghost player's own client their journal, which
// stays on the body, is left to src/journal.cpp.
void hide(void* player, void* keep) {
  // The type objects once per sweep, not once per object.
  void* renderer = phasmo::Renderer::type.type_object();
  void* collider = phasmo::Collider::type.type_object();
  void* light = phasmo::Light::type.type_object();
  void* audio = phasmo::AudioSource::type.type_object();
  each_object(game::transform_of(player), [&](void* go) {
    if (keep && game::is_child_of(game::object_transform(go), keep)) return;
    switch_off(go, renderer, g_parked.renderers, game::renderer_enabled, game::set_renderer_enabled);
    switch_off(go, collider, g_parked.colliders, game::collider_enabled, game::set_collider_enabled);
    switch_off(go, light, g_parked.lights, game::enabled, game::set_enabled);
    switch_off(go, audio, g_parked.sounds, game::enabled, game::set_enabled);
  });
}

// The parked body is an investigator's again: seen, solid and back in the player list.
void release_parked() {
  outlist::put_back();
  if (!g_parked.player) return;
  for (void* r : g_parked.renderers)
    if (game::alive(r)) game::set_renderer_enabled(r, true);
  for (void* c : g_parked.colliders)
    if (game::alive(c)) game::set_collider_enabled(c, true);
  for (void* l : g_parked.lights)
    if (game::alive(l)) game::set_enabled(l, true);
  for (void* a : g_parked.sounds)
    if (game::alive(a)) game::set_enabled(a, true);
  if (game::alive(g_parked.player)) PM_LOG("body: the ghost player's body is back in play");
  g_parked = OutOfPlay{};
}

// Keeps the ghost player's networked body out of play: its spot out of the player list once this
// client's own load wait is over (GameController's, which waits for a live Player in every spot), and
// the body unseen and not solid. The ghost player's own client does this once they have stepped out.
void park(void* player, int actor, bool own) {
  if (player != g_parked.player) {
    release_parked();
    g_parked.player = player;
    PM_LOG("body: the ghost player's body is parked out of play%s", own ? " (their own client)" : "");
  }
  void* gc = live(phasmo::GameController::instance.get());
  if (gc && phasmo::GameController::allPlayersLoaded.get(gc)) outlist::take_out(actor);
  const ULONGLONG now = GetTickCount64();
  if (now < g_parked.nextSweep) return;
  g_parked.nextSweep = now + kSweepMs;
  hide(player, own ? journal::root() : nullptr);
}

// The ghost player's own client: the camera becomes the ghost's eyes and the investigator rig
// (controls, hands, HUD) is switched off. The networked body stays where it stood.
void step_out() {
  void* net = game::network();
  void* player = net ? Network::localPlayer.get(net) : nullptr;
  void* rig = net ? Network::localRig.get(net) : nullptr;
  void* camera = rig && game::alive(rig) ? LocalPlayer::camera.get(rig) : nullptr;
  if (!game::alive(player) || !game::alive(camera)) {
    // Nobody parks the body until this works: the player stays an ordinary investigator meanwhile.
    if (!g_stepOutFailLogged) PM_LOG("body: cannot step out yet: no %s", game::alive(player) ? "rig camera" : "local Player");
    g_stepOutFailLogged = true;
    return;
  }
  try {
    journal::keep(player);  // the ghost player's own journal, still on the body
  } catch (...) {
    PM_LOG("body: the journal could not be kept; the ghost player has none");
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
  Network::localPlayer.set(net, nullptr);
  LocalPlayer::player.set(rig, nullptr);
  g.active = true;
  g_steppedOutThisContract = true;
  PM_LOG("body: the ghost player stepped out of their investigator");
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
      if (void* net = game::network()) Network::localPlayer.set(net, g.player);
      LocalPlayer::player.set(g.rig, g.player);
    }
    PM_LOG("body: the ghost player is an investigator again");
  }
  g = SteppedOut{};
}

void give_back() {
  step_in();
  release_parked();
}

}  // namespace

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
    void* gc = live(phasmo::GameController::instance.get());
    if (gc && phasmo::GameController::allPlayersLoaded.get(gc)) step_out();
  }
  // Every client parks the ghost player's body once the ghost player has stepped out of it: their own
  // client knows, the others hear it from them (session HELLO). Until then it is their investigator.
  // Having stepped out, they stay out for the rest of the contract as far as the others know: a
  // leave gives their own client its rig back for the menu, but the body must not rejoin the
  // contract on everyone else's screen before Photon takes the player out of the room.
  s.set_stepped_out(s.local_is_ghost() && g_steppedOutThisContract);
  const bool parked = s.local_is_ghost() ? g.active : s.ghost_stepped_out();
  void* body = nullptr;
  if (ghostActor > 0 && parked)
    body = outlist::actor() == ghostActor ? live(outlist::player()) : player_of(ghostActor);  // out: held
  if (body) park(body, ghostActor, s.local_is_ghost());
  else release_parked();
}

void teardown() { give_back(); }

bool stepped_out() { return g.active; }

// Network's local player is what the game's own Leave, Quit and ExitLevel.Exit read for this client's
// investigator.
bool investigator_back() {
  void* net = game::network();
  return !g.active && net && Network::localPlayer.get(net);
}

void* camera() { return g.active && game::alive(g.camera) ? g.camera.get() : nullptr; }

}  // namespace pm::body
