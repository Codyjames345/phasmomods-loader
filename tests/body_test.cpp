// Offline tests for the ghost player's body (mods/playable_ghost/body.cpp) over the fake backend: the load wait before
// the ghost player steps out of their investigator, what teardown gives back, whether the game's Leave would find an
// investigator, and that having stepped out is once a contract. Run: build\body_test.exe
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

#include "GameController.h"
#include "check.h"
#include "fake_game.h"
#include "hooks.h"

#include "../mods/playable_ghost/body.cpp"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace body = pm::body;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}
template <class T>
T got(const Obj& o, int32_t offset) {
  T value;
  std::memcpy(&value, reinterpret_cast<const char*>(&o) + offset, sizeof value);
  return value;
}

// The fake's field offsets.
constexpr int32_t kLocalPlayer = 0x10, kLocalRig = 0x18, kSpots = 0x20;  // Network
constexpr int32_t kRigCamera = 0x10, kRigPlayer = 0x18;                  // LocalPlayer
constexpr int32_t kAllLoaded = 0x10;                                     // GameController

Obj g_net, g_player, g_rig, g_camera, g_cameraParent, g_gc, g_level, g_renderer, g_rendererClass;
void* g_gcInstance = nullptr;
void* g_levelNow = nullptr;  // LevelController's instance as core/game answers: null in the lobby
bool g_rigActive = true, g_rendererOn = true, g_journalKept = false, g_privateRoom = true;
void* g_cameraParentNow = nullptr;
int g_outListPutBacks = 0;

void* components(void* go, void* type, const void*) {
  if (go != &g_player || type != phasmo::Renderer::type.type_object()) return nullptr;
  void* found = fake::array(sizeof(void*), 1);
  pm::api::RefArray{found}.set(0, &g_renderer);
  return found;
}

void give_entries() {
  fake::reset();
  fake::field(map::Network_localPlayer, kLocalPlayer);
  fake::field(map::Network_localRig, kLocalRig);
  fake::field(map::Network_playerSpots, kSpots);
  fake::field(map::LocalPlayer_camera, kRigCamera);
  fake::field(map::LocalPlayer_player, kRigPlayer);
  fake::field(map::GameController_allPlayersLoaded, kAllLoaded);
  fake::static_field(map::GameController_instance, &g_gcInstance, sizeof g_gcInstance, true);
  fake::type(map::Renderer, &g_rendererClass);
  fake::method(map::GameObject_GetComponents, reinterpret_cast<void*>(&components));
  std::string missing;
  pm::api::bind(fake::host(), missing);
}

// A contract's start: this client is the host and the ghost player, its investigator whole and the level's load wait on.
void start_contract(pm::session::Session& s) {
  give_entries();
  for (Obj* o : {&g_net, &g_player, &g_rig, &g_camera, &g_cameraParent, &g_gc, &g_level, &g_renderer}) *o = Obj{};
  put(g_net, kLocalPlayer, static_cast<void*>(&g_player));
  put(g_net, kLocalRig, static_cast<void*>(&g_rig));
  put(g_net, kSpots, fake::list({}));
  put(g_rig, kRigCamera, static_cast<void*>(&g_camera));
  put(g_rig, kRigPlayer, static_cast<void*>(&g_player));
  g_gcInstance = &g_gc;
  g_levelNow = &g_level;
  g_rigActive = g_rendererOn = g_privateRoom = true;
  g_journalKept = false;
  g_cameraParentNow = &g_cameraParent;
  g_outListPutBacks = 0;
  s.set_volunteer(true);
  for (uint64_t now = 1000; now < 20000; now += 1000) s.observe(1, 1, {1}, now);
  CHECK(s.ghost_actor() == 1 && s.local_is_ghost());
  body::teardown();
  pm::session::Session lobby;
  void* level = g_levelNow;
  g_levelNow = nullptr;
  body::tick(lobby);  // from the lobby, so the contract below is a new one
  g_levelNow = level;
}

// The game's load wait: nobody steps out before every player's Player is live, or its own wait never ends.
void waits_for_every_player_to_load() {
  pm::session::Session s;
  start_contract(s);
  body::tick(s);
  CHECK(!body::stepped_out() && !body::camera());
  CHECK(got<void*>(g_net, kLocalPlayer) == &g_player && g_rigActive && !g_journalKept);
  CHECK(body::investigator_back());
  g_gcInstance = nullptr;
  body::tick(s);
  CHECK(!body::stepped_out());  // no controller: no wait to be over
  g_gcInstance = &g_gc;
  put(g_gc, kAllLoaded, true);
  body::tick(s);
  CHECK(body::stepped_out() && body::camera() == &g_camera);
}

// Stepped out, this client's investigator is parked and hidden, and teardown gives it all back. The game still finds the parked
// body as this client's local player: its code reads that reference with no check in many places, and an exception inside the
// ghost's state change recurses until the stack runs out, which crashed the first live contract when the main door opened.
void steps_out_and_teardown_gives_it_back() {
  pm::session::Session s;
  start_contract(s);
  put(g_gc, kAllLoaded, true);
  body::tick(s);
  CHECK(body::stepped_out() && g_journalKept && !g_rigActive);
  CHECK(!body::investigator_back());  // playing the ghost, though the game's reference is set
  CHECK(got<void*>(g_net, kLocalPlayer) == &g_player && got<void*>(g_rig, kRigPlayer) == &g_player);
  CHECK(!g_cameraParentNow);  // the camera left the rig
  body::tick(s);
  CHECK(s.ghost_stepped_out());

  body::teardown();
  CHECK(!body::stepped_out() && !body::camera());
  CHECK(body::investigator_back());
  CHECK(got<void*>(g_net, kLocalPlayer) == &g_player && got<void*>(g_rig, kRigPlayer) == &g_player);
  CHECK(g_rigActive && !g_journalKept && g_cameraParentNow == &g_cameraParent);
  CHECK(g_outListPutBacks > 0);
}

// A gone body or a null Network: what the game's Leave would read is not there.
void investigator_back_needs_the_local_player() {
  pm::session::Session s;
  start_contract(s);
  put(g_net, kLocalPlayer, static_cast<void*>(nullptr));
  CHECK(!body::investigator_back());
}

// Giving the body back is for good this contract: the level outlives a leave, and a leave must not step them out again.
void steps_out_once_a_contract() {
  pm::session::Session s;
  start_contract(s);
  put(g_gc, kAllLoaded, true);
  body::tick(s);
  CHECK(body::stepped_out());
  body::teardown();
  body::tick(s);
  CHECK(!body::stepped_out());
  CHECK(s.ghost_stepped_out());  // the others are told it stayed out

  void* level = g_levelNow;
  g_levelNow = nullptr;
  body::tick(s);
  CHECK(!body::stepped_out());
  g_levelNow = level;  // the next contract
  body::tick(s);
  CHECK(body::stepped_out());
  body::teardown();
}

// A public room is none of the mod's business: nobody steps out.
void a_public_room_keeps_the_investigator() {
  pm::session::Session s;
  start_contract(s);
  g_privateRoom = false;
  put(g_gc, kAllLoaded, true);
  body::tick(s);
  CHECK(!body::stepped_out() && body::investigator_back());
  body::teardown();
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
void* network() { return &g_net; }
int actor_number(void*) { return 0; }
void* game_object_of(void* c) { return c; }
void* transform_of(void* c) { return c; }
void* object_transform(void* go) { return go; }
int child_count(void*) { return 0; }
void* child(void*, int) { return nullptr; }
bool is_child_of(void*, void*) { return false; }
bool renderer_enabled(void*) { return g_rendererOn; }
void set_renderer_enabled(void*, bool on) { g_rendererOn = on; }
bool collider_enabled(void*) { return false; }
void set_collider_enabled(void*, bool) {}
bool enabled(void*) { return false; }
void set_enabled(void*, bool) {}
void* parent(void*) { return g_cameraParentNow; }
void set_parent(void*, void* p, bool) { g_cameraParentNow = p; }
phasmo::Vector3 local_position(void*) { return {}; }
phasmo::Quaternion local_rotation(void*) { return {0, 0, 0, 1}; }
void set_local_position(void*, phasmo::Vector3) {}
void set_local_rotation(void*, phasmo::Quaternion) {}
void set_active(void* go, bool on) {
  if (go == &g_rig) g_rigActive = on;
}
void* level_controller() { return g_levelNow; }
bool private_room() { return g_privateRoom; }
}  // namespace pm::game
namespace pm::journal {
void keep(void*) { g_journalKept = true; }
void release() { g_journalKept = false; }
void* root() { return nullptr; }
}  // namespace pm::journal
namespace pm::outlist {
bool take_out(int) { return true; }
void put_back() { ++g_outListPutBacks; }
int actor() { return 0; }
void* player() { return nullptr; }
}  // namespace pm::outlist
namespace pm::reward {
void contract_started() {}
}  // namespace pm::reward

int main() {
  waits_for_every_player_to_load();
  steps_out_and_teardown_gives_it_back();
  investigator_back_needs_the_local_player();
  steps_out_once_a_contract();
  a_public_room_keeps_the_investigator();
  std::printf(g_failures ? "%d check(s) failed\n" : "all body tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
