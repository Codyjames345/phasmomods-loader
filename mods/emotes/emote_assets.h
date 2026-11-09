#pragma once

// The eight emotes and where their parts are (the author's assets, 2026-11-10): each one's folder under
// mods/emotes/assets (its song and icon as supplied), its clip in the author's animation package, and its name's key in
// the mod's string table. The wheel's segments follow this order, the first straight up and the rest clockwise. The
// bundle (built in the private assets repository, Assets/Editor/EmotesBuild.cs) holds each clip, song and icon at the
// paths below; each song's loudness, measured from the same MP3 at build time, is the text file loudness_path names.

#include <string>

#include "emote_rule.h"
#include "text_table.h"

namespace pm::emotes {

struct EmoteAsset {
  const char* folder;   ///< its folder under mods/emotes/assets, and the stem of its song's and icon's bundle names
  const char* clip;     ///< its AnimationClip's name in the animation package
  const char* nameKey;  ///< the key of its name under the wheel
  /// Where its song loops back to, in seconds, and how long its loop is; a length of 0 loops the song with its clip
  /// (the author's rule, 2026-11-10). The songs mark no loop of their own, so every emote waits for the author's points.
  float songLoopStart = 0.0f;
  float songLoopSeconds = 0.0f;
};

inline constexpr EmoteAsset kEmotes[kSegments] = {
    {"disco_fever", "DiscoFever", pm::i18n::key("emotes.name.0")},
    {"electro_swing", "ElectroSwing", pm::i18n::key("emotes.name.1")},
    {"floss", "Floss", pm::i18n::key("emotes.name.2")},
    {"groove_jam", "GrooveJam", pm::i18n::key("emotes.name.3")},
    {"orange_justice", "OrangeJustice", pm::i18n::key("emotes.name.4")},
    {"scenario", "Scenario", pm::i18n::key("emotes.name.5")},
    {"smooth_moves", "SmoothMoves", pm::i18n::key("emotes.name.6")},
    {"take_the_l", "TakeTheL", pm::i18n::key("emotes.name.7")},
};

/// An emote's song loop: its own points when the author gave them, else its clip's length from the song's start.
/// @param e The emote.
/// @param clipSeconds Its clip's length.
inline SongLoop song_loop_of(const EmoteAsset& e, float clipSeconds) {
  if (e.songLoopSeconds > 0.0f) return {e.songLoopStart, e.songLoopSeconds};
  return {0.0f, clipSeconds};
}

/// The key of the label of the mod's key in the game's keybinds menu.
inline constexpr const char* kKeyLabel = pm::i18n::key("emotes.key-label");

/// The bundle's file name, beside the mod's DLL in its assets folder.
inline constexpr const char* kBundleFile = "emotes.bundle";

/// Where an emote's clip is in the bundle (an asset's bundle name is its project path, in lower case).
inline std::string clip_path(const EmoteAsset& e) {
  std::string clip = e.clip;
  for (char& c : clip) c = static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  return "assets/emotes/clips/" + clip + ".anim";
}

/// Where an emote's song is in the bundle.
inline std::string song_path(const EmoteAsset& e) { return std::string("assets/emotes/songs/") + e.folder + ".mp3"; }

/// Where an emote's icon is in the bundle.
inline std::string icon_path(const EmoteAsset& e) { return std::string("assets/emotes/icons/") + e.folder + ".png"; }

/// Where an emote's measured loudness is, relative to the mod's assets folder.
inline std::string loudness_path(const EmoteAsset& e) { return std::string(e.folder) + "/loudness.txt"; }

}  // namespace pm::emotes
