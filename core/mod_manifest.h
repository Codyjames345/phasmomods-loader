#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "mod_ids.h"

#define JSON_NO_IO  // the parser reads a string here, and needs no stream operators
#include "../third_party/nlohmann/json.hpp"
#include "text_table.h"
static_assert(NLOHMANN_JSON_VERSION_MAJOR == 3 && NLOHMANN_JSON_VERSION_MINOR == 12 && NLOHMANN_JSON_VERSION_PATCH == 0,
              "third_party/nlohmann is pinned: change this with its VERSION.txt and NOTICE");

// A mod's manifest.json, the one file that says who a mod is: its identity, version, author and
// dependencies, shared by the launcher and the host (sdk/README.md, "The shape of a mod";
// sdk/manifest.schema.json). A manifest may come from anybody, so it is untrusted input: it is bounded
// in size, depth and every field, nothing thrown leaves the parser, and anything wrong refuses that mod
// with a reason for the player, and never stops the game or the other mods. Pure logic, tested offline.
namespace pm::modlist {

constexpr size_t kMaxManifestBytes = 64 * 1024;
constexpr size_t kMaxManifestDepth = 16;     // past anything a later format would need
constexpr size_t kMaxDescriptionBytes = 512;
constexpr size_t kMaxWebsiteBytes = 256;
constexpr size_t kMaxListEntries = 64;       // dependencies, and incompatibilities
constexpr size_t kMaxUnknownFieldsListed = 16;  // in the log's one line

struct Dependency {
  std::string id;              // a mod id of either format
  std::string minimumVersion;  // MAJOR.MINOR.PATCH, or empty for any version
  bool required = true;        // false: the mod uses it when it is there
};

struct Manifest {
  std::string id, name, dll, description;
  std::string author, version, website;  // an old-format mod has no author, and its version is unknown: empty
  std::vector<Dependency> dependencies;
  std::vector<std::string> incompatibilities;  // the ids of mods it cannot run beside
  std::vector<std::string> unknownFields;      // what this loader does not know, for one log line
  bool recordsVoice = false;                   // it keeps other players' speech in memory: the consent notice says so
  bool developerOnly = false;                  // it ships for developers: only offered with launcher.ini developer=1
  bool oldFormat = false;                      // from a mod.ini
};

/** Whether Windows takes `name`'s part before its first dot as a device, not a file: NUL.dll opens nothing a mod could be. */
inline bool reserved_device_name(std::string_view name) {
  std::string stem(name.substr(0, name.find('.')));
  for (char& c : stem)
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
  if (stem == "CON" || stem == "PRN" || stem == "AUX" || stem == "NUL") return true;
  return stem.size() == 4 && (stem.rfind("COM", 0) == 0 || stem.rfind("LPT", 0) == 0) && stem[3] >= '1' && stem[3] <= '9';
}

/** A DLL file name with no folder in it, so a manifest can only name a file beside itself. */
inline bool valid_dll(std::string_view name) {
  if (name.size() < 5 || name.size() > 80 || name.find_first_of("/\\:<>\"|?*") != std::string_view::npos ||
      reserved_device_name(name))
    return false;
  for (char c : name)
    if (static_cast<unsigned char>(c) < 0x20) return false;
  std::string_view ext = name.substr(name.size() - 4);
  return (ext[0] == '.') && (ext[1] == 'd' || ext[1] == 'D') && (ext[2] == 'l' || ext[2] == 'L') &&
         (ext[3] == 'l' || ext[3] == 'L');
}

namespace detail {

using nlohmann::json;

// A field's name as the log may repeat it; the name itself came from the file.
inline std::string printable_field(const std::string& name) {
  return modid::display_safe(name, 32) ? name : "(a field with an unprintable name)";
}

inline void note_unknown(Manifest& m, const std::string& name) {
  if (m.unknownFields.size() < kMaxUnknownFieldsListed) m.unknownFields.push_back(printable_field(name));
}

// One field a mod must have: a string the game can draw.
inline bool shown_text(const json& j, const char* field, const std::string& missing, size_t maxBytes, std::string& out, std::string& why) {
  const auto it = j.find(field);
  if (it == j.end() || !it->is_string() || it->get_ref<const std::string&>().empty()) {
    why = missing;
    return false;
  }
  out = it->get_ref<const std::string&>();
  if (modid::display_safe(out, maxBytes)) return true;
  why = i18n::text("manifest.field-unsafe", {{"field", field}, {"bytes", std::to_string(maxBytes)}});
  return false;
}

inline bool read_id_list_entry(const std::string& id, const Manifest& m, const std::vector<std::string>& seen, const char* what, std::string& why) {
  if (!modid::valid_any_id(id)) why = i18n::text("manifest.list-not-id", {{"field", what}});
  else if (id == m.id) why = i18n::text("manifest.list-itself", {{"field", what}});
  else if (std::find(seen.begin(), seen.end(), id) != seen.end()) why = i18n::text("manifest.list-twice", {{"field", what}});
  else return true;
  return false;
}

inline bool read_dependencies(const json& j, Manifest& m, std::string& why) {
  const auto list = j.find("dependencies");
  if (list == j.end()) return true;
  if (!list->is_array() || list->size() > kMaxListEntries) {
    why = i18n::text("manifest.dependencies-list", {{"count", std::to_string(kMaxListEntries)}});
    return false;
  }
  std::vector<std::string> seen;
  for (const json& entry : *list) {
    Dependency d;
    const auto id = entry.is_object() ? entry.find("id") : entry.end();
    if (!entry.is_object() || id == entry.end() || !id->is_string()) {
      why = i18n::text("manifest.dependency-no-id");
      return false;
    }
    d.id = modid::migrated_id(id->get_ref<const std::string&>());  // a bundled mod's old id always means that mod
    if (!read_id_list_entry(d.id, m, seen, "dependencies", why)) return false;
    seen.push_back(d.id);
    for (const auto& [key, value] : entry.items()) {
      if (key == "id") continue;
      if (key == "minimumVersion") {
        if (!value.is_string() || !modid::parse_version(value.get_ref<const std::string&>())) {
          why = i18n::text("manifest.dependency-version");
          return false;
        }
        d.minimumVersion = value.get_ref<const std::string&>();
      } else if (key == "required") {
        if (!value.is_boolean()) {
          why = i18n::text("manifest.dependency-required");
          return false;
        }
        d.required = value.get<bool>();
      } else {
        note_unknown(m, "dependencies." + key);
      }
    }
    m.dependencies.push_back(std::move(d));
  }
  return true;
}

inline bool read_incompatibilities(const json& j, Manifest& m, std::string& why) {
  const auto list = j.find("incompatibilities");
  if (list == j.end()) return true;
  if (!list->is_array() || list->size() > kMaxListEntries) {
    why = i18n::text("manifest.incompatibilities-list", {{"count", std::to_string(kMaxListEntries)}});
    return false;
  }
  for (const json& entry : *list) {
    if (!entry.is_string()) {
      why = i18n::text("manifest.incompatibility-not-id");
      return false;
    }
    std::string id = modid::migrated_id(entry.get_ref<const std::string&>());
    if (!read_id_list_entry(id, m, m.incompatibilities, "incompatibilities", why)) return false;
    m.incompatibilities.push_back(id);
  }
  return true;
}

inline bool read_manifest(std::string_view text, Manifest& m, std::string& why) {
  bool tooDeep = false, repeated = false;
  std::vector<std::set<std::string>> open;  // the field names of each object still open
  // Parsing keeps its own stack, so depth costs no native stack; the limit bounds what is kept. A
  // refused value is dropped as it is read, and its object's end is not reported, but tooDeep refuses the file then.
  json::parser_callback_t watch = [&](int depth, json::parse_event_t event, json& value) {
    if (static_cast<size_t>(depth) > kMaxManifestDepth) {
      tooDeep = true;
      return false;
    }
    if (event == json::parse_event_t::object_start) open.emplace_back();
    else if (event == json::parse_event_t::object_end && !open.empty()) open.pop_back();
    else if (event == json::parse_event_t::key && !open.empty() && !open.back().insert(value.get_ref<const std::string&>()).second)
      repeated = true;
    return true;
  };
  const json j = json::parse(text.data(), text.data() + text.size(), watch, false);
  if (tooDeep) {
    why = i18n::text("manifest.too-deep", {{"levels", std::to_string(kMaxManifestDepth)}});
    return false;
  }
  if (j.is_discarded()) {
    why = i18n::text("manifest.not-json");
    return false;
  }
  if (!j.is_object()) {
    why = i18n::text("manifest.not-object");
    return false;
  }
  if (repeated) {
    why = i18n::text("manifest.field-twice");
    return false;
  }
  const auto id = j.find("id");
  if (id == j.end() || !id->is_string()) {
    why = i18n::text("manifest.no-id");
    return false;
  }
  m.id = id->get_ref<const std::string&>();
  if (m.id.substr(0, m.id.find('.')) == modid::kReservedAuthor && m.id.find('.') != std::string::npos) {
    why = i18n::text("manifest.vanilla-author");
    return false;
  }
  if (!modid::valid_id(m.id)) {
    why = i18n::text("manifest.bad-id");
    return false;
  }
  const auto version = j.find("version");
  if (version == j.end() || !version->is_string() || !modid::parse_version(version->get_ref<const std::string&>())) {
    why = i18n::text("manifest.bad-version");
    return false;
  }
  m.version = version->get_ref<const std::string&>();
  if (!shown_text(j, "name", i18n::text("manifest.no-name"), modid::kMaxNameBytes, m.name, why) ||
      !shown_text(j, "author", i18n::text("manifest.no-author"), modid::kMaxNameBytes, m.author, why) ||
      !shown_text(j, "description", i18n::text("manifest.no-description"), kMaxDescriptionBytes, m.description, why))
    return false;
  const auto dll = j.find("dll");
  if (dll == j.end() || !dll->is_string() || !valid_dll(dll->get_ref<const std::string&>())) {
    why = i18n::text("manifest.bad-dll");
    return false;
  }
  m.dll = dll->get_ref<const std::string&>();
  if (const auto website = j.find("website"); website != j.end()) {
    if (!website->is_string()) {
      why = i18n::text("manifest.website-not-text");
      return false;
    }
    m.website = website->get_ref<const std::string&>();
    if (!m.website.empty() && (!modid::display_safe(m.website, kMaxWebsiteBytes) ||
                               !(m.website.rfind("https://", 0) == 0 || m.website.rfind("http://", 0) == 0))) {
      why = i18n::text("manifest.bad-website", {{"bytes", std::to_string(kMaxWebsiteBytes)}});
      return false;
    }
  }
  if (const auto voice = j.find("recordsVoice"); voice != j.end()) {
    if (!voice->is_boolean()) {
      why = i18n::text("manifest.records-voice");
      return false;
    }
    m.recordsVoice = voice->get<bool>();
  }
  if (const auto dev = j.find("developerOnly"); dev != j.end()) {
    if (!dev->is_boolean()) {
      why = i18n::text("manifest.developer-only");
      return false;
    }
    m.developerOnly = dev->get<bool>();
  }
  if (!read_dependencies(j, m, why) || !read_incompatibilities(j, m, why)) return false;
  static constexpr std::string_view kKnown[] = {"$schema", "id",  "name",    "author",       "version",
                                                "description", "dll", "website", "dependencies", "incompatibilities",
                                                "recordsVoice", "developerOnly"};
  for (const auto& entry : j.items())
    if (std::find(std::begin(kKnown), std::end(kKnown), entry.key()) == std::end(kKnown)) note_unknown(m, entry.key());
  std::sort(m.unknownFields.begin(), m.unknownFields.end());
  return true;
}

}  // namespace detail

/**
 * Read a manifest.json. On failure `out` is empty and `why` says, in a sentence for the player, what is
 * wrong with it. An unknown field is listed in `out.unknownFields` and otherwise ignored, so a later
 * loader can add fields. Nothing is thrown: a failure of any kind, down to memory, refuses the mod.
 */
inline bool parse_manifest_json(std::string_view text, Manifest& out, std::string& why) noexcept {
  try {
    out = {};
    why.clear();
    if (text.size() > kMaxManifestBytes) {
      why = i18n::text("manifest.too-large-json");
      return false;
    }
    Manifest m;
    if (!detail::read_manifest(text, m, why)) return false;
    out = std::move(m);
    return true;
  } catch (...) {
    try {
      why = i18n::text("manifest.unreadable-json");
    } catch (...) {
      // No memory left even for the reason: the mod stays off, and the caller's empty reason says no more.
    }
    return false;
  }
}

/** Read at most kMaxManifestBytes of a manifest file; false, with `why`, when it cannot be read or is larger. */
inline bool read_manifest_file(const std::filesystem::path& file, std::string& text, std::string& why) {
  std::ifstream in(file, std::ios::binary);
  if (!in) {
    why = i18n::text("manifest.unreadable");
    return false;
  }
  text.resize(kMaxManifestBytes + 1);
  in.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<size_t>(in.gcount()));
  if (text.size() > kMaxManifestBytes) {
    why = i18n::text("manifest.too-large");
    return false;
  }
  return true;
}

}  // namespace pm::modlist
