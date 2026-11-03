// Offline tests for the Brownie's footsteps (mods/custom_ghosts/brownie.cpp) over the fake backend: its hook on
// Footstep.Play, the host's word on whether anyone looks at it, and the same rule whoever steers it, its AI or a
// player. The fake Footstep.Play does what the game's does (sdk/map/Footstep.h): it gives the Noise the step's
// reach, plays the clip through Noise.PlaySound (which stores the Noise's volume and loudness and gives the
// AudioSource the clip's volume, and activates the Noise's object, its "Noise Cube"), then multiplies the
// AudioSource's volume by its own argument. A fake sensor pulse stands in for the sound sensor and the
// parabolic microphone: like their physics queries, it finds an active Noise Cube only while its collider is on.
// Run: build\brownie_test.exe
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <vector>

#include "Equipment.h"
#include "Footstep.h"
#include "GhostAI.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "UnityObject.h"
#include "brownie.h"
#include "brownie_rule.h"
#include "check.h"
#include "fake_game.h"
#include "flame_net.h"
#include "ghosts.h"
#include "net.h"
#include "players.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};  // the count of hooked bodies running, which hooks.cpp keeps in the game
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
using namespace pm::custom;

constexpr float kReach = 20.0f;       // the footstep prefab's AudioSource maxDistance
constexpr float kClipVolume = 0.2f;   // the footstep prefab's AudioSource volume
constexpr float kLoudness = 150.0f;   // Noise.SetVolume: loudness is 150 times the volume
constexpr int kMasterActor = 1, kOtherActor = 2;

// A fake managed object: its class pointer first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};

// Where the fields lie in the fake objects. The Noise's "active" and its collider stand in for its object's
// active state and the BoxCollider on it.
constexpr int32_t kFootstepNoise = 0x08, kFootstepGhost = 0x10, kFootstepReach = 0x18;
constexpr int32_t kGhostHunting = 0x08;
constexpr int32_t kNoiseVolume = 0x08, kNoiseLoudness = 0x0C, kNoiseReach = 0x10, kNoiseAudio = 0x14,
                  kNoiseActive = 0x18;
constexpr int32_t kColliderOn = 0x08;
constexpr int32_t kNetworkSpots = 0x20, kSpotPlayer = 0x20, kPlayerDead = 0x30;

template <class T>
T& at(Obj& o, int32_t offset) {
  return *reinterpret_cast<T*>(reinterpret_cast<unsigned char*>(&o) + offset);
}

constexpr int32_t kMonitorViewers = 0x20;

Obj g_ghost, g_otherGhost, g_footstep, g_noise, g_collider, g_viewer, g_spot, g_net, g_colliderClass,
    g_cctvClass, g_monitor, g_camera;
void* g_monitorRef = nullptr;     // CCTVController.instance
void* g_monitorCameras = nullptr;  // what the monitor's list of its kind of camera holds
bool g_master = true;
bool g_noCollider = false;  // the Noise's object holds no collider
std::map<const void*, phasmo::Vector3> g_at, g_facing;  // where each object is, and the way it faces
std::vector<std::vector<uint8_t>> g_sent;               // the host's kSeen words, in order
int g_plays = 0, g_colliderSwitches = 0;
float g_playVolume = -1.0f;  // the volume the game's own play was given
void* g_playName = nullptr;

// What the sensors made of the Noise Cubes they found: the parabolic radar's points, the parabolic
// microphone's least reading over what it holds (none: no reading), and the sound sensor's challenge count.
struct Sensors {
  int radarPoints = 0, challenges = 0;
  float least = -1.0f;
} g_sensors;

PmHostApi g_host{};
bool (*g_fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;
std::map<std::string, void*> g_detour;
bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!g_fakeHookBody(key, index, detour, original, name)) return false;
  g_detour[key] = detour;
  return true;
}

// The game's bodies.
void fake_set_max_distance(void* noise, float metres, const void*) { at<float>(*static_cast<Obj*>(noise), kNoiseReach) = metres; }
void fake_play(void* footstep, float volume, void* name, const void*) {
  ++g_plays;
  g_playVolume = volume;
  g_playName = name;
  Obj& step = *static_cast<Obj*>(footstep);
  Obj& noise = *at<Obj*>(step, kFootstepNoise);
  fake_set_max_distance(&noise, at<float>(step, kFootstepReach), nullptr);
  at<float>(noise, kNoiseVolume) = kClipVolume;
  at<float>(noise, kNoiseLoudness) = kLoudness * kClipVolume;
  at<float>(noise, kNoiseAudio) = kClipVolume;
  at<bool>(noise, kNoiseActive) = true;  // PlaySound activates the Noise Cube
  at<float>(noise, kNoiseAudio) *= volume;
}
void* fake_get_component(void* component, void* type, const void*) {
  return !g_noCollider && component == &g_noise && type == phasmo::Collider::type.type_object() ? &g_collider : nullptr;
}
void fake_collider_set_enabled(void* collider, bool on, const void*) {
  ++g_colliderSwitches;
  at<bool>(*static_cast<Obj*>(collider), kColliderOn) = on;
}
bool fake_is_master(const void*) { return g_master; }
void* fake_find_none(void*, const void*) { return fake::array(sizeof(void*), 0); }  // no video cameras
void* fake_monitor_cameras(void*, const void*) { return g_monitorCameras; }

// One footstep of `ghost` through the hooked play, given `volume`.
void play(void* ghost, float volume = 1.0f) {
  at<void*>(g_footstep, kFootstepGhost) = ghost;
  at<Obj*>(g_footstep, kFootstepNoise) = &g_noise;
  at<float>(g_footstep, kFootstepReach) = kReach;
  g_noise = Obj{};
  using F = void (*)(void*, float, void*, const void*);
  reinterpret_cast<F>(g_detour.at("Footstep.Play"))(&g_footstep, volume, &g_playName, nullptr);
}

float audio() { return at<float>(g_noise, kNoiseAudio); }
float reach() { return at<float>(g_noise, kNoiseReach); }
float volume() { return at<float>(g_noise, kNoiseVolume); }
float loudness() { return at<float>(g_noise, kNoiseLoudness); }
bool collider_on() { return at<bool>(g_collider, kColliderOn); }

// One pulse of the sensors near the step: they find its Noise Cube only while it is active and its collider
// is on. The parabolic microphone (in radar mode) adds a point labelled with the loudness and holds it for
// its readings; the sound sensor moves its challenge counter on.
void pulse() {
  g_sensors = Sensors{};
  if (!at<bool>(g_noise, kNoiseActive) || !collider_on()) return;
  ++g_sensors.radarPoints;
  g_sensors.least = loudness();
  ++g_sensors.challenges;
}

// The game's own step, untouched: its reach, its sound at the volume given, what equipment reads, and its
// Noise Cube found by the sensors.
bool vanilla(float given) {
  pulse();
  return g_plays == 1 && g_playVolume == given && reach() == kReach && audio() == kClipVolume * given &&
         volume() == kClipVolume && loudness() == kLoudness * kClipVolume && collider_on() &&
         g_sensors.radarPoints == 1 && g_sensors.challenges == 1 && g_sensors.least == kLoudness * kClipVolume;
}

// The host says whether anyone looks at the Brownie, as a client receives it.
void hear(bool seen, int sender = kMasterActor) {
  const std::vector<uint8_t> message = net_msg::seen(seen);
  brownie::receive(sender, message.data(), message.size());
}

// What the host's `index`th word says through read_seen: 1 or 0, or -1 when read_seen refuses it.
int sent_seen(size_t index) {
  bool seen = false;
  if (index >= g_sent.size()) return -1;
  return net_msg::read_seen(g_sent[index].data(), g_sent[index].size(), seen) ? (seen ? 1 : 0) : -1;
}

// One game in which the Brownie haunts, `master` says whether this client is the host, and its hook is made.
void start(bool master, bool steered, bool hunting) {
  brownie::reset();
  fake::reset();
  fake::field(map::Footstep_noise, kFootstepNoise);
  fake::field(map::Footstep_ghost, kFootstepGhost);
  fake::field(map::Footstep_reach, kFootstepReach);
  fake::field(map::GhostAI_isHunting, kGhostHunting);
  fake::field(map::Noise_volume, kNoiseVolume);
  fake::field(map::Noise_loudness, kNoiseLoudness);
  fake::field(map::Network_playerSpots, kNetworkSpots);
  fake::field(map::PlayerSpot_player, kSpotPlayer);
  fake::field(map::Player_isDead, kPlayerDead);
  fake::method(map::Footstep_Play, reinterpret_cast<void*>(&fake_play));
  fake::method(map::Noise_SetMaxDistance, reinterpret_cast<void*>(&fake_set_max_distance));
  fake::method(map::PhotonNetwork_get_IsMasterClient, reinterpret_cast<void*>(&fake_is_master));
  fake::method(map::UnityObject_FindObjectsOfType, reinterpret_cast<void*>(&fake_find_none));
  fake::type(map::CCTV, &g_cctvClass);
  g_monitorRef = nullptr;  // no monitor until a test puts one in the contract
  fake::static_field(map::CCTVController_instance, &g_monitorRef, sizeof g_monitorRef, true);
  fake::field(map::CCTVController_viewers, kMonitorViewers);
  fake::method(map::CCTVController_GetCCTVCamerasByCurrentCameraType, reinterpret_cast<void*>(&fake_monitor_cameras));
  fake::type(map::Collider, &g_colliderClass);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::Collider_set_enabled, reinterpret_cast<void*>(&fake_collider_set_enabled));
  g_host = *fake::host();
  g_fakeHookBody = g_host.hook_body;
  g_host.hook_body = &capture_hook_body;
  g_detour.clear();
  std::string missing;
  pm::api::bind(&g_host, missing);
  CHECK(brownie::create_hooks());
  fake::player_controlled(steered);
  g_master = master;
  g_plays = g_colliderSwitches = 0;
  g_playVolume = -1.0f;
  g_noCollider = false;
  g_sent.clear();
  g_collider = Obj{};
  at<bool>(g_collider, kColliderOn) = true;  // the prefab's BoxCollider is on
  at<bool>(g_ghost, kGhostHunting) = hunting;
  at<bool>(g_otherGhost, kGhostHunting) = hunting;
  brownie::tick_all(&g_ghost, kBrownie);
  // One investigator, in the network's player spots as core/players.h reads them through the map, 5 m from the ghost and facing it.
  g_viewer = g_spot = g_net = Obj{};
  at<void*>(g_spot, kSpotPlayer) = &g_viewer;
  at<void*>(g_net, kNetworkSpots) = pm::fake::list({&g_spot});
  g_at.clear();
  g_facing.clear();
  g_at[&g_ghost] = {0, 0, 5};
  g_at[&g_viewer] = {0, 0, 0};
  g_facing[&g_viewer] = {0, 0, 1};
}

template <class Case>
void for_ai_and_player(Case each) {
  for (const bool steered : {false, true}) each(steered);
}

// Outside hunts its steps are the game's own, watched or not, on the host and on a client.
void brownie_steps_unchanged_outside_hunts() {
  for (const bool master : {true, false}) {
    for (const bool seen : {false, true}) {
      start(master, false, false);
      hear(seen);
      play(&g_ghost, 0.5f);
      CHECK(vanilla(0.5f));
      CHECK(g_playName == &g_playName);  // the name goes to the game as given
      CHECK(g_colliderSwitches == 0);    // its Noise Cube's collider is left alone
    }
  }
  // Another ghost's steps, hunting: the game's own.
  start(true, false, true);
  play(&g_otherGhost, 1.5f);
  CHECK(vanilla(1.5f));
  // No Brownie haunts: the game's own.
  start(true, false, true);
  brownie::tick_all(&g_ghost, kBrownie + 1);
  play(&g_ghost);
  CHECK(vanilla(1.0f));
}

// In a hunt while nobody looks at it, a step makes no sound: the game's play gets volume 0, which it
// multiplies into the step's AudioSource alone. The step still plays once, with its reach.
void brownie_steps_silent_when_unwatched() {
  CHECK(brownie_step(true, false) == BrownieStep::Silent);
  CHECK(brownie_step_volume(BrownieStep::Silent, 1.5f) == 0.0f);
  start(true, false, true);  // the host: nobody looks at it until its tick says so
  play(&g_ghost, 1.5f);
  CHECK(g_plays == 1 && g_playVolume == 0.0f && audio() == 0.0f);
  CHECK(reach() == kReach);  // its reach stays the game's
  start(false, false, true);  // a client: the host said nobody looks
  hear(false);
  play(&g_ghost);
  CHECK(g_plays == 1 && audio() == 0.0f && reach() == kReach);
}

// In a hunt while nobody looks at it, the sound sensor (Noise.volume) and the parabolic microphone
// (Noise.loudness) read nothing of a step either; while anyone looks they read the game's own values.
void brownie_steps_invisible_to_electronics_when_unwatched() {
  start(true, false, true);
  play(&g_ghost);
  CHECK(volume() == 0.0f && loudness() == 0.0f);
  start(false, false, true);
  hear(true);
  play(&g_ghost);
  CHECK(volume() == kClipVolume && loudness() == kLoudness * kClipVolume);
}

// In a hunt while nobody looks at it, the sensors never detect a step: its Noise Cube's collider is off from
// before the game's play activates the cube, so the parabolic radar shows no point, the parabolic microphone
// holds nothing for its least reading, and the sound sensor's challenge counter does not move. The same pooled
// footstep's next step that is not silent has its collider on again before the game plays it, and once no
// Brownie haunts, or the mod stops, every collider it switched off is on again.
void brownie_step_not_detected_when_unwatched() {
  start(true, false, true);
  play(&g_ghost);
  pulse();
  CHECK(!collider_on() && at<bool>(g_noise, kNoiseActive));  // the cube is active, as the game left it
  CHECK(g_sensors.radarPoints == 0 && g_sensors.least < 0.0f && g_sensors.challenges == 0);
  CHECK(g_plays == 1 && reach() == kReach);  // the step itself still played
  // Outside a hunt the same footstep is the game's own again.
  at<bool>(g_ghost, kGhostHunting) = false;
  g_plays = 0;
  play(&g_ghost);
  CHECK(vanilla(1.0f));
  // Watched in a hunt (a client told by the host): found, at its reach of 12 m.
  start(false, false, true);
  hear(false);
  play(&g_ghost);
  pulse();
  CHECK(!collider_on() && g_sensors.challenges == 0);
  hear(true);
  play(&g_ghost);
  pulse();
  CHECK(collider_on() && g_sensors.radarPoints == 1 && g_sensors.challenges == 1 && reach() == 12.0f);
  // Another ghost's step through the same pooled footstep after a silent one: found.
  start(true, false, true);
  play(&g_ghost);
  CHECK(!collider_on());
  g_plays = 0;
  play(&g_otherGhost);
  CHECK(vanilla(1.0f));
  // The Brownie stops haunting before its footstep plays again: the collider is on at once.
  start(true, false, true);
  play(&g_ghost);
  brownie::tick_all(nullptr, 0);
  CHECK(collider_on());
  // The mod stops: the same.
  start(true, false, true);
  play(&g_ghost);
  brownie::reset();
  CHECK(collider_on());
}

// In a hunt while anyone looks at it, a step carries 8 m less far, the game's hunting Myling's rule, at
// the game's own volume.
void brownie_steps_quiet_like_myling_when_watched() {
  CHECK(brownie_step(true, true) == BrownieStep::Quiet);
  CHECK(brownie_step_volume(BrownieStep::Quiet, 1.5f) == 1.5f);
  start(false, false, true);
  hear(true);
  play(&g_ghost, 1.5f);
  CHECK(g_plays == 1 && g_playVolume == 1.5f && audio() == kClipVolume * 1.5f);
  CHECK(reach() == quiet_step_distance(kReach) && reach() == 12.0f);
}

// The host says whether anyone looks at once when it changes and again every second of a hunt; a client
// takes it only from the master, and forgets it when the Brownie no longer haunts.
void brownie_watched_state_reaches_clients() {
  bool seen = false;
  const std::vector<uint8_t> yes = net_msg::seen(true), no = net_msg::seen(false);
  CHECK(net_msg::read_seen(yes.data(), yes.size(), seen) && seen);
  CHECK(net_msg::read_seen(no.data(), no.size(), seen) && !seen);
  CHECK(!net_msg::read_seen(yes.data(), yes.size() - 1, seen));  // cut short
  const std::vector<uint8_t> anger = net_msg::anger(true);
  CHECK(!net_msg::read_seen(anger.data(), anger.size(), seen));  // another kind
  std::vector<uint8_t> odd = yes;
  odd[4] = 2;
  CHECK(!net_msg::read_seen(odd.data(), odd.size(), seen));

  SeenTeller teller;
  CHECK(teller.due(false, false, 0));       // the first word, outside a hunt
  CHECK(!teller.due(false, false, 5000));   // unchanged outside a hunt: nothing more
  CHECK(!teller.due(true, false, 5001));    // a change outside a hunt: nothing reads it yet
  CHECK(!teller.due(false, false, 9000));
  CHECK(teller.due(true, true, 9001));      // entering a hunt: the current state goes out at once
  CHECK(!teller.due(true, true, 9002));     // unchanged, just said
  CHECK(teller.due(true, true, 10001));     // a second of the hunt later: again
  CHECK(!teller.due(true, true, 10500));
  CHECK(teller.due(false, true, 10600));    // a change in the hunt: at once
  CHECK(!teller.due(false, true, 11599));
  CHECK(teller.due(false, true, 11600));
  CHECK(!teller.due(true, false, 11700));   // the hunt ends: nothing more
  CHECK(teller.due(true, true, 11800));     // the next hunt's first frame sends
  CHECK(!teller.due(true, false, 11900));
  CHECK(teller.due(true, true, 11901));     // and sends even when the state is what was last said

  start(false, false, true);
  hear(true);
  play(&g_ghost);
  CHECK(reach() == 12.0f);  // taken from the master
  hear(false, kOtherActor);
  play(&g_ghost);
  CHECK(reach() == 12.0f && audio() == kClipVolume);  // another client's word: ignored
  const uint8_t bad[] = {'C', 'G', 'F', net_msg::kSeen};
  brownie::receive(kMasterActor, bad, sizeof bad);
  play(&g_ghost);
  CHECK(reach() == 12.0f);  // a malformed word: ignored
  hear(false);
  play(&g_ghost);
  CHECK(audio() == 0.0f && volume() == 0.0f);
  // The Brownie stops haunting: what the host said goes with it.
  hear(true);
  brownie::tick_all(nullptr, 0);
  brownie::tick_all(&g_ghost, kBrownie);
  play(&g_ghost);
  CHECK(audio() == 0.0f);
}

// Seen on the truck's monitor counts as being looked at, whoever steers it: someone actively views the monitor and a
// camera of the kind it shows has the ghost in view. A camera alone, with nobody at the monitor, does not slow it.
void brownie_seen_on_the_monitor() {
  for_ai_and_player([](bool steered) {
    start(true, steered, true);
    g_facing[&g_viewer] = {0, 0, -1};  // the one investigator looks away
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen());  // no monitor in this contract yet
    g_monitor = g_camera = Obj{};
    g_monitorRef = &g_monitor;
    g_at[&g_camera] = {0, 0, 0};
    g_facing[&g_camera] = {0, 0, 1};  // 5 m from the ghost, pointing at it
    g_monitorCameras = fake::list({&g_camera});
    at<void*>(g_monitor, kMonitorViewers) = fake::list({});
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen());  // nobody at the monitor
    at<void*>(g_monitor, kMonitorViewers) = fake::list({&g_viewer});
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(brownie::seen() && brownie::watched());
    CHECK(!g_sent.empty() && sent_seen(g_sent.size() - 1) == 1);  // the clients hear it, as for a player's look
    play(&g_ghost);
    CHECK(reach() == 12.0f && audio() == kClipVolume);  // Quiet
    g_facing[&g_camera] = {0, 0, -1};  // the camera points away
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen());
    g_facing[&g_camera] = {0, 0, 1};
    g_monitorCameras = fake::list({});  // the monitor shows another kind of camera
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen());
    g_monitorCameras = nullptr;  // the call found nothing
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen());
  });
}

// The host works out who looks at the ghost and says so: a player in view makes its steps Quiet, a player facing
// away leaves them Silent, whoever steers it. A word goes out when it changes and again after a second of the hunt,
// reliably to the others, and not for changes outside a hunt.
void brownie_host_tells_clients_who_looks() {
  for_ai_and_player([](bool steered) {
    start(true, steered, true);
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(brownie::seen() && brownie::watched());
    CHECK(g_sent.size() == 1 && sent_seen(0) == 1);  // the first word, as the first hunting frame
    play(&g_ghost);
    CHECK(reach() == 12.0f && audio() == kClipVolume);  // Quiet
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(g_sent.size() == 1);  // unchanged: nothing more yet
    g_facing[&g_viewer] = {0, 0, -1};  // the player turns away
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(!brownie::seen() && g_sent.size() == 2 && sent_seen(1) == 0);  // a change goes out at once
    play(&g_ghost);
    CHECK(audio() == 0.0f && volume() == 0.0f && reach() == kReach);  // Silent
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(g_sent.size() == 2);
    Sleep(kBrownieSeenResendMs + 50);
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(g_sent.size() == 3 && sent_seen(2) == 0);  // a second later: again
    // Outside a hunt a change says nothing; the hunt's first frame says the state it finds.
    at<bool>(g_ghost, kGhostHunting) = false;
    g_facing[&g_viewer] = {0, 0, 1};
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(brownie::seen() && g_sent.size() == 3);
    at<bool>(g_ghost, kGhostHunting) = true;
    brownie::tick_host(&g_ghost, kBrownie);
    CHECK(g_sent.size() == 4 && sent_seen(3) == 1);
    // Not a Brownie: nothing is told.
    brownie::tick_host(&g_ghost, kBrownie + 1);
    CHECK(!brownie::seen() && g_sent.size() == 4);
  });
}

// When the Noise's object has no collider to switch off, the step stays silent with the readings zeroed.
void brownie_step_silent_without_a_collider() {
  start(true, false, true);
  g_noCollider = true;
  play(&g_ghost);
  CHECK(g_colliderSwitches == 0);
  CHECK(g_plays == 1 && audio() == 0.0f && volume() == 0.0f && loudness() == 0.0f && reach() == kReach);
  brownie::tick_all(nullptr, 0);  // nothing was held, so nothing to restore
  CHECK(g_colliderSwitches == 0);
}

// A player's Brownie gets its AI's rule: the hook asks nothing of who steers, and the host works out who
// looks at the ghost it runs whoever steers it (mods/custom_ghosts/mod.cpp hands brownie::tick_host the
// host's ghost before any steering).
void brownie_player_steps_follow_same_rule() {
  for_ai_and_player([](bool steered) {
    start(true, steered, true);
    play(&g_ghost);
    CHECK(audio() == 0.0f && volume() == 0.0f && loudness() == 0.0f && reach() == kReach);
    pulse();
    CHECK(!collider_on() && g_sensors.radarPoints == 0 && g_sensors.challenges == 0);
    start(false, steered, true);
    hear(true);
    play(&g_ghost);
    CHECK(reach() == 12.0f && audio() == kClipVolume);
    start(false, steered, false);
    play(&g_ghost);
    CHECK(vanilla(1.0f));
  });
  std::ifstream in(std::string(PM_SOURCE_DIR) + "/mods/custom_ghosts/mod.cpp", std::ios::binary);
  const std::string mod((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(mod.find("void* ghost = haunting && phasmo::PhotonNetwork::get_IsMasterClient() ? haunting : nullptr;") !=
        std::string::npos);
  CHECK(mod.find("brownie::tick_host(ghost, type);") != std::string::npos);
  CHECK(mod.find("case net_msg::kSeen: brownie::receive(sender, data, size); break;") != std::string::npos);
}

}  // namespace

// What the Brownie calls of the game.
namespace pm::game {
bool alive(void* o) { return o != nullptr; }
void* transform_of(void* o) { return o; }
void* network() { return &g_net; }
int master_actor() { return kMasterActor; }
phasmo::Vector3 position(void* transform) {
  const auto it = g_at.find(transform);
  return it == g_at.end() ? phasmo::Vector3{} : it->second;
}
phasmo::Vector3 forward(void* transform) {
  const auto it = g_facing.find(transform);
  return it == g_facing.end() ? phasmo::Vector3{0, 0, 1} : it->second;
}
void* raycast(phasmo::Vector3, phasmo::Vector3, float) { return nullptr; }  // nothing solid in the way
void each_running_camera(CameraScan&, uint64_t, const std::function<bool(void*)>&) {}  // no video cameras
}  // namespace pm::game
namespace pm::net {
bool send(const uint8_t* data, size_t size, To to, bool reliable, int) {
  if (to == To::Others && reliable && size > 3 && data[3] == net_msg::kSeen) g_sent.emplace_back(data, data + size);
  return true;
}
}  // namespace pm::net

int main() {
  brownie_steps_unchanged_outside_hunts();
  brownie_steps_silent_when_unwatched();
  brownie_steps_invisible_to_electronics_when_unwatched();
  brownie_step_not_detected_when_unwatched();
  brownie_steps_quiet_like_myling_when_watched();
  brownie_watched_state_reaches_clients();
  brownie_seen_on_the_monitor();
  brownie_host_tells_clients_who_looks();
  brownie_step_silent_without_a_collider();
  brownie_player_steps_follow_same_rule();
  pm::custom::brownie::reset();  // what it holds goes back before the fake's own storage does
  std::printf(g_failures ? "%d check(s) failed\n" : "all brownie tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
