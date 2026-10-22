#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <vector>

// What the Echo needs from voices, kept free of game calls so it can be tested offline: a short
// recording of each speaker at telephone quality, the last thing they said, and the G.711 mu-law
// coding and chunking that carry it to the other clients.
namespace pg::custom::voice {

constexpr int kRate = 8000;                   // samples a second, as kept and sent
constexpr int kKeepSeconds = 60;              // how far back a speaker is remembered
constexpr int kFrame = kRate / 50;            // 20 ms of samples: the unit of speech detection
constexpr float kSpeechLevel = 0.02f;         // RMS above which a frame counts as speech
constexpr int kGapFrames = 15;                // 300 ms of quiet still within one phrase
constexpr int kMinPhraseFrames = 25;          // 0.5 s
constexpr int kMaxPhraseFrames = 150;         // 3 s
constexpr int kSettledFrames = 20;            // a phrase counts once 400 ms have passed in silence

/** One speaker's recent audio, resampled to kRate mono, and which 20 ms frames held speech. */
class Recorder {
 public:
  /** Add interleaved float audio as Vivox delivers it (any rate and channel count). */
  void push(const float* data, int frames, int channels, int rate) {
    if (!data || frames <= 0 || channels <= 0 || rate <= 0) return;
    for (int i = 0; i < frames; ++i) {
      float mono = 0;
      for (int c = 0; c < channels; ++c) mono += data[i * channels + c];
      acc_ += mono / static_cast<float>(channels);
      accCount_++;
      phase_ += kRate;
      if (phase_ < rate) continue;  // average the source samples that fall in one output sample
      phase_ -= rate;
      add_sample(acc_ / static_cast<float>(accCount_));
      acc_ = 0;
      accCount_ = 0;
    }
  }

  /** Whether any of the last `frames` 20 ms frames held speech. */
  bool spoke_within(int frames) const {
    const int n = static_cast<int>(speech_.size());
    for (int i = std::max(0, n - frames); i < n; ++i)
      if (speech_[i]) return true;
    return false;
  }

  /**
   * The last finished phrase (speech of 0.5 to 3 s, gaps under 300 ms, followed by 400 ms of quiet),
   * as samples at kRate; false when there is none in what is remembered.
   */
  bool last_phrase(std::vector<float>& out) const {
    const int n = static_cast<int>(speech_.size());
    int end = n - 1;
    int quiet = 0;
    while (end >= 0 && !speech_[end]) --end, ++quiet;
    if (end < 0 || quiet < kSettledFrames) return false;  // still talking, or nothing said
    int start = end, gap = 0;
    for (int i = end; i >= 0; --i) {
      if (speech_[i]) {
        start = i;
        gap = 0;
      } else if (++gap > kGapFrames) {
        break;
      }
    }
    const int length = end - start + 1;
    if (length < kMinPhraseFrames) return false;
    const int first = std::max(start, end + 1 - kMaxPhraseFrames);  // the end of a long speech
    out.assign(samples_.begin() + static_cast<long long>(first) * kFrame,
               samples_.begin() + static_cast<long long>(end + 1) * kFrame);
    return true;
  }

 private:
  void add_sample(float s) {
    samples_.push_back(s);
    frameEnergy_ += s * s;
    if (++frameFill_ < kFrame) return;
    speech_.push_back(std::sqrt(frameEnergy_ / kFrame) >= kSpeechLevel);
    frameEnergy_ = 0;
    frameFill_ = 0;
    while (speech_.size() > static_cast<size_t>(kKeepSeconds) * 50) {
      speech_.pop_front();
      samples_.erase(samples_.begin(), samples_.begin() + kFrame);
    }
  }

  std::deque<float> samples_;  // whole frames only, oldest first (plus the frame being filled)
  std::deque<bool> speech_;
  float frameEnergy_ = 0;
  int frameFill_ = 0;
  float acc_ = 0;
  int accCount_ = 0;
  int phase_ = 0;
};

/** G.711 mu-law: one byte a sample. */
inline uint8_t encode(float sample) {
  constexpr int kBias = 0x84, kClip = 32635;
  int pcm = static_cast<int>(std::clamp(sample, -1.0f, 1.0f) * 32767.0f);
  const int sign = pcm < 0 ? 0x80 : 0;
  if (sign) pcm = -pcm;
  pcm = std::min(pcm, kClip) + kBias;
  int exponent = 7;
  for (int mask = 0x4000; exponent > 0 && !(pcm & mask); mask >>= 1) --exponent;
  const int mantissa = (pcm >> (exponent + 3)) & 0x0F;
  return static_cast<uint8_t>(~(sign | (exponent << 4) | mantissa));
}

inline float decode(uint8_t byte) {
  const int u = ~byte & 0xFF;
  const int exponent = (u >> 4) & 0x07, mantissa = u & 0x0F;
  const int magnitude = (((mantissa << 3) + 0x84) << exponent) - 0x84;
  return static_cast<float>((u & 0x80) ? -magnitude : magnitude) / 32768.0f;
}

constexpr size_t kChunkBytes = 480;  // fits a mod message with its header

/** A clip split into chunks of kChunkBytes, at most 255 of them. */
inline std::vector<std::vector<uint8_t>> chunks(const std::vector<float>& clip) {
  std::vector<std::vector<uint8_t>> out;
  for (size_t at = 0; at < clip.size() && out.size() < 255; at += kChunkBytes) {
    std::vector<uint8_t> c;
    for (size_t i = at; i < std::min(clip.size(), at + kChunkBytes); ++i) c.push_back(encode(clip[i]));
    out.push_back(std::move(c));
  }
  return out;
}

/** Collects one clip's chunks as they arrive, in any order. */
class Assembler {
 public:
  void begin(uint16_t clip, int count) {
    clip_ = clip;
    parts_.assign(static_cast<size_t>(std::clamp(count, 0, 255)), {});
    have_ = 0;
  }
  /** Add a chunk; true when the clip is complete (then `out` holds its samples). */
  bool add(uint16_t clip, int index, int count, const uint8_t* data, size_t size, std::vector<float>& out) {
    if (clip != clip_ || parts_.empty() || count != static_cast<int>(parts_.size())) begin(clip, count);
    if (index < 0 || index >= static_cast<int>(parts_.size()) || !parts_[index].empty() || size == 0) return false;
    parts_[index].assign(data, data + size);
    if (++have_ < static_cast<int>(parts_.size())) return false;
    out.clear();
    for (const auto& p : parts_)
      for (uint8_t b : p) out.push_back(decode(b));
    parts_.clear();
    return true;
  }

 private:
  uint16_t clip_ = 0;
  std::vector<std::vector<uint8_t>> parts_;
  int have_ = 0;
};

}  // namespace pg::custom::voice
