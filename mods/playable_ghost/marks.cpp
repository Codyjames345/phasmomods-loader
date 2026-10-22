#include "marks.h"

#include <windows.h>

#include <cmath>
#include <vector>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "services.h"

namespace pg::marks {
namespace {

using il2cpp::Vector3;
using perceive::Mark;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kGhostHearing = 0x98;        // GhostAI: GhostHearing
constexpr size_t kHearingDetected = 0x58;     // GhostHearing: List<(PlayerNoise noise, float distance)>, 16-byte entries
constexpr size_t kNoiseLevel = 0x48;          // PlayerNoise: 0 walking, 1 sprinting, 2 crouching
constexpr size_t kNoisePlayer = 0x50;         // PlayerNoise: its Player
constexpr size_t kGhostSightTargets = 0x138;  // GhostAI: List<PlayerLOSTarget>, nearest first (DayanPlayerChecks)
constexpr size_t kSightPlayer = 0x20;         // PlayerLOSTarget: its Player
constexpr size_t kPlayerStamina = 0xE8;       // Player: NetworkedStamina
constexpr size_t kStaminaMoving = 0x4C;       // NetworkedStamina: moving (what empowers a Dayan)
constexpr size_t kActivityElectronics = 0x30; // GhostActivity: the Raiju's electronics RangeSensor
constexpr size_t kNetworkSpots = 0x38;        // Network: List<PlayerSpot>
constexpr size_t kSpotPlayer = 0x38;          // PlayerSpot: networked Player
constexpr size_t kPlayerDead = 0x28;          // Player: dead
constexpr uintptr_t kDetectionsRva = 0x3D19D20;  // Micosmo Sensor.GetDetections(List<GameObject> = null)
constexpr float kDayanReach = 10.0f;          // metres: the Dayan feels the investigator it watches this close
constexpr int kRaiju = 18, kDeogen = 22, kDayan = 26, kKormos = 28, kBlackShuck = 35;
constexpr ULONGLONG kSendMs = 200;

struct HeardSound {  // GhostHearing's detected-sound entry
  void* noise;
  float distance;
  float pad;
};

struct Api {
  il2cpp::List* (*detections)(void*, il2cpp::List*, const void*) = nullptr;
} api;

ULONGLONG g_next = 0;

Mark at(uint8_t kind, Vector3 p) { return {kind, p.x, p.y, p.z}; }

bool living(void* player) { return player && game::alive(player) && !il2cpp::field<bool>(player, kPlayerDead); }

void hearing(void* ghost, std::vector<Mark>& out) {
  void* ears = il2cpp::field<void*>(ghost, kGhostHearing);
  auto* heard = ears && game::alive(ears) ? il2cpp::field<il2cpp::List*>(ears, kHearingDetected) : nullptr;
  for (int i = 0; heard && heard->items && i < heard->size; ++i) {
    void* noise = heard->items->data<HeardSound>()[i].noise;
    void* player = noise ? il2cpp::field<void*>(noise, kNoisePlayer) : nullptr;
    if (!living(player)) continue;
    const int level = il2cpp::field<int>(noise, kNoiseLevel);
    const uint8_t kind = level == 1 ? perceive::kStepSprint : level == 2 ? perceive::kStepCreep : perceive::kStepWalk;
    out.push_back(at(kind, game::position(game::transform_of(player))));
  }
}

void everyone(std::vector<Mark>& out) {
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    if (living(player)) out.push_back(at(perceive::kPrey, game::position(game::transform_of(player))));
  }
}

void watched(void* ghost, std::vector<Mark>& out) {
  auto* targets = il2cpp::field<il2cpp::List*>(ghost, kGhostSightTargets);
  void* target = targets && targets->items && targets->size > 0 ? targets->items->data<void*>()[0] : nullptr;
  void* player = target ? il2cpp::field<void*>(target, kSightPlayer) : nullptr;
  if (!living(player)) return;
  const Vector3 p = game::position(game::transform_of(player)), g = game::position(game::transform_of(ghost));
  if (std::hypot(p.x - g.x, p.z - g.z) > kDayanReach) return;  // the list is kept from the game's last check
  void* stamina = il2cpp::field<void*>(player, kPlayerStamina);
  const bool moving = stamina && il2cpp::field<bool>(stamina, kStaminaMoving);
  out.push_back(at(moving ? perceive::kMover : perceive::kStill, p));
}

void electronics(void* ghost, std::vector<Mark>& out) {
  void* activity = game::ghost_activity(ghost);
  void* sensor = activity ? il2cpp::field<void*>(activity, kActivityElectronics) : nullptr;
  il2cpp::List* found = sensor && game::alive(sensor) ? api.detections(sensor, nullptr, nullptr) : nullptr;
  for (int i = 0; found && found->items && i < found->size; ++i) {
    void* object = found->items->data<void*>()[i];
    if (object && game::alive(object)) out.push_back(at(perceive::kCharge, game::position(game::object_transform(object))));
  }
}

void scent(std::vector<Mark>& out) {
  const auto* s = static_cast<const PgCustomGhostsService*>(mod::host().service(PG_SERVICE_CUSTOM_GHOSTS));
  if (!s || s->version < 4 || !s->scent) return;
  float xyz[3 * perceive::kMaxMarks];
  const int n = s->scent(xyz, perceive::kMaxMarks);
  for (int i = 0; i < n && i < perceive::kMaxMarks; ++i) out.push_back({perceive::kScent, xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]});
}

}  // namespace

bool resolve() {
  api.detections = reinterpret_cast<decltype(api.detections)>(game::game_assembly_address(kDetectionsRva));
  return api.detections != nullptr;
}

void tick(session::Session& s, void* ghost, int behaviourType, int identity) {
  const ULONGLONG now = GetTickCount64();
  if (!s.local_is_master() || s.ghost_actor() <= 0 || now < g_next) return;
  g_next = now + kSendMs;
  std::vector<Mark> out;
  if (ghost && game::alive(ghost)) {
    const bool hunting = game::ghost_state(ghost) == game::kStateHunting;
    if (behaviourType == kKormos && hunting) hearing(ghost, out);  // it hears only in a hunt
    else if (behaviourType == kDeogen && hunting) everyone(out);
    else if (behaviourType == kDayan) watched(ghost, out);
    else if (behaviourType == kRaiju && hunting) electronics(ghost, out);  // its sensor pulses in hunts
    else if (identity == kBlackShuck && hunting) scent(out);
  }
  if (out.size() > static_cast<size_t>(perceive::kMaxMarks)) out.resize(perceive::kMaxMarks);
  s.send_marks(out);
}

}  // namespace pg::marks
