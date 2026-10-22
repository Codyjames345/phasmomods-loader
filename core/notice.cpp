#include "notice.h"

#include <windows.h>

#include "game.h"
#include "il2cpp.h"
#include "ui.h"

namespace pg::notice {
namespace {

using ui::Color;

constexpr unsigned kWaitShowMs = 3500;
constexpr Color kPanel{0, 0, 0, 0.75f};
constexpr Color kTitle{1, 1, 1, 0.95f};
constexpr Color kBody{1, 1, 1, 0.75f};

// Held: the popup outlives frames, and a scene change destroys it (see il2cpp::Held).
struct Popup {
  il2cpp::Held canvas;
  il2cpp::Held title;
  il2cpp::Held body;
  ULONGLONG hideAt = 0;
  bool waiting = false;  // the wait popup, whose countdown follows the truck lock
  unsigned shownSeconds = ~0u;
  std::string titleShown, bodyShown;
} g;

void build() {
  g = Popup{};
  void* canvas = ui::create_canvas("PlayableGhostNotice", 60);
  if (!canvas) return;
  g.canvas = il2cpp::Held(canvas);
  void* panel = ui::create_image(canvas, "Panel", kPanel);
  ui::place(panel, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -160}, {820, 120});
  void* title = ui::create_text(canvas, "Title", 30);
  ui::place(title, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -135}, {800, 40});
  ui::set_color(title, kTitle);
  g.title = il2cpp::Held(title);
  void* body = ui::create_text(canvas, "Body", 24);
  ui::place(body, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -180}, {800, 36});
  ui::set_color(body, kBody);
  g.body = il2cpp::Held(body);
}

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

std::string wait_text(unsigned seconds) {
  return "Wait in the truck until the ghost player is ready (0:" + std::string(seconds < 10 ? "0" : "") +
         std::to_string(seconds) + ")";
}

}  // namespace

void show(const std::string& title, const std::string& body, unsigned ms) {
  if (!game::alive(g.canvas.get())) build();  // first use, or the scene it was in was unloaded
  if (!g.canvas.get()) return;
  set_if_changed(g.title.get(), g.titleShown, title);
  set_if_changed(g.body.get(), g.bodyShown, body);
  g.hideAt = GetTickCount64() + ms;
  g.waiting = false;
}

void show_wait(unsigned secondsLeft) {
  show("The truck stays shut while the ghost is chosen", wait_text(secondsLeft), kWaitShowMs);
  g.waiting = true;
  g.shownSeconds = secondsLeft;
}

void tick_wait(unsigned secondsLeft) {
  if (!game::alive(g.canvas.get()) || !g.waiting) return;
  if (secondsLeft == 0) {
    hide();
    return;
  }
  if (secondsLeft != g.shownSeconds) {
    g.shownSeconds = secondsLeft;
    set_if_changed(g.body.get(), g.bodyShown, wait_text(secondsLeft));
  }
}

void tick() {
  if (g.canvas.get() && GetTickCount64() >= g.hideAt) hide();
}

void hide() {
  if (!g.canvas.get()) return;
  ui::destroy(g.canvas.get());
  g = Popup{};
}

}  // namespace pg::notice
