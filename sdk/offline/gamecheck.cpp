// The offline check: resolves every map entry against a Cpp2IL dump and a copy of GameAssembly.dll,
// without running the game. tools/gamecheck.ps1 prepares both and runs it.
//
// usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--addresses <tsv>]
// It prints one line per entry and a summary. With --addresses it writes key, kind, value and length
// for each resolved entry. Exit 0 when every required entry resolved, 1 when one did not, 2 on bad
// arguments or unreadable input.
#include <cstdio>
#include <filesystem>
#include <format>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include "all.h"
#include "dump_model.h"
#include "image.h"
#include "resolver.h"

namespace {

using namespace pg::sdk;

constexpr const char* kUsage = "usage: gamecheck --dump <DiffableCs root> --image <GameAssembly.dll> [--addresses <tsv>]\n";

struct Args {
  std::filesystem::path dump, image, addresses;
};

bool parse(int argc, wchar_t** argv, Args& out) {
  for (int i = 1; i < argc; ++i) {
    const std::wstring_view flag = argv[i];
    if (i + 1 >= argc) return false;
    std::filesystem::path* target = flag == L"--dump"        ? &out.dump
                                    : flag == L"--image"     ? &out.image
                                    : flag == L"--addresses" ? &out.addresses
                                                             : nullptr;
    if (!target || !target->empty()) return false;
    *target = argv[++i];
  }
  return !out.dump.empty() && !out.image.empty();
}

const char* status_word(Status s) {
  switch (s) {
    case Status::Resolved: return "OK";
    case Status::Missing: return "MISSING";
    case Status::Ambiguous: return "AMBIGUOUS";
    case Status::Blocked: return "BLOCKED";
  }
  return "?";
}

const char* kind_word(Kind k) {
  switch (k) {
    case Kind::Type: return "type";
    case Kind::Method: return "method";
    case Kind::Field: return "field";
    case Kind::EnumValue: return "enum";
    case Kind::Code: return "code";
  }
  return "?";
}

// A resolved entry's value: a type's name in this build, a method's or code entry's RVA, a field's
// offset, an enum value's number.
std::string value_of(const Result& r) {
  switch (r.desc->kind) {
    case Kind::Type: return r.currentName;
    case Kind::Method:
    case Kind::Code: return std::format("0x{:X}", r.rva);
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

std::string line_for(const Result& r) {
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

  // Only the assemblies the map names: more would only add look-alike types.
  std::set<std::string> named;
  for (const EntryDesc* e : pg::map::kAllEntries)
    if (e->assembly) named.insert(e->assembly);
  const std::vector<std::string> assemblies(named.begin(), named.end());
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
  size_t counts[4] = {};
  for (const Result& r : resolution.results) {
    ++counts[static_cast<int>(r.status)];
    std::printf("%s\n", line_for(r).c_str());
  }
  std::printf("%zu entries: %zu resolved, %zu missing, %zu ambiguous, %zu blocked\n", resolution.results.size(),
              counts[static_cast<int>(Status::Resolved)], counts[static_cast<int>(Status::Missing)],
              counts[static_cast<int>(Status::Ambiguous)], counts[static_cast<int>(Status::Blocked)]);

  if (!args.addresses.empty()) {
    std::FILE* out = nullptr;
    if (_wfopen_s(&out, args.addresses.c_str(), L"wb") != 0 || !out) {
      std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(args.addresses).c_str());
      return 2;
    }
    for (const Result& r : resolution.results) {
      if (r.status != Status::Resolved) continue;
      // A code entry's length is that of the function it lies in.
      const bool code = r.desc->kind == Kind::Method || r.desc->kind == Kind::Code;
      const std::string length = code ? std::format("0x{:X}", image.function(r.rva).size()) : "";
      std::fprintf(out, "%s\t%s\t%s\t%s\n", r.desc->key, kind_word(r.desc->kind), value_of(r).c_str(), length.c_str());
    }
    if (std::fclose(out) != 0) {
      std::filesystem::remove(args.addresses, removeError);
      std::fprintf(stderr, "gamecheck: cannot write %s\n", utf8(args.addresses).c_str());
      return 2;
    }
  }
  return resolution.required_resolved() ? 0 : 1;
}
