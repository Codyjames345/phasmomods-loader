// Wrappers that must not compile, one per value of PM_EXPECT_COMPILE_FAIL (1 to 7), each stopped by
// its own static_assert in sdk/api/game_api.h, and typed handles used wrongly (8 to 10), each stopped
// by a conversion sdk/api/ref.h does not give. With PM_EXPECT_COMPILE_FAIL 0 the same code, written
// right, must compile, so a failing case fails for its own error and never for a broken build.
// Built by tests/compile_fail_test.cmake.
#include <cstdint>

#include "Animator.h"
#include "Camera.h"
#include "GhostAI.h"
#include "UnityObject.h"
#include "entry.h"
#include "game_api.h"
#include "ref.h"

// Functions taking handles, outside the unnamed namespace so an unused one is no warning. GhostAI derives
// from MonoBehaviour (through MonoBehaviourPun); Camera and GhostAI are unrelated.
namespace handles {
inline void* camera_of(pm::api::Ref<pm::map::Camera> camera) { return camera.raw(); }
#if PM_EXPECT_COMPILE_FAIL == 8
// A ghost is no camera: an unrelated handle is refused.
inline void* wrong() { return camera_of(pm::api::Ref<pm::map::GhostAI>{}); }
#elif PM_EXPECT_COMPILE_FAIL == 9
// A MonoBehaviour may be no ghost: down a base handle only through pm::api::as.
inline pm::api::Ref<pm::map::GhostAI> wrong(pm::api::Ref<pm::map::MonoBehaviour> behaviour) { return behaviour; }
#elif PM_EXPECT_COMPILE_FAIL == 10
// A raw reference becomes a handle only through Ref::unchecked, never on its own.
inline pm::api::Ref<pm::map::GhostAI> wrong(void* raw) { return raw; }
#else
inline void* right() { return camera_of(pm::api::Ref<pm::map::Camera>{}); }
inline pm::api::Ref<pm::map::MonoBehaviour> up(pm::api::Ref<pm::map::GhostAI> ghost) { return ghost; }
inline pm::api::Ref<pm::map::GhostAI> down(pm::api::Ref<pm::map::MonoBehaviour> behaviour) {
  return pm::api::as<pm::map::GhostAI>(behaviour);
}
inline pm::api::Ref<pm::map::GhostAI> wrap(void* raw) { return pm::api::Ref<pm::map::GhostAI>::unchecked(raw); }
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
