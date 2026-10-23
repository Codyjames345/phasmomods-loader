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
// When an entry did not resolve anyway (an optional one, or the mod was not bound), nothing jumps to
// null: a call, a read or a write logs "<key> is not available in this game build" once and returns
// the type's default value (0, false, null) or does nothing.
//
// Threading: everything here runs on Unity's main thread, where the host calls pgmod_start and
// pgmod_tick, except the registration a wrapper makes while the mod's DLL loads.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#include "entry.h"
#include "mod_api.h"

namespace pg::api {

namespace detail {

// One game entry the mod's code uses, and what the host gave for it at bind.
struct Slot {
  const sdk::EntryDesc* desc;
  const PgGameEntry* entry;  // null before bind and when the entry did not resolve
  bool reported;             // its "not available" line was logged since the last bind
};

// Adds a used entry to the mod's list. Runs while the mod's DLL loads; always true.
bool register_use(Slot* slot);
// The host contract given to bind, or null before it.
const PgHostApi* host();
// Logs "<key> is not available in this game build", once per bind.
void report_unavailable(Slot& slot);
// Whether the managed object `obj` (non-null) is of the class `klass` or derives from it.
bool is_instance(const void* obj, void* klass);
// The host contract's hook_entry, or false (logged) before bind.
bool hook(const char* key, void* detour, void** original, const char* name, bool allTwins);
// The host contract's inside_entry, or 0 before bind.
int32_t inside(const char* key);

}  // namespace detail

/// The use of the map entry D by the mod's code. Odr-using `registered` adds D to what the mod
/// requires (unless D is kOptional); the accessors below do it only from the members a mod calls.
template <const sdk::EntryDesc& D>
struct Use {
  static inline detail::Slot slot{&D, nullptr, false};
  static inline const bool registered = detail::register_use(&slot);

  /// The resolved entry, or null when it did not resolve (logged once per bind). Main thread.
  static const PgGameEntry* entry() {
    if (!resolved()) detail::report_unavailable(slot);
    return slot.entry;
  }
  /// Whether the entry resolved, without logging. Main thread.
  static bool resolved() {
    static_cast<void>(registered);
    return slot.entry != nullptr;
  }
};

template <const sdk::EntryDesc& D, class Sig>
struct Method;

/// A game method, called as R(A...). For an instance method the first parameter is the object
/// (`this`). A struct parameter whose size is not 1, 2, 4 or 8 bytes is passed as a pointer to it, as the x64
/// calling convention does. Main thread.
template <const sdk::EntryDesc& D, class R, class... A>
struct Method<D, R(A...)> {
  static_assert(D.kind == sdk::Kind::Method, "Method wraps a map entry of kind Method");
  static constexpr const sdk::EntryDesc& desc = D;
  using use = Use<D>;

  /// Calls the method's code with the arguments and its MethodInfo, as the game does. Unresolved:
  /// logs once and returns R{} without calling anything.
  template <int = 0>
  R operator()(A... a) const {
    const PgGameEntry* e = use::entry();
    if (!e || !e->code) {
      if constexpr (std::is_void_v<R>) return;
      else return R{};
    }
    return reinterpret_cast<R (*)(A..., const void*)>(e->code)(a..., e->method);
  }

  /// Whether the method resolved in this game build. Never logs.
  template <int = 0>
  bool available() const {
    return use::resolved();
  }
};

/// An instance field of type T. T is a plain value (bool, int32_t, float, a struct copied whole) or
/// void* for a managed reference. Reads and writes go to the resolved offset in the object. Main
/// thread.
template <const sdk::EntryDesc& D, class T>
struct Field {
  static_assert(D.kind == sdk::Kind::Field, "Field wraps a map entry of kind Field");
  static_assert(!(D.flags & sdk::kStatic), "static fields are not part of the game API yet");
  static_assert(std::is_trivially_copyable_v<T>, "a field is read and written as plain bytes");
  static_assert(!std::is_pointer_v<T> || std::is_same_v<T, void*>, "a managed reference field is Field<D, void*>");
  static constexpr const sdk::EntryDesc& desc = D;
  using use = Use<D>;

  /// The field's value in the managed object `obj`. Unresolved (logged once) or a null `obj`: T{}.
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

/// One value of a game enum, by its readable name. Main thread.
template <const sdk::EntryDesc& D>
struct EnumValue {
  static_assert(D.kind == sdk::Kind::EnumValue, "EnumValue wraps a map entry of kind EnumValue");
  static constexpr const sdk::EntryDesc& desc = D;
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
  static constexpr const sdk::EntryDesc& desc = D;
  using use = Use<D>;

  /// The type's Il2CppClass*: runtime metadata, never a managed object. Unresolved: logs once and
  /// returns null.
  template <int = 0>
  void* klass() const {
    const PgGameEntry* e = use::entry();
    return e ? e->klass : nullptr;
  }

  /// Whether the managed object `obj` is of this type or of a type derived from it. False for a
  /// null `obj`, and when the type did not resolve (logged once).
  template <int = 0>
  bool is(const void* obj) const {
    const PgGameEntry* e = use::entry();
    return e && e->klass && obj && detail::is_instance(obj, e->klass);
  }
};

/// Which bodies of a method a hook covers: every twin the obfuscator made (so the detour runs
/// whichever body the game calls), or only the canonical body.
enum class Twins { All, CanonicalOnly };

/// Hooks the game method M names (a Method wrapper, e.g. phasmo::GhostAI::ChangeState) through the
/// host's single MinHook instance. The hook is created, not enabled: the host contract's enable_hooks
/// enables it. `original` receives what reaches the canonical body; `name` names the hook in the log
/// (the entry's key when null). False when M did not resolve (logged once) or the host could not
/// hook it (the host logs why). Main thread.
template <auto& M>
bool hook(void* detour, void** original, const char* name, Twins twins = Twins::All) {
  using W = std::remove_cvref_t<decltype(M)>;
  static_assert(W::desc.kind == sdk::Kind::Method, "hook takes a Method wrapper");
  if (!W::use::entry()) return false;
  return detail::hook(W::desc.key, detour, original, name ? name : W::desc.key, twins == Twins::All);
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
/// array kept until the next call and returns its length. The mod's pgmod_requires export returns
/// this. Main thread, once the mod's DLL has loaded.
size_t used_required(const char* const** keys);

}  // namespace pg::api
