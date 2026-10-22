#include "spirit.h"

#include <random>

#include "game.h"
#include "gate.h"
#include "hold.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"
#include "players.h"
#include "spirit_rule.h"
#include "tells.h"
#include "tells_net.h"
#include "world.h"

namespace pg::tells::spirit {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (class dump, REA).
constexpr size_t kGhostInfo = 0x38;         // GhostAI: GhostInfo
constexpr size_t kGhostIncense = 0xF5;      // GhostAI: incense outside a hunt holds its hunts off
constexpr size_t kGhostSmudge = 0xF8;       // GhostAI: the WhiteSage that smudged it in this hunt
constexpr size_t kInfoName = 0x48;          // GhostInfo: its name (what the game's own phrase check compares)
constexpr uintptr_t kPhraseRva = 0x2370E00; // PhraseListenerController.OnPhraseRecognized(string)
constexpr int kStateDoorKnock = 9;          // GhostAI.State.doorKnock
constexpr int kStateAppear = 8, kStateRandomEvent = 14, kStateAbility = 15;
constexpr size_t kMaxPhrase = 512;

using Phrase_t = void (*)(void*, il2cpp::String*, const void*);
Phrase_t g_phrase = nullptr;

struct State {
  il2cpp::Held ghost;
  bool spirit = false;  // the told ghost is a Spirit
  IncenseStop stop;
  Quiet quiet;
  Answer answer;
} g;

std::mt19937 g_random{std::random_device{}()};

/** It may knock now: not hunting, showing itself, in an event or ability, or in its quiet minute. */
bool may_answer(void* ghost) {
  const int state = game::ghost_state(ghost);
  return !world::hunting(ghost) && !world::appeared(ghost) && state != kStateAppear && state != kStateRandomEvent &&
         state != kStateAbility && !g.quiet.quiet();
}

bool quiet_rule(gate::Kind kind) { return g.spirit && g.quiet.quiet() && kind != gate::kHunt; }

/** A player is near enough for the Spirit to hear its name: living, within kSpiritHearRange, on its floor. */
bool within_hearing(void* ghost, void* player) {
  if (!game::living(player)) return false;
  const Vector3 a = world::feet(ghost), b = world::feet(player);
  return world::same_floor(a, b) && game::distance(a, b) <= kSpiritHearRange;
}

void detour_phrase(void* self, il2cpp::String* phrase, const void* method) {
  hooks::Guard guard;
  g_phrase(self, phrase, method);  // the game's own handling first (it acts only in hunts)
  if (!phrase || !tells_on()) return;
  void* ghost = game::ghost_ai();
  if (world::behaviour_type(ghost) != kSpirit || world::hunting(ghost)) return;
  void* info = il2cpp::field<void*>(ghost, kGhostInfo);
  auto* name = info ? il2cpp::field<il2cpp::String*>(info, kInfoName) : nullptr;
  if (!name || !within_hearing(ghost, game::local_player())) return;
  char said[kMaxPhrase], named[kMaxPhrase];
  il2cpp::to_utf8(phrase, said, sizeof said);
  il2cpp::to_utf8(name, named, sizeof named);
  if (!says_name(said, named)) return;
  if (game::is_master()) {
    heard_name(game::local_actor());
  } else {
    const std::vector<uint8_t> m = net::name_heard();
    pg::net::send(m.data(), m.size(), pg::net::To::Master, true);
  }
}

}  // namespace

bool resolve() {
  gate::add_rule(&quiet_rule);
  return true;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kPhraseRva), reinterpret_cast<void*>(&detour_phrase),
                       reinterpret_cast<void**>(&g_phrase), "PhraseListenerController.OnPhraseRecognized");
}

void tick_host(void* ghost, int type, float dt) {
  if (ghost != g.ghost || (type == kSpirit) != g.spirit) {
    reset();
    g.ghost = ghost;
    g.spirit = type == kSpirit;
  }
  if (!g.spirit) return;
  const bool hunting = world::hunting(ghost);
  const bool stopped = g.stop.step(game::alive(il2cpp::field<void*>(ghost, kGhostSmudge)), hunting, dt);
  hold::hold_still(ghost, stopped);
  const bool wasQuiet = g.quiet.quiet();
  if (g.quiet.step(il2cpp::field<bool>(ghost, kGhostIncense), hunting, dt) && !wasQuiet)
    PG_LOG("spirit: incense reached it outside a hunt: quiet for a minute");
  if (g.answer.step(may_answer(ghost), dt)) {
    gate::change_state(ghost, kStateDoorKnock);
    PG_LOG("spirit: it knocks in answer to its name");
  }
}

void heard_name(int actor) {
  if (!g.spirit || !g.ghost || !game::alive(g.ghost)) return;
  if (!within_hearing(g.ghost, game::player_of_actor(actor))) return;  // as the host sees them now
  if (g.answer.hear(may_answer(g.ghost), std::uniform_real_distribution<float>(0.0f, 1.0f)(g_random)))
    PG_LOG("spirit: a player said its name");
}

void reset() {
  if (g.ghost) hold::hold_still(g.ghost, false);
  g = State{};
}

}  // namespace pg::tells::spirit
