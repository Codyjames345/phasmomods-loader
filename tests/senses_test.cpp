#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "Player.h"
#include "PlayerVoice.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "session.h"

// What the ghost player hears of dead investigators. VoiceVolume.Update (REA, build 25811160) mutes
// a dead player's voice (its death multiplier) for a living listener, and reads the listener from
// Network's local Player, which the ghost player's client has cleared (body.h). The game is the fake
// backend, with its fields at offsets of the test's own.
namespace {
struct Object {
  void* klass = nullptr;
  alignas(8) unsigned char fields[0x80]{};
};
Object fixtureSpeaker, fixtureVolume, fixtureMultiplier, fixtureGhost, fixtureLevel, fixtureVivox;
void* localPlayer = nullptr;
float setTo = -1;       // the last value the multiplier was given
int nativeUpdates = 0;  // calls that reached the game's own Update
int unmutes = 0;        // calls that reached Vivox's own UnmuteInputDevice
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

template <class T>
void put(Object& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

void native_update(void*, const void*) { ++nativeUpdates; }
void set_multiplier(void* m, float value, const void*) {
  CHECK(m == &fixtureMultiplier);
  setTo = value;
}
void* vivox_instance(const void*) { return &fixtureVivox; }
void vivox_mute(void*, const void*) {}
void vivox_unmute(void*, const void*) { ++unmutes; }
float listener_volume(const void*) { return 1.0f; }
void set_listener_volume(float, const void*) {}
void* find_objects(void*, const void*) { return nullptr; }
void* get_component(void*, void*, const void*) { return nullptr; }
void set_mute(void*, bool, const void*) {}

using Update_t = void (*)(void*, const void*);
void update() {
  setTo = -1;
  const auto hooked = detours.find("VoiceVolume.Update");
  CHECK(hooked != detours.end());
  reinterpret_cast<Update_t>(hooked != detours.end() ? hooked->second : reinterpret_cast<void*>(&native_update))(&fixtureVolume, nullptr);
}

/** Gives every entry senses.cpp uses, VoiceVolume's fields at the test's own offsets, and binds through a host
 *  that records each hook's detour. */
void give_entries() {
  namespace map = pm::map;
  namespace fake = pm::fake;
  fake::reset();
  static Object voiceClass, sourceClass;
  fake::method(map::VivoxService_get_Instance, reinterpret_cast<void*>(&vivox_instance));
  fake::method(map::VivoxServiceInternal_MuteInputDevice, reinterpret_cast<void*>(&vivox_mute));
  fake::method(map::VivoxServiceInternal_UnmuteInputDevice, reinterpret_cast<void*>(&vivox_unmute));
  fake::method(map::AudioListener_get_volume, reinterpret_cast<void*>(&listener_volume));
  fake::method(map::AudioListener_set_volume, reinterpret_cast<void*>(&set_listener_volume));
  fake::method(map::UnityObject_FindObjectsOfType, reinterpret_cast<void*>(&find_objects));
  fake::type(map::PlayerVoice, &voiceClass);
  fake::type(map::AudioSource, &sourceClass);
  fake::field(map::PlayerVoice_globalTap, 0x10);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&get_component));
  fake::method(map::AudioSource_set_mute, reinterpret_cast<void*>(&set_mute));
  fake::method(map::VoiceVolume_Update, reinterpret_cast<void*>(&native_update));
  fake::field(map::VoiceVolume_player, 0x18);
  fake::field(map::VoiceVolume_deathMultiplier, 0x30);
  fake::method(map::VolumeMultiplier_set_value, reinterpret_cast<void*>(&set_multiplier));
  fake::field(map::Player_isDead, 0x10);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
}

/** A one-player private room in which this client plays the ghost. */
pm::session::Session ghost_session() {
  pm::session::Session s;
  s.set_volunteer(true);
  for (uint64_t now = 1000; now < 60000 && !s.local_is_ghost(); now += 100) s.observe(1, 1, {1}, now);
  CHECK(s.local_is_ghost());
  return s;
}
}  // namespace

#include "../mods/playable_ghost/senses.cpp"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks
namespace pm::game {
void* level_controller() { return &fixtureLevel; }
void* local_player() { return localPlayer; }
bool alive(void* object) { return object != nullptr; }
}  // namespace pm::game
namespace il2cpp {
uint32_t gc_hold(void*) { return 0; }
void gc_release(uint32_t) {}
}  // namespace il2cpp

int main() {
  give_entries();
  CHECK(pm::senses::create_hooks());
  put(fixtureVolume, 0x18, static_cast<void*>(&fixtureSpeaker));
  put(fixtureVolume, 0x30, static_cast<void*>(&fixtureMultiplier));

  // Before this client plays the ghost, the game decides as it always does.
  update();
  CHECK(nativeUpdates == 1 && setTo == -1);

  // Playing the ghost, with no local investigator: they hear as a living investigator does.
  pm::session::Session s = ghost_session();
  pm::senses::tick(s, &fixtureGhost);
  put(fixtureSpeaker, 0x10, true);  // dead
  update();
  CHECK(setTo == 0.0f && nativeUpdates == 1);
  put(fixtureSpeaker, 0x10, false);  // alive
  update();
  CHECK(setTo == 1.0f && nativeUpdates == 1);

  // The microphone stays off: the game's own unmute is refused while they play the ghost.
  const auto unmute = detours.find("VivoxServiceInternal.UnmuteInputDevice");
  CHECK(unmute != detours.end());
  if (unmute != detours.end()) reinterpret_cast<void (*)(void*, const void*)>(unmute->second)(&fixtureVivox, nullptr);
  CHECK(unmutes == 0);

  // Their investigator back (leaving, quitting): the game's own Update again.
  localPlayer = &fixtureLevel;
  update();
  CHECK(nativeUpdates == 2 && setTo == -1);
  localPlayer = nullptr;
  pm::senses::teardown();
  CHECK(unmutes == 1);  // the microphone is given back, through the game's own call
  // Once given back, the game's own unmute passes through our detour again.
  if (unmute != detours.end()) reinterpret_cast<void (*)(void*, const void*)>(unmute->second)(&fixtureVivox, nullptr);
  CHECK(unmutes == 2);
  update();
  CHECK(nativeUpdates == 3 && setTo == -1);
  return g_failures ? 1 : 0;
}
