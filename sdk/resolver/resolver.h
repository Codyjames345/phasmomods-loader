#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "entry.h"
#include "model.h"

namespace pm::sdk {

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
  // A field within an embedded struct (a `within` entry), whose member is -1: the struct type and the
  // index of the field in it.
  int innerType = -1;
  int inner = -1;
  // A method's canonical body, a code entry's address, or an export's function, in GameAssembly; a
  // native site's address in UnityPlayer.
  uint64_t rva = 0;
  // A method's same behaviour at other addresses: bodies that read the same once placed (twins), and
  // the names of other methods folded onto its own body (aliases).
  std::vector<uint64_t> twins;
  std::vector<std::string> aliases;
  int32_t offset = 0;  // a field's offset
  int64_t value = 0;   // an enum value's number
  uint64_t address = 0;  // an icall's or an inflated method's code in the running game (not an rva)
  std::string currentName;  // the name in this build
  // An Inflated entry: its type arguments, as indices into GameModel::types (set once the definition and
  // the arguments were found, also while it waits for the running game), and what the runtime made of
  // it there: the inflated MethodInfo* and the class it belongs to (the inflated class, or the owner).
  std::vector<int> typeArgs;
  const void* inflatedMethod = nullptr;
  void* inflatedClass = nullptr;
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

// Of those, the ones only kOptional Type entries name, each once and sorted. A build may lack such an assembly (a
// Unity module the game stops using is stripped from it): the model then leaves it out and its entries stay
// unresolved, while a required assembly the build lacks fails the whole model. So one module a mod may do without
// never takes the map, and every mod with it, away.
std::vector<std::string> optional_assemblies_of(std::span<const EntryDesc* const> entries);

// assemblies_of(entries), less each optional one (optional_assemblies_of) for which `present` says false: the
// assemblies to read a model from. The offline check asks the dump whether it has a folder for one.
std::vector<std::string> present_assemblies(std::span<const EntryDesc* const> entries,
                                            const std::function<bool(const std::string&)>& present);

class CodeImage;

// The running runtime's il2cpp_resolve_icall: the code registered under an icall name, or null.
using IcallLookup = std::function<const void*(const char* name)>;

// The detail of a well-formed icall entry resolved without an IcallLookup, as offline, and of every
// well-formed Inflated entry whose definition and type arguments were found: only the running runtime
// makes a generic instance and says whether this build has code for it (host/game_map.h,
// resolve_inflated). The dump gives a generic definition's methods no code of their own, and the
// code-gen modules' generic method tables are not read offline, so an instance is never claimed
// resolved there.
inline constexpr std::string_view kCheckedAtStartup = "checked at startup only";

// The detail of a well-formed Scene, Layer, AnimatorParam or ShaderProperty entry: the resolver never
// resolves one. The offline check verifies them in the game's scene and asset files
// (tools/assetcheck.py), and the host when it activates (a layer, a shader property's id) or where a
// mod uses them (a path, a parameter, a property on a material).
inline constexpr std::string_view kCheckedInScene = "checked in the scene files and in the running game";

// A well-formed icall or Inflated entry left Missing for the running game, which alone resolves it (at
// startup, on the main thread). A malformed one has another detail and is reported as any other miss.
inline bool checked_at_startup(const Result& r) {
  return (r.desc->kind == Kind::Icall || r.desc->kind == Kind::Inflated) && r.status == Status::Missing &&
         r.detail == kCheckedAtStartup;
}

// A well-formed Scene, Layer, AnimatorParam or ShaderProperty entry left Missing for the scene files and
// the running game. A malformed one has another detail and is reported as any other miss.
inline bool checked_in_scene(const Result& r) {
  const Kind k = r.desc->kind;
  return (k == Kind::Scene || k == Kind::Layer || k == Kind::AnimatorParam || k == Kind::ShaderProperty) &&
         r.status == Status::Missing && r.detail == kCheckedInScene;
}

// Resolves every entry against the model, each after the entries it refers to. Without an image,
// entries that need machine code or the export table are Missing with the detail "no code image".
// Icall entries resolve only through `icalls`, which only the running game has: without it they are
// Missing with the detail "checked at startup only". Scene, Layer, AnimatorParam and ShaderProperty
// entries are Missing with the detail kCheckedInScene when well formed. NativeSite entries resolve over
// `unityPlayer`, UnityPlayer.dll laid out by RVA, or over `image` when their assembly is GameAssembly:
// without it they are Missing with the detail "no UnityPlayer image" ("no GameAssembly image").
Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image,
                   const IcallLookup& icalls = {}, const CodeImage* unityPlayer = nullptr);

}  // namespace pm::sdk
