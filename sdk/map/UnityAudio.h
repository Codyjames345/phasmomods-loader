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

// The game API for Unity's audio and time (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this game
// build" once and returns its type's default (0, false, null) without touching the game.
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
  /// set_clip(source, clip). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_clip, void(void*, void*)> set_clip{};
  /// set_spatialBlend(source, blend). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_spatialBlend, void(void*, float)> set_spatialBlend{};
  /// set_mute(source, muted). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::AudioSource_set_mute, void(void*, bool)> set_mute{};
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
};

/// Unity's AudioClip. `clip` below is one.
struct AudioClip {
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
};

/// @}
}  // namespace phasmo
