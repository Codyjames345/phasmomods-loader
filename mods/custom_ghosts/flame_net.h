#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// Messages of the flame ghosts on Custom Ghosts' channel: 'C' 'G' 'F' kind, then the kind's body.
// The Echo's own messages start 'C' 'G' with a version byte, never 'F', so the two never mix.
namespace pg::custom::net_msg {

enum Kind : uint8_t {
  kDecoys = 1,  // host to all: the Hydra's false heads, count:u8 then x y z yaw (f32 each) per head
  kDouble = 2,  // host to all: the Hydra's second apparition, x y z yaw
  kHit = 3,     // host to one: a Lithobolia's throw hit you, sanity:f32
  kStrike = 4,  // host to all: the Tempest's lightning strikes now, no body
  // 5 is unused: each client works out its own Mesmer view from the appearance and its actor.
  kViewing = 6, // a client to the host: its player has the journal's media pages open
  kForget = 7,  // host to all: the Lethe makes a piece of media fade, list:u8 (0 photo, 1 video, 2 sound) slot:u8
};

inline std::vector<uint8_t> header(Kind kind) { return {'C', 'G', 'F', kind}; }

/** The kind of a flame-ghost message, or 0 for any other message on the channel. */
inline uint8_t kind_of(const uint8_t* data, size_t size) {
  return data && size >= 4 && data[0] == 'C' && data[1] == 'G' && data[2] == 'F' ? data[3] : 0;
}

inline void put(std::vector<uint8_t>& out, float v) {
  uint8_t b[4];
  std::memcpy(b, &v, 4);
  out.insert(out.end(), b, b + 4);
}

inline void put(std::vector<uint8_t>& out, uint32_t v) {
  uint8_t b[4];
  std::memcpy(b, &v, 4);
  out.insert(out.end(), b, b + 4);
}

/** Read a 4-byte value at `at`, advancing it; false when the message is too short. */
template <class T>
bool take(const uint8_t* data, size_t size, size_t& at, T& out) {
  static_assert(sizeof(T) == 4);
  if (at + 4 > size) return false;
  std::memcpy(&out, data + at, 4);
  at += 4;
  return true;
}

inline std::vector<uint8_t> hit(float sanity) {
  std::vector<uint8_t> out = header(kHit);
  put(out, sanity);
  return out;
}

inline bool read_hit(const uint8_t* data, size_t size, float& sanity) {
  size_t at = 4;
  return kind_of(data, size) == kHit && take(data, size, at, sanity) && at == size;
}

inline std::vector<uint8_t> strike() { return header(kStrike); }

inline bool read_strike(const uint8_t* data, size_t size) { return kind_of(data, size) == kStrike && size == 4; }

inline std::vector<uint8_t> forget(int list, int slot) {
  std::vector<uint8_t> out = header(kForget);
  out.push_back(static_cast<uint8_t>(list));
  out.push_back(static_cast<uint8_t>(slot));
  return out;
}

/** A fade of slot `slot` of photo page list `list` (0 photos, 1 videos, 2 sounds). */
inline bool read_forget(const uint8_t* data, size_t size, int& list, int& slot) {
  if (kind_of(data, size) != kForget || size != 6 || data[4] >= 3) return false;
  list = data[4];
  slot = data[5];
  return true;
}

}  // namespace pg::custom::net_msg
