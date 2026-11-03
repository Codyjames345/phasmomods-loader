// Offline tests for a ghost player's actions (mods/playable_ghost/actions.cpp) over the fake backend: what an
// aimed interaction costs when the ghost's own rules lose it, an aim that hits nothing, whom the hunt check
// measures, the hunt's insanity whichever mod's hook on it runs first, and whose the idle roll is. The source is
// included so its state can be read. Run: build\actions_test.exe
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "fake_game.h"
#include "hooks.h"

#include "../mods/playable_ghost/actions.cpp"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace content = pm::api::content;
namespace actions = pm::actions;
namespace tether = pm::tether;
namespace session = pm::session;

constexpr int32_t kNoGhost = 24;   // GhostType.None in build 25811160
constexpr int32_t kSpirit = 0;     // GhostType.Spirit
constexpr int32_t kBanshee = 4;    // GhostType.Banshee
constexpr int kLight = 9;          // a state of the interaction chain, the fake's number for GhostState.light
// The fake's field offsets.
constexpr int32_t kInfo = 0x10, kBansheeTarget = 0x18, kBoost = 0x20;  // GhostAI
constexpr int32_t kBehaviour = 0x10, kIdentity = 0x14;                 // GhostInfo
constexpr int32_t kIdleGhost = 0x10, kIdleInsanity = 0x18;             // GhostIdleState
constexpr int32_t kSanity = 0x10, kDead = 0x18;                        // Player
constexpr int32_t kInsanity = 0x10;                                    // PlayerSanity
constexpr int32_t kSpots = 0x10, kSpotPlayer = 0x10;                   // Network, PlayerSpot
constexpr int32_t kDoorOpen = 0x10;                                    // SetupPhaseController
constexpr int32_t kFuseBoxField = 0x10;                                // LevelController
constexpr int32_t kPowerOn = 0x10;                                     // FuseBox
// The game's GhostState numbers in the audited build, which actions.cpp reads through the map.
constexpr int32_t kIdleState = 0, kWanderState = 1, kHuntingState = 2, kFavouriteState = 3, kThrowState = 6,
                  kFuseState = 7, kEventState = 14, kAbilityState = 15, kDotsState = 21;
constexpr int32_t kJinn = 5;                                           // GhostType.Jinn

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x38]{};
};
template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

Obj g_ghost, g_info, g_activity, g_setup, g_net, g_idle, g_aimed, g_elsewhere, g_interact, g_level, g_box;
Obj g_playerA, g_playerB, g_sanityA, g_sanityB, g_spotA, g_spotB;
int g_state = kIdleState;
void* g_hit = nullptr;              // what the aim's raycast hits

// The game's bodies, and what they saw.
int g_nativeChanges = 0, g_lastNative = -1, g_pulses = 0;
int g_chosenState = -1;             // the state the interaction chain chooses for what the sensor detects
bool g_repeatFires = false;         // a mod's hook on the pulse makes its repeat, whatever the aim
size_t g_detectedAfterFilter = 99;
bool g_rollTakenInExecute = false;

std::map<std::string, void*> g_detour;  // the hooks made, each entry's by its key

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

using ChangeState_t = void (*)(void*, int, void*, bool, const void*);
using IdleExecute_t = void (*)(void*, const void*);
using DetectedComponents_t = void* (*)(void*, void*, const void*);

// The game's GhostAI.ChangeState under every hook.
void native_change(void*, int state, void*, bool, const void*) {
  ++g_nativeChanges;
  g_lastNative = state;
}
// Its entry, as a mod's call reaches it: through this mod's hook.
void entry_change(void* ghost, int state, void* interact, bool flag, const void* method) {
  reinterpret_cast<ChangeState_t>(g_detour.at(map::GhostAI_ChangeState.key))(ghost, state, interact, flag, method);
}
void chain_change(int state, void* interact) { entry_change(&g_ghost, state, interact, false, nullptr); }
// The interaction chain of the sensor's pulse: a repeat of a mod's first, whatever the aim; otherwise what the
// sensor reports, which the aim narrows, decides what happens.
void native_pulse(void*, bool, const void*) {
  ++g_pulses;
  if (g_repeatFires) {
    chain_change(g_chosenState, &g_interact);
    return;
  }
  void* detected = fake::list({&g_aimed, &g_elsewhere});
  reinterpret_cast<DetectedComponents_t>(g_detour.at(map::Sensor_GetDetectedComponents.key))(nullptr, detected, nullptr);
  const std::vector<void*> left = fake::list_items_of(detected);
  g_detectedAfterFilter = left.size();
  if (!left.empty()) chain_change(g_chosenState, &g_interact);
}
void* native_detected(void*, void* list, const void*) { return list; }
void native_try_interact(void*, const void*) {}
int native_random(int min, int, const void*) { return min; }
void plain_idle_execute(void*, const void*) { g_rollTakenInExecute = actions::roll_taken(); }
IdleExecute_t g_executeBody = &plain_idle_execute;  // what the game's body does, which a test may replace
void native_idle_execute(void* idle, const void* method) { g_executeBody(idle, method); }
void* native_twin(void*, bool, const void*) { return nullptr; }

// A hook on the entries this mod hooks: its detour stands in for the entry, and its original is the game's body under
// it, as MinHook gives. A mod's call of the entry reaches the detour (entry_change).
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  const std::string k = key;
  if (k == map::GhostAI_ChangeState.key) *original = reinterpret_cast<void*>(&native_change);
  if (k == map::GhostIdleState_Execute.key) *original = reinterpret_cast<void*>(&native_idle_execute);
  if (k == map::GhostActivity_TryInteract.key) *original = reinterpret_cast<void*>(&native_try_interact);
  if (k == map::Random_RandomRangeInt.key) *original = reinterpret_cast<void*>(&native_random);
  if (k == map::Sensor_GetDetectedComponents.key) *original = reinterpret_cast<void*>(&native_detected);
  return true;
}

void bind() {
  std::string missing;
  pm::api::bind(&g_host, missing);
}

// How the ghost's rules answer (a registered ghost's definition).
bool g_lost = false, g_abilityHeld = false, g_interactHeld = false, g_fuseHeld = false;
int g_lostAsks = 0;
int32_t g_custom = -1;

void no_insanity(void*, const void*) {}

void start(void (*insanityBody)(void*, const void*) = &no_insanity) {
  fake::reset();
  fake::field(map::GhostAI_info, kInfo);
  fake::field(map::GhostAI_bansheeTarget, kBansheeTarget);
  fake::field(map::GhostAI_interactionBoost, kBoost);
  fake::field(map::GhostInfo_behaviourType, kBehaviour);
  fake::field(map::GhostInfo_identityType, kIdentity);
  fake::field(map::GhostIdleState_ghost, kIdleGhost);
  fake::field(map::GhostIdleState_insanity, kIdleInsanity);
  fake::field(map::Player_sanity, kSanity);
  fake::field(map::Player_isDead, kDead);
  fake::field(map::PlayerSanity_insanity, kInsanity);
  fake::field(map::Network_playerSpots, kSpots);
  fake::field(map::PlayerSpot_player, kSpotPlayer);
  fake::field(map::SetupPhaseController_mainDoorOpen, kDoorOpen);
  fake::field(map::LevelController_fuseBox, kFuseBoxField);
  fake::field(map::FuseBox_powerOn, kPowerOn);
  fake::value(map::GhostState_light, kLight);
  fake::value(map::GhostState_idle, kIdleState);
  fake::value(map::GhostState_wander, kWanderState);
  fake::value(map::GhostState_dots, kDotsState);
  fake::value(map::GhostState_hunting, kHuntingState);
  fake::value(map::GhostState_favouriteRoom, kFavouriteState);
  fake::value(map::GhostState_throwing, kThrowState);
  fake::value(map::GhostState_fusebox, kFuseState);
  fake::value(map::GhostState_randomEvent, kEventState);
  fake::value(map::GhostState_GhostAbility, kAbilityState);
  fake::method(map::GhostIdleState_HuntInsanity, reinterpret_cast<void*>(insanityBody));
  fake::method(map::GhostAI_ChangeState, reinterpret_cast<void*>(&entry_change));
  fake::method(map::GhostIdleState_Execute, reinterpret_cast<void*>(&native_idle_execute));
  fake::method(map::GhostActivity_TryInteract, reinterpret_cast<void*>(&native_try_interact));
  fake::method(map::Random_RandomRangeInt, reinterpret_cast<void*>(&native_random));
  fake::method(map::Sensor_GetDetectedComponents, reinterpret_cast<void*>(&native_detected));
  fake::method(map::GhostActivity_NormalInteraction, reinterpret_cast<void*>(&native_pulse));
  fake::method(map::GhostActivity_TwinInteraction, reinterpret_cast<void*>(&native_twin));
  g_executeBody = &plain_idle_execute;
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  bind();
  content::GhostDef def;
  def.id = "a.ghost";
  def.name = content::Text("en", "Ghost");
  def.description = def.strength = def.weakness = content::Text("en", "-");
  def.evidence = {"emf", "orbs", "dots"};
  def.abilityName = def.abilityDescription = content::Text("en", "Ability");
  def.useAbility = [](void*) { return true; };
  def.heldBack = [](int32_t kind, void*) {
    return (kind == pm::content_link::kAbilityAlone && g_abilityHeld) || (kind == 0 && g_interactHeld) ||
           (kind == pm::content_link::kFuseBox && g_fuseHeld);
  };
  def.interactionLost = [](void*) {
    ++g_lostAsks;
    return g_lost;
  };
  CHECK(content::register_ghost(def) > 0);
  fake::commit_content();
  fake::room(1, {1});
  g_custom = content::type_of("a.ghost");
  fake::contract_ghost(g_custom, kNoGhost);
  CHECK(actions::create_hooks());

  for (Obj* o : {&g_ghost, &g_info, &g_activity, &g_setup, &g_net, &g_idle, &g_level, &g_box, &g_playerA, &g_playerB, &g_sanityA, &g_sanityB, &g_spotA, &g_spotB})
    *o = Obj{};
  put(g_ghost, kInfo, static_cast<void*>(&g_info));
  put(g_info, kBehaviour, kNoGhost);
  put(g_info, kIdentity, g_custom);
  put(g_setup, kDoorOpen, true);
  put(g_level, kFuseBoxField, static_cast<void*>(&g_box));
  put(g_box, kPowerOn, true);
  put(g_idle, kIdleGhost, static_cast<void*>(&g_ghost));
  put(g_playerA, kSanity, static_cast<void*>(&g_sanityA));
  put(g_playerB, kSanity, static_cast<void*>(&g_sanityB));
  put(g_spotA, kSpotPlayer, static_cast<void*>(&g_playerA));
  put(g_spotB, kSpotPlayer, static_cast<void*>(&g_playerB));
  put(g_net, kSpots, fake::list({&g_spotA, &g_spotB}));
  put(g_sanityA, kInsanity, 80.0f);  // a at 20% sanity
  put(g_sanityB, kInsanity, 20.0f);  // b at 80%: the team at 50%
  g_state = kIdleState;
  g_hit = nullptr;
  g_nativeChanges = g_pulses = 0;
  g_lastNative = -1;
  g_chosenState = kLight;
  g_repeatFires = false;
  g_detectedAfterFilter = 99;
  g_lost = g_abilityHeld = g_interactHeld = g_fuseHeld = false;
  g_lostAsks = 0;
  actions::clear();
  actions::tick(&g_ghost);
  actions::g_interact = 2.0f;
}

float charge() { return actions::g_interact; }
bool aim_at_nothing() {
  const float aim[3] = {0, 0, 1};
  return actions::perform(actions::kInteract, aim);
}

// A ghost's own rules may lose an interaction a player has chosen, as they lose a share of its AI's: it does not
// happen and it stays paid for, at what it would have cost. Free, a press could be repeated until one took.
void a_lost_interaction_is_paid_for() {
  start();
  g_hit = &g_aimed;
  g_lost = true;
  CHECK(aim_at_nothing());
  CHECK(g_pulses == 1 && g_lostAsks == 1);
  CHECK(g_nativeChanges == 0);          // the state the chain chose never begins
  CHECK(charge() == 1.0f);              // the light's cost, as if it had happened
  g_lost = false;
  CHECK(aim_at_nothing());
  CHECK(g_nativeChanges == 1 && g_lastNative == kLight && charge() == 0.0f && g_lostAsks == 2);
}

// An aim at something the chain chooses nothing for is no action: nothing to lose, nothing paid, no roll drawn.
void an_aim_that_chooses_nothing_is_not_lost() {
  start();
  g_hit = &g_aimed;
  g_lost = true;
  g_chosenState = kFavouriteState;  // "nothing qualified"
  CHECK(!aim_at_nothing());
  CHECK(g_lostAsks == 0 && charge() == 2.0f && g_nativeChanges == 0);
}

// An aim whose ray hits nothing still pulses, with nothing detected, so a mod's rule on the pulse (the Echo's
// repeat, which needs only its sensor) runs as for the AI; with no such rule nothing happens and nothing is paid.
void an_aim_that_hits_nothing_still_pulses() {
  start();
  g_hit = nullptr;
  CHECK(!aim_at_nothing());
  CHECK(g_pulses == 1 && g_detectedAfterFilter == 0 && g_nativeChanges == 0 && charge() == 2.0f);
  g_repeatFires = true;
  CHECK(aim_at_nothing());
  CHECK(g_pulses == 2 && g_nativeChanges == 1 && g_lastNative == kLight && charge() == 1.0f);
}

// An aim that hits something narrows what the sensor reports to it, as before.
void an_aim_narrows_what_the_sensor_reports() {
  start();
  g_hit = &g_aimed;
  CHECK(aim_at_nothing());
  CHECK(g_detectedAfterFilter == 1);
}

// A rule that silences only the ability (the Echo's while dormant) keeps the ability from being used or offered,
// and leaves interactions and events to it.
void an_ability_can_be_held_alone() {
  start();
  actions::g_abilities = 1;
  const float aim[3] = {0, 0, 1};
  CHECK((actions::status().flags & session::kAbilityAvailable) != 0);
  g_abilityHeld = true;
  CHECK((actions::status().flags & session::kAbilityAvailable) == 0);
  CHECK(!actions::perform(actions::kAbility, aim) && actions::g_abilities == 1);
  actions::g_events = 1;
  CHECK(actions::perform(actions::kEvent, aim) && actions::g_events == 0 && g_lastNative == kEventState);
  g_hit = &g_aimed;
  CHECK(aim_at_nothing());
  g_abilityHeld = false;
  CHECK(actions::perform(actions::kAbility, aim) && actions::g_abilities == 0);
}

// Its AI's fuse box is no interaction: nothing that holds back an interaction holds it (a Kikimora in a lit room, a
// watched Brownie), so a player's is not held either. A rule that does hold its AI's says so of the fuse box
// itself, and then nothing is paid. The game's own rules still stand: a Jinn does not turn the power off.
void player_fuse_not_held_back_like_ai() {
  start();
  const float aim[3] = {0, 0, 1};
  g_interactHeld = true;
  actions::g_fuses = 2;
  CHECK(actions::perform(actions::kFuse, aim));
  CHECK(actions::g_fuses == 1 && g_nativeChanges == 1 && g_lastNative == kFuseState);
  g_interactHeld = false;
  g_fuseHeld = true;
  CHECK(!actions::perform(actions::kFuse, aim));
  CHECK(actions::g_fuses == 1 && g_nativeChanges == 1);
  g_fuseHeld = false;
  put(g_info, kBehaviour, kJinn);
  CHECK(!actions::perform(actions::kFuse, aim));
  CHECK(actions::g_fuses == 1 && g_nativeChanges == 1);
}

// The hunt check measures the Banshee target whatever the type, outside a hunt, as the game's own does: a Mimic that
// has been a Banshee keeps the field through later copies, and nothing clears it. In a hunt, for another type, a
// target is the host's hold and never the check's focus.
void the_hunt_check_measures_what_the_game_measures() {
  start();
  put(g_info, kBehaviour, kSpirit);  // a Mimic's copy after a Banshee: the game's field stays
  put(g_ghost, kBansheeTarget, static_cast<void*>(&g_playerA));
  session::Status s = actions::status();
  CHECK(s.sanity == 20 && (s.flags & session::kFocusSanity));
  CHECK(session::hunt_sanity(s) == 20);
  g_state = kHuntingState;  // the host's hold on another type's hunt: the team's is what the check measures
  put(g_info, kBehaviour, kNoGhost);
  s = actions::status();
  CHECK(session::hunt_sanity(s) == 50);
  put(g_info, kBehaviour, kBanshee);  // the Banshee's own, in or out of a hunt
  s = actions::status();
  CHECK(session::hunt_sanity(s) == 20 && (s.flags & session::kFocusSanity));
  g_state = kIdleState;
  put(g_info, kBehaviour, kSpirit);
  put(g_ghost, kBansheeTarget, static_cast<void*>(nullptr));
  s = actions::status();
  CHECK(session::hunt_sanity(s) == 50 && !(s.flags & session::kFocusSanity));
}

// The idle state's HuntInsanity, as another mod's hook on it adds to it (the Draugr's angered bonus), its detour
// outside this mod's when it started later or inside when it started first.
constexpr int kBonus = 15;
void base_insanity(void*, const void*) { put(g_idle, kIdleInsanity, 35); }
bool g_bonusInside = false;
void inner_insanity(void* idle, const void* m) {
  base_insanity(idle, m);
  if (g_bonusInside) put(g_idle, kIdleInsanity, 35 + kBonus);
}
void idle_execute_with_bonus(void* idle, const void*) {
  using F = void (*)(void*, const void*);
  reinterpret_cast<F>(g_detour.at("GhostIdleState.HuntInsanity"))(idle, nullptr);
  if (!g_bonusInside) put(g_idle, kIdleInsanity, 35 + kBonus);  // the other mod's detour, around this mod's
  g_rollTakenInExecute = actions::roll_taken();
}

// What a hunt's sanity threshold shows does not depend on which mod started first.
void the_hunt_modifier_reads_every_mods_bonus_in_any_order() {
  for (const bool inside : {true, false}) {
    start(&inner_insanity);
    g_bonusInside = inside;
    g_executeBody = &idle_execute_with_bonus;
    put(g_net, kSpots, fake::list({&g_spotA}));  // the team is a, at insanity 35
    put(g_sanityA, kInsanity, 35.0f);
    reinterpret_cast<IdleExecute_t>(g_detour.at(map::GhostIdleState_Execute.key))(&g_idle, nullptr);
    actions::g_huntAvailable = true;
    put(g_sanityA, kInsanity, 36.0f);
    CHECK(actions::g_huntModifier == kBonus);
    actions::tick(&g_ghost);
    CHECK(actions::g_huntAvailable);  // 36 + 15 passes the game's own check: it stays offered
    CHECK(actions::status().huntBelow == tether::kHuntInsanity + kBonus);
  }
}

// The roll is the player's only inside it, while they are not restless and no Tower card boosts it.
void the_roll_is_taken_only_while_the_player_holds_it() {
  start();
  const auto taken_in_a_roll = [] {
    g_rollTakenInExecute = false;
    reinterpret_cast<IdleExecute_t>(g_detour.at(map::GhostIdleState_Execute.key))(&g_idle, nullptr);
    return g_rollTakenInExecute;
  };
  CHECK(!actions::roll_taken());  // outside the roll
  CHECK(taken_in_a_roll());
  actions::g_lastAttempt = GetTickCount64() - tether::kRestlessMs - 1;
  CHECK(!taken_in_a_roll());  // restless: the roll acts on its own
  actions::g_lastAttempt = GetTickCount64();
  put(g_ghost, kBoost, true);
  CHECK(!taken_in_a_roll());  // the Tower card's boost
  put(g_ghost, kBoost, false);
  actions::tick(nullptr);
  CHECK(!taken_in_a_roll());  // nobody steers it
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::chances {
bool create_hooks() { return true; }
int draw(int, int, int r, bool) { return r; }
}  // namespace pm::chances
namespace pm::round {
bool ghost_in_favourite_room() { return false; }
}  // namespace pm::round
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
int actor_number(void*) { return 0; }
bool any_players_in_room(void*) { return false; }
void* ghost_activity(void*) { return &g_activity; }
int ghost_state(void*) { return g_state; }
bool is_child_of(void* transform, void* parent) { return transform == parent; }
void* level_controller() { return &g_level; }
void* level_values() { return nullptr; }
void* network() { return &g_net; }
phasmo::Vector3 position(void*) { return {}; }
void* raycast(phasmo::Vector3, phasmo::Vector3, float) { return g_hit; }
void* setup_phase() { return &g_setup; }
void start_coroutine(void*, void*) {}
void* transform_of(void* o) { return o; }
}  // namespace pm::game

int main() {
  a_lost_interaction_is_paid_for();
  an_aim_that_chooses_nothing_is_not_lost();
  an_aim_that_hits_nothing_still_pulses();
  an_aim_narrows_what_the_sensor_reports();
  an_ability_can_be_held_alone();
  player_fuse_not_held_back_like_ai();
  the_hunt_check_measures_what_the_game_measures();
  the_hunt_modifier_reads_every_mods_bonus_in_any_order();
  the_roll_is_taken_only_while_the_player_holds_it();
  actions::clear();  // what it holds goes back
  std::printf(g_failures ? "%d check(s) failed\n" : "all action tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
