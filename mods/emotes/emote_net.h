#pragma once

// The Emotes mod's one message, over its own channel (sdk/README.md, "What the loader does for you"): an emote starting
// where the emoter stands, the body's facing as it turns, and the end. The emoter's client decides all three; every
// other client plays what it is told on the emoter's body. Free of game calls.

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

#include "emote_rule.h"

namespace pm::emotes {

/// The first byte of every message, so a stray payload on the channel is passed over.
inline constexpr uint8_t kMagic = 'E';
/// How far the body must turn since the last Facing sent before another is sent, in degrees. A starting value.
inline constexpr float kFacingSendDegrees = 5.0f;

/// What a message says.
enum class Kind : uint8_t { Start = 1, Facing = 2, End = 3 };

/// A message: for Start, the emote's segment, where the emoter stands and the body's yaw; for Facing, the yaw; End
/// carries nothing it needs.
struct EmoteMsg {
  Kind kind = Kind::Start;
  uint8_t emote = 0;
  float x = 0, y = 0, z = 0;
  float yaw = 0;
};

/// The message's bytes: the magic, the kind, the emote, then x, y, z and yaw as little-endian floats.
inline constexpr size_t kMessageBytes = 3 + 4 * sizeof(float);

inline std::vector<uint8_t> pack(const EmoteMsg& m) {
  std::vector<uint8_t> out(kMessageBytes);
  out[0] = kMagic;
  out[1] = static_cast<uint8_t>(m.kind);
  out[2] = m.emote;
  const float values[4] = {m.x, m.y, m.z, m.yaw};
  std::memcpy(out.data() + 3, values, sizeof values);
  return out;
}

/// Reads a message; nothing for one of another length, without the magic, of an unknown kind, an emote past the wheel,
/// or a number that is not finite.
inline std::optional<EmoteMsg> unpack(std::span<const uint8_t> data) {
  if (data.size() != kMessageBytes || data[0] != kMagic) return std::nullopt;
  if (data[1] < static_cast<uint8_t>(Kind::Start) || data[1] > static_cast<uint8_t>(Kind::End)) return std::nullopt;
  if (data[2] >= kSegments) return std::nullopt;
  float values[4];
  std::memcpy(values, data.data() + 3, sizeof values);
  for (const float v : values)
    if (!std::isfinite(v)) return std::nullopt;
  return EmoteMsg{static_cast<Kind>(data[1]), data[2], values[0], values[1], values[2], values[3]};
}

/// Whether a client sees and hears an emote (the user's rule, as the game's for a dead player's voice): a living
/// emoter's by everyone, a dead emoter's only by the dead.
inline bool shows_to(bool senderDead, bool receiverDead) { return !senderDead || receiverDead; }

}  // namespace pm::emotes
