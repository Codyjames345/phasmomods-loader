// Offline tests for moving the ghost over the fake game: the host's steering (host/steering.cpp), its real stub patched
// by the real hooks layer at a copy of the map's UnityPlayer.SteerSite and run the way the crowd update reaches it, and
// what the ghost's state objects say about its walk (mods/playable_ghost/walk_state.cpp). Run: build\movement_test.exe
#include <windows.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include "GhostHuntState.h"
#include "GhostWanderState.h"
#include "UnityPlayer.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "hooks.h"
#include "steering.h"
#include "walk_state.h"

extern "C" void pm_steer_run(void* site, void* agent, float* x, float* y, float* z, float* speed, int frameSlot);
extern "C" unsigned char pm_steer_begin[], pm_steer_end[];

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;
namespace steering = pm::steering;

// A code page holding `bytes`, and straight after them the code the host's jump back resumes at, here a return to the
// harness. As with the game's code, nothing writes it without changing its protection.
unsigned char* code_page(const unsigned char* bytes, size_t size) {
  auto* at = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
  std::memcpy(at, bytes, size);
  at[size] = 0xC3;  // ret
  DWORD old = 0;
  VirtualProtect(at, 0x1000, PAGE_EXECUTE_READ, &old);
  return at;
}

// The site as the game build holds it, whose first bytes the map's pattern pins.
unsigned char* site() {
  static unsigned char* const code = code_page(steering::kReplaced.data(), steering::kReplaced.size());
  return code;
}

// The site as another build holds it: not the bytes the map pins.
unsigned char* other_build() {
  static const unsigned char kOther[8] = {0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90, 0x90};
  static unsigned char* const code = code_page(kOther, sizeof kOther);
  return code;
}

bool site_intact() { return std::memcmp(site(), steering::kReplaced.data(), steering::kReplaced.size()) == 0; }

// Where the jump at the site leads: the page the host put the stub on, or null with no jump there.
unsigned char* stub_page() {
  if (site()[0] != 0xE9) return nullptr;
  int32_t distance = 0;
  std::memcpy(&distance, site() + 1, sizeof distance);
  return site() + 5 + distance;
}

// A CrowdAgent, big enough for the replaced instruction's read: its displacement is the instruction's last four
// bytes, and the speed lies there.
struct Agent {
  unsigned char bytes[0x1000] = {};
  explicit Agent(float speed) { std::memcpy(bytes + displacement(), &speed, sizeof speed); }
  static int32_t displacement() {
    int32_t at = 0;
    std::memcpy(&at, steering::kReplaced.data() + 4, sizeof at);
    return at;
  }
};

// A managed NavMeshAgent, as far as the host reads one: its class first, as a real object's, and its native object at
// the offset give_entries names.
char g_agentClass, g_notAnAgentClass;
struct Managed {
  void* klass;
  void* monitor;
  void* cachedPtr;
};

// A state object: the class pointer first, as a real one, then room for its fields. The offsets are not the game's:
// nothing here may know them.
struct StateObject {
  void* klass;
  unsigned char bytes[0x80] = {};
  template <class T>
  void put(int32_t offset, T value) {
    std::memcpy(reinterpret_cast<char*>(this) + offset, &value, sizeof value);
  }
};
char g_huntClass, g_wanderClass, g_dotsClass, g_otherClass, g_playerClass;
constexpr int32_t kHuntTarget = 0x18, kHuntSearch = 0x28;
constexpr int32_t kWanderRadius = 0x10, kWanderHas = 0x14, kWanderPoint = 0x18;
constexpr int32_t kDotsRadius = 0x30, kDotsHas = 0x34, kDotsPoint = 0x38;

// The native function the map names: from a native agent, the CrowdAgent the crowd steers.
int g_nativeObject = 0;
void* g_nativeAgent = nullptr;
void* g_crowdAgent = nullptr;
int g_internalCalls = 0;
void* fake_internal_agent(void* native) {
  g_nativeAgent = native;
  ++g_internalCalls;
  return g_crowdAgent;
}

void give_entries() {
  fake::reset();
  fake::native_site(map::UnityPlayer_SteerSite, site());
  fake::native_site(map::UnityPlayer_GetInternalAgent, reinterpret_cast<void*>(&fake_internal_agent));
  fake::field(map::UnityObject_m_CachedPtr, static_cast<int32_t>(offsetof(Managed, cachedPtr)));
  fake::type(map::NavMeshAgent, &g_agentClass);
  fake::type(map::GhostHuntState, &g_huntClass);
  fake::field(map::GhostHuntState_target, kHuntTarget);
  fake::field(map::GhostHuntState_searchTarget, kHuntSearch);
  fake::type(map::GhostWanderState, &g_wanderClass);
  fake::field(map::GhostWanderState_radius, kWanderRadius);
  fake::field(map::GhostWanderState_hasDestination, kWanderHas);
  fake::field(map::GhostWanderState_destination, kWanderPoint);
  fake::type(map::GhostDotsState, &g_dotsClass);
  fake::field(map::GhostDotsState_radius, kDotsRadius);
  fake::field(map::GhostDotsState_hasDestination, kDotsHas);
  fake::field(map::GhostDotsState_destination, kDotsPoint);
}

bool bind() {
  std::string missing;
  return api::bind(fake::host(), missing) && missing.empty();
}

// The site back as the game holds it, every entry resolved, and the mods starting afresh.
void start() {
  pm::hooks::rollback_to(0);
  CHECK(site_intact());
  give_entries();
  CHECK(bind());
  steering::reset();
}

struct Read {
  float x, y, z, speed;
};

// One pass of the crowd update over `agent`, through the site itself, with the direction (1, 2, 3) and the speed from
// the agent.
Read crowd_update(void* agent) {
  Read r{1, 2, 3, 0};
  pm_steer_run(site(), agent, &r.x, &r.y, &r.z, &r.speed, steering::kDirectionZ);
  return r;
}

bool native(const Read& r, float speed) { return r.x == 1 && r.y == 2 && r.z == 3 && r.speed == speed; }

// The pattern's bytes as written in the map, read here at run time, -1 for a wildcard: no value here is the game's.
std::vector<int> pattern_bytes() {
  std::vector<int> out;
  std::istringstream in(map::UnityPlayer_SteerSite.pattern);
  for (std::string token; in >> token;) out.push_back(token == "??" ? -1 : std::stoi(token, nullptr, 16));
  return out;
}

// The bytes the host patches and the frame slot its stub writes are the map's pattern's own: its first 8 bytes, and its
// byte 17, the third instruction's displacement. The static_asserts in steering.h hold the instructions' forms.
void the_patch_reads_its_bytes_from_the_map() {
  const std::vector<int> pattern = pattern_bytes();
  CHECK(pattern.size() > 17);
  if (pattern.size() <= 17) return;
  for (size_t i = 0; i < steering::kReplaced.size(); ++i) CHECK(steering::kReplaced[i] == pattern[i]);
  CHECK(steering::kDirectionZ == pattern[17]);
}

// Asking puts the jump over the site. With nobody steering, the stub runs the replaced instruction and goes back, so the
// crowd update reads what it would have read. The block holds no game byte of its own: they are the map's, written into
// the copy the host patches with.
void the_stub_stays_passive_until_asked() {
  start();
  CHECK(site_intact());  // nothing is patched until something asks
  steering::expire();    // nothing to end yet
  CHECK(steering::steer(nullptr, 0, 0));
  CHECK(!site_intact() && stub_page() != nullptr);
  if (!stub_page()) return;
  Agent mine(2.5f);
  CHECK(native(crowd_update(mine.bytes), 2.5f));
  CHECK(std::memcmp(pm_steer_begin, stub_page(), static_cast<size_t>(pm_steer_end - pm_steer_begin)) != 0);
}

// Only the asked agent's direction changes; its speed stays native, and Y is zeroed because a ghost walks on the level.
// It lasts through the crowd updates until the host opens the next frame.
void only_the_asked_agents_direction_is_replaced_this_frame() {
  start();
  Agent mine(2.5f), other(4.0f);
  Managed managed{&g_agentClass, nullptr, &g_nativeObject};
  g_crowdAgent = mine.bytes;
  CHECK(steering::steer(&managed, 0.6f, -0.8f));
  CHECK(g_nativeAgent == &g_nativeObject);
  const Read steered = crowd_update(mine.bytes);
  CHECK(steered.x == 0.6f && steered.y == 0.0f && steered.z == -0.8f && steered.speed == 2.5f);
  CHECK(crowd_update(mine.bytes).x == 0.6f);
  CHECK(native(crowd_update(other.bytes), 4.0f));
  steering::expire();  // nobody asked again: the game's own direction
  CHECK(native(crowd_update(mine.bytes), 2.5f));
  CHECK(steering::steer(&managed, 0, 0));  // a zero direction: it holds still, at its native deceleration
  const Read still = crowd_update(mine.bytes);
  CHECK(still.x == 0 && still.y == 0 && still.z == 0 && still.speed == 2.5f);
  CHECK(steering::steer(&managed, 3, 3));  // not a direction: nothing steers, the earlier ask included
  CHECK(native(crowd_update(mine.bytes), 2.5f));
  CHECK(steering::steer(&managed, 1, 0));  // until a valid ask after it, in the same frame: the last valid ask counts
  const Read after = crowd_update(mine.bytes);
  CHECK(after.x == 1 && after.y == 0 && after.z == 0 && after.speed == 2.5f);
  CHECK(steering::steer(&managed, std::nanf(""), 0));
  CHECK(native(crowd_update(mine.bytes), 2.5f));
  steering::expire();
}

// An object that is not a live NavMeshAgent changes nothing, and the engine's function is never given what it holds:
// another class's object (a GhostAI handed in by mistake), and one destroyed since. An earlier ask of the frame stands.
void only_a_live_nav_mesh_agent_is_steered() {
  start();
  Agent mine(2.5f);
  Managed agent{&g_agentClass, nullptr, &g_nativeObject};
  g_crowdAgent = mine.bytes;
  CHECK(steering::steer(&agent, 1, 0));
  g_internalCalls = 0;
  Managed notAnAgent{&g_notAnAgentClass, nullptr, &g_nativeObject};
  CHECK(steering::steer(&notAnAgent, 0, 1));
  Managed destroyed{&g_agentClass, nullptr, &g_nativeObject};
  fake::destroy(&destroyed);
  CHECK(steering::steer(&destroyed, 0, 1));
  CHECK(g_internalCalls == 0);
  const Read read = crowd_update(mine.bytes);
  CHECK(read.x == 1 && read.z == 0);
  steering::expire();
}

// The host turns a managed agent into its CrowdAgent through the map. A null or destroyed agent asks for nothing, and an
// earlier ask of the frame stands.
void a_destroyed_agent_is_not_steered() {
  start();
  Agent mine(2.5f);
  Managed managed{&g_agentClass, nullptr, nullptr};
  g_internalCalls = 0;
  CHECK(steering::steer(&managed, 1, 0));
  CHECK(steering::steer(nullptr, 1, 0));
  CHECK(g_internalCalls == 0 && native(crowd_update(mine.bytes), 2.5f));
  managed.cachedPtr = &g_nativeObject;
  g_crowdAgent = mine.bytes;
  CHECK(steering::steer(&managed, 1, 0));
  managed.cachedPtr = nullptr;
  CHECK(steering::steer(&managed, 0, 1));
  const Read read = crowd_update(mine.bytes);
  CHECK(read.x == 1 && read.z == 0);
  managed.cachedPtr = &g_nativeObject;
  g_crowdAgent = nullptr;  // a native agent the crowd has no entry for
  CHECK(steering::steer(&managed, 0, 1));
  const Read again = crowd_update(mine.bytes);
  CHECK(again.x == 1 && again.z == 0);
  steering::expire();
}

// One of the host's steering entries did not resolve in this build: the effect says so and names it, and the site stays
// as it is. Each one counts. (That a mod requires none of them is game_api_test's to show: this binary is the host's.)
// The patch in place proves nothing about the entries the managed agent's way to its CrowdAgent reads.
void an_entry_gone_after_the_patch_steers_nothing() {
  for (const pm::sdk::EntryDesc* lacking : {&map::UnityObject_m_CachedPtr, &map::UnityPlayer_GetInternalAgent}) {
    pm::hooks::rollback_to(0);
    start();
    Agent mine(2.5f);
    Managed agent{&g_agentClass, nullptr, &g_nativeObject};
    g_crowdAgent = mine.bytes;
    CHECK(steering::steer(&agent, 1, 0));
    steering::expire();
    fake::forget(*lacking);
    g_internalCalls = 0;
    CHECK(steering::steer(&agent, 0, 1));
    CHECK(g_internalCalls == 0 && native(crowd_update(mine.bytes), 2.5f));
  }
  // The host and the offline check judge an entry alike: a native site needs its address, the others only to resolve.
  CHECK(map::steering_entry_usable(map::UnityPlayer_SteerSite, true, true));
  CHECK(!map::steering_entry_usable(map::UnityPlayer_SteerSite, true, false));
  CHECK(!map::steering_entry_usable(map::UnityPlayer_SteerSite, false, true));
  CHECK(map::steering_entry_usable(map::NavMeshAgent, true, false));
  CHECK(!map::steering_entry_usable(map::NavMeshAgent, false, false));
  pm::hooks::rollback_to(0);
  give_entries();
  fake::native_site(map::UnityPlayer_SteerSite, nullptr);
  bind();
  steering::reset();
  CHECK(steering::missing() == map::UnityPlayer_SteerSite.key);
  give_entries();
  bind();
}

void an_entry_that_did_not_resolve_makes_it_unavailable() {
  for (const pm::sdk::EntryDesc* lacking : map::kSteeringEntries) {
    pm::hooks::rollback_to(0);
    give_entries();
    fake::forget(*lacking);
    bind();
    steering::reset();
    CHECK(steering::missing() == lacking->key);
    CHECK(!steering::steer(nullptr, 0, 0));
    CHECK(site_intact());
  }
  // The type every agent is checked against is among them: without it nothing may steer, since nothing could be told
  // apart from a NavMeshAgent.
  pm::hooks::rollback_to(0);
  give_entries();
  fake::forget(map::NavMeshAgent);
  bind();
  steering::reset();
  CHECK(steering::missing() == map::NavMeshAgent.key);
  CHECK(!steering::steer(nullptr, 0, 0) && site_intact());
  give_entries();
  CHECK(steering::missing().empty());
}

// Another build's bytes at the site: refused, nothing written. Nothing tries again until the mods start afresh, neither
// a frame's ask nor a start's question, or a mod asking each frame in a refused build would stop every thread each
// frame. After that start, one question tries again and finds the map's bytes.
void a_refused_patch_leaves_nothing_installed() {
  start();
  CHECK(steering::steer(nullptr, 0, 0));  // an earlier page, which the refusal must forget
  pm::hooks::rollback_to(0);
  fake::native_site(map::UnityPlayer_SteerSite, other_build());
  bind();
  CHECK(!steering::steer(nullptr, 0, 0));
  CHECK(other_build()[0] == 0x90);
  Agent mine(2.5f);
  Managed managed{&g_agentClass, nullptr, &g_nativeObject};
  g_crowdAgent = mine.bytes;
  CHECK(!steering::steer(&managed, 1, 0));
  fake::native_site(map::UnityPlayer_SteerSite, site());
  bind();
  CHECK(!steering::steer(&managed, 1, 0) && site_intact());
  CHECK(!steering::steer(nullptr, 0, 0) && site_intact());  // available() again: no second try
  CHECK(native(crowd_update(mine.bytes), 2.5f));
  steering::reset();  // the mods start afresh
  CHECK(steering::steer(nullptr, 0, 0));
  CHECK(!site_intact());
  CHECK(steering::steer(&managed, 1, 0));
  const Read read = crowd_update(mine.bytes);
  CHECK(read.x == 1 && read.z == 0);
  steering::expire();
}

// The patch is a site patch like any other: it goes with the group that made it, with the mods' restart and at
// shutdown, the site's own bytes written back. The next ask patches again, on a new page.
void the_site_is_restored_on_the_paths_every_site_is() {
  start();
  Agent mine(2.5f);
  Managed managed{&g_agentClass, nullptr, &g_nativeObject};
  g_crowdAgent = mine.bytes;

  pm::hooks::begin_group();  // a mod that does not start
  CHECK(steering::steer(nullptr, 0, 0));
  unsigned char* const first = stub_page();
  pm::hooks::rollback_group();
  CHECK(site_intact());

  const size_t mods = pm::hooks::mark();  // the mods' restart
  CHECK(steering::steer(&managed, 1, 0));
  unsigned char* const second = stub_page();
  CHECK(second != nullptr && second != first);
  CHECK(crowd_update(mine.bytes).z == 0);
  pm::hooks::rollback_to(mods);
  CHECK(site_intact());

  CHECK(steering::steer(&managed, 0, 1));  // shutdown
  unsigned char* const third = stub_page();
  CHECK(third != nullptr && third != second);
  const Read read = crowd_update(mine.bytes);
  CHECK(read.x == 0 && read.z == 1);
  pm::hooks::restore_sites();
  CHECK(site_intact() && !pm::hooks::site_patched(site()));
  steering::expire();
  pm::hooks::rollback_to(0);
}

// A hunt searches once it has lost its target and has a player to search for. Only the hunt's own object says so, and
// each of the two is read through its own entry.
void a_hunt_searches_with_no_target_and_someone_to_find() {
  namespace walk = pm::walk_state;
  start();
  void* player[2] = {&g_playerClass, nullptr};
  StateObject hunt{&g_huntClass};
  CHECK(!walk::hunt_searching(&hunt));  // neither
  hunt.put<void*>(kHuntSearch, player);
  CHECK(walk::hunt_searching(&hunt));
  hunt.put<void*>(kHuntTarget, player);
  CHECK(!walk::hunt_searching(&hunt));  // it has them in its sights
  hunt.put<void*>(kHuntTarget, nullptr);
  fake::destroy(player);
  CHECK(!walk::hunt_searching(&hunt));  // whom it searched for has gone
  StateObject wander{&g_wanderClass};
  wander.put<void*>(kHuntSearch, &g_playerClass);
  CHECK(!walk::hunt_searching(&wander));
  CHECK(!walk::hunt_searching(nullptr));
}

// A wander's reach is its radius, or its point when that is further; a D.O.T.S. walk's the same, through its own
// entries. Another state says nothing, and the reach it is given stays.
void a_wanders_reach_is_read_from_its_own_class() {
  namespace walk = pm::walk_state;
  namespace tether = pm::tether;
  start();
  StateObject wander{&g_wanderClass};
  wander.put<int32_t>(kWanderRadius, 3);
  tether::Reach reach;
  CHECK(walk::wander_reach(&wander, 1, 2, reach));
  CHECK(reach.x == 1 && reach.z == 2 && reach.radius == 3);
  // No point chosen yet: what the point's field holds is not one.
  wander.put(kWanderPoint, phasmo::Vector3{100, 0, 100});
  CHECK(walk::wander_reach(&wander, 1, 2, reach) && reach.radius == 3);
  wander.put<bool>(kWanderHas, true);
  wander.put(kWanderPoint, phasmo::Vector3{7, 40, 10});
  CHECK(walk::wander_reach(&wander, 1, 2, reach) && reach.radius == 10);  // on the level: y plays no part

  StateObject dots{&g_dotsClass};
  dots.put<int32_t>(kDotsRadius, 2);
  CHECK(walk::wander_reach(&dots, 0, 0, reach) && reach.radius == 2);
  dots.put<bool>(kDotsHas, true);
  dots.put(kDotsPoint, phasmo::Vector3{0, 0, 5});
  CHECK(walk::wander_reach(&dots, 0, 0, reach) && reach.radius == 5);

  StateObject other{&g_otherClass};
  other.put<int32_t>(kWanderRadius, 9);
  tether::Reach kept{1, 2, 3};
  CHECK(!walk::wander_reach(&other, 0, 0, kept) && !walk::wander_reach(nullptr, 0, 0, kept));
  CHECK(kept.x == 1 && kept.z == 2 && kept.radius == 3);
}

}  // namespace

// The host's own view of the map, which the steering reads (game_map.cpp defines it in the host): the fake's every entry.
const PmGameEntry* pm::api::host_only::game_entry(const char* key) { return pm::fake::host_entry(key); }

int main() {
  the_patch_reads_its_bytes_from_the_map();
  the_stub_stays_passive_until_asked();
  only_the_asked_agents_direction_is_replaced_this_frame();
  only_a_live_nav_mesh_agent_is_steered();
  a_destroyed_agent_is_not_steered();
  an_entry_gone_after_the_patch_steers_nothing();
  an_entry_that_did_not_resolve_makes_it_unavailable();
  a_refused_patch_leaves_nothing_installed();
  the_site_is_restored_on_the_paths_every_site_is();
  a_hunt_searches_with_no_target_and_someone_to_find();
  a_wanders_reach_is_read_from_its_own_class();
  std::printf(g_failures ? "%d check(s) failed\n" : "all movement tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
