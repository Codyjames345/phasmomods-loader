#include "mods_panel.h"

#include <windows.h>

#include <algorithm>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "menu_panel.h"
#include "mod_list.h"
#include "mods.h"
#include "ui.h"

namespace pg::mods_panel {
namespace {

constexpr ULONGLONG kSearchMs = 1000;    // retry unavailable native button templates at most once a second
constexpr float kAboveNews = 1.25f;      // when the game shows News, the Mods tape sits this many heights above it

struct Api {
  il2cpp::Object* textType = nullptr;
  void* (*mainMenu)(const void*) = nullptr;
  ui::Vec2 (*anchoredPosition)(void*, const void*) = nullptr;
  void (*setAnchoredPosition)(void*, ui::Vec2, const void*) = nullptr;
  ui::Vec2 (*sizeDelta)(void*, const void*) = nullptr;
  bool ok = false;
} api;

struct State {
  il2cpp::Held owner;  // the native main-menu instance this tape belongs to
  il2cpp::Held tape;   // the Mods button (Transform)
  menu_panel::Control control;  // its Image and colour animator
  ULONGLONG nextSearch = 0;
  menu_panel::Panel panel;
  std::vector<std::string> pending;  // the choice being made in the panel
  std::string shown;                 // what the rows and status last said
  std::string status;                // after Apply
  std::string note;                  // why a click switched another mod too
  bool requested = false;
  std::vector<std::string> request;
} g;

bool same_choice(std::vector<std::string> a, std::vector<std::string> b) {
  std::sort(a.begin(), a.end());
  std::sort(b.begin(), b.end());
  return a == b;
}

// The Mods tape: a copy of the game's News tape, beside it in the main menu's buttons.
void make_tape(void* menu) {
  void* news = game::alive(menu) ? game::find_child(game::transform_of(menu), "Buttons/News") : nullptr;
  if (!news) return;
  void* copy = game::instantiate(news, game::parent(news), false);
  if (!copy) return;
  menu_panel::make_inert(copy);
  game::set_active(game::game_object_of(copy), true);
  ui::Vec2 at = api.anchoredPosition(news, nullptr);
  if (game::active_self(game::game_object_of(news))) at.y += api.sizeDelta(news, nullptr).y * kAboveNews;
  api.setAnchoredPosition(copy, at, nullptr);
  void* label = game::find_child(copy, "Text (TMP)");
  void* text = label ? game::get_component(label, api.textType) : nullptr;
  if (text) ui::set_text(text, "Mods");
  g.tape = il2cpp::Held(copy);
  g.owner = il2cpp::Held(menu);
  g.control = menu_panel::control_of(copy);
  PG_LOG("mods panel: Mods button added to the main menu");
}

void refresh(bool canChange) {
  const std::vector<mods::Mod>& all = mods::loaded();
  std::string status = g.status;
  if (status.empty()) {
    if (!canChange) status = "Leave the lobby to change mods.";
    else if (same_choice(g.pending, mods::started_ids())) status = "Click a mod to switch it on or off.";
    else status = "Apply to restart the mods with your choice. The game keeps running.";
  }
  if (!g.note.empty()) status = g.note + "\n" + status;
  std::string shown = status;
  for (size_t i = 0; i < all.size(); ++i) shown += modlist::enabled(g.pending, all[i].info->id) ? '1' : '0';
  if (shown == g.shown) return;
  g.shown = shown;
  for (size_t i = 0; i < all.size() && i < g.panel.rows.size(); ++i)
    menu_panel::set_row_checked(g.panel, i, modlist::enabled(g.pending, all[i].info->id));
  menu_panel::set_text(g.panel.body, status);
}

void open_panel(bool canChange) {
  g.pending = mods::started_ids();
  g.status.clear();
  g.note.clear();
  g.shown.clear();
  if (!menu_panel::open(g.panel, "Mods", "", {"Apply", "Close"})) {
    PG_LOG("mods panel: the game's popup could not be copied");
    return;
  }
  const int count = static_cast<int>(mods::loaded().size());
  for (int i = 0; i < count; ++i) {
    const mods::Mod& mod = mods::loaded()[i];
    if (!menu_panel::add_row(g.panel, i, count, mod.info->name, mod.description)) {
      PG_LOG("mods panel: native checkbox or row could not be copied");
      menu_panel::close(g.panel);
      return;
    }
  }
  refresh(canChange);
}

/** Switch a mod, with what it requires (on) or what requires it (off), and say so when that is more. */
void toggle(const std::string& id) {
  const bool on = !modlist::enabled(g.pending, id);
  const std::vector<modlist::Manifest> all = mods::manifests();
  std::vector<std::string> next = modlist::switched(g.pending, all, id, on);
  g.note = modlist::switch_note(all, g.pending, next, id, on);
  g.pending = std::move(next);
  g.status.clear();
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* rect = il2cpp::cls(kCore, "UnityEngine", "RectTransform");
  api.textType = il2cpp::type_object(il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI"));
  api.mainMenu = reinterpret_cast<decltype(api.mainMenu)>(
      il2cpp::method_ptr(il2cpp::cls("Assembly-CSharp.dll", "", "ConnectToServer"), "get_Instance", 0));
  api.anchoredPosition = reinterpret_cast<decltype(api.anchoredPosition)>(il2cpp::method_ptr(rect, "get_anchoredPosition", 0));
  api.setAnchoredPosition = reinterpret_cast<decltype(api.setAnchoredPosition)>(il2cpp::method_ptr(rect, "set_anchoredPosition", 1));
  api.sizeDelta = reinterpret_cast<decltype(api.sizeDelta)>(il2cpp::method_ptr(rect, "get_sizeDelta", 0));
  api.ok = api.mainMenu && api.textType && api.anchoredPosition &&
           api.setAnchoredPosition && api.sizeDelta;
  PG_LOG("mods panel: resolve %s", api.ok ? "ok" : "unavailable (mods are chosen in the launcher only)");
  return true;  // optional: the launcher still chooses the mods
}

void tick(bool canChange) {
  if (!api.ok) return;
  void* menu = api.mainMenu(nullptr);
  if (g.tape.get() && (!game::alive(g.tape.get()) || g.owner.get() != menu)) {
    if (game::alive(g.tape.get())) game::destroy(game::game_object_of(g.tape.get()));
    menu_panel::close(g.panel);
    g.tape = nullptr;
    g.control = {};
    g.owner = nullptr;
    g.nextSearch = 0;
  }
  if (!game::alive(menu)) {  // between scenes
    menu_panel::close(g.panel);
    return;
  }
  // Shown and hidden with the game's own buttons beside it, which stay on the board while the camera
  // looks at another one (the map, the shop); it is clicked only on the focused board.
  const bool shown = menu_panel::menu_buttons_shown();
  const ULONGLONG now = GetTickCount64();
  if (shown && !g.tape.get() && now >= g.nextSearch) {
    g.nextSearch = now + kSearchMs;
    make_tape(menu);
  }
  if (game::alive(g.tape.get())) game::set_active(game::game_object_of(g.tape.get()), shown);
  if (!menu_panel::main_menu_visible()) {
    menu_panel::close(g.panel);
    return;
  }
  if (menu_panel::is_open(g.panel)) {
    refresh(canChange);
    const int button = menu_panel::tick(g.panel);
    const int row = menu_panel::row_clicked(g.panel);
    const std::vector<mods::Mod>& all = mods::loaded();
    if (canChange && row >= 0 && static_cast<size_t>(row) < all.size()) toggle(all[row].info->id);
    if (button == 0 && canChange) {
      if (same_choice(g.pending, mods::started_ids())) {
        menu_panel::close(g.panel);
      } else {
        g.request = g.pending;
        g.requested = true;
        g.status = "Restarting the mods...";
      }
    }
    if (button == 1) menu_panel::close(g.panel);
    return;
  }
  if (menu_panel::press(g.control)) open_panel(canChange);
}

bool take_choice(std::vector<std::string>& ids) {
  if (!g.requested) return false;
  g.requested = false;
  ids = g.request;
  return true;
}

void applied(bool ok) {
  g.pending = mods::started_ids();
  g.status = ok ? "Done: the mods restarted with your choice." : "Some mods could not start. mods.log says why.";
}

void teardown() {
  menu_panel::close(g.panel);
  if (game::alive(g.tape.get())) game::destroy(game::game_object_of(g.tape.get()));
  g = State{};
}

}  // namespace pg::mods_panel
