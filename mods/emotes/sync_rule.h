#pragma once

// Joining a synced emote, free of game calls (docs/superpowers/specs/2026-11-13-synced-emotes-design.md): where the
// second dancer stands, when the join prompt shows over a dancer, and the glide that takes the joiner to their spot.
// The glue (emote.cpp) reads the game into these and carries out what they say.

#include <algorithm>
#include <cmath>

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
  bool spotClear = false;      ///< the partner's spot fits the joiner, on ground
  bool interactableInWay = false;  ///< the game's own Interact ray would act on something, so a press would do both
};

/// Whether the prompt shows: every condition of the design at once.
inline bool shows_prompt(const PromptCheck& c) {
  if (!c.canDance || !c.targetSynced || !c.partFree || c.targetInIntro || !c.spotClear || c.interactableInWay)
    return false;
  const float dx = c.target.x - c.me.x, dy = c.target.y - c.me.y, dz = c.target.z - c.me.z;
  const float distance = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (distance > kJoinRange || distance <= 0.0f) return false;
  const float yaw = c.lookYaw * 0.01745329252f, pitch = c.lookPitch * 0.01745329252f;
  const float lx = std::cos(pitch) * std::sin(yaw), ly = std::sin(pitch), lz = std::cos(pitch) * std::cos(yaw);
  const float cosine = std::clamp((lx * dx + ly * dy + lz * dz) / distance, -1.0f, 1.0f);
  return std::acos(cosine) * 57.29577951f <= kJoinLookDegrees;
}

/// The joiner's glide into the partner's spot: from where they stood and faced, eased in and out over `seconds`.
struct Glide {
  Point from{}, to{};
  float fromYaw = 0.0f, toYaw = 0.0f;
  float seconds = kGlideSeconds;
  float t = 0.0f;  ///< seconds into the glide

  /// How far along, 0 to 1, eased (smoothstep).
  float eased() const {
    const float s = seconds > 0.0f ? std::clamp(t / seconds, 0.0f, 1.0f) : 1.0f;
    return s * s * (3.0f - 2.0f * s);
  }
  /// Where the joiner is now.
  Point at() const {
    const float e = eased();
    return {from.x + (to.x - from.x) * e, from.y + (to.y - from.y) * e, from.z + (to.z - from.z) * e};
  }
  /// The way the joiner faces now, turning the short way round.
  float yaw() const { return wrap_degrees(fromYaw + wrap_degrees(toYaw - fromYaw) * eased()); }
  bool done() const { return t >= seconds; }
};

}  // namespace pm::emotes
