#pragma once

#include <cstdint>
#include <span>

namespace pm::sdk {

// What a map entry names in the game. A static field is a Field flagged kStatic. An Icall is a Unity
// engine function the runtime registers under the icall name in `name`
// ("UnityEngine.Animator::SetBoolID"); only the running game can resolve one. An Export is a function
// GameAssembly.dll exports under `name` ("il2cpp_init"); only the host may use one.
//
// Scene, Layer, AnimatorParam and ShaderProperty are facts of the game's scene and asset files, not of
// its code. A Scene entry is a GameObject found by its path from an object a mod already holds; a Layer
// entry is a layer by its name; an AnimatorParam entry is a parameter of an Animator by its name; a
// ShaderProperty entry is a property of a shader by its name. None of them is in the IL2CPP metadata:
// the offline check verifies them in the scene and asset files (tools/assetcheck.py), and the running
// game checks them where they are used (sdk/api/game_api.h says when).
//
// A NativeSite is a place in machine code that has no metadata: the one place its `pattern` matches,
// searched inside the export `within` names, or in the whole ".text" section. `assembly` says which
// module: "UnityPlayer", or "GameAssembly" for the IL2CPP runtime's own functions. Only the host acts on
// one (host/steering.h): a mod asks the game API for the effect.
//
// An Inflated entry is a method of a generic instance: a method of a generic class made for type arguments
// (InputControl<Vector2>.ReadValue), or a generic method made for them (Sensor.GetDetectedComponents<T> for
// one T, flagged kGenericMethod). Its owner is the generic definition's Type entry, its name, type and
// params are the definition's method as declared (type parameters by their names, "TValue", "List<T>"),
// and typeArgs the arguments. A definition has no code of its own and the instance's code is the
// runtime's, so only the running game resolves one (at startup, on the main thread). A mod calls and
// hooks it as a Method.
enum class Kind : uint8_t {
  Type, Method, Field, EnumValue, Code, Icall, Export, Scene, Layer, AnimatorParam, NativeSite, ShaderProperty,
  Inflated
};

// How an entry's identification was established. A note never claims more than this.
enum class Confidence : uint8_t { Inferred, Decompiled, Live };

enum EntryFlags : uint32_t {
  kStatic = 1,    // a static method or field
  kOptional = 2,  // the entry may stay unresolved without failing the build
  kTrack = 4,     // the entry is recorded in the checked-build record
  // A type found by its fieldTypes declares exactly those instance fields and no other: a class that
  // declares one more (a look-alike with a superset of the fields) is no candidate.
  kExactFields = 8,
  // A type found by shape that the compiler generated, such as an async method's state machine, whose name
  // the obfuscator changes but whose fields keep their "<>" names: the shape search then looks at those types
  // only, where it otherwise leaves them out.
  kCompilerGenerated = 16,
  // With kExactFields: the instance fields also lie in memory in the order fieldTypes lists them (by offset), so the
  // type's size is fixed too. A reordered look-alike is no candidate.
  kFieldOrder = 32,
  // An Inflated entry's type arguments are the method's own (a generic method), not its class's.
  kGenericMethod = 64,
  // A Type entry that is no generic definition: the one of a name several generic arities share, which the models
  // name alike (UnityEvent beside UnityEvent<T0> to UnityEvent<T0, T1, T2, T3>).
  kNotGeneric = 128,
};

// A member's accessibility as C# declares it, numbered as ECMA-335's FieldAccess mask (II.23.1.5);
// Any means not known, or for an entry not checked.
enum class Visibility : uint8_t {
  Any = 0,
  Private = 1,
  PrivateProtected = 2,  // ECMA-335 FamANDAssem
  Internal = 3,          // Assembly
  Protected = 4,         // Family
  ProtectedInternal = 5, // FamORAssem
  Public = 6,
};

// A code fact about a method's body, used to tell look-alike candidates apart. `Called` names no
// method: the candidate is called (or jumped to) directly from another method of its own type, which
// the obfuscator's uncalled decoys never are. `Compares` names an EnumValue entry, not a method: the
// candidate method's own code compares something with that value's number (a cmp with it as an
// immediate), as a handler that acts for one ghost type tests the type it acts for. `IsNot` names a
// Field entry of the same owner, not a method: the candidate is not the field that entry resolved to, so
// a cross-type fact that matches several fields can be narrowed by the ones already told apart. While
// that entry is unresolved this one is Blocked, as for any entry a fact names. `MovesConst` names no method: the
// candidate method's own code moves the number `value` into a register or a place, for look-alikes that differ only in
// their constants.
struct Fact {
  enum class Is : uint8_t { Stores, StoresConst, Loads, Calls, CalledBy, Called, Compares, IsNot, MovesConst };
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
  const char* hint = nullptr;      // a readable name, for the report only; never an obfuscated one, which updates shuffle
  const char* assembly = nullptr;  // image name without ".dll"
  const char* ns = nullptr;        // null: the global namespace for a top-level type, any for a nested one
  // A field's type or a method's return type. "@Key" means the very type that entry Key resolved to,
  // not just its name: where the model says which type a member uses, it must be that one, and where
  // it does not, a name several types share makes the entry Ambiguous. The same holds in params. On a Type
  // found by shape it and `params` give a static method's shape: the type declares one (a static class has
  // no fields to go by).
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
  // UnityPlayer export whose function bounds the search, or ".text" for the whole code section. A
  // Field's: the key of an instance Field entry of the same owner that holds a struct; the entry is then
  // a field of that struct, resolved to its offset in the owner, and its facts read the owner's code.
  // A Type's: the key of another Type entry; the entry is then the one interface that type declares in
  // its own base list. A type that declares none, or a base that names no model type or several, leaves
  // it unresolved: the resolver never guesses which base is meant.
  const char* within = nullptr;
  // Where the entry lies from the pattern's first byte. A NativeSite's lies inside the pattern.
  int32_t patternOffset = 0;
  // Where the offline check finds a Scene or AnimatorParam entry: a scene's name as the build lists it
  // ("Main_Menu"), or "Resources/<path>" for a prefab the game loads with Resources.Load
  // ("Resources/PCPlayer"). A ShaderProperty entry's is a scene whose assets (its shared assets file and
  // the files that one refers to) hold the shader.
  const char* scene = nullptr;
  // A Scene entry's path from the object a lookup starts from, as Transform.Find takes it ("a/b").
  const char* path = nullptr;
  // For a Scene entry, the component that marks the object a lookup starts from: a script's class
  // ("JournalController") or an engine component's ("Canvas"). For an AnimatorParam entry, the name
  // of the AnimatorController whose parameter it is; for a ShaderProperty entry, the shader's name
  // ("Candle Flame"). A Layer entry names its layer in `name`, an AnimatorParam entry its parameter, a
  // ShaderProperty entry its property ("_TintColour").
  const char* component = nullptr;
  // For a Field `within` a struct only: the key of another `within` Field entry. The entry is then the
  // very struct field that entry resolved to, in a struct of the same type held elsewhere (GhostInfo's
  // traits and GhostController's are one struct type, so a fact found for one decides the other).
  // Another struct type, or a target that is not a `within` field, leaves the entry Missing.
  const char* sameFieldAs = nullptr;
  // A Field found by shape: the accessibility it must have, as C# declares it (ECMA-335 FieldAccess).
  // Any (the default) does not check it. A field whose accessibility the model does not know never
  // matches one that is asked for.
  Visibility access = Visibility::Any;
  // An Inflated entry's type arguments, one type each: "@Key" for the type a Type entry resolved to, or a
  // name exactly one model type has.
  std::span<const char* const> typeArgs{};
};

}  // namespace pm::sdk
