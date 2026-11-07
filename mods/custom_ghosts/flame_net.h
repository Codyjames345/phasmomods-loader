#pragma once

#include <cstdint>
#include <cstring>
#include <vector>

// Messages of the flame ghosts on Custom Ghosts' channel: 'C' 'G' 'F' kind, then the kind's body.
// The Echo's own messages start 'C' 'G' with a version byte, never 'F', so the two never mix.
namespace pm::custom::net_msg {

enum Kind : uint8_t {
  kDecoys = 1,  // host to all: the Hydra's false heads, count:u8 then x y z yaw (f32 each) per head
  kDouble = 2,  // host to all: the Hydra's second apparition, x y z yaw
  kHit = 3,     // host to one: a Lithobolia's throw hit you, sanity:f32
  kStrike = 4,  // host to all: the Tempest's lightning strikes now, no body
  // 5 is unused: each client works out its own Mesmer view from the ghost's name, the appearance and its actor.
  kViewing = 6, // a client to the host: its player has the journal's media pages open
  kForget = 7,  // host to all: the Lethe makes a piece of media fade, list:u8 (0 photo, 1 video, 2 sound) slot:u8
  kAnger = 8,   // host to all: the Draugr's anger, angered:u8 (0 or 1)
  kSeen = 9,    // host to all: whether anyone looks at the Brownie, seen:u8 (0 or 1)
  kDoors = 10,  // host to all: the Asterion breaks these doors open, count:u8 then ViewID:u32 each
  kDrag = 11,   // host to all: the Draugr drags a thing, or stops, ViewID:u32 then dragging:u8 (0 or 1)
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

inline std::vector<uint8_t> anger(bool angered) {
  std::vector<uint8_t> out = header(kAnger);
  out.push_back(angered ? 1 : 0);
  return out;
}

/** Whether the Draugr is angered, as the host says. */
inline bool read_anger(const uint8_t* data, size_t size, bool& angered) {
  if (kind_of(data, size) != kAnger || size != 5 || data[4] > 1) return false;
  angered = data[4] == 1;
  return true;
}

inline std::vector<uint8_t> seen(bool seen) {
  std::vector<uint8_t> out = header(kSeen);
  out.push_back(seen ? 1 : 0);
  return out;
}

/** Whether anyone looks at the Brownie, as the host says. */
inline bool read_seen(const uint8_t* data, size_t size, bool& seen) {
  if (kind_of(data, size) != kSeen || size != 5 || data[4] > 1) return false;
  seen = data[4] == 1;
  return true;
}

/** The doors the Asterion breaks open, by their PhotonView's ViewID (at most 255). */
inline std::vector<uint8_t> doors(const std::vector<uint32_t>& ids) {
  std::vector<uint8_t> out = header(kDoors);
  const size_t n = ids.size() > 255 ? 255 : ids.size();
  out.push_back(static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) put(out, ids[i]);
  return out;
}

/** The doors the host says the Asterion broke open: false for another kind, a bad length or none. */
inline bool read_doors(const uint8_t* data, size_t size, std::vector<uint32_t>& ids) {
  if (kind_of(data, size) != kDoors || size < 5 || data[4] == 0 || size != 5 + 4 * size_t{data[4]}) return false;
  ids.clear();
  size_t at = 5;
  for (int i = 0; i < data[4]; ++i) {
    uint32_t id = 0;
    take(data, size, at, id);
    ids.push_back(id);
  }
  return true;
}

/** The thing the Draugr drags, by its PhotonView's ViewID, and whether the drag goes on or is over. */
inline std::vector<uint8_t> drag(uint32_t id, bool dragging) {
  std::vector<uint8_t> out = header(kDrag);
  put(out, id);
  out.push_back(dragging ? 1 : 0);
  return out;
}

/** The host's drag message: false for another kind or a bad length. */
inline bool read_drag(const uint8_t* data, size_t size, uint32_t& id, bool& dragging) {
  size_t at = 4;
  if (kind_of(data, size) != kDrag || size != 9 || data[8] > 1 || !take(data, size, at, id)) return false;
  dragging = data[8] == 1;
  return true;
}

}  // namespace pm::custom::net_msg
