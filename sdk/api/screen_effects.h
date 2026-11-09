#pragma once

/// @file
/// @brief A screen blur and vignette of a mod's own, under its UI: what the emote wheel puts behind itself.
///
/// The effects are a URP volume of the mod's own: a GameObject with a global Volume of high priority on the lowest
/// layer the local player's camera takes volumes from (its UniversalAdditionalCameraData's volume layer mask), holding
/// a new VolumeProfile with a Gaussian DepthOfField (the blur, from the camera outward) and a Vignette. A camera of the
/// mod's own that copies the player's mask (third_person.h) shows them too. Overlay canvases are drawn after the
/// effects, so a mod's UI stays sharp. The game's side is in sdk/systems/media.md, "A camera of a mod's own, and screen
/// effects".
///
/// Main thread. Local only.
/// @ingroup runtime
/// @since Contract 2.3

namespace pm::api::fx {

/// The volume's priority: above the game's own volumes, so its values win where both set one.
inline constexpr float kPriority = 100.0f;

/// Sets the blur and vignette: makes the volume on the first call, changes its values after, and removes it when both
/// are 0.
/// @param blur 0 (none) to 1 (the strongest Gaussian blur URP gives, gaussianMaxRadius 1.5).
/// @param vignette 0 (none) to 1 (the vignette's full intensity).
/// @return False with no local camera, or where the URP entries did not resolve (logged); true otherwise, and true
///   when both are 0.
bool set_screen_effects(float blur, float vignette);

/// Whether the volume exists.
/// @return True between a set_screen_effects with a strength above 0 and one with both at 0.
bool screen_effects_on();

/// Removes the volume at once (a scene change, or the mod stopping).
void clear_screen_effects();

}  // namespace pm::api::fx
