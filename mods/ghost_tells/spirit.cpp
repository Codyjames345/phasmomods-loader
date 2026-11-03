#include "spirit.h"

#include <random>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostInfo.h"
#include "Player.h"
#include "game_api.h"
#include "gate.h"
#include "held.h"
#include "hold.h"
#include "log.h"
#include "net.h"
#include "runtime.h"
#include "spirit_rule.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pm::tells::spirit {
namespace {

using phasmo::GhostAI;
using world::Vector3;

struct State {
  api::Held ghost;
  IncenseStop stop;
  Quiet quiet;
  Answer answer;
} g;

std::mt19937 g_random{std::random_device{}()};

/** It may knock now: not hunting, showing itself, in an event or ability, or in its quiet minute. */
bool may_answer(void* ghost) {
  const int state = world::state(ghost);
  const GateStates& s = gate::states();
  return !world::hunting(ghost) && !world::appeared(ghost) && state != s.events[0] && state != s.events[1] &&
         state != s.events[2] && !g.quiet.quiet();
}

bool quiet_rule(gate::Kind kind) { return told_now() == kSpirit && g.quiet.quiet() && kind != gate::kHunt; }

/** A player is near enough for the Spirit to hear its name: living, within kSpiritHearRange, on its floor. */
bool within_hearing(void* ghost, void* player) {
  if (!world::living(player)) return false;
  const Vector3 a = world::feet(ghost), b = world::feet(player);
  return world::same_floor(a, b) && world::distance(a, b) <= kSpiritHearRange;
}

void detour_phrase(api::Original<phasmo::PhraseListenerController::OnPhraseRecognized> original, void* self,
                   void* phrase) {
  original(self, phrase);  // the game's own handling first
  if (!phrase || told_now() != kSpirit) return;
  void* ghost = world::ghost();
  if (!ghost || world::hunting(ghost)) return;
  void* info = GhostAI::info.get(ghost);
  void* name = info ? phasmo::GhostInfo::name.get(info) : nullptr;
  if (!name || !within_hearing(ghost, world::local_player())) return;
  if (!says_name(api::to_utf8(phrase), api::to_utf8(name))) return;
  if (world::is_master()) {
    heard_name(world::local_actor());
  } else {
    const std::vector<uint8_t> m = net::name_heard();
    pm::net::send(m.data(), m.size(), pm::net::To::Master, true);
  }
}

// The incense outside a hunt asks the ghost for its hunt hold (WhiteSage, and a Monkey Paw wish the
// same way); the game starts the hold after this returns, and starts none while one is on.
void* detour_stop_hunting(api::Original<GhostAI::StopGhostFromHunting> original, void* ai) {
  if (told_now() == kSpirit && world::is_master() && ai == g.ghost.get()) {
    const bool wasQuiet = g.quiet.quiet();
    g.quiet.incense(world::hunting(ai), GhostAI::huntsHeldOff.get(ai));
    if (g.quiet.quiet() && !wasQuiet) PM_LOG("spirit: incense reached it outside a hunt: quiet for a minute");
  }
  return original(ai);
}

}  // namespace

void resolve() { gate::add_rule(&quiet_rule); }

bool create_hooks() {
  return api::hook<phasmo::PhraseListenerController::OnPhraseRecognized>(
             &detour_phrase, "PhraseListenerController.OnPhraseRecognized", api::Twins::CanonicalOnly) &&
         api::hook<GhostAI::StopGhostFromHunting>(&detour_stop_hunting, "GhostAI.StopGhostFromHunting",
                                                  api::Twins::CanonicalOnly);
}

void tick_host(void* ghost, float dt) {
  if (ghost != g.ghost.get()) {
    reset();
    g.ghost = ghost;
  }
  const bool hunting = world::hunting(ghost);
  const bool stopped = g.stop.step(api::alive(GhostAI::smudge.get(ghost)), hunting, dt);
  hold::hold_still(stopped);
  g.quiet.step(dt);
  if (g.answer.step(may_answer(ghost), dt)) {
    gate::change_state(ghost, phasmo::GhostState::doorKnock());
    PM_LOG("spirit: it knocks in answer to its name");
  }
}

void heard_name(int actor) {
  void* ghost = g.ghost.get();
  if (told_now() != kSpirit || !api::alive(ghost)) return;
  if (!within_hearing(ghost, world::player_of_actor(actor))) return;  // as the host sees them now
  if (g.answer.hear(may_answer(ghost), std::uniform_real_distribution<float>(0.0f, 1.0f)(g_random)))
    PM_LOG("spirit: a player said its name");
}

void reset() {
  hold::hold_still(false);
  g = State{};
}

}  // namespace pm::tells::spirit
