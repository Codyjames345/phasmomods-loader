#pragma once

/// @file
/// @brief The game API's templates: the typed surface a mod calls to reach the game, by the readable
/// names of the game map (sdk/map).
///
/// A mod never holds a game address or a field offset; the host resolves the map in the running game
/// and hands each entry over through the host contract (core/mod_api.h).
///
/// Each map header ends with ready-made wrappers in namespace phasmo, for example
/// `phasmo::GhostAI::UnAppear(ghost)`, `phasmo::GhostAI::state.get(ghost)` and
/// `phasmo::GhostState::hunting()`. They are built from the templates below.
///
/// **What a mod requires is what its code calls.** Every accessor's members are templates, so a
/// wrapper's entry is registered only when code in the mod calls it, and the mod's pmmod_requires
/// export lists exactly those entries (less the ones marked kOptional). The host does not start a mod
/// while one of them did not resolve in the running game build.
///
/// **A wrapper's shape must be its entry's:** where the map gives a method's or icall's params, a
/// wrapper with another number of arguments does not compile, and where it gives a field's type as a
/// C# value type by name (bool, int, float and the like), neither does a T of another size.
///
/// **A call of a game method is never virtual:** it runs the body the map entry names, even when the
/// object's class overrides it. Method::virtual_call runs the override, as C# calls a virtual method.
///
/// **Handles:** where a wrapper names a game object, its parameter, return or field type is a typed
/// handle (sdk/api/ref.h): `pm::api::Ref<D>` for a mapped type, `pm::api::Object` for any managed
/// object. The game itself never sees one: every call, and every hook, crosses the boundary with the
/// raw pointers, the wrapper converting at its own boundary (MSVC x64 returns a class with a
/// constructor through a hidden pointer, so a game function must always be called, and hooked, with
/// raw types). Code that still keeps a game object as a void* compiles through a transitional bridge
/// (an Object constructor, a handle's `operator void*`, and a raw overload on each handle-taking
/// wrapper), which is removed in part 5 of the typed-handle migration.
///
/// **Failure:** when an entry did not resolve anyway (an optional one, or the mod was not bound),
/// nothing jumps to null: a call, a read or a write logs "<key> is not available in this game build"
/// once and returns the type's default value (0, false, null) or does nothing.
///
/// **Lifetime:** a managed reference a wrapper gives (a handle read from a field or returned by a
/// call, a type object, a new object or array) is valid for the current frame only. Do not keep it
/// across frames: keep the object in a pm::api::Held (held.h) instead.
///
/// **Threading:** everything here runs on Unity's main thread, where the host calls pmmod_start and
/// pmmod_tick, except the registration a wrapper makes while the mod's DLL loads. Each "logged once"
/// is kept with an atomic flag, so even two threads log a line once.
///
/// Exports of GameAssembly.dll (Export below) are for the host's own code only: a mod never sees the
/// handle, and the host contract's game_entry answers null for an export.
///
/// Managed strings, arrays, lists, alive checks and GC holds are in runtime.h and held.h.
/// @ingroup runtime
/// @since Contract 2.0

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
#include "ref.h"

namespace pm::api {

namespace detail {

// One game entry the mod's code uses, and what the host gave for it at bind. The two flags are
// atomic, so two threads that log at once still log each line once.
struct Slot {
  const sdk::EntryDesc* desc;
  const PmGameEntry* entry;        // null before bind and when the entry did not resolve
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
const PmHostApi* host();
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
// The host contract's copy_settings and fields_of_type (version 2.2), or false and 0 before bind.
bool copy_settings(const char* typeKey, void* dst, const void* src);
uint32_t fields_of_type(const char* typeKey, const void* obj, const char* fieldTypeKey, bool arrayOf, void** out,
                        uint32_t max);
// The object a virtual call is made on: an instance method's first argument, already brought through
// to_raw by the caller, so a handle arrives as the void* it holds.
template <class Obj, class... Rest>
const void* object_of(Obj obj, Rest...) {
  static_assert(std::is_pointer_v<Obj>, "an instance method's first parameter is the object, a pointer");
  return obj;
}

// The conversions behind the wrappers (ruling 1, typed handles part 2a): a wrapper's R or A may be
// Ref<D> or Object (is_handle_v, ref.h), and the game's own signature, and a hook body's, always
// names raw_t of them: void* for a handle, the type itself otherwise.
template <class T>
using raw_t = std::conditional_t<is_handle_v<T>, void*, T>;
// The game's argument for a wrapper's argument: a handle's raw reference, or the value itself.
template <class T>
constexpr raw_t<T> to_raw(T x) {
  if constexpr (is_handle_v<T>) return x.raw();
  else return x;
}
// The wrapper's type for a raw result or argument: a handle wrapping it, or it unchanged.
template <class T>
constexpr T from_raw(raw_t<T> raw) {
  if constexpr (is_handle_v<T>) return T::unchecked(raw);
  else return raw;
}
// One argument the transitional raw overload of a handle-taking wrapper accepts where its parameter A
// is a handle (removed in part 5): exactly void*, const void* or nullptr_t. A Ref of another type
// never fits, typed or through the bridge: that is the point of the handles. For any other A,
// anything convertible to A.
template <class P, class A>
concept RawBridgeArg =
  (is_handle_v<A> && (std::is_same_v<std::decay_t<P>, void*> || std::is_same_v<std::decay_t<P>, const void*> ||
                      std::is_same_v<std::decay_t<P>, std::nullptr_t>)) ||
  (!is_handle_v<A> && std::is_convertible_v<P, A>);
// Converts one raw bridge argument to the wrapper's parameter type A.
template <class A, class P>
constexpr A raw_bridge_to(P p) {
  if constexpr (is_handle_v<A>) {
    if constexpr (std::is_same_v<std::remove_cvref_t<P>, std::nullptr_t>) return A{};
    else if constexpr (std::is_same_v<std::remove_cv_t<P>, const void*>) return from_raw<A>(const_cast<void*>(p));
    else return from_raw<A>(p);
  } else {
    return static_cast<A>(p);
  }
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
// The host contract's scene_find and animator_param (version 2.1), or null and false before bind.
void* scene_find(const char* key, void* root);
bool animator_param(const char* key, const void* animator, int32_t* hash);
// The host contract's steer_agent (version 2.2), or false before bind.
bool steer_agent(void* agent, float dx, float dz);
// The host contract's abandon_ghost_spawn (version 2.2), or nothing before bind.
void abandon_ghost_spawn();

}  // namespace detail

/// The use of the map entry D by the mod's code, which every wrapper below names as its member `use`.
/// Calling entry() or resolved() adds D to what the mod requires (unless D is kOptional); naming
/// Use<D> or a wrapper's class does not, so the wrappers call them only from the members a mod calls.
template <const sdk::EntryDesc& D>
struct Use {
  /// The resolved entry, or null when it did not resolve (logged once per bind). Main thread.
  /// @return The host's PmGameEntry for D, or null.
  static const PmGameEntry* entry() {
    if (!resolved()) detail::report_unavailable(slot());
    return slot().entry;
  }
  /// Whether the entry resolved, without logging. Main thread.
  /// @return True when the host resolved D in this game build.
  static bool resolved() {
    static_cast<void>(detail::Registration<D>::done);
    return slot().entry != nullptr;
  }
  /// What the host gave for D at bind, and whether its lines were logged.
  /// @return D's slot, which the game API keeps for the process lifetime.
  static detail::Slot& slot() { return detail::slot<D>; }
};

template <const sdk::EntryDesc& D, class Sig>
struct Method;

/// A game method, called as R(A...). For an instance method the first parameter is the object
/// (`this`). A struct parameter whose size is not 1, 2, 4 or 8 bytes is passed by address, as the x64
/// calling convention does, and the callee may modify it: write it as a pointer, and pass a copy when
/// the original must stay unchanged. Where the entry gives its params, A... is one argument per
/// param, after the object for an instance method, or the wrapper does not compile. R and any A may
/// be the handles of ref.h where the map names a game object; the method is still called with the raw
/// pointers, and a mod that still holds a raw void* for a handle parameter compiles through the
/// wrapper's transitional raw overload (removed in part 5). A handle or void* the method returns is a
/// managed reference: do not keep it across frames. Main thread.
template <const sdk::EntryDesc& D, class R, class... A>
struct Method<D, R(A...)> {
  static_assert(D.kind == sdk::Kind::Method || D.kind == sdk::Kind::Inflated,
                "Method wraps a map entry of kind Method or Inflated");
  static_assert(detail::arity(D) < 0 || detail::arity(D) == static_cast<int>(sizeof...(A)),
                "the wrapper's arity is not its entry's: one argument per param, after the object for "
                "an instance method");
  /// The map entry this wraps (its key, kind and flags), as hook<M> and inside<M> read it.
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// For an Inflated entry: a new object of its generic instance (a List<UInt32> for List<UInt32>.ctor), fields
  /// zeroed and no constructor run, so call the instance's constructor on it next, as C#'s `new` does. A managed
  /// reference, valid this frame. Main thread. Unresolved, or a host older than Contract 2.5: null (logged once when
  /// unresolved).
  /// @return The object, or null.
  /// @since Contract 2.5
  template <int = 0>
  void* new_object() const {
    static_assert(D.kind == sdk::Kind::Inflated, "new_object makes an object of an Inflated entry's generic instance");
    return use::entry() ? detail::object_new(D.key) : nullptr;
  }

  /// Calls the method's code with the arguments and its MethodInfo, as the game does: the code is
  /// reached through the game's raw signature (a void* for every handle in R and A...), and the raw
  /// result comes back wrapped. The call is never virtual: it runs the declared body even when the
  /// object's class overrides it (see virtual_call). Unresolved: logs once and returns R{} (a null
  /// handle for a handle R) without calling anything.
  /// @param a The method's arguments, the object first for an instance method, as the entry's params
  ///   give them.
  /// @return What the method returns; R{} when the entry did not resolve. A handle or void* is a
  ///   managed reference, valid this frame only.
  template <int = 0>
  R operator()(A... a) const {
    const PmGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    if constexpr (std::is_void_v<R>) {
      reinterpret_cast<void (*)(detail::raw_t<A>..., const void*)>(e->code)(detail::to_raw(a)..., e->method);
    } else {
      return detail::from_raw<R>(reinterpret_cast<detail::raw_t<R> (*)(detail::raw_t<A>..., const void*)>(
          e->code)(detail::to_raw(a)..., e->method));
    }
  }

  /// transitional: removed in part 5. The same call as code that still holds raw void* makes it: each
  /// argument going to a handle parameter must be void*, const void* or nullptr, and each argument is
  /// converted to its parameter's type (a handle with Ref::unchecked) before operator() runs. A Ref
  /// of another type is refused here too: the handles keep meaning what they say.
  /// @tparam P The arguments' types: raw references where the wrapper's parameter is a handle, and
  ///   anything convertible to a parameter otherwise.
  /// @param p The arguments, the object first for an instance method.
  /// @return What the method returns; R{} when the entry did not resolve.
  template <class... P>
    requires((detail::is_handle_v<A> || ...) && sizeof...(P) == sizeof...(A) && (detail::RawBridgeArg<P, A> && ...))
  R operator()(P... p) const {
    R (Method::*typed)(A...) const = &Method::operator();
    return (this->*typed)(detail::raw_bridge_to<A, P>(p)...);
  }

  /// Calls the method as C# calls a virtual method: the method the class of the object (the first
  /// argument) runs, its override or else the declared body, with that method's MethodInfo. The code
  /// is reached through the game's raw signature, as operator() does. A null object, or one not of
  /// the method's declaring type, is refused: nothing is called, R{} is returned, and the refusal is
  /// logged once per bind. Before bind it calls the declared body, as operator() does. Unresolved:
  /// logs once and returns R{} without calling anything.
  /// @param a The method's arguments, the object first.
  /// @return What the method the object's class runs returns; R{} when the call is refused or the entry
  ///   did not resolve.
  template <int = 0>
  R virtual_call(A... a) const {
    static_assert(!(D.flags & sdk::kStatic), "a static method has no object to dispatch on");
    static_assert(sizeof...(A) > 0, "virtual_call needs the object as the first argument");
    const PmGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    void* code = e->code;
    const void* method = e->method;
    if (detail::host()) {
      const void* obj = detail::object_of(detail::to_raw(a)...);
      method = obj ? detail::virtual_method(obj, D.key) : nullptr;
      // core/mod_api.h: a MethodInfo's first member is the address of its code.
      code = method ? *static_cast<void* const*>(method) : nullptr;
      if (!code) {
        detail::report_refused_call(use::slot());
        if constexpr (std::is_void_v<R>) return;
        else return R{};
      }
    }
    if constexpr (std::is_void_v<R>) {
      reinterpret_cast<void (*)(detail::raw_t<A>..., const void*)>(code)(detail::to_raw(a)..., method);
    } else {
      return detail::from_raw<R>(
          reinterpret_cast<detail::raw_t<R> (*)(detail::raw_t<A>..., const void*)>(code)(detail::to_raw(a)..., method));
    }
  }

  /// transitional: removed in part 5. virtual_call as code that still holds raw void* makes it,
  /// converting each argument as operator()'s raw overload does.
  /// @tparam P The arguments' types: raw references where the wrapper's parameter is a handle, and
  ///   anything convertible to a parameter otherwise.
  /// @param p The arguments, the object first.
  /// @return What the method the object's class runs returns; R{} when the call is refused or the
  ///   entry did not resolve.
  template <class... P>
    requires((detail::is_handle_v<A> || ...) && sizeof...(P) == sizeof...(A) && (detail::RawBridgeArg<P, A> && ...))
  R virtual_call(P... p) const {
    R (Method::*typed)(A...) const = &Method::virtual_call;
    return (this->*typed)(detail::raw_bridge_to<A, P>(p)...);
  }

  /// Whether the method resolved in this game build. Never logs.
  /// @return True when the entry resolved. Calling available() makes the entry required, unless it is kOptional.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// An instance field of type T. T is a plain value (bool, int32_t, float, a struct copied whole), one
/// of the handles of ref.h, or void* for a managed reference. Where the entry's type is a C# value
/// type by name (bool, int, float and the like), a T of another size does not compile. Reads and
/// writes go to the resolved offset in the object; a handle reads the reference and wraps it, and
/// writing one (or a void*) goes through the GC write barrier exactly as before. Main thread.
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

  /// The field's value in the managed object `obj`. Unresolved (logged once) or a null `obj`: T{} (a
  /// null handle for a handle T). A reference (a handle, or void*) is valid for the current frame only:
  /// do not keep it across frames. The wrapper takes any Object because it cannot know the owner type
  /// at compile time: an EntryDesc names its owner by a key string, not by a C++ type, so knowing the
  /// object is of the field's owner class is the caller's job.
  /// @param obj The managed object that holds the field, of the entry's owner type; null allowed.
  /// @return The field's value, or T{}.
  template <int = 0>
  T get(Object obj) const {
    const PmGameEntry* e = use::entry();
    if (!e || !obj) return T{};
    if constexpr (detail::is_handle_v<T>) {
      void* value = nullptr;
      std::memcpy(&value, static_cast<const char*>(obj.raw()) + e->offset, sizeof value);
      return detail::from_raw<T>(value);
    } else {
      T value;
      std::memcpy(&value, static_cast<const char*>(obj.raw()) + e->offset, sizeof value);
      return value;
    }
  }

  /// Stores `value` in the managed object `obj`. A managed reference (a handle, or void*) goes through
  /// the GC write barrier (the host's set_reference); a plain value is written directly. Unresolved
  /// (logged once) or a null `obj`: nothing is written. A write changes only this client's copy, and
  /// skips whatever the game does when it sets the field itself: the field's note names the method to
  /// call for the game's own behaviour. As for get, the owner type is the caller's to know.
  /// @param obj The managed object that holds the field, of the entry's owner type; null allowed.
  /// @param value The value to store.
  template <int = 0>
  void set(Object obj, T value) const {
    const PmGameEntry* e = use::entry();
    if (!e || !obj) return;
    if constexpr (std::is_same_v<T, void*> || detail::is_handle_v<T>) {
      if (const PmHostApi* h = detail::host()) h->set_reference(obj.raw(), e->offset, detail::to_raw(value));
    } else {
      std::memcpy(static_cast<char*>(obj.raw()) + e->offset, &value, sizeof value);
    }
  }
};

/// A static field of type T, one value for the whole class. T is a number, a bool, an enum, one of
/// the handles of ref.h, or void* for a managed reference. A struct is not supported: its bytes are
/// copied whole, which would bypass the GC write barrier for any managed reference inside it, and the
/// API cannot tell whether one does.
/// Reading or writing it first runs the class's static constructor when that has not run
/// yet, as the game's own first use would, so use it only from code that runs while the game plays
/// (pmmod_tick, a hook's detour), never from a DLL's initialisation; the host never reads a static
/// field while it resolves the map at startup. As for Field, a T whose size is not that of the
/// entry's type, where the map names a C# value type, does not compile. Main thread.
template <const sdk::EntryDesc& D, class T>
struct StaticField {
  static_assert(D.kind == sdk::Kind::Field && (D.flags & sdk::kStatic),
                "StaticField wraps a map entry of kind Field flagged kStatic");
  static_assert(std::is_arithmetic_v<T> || std::is_enum_v<T> || std::is_same_v<T, void*> || detail::is_handle_v<T>,
                "a static field is a number, a bool, an enum, or void* or a handle for a managed "
                "reference; a struct may hold references, which a whole copy would store without the "
                "GC write barrier");
  static_assert(detail::value_size(D.type) == 0 || detail::value_size(D.type) == sizeof(T),
                "T's size is not the size of the entry's type");
  /// The map entry this wraps (its key, kind and flags).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The field's value. Unresolved: logs once and returns T{} (a null handle for a handle T). When
  /// the host cannot read it (T's size is not the field's, or the class's static constructor threw):
  /// logs once per bind and returns T{}. A reference (a handle, or void*) is valid for the current
  /// frame only: do not keep it across frames.
  /// @return The field's value, or T{}.
  template <int = 0>
  T get() const {
    if (!use::entry()) return T{};
    if constexpr (detail::is_handle_v<T>) {
      void* value = nullptr;
      if (detail::static_get(D.key, &value, sizeof value)) return detail::from_raw<T>(value);
    } else {
      T value{};
      if (detail::static_get(D.key, &value, sizeof value)) return value;
    }
    detail::report_refused_static(use::slot());
    return T{};
  }

  /// Whether the field could be read, with its value in `out`: false, leaving `out` alone and logging nothing, when it did
  /// not resolve or the host could not read it (T's size is not the field's, or the class's static constructor threw). For
  /// a gate that must tell "null" from "unknown", which get() answers alike. A reference is valid for the current frame only.
  /// @param out Receives the value when the read succeeds.
  /// @return True when the field was read.
  template <int = 0>
  bool read(T& out) const {
    if (!use::resolved()) return false;
    if constexpr (detail::is_handle_v<T>) {
      void* value = nullptr;
      if (!detail::static_get(D.key, &value, sizeof value)) return false;
      out = detail::from_raw<T>(value);
      return true;
    } else {
      return detail::static_get(D.key, &out, sizeof out);
    }
  }

  /// Stores `value` in the field; a managed reference (a handle, or void*) goes through the GC write
  /// barrier. Unresolved: logs once and writes nothing. When the host cannot write it: logs once per
  /// bind and writes nothing. The write is this client's alone.
  /// @param value The value to store.
  template <int = 0>
  void set(T value) const {
    if (!use::entry()) return;
    if constexpr (detail::is_handle_v<T>) {
      void* raw = detail::to_raw(value);
      if (!detail::static_set(D.key, &raw, sizeof raw)) detail::report_refused_static(use::slot());
    } else {
      if (!detail::static_set(D.key, &value, sizeof value)) detail::report_refused_static(use::slot());
    }
  }

  /// Whether the field resolved in this game build. Never logs.
  /// @return True when the entry resolved.
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
/// the callee may modify it. R and any A may be the handles of ref.h where the map names a game
/// object; the function is still called with the raw pointers, with the same transitional raw
/// overload as Method (removed in part 5). A handle or void* it returns is a managed reference: do
/// not keep it across frames. Main thread.
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

  /// Calls the engine function with the arguments, through the function's raw signature (a void* for
  /// every handle in R and A...), and brings the raw result back wrapped. Unresolved: logs once and
  /// returns R{} (a null handle for a handle R) without calling anything.
  /// @param a The function's arguments, the object first unless the entry is kStatic.
  /// @return What the function returns; R{} when the entry did not resolve.
  template <int = 0>
  R operator()(A... a) const {
    const PmGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    if constexpr (std::is_void_v<R>) {
      reinterpret_cast<void (*)(detail::raw_t<A>...)>(e->code)(detail::to_raw(a)...);
    } else {
      return detail::from_raw<R>(
          reinterpret_cast<detail::raw_t<R> (*)(detail::raw_t<A>...)>(e->code)(detail::to_raw(a)...));
    }
  }

  /// transitional: removed in part 5. The same call as code that still holds raw void* makes it: each
  /// argument going to a handle parameter must be void*, const void* or nullptr, and each argument is
  /// converted to its parameter's type (a handle with Ref::unchecked) before operator() runs. A Ref of
  /// another type is refused here too.
  /// @tparam P The arguments' types: raw references where the wrapper's parameter is a handle, and
  ///   anything convertible to a parameter otherwise.
  /// @param p The arguments, the object first unless the entry is kStatic.
  /// @return What the function returns; R{} when the entry did not resolve.
  template <class... P>
    requires((detail::is_handle_v<A> || ...) && sizeof...(P) == sizeof...(A) && (detail::RawBridgeArg<P, A> && ...))
  R operator()(P... p) const {
    R (Icall::*typed)(A...) const = &Icall::operator();
    return (this->*typed)(detail::raw_bridge_to<A, P>(p)...);
  }

  /// Whether the icall resolved in this game build. Never logs.
  /// @return True when the entry resolved.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

#ifdef PM_HOST
namespace host_only {
/// The host's own resolved entry for `key` (host/game_map.cpp), or null when it is unknown or
/// unresolved. Main thread, after the map resolved at startup.
const PmGameEntry* game_entry(const char* key);
}  // namespace host_only
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
  /// @return The number, read from the running game: never write it into a mod's code.
  template <int = 0>
  int32_t operator()() const {
    const PmGameEntry* e = use::entry();
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
  /// @return The class, or null.
  template <int = 0>
  void* klass() const {
    const PmGameEntry* e = use::entry();
    return e ? e->klass : nullptr;
  }

  /// Whether the managed object `obj` is of this type or of a class derived from it (following the
  /// class's parents). An interface type never matches: the check does not look at the interfaces
  /// a class implements. False for a null `obj`, and when the type did not resolve (logged once).
  /// @param obj Any managed handle, or null; a Ref of any type converts to the Object it takes, and
  ///   a raw void* still reaches it through the transitional bridge.
  /// @return True when `obj` is of this type.
  template <int = 0>
  bool is(Object obj) const {
    return use::entry() && obj && detail::is_instance(obj.raw(), D.key);
  }

  /// The type's System.Type object, as C#'s typeof gives it (for GetComponent, AddComponent and
  /// the like). It stays a raw void* through the typed-handle migration: a System.Type is never used
  /// as a handle of a game type, so a handle would add nothing to it. A managed object: do not keep
  /// it across frames without a pm::api::Held. Unresolved: logs once and returns null.
  /// @return The System.Type, or null.
  template <int = 0>
  void* type_object() const {
    return use::entry() ? detail::type_object(D.key) : nullptr;
  }

  /// A new managed object of this class with every field zeroed. No constructor runs. Nothing else
  /// refers to it yet, so do not keep it across frames without a pm::api::Held (held.h). Not for engine
  /// objects: a Component or MonoBehaviour comes from AddComponent or Instantiate, never from
  /// make(), which gives an object Unity knows nothing of. Unresolved: logs once and returns a null
  /// handle.
  /// @return A handle to the new object, or null.
  template <int = 0>
  Ref<D> make() const {
    return Ref<D>::unchecked(use::entry() ? detail::object_new(D.key) : nullptr);
  }

  /// A new managed array of `length` elements of this type, zeroed (read it with pm::api::Array or
  /// RefArray, runtime.h). Do not keep it across frames without a pm::api::Held. Unresolved: logs
  /// once and returns a null handle.
  /// @param length The number of elements.
  /// @return A handle to the new array, or null.
  template <int = 0>
  Object make_array(size_t length) const {
    return Object::unchecked(use::entry() ? detail::array_new(D.key, length) : nullptr);
  }

  /// Copies `src`'s settings onto `dst`, two objects of this class or of classes derived from it:
  /// every instance field the class itself declares as public and of a value type (the settings
  /// Unity's inspector shows for a component of the class), a struct whole and any reference inside it
  /// through the GC write barrier, read from the game's metadata (the host contract's copy_settings).
  /// Inherited fields and reference fields are left alone. False, copying nothing, when either object
  /// is null or not of the class, when the runtime cannot type one of the settings (the host logs it),
  /// and when the type did not resolve (logged once).
  /// @param dst The object to copy onto.
  /// @param src The object to copy from.
  /// @return True when every setting was copied; false when nothing was.
  template <int = 0>
  bool copy_settings(Object dst, Object src) const {
    return use::entry() && dst && src && detail::copy_settings(D.key, dst.raw(), src.raw());
  }

  /// The references `obj` holds in every instance field this class itself declares of the type
  /// FieldType names (a Type wrapper), or with `arrayOf` of an array of it, in the order the class lays
  /// them out: the first `max` of them by layout into `out`. Returns how many such fields the class declares (more than
  /// `max` is possible). For fields of one type that nothing in the game's code tells apart, so no map
  /// entry can name one (the host contract's fields_of_type). 0 for a null or foreign `obj`, and when
  /// either type did not resolve (logged once).
  /// @tparam FieldType The Type wrapper of the fields' type, for example phasmo::GhostModel::type.
  /// @param obj The managed object of this class to read.
  /// @param arrayOf True to read the fields of an array of FieldType instead.
  /// @param out Room for `max` raw references, in layout order; null only with `max` 0. The output
  ///   stays void** for now; a mod wrapping a handle around each is part 2b of the migration.
  /// @param max How many references `out` takes.
  /// @return How many such fields the class declares, or 0.
  template <auto& FieldType>
  size_t fields_of(Object obj, bool arrayOf, void** out, size_t max) const {
    using F = std::remove_cvref_t<decltype(FieldType)>;
    if (!use::entry() || !F::use::entry() || !obj) return 0;
    return detail::fields_of_type(D.key, obj.raw(), F::desc.key, arrayOf, out, static_cast<uint32_t>(max));
  }
};

/// The object `x` as a handle of the map's type D, when the runtime says it is one: of D or of a class
/// derived from it, following the class's parents as Type::is does. The checked way down from a base
/// type's handle or an Object (ref.h). Calling it makes D required, as Type::is does. Main thread.
/// @tparam D A map entry of kind Type; never an interface, which no object matches.
/// @param x Any handle, or null; a Ref of any type converts to the Object it takes.
/// @return The handle of type D to the same object; null for a null `x`, an object of another type, and
///   when D did not resolve (logged once).
template <const sdk::EntryDesc& D>
Ref<D> as(Object x) {
  return Use<D>::entry() && x && detail::is_instance(x.raw(), D.key) ? Ref<D>::unchecked(x.raw()) : Ref<D>{};
}

/// A GameObject of the game's scenes or prefabs, found by its path from an object the mod already holds
/// (a Scene entry). The path is the entry's, never the mod's: the game API takes no raw path string.
/// Main thread, while the game plays.
template <const sdk::EntryDesc& D>
struct SceneObject {
  static_assert(D.kind == sdk::Kind::Scene, "SceneObject wraps a map entry of kind Scene");
  /// The map entry this wraps (its key, path and where the offline check found it).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The Transform at the entry's path under the Transform `root`, as Transform.Find finds it. The host
  /// keeps one answer per entry, for the last root found under, until the next scene load: calling it
  /// again with the same root costs no search, but a call with another root searches again, so
  /// alternating roots searches every time. Null when nothing is there (logged once per scene load,
  /// and searched again next call), when `root` is destroyed or not a Transform (logged the same way),
  /// when `root` is null (at once: no search, nothing logged, the kept answer stays), and when the
  /// entry did not resolve (logged once). A managed reference: do not keep it across
  /// frames without a pm::api::Held.
  /// @param root The Transform to search under; null allowed.
  /// @return The Transform found, or null.
  template <int = 0>
  void* find(void* root) const {
    return use::entry() ? detail::scene_find(D.key, root) : nullptr;
  }

  /// Whether the entry resolved in this game build. Never logs. A resolved entry may still be missing
  /// from the scene that is loaded: find says so.
  /// @return True when the entry resolved.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// A layer, by its name (a Layer entry). The host resolves it through LayerMask.NameToLayer when it
/// activates. The game API takes no raw layer number. Main thread.
template <const sdk::EntryDesc& D>
struct Layer {
  static_assert(D.kind == sdk::Kind::Layer, "Layer wraps a map entry of kind Layer");
  /// The map entry this wraps (its key and layer name).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The layer's index, 0 to 31. Unresolved (no layer has the name in this build): logs once and
  /// returns -1, never 0, which is the Default layer; check available() before using it.
  /// @return The index, or -1.
  template <int = 0>
  int32_t index() const {
    const PmGameEntry* e = use::entry();
    return e ? static_cast<int32_t>(e->value) : -1;
  }

  /// Whether the layer resolved in this game build. Never logs.
  /// @return True when the entry resolved.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// A parameter of an Animator, by its name (an AnimatorParam entry). Main thread, while the game plays.
template <const sdk::EntryDesc& D>
struct AnimatorParam {
  static_assert(D.kind == sdk::Kind::AnimatorParam, "AnimatorParam wraps a map entry of kind AnimatorParam");
  /// The map entry this wraps (its key, parameter name and controller).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The parameter's hash (Animator.StringToHash), for the Animator's by-id setters such as
  /// phasmo::Animator::SetBoolID. The host checks it against `animator`'s own parameters the first time
  /// that Animator is asked, and again after a scene load. 0 when `animator` lacks it (logged once per
  /// scene load), when `animator` is null, destroyed or not an Animator, and when the entry did not
  /// resolve (logged once).
  /// @param animator The Animator to check the parameter against.
  /// @return The hash, or 0.
  template <int = 0>
  int32_t hash(const void* animator) const {
    int32_t value = 0;
    if (use::entry() && detail::animator_param(D.key, animator, &value)) return value;
    return 0;
  }

  /// Whether the entry resolved in this game build. Never logs. A resolved parameter may still be
  /// missing from a given Animator: hash says so.
  /// @return True when the entry resolved.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// A property of a shader, by its name (a ShaderProperty entry). The host reads its id through
/// Shader.PropertyToID when it activates. Pass the id to a material's by-id functions (such as
/// phasmo::Material::HasProperty and SetColorImpl): whether a material has the property is the
/// material's to answer. Main thread.
template <const sdk::EntryDesc& D>
struct ShaderProperty {
  static_assert(D.kind == sdk::Kind::ShaderProperty, "ShaderProperty wraps a map entry of kind ShaderProperty");
  /// The map entry this wraps (its key, property name and shader).
  static constexpr const sdk::EntryDesc& desc = D;
  /// The entry's use: calling use::entry() or use::resolved() makes the entry required (see Use).
  using use = Use<D>;

  /// The property's id. Unresolved: logs once and returns 0; check available() before using it.
  /// @return The id (Shader.PropertyToID), or 0.
  template <int = 0>
  int32_t id() const {
    const PmGameEntry* e = use::entry();
    return e ? static_cast<int32_t>(e->value) : 0;
  }

  /// Whether the property's id was read in this game build. Never logs.
  /// @return True when the entry resolved.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// An effect on a NavMeshAgent's walk that the host carries out in the game's native code (the host contract's
/// steer_agent). A mod names no part of how: it wraps no map entry and requires none, so the host may change how
/// it steers, and what it needs from the game, without the mod being rebuilt. Main thread.
struct AgentSteering {
  /// @brief Steers `agent` for this frame: it walks in the direction (dx, dz) in place of the one Unity's crowd
  /// simulation chose. Its destination, speed, acceleration, braking and avoidance of other agents stay the game's.
  /// @param agent The managed NavMeshAgent, as phasmo::GhostAI::agent gives it. Null, destroyed or not a
  /// NavMeshAgent: nothing changes.
  /// @param dx World X of the direction, unitless.
  /// @param dz World Z of the direction, unitless. (dx, dz) has a length of at most 1. The agent's desired velocity
  /// is this direction times its speed, so a shorter one walks it slower, and (0, 0) holds it still as it
  /// decelerates. World Y is zero. A direction longer than 1, or not finite, ends this frame's steering of any agent
  /// until a valid call after it.
  /// @pre Main thread, from pmmod_tick. It lasts until the mods tick next frame: ask every frame the agent is to stay
  /// steered, and stop asking to hand it back to the game. One agent at a time: the last valid call of a frame counts.
  /// @note This client only: it sends nothing, and other clients see only what the game itself syncs. Stopping the
  /// mods ends it. When the host cannot steer (available() false) it does nothing; the host logged why once.
  /// @since Contract 2.2
  /// @code
  /// void* agent = phasmo::GhostAI::agent.get(ghost);
  /// phasmo::NavMeshAgent::steer(agent, 0.0f, 1.0f);  // this frame it walks towards world +Z
  /// @endcode
  void operator()(void* agent, float dx, float dz) const { detail::steer_agent(agent, dx, dz); }

  /// @brief Whether the host can steer an agent in this game build. Ask it in pmmod_start, and stay inactive when it
  /// cannot.
  /// @return False before the game API is bound, when the game build lacks what the host needs (it logs which game
  /// entries at startup), or when the host could not make the effect ready in the game's code (it logs why, and the
  /// game's code stays as it was). Unlike other wrappers' available(), the first call makes the effect ready, and it
  /// stays ready until the mods restart. After a refusal the host tries again only when the mods start afresh, so a
  /// later call answers false at once.
  /// @pre Main thread, from pmmod_start.
  /// @note This client only.
  /// @since Contract 2.2
  bool available() const { return detail::steer_agent(nullptr, 0.0f, 0.0f); }
};

/// An effect on the game's own ghost setup that the host carries out (the host contract's abandon_ghost_spawn):
/// stop a running GhostController.Spawn at once, as if it had never been called. A mod names no part of how, and it
/// wraps no map entry. For a mod that wants to see what the setup's roll would offer and hold the setup back. Main
/// thread.
struct GhostSpawnAbandon {
  /// @brief Unwinds the running GhostController.Spawn: the call that started it ends without the setup writing or
  /// sending anything more, and control goes on after the `catch` around that call.
  /// @pre Main thread, from a hook whose method the spawn calls (GhostController.PickType, before it returns), under
  /// a call of GhostController.Spawn the mod made inside a C++ `try { ... } catch (...)`. It is not a way to stop any
  /// other code.
  /// @note This client only, and it sends nothing. Outside a running spawn it does nothing and returns, logged once.
  /// Before the game API is bound it does nothing and returns. Otherwise it does not return. What the spawn wrote before
  /// the call stays, so make the call before anything it writes. The unwind passes through every hook on the stack, on
  /// GhostController.Spawn and on the method the call is made from, other mods' included: a detour there releases what
  /// it holds by RAII, never by code after its call of the original.
  /// @since Contract 2.2
  /// @code
  /// try {
  ///   original(controller);  // reaches a hook of GhostController.PickType, which calls this
  /// } catch (...) {
  /// }
  /// @endcode
  void operator()() const { detail::abandon_ghost_spawn(); }
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
/// only during the detour's call: do not keep it. `code` runs through the trampoline the host made
/// for this link and leads on into the links below it, and unwinding the chain frees the
/// trampolines: a kept original calls freed code once its own link, or one below it, is removed.
/// A mod's hooks are removed when a start fails and when the mods stop, so a mod can never keep
/// one; only the holder of the chain's lowest link, with nothing removable below it while it is
/// kept, may. Main thread, as the hooked call.
template <const sdk::EntryDesc& D, class R, class... A>
struct OriginalOf<Method<D, R(A...)>> {
  /// the next detour or the body, in the game's raw types (a void* for every handle in R and A...);
  /// null before the hook is placed
  detail::raw_t<R> (*code)(detail::raw_t<A>..., const void*);
  const void* method;  ///< the game's MethodInfo, passed on as the call's last argument
  /// Goes on with the game's call: the typed arguments go through to_raw to the next link, and its
  /// raw result comes back through from_raw.
  /// @param a The call's arguments, as the detour received them or changed.
  /// @return What the rest of the chain returns; R{} when `code` is null.
  R operator()(A... a) const {
    if (code) {
      if constexpr (std::is_void_v<R>) code(detail::to_raw(a)..., method);
      else return detail::from_raw<R>(code(detail::to_raw(a)..., method));
    } else if constexpr (!std::is_void_v<R>) return R{};
  }
};

/// The `original` a detour of a hook on M (a Method wrapper) receives.
template <auto& M>
using Original = OriginalOf<std::remove_cvref_t<decltype(M)>>;

namespace detail {

// One entry function per body slot, for the Method wrapper W and the handler H that runs for each
// call (H::run(original, args...)): body_entry<I> is what is hooked on body I, and `original[I]` is
// what continues that body's chain. Each body_entry<I> reads its own slot, so no two of them are the
// same code and the linker never folds them into one. Each holds a hooks::Guard while it runs. The
// entry functions the game calls take and return the game's raw types (a void* for every handle in
// the wrapper's R and A...): the game never sees a handle, and H::run gets the wrapper's typed
// arguments and gives back its typed result, converted at this boundary.
template <class W, class H>
struct Bodies;

template <const sdk::EntryDesc& D, class R, class... A, class H>
struct Bodies<Method<D, R(A...)>, H> {
  using Code = raw_t<R> (*)(raw_t<A>..., const void*);
  static inline Code original[kMaxBodies] = {};

  template <uint32_t I>
  static raw_t<R> body_entry(raw_t<A>... a, const void* method) {
    hooks::Guard guard;
    if constexpr (std::is_void_v<R>) {
      H::run(OriginalOf<Method<D, R(A...)>>{original[I], method}, detail::from_raw<A>(a)...);
    } else {
      return detail::to_raw(H::run(OriginalOf<Method<D, R(A...)>>{original[I], method}, detail::from_raw<A>(a)...));
    }
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
///   R detour(pm::api::Original<M> original, A... args)
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
/// @tparam M The Method wrapper to hook, for example phasmo::GhostAI::ChangeState.
/// @param detour The detour, called for every call of the method on this client, on the thread the game
///   calls it on (the main thread for the game's own code).
/// @param name What the log calls the hook; null for the entry's key.
/// @param twins Twins::All (the default) to cover every body the obfuscator made, so the detour runs
///   whichever body the game calls; Twins::CanonicalOnly for the canonical body alone.
/// @return True when every body was hooked; enable it with the host contract's enable_hooks.
/// @note A detour that does not call `original` skips the game's method and every older detour on
///   that body: the game then does not do what the method does, on this client.
/// @code
/// void on_unappear(pm::api::Original<phasmo::GhostAI::UnAppear> original, void* ghost) {
///   original(ghost);  // the game's own UnAppear first
/// }
/// pm::api::hook<phasmo::GhostAI::UnAppear>(&on_unappear, "my mod: UnAppear");
/// @endcode
template <auto& M>
bool hook(typename detail::Hooked<std::remove_cvref_t<decltype(M)>>::Detour detour, const char* name,
          Twins twins = Twins::All) {
  using W = std::remove_cvref_t<decltype(M)>;
  static_assert(W::desc.kind == sdk::Kind::Method || W::desc.kind == sdk::Kind::Inflated, "hook takes a Method wrapper");
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

/// What a detour's `original` for hook<M> on M's canonical body calls, for a mod's own code outside a detour: a call
/// that goes under this mod's hook, as its detour's `original(...)` would, and over no hook made after it. It passes M's
/// MethodInfo. Before hook<M>, or when M did not resolve, it does nothing and returns R{}. After the mods stop it is invalid
/// until hook<M> succeeds again: the host has removed the hook, and calling it then calls freed code. A later hook on M by
/// another mod does not see such a call: use M's wrapper itself for the game's own entry. Main thread.
/// @tparam M The Method wrapper this mod hooked with hook<M>.
/// @return The callable for the chain under this mod's hook on the canonical body.
template <auto& M>
Original<M> below_hook() {
  using W = std::remove_cvref_t<decltype(M)>;
  const PmGameEntry* e = W::use::entry();
  return {detail::Bodies<W, detail::Hooked<W>>::original[0], e ? e->method : nullptr};
}

/// Whether a call of the game method M names is running on this thread, as the host's hook on it
/// sees it. False when M did not resolve or the host does not track it. Never logs. Main thread.
/// @tparam M The Method wrapper to ask about, for example phasmo::GhostController::Spawn.
/// @return True while a call of the method runs on this thread.
template <auto& M>
bool inside() {
  using W = std::remove_cvref_t<decltype(M)>;
  static_assert(W::desc.kind == sdk::Kind::Method || W::desc.kind == sdk::Kind::Inflated, "inside takes a Method wrapper");
  return W::use::resolved() && detail::inside(W::desc.key) > 0;
}

/// Whether every one of the wrappers W... resolved in this game build, as one test for a feature that
/// needs them all: all_resolved<phasmo::Light::type, phasmo::Light::set_range>(). Like each wrapper's
/// available(), it never logs; naming a wrapper here makes its entry required unless the entry is
/// kOptional. False before bind. Main thread.
/// @tparam W The wrappers a feature needs.
/// @return True when each of them resolved.
template <const auto&... W>
bool all_resolved() {
  return (std::remove_cvref_t<decltype(W)>::use::resolved() && ...);
}

/// Looks up, through `host`, every entry the mod's code uses, and keeps `host` for the wrappers. True
/// when every required (not kOptional) entry resolved; otherwise false, with `missing` naming the
/// keys that did not, sorted and separated by ", ". Each entry's "not available" line may be logged
/// again after it. `host` must be compatible with this game API (pm_api_compatible); pm::mod::bind
/// checks that and calls this. Main thread.
/// @param host The host contract pmmod_start was given.
/// @param missing Receives the keys of the required entries that did not resolve.
/// @return True when every required entry resolved.
bool bind(const PmHostApi* host, std::string& missing);

/// The keys of the entries the mod's code uses that are not kOptional, sorted. Points `keys` at an
/// array the mod keeps, unchanged, for as long as its DLL stays loaded, and returns its length; the
/// array is built at the first call, when every entry has registered. The mod's pmmod_requires
/// export returns this. Any thread, once the mod's DLL has loaded.
/// @param keys Receives the array of keys.
/// @return How many keys the array holds.
size_t used_required(const char* const** keys);

}  // namespace pm::api
