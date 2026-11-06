// Offline tests for the Black Shuck's and the Bogeyman's hunts as mods/custom_ghosts/behaviour.cpp and bogeyman.cpp
// run them, hooks and ticks, over the fake backend: their senses, prey, kill reach, closet search and hiding places
// are the same whoever steers the ghost, its AI or a player, and only its walking differs (the player steers).
// The sources are included so their clock can be the test's. Run: build\hunt_rules_test.exe
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <cmath>
#include <vector>

#include "check.h"
#include "fake_game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "net.h"

namespace {
ULONGLONG g_now = 100000;
ULONGLONG test_clock() { return g_now; }
}  // namespace
#define GetTickCount64 test_clock
#include "../mods/custom_ghosts/behaviour.cpp"
#include "../mods/custom_ghosts/bogeyman.cpp"
#undef GetTickCount64

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace behaviour = pm::custom::behaviour;
namespace bogeyman = pm::custom::bogeyman;
using phasmo::Vector3;
using namespace pm::custom;

constexpr int kIdle = 0, kHunting = 2;
constexpr int32_t kOutsideMap = 1;
// The fake's field offsets.
constexpr int32_t kNetworkSpots = 0x38, kSpotPlayer = 0x38;
constexpr int32_t kHuntGhost = 0x10, kHuntTarget = 0x18, kHuntSearch = 0x20, kHuntLast = 0x28, kHuntSeen = 0x30,
                  kHuntKilled = 0x38, kHuntCloset = 0x40, kHuntDelay = 0x48, kHuntReach = 0x4C, kHuntSlowReach = 0x50;
constexpr int32_t kAiAgent = 0x10, kAiSmudge = 0x18, kAiInfo = 0x20;
constexpr int32_t kPlayerRoom = 0x40, kPlayerCloset = 0x48, kPlayerLos = 0x50;
constexpr int32_t kRoomKind = 0x10;
constexpr int32_t kLevelGhostRoom = 0x10;
constexpr float kReach = 3.0f, kSlowReach = 2.0f;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x78]{};
};
template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}
template <class T>
T got(const Obj& o, int32_t offset) {
  T value{};
  std::memcpy(&value, reinterpret_cast<const char*>(&o) + offset, sizeof value);
  return value;
}

// The game, as these rules read it.
Obj g_ghost, g_agent, g_hunt, g_house, g_a, g_b, g_spotA, g_spotB, g_level, g_net, g_salt, g_closet, g_otherCloset, g_hiding;
std::map<const void*, Vector3> g_at;  // where each object is
int g_state = kIdle;
bool g_master = true;
std::vector<void*> g_gameSees, g_gameHears, g_gameSensesGear;  // what the game's own tests would pass
std::vector<void*> g_salts, g_closets, g_hidings;
float g_roomArea = 0;
void* g_ghostRoom = nullptr;

// What the rules did.
std::vector<void*> g_setTargets;
std::vector<void*> g_checkedClosets;
int g_setDestinations = 0, g_warps = 0;
Vector3 g_lastDestination{};

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

bool sees(void* player) {
  using F = bool (*)(void*, void*, bool, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostHuntState.Sees"))(&g_hunt, player, false, nullptr);
}
bool hears(void* player) {
  using F = bool (*)(void*, void*, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostHuntState.Hears"))(&g_hunt, player, nullptr);
}
bool senses_gear(void* player) {
  using F = bool (*)(void*, void*, const void*);
  return reinterpret_cast<F>(g_detour.at("GhostHuntState.SensesGear"))(&g_hunt, player, nullptr);
}

void game_other(void*, const void*) {}  // a hooked method these rules do not reach here
bool in(const std::vector<void*>& list, void* p) { return std::find(list.begin(), list.end(), p) != list.end(); }
bool game_sees(void*, void* player, bool, const void*) { return in(g_gameSees, player); }
bool game_hears(void*, void* player, const void*) { return in(g_gameHears, player); }
bool game_senses_gear(void*, void* player, const void*) { return in(g_gameSensesGear, player); }
// GhostHuntState.SetTarget: makes the player its target and searches the closet they hide in.
void game_set_target(void* hunt, void* player, const void*) {
  g_setTargets.push_back(player);
  put(*static_cast<Obj*>(hunt), kHuntTarget, player);
  put(*static_cast<Obj*>(hunt), kHuntCloset, got<void*>(*static_cast<Obj*>(player), kPlayerCloset));
}
void game_set_closet(void* hunt, void* closet, const void*) { put(*static_cast<Obj*>(hunt), kHuntCloset, closet); }
bool game_set_destination(void*, const phasmo::Vector3* to, const void*) {
  ++g_setDestinations;
  g_lastDestination = {to->x, to->y, to->z};
  return true;
}
void game_check_closet(void* closet, const void*) { g_checkedClosets.push_back(closet); }
void* array_of(const std::vector<void*>& items) {
  void* array = fake::array(sizeof(void*), items.size());
  void** data = static_cast<void**>(fake::host()->array_data(array));
  for (size_t i = 0; i < items.size(); ++i) data[i] = items[i];
  return array;
}
void* game_find_objects(void* type, const void*) {
  if (type && type == phasmo::SaltSpot::type.type_object()) return array_of(g_salts);
  if (type && type == phasmo::ClosetZone::type.type_object()) return array_of(g_closets);
  if (type && type == phasmo::HidingSpot::type.type_object()) return array_of(g_hidings);
  return nullptr;
}
bool game_sample(const phasmo::Vector3* from, phasmo::NavMeshHit* hit, float, int, const void*) {
  hit->position = *from;
  return true;
}
bool game_warp(void*, const phasmo::Vector3*, const void*) { return ++g_warps > 0; }

bool g_pathEntries = true;  // this world's map resolves all the agent's path needs, not all but its status
// The agent's path to where it is asked: a complete one by these corners, its start first.
bool g_pathKnown = true;
int32_t g_pathStatus = 0;
std::vector<phasmo::Vector3> g_corners;
Vector3 g_pathAskedFor{};
int g_pathAsks = 0;
bool game_calculate_path(void*, const phasmo::Vector3* to, void* path, const void*) {
  ++g_pathAsks;
  g_pathAskedFor = {to->x, to->y, to->z};
  return path && g_pathKnown;
}
int32_t game_path_status(void*, const void*) { return g_pathStatus; }
void* game_path_corners(void*, const void*) {
  void* array = fake::array(sizeof(phasmo::Vector3), g_corners.size());
  if (!g_corners.empty()) std::memcpy(fake::host()->array_data(array), g_corners.data(), g_corners.size() * sizeof(phasmo::Vector3));
  return array;
}
void game_path_ctor(void*, const void*) {}
void give_the_path_entries() {
  static int pathClass;
  fake::type(map::NavMeshPath, &pathClass);
  fake::method(map::NavMeshPath_ctor, reinterpret_cast<void*>(&game_path_ctor));
  if (g_pathEntries) fake::method(map::NavMeshPath_get_status, reinterpret_cast<void*>(&game_path_status));
  fake::method(map::NavMeshPath_get_corners, reinterpret_cast<void*>(&game_path_corners));
  fake::method(map::NavMeshAgent_CalculatePath, reinterpret_cast<void*>(&game_calculate_path));
  fake::value(map::NavMeshPathStatus_PathComplete, 0);
}

int g_saltClass, g_closetClass, g_hidingClass;

void world() {
  fake::reset();
  fake::field(map::GhostHuntState_ghost, kHuntGhost);
  fake::field(map::GhostHuntState_target, kHuntTarget);
  fake::field(map::GhostHuntState_searchTarget, kHuntSearch);
  fake::field(map::GhostHuntState_lastPlayer, kHuntLast);
  fake::field(map::GhostHuntState_seen, kHuntSeen);
  fake::field(map::GhostHuntState_killed, kHuntKilled);
  fake::field(map::GhostHuntState_searchCloset, kHuntCloset);
  fake::field(map::GhostHuntState_startDelay, kHuntDelay);
  fake::field(map::GhostHuntState_killReach, kHuntReach);
  fake::field(map::GhostHuntState_slowKillReach, kHuntSlowReach);
  fake::field(map::GhostAI_agent, kAiAgent);
  fake::field(map::GhostAI_smudge, kAiSmudge);
  fake::field(map::GhostAI_info, kAiInfo);
  fake::field(map::Player_currentRoom, kPlayerRoom);
  fake::field(map::Player_isDead, 0x28);
  fake::field(map::Network_playerSpots, kNetworkSpots);
  fake::field(map::PlayerSpot_player, kSpotPlayer);
  fake::field(map::Player_closet, kPlayerCloset);
  fake::field(map::Player_losTargets, kPlayerLos);
  fake::field(map::LevelRoom_kind, kRoomKind);
  fake::field(map::LevelController_currentGhostRoom, kLevelGhostRoom);
  fake::value(map::RoomKind_outsideMap, kOutsideMap);
  fake::value(map::GhostState_hunting, kHunting);
  fake::type(map::SaltSpot, &g_saltClass);
  fake::type(map::ClosetZone, &g_closetClass);
  fake::type(map::HidingSpot, &g_hidingClass);
  fake::method(map::GhostIdleState_HuntInsanity, reinterpret_cast<void*>(&game_other));
  fake::method(map::GhostWanderState_PickDestination, reinterpret_cast<void*>(&game_other));
  fake::method(map::LevelRoom_Update, reinterpret_cast<void*>(&game_other));
  fake::method(map::GhostHuntState_Sees, reinterpret_cast<void*>(&game_sees));
  fake::method(map::GhostHuntState_Hears, reinterpret_cast<void*>(&game_hears));
  fake::method(map::GhostHuntState_SensesGear, reinterpret_cast<void*>(&game_senses_gear));
  fake::method(map::GhostHuntState_SetTarget, reinterpret_cast<void*>(&game_set_target));
  fake::method(map::GhostHuntState_SetCloset, reinterpret_cast<void*>(&game_set_closet));
  fake::method(map::NavMeshAgent_SetDestination, reinterpret_cast<void*>(&game_set_destination));
  fake::method(map::ClosetZone_CheckCloset, reinterpret_cast<void*>(&game_check_closet));
  fake::method(map::UnityObject_FindObjectsOfType, reinterpret_cast<void*>(&game_find_objects));
  fake::method(map::NavMesh_SamplePosition, reinterpret_cast<void*>(&game_sample));
  fake::method(map::NavMeshAgent_Warp, reinterpret_cast<void*>(&game_warp));
  give_the_path_entries();
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  pm::api::bind(&g_host, missing);
  behaviour::teardown();
  bogeyman::reset();
  CHECK(behaviour::create_hooks());
  for (Obj* o : {&g_ghost, &g_agent, &g_hunt, &g_house, &g_a, &g_b, &g_spotA, &g_spotB, &g_level, &g_net, &g_salt,
                 &g_closet, &g_otherCloset, &g_hiding})
    *o = Obj{};
  g_salt.klass = &g_saltClass;
  g_closet.klass = g_otherCloset.klass = &g_closetClass;
  g_hiding.klass = &g_hidingClass;
  put(g_ghost, kAiAgent, static_cast<void*>(&g_agent));
  put(g_hunt, kHuntGhost, static_cast<void*>(&g_ghost));
  put(g_hunt, kHuntSeen, fake::list({}));
  put(g_hunt, kHuntKilled, fake::list({}));
  put(g_hunt, kHuntDelay, -1.0f);  // past its start delay
  put(g_hunt, kHuntReach, kReach);
  put(g_hunt, kHuntSlowReach, kSlowReach);
  for (Obj* p : {&g_a, &g_b}) {
    put(*p, kPlayerRoom, static_cast<void*>(&g_house));
    put(*p, kPlayerLos, static_cast<void*>(&g_house));  // any non-null array
  }
  put(g_spotA, kSpotPlayer, static_cast<void*>(&g_a));
  put(g_spotB, kSpotPlayer, static_cast<void*>(&g_b));
  put(g_net, kNetworkSpots, fake::list({&g_spotA, &g_spotB}));
  put(g_level, kLevelGhostRoom, static_cast<void*>(&g_house));
  g_at.clear();
  g_at[&g_ghost] = {100, 0, 100};
  g_at[&g_a] = {50, 0, 0};
  g_at[&g_b] = {50, 0, 50};
  g_state = kIdle;
  g_master = true;
  g_gameSees = g_gameHears = g_gameSensesGear = {};
  g_salts = g_closets = g_hidings = {};
  g_roomArea = 0;
  g_ghostRoom = &g_house;
  g_setTargets.clear();
  g_checkedClosets.clear();
  g_setDestinations = g_warps = 0;
  g_pathKnown = true;
  g_pathStatus = 0;
  g_corners.clear();
  g_pathAsks = 0;
  g_now += 3600000;
}

// One frame of the Shuck's rules; the clock moves on by a trail step.
void shuck_frame(bool steered) {
  behaviour::tick(&g_ghost, kBlackShuck, steered);
  g_now += 500;
}
// `player` walks along +x at z from x0 to x1, a step each frame, its trail laid behind it.
void walk(Obj& player, float x0, float x1, float z, bool steered) {
  for (float x = x0; x <= x1 + 1e-3f; x += 0.75f) {
    g_at[&player] = {x, 0, z};
    shuck_frame(steered);
  }
}
void hunt_starts() { g_state = kHunting; }
void* hunt_target() { return got<void*>(g_hunt, kHuntTarget); }

template <class Case>
void for_ai_and_player(Case each) {
  for (const bool steered : {false, true}) {
    world();
    each(steered);
  }
}

// It is blind and deaf, its player's hunt as its AI's: the game's own sight, voice and electronics tests answer for
// nobody; only the player whose trail it walks passes the sight test, and they become the game's target through
// SetTarget, so the game's own chase and kill act on them.
void the_shucks_hunt_is_blind_and_deaf() {
  for_ai_and_player([](bool steered) {
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    g_gameSees = {&g_b};  // what the game's own sight would pass, and a player talking near it with gear on
    g_gameHears = g_gameSensesGear = {&g_b};
    hunt_starts();
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a && !g_setTargets.empty() && g_setTargets.back() == &g_a);
    CHECK(sees(&g_a));   // though the game's own test says no
    CHECK(!sees(&g_b));  // though the game's own test says yes
    CHECK(!hears(&g_b) && !senses_gear(&g_b) && !hears(&g_a) && !senses_gear(&g_a));
    // The seen list is the sight test's, as the game's own keeps it.
    CHECK(fake::list_items_of(got<void*>(g_hunt, kHuntSeen)) == std::vector<void*>{&g_a});
  });
}

// With no scent in reach it has no prey: nobody passes, its AI's hunt and a player's alike.
void with_no_scent_nobody_passes_its_tests() {
  for_ai_and_player([](bool steered) {
    g_at[&g_ghost] = {30, 0, 30};
    g_gameSees = g_gameHears = g_gameSensesGear = {&g_a, &g_b};
    hunt_starts();
    shuck_frame(steered);
    CHECK(g_setTargets.empty() && !hunt_target());
    CHECK(!sees(&g_a) && !sees(&g_b) && !hears(&g_a) && !senses_gear(&g_b));
  });
}

// Only its AI's walking is its own: a player's Shuck is sent nowhere, and the game's own SetDestination stands.
void only_its_ai_is_walked() {
  for_ai_and_player([](bool steered) {
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    hunt_starts();
    g_setDestinations = 0;
    shuck_frame(steered);
    CHECK(steered ? g_setDestinations == 0 : g_setDestinations == 1);
    const phasmo::Vector3 asked{40, 0, 40};
    using F = bool (*)(void*, const phasmo::Vector3*, const void*);
    reinterpret_cast<F>(g_detour.at("NavMeshAgent.SetDestination"))(&g_agent, &asked, nullptr);
    CHECK(g_lastDestination.x == (steered ? 40.0f : g_lastDestination.x));  // the player's agent: the game's call
    CHECK(steered || g_lastDestination.x < 10.0f);                          // its AI's: on along the trail
  });
}

// The hunt's unforced kill reaches only a prey the game's own vision sensor sees, for its AI and a player alike: a
// distance alone spans a thin wall, and the Shuck's sight test passes wherever its prey is.
void the_kill_reaches_only_a_prey_it_sees() {
  for_ai_and_player([](bool steered) {
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    hunt_starts();
    shuck_frame(steered);  // the game's own sight does not see them
    CHECK(got<float>(g_hunt, kHuntReach) == 0.0f && got<float>(g_hunt, kHuntSlowReach) == 0.0f);
    g_gameSees = {&g_a};
    shuck_frame(steered);
    CHECK(got<float>(g_hunt, kHuntReach) == kReach && got<float>(g_hunt, kHuntSlowReach) == kSlowReach);
    g_gameSees = {};
    shuck_frame(steered);
    CHECK(got<float>(g_hunt, kHuntReach) == 0.0f);
    g_state = kIdle;  // the hunt ends: the reach goes back
    shuck_frame(steered);
    CHECK(got<float>(g_hunt, kHuntReach) == kReach);
  });
}

// A trail that ends at a hiding place hands them to the game's own closet search: SetTarget gives it the closet,
// and they then pass no sight test, so it finds them unseen. The same for its AI and a player.
void a_hidden_prey_goes_to_the_games_closet_search() {
  for_ai_and_player([](bool steered) {
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {4.5f, 0, 0.2f};
    hunt_starts();
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
    put(g_a, kPlayerCloset, static_cast<void*>(&g_closet));
    g_at[&g_ghost] = {5.5f, 0, 0.2f};  // within its search's reach of them
    shuck_frame(steered);
    CHECK(got<void*>(g_hunt, kHuntCloset) == &g_closet);
    CHECK(!sees(&g_a));
    // Its unforced kill is not held for this: the search's own kill measures no reach.
    CHECK(got<float>(g_hunt, kHuntReach) == kReach);
  });
}

// Salt across its prey's trail loses them, and it ignores their trail for 30 s, whoever steers: the hunt forgets
// them, and a player's corridor (below) no longer runs along that trail.
void a_cut_trail_is_ignored_for_thirty_seconds() {
  for_ai_and_player([](bool steered) {
    walk(g_a, 0, 9, 0, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    hunt_starts();
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
    g_salts = {&g_salt};
    g_at[&g_salt] = {6, 0, 0};
    g_now += 500;                  // a trail step falls due: it reads the breaks
    behaviour::tick(&g_ghost, kBlackShuck, steered);
    CHECK(!hunt_target() && !sees(&g_a));
    g_salts = {};                  // swept away: the trail is whole again, and still ignored
    shuck_frame(steered);
    CHECK(!hunt_target() && g_setTargets.size() == 1 && !sees(&g_a));
    // At the edge of the corridor it would be held to; ignored, the player steers freely.
    const float at[3] = {3, 0, 0.8f};
    float dx = 0, dz = 1;
    behaviour::player_move(at, &dx, &dz);
    CHECK(dx == 0.0f && dz == 1.0f);
    // Thirty seconds on it may pick them up again, on a trail laid since.
    g_now += trail::kTrailMs + 1000;
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
    dx = 0;
    dz = 1;
    const float edge[3] = {3, 0, 0.8f};
    behaviour::player_move(edge, &dx, &dz);
    CHECK(dx == 0.0f && dz == 0.0f);  // the corridor holds again
  });
}

// A player's Shuck takes as prey the player whose trail it walks: the one whose corridor it stands in, and it keeps
// them while it may. Its AI keeps its prey until it loses them.
void a_players_shuck_takes_the_trail_it_walks() {
  world();
  walk(g_a, 0, 6, 0, true);
  walk(g_b, 0, 6, 4, true);
  hunt_starts();
  g_at[&g_ghost] = {3, 0, 0.5f};  // on a's trail
  shuck_frame(true);
  CHECK(hunt_target() == &g_a);
  g_at[&g_ghost] = {3, 0, 2.0f};  // between them, in neither corridor: it keeps a
  shuck_frame(true);
  CHECK(hunt_target() == &g_a);
  g_at[&g_ghost] = {3, 0, 3.5f};  // on b's trail: b is its prey now
  shuck_frame(true);
  CHECK(hunt_target() == &g_b && sees(&g_b) && !sees(&g_a));
  // Its AI, on the same walk, keeps a while it can follow them.
  world();
  walk(g_a, 0, 6, 0, false);
  walk(g_b, 0, 6, 4, false);
  hunt_starts();
  g_at[&g_ghost] = {3, 0, 0.5f};
  shuck_frame(false);
  CHECK(hunt_target() == &g_a);
  g_at[&g_ghost] = {3, 0, 3.5f};
  shuck_frame(false);
  CHECK(hunt_target() == &g_a);
}

// Its player's steering follows the rule its AI's hunt keeps to: on scent, the corridor; with none reaching it, as
// its AI then roams, free.
void the_corridor_uses_the_ais_scent() {
  world();
  walk(g_a, 0, 6, 0, true);
  hunt_starts();
  shuck_frame(true);
  float dx = 0, dz = 1;
  const float edge[3] = {3, 0, 0.8f};
  behaviour::player_move(edge, &dx, &dz);
  CHECK(dx == 0.0f && dz == 0.0f);
  g_state = kIdle;  // outside a hunt it walks freely
  dx = 0;
  dz = 1;
  behaviour::player_move(edge, &dx, &dz);
  CHECK(dx == 0.0f && dz == 1.0f);
}

// A wall between a player's Shuck and the trail: "nearer" is along the path its agent would walk to the step its
// scent reaches, as the game's own wander asks for one. With no complete path, or no way to ask, it steers freely.
void player_shuck_not_stalled_by_wall() {
  world();
  walk(g_a, 0, 6, 0, true);
  hunt_starts();
  const float room[3] = {3, 0, 3};
  g_at[&g_ghost] = {3, 0, 3};
  shuck_frame(true);
  g_corners = {{3, 0, 3}, {8, 0, 2}, {6, 0, 0}};  // round by the door
  float dx = 1, dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == 1.0f && dz == 0.0f);  // towards the door, no nearer by the straight line
  CHECK(g_pathAsks == 1 && g_pathAskedFor.x == 3.0f && g_pathAskedFor.z == 0.0f);  // the step its scent reaches
  dx = -1;
  dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == 0.0f && dz == 0.0f);  // away from it
  // The agent cannot make a path, or makes only part of one, or has only its own place: free.
  g_pathKnown = false;
  g_now += trail::kWayMaxAgeMs;
  dx = -1;
  dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == -1.0f && dz == 0.0f);
  g_pathKnown = true;
  g_pathStatus = 1;
  g_now += trail::kWayMaxAgeMs;
  dx = -1;
  dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == -1.0f && dz == 0.0f);
  g_pathStatus = 0;
  g_corners = {{3, 0, 3}};
  g_now += trail::kWayMaxAgeMs;
  dx = -1;
  dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == -1.0f && dz == 0.0f);
  // A map missing one of the path's entries (its status, which would read as complete): it fails closed to free
  // steering, and asks nothing of the game.
  g_pathEntries = false;
  world();
  g_pathEntries = true;
  walk(g_a, 0, 6, 0, true);
  hunt_starts();
  g_at[&g_ghost] = {3, 0, 3};
  shuck_frame(true);
  dx = -1;
  dz = 0;
  behaviour::player_move(room, &dx, &dz);
  CHECK(dx == -1.0f && dz == 0.0f && g_pathAsks == 0);
}

// Salt in the doorway its agent's way goes through holds neither: its AI's agent goes on to the trail, and a player
// walks on towards that way's next corner. Its prey is its prey all the while.
void salt_on_its_agents_way_holds_neither() {
  const auto set_up = [](bool steered) {
    world();
    walk(g_a, 0, 6, 0, steered);
    hunt_starts();
    g_salts = {&g_salt};
    g_at[&g_salt] = {5, 0, 2.6f};
    g_at[&g_ghost] = {4.3f, 0, 2.7f};
    g_corners = {{4.3f, 0, 2.7f}, {8, 0, 2}, {4.5f, 0, 0}};
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
  };
  set_up(false);
  CHECK(g_setDestinations > 0 && g_lastDestination.z == 0.0f);
  set_up(true);
  const float at[3] = {4.3f, 0, 2.7f};
  const float n = std::hypot(8 - at[0], 2 - at[2]);
  const float wantX = (8 - at[0]) / n, wantZ = (2 - at[2]) / n;
  float dx = wantX, dz = wantZ;
  behaviour::player_move(at, &dx, &dz);
  CHECK(dx == wantX && dz == wantZ);
  dx = -wantX;  // nearer means along its way, salt or no salt
  dz = -wantZ;
  behaviour::player_move(at, &dx, &dz);
  CHECK(dx == 0.0f && dz == 0.0f);
}

// The agent's path is worked out on the main thread, so a frame that would get the same answer does not ask. A walk,
// another step or the time a door could shut in do.
void the_way_is_asked_only_when_it_may_have_changed() {
  world();
  walk(g_a, 0, 6, 0, true);
  hunt_starts();
  g_at[&g_ghost] = {3, 0, 3};
  shuck_frame(true);
  g_corners = {{3, 0, 3}, {8, 0, 2}, {6, 0, 0}};
  g_pathAsks = 0;
  const auto steer = [](float x) {
    const float at[3] = {x, 0, 3};
    float dx = 1, dz = 0;
    behaviour::player_move(at, &dx, &dz);
    CHECK(dx == 1.0f && dz == 0.0f);
  };
  for (int frame = 0; frame < 5; ++frame) {
    steer(3);
    g_now += 16;
  }
  CHECK(g_pathAsks == 1);
  steer(3 + trail::kWayMoved);
  CHECK(g_pathAsks == 2);
  g_now += trail::kWayMaxAgeMs;
  steer(3 + trail::kWayMoved);
  CHECK(g_pathAsks == 3);
  steer(3 + trail::kWayMoved);
  CHECK(g_pathAsks == 3);
}

// The scent its player sees is what the hunt goes by, and they do see it. It leaves out a trail the hunt ignores,
// a player it may not go for, and steps its scent does not reach.
void the_scent_view_shows_what_the_hunt_follows() {
  const auto view = [] {
    float xyz[3 * 64];
    const int n = behaviour::scent(xyz, 64);
    return std::vector<float>(xyz, xyz + 3 * n);
  };
  const auto all_on = [](const std::vector<float>& xyz, float z) {
    for (size_t i = 2; i < xyz.size(); i += 3)
      if (xyz[i] != z) return false;
    return true;
  };
  for_ai_and_player([&](bool steered) {
    CHECK(view().empty());  // no hunt: it follows scent in hunts
    walk(g_a, 0, 6, 0, steered);
    walk(g_b, 0, 6, 4, steered);
    g_at[&g_ghost] = {3, 0, 0.2f};
    hunt_starts();
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
    const std::vector<float> both = view();
    CHECK(!both.empty() && !all_on(both, 0) && !all_on(both, 4));
    // Salt across its prey's trail: the hunt loses them and ignores their trail, so the view shows only the other's.
    g_salts = {&g_salt};
    g_at[&g_salt] = {6, 0, 0};
    g_now += 500;
    behaviour::tick(&g_ghost, kBlackShuck, steered);
    CHECK(!hunt_target());
    const std::vector<float> left = view();
    CHECK(!left.empty() && all_on(left, 4));
    g_salts = {};  // swept away, and still ignored
    shuck_frame(steered);
    CHECK(all_on(view(), 4));
    put(g_b, 0x28, true);  // dead: their trail is no trail
    shuck_frame(steered);
    CHECK(view().empty());
  });
  // Salt beside the trail leaves it whole, but scent does not reach past it from here: those steps do not show.
  for_ai_and_player([&](bool steered) {
    walk(g_a, 0, 6, 0, steered);
    g_at[&g_ghost] = {0, 0, 1};
    hunt_starts();
    g_salts = {&g_salt};
    g_at[&g_salt] = {3, 0, 0.7f};
    shuck_frame(steered);
    CHECK(hunt_target() == &g_a);
    const std::vector<float> seen = view();
    CHECK(!seen.empty());
    for (size_t i = 0; i < seen.size(); i += 3) CHECK(seen[i] <= 3.0f);
  });
}

// The Bogeyman, in the sight test: it sees as the game does, but only its target, and never beyond its short sight
// in an open room, its AI's hunt and a player's alike.
void the_bogeyman_sees_as_the_game_does_within_its_short_sight() {
  for_ai_and_player([](bool steered) {
    g_roomArea = 40;  // an open room
    g_at[&g_ghost] = {0, 0, 0};
    g_at[&g_a] = {3, 0, 0};
    g_at[&g_b] = {20, 0, 0};
    hunt_starts();
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    behaviour::tick(&g_ghost, kBogeyman, steered);
    CHECK(bogeyman::in_open_room());
    g_gameSees = {&g_a, &g_b};
    CHECK(sees(&g_a));
    CHECK(!sees(&g_b));  // beyond its short sight
    g_gameSees = {};
    CHECK(!sees(&g_a));  // the game's own test says no
    g_roomArea = 4;      // a small room: its sight is the game's
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    behaviour::tick(&g_ghost, kBogeyman, steered);
    g_gameSees = {&g_b};
    CHECK(sees(&g_b));
  });
}

void place(Obj& spot, Vector3 at) { g_at[&spot] = at; }

// With no target a Bogeyman searches a hiding place it reaches, closets through the game's own CheckCloset, each once
// per hunt, for its AI and a player alike. The AI walks there itself; a player's route is the player's.
void the_bogeyman_searches_what_it_reaches() {
  for_ai_and_player([](bool steered) {
    g_closets = {&g_closet};
    g_hidings = {&g_hiding};
    place(g_closet, {5, 0, 0});
    place(g_hiding, {0, 0, 5});
    g_at[&g_ghost] = {0, 0, 0};
    hunt_starts();
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    CHECK(g_checkedClosets.empty());
    CHECK(steered ? g_setDestinations == 0 : g_setDestinations > 0);
    const int walked = g_setDestinations;
    g_at[&g_ghost] = {5, 0, 0};  // it has reached the closet
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    CHECK(g_checkedClosets == std::vector<void*>{&g_closet});
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    CHECK(g_checkedClosets.size() == 1);  // once per hunt
    if (steered) CHECK(g_setDestinations == walked);
    // A hiding spot is searched by walking to it, with no closet to check.
    g_at[&g_ghost] = {0, 0, 5};
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    CHECK(g_checkedClosets.size() == 1);
    // With a target it chases: nothing is searched, whoever steers.
    g_state = kIdle;
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);  // every place again next hunt
    hunt_starts();
    put(g_hunt, kHuntTarget, static_cast<void*>(&g_a));
    g_at[&g_a] = {6, 0, 0};
    g_at[&g_ghost] = {5, 0, 0};
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);
    CHECK(g_checkedClosets.size() == 1);
    // Its target out of its short sight in an open room is lost, and then it searches.
    put(g_hunt, kHuntTarget, static_cast<void*>(nullptr));
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);  // its AI sets out for it
    g_now += 100;
    bogeyman::tick(&g_ghost, kBogeyman, steered);  // and is there
    CHECK(g_checkedClosets.size() == 2);
  });
}

// The Draugr's target is asked on the host alone, which keeps its things; off the host it names nobody, and no client
// is told who the thief is.
void the_draugr_names_its_target_on_the_host_alone() {
  world();
  behaviour::tick(&g_ghost, kDraugr, false);
  CHECK(!behaviour::target(kDraugr, {}));  // calm
  g_master = false;
  CHECK(!behaviour::target(kDraugr, {}));
  // The word the clients hear is the anger alone.
  bool angered = true;
  const std::vector<uint8_t> calm = net_msg::anger(false);
  CHECK(calm.size() == 5 && net_msg::read_anger(calm.data(), calm.size(), angered) && !angered);
}

// A hunt's start asks where it steps out before the rules have had a frame: a Mimic's copy made at the hunt's own
// change is a Bogeyman at once, with its hiding places looked for there and then.
void a_new_bogeymans_first_hunt_finds_its_closet() {
  world();
  g_closets = {&g_closet};
  place(g_closet, {5, 0, 0});
  g_at[&g_a] = {6, 0, 0};
  g_at[&g_b] = {60, 0, 0};
  g_state = kHunting;  // the game has just made the hunt: its first tick has not run
  bogeyman::hunt_begins(&g_ghost);
  CHECK(g_warps == 1);
}

}  // namespace

// What these rules call of the game.
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
Vector3 forward(void*) { return {0, 0, 1}; }
void* game_object_of(void* o) { return o; }
void* ghost_activity(void*) { return nullptr; }
void* ghost_agent(void*) { return &g_agent; }
int ghost_state(void*) { return g_state; }
void* ghost_state_object(void*) { return g_state == kHunting ? &g_hunt : nullptr; }
bool in_room(void*, Vector3, float) { return false; }
bool is_master() { return g_master; }
void* level_controller() { return &g_level; }
int master_actor() { return 1; }
void* network() { return &g_net; }
Vector3 position(void* transform) {
  const auto it = g_at.find(transform);
  return it == g_at.end() ? Vector3{} : it->second;
}
std::vector<Bounds> room_bounds(void*) {
  const float half = std::sqrt(g_roomArea) / 2;
  return {{{}, {half, 1, half}}};
}
float distance_to_room(void* room, Vector3 p) {  // as core/game.cpp, over the room above
  float best = 1e9f;
  for (const Bounds& b : room_bounds(room)) {
    const float dx = std::max(0.0f, std::fabs(p.x - b.center.x) - b.extents.x);
    const float dy = std::max(0.0f, std::fabs(p.y - b.center.y) - b.extents.y);
    const float dz = std::max(0.0f, std::fabs(p.z - b.center.z) - b.extents.z);
    best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return best;
}
bool sensor_detects(void*, void*) { return false; }
bool set_destination(void*, Vector3 to) {
  ++g_setDestinations;
  g_lastDestination = to;
  return true;
}
void* transform_of(void* o) { return o; }
}  // namespace pm::game
// The rules that have nothing to do with these two ghosts.
namespace pm::custom {
namespace asterion {
bool battering() { return false; }
}  // namespace asterion
namespace brownie {
bool available() { return false; }
bool seen() { return false; }
}  // namespace brownie
namespace tempest {
bool available() { return false; }
bool striking() { return false; }
}  // namespace tempest
namespace carry {
bool Carry::take(void*, phasmo::Vector3, ULONGLONG) { return false; }
bool Carry::take_dragging(void*, phasmo::Vector3, ULONGLONG, float) { return false; }
Carry::Step Carry::step(ULONGLONG, Still) { return Step::Idle; }
void Carry::reset() {}
}  // namespace carry
}  // namespace pm::custom
namespace pm::net {
bool send(const uint8_t*, size_t, To, bool, int) { return true; }
}  // namespace pm::net

int main() {
  the_shucks_hunt_is_blind_and_deaf();
  with_no_scent_nobody_passes_its_tests();
  only_its_ai_is_walked();
  the_kill_reaches_only_a_prey_it_sees();
  a_hidden_prey_goes_to_the_games_closet_search();
  a_cut_trail_is_ignored_for_thirty_seconds();
  a_players_shuck_takes_the_trail_it_walks();
  the_corridor_uses_the_ais_scent();
  salt_on_its_agents_way_holds_neither();
  the_way_is_asked_only_when_it_may_have_changed();
  the_scent_view_shows_what_the_hunt_follows();
  player_shuck_not_stalled_by_wall();
  the_bogeyman_sees_as_the_game_does_within_its_short_sight();
  the_bogeyman_searches_what_it_reaches();
  a_new_bogeymans_first_hunt_finds_its_closet();
  the_draugr_names_its_target_on_the_host_alone();
  behaviour::teardown();  // what they hold goes back before the fake's own storage does
  bogeyman::reset();
  std::printf(g_failures ? "%d check(s) failed\n" : "all hunt rule tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
