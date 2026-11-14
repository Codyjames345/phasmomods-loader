#pragma once

// Joining a synced emote, free of game calls (docs/superpowers/specs/2026-11-13-synced-emotes-design.md): where the
// second dancer stands, when the join prompt shows over a dancer, and the glide that takes the joiner to their spot.
// The glue (emote.cpp) reads the game into these and carries out what they say.

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

#include "emote_rule.h"

namespace pm::emotes {

/// How far from a dancer the prompt shows, in metres (camera to the dancer's chest). A starting value.
inline constexpr float kJoinRange = 3.0f;
/// How far off the view's direction the dancer's chest may be for the prompt, in degrees. A starting value.
inline constexpr float kJoinLookDegrees = 15.0f;
/// How long the glide into the partner's spot takes, in seconds: short, so a VR player is not moved for long. A
/// starting value.
inline constexpr float kGlideSeconds = 0.6f;

/// The second dancer's spot: `right` metres to the first dancer's right and `forward` in front of them, at their
/// height. Yaw in degrees, 0 along +z and 90 along +x (the view's yaw), so facing yaw 0 the right is +x.
inline Point partner_spot(Point first, float firstYaw, float right, float forward) {
  const float r = firstYaw * 0.01745329252f;
  const float fx = std::sin(r), fz = std::cos(r);  // forward
  const float rx = fz, rz = -fx;                   // right of it
  return {first.x + rx * right + fx * forward, first.y, first.z + rz * right + fz * forward};
}

/// The yaw that looks from `from` to `to`, in the same degrees as the facing (the user, 2026-11-14).
inline float yaw_toward(Point from, Point to) {
  return wrap_degrees(std::atan2(to.x - from.x, to.z - from.z) * 57.29577951f);
}

/// The yaws a pair dances with once the join has landed: side by side both bodies keep the facing the join was made
/// at; with `faceEachOther` each turns toward the other, the first dancer to the joiner's spot and the joiner to the
/// first dancer's body (the user, 2026-11-14).
struct PairFacing {
  float first = 0.0f;   ///< the first dancer's body
  float joiner = 0.0f;  ///< the joiner's body
};
inline PairFacing pair_facing(Point first, float fixedYaw, Point spot, bool faceEachOther) {
  if (!faceEachOther) return {fixedYaw, fixedYaw};
  return {yaw_toward(first, spot), yaw_toward(spot, first)};
}

/// The glide and the pair's turns ease the same way: smoothstep over the seconds, flat at both ends.
inline float ease_in_out(float t, float seconds) {
  const float s = seconds > 0.0f ? std::clamp(t / seconds, 0.0f, 1.0f) : 1.0f;
  return s * s * (3.0f - 2.0f * s);
}

/// What the prompt over one dancer depends on this frame.
struct PromptCheck {
  Point me;                 ///< the joiner's view (the camera), metres
  Point target;             ///< the dancer's chest
  float lookYaw = 0.0f;     ///< the view's yaw, degrees (0 along +z, 90 along +x)
  float lookPitch = 0.0f;   ///< the view's pitch, degrees, up positive
  bool canDance = false;    ///< this client could start an emote now (alive or dead as the dancer is, not dancing,
                            ///< no wheel, menu or journal)
  bool targetSynced = false;   ///< the dancer dances a synced emote's first part
  bool partFree = false;       ///< nobody has joined them yet
  bool targetInIntro = false;  ///< they are still in the emote's intro (no joining then)
  bool spotClear = false;    ///< the partner's spot fits the joiner, on ground
  bool pickupInWay = false;  ///< the game's own Pickup ray would act on something, so a press would do both
};

/// Whether the prompt shows: every condition of the design at once.
inline bool shows_prompt(const PromptCheck& c) {
  if (!c.canDance || !c.targetSynced || !c.partFree || c.targetInIntro || !c.spotClear || c.pickupInWay)
    return false;
  const float dx = c.target.x - c.me.x, dy = c.target.y - c.me.y, dz = c.target.z - c.me.z;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (distance > kJoinRange || distance <= 0.0f) return false;
  const float yaw = c.lookYaw * 0.01745329252f, pitch = c.lookPitch * 0.01745329252f;
  const float lx = std::cos(pitch) * std::sin(yaw), ly = std::sin(pitch), lz = std::cos(pitch) * std::cos(yaw);
  const float cosine = std::clamp((lx * dx + ly * dy + lz * dz) / distance, -1.0f, 1.0f);
  return std::acos(cosine) * 57.29577951f <= kJoinLookDegrees;
}

/// A body turning from one yaw to another over the glide's time, eased as the glide is: the first dancer of a facing
/// pair turns to the joiner this way, so the turn never jumps (the user, 2026-11-14).
struct Turn {
  float from = 0.0f, to = 0.0f;
  float seconds = kGlideSeconds;
  float t = 0.0f;  ///< seconds into the turn

  /// The way the body faces now, turning the short way round.
  float yaw() const { return wrap_degrees(from + wrap_degrees(to - from) * ease_in_out(t, seconds)); }
  bool done() const { return t >= seconds; }
};

/// The joiner's glide into the partner's spot: from where they stood and faced, eased in and out over `seconds`.
struct Glide {
  Point from{}, to{};
  float fromYaw = 0.0f, toYaw = 0.0f;
  float seconds = kGlideSeconds;
  float t = 0.0f;  ///< seconds into the glide

  /// How far along, 0 to 1, eased (smoothstep).
  float eased() const { return ease_in_out(t, seconds); }
  /// Where the joiner is now.
  Point at() const {
    const float e = eased();
    return {from.x + (to.x - from.x) * e, from.y + (to.y - from.y) * e, from.z + (to.z - from.z) * e};
  }
  /// The way the joiner faces now, turning the short way round.
  float yaw() const { return wrap_degrees(fromYaw + wrap_degrees(toYaw - fromYaw) * eased()); }
  bool done() const { return t >= seconds; }
};

/// What the join prompt draws for one of the game's actions: its gamepad control below <Gamepad>/ ("buttonWest"),
/// and its keyboard key's name for the keycap ("E"), each empty when the action has no such binding. A mouse-only
/// action has neither: the prompt then shows the plain keycap with nothing in it, since a mouse glyph would be a
/// new UI piece for the user to decide.
struct ActionKeys {
  std::string pad;
  std::string key;
};

/// A keyboard control's name as the keycap shows it: "e" is E, "space" is Space.
inline std::string keycap_name(std::string control) {
  if (!control.empty()) control[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(control[0])));
  return control;
}

/// The keys of one action, from its bindings' paths as the player has them now (each the override path after a
/// rebinding in the game's menu, else the path): the first <Keyboard>/ binding gives the keycap's name, the first
/// <Gamepad>/ binding the pad's glyph. Paths Unity writes as "<Keyboard>/e", "<Gamepad>/buttonWest",
/// "<Mouse>/leftButton".
inline ActionKeys read_binding_keys(const std::vector<std::string>& paths) {
  ActionKeys out;
  for (const std::string& path : paths) {
    if (out.pad.empty() && path.rfind("<Gamepad>/", 0) == 0) out.pad = path.substr(10);
    if (out.key.empty() && path.rfind("<Keyboard>/", 0) == 0) out.key = keycap_name(path.substr(11));
  }
  return out;
}

/// Where one dancer's copy of the pair's song plays on this client (the user, 2026-11-14): each dancer hears their
/// own body's copy as their own (flat, in both ears, as for any lone dancer); on the joiner's client the first
/// dancer's copy is silent; every client that is neither dancer hears one copy, sitting at the point between the
/// dancers and moving with them. Absent: that copy does not exist on this client (only the joiner's client makes a
/// second one). `first` and `joiner` are actors; `copyOf` is the dancer the copy belongs to.
enum class SongCopy { Own, Silent, Midpoint, Absent };
inline SongCopy pair_song_copy(int localActor, int first, int joiner, int copyOf) {
  if (copyOf == first) {
    if (localActor == first) return SongCopy::Own;
    if (localActor == joiner) return SongCopy::Silent;
    return SongCopy::Midpoint;
  }
  return localActor == joiner ? SongCopy::Own : SongCopy::Absent;
}

/// The ghost hears one song per pair, the first dancer's copy (Left to the builder, 2026-11-13): the joiner's own
/// copy adds no loudness.
inline bool counts_for_ghost(int first, int copyOf) { return copyOf == first; }

}  // namespace pm::emotes
