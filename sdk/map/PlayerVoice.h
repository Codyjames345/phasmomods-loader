#pragma once

// A player's voice chat: PlayerVoice (Assembly-CSharp) and the Vivox audio taps it makes for each
// participant (the Unity.Services.Vivox assembly), through which the game hears the voice audio Vivox
// hands it. Custom Ghosts' Echo records other players' voices from them. Notes read from decompiled code
// are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// One player's voice chat (Assembly-CSharp, global namespace), a MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerVoice{.key = "PlayerVoice", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "PlayerVoice", .assembly = "Assembly-CSharp", .ns = ""};

/// The voice's player, the game's Player. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc PlayerVoice_player{.key = "PlayerVoice.player", .kind = pm::sdk::Kind::Field,
                                                       .owner = "PlayerVoice", .name = "player"};

inline constexpr const char* PlayerVoice_VivoxParticipant[] = {"VivoxParticipant"};

/// Makes the tap that globalTap holds for a Vivox participant: unless one exists, it asks the
/// participant for a VivoxParticipantTap on a new object named after the player, keeps it in globalTap,
/// and lets its audio play in the channel mix. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerVoice_CreateGlobalAudioTap{
    .key = "PlayerVoice.CreateGlobalAudioTap", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerVoice", .name = "CreateGlobalAudioTap",
    .params = PlayerVoice_VivoxParticipant};

// PlayerVoice has two VivoxParticipantTap fields; CreateGlobalAudioTap stores one, CreateLocalAudioTap the
// other. CreateLocalAudioTap stores its tap through a register the fact reader cannot prove holds this, so
// the local tap is the one CreateGlobalAudioTap does not store.
inline constexpr pm::sdk::Fact PlayerVoice_localTap_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "PlayerVoice.CreateGlobalAudioTap", .negate = true}};

/// The player's positional voice tap, a VivoxParticipantTap, or null: the voice heard where the player
/// stands. PlayerVoice.CreateLocalAudioTap makes it for a Vivox participant: unless one exists, it asks the
/// participant for a tap, parents it to the player and gives its AudioSource the volume, spatial blend and
/// rolloff of the player's own voice source. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerVoice_localTap{.key = "PlayerVoice.localTap", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "PlayerVoice",
                                                         .type = "VivoxParticipantTap",
                                                         .facts = PlayerVoice_localTap_facts};

inline constexpr pm::sdk::Fact PlayerVoice_globalTap_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "PlayerVoice.CreateGlobalAudioTap"}};

/// The player's voice tap that CreateGlobalAudioTap makes, a VivoxParticipantTap, or null. The Echo
/// records each other player's voice from it. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerVoice_globalTap{.key = "PlayerVoice.globalTap", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "PlayerVoice",
                                                          .type = "VivoxParticipantTap",
                                                          .facts = PlayerVoice_globalTap_facts};

/// A Vivox audio tap of one participant's voice (Unity.Services.Vivox, namespace
/// Unity.Services.Vivox.AudioTaps), on an object with an AudioSource. Inferred: Vivox's documented API.
inline constexpr pm::sdk::EntryDesc VivoxParticipantTap{.key = "VivoxParticipantTap", .name = "VivoxParticipantTap",
                                                        .assembly = "Unity.Services.Vivox",
                                                        .ns = "Unity.Services.Vivox.AudioTaps"};

inline constexpr const char* VivoxParticipantTap_DoAudioFilterRead_params[] = {"int", "Single[]", "int", "int", "int"};

/// Fills the tap's audio buffer with the participant's voice: DoAudioFilterRead(tapId, samples,
/// frames, channels, sampleRate), the samples interleaved, and returns a status (negative on failure).
/// It runs on the main thread, from Vivox's coroutine, as Custom Ghosts' Echo has relied on since it
/// was built. Inferred: from its name and Vivox's documented audio taps.
inline constexpr pm::sdk::EntryDesc VivoxParticipantTap_DoAudioFilterRead{
    .key = "VivoxParticipantTap.DoAudioFilterRead", .kind = pm::sdk::Kind::Method, .owner = "VivoxParticipantTap",
    .name = "DoAudioFilterRead", .params = VivoxParticipantTap_DoAudioFilterRead_params};

/// Vivox's service (Unity.Services.Vivox). Inferred: Vivox's documented API.
inline constexpr pm::sdk::EntryDesc VivoxService{.key = "VivoxService", .name = "VivoxService",
                                                 .assembly = "Unity.Services.Vivox", .ns = "Unity.Services.Vivox"};

/// The running Vivox service, an IVivoxService: a VivoxServiceInternal. Static. Inferred: Vivox's
/// documented API.
inline constexpr pm::sdk::EntryDesc VivoxService_get_Instance{.key = "VivoxService.get_Instance",
                                                              .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
                                                              .owner = "VivoxService", .name = "get_Instance"};

/// Vivox's implementation of the service (Unity.Services.Vivox). Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc VivoxServiceInternal{.key = "VivoxServiceInternal", .name = "VivoxServiceInternal",
                                                         .assembly = "Unity.Services.Vivox",
                                                         .ns = "Unity.Services.Vivox"};

/// Mutes this client's microphone in every channel. Inferred: Vivox's documented API.
inline constexpr pm::sdk::EntryDesc VivoxServiceInternal_MuteInputDevice{
    .key = "VivoxServiceInternal.MuteInputDevice", .kind = pm::sdk::Kind::Method, .owner = "VivoxServiceInternal",
    .name = "MuteInputDevice"};

/// Unmutes this client's microphone; the game's push-to-talk and settings call it. Inferred: Vivox's
/// documented API.
inline constexpr pm::sdk::EntryDesc VivoxServiceInternal_UnmuteInputDevice{
    .key = "VivoxServiceInternal.UnmuteInputDevice", .kind = pm::sdk::Kind::Method, .owner = "VivoxServiceInternal",
    .name = "UnmuteInputDevice"};

/// How loud one player's voice plays here (Assembly-CSharp, global namespace), a MonoBehaviour on that
/// player. Its volume is a product of several volume multipliers. Decompiled.
inline constexpr pm::sdk::EntryDesc VoiceVolume{.key = "VoiceVolume", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "VoiceVolume", .assembly = "Assembly-CSharp", .ns = ""};

/// The Player whose voice it is. Decompiled.
inline constexpr pm::sdk::EntryDesc VoiceVolume_player{.key = "VoiceVolume.player", .kind = pm::sdk::Kind::Field,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "VoiceVolume", .name = "player"};

/// Every frame, the voice's death multiplier: 0 while its player is dead and the local investigator
/// (Network's local Player) is alive, otherwise 1. It reads the local Player without a null check, so
/// it throws on a client with none. Decompiled.
inline constexpr pm::sdk::EntryDesc VoiceVolume_Update{.key = "VoiceVolume.Update", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "VoiceVolume", .name = "Update"};

// The stack a multiplier notifies when it changes (obfuscated). It is found by its fields.
inline constexpr const char* VolumeStack_fieldTypes[] = {"float", "Action<float>"};

/// The product of a voice's volume multipliers (Assembly-CSharp, global namespace; obfuscated, so found
/// by its fields): it calls its Action<float> when one of them changes. Decompiled.
inline constexpr pm::sdk::EntryDesc VolumeStack{.key = "VolumeStack", .confidence = pm::sdk::Confidence::Decompiled,
                                                .assembly = "Assembly-CSharp", .ns = "",
                                                .fieldTypes = VolumeStack_fieldTypes};

inline constexpr const char* VolumeMultiplier_fieldTypes[] = {"float", "List<@VolumeStack>"};

/// One factor of a voice's volume (Assembly-CSharp, global namespace; obfuscated, so found by its
/// fields): a float and the stacks it belongs to. Decompiled.
inline constexpr pm::sdk::EntryDesc VolumeMultiplier{.key = "VolumeMultiplier",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .flags = pm::sdk::kExactFields,
                                                     .assembly = "Assembly-CSharp", .ns = "",
                                                     .fieldTypes = VolumeMultiplier_fieldTypes};

// Update reads the death multiplier and no other of the voice's five multipliers.
inline constexpr pm::sdk::Fact VoiceVolume_deathMultiplier_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "VoiceVolume.Update"}};

/// The voice's death multiplier, a VolumeMultiplier: the one Update sets. Decompiled.
inline constexpr pm::sdk::EntryDesc VoiceVolume_deathMultiplier{
    .key = "VoiceVolume.deathMultiplier", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "VoiceVolume", .type = "@VolumeMultiplier",
    .facts = VoiceVolume_deathMultiplier_facts};

inline constexpr const char* VolumeMultiplier_float[] = {"float"};

// The type has 33 methods of this shape; Update sets the death multiplier through the property's setter.
inline constexpr pm::sdk::Fact VolumeMultiplier_set_value_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "VoiceVolume.Update"}};

/// Sets the multiplier's value; when it changes, every stack it belongs to is told. Decompiled.
inline constexpr pm::sdk::EntryDesc VolumeMultiplier_set_value{
    .key = "VolumeMultiplier.set_value", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "VolumeMultiplier", .type = "void", .params = VolumeMultiplier_float,
    .facts = VolumeMultiplier_set_value_facts};

}  // namespace pm::map

// The game API for voice chat (sdk/api/game_api.h). Every member is called on Unity's main thread. One
// that did not resolve in the running game build logs "<key> is not available in this game build" once
// and returns its type's default (0, null) without touching the game.
namespace phasmo {
/// @addtogroup voice
/// @{

/// One player's voice chat. `voice` below is the PlayerVoice.
struct PlayerVoice {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::PlayerVoice> type{};
  /// Its Player: player.get(voice). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerVoice_player, void*> player{};
  /// Its global voice tap, or null: globalTap.get(voice). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerVoice_globalTap, void*> globalTap{};
  /// Its positional voice tap, or null: localTap.get(voice). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerVoice_localTap, void*> localTap{};
};

/// A participant's audio tap. `tap` below is one.
struct VivoxParticipantTap {
  /// DoAudioFilterRead(tap, tapId, samples, frames, channels, sampleRate): `samples` a float[]. A mod
  /// usually hooks it. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::VivoxParticipantTap_DoAudioFilterRead,
                                   int32_t(void*, int32_t, void*, int32_t, int32_t, int32_t)>
      DoAudioFilterRead{};
};

/// Vivox's service.
struct VivoxService {
  /// The running service, a VivoxServiceInternal: get_Instance(), or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::VivoxService_get_Instance, void*()> get_Instance{};
};

/// Vivox's implementation of the service. `vivox` below is VivoxService::get_Instance().
struct VivoxServiceInternal {
  /// MuteInputDevice(vivox): mutes this client's microphone. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VivoxServiceInternal_MuteInputDevice, void(void*)> MuteInputDevice{};
  /// UnmuteInputDevice(vivox): unmutes it. A mod hooks it to keep the microphone off. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::VivoxServiceInternal_UnmuteInputDevice, void(void*)>
      UnmuteInputDevice{};
};

/// How loud one player's voice plays here. `volume` below is the VoiceVolume.
struct VoiceVolume {
  /// Update(volume): a mod hooks it to decide the death multiplier itself. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VoiceVolume_Update, void(void*)> Update{};
  /// The Player whose voice it is: player.get(volume). Unresolved: null.
  static constexpr pm::api::Field<pm::map::VoiceVolume_player, void*> player{};
  /// Its death multiplier, a VolumeMultiplier: deathMultiplier.get(volume). Unresolved: null.
  static constexpr pm::api::Field<pm::map::VoiceVolume_deathMultiplier, void*> deathMultiplier{};
};

/// One factor of a voice's volume. `multiplier` below is one.
struct VolumeMultiplier {
  /// set_value(multiplier, value): sets it and tells its stacks. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VolumeMultiplier_set_value, void(void*, float)> set_value{};
};

/// @}
}  // namespace phasmo
