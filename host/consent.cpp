#include "consent.h"

#include <windows.h>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "menu_panel.h"
#include "ui.h"

namespace pg::consent {
namespace {

constexpr int kSortingOrder = 32760;  // above the game's own overlays and the mods' HUDs
constexpr ui::Color kBackdrop{0.0f, 0.0f, 0.0f, 0.88f};
constexpr ui::Color kPanel{0.07f, 0.07f, 0.07f, 0.98f};
constexpr ui::Color kAgree{0.55f, 0.10f, 0.06f, 1.0f};   // the journal's red
constexpr ui::Color kDecline{0.22f, 0.22f, 0.22f, 1.0f};
constexpr ui::Vec2 kMiddle{0.5f, 0.5f};
constexpr ui::Vec2 kButtonSize{340, 70};
// The main menu's popup is copied for the notice; if the menu has not appeared by then (another
// scene, or a game build whose popup moved), the mod's own overlay asks instead.
constexpr ULONGLONG kMenuWaitMs = 20000;

struct Button {
  il2cpp::Held image;  // Held (il2cpp.h): kept between frames
  ui::Color color{};
};

struct State {
  Notice notice;
  std::string fingerprint;
  std::wstring path;
  Answer answer = Answer::Pending;
  ULONGLONG overlayAt = 0;  // when the overlay takes over from waiting for the menu
  ULONGLONG nextTry = 0;    // copying the menu's popup searches the scene: at most once a second
  menu_panel::Panel panel;  // the notice as the game's popup
  il2cpp::Held canvas;      // or as the mod's overlay
  Button agree, decline;
  // As last written to mods.log, where a notice nobody sees is diagnosed: why the main menu was out
  // of view (null: in view; "" before the first tick), whether copying the menu's popup failed, and
  // whether the popup was drawn.
  const char* waitingFor = "";
  bool copyFailed = false;
  bool drawn = false;
} g;

Button make_button(void* parent, const char* label, ui::Color color, float x) {
  Button b{ui::create_image(parent, label, color), color};
  ui::place(b.image, kMiddle, kMiddle, {x, -290}, kButtonSize);
  void* text = ui::create_text(b.image, "Label", 30);
  ui::place(text, kMiddle, kMiddle, {0, 0}, kButtonSize);
  ui::set_text(text, label);
  return b;
}

void build() {
  g.canvas = ui::create_canvas("PhasmoMods notice", kSortingOrder);
  if (!g.canvas) return;
  ui::block_pointer(g.canvas);
  void* backdrop = ui::create_image(g.canvas, "Backdrop", kBackdrop);
  ui::place(backdrop, kMiddle, kMiddle, {0, 0}, {6000, 4000});
  ui::set_raycast_target(backdrop, true);
  void* panel = ui::create_image(g.canvas, "Panel", kPanel);
  ui::place(panel, kMiddle, kMiddle, {0, 0}, {1240, 760});
  void* title = ui::create_text(panel, "Title", 46);
  ui::place(title, kMiddle, kMiddle, {0, 320}, {1140, 70});
  ui::set_text(title, g.notice.title.c_str());
  void* body = ui::create_text(panel, "Body", 25);
  ui::place(body, kMiddle, kMiddle, {0, 10}, {1120, 520});
  ui::set_word_wrap(body, true);
  ui::set_alignment(body, ui::Align::TopLeft);
  ui::set_text(body, g.notice.body.c_str());
  g.agree = make_button(panel, "I agree", kAgree, -190);
  g.decline = make_button(panel, "Play without mods", kDecline, 190);
}

ui::Color lighter(ui::Color c) { return {c.r * 1.35f + 0.06f, c.g * 1.35f + 0.06f, c.b * 1.35f + 0.06f, c.a}; }

void answer(Answer a) {
  g.answer = a;
  if (a == Answer::Agreed && !remember(g.path, g.fingerprint))
    PG_LOG("consent: could not remember the answer (the notice will be shown again next time)");
  PG_LOG("consent: %s (%s)", a == Answer::Agreed ? "agreed" : "declined; mods stay off", g.fingerprint.c_str());
  hide();
}

// The notice as the game's own popup over the main menu: true while it is (or can be) shown so.
bool tick_menu() {
  if (!menu_panel::is_open(g.panel)) {
    const ULONGLONG now = GetTickCount64();
    if (now < g.nextTry) return false;
    g.nextTry = now + 1000;
    if (!menu_panel::open(g.panel, g.notice.title, g.notice.body, {"I agree", "Play without mods"})) {
      if (!g.copyFailed) PG_LOG("consent: the menu's popup could not be copied; trying again each second");
      g.copyFailed = true;
      return false;
    }
    g.copyFailed = false;
    g.drawn = false;
    PG_LOG("consent: shown as the menu's popup");
  }
  const int clicked = menu_panel::tick(g.panel);
  if (!menu_panel::is_open(g.panel)) {
    const char* why = menu_panel::why_menu_hidden();
    PG_LOG("consent: the menu's popup closed (%s)", why ? why : "it was switched off");
    return true;
  }
  if (menu_panel::drawn(g.panel) != g.drawn) {
    g.drawn = !g.drawn;
    PG_LOG(g.drawn ? "consent: the menu's popup is drawn" : "consent: the menu's popup is not drawn (it or its canvas is switched off)");
  }
  if (clicked >= 0) answer(clicked == 0 ? Answer::Agreed : Answer::Declined);
  return true;
}

void tick_overlay() {
  if (!g.canvas || !game::alive(g.canvas)) {  // first shown, or a scene change destroyed it
    build();
    if (g.canvas) PG_LOG("consent: shown as the mod's own overlay");
  }
  if (!g.canvas) return;
  if (game::cursor_locked()) game::free_cursor();
  float x = 0, y = 0;
  const bool pointer = game::mouse_position(x, y);
  const bool click = pointer && game::mouse_left_pressed();
  for (Button* b : {&g.agree, &g.decline}) {
    const bool over = pointer && ui::contains(b->image, {x, y});
    ui::set_color(b->image, over ? lighter(b->color) : b->color);
    if (over && click) {
      answer(b == &g.agree ? Answer::Agreed : Answer::Declined);
      return;
    }
  }
}

}  // namespace

void begin(const Notice& notice, const std::wstring& rememberedPath) {
  g = {};
  g.notice = notice;
  g.fingerprint = fingerprint(notice);
  g.path = rememberedPath;
  g.overlayAt = GetTickCount64() + kMenuWaitMs;
  if (remembered(g.path, g.fingerprint)) {
    g.answer = Answer::Agreed;
    PG_LOG("consent: agreed earlier to this notice (%s)", g.fingerprint.c_str());
  }
}

Answer tick() {
  if (g.answer != Answer::Pending) return g.answer;
  const char* hidden = menu_panel::why_menu_hidden();
  if (hidden != g.waitingFor) {
    if (hidden) PG_LOG("consent: waiting for the main menu (%s)", hidden);
    else PG_LOG("consent: the main menu is in view");
    g.waitingFor = hidden;
  }
  if (hidden) {
    hide();
    g.nextTry = 0;
    return g.answer;
  }
  if (g.canvas || !tick_menu()) {
    if (g.canvas || GetTickCount64() >= g.overlayAt) tick_overlay();
  }
  return g.answer;
}

void hide() {
  menu_panel::close(g.panel);
  if (g.canvas) ui::destroy(g.canvas);
  g.canvas = nullptr;
  g.agree = {};
  g.decline = {};
}

}  // namespace pg::consent
