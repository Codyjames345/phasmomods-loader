#pragma once

// Unity's audio: an AudioSource plays an AudioClip in the world, and a clip can be made from samples.
// Unity's time: the frame count and the time scale. Not obfuscated, so every entry is by real name.
// Custom Ghosts plays its own sounds with them (the Knell's steps, the Mesmer's sound, the Echo's
// voice).

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* UnityAudio_float[] = {"float"};
inline constexpr const char* UnityAudio_bool[] = {"bool"};
inline constexpr const char* UnityAudio_AudioClip[] = {"AudioClip"};
inline constexpr const char* UnityAudio_ulong[] = {"ulong"};
inline constexpr const char* AudioClip_Create_params[] = {"string", "int", "int", "int", "bool"};
inline constexpr const char* AudioClip_SetData_params[] = {"Single[]", "int"};

/// Unity's audio player component (UnityEngine.AudioModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSource{.key = "AudioSource", .name = "AudioSource",
                                                .assembly = "UnityEngine.AudioModule", .ns = "UnityEngine"};

/// Sets the clip the source plays. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_clip{.key = "AudioSource.set_clip", .kind = pm::sdk::Kind::Method,
                                                         .owner = "AudioSource", .name = "set_clip",
                                                         .params = UnityAudio_AudioClip};

/// Sets how far the source is placed in the world: 0 heard alike everywhere, 1 fully 3D. Inferred:
/// Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_spatialBlend{
    .key = "AudioSource.set_spatialBlend", .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
    .name = "set_spatialBlend", .params = UnityAudio_float};

/// Mutes or unmutes the source; a muted source keeps playing silently. Inferred: Unity's documented
/// property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_mute{.key = "AudioSource.set_mute",
                                                         .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                         .name = "set_mute", .params = UnityAudio_bool};

/// Sets whether the source loops its clip. The engine loops it sample for sample, so a clip cut to its loop repeats
/// without a seam. Inferred: Unity's documented property setter (in the class dump).
inline constexpr pm::sdk::EntryDesc AudioSource_set_loop{.key = "AudioSource.set_loop",
                                                         .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                         .name = "set_loop", .params = UnityAudio_bool};

/// Sets where the source plays in its clip, in seconds. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_time{.key = "AudioSource.set_time", .kind = pm::sdk::Kind::Method,
                                                         .owner = "AudioSource", .name = "set_time",
                                                         .params = UnityAudio_float};

/// Sets the source's volume, 0 to 1. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_volume{.key = "AudioSource.set_volume",
                                                           .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                           .name = "set_volume", .params = UnityAudio_float};

/// Sets the distance in metres within which a 3D source stops getting louder. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_minDistance{
    .key = "AudioSource.set_minDistance", .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
    .name = "set_minDistance", .params = UnityAudio_float};

/// Sets the distance in metres beyond which a 3D source stops getting quieter. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_maxDistance{
    .key = "AudioSource.set_maxDistance", .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
    .name = "set_maxDistance", .params = UnityAudio_float};

inline constexpr const char* AudioRolloffMode_values[] = {"Logarithmic", "Linear", "Custom"};

/// How a 3D source's volume falls with distance (UnityEngine.AudioModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioRolloffMode{.key = "AudioRolloffMode", .name = "AudioRolloffMode",
                                                     .assembly = "UnityEngine.AudioModule", .ns = "UnityEngine",
                                                     .values = AudioRolloffMode_values};

/// Linear rolloff: full volume within the source's minimum distance, falling evenly to silence at its maximum. The
/// default, Logarithmic, never reaches silence: beyond the maximum distance it only stops falling. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc AudioRolloffMode_Linear{.key = "AudioRolloffMode.Linear",
                                                            .kind = pm::sdk::Kind::EnumValue,
                                                            .owner = "AudioRolloffMode", .name = "Linear"};

/// How the source's volume falls with distance, an AudioRolloffMode. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc AudioSource_get_rolloffMode{.key = "AudioSource.get_rolloffMode",
                                                                .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                                .name = "get_rolloffMode"};

/// The distance in metres within which a 3D source stops getting louder. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc AudioSource_get_minDistance{.key = "AudioSource.get_minDistance",
                                                                .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                                .name = "get_minDistance"};

/// The distance in metres the source's rolloff is measured to: silent there with a Linear or a Custom curve that ends
/// at 0. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc AudioSource_get_maxDistance{.key = "AudioSource.get_maxDistance",
                                                                .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                                .name = "get_maxDistance"};

inline constexpr const char* AudioSourceCurveType_values[] = {"CustomRolloff", "SpatialBlend", "ReverbZoneMix",
                                                              "Spread"};

/// Which of a source's curves (UnityEngine.AudioModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSourceCurveType{.key = "AudioSourceCurveType", .name = "AudioSourceCurveType",
                                                         .assembly = "UnityEngine.AudioModule", .ns = "UnityEngine",
                                                         .values = AudioSourceCurveType_values};

/// The custom rolloff curve: the volume (0 to 1) against the distance over the maximum distance (0 to 1), which a Custom
/// rolloff follows. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSourceCurveType_CustomRolloff{.key = "AudioSourceCurveType.CustomRolloff",
                                                                       .kind = pm::sdk::Kind::EnumValue,
                                                                       .owner = "AudioSourceCurveType",
                                                                       .name = "CustomRolloff"};

inline constexpr const char* AudioSource_GetCustomCurve_params[] = {"@AudioSourceCurveType"};
inline constexpr const char* AudioSource_SetCustomCurve_params[] = {"@AudioSourceCurveType", "AnimationCurve"};

/// A copy of one of the source's curves, a new AnimationCurve. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSource_GetCustomCurve{.key = "AudioSource.GetCustomCurve",
                                                               .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                               .name = "GetCustomCurve",
                                                               .params = AudioSource_GetCustomCurve_params};

/// Sets one of the source's curves from an AnimationCurve. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSource_SetCustomCurve{.key = "AudioSource.SetCustomCurve",
                                                               .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                               .name = "SetCustomCurve",
                                                               .params = AudioSource_SetCustomCurve_params};

inline constexpr const char* AudioSource_set_outputAudioMixerGroup_params[] = {"AudioMixerGroup"};

/// Sets the mixer group the source plays through (an AudioMixerGroup; null plays straight to the listener, past the
/// game's mixer and its muffling). Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_outputAudioMixerGroup{
    .key = "AudioSource.set_outputAudioMixerGroup", .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
    .name = "set_outputAudioMixerGroup", .params = AudioSource_set_outputAudioMixerGroup_params};

inline constexpr const char* AudioSource_set_rolloffMode_params[] = {"@AudioRolloffMode"};

/// Sets how the source's volume falls with distance (AudioRolloffMode). Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_rolloffMode{
    .key = "AudioSource.set_rolloffMode", .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
    .name = "set_rolloffMode", .params = AudioSource_set_rolloffMode_params};

/// Sets the source's pitch, 1 as recorded. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioSource_set_pitch{.key = "AudioSource.set_pitch",
                                                          .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                          .name = "set_pitch", .params = UnityAudio_float};

/// Plays the source's clip after a delay in samples (at 44.1 kHz); a delay of 0 plays it now, as
/// Play() does. Play() takes no parameters, which an entry cannot name apart from this overload.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSource_Play{.key = "AudioSource.Play", .kind = pm::sdk::Kind::Method,
                                                     .owner = "AudioSource", .name = "Play",
                                                     .params = UnityAudio_ulong};

/// Unity's sound asset (UnityEngine.AudioModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioClip{.key = "AudioClip", .name = "AudioClip",
                                              .assembly = "UnityEngine.AudioModule", .ns = "UnityEngine"};

/// A new empty clip: Create(name, lengthSamples, channels, frequency, stream). Static. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioClip_Create{.key = "AudioClip.Create", .kind = pm::sdk::Kind::Method,
                                                     .flags = pm::sdk::kStatic, .owner = "AudioClip",
                                                     .name = "Create", .params = AudioClip_Create_params};

/// Fills the clip with samples (a float[], -1 to 1) from a sample offset; false when it cannot.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioClip_SetData{.key = "AudioClip.SetData", .kind = pm::sdk::Kind::Method,
                                                      .owner = "AudioClip", .name = "SetData",
                                                      .params = AudioClip_SetData_params};

/// The clip's length in seconds. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc AudioClip_get_length{.key = "AudioClip.get_length", .kind = pm::sdk::Kind::Method,
                                                         .owner = "AudioClip", .name = "get_length"};

/// .NET's float (mscorlib, System.Single): the element type of the samples an AudioClip takes.
/// Inferred: .NET's documented API.
inline constexpr pm::sdk::EntryDesc Single{.key = "Single", .name = "Single", .assembly = "mscorlib",
                                           .ns = "System"};

/// Unity's time (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Time{.key = "Time", .name = "Time", .assembly = "UnityEngine.CoreModule",
                                         .ns = "UnityEngine"};

/// How many frames have begun since the game started. Static. Inferred: Unity's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc Time_get_frameCount{.key = "Time.get_frameCount", .kind = pm::sdk::Kind::Method,
                                                        .flags = pm::sdk::kStatic, .owner = "Time",
                                                        .name = "get_frameCount"};

/// How fast game time passes, 1 as in real time. Static. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Time_get_timeScale{.key = "Time.get_timeScale", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "Time",
                                                       .name = "get_timeScale"};

/// Sets how fast game time passes, 1 as in real time and 0 stopping it (every Update then runs with a
/// delta time of 0). Static. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Time_set_timeScale{.key = "Time.set_timeScale", .kind = pm::sdk::Kind::Method,
                                                       .flags = pm::sdk::kStatic, .owner = "Time",
                                                       .name = "set_timeScale"};

/// Unity's audio listener (UnityEngine.AudioModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioListener{.key = "AudioListener", .name = "AudioListener",
                                                  .assembly = "UnityEngine.AudioModule", .ns = "UnityEngine"};

/// The volume everything this client hears plays at, 0 to 1. Static. Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc AudioListener_get_volume{.key = "AudioListener.get_volume",
                                                             .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
                                                             .owner = "AudioListener", .name = "get_volume"};

inline constexpr const char* AudioListener_float[] = {"float"};

/// Sets that volume. Static. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc AudioListener_set_volume{
    .key = "AudioListener.set_volume", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "AudioListener", .name = "set_volume", .params = AudioListener_float};

/// Stops the source's clip, and with true its one-shots too: the private overload with a bool. The
/// public Stop() is this with true (both pass straight to the same engine function), and a name alone
/// cannot tell the two overloads apart. Decompiled.
inline constexpr pm::sdk::EntryDesc AudioSource_Stop{.key = "AudioSource.Stop", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "AudioSource", .name = "Stop",
                                                     .params = UnityAudio_bool};

/// Plays a clip once on the source, beside its own clip: the overload without a volume. Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc AudioSource_PlayOneShot{.key = "AudioSource.PlayOneShot",
                                                            .kind = pm::sdk::Kind::Method, .owner = "AudioSource",
                                                            .name = "PlayOneShot", .params = UnityAudio_AudioClip};

}  // namespace pm::map

/// @file
/// @brief The game API for Unity's audio and time (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's audio listener: one volume for everything this client hears.
struct AudioListener {
  /// The type: type_object() for GameObject.GetComponents. Unresolved: null.
  static constexpr pm::api::Type<pm::map::AudioListener> type{};
  /// get_volume(). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::AudioListener_get_volume, float()> get_volume{};
  /// set_volume(volume). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioListener_set_volume, void(float)> set_volume{};
};

/// Unity's AudioSource. `source` below is one.
struct AudioSource {
  /// The type: klass(), is(obj), type_object() (for GameObject.AddComponent). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::AudioSource> type{};
  /// get_rolloffMode(source): an AudioRolloffMode number. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::AudioSource_get_rolloffMode, int32_t(void*)> get_rolloffMode{};
  /// get_minDistance(source): metres. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::AudioSource_get_minDistance, float(void*)> get_minDistance{};
  /// get_maxDistance(source): metres. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::AudioSource_get_maxDistance, float(void*)> get_maxDistance{};
  /// GetCustomCurve(source, type): a copy of the curve, an AnimationCurve (type an AudioSourceCurveType). Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::AudioSource_GetCustomCurve, void*(void*, int32_t)> GetCustomCurve{};
  /// SetCustomCurve(source, type, curve). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_SetCustomCurve, void(void*, int32_t, void*)> SetCustomCurve{};
  /// set_clip(source, clip). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_clip, void(void*, void*)> set_clip{};
  /// set_spatialBlend(source, blend). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_spatialBlend, void(void*, float)> set_spatialBlend{};
  /// set_mute(source, muted). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_mute, void(void*, bool)> set_mute{};
  /// set_loop(source, loops). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_loop, void(void*, bool)> set_loop{};
  /// set_time(source, seconds): where it plays in its clip. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_time, void(void*, float)> set_time{};
  /// set_volume(source, volume). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_volume, void(void*, float)> set_volume{};
  /// set_minDistance(source, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_minDistance, void(void*, float)> set_minDistance{};
  /// set_maxDistance(source, metres). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_maxDistance, void(void*, float)> set_maxDistance{};
  /// set_pitch(source, pitch). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_pitch, void(void*, float)> set_pitch{};
  /// Play(source, delaySamples): 0 plays now. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_Play, void(void*, uint64_t)> Play{};
  /// Stop(source, stopOneShots): Stop(source, true) is Unity's Stop(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_Stop, void(void*, bool)> Stop{};
  /// PlayOneShot(source, clip). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_PlayOneShot, void(void*, void*)> PlayOneShot{};
  /// set_outputAudioMixerGroup(source, group): the mixer group it plays through. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_outputAudioMixerGroup, void(void*, void*)>
      set_outputAudioMixerGroup{};
  /// set_rolloffMode(source, mode): how its volume falls with distance, an AudioRolloffMode. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_rolloffMode, void(void*, int32_t)> set_rolloffMode{};
};

/// How a 3D source's volume falls with distance.
namespace AudioRolloffMode {
/// Linear(): full within the minimum distance, silent at the maximum. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AudioRolloffMode_Linear> Linear{};
}  // namespace AudioRolloffMode

/// Which of a source's curves (AudioSource.GetCustomCurve, SetCustomCurve).
namespace AudioSourceCurveType {
/// CustomRolloff(): the volume against distance a Custom rolloff follows. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::AudioSourceCurveType_CustomRolloff> CustomRolloff{};
}  // namespace AudioSourceCurveType

/// Unity's AudioClip. `clip` below is one.
struct AudioClip {
  /// The type: type_object() for AssetBundle.LoadAsset_Internal. Unresolved: null.
  static constexpr pm::api::Type<pm::map::AudioClip> type{};
  /// Create(name, lengthSamples, channels, frequency, stream): a new clip, `name` a managed string
  /// (pm::api::make_string). Unresolved: null.
  static constexpr pm::api::Method<pm::map::AudioClip_Create, void*(void*, int32_t, int32_t, int32_t, bool)> Create{};
  /// SetData(clip, samples, offsetSamples): `samples` a float[] (Single::type.make_array). Unresolved:
  /// false.
  static constexpr pm::api::Method<pm::map::AudioClip_SetData, bool(void*, void*, int32_t)> SetData{};
  /// get_length(clip): seconds. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::AudioClip_get_length, float(void*)> get_length{};
};

/// .NET's float.
struct Single {
  /// The type: make_array(n) gives a float[] of n zeros. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Single> type{};
};

/// Unity's time.
struct Time {
  /// get_frameCount(): frames begun since the game started. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Time_get_frameCount, int32_t()> get_frameCount{};
  /// get_timeScale(): 1 in real time. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Time_get_timeScale, float()> get_timeScale{};
  /// set_timeScale(value): how fast game time passes, 0 freezing the game's own Update bodies (they
  /// still run, with a delta time of 0). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Time_set_timeScale, void(float)> set_timeScale{};
};

/// @}
}  // namespace phasmo
