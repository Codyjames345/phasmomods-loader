#include "kikimora.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <random>
#include <vector>

#include "game.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::custom::kikimora {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kInteractView = 0x20;           // PhotonObjectInteract: PhotonView
constexpr size_t kInteractRigidbody = 0x38;      // PhotonObjectInteract: Rigidbody
constexpr size_t kInteractProp = 0x108;          // PhotonObjectInteract: Prop (Equipment is a Prop)
constexpr size_t kViewOwnershipTransfer = 0x50;  // PhotonView.OwnershipTransfer
constexpr int kOwnershipTakeover = 1;            // OwnershipOption.Takeover: anyone may take it over
constexpr size_t kLevelRooms = 0x50;             // LevelController: the level's rooms
constexpr size_t kActivityInteraction = 0x28;    // GhostActivity: GhostInteraction
constexpr uintptr_t kRequestOwnershipRva = 0x4363AD0;  // PhotonView.RequestOwnership()
constexpr uintptr_t kKinematicRva = 0x5286900;         // Rigidbody.get_isKinematic
constexpr uintptr_t kRbPositionRva = 0x52874B0;        // Rigidbody.set_position
constexpr uintptr_t kRbVelocityRva = 0x5287710;        // Rigidbody.set_velocity
constexpr uintptr_t kRbSpinRva = 0x5286E60;            // Rigidbody.set_angularVelocity
constexpr uintptr_t kSampleRva = 0x518F100;            // NavMesh.SamplePosition(Vector3, out hit, float, int)
constexpr uintptr_t kLightsOnRva = 0x8A9CD0;           // LevelRoom.AreRoomLightsOn(out bool)
constexpr uintptr_t kThrowEmfRva = 0x207EEF0;          // GhostInteraction.CreateThrowingEMF
constexpr float kReach = 8.0f;           // metres from the ghost
constexpr float kAboveFloor = 0.3f;      // metres: where a hidden item is set down
constexpr ULONGLONG kMinGapMs = 25000, kMaxGapMs = 45000;
constexpr ULONGLONG kOwnershipWaitMs = 2000;  // how long a taken item waits for the takeover to land

struct NavMeshHit {
  Vector3 position, normal;
  float distance;
  int mask;
  bool hit;
};

struct Api {
  il2cpp::Object* interactType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  void (*requestOwnership)(void*, const void*) = nullptr;
  bool (*kinematic)(void*, const void*) = nullptr;
  void (*rbPosition)(void*, Vector3, const void*) = nullptr;
  void (*rbVelocity)(void*, Vector3, const void*) = nullptr;
  void (*rbSpin)(void*, Vector3, const void*) = nullptr;
  bool (*sample)(Vector3, NavMeshHit*, float, int, const void*) = nullptr;
  bool (*isMine)(void*, const void*) = nullptr;
  bool (*lightsOn)(void*, bool*, const void*) = nullptr;
  void (*throwEmf)(void*, void*, int, bool, float, const void*) = nullptr;
  bool ok = false;
} api;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};
ULONGLONG g_next = 0;

// An item taken over and waiting to be moved: PhotonView.RequestOwnership only asks every client,
// and the item is the host's once that request comes back. Moved before then, the investigator who
// still owns it would sync it straight back.
struct Pending {
  il2cpp::Held item;
  Vector3 spot{};
  ULONGLONG deadline = 0;
} g_pending;

il2cpp::Array* rooms() {
  void* level = game::level_controller();
  return level && game::alive(level) ? il2cpp::field<il2cpp::Array*>(level, kLevelRooms) : nullptr;
}

void* room_at(Vector3 p) {
  il2cpp::Array* all = rooms();
  for (uintptr_t i = 0; all && i < all->max_length; ++i) {
    void* room = all->data<void*>()[i];
    if (room && game::alive(room) && game::in_room(room, p, 0.2f)) return room;
  }
  return nullptr;
}

bool dark(void* room) {
  bool flickering = false;
  return room && !api.lightsOn(room, &flickering, nullptr);
}

/** Loose equipment: a PhotonObjectInteract whose Prop is Equipment and which lies free, not held or placed. */
bool loose_equipment(void* item) {
  if (!item || !game::alive(item)) return false;
  void* rb = il2cpp::field<void*>(item, kInteractRigidbody);
  void* view = il2cpp::field<void*>(item, kInteractView);
  return il2cpp::is_kind(il2cpp::field<void*>(item, kInteractProp), "Equipment") && rb && game::alive(rb) &&
         !api.kinematic(rb, nullptr) && view && il2cpp::field<int>(view, kViewOwnershipTransfer) == kOwnershipTakeover;
}

/** A dark room with nobody in it, other than `except`, and a spot on its floor. */
bool hiding_place(void* except, Vector3& spot) {
  il2cpp::Array* all = rooms();
  std::vector<void*> candidates;
  for (uintptr_t i = 0; all && i < all->max_length; ++i) {
    void* room = all->data<void*>()[i];
    if (room && room != except && game::alive(room) && dark(room) && !game::any_players_in_room(room)) candidates.push_back(room);
  }
  std::shuffle(candidates.begin(), candidates.end(), g_random);
  for (void* room : candidates) {
    const std::vector<game::Bounds> boxes = game::room_bounds(room);
    if (boxes.empty()) continue;
    NavMeshHit hit{};
    if (api.sample(boxes[0].center, &hit, 3.0f, -1, nullptr) && game::in_room(room, hit.position, 0.2f)) {
      spot = {hit.position.x, hit.position.y + kAboveFloor, hit.position.z};
      return true;
    }
  }
  return false;
}

}  // namespace

bool resolve() {
  api.interactType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PhotonObjectInteract"));
  api.findObjects = reinterpret_cast<decltype(api.findObjects)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1));
  api.requestOwnership = reinterpret_cast<decltype(api.requestOwnership)>(game::game_assembly_address(kRequestOwnershipRva));
  api.kinematic = reinterpret_cast<decltype(api.kinematic)>(game::game_assembly_address(kKinematicRva));
  api.rbPosition = reinterpret_cast<decltype(api.rbPosition)>(game::game_assembly_address(kRbPositionRva));
  api.rbVelocity = reinterpret_cast<decltype(api.rbVelocity)>(game::game_assembly_address(kRbVelocityRva));
  api.rbSpin = reinterpret_cast<decltype(api.rbSpin)>(game::game_assembly_address(kRbSpinRva));
  api.sample = reinterpret_cast<decltype(api.sample)>(game::game_assembly_address(kSampleRva));
  api.lightsOn = reinterpret_cast<decltype(api.lightsOn)>(game::game_assembly_address(kLightsOnRva));
  api.throwEmf = reinterpret_cast<decltype(api.throwEmf)>(game::game_assembly_address(kThrowEmfRva));
  api.isMine = reinterpret_cast<decltype(api.isMine)>(
      il2cpp::method_ptr(il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonView"), "get_IsMine", 0));
  api.ok = api.isMine && api.interactType && api.findObjects && api.requestOwnership && api.kinematic && api.rbPosition &&
           api.rbVelocity && api.rbSpin && api.sample && api.lightsOn && api.throwEmf;
  PG_LOG("kikimora: resolve %s", api.ok ? "ok" : "FAILED");
  return api.ok;
}

void* nearest(void* ghost) {
  if (!api.ok || !ghost) return nullptr;
  const Vector3 at = game::position(game::transform_of(ghost));
  il2cpp::Array* items = api.findObjects(api.interactType, nullptr);
  void* best = nullptr;
  float bestDistance = kReach;
  for (uintptr_t i = 0; items && i < items->max_length; ++i) {
    void* item = items->data<void*>()[i];
    if (!loose_equipment(item)) continue;
    const Vector3 p = game::position(game::transform_of(item));
    const float d = game::distance(p, at);
    if (d < bestDistance && dark(room_at(p))) {
      best = item;
      bestDistance = d;
    }
  }
  return best;
}

bool hide(void* ghost, void* item) {
  if (g_pending.item) return false;  // still carrying the last one
  if (!api.ok || !ghost || !loose_equipment(item)) return false;
  void* transform = game::transform_of(item);
  const Vector3 from = game::position(transform);
  void* room = room_at(from);
  Vector3 spot{};
  if (game::distance(from, game::position(game::transform_of(ghost))) > kReach || !dark(room) || !hiding_place(room, spot)) return false;
  // EMF where it was taken from, as a ghost's throw leaves.
  void* activity = game::ghost_activity(ghost);
  if (void* interaction = activity ? il2cpp::field<void*>(activity, kActivityInteraction) : nullptr)
    api.throwEmf(interaction, item, 1, false, 20.0f, nullptr);
  void* view = il2cpp::field<void*>(item, kInteractView);
  if (!api.isMine(view, nullptr)) api.requestOwnership(view, nullptr);  // Takeover: granted when it comes back
  g_pending = {item, spot, GetTickCount64() + kOwnershipWaitMs};
  PG_LOG("kikimora: taking a piece of equipment %.1f m away", game::distance(from, spot));
  return true;
}

void move_pending(ULONGLONG now) {
  void* item = g_pending.item;
  if (!item) return;
  if (!game::alive(item) || !loose_equipment(item) || now >= g_pending.deadline) {
    PG_LOG("kikimora: the equipment was not handed over in time; left where it was");
    g_pending = {};
    return;
  }
  if (!api.isMine(il2cpp::field<void*>(item, kInteractView), nullptr)) return;
  void* rb = il2cpp::field<void*>(item, kInteractRigidbody);
  game::set_position(game::transform_of(item), g_pending.spot);
  api.rbPosition(rb, g_pending.spot, nullptr);
  api.rbVelocity(rb, {0, 0, 0}, nullptr);
  api.rbSpin(rb, {0, 0, 0}, nullptr);
  PG_LOG("kikimora: hid a piece of equipment");
  g_pending = {};
}

void tick(void* ghost, bool steered) {
  const ULONGLONG now = GetTickCount64();
  move_pending(now);
  if (!g_next) g_next = now + kMinGapMs;
  if (steered || now < g_next) return;
  g_next = now + kMinGapMs + std::uniform_int_distribution<ULONGLONG>(0, kMaxGapMs - kMinGapMs)(g_random);
  if (void* item = nearest(ghost)) hide(ghost, item);
}

bool in_lit_room(void* ghost) {
  if (!api.ok || !ghost || !game::alive(ghost)) return false;
  void* room = room_at(game::position(game::transform_of(ghost)));
  return room && !dark(room);
}

void reset() {
  g_next = 0;
  g_pending = {};
}

}  // namespace pg::custom::kikimora
