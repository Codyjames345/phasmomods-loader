#pragma once

#include <windows.h>

#include <charconv>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

// What names a mod and its content: mod ids ("<author>.<mod>"), content ids ("<mod id>.<name>"),
// versions (MAJOR.MINOR.PATCH), the text the game may draw, and the old bundled ids. Shared by the
// launcher, the host and the content registry; plain string logic, tested offline.
namespace pm::modid {

constexpr size_t kMaxIdBytes = 64;
constexpr size_t kMaxOldIdBytes = 40;
constexpr size_t kMaxNameBytes = 64;  // a mod's name and its author's
constexpr size_t kMaxVersionBytes = 32;  // "4294967295.4294967295.4294967295", the largest a version can be

inline constexpr std::string_view kReservedAuthor = "vanilla";  // content ids use it for the game's own
// The project's own mods, which ship in the loader's install folder: their id author, and (in any case) their manifest
// author "PhasmoMods". A mod found anywhere else may use neither (core/mod_list.h reserved_author_refusal).
inline constexpr std::string_view kProjectAuthor = "phasmomods";

/** Whether `text` is the project's author, ignoring ASCII case and surrounding spaces: " PhasmoMods " is. */
inline bool names_project_author(std::string_view text) {
  while (!text.empty() && text.front() == ' ') text.remove_prefix(1);
  while (!text.empty() && text.back() == ' ') text.remove_suffix(1);
  if (text.size() != kProjectAuthor.size()) return false;
  for (size_t i = 0; i < text.size(); ++i) {
    const char c = text[i] >= 'A' && text[i] <= 'Z' ? static_cast<char>(text[i] - 'A' + 'a') : text[i];
    if (c != kProjectAuthor[i]) return false;
  }
  return true;
}

/** One part of a mod id: a-z, 0-9 and '-', and not empty. */
inline bool valid_part(std::string_view part) {
  if (part.empty()) return false;
  for (char c : part)
    if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
  return true;
}

/** A mod id: "<author>.<mod>", exactly one dot, 64 characters at most, and not the reserved author. */
inline bool valid_id(std::string_view id) {
  const size_t dot = id.find('.');
  if (id.size() > kMaxIdBytes || dot == std::string_view::npos) return false;
  const std::string_view author = id.substr(0, dot);
  return author != kReservedAuthor && valid_part(author) && valid_part(id.substr(dot + 1));
}

/** An old-format mod's id, from its mod.ini: flat, 1 to 40 bytes of a-z, 0-9 and '-'. */
inline bool valid_old_id(std::string_view id) { return id.size() <= kMaxOldIdBytes && valid_part(id); }

/** An id some installed mod can have, of either format. */
inline bool valid_any_id(std::string_view id) { return valid_id(id) || valid_old_id(id); }

struct Version {
  uint32_t major = 0, minor = 0, patch = 0;
  auto operator<=>(const Version&) const = default;  // part by part, as numbers: 1.0.10 is newer than 1.0.1
};

/**
 * A version: strictly MAJOR.MINOR.PATCH, as Semantic Versioning means them. No pre-release or build
 * tag, no leading zero, and each part fits 32 bits; anything else is nullopt.
 */
inline std::optional<Version> parse_version(std::string_view text) {
  Version v;
  for (uint32_t* part : {&v.major, &v.minor, &v.patch}) {
    const size_t digits = text.find_first_not_of("0123456789");
    const std::string_view number = text.substr(0, digits);
    if (number.empty() || (number.size() > 1 && number[0] == '0')) return std::nullopt;
    const auto [end, error] = std::from_chars(number.data(), number.data() + number.size(), *part);
    if (error != std::errc() || end != number.data() + number.size()) return std::nullopt;
    text.remove_prefix(number.size());
    if (part == &v.patch) break;
    if (text.empty() || text.front() != '.') return std::nullopt;
    text.remove_prefix(1);
  }
  if (!text.empty()) return std::nullopt;
  return v;
}

/**
 * Whether text from a manifest or another player's client can be drawn: 1 to `maxBytes` bytes of
 * valid UTF-8, with no control character or line separator (U+0000 to U+001F, U+007F to U+009F, U+2028
 * and U+2029, any of which could forge a line) and no angle bracket (the game's text is rich text,
 * where a tag could hide the consent notice's words).
 */
inline bool display_safe(std::string_view text, size_t maxBytes) {
  if (text.empty() || text.size() > maxBytes) return false;
  std::wstring wide(text.size(), L'\0');
  const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                        wide.data(), static_cast<int>(wide.size()));
  if (count <= 0) return false;
  wide.resize(static_cast<size_t>(count));
  for (const wchar_t c : wide)
    if (c < L' ' || (c >= L'\x7F' && c <= L'\x9F') || c == L'\x2028' || c == L'\x2029' || c == L'<' || c == L'>') return false;
  return true;
}

struct ContentId {
  std::string_view mod, name;
};

/**
 * A content id split into its mod's id and its name, at the last dot, since a name holds no dot:
 * "phasmomods.custom-ghosts.draugr" is the mod "phasmomods.custom-ghosts" and the name "draugr". The
 * game's own are "vanilla.<name>". nullopt when the part before the last dot cannot be a mod's id or
 * the name is empty. The views are into `id`.
 */
inline std::optional<ContentId> split_content_id(std::string_view id) {
  const size_t dot = id.rfind('.');
  if (dot == std::string_view::npos || dot + 1 == id.size()) return std::nullopt;
  const std::string_view mod = id.substr(0, dot);
  if (!valid_any_id(mod)) return std::nullopt;
  return ContentId{mod, id.substr(dot + 1)};
}

// The bundled mods' ids before they took an author, and the prefix their content ids used then. The
// saved choice, the saved custom difficulty pick, a mod.ini's requirement and an old mod's question
// about another player's mods move through this table.
struct OldBundled {
  std::string_view id, contentPrefix, newId;
};
inline constexpr OldBundled kOldBundled[] = {
    {"playable-ghost", "playable_ghost", "phasmomods.playable-ghost"},
    {"custom-ghosts", "custom_ghosts", "phasmomods.custom-ghosts"},
    {"spectral-flame", "spectral_flame", "phasmomods.spectral-flame"},
    {"ghost-tells", "ghost_tells", "phasmomods.ghost-tells"},
};

/** Whether `id` is a bundled mod's old id, which always means that mod under its new one. */
inline bool old_bundled_id(std::string_view id) {
  for (const OldBundled& b : kOldBundled)
    if (id == b.id) return true;
  return false;
}

/** The id a saved mod id has now: the new one for an old bundled id, itself otherwise. */
inline std::string migrated_id(std::string_view id) {
  for (const OldBundled& b : kOldBundled)
    if (id == b.id) return std::string(b.newId);
  return std::string(id);
}

/** The id a saved content id has now: "custom_ghosts.brownie" is "phasmomods.custom-ghosts.brownie". */
inline std::string migrated_content_id(std::string_view id) {
  const size_t dot = id.rfind('.');
  if (dot != std::string_view::npos && dot + 1 < id.size())
    for (const OldBundled& b : kOldBundled)
      if (id.substr(0, dot) == b.contentPrefix) return std::string(b.newId) + std::string(id.substr(dot));
  return std::string(id);
}

}  // namespace pm::modid
