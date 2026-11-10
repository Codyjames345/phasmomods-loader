#pragma once

// The Emotes wheel on screen and in VR (the author's design; wheel_style.h, wheel_layout.h): a grey disc of eight
// segments with gaps and a centre circle, each segment's white icon, the picked segment blue and its name in white
// under the disc. On screen it is an overlay canvas over a slight blur and vignette (sdk/api/screen_effects.h); in VR a
// world canvas floating over the left hand and turned to the head, without the effects. Main thread.

namespace pm::emotes::wheel {

/// Keeps the icons from the bundle and draws the segments' and centre's textures. A missing icon leaves its segment
/// bare (logged). False when the textures cannot be made in this game build.
bool load(void* bundle);

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
