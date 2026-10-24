#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "entry.h"
#include "model.h"

namespace pg::sdk {

// Resolved: exactly one candidate survived every check. Missing: none did. Ambiguous: more than one
// did, and the resolver never picks among them. Blocked: an entry this one depends on did not resolve.
enum class Status { Resolved, Missing, Ambiguous, Blocked };

struct Result {
  const EntryDesc* desc = nullptr;
  Status status = Status::Missing;
  std::string detail;                   // why it is not Resolved, or what changed
  std::vector<std::string> candidates;  // the candidates of an Ambiguous entry, readable
  int type = -1;    // the type, or a member's owner type, as an index into GameModel::types
  int member = -1;  // index into the owner's fields (Field, EnumValue) or methods (Method)
  // A method's canonical body, a code entry's address, or an export's function, in GameAssembly; a
  // native site's address in UnityPlayer.
  uint64_t rva = 0;
  // A method's same behaviour at other addresses: bodies that read the same once placed (twins), and
  // the names of other methods folded onto its own body (aliases).
  std::vector<uint64_t> twins;
  std::vector<std::string> aliases;
  int32_t offset = 0;  // a field's offset
  int64_t value = 0;   // an enum value's number
  uint64_t address = 0;  // an icall's code in the running game (outside GameAssembly, so not an rva)
  std::string currentName;  // the name in this build
};

struct Resolution {
  std::vector<Result> results;  // one per entry, in the order the entries were given
  const Result* find(std::string_view key) const;
  // Every entry not marked kOptional resolved.
  bool required_resolved() const;
};

// "OK", "MISSING", "AMBIGUOUS" or "BLOCKED", as the reports write a status.
const char* status_word(Status status);

// The IL2CPP assemblies the entries name, each once and sorted: the only ones a model needs, since more
// would only add look-alike types. A native site's "UnityPlayer" is none of them.
std::vector<std::string> assemblies_of(std::span<const EntryDesc* const> entries);

class CodeImage;

// The running runtime's il2cpp_resolve_icall: the code registered under an icall name, or null.
using IcallLookup = std::function<const void*(const char* name)>;

// The detail of a well-formed icall entry resolved without an IcallLookup, as offline.
inline constexpr std::string_view kCheckedAtStartup = "checked at startup only";

// The detail of a well-formed Scene, Layer or AnimatorParam entry: the resolver never resolves one.
// The offline check verifies them in the game's scene files (tools/assetcheck.py), and the host when
// it activates (a layer) or where a mod uses them (a path, a parameter).
inline constexpr std::string_view kCheckedInScene = "checked in the scene files and in the running game";

// A well-formed icall entry left Missing for the running game, which alone resolves it (at startup,
// on the main thread). A malformed one has another detail and is reported as any other miss.
inline bool checked_at_startup(const Result& r) {
  return r.desc->kind == Kind::Icall && r.status == Status::Missing && r.detail == kCheckedAtStartup;
}

// A well-formed Scene, Layer or AnimatorParam entry left Missing for the scene files and the running
// game. A malformed one has another detail and is reported as any other miss.
inline bool checked_in_scene(const Result& r) {
  const Kind k = r.desc->kind;
  return (k == Kind::Scene || k == Kind::Layer || k == Kind::AnimatorParam) && r.status == Status::Missing &&
         r.detail == kCheckedInScene;
}

// Resolves every entry against the model, each after the entries it refers to. Without an image,
// entries that need machine code or the export table are Missing with the detail "no code image".
// Icall entries resolve only through `icalls`, which only the running game has: without it they are
// Missing with the detail "checked at startup only". Scene, Layer and AnimatorParam entries are
// Missing with the detail kCheckedInScene when well formed. NativeSite entries resolve over
// `unityPlayer`, UnityPlayer.dll laid out by RVA: without it they are Missing with the detail
// "no UnityPlayer image".
Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image,
                   const IcallLookup& icalls = {}, const CodeImage* unityPlayer = nullptr);

}  // namespace pg::sdk
