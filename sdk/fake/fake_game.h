#pragma once

// The fake game backend: a host contract for testing a mod's game logic offline, without the game.
// A test says what each map entry the code under test uses resolves to (a C++ function standing in
// for a game method or an icall, an offset into a test buffer standing in for a field, a test
// variable standing in for a static field, an enum number, a class pointer), then binds the game API
// to fake::host() and calls the mod's code:
//
//   pg::fake::reset();
//   pg::fake::method(pg::map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
//   std::string missing;
//   pg::api::bind(pg::fake::host(), missing);
//   phasmo::GhostAI::UnAppear(ghost);  // runs fake_unappear(ghost, nullptr)
//
// An entry the test did not give stays unresolved, as in a game build that lacks it. A fake method is
// called with a null MethodInfo as its last argument. Every fake managed object begins with its class
// pointer, as a real one does, so a test can stand an object in with a buffer whose first pointer is
// the class it gave to type(). Inheritance exists only as parent() declares it. The runtime services
// (sdk/api/runtime.h, held.h) work on the fake's own strings, arrays, lists and objects: those made by
// the game API (make_string, Type::make, make_array) or by the givers below. Single-threaded, like the
// game API.

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "entry.h"
#include "mod_api.h"

namespace pg::fake {

/// Forgets every entry, hook, barrier write, class parent, override, destroyed object and GC hold,
/// the fake's scene (children, layer names, parameters, counts) and what its scene facts kept, and
/// frees every fake managed object, so the next test starts from an empty game. Bind the game API
/// again after it (and after giving entries): a bound wrapper keeps the entry it was given.
void reset();

/// A complete host contract in which only the game functions are live: game_entry answers from the
/// entries given below, hook_entry and hook_body record the hook (see hooked) and hand back the fake
/// method as `original` (a fake method has one body: body_count is 1, and hook_body takes body 0
/// only), set_reference stores the reference and counts the write (see barrier_writes), and
/// inside_entry is always 0. The runtime services of version 2.1 work on the fake's memory:
/// string_new and string_utf8 convert between UTF-8 and UTF-16 as the host does; string_utf8,
/// array_length, array_data, list_count and list_items check that they were given a fake string,
/// array or list, and answer 0 or null otherwise, logged once per kind until reset; type_object gives
/// one fake System.Type per class; object_new gives a zeroed object of 0x200 bytes and array_new an
/// array of zeroed 16-byte elements, for a type given to type() (null otherwise); is_instance follows
/// parent(); alive is false after destroy(); virtual_method answers with an override_method() of the
/// object's class or a parent, else the declared method, and null for a null object or one not of
/// the method's owner class; gc_hold and
/// gc_release count live holds (see held); log_exception does nothing. static_get and static_set
/// copy between the caller and the storage given to static_field(), and answer false for a key not
/// given there or a size other than the one given. scene_find and animator_param run the host's own
/// scene facts (sdk/runtime/scene_facts.h) over the fake's scene: the children given to child(), the
/// parameters given to parameters() and the scene load_scene() loads. Every other function does
/// nothing and answers false or null. The pointer stays valid for the process lifetime.
const PgHostApi* host();

// Giving an entry again replaces it.

/// The method entry `d` resolves to `fn`, called as fn(args..., nullptr). `owner`, when given, is
/// its declaring class: virtual_call refuses an object not of it (or of a class derived from it).
/// Through virtual_call the declared `fn` gets a non-null fake MethodInfo as its last argument.
void method(const sdk::EntryDesc& d, void* fn, void* owner = nullptr);
/// The field entry `d` resolves to `offset` from the start of the object.
void field(const sdk::EntryDesc& d, int32_t offset);
/// The static field entry `d` resolves to `size` bytes at `storage`, which the test keeps alive.
/// With `reference`, a store through static_set counts as a GC write barrier (see barrier_writes).
void static_field(const sdk::EntryDesc& d, void* storage, size_t size, bool reference = false);
/// The icall entry `d` resolves to `fn`, called as fn(args...) with no MethodInfo.
void icall(const sdk::EntryDesc& d, void* fn);
/// The enum value entry `d` resolves to the number `v`.
void value(const sdk::EntryDesc& d, int64_t v);
/// The type entry `d` resolves to `klass`, which the fake compares with an object's first pointer.
void type(const sdk::EntryDesc& d, void* klass);

/// The class `klass` derives from `parentClass`: is_instance and virtual_method follow it.
void parent(void* klass, void* parentClass);
/// Objects of the class `klass`, or of a class derived from it, run `fn` for the method entry `d`
/// when called through virtual_call, as fn(args..., info) with a non-null fake MethodInfo. Give the
/// method entry itself with method() as well.
void override_method(const sdk::EntryDesc& d, void* klass, void* fn);
/// A new fake managed array of `length` zeroed elements of `elementSize` bytes, freed by reset.
void* array(size_t elementSize, size_t length);
/// A new fake System.Collections.Generic.List holding `items`, freed by reset.
void* list(const std::vector<void*>& items);
/// The engine object `unityObject` is destroyed: alive answers false for it until reset.
void destroy(const void* unityObject);

/// The Scene or AnimatorParam entry `d` resolves: scene_find or animator_param answers for it.
void scene_fact(const sdk::EntryDesc& d);
/// The layers the fake's LayerMask.NameToLayer knows: names[i] is layer i's name, "" for none. No
/// names at all stands for a NameToLayer that did not resolve.
void layer_names(const std::vector<std::string>& names);
/// The Layer entry `d` resolves as the host resolves it when it activates, through NameToLayer by its
/// name: to that layer's index, or not at all (logged) when no layer has the name.
void layer(const sdk::EntryDesc& d);
/// Transform.Find(root, path) answers `found` (until `found` is destroyed); any other path, null.
void child(void* root, const char* path, void* found);
/// A scene loads: the active scene's handle changes.
void load_scene();
/// `obj` is neither a Transform nor an Animator: scene_find and animator_param refuse it, saying so.
void wrong_type(const void* obj);
/// The Animator `animator` has these parameters (name and hash); any other object has none.
void parameters(const void* animator, const std::vector<std::pair<std::string, int32_t>>& list);
/// How many Transform.Find calls, and how many reads of an Animator's parameters, the fake answered
/// since reset.
int finds();
int parameter_reads();

/// The keys hooked through hook_entry or hook_body since reset, in order, once per body hooked.
std::vector<std::string> hooked();
/// How many managed references were stored through set_reference or a reference static_field (the GC
/// write barrier) since reset.
int barrier_writes();
/// How many GC handles from gc_hold are not yet released.
int held();

}  // namespace pg::fake
