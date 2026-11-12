#pragma once

// What an emote pack's emotes.json holds (docs/superpowers/plans/2026-11-12-emotes-pages-packs-walking.md): the pack's
// bundle and, for each emote, its clips, song, icon, label and loudness table. Reading it makes no game calls, so the
// offline tests (tests/emotes_catalogue_test.cpp) check the whole file format. A pack whose file breaks the rules is
// refused with one sentence for the log and nothing read from it; the loader keeps the packs that did read and starts
// the wheel from those.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../../third_party/nlohmann/json.hpp"
#include "emote_rule.h"

namespace pm::emotes {

/// The most emotes one pack's file may hold.
inline constexpr size_t kMaxEmotesPerPack = 64;

/// One emote as its pack's file describes it.
struct EmoteDef {
  std::string id;            ///< "<pack id>.<name>"
  std::string name;          ///< [a-z0-9-], 1 to 40 characters, unique in its pack
  std::string clip;          ///< a project path in the bundle, "Assets/....anim"
  std::string moveClip;      ///< the walking variant's clip, or empty; may equal clip (a clip that walks as it is)
  std::string introClip;     ///< played once before clip, or empty
  std::string song;          ///< "Assets/....wav", ".ogg" or ".mp3"
  std::string introSong;     ///< played once before song, or empty
  std::string icon;          ///< "Assets/....png"
  std::string label;         ///< the key of its name in the pack's i18n/default.json
  std::string loudness;      ///< a path under the pack's assets folder, such as "loudness/floss.txt"
  std::string introLoudness; ///< the same for introSong; required when introSong is set, else empty
  bool loops = true;
  SongLoop songLoop;         ///< {start, length} seconds; length 0 loops the song with its clip
  uint32_t key = 0;          ///< emote_key(id)
};

/// One pack's whole catalogue.
struct Catalogue {
  std::string packId;
  std::string bundle;                 ///< a file name in the pack's assets folder, "<name>.bundle"
  std::vector<EmoteDef> emotes;       ///< in file order
  std::vector<std::string> warnings;  ///< unknown fields, as "emotes[2].colour" or "colour"
};

/// A project path as Unity names the asset in a bundle: lower case (ASCII letters only).
inline std::string bundle_path(std::string_view projectPath) {
  std::string out(projectPath);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

/// FNV-1a 32-bit of the bytes of fullId (offset basis 2166136261, prime 16777619): the small number the wheel and its
/// messages name an emote by.
inline uint32_t emote_key(std::string_view fullId) {
  uint32_t hash = 2166136261u;
  for (const char c : fullId) {
    hash ^= static_cast<uint8_t>(c);
    hash *= 16777619u;
  }
  return hash;
}

namespace detail {

inline bool ends_with(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Nothing in a path may climb out of the folder it is read from.
inline bool climbs_out(std::string_view path) {
  return path.find("..") != std::string_view::npos || path.find('\\') != std::string_view::npos;
}

// An emote's name: lower case letters, digits and dashes, 1 to 40 of them.
inline bool is_emote_name(std::string_view name) {
  if (name.empty() || name.size() > 40) return false;
  for (const char c : name)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  return true;
}

// A clip or icon sits in the bundle at a project path under Assets/.
inline bool is_project_path(std::string_view path, std::string_view suffix) {
  return path.rfind("Assets/", 0) == 0 && !climbs_out(path) && ends_with(path, suffix);
}

inline bool is_song_path(std::string_view path) {
  return path.rfind("Assets/", 0) == 0 && !climbs_out(path) &&
         (ends_with(path, ".wav") || ends_with(path, ".ogg") || ends_with(path, ".mp3"));
}

// A loudness table lies under the pack's assets folder by a plain relative path.
inline bool is_loudness_path(std::string_view path) {
  if (path.empty() || path.front() == '/' || path.front() == '\\') return false;
  if (path.find("..") != std::string_view::npos || path.find(':') != std::string_view::npos ||
      path.find('\\') != std::string_view::npos)
    return false;
  return ends_with(path, ".txt");
}

// clipSource, clipLoopGuess, songSource and songLoopGuess are what the loop tool left behind: read silently, never
// warned about.
inline bool known_emote_field(std::string_view field) {
  for (std::string_view f : {"name", "clip",   "moveClip", "introClip", "song", "introSong", "icon", "label",
                             "loudness", "introLoudness", "loops", "songLoop", "clipSource", "clipLoopGuess",
                             "songSource", "songLoopGuess"})
    if (f == field) return true;
  return false;
}

// Reads a string field: a `required` one must be there, and any that is there must be text, and not empty. `who` names
// the owner in the why.
inline bool read_string(const nlohmann::json& object, const char* field, std::string& value, std::string& why,
                        bool required, std::string_view who) {
  const auto it = object.find(field);
  if (it == object.end()) {
    if (required) {
      why = std::string(who) + " has no " + field;
      return false;
    }
    value.clear();
    return true;
  }
  if (!it->is_string()) {
    why = std::string(who) + " has a " + field + " that is not text";
    return false;
  }
  value = it->get<std::string>();
  if (value.empty()) {
    why = std::string(who) + " has an empty " + field;
    return false;
  }
  return true;
}

}  // namespace detail

/// Reads an emotes.json; false with `why` (one English sentence for the log) on any error, `out` then empty.
/// Comments are allowed in the JSON, as the text tables' are. An unknown field is only a warning: a pack built by a
/// newer tool still loads, and its unknown fields are named for the log.
inline bool parse_catalogue(std::string_view json, std::string_view packId, Catalogue& out, std::string& why) {
  out = Catalogue{};
  try {
    nlohmann::json root = nlohmann::json::parse(json.begin(), json.end(), nullptr, false, true);
    if (root.is_discarded() || !root.is_object()) {
      why = "the pack's emotes.json is not a JSON object";
      return false;
    }
    Catalogue result;
    for (auto item = root.begin(); item != root.end(); ++item) {
      const std::string_view field = item.key();
      if (field != "format" && field != "bundle" && field != "emotes") result.warnings.push_back(item.key());
    }
    const auto format = root.find("format");
    if (format == root.end() || !format->is_number() || format->get<double>() != 1.0) {
      why = "the pack's emotes.json is not of format 1";
      return false;
    }
    if (!detail::read_string(root, "bundle", result.bundle, why, true, "the pack's emotes.json")) return false;
    if (result.bundle.find_first_of("/\\:") != std::string::npos || !detail::ends_with(result.bundle, ".bundle")) {
      why = "the pack's bundle must be one file name in its assets folder, ending in .bundle";
      return false;
    }
    const auto emotes = root.find("emotes");
    if (emotes == root.end() || !emotes->is_array() || emotes->empty() || emotes->size() > kMaxEmotesPerPack) {
      why = "the pack needs from 1 to " + std::to_string(kMaxEmotesPerPack) + " emotes";
      return false;
    }
    result.packId = std::string(packId);
    std::set<std::string> names;
    for (size_t i = 0; i < emotes->size(); ++i) {
      const nlohmann::json& e = (*emotes)[i];
      const std::string where = "emotes[" + std::to_string(i) + "]";
      if (!e.is_object()) {
        why = where + " is not an object";
        return false;
      }
      for (auto item = e.begin(); item != e.end(); ++item)
        if (!detail::known_emote_field(item.key())) result.warnings.push_back(where + "." + item.key());
      EmoteDef def;
      if (!detail::read_string(e, "name", def.name, why, true, where)) return false;
      if (!detail::read_string(e, "clip", def.clip, why, true, where)) return false;
      if (!detail::read_string(e, "song", def.song, why, true, where)) return false;
      if (!detail::read_string(e, "icon", def.icon, why, true, where)) return false;
      if (!detail::read_string(e, "label", def.label, why, true, where)) return false;
      if (!detail::read_string(e, "loudness", def.loudness, why, true, where)) return false;
      if (!detail::read_string(e, "moveClip", def.moveClip, why, false, where)) return false;
      if (!detail::read_string(e, "introClip", def.introClip, why, false, where)) return false;
      if (!detail::read_string(e, "introSong", def.introSong, why, false, where)) return false;
      if (!detail::read_string(e, "introLoudness", def.introLoudness, why, false, where)) return false;
      def.loops = true;
      if (const auto loops = e.find("loops"); loops != e.end()) {
        if (!loops->is_boolean()) {
          why = where + " has a loops that is not true or false";
          return false;
        }
        def.loops = loops->get<bool>();
      }
      if (const auto songLoop = e.find("songLoop"); songLoop != e.end()) {
        if (!songLoop->is_object()) {
          why = where + " has a songLoop that is not an object";
          return false;
        }
        const auto start = songLoop->find("start"), seconds = songLoop->find("seconds");
        if (start == songLoop->end() || seconds == songLoop->end() || !start->is_number() || !seconds->is_number()) {
          why = where + " has a songLoop without a start and a seconds number";
          return false;
        }
        const double begin = start->get<double>(), length = seconds->get<double>();
        if (!std::isfinite(begin) || !std::isfinite(length) || begin < 0.0 || length < 0.0) {
          why = where + " has a songLoop that is negative or too big to be a length of seconds";
          return false;
        }
        def.songLoop = {static_cast<float>(begin), static_cast<float>(length)};
      }
      if (!detail::is_emote_name(def.name)) {
        why = where + " has a name that is not 1 to 40 lower case letters, digits or dashes";
        return false;
      }
      if (!names.insert(def.name).second) {
        why = where + " repeats the name " + def.name;
        return false;
      }
      auto badPath = [&](const char* field, const std::string& path, bool good) {
        if (good) return false;
        why = where + " has a " + field + " that is not a project path under Assets/ with its proper ending";
        return true;
      };
      if (badPath("clip", def.clip, detail::is_project_path(def.clip, ".anim"))) return false;
      if (badPath("icon", def.icon, detail::is_project_path(def.icon, ".png"))) return false;
      if (badPath("song", def.song, detail::is_song_path(def.song))) return false;
      if (!def.moveClip.empty() && badPath("moveClip", def.moveClip, detail::is_project_path(def.moveClip, ".anim")))
        return false;
      if (!def.introClip.empty() &&
          badPath("introClip", def.introClip, detail::is_project_path(def.introClip, ".anim")))
        return false;
      if (!def.introSong.empty() && badPath("introSong", def.introSong, detail::is_song_path(def.introSong)))
        return false;
      if (!detail::is_loudness_path(def.loudness)) {
        why = where + " has a loudness path that is not a .txt file under the pack's assets folder";
        return false;
      }
      if (!def.introLoudness.empty() && !detail::is_loudness_path(def.introLoudness)) {
        why = where + " has an introLoudness path that is not a .txt file under the pack's assets folder";
        return false;
      }
      if (def.introSong.empty() != def.introLoudness.empty()) {
        why = where + " sets one of introSong and introLoudness without the other";
        return false;
      }
      if (!def.loops && (!def.moveClip.empty() || !def.introClip.empty())) {
        why = where + " is a one shot but sets a moveClip or introClip, which only a looping emote can play";
        return false;
      }
      def.id = result.packId + "." + def.name;
      def.key = emote_key(def.id);
      result.emotes.push_back(std::move(def));
    }
    out = std::move(result);
    return true;
  } catch (const std::exception&) {
    out = Catalogue{};
    why = "the pack's emotes.json could not be read";
    return false;
  } catch (...) {
    out = Catalogue{};
    why = "the pack's emotes.json could not be read";
    return false;
  }
}

/// The wheel's order: `own`'s emotes first, then each of `packs` sorted by packId, each in its file order. False with
/// `why` when two emotes share a key (a hash collision or a repeated id); `out` then empty.
inline bool wheel_order(const Catalogue& own, std::vector<Catalogue> packs, std::vector<EmoteDef>& out,
                        std::string& why) {
  out.clear();
  std::sort(packs.begin(), packs.end(),
            [](const Catalogue& a, const Catalogue& b) { return a.packId < b.packId; });
  std::vector<const Catalogue*> order;
  order.reserve(packs.size() + 1);
  order.push_back(&own);
  for (const Catalogue& pack : packs) order.push_back(&pack);
  std::vector<uint32_t> keys;
  std::vector<EmoteDef> wheel;
  for (const Catalogue* pack : order)
    for (const EmoteDef& e : pack->emotes) {
      const uint32_t key = emote_key(e.id);
      if (std::find(keys.begin(), keys.end(), key) != keys.end()) {
        why = "two emotes share the key " + std::to_string(key) + ", " + e.id + " among them";
        out.clear();
        return false;
      }
      keys.push_back(key);
      wheel.push_back(e);
    }
  out = std::move(wheel);
  return true;
}

}  // namespace pm::emotes
