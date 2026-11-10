#pragma once

/// @file
/// @brief A third-person view of the local player, and the local player's own body shown to them: what an emote's
/// camera does.
///
/// begin_third_person makes a camera of the mod's own, copied from the player's (LocalPlayer.camera, with its URP
/// post-processing and volume mask), moves the body's and the head's renderers to the layer other players' bodies are
/// drawn on (Layers::AlivePlayer: on their own layer the game draws the local body in first-person passes over
/// everything) until the view ends, and switches the player's camera off: the player's camera keeps its place, its AudioListener, the held item's slot and the game's raycasts from it.
/// Each frame the view sits straight back from the player's head camera along its look, eased out over
/// orbit_rule.h's kEaseSeconds to kOrbitDistance, pulled in short of a wall; the mouse goes on turning the head camera
/// as the game does, so it turns the view. The wall check passes through the local player's own colliders and rig (their
/// layers), which follow the camera a frame late. The head, which the game draws as shadows only on the player's own
/// client (PlayerCharacter.headRenderersToTurnOff), is drawn while the view is on and given back its mode after.
/// end_third_person eases it back into the head and then gives the player their camera again. show_own_body shows the body on the player's own camera instead (VR, whose view never moves).
/// The game's side is in sdk/systems/media.md, "A camera of a mod's own, and screen effects".
///
/// Main thread. Local only. Call tick_third_person every frame.
/// @ingroup runtime
/// @since Contract 2.3

#include "orbit_rule.h"

namespace pm::api::view {

/// Starts the third-person view of the local player, easing out from the head; while it eases back in, it turns out
/// again from where it is.
/// @return False with no local player or camera, when the view is already on or easing out, or where the camera
///   entries did not resolve (logged).
bool begin_third_person();

/// Eases the view back into the head; when it arrives, the player's own camera is on again and the mod's camera gone.
/// Nothing when the view is off.
void end_third_person();

/// Each frame: places the view behind the head (or eases it), and ends it when it has eased back. Ends it at once if
/// the player's camera was destroyed (a scene change).
/// @param dt Seconds since the last tick.
void tick_third_person(float dt);

/// Whether the view is on, or easing back.
/// @return True from begin_third_person until it has eased back.
bool in_third_person();

/// While the view is on, draws `object` and what is under it as other players see it: each GameObject on the
/// first-person layer (PlayerView) moves to AlivePlayer until the view ends, as the body's own did. For something a mod
/// puts on the body while the view is on, such as a held item moved to the hand. Nothing while the view is off.
/// @param object A Component or GameObject (null or destroyed: nothing).
void draw_as_seen(void* object);

/// How long a begin_third_person now would take to have the view fully out: kEaseSeconds while it is off, what is left
/// of the ease while it eases out, 0 while it is out, and while it eases back the time it would take to turn out again
/// from where it is.
/// @return Seconds, 0 to kEaseSeconds.
float seconds_until_out();

/// The yaw the view looks along, in degrees about the up axis (0 along +z, 90 along +x): the player's head camera's.
/// @return The yaw, or 0 with no local camera.
float view_yaw();

/// Shows the local player's whole body on their own camera (on) or hides it again as the game had it (off), by moving
/// the body's renderers to the layer other players' bodies are drawn on (Layers::AlivePlayer) and back. For VR, whose view stays the headset's.
/// @param on True to show.
void show_own_body(bool on);

/// Ends the view and the shown body at once, the player's camera on and its mask as it was (a scene change, or the
/// mod stopping).
void stop_third_person();

}  // namespace pm::api::view
