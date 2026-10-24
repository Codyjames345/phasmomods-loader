#pragma once

#include <cstdint>
#include <span>

namespace pg::sdk {

// What a map entry names in the game. A static field is a Field flagged kStatic. An Icall is a Unity
// engine function the runtime registers under the icall name in `name`
// ("UnityEngine.Animator::SetBoolID"); only the running game can resolve one. An Export is a function
// GameAssembly.dll exports under `name` ("il2cpp_shutdown"); only the host may use one.
//
// Scene, Layer and AnimatorParam are facts of the game's scene and asset files, not of its code. A
// Scene entry is a GameObject found by its path from an object a mod already holds; a Layer entry is a
// layer by its name; an AnimatorParam entry is a parameter of an Animator by its name. None of them is
// in the IL2CPP metadata: the offline check verifies them in the scene files (tools/assetcheck.py), and
// the running game checks them where they are used (sdk/api/game_api.h says when).
//
// A NativeSite is a place in UnityPlayer.dll's machine code, which has no metadata: the one place its
// `pattern` matches, searched inside the UnityPlayer export `within` names, or in the whole ".text"
// section. `assembly` is "UnityPlayer". The host patches one through the host contract's patch_site.
enum class Kind : uint8_t { Type, Method, Field, EnumValue, Code, Icall, Export, Scene, Layer, AnimatorParam, NativeSite };

// How an entry's identification was established. A note never claims more than this.
enum class Confidence : uint8_t { Inferred, Decompiled, Live };

enum EntryFlags : uint32_t {
  kStatic = 1,    // a static method or field
  kOptional = 2,  // the entry may stay unresolved without failing the build
  kTrack = 4,     // the entry is recorded in the checked-build record
};

// A code fact about a method's body, used to tell look-alike candidates apart.
struct Fact {
  enum class Is : uint8_t { Stores, StoresConst, Loads, Calls, CalledBy };
  Is is;
  const char* method;
  int64_t value = 0;
  bool negate = false;
};

// One entry of the game map. Every field has a default so map headers can use designated
// initialisers and name only what an entry needs.
struct EntryDesc {
  const char* key = nullptr;
  Kind kind = Kind::Type;
  Confidence confidence = Confidence::Inferred;
  uint32_t flags = 0;
  const char* owner = nullptr;     // key of the declaring Type entry
  const char* name = nullptr;      // the real name, or null when obfuscated; an icall's or export's name
  const char* hint = nullptr;      // the obfuscated name in the last mapped build
  const char* assembly = nullptr;  // image name without ".dll"
  const char* ns = nullptr;        // null: the global namespace for a top-level type, any for a nested one
  // A field's type or a method's return type. "@Key" means the very type that entry Key resolved to,
  // not just its name: where the model says which type a member uses, it must be that one, and where
  // it does not, a name several types share makes the entry Ambiguous. The same holds in params.
  const char* type = nullptr;
  // A method's parameter types. Empty means the method is matched by name alone, at any arity, so
  // overloads make it Ambiguous. An icall's are never matched (the runtime has no metadata for one);
  // like a method's, they fix the number of arguments its wrapper takes (sdk/api/game_api.h).
  std::span<const char* const> params{};
  std::span<const char* const> fieldTypes{};  // a Type found by shape: field types it must have
  std::span<const char* const> values{};      // an enum Type found by shape: value names it must have
  std::span<const Fact> facts{};
  const char* pattern = nullptr;  // hex bytes, "??" for a wildcard
  // A Code entry's: the key of the Method the pattern is searched in. A NativeSite's: the name of the
  // UnityPlayer export whose function bounds the search, or ".text" for the whole code section.
  const char* within = nullptr;
  // Where the entry lies from the pattern's first byte. A NativeSite's lies inside the pattern.
  int32_t patternOffset = 0;
  // Where the offline check finds a Scene or AnimatorParam entry: a scene's name as the build lists it
  // ("Main_Menu"), or "Resources/<path>" for a prefab the game loads with Resources.Load
  // ("Resources/PCPlayer").
  const char* scene = nullptr;
  // A Scene entry's path from the object a lookup starts from, as Transform.Find takes it ("a/b").
  const char* path = nullptr;
  // For a Scene entry, the component that marks the object a lookup starts from: a script's class
  // ("JournalController") or an engine component's ("Canvas"). For an AnimatorParam entry, the name
  // of the AnimatorController whose parameter it is. A Layer entry names its layer in `name`, an
  // AnimatorParam entry its parameter.
  const char* component = nullptr;
};

}  // namespace pg::sdk
