#pragma once

// How the Emotes wheel looks: the author's design (docs/superpowers/specs/2026-11-10-emotes-design.md, "The wheel") - a
// grey disc with a centre circle, eight equal segments with a thin transparent gap between them, white silhouette icons,
// the picked segment blue, its name in white under the disc, and a slight blur and vignette behind it. Every number
// here is a placeholder for the author to set in game: colours, sizes, the gap and the effects' strengths. Sizes are in
// the overlay canvas's units (1920 by 1080, scaled to the screen).

namespace pm::emotes::style {

struct Rgba {
  float r, g, b, a;
};

inline constexpr Rgba kDiscGrey{0.32f, 0.32f, 0.32f, 0.85f};   ///< an unpicked segment and the centre circle
inline constexpr Rgba kPickedBlue{0.15f, 0.45f, 0.95f, 0.92f};  ///< the picked segment
inline constexpr Rgba kIconWhite{1.0f, 1.0f, 1.0f, 1.0f};       ///< the icons, white silhouettes as supplied
inline constexpr Rgba kNameWhite{1.0f, 1.0f, 1.0f, 1.0f};       ///< the picked emote's name

inline constexpr float kDiscRadius = 300.0f;    ///< the disc's outer radius
inline constexpr float kCentreRadius = 95.0f;   ///< the centre circle's radius
inline constexpr float kRingInner = 105.0f;     ///< where the segments begin, leaving a gap round the centre circle
inline constexpr float kGapDegrees = 2.0f;      ///< the thin transparent gap between segments
inline constexpr float kIconSize = 110.0f;      ///< an icon's width and height
inline constexpr float kNameFontSize = 44.0f;   ///< the name's text size
inline constexpr float kNameBelow = 48.0f;      ///< how far under the disc the name sits

inline constexpr float kBlur = 0.35f;      ///< the blur behind the wheel, 0 to 1 (screen only)
inline constexpr float kVignette = 0.35f;  ///< the vignette behind the wheel, 0 to 1 (screen only)

inline constexpr float kVrDiscMetres = 0.24f;    ///< the VR wheel's width across, in metres
inline constexpr float kVrAboveHand = 0.12f;     ///< how far above the left hand the VR wheel floats, in metres
inline constexpr int kTextureSize = 1024;        ///< the drawn segments' and centre's texture size, in texels: as large as the disc on a large screen, so the edges stay sharp
inline constexpr int kSortingOrder = 30000;      ///< the overlay canvas's order: over the game's HUD

}  // namespace pm::emotes::style
