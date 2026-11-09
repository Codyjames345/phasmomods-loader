#include "talk_loudness.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>

#include "Player.h"
#include "runtime.h"

namespace pm::api::talk {

float loudness_of_rms(float rms) { return std::clamp(rms * kEnergyPerRms, 0.0f, 1.0f) - kEnergyOffset; }

float loudness_of(std::span<const float> samples) {
  if (samples.empty()) return -kEnergyOffset;
  double sum = 0.0;
  for (const float s : samples) sum += static_cast<double>(s) * s;
  return loudness_of_rms(static_cast<float>(std::sqrt(sum / static_cast<double>(samples.size()))));
}

Envelope envelope_of_rms(std::span<const float> rms) {
  Envelope env;
  env.windows.reserve(rms.size());
  for (const float r : rms) env.windows.push_back(loudness_of_rms(r));
  return env;
}

std::vector<float> parse_rms(std::string_view text) {
  std::vector<float> out;
  while (!text.empty()) {
    const size_t nl = text.find('\n');
    std::string line(text.substr(0, nl));
    text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
    char* end = nullptr;
    const float v = std::strtof(line.c_str(), &end);
    if (end != line.c_str() && std::isfinite(v)) out.push_back(v);
  }
  return out;
}

float Envelope::at(float seconds) const {
  if (windows.empty()) return -kEnergyOffset;
  const float index = std::max(0.0f, seconds) / kWindowSeconds;
  const size_t i = static_cast<size_t>(index);
  return i < windows.size() ? windows[i] : -kEnergyOffset;
}

Envelope envelope_of(std::span<const float> samples, int sampleRate) {
  Envelope env;
  if (sampleRate <= 0) return env;
  const size_t window = std::max<size_t>(1, static_cast<size_t>(static_cast<float>(sampleRate) * kWindowSeconds));
  for (size_t at = 0; at < samples.size(); at += window)
    env.windows.push_back(loudness_of(samples.subspan(at, std::min(window, samples.size() - at))));
  return env;
}

void add_talk_loudness(void* player, float loudness) {
  if (!player || !alive(player)) return;
  void* audio = phasmo::Player::audio.get(player);
  if (!audio) return;
  if (phasmo::PlayerAudio::loudness.get(audio) < loudness) phasmo::PlayerAudio::loudness.set(audio, loudness);
}

}  // namespace pm::api::talk
