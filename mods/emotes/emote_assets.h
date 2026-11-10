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
  /// (the author's rule, 2026-11-10). The songs mark no loop of their own: from the author's rough points, each was
  /// measured where the waveform repeats its start (normalised cross-correlation, given with each), and the bundle holds
  /// each song cut to its loop, sample for sample, which the game loops itself (the private repository's
  /// unity/emotes-bundle/cut_loops.ps1 has the sample counts). These seconds time the song's loudness for the ghost.
  float songLoopStart = 0.0f;
  float songLoopSeconds = 0.0f;
};

inline constexpr EmoteAsset kEmotes[kSegments] = {
    // Its song loops every 8.542 s from its start (the author: about 8.2 s; correlation 0.999).
    {"disco_fever", "DiscoFever", pm::i18n::key("emotes.name.0"), 0.0f, 8.542f},
    // Every 16.000 s (the author: about 15.8 s; correlation 0.9996).
    {"electro_swing", "ElectroSwing", pm::i18n::key("emotes.name.1"), 0.0f, 16.0f},
    // Its song loops every 16.692 s from its start, not with its 1.633 s clip (the author, 2026-11-10; measured: the
    // track repeats sample for sample, correlation 0.9994).
    {"floss", "Floss", pm::i18n::key("emotes.name.2"), 0.0f, 16.692f},
    // Every 15.118 s (the author: about 15 s; correlation 0.994).
    {"groove_jam", "GrooveJam", pm::i18n::key("emotes.name.3"), 0.0f, 15.118f},
    // Every 18.935 s (the author: about 19 s; correlation 0.996).
    {"orange_justice", "OrangeJustice", pm::i18n::key("emotes.name.4"), 0.0f, 18.935f},
    // Every 32.542 s (the author: about 32.5 s; correlation 0.983).
    {"scenario", "Scenario", pm::i18n::key("emotes.name.5"), 0.0f, 32.542f},
    // Every 19.200 s (the author: about 19.1 s; correlation 1.000).
    {"smooth_moves", "SmoothMoves", pm::i18n::key("emotes.name.6"), 0.0f, 19.2f},
    // Its song loops every 12.632 s (eight bars) from its start, not with its 0.833 s clip (the author, 2026-11-10;
    // measured on the waveform at 48 kHz, correlation 0.9998).
    {"take_the_l", "TakeTheL", pm::i18n::key("emotes.name.7"), 0.0f, 12.632f},
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
inline std::string song_path(const EmoteAsset& e) { return std::string("assets/emotes/loops/") + e.folder + ".wav"; }

/// Where an emote's icon is in the bundle.
inline std::string icon_path(const EmoteAsset& e) { return std::string("assets/emotes/icons/") + e.folder + ".png"; }

/// Where an emote's measured loudness is, relative to the mod's assets folder.
inline std::string loudness_path(const EmoteAsset& e) { return std::string(e.folder) + "/loudness.txt"; }

}  // namespace pm::emotes
