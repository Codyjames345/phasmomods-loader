#pragma once

// The game API: the typed surface a mod calls to reach the game, by the readable names of the game
// map (sdk/map). A mod never holds a game address or a field offset; the host resolves the map in
// the running game and hands each entry over through the host contract (core/mod_api.h).
//
// Each map header ends with ready-made wrappers in namespace phasmo, for example
// `phasmo::GhostAI::UnAppear(ghost)`, `phasmo::GhostAI::state.get(ghost)` and
// `phasmo::GhostState::hunting()`. They are built from the templates below.
//
// What a mod requires is what its code calls. Every accessor's members are templates, so a wrapper's
// entry is registered only when code in the mod calls it, and the mod's pgmod_requires export lists
// exactly those entries (less the ones marked kOptional). The host does not start a mod while one of
// them did not resolve in the running game build.
//
// A wrapper's shape must be its entry's: where the map gives a method's or icall's params, a wrapper
// with another number of arguments does not compile, and where it gives a field's type as a C#
// value type by name (bool, int, float and the like), neither does a T of another size.
//
// A call of a game method is never virtual: it runs the body the map entry names, even when the
// object's class overrides it. Method::virtual_call runs the override, as C# calls a virtual method.
//
// When an entry did not resolve anyway (an optional one, or the mod was not bound), nothing jumps to
// null: a call, a read or a write logs "<key> is not available in this game build" once and returns
// the type's default value (0, false, null) or does nothing.
//
// A managed reference a wrapper gives (a void* read from a field or returned by a call, a type
// object, a new object or array) is valid for the current frame only. Do not keep it across frames:
// keep the object in a pg::api::Held (held.h) instead.
//
// Threading: everything here runs on Unity's main thread, where the host calls pgmod_start and
// pgmod_tick, except the registration a wrapper makes while the mod's DLL loads. Each "logged once"
// is kept with an atomic flag, so even two threads log a line once.
//
// Exports of GameAssembly.dll (Export below) are for the host's own code only: a mod never sees the
// handle, and the host contract's game_entry answers null for an export.
//
// Managed strings, arrays, lists, alive checks and GC holds are in runtime.h and held.h.

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "entry.h"
#include "hooks.h"
#include "mod_api.h"

namespace pg::api {

namespace detail {

// One game entry the mod's code uses, and what the host gave for it at bind. The two flags are
// atomic, so two threads that log at once still log each line once.
struct Slot {
  const sdk::EntryDesc* desc;
  const PgGameEntry* entry;        // null before bind and when the entry did not resolve
  std::atomic<bool> reported;      // its "not available" line was logged since the last bind
  std::atomic<bool> refused;       // a refusal (a virtual_call, a static field the host could not
                                   // reach) was logged since the last bind
};

// Adds a used entry to the mod's list. Runs while the mod's DLL loads; always true.
bool register_use(Slot* slot);

// The entry D's slot, a variable template: it is instantiated only where code names it.
template <const sdk::EntryDesc& D>
inline Slot slot{&D, nullptr, false, false};

// The entry D's registration, made while the mod's DLL loads. MSVC instantiates a class template's
// static data members along with the class, so `done` is not a member of Use, which a wrapper names
// without calling anything: only Use<D>::resolved names Registration<D>, so only a call registers D.
// It is not a variable template either: MSVC emits no initialiser for one whose argument has
// internal linkage (an entry declared in an unnamed namespace), which would never register.
template <const sdk::EntryDesc& D>
struct Registration {
  static inline const bool done = register_use(&slot<D>);
};

// The number of arguments a wrapper of the entry D takes, or -1 when the map does not say: the
// entry's params, after the object for an instance member. A method named without params is
// matched at any arity, so its count is unknown; one found by shape without params takes none.
constexpr int arity(const sdk::EntryDesc& d) {
  if (d.params.empty() && d.name) return -1;
  return static_cast<int>(d.params.size()) + ((d.flags & sdk::kStatic) ? 0 : 1);
}

// The size in bytes of a value of the C# type `type` names, or 0 when it is not one the API can size
// by its name alone (a struct, an enum, an "@Key" entry type, or none given).
constexpr size_t value_size(const char* type) {
  if (!type) return 0;
  const std::string_view t(type);
  if (t == "bool" || t == "byte" || t == "sbyte") return 1;
  if (t == "short" || t == "ushort" || t == "char") return 2;
  if (t == "int" || t == "uint" || t == "float") return 4;
  if (t == "long" || t == "ulong" || t == "double" || t == "string" || t == "object") return 8;
  return 0;
}

// The host contract given to bind, or null before it.
const PgHostApi* host();
// Logs "<key> is not available in this game build", once per bind.
void report_unavailable(Slot& slot);
// Logs that a virtual_call of the entry was refused (a null object or one not of its declaring
// type), once per bind.
void report_refused_call(Slot& slot);
// Logs that the host could not read or write the static field, once per bind.
void report_refused_static(Slot& slot);
// The host contract's static_get and static_set (version 2.1), or false before bind.
bool static_get(const char* key, void* out, size_t size);
bool static_set(const char* key, const void* in, size_t size);
// The host contract's runtime services (core/mod_api.h, version 2.1) for an entry's key, or null,
// false or 0 before bind.
bool is_instance(const void* obj, const char* typeKey);
void* type_object(const char* typeKey);
void* object_new(const char* typeKey);
void* array_new(const char* elementTypeKey, size_t length);
const void* virtual_method(const void* obj, const char* methodKey);
// The object a virtual call is made on: an instance method's first argument.
template <class Obj, class... Rest>
const void* object_of(Obj obj, Rest...) {
  static_assert(std::is_pointer_v<Obj>, "an instance method's first parameter is the object, a pointer");
  return obj;
}
// Hooks bodies 0 to n - 1 of the method entry `key` through the host contract's hook_body, where n
// is 1, or with `allTwins` its body_count: body i with entry(i), its `original` into originals[i].
// Stops at the first body the host refuses. False (logged) before bind, without a detour, when the
// method has more bodies than kMaxBodies (each with nothing hooked), or when a body was refused (the
// host logs why); `created` is how many bodies were hooked, from body 0 on.
bool hook_bodies(const char* key, bool hasDetour, void* (*entry)(uint32_t), void** originals, const char* name,
                 bool allTwins, uint32_t& created);
// The host contract's inside_entry, or 0 before bind.
int32_t inside(const char* key);

}  // namespace detail

/// The use of the map entry D by the mod's code, which every wrapper below names as its member `use`.
/// Calling entry() or resolved() adds D to what the mod requires (unless D is kOptional); naming
/// Use<D> or a wrapper's class does not, so the wrappers call them only from the members a mod calls.
template <const sdk::EntryDesc& D>
struct Use {
  /// The resolved entry, or null when it did not resolve (logged once per bind). Main thread.
  static const PgGameEntry* entry() {
    if (!resolved()) detail::report_unavailable(slot());
    return slot().entry;
  }
  /// Whether the entry resolved, without logging. Main thread.
  static bool resolved() {
    static_cast<void>(detail::Registration<D>::done);
    return slot().entry != nullptr;
  }
  /// What the host gave for D at bind, and whether its lines were logged.
  static detail::Slot& slot() { return detail::slot<D>; }
};

template <const sdk::EntryDesc& D, class Sig>
struct Method;

/// A game method, called as R(A...). For an instance method the first parameter is the object
/// (`this`). A struct parameter whose size is not 1, 2, 4 or 8 bytes is passed by address, as the x64
/// calling convention does, and the callee may modify it: write it as a pointer, and pass a copy when
/// the original must stay unchanged. Where the entry gives its params, A... is one argument per
/// param, after the object for an instance method, or the wrapper does not compile. A void* the
/// method returns is a managed reference: do not keep it across frames. Main thread.
template <const sdk::EntryDesc& D, class R, class... A>
struct Method<D, R(A...)> {
  static_assert(D.kind == sdk::Kind::Method, "Method wraps a map entry of kind Method");
  static_assert(detail::arity(D) < 0 || detail::arity(D) == static_cast<int>(sizeof...(A)),
                "the wrapper's arity is not its entry's: one argument per param, after the object for "
                "an instance method");
  /// The map entry this wraps (its key, kind and flags), as hook<M> and inside<M> read it.
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// Calls the method's code with the arguments and its MethodInfo, as the game does. The call is
  /// never virtual: it runs the declared body even when the object's class overrides it (see
  /// virtual_call). Unresolved: logs once and returns R{} without calling anything.
  template <int = 0>
  R operator()(A... a) const {
    const PgGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    return reinterpret_cast<R (*)(A..., const void*)>(e->code)(a..., e->method);
  }

  /// Calls the method as C# calls a virtual method: the method the class of the object (the first
  /// argument) runs, its override or else the declared body, with that method's MethodInfo. A null
  /// object, or one not of the method's declaring type, is refused: nothing is called, R{} is
  /// returned, and the refusal is logged once per bind. Before bind it calls the declared body, as
  /// operator() does. Unresolved: logs once and returns R{} without calling anything.
  template <int = 0>
  R virtual_call(A... a) const {
    static_assert(!(D.flags & sdk::kStatic), "a static method has no object to dispatch on");
    static_assert(sizeof...(A) > 0, "virtual_call needs the object as the first argument");
    const PgGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    void* code = e->code;
    const void* method = e->method;
    if (detail::host()) {
      const void* obj = detail::object_of(a...);
      method = obj ? detail::virtual_method(obj, D.key) : nullptr;
      // core/mod_api.h: a MethodInfo's first member is the address of its code.
      code = method ? *static_cast<void* const*>(method) : nullptr;
      if (!code) {
        detail::report_refused_call(use::slot());
        if constexpr (std::is_void_v<R>) return;
        else return R{};
      }
    }
    return reinterpret_cast<R (*)(A..., const void*)>(code)(a..., method);
  }

  /// Whether the method resolved in this game build. Never logs.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// An instance field of type T. T is a plain value (bool, int32_t, float, a struct copied whole) or
/// void* for a managed reference. Where the entry's type is a C# value type by name (bool, int,
/// float and the like), a T of another size does not compile. Reads and writes go to the resolved
/// offset in the object. Main thread.
template <const sdk::EntryDesc& D, class T>
struct Field {
  static_assert(D.kind == sdk::Kind::Field, "Field wraps a map entry of kind Field");
  static_assert(!(D.flags & sdk::kStatic), "a static field is StaticField<D, T>");
  static_assert(std::is_trivially_copyable_v<T>, "a field is read and written as plain bytes");
  static_assert(!std::is_pointer_v<T> || std::is_same_v<T, void*>, "a managed reference field is Field<D, void*>");
  static_assert(detail::value_size(D.type) == 0 || detail::value_size(D.type) == sizeof(T),
                "T's size is not the size of the entry's type");
  /// The map entry this wraps (its key, kind and flags).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The field's value in the managed object `obj`. Unresolved (logged once) or a null `obj`: T{}.
  /// A reference (void*) is valid for the current frame only: do not keep it across frames.
  template <int = 0>
  T get(const void* obj) const {
    const PgGameEntry* e = use::entry();
    if (!e || !obj) return T{};
    T value;
    std::memcpy(&value, static_cast<const char*>(obj) + e->offset, sizeof value);
    return value;
  }

  /// Stores `value` in the managed object `obj`. A managed reference (void*) goes through the GC
  /// write barrier (the host's set_reference); a plain value is written directly. Unresolved (logged
  /// once) or a null `obj`: nothing is written.
  template <int = 0>
  void set(void* obj, T value) const {
    const PgGameEntry* e = use::entry();
    if (!e || !obj) return;
    if constexpr (std::is_same_v<T, void*>) {
      if (const PgHostApi* h = detail::host()) h->set_reference(obj, e->offset, value);
    } else {
      std::memcpy(static_cast<char*>(obj) + e->offset, &value, sizeof value);
    }
  }
};

/// A static field of type T, one value for the whole class. T is a number, a bool, an enum, or void* for
/// a managed reference. A struct is not supported: its bytes are copied whole, which would bypass the
/// GC write barrier for any managed reference inside it, and the API cannot tell whether one does.
/// Reading or writing it first runs the class's static constructor when that has not run
/// yet, as the game's own first use would, so use it only from code that runs while the game plays
/// (pgmod_tick, a hook's detour), never from a DLL's initialisation; the host never reads a static
/// field while it resolves the map at startup. As for Field, a T whose size is not that of the
/// entry's type, where the map names a C# value type, does not compile. Main thread.
template <const sdk::EntryDesc& D, class T>
struct StaticField {
  static_assert(D.kind == sdk::Kind::Field && (D.flags & sdk::kStatic),
                "StaticField wraps a map entry of kind Field flagged kStatic");
  static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, void*>,
                "a static field is a number, a bool, an enum, or void* for a managed reference; a struct "
                "may hold references, which a whole copy would store without the GC write barrier");
  static_assert(detail::value_size(D.type) == 0 || detail::value_size(D.type) == sizeof(T),
                "T's size is not the size of the entry's type");
  /// The map entry this wraps (its key, kind and flags).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The field's value. Unresolved: logs once and returns T{}. When the host cannot read it (T's size
  /// is not the field's, or the class's static constructor threw): logs once per bind and returns T{}.
  /// A reference (void*) is valid for the current frame only: do not keep it across frames.
  template <int = 0>
  T get() const {
    if (!use::entry()) return T{};
    T value{};
    if (detail::static_get(D.key, &value, sizeof value)) return value;
    detail::report_refused_static(use::slot());
    return T{};
  }

  /// Stores `value` in the field; a managed reference goes through the GC write barrier. Unresolved:
  /// logs once and writes nothing. When the host cannot write it: logs once per bind and writes
  /// nothing.
  template <int = 0>
  void set(T value) const {
    if (!use::entry()) return;
    if (!detail::static_set(D.key, &value, sizeof value)) detail::report_refused_static(use::slot());
  }

  /// Whether the field resolved in this game build. Never logs.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

template <const sdk::EntryDesc& D, class Sig>
struct Icall;

/// A Unity engine function the runtime registered by its icall name (the map entry's name, such as
/// "UnityEngine.Animator::SetBoolID"), called as R(A...). It is native code, so unlike a game method
/// it takes no MethodInfo; an instance function's first parameter is the managed object. Where the
/// entry gives its params, A... is one argument per param, after the object unless the entry is
/// kStatic, or the wrapper does not compile; a struct parameter goes as for Method, by address, and
/// the callee may modify it. A void* it returns is a managed reference: do not keep it across
/// frames. Main thread.
template <const sdk::EntryDesc& D, class R, class... A>
struct Icall<D, R(A...)> {
  static_assert(D.kind == sdk::Kind::Icall, "Icall wraps a map entry of kind Icall");
  static_assert(detail::arity(D) < 0 || detail::arity(D) == static_cast<int>(sizeof...(A)),
                "the wrapper's arity is not its entry's: one argument per param, after the object unless "
                "the entry is kStatic");
  /// The map entry this wraps (its key, kind, flags and icall name).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// Calls the engine function with the arguments. Unresolved: logs once and returns R{} without
  /// calling anything.
  template <int = 0>
  R operator()(A... a) const {
    const PgGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    return reinterpret_cast<R (*)(A...)>(e->code)(a...);
  }

  /// Whether the icall resolved in this game build. Never logs.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

#ifdef PG_HOST
namespace host_only {
/// The host's own resolved entry for `key` (host/game_map.cpp), or null when it is unknown or
/// unresolved. Main thread, after the map resolved at startup.
const PgGameEntry* game_entry(const char* key);
}  // namespace host_only

/// A function GameAssembly.dll exports, such as il2cpp_shutdown. Host code only: it exists where
/// PG_HOST is defined (ModHost), a mod never sees it, and the host contract's game_entry answers null
/// for an export. The host hooks one through its own MinHook instance. Main thread, after the map resolved
/// at startup.
template <const sdk::EntryDesc& D>
struct Export {
  static_assert(D.kind == sdk::Kind::Export, "Export wraps a map entry of kind Export");
  /// The map entry this wraps (its key and export name). An export has no use: the host's own code
  /// requires nothing through pgmod_requires.
  static constexpr const sdk::EntryDesc& desc = D;

  /// The export's code in the loaded GameAssembly.dll, or null when it did not resolve (the host
  /// logged that at startup).
  void* address() const {
    const PgGameEntry* e = host_only::game_entry(D.key);
    return e ? e->code : nullptr;
  }

  /// Whether the export resolved in this game build. Never logs.
  bool available() const { return address() != nullptr; }
};
#endif

/// One value of a game enum, by its readable name. Main thread.
template <const sdk::EntryDesc& D>
struct EnumValue {
  static_assert(D.kind == sdk::Kind::EnumValue, "EnumValue wraps a map entry of kind EnumValue");
  /// The map entry this wraps (its key and value name).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The value's number in this game build. Unresolved: logs once and returns 0.
  template <int = 0>
  int32_t operator()() const {
    const PgGameEntry* e = use::entry();
    return e ? static_cast<int32_t>(e->value) : 0;
  }
};

/// A game type (class, struct or enum). Main thread.
template <const sdk::EntryDesc& D>
struct Type {
  static_assert(D.kind == sdk::Kind::Type, "Type wraps a map entry of kind Type");
  /// The map entry this wraps (its key, name and assembly).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The type's Il2CppClass*: runtime metadata, never a managed object. Unresolved: logs once and
  /// returns null.
  template <int = 0>
  void* klass() const {
    const PgGameEntry* e = use::entry();
    return e ? e->klass : nullptr;
  }

  /// Whether the managed object `obj` is of this type or of a class derived from it (following the
  /// class's parents). An interface type never matches: the check does not look at the interfaces
  /// a class implements. False for a null `obj`, and when the type did not resolve (logged once).
  template <int = 0>
  bool is(const void* obj) const {
    return use::entry() && obj && detail::is_instance(obj, D.key);
  }

  /// The type's System.Type object, as C#'s typeof gives it (for GetComponent, AddComponent and
  /// the like). A managed object: do not keep it across frames without a pg::api::Held. Unresolved:
  /// logs once and returns null.
  template <int = 0>
  void* type_object() const {
    return use::entry() ? detail::type_object(D.key) : nullptr;
  }

  /// A new managed object of this class with every field zeroed. No constructor runs. Nothing else
  /// refers to it yet, so do not keep it across frames without a pg::api::Held (held.h). Not for engine
  /// objects: a Component or MonoBehaviour comes from AddComponent or Instantiate, never from
  /// make(), which gives an object Unity knows nothing of. Unresolved: logs once and returns null.
  template <int = 0>
  void* make() const {
    return use::entry() ? detail::object_new(D.key) : nullptr;
  }

  /// A new managed array of `length` elements of this type, zeroed (read it with pg::api::Array or
  /// RefArray, runtime.h). Do not keep it across frames without a pg::api::Held. Unresolved: logs
  /// once and returns null.
  template <int = 0>
  void* make_array(size_t length) const {
    return use::entry() ? detail::array_new(D.key, length) : nullptr;
  }
};

/// Which bodies of a method a hook covers: every twin the obfuscator made (so the detour runs
/// whichever body the game calls), or only the canonical body.
enum class Twins { All, CanonicalOnly };

/// The most bodies (the canonical body and its twins) a typed hook covers: the game API makes one
/// entry function per body, and it makes this many. An all-twins hook on a method with more bodies
/// is refused (logged) and hooks nothing.
inline constexpr uint32_t kMaxBodies = 8;

template <class W>
struct OriginalOf;

/// What a hook's detour calls to go on with the game's call: the next detour in the chain of the
/// body the call came in through, or that body itself. It passes the game's MethodInfo on. Valid
/// only during the detour's call: do not keep it. Main thread, as the hooked call.
template <const sdk::EntryDesc& D, class R, class... A>
struct OriginalOf<Method<D, R(A...)>> {
  R (*code)(A..., const void*);
  const void* method;
  R operator()(A... a) const { return code(a..., method); }
};

/// The `original` a detour of a hook on M (a Method wrapper) receives.
template <auto& M>
using Original = OriginalOf<std::remove_cvref_t<decltype(M)>>;

namespace detail {

// One entry function per body slot, for the Method wrapper W and the handler H that runs for each
// call (H::run(original, args...)): body_entry<I> is what is hooked on body I, and `original[I]` is
// what continues that body's chain. Each body_entry<I> reads its own slot, so no two of them are the
// same code and the linker never folds them into one. Each holds a hooks::Guard while it runs.
template <class W, class H>
struct Bodies;

template <const sdk::EntryDesc& D, class R, class... A, class H>
struct Bodies<Method<D, R(A...)>, H> {
  using Code = R (*)(A..., const void*);
  static inline Code original[kMaxBodies] = {};

  template <uint32_t I>
  static R body_entry(A... a, const void* method) {
    hooks::Guard guard;
    return H::run(OriginalOf<Method<D, R(A...)>>{original[I], method}, a...);
  }

  template <uint32_t... I>
  static constexpr std::array<Code, kMaxBodies> make(std::integer_sequence<uint32_t, I...>) {
    return {&body_entry<I>...};
  }
  static constexpr std::array<Code, kMaxBodies> entries = make(std::make_integer_sequence<uint32_t, kMaxBodies>{});

  // The entry function hooked on body `i`, below kMaxBodies.
  static void* entry(uint32_t i) { return reinterpret_cast<void*>(entries[i]); }
};

// The handler of hook<M>: the mod's detour, given the body's original. One per wrapper, so a mod
// hooks a method once.
template <class W>
struct Hooked;

template <const sdk::EntryDesc& D, class R, class... A>
struct Hooked<Method<D, R(A...)>> {
  using Detour = R (*)(OriginalOf<Method<D, R(A...)>>, A...);
  static inline Detour detour = nullptr;

  static R run(OriginalOf<Method<D, R(A...)>> original, A... a) {
    const Detour d = detour;
    return d ? d(original, a...) : original(a...);
  }
};

}  // namespace detail

/// Hooks the game method M names (a Method wrapper, e.g. phasmo::GhostAI::ChangeState) through the
/// host's single MinHook instance, with `detour` written as
///
///   R detour(pg::api::Original<M> original, A... args)
///
/// for M's R(A...). `original(args...)` goes on with the call: the next detour in the chain of the
/// body the call came in through, or that body itself. Each body (the canonical body and, with
/// Twins::All, each twin) has its own chain, so another hook on one body never runs for a call
/// that entered through another. The hook is created, not enabled: the host contract's
/// enable_hooks enables it. `name` names the hook in the log (the entry's key when null). A mod
/// hooks a method once: a second hook<M> is refused (the host logs it) and the first keeps its
/// detour. False when M did not resolve (logged once), when `detour` is null, when an all-twins hook
/// meets a method with more than kMaxBodies bodies (logged, nothing hooked), or when the host could
/// not hook a body (it logs why). After such a partial failure the bodies hooked before it stay
/// hooked (the contract cannot unhook them) but run no detour: each passes the call straight on, as
/// if nothing were hooked, until the host removes them when the mod's start fails. Main thread.
template <auto& M>
bool hook(typename detail::Hooked<std::remove_cvref_t<decltype(M)>>::Detour detour, const char* name,
          Twins twins = Twins::All) {
  using W = std::remove_cvref_t<decltype(M)>;
  static_assert(W::desc.kind == sdk::Kind::Method, "hook takes a Method wrapper");
  if (!W::use::entry()) return false;
  using B = detail::Bodies<W, detail::Hooked<W>>;
  uint32_t created = 0;
  const bool ok = detail::hook_bodies(W::desc.key, detour != nullptr, &B::entry, reinterpret_cast<void**>(B::original),
                                      name ? name : W::desc.key, twins == Twins::All, created);
  // Before anything enables the new hooks. A partial hook runs no detour, and any detour left from
  // an earlier attempt goes with it.
  if (created > 0) detail::Hooked<W>::detour = ok ? detour : nullptr;
  return ok;
}

/// Whether a call of the game method M names is running on this thread, as the host's hook on it
/// sees it. False when M did not resolve or the host does not track it. Never logs. Main thread.
template <auto& M>
bool inside() {
  using W = std::remove_cvref_t<decltype(M)>;
  static_assert(W::desc.kind == sdk::Kind::Method, "inside takes a Method wrapper");
  return W::use::resolved() && detail::inside(W::desc.key) > 0;
}

/// Looks up, through `host`, every entry the mod's code uses, and keeps `host` for the wrappers. True
/// when every required (not kOptional) entry resolved; otherwise false, with `missing` naming the
/// keys that did not, sorted and separated by ", ". Each entry's "not available" line may be logged
/// again after it. `host` must be compatible with this game API (pg_api_compatible); pg::mod::bind
/// checks that and calls this. Main thread.
bool bind(const PgHostApi* host, std::string& missing);

/// The keys of the entries the mod's code uses that are not kOptional, sorted. Points `keys` at an
/// array the mod keeps, unchanged, for as long as its DLL stays loaded, and returns its length; the
/// array is built at the first call, when every entry has registered. The mod's pgmod_requires
/// export returns this. Any thread, once the mod's DLL has loaded.
size_t used_required(const char* const** keys);

}  // namespace pg::api
