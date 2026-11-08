#include "squatter.h"

#include <windows.h>

#include <cmath>
#include <type_traits>
#include <vector>

#include "Difficulty.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "net.h"
#include "runtime.h"
#include "squatter_difficulty.h"
#include "squatter_door.h"
#include "squatter_figure.h"

namespace pm::custom::squatter {
namespace {

struct State {
  std::optional<Phase> phase;
  ShownInbox shown;  // .last: the master's last word on another client, the master's own on the master
  TryTeller tryTeller;
  TryInbox tries;
  SprintTeller sprintTeller;
  SprintBook sprints;
};
State g;

uint64_t now() { return GetTickCount64(); }

// Every client shows its own figure where the contract's ghost is, facing the way it faces.
void show(void* ghost) {
  void* t = phasmo::Component::get_transform(ghost);
  if (!t) return;
  const phasmo::Vector3 at = phasmo::Transform::get_position(t);
  if (!figure::ready()) figure::create(at);
  const phasmo::Vector3 f = phasmo::Transform::get_forward(t);
  figure::place(at, std::atan2(f.x, f.z));
}

template <auto& Member>
bool resolved() {
  return std::remove_cvref_t<decltype(Member)>::use::resolved();
}

// This client's own player's sprint, as its rig works it out in a contract (squatter_net.h sprint_speed_if_on,
// can_sprint); none while any of it cannot be read, so the master never takes a guess.
std::optional<Sprint> own_sprint() {
  using phasmo::LocalPlayer;
  if (!resolved<LocalPlayer::sprintSpeed>() || !resolved<LocalPlayer::speedMultiplier>() ||
      !resolved<LocalPlayer::isDead>() || !resolved<LocalPlayer::stamina>() ||
      !resolved<phasmo::PlayerStamina::canSprint>() || !resolved<phasmo::Difficulty::sprinting>())
    return std::nullopt;
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  void* stamina = rig && api::alive(rig) ? LocalPlayer::stamina.get(rig) : nullptr;
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = values && api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  if (!stamina || !difficulty) return std::nullopt;
  const bool can =
      can_sprint(phasmo::PlayerStamina::canSprint.get(stamina), phasmo::Difficulty::sprinting.get(difficulty));
  return Sprint{can, sprint_speed_if_on(LocalPlayer::sprintSpeed.get(rig), LocalPlayer::speedMultiplier.get(rig),
                                        LocalPlayer::isDead.get(rig))};
}

// Tells the master this client's sprint: the master records its own, any other client sends it on a change and at a
// modest rate.
void tell_sprint(bool master) {
  const std::optional<Sprint> s = own_sprint();
  if (!s) return;
  if (master) {
    g.sprints.report(game::local_actor(), *s, now());
    return;
  }
  if (!g.sprintTeller.due(*s, now())) return;
  const std::vector<uint8_t> message = sprint_message(*s);
  net::send(message.data(), message.size(), net::To::Master, true);
}

// The local player tried an exit this client holds (squatter_door.h on_attempt): one try per input to the master,
// which picks the line.
void tried(void*) {
  if (!g.tryTeller.due(now())) return;
  if (game::is_master()) {
    g.tries.report(game::local_actor(), now());
    return;
  }
  const std::vector<uint8_t> message = try_message();
  net::send(message.data(), message.size(), net::To::Master, true);
}

// What the channel brought, forgotten as the Squatter stops haunting.
void forget_channel() {
  g.shown = ShownInbox{};
  g.tryTeller = TryTeller{};
  g.tries.clear();
  g.sprintTeller = SprintTeller{};
  g.sprints.clear();
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  door::on_attempt(&tried);
  return door::create_hooks();
}

void tick(void* haunting, int type, bool /*steered: a player's own lines come in a later task*/) {
  const bool squatter = haunting && type == kSquatter;
  door::set_haunting(squatter);
  if (!squatter) {
    g.phase.reset();
    forget_channel();
    figure::destroy();
    // The exits' lock is kept: it belongs to the map, and squatter_door forgets it once the map's LevelController
    // is gone, so a tick without the ghost mid-contract never locks them twice.
    return;
  }
  const bool master = game::is_master();
  // The first phase waits for the difficulty: guessed once, it would hold for the whole contract.
  if (!g.phase)
    if (const std::optional<bool> friendlyGhost = friendly_if_read()) g.phase = first_phase(*friendlyGhost);
  g.phase = kept_phase(g.phase, g.shown.last, master);
  if (master) g.sprints.drop_stale(now());
  tell_sprint(master);
  show(haunting);
}

std::optional<Phase> phase() { return g.phase; }

void receive(int sender, const uint8_t* data, size_t size) {
  switch (net_msg::kind_of(data, size)) {
    case net_msg::kSquatter: g.shown.take(sender, game::master_actor(), data, size); break;
    case net_msg::kSquatterTry: {
      std::vector<int> room;
      game::room_actors(room);
      if (try_from(sender, game::is_master(), room, data, size)) g.tries.report(sender, now());
      break;
    }
    case net_msg::kSquatterSprint: {
      std::vector<int> room;
      game::room_actors(room);
      if (Sprint s; sprint_from(sender, game::is_master(), room, data, size, s)) g.sprints.report(sender, s, now());
      break;
    }
    default: break;
  }
}

std::optional<Shown> shown() { return g.shown.last; }

bool share(const Shown& s) {
  if (!game::is_master() || (g.shown.last && *g.shown.last == s)) return false;
  g.shown.last = s;
  const std::vector<uint8_t> message = shown_message(s);
  net::send(message.data(), message.size(), net::To::Others, true);
  return true;
}

std::vector<int> take_tries() { return game::is_master() ? g.tries.take() : std::vector<int>{}; }

std::optional<Sprint> sprint_of(int actor) { return g.sprints.of(actor, now()); }

void reset() {
  g = State{};
  figure::destroy();
  door::set_haunting(false);
  door::reset();
}

}  // namespace pm::custom::squatter
