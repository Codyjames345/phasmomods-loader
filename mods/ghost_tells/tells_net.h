#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

// Messages on Ghost Tells' channel: 'G' 'T' kind, then the kind's body. The host makes every change
// to the ghost; a client tells it what only the client saw, and the host tells every client what to
// show.
namespace pg::tells::net {

enum Kind : uint8_t {
  kName = 1,        // a client to the host: its player said the ghost's name near it (the Spirit)
  kTrap = 2,        // a client to the host: its incense set off the Yurei's trap
  kGoryoShow = 3,   // host to all: the Goryo's camera-only figure, on:u8
  kOnryoCount = 4,  // host to all: the Onryo's count of blown-out flames, count:u8
  kOnryoSpite = 5,  // host to all: a flame the Onryo blew out will not catch, view:i32 tenths:u16 (seconds left)
};

inline std::vector<uint8_t> header(Kind kind) { return {'G', 'T', kind}; }

/** The kind of one of this mod's messages, or 0 for anything else. */
inline uint8_t kind_of(const uint8_t* data, size_t size) {
  return data && size >= 3 && data[0] == 'G' && data[1] == 'T' ? data[2] : 0;
}

inline std::vector<uint8_t> name_heard() { return header(kName); }
inline bool read_name_heard(const uint8_t* data, size_t size) { return kind_of(data, size) == kName && size == 3; }

inline std::vector<uint8_t> trapped() { return header(kTrap); }
inline bool read_trapped(const uint8_t* data, size_t size) { return kind_of(data, size) == kTrap && size == 3; }

inline std::vector<uint8_t> goryo_show(bool on) {
  std::vector<uint8_t> out = header(kGoryoShow);
  out.push_back(on ? 1 : 0);
  return out;
}

inline bool read_goryo_show(const uint8_t* data, size_t size, bool& on) {
  if (kind_of(data, size) != kGoryoShow || size != 4) return false;
  on = data[3] != 0;
  return true;
}

inline std::vector<uint8_t> onryo_count(int count) {
  std::vector<uint8_t> out = header(kOnryoCount);
  out.push_back(static_cast<uint8_t>(count < 0 ? 0 : count > 255 ? 255 : count));
  return out;
}

inline bool read_onryo_count(const uint8_t* data, size_t size, int& count) {
  if (kind_of(data, size) != kOnryoCount || size != 4) return false;
  count = data[3];
  return true;
}

inline std::vector<uint8_t> onryo_spite(int view, float seconds) {
  std::vector<uint8_t> out = header(kOnryoSpite);
  uint8_t b[4];
  std::memcpy(b, &view, 4);
  out.insert(out.end(), b, b + 4);
  const float tenths = std::floor(seconds * 10.0f);
  const uint16_t t = static_cast<uint16_t>(tenths < 0 ? 0 : tenths > 65535 ? 65535 : tenths);
  out.push_back(static_cast<uint8_t>(t & 0xFF));
  out.push_back(static_cast<uint8_t>(t >> 8));
  return out;
}

inline bool read_onryo_spite(const uint8_t* data, size_t size, int& view, float& seconds) {
  if (kind_of(data, size) != kOnryoSpite || size != 9) return false;
  std::memcpy(&view, data + 3, 4);
  seconds = static_cast<float>(data[7] | (data[8] << 8)) / 10.0f;
  return true;
}

}  // namespace pg::tells::net
