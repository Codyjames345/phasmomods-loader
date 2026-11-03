// Wrappers that must not compile, one per value of PM_EXPECT_COMPILE_FAIL (1 to 7), each stopped by
// its own static_assert in sdk/api/game_api.h. With PM_EXPECT_COMPILE_FAIL 0 the same wrappers,
// written right, must compile, so a failing case fails for its assert and never for a broken build.
// Built by tests/compile_fail_test.cmake.
#include <cstdint>

#include "Animator.h"
#include "GhostAI.h"
#include "entry.h"
#include "game_api.h"

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
#else
constexpr Method<map::GhostAI_LookAtPlayer, void(void*, void*)> lookAtPlayer{};
constexpr Icall<map::Animator_SetBoolID, void(void*, int32_t, bool)> setBoolID{};
constexpr Method<kStaticMethod, void(int32_t)> staticMethod{};
constexpr Field<map::GhostAI_isHunting, bool> isHunting{};
constexpr StaticField<kStaticInt, int32_t> staticInt{};
constexpr EnumValue<map::GhostState_idle> idle{};
constexpr Type<map::GhostAI> ghostAI{};
#endif

}  // namespace
