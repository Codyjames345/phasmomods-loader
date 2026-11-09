#pragma once

// LocalisationSystem: the game's text by key in the player's language, and GameController, whose
// players' insanity a ghost's hunt asks for. Both types and these members keep their real names. Notes
// read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* LocalisationSystem_GetLocalisedValue_params[] = {"string", "bool"};

/// The game's localisation (Assembly-CSharp, global namespace), a MonoBehaviour with static members.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem{.key = "LocalisationSystem",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "LocalisationSystem", .assembly = "Assembly-CSharp",
                                                       .ns = ""};

/// The text of a key in the player's language: GetLocalisedValue(key, fallback). The game names a ghost
/// type through the key "Journal_<the GhostType value's name>Title" (a type with no enum name is a
/// number there), which every ghost's GhostData holds as its name key. The overload taking a language
/// first is another method. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_GetLocalisedValue{
    .key = "LocalisationSystem.GetLocalisedValue", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "GetLocalisedValue", .type = "string", .params = LocalisationSystem_GetLocalisedValue_params};

inline constexpr const char* LocalisationSystem_TryGetLocalisedValue_params[] = {"string", "out string", "bool"};
inline constexpr const char* LocalisationSystem_int[] = {"int"};
inline constexpr const char* Language_values[] = {"English", "French", "German", "Japanese"};
inline constexpr const char* LocalisationSystem_LookUp_params[] = {"@Language", "string", "out string"};
// Several private static bool(language, string, out string) look-alikes exist; TryGetLocalisedValue
// tail-jumps to one.
inline constexpr pm::sdk::Fact LocalisationSystem_LookUp_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "LocalisationSystem.TryGetLocalisedValue"}};

/// The game's languages, an enum nested in LocalisationSystem (name obfuscated, value names real):
/// English 0, then the others. Decompiled.
inline constexpr pm::sdk::EntryDesc Language{.key = "Language", .confidence = pm::sdk::Confidence::Decompiled,
                                             .owner = "LocalisationSystem",
                                             .values = Language_values};

/// English, the language TryGetLocalisedValue falls back to (it passes 0). Decompiled.
inline constexpr pm::sdk::EntryDesc Language_English{.key = "Language.English", .kind = pm::sdk::Kind::EnumValue,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Language", .name = "English"};

/// The text of a key in a given language: LookUp(language, key, out text) looks the key up in that
/// language, then in English (the value 0), through a one-language look-up, and writes the out slot
/// through the GC write barrier; a key in neither gets a marker and the key, and false.
/// TryGetLocalisedValue tail-jumps to it with the player's or the voice language. The public
/// GetLocalisedValue(language, key) overload ignores its language and passes the player's, so this
/// is the way to read English. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_LookUp{
    .key = "LocalisationSystem.LookUp", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "LocalisationSystem", .type = "bool",
    .params = LocalisationSystem_LookUp_params, .facts = LocalisationSystem_LookUp_facts};

/// The text of a key: TryGetLocalisedValue(key, out text, current). It looks the key up in the
/// player's language (the language get_language gives when `current`, else get_voiceLanguage's), then
/// in English (the language enum's 0), and writes the text and true; for a key in neither it writes a
/// marker and the key and returns false. GetLocalisedValue is this with the text returned, and every
/// TextLocaliserUI calls this one directly, so a key answered here reaches both. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_TryGetLocalisedValue{
    .key = "LocalisationSystem.TryGetLocalisedValue", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "TryGetLocalisedValue", .type = "bool", .params = LocalisationSystem_TryGetLocalisedValue_params};

/// The player's language, a value of the game's language enum (English is 0), read from the class's
/// statics. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_get_language{
    .key = "LocalisationSystem.get_language", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "get_language"};

/// The language voice recognition uses, the language TryGetLocalisedValue looks in when `current` is
/// false. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_get_voiceLanguage{
    .key = "LocalisationSystem.get_voiceLanguage", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "get_voiceLanguage"};

/// The game's two-letter abbreviation of a language value ("EN" for English): regional variants share
/// one (both Portuguese, both Spanish, both Chinese), and an unknown value gets the default's.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_GetLanguageAbbreviationText{
    .key = "LocalisationSystem.GetLanguageAbbreviationText", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "GetLanguageAbbreviationText", .type = "string", .params = LocalisationSystem_int};

/// The language `step` places after a language value (before it for a negative step): the value plus
/// the step while that is from 0 (or -1 when the third argument is true) to below the language count
/// (a static), else the value itself. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalisationSystem_GetNextOrPreviousLanguage{
    .key = "LocalisationSystem.GetNextOrPreviousLanguage", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "LocalisationSystem",
    .name = "GetNextOrPreviousLanguage"};

/// The game's controller (Assembly-CSharp, global namespace), a MonoBehaviourPunCallbacks. Decompiled.
inline constexpr pm::sdk::EntryDesc GameController{.key = "GameController",
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "GameController", .assembly = "Assembly-CSharp", .ns = ""};

/// The average insanity of the players. The hunt's base refresh asks for it for the Moroi.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_GetAveragePlayerInsanity{
    .key = "GameController.GetAveragePlayerInsanity", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameController", .name = "GetAveragePlayerInsanity"};

// Found only for that fact.
/// Ends a hunt's effects on this client. The hunt state's exit (GhostHuntState.Exit) calls it when the
/// hunt was on. Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_ResetHuntEffects{
    .key = "GameController.ResetHuntEffects", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameController", .name = "ResetHuntEffects"};

}  // namespace pm::map

/// @file
/// @brief The game API for LocalisationSystem (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (null) without touching the game.
namespace phasmo {
/// @addtogroup journal
/// @{

/// The game's localisation.
struct LocalisationSystem {
  /// The LocalisationSystem type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::LocalisationSystem> type{};
  /// GetLocalisedValue(key, fallback): the text of `key`, a System.String, in the player's language.
  /// Unresolved: null.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_GetLocalisedValue, void*(void*, bool)>
      GetLocalisedValue{};
  /// TryGetLocalisedValue(key, &text, current): writes the text (a System.String) and returns whether
  /// the key was found. Unresolved: false, writes nothing.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_TryGetLocalisedValue, bool(void*, void**, bool)>
      TryGetLocalisedValue{};
  /// LookUp(language, key, &text): the key's text in that language, else in English. Unresolved: false.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_LookUp, bool(int32_t, void*, void**)> LookUp{};
  /// get_language(): the player's language value. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_get_language, int32_t()> get_language{};
  /// get_voiceLanguage(): voice recognition's language value. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_get_voiceLanguage, int32_t()> get_voiceLanguage{};
  /// GetLanguageAbbreviationText(language): its abbreviation, a System.String. Unresolved: null.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_GetLanguageAbbreviationText, void*(int32_t)>
      GetLanguageAbbreviationText{};
  /// GetNextOrPreviousLanguage(language, step, fromMinusOne). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::LocalisationSystem_GetNextOrPreviousLanguage, int32_t(int32_t, int32_t, bool)>
      GetNextOrPreviousLanguage{};
};

/// The game's languages.
namespace Language {
/// English: English(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::Language_English> English{};
}  // namespace Language

/// @}
}  // namespace phasmo
