#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "game.h"
#include "held.h"
#include "ui.h"

namespace {
struct Element {
  std::string name, text;
  Element* parent = nullptr;
  bool alive = true;
};
std::vector<std::unique_ptr<Element>> elements;
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
ULONGLONG now = 1000;

Element* make(const char* name, void* parent) {
  elements.push_back(std::make_unique<Element>());
  elements.back()->name = name;
  elements.back()->parent = static_cast<Element*>(parent);
  return elements.back().get();
}
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
ULONGLONG WINAPI clock_ms() { return now; }
}  // namespace

#define GetTickCount64 clock_ms
#include "../core/notice.cpp"
#undef GetTickCount64

namespace pm::api::detail {
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
}  // namespace pm::api::detail
namespace pm::game {
bool alive(void* value) {
  for (auto* e = static_cast<Element*>(value); e; e = e->parent)
    if (!e->alive) return false;
  return value != nullptr;
}
}  // namespace pm::game
namespace pm::ui {
api::Object create_canvas(const char* name, int) { return make(name, nullptr); }
api::Object create_image(void* parent, const char* name, Color) { return make(name, parent); }
api::Object create_text(void* parent, const char* name, float) { return make(name, parent); }
void place(void*, Vec2, Vec2, Vec2, Vec2) {}
void set_color(void*, Color) {}
void set_text(void* text, const char* utf8) {
  CHECK(game::alive(text));  // the engine call on a destroyed object is the crash
  static_cast<Element*>(text)->text = utf8;
}
void destroy(void* component) {
  if (game::alive(component)) static_cast<Element*>(component)->alive = false;
}
}  // namespace pm::ui

namespace {
Element* canvas() {
  for (auto it = elements.rbegin(); it != elements.rend(); ++it)
    if ((*it)->name == "PlayableGhostNotice" && (*it)->alive) return it->get();
  return nullptr;
}
/** The popup's objects stay reachable for the GC while it shows, and are released when it hides. */
void popup_is_held() {
  pm::notice::show_wait(20, "Title", "Wait (0:20)");
  Element* shown = canvas();
  CHECK(shown && held(shown));
  int heldCount = 0;
  for (void* object : holds) heldCount += object != nullptr;
  CHECK(heldCount == 3);  // the canvas, the title and the body
  now += 3500;            // the wait popup's own time
  pm::notice::tick();
  CHECK(!canvas());
  for (void* object : holds) CHECK(!object);
}
/** A scene change destroys the popup's canvas: the next notice builds a new one. */
void destroyed_canvas_is_rebuilt() {
  pm::notice::show_wait(20, "Title", "Wait (0:20)");
  Element* first = canvas();
  CHECK(first);
  if (first) first->alive = false;  // the scene that held it was unloaded
  pm::notice::show_wait(19, "Title", "Wait (0:19)");
  Element* second = canvas();
  CHECK(second && second != first && held(second) && !held(first));
  bool bodyShown = false;
  for (const auto& e : elements) bodyShown |= e->alive && e->text.find("0:19") != std::string::npos;
  CHECK(bodyShown);
  pm::notice::hide();
  for (void* object : holds) CHECK(!object);
}
/** The truck countdown stops touching its popup once a scene change has destroyed it. */
void destroyed_wait_is_left_alone() {
  pm::notice::show_wait(20, "Title", "Wait (0:20)");
  Element* shown = canvas();
  CHECK(shown);
  if (shown) shown->alive = false;
  pm::notice::tick_wait(19, "Wait (0:19)");  // set_text on the destroyed body fails the fake's check
  pm::notice::hide();
}
}  // namespace

int main() {
  popup_is_held();
  destroyed_canvas_is_rebuilt();
  destroyed_wait_is_left_alone();
  return g_failures ? 1 : 0;
}
