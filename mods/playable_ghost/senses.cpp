#include "senses.h"

#include <windows.h>

#include <algorithm>
#include <vector>

#include "PlayerVoice.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::senses {
namespace {

constexpr ULONGLONG kTapScanMs = 2000;

using phasmo::AudioListener;
using phasmo::VivoxServiceInternal;

bool g_muted = false;
bool g_silenced = false;  // listener volume zeroed while the ghost is chosen
float g_savedVolume = 1.0f;
std::vector<api::Held> g_mutedTaps;  // AudioSources of walkie-talkie taps we muted
ULONGLONG g_nextTapScan = 0;

void detour_unmute(api::Original<VivoxServiceInternal::UnmuteInputDevice> original, void* service) {
  if (g_muted) return;  // the ghost player's voice stays off for the whole contract
  original(service);
}

void set_mic_muted(bool muted) {
  void* vivox = phasmo::VivoxService::get_Instance();
  if (!vivox) return;
  g_muted = muted;
  // Unmuting goes through the game's own call, which detour_unmute lets pass now that g_muted is clear.
  muted ? VivoxServiceInternal::MuteInputDevice(vivox) : VivoxServiceInternal::UnmuteInputDevice(vivox);
  PM_LOG("senses: microphone %s", muted ? "muted" : "unmuted");
}

void silence(bool on) {
  if (on == g_silenced) return;
  if (on) g_savedVolume = AudioListener::get_volume();
  AudioListener::set_volume(on ? 0.0f : g_savedVolume);
  g_silenced = on;
}

void mute_global_taps() {
  const api::RefArray voices{phasmo::Object::FindObjectsOfType(phasmo::PlayerVoice::type.type_object())};
  for (size_t i = 0, n = voices.size(); i < n; ++i) {
    void* voice = voices[i];
    void* tap = voice ? phasmo::PlayerVoice::globalTap.get(voice) : nullptr;
    void* source = game::alive(tap) ? phasmo::Component::GetComponent(tap, phasmo::AudioSource::type.type_object())
                                    : nullptr;
    if (!source) continue;
    phasmo::AudioSource::set_mute(source, true);
    if (std::find(g_mutedTaps.begin(), g_mutedTaps.end(), source) == g_mutedTaps.end()) g_mutedTaps.push_back(source);
  }
}

void release() {
  if (g_muted) set_mic_muted(false);
  silence(false);
  for (void* source : g_mutedTaps) {
    if (game::alive(source)) phasmo::AudioSource::set_mute(source, false);
  }
  g_mutedTaps.clear();
}

}  // namespace

bool create_hooks() {
  return api::hook<VivoxServiceInternal::UnmuteInputDevice>(&detour_unmute, "VivoxServiceInternal.UnmuteInputDevice",
                                                             api::Twins::CanonicalOnly);
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

}  // namespace pm::senses
