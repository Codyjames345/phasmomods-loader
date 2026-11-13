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
#include "GhostAI.h"
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
  fake::method(kSeeHandle, reinterpret_cast<void*>(&fake_see_handle), kGhostClass);
  fake::override_method(kSeeHandle, kDerivedClass, reinterpret_cast<void*>(&fake_see_handle_override));
  fake::field(kHolderField, 0x10);
  fake::static_field(kStaticHolder, &g_staticHolder, sizeof g_staticHolder, true);
  fake::method(kTypedHook, reinterpret_cast<void*>(&fake_typed_hook), kGhostClass);
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
  type_make_gives_a_ref();
  bridge_keeps_comparisons_unambiguous();
  return g_failures ? 1 : 0;
}
