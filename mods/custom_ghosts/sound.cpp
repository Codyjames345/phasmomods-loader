#include "sound.h"

#include <algorithm>

#include "game.h"
#include "log.h"

namespace pg::custom::sound {
namespace {

struct Api {
  il2cpp::Object* sourceType = nullptr;
  void (*setClip)(void*, void*, const void*) = nullptr;
  void (*setSpatialBlend)(void*, float, const void*) = nullptr;
  void (*setVolume)(void*, float, const void*) = nullptr;
  void (*play)(void*, const void*) = nullptr;
  float (*length)(void*, const void*) = nullptr;     // AudioClip.length, seconds
  float (*timeScale)(const void*) = nullptr;
  void (*destroyAfter)(void*, float, const void*) = nullptr;  // Object.Destroy(obj, t)
  bool ok = false;
} api;

}  // namespace

bool resolve() {
  constexpr const char* kAudio = "UnityEngine.AudioModule.dll";
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* source = il2cpp::cls(kAudio, "UnityEngine", "AudioSource");
  api.sourceType = source ? il2cpp::type_object(source) : nullptr;
  bool ok = api.sourceType != nullptr;
  ok &= il2cpp::bind(api.setClip, source, "set_clip", 1);
  ok &= il2cpp::bind(api.setSpatialBlend, source, "set_spatialBlend", 1);
  ok &= il2cpp::bind(api.setVolume, source, "set_volume", 1);
  ok &= il2cpp::bind(api.play, source, "Play", 0);
  ok &= il2cpp::bind(api.length, il2cpp::cls(kAudio, "UnityEngine", "AudioClip"), "get_length", 0);
  ok &= il2cpp::bind(api.timeScale, il2cpp::cls(kCore, "UnityEngine", "Time"), "get_timeScale", 0);
  ok &= il2cpp::bind(api.destroyAfter, il2cpp::cls(kCore, "UnityEngine", "Object"), "Destroy", 2);
  api.ok = ok;
  PG_LOG("sound: resolve %s", ok ? "ok" : "unavailable (the ghosts' own sounds are silent)");
  return true;  // optional: only the Knell's steps and the Mesmer's sound use it
}

bool available() { return api.ok; }

void play_at(void* clip, il2cpp::Vector3 at, float volume) {
  if (!api.ok || !game::alive(clip)) return;
  void* go = game::new_game_object("One shot audio");
  if (!go) return;
  game::set_position(game::object_transform(go), at);
  void* source = game::add_component(go, api.sourceType);
  if (!source) {
    game::destroy(go);
    return;
  }
  api.setClip(source, clip, nullptr);
  api.setSpatialBlend(source, 1.0f, nullptr);
  api.setVolume(source, volume, nullptr);
  api.play(source, nullptr);
  api.destroyAfter(go, api.length(clip, nullptr) * std::max(api.timeScale(nullptr), 0.01f), nullptr);
}

}  // namespace pg::custom::sound
