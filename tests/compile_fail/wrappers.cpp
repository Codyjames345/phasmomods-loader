// Wrappers that must not compile, one per value of PM_EXPECT_COMPILE_FAIL (1 to 7), each stopped by
// its own static_assert in sdk/api/game_api.h; typed handles used wrongly (8 to 10), each stopped by
// a conversion sdk/api/ref.h does not give; a handle-taking wrapper's arguments used wrongly (11 to
// 13), where the typed call and the transitional raw bridge (removed in part 5) each refuse them; a
// field of a pointer type (14), still refused by game_api.h's static_assert; a hook's detour naming
// the wrong handle (15), which neither hook<> overload takes; a HeldRef of an unrelated type given
// for a handle parameter (16); and a Ref of an unrelated type stored into a handle field (17), both
// refused by the bridge's exact-name rule. With PM_EXPECT_COMPILE_FAIL 0 the same code, written
// right, must compile, so a failing case fails for its own error and never for a broken build.
// Built by tests/compile_fail_test.cmake.
#include <cstdint>

#include "Animator.h"
#include "Camera.h"
#include "GhostAI.h"
#include "UnityObject.h"
#include "entry.h"
#include "game_api.h"
#include "held.h"
#include "ref.h"

// Functions taking handles, outside the unnamed namespace so an unused one is no warning. GhostAI derives
// from MonoBehaviour (through MonoBehaviourPun); Camera and GhostAI are unrelated.
namespace handles {
inline void* camera_of(pm::api::Ref<pm::map::Camera> camera) { return camera.raw(); }
// Wrappers whose parameters and return are handles, for the bridge cases 11 to 14. An instance
// method's params are its parameters after the object: SeeMethod takes one and TwoHandles two.
inline constexpr const char* kSeeParams[] = {"int"};
inline constexpr const char* kTwoParams[] = {"object"};
inline constexpr pm::sdk::EntryDesc kSeeMethod{.key = "Test.SeeMethod", .kind = pm::sdk::Kind::Method,
                                               .owner = "GhostAI", .name = "SeeMethod", .params = kSeeParams};
inline constexpr pm::sdk::EntryDesc kTwoHandles{.key = "Test.TwoHandles", .kind = pm::sdk::Kind::Method,
                                                .owner = "GhostAI", .name = "TwoHandles", .params = kTwoParams};
inline constexpr pm::sdk::EntryDesc kHolderField{.key = "Test.holderField", .kind = pm::sdk::Kind::Field,
                                                 .owner = "GhostAI", .type = "object"};
inline constexpr pm::api::Method<kSeeMethod, pm::api::Ref<pm::map::Camera>(pm::api::Ref<pm::map::GhostAI>, int32_t)>
    seeHandle{};
inline constexpr pm::api::Method<kTwoHandles, void(pm::api::Ref<pm::map::GhostAI>, pm::api::Ref<pm::map::Camera>)>
    twoHandles{};
#if PM_EXPECT_COMPILE_FAIL == 8
// A ghost is no camera: an unrelated handle is refused.
inline void* wrong() { return camera_of(pm::api::Ref<pm::map::GhostAI>{}); }
#elif PM_EXPECT_COMPILE_FAIL == 9
// A MonoBehaviour may be no ghost: down a base handle only through pm::api::as.
inline pm::api::Ref<pm::map::GhostAI> wrong(pm::api::Ref<pm::map::MonoBehaviour> behaviour) { return behaviour; }
#elif PM_EXPECT_COMPILE_FAIL == 10
// A raw reference becomes a handle only through Ref::unchecked, never on its own.
inline pm::api::Ref<pm::map::GhostAI> wrong(void* raw) { return raw; }
#elif PM_EXPECT_COMPILE_FAIL == 11
// A handle-taking Method's object given a handle of an unrelated type: the typed call refuses it.
inline void* wrongObject(pm::api::Ref<pm::map::Camera> camera) { return seeHandle(camera, 1).raw(); }
#elif PM_EXPECT_COMPILE_FAIL == 12
// Through the raw bridge too: void* for one handle parameter goes, an unrelated handle for the
// other does not.
inline void* wrongBridge(void* object, pm::api::Ref<pm::map::GhostAI> ghost) { twoHandles(object, ghost); }
#elif PM_EXPECT_COMPILE_FAIL == 13
// A raw reference still becomes a handle only through Ref::unchecked: passing one built straight
// from a void* to a handle parameter is the same refusal.
inline void* wrongWrap(void* object, void* camera) {
  twoHandles(object, pm::api::Ref<pm::map::GhostAI>(camera));
}
#elif PM_EXPECT_COMPILE_FAIL == 14
// A field's parameter is still refused: a managed reference is a handle or void*, never int*.
constexpr pm::api::Field<kHolderField, int*> badField{};
#elif PM_EXPECT_COMPILE_FAIL == 15
// A hook's detour written with a handle of the wrong type: seeHandle's first parameter is a GhostAI
// handle, and neither hook<> overload takes a detour naming a Camera one there.
inline pm::api::Ref<pm::map::Camera> wrongDetour(pm::api::Original<seeHandle>, pm::api::Ref<pm::map::Camera>,
                                                 int32_t) {
  return pm::api::Ref<pm::map::Camera>{};
}
inline bool wrongHook() { return pm::api::hook<seeHandle>(&wrongDetour, "wrong detour"); }
#elif PM_EXPECT_COMPILE_FAIL == 16
// A HeldRef of a type the parameter does not name: it converts to no handle of the parameter's
// type, and the bridge takes only raw references and a plain Held, never a HeldRef.
inline void wrongHeldRef() { seeHandle(pm::api::HeldRef<pm::map::Camera>{}, 1); }
#elif PM_EXPECT_COMPILE_FAIL == 17
// A handle field's set takes its own handle or a raw reference; a Ref of an unrelated type is
// refused typed and through the bridge alike.
inline constexpr pm::api::Field<kHolderField, pm::api::Ref<pm::map::GhostAI>> holderField{};
inline void wrongSet(void* object) { holderField.set(object, pm::api::Ref<pm::map::Camera>{}); }
#else
inline void* right() { return camera_of(pm::api::Ref<pm::map::Camera>{}); }
inline pm::api::Ref<pm::map::MonoBehaviour> up(pm::api::Ref<pm::map::GhostAI> ghost) { return ghost; }
inline pm::api::Ref<pm::map::GhostAI> down(pm::api::Ref<pm::map::MonoBehaviour> behaviour) {
  return pm::api::as<pm::map::GhostAI>(behaviour);
}
inline pm::api::Ref<pm::map::GhostAI> wrap(void* raw) { return pm::api::Ref<pm::map::GhostAI>::unchecked(raw); }
// The handle-taking wrappers written right: all handles through the typed call, all raw references
// through the bridge, and the field taking a handle.
inline constexpr pm::api::Field<kHolderField, pm::api::Ref<pm::map::GhostAI>> holderField{};
inline pm::api::Ref<pm::map::GhostAI> readHolder(void* object) { return holderField.get(object); }
inline void* rightTyped(pm::api::Ref<pm::map::GhostAI> ghost, pm::api::Ref<pm::map::Camera> camera) {
  twoHandles(ghost, camera);
  return seeHandle(ghost, 1).raw();
}
inline void* rightRaw(void* object, void* camera) {
  twoHandles(object, camera);
  return seeHandle(object, 1).raw();
}
// A Held, which reads as a raw void*, reaches an Object parameter through the bridge's one
// class-type constructor (ruling 4b).
inline pm::api::Object heldAsObject(pm::api::Held held) { return held; }
// A hook's detours, written right: one typed, one in the game's own raw shape (the transitional hook
// overload, removed in part 5), whose original takes the raw arguments back.
inline pm::api::Ref<pm::map::Camera> rightTypedDetour(pm::api::Original<seeHandle> original,
                                                      pm::api::Ref<pm::map::GhostAI> ghost, int32_t count) {
  return original(ghost, count);
}
inline void* rightRawDetour(pm::api::Original<seeHandle> original, void* ghost, int32_t count) {
  return original(ghost, count);
}
inline bool rightHooks() {
  return pm::api::hook<seeHandle>(&rightTypedDetour, "typed detour") &&
         pm::api::hook<seeHandle>(&rightRawDetour, "raw detour");
}
// A Held takes a handle straight in, as it took the void* (transitional, removed in part 5).
inline bool heldFromHandle(pm::api::Ref<pm::map::GhostAI> ghost) {
  pm::api::Held keeper = ghost;
  const pm::api::Object asObject = ghost;  // the Ref converts to Object once
  keeper = asObject;
  return keeper != nullptr;
}
// The bridge's pieces for code that keeps raw references (transitional, removed in part 5): a Held
// given where a handle parameter sits, and raw references given where a handle field takes its value.
inline void rightHeldArg(pm::api::Held held) { seeHandle(held, 1); }
inline void rightSetRaw(void* object, void* value) { holderField.set(object, value); }
#endif
}  // namespace handles

namespace {

using pm::api::EnumValue;
using pm::api::Field;
using pm::api::Icall;
using pm::api::Method;
using pm::api::StaticField;
using pm::api::Type;
namespace map = pm::map;
namespace sdk = pm::sdk;

inline constexpr const char* kIntParam[] = {"int"};
/// A static method taking an int: no object comes first.
inline constexpr sdk::EntryDesc kStaticMethod{.key = "Test.StaticMethod", .kind = sdk::Kind::Method,
                                              .flags = sdk::kStatic, .owner = "GhostAI", .name = "StaticMethod",
                                              .params = kIntParam};
/// A static int field.
inline constexpr sdk::EntryDesc kStaticInt{.key = "Test.staticInt", .kind = sdk::Kind::Field, .flags = sdk::kStatic,
                                           .owner = "GhostAI", .type = "int"};

#if PM_EXPECT_COMPILE_FAIL == 1
// LookAtPlayer takes a Player after the object: one argument short.
constexpr Method<map::GhostAI_LookAtPlayer, void(void*)> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 2
// SetBoolID takes an id and a bool after the animator: one argument too many.
constexpr Icall<map::Animator_SetBoolID, void(void*, int32_t, bool, bool)> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 3
// A static method has no object to take first.
constexpr Method<kStaticMethod, void(void*, int32_t)> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 4
// isHunting is a bool, one byte, not four.
constexpr Field<map::GhostAI_isHunting, int32_t> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 5
// The static field is an int, four bytes, not one.
constexpr StaticField<kStaticInt, bool> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 6
// GhostAI is a type, not an enum value.
constexpr EnumValue<map::GhostAI> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 7
// UnAppear is a method, not a type.
constexpr Type<map::GhostAI_UnAppear> wrapper{};
#elif PM_EXPECT_COMPILE_FAIL == 0
constexpr Method<map::GhostAI_LookAtPlayer, void(void*, void*)> lookAtPlayer{};
constexpr Icall<map::Animator_SetBoolID, void(void*, int32_t, bool)> setBoolID{};
constexpr Method<kStaticMethod, void(int32_t)> staticMethod{};
constexpr Field<map::GhostAI_isHunting, bool> isHunting{};
constexpr StaticField<kStaticInt, int32_t> staticInt{};
constexpr EnumValue<map::GhostState_idle> idle{};
constexpr Type<map::GhostAI> ghostAI{};
#endif

}  // namespace
