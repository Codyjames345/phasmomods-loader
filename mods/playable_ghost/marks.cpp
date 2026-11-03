#include "marks.h"

#include <windows.h>

#include <cmath>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostSenses.h"
#include "Player.h"
#include "Sensor.h"
#include "Transform.h"
#include "content.h"
#include "footsteps.h"
#include "game.h"
#include "log.h"
#include "runtime.h"

namespace pm::marks {
namespace {

using phasmo::Vector3;
using perceive::Mark;

// Metres: the Dayan feels the investigator it watches this close.
constexpr float kDayanReach = 10.0f;  // pm:not-game-data: a literal in the Dayan's checks' code (GhostAI.DayanPlayerChecks)
constexpr ULONGLONG kSendMs = 200;

ULONGLONG g_next = 0;

Mark at(uint8_t kind, Vector3 p) { return {kind, p.x, p.y, p.z}; }

bool living(void* player) { return player && api::alive(player) && !phasmo::Player::isDead.get(player); }

void everyone(std::vector<Mark>& out) {
  void* net = game::network();
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    void* player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (living(player)) out.push_back(at(perceive::kPrey, game::position(game::transform_of(player))));
  }
}

void watched(void* ghost, std::vector<Mark>& out) {
  const api::List targets{phasmo::GhostAI::sightTargets.get(ghost)};
  void* target = targets.size() > 0 ? targets[0] : nullptr;
  void* player = target ? phasmo::PlayerLOSTarget::player.get(target) : nullptr;
  if (!living(player)) return;
  const Vector3 p = game::position(game::transform_of(player)), g = game::position(game::transform_of(ghost));
  if (std::hypot(p.x - g.x, p.z - g.z) > kDayanReach) return;  // the list is kept from the game's last check
  void* stamina = phasmo::Player::stamina.get(player);
  const bool moving = stamina && phasmo::NetworkedStamina::moving.get(stamina);
  out.push_back(at(moving ? perceive::kMover : perceive::kStill, p));
}

void electronics(void* ghost, std::vector<Mark>& out) {
  void* activity = game::ghost_activity(ghost);
  void* sensor = activity ? phasmo::GhostActivity::electronicsSensor.get(activity) : nullptr;
  const api::List found{sensor && api::alive(sensor) ? phasmo::Sensor::GetDetections(sensor, nullptr) : nullptr};
  for (size_t i = 0; i < found.size(); ++i) {
    void* object = found[i];
    if (object && api::alive(object)) out.push_back(at(perceive::kCharge, game::position(game::object_transform(object))));
  }
}

// What the definition of the type the ghost acts as shows its player alone (the Black Shuck's scent).
void player_view(std::vector<Mark>& out) {
  float xyz[3 * perceive::kMaxMarks];
  const uint32_t n = api::content::player_view(xyz, perceive::kMaxMarks);
  for (uint32_t i = 0; i < n && i < perceive::kMaxMarks; ++i)
    out.push_back({perceive::kScent, xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]});
}

}  // namespace

// The game members it reads are map entries, which the host checks before the mod starts.
bool resolve() { return true; }

void tick(session::Session& s, void* ghost, int behaviourType) {
  const ULONGLONG now = GetTickCount64();
  if (!s.local_is_master() || s.ghost_actor() <= 0 || now < g_next) return;
  g_next = now + kSendMs;
  std::vector<Mark> out;
  if (ghost && game::alive(ghost)) {
    const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
    const std::string type = api::content::id_of(behaviourType);
    if (type == "vanilla.kormos" && hunting) footsteps::heard(ghost, out);  // it hears only in a hunt
    else if (type == "vanilla.deogen" && hunting) everyone(out);
    else if (type == "vanilla.dayan") watched(ghost, out);
    else if (type == "vanilla.raiju" && hunting) electronics(ghost, out);  // its sensor pulses in hunts
    else player_view(out);
  }
  if (out.size() > static_cast<size_t>(perceive::kMaxMarks)) out.resize(perceive::kMaxMarks);
  s.send_marks(out);
}

}  // namespace pm::marks
