// Offline tests for a sound counted as a player talking (sdk/api/talk_loudness.h): its loudness in the game's own scale
// (Vivox energy less 0.4, PlayerAudio.Update), its envelope over a clip, and over the fake backend the raise of
// PlayerAudio.loudness that the ghost's voice test (PlayerAudio.IsTalkingLoudly, mimicked here as decompiled) then
// hears. The game's side is in sdk/systems/voice.md, "How loud a player talks". Run: build\talk_loudness_test.exe
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"
#include "talk_loudness.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace talk = pm::api::talk;
namespace map = pm::map;
namespace fake = pm::fake;

std::vector<float> sine(float amplitude, size_t n) {
  std::vector<float> s(n);
  for (size_t i = 0; i < n; ++i) s[i] = amplitude * std::sin(static_cast<float>(i) * 0.05f);
  return s;
}

void silence_is_not_talking() {
  const std::vector<float> zeros(4800, 0.0f);
  CHECK(talk::loudness_of(zeros) < talk::kTalkingLoudly);
  CHECK(talk::loudness_of({}) < talk::kTalkingLoudly);
}

void a_loud_sound_is_talking() {
  CHECK(talk::loudness_of(sine(1.0f, 4800)) > talk::kTalkingLoudly);
  // Quieter is less loud; the scale is the game's: energy less 0.4, so never above 0.6.
  CHECK(talk::loudness_of(sine(0.05f, 4800)) < talk::loudness_of(sine(0.5f, 4800)));
  CHECK(talk::loudness_of(sine(1.0f, 4800)) <= 0.6f + 1e-5f);
  CHECK(talk::loudness_of(sine(1.0f, 4800)) >= -0.4f);
}

void an_envelope_reads_from_measured_rms() {
  // One RMS a window, as the emote pack tool writes them (LoopFinder.RmsWindows): the same scale as measuring the samples.
  CHECK(std::fabs(talk::loudness_of_rms(0.0f) - talk::loudness_of({})) < 1e-6f);
  const std::vector<float> loud = sine(1.0f, 4800);
  double sum = 0;
  for (float s : loud) sum += s * s;
  const float rms = static_cast<float>(std::sqrt(sum / loud.size()));
  CHECK(std::fabs(talk::loudness_of_rms(rms) - talk::loudness_of(loud)) < 1e-5f);
  const std::vector<float> windows{0.0f, 0.0f, 0.5f};
  const talk::Envelope env = talk::envelope_of_rms(windows);
  CHECK(env.windows.size() == 3);
  CHECK(env.at(0.05f) < talk::kTalkingLoudly);
  CHECK(env.at(0.25f) > talk::kTalkingLoudly);
  CHECK(talk::parse_rms("0.1\n0.25\r\nnot\n\n0.5\n").size() == 3);
}

void the_envelope_follows_the_clip() {
  // Half a second of silence, then half a second loud, at 1000 samples a second.
  std::vector<float> clip(1000, 0.0f);
  for (size_t i = 500; i < 1000; ++i) clip[i] = std::sin(static_cast<float>(i) * 0.3f);
  const talk::Envelope env = talk::envelope_of(clip, 1000);
  CHECK(!env.windows.empty());
  CHECK(env.at(0.1f) < talk::kTalkingLoudly);
  CHECK(env.at(0.8f) > talk::kTalkingLoudly);
  CHECK(env.at(-1.0f) == env.at(0.0f));   // before the start: the first window
  CHECK(env.at(99.0f) < talk::kTalkingLoudly);  // past the end: silent
  CHECK(talk::envelope_of({}, 1000).at(0.5f) < talk::kTalkingLoudly);
  CHECK(talk::envelope_of(clip, 0).windows.empty());
}

// ---- Over the fake game ----

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
constexpr int32_t kPlayerAudio = 0x08, kLoudness = 0x10;
Obj g_player, g_audio;
float& loudness() { return *reinterpret_cast<float*>(reinterpret_cast<unsigned char*>(&g_audio) + kLoudness); }

// PlayerAudio.IsTalkingLoudly as decompiled (no walkie-talkie here): above the threshold, it zeroes and answers true.
bool fake_is_talking_loudly(void*, float threshold, void*) {
  if (loudness() <= threshold) return false;
  loudness() = 0.0f;
  return true;
}

void give() {
  fake::reset();
  fake::field(map::Player_audio, kPlayerAudio);
  fake::field(map::PlayerAudio_loudness, kLoudness);
  fake::method(map::PlayerAudio_IsTalkingLoudly, reinterpret_cast<void*>(&fake_is_talking_loudly));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  *reinterpret_cast<void**>(reinterpret_cast<unsigned char*>(&g_player) + kPlayerAudio) = &g_audio;
  loudness() = -0.4f;  // a silent voice, as PlayerAudio.Update leaves it
}

void the_ghost_hears_the_sound_as_talking() {
  give();
  void* audio = phasmo::Player::audio.get(&g_player);
  CHECK(!phasmo::PlayerAudio::IsTalkingLoudly(audio, 0.1f));
  talk::add_talk_loudness(&g_player, 0.3f);
  CHECK(phasmo::PlayerAudio::IsTalkingLoudly(audio, 0.1f));
  CHECK(!phasmo::PlayerAudio::IsTalkingLoudly(audio, 0.1f));  // consumed, as a voice's is
}

void added_loudness_never_lowers_a_voice() {
  give();
  loudness() = 0.5f;  // shouting
  talk::add_talk_loudness(&g_player, 0.2f);
  CHECK(loudness() == 0.5f);
  talk::add_talk_loudness(&g_player, 0.55f);
  CHECK(loudness() == 0.55f);
  talk::add_talk_loudness(nullptr, 0.9f);  // no player: nothing
}

void the_raise_keeps_the_games_pace() {
  talk::Pace pace;
  CHECK(pace.due(0.0f));   // the first raise at once
  CHECK(!pace.due(0.5f));
  CHECK(!pace.due(0.99f));
  CHECK(pace.due(1.0f));   // then once a second, as PlayerAudio.Update refreshes a voice
  CHECK(!pace.due(1.5f));
  CHECK(pace.due(2.2f));
}

}  // namespace

int main() {
  silence_is_not_talking();
  a_loud_sound_is_talking();
  the_envelope_follows_the_clip();
  an_envelope_reads_from_measured_rms();
  the_ghost_hears_the_sound_as_talking();
  added_loudness_never_lowers_a_voice();
  the_raise_keeps_the_games_pace();
  std::printf("%s\n", g_failures ? "talk_loudness tests FAILED" : "talk_loudness tests passed");
  return g_failures ? 1 : 0;
}
