#pragma once

/// @file
/// @brief Playing an AnimationClip on an investigator's own body, over the animation the game gives it, and handing
/// the body back: what an emote does.
///
/// Each body has one Playables graph of the API's own, made once (prepare_player, or the first play) and kept while the
/// body lives. Its one animation output targets the body's Animator (Player.animator) at a sorting order above the
/// game's own and above Animation Rigging's (1000), so a clip wins where the output's weight is 1, over the rig's aims
/// too (the head and arms turned toward the camera), and the game's animation shows through as the weight falls. Under
/// the output, a mixer holds each clip the body has played, each connected once, and crossfades from one to the next.
/// Between dances the output stays at weight 0. So after the first, nothing is ever added to or removed from the
/// Animator, only weights change: in a live test (2026-11-14), a body whose output was added and removed for each clip
/// kept a wrong held-item pose after the dance (an empty hand posed as holding, a held item too high), and the game's
/// own rig builds its output only as it is enabled (RigBuilder.Build's one caller is RigBuilder.OnEnable). Prepare
/// each body as soon as it exists.
/// What the rig places from the body's pose rather than animates (the journal's hanger,
/// JournalRigController.constraintParent) keeps the game's pose; a mod that wants the journal on the dancing body moves
/// the journal itself each frame from the mod's tick, which runs after the animation (the game moves it back to its
/// hanger every Update, so nothing stays; the Emotes mod does this). Humanoid clips retarget onto the investigator's
/// body (sdk/guides/custom-animation.md). The game's side is in sdk/systems/players.md, "The body".
///
/// Main thread. Local only: each client plays what it is told on its own copy of a player's body; nothing is sent.
/// Call tick_player_anims every frame.
/// @ingroup runtime
/// @since Contract 2.3

namespace pm::api::anim {

/// The sorting order of a body's output: above the game's Animator (0) and Animation Rigging's RigBuilder (1000).
inline constexpr int kSortingOrder = 2000;

/// Plays a clip on a player's body from its start, blending it in from the pose the body has now (from where a dance
/// still blending out has it). A clip already playing on that body crossfades into the new one over the same time; a
/// third clip asked for while two crossfade ends the older at once, so at most two show. The body is prepared first
/// if it was not (prepare_player).
/// @param player A Player (any client's).
/// @param clip An AnimationClip, loaded from a mod's bundle (sdk/api/bundles.h).
/// @param blendIn Seconds to blend in; 0 or less is at once.
/// @return False for a null or destroyed player or clip, a body with no Animator, or a build where the Playables
///   entries did not resolve (logged).
bool play_on_player(void* player, void* clip, float blendIn);

/// As play_on_player, starting the clip `startSeconds` in rather than at its start: a looping clip wraps the time to its
/// length, and a clip that does not loop holds it within its length. For a body joining another's dance part-way (a
/// synced emote's second dancer).
/// @param player A Player (any client's).
/// @param clip An AnimationClip, loaded from a mod's bundle (sdk/api/bundles.h).
/// @param blendIn Seconds to blend in; 0 or less is at once.
/// @param startSeconds Seconds into the clip, 0 or more.
/// @return As play_on_player without a start: false for a null or destroyed player or clip, a body with no Animator,
///   or a build where the Playables entries did not resolve (logged).
/// @since Contract 2.6
bool play_on_player(void* player, void* clip, float blendIn, double startSeconds);

/// Blends a player's clips out and ends them; the body keeps its graph and output, at weight 0. Nothing for a player
/// with none.
/// @param player The Player.
/// @param blendOut Seconds to blend out; 0 or less is at once.
void stop_on_player(void* player, float blendOut);

/// Whether a clip plays, or blends out, on a player's body.
/// @param player The Player.
/// @return True until its weight is back to 0.
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

/// Each frame: moves every blend on by `dt` seconds, and destroys the graph of a body that was destroyed or given
/// another Animator, forgetting it.
/// @param dt Seconds since the last tick (Time.deltaTime).
void tick_player_anims(float dt);

/// Ends every clip on every body at once and destroys every body's graph (when the mod stops). Removing a body's
/// output from its Animator is the change this API otherwise avoids, so call it only when nothing of the mod may stay.
void stop_all_player_anims();

/// Gives a player's body its graph and output now, at weight 0, so that a later play_on_player changes nothing on the
/// body's Animator but weights. Call it for each player as soon as the body exists (the Emotes mod calls it for every
/// player each frame), before the player holds anything. Calling it again for a prepared body does nothing.
/// @param player A Player (any client's).
/// @return True when the body has its graph; false for a null or destroyed player, a body with no Animator, or a
///   build where the Playables entries did not resolve (logged).
/// @since Contract 2.7
bool prepare_player(void* player);

}  // namespace pm::api::anim
