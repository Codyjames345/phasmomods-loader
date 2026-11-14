// Task 0b-3a typed the C++ helpers' returns: a helper that hands back a managed object returns a
// handle, not a void*. One assertion per rule-1 or rule-2 helper (a body that is one wrapper call,
// one Field read, or another helper this rule already typed): it must return exactly the handle
// that body's evidence names, which task-0b3a-report.md lists helper by helper. Every assertion is
// a static_assert, so compiling this test is the check; main only reports that the build reached it.
// Run: build\helper_returns_are_typed_test.exe
#include <type_traits>
#include <utility>

#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "bundles.h"
#include "buttons.h"
#include "menu_rows.h"
#include "models.h"

namespace {

// sdk/api, one line per helper in header order.
static_assert(std::is_same_v<decltype(pm::api::lit_material_of(std::declval<void*>())),
                             pm::api::Ref<pm::map::Material>>,
              "lit_material_of must return Ref<Material>");
static_assert(std::is_same_v<decltype(pm::api::rows::journal_list(std::declval<void*>())),
                             pm::api::Ref<pm::map::Transform>>,
              "rows::journal_list must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::api::rows::divider_beside(std::declval<void*>())),
                             pm::api::Ref<pm::map::GameObject>>,
              "rows::divider_beside must return Ref<GameObject>");
static_assert(std::is_same_v<decltype(pm::api::rows::line_divider(std::declval<void*>())),
                             pm::api::Ref<pm::map::GameObject>>,
              "rows::line_divider must return Ref<GameObject>");
static_assert(std::is_same_v<decltype(pm::api::buttons::input_module()), pm::api::Object>,
              "buttons::input_module must return Object");
static_assert(std::is_same_v<decltype(pm::api::buttons::selected()), pm::api::Ref<pm::map::GameObject>>,
              "buttons::selected must return Ref<GameObject>");
static_assert(std::is_same_v<decltype(pm::api::load_bundle(std::declval<const std::filesystem::path&>())),
                             pm::api::Object>,
              "load_bundle must return Object");
static_assert(std::is_same_v<decltype(pm::api::load_bundle_asset(std::declval<void*>(), std::declval<const char*>(),
                                                                 std::declval<void*>())),
                             pm::api::Object>,
              "load_bundle_asset must return Object");
static_assert(std::is_same_v<decltype(pm::api::load_controller(std::declval<void*>(), std::declval<const char*>())),
                             pm::api::Object>,
              "load_controller must return Object");

}  // namespace

int main() { return 0; }
