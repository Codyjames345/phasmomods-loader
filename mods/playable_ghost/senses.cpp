#include "senses.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::senses {
namespace {

constexpr const char* kVivox = "Unity.Services.Vivox.dll";
constexpr size_t kGlobalTap = 0x70;  // PlayerVoice: walkie-talkie (global) VivoxParticipantTap
constexpr ULONGLONG kTapScanMs = 2000;
// Build 25811160 (verified by the host at load); obfuscated names, so by address.
constexpr uintptr_t kVoiceVolumeUpdateRva = 0xDE7DD0;  // VoiceVolume.Update
constexpr uintptr_t kSetMultiplierRva = 0x225CC10;     // its volume multiplier's setter (value, then the stack)
constexpr size_t kVolumePlayer = 0x20;                 // VoiceVolume: the Player whose voice it is
constexpr size_t kVolumeDeathMultiplier = 0x38;        // VoiceVolume: 0 while a living listener must not hear it
constexpr size_t kPlayerDead = 0x28;

struct Api {
  void* (*vivoxInstance)(const void*) = nullptr;
  void (*mute)(void*, const void*) = nullptr;
  float (*listenerVolume)(const void*) = nullptr;
  void (*setListenerVolume)(float, const void*) = nullptr;
  void (*setSourceMute)(void*, bool, const void*) = nullptr;
  il2cpp::Array* (*findObjectsOfType)(void*, const void*) = nullptr;
  il2cpp::Object* sourceType = nullptr;
  il2cpp::Object* playerVoiceType = nullptr;
  void* unmute = nullptr;
  void (*setMultiplier)(void*, float, const void*) = nullptr;
} api;

using Unmute_t = void (*)(void*, const void*);
using VoiceVolume_t = void (*)(void*, const void*);
Unmute_t g_unmute = nullptr;
VoiceVolume_t g_voiceVolume = nullptr;

bool g_muted = false;
bool g_silenced = false;  // listener volume zeroed while the ghost is chosen
float g_savedVolume = 1.0f;
std::vector<il2cpp::Held> g_mutedTaps;  // AudioSources of walkie-talkie taps we muted
ULONGLONG g_nextTapScan = 0;

using il2cpp::bind;

void detour_unmute(void* service, const void* method) {
  hooks::Guard guard;
  if (g_muted) return;  // the ghost player's voice stays off for the whole contract
  g_unmute(service, method);
}

// VoiceVolume.Update silences a dead player's voice for a living listener, and asks the local
// investigator whether the listener is alive: the ghost player's client has none (body.h), so the
// game's own Update throws there every frame. While they play the ghost they hear as a living
// investigator does: the dead stay silent, so they cannot tell the ghost where the living are.
void detour_voice_volume(void* volume, const void* method) {
  hooks::Guard guard;
  if (!g_muted || !volume || game::local_player()) {
    g_voiceVolume(volume, method);
    return;
  }
  void* player = il2cpp::field<void*>(volume, kVolumePlayer);
  void* multiplier = il2cpp::field<void*>(volume, kVolumeDeathMultiplier);
  if (game::alive(player) && multiplier)
    api.setMultiplier(multiplier, il2cpp::field<bool>(player, kPlayerDead) ? 0.0f : 1.0f, nullptr);
}

void set_mic_muted(bool muted) {
  void* vivox = api.vivoxInstance(nullptr);
  if (!vivox) return;
  g_muted = muted;
  muted ? api.mute(vivox, nullptr) : g_unmute(vivox, nullptr);
  PG_LOG("senses: microphone %s", muted ? "muted" : "unmuted");
}

void silence(bool on) {
  if (on == g_silenced) return;
  if (on) g_savedVolume = api.listenerVolume(nullptr);
  api.setListenerVolume(on ? 0.0f : g_savedVolume, nullptr);
  g_silenced = on;
}

void mute_global_taps() {
  il2cpp::Array* voices = api.findObjectsOfType(api.playerVoiceType, nullptr);
  for (uintptr_t i = 0; voices && i < voices->max_length; ++i) {
    void* voice = voices->data<void*>()[i];
    void* tap = voice ? il2cpp::field<void*>(voice, kGlobalTap) : nullptr;
    void* source = game::get_component(tap, api.sourceType);
    if (!source) continue;
    api.setSourceMute(source, true, nullptr);
    if (std::find(g_mutedTaps.begin(), g_mutedTaps.end(), source) == g_mutedTaps.end()) g_mutedTaps.push_back(source);
  }
}

void release() {
  if (g_muted) set_mic_muted(false);
  silence(false);
  for (void* source : g_mutedTaps) {
    if (game::alive(source)) api.setSourceMute(source, false, nullptr);
  }
  g_mutedTaps.clear();
}

}  // namespace

bool resolve() {
  il2cpp::Class* service = il2cpp::cls(kVivox, "Unity.Services.Vivox", "VivoxService");
  il2cpp::Class* internal = il2cpp::cls(kVivox, "Unity.Services.Vivox", "VivoxServiceInternal");
  il2cpp::Class* listener = il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioListener");
  il2cpp::Class* source = il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  bool ok = bind(api.vivoxInstance, service, "get_Instance", 0);
  ok &= bind(api.mute, internal, "MuteInputDevice", 0);
  ok &= bind(api.unmute, internal, "UnmuteInputDevice", 0);
  ok &= bind(api.listenerVolume, listener, "get_volume", 0);
  ok &= bind(api.setListenerVolume, listener, "set_volume", 1);
  ok &= bind(api.setSourceMute, source, "set_mute", 1);
  ok &= bind(api.findObjectsOfType, object, "FindObjectsOfType", 1);
  api.sourceType = il2cpp::type_object(source);
  api.playerVoiceType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PlayerVoice"));
  ok &= api.sourceType && api.playerVoiceType;
  api.setMultiplier = reinterpret_cast<decltype(api.setMultiplier)>(game::game_assembly_address(kSetMultiplierRva));
  ok &= api.setMultiplier != nullptr;
  PG_LOG("senses: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(api.unmute, reinterpret_cast<void*>(&detour_unmute), reinterpret_cast<void**>(&g_unmute),
                       "VivoxServiceInternal.UnmuteInputDevice") &&
         hooks::create(game::game_assembly_address(kVoiceVolumeUpdateRva), reinterpret_cast<void*>(&detour_voice_volume),
                       reinterpret_cast<void**>(&g_voiceVolume), "VoiceVolume.Update");
}

void tick(session::Session& s, void* ghost) {
  if (!s.local_is_ghost() || !game::level_controller()) {
    release();
    return;
  }
  if (!g_muted) set_mic_muted(true);
  if (!ghost) {  // still being chosen: the truck's conversation is not the ghost's to hear
    silence(true);
    return;
  }
  silence(false);
  const ULONGLONG now = GetTickCount64();
  if (now >= g_nextTapScan) {
    mute_global_taps();
    g_nextTapScan = now + kTapScanMs;
  }
}

void teardown() { release(); }

}  // namespace pg::senses
