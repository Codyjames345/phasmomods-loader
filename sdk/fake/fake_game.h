#pragma once

// The fake game backend: a host contract for testing a mod's game logic offline, without the game.
// A test says what each map entry the code under test uses resolves to (a C++ function standing in
// for a game method or an icall, an offset into a test buffer standing in for a field, a test
// variable standing in for a static field, an enum number, a class pointer), then binds the game API
// to fake::host() and calls the mod's code:
//
//   pm::fake::reset();
//   pm::fake::method(pm::map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
//   std::string missing;
//   pm::api::bind(pm::fake::host(), missing);
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

#include "content_acting.h"
#include "content_registry.h"
#include "entry.h"
#include "mod_api.h"

namespace pm::fake {

/// Forgets every entry, hook, barrier write, class parent, override, destroyed object and GC hold,
/// the fake's scene (children, layer names, parameters, counts) and what its scene facts kept, and
/// frees every fake managed object, so the next test starts from an empty game. Bind the game API
/// again after it (and after giving entries): a bound wrapper keeps the entry it was given.
void reset();

/// A complete host contract in which only the game functions are live: game_entry answers from the
/// entries given below (never a native site, as the host's never does), hook_entry and hook_body record the hook (see hooked) and hand back the fake
/// method as `original` (a fake method has one body: body_count is 1, and hook_body takes body 0
/// only) and refuse a detour already hooked on that method, as the host's hook table does,
/// set_reference stores the reference and counts the write (see barrier_writes), and
/// inside_entry answers what set_inside says, else 0. The runtime services of version 2.1 work on the fake's memory:
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
/// parameters given to parameters() and the scene load_scene() loads. The content functions of
/// version 2.2 run the host's content registry over the room given to room() (see below; registration
/// is open from reset until commit_content), content_acting_as, content_identity and
/// content_evidence_look answer from the host's acting-as dispatch as contract_ghost() drives it, as
/// do the controlled ghost's functions after fields_of_type (content_ability_text in English),
/// content_player_controlled answers what player_controlled() set, content_roll_taken what roll_taken()
/// set, room_agrees what room_agrees() set,
/// and journal_lines records lines in
/// the registry. list_add adds to a fake List of references; copy_settings and fields_of_type read the
/// fields declare_fields() gave; steer_agent records the ask (see steer_asks). Every other function does nothing and answers false or null. The pointer stays valid
/// for the process lifetime.
const PmHostApi* host();

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
/// How many calls of the method `d` inside_entry says are running on this thread (pm::api::inside).
void set_inside(const sdk::EntryDesc& d, int32_t depth);
/// What abandon_ghost_spawn throws, for a test's own catch around the spawn; it does nothing until spawn_running(true).
struct SpawnAbandoned {};
void spawn_running(bool running);
int spawn_abandons();
/// `dict` holds these entries, as dictionary_int_entries reads them (free slots already left out).
void dictionary(const void* dict, std::vector<PmIntEntry> entries);
/// The mods the host contract's mod_started answers true for, as running on this client; none until this is called.
void started_mods(std::vector<std::string> ids);
/// Every message the host contract's show_message was asked for since the last reset, as (title, body), in order.
const std::vector<std::pair<std::string, std::string>>& messages();
/// The native site entry `d` resolves to `code`. As in the host, the contract's game_entry never gives it to a mod:
/// host_entry does, as pm::api::host_only::game_entry does in the host. A mod reaches steering only through
/// steer_agent (see steer_asks).
void native_site(const sdk::EntryDesc& d, void* code);
/// The entry `d` is unresolved again, as in a game build that lacks it, whatever gave it. Bind again after it.
void forget(const sdk::EntryDesc& d);
/// Every entry given, native sites included, as the host's own code sees them (pm::api::host_only::game_entry); a
/// host test defines that function over this. Null for a key not given.
const PmGameEntry* host_entry(const char* key);

/// A steering ask a mod made through the host contract's steer_agent.
struct SteerAsk {
  void* agent;
  float dx, dz;
};
/// Every ask since reset that named an agent, oldest first: phasmo::NavMeshAgent::steer as the mod called it, whatever
/// the direction. A null agent only asks whether the host can steer, and is not listed.
const std::vector<SteerAsk>& steer_asks();
/// steer_agent answers false from now on and lists nothing, as the host does in a game build it cannot steer in, until
/// reset.
void refuse_steering();

/// The class `klass` derives from `parentClass`: is_instance and virtual_method follow it.
void parent(void* klass, void* parentClass);
/// A field a class declares itself, as the host contract's copy_settings and fields_of_type read the
/// runtime's metadata: `size` bytes at `offset`. `publicValue`: a public field of a value type, which
/// copy_settings copies whole, storing each reference at `references` (offsets within the field)
/// through the barrier; one of `size` 0 stands for a field the host cannot read, and copy_settings
/// then refuses the whole copy, copying nothing, as the host does. `type`: the class of its type (with
/// `array`, of its element), which fields_of_type compares.
struct DeclaredField {
  int32_t offset;
  size_t size;
  bool publicValue = false;
  void* type = nullptr;
  bool array = false;
  std::vector<size_t> references = {};
};
/// The fields the class `klass` declares itself (not those it inherits), for copy_settings and
/// fields_of_type; none until given.
void declare_fields(void* klass, std::vector<DeclaredField> fields);
/// Objects of the class `klass`, or of a class derived from it, run `fn` for the method entry `d`
/// when called through virtual_call, as fn(args..., info) with a non-null fake MethodInfo. Give the
/// method entry itself with method() as well.
void override_method(const sdk::EntryDesc& d, void* klass, void* fn);
/// A new fake managed array of `length` zeroed elements of `elementSize` bytes, freed by reset.
void* array(size_t elementSize, size_t length);
/// A new fake System.Collections.Generic.HashSet of `count` items, as collection_count counts it, freed
/// by reset.
void* collection(int32_t count);
/// A new fake List of `count` zeroed structs of `elementSize` bytes each (a List<T> of a game struct),
/// whose backing array array_element_size sizes, freed by reset.
void* struct_list(size_t elementSize, size_t count);
/// A new fake System.Collections.Generic.List holding `items`, freed by reset.
void* list(const std::vector<void*>& items);
/// A new fake List of 32-bit values (a List<GhostType>, say) holding `values`, freed by reset.
void* value_list(const std::vector<int32_t>& values);
/// What a value_list holds now, in order (empty for any other list).
std::vector<int32_t> list_values(const void* list);
/// What a list of references holds now, in order (empty for a value_list).
std::vector<void*> list_items_of(const void* list);
/// The game's List<T>.Insert, RemoveAt and Add (of a 32-bit value) on a fake list, as the host's
/// content code calls them: false for an index out of range or a list of the other kind.
bool list_insert(void* list, int32_t index, void* item);
bool list_remove_at(void* list, int32_t index);
bool list_add_value(void* list, int32_t value);
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
/// The ShaderProperty entry `d` resolves as the host resolves it when it activates, to its name's id
/// through the fake's Shader.PropertyToID. An entry not given stays unresolved.
void shader_property(const sdk::EntryDesc& d);
/// What the fake's Shader.PropertyToID answers for `name`: the same id for the same name, each name
/// its own, until reset().
int32_t property_id(const char* name);
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

// Content (contract 2.2). The fake's content functions run the host's own content registry
// (sdk/runtime/content_registry.h). reset forgets its content and its room, gives it the audited
// build's enums (game_enums), and opens registration as while a mod starts; commit_content then settles the content as the host does once every mod has
// started. A test fakes the room's other players with registries of their own and delivers their
// manifests here, as the host's channel would.

/// Closes registration and settles the content (Registry::commit): nothing is in play before it.
void commit_content();
/// This client is `localActor` (0: outside a private room) in a room of `actors` at the time `now`
/// in milliseconds, as the host sees it every frame.
void room(int localActor, const std::vector<int>& actors, uint64_t now = 0, int master = content::Registry::kLowestActor);
/// Where content stands in the room, as the host's content_state answers.
content::State content_state();
/// The game's GhostType and EvidenceType enums, every value with its name, and each enum's none value,
/// as the host reads them from the map at startup: custom numbers begin one past each enum's largest
/// value and the vanilla ids come from the names. reset gives the audited build's (below). Values that
/// cannot be read keep content off, as in the host. Give them before registering content.
void game_enums(const content::EnumValues& ghostTypes, const content::EnumValues& evidenceTypes, int64_t noGhost,
                int64_t noEvidence);
/// The audited build's enums: GhostType Spirit 0 to Deildegast 30 with None at 24, and EvidenceType None
/// 0 to DotsProjector 7. A test adds to them to stand for a game update.
content::EnumValues audited_ghost_types();
content::EnumValues audited_evidence();
inline constexpr int64_t kAuditedNoGhost = 24;
inline constexpr int64_t kAuditedNoEvidence = 0;
/// A message on the host's channel from the client of `sender` (another client's content manifest).
void host_message(int sender, const uint8_t* data, size_t size);
/// The messages the host would send now on its channel: this client's manifest when it is due.
std::vector<std::vector<uint8_t>> host_messages();
/// Whether a player controls the contract's ghost, as content_player_controlled answers (false until
/// set, and after reset).
void player_controlled(bool controlled);
/// Whether a player takes the actions of the idle roll running now, as content_roll_taken answers (false until
/// set, and after reset).
void roll_taken(bool taken);
/// Whether this client's mods match the host's, as room_agrees answers (false until set, and after reset).
void room_agrees(bool agrees);

// Ghost types (contract 2.2): ghost_rules records rules in the registry, and content_acting_as and
// content_identity answer from the host's own acting-as dispatch (sdk/runtime/content_acting.h),
// which a test drives as the host does every frame.

/// The fake's content registry and acting-as dispatch, for code that the host runs over them. Nothing
/// is gated on this client's match with the host until a test gives one (content_registry().use_agreement).
content::Registry& content_registry();
content::Acting& acting();
/// The contract's ghost as this client's GhostInfo has it (identity -1: none), and whether this client
/// is the master: the acting-as type follows, and the rules switch to it.
void contract_ghost(int32_t identity, int32_t behaviour, bool master = true);
/// Runs the tick of the rules running now, as the host does every frame.
void tick_rules();

/// The keys hooked through hook_entry or hook_body since reset, in order, once per body hooked.
std::vector<std::string> hooked();
/// How many managed references were stored through set_reference or a reference static_field (the GC
/// write barrier) since reset.
int barrier_writes();
/// How many GC handles from gc_hold are not yet released.
int held();
/// Whether a GC handle from gc_hold, not yet released, holds `object`.
bool held(const void* object);

}  // namespace pm::fake
