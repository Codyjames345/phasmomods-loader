#pragma once

/// @file
/// @brief Playing an AnimationClip on an investigator's own body, over the animation the game gives it, and handing
/// the body back: what an emote does.
///
/// Each clip plays in a Playables graph of its own whose animation output targets the body's Animator
/// (Player.animator) at a sorting order above the game's own and above Animation Rigging's (1000), so the clip wins
/// where its weight is 1, over the rig's aims too (the head and arms turned toward the camera), and the game's
/// animation shows through as the weight falls. What the rig places from the body's pose rather than animates (the
/// journal at the waist, JournalRigController.constraintParent) keeps the game's pose; a mod that wants it on the
/// dancing body places it again each frame from the mod's tick, which runs after the animation (the Emotes mod does). A new clip blends in over the one
/// playing; when it is in fully, the old graph is destroyed. Stopping blends the weight to 0 and destroys the graphs,
/// so the Animator is the game's alone again, exactly as before. Humanoid clips retarget onto the investigator's body
/// (sdk/guides/custom-animation.md). The game's side is in sdk/systems/players.md, "The body".
///
/// Main thread. Local only: each client plays what it is told on its own copy of a player's body; nothing is sent.
/// Call tick_player_anims every frame.
/// @ingroup runtime
/// @since Contract 2.3

namespace pm::api::anim {

/// The sorting order of a clip's output: above the game's Animator (0) and Animation Rigging's RigBuilder (1000). A
/// clip blending in over another takes the next order; a lone clip is set back to this one.
inline constexpr int kSortingOrder = 2000;

/// Plays a clip on a player's body, blending it in from the pose the body has now. A clip already playing on that body
/// stays under the new one until the new one is in fully, then is ended; a third clip asked for while two blend ends
/// the older at once, so a body has at most two clips.
/// @param player A Player (any client's).
/// @param clip An AnimationClip, loaded from a mod's bundle (sdk/api/bundles.h).
/// @param blendIn Seconds to blend in; 0 or less is at once.
/// @return False for a null or destroyed player or clip, a body with no Animator, or a build where the Playables
///   entries did not resolve (logged).
bool play_on_player(void* player, void* clip, float blendIn);

/// Blends a player's clips out and ends them. Nothing for a player with none.
/// @param player The Player.
/// @param blendOut Seconds to blend out; 0 or less is at once.
void stop_on_player(void* player, float blendOut);

/// Whether a clip plays, or blends out, on a player's body.
/// @param player The Player.
/// @return True until the last of its clips is ended.
bool playing_on(void* player);

/// The newest clip's own time on a player's body, in seconds since it began; it keeps counting past the clip's length
/// as a looping clip repeats, so time modulo clip_seconds is the place in the loop.
/// @param player The Player.
/// @return Seconds, or -1 when no clip plays on it.
double clip_time(void* player);

/// A clip's length.
/// @param clip An AnimationClip.
/// @return Seconds, or 0 for null or unresolved.
float clip_seconds(void* clip);

/// Whether a clip repeats (its import setting Loop Time).
/// @param clip An AnimationClip.
/// @return Its loop flag; false for null or unresolved.
bool clip_loops(void* clip);

/// Each frame: moves every blend on by `dt` seconds, ends what has blended out or replaced, and forgets a body that
/// was destroyed (its graphs with it).
/// @param dt Seconds since the last tick (Time.deltaTime).
void tick_player_anims(float dt);

/// Ends every clip on every body at once (on a scene change or when the mod stops).
void stop_all_player_anims();

}  // namespace pm::api::anim
