// The offline check: resolves every map entry against a Cpp2IL dump and a copy of GameAssembly.dll,
// without running the game. tools/gamecheck.ps1 prepares both and runs it.
//
// usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--addresses <tsv>] [--mods <dir>]
// It prints one line per entry and a summary. Exports resolve from the image's export table. An icall
// cannot be checked without the running game: it is listed as checked at startup only, and never
// counts as a failure. With --addresses it writes key, kind, value and length for each resolved
// entry. With --mods it loads every mod DLL in <dir> and its subfolders (a DLL that exports
// pgmod_info), and after the summary prints "would not load <mod id> (contract <version>)" for each
// mod built against a host contract this build's host does not run (pg_api_compatible), and, for
// the others, asks for the game entries each requires (pgmod_requires) and prints
// "would switch off <mod id>: <keys>" for each one the host would not start on this build.
// Exit 0 when every required entry it can check resolved and no mod would be switched off, 1 when
// one did not or one would, 2 on bad arguments or unreadable input.
#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <format>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "all.h"
#include "dump_model.h"
#include "image.h"
#include "mod_api.h"
#include "resolver.h"

namespace {

using namespace pg::sdk;

constexpr const char* kUsage =
    "usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--addresses <tsv>] [--mods <dir>]\n";

struct Args {
  std::filesystem::path dump, image, addresses, mods;
};

bool parse(int argc, wchar_t** argv, Args& out) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view flag = argv[i];
    if (i + 1 >= argc) return false;
    std::filesystem::path* target = flag == L"--dump"        ? &out.dump
                                    : flag == L"--image"     ? &out.image
                                    : flag == L"--addresses" ? &out.addresses
                                    : flag == L"--mods"      ? &out.mods
                                                             : nullptr;
    if (!target || !target->empty()) return false;
    *target = argv[++i];
  }
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
  }
  return "?";
}

// A resolved entry's value: a type's name in this build, a method's, code entry's or export's RVA, a
// field's offset, an enum value's number, an icall's name.
std::string value_of(const Result& r) {
  switch (r.desc->kind) {
    case Kind::Type:
    case Kind::Icall: return r.currentName;
    case Kind::Method:
    case Kind::Code:
    case Kind::Export: return std::format("0x{:X}", r.rva);
    case Kind::Field: return std::format("+0x{:X}", r.offset);
    case Kind::EnumValue: return std::to_string(r.value);
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

// A well-formed icall entry, which only the running game can resolve. A malformed one is reported as
// any other entry that did not resolve.
bool startup_only(const Result& r) {
  return r.desc->kind == Kind::Icall && r.status == Status::Missing && r.detail == kCheckedAtStartup;
}

std::string line_for(const Result& r) {
  if (startup_only(r)) return std::format("STARTUP {} {} ({})", r.desc->key, r.desc->name, kCheckedAtStartup);
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

// The DLLs in `dir` and in its subfolders (a release keeps each mod in its own), sorted. False when
// a folder cannot be read.
bool dlls_in(const std::filesystem::path& dir, std::vector<std::filesystem::path>& out) {
  std::error_code error;
  std::vector<std::filesystem::path> folders{dir};
  for (const auto& e : std::filesystem::directory_iterator(dir, error))
    if (e.is_directory(error)) folders.push_back(e.path());
  if (error) return false;
  for (const std::filesystem::path& folder : folders) {
    for (const auto& e : std::filesystem::directory_iterator(folder, error))
      if (e.is_regular_file(error) && _wcsicmp(e.path().extension().c_str(), L".dll") == 0) out.push_back(e.path());
    if (error) return false;
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
    if (!exports(dll, "pgmod_info")) continue;  // not a mod
    HMODULE module = LoadLibraryExW(dll.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    const auto info = module ? reinterpret_cast<PgModInfoFn>(GetProcAddress(module, "pgmod_info")) : nullptr;
    const PgModInfo* mod = info ? info() : nullptr;
    if (!mod || !mod->id) {
      std::fprintf(stderr, "gamecheck: cannot load the mod %s\n", utf8(dll).c_str());
      unreadable = true;
      continue;
    }
    if (!pg_api_compatible(PGMOD_API_VERSION, mod->apiVersion)) {
      std::printf("would not load %s (contract %s)\n", mod->id, contract(mod->apiVersion).c_str());
      ++off;
      continue;
    }
    // A mod without pgmod_requires requires no game entry. One that requires an export is never
    // started: the host gives exports to its own code only.
    std::string lacking;
    if (const auto listRequired = reinterpret_cast<PgModRequiresFn>(GetProcAddress(module, "pgmod_requires"))) {
      const char* const* keys = nullptr;
      const size_t n = listRequired(&keys);
      for (size_t i = 0; keys && i < n; ++i) {
        const Result* r = keys[i] ? resolution.find(keys[i]) : nullptr;
        if (keys[i] && (!r || r->desc->kind == Kind::Export || (r->status != Status::Resolved && !startup_only(*r))))
          lacking += (lacking.empty() ? "" : ", ") + std::string(keys[i]);
      }
    }
    if (!lacking.empty()) {
      std::printf("would switch off %s: %s\n", mod->id, lacking.c_str());
      ++off;
    }
  }
  return unreadable ? -1 : off;
}

}  // namespace

int wmain(int argc, wchar_t** argv) {
  Args args;
  if (!parse(argc, argv, args)) {
    std::fputs(kUsage, stderr);
    return 2;
  }
  // The previous build's addresses must not outlive a failed check: the RE scripts would read them.
  std::error_code removeError;
  if (!args.addresses.empty() && !std::filesystem::remove(args.addresses, removeError) && removeError) {
    std::fprintf(stderr, "gamecheck: cannot remove %s\n", utf8(args.addresses).c_str());
    return 2;
  }

  const std::vector<std::string> assemblies = assemblies_of(pg::map::kAllEntries);
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

  const Resolution resolution = resolve(pg::map::kAllEntries, model, &image);
  size_t counts[4] = {}, startupOnly = 0;
  bool failed = false;  // a required entry that could be checked here did not resolve
  for (const Result& r : resolution.results) {
    if (startup_only(r)) ++startupOnly;
    else ++counts[static_cast<int>(r.status)];
    failed = failed || (!startup_only(r) && r.status != Status::Resolved && !(r.desc->flags & kOptional));
    std::printf("%s\n", line_for(r).c_str());
  }
  std::printf("%zu entries: %zu resolved, %zu missing, %zu ambiguous, %zu blocked, %zu checked at startup only\n",
              resolution.results.size(), counts[static_cast<int>(Status::Resolved)],
              counts[static_cast<int>(Status::Missing)], counts[static_cast<int>(Status::Ambiguous)],
              counts[static_cast<int>(Status::Blocked)], startupOnly);
  const int switchedOff = args.mods.empty() ? 0 : check_mods(args.mods, resolution);
  if (switchedOff < 0) return 2;

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
  return !failed && switchedOff == 0 ? 0 : 1;
}
