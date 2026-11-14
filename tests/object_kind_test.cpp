// Offline tests of the helpers in core/game.cpp and core/ui.cpp that ask an object about itself (its Transform,
// its GameObject, whether that Transform is a RectTransform), over the fake game backend. Each question has to go
// through the accessor the object's own kind answers, because a GameObject is not a Component: the dev menu's
// first live scroll list asked the GameObjects it had just made through Component.get_transform, which reads a
// layout those objects do not have, and the game faulted (2026-11-12). A component of a class derived from
// Component is still asked through the component accessor, so the answer follows the game's class parents and not
// a name. The same shape decides the layout setters: an object whose transform is a plain Transform has no
// anchors, and setting them on it is the same mistake one step on.
// Run: build\object_kind_test.exe
#include <cstdint>
#include <string>

#include "Transform.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "game.h"
#include "game_api.h"
#include "ui.h"

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace game = pm::game;
namespace map = pm::map;

// The classes, as the fake compares them with an object's first pointer. Canvas derives from Component, so an
// answer that only matched exact classes would get it wrong.
const void* kGoClass = reinterpret_cast<void*>(0x1000);
const void* kComponentClass = reinterpret_cast<void*>(0x2000);
const void* kCanvasClass = reinterpret_cast<void*>(0x3000);

// A managed object as far as these helpers reach: its class, then the engine pointer every
// UnityEngine.Object keeps.
struct FakeObject {
  void* klass;
  void* cached;
};
FakeObject g_go{const_cast<void*>(kGoClass), reinterpret_cast<void*>(0x10)};
FakeObject g_canvas{const_cast<void*>(kCanvasClass), reinterpret_cast<void*>(0x20)};

// What each accessor answers with, and how many times it ran, so a test can see which path was taken.
void* g_goTransform = reinterpret_cast<void*>(0x100);
void* g_componentTransform = reinterpret_cast<void*>(0x200);
void* g_canvasGameObject = reinterpret_cast<void*>(0x300);
int gGoTransformCalls = 0, gComponentTransformCalls = 0, gComponentGameObjectCalls = 0;

void* fake_go_transform(void*, void*) {
  ++gGoTransformCalls;
  return g_goTransform;
}
void* fake_component_transform(void*, void*) {
  ++gComponentTransformCalls;
  return g_componentTransform;
}
void* fake_component_game_object(void*, void*) {
  ++gComponentGameObjectCalls;
  return g_canvasGameObject;
}

void give_everything() {
  fake::reset();
  gGoTransformCalls = gComponentTransformCalls = gComponentGameObjectCalls = 0;
  fake::type(map::GameObject, const_cast<void*>(kGoClass));
  fake::type(map::Component, const_cast<void*>(kComponentClass));
  fake::parent(const_cast<void*>(kCanvasClass), const_cast<void*>(kComponentClass));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_go_transform));
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_component_transform));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_component_game_object));
  std::string missing;
  api::bind(fake::host(), missing);
}

// The object the menu makes with new_game_object: its transform is its RectTransform, and it answers that
// through its own kind. Asking it through Component is what faulted the game.
void a_game_object_answers_for_its_own_transform() {
  give_everything();
  CHECK(game::transform_of(&g_go) == g_goTransform);
  CHECK(gGoTransformCalls == 1);
  CHECK(gComponentTransformCalls == 0);
}

// A component, including one of a class derived from Component, goes through the component accessor; the
// GameObject accessor would answer nothing for it.
void a_component_answers_through_the_component_accessor() {
  give_everything();
  CHECK(game::transform_of(&g_canvas) == g_componentTransform);
  CHECK(gComponentTransformCalls == 1);
  CHECK(gGoTransformCalls == 0);
}

// Nothing is asked of nothing: no accessor runs, and the answer is the one a mod already has to handle.
void nothing_is_asked_of_nothing() {
  give_everything();
  CHECK(game::transform_of(nullptr) == nullptr);
  CHECK(gGoTransformCalls == 0 && gComponentTransformCalls == 0);
}

// With the class itself unreadable the kind is unknown, and guessing it is the one answer that can fault the
// game: the helper gives up instead, as it does wherever an entry cannot be read.
void the_kind_is_not_guessed_when_it_cannot_be_read() {
  give_everything();
  fake::forget(map::GameObject);
  std::string missing;
  api::bind(fake::host(), missing);
  CHECK(game::transform_of(&g_go) == nullptr);
  CHECK(gGoTransformCalls == 0 && gComponentTransformCalls == 0);
}

// The same rule for the GameObject an object belongs to: a GameObject's own is itself, and the Component
// accessor is not for it.
void an_object_asked_for_its_game_object() {
  give_everything();
  CHECK(static_cast<void*>(game::game_object_of(&g_go)) == &g_go);
  CHECK(gComponentGameObjectCalls == 0);
  CHECK(static_cast<void*>(game::game_object_of(&g_canvas)) == g_canvasGameObject);
  CHECK(gComponentGameObjectCalls == 1);
  CHECK(game::game_object_of(nullptr) == nullptr);
  CHECK(gComponentGameObjectCalls == 1);
}

// The path that already knew what it had: a GameObject named as one needs no class check to answer.
void the_explicit_object_path_needs_no_check() {
  give_everything();
  CHECK(game::object_transform(&g_go) == g_goTransform);
  CHECK(gGoTransformCalls == 1 && gComponentTransformCalls == 0);
  CHECK(game::object_transform(nullptr) == nullptr);
}

// ---- what the layout setters may act on ----

// A transform, read as its own class: a RectTransform is what a laid-out UI object has, a plain Transform what
// an object made at run time has until it takes a component that requires one (every UI class in the game's own
// UnityEngine.UI carries [RequireComponent(typeof(RectTransform))]: Graphic, RectMask2D, ScrollRect,
// LayoutElement). RectTransform derives from Transform, so the check follows the class parents, not a name.
const void* kRectClass = reinterpret_cast<void*>(0x4000);
const void* kTransformClass = reinterpret_cast<void*>(0x5000);
FakeObject g_rectTransform{const_cast<void*>(kRectClass), reinterpret_cast<void*>(0x40)};
FakeObject g_plainTransform{const_cast<void*>(kTransformClass), reinterpret_cast<void*>(0x50)};

// What the object's kind answers for its own transform, so a test can give it one kind or the other.
void* g_itsTransform = &g_rectTransform;
void* fake_its_transform(void*, void*) { return g_itsTransform; }

// Two of the five setters place() runs, counted, so a test sees whether the layout ran at all.
int gAnchorMinCalls = 0, gSizeDeltaCalls = 0;
void fake_set_anchor_min(void*, phasmo::Vector2, void*) { ++gAnchorMinCalls; }
void fake_set_size_delta(void*, phasmo::Vector2, void*) { ++gSizeDeltaCalls; }

void give_layout() {
  give_everything();
  g_itsTransform = &g_rectTransform;
  gAnchorMinCalls = gSizeDeltaCalls = 0;
  fake::type(map::Transform, const_cast<void*>(kTransformClass));
  fake::type(map::RectTransform, const_cast<void*>(kRectClass));
  fake::parent(const_cast<void*>(kRectClass), const_cast<void*>(kTransformClass));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_its_transform));
  fake::method(map::RectTransform_set_anchorMin, reinterpret_cast<void*>(&fake_set_anchor_min));
  fake::method(map::RectTransform_set_sizeDelta, reinterpret_cast<void*>(&fake_set_size_delta));
  std::string missing;
  api::bind(fake::host(), missing);
}

// The transform a layout setter can act on, when the object has one.
void a_rect_transform_answers() {
  give_layout();
  CHECK(game::rect_transform_of(&g_go) == &g_rectTransform);
}

// An object whose transform is a plain Transform has no anchors to set: the helper answers null, and place()
// leaves it alone rather than running the setters over a layout it does not have.
void a_plain_transform_is_never_laid_out() {
  give_layout();
  g_itsTransform = &g_plainTransform;
  CHECK(game::rect_transform_of(&g_go) == nullptr);
  pm::ui::place(&g_go, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {10.0f, 10.0f});
  CHECK(gAnchorMinCalls == 0 && gSizeDeltaCalls == 0);
}

// The layout does run once the object's transform is a RectTransform.
void a_rect_transform_is_laid_out() {
  give_layout();
  pm::ui::place(&g_go, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {10.0f, 10.0f});
  CHECK(gAnchorMinCalls == 1);
  CHECK(gSizeDeltaCalls == 1);
}

// With the RectTransform class unreadable the kind is unknown again, and nothing is guessed about it either.
void the_rect_transform_is_not_guessed_without_the_class() {
  give_layout();
  fake::forget(map::RectTransform);
  std::string missing;
  api::bind(fake::host(), missing);
  CHECK(game::rect_transform_of(&g_go) == nullptr);
  pm::ui::place(&g_go, {0.5f, 0.5f}, {0.5f, 0.5f}, {0.0f, 0.0f}, {10.0f, 10.0f});
  CHECK(gAnchorMinCalls == 0 && gSizeDeltaCalls == 0);
}

}  // namespace

int main() {
  a_game_object_answers_for_its_own_transform();
  a_component_answers_through_the_component_accessor();
  nothing_is_asked_of_nothing();
  the_kind_is_not_guessed_when_it_cannot_be_read();
  an_object_asked_for_its_game_object();
  the_explicit_object_path_needs_no_check();
  a_rect_transform_answers();
  a_plain_transform_is_never_laid_out();
  a_rect_transform_is_laid_out();
  the_rect_transform_is_not_guessed_without_the_class();
  std::printf(g_failures ? "%d check(s) failed\n" : "all object kind tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
