// Task 0b-3a typed the C++ helpers' returns: a helper that hands back a managed object returns a
// handle, not a void*. One assertion per rule-1 or rule-2 helper (a body that is one wrapper call,
// one Field read, or another helper this rule already typed): it must return exactly the handle
// that body's evidence names, which task-0b3a-report.md lists helper by helper. Every assertion is
// a static_assert, so compiling this test is the check; main only reports that the build reached it.
// Run: build\helper_returns_are_typed_test.exe
#include <type_traits>
#include <utility>

#include "Camera.h"
#include "Difficulty.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "bundles.h"
#include "buttons.h"
#include "game.h"
#include "menu_rows.h"
#include "models.h"
#include "ui.h"

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

// core/game.h, one line per helper in header order.
static_assert(std::is_same_v<decltype(pm::game::transform_of(std::declval<void*>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::transform_of must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::object_transform(std::declval<void*>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::object_transform must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::rect_transform_of(std::declval<void*>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::rect_transform_of must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::add_component(std::declval<void*>(), std::declval<void*>())),
                             pm::api::Object>,
              "game::add_component must return Object");
static_assert(std::is_same_v<decltype(pm::game::parent(std::declval<void*>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::parent must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::child(std::declval<void*>(), std::declval<int>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::child must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::level_controller()), pm::api::Ref<pm::map::LevelController>>,
              "game::level_controller must return Ref<LevelController>");
static_assert(std::is_same_v<decltype(pm::game::ghost_ai()), pm::api::Ref<pm::map::GhostAI>>,
              "game::ghost_ai must return Ref<GhostAI>");
static_assert(std::is_same_v<decltype(pm::game::ghost_agent(std::declval<void*>())),
                             pm::api::Ref<pm::map::NavMeshAgent>>,
              "game::ghost_agent must return Ref<NavMeshAgent>");
static_assert(std::is_same_v<decltype(pm::game::network()), pm::api::Ref<pm::map::Network>>,
              "game::network must return Ref<Network>");
static_assert(std::is_same_v<decltype(pm::game::local_player()), pm::api::Object>,
              "game::local_player must return Object");
static_assert(std::is_same_v<decltype(pm::game::player_of_actor(std::declval<int>())), pm::api::Object>,
              "game::player_of_actor must return Object");
static_assert(std::is_same_v<decltype(pm::game::local_camera()), pm::api::Ref<pm::map::Camera>>,
              "game::local_camera must return Ref<Camera>");
static_assert(std::is_same_v<decltype(pm::game::ghost_activity(std::declval<void*>())),
                             pm::api::Ref<pm::map::GhostActivity>>,
              "game::ghost_activity must return Ref<GhostActivity>");
static_assert(std::is_same_v<decltype(pm::game::level_values()), pm::api::Ref<pm::map::LevelValues>>,
              "game::level_values must return Ref<LevelValues>");
static_assert(std::is_same_v<decltype(pm::game::setup_phase()), pm::api::Ref<pm::map::SetupPhaseController>>,
              "game::setup_phase must return Ref<SetupPhaseController>");
static_assert(std::is_same_v<decltype(pm::game::local_player_component()), pm::api::Ref<pm::map::LocalPlayer>>,
              "game::local_player_component must return Ref<LocalPlayer>");
static_assert(std::is_same_v<decltype(pm::game::raycast(std::declval<phasmo::Vector3>(), std::declval<phasmo::Vector3>(),
                                                        std::declval<float>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::raycast (no mask) must return Ref<Transform>");
static_assert(std::is_same_v<decltype(pm::game::raycast(std::declval<phasmo::Vector3>(), std::declval<phasmo::Vector3>(),
                                                        std::declval<float>(), std::declval<int32_t>())),
                             pm::api::Ref<pm::map::Transform>>,
              "game::raycast (with mask) must return Ref<Transform>");

// core/ui.h, one line per helper in header order.
static_assert(std::is_same_v<decltype(pm::ui::create_canvas(std::declval<const char*>(), std::declval<int>())),
                             pm::api::Object>,
              "ui::create_canvas must return Object");
static_assert(std::is_same_v<decltype(pm::ui::create_text(std::declval<void*>(), std::declval<const char*>(),
                                                          std::declval<float>())),
                             pm::api::Object>,
              "ui::create_text must return Object");
static_assert(std::is_same_v<decltype(pm::ui::create_image(std::declval<void*>(), std::declval<const char*>(),
                                                           std::declval<pm::ui::Color>())),
                             pm::api::Object>,
              "ui::create_image must return Object");
static_assert(std::is_same_v<decltype(pm::ui::create_raw_image(std::declval<void*>(), std::declval<const char*>(),
                                                               std::declval<void*>())),
                             pm::api::Object>,
              "ui::create_raw_image must return Object");
static_assert(std::is_same_v<decltype(pm::ui::create_sprite_image(std::declval<void*>(), std::declval<const char*>(),
                                                                  std::declval<void*>())),
                             pm::api::Object>,
              "ui::create_sprite_image must return Object");
static_assert(std::is_same_v<decltype(pm::ui::create_world_canvas(std::declval<const char*>(), std::declval<void*>(),
                                                                  std::declval<float>())),
                             pm::api::Object>,
              "ui::create_world_canvas must return Object");

}  // namespace

int main() { return 0; }
