#include "mods_panel.h"

#include <windows.h>

#include <algorithm>

#include "MainMenu.h"
#include "Transform.h"
#include "UnityUI.h"
#include "find_child.h"
#include "host_game.h"
#include "il2cpp.h"
#include "log.h"
#include "menu_panel.h"
#include "mod_deps.h"
#include "mod_list.h"
#include "mods.h"
#include "text_table.h"
#include "ui.h"

namespace pm::mods_panel {
namespace {

constexpr ULONGLONG kSearchMs = 1000;    // retry unavailable native button templates at most once a second
constexpr float kAboveNews = 1.25f;      // when the game shows News, the Mods tape sits this many heights above it

using phasmo::RectTransform;

// Whether the tape can be made: without these the mods are chosen in the launcher only.
bool available() {
  return menu_panel::can_copy() && menu_panel::can_copy_rows() &&
         api::all_resolved<phasmo::ConnectToServer::get_Instance, phasmo::TextMeshProUGUI::type,
                           RectTransform::get_anchoredPosition, RectTransform::set_anchoredPosition,
                           RectTransform::get_sizeDelta>();
}

struct State {
  il2cpp::Held owner;  // the native main-menu instance this tape belongs to
  il2cpp::Held tape;   // the Mods button (Transform)
  menu_panel::Control control;  // its UIButton, with the loader's listener
  ULONGLONG nextSearch = 0;
  il2cpp::Held refusedBy;  // the menu whose News copy took no listener: not tried again while it lasts
  menu_panel::Panel panel;
  std::vector<std::string> pending;  // the choice being made in the panel
  std::vector<std::string> saved;    // what it started from: what Apply has nothing to do for
  bool changeable = false;           // whether pending was read for changing it, or to show what runs in this room
  std::string cause;                 // why no mod runs when nobody chose that (tick_off); empty otherwise
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
  void* news = hostgame::alive(menu) ? host::find_child(hostgame::transform_of(menu), "Buttons/News") : nullptr;
  if (!news) return;
  void* copy = hostgame::instantiate(news, hostgame::parent(news), false);
  if (!copy) return;
  menu_panel::make_inert(copy);
  // News's own click starts the training: the copy keeps the game's button with that listener taken off and the
  // loader's on (sdk/systems/menus.md).
  menu_panel::Control control;
  if (!menu_panel::clickable(control, copy)) {
    hostgame::destroy(hostgame::game_object_of(copy));
    PM_LOG("mods panel: the News button could not take the mod's listener; no Mods button");
    g.refusedBy = il2cpp::Held(menu);
    return;
  }
  hostgame::set_active(hostgame::game_object_of(copy), true);
  phasmo::Vector2 at = RectTransform::get_anchoredPosition(news);
  if (hostgame::active_self(hostgame::game_object_of(news))) at.y += RectTransform::get_sizeDelta(news).y * kAboveNews;
  RectTransform::set_anchoredPosition(copy, at);
  void* label = host::find_child(copy, "Text (TMP)");
  void* text = label ? hostgame::get_component(label, phasmo::TextMeshProUGUI::type.type_object()) : nullptr;
  if (text) ui::set_text(text, i18n::text("panel.button").c_str());
  g.tape = il2cpp::Held(copy);
  g.owner = il2cpp::Held(menu);
  g.control = std::move(control);
  PM_LOG("mods panel: Mods button added to the main menu");
}

// A row's name column: the mod's name and version. A mod in the old format has no version it can say.
std::string row_name(const modlist::Manifest& m) {
  return i18n::text("panel.row-name", {{"name", m.name}, {"version", m.version.empty() ? i18n::text("panel.version-unknown") : m.version}});
}

// A row's description column: who made the mod and what it does, after why it is off when the last start left it off.
std::string row_text(const modlist::Manifest& m) {
  const std::string about =
      m.author.empty() ? m.description : i18n::text("panel.by-author", {{"author", m.author}, {"description", m.description}});
  const std::string off = mods::off_reason(m.id);
  return off.empty() ? about : i18n::text("panel.off-reason", {{"reason", off}, {"about", about}});
}

void refresh(bool canChange) {
  const std::vector<mods::Mod>& all = mods::installed();
  std::string status = g.status;
  if (!g.cause.empty()) {
    status = g.cause;
  } else if (status.empty()) {
    if (!canChange) status = i18n::text("panel.status.in-lobby");
    else if (same_choice(g.pending, g.saved)) status = i18n::text("panel.status.unchanged");
    else status = i18n::text("panel.status.changed");
  }
  if (!g.note.empty()) status = g.note + "\n" + status;
  std::string shown = status + (canChange ? '+' : '-');
  std::vector<std::string> texts;  // each mod's description, which an Apply can change
  for (size_t i = 0; i < all.size(); ++i) {
    shown += modlist::enabled(g.pending, all[i].manifest.id) ? '1' : '0';
    texts.push_back(row_text(all[i].manifest));
    shown += texts.back() + '\n';
  }
  if (shown == g.shown) return;
  g.shown = shown;
  for (size_t i = 0; i < all.size() && i < g.panel.rows.size(); ++i) {
    menu_panel::set_row_checked(g.panel, i, modlist::enabled(g.pending, all[i].manifest.id));
    menu_panel::set_text(g.panel.rows[i].description.get(), texts[i]);
  }
  menu_panel::set_rows_interactable(g.panel, canChange);  // the game greys a checkbox it does not let you click
  menu_panel::set_text(g.panel.body, status);
}

// What the rows start from. Out of a room, the saved choice, whether or not each of those mods could run: Apply saves
// what the panel holds, so a mod left off by this game build must stay chosen for the build that can run it. In a room,
// which nothing here changes, what runs there.
void read_pending(bool canChange) {
  if (!g.cause.empty()) g.pending.clear();
  else g.pending = canChange ? mods::chosen() : mods::started_ids();
  g.saved = g.pending;
  g.changeable = canChange;
  g.status.clear();
  g.note.clear();
  g.shown.clear();
}

void open_panel(bool canChange) {
  read_pending(canChange);
  const std::vector<std::string> buttons = g.cause.empty() ? std::vector<std::string>{i18n::text("panel.apply"), i18n::text("panel.close")}
                                                          : std::vector<std::string>{i18n::text("panel.close")};
  if (!menu_panel::open(g.panel, i18n::text("panel.title"), "", buttons)) {
    PM_LOG("mods panel: the game's popup could not be copied");
    return;
  }
  const std::vector<mods::Mod>& all = mods::installed();
  const int found = static_cast<int>(all.size());
  const int count = found + static_cast<int>(mods::refused().size());
  for (int i = 0; i < count; ++i) {
    // The mods that stay off come last, greyed and saying why, so the rows before them are the installed mods' indices.
    const bool made = i < found ? menu_panel::add_row(g.panel, i, count, row_name(all[i].manifest), row_text(all[i].manifest))
                                : menu_panel::add_row(g.panel, i, count, mods::refused()[i - found].name,
                                                      i18n::text("panel.refused", {{"reason", mods::refused()[i - found].why}}), true);
    if (!made) {
      PM_LOG("mods panel: native checkbox or row could not be copied");
      menu_panel::close(g.panel);
      return;
    }
  }
  refresh(canChange);
}

/** Switch a mod, with what it requires (on) or what requires it (off), and say so when that is more. Optional dependencies switch nothing. */
void toggle(const std::string& id) {
  const bool on = !modlist::enabled(g.pending, id);
  const std::vector<modlist::Manifest> all = mods::manifests();
  std::vector<std::string> next = moddeps::switched(g.pending, all, id, on);
  g.note = moddeps::switch_note(all, g.pending, next, id, on);
  g.pending = std::move(next);
  g.status.clear();
}

}  // namespace

void report() {
  PM_LOG("mods panel: %s", available() ? "available" : "unavailable (mods are chosen in the launcher only)");
}

void tick(bool canChange) {
  if (!available()) return;
  void* menu = phasmo::ConnectToServer::get_Instance();
  if (g.tape.get() && (!hostgame::alive(g.tape.get()) || g.owner.get() != menu)) {
    if (hostgame::alive(g.tape.get())) hostgame::destroy(hostgame::game_object_of(g.tape.get()));
    menu_panel::close(g.panel);
    g.tape = nullptr;
    menu_panel::release(g.control);
    g.owner = nullptr;
    g.nextSearch = 0;
  }
  if (!hostgame::alive(menu)) {  // between scenes
    menu_panel::close(g.panel);
    return;
  }
  // Shown and hidden with the game's own buttons beside it, which stay on the board while the camera
  // looks at another one (the map, the shop); it is clicked only on the focused board.
  const bool shown = menu_panel::menu_buttons_shown();
  const ULONGLONG now = GetTickCount64();
  if (shown && !g.tape.get() && g.refusedBy.get() != menu && now >= g.nextSearch) {
    g.nextSearch = now + kSearchMs;
    make_tape(menu);
  }
  if (hostgame::alive(g.tape.get())) hostgame::set_active(hostgame::game_object_of(g.tape.get()), shown);
  // Taken every frame, so a click the game gave while the panel was open or the board out of view opens nothing later.
  const bool tapeClicked = menu_panel::clicked(g.control);
  if (!menu_panel::main_menu_visible()) {
    menu_panel::close(g.panel);
    return;
  }
  if (menu_panel::is_open(g.panel)) {
    if (canChange != g.changeable) read_pending(canChange);  // into or out of a room with the panel open
    refresh(canChange);
    const int button = menu_panel::tick(g.panel);
    const int row = menu_panel::row_clicked(g.panel);
    const std::vector<mods::Mod>& all = mods::installed();
    if (row >= 0) {
      // The game flipped the row's checkbox; the next refresh sets every row to the choice, this one included.
      if (canChange && static_cast<size_t>(row) < all.size()) toggle(all[row].manifest.id);
      g.shown.clear();
      refresh(canChange);
    }
    if (button == 0 && canChange) {
      if (same_choice(g.pending, g.saved)) {
        menu_panel::close(g.panel);
      } else {
        g.request = g.pending;
        g.requested = true;
        g.status = i18n::text("panel.status.restarting");
      }
    }
    if (button == (g.cause.empty() ? 1 : 0)) menu_panel::close(g.panel);
    return;
  }
  if (tapeClicked) open_panel(canChange);
}

void tick_off(const std::string& cause) {
  g.cause = cause;
  tick(false);
}

bool take_choice(std::vector<std::string>& ids) {
  if (!g.requested) return false;
  g.requested = false;
  ids = g.request;
  return true;
}

void applied(bool ok, bool saved) {
  g.pending = g.request;
  g.saved = g.request;
  if (!saved) g.status = i18n::text("panel.status.not-saved");
  else g.status = ok ? i18n::text("panel.status.done") : i18n::text("panel.status.some-failed");
}

void teardown() {
  menu_panel::close(g.panel);
  menu_panel::release(g.control);
  if (hostgame::alive(g.tape.get())) hostgame::destroy(hostgame::game_object_of(g.tape.get()));
  g = State{};
}

}  // namespace pm::mods_panel
