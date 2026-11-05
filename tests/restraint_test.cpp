// Offline tests for the hooks of mods/custom_ghosts/restraint.cpp over the fake backend: what the Echo's, the
// Bogeyman's and the Lithobolia's rules do to a ghost's attempts is the same whoever steers it, its AI or a
// player. Every case runs for both. The rules these hooks call are their own tests' (tests/custom_ghosts_test.cpp);
// here they are stubs that count their calls. Run: build\restraint_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>

#include "GhostAI.h"
#include "Props.h"
#include "bogeyman.h"
#include "brownie.h"
#include "check.h"
#include "content.h"
#include "echo.h"
#include "fake_game.h"
#include "game.h"
#include "ghosts.h"
#include "gremlin.h"
#include "hooks.h"
#include "hydra.h"
#include "kikimora.h"
#include "lithobolia.h"
#include "restraint.h"
#include "runtime.h"
#include "tempest.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace content = pm::api::content;
using namespace pm::custom;

constexpr int kNoGhost = 24;  // GhostType.None in build 25811160
constexpr int kIdle = 0, kHunting = 2, kLight = 3, kThrowing = 6, kRandomEvent = 14, kAppear = 13;
// The states the Squatter may still go into, numbered apart from the others above.
constexpr int kWander = 1, kFavouriteRoom = 20;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x38]{};
};

// The game, as restraint.cpp reads it.
Obj g_ghost, g_activity, g_gameDoor, g_closetDoor, g_prop, g_target, g_stateA, g_stateB;
int g_state = kIdle;
void* g_stateObject = &g_stateA;
bool g_master = true;

// What each hook's game body and each stubbed rule saw.
int g_interactBodies = 0, g_pulseBodies = 0, g_throwBodies = 0, g_changeBodies = 0;
int g_closetDoorCalls = 0, g_huntBegins = 0, g_repeatCalls = 0, g_gremlinCalls = 0, g_throwAims = 0, g_tempestEvents = 0;
int g_lastChange = -1;
void* g_doorChancePicked = nullptr;
void* g_gamePick = nullptr;      // what the game's door pick finds
void* g_closetAnswer = nullptr;  // what the Bogeyman's closet door answers
bool g_repeats = false;          // whether the Echo's repeat fires
bool g_lostDuringRepeat = false; // whether the repeat's own state change counted as lost
bool g_dormant = false;
int g_copyTo = -1;               // the type a Mimic's copy turns to inside the game's ChangeState
bool g_ignoreChange = false;     // the game ignores the change
bool g_turnsToIdle = false;      // the difficulty turns a hunt into idling
bool g_throwAimAnswer = true;    // what the Lithobolia's hunt throw answers

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

void* door_pick(void* activity) {
  using F = void* (*)(void*, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostActivity.GetDoorToOpen"))(activity, nullptr);
}
void change_state(void* ai, int state, void* target = nullptr, bool flag = false) {
  using F = void (*)(void*, int32_t, void*, bool, const void*);
  reinterpret_cast<F>(g_detour.at("GhostAI.ChangeState"))(ai, state, target, flag, nullptr);
}
void try_interact(void* activity) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("GhostActivity.TryInteract"))(activity, nullptr);
}
void normal_interaction(void* activity, bool force) {
  using F = void (*)(void*, bool, const void*);
  reinterpret_cast<F>(g_detour.at("GhostActivity.NormalInteraction"))(activity, force, nullptr);
}
bool door_chance(void* activity) {
  using F = bool (*)(void*, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostActivity.doorChance"))(activity, nullptr);
}
void try_throw(void* prop) {
  using F = void (*)(void*, bool, bool, const void*);
  reinterpret_cast<F>(g_detour.at("Prop.TryThrow"))(prop, false, false, nullptr);
}

// The game's bodies.
void fake_try_interact(void*, const void*) { ++g_interactBodies; }
void fake_normal_interaction(void*, bool, const void*) { ++g_pulseBodies; }
void* fake_get_door(void*, const void*) { return g_gamePick; }
// GhostActivity.doorChance asks for its door once, before it acts on the door.
bool fake_door_chance(void* activity, const void*) {
  g_doorChancePicked = door_pick(activity);
  return g_doorChancePicked != nullptr;
}
void fake_try_throw(void*, bool, bool, const void*) { ++g_throwBodies; }
// GhostAI.ChangeState: a Mimic's copy runs first (AttemptGhostTypeStateChange at its top); a hunt makes a new
// state object unless the game ignores the change or the difficulty turns it into idling.
void fake_change_state(void*, int32_t state, void*, bool, const void*) {
  ++g_changeBodies;
  g_lastChange = state;
  if (g_copyTo >= 0) fake::contract_ghost(g_copyTo, kNoGhost);
  if (g_ignoreChange) return;
  if (state == kHunting && g_turnsToIdle) state = kIdle;
  g_state = state;
  g_stateObject = g_stateObject == &g_stateA ? &g_stateB : &g_stateA;
}

int type_of(int type) { return content::type_of(id_of(*find(type))); }

// One game with every custom ghost's rule stubbed, the ghost acting as `type`, and the hooks made.
void start(int type, bool steered) {
  fake::reset();
  fake::value(map::GhostState_hunting, kHunting);
  fake::value(map::GhostState_randomEvent, kRandomEvent);
  fake::value(map::GhostState_appear, kAppear);
  fake::value(map::GhostState_throwing, kThrowing);
  fake::value(map::GhostState_idle, kIdle);
  fake::value(map::GhostState_wander, kWander);
  fake::value(map::GhostState_favouriteRoom, kFavouriteRoom);
  fake::method(map::GhostActivity_TryInteract, reinterpret_cast<void*>(&fake_try_interact));
  fake::method(map::GhostActivity_NormalInteraction, reinterpret_cast<void*>(&fake_normal_interaction));
  fake::method(map::GhostActivity_GetDoorToOpen, reinterpret_cast<void*>(&fake_get_door));
  fake::method(map::GhostActivity_doorChance, reinterpret_cast<void*>(&fake_door_chance));
  fake::method(map::GhostAI_ChangeState, reinterpret_cast<void*>(&fake_change_state));
  fake::method(map::Prop_TryThrow, reinterpret_cast<void*>(&fake_try_throw));
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  pm::api::bind(&g_host, missing);
  const char* const evidence[][3] = {{"emf", "orbs", "dots"}, {"emf", "orbs", "writing"}};
  int n = 0;
  for (const int registered : {kBogeyman, kEcho}) {
    content::GhostDef def;
    def.id = id_of(*find(registered));
    def.name = content::Text("en", find(registered)->name);
    def.description = def.strength = def.weakness = content::Text("en", "-");
    for (int i = 0; i < 3; ++i) def.evidence[i] = evidence[n][i];
    ++n;
    CHECK(content::register_ghost(def) > 0);
  }
  fake::commit_content();
  fake::room(1, {1});
  CHECK(restraint::create_hooks());
  fake::player_controlled(steered);
  g_state = kIdle;
  g_stateObject = &g_stateA;
  g_master = true;
  g_interactBodies = g_pulseBodies = g_throwBodies = g_changeBodies = 0;
  g_closetDoorCalls = g_huntBegins = g_repeatCalls = g_gremlinCalls = g_throwAims = g_tempestEvents = 0;
  g_lastChange = -1;
  g_doorChancePicked = nullptr;
  g_gamePick = &g_gameDoor;
  g_closetAnswer = &g_closetDoor;
  g_repeats = false;
  g_lostDuringRepeat = false;
  g_dormant = false;
  g_copyTo = -1;
  g_ignoreChange = g_turnsToIdle = false;
  g_throwAimAnswer = true;
  fake::contract_ghost(type == kBogeyman || type == kEcho ? type_of(type) : 0, type == kBogeyman || type == kEcho ? kNoGhost : 0);
  restraint::tick(&g_ghost, type);
}

template <class Case>
void for_ai_and_player(int type, Case each) {
  for (const bool steered : {false, true}) {
    start(type, steered);
    each();
  }
}

// The door chance's own pick that finds a door becomes the closet door the rule answers, in the idle state on
// the host, and in nothing else.
void the_bogeyman_turns_the_door_chances_door() {
  for_ai_and_player(kBogeyman, [] {
    CHECK(door_chance(&g_activity) && g_doorChancePicked == &g_closetDoor && g_closetDoorCalls == 1);
    // Outside the chance's own pick: another caller's, the hunt state's.
    CHECK(door_pick(&g_activity) == &g_gameDoor && g_closetDoorCalls == 1);
    // The rule found no closet door: the game's own pick stands.
    g_closetAnswer = nullptr;
    CHECK(door_chance(&g_activity) && g_doorChancePicked == &g_gameDoor && g_closetDoorCalls == 2);
    g_closetAnswer = &g_closetDoor;
    // No door found: nothing to turn.
    g_gamePick = nullptr;
    CHECK(!door_chance(&g_activity) && g_closetDoorCalls == 2);
    g_gamePick = &g_gameDoor;
    // In a hunt.
    g_state = kHunting;
    CHECK(door_chance(&g_activity) && g_doorChancePicked == &g_gameDoor && g_closetDoorCalls == 2);
    g_state = kIdle;
    // Off the host.
    g_master = false;
    CHECK(door_chance(&g_activity) && g_doorChancePicked == &g_gameDoor && g_closetDoorCalls == 2);
    g_master = true;
    // Another ghost's activity.
    Obj other;
    CHECK(door_chance(&other) && g_doorChancePicked == &g_gameDoor && g_closetDoorCalls == 2);
  });
}

// A change to the hunt that really made a new hunt steps it out of a closet, once; a second change the game
// ignores, and a hunt the difficulty turned to idle, do not.
void the_bogeyman_steps_out_at_a_new_hunt() {
  for_ai_and_player(kBogeyman, [] {
    change_state(&g_ghost, kHunting);
    CHECK(g_changeBodies == 1 && g_huntBegins == 1);
    change_state(&g_ghost, kHunting);  // already hunting: it makes a new state, but no hunt begins from another state
    CHECK(g_changeBodies == 2 && g_huntBegins == 1);
    g_state = kIdle;
    g_ignoreChange = true;
    change_state(&g_ghost, kHunting);  // the game ignored it: the state object stays
    CHECK(g_huntBegins == 1);
    g_ignoreChange = false;
    g_turnsToIdle = true;
    change_state(&g_ghost, kHunting);
    CHECK(g_state == kIdle && g_huntBegins == 1);
    g_turnsToIdle = false;
    change_state(&g_ghost, kLight);  // another state
    CHECK(g_huntBegins == 1);
    g_state = kIdle;
    g_master = false;
    change_state(&g_ghost, kHunting);  // off the host
    CHECK(g_huntBegins == 1);
  });
}

// A Mimic's copy runs inside the game's change. A copy made at the hunt's own change steps out when it has become
// a Bogeyman, though the last frame's rules were another's, and not when it has just stopped being one.
void a_copy_at_the_hunts_change_decides_its_closet_start() {
  for_ai_and_player(kEcho, [] {
    g_copyTo = type_of(kBogeyman);
    change_state(&g_ghost, kHunting);
    CHECK(g_huntBegins == 1);
  });
  for_ai_and_player(kBogeyman, [] {
    g_copyTo = type_of(kEcho);
    change_state(&g_ghost, kHunting);
    CHECK(g_huntBegins == 0);
  });
  // A Mimic acting as a game type, whose custom rules run nothing before the copy, steps out all the same.
  for_ai_and_player(kGremlin, [] {
    restraint::tick(nullptr, 0);
    g_copyTo = type_of(kBogeyman);
    change_state(&g_ghost, kHunting);
    CHECK(g_huntBegins == 1);
  });
}

// An unforced pulse outside the roll tries the Echo's repeat first and skips the game's when it fires; a forced
// one (the voodoo doll's) never repeats.
void the_echos_pulse_repeats() {
  for_ai_and_player(kEcho, [] {
    g_repeats = true;
    normal_interaction(&g_activity, false);
    CHECK(g_repeatCalls == 1 && g_pulseBodies == 0);
    normal_interaction(&g_activity, true);
    CHECK(g_repeatCalls == 1 && g_pulseBodies == 1);
    g_repeats = false;
    normal_interaction(&g_activity, false);  // nothing to repeat: the game's own
    CHECK(g_repeatCalls == 2 && g_pulseBodies == 2);
    g_repeats = true;
    g_master = false;
    normal_interaction(&g_activity, false);  // off the host
    CHECK(g_repeatCalls == 2 && g_pulseBodies == 3);
  });
  for_ai_and_player(kGremlin, [] {  // another ghost
    normal_interaction(&g_activity, false);
    CHECK(g_repeatCalls == 0 && g_pulseBodies == 1);
  });
}

// The roll's interaction when it acts on its own repeats alike, its AI's or a restless player's.
void the_echos_own_roll_repeats() {
  for_ai_and_player(kEcho, [] {
    g_repeats = true;
    try_interact(&g_activity);
    CHECK(g_repeatCalls == 1 && g_interactBodies == 0);
    g_repeats = false;
    try_interact(&g_activity);
    CHECK(g_repeatCalls == 2 && g_interactBodies == 1);
  });
}

// A roll a player takes (Playable Ghost grants it as a charge) goes straight on, whichever hook comes first: the
// AI's repeat, dormancy, sabotage and events' rules wait for the attempt the player then makes.
void a_roll_a_player_takes_goes_straight_on() {
  start(kEcho, true);
  fake::roll_taken(true);
  g_repeats = g_dormant = true;
  for (int i = 0; i < 40; ++i) try_interact(&g_activity);
  CHECK(g_repeatCalls == 0 && g_interactBodies == 40);
  for (int i = 0; i < 40; ++i) change_state(&g_ghost, kRandomEvent);
  CHECK(g_changeBodies == 40);
  start(kGremlin, true);
  fake::roll_taken(true);
  try_interact(&g_activity);
  CHECK(g_gremlinCalls == 0 && g_interactBodies == 1);
  start(kTempest, true);
  fake::roll_taken(true);
  change_state(&g_ghost, kRandomEvent);
  CHECK(g_tempestEvents == 0 && g_changeBodies == 1);
  fake::roll_taken(false);
  change_state(&g_ghost, kRandomEvent);
  CHECK(g_tempestEvents == 1);
}

// Dormant, an Echo loses 3 in 4 of its interactions and events, its AI's and a player's alike, and never a repeat.
// Its ability it does not use at all, as its AI never speaks while dormant. Nothing holds a player back outright.
void a_dormant_echo_loses_three_in_four() {
  constexpr int kTries = 4000;
  const auto about_three_in_four = [](int lost) { return lost > kTries * 7 / 10 && lost < kTries * 8 / 10; };
  for_ai_and_player(kEcho, [&] {
    CHECK(!restraint::held_back(kEcho, restraint::kInteract) && !restraint::held_back(kEcho, restraint::kEvent) &&
          !restraint::held_back(kEcho, restraint::kAbility));
    CHECK(!restraint::interaction_lost(kEcho));  // awake
    g_dormant = true;
    CHECK(!restraint::held_back(kEcho, restraint::kInteract) && !restraint::held_back(kEcho, restraint::kEvent) &&
          restraint::held_back(kEcho, restraint::kAbility));
    // The roll's interaction, as it acts on its own.
    for (int i = 0; i < kTries; ++i) try_interact(&g_activity);
    CHECK(about_three_in_four(kTries - g_interactBodies));
    // The event the game's state change would begin, its AI's and a player's chosen one alike.
    for (int i = 0; i < kTries; ++i) change_state(&g_ghost, kRandomEvent);
    CHECK(about_three_in_four(kTries - g_changeBodies));
    // A player's aimed interaction, which has chosen what it does.
    int lost = 0;
    for (int i = 0; i < kTries; ++i) lost += restraint::interaction_lost(kEcho) ? 1 : 0;
    CHECK(about_three_in_four(lost));
    // A repeat is never lost, its own state change included (the stub asks as Playable Ghost does).
    g_repeats = true;
    g_repeatCalls = 0;
    for (int i = 0; i < 200; ++i) normal_interaction(&g_activity, false);
    CHECK(!g_lostDuringRepeat && g_repeatCalls == 200);
    // Nothing but a repeat's own change: the next aimed interaction may be lost again.
    lost = 0;
    for (int i = 0; i < kTries; ++i) lost += restraint::interaction_lost(kEcho) ? 1 : 0;
    CHECK(about_three_in_four(lost));
    // Only for the Echo.
    CHECK(!restraint::interaction_lost(kGremlin));
  });
}

// The hunt's throw flies at someone in sight, its AI's and a player's alike; none of this outside a hunt, off the
// host, or for another ghost.
void the_lithobolias_hunt_throw_is_aimed() {
  for_ai_and_player(kLithobolia, [] {
    g_state = kHunting;
    try_throw(&g_prop);
    CHECK(g_throwAims == 1 && g_throwBodies == 0);
    g_throwAimAnswer = false;  // not a throw it can aim: the game's own
    try_throw(&g_prop);
    CHECK(g_throwAims == 2 && g_throwBodies == 1);
    g_throwAimAnswer = true;
    g_state = kIdle;
    try_throw(&g_prop);
    CHECK(g_throwAims == 2 && g_throwBodies == 2);
    g_state = kHunting;
    g_master = false;
    try_throw(&g_prop);
    CHECK(g_throwAims == 2 && g_throwBodies == 3);
  });
  for_ai_and_player(kGremlin, [] {
    g_state = kHunting;
    try_throw(&g_prop);
    CHECK(g_throwAims == 0 && g_throwBodies == 1);
  });
}

// The Squatter attempts nothing, its AI and a player steering it alike (the Squatter's spec, "There is no paranormal
// activity"): no interaction from its roll, no pulse of its interaction sensor (a voodoo pin's forced one included),
// and no state change but idling, wandering and going to its favourite room, forced or not, so no event, ability,
// interaction, cursed pull or hunt begins (sdk/systems/ghost-ai.md, "Every route to a state change"). A forced hunt on
// a friendly ghost's difficulty, which the game would turn into idling, never reaches the game's change at all, so
// no hunt state is made and nobody can be killed. A player who takes the roll changes nothing. Its rules hold back
// every kind a player may ask (PmGhostDef.heldBack). Another ghost is left alone.
void the_squatter_starts_no_interaction_event_or_hunt() {
  for_ai_and_player(kSquatter, [] {
    try_interact(&g_activity);
    normal_interaction(&g_activity, false);
    normal_interaction(&g_activity, true);  // the voodoo doll's pin
    CHECK(g_interactBodies == 0 && g_pulseBodies == 0);
    for (const int state : {kHunting, kRandomEvent, kAppear, kLight, kThrowing, 15, 19, 21, 22, 23, 42, -1})
      for (const bool forced : {false, true}) change_state(&g_ghost, state, &g_prop, forced);
    CHECK(g_changeBodies == 0 && g_huntBegins == 0 && g_tempestEvents == 0 && g_state == kIdle);
    // A forced hunt on a friendly ghost's difficulty, as a cursed possession or the Death card makes one.
    g_turnsToIdle = true;
    change_state(&g_ghost, kHunting, nullptr, true);
    CHECK(g_changeBodies == 0 && g_lastChange == -1 && g_huntBegins == 0 && g_state == kIdle);
    g_turnsToIdle = false;
    // Idling, wandering and its favourite room stay the game's.
    change_state(&g_ghost, kWander);
    change_state(&g_ghost, kFavouriteRoom, nullptr, true);
    change_state(&g_ghost, kIdle);
    CHECK(g_changeBodies == 3 && g_state == kIdle);
    // A player taking the roll is refused alike.
    fake::roll_taken(true);
    try_interact(&g_activity);
    change_state(&g_ghost, kRandomEvent);
    CHECK(g_interactBodies == 0 && g_changeBodies == 3);
    fake::roll_taken(false);
    for (const restraint::Kind kind : {restraint::kInteract, restraint::kEvent, restraint::kHunt, restraint::kAbility,
                                       restraint::kFuseBox})
      CHECK(restraint::held_back(kSquatter, kind));
    // Another ghost's activity and another ghost's change are the game's.
    Obj other;
    try_interact(&other);
    normal_interaction(&other, true);
    change_state(&other, kHunting);
    CHECK(g_interactBodies == 1 && g_pulseBodies == 1 && g_changeBodies == 4);
  });
  for_ai_and_player(kGremlin, [] {
    try_interact(&g_activity);
    change_state(&g_ghost, kHunting, nullptr, true);
    CHECK(g_interactBodies == 1 && g_changeBodies == 1);
    CHECK(!restraint::held_back(kGremlin, restraint::kHunt) && !restraint::held_back(kGremlin, restraint::kFuseBox));
  });
}

}  // namespace

// What restraint.cpp calls of the game.
namespace pm::game {
void* ghost_activity(void*) { return &g_activity; }
int ghost_state(void*) { return g_state; }
void* ghost_state_object(void*) { return g_stateObject; }
bool is_master() { return g_master; }
}  // namespace pm::game

// What restraint.cpp calls of the ghosts' own rules: each has its own tests.
namespace pm::custom {
namespace bogeyman {
void* closet_door(void*) {
  ++g_closetDoorCalls;
  return g_closetAnswer;
}
void hunt_begins(void*) { ++g_huntBegins; }
}  // namespace bogeyman
namespace brownie {
bool watched() { return false; }
}  // namespace brownie
namespace echo {
bool dormant_now() { return g_dormant; }
bool repeat(void*) {
  ++g_repeatCalls;
  // A repeat's interaction runs the game's state change inside this call, where Playable Ghost asks whether an
  // aimed interaction is lost.
  if (g_repeats && restraint::interaction_lost(kEcho)) g_lostDuringRepeat = true;
  return g_repeats;
}
}  // namespace echo
namespace gremlin {
bool interact(void*) {
  ++g_gremlinCalls;
  return false;
}
}  // namespace gremlin
namespace hydra {
void ghost_threw(void*, void*) {}
}  // namespace hydra
namespace kikimora {
bool in_lit_room(void*) { return false; }
}  // namespace kikimora
namespace lithobolia {
bool intercept(void*, void*) { return false; }
bool intercept_hunt_throw(void*, void*) {
  ++g_throwAims;
  return g_throwAimAnswer;
}
}  // namespace lithobolia
namespace tempest {
void event_begins() { ++g_tempestEvents; }
}  // namespace tempest
}  // namespace pm::custom

int main() {
  the_bogeyman_turns_the_door_chances_door();
  the_bogeyman_steps_out_at_a_new_hunt();
  a_copy_at_the_hunts_change_decides_its_closet_start();
  the_echos_pulse_repeats();
  the_echos_own_roll_repeats();
  a_roll_a_player_takes_goes_straight_on();
  a_dormant_echo_loses_three_in_four();
  the_lithobolias_hunt_throw_is_aimed();
  the_squatter_starts_no_interaction_event_or_hunt();
  restraint::tick(nullptr, 0);  // what it holds goes back before the fake's own storage does
  std::printf(g_failures ? "%d check(s) failed\n" : "all restraint tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
