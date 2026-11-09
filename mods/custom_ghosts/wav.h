#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

// Reads a WAV file held in memory: 16-bit PCM, one or two channels, any sample rate. Nothing else is
// accepted (the Squatter's recordings are made in this one format). Free of game calls.
namespace pm::custom::wav {

/** Decoded audio: float samples in -1 to 1, interleaved by channel, as AudioClip.SetData takes them. */
struct Pcm {
  std::vector<float> samples;  // frames * channels values
  int channels = 0;            // 1 or 2
  int frequency = 0;           // samples per second per channel
  size_t frames() const { return channels > 0 ? samples.size() / static_cast<size_t>(channels) : 0; }
};

namespace detail {
inline uint32_t le32(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 |
         static_cast<uint32_t>(p[3]) << 24;
}
inline uint16_t le16(const uint8_t* p) { return static_cast<uint16_t>(p[0] | p[1] << 8); }
}  // namespace detail

/**
 * Decodes `file` into `out`. Every chunk length is checked against the file's size: a chunk that claims
 * more than the file holds, a missing or late "fmt " chunk, or a missing "data" chunk is refused, an
 * unknown chunk is skipped (odd lengths are padded to even, as RIFF says), and a trailing partial frame
 * in "data" is dropped. Refused: any format but 16-bit integer PCM, more than two channels, a zero
 * sample rate, and no audio at all.
 * @param file the whole file's bytes.
 * @param out receives the audio on success; left empty on failure.
 * @param reason when not null, receives why a file was refused (for the log).
 * @return true when `out` holds audio. Never throws or reads outside `file`. Any thread.
 */
inline bool decode(std::span<const uint8_t> file, Pcm* out, std::string* reason = nullptr) {
  using namespace detail;
  auto refuse = [&](const char* why) {
    if (reason) *reason = why;
    if (out) *out = Pcm{};
    return false;
  };
  if (!out) return refuse("no output");
  *out = Pcm{};
  if (file.size() < 12) return refuse("shorter than a WAV header");
  const uint8_t* b = file.data();
  if (std::string_view(reinterpret_cast<const char*>(b), 4) != "RIFF" ||
      std::string_view(reinterpret_cast<const char*>(b + 8), 4) != "WAVE")
    return refuse("not a RIFF WAVE file");

  bool haveFmt = false;
  int channels = 0, frequency = 0;
  size_t pos = 12;
  while (pos <= file.size() && file.size() - pos >= 8) {
    const std::string_view id(reinterpret_cast<const char*>(b + pos), 4);
    const size_t len = le32(b + pos + 4);
    const size_t body = pos + 8;
    const size_t left = file.size() - body;
    if (id == "fmt ") {
      if (len > left) return refuse("fmt chunk runs past the end of the file");
      if (len < 16) return refuse("fmt chunk is too short");
      const uint16_t tag = le16(b + body), ch = le16(b + body + 2), bits = le16(b + body + 14);
      const uint32_t rate = le32(b + body + 4);
      if (tag != 1) return refuse("not integer PCM");
      if (bits != 16) return refuse("not 16-bit");
      if (ch != 1 && ch != 2) return refuse("not mono or stereo");
      if (rate == 0 || rate > static_cast<uint32_t>(std::numeric_limits<int>::max())) return refuse("bad sample rate");
      haveFmt = true;
      channels = ch;
      frequency = static_cast<int>(rate);
    } else if (id == "data") {
      if (!haveFmt) return refuse("data chunk before the fmt chunk");
      if (len > left) return refuse("data chunk runs past the end of the file");
      const size_t frames = len / (static_cast<size_t>(channels) * 2);
      if (frames == 0) return refuse("no audio in the data chunk");
      out->samples.resize(frames * static_cast<size_t>(channels));
      const uint8_t* p = b + body;
      for (float& s : out->samples) {
        s = static_cast<float>(static_cast<int16_t>(le16(p))) / 32768.0f;
        p += 2;
      }
      out->channels = channels;
      out->frequency = frequency;
      return true;
    }
    if (len > left) break;  // an unknown chunk past the end: nothing more can be read
    pos = body + len + (len & 1);  // pos <= size + 1 here
  }
  return refuse(haveFmt ? "no data chunk" : "no fmt chunk");
}

}  // namespace pm::custom::wav
