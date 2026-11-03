#include "notice.h"

#include <windows.h>

#include "game.h"
#include "held.h"
#include "ui.h"

namespace pm::notice {
namespace {

using ui::Color;

constexpr unsigned kWaitShowMs = 3500;
constexpr Color kPanel{0, 0, 0, 0.75f};
constexpr Color kTitle{1, 1, 1, 0.95f};
constexpr Color kBody{1, 1, 1, 0.75f};

// Held: the popup outlives frames, and a scene change destroys it (see api::Held).
struct Popup {
  api::Held canvas;
  api::Held title;
  api::Held body;
  ULONGLONG hideAt = 0;
  bool waiting = false;  // the wait popup, whose countdown follows the truck lock
  unsigned shownSeconds = ~0u;
  std::string titleShown, bodyShown;
} g;

void build() {
  g = Popup{};
  void* canvas = ui::create_canvas("PlayableGhostNotice", 60);
  if (!canvas) return;
  g.canvas = api::Held(canvas);
  void* panel = ui::create_image(canvas, "Panel", kPanel);
  ui::place(panel, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -160}, {820, 120});
  void* title = ui::create_text(canvas, "Title", 30);
  ui::place(title, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -135}, {800, 40});
  ui::set_color(title, kTitle);
  g.title = api::Held(title);
  void* body = ui::create_text(canvas, "Body", 24);
  ui::place(body, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -180}, {800, 36});
  ui::set_color(body, kBody);
  g.body = api::Held(body);
}

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

void show(const std::string& title, const std::string& body, unsigned ms) {
  if (!game::alive(g.canvas.get())) build();  // first use, or the scene it was in was unloaded
  if (!g.canvas.get()) return;
  set_if_changed(g.title.get(), g.titleShown, title);
  set_if_changed(g.body.get(), g.bodyShown, body);
  g.hideAt = GetTickCount64() + ms;
  g.waiting = false;
}

}  // namespace

void show_wait(unsigned secondsLeft, const std::string& title, const std::string& body) {
  show(title, body, kWaitShowMs);
  g.waiting = true;
  g.shownSeconds = secondsLeft;
}

void tick_wait(unsigned secondsLeft, const std::string& body) {
  if (!game::alive(g.canvas.get()) || !g.waiting) return;
  if (secondsLeft == 0) {
    hide();
    return;
  }
  if (secondsLeft != g.shownSeconds) {
    g.shownSeconds = secondsLeft;
    set_if_changed(g.body.get(), g.bodyShown, body);
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

}  // namespace pm::notice
