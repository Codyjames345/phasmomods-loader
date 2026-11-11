// The dev menu on screen. One overlay canvas over the game's 1920x1080 layout: the six sections down the
// left, the current section's rows on the right, each row one line of plain text over a click area (the
// user's call, 2026-11-10: the menu itself needs no vanilla widget dressing). Every value shown is reread
// from the game every frame; every click goes through menu_rule.h's refuse() before actions.cpp runs it,
// and a refusal shows its reason on the bottom line rather than working around a game guard.
#include "menu.h"

#include <cstdio>
#include <string>
#include <vector>

#include "actions.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "text_table.h"
#include "ui.h"

namespace pm::devmenu::menu {
namespace {

namespace i18n = pm::i18n;

// The labels live in the mod's table (the wording is a PM_TEXT_AI placeholder; the user writes the final
// text). i18n::key names each one in the code, so the table checks count it as asked for.
constexpr const char* kTitleKey = i18n::key("dev-menu.title");
constexpr const char* kHintKey = i18n::key("dev-menu.hint");
constexpr const char* kOnKey = i18n::key("dev-menu.on");
constexpr const char* kOffKey = i18n::key("dev-menu.off");
constexpr const char* kRefusedKey = i18n::key("dev-menu.refused");
constexpr const char* kWhyKeys[] = {nullptr, i18n::key("dev-menu.why.not-offline"),
                                    i18n::key("dev-menu.why.no-contract"),
                                    i18n::key("dev-menu.why.no-ghost"),
                                    i18n::key("dev-menu.why.no-difficulty"),
                                    i18n::key("dev-menu.why.no-player"),
                                    i18n::key("dev-menu.why.hunting"), i18n::key("dev-menu.why.not-hunting"),
                                    i18n::key("dev-menu.why.no-rooms"), i18n::key("dev-menu.why.dead"),
                                    i18n::key("dev-menu.why.alive")};

// The sections, in the order they show down the left.
constexpr int kSections = 6;
constexpr const char* kSectionKeys[kSections] = {i18n::key("dev-menu.section.difficulty"),
                                                 i18n::key("dev-menu.section.contract"),
                                                 i18n::key("dev-menu.section.ghost"),
                                                 i18n::key("dev-menu.section.player"),
                                                 i18n::key("dev-menu.section.equipment"),
                                                 i18n::key("dev-menu.section.game")};

// What a row shows after its label. Each row names one of these; the value is read from the game now.
enum ValueKind {
  NoValue,
  OnFriendly, OnLoseEquipment, CountEvidence, NameType, NumberReward, NumberPayout, LineState, LineIdentity,
  NameRoom, NumberSpeed, PickRoom, OnEvents, PickEvent, NumberSanity, OnFrozen, OnGod, OnKeep, NumberScale,
  OnPause, OnSession, OnOffline,
};

enum Kind { Nav, Click, Read };

struct Row {
  Kind kind = Read;
  int section = 0;
  Action action = Action::Info;  // what a click runs (Nav and Read rows are never clicked)
  int arg = 0;                   // a row's parameter: a delta, a sanity value, a direction
  const char* label = "";        // the row's words, a key in the mod's table
  ValueKind value = NoValue;
  void* hit = nullptr;           // the invisible click area
  void* text = nullptr;          // the line itself
};

api::Held g_canvas;
bool g_built = false;     // the canvas and its rows exist (they are kept between openings)
bool g_active = false;    // the rows are shown
int g_section = 0;
int g_roomPick = 0;       // the room picker's cursor index
int g_eventPick = 0;      // the event picker's cursor index
Pause g_pause;
bool g_wasCursorLocked = true;
Why g_refusal = Why::None;  // the last refused click, shown until a click works
std::vector<Row> g_rows;
void* g_title = nullptr;
void* g_hint = nullptr;
void* g_refusalText = nullptr;

std::string onOff(bool on) { return i18n::text(on ? kOnKey : kOffKey); }

std::string number(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
  return buf;
}

std::string whole(int v) { return std::to_string(v); }

// The pickers' lines: the game's own name and index, since a developer reads both.
std::string roomPickText() {
  const std::string name = actions::room_name(g_roomPick);
  return name.empty() ? whole(g_roomPick) : name + " (" + whole(g_roomPick) + ")";
}

std::string eventPickText() {
  const std::vector<actions::EventKind> kinds = actions::event_kinds();
  if (g_eventPick < 0 || static_cast<size_t>(g_eventPick) >= kinds.size()) return whole(g_eventPick);
  return std::string(kinds[g_eventPick].name) + " (" + whole(g_eventPick) + ")";
}

std::string valueText(const Row& r) {
  switch (r.value) {
    case NoValue: return {};
    case OnFriendly: return onOff(actions::friendly());
    case OnLoseEquipment: return onOff(actions::lose_equipment());
    case CountEvidence: return whole(actions::evidence_given());
    case NameType: return actions::forced_type_name();
    case NumberReward: return number(actions::reward_multiplier());
    case NumberPayout: return whole(actions::bonus_reward());
    case LineState: return actions::ghost_line();
    case LineIdentity: return actions::ghost_identity();
    case NameRoom: return actions::favourite_room_name();
    case NumberSpeed: return number(actions::base_speed());
    case PickRoom: return roomPickText();
    case OnEvents: return onOff(actions::events_blocked());
    case PickEvent: return eventPickText();
    case NumberSanity: return number(actions::sanity());
    case OnFrozen: return onOff(actions::frozen());
    case OnGod: return onOff(actions::switches().godMode);
    case OnKeep: return onOff(actions::switches().keepConsumables);
    case NumberScale: return number(actions::time_scale());
    case OnPause: return onOff(actions::switches().pauseWhenOpen);
    case OnSession: return onOff(actions::session_on());
    case OnOffline: return onOff(actions::offline());
  }
  return {};
}

// ---- The rows, in the order they show. ----

std::vector<Row> sectionRows(int section) {
  std::vector<Row> rows;
  const auto add = [&](Kind kind, Action action, int arg, const char* label, ValueKind value) {
    Row r;
    r.kind = kind;
    r.section = section;
    r.action = action;
    r.arg = arg;
    r.label = label;
    r.value = value;
    rows.push_back(r);
  };
  switch (section) {
    case 0:  // Difficulty
      add(Click, Action::SetFriendly, 0, i18n::key("dev-menu.friendly"), OnFriendly);
      add(Click, Action::SetLoseEquipment, 0, i18n::key("dev-menu.lose-equipment"), OnLoseEquipment);
      add(Read, Action::Info, 0, i18n::key("dev-menu.evidence"), CountEvidence);
      add(Click, Action::BumpEvidence, -1, i18n::key("dev-menu.evidence-down"), NoValue);
      add(Click, Action::BumpEvidence, +1, i18n::key("dev-menu.evidence-up"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.ghost-type"), NameType);
      add(Click, Action::BumpType, -1, i18n::key("dev-menu.ghost-type-down"), NoValue);
      add(Click, Action::BumpType, +1, i18n::key("dev-menu.ghost-type-up"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.reward"), NumberReward);
      break;
    case 1:  // Contract
      add(Click, Action::UnlockDoor, 0, i18n::key("dev-menu.unlock-door"), NoValue);
      add(Click, Action::Leave, 0, i18n::key("dev-menu.leave"), NoValue);
      add(Click, Action::Quit, 0, i18n::key("dev-menu.quit"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.payout"), NumberPayout);
      break;
    case 2:  // Ghost
      add(Read, Action::Info, 0, i18n::key("dev-menu.state"), LineState);
      add(Read, Action::Info, 0, i18n::key("dev-menu.identity"), LineIdentity);
      add(Read, Action::Info, 0, i18n::key("dev-menu.room"), NameRoom);
      add(Read, Action::Info, 0, i18n::key("dev-menu.base-speed"), NumberSpeed);
      add(Click, Action::ForceHunt, 0, i18n::key("dev-menu.force-hunt"), NoValue);
      add(Click, Action::EndHunt, 0, i18n::key("dev-menu.end-hunt"), NoValue);
      add(Click, Action::ClearCooldown, 0, i18n::key("dev-menu.clear-cooldown"), NoValue);
      add(Click, Action::Appear, 0, i18n::key("dev-menu.appear"), NoValue);
      add(Click, Action::UnAppear, 0, i18n::key("dev-menu.unappear"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.room-pick"), PickRoom);
      add(Click, Action::PickRoom, -1, i18n::key("dev-menu.room-prev"), NoValue);
      add(Click, Action::PickRoom, +1, i18n::key("dev-menu.room-next"), NoValue);
      add(Click, Action::ApplyRoom, 0, i18n::key("dev-menu.room-apply"), NoValue);
      add(Click, Action::RandomRoom, 0, i18n::key("dev-menu.room-random"), NoValue);
      add(Click, Action::BumpSpeed, -1, i18n::key("dev-menu.speed-down"), NoValue);
      add(Click, Action::BumpSpeed, +1, i18n::key("dev-menu.speed-up"), NoValue);
      add(Click, Action::ToggleEvents, 0, i18n::key("dev-menu.block-events"), OnEvents);
      add(Read, Action::Info, 0, i18n::key("dev-menu.event"), PickEvent);
      add(Click, Action::FireEvent, -1, i18n::key("dev-menu.event-prev"), NoValue);
      add(Click, Action::FireEvent, +1, i18n::key("dev-menu.event-next"), NoValue);
      add(Click, Action::FireEvent, 0, i18n::key("dev-menu.event-fire"), NoValue);
      break;
    case 3:  // Player
      add(Read, Action::Info, 0, i18n::key("dev-menu.sanity"), NumberSanity);
      add(Click, Action::SetSanity, 0, i18n::key("dev-menu.sanity-0"), NoValue);
      add(Click, Action::SetSanity, 50, i18n::key("dev-menu.sanity-50"), NoValue);
      add(Click, Action::SetSanity, 100, i18n::key("dev-menu.sanity-100"), NoValue);
      add(Click, Action::ToggleFreeze, 0, i18n::key("dev-menu.freeze"), OnFrozen);
      add(Click, Action::KillLocal, 0, i18n::key("dev-menu.die"), NoValue);
      add(Click, Action::ReviveLocal, 0, i18n::key("dev-menu.revive"), NoValue);
      add(Click, Action::SetGodMode, 0, i18n::key("dev-menu.god-mode"), OnGod);
      break;
    case 4:  // Equipment
      add(Click, Action::SetKeepConsumables, 0, i18n::key("dev-menu.keep-consumables"), OnKeep);
      break;
    case 5:  // Game
      add(Click, Action::CycleTimeScale, 0, i18n::key("dev-menu.tick-speed"), NumberScale);
      add(Click, Action::SetPauseWhenOpen, 0, i18n::key("dev-menu.pause-when-open"), OnPause);
      add(Read, Action::Info, 0, i18n::key("dev-menu.session"), OnSession);
      add(Read, Action::Info, 0, i18n::key("dev-menu.offline-state"), OnOffline);
      break;
  }
  return rows;
}

const ui::Color kText{0.92f, 0.92f, 0.92f, 1.0f};
const ui::Color kDim{0.65f, 0.65f, 0.65f, 1.0f};
const ui::Color kSelect{0.98f, 0.85f, 0.37f, 1.0f};
const ui::Color kRefuse{1.0f, 0.45f, 0.45f, 1.0f};
const ui::Color kBack{0.0f, 0.0f, 0.0f, 0.72f};
const ui::Color kHit{0.0f, 0.0f, 0.0f, 0.0f};  // a click area you cannot see

// The layout, in canvas units about the canvas centre (the emotes wheel's convention).
constexpr float kTop = 380.0f;      // the first row's line
constexpr float kPitch = 34.0f;     // line to line
constexpr float kNavX = -970.0f;    // the section column's left edge
constexpr float kRowX = -560.0f;    // the rows' left edge
constexpr float kRowWide = 1460.0f;  // a row's click area width

float rowY(int index) { return kTop - static_cast<float>(index) * kPitch; }

void setSectionShown(int section, bool on) {
  for (Row& r : g_rows) {
    if (r.kind == Nav || r.section != section) continue;
    if (r.hit) ui::set_active(r.hit, on);
    if (r.text) ui::set_active(r.text, on);
  }
}

void refreshNav() {
  for (Row& r : g_rows) {
    if (r.kind != Nav || !r.text) continue;
    const bool sel = r.arg == g_section;
    ui::set_color(r.text, sel ? kSelect : kDim);
    const std::string nav = (sel ? "> " : "   ") + i18n::text(r.label);
    ui::set_text(r.text, nav.c_str());
  }
}

bool build() {
  void* canvas = ui::create_canvas("pm dev menu", 30000);
  if (!canvas) return false;
  g_canvas = canvas;
  const ui::Vec2 mid{0.5f, 0.5f};
  if (void* back = ui::create_image(canvas, "pm dev menu back", kBack))
    ui::place(back, mid, mid, {0, 0}, {1920, 1080});
  g_title = ui::create_text(canvas, "pm dev menu title", 42.0f);
  if (g_title) {
    ui::set_color(g_title, kText);
    ui::place(g_title, mid, mid, {0, kTop + 60}, {1500, 54});
    ui::set_text(g_title, i18n::text(kTitleKey).c_str());
  }
  g_hint = ui::create_text(canvas, "pm dev menu hint", 24.0f);
  if (g_hint) {
    ui::set_color(g_hint, kDim);
    ui::place(g_hint, mid, mid, {0, kTop + 20}, {1500, 30});
    ui::set_text(g_hint, i18n::text(kHintKey).c_str());
  }
  g_refusalText = ui::create_text(canvas, "pm dev menu refusal", 26.0f);
  if (g_refusalText) {
    ui::set_color(g_refusalText, kRefuse);
    ui::place(g_refusalText, mid, mid, {0, -kTop - 60}, {1700, 34});
    ui::set_text(g_refusalText, "");
  }

  g_rows.clear();
  for (int s = 0; s < kSections; ++s) {
    Row nav;
    nav.kind = Nav;
    nav.arg = s;
    nav.label = kSectionKeys[s];
    g_rows.push_back(nav);
  }
  for (int s = 0; s < kSections; ++s)
    for (const Row& r : sectionRows(s)) g_rows.push_back(r);

  int navAt = 0;
  std::vector<int> rowAt(kSections, 0);
  for (Row& r : g_rows) {
    if (r.kind == Nav) {
      const float y = rowY(navAt) - 80.0f;
      ++navAt;
      r.hit = ui::create_image(canvas, "pm dev menu nav", kHit);
      if (r.hit) ui::place(r.hit, mid, mid, {kNavX + 200, y}, {400, kPitch});
      r.text = ui::create_text(canvas, "pm dev menu nav text", 28.0f);
      if (r.text) {
        ui::set_color(r.text, kDim);
        ui::place(r.text, mid, {0.0f, 0.5f}, {kNavX, y}, {400, kPitch});
      }
      continue;
    }
    const float y = rowY(rowAt[r.section]++);
    if (r.kind == Click) {
      r.hit = ui::create_image(canvas, "pm dev menu row", kHit);
      if (r.hit) ui::place(r.hit, mid, mid, {kRowX + kRowWide / 2, y}, {kRowWide, kPitch});
    }
    r.text = ui::create_text(canvas, "pm dev menu row text", 26.0f);
    if (r.text) {
      ui::set_color(r.text, r.kind == Read ? kDim : kText);
      ui::place(r.text, mid, {0.0f, 0.5f}, {kRowX, y}, {kRowWide, kPitch});
    }
  }
  // Every section exists; only the current one is shown.
  for (int s = 0; s < kSections; ++s) setSectionShown(s, s == g_section);
  g_built = true;
  return true;
}

// Closing hides the whole canvas, rows, nav column and background together: the section switches hide
// single rows, closing hides everything at once, so nothing of the menu can be left over the game.
void show(bool on) {
  g_active = on;
  if (g_canvas) ui::set_active(g_canvas, on);
  refreshNav();
}

// The canvas died under us (a scene change while the menu was open): put the game back as open() found
// it and forget the rows; the next open builds fresh ones.
void recover() {
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  if (g_wasCursorLocked) game::lock_cursor();
  g_active = false;
  g_built = false;
  g_canvas = nullptr;
  g_rows.clear();
  g_title = g_hint = g_refusalText = nullptr;
  g_refusal = Why::None;
}

// ---- Clicks ----

std::string whyText(Why w) {
  const int i = static_cast<int>(w);
  const int last = static_cast<int>(sizeof kWhyKeys / sizeof kWhyKeys[0]);
  if (i <= 0 || i >= last) return {};
  return i18n::text(kRefusedKey, {{"why", i18n::text(kWhyKeys[i])}});
}

// The forced-type option that names the game's current override, or 0 (the game's Random) when none does.
int typeOptionNow() {
  const int want = actions::forced_type();
  const int options = actions::forced_type_options();
  for (int o = 0; o < options; ++o)
    if (actions::forced_type_value(o) == want) return o;
  return 0;
}

void run(const Row& r) {
  const Can c = actions::snapshot();
  if (const Why w = refuse(r.action, c); w != Why::None) {
    g_refusal = w;
    return;
  }
  g_refusal = Why::None;
  Switches& s = actions::switches();
  switch (r.action) {
    case Action::UnlockDoor: actions::unlock_main_door(); break;
    case Action::Leave:
      close();
      actions::leave();
      break;
    case Action::Quit:
      close();
      actions::quit();
      break;
    case Action::ForceHunt: actions::force_hunt(); break;
    case Action::EndHunt: actions::end_hunt(); break;
    case Action::ClearCooldown: actions::clear_cooldown(); break;
    case Action::Appear: actions::appear(); break;
    case Action::UnAppear: actions::unappear(); break;
    case Action::ApplyRoom: actions::apply_room(g_roomPick); break;
    case Action::RandomRoom: {
      const int i = actions::random_room();
      if (i >= 0) g_roomPick = i;
      break;
    }
    case Action::PickRoom: {
      const int n = actions::room_count();
      if (n > 0) g_roomPick = ((g_roomPick + r.arg) % n + n) % n;
      break;
    }
    case Action::BumpSpeed: actions::bump_base_speed(static_cast<float>(r.arg) * 0.1f); break;
    case Action::ToggleEvents: actions::set_events_blocked(!actions::events_blocked()); break;
    case Action::FireEvent: {
      const std::vector<actions::EventKind> kinds = actions::event_kinds();
      const int n = static_cast<int>(kinds.size());
      if (r.arg != 0) {
        if (n > 0) g_eventPick = ((g_eventPick + r.arg) % n + n) % n;
      } else if (g_eventPick >= 0 && g_eventPick < n) {
        actions::fire_event(kinds[g_eventPick].value);
      }
      break;
    }
    case Action::SetFriendly: actions::set_friendly(!actions::friendly()); break;
    case Action::SetLoseEquipment: actions::set_lose_equipment(!actions::lose_equipment()); break;
    case Action::BumpEvidence: actions::bump_evidence(r.arg); break;
    case Action::BumpType: {
      const int options = actions::forced_type_options();
      int pick = 0;
      if (options > 0) pick = ((typeOptionNow() + r.arg) % options + options) % options;
      actions::set_forced_type(actions::forced_type_value(pick));
      break;
    }
    case Action::SetSanity: actions::set_sanity(r.arg); break;
    case Action::ToggleFreeze: actions::set_frozen(!actions::frozen()); break;
    case Action::KillLocal: actions::kill_local(); break;
    case Action::ReviveLocal: actions::revive_local(); break;
    case Action::SetGodMode: s.godMode = !s.godMode; break;
    case Action::SetKeepConsumables: s.keepConsumables = !s.keepConsumables; break;
    case Action::SetPauseWhenOpen: {
      s.pauseWhenOpen = !s.pauseWhenOpen;
      if (s.pauseWhenOpen && !g_pause.frozen) {
        actions::set_time_scale(pause_open(g_pause, actions::time_scale(), true));
      } else if (!s.pauseWhenOpen && g_pause.frozen) {
        float restore = 0.0f;
        if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
      }
      break;
    }
    case Action::CycleTimeScale: {
      const float v = next_time_scale(actions::time_scale());
      actions::set_time_scale(v);
      if (g_pause.frozen) g_pause.saved = v;  // closing restores the new speed, not the old one
      break;
    }
    case Action::Info: break;
  }
}

// Reread every shown row's value, and the refusal line.
void refresh() {
  for (Row& r : g_rows) {
    if (r.kind == Nav || !r.text) continue;
    const std::string value = valueText(r);
    const std::string line = value.empty() ? i18n::text(r.label) : i18n::text(r.label) + "  " + value;
    ui::set_text(r.text, line.c_str());
  }
  if (g_refusalText)
    ui::set_text(g_refusalText, g_refusal == Why::None ? "" : whyText(g_refusal).c_str());
}

}  // namespace

bool is_open() { return g_active && g_canvas && game::alive(g_canvas); }

void open() {
  if (is_open()) return;
  actions::note_session();  // the dev session starts: from here the contract can earn no progress
  if (g_built && (!g_canvas || !game::alive(g_canvas))) recover();  // a scene change took the canvas away
  if (!g_built && !build()) {
    PM_LOG("dev-menu: the menu canvas could not be made");
    return;
  }
  g_wasCursorLocked = game::cursor_locked();
  show(true);
  game::free_cursor();
  const float scale = pause_open(g_pause, actions::time_scale(), actions::switches().pauseWhenOpen);
  if (g_pause.frozen) actions::set_time_scale(scale);
}

void close() {
  if (!is_open()) return;
  show(false);
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  if (g_wasCursorLocked) game::lock_cursor();
}

void tick() {
  if (!g_active) return;
  if (!g_canvas || !game::alive(g_canvas)) {
    recover();  // the scene changed under the menu: the game goes back to how open() found it
    return;
  }
  refresh();
  float x = 0.0f, y = 0.0f;
  if (!game::mouse_left_pressed() || !game::mouse_position(x, y)) return;
  const ui::Vec2 pt{x, y};
  for (Row& r : g_rows) {
    if (r.kind != Nav) continue;
    if (r.hit && ui::contains(r.hit, pt)) {
      if (r.arg != g_section) {
        setSectionShown(g_section, false);
        g_section = r.arg;
        setSectionShown(g_section, true);
        refreshNav();
      }
      return;
    }
  }
  for (Row& r : g_rows) {
    if (r.kind != Click || r.section != g_section) continue;
    if (r.hit && ui::contains(r.hit, pt)) {
      run(r);
      return;
    }
  }
}

void destroy() {
  // pmmod_stop must not leave the game frozen or the cursor loose because the menu was open.
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  if (g_active && g_wasCursorLocked) game::lock_cursor();
  if (g_canvas) ui::destroy(g_canvas);
  g_canvas = nullptr;
  g_built = false;
  g_active = false;
  g_rows.clear();
  g_title = g_hint = g_refusalText = nullptr;
  g_refusal = Why::None;
  g_pause = Pause{};
}

}  // namespace pm::devmenu::menu
