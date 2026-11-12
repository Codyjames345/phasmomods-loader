#pragma once

// The Emotes mod's one message, over its own channel (sdk/README.md, "What the loader does for you"): an emote starting
// where the emoter stands, the body's facing as it turns, and the end. The emoter's client decides all three; every
// other client plays what it is told on the emoter's body. A message names its emote by the key from its pack's id
// (catalogue.h), never by a wheel index, so a client whose list differs can look the emote up in its own table and
// pass over one it does not hold. Free of game calls.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

namespace pm::emotes {

/// The first byte of every message, so a stray payload on the channel is passed over.
inline constexpr uint8_t kMagic = 'E';
/// How far the body must turn since the last Facing sent before another is sent, in degrees. A starting value.
inline constexpr float kFacingSendDegrees = 5.0f;

/// What a message says. Move is reserved for walking during an emote: nothing sends it yet, and a client that receives
/// one does nothing with it yet (a later task adds the walking).
enum class Kind : uint8_t { Start = 1, Facing = 2, End = 3, Move = 4 };

/// A message: for Start, the emote's key, where the emoter stands and the body's yaw; for Facing, the yaw; End
/// carries nothing it needs. `moving` says the emoter is walking (bit 0 of the flags byte).
struct EmoteMsg {
  Kind kind = Kind::Start;
  uint32_t key = 0;
  bool moving = false;
  float x = 0, y = 0, z = 0;
  float yaw = 0;
};

/// The message's bytes: the magic, the kind, the flags, the key as four little-endian bytes, then x, y, z and yaw as
/// little-endian floats.
inline constexpr size_t kMessageBytes = 23;

inline std::vector<uint8_t> pack(const EmoteMsg& m) {
  std::vector<uint8_t> out(kMessageBytes);
  out[0] = kMagic;
  out[1] = static_cast<uint8_t>(m.kind);
  out[2] = m.moving ? 1 : 0;  // bit 0: the emoter is walking. No other flag is defined yet.
  out[3] = static_cast<uint8_t>(m.key & 255u);
  out[4] = static_cast<uint8_t>((m.key >> 8) & 255u);
  out[5] = static_cast<uint8_t>((m.key >> 16) & 255u);
  out[6] = static_cast<uint8_t>((m.key >> 24) & 255u);
  const float values[4] = {m.x, m.y, m.z, m.yaw};
  std::memcpy(out.data() + 7, values, sizeof values);
  return out;
}

/// Reads a message; nothing for one of another length, without the magic, of an unknown kind, with a flag bit that is
/// not the walking flag, or with a number that is not finite. The key is not checked against any emote list: the
/// session passes over keys its own table does not hold.
inline std::optional<EmoteMsg> unpack(std::span<const uint8_t> data) {
  if (data.size() != kMessageBytes || data[0] != kMagic) return std::nullopt;
  if (data[1] < static_cast<uint8_t>(Kind::Start) || data[1] > static_cast<uint8_t>(Kind::Move)) return std::nullopt;
  if ((data[2] & ~1) != 0) return std::nullopt;  // only bit 0 means anything
  const uint32_t key = static_cast<uint32_t>(data[3]) | (static_cast<uint32_t>(data[4]) << 8) |
                       (static_cast<uint32_t>(data[5]) << 16) | (static_cast<uint32_t>(data[6]) << 24);
  float values[4];
  std::memcpy(values, data.data() + 7, sizeof values);
  for (const float v : values)
    if (!std::isfinite(v)) return std::nullopt;
  return EmoteMsg{static_cast<Kind>(data[1]), key, (data[2] & 1) != 0, values[0], values[1], values[2], values[3]};
}

/// Whether a client sees and hears an emote (the user's rule, as the game's for a dead player's voice): a living
/// emoter's by everyone, an emoter who is down by the down only. Down covers a death that has begun as well as one the
/// game has marked: the dying investigator's body is the game's, not the dance's (2026-11-10).
inline bool shows_to(bool senderDown, bool receiverDown) { return !senderDown || receiverDown; }

}  // namespace pm::emotes
