// The offline check: resolves every map entry against a Cpp2IL dump and a copy of GameAssembly.dll,
// and the native sites against a copy of UnityPlayer.dll, without running the game.
// tools/gamecheck.ps1 prepares them and runs it.
//
// usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--unity-player <UnityPlayer.dll>]
//                  [--addresses <tsv>] [--mods <dir>] [--scene-entries <tsv>] [--expect <file>]...
// It prints one line per entry and a summary. Exports resolve from the image's export table. Without
// --unity-player every native site is missing, and counts as a failure when it is required. An icall
// cannot be checked without the running game: it is listed as checked at startup only, and never
// counts as a failure. A well-formed scene fact (a Scene, Layer, AnimatorParam or ShaderProperty entry)
// is not in the code either: it is listed as checked in the scene files and never counts as a failure
// here. With --scene-entries the scene facts go to that file for tools/assetcheck.py, one per line:
// key, kind (scene, layer, animator or shader), 1 when required or 0, scene, path, component and
// name, tab separated, with an empty column for a field the entry does not set. With --addresses it
// writes key, kind, value and length for each resolved entry (a native site's value is its RVA in
// UnityPlayer, with no length). With --mods it loads every mod DLL in <dir> and its subfolders (a DLL that exports
// pmmod_info), and after the summary prints "would not load <mod id> (contract <version>)" for each
// mod built against a host contract this build's host does not run (pm_api_compatible), and, for
// the others, asks for the game entries each requires (pmmod_requires) and prints
// "would switch off <mod id>: <keys>" for each one the host would not start on this build, and the same
// when it records voice and the manifest.json beside it does not say so.
// With --expect (repeatable) each line "<key> <hex>" of the file is a value the key must resolve to: a
// method's or native site's RVA, a field's offset, an enum value's number. A key that resolves to
// another value (or does not resolve) prints "EXPECT <key>: map 0x.. != expected 0x.." and counts as
// a failure; a key the map lacks, a kind that has no number, or a line that is not "<key> <hex>" is
// unreadable input. The files are the migration's record of the values an entry replaced, kept
// outside the repository (re/dump/expect).
// After the summary it prints the ranges of the game's GhostType and EvidenceType enums, from which
// custom content is numbered at runtime ("CONTENT GhostType 0..30, EvidenceType 0..7"), and a line for
// each value whose name cannot be a vanilla id, or "CONTENT not read: <why>" when the enums cannot be
// read (the entries' own lines are the failure). Nothing here asserts a fixed number. Then "HOST steering
// available", or "HOST steering unavailable: <keys>" naming the host's own steering entries
// (pm::map::kSteeringEntries) that did not resolve; their own lines are the failure.
// Exit 0 when every required entry it can check resolved and no mod would be switched off, 1 when
// one did not or one would, 2 on bad arguments, unreadable input or an exception (its reason printed).
//
// usage: gamecheck --map-digest <file>
// Writes the map's digest (sdk/resolver/map_digest.h) to the file and checks nothing. The build runs it, for
// what must refuse a stale checked-build record (tools/check_build_record.ps1).
//
// With --game-data <dir> (the game's data folder, or a copy holding globalgamemanagers and
// il2cpp_data/Metadata/global-metadata.dat) it prints "ENGINE <Unity version>, metadata <version>", and refuses a build
// the host's compiled-in layouts were not written for (core/engine_version.h) with "ENGINE refused: <why>", which counts as a
// failure. The host stays inactive on such a build.
//
// usage: gamecheck --write-record <folder> --image <GameAssembly.dll> --unity-player <UnityPlayer.dll> --game-data <dir>
// Writes this build's checked-build record (core/build_record.h) into the folder, replacing the one it had,
// and prints its name. It checks nothing else, so tools/gamecheck.ps1 runs it only once the checks have passed. A build whose
// engine or metadata version the host refuses gets no record.
// Exit 0 when it wrote the file, 2 when a file cannot be read or written, or the versions are refused.
#include <windows.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <format>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "all.h"
#include "build_record.h"
#include "file_hash.h"
#include "map_digest.h"
#include "mod_exports.h"
#include "quiet_load.h"
#include "dump_model.h"
#include "engine_version.h"
#include "game_enums.h"
#include "image.h"
#include "mod_api.h"
#include "resolver.h"

namespace {

using namespace pm::sdk;

constexpr const char* kUsage =
    "usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--unity-player <UnityPlayer.dll>]\n"
    "                 [--addresses <tsv>] [--mods <dir>] [--scene-entries <tsv>] [--expect <file>]...\n"
    "                 [--game-data <dir>] [--no-method-tables]\n"
    "       gamecheck --map-digest <file>\n"
    "       gamecheck --write-record <folder> --image <GameAssembly.dll> --unity-player <UnityPlayer.dll> --game-data <dir>\n";

struct Args {
  std::filesystem::path dump, image, unityPlayer, addresses, mods, sceneEntries, mapDigest, recordFolder, gameData;
  std::vector<std::filesystem::path> expect;
  bool noMethodTables = false;  // an image without code-gen modules: the dump's addresses stand (the tool's tests)
};

bool parse(int argc, wchar_t** argv, Args& out) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view flag = argv[i];
    if (flag == L"--no-method-tables") {
      out.noMethodTables = true;
      continue;
    }
    if (i + 1 >= argc) return false;
    if (flag == L"--expect") {
      out.expect.emplace_back(argv[++i]);
      continue;
    }
    std::filesystem::path* target = flag == L"--dump"            ? &out.dump
                                    : flag == L"--image"         ? &out.image
                                    : flag == L"--unity-player"  ? &out.unityPlayer
                                    : flag == L"--addresses"     ? &out.addresses
                                    : flag == L"--mods"          ? &out.mods
                                    : flag == L"--scene-entries" ? &out.sceneEntries
                                    : flag == L"--map-digest"    ? &out.mapDigest
                                    : flag == L"--write-record"  ? &out.recordFolder
                                    : flag == L"--game-data"     ? &out.gameData
                                                                 : nullptr;
    if (!target || !target->empty()) return false;
    *target = argv[++i];
  }
  if (!out.mapDigest.empty()) return true;
  if (!out.recordFolder.empty()) return !out.image.empty() && !out.unityPlayer.empty() && !out.gameData.empty();
  return !out.dump.empty() && !out.image.empty();
}

const char* kind_word(Kind k) {
  switch (k) {
    case Kind::Type: return "type";
    case Kind::Method: return "method";
    case Kind::Field: return "field";
    case Kind::EnumValue: return "enum";
    case Kind::Code: return "code";
    case Kind::Icall: return "icall";
    case Kind::Export: return "export";
    case Kind::Scene: return "scene";
    case Kind::Layer: return "layer";
    case Kind::AnimatorParam: return "animator";
    case Kind::NativeSite: return "native";
    case Kind::ShaderProperty: return "shader";
    case Kind::Inflated: return "inflated";
  }
  return "?";
}

// A resolved entry's value: a type's name in this build, a method's, code entry's or export's RVA, a
// native site's RVA in UnityPlayer, a field's offset, an enum value's number, an icall's name, a
// layer's index, a path, a parameter's or shader property's name.
std::string value_of(const Result& r) {
  switch (r.desc->kind) {
    case Kind::Type:
    case Kind::Icall: return r.currentName;
    case Kind::Method:
    case Kind::Code:
    case Kind::Export:
    case Kind::NativeSite: return std::format("0x{:X}", r.rva);
    case Kind::Field: return std::format("+0x{:X}", r.offset);
    case Kind::EnumValue:
    case Kind::Layer: return std::to_string(r.value);
    case Kind::Scene:
    case Kind::AnimatorParam:
    case Kind::ShaderProperty:
    case Kind::Inflated: return r.currentName;  // never resolved offline (checked at startup only)
  }
  return {};
}

// A path for messages, in UTF-8 like the rest of the output.
std::string utf8(const std::filesystem::path& p) {
  const std::u8string text = p.u8string();
  return std::string(text.begin(), text.end());
}

// A host contract version as people write it: "2.1". Version 1, the first contract, had no minor.
std::string contract(uint32_t version) {
  if (version >> 16 == 0) return std::format("{}.0", version);
  return std::format("{}.{}", version >> 16, version & 0xFFFFu);
}

// Checked elsewhere than here: in the running game at startup, or in the scene files (which
// tools/assetcheck.py checks).
bool checked_elsewhere(const Result& r) { return checked_at_startup(r) || checked_in_scene(r); }

std::string line_for(const Result& r) {
  if (checked_at_startup(r)) return std::format("STARTUP {} {} ({})", r.desc->key, r.desc->name, kCheckedAtStartup);
  if (checked_in_scene(r)) return std::format("SCENE {} {} ({})", r.desc->key, r.currentName, kCheckedInScene);
  std::string line = std::format("{} {}", status_word(r.status), r.desc->key);
  if (r.status != Status::Resolved) {
    line += " (" + r.detail + ")";
    for (const std::string& c : r.candidates) line += "\n    " + c;
    return line;
  }
  line += " " + value_of(r);
  if (!r.twins.empty()) {
    line += " twins";
    for (const uint64_t t : r.twins) line += std::format(" 0x{:X}", t);
  }
  if (!r.aliases.empty()) {
    line += " aliases";
    for (const std::string& a : r.aliases) line += " " + a;
  }
  // A resolved entry's detail says only what changed since the last mapped build.
  if (!r.detail.empty()) line += ", " + r.detail;
  return line;
}

// Whether the DLL exports a function named `name`. Read from the file, so a DLL that is not a mod
// (the host itself, whose loading starts a thread) is never loaded.
bool exports(const std::filesystem::path& dll, std::string_view name) {
  const std::vector<uint8_t> image = map_pe_file(dll);
  return CodeImage(image).exported(name) != 0;
}

// Calls `take` with each entry of `dir`. False when `dir` cannot be listed, when moving to the next
// entry fails, or when `take` says it could not read an entry. Nothing here throws: the iterator
// moves with increment(error), never the throwing operator++ a range-for uses.
template <class Take>
bool each_entry(const std::filesystem::path& dir, Take take) {
  std::error_code error;
  for (std::filesystem::directory_iterator it(dir, error), end; !error && it != end; it.increment(error))
    if (!take(*it)) return false;
  return !error;
}

// The DLLs in `dir` and in its subfolders (a release keeps each mod in its own), sorted. False when
// a folder or an entry in it cannot be read. Each entry's kind is asked with its own error_code, so
// one entry's failure is never cleared by the next one's success.
bool dlls_in(const std::filesystem::path& dir, std::vector<std::filesystem::path>& out) {
  std::vector<std::filesystem::path> folders{dir};
  const bool listed = each_entry(dir, [&](const std::filesystem::directory_entry& e) {
    std::error_code error;
    if (e.is_directory(error)) folders.push_back(e.path());
    return !error;
  });
  if (!listed) return false;
  for (const std::filesystem::path& folder : folders) {
    const bool read = each_entry(folder, [&](const std::filesystem::directory_entry& e) {
      std::error_code error;
      if (e.is_regular_file(error) && _wcsicmp(e.path().extension().c_str(), L".dll") == 0) out.push_back(e.path());
      return !error;
    });
    if (!read) return false;
  }
  std::sort(out.begin(), out.end());
  return true;
}

// Loads each mod in `dir` and prints, for each one the host would not start on this build, why: the
// host contract it was built against, which the host refuses to load, or the required entries that
// did not resolve. The number of such mods, or -1 when `dir` or a mod cannot be read (reported).
int check_mods(const std::filesystem::path& dir, const Resolution& resolution) {
  std::vector<std::filesystem::path> dlls;
  if (!dlls_in(dir, dlls)) {
    std::fprintf(stderr, "gamecheck: cannot read the mods folder %s\n", utf8(dir).c_str());
    return -1;
  }
  int off = 0;
  bool unreadable = false;
  for (const std::filesystem::path& dll : dlls) {
    const std::string newInfo = std::string(pm::modexports::kPrefix) + "info", oldInfo = std::string(pm::modexports::kOldPrefix) + "info";
    if (!exports(dll, newInfo.c_str()) && !exports(dll, oldInfo.c_str())) continue;  // not a mod
    HMODULE module = pm::dll::load_mod(dll.c_str(), LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto lookup = [&](const char* full) { return reinterpret_cast<void*>(GetProcAddress(module, full)); };
    const auto info = module ? reinterpret_cast<PmModInfoFn>(pm::modexports::find(lookup, "info").address) : nullptr;
    const PmModInfo* mod = info ? info() : nullptr;
    if (!mod || !mod->id) {
      std::fprintf(stderr, "gamecheck: cannot load the mod %s\n", utf8(dll).c_str());
      unreadable = true;
      continue;
    }
    if (!pm_api_compatible(PMMOD_API_VERSION, mod->apiVersion)) {
      std::printf("would not load %s (contract %s)\n", mod->id, contract(mod->apiVersion).c_str());
      ++off;
      continue;
    }
    // A mod without pmmod_requires requires no game entry. One that requires an export or a native site
    // is never started: the host gives those to its own code only.
    std::string lacking;
    if (const auto listRequired = reinterpret_cast<PmModRequiresFn>(pm::modexports::find(lookup, "requires").address)) {
      const char* const* keys = nullptr;
      const size_t n = listRequired(&keys);
      for (size_t i = 0; keys && i < n; ++i) {
        const Result* r = keys[i] ? resolution.find(keys[i]) : nullptr;
        if (keys[i] && (!r || r->desc->kind == Kind::Export || r->desc->kind == Kind::NativeSite ||
                        (r->status != Status::Resolved && !checked_elsewhere(*r))))
          lacking += (lacking.empty() ? "" : ", ") + std::string(keys[i]);
      }
    }
    if (!lacking.empty()) {
      std::printf("would switch off %s: %s\n", mod->id, lacking.c_str());
      ++off;
    }
    // The host keeps off a DLL that records voice when its manifest does not say so: the player agreed on the manifest's word.
    pm::modlist::Manifest manifest;
    std::string text, why;
    const std::filesystem::path beside = dll.parent_path() / L"manifest.json";
    if ((mod->flags & PM_MOD_RECORDS_VOICE) && pm::modlist::read_manifest_file(beside, text, why) &&
        pm::modlist::parse_manifest_json(text, manifest, why) && !manifest.recordsVoice) {
      std::printf("would switch off %s: it records voice, and its manifest does not say so\n", mod->id);
      ++off;
    }
  }
  return unreadable ? -1 : off;
}

// Writes the well-formed scene facts for tools/assetcheck.py. False, with nothing left behind and the
// reason printed, when the file cannot be written.
bool write_scene_entries(const std::filesystem::path& path, const Resolution& resolution) {
  std::FILE* out = nullptr;
  if (_wfopen_s(&out, path.c_str(), L"wb") != 0 || !out) {
    std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(path).c_str());
    return false;
  }
  const auto text = [](const char* s) { return s ? s : ""; };
  for (const Result& r : resolution.results) {
    if (!checked_in_scene(r)) continue;
    const EntryDesc& d = *r.desc;
    std::fprintf(out, "%s\t%s\t%d\t%s\t%s\t%s\t%s\n", d.key, kind_word(d.kind), (d.flags & kOptional) ? 0 : 1,
                 text(d.scene), text(d.path), text(d.component), text(d.name));
  }
  if (std::fclose(out) != 0) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
    std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(path).c_str());
    return false;
  }
  return true;
}

// A resolved entry's number for --expect: an RVA, an offset or an enum value. False for a kind
// that has none.
bool number_of(const Result& r, uint64_t& out) {
  switch (r.desc->kind) {
    case Kind::Method:
    case Kind::Code:
    case Kind::Export:
    case Kind::NativeSite: out = r.rva; return true;
    case Kind::Field: out = r.offset; return true;
    case Kind::EnumValue: out = static_cast<uint64_t>(r.value); return true;
    default: return false;
  }
}

// Checks the expectation files against the resolution. The number of keys that differ, or -1 when a
// file cannot be read, a line is malformed, a key is not in the map or has no number (reported).
int check_expectations(const std::vector<std::filesystem::path>& files, const Resolution& resolution) {
  int different = 0;
  bool unreadable = false;
  for (const std::filesystem::path& file : files) {
    std::ifstream in(file);
    if (!in) {
      std::fprintf(stderr, "gamecheck: cannot read %s\n", utf8(file).c_str());
      return -1;
    }
    std::string line;
    for (int number = 1; std::getline(in, line); ++number) {
      std::istringstream fields(line);
      std::string key, hex, extra;
      if (!(fields >> key)) continue;  // a blank line
      uint64_t expected = 0;
      bool formed = (fields >> hex) && !(fields >> extra) && hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X');
      if (formed) {  // all of it must be hex digits that fit 64 bits
        const auto parsed = std::from_chars(hex.data() + 2, hex.data() + hex.size(), expected, 16);
        formed = parsed.ec == std::errc() && parsed.ptr == hex.data() + hex.size();
      }
      if (!formed) {
        std::fprintf(stderr, "gamecheck: %s:%d: expected \"<key> <hex>\"\n", utf8(file).c_str(), number);
        unreadable = true;
        continue;
      }
      const Result* r = resolution.find(key.c_str());
      uint64_t actual = 0;
      if (!r || !number_of(*r, actual)) {
        std::fprintf(stderr, "gamecheck: %s:%d: %s is %s\n", utf8(file).c_str(), number, key.c_str(),
                     r ? "an entry with no number to expect" : "not a map entry");
        unreadable = true;
        continue;
      }
      if (r->status != Status::Resolved) {
        std::printf("EXPECT %s: map does not resolve != expected 0x%llX\n", key.c_str(),
                    static_cast<unsigned long long>(expected));
        ++different;
      } else if (actual != expected) {
        std::printf("EXPECT %s: map 0x%llX != expected 0x%llX\n", key.c_str(), static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
        ++different;
      }
    }
  }
  return unreadable ? -1 : different;
}

// Writes `text` to `path`. False, with the reason printed and nothing left behind, when it cannot.
bool write_text(const std::filesystem::path& path, const std::string& text) {
  std::FILE* out = nullptr;
  if (_wfopen_s(&out, path.c_str(), L"wb") != 0 || !out) {
    std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(path).c_str());
    return false;
  }
  const bool written = std::fwrite(text.data(), 1, text.size(), out) == text.size();
  if (std::fclose(out) == 0 && written) return true;
  std::error_code ignored;
  std::filesystem::remove(path, ignored);
  std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(path).c_str());
  return false;
}

// This build's record, from the two libraries as they were checked. 2 when one cannot be hashed or the file written.
int write_build_record(const Args& args) {
  if (const pm::engine::Versions versions = pm::engine::read(args.gameData); !pm::engine::known(versions)) {
    std::fprintf(stderr, "gamecheck: no record: %s\n", pm::engine::refusal(versions).c_str());
    return 2;
  }
  char gameAssembly[65], unityPlayer[65];
  if (!pm::build::sha256_file(args.image.c_str(), gameAssembly) || !pm::build::sha256_file(args.unityPlayer.c_str(), unityPlayer)) {
    std::fprintf(stderr, "gamecheck: cannot read %s or %s\n", utf8(args.image).c_str(), utf8(args.unityPlayer).c_str());
    return 2;
  }
  const pm::build::Record record{gameAssembly, unityPlayer, map_digest(pm::map::kAllEntries)};
  std::error_code error;
  std::filesystem::create_directories(args.recordFolder, error);
  const std::string name = pm::build::record_file_name(record);
  if (!write_text(args.recordFolder / pm::text::widen(name), pm::build::write_record(record))) return 2;
  std::printf("RECORD %s\n", name.c_str());
  return 0;
}

// The check itself; wmain contains what it throws.
int run(int argc, wchar_t** argv) {
  Args args;
  if (!parse(argc, argv, args)) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  if (!args.mapDigest.empty()) return write_text(args.mapDigest, map_digest(pm::map::kAllEntries) + "\n") ? 0 : 2;
  if (!args.recordFolder.empty()) return write_build_record(args);
  // The previous build's addresses must not outlive a failed check: the RE scripts would read them.
  std::error_code removeError;
  if (!args.addresses.empty() && !std::filesystem::remove(args.addresses, removeError) && removeError) {
    std::fprintf(stderr, "gamecheck: cannot remove %s\n", utf8(args.addresses).c_str());
    return 2;
  }

  const std::vector<std::string> assemblies = assemblies_of(pm::map::kAllEntries);
  GameModel model;
  std::string error;
  size_t unparsed = 0;
  if (!load_dump(args.dump, assemblies, model, error, &unparsed)) {
    std::fprintf(stderr, "gamecheck: %s\n", error.c_str());
    return 2;
  }
  // A member the model lacks could make a look-alike seem unique, so a partly read dump is refused.
  if (unparsed != 0) {
    std::fprintf(stderr, "gamecheck: %zu member lines of the dump could not be read\n", unparsed);
    return 2;
  }
  const std::vector<uint8_t> bytes = map_pe_file(args.image);
  const CodeImage image(bytes);
  if (bytes.empty() || !image.valid()) {
    std::fprintf(stderr, "gamecheck: %s is not a readable PE32+ image\n", utf8(args.image).c_str());
    return 2;
  }
  // The runtime takes a method's address from these tables, so an address the dump alone gives (a generic
  // definition's) must not pass here and be missing in the game.
  if (!args.noMethodTables) {
    MethodTables tables;
    if (!read_method_tables(bytes, assemblies, tables, error)) {
      std::fprintf(stderr, "gamecheck: %s: %s\n", utf8(args.image).c_str(), error.c_str());
      return 2;
    }
    apply_method_tables(tables, model);
  }

  // UnityPlayer.dll, for the native sites. One given that cannot be read is unreadable input.
  std::vector<uint8_t> unityBytes;
  std::optional<CodeImage> unity;
  if (!args.unityPlayer.empty()) {
    unityBytes = map_pe_file(args.unityPlayer);
    unity.emplace(unityBytes);
    if (unityBytes.empty() || !unity->valid()) {
      std::fprintf(stderr, "gamecheck: %s is not a readable PE32+ image\n", utf8(args.unityPlayer).c_str());
      return 2;
    }
  }

  const Resolution resolution = resolve(pm::map::kAllEntries, model, &image, {}, unity ? &*unity : nullptr);
  size_t counts[4] = {}, startupOnly = 0, inScene = 0;
  bool failed = false;  // a required entry that could be checked here did not resolve
  for (const Result& r : resolution.results) {
    if (checked_at_startup(r)) ++startupOnly;
    else if (checked_in_scene(r)) ++inScene;
    else ++counts[static_cast<int>(r.status)];
    failed = failed || (!checked_elsewhere(r) && r.status != Status::Resolved && !(r.desc->flags & kOptional));
    std::printf("%s\n", line_for(r).c_str());
  }
  std::printf("%zu entries: %zu resolved, %zu missing, %zu ambiguous, %zu blocked, %zu checked at startup only, "
              "%zu checked in the scene files\n",
              resolution.results.size(), counts[static_cast<int>(Status::Resolved)],
              counts[static_cast<int>(Status::Missing)], counts[static_cast<int>(Status::Ambiguous)],
              counts[static_cast<int>(Status::Blocked)], startupOnly, inScene);
  // Custom content numbers from one past the game's own ghost types and evidence, and names the game's
  // ghosts by their values' names (sdk/runtime/content_ids.h): report what this build holds.
  pm::content::GameEnums enums;
  if (const std::string why = pm::content::read_game_enums(model, resolution, enums); !why.empty()) {
    std::printf("CONTENT not read: %s\n", why.c_str());
  } else {
    std::printf("CONTENT GhostType %lld..%d, EvidenceType %lld..%d\n", static_cast<long long>(enums.lowestGhostType),
                enums.firstGhostType - 1, static_cast<long long>(enums.lowestEvidence), enums.firstEvidence - 1);
    for (const std::string& name : enums.unusable) std::printf("CONTENT no id for the value %s\n", name.c_str());
  }
  // The host's steering needs entries no mod names (host/steering.h): say whether this build gives them.
  std::string steeringLacks;
  for (const EntryDesc* d : pm::map::kSteeringEntries) {
    const Result* r = resolution.find(d->key);
    if (!r || !pm::map::steering_entry_usable(*d, r->status == Status::Resolved, r->rva != 0))
      steeringLacks += (steeringLacks.empty() ? "" : ", ") + std::string(d->key);
  }
  if (steeringLacks.empty()) std::printf("HOST steering available\n");
  else std::printf("HOST steering unavailable: %s\n", steeringLacks.c_str());
  if (!args.gameData.empty()) {
    const pm::engine::Versions versions = pm::engine::read(args.gameData);
    if (pm::engine::known(versions)) {
      std::printf("ENGINE %s, metadata %d\n", versions.engine.c_str(), versions.metadata);
    } else {
      std::printf("ENGINE refused: %s\n", pm::engine::refusal(versions).c_str());
      failed = true;
    }
  }
  const int switchedOff = args.mods.empty() ? 0 : check_mods(args.mods, resolution);
  if (switchedOff < 0) return 2;
  const int expectDifferent = args.expect.empty() ? 0 : check_expectations(args.expect, resolution);
  if (expectDifferent < 0) return 2;

  if (!args.sceneEntries.empty() && !write_scene_entries(args.sceneEntries, resolution)) return 2;
  if (!args.addresses.empty()) {
    std::FILE* out = nullptr;
    if (_wfopen_s(&out, args.addresses.c_str(), L"wb") != 0 || !out) {
      std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(args.addresses).c_str());
      return 2;
    }
    for (const Result& r : resolution.results) {
      if (r.status != Status::Resolved) continue;
      // A code entry's length is that of the function it lies in.
      const bool code = r.desc->kind == Kind::Method || r.desc->kind == Kind::Code || r.desc->kind == Kind::Export;
      const std::string length = code ? std::format("0x{:X}", image.function(r.rva).size()) : "";
      std::fprintf(out, "%s\t%s\t%s\t%s\n", r.desc->key, kind_word(r.desc->kind), value_of(r).c_str(), length.c_str());
    }
    if (std::fclose(out) != 0) {
      std::filesystem::remove(args.addresses, removeError);
      std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(args.addresses).c_str());
      return 2;
    }
  }
  return !failed && switchedOff == 0 && expectDifferent == 0 ? 0 : 1;
}

}  // namespace

// Anything thrown (a filesystem or allocation failure the code above does not foresee) is unreadable
// input: exit 2 with the reason, never an unhandled exception.
int wmain(int argc, wchar_t** argv) {
  try {
    return run(argc, argv);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "gamecheck: stopped by an exception: %s\n", e.what());
  } catch (...) {
    std::fprintf(stderr, "gamecheck: stopped by an exception\n");
  }
  return 2;
}
