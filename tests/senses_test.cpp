#include <atomic>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "check.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "session.h"

// What the ghost player hears of dead investigators. VoiceVolume.Update (REA, build 25811160) mutes
// a dead player's voice (+0x38, a volume multiplier) for a living listener, and reads the listener
// from Network's local Player, which the ghost player's client has cleared (body.h).
namespace {
struct Object {
  alignas(8) unsigned char fields[0x80]{};
};
Object fixtureSpeaker, fixtureVolume, fixtureMultiplier, fixtureGhost, fixtureLevel, fixtureVivox;
void* localPlayer = nullptr;
float setTo = -1;       // the last value the multiplier was given
int nativeUpdates = 0;  // calls that reached the game's own Update
std::map<std::string, void*> detours;

void native_update(void*, const void*) { ++nativeUpdates; }
void set_multiplier(void* m, float value, const void*) {
  CHECK(m == &fixtureMultiplier);
  setTo = value;
}
void* vivox_instance(const void*) { return &fixtureVivox; }
void vivox_toggle(void*, const void*) {}
float listener_volume(const void*) { return 1.0f; }
void set_listener_volume(float, const void*) {}
void set_mute(void*, bool, const void*) {}
il2cpp::Array* find_objects(void*, const void*) { return nullptr; }

using Update_t = void (*)(void*, const void*);
void update() {
  setTo = -1;
  const auto hooked = detours.find("VoiceVolume.Update");
  CHECK(hooked != detours.end());
  reinterpret_cast<Update_t>(hooked != detours.end() ? hooked->second : reinterpret_cast<void*>(&native_update))(&fixtureVolume, nullptr);
}

/** A one-player private room in which this client plays the ghost. */
pg::session::Session ghost_session() {
  pg::session::Session s;
  s.set_volunteer(true);
  for (uint64_t now = 1000; now < 60000 && !s.local_is_ghost(); now += 100) s.observe(1, 1, {1}, now);
  CHECK(s.local_is_ghost());
  return s;
}
}  // namespace

#include "../mods/playable_ghost/senses.cpp"

namespace pg::hooks {
std::atomic<int> g_inflight{0};
bool create(void*, void* detour, void** original, const char* name) {
  detours[name] = detour;
  *original = std::string(name) == "VoiceVolume.Update" ? reinterpret_cast<void*>(&native_update)
                                                         : reinterpret_cast<void*>(&vivox_toggle);
  return true;
}
}  // namespace pg::hooks
namespace pg::game {
void* game_assembly_address(uintptr_t rva) {
  return rva == 0x225CC10 ? reinterpret_cast<void*>(&set_multiplier) : reinterpret_cast<void*>(rva);
}
void* level_controller() { return &fixtureLevel; }
void* local_player() { return localPlayer; }
bool alive(void* object) { return object != nullptr; }
void* get_component(void*, il2cpp::Object*) { return nullptr; }
}  // namespace pg::game
namespace il2cpp {
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
Object* type_object(Class* klass) { return reinterpret_cast<Object*>(klass); }
void* method_ptr(Class*, const char* name, int) {
#define BIND(text, fn) if (std::strcmp(name, text) == 0) return reinterpret_cast<void*>(&fn)
  BIND("get_Instance", vivox_instance);
  BIND("MuteInputDevice", vivox_toggle);
  BIND("UnmuteInputDevice", vivox_toggle);
  BIND("get_volume", listener_volume);
  BIND("set_volume", set_listener_volume);
  BIND("set_mute", set_mute);
  BIND("FindObjectsOfType", find_objects);
#undef BIND
  return nullptr;
}
uint32_t gc_hold(void*) { return 0; }
void gc_release(uint32_t) {}
}  // namespace il2cpp
namespace pg::log {
void write(const char*, ...) {}
}  // namespace pg::log

int main() {
  CHECK(pg::senses::resolve());
  CHECK(pg::senses::create_hooks());
  il2cpp::set_field<void*>(&fixtureVolume, 0x20, &fixtureSpeaker);
  il2cpp::set_field<void*>(&fixtureVolume, 0x38, &fixtureMultiplier);

  // Before this client plays the ghost, the game decides as it always does.
  update();
  CHECK(nativeUpdates == 1 && setTo == -1);

  // Playing the ghost, with no local investigator: they hear as a living investigator does.
  pg::session::Session s = ghost_session();
  pg::senses::tick(s, &fixtureGhost);
  il2cpp::set_field<bool>(&fixtureSpeaker, 0x28, true);  // dead
  update();
  CHECK(setTo == 0.0f && nativeUpdates == 1);
  il2cpp::set_field<bool>(&fixtureSpeaker, 0x28, false);  // alive
  update();
  CHECK(setTo == 1.0f && nativeUpdates == 1);

  // Their investigator back (leaving, quitting): the game's own Update again.
  localPlayer = &fixtureLevel;
  update();
  CHECK(nativeUpdates == 2 && setTo == -1);
  localPlayer = nullptr;
  pg::senses::teardown();
  update();
  CHECK(nativeUpdates == 3 && setTo == -1);
  return g_failures ? 1 : 0;
}
