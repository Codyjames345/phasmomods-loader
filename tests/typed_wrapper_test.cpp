// The wrapper templates (sdk/api/game_api.h) take and give typed handles (sdk/api/ref.h), and the
// transitional bridge keeps every void* caller compiling while they do (removed in part 5). The game
// is still called, and hooked, with raw pointers: the wrappers convert at their own boundary. Run:
// build\typed_wrapper_test.exe
#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include "Camera.h"
#include "Footstep.h"
#include "GhostAI.h"
#include "Props.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "held.h"
#include "ref.h"
#include "runtime.h"

// Hooks' detours hold a hooks::Guard; a mod's glue counts them, a test does here.
namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;
namespace sdk = pm::sdk;

using api::Object;
using api::Ref;

// The two field positions 0b-2b left as Object because typing them closed a map-header cycle;
// 0b-2d broke the cycles (sdk/internals/mapping.md), so each field now reads a typed handle.
static_assert(std::is_same_v<decltype(phasmo::Footstep::ghost), const api::Field<map::Footstep_ghost, Ref<map::GhostAI>>>,
              "Footstep.ghost must read a Ref<GhostAI>");
static_assert(std::is_same_v<decltype(phasmo::PhotonObjectInteract::door),
                             const api::Field<map::PhotonObjectInteract_door, Ref<map::Door>>>,
              "PhotonObjectInteract.door must read a Ref<Door>");

// Class pointers for the fake: GhostAI, a class derived from it, and Camera.
int g_ghostClass = 0, g_derivedClass = 0, g_cameraClass = 0;
void* const kGhostClass = &g_ghostClass;
void* const kDerivedClass = &g_derivedClass;
void* const kCameraClass = &g_cameraClass;

// A method whose object is a GhostAI handle, its one parameter after the object a number and its
// return a Camera handle. The fake's answer is always this object, which no test takes a handle of
// otherwise.
inline constexpr const char* kSeeParams[] = {"int"};
inline constexpr sdk::EntryDesc kSeeHandle{.key = "Test.seeHandle", .kind = sdk::Kind::Method,
                                           .flags = sdk::kOptional, .owner = "GhostAI", .name = "SeeHandle",
                                           .params = kSeeParams};
constexpr api::Method<kSeeHandle, Ref<map::Camera>(Ref<map::GhostAI>, int32_t)> seeHandle{};
int g_cameraBody = 0;  // the object the fake method answers with
void* g_sawGhost = nullptr;
int32_t g_sawCount = 0;
const void* g_sawMethod = nullptr;
void* fake_see_handle(void* ghost, int32_t count, const void* method) {
  g_sawGhost = ghost;
  g_sawCount = count;
  g_sawMethod = method;
  return &g_cameraBody;
}

// The same entry through virtual_call, and the object the override answers with. The fake's owner
// check makes virtual_call refuse a null object or one not of GhostAI.
void* g_sawVirtual = nullptr;
void* fake_see_handle_override(void* ghost, int32_t, const void*) {
  g_sawVirtual = ghost;
  return &g_cameraBody;
}

// A field of GhostAI's object holding a GhostAI handle, and a static field holding a Camera handle.
inline constexpr sdk::EntryDesc kHolderField{.key = "Test.holderField", .kind = sdk::Kind::Field,
                                             .flags = sdk::kOptional, .owner = "GhostAI", .type = "object"};
constexpr api::Field<kHolderField, Ref<map::GhostAI>> holderField{};
inline constexpr sdk::EntryDesc kStaticHolder{.key = "Test.staticHolder", .kind = sdk::Kind::Field,
                                              .flags = sdk::kStatic | sdk::kOptional, .owner = "GhostAI",
                                              .type = "object"};
constexpr api::StaticField<kStaticHolder, Ref<map::Camera>> staticHolder{};
void* g_staticHolder = nullptr;

// A managed object as far as the field below reaches: Test.holderField at 0x10.
struct FakeObject {
  alignas(8) uint8_t bytes[0x60] = {};
};

// A method to hook, with the same handle signature, and its detour: it receives handles and the
// fake method behind the hook still sees raw pointers.
inline constexpr sdk::EntryDesc kTypedHook{.key = "Test.typedHook", .kind = sdk::Kind::Method,
                                           .flags = sdk::kOptional, .owner = "GhostAI", .name = "TypedHook",
                                           .params = kSeeParams};
constexpr api::Method<kTypedHook, Ref<map::Camera>(Ref<map::GhostAI>, int32_t)> typedHook{};
void* g_hookSawGhost = nullptr;
int32_t g_hookSawCount = 0;
void* fake_typed_hook(void* ghost, int32_t count, const void*) {
  g_hookSawGhost = ghost;
  g_hookSawCount = count;
  return &g_cameraBody;
}
int g_detours = 0;
void* g_detourGhost = nullptr;
int32_t g_detourCount = 0;
Ref<map::Camera> typed_hook_detour(api::Original<typedHook> original, Ref<map::GhostAI> ghost, int32_t count) {
  ++g_detours;
  g_detourGhost = ghost.raw();
  g_detourCount = count;
  return original(ghost, count);
}

// The same handle signature hooked with a raw detour: the mod writes the game's own void* shape,
// Hooked keeps that slot, and run converts at the body boundary (transitional bridge, part 5).
inline constexpr sdk::EntryDesc kRawHook{.key = "Test.rawHook", .kind = sdk::Kind::Method,
                                         .flags = sdk::kOptional, .owner = "GhostAI", .name = "RawHook",
                                         .params = kSeeParams};
constexpr api::Method<kRawHook, Ref<map::Camera>(Ref<map::GhostAI>, int32_t)> rawHook{};
void* g_rawSawGhost = nullptr;
int32_t g_rawSawCount = 0;
void* fake_raw_hook(void* ghost, int32_t count, const void*) {
  g_rawSawGhost = ghost;
  g_rawSawCount = count;
  return &g_cameraBody;
}
int g_rawDetours = 0;
void* g_rawDetourGhost = nullptr;
int32_t g_rawDetourCount = 0;
void* raw_hook_detour(api::Original<rawHook> original, void* ghost, int32_t count) {
  ++g_rawDetours;
  g_rawDetourGhost = ghost;
  g_rawDetourCount = count;
  return original(ghost, count);  // raw arguments back into the original (the bridge, part 5)
}
int g_lateDetours = 0;
Ref<map::Camera> late_typed_detour(api::Original<rawHook> original, Ref<map::GhostAI> ghost, int32_t count) {
  ++g_lateDetours;
  return original(ghost, count);
}

// A void-returning method hooked the same raw way: run gives the raw detour the whole call, so a
// void method must not also reach the game's body behind it (a fall-through ran it twice).
inline constexpr sdk::EntryDesc kRawHookVoid{.key = "Test.rawHookVoid", .kind = sdk::Kind::Method,
                                             .flags = sdk::kOptional, .owner = "GhostAI", .name = "RawHookVoid"};
constexpr api::Method<kRawHookVoid, void(Ref<map::GhostAI>)> rawHookVoid{};
int g_rawVoidBodies = 0;
void fake_raw_hook_void(void*, const void*) { ++g_rawVoidBodies; }
int g_rawVoidDetours = 0;
void raw_hook_void_detour(api::Original<rawHookVoid> original, void* ghost) {
  ++g_rawVoidDetours;
  original(ghost);
}

// Every entry this binary calls, then bind. The optional Test entries resolve because the fake is
// given them here.
void bind_fake() {
  fake::reset();
  fake::type(map::GhostAI, kGhostClass);
  fake::type(map::Camera, kCameraClass);
  fake::parent(kDerivedClass, kGhostClass);
  g_staticHolder = nullptr;
  g_sawGhost = g_sawVirtual = g_hookSawGhost = g_detourGhost = nullptr;
  g_sawCount = g_hookSawCount = g_detourCount = 0;
  g_sawMethod = nullptr;
  g_detours = 0;
  g_rawSawGhost = g_rawDetourGhost = nullptr;
  g_rawSawCount = g_rawDetourCount = 0;
  g_rawDetours = g_lateDetours = 0;
  g_rawVoidBodies = g_rawVoidDetours = 0;
  fake::method(kSeeHandle, reinterpret_cast<void*>(&fake_see_handle), kGhostClass);
  fake::override_method(kSeeHandle, kDerivedClass, reinterpret_cast<void*>(&fake_see_handle_override));
  fake::field(kHolderField, 0x10);
  fake::static_field(kStaticHolder, &g_staticHolder, sizeof g_staticHolder, true);
  fake::method(kTypedHook, reinterpret_cast<void*>(&fake_typed_hook), kGhostClass);
  fake::method(kRawHook, reinterpret_cast<void*>(&fake_raw_hook), kGhostClass);
  fake::method(kRawHookVoid, reinterpret_cast<void*>(&fake_raw_hook_void), kGhostClass);
  std::string missing;
  CHECK(api::bind(fake::host(), missing));
  CHECK(missing.empty());
}

void* object_of(Object object) { return object.raw(); }

// A Method whose object and return are handles, called typed: the fake sees the raw pointers and the
// returned handle holds the fake's raw result.
void method_takes_and_gives_handles() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  const Ref<map::Camera> given = seeHandle(Ref<map::GhostAI>::unchecked(ghost), 7);
  CHECK(g_sawGhost == ghost && g_sawCount == 7);  // the fake saw the handle's raw pointer
  CHECK(g_sawMethod == nullptr);                  // the fake's null MethodInfo, as for any plain call
  CHECK(given && given.raw() == &g_cameraBody);   // the handle wraps the fake's raw result
  g_sawGhost = nullptr;
  const Ref<map::Camera> viaVirtual = seeHandle.virtual_call(Ref<map::GhostAI>::unchecked(ghost), 7);
  CHECK(g_sawGhost == ghost && viaVirtual.raw() == &g_cameraBody);
  g_sawVirtual = nullptr;
  void* derived[1] = {kDerivedClass};
  const Ref<map::Camera> viaOverride = seeHandle.virtual_call(Ref<map::GhostAI>::unchecked(derived), 7);
  CHECK(g_sawVirtual == derived && viaOverride.raw() == &g_cameraBody);  // the fake's override, same boundary
}

// The transitional bridge: the same Method called with void* arguments still compiles and means the
// same; so do const void* and nullptr.
void method_raw_bridge_still_compiles() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  void* object = ghost;  // the raw reference, kept in a void* as a mod's code keeps one
  const Ref<map::Camera> given = seeHandle(object, 7);
  CHECK(g_sawGhost == object && g_sawCount == 7 && given.raw() == &g_cameraBody);
  CHECK(seeHandle(static_cast<const void*>(object), 7).raw() == &g_cameraBody);  // const void* too
  g_sawGhost = reinterpret_cast<void*>(1);
  seeHandle(nullptr, 7);  // nullptr becomes a null handle, which the wrapper still passes on
  CHECK(g_sawGhost == nullptr);
  CHECK(seeHandle.virtual_call(object, 7).raw() == &g_cameraBody);
}

// A Field of a handle type reads and writes the reference; the write goes through set_reference, the
// GC write barrier, exactly as a void* field's does.
void field_of_a_handle_reads_and_writes_through_the_barrier() {
  bind_fake();
  FakeObject obj;
  int agent = 0;
  holderField.set(&obj, Ref<map::GhostAI>::unchecked(&agent));
  CHECK(fake::barrier_writes() == 1);  // the store went through set_reference
  CHECK(*reinterpret_cast<void**>(obj.bytes + 0x10) == &agent);
  const Ref<map::GhostAI> read = holderField.get(&obj);
  CHECK(read.raw() == &agent);
  CHECK(holderField.get(Ref<map::GhostAI>::unchecked(&obj)).raw() == &agent);  // a handle where an Object is taken
  CHECK(!holderField.get(nullptr));
  holderField.set(nullptr, Ref<map::GhostAI>::unchecked(&agent));  // a null object writes nothing
  CHECK(fake::barrier_writes() == 1);
}

// A StaticField of a handle type likewise reads and writes the reference through the barrier.
void static_field_of_a_handle() {
  bind_fake();
  CHECK(g_staticHolder == nullptr);
  staticHolder.set(Ref<map::Camera>::unchecked(&g_cameraBody));
  CHECK(fake::barrier_writes() == 1);  // the store counted as a GC write barrier
  CHECK(g_staticHolder == &g_cameraBody);
  CHECK(staticHolder.get().raw() == &g_cameraBody);
  Ref<map::Camera> out;
  CHECK(staticHolder.read(out) && out.raw() == &g_cameraBody);
  staticHolder.set(nullptr);
  CHECK(staticHolder.get() == nullptr && g_staticHolder == nullptr);
}

// The transitional bridge's Held path (ruling 4b): a pm::api::Held, which reads as a raw void* and
// nothing else, feeds a wrapper's Object parameter directly, reads and writes included.
void held_bridge_feeds_the_object_parameter() {
  bind_fake();
  FakeObject obj;
  *reinterpret_cast<void**>(obj.bytes) = kGhostClass;  // a GhostAI as far as the fake's is_instance reads
  int agent = 0;
  api::Held keeper = &obj;
  holderField.set(keeper, Ref<map::GhostAI>::unchecked(&agent));
  CHECK(fake::barrier_writes() == 1);  // the write still goes through set_reference
  CHECK(holderField.get(keeper).raw() == &agent);
  CHECK(phasmo::GhostAI::type.is(keeper));   // Type::is takes the Held too
  CHECK(!phasmo::Camera::type.is(keeper));
  CHECK(!holderField.get(api::Held{}));  // a Held holding nothing reads as null
}

// The bridge's Held piece for call parameters (transitional, removed in part 5): a Held given where
// a handle parameter sits crosses like the raw void* it reads as, the fake seeing the raw pointer.
// Held cannot be copied, so the raw overloads take their arguments by reference.
void held_crosses_the_raw_bridge() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  api::Held keeper = ghost;
  const Ref<map::Camera> given = seeHandle(keeper, 7);
  CHECK(g_sawGhost == ghost && g_sawCount == 7 && given.raw() == &g_cameraBody);
  g_sawGhost = nullptr;
  CHECK(seeHandle.virtual_call(keeper, 7).raw() == &g_cameraBody && g_sawGhost == ghost);
  g_sawGhost = reinterpret_cast<void*>(1);
  seeHandle(api::Held{}, 7);  // a Held holding nothing is a null handle, as its void* was
  CHECK(g_sawGhost == nullptr);
}

// The bridge's raw-reference piece for stores (transitional, removed in part 5): a handle field's
// set takes the void* or Held a mod still keeps, and the write still goes through set_reference.
void field_set_takes_a_raw_reference() {
  bind_fake();
  FakeObject obj;
  int agent = 0;
  void* kept = &agent;
  holderField.set(&obj, kept);  // void* where the field is a handle
  CHECK(fake::barrier_writes() == 1);
  CHECK(*reinterpret_cast<void**>(obj.bytes + 0x10) == &agent);
  api::Held keeper = &agent;
  holderField.set(&obj, keeper);  // a Held, too
  CHECK(fake::barrier_writes() == 2);
  holderField.set(&obj, nullptr);  // nullptr clears it
  CHECK(*reinterpret_cast<void**>(obj.bytes + 0x10) == nullptr);
  staticHolder.set(kept);  // the static field takes the same raw reference
  CHECK(g_staticHolder == &agent);
}

// The bridge's read piece (transitional, removed in part 5): a handle StaticField reads into the
// void* a mod still keeps its reference in.
void static_field_reads_into_a_void_pointer() {
  bind_fake();
  void* out = reinterpret_cast<void*>(1);
  CHECK(staticHolder.read(out) && out == nullptr);  // unset: null, and the read itself succeeded
  staticHolder.set(Ref<map::Camera>::unchecked(&g_cameraBody));
  CHECK(staticHolder.read(out) && out == &g_cameraBody);
}

// The bridge's Object piece (transitional, removed in part 5): an Object, the untyped handle,
// crosses into a handle parameter at the wrapper's raw boundary, unchecked. It exists because many
// map getters record no return type and so answer Object, while the next wrapper names a handle.
// A Ref of another type stays refused, here and in compile_fail.
void object_crosses_the_raw_bridge() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  const Object object = Object::unchecked(ghost);
  const Ref<map::Camera> given = seeHandle(object, 7);
  CHECK(g_sawGhost == ghost && g_sawCount == 7 && given.raw() == &g_cameraBody);
  g_sawGhost = nullptr;
  CHECK(seeHandle.virtual_call(object, 7).raw() == &g_cameraBody && g_sawGhost == ghost);
  FakeObject obj;
  int agent = 0;
  holderField.set(&obj, Object::unchecked(&agent));  // an Object stored into a Ref field
  CHECK(fake::barrier_writes() == 1);
  CHECK(*reinterpret_cast<void**>(obj.bytes + 0x10) == &agent);
  CHECK(holderField.get(&obj).raw() == &agent);
}

// A typed hook's detour receives handles, calls its original, and the game side still sees raw
// pointers: body_entry takes and returns them, and the wrapper converts at its boundary.
void hook_detour_gets_handles() {
  bind_fake();
  CHECK(api::hook<typedHook>(&typed_hook_detour, "typed handles"));
  CHECK(fake::hooked() == std::vector<std::string>{"Test.typedHook"});
  using W = std::remove_cvref_t<decltype(typedHook)>;
  using Entry = void* (*)(void*, int32_t, const void*);  // what the game calls: raw in, raw out
  void* ghost[1] = {kGhostClass};
  void* const result = reinterpret_cast<Entry>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0))(ghost, 3,
                                                                                                        nullptr);
  CHECK(g_detours == 1);
  CHECK(g_detourGhost == ghost && g_detourCount == 3);    // the detour got a handle for the raw call
  CHECK(g_hookSawGhost == ghost && g_hookSawCount == 3);  // its original reached the fake with raw pointers
  CHECK(result == &g_cameraBody);                         // the fake's raw result comes back untouched
}

// The transitional raw detour overload of hook<M> (removed in part 5): a detour written with the
// game's own void* shape still hooks a wrapper whose parameters and return are handles. body_entry
// converts at its boundary, the detour keeps raw pointers, and its original takes them back. A mod
// still hooks a method once: the second hook of either kind is refused, and the first keeps its slot.
void raw_detour_still_hooks_a_typed_method() {
  bind_fake();
  CHECK(api::hook<rawHook>(&raw_hook_detour, "raw handles"));
  CHECK(fake::hooked() == std::vector<std::string>{"Test.rawHook"});
  using W = std::remove_cvref_t<decltype(rawHook)>;
  using Entry = void* (*)(void*, int32_t, const void*);  // what the game calls: raw in, raw out
  void* ghost[1] = {kGhostClass};
  void* const result = reinterpret_cast<Entry>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0))(ghost, 3,
                                                                                                        nullptr);
  CHECK(g_rawDetours == 1);
  CHECK(g_rawDetourGhost == ghost && g_rawDetourCount == 3);    // the detour kept the game's raw pointers
  CHECK(g_rawSawGhost == ghost && g_rawSawCount == 3);          // its original reached the fake raw
  CHECK(result == &g_cameraBody);                              // the fake's raw result comes back untouched
  // The same method is hooked once: a typed hook over a raw one is refused, and the raw detour keeps
  // its place (the host's existing refusal, through hook_bodies).
  CHECK(!api::hook<rawHook>(&late_typed_detour, "second"));
  CHECK(reinterpret_cast<Entry>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0))(ghost, 4, nullptr) ==
        &g_cameraBody);
  CHECK(g_rawDetours == 2 && g_lateDetours == 0);
}

// A void method with a raw detour: the body runs exactly once, through the detour's original.
void a_void_raw_detour_runs_the_body_once() {
  bind_fake();
  CHECK(api::hook<rawHookVoid>(&raw_hook_void_detour, "void raw handles"));
  using WV = std::remove_cvref_t<decltype(rawHookVoid)>;
  using Entry = void (*)(void*, const void*);
  void* ghost[1] = {kGhostClass};
  reinterpret_cast<Entry>(api::detail::Bodies<WV, api::detail::Hooked<WV>>::entry(0))(ghost, nullptr);
  CHECK(g_rawVoidDetours == 1 && g_rawVoidBodies == 1);
}

// OriginalOf's raw overload, the same bridge rule as a wrapper's call (removed in part 5): an
// original called with void* arguments where the wrapper's parameters are handles still compiles and
// means the same. below_hook hands one out to call outside a detour.
void original_takes_raw_arguments() {
  bind_fake();
  CHECK(api::hook<typedHook>(&typed_hook_detour, "typed handles"));
  const api::Original<typedHook> under = api::below_hook<typedHook>();
  void* ghost[1] = {kGhostClass};
  void* object = ghost;
  const Ref<map::Camera> given = under(object, 4);
  CHECK(g_detours == 0);                                        // below_hook runs under this mod's detour
  CHECK(g_hookSawGhost == ghost && g_hookSawCount == 4);        // the fake saw the raw reference
  CHECK(given.raw() == &g_cameraBody);
  CHECK(under(nullptr, 5).raw() == &g_cameraBody);              // nullptr for a handle parameter, too
  const void* constRaw = object;
  CHECK(under(constRaw, 6).raw() == &g_cameraBody);             // and const void*
}

// The bridge's Held piece (transitional, removed in part 5): a wrapper's handle answer goes straight
// into a Held, as the void* it used to give did: a Ref through the Object it converts to, an Object
// directly, with assignment as well as construction. Assigning the object already held keeps its
// GC handle, as the void* assignment does.
void held_takes_a_ref() {
  bind_fake();
  const Ref<map::GhostAI> made = phasmo::GhostAI::type.make();
  api::Held keeper = made;  // a handle, where the Held used to take a void*
  CHECK(keeper && phasmo::GhostAI::type.is(keeper));
  CHECK(fake::held(made.raw()));
  api::Held reassigned;
  reassigned = made;  // assignment from a handle
  CHECK(reassigned && reassigned.get() == made.raw());
  const Object asObject = made;  // the Ref converts to Object once
  reassigned = asObject;         // and from an Object
  CHECK(reassigned.get() == made.raw());
  const int holdsBefore = fake::held();
  reassigned = asObject;  // the object it already holds: its handle stays
  CHECK(fake::held() == holdsBefore);
  reassigned = nullptr;
  CHECK(!reassigned && fake::held() == holdsBefore - 1);
  static_assert(std::is_convertible_v<Ref<map::GhostAI>, api::Held>);      // transitional (part 5)
  static_assert(std::is_convertible_v<Object, api::Held>);                // transitional (part 5)
}

// Type<D>::make() gives a Ref<D>; make_array() gives an Object; type_object() and klass() stay raw
// void*s (a System.Type is untyped either way). The bridge keeps callers that kept these as void*
// compiling.
void type_make_gives_a_ref() {
  bind_fake();
  const Ref<map::GhostAI> made = phasmo::GhostAI::type.make();
  CHECK(made && made.raw() != nullptr);
  CHECK(phasmo::GhostAI::type.is(made));       // an Object parameter takes the handle
  CHECK(!phasmo::Camera::type.is(made));
  void* bridge = phasmo::GhostAI::type.make();  // the same call where a caller kept a void*
  CHECK(bridge != nullptr && phasmo::GhostAI::type.is(bridge));
  void* typeObject = phasmo::GhostAI::type.type_object();  // raw on raw: no handle around a System.Type
  CHECK(typeObject != nullptr && typeObject == phasmo::GhostAI::type.type_object());
  CHECK(api::RefArray{phasmo::Camera::type.make_array(4)}.size() == 4);
  void* copyFrom = phasmo::GhostAI::type.make();
  void* copyTo = phasmo::GhostAI::type.make();
  CHECK(phasmo::GhostAI::type.copy_settings(copyTo, copyFrom));  // two void*s still fit the Object pair
  CHECK(phasmo::GhostAI::type.klass() == kGhostClass);           // metadata: never a handle
}

// The bridge adds no ambiguity: none of these comparisons and conversions changed meaning.
void bridge_keeps_comparisons_unambiguous() {
  void* ghost[1] = {kGhostClass};
  const Ref<map::GhostAI> handle = Ref<map::GhostAI>::unchecked(ghost);
  const Ref<map::GhostAI> same = Ref<map::GhostAI>::unchecked(ghost);
  const Ref<map::MonoBehaviour> base = handle;  // the map says GhostAI derives from MonoBehaviour
  const Ref<map::Camera> unrelated = Ref<map::Camera>::unchecked(ghost);
  const Object object = Object::unchecked(ghost);
  if (handle) CHECK(true); else CHECK(false);
  if (!Ref<map::Camera>{}) CHECK(true); else CHECK(false);
  if (object) CHECK(true); else CHECK(false);
  CHECK(!Object{});
  CHECK(handle != nullptr && nullptr != handle);      // comparisons to null, both ways, unchanged
  CHECK(object != nullptr && nullptr != object);
  CHECK(Ref<map::GhostAI>{} == nullptr && Object{} == nullptr);
  CHECK(handle == same && handle != Ref<map::GhostAI>{});
  CHECK(handle == base && base == handle);            // a handle against another of its base's
  CHECK(handle == unrelated && unrelated == handle);  // handles of unrelated types, same object
  CHECK(handle == object && object == handle);        // a handle against an Object, both ways
  CHECK(handle != Object{});
  // the bridge's conversions, in the directions existing code uses them
  void* kept = handle;
  void* keptObject = object;
  CHECK(kept == ghost && keptObject == ghost);
  CHECK(object_of(handle) == ghost && object_of(object) == ghost);
  const Object fromRaw = static_cast<void*>(ghost);  // the bridge's constructor: a void* becomes an Object
  CHECK(fromRaw.raw() == ghost);
  // and the types the bridge must not open: void* never becomes a Ref on its own
  static_assert(!std::is_convertible_v<void*, Ref<map::GhostAI>>);
  static_assert(!std::is_convertible_v<const void*, Ref<map::GhostAI>>);
  static_assert(std::is_convertible_v<void*, Object>);      // transitional: removed in part 5
  static_assert(std::is_convertible_v<const void*, Object>);  // transitional: removed in part 5
  static_assert(std::is_convertible_v<api::Held, Object>);  // transitional too (ruling 4b): classes that read as void*
}

}  // namespace

int main() {
  method_takes_and_gives_handles();
  method_raw_bridge_still_compiles();
  field_of_a_handle_reads_and_writes_through_the_barrier();
  static_field_of_a_handle();
  held_bridge_feeds_the_object_parameter();
  hook_detour_gets_handles();
  raw_detour_still_hooks_a_typed_method();
  a_void_raw_detour_runs_the_body_once();
  original_takes_raw_arguments();
  held_takes_a_ref();
  held_crosses_the_raw_bridge();
  field_set_takes_a_raw_reference();
  static_field_reads_into_a_void_pointer();
  object_crosses_the_raw_bridge();
  type_make_gives_a_ref();
  bridge_keeps_comparisons_unambiguous();
  return g_failures ? 1 : 0;
}
