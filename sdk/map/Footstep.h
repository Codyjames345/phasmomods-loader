#pragma once

// A ghost's footstep (Footstep, spawned by an RPC on every client) and the Noise it plays through: its
// AudioSource is what players hear, the trigger collider on its object (the "Noise Cube", active only
// while the step sounds) is how the sound sensor and the parabolic microphone detect it, and its volume
// and loudness are what they read. The ghost's own hearing takes only a PlayerNoise, never a footstep's
// (sdk/internals/ghost-ai.md, "A ghost's footsteps"). Notes read from decompiled code are from build
// 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// One footstep of a ghost (Assembly-CSharp, global namespace), a MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep{.key = "Footstep", .confidence = pm::sdk::Confidence::Decompiled,
                                             .name = "Footstep", .assembly = "Assembly-CSharp", .ns = ""};

/// Unity's Awake for the footstep: it saves its Noise's AudioSource's max distance as the
/// footstep's reach. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep_Awake{.key = "Footstep.Awake", .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Footstep",
                                                   .name = "Awake"};

/// Unity's OnEnable for the footstep: it picks the step's sound by the ground and the ghost's pace
/// and plays it once through Play. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep_OnEnable{.key = "Footstep.OnEnable", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "Footstep", .name = "OnEnable"};

/// The footstep's Noise. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc Footstep_noise{.key = "Footstep.noise", .kind = pm::sdk::Kind::Field,
                                                   .owner = "Footstep", .name = "noise"};

/// The ghost the footstep belongs to: Footstep's one GhostAI field. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep_ghost{.key = "Footstep.ghost", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Footstep",
                                                   .type = "@GhostAI"};

// Awake stores one float of the footstep, its reach; OnEnable stores the other.
inline constexpr pm::sdk::Fact Footstep_reach_facts[] = {{.is = pm::sdk::Fact::Is::Stores, .method = "Footstep.Awake"}};

/// The footstep's reach in metres: what Awake read from its Noise's AudioSource (maxDistance). Play
/// gives the Noise this reach, less a margin for some types. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep_reach{.key = "Footstep.reach", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Footstep",
                                                   .type = "float", .facts = Footstep_reach_facts};

inline constexpr const char* Footstep_Play_params[] = {"float", "string"};
// Twenty private void(float, string) methods of Footstep look alike: the play and the obfuscator's
// copies. OnEnable calls only the play.
inline constexpr pm::sdk::Fact Footstep_Play_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "Footstep.OnEnable"}};

/// Plays the step: sets the Noise's pitch and its reach (Noise.SetMaxDistance, from Footstep.reach,
/// 8 m less for a hunting Myling), and when a line down finds the floor, picks the clip for it and
/// plays it through Noise.PlaySound at the clip's own volume. Last it multiplies the AudioSource's
/// volume by its float argument, which it uses for nothing else: 0 leaves the step's sound silent and
/// the Noise's volume and loudness as they were. Footstep.OnEnable gives 1, 0.5 or 1.5. Runs on every
/// client. Decompiled.
inline constexpr pm::sdk::EntryDesc Footstep_Play{.key = "Footstep.Play", .kind = pm::sdk::Kind::Method,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Footstep",
                                                  .type = "void", .params = Footstep_Play_params,
                                                  .facts = Footstep_Play_facts};

/// A sound in the world through an AudioSource, which the sound sensor and the parabolic microphone
/// detect (Assembly-CSharp, global namespace). A player's footstep is the subclass PlayerNoise, the
/// only kind the ghost's hearing takes. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise{.key = "Noise", .confidence = pm::sdk::Confidence::Decompiled,
                                          .name = "Noise", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* Noise_float[] = {"float"};
inline constexpr const char* Noise_PlaySound_params[] = {"AudioClip", "float", "bool", "bool"};

/// Plays a clip: stops the AudioSource, sets its clip, calls Noise.SetVolume with the volume given (with
/// -1, Noise.volume itself) and plays. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_PlaySound{.key = "Noise.PlaySound", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "Noise", .name = "PlaySound", .type = "void",
                                                    .params = Noise_PlaySound_params};

/// Sets the noise's volume: stores it as Noise.volume and its loudness as 150 times it (a random 100
/// to 200 times with the prefab's random volume), and gives the AudioSource that volume (0 while the
/// Noise is muted). Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_SetVolume{.key = "Noise.SetVolume", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "Noise", .name = "SetVolume", .type = "void",
                                                    .params = Noise_float};

/// Whether the noise takes its mixer group from where it plays each time it plays: Noise.PlaySound then routes its
/// AudioSource through SoundController.GetFloorAudioSnapshot of the noise's position, so the game's mixer muffles it as
/// that place calls for (another floor, outdoors). Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_changeMixerOnPlay{.key = "Noise.changeMixerOnPlay",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "Noise", .name = "changeMixerOnPlay"};

// PlaySound reads the volume when it is given -1, and no other public float of Noise.
inline constexpr pm::sdk::Fact Noise_volume_facts[] = {{.is = pm::sdk::Fact::Is::Loads, .method = "Noise.PlaySound"}};

/// The volume of the noise's last play, as Noise.SetVolume stored it. The sound sensor reads it (the
/// sum over the noises it detects is its reading) and replays the clip in the truck at it; the
/// parabolic microphone replays a muted noise at it. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_volume{.key = "Noise.volume", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "Noise",
                                                 .type = "float", .facts = Noise_volume_facts,
                                                 .access = pm::sdk::Visibility::Public};

// SetVolume stores the volume and the loudness, Noise's two public floats.
inline constexpr pm::sdk::Fact Noise_loudness_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Noise.SetVolume"},
    {.is = pm::sdk::Fact::Is::IsNot, .method = "Noise.volume"}};

/// The loudness of the noise's last play, as Noise.SetVolume stored it. The parabolic microphone's
/// reading is the sum of it over the playing noises it detects (with their least and greatest), and its
/// radar labels each noise with it. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_loudness{.key = "Noise.loudness", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .owner = "Noise",
                                                   .type = "float", .facts = Noise_loudness_facts,
                                                   .access = pm::sdk::Visibility::Public};

/// The noise's AudioSource, which Noise.PlaySound plays its clip through: Noise's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_source{.key = "Noise.source", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "Noise",
                                                 .type = "AudioSource"};

/// Sets how far the noise reaches, in metres. The hunt calls it on the ghost's sounds, and
/// Footstep.Play on each step's. Decompiled.
inline constexpr pm::sdk::EntryDesc Noise_SetMaxDistance{.key = "Noise.SetMaxDistance", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "Noise", .name = "SetMaxDistance",
                                                         .params = Noise_float};

/// A player's footsteps (Assembly-CSharp, global namespace), a MonoBehaviourPun on the player: AttemptPlaySound casts
/// a line straight down from the player on FootstepController.mask, takes the tag of the collider it hits (Carpet,
/// Concrete, Wood, OldWood, Grass, Snow, Truck, Wet, Glass, Metal, Gravel, Vinyl, Heavyconcrete, Stairs), and plays one
/// of that surface's clips through the player's PlayerNoise: the run clips (run<Surface>AudioClips) while the player
/// sprints, the walking ones (<surface>AudioClips) otherwise; Stairs has one list for both. A tag that is none of these
/// plays nothing. Its Update plays one each time a timer of its own runs out while the player walks or sprints, at a
/// volume set for each surface in its code. Decompiled: the switch's string hashes name each tag's lists.
inline constexpr pm::sdk::EntryDesc FootstepController{.key = "FootstepController",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "FootstepController", .assembly = "Assembly-CSharp",
                                                       .ns = ""};
/// The player's PlayerNoise its steps play through (FootstepController.AttemptPlaySound calls Noise.PlaySound on it, at a
/// volume set for each surface in its code). Its AudioSource (Noise.source) holds how a player's step falls off with
/// distance: a Custom rolloff curve, full to a fifth of its 15 m reach and silent at it (read from the asset files).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_noise{
    .key = "FootstepController.noise", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "noise"};
/// The layers its downward line hits, a LayerMask. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_mask{
    .key = "FootstepController.mask", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "mask"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_carpetAudioClips{
    .key = "FootstepController.carpetAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "carpetAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runCarpetAudioClips{
    .key = "FootstepController.runCarpetAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runCarpetAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_concreteAudioClips{
    .key = "FootstepController.concreteAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "concreteAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runConcreteAudioClips{
    .key = "FootstepController.runConcreteAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runConcreteAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_woodAudioClips{
    .key = "FootstepController.woodAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "woodAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runWoodAudioClips{
    .key = "FootstepController.runWoodAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runWoodAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_stairsAudioClips{
    .key = "FootstepController.stairsAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "stairsAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_oldwoodAudioClips{
    .key = "FootstepController.oldwoodAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "oldwoodAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runOldWoodAudioClips{
    .key = "FootstepController.runOldWoodAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runOldWoodAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_grassAudioClips{
    .key = "FootstepController.grassAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "grassAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runGrassAudioClips{
    .key = "FootstepController.runGrassAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runGrassAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_snowAudioClips{
    .key = "FootstepController.snowAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "snowAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runSnowAudioClips{
    .key = "FootstepController.runSnowAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runSnowAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_truckAudioClips{
    .key = "FootstepController.truckAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "truckAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runTruckAudioClips{
    .key = "FootstepController.runTruckAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runTruckAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_wetAudioClips{
    .key = "FootstepController.wetAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "wetAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runWetAudioClips{
    .key = "FootstepController.runWetAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runWetAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_glassAudioClips{
    .key = "FootstepController.glassAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "glassAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runGlassAudioClips{
    .key = "FootstepController.runGlassAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runGlassAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_metalAudioClips{
    .key = "FootstepController.metalAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "metalAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runMetalAudioClips{
    .key = "FootstepController.runMetalAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runMetalAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_gravelAudioClips{
    .key = "FootstepController.gravelAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "gravelAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runGravelAudioClips{
    .key = "FootstepController.runGravelAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runGravelAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_vinylAudioClips{
    .key = "FootstepController.vinylAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "vinylAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runVinylAudioClips{
    .key = "FootstepController.runVinylAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runVinylAudioClips"};
/// The walking step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_heavyconcreteAudioClips{
    .key = "FootstepController.heavyconcreteAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "heavyconcreteAudioClips"};
/// The running step clips for a surface (FootstepController.AttemptPlaySound), an
/// AudioClip[]. Decompiled.
inline constexpr pm::sdk::EntryDesc FootstepController_runHeavyconcreteAudioClips{
    .key = "FootstepController.runHeavyconcreteAudioClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FootstepController", .name = "runHeavyconcreteAudioClips"};

/// The player's FootstepController: Player's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_footstepController{
    .key = "Player.footstepController", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Player", .type = "FootstepController"};

}  // namespace pm::map

/// @file
/// @brief The game API for footsteps (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// A ghost's footstep. `footstep` below is one.
struct Footstep {
  /// Its Noise: noise.get(footstep). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Footstep_noise, void*> noise{};
  /// Its ghost's GhostAI: ghost.get(footstep). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Footstep_ghost, void*> ghost{};
  /// Its reach in metres: reach.get(footstep). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Footstep_reach, float> reach{};
  /// Play(footstep, volume, name): `name` a managed string. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Footstep_Play, void(void*, float, void*)> Play{};
};

/// A sound in the world. `noise` below is one.
struct Noise {
  /// Its AudioSource: source.get(noise). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Noise_source, void*> source{};
  /// PlaySound(noise, clip, volume, a, b): plays `clip` through its AudioSource on this client only, at `volume`
  /// (-1 for Noise.volume). The two flags are the game's own; its door sounds pass false for both. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::Noise_PlaySound, void(void*, void*, float, bool, bool)> PlaySound{};
  /// SetMaxDistance(noise, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Noise_SetMaxDistance, void(void*, float)> SetMaxDistance{};
  /// The volume of its last play: volume.get(noise), volume.set(noise, v). What the sound sensor reads
  /// and replays at. Unresolved: 0, and a set does nothing.
  static constexpr pm::api::Field<pm::map::Noise_volume, float> volume{};
  /// The loudness of its last play: loudness.get(noise), loudness.set(noise, v). What the parabolic
  /// microphone reads. Unresolved: 0, and a set does nothing.
  static constexpr pm::api::Field<pm::map::Noise_loudness, float> loudness{};
};

/// A player's footsteps. `steps` below is one.
struct FootstepController {
  /// The type. Unresolved: null.
  static constexpr pm::api::Type<pm::map::FootstepController> type{};
  /// The player's PlayerNoise its steps play through: noise.get(steps). Unresolved: null.
  static constexpr pm::api::Field<pm::map::FootstepController_noise, void*> noise{};
  /// The layers its downward line hits, a bit per layer: mask.get(steps). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::FootstepController_mask, int32_t> mask{};
  /// Each surface's step clips, an AudioClip[]: carpetAudioClips and the rest walking, runCarpetAudioClips and the
  /// rest sprinting, stairsAudioClips for both. Unresolved: null.
  static constexpr pm::api::Field<pm::map::FootstepController_carpetAudioClips, void*> carpetAudioClips{};  ///< walking on carpet
  static constexpr pm::api::Field<pm::map::FootstepController_runCarpetAudioClips, void*> runCarpetAudioClips{};  ///< sprinting on carpet
  static constexpr pm::api::Field<pm::map::FootstepController_concreteAudioClips, void*> concreteAudioClips{};  ///< walking on concrete
  static constexpr pm::api::Field<pm::map::FootstepController_runConcreteAudioClips, void*> runConcreteAudioClips{};  ///< sprinting on concrete
  static constexpr pm::api::Field<pm::map::FootstepController_woodAudioClips, void*> woodAudioClips{};  ///< walking on wood
  static constexpr pm::api::Field<pm::map::FootstepController_runWoodAudioClips, void*> runWoodAudioClips{};  ///< sprinting on wood
  static constexpr pm::api::Field<pm::map::FootstepController_stairsAudioClips, void*> stairsAudioClips{};  ///< on stairs, walking or sprinting
  static constexpr pm::api::Field<pm::map::FootstepController_oldwoodAudioClips, void*> oldwoodAudioClips{};  ///< walking on old wood
  static constexpr pm::api::Field<pm::map::FootstepController_runOldWoodAudioClips, void*> runOldWoodAudioClips{};  ///< sprinting on old wood
  static constexpr pm::api::Field<pm::map::FootstepController_grassAudioClips, void*> grassAudioClips{};  ///< walking on grass
  static constexpr pm::api::Field<pm::map::FootstepController_runGrassAudioClips, void*> runGrassAudioClips{};  ///< sprinting on grass
  static constexpr pm::api::Field<pm::map::FootstepController_snowAudioClips, void*> snowAudioClips{};  ///< walking on snow
  static constexpr pm::api::Field<pm::map::FootstepController_runSnowAudioClips, void*> runSnowAudioClips{};  ///< sprinting on snow
  static constexpr pm::api::Field<pm::map::FootstepController_truckAudioClips, void*> truckAudioClips{};  ///< walking on truck
  static constexpr pm::api::Field<pm::map::FootstepController_runTruckAudioClips, void*> runTruckAudioClips{};  ///< sprinting on truck
  static constexpr pm::api::Field<pm::map::FootstepController_wetAudioClips, void*> wetAudioClips{};  ///< walking on wet
  static constexpr pm::api::Field<pm::map::FootstepController_runWetAudioClips, void*> runWetAudioClips{};  ///< sprinting on wet
  static constexpr pm::api::Field<pm::map::FootstepController_glassAudioClips, void*> glassAudioClips{};  ///< walking on glass
  static constexpr pm::api::Field<pm::map::FootstepController_runGlassAudioClips, void*> runGlassAudioClips{};  ///< sprinting on glass
  static constexpr pm::api::Field<pm::map::FootstepController_metalAudioClips, void*> metalAudioClips{};  ///< walking on metal
  static constexpr pm::api::Field<pm::map::FootstepController_runMetalAudioClips, void*> runMetalAudioClips{};  ///< sprinting on metal
  static constexpr pm::api::Field<pm::map::FootstepController_gravelAudioClips, void*> gravelAudioClips{};  ///< walking on gravel
  static constexpr pm::api::Field<pm::map::FootstepController_runGravelAudioClips, void*> runGravelAudioClips{};  ///< sprinting on gravel
  static constexpr pm::api::Field<pm::map::FootstepController_vinylAudioClips, void*> vinylAudioClips{};  ///< walking on vinyl
  static constexpr pm::api::Field<pm::map::FootstepController_runVinylAudioClips, void*> runVinylAudioClips{};  ///< sprinting on vinyl
  static constexpr pm::api::Field<pm::map::FootstepController_heavyconcreteAudioClips, void*> heavyconcreteAudioClips{};  ///< walking on heavy concrete
  static constexpr pm::api::Field<pm::map::FootstepController_runHeavyconcreteAudioClips, void*> runHeavyconcreteAudioClips{};  ///< sprinting on heavy concrete
  /// The player's own: footstepController.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_footstepController, void*> footstepController{};
};

/// @}
}  // namespace phasmo
