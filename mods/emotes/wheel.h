#pragma once

// The Emotes wheel on screen and in VR (the author's design; wheel_style.h, wheel_layout.h): a black, slightly see-through disc of eight
// segments with gaps and a centre circle, each segment's white icon, the picked segment blue and its name in white
// under the disc. On screen it is an overlay canvas over a slight blur and vignette (sdk/api/screen_effects.h); in VR a
// world canvas floating over the left hand and turned to the head, without the effects. Main thread.

#include <string>
#include <vector>

namespace pm::emotes::wheel {

/// Keeps every emote's icon and name, in wheel order (catalogue.h wheel_order), and draws the segments' and centre's
/// textures. A null icon leaves its segment bare. False when the textures cannot be made in this game build.
/// @param icons Each emote's Texture2D, or null.
/// @param names Each emote's name as the player reads it.
bool load(const std::vector<void*>& icons, const std::vector<std::string>& names);

/// Shows page `page`: its emotes are page * kSegments onwards. The open wheel's icons change at once and nothing is
/// picked; a closed wheel opens on it.
void set_page(int page);

/// The emote under `segment` of the page shown, or -1 for an empty segment or one out of range.
int emote_at(int segment);

/// Each frame while open: the glyphs of the page controls, the game's own Sprites for the device in use, or null for none
/// (the page row hides a null glyph, and the whole row with one page).
void show_glyphs(void* previousSprite, void* nextSprite);

/// The page glyph a VR controller points at: -1 the previous page's, +1 the next page's, 0 neither (and 0 when the
/// wheel is not open in VR or has one page).
/// @param hand The pointing hand's Transform (VRPlayer.rightHandIK).
int pointed_glyph(void* hand);

/// Shows the wheel on screen, nothing picked, with the blur and vignette behind it. False when it cannot be built.
bool open_screen();

/// Shows the wheel over a VR player's left hand, nothing picked. False when it cannot be built.
/// @param leftHand The left hand's Transform (VRPlayer.leftHandIK).
bool open_vr(void* leftHand);

/// Each frame while open in VR: turns the wheel to face the head.
/// @param head The head camera's Transform.
void face(void* head);

/// Shows a segment as picked (blue) with its name under the disc; -1 shows none picked and no name.
void show_pick(int segment);

/// The segment a VR controller points at: its forward ray against the disc.
/// @param hand The pointing hand's Transform (VRPlayer.rightHandIK).
/// @return The segment, or -1 when it points at none, or when the wheel is not open in VR.
int pointed(void* hand);

/// Hides the wheel and its effects. Nothing when it is closed.
void close();

/// Whether the wheel shows.
bool is_open();
// How many of the bundle's icons and of the drawn segment textures are still alive, for the log.
int icons_alive();
int segments_alive();

/// Frees the wheel's objects and textures (the mod stopping).
void unload();

}  // namespace pm::emotes::wheel
