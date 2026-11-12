// The dev menu on screen. One overlay canvas over the game's 1920x1080 layout: the six sections down the
// left, the current section's rows on the right, each row a bar carrying the game's own Button component
// with its line of text over it, so hover, press, selection and arrow-key navigation are all the game's own
// (the user's live-test ruling, 2026-11-11: hand-polled click areas gave none of that). The canvas gets a
// GraphicRaycaster, and while the menu is open the game's UI input module runs again (the game keeps it off
// outside its journal) and the player is held in place as the journal holds them. Every value shown is
// reread from the game every frame; every click goes through menu_rule.h's refuse() before actions.cpp runs
// it, and a refusal shows its reason on the bottom line rather than working around a game guard.
#include "menu.h"

#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "actions.h"
#include "dummy.h"
#include "buttons.h"
#include "menu_layout.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "managed_exception.h"
#include "UnityObject.h"
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
                                    i18n::key("dev-menu.why.no-truck"), i18n::key("dev-menu.why.friendly"),
                                    i18n::key("dev-menu.why.no-rig"), i18n::key("dev-menu.why.alive"),
                                    i18n::key("dev-menu.why.dying"), i18n::key("dev-menu.why.no-dummy")};
static_assert(sizeof kWhyKeys / sizeof kWhyKeys[0] == static_cast<size_t>(Why::NoDummy) + 1,
              "every refusal reason has its text, in the order of menu_rule.h's Why");

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
  NameRoom, NumberSpeed, PickRoom, OnEvents, PickEvent, NumberSanity, OnFrozen, OnGod, NumberScale,
  OnPause, OnSession, OnOffline, NumberMultiplier, OnBlind, OnDebugHud, LineEvidence, OnNoclip, OnLockSanity,
  NumberLockedSanity, OnEspGhost, OnEspRooms, NameWeather, PickWeatherValue, PickRetypeValue, OnUnlockAll, OnFreeShop, OnSaveFrozen, OnDummy,
};

enum Kind { Nav, Click, Read };

struct Row {
  Kind kind = Read;
  int section = 0;
  Action action = Action::Info;  // what a click runs (Nav and Read rows are never clicked)
  int arg = 0;                   // a row's parameter: a delta, a sanity value, a direction
  const char* label = "";        // the row's words, a key in the mod's table
  ValueKind value = NoValue;
  void* hit = nullptr;           // the row's bar: the object the game's Button sits on
  void* hitGo = nullptr;         // that bar's GameObject, which is what the game's selection moves onto
  void* text = nullptr;          // the line itself
};

api::Held g_canvas;
bool g_built = false;     // the canvas and its rows exist (they are kept between openings)
bool g_active = false;    // the rows are shown
int g_section = 0;
int g_roomPick = 0;       // the room picker's cursor index
int g_eventPick = 0;      // the event picker's cursor index
int g_weatherPick = 0;    // the weather picker's cursor index
int g_retypePick = 0;     // the re-type picker's cursor index
Pause g_pause;
bool g_wasCursorLocked = true;
// Unity's UI input module, borrowed while the menu is open: the game runs it only while its journal is
// open, and without it no canvas takes clicks and no menu takes arrow keys.
void* g_module = nullptr;
bool g_moduleWas = false;
// Whether open() asked the game to hold the player (actions::pause_player answered true): only a hold the
// menu took is re-asserted while it is open and released when it closes.
bool g_held = false;
// The rows whose read has thrown once already: logged once each, then shown empty.
std::vector<bool> g_readFailed;
Why g_refusal = Why::None;  // the last refused click, shown until a click works
std::vector<Row> g_rows;
void* g_title = nullptr;
void* g_hint = nullptr;
void* g_refusalText = nullptr;
// The rows' list, on the game's own ScrollRect and clip (the user's original brief asked for a ScrollRect).
// Its parts are null when the game's classes did not resolve; the menu still works, the rows just do not move.
ui::ScrollView g_view;

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
    case NumberScale: return number(actions::time_scale());
    case OnPause: return onOff(actions::switches().pauseWhenOpen);
    case OnSession: return onOff(actions::session_on());
    case OnOffline: return onOff(actions::offline());
    case NumberMultiplier: return number(actions::speed_multiplier());
    case OnBlind: return onOff(actions::ghost_blind());
    case OnDebugHud: return onOff(actions::switches().debugHud);
    case LineEvidence: return actions::given_evidence();
    case OnNoclip: return onOff(actions::noclip());
    case OnLockSanity: return onOff(actions::switches().lockSanity);
    case NumberLockedSanity: return whole(actions::switches().lockedSanity);
    case OnEspGhost: return onOff(actions::switches().espGhost);
    case OnEspRooms: return onOff(actions::switches().espRooms);
    case NameWeather: return actions::weather_now();
    case OnUnlockAll: return onOff(actions::switches().unlockAll);
    case OnDummy: return onOff(dummy::available());
    case OnFreeShop: return onOff(actions::switches().freeShop);
    case OnSaveFrozen: return onOff(freeze_save(actions::session_on()));
    case PickWeatherValue: return actions::weather_name(g_weatherPick);
    case PickRetypeValue: return actions::type_name(actions::type_option(g_retypePick));
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
      add(Read, Action::Info, 0, i18n::key("dev-menu.weather"), NameWeather);
      add(Read, Action::Info, 0, i18n::key("dev-menu.weather-pick"), PickWeatherValue);
      add(Click, Action::PickWeather, -1, i18n::key("dev-menu.weather-prev"), NoValue);
      add(Click, Action::PickWeather, +1, i18n::key("dev-menu.weather-next"), NoValue);
      add(Click, Action::ApplyWeather, 0, i18n::key("dev-menu.weather-apply"), NoValue);
      add(Click, Action::EndContract, 0, i18n::key("dev-menu.end-contract"), NoValue);
      add(Click, Action::Leave, 0, i18n::key("dev-menu.leave"), NoValue);
      add(Click, Action::Quit, 0, i18n::key("dev-menu.quit"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.payout"), NumberPayout);
      break;
    case 2:  // Ghost
      add(Read, Action::Info, 0, i18n::key("dev-menu.state"), LineState);
      add(Read, Action::Info, 0, i18n::key("dev-menu.identity"), LineIdentity);
      add(Read, Action::Info, 0, i18n::key("dev-menu.evidence-given"), LineEvidence);
      add(Read, Action::Info, 0, i18n::key("dev-menu.retype-pick"), PickRetypeValue);
      add(Click, Action::PickRetype, -1, i18n::key("dev-menu.retype-prev"), NoValue);
      add(Click, Action::PickRetype, +1, i18n::key("dev-menu.retype-next"), NoValue);
      add(Click, Action::Retype, 0, i18n::key("dev-menu.retype-apply"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.room"), NameRoom);
      add(Read, Action::Info, 0, i18n::key("dev-menu.base-speed"), NumberSpeed);
      add(Click, Action::ForceHunt, 0, i18n::key("dev-menu.force-hunt"), NoValue);
      add(Click, Action::EndHunt, 0, i18n::key("dev-menu.end-hunt"), NoValue);
      add(Click, Action::ClearCooldown, 0, i18n::key("dev-menu.clear-cooldown"), NoValue);
      add(Click, Action::Appear, 0, i18n::key("dev-menu.appear"), NoValue);
      add(Click, Action::UnAppear, 0, i18n::key("dev-menu.unappear"), NoValue);
      add(Click, Action::FireState, 0, i18n::key("dev-menu.interact-now"), NoValue);
      add(Click, Action::FireState, 1, i18n::key("dev-menu.ability-now"), NoValue);
      add(Click, Action::FireState, 2, i18n::key("dev-menu.throw-now"), NoValue);
      add(Click, Action::GhostToMe, 0, i18n::key("dev-menu.ghost-to-me"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.room-pick"), PickRoom);
      add(Click, Action::PickRoom, -1, i18n::key("dev-menu.room-prev"), NoValue);
      add(Click, Action::PickRoom, +1, i18n::key("dev-menu.room-next"), NoValue);
      add(Click, Action::ApplyRoom, 0, i18n::key("dev-menu.room-apply"), NoValue);
      add(Click, Action::RandomRoom, 0, i18n::key("dev-menu.room-random"), NoValue);
      add(Click, Action::BumpSpeed, -1, i18n::key("dev-menu.ghost-speed-down"), NoValue);
      add(Click, Action::BumpSpeed, +1, i18n::key("dev-menu.ghost-speed-up"), NoValue);
      add(Click, Action::ToggleEvents, 0, i18n::key("dev-menu.block-events"), OnEvents);
      add(Click, Action::SetGhostBlind, 0, i18n::key("dev-menu.ghost-blind"), OnBlind);
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
      add(Click, Action::SetLockSanity, 0, i18n::key("dev-menu.sanity-lock"), OnLockSanity);
      add(Click, Action::CycleLockedSanity, 0, i18n::key("dev-menu.sanity-lock-value"), NumberLockedSanity);
      add(Click, Action::ToggleFreeze, 0, i18n::key("dev-menu.freeze"), OnFrozen);
      add(Click, Action::KillLocal, 0, i18n::key("dev-menu.die"), NoValue);
      add(Click, Action::ReviveLocal, 0, i18n::key("dev-menu.revive"), NoValue);
      add(Click, Action::SetGodMode, 0, i18n::key("dev-menu.god-mode"), OnGod);
      add(Click, Action::SetNoclip, 0, i18n::key("dev-menu.noclip"), OnNoclip);
      add(Click, Action::MeToGhost, 0, i18n::key("dev-menu.me-to-ghost"), NoValue);
      add(Click, Action::MeToGhostRoom, 0, i18n::key("dev-menu.me-to-ghost-room"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.dummy"), OnDummy);
      add(Click, Action::SpawnDummy, 0, i18n::key("dev-menu.dummy-spawn"), NoValue);
      add(Click, Action::DummyAction, 0, i18n::key("dev-menu.dummy-to-me"), NoValue);
      add(Click, Action::DummyAction, 1, i18n::key("dev-menu.dummy-banshee"), NoValue);
      add(Click, Action::DummyAction, 2, i18n::key("dev-menu.dummy-revive"), NoValue);
      add(Click, Action::DummyAction, 3, i18n::key("dev-menu.dummy-remove"), NoValue);
      add(Read, Action::Info, 0, i18n::key("dev-menu.player-speed"), NumberMultiplier);
      add(Click, Action::BumpPlayerSpeed, -1, i18n::key("dev-menu.player-speed-down"), NoValue);
      add(Click, Action::BumpPlayerSpeed, +1, i18n::key("dev-menu.player-speed-up"), NoValue);
      add(Click, Action::ResetPlayerSpeed, 0, i18n::key("dev-menu.speed-reset"), NoValue);
      break;
    case 4:  // Equipment
      // A dev session keeps every consumable and every brought item whatever happens (actions.cpp's hooks), so
      // the section says so instead of offering a switch that could only ever be on.
      add(Read, Action::Info, 0, i18n::key("dev-menu.equipment-protected"), OnSession);
      add(Read, Action::Info, 0, i18n::key("dev-menu.save-frozen"), OnSaveFrozen);
      add(Click, Action::SetUnlockAll, 0, i18n::key("dev-menu.unlock-all"), OnUnlockAll);
      add(Click, Action::SetFreeShop, 0, i18n::key("dev-menu.free-shop"), OnFreeShop);
      break;
    case 5:  // Game
      add(Click, Action::CycleTimeScale, 0, i18n::key("dev-menu.tick-speed"), NumberScale);
      add(Click, Action::SetPauseWhenOpen, 0, i18n::key("dev-menu.pause-when-open"), OnPause);
      add(Click, Action::SetDebugHud, 0, i18n::key("dev-menu.debug-hud"), OnDebugHud);
      add(Click, Action::SetEspGhost, 0, i18n::key("dev-menu.esp-ghost"), OnEspGhost);
      add(Click, Action::SetEspRooms, 0, i18n::key("dev-menu.esp-rooms"), OnEspRooms);
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
const ui::Color kHit{1.0f, 1.0f, 1.0f, 0.09f};  // the row's bar: the game's Button brightens it on hover

// A row's index travels to its click handler as the wiring's user pointer.
void* toUser(size_t i) { return reinterpret_cast<void*>(static_cast<uintptr_t>(i)); }
size_t fromUser(void* user) { return static_cast<size_t>(reinterpret_cast<uintptr_t>(user)); }

void rowClicked(void* user);
void selectSectionRow();
void borrowInput();
void returnInput();
void releasePlayer();
void refresh();
void fitList();
void followSelection();

// The geometry lives in menu_layout.h, where the offline test checks it: the live test of 2026-11-12 found
// the rows reaching up into the hint line, and a Ghost section too long for the screen, because the layout
// was a handful of constants nothing was compared against.
using namespace layout;

void setSectionShown(int section, bool on) {
  for (Row& r : g_rows) {
    if (r.kind == Nav || r.section != section) continue;
    if (r.hit) ui::set_active(r.hit, on);
    if (r.text) ui::set_active(r.text, on);
  }
}

// The list's length follows the section showing: another section's rows are switched off and take no room,
// so the range the player can scroll through is this section's rows and nothing else.
void fitList() {
  if (!g_view.content) return;
  int rows = 0;
  for (const Row& r : g_rows)
    if (r.kind != Nav && r.section == g_section) ++rows;
  ui::set_scroll_height(g_view, listHeight(rows));
  followSelection();
}

// Arrow keys and a gamepad move the selection inside the game, which tells the menu nothing, so follow it:
// the row the selection has reached comes back into the view. A drag leaves the list where the player put it
// until the selection needs somewhere else.
void followSelection() {
  if (!g_view.scroll) return;
  void* selected = api::buttons::selected();
  if (!selected) return;
  int index = 0;
  for (const Row& r : g_rows) {
    if (r.kind == Nav || r.section != g_section) continue;
    if (r.hitGo == selected) {
      ui::scroll_reveal(g_view, rowTop(index), kPitch);
      return;
    }
    ++index;
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
  api::buttons::catch_pointer(canvas);  // without a GraphicRaycaster the canvas catches no pointer at all
  const ui::Vec2 mid{0.5f, 0.5f};
  if (void* back = ui::create_image(canvas, "pm dev menu back", kBack))
    ui::place(back, mid, mid, {0, 0}, {1920, 1080});
  g_title = ui::create_text(canvas, "pm dev menu title", 42.0f);
  if (g_title) {
    ui::set_color(g_title, kText);
    ui::place(g_title, mid, mid, {0, kTitleY}, {1500, kTitleBox});
    ui::set_text(g_title, i18n::text(kTitleKey).c_str());
  }
  g_hint = ui::create_text(canvas, "pm dev menu hint", 24.0f);
  if (g_hint) {
    ui::set_color(g_hint, kDim);
    ui::place(g_hint, mid, mid, {0, kHintY}, {1500, kHintBox});
    ui::set_text(g_hint, i18n::text(kHintKey).c_str());
  }
  g_refusalText = ui::create_text(canvas, "pm dev menu refusal", 26.0f);
  if (g_refusalText) {
    ui::set_color(g_refusalText, kRefuse);
    ui::place(g_refusalText, mid, mid, {0, kRefuseY}, {1700, kRefuseBox});
    ui::set_text(g_refusalText, "");
  }
  // The rows live in a scroll view (the user's own ask in the original brief), so a section longer than the
  // screen scrolls instead of running out from under the heading. Its height follows the section showing.
  g_view = ui::create_scroll_view(canvas, "pm dev menu list", {kViewX, kViewY}, {kViewWide, kViewHigh},
                                  listHeight(sectionRows(g_section).size()));
  if (!g_view.content) PM_LOG("dev-menu: the game's scroll view could not be made; the rows are placed plainly");

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
  // The rows live under the list's content when the game's scroll view was made, pinned to its top edge so a
  // scroll moves the whole section together. Without one they sit on the canvas where the list would be.
  void* listParent = g_view.content ? static_cast<void*>(g_view.content) : canvas;
  const bool inList = g_view.content != nullptr;
  for (Row& r : g_rows) {
    if (r.kind == Nav) {
      const float y = navOffset(navAt);
      ++navAt;
      r.hit = ui::create_image(canvas, "pm dev menu nav", kHit);
      if (r.hit) ui::place(r.hit, mid, mid, {kNavX + 200, y}, {400, kNavPitch});
      r.text = ui::create_text(canvas, "pm dev menu nav text", 28.0f);
      if (r.text) {
        ui::set_color(r.text, kDim);
        ui::place(r.text, mid, {0.0f, 0.5f}, {kNavX, y}, {400, kNavPitch});
      }
      continue;
    }
    const int index = rowAt[r.section]++;
    const ui::Vec2 anchor = inList ? ui::Vec2{0.5f, 1.0f} : mid;
    const ui::Vec2 where =
        inList ? ui::Vec2{0.0f, rowOffset(index)} : ui::Vec2{kViewX, listTop() + rowOffset(index)};
    if (r.kind == Click) {
      r.hit = ui::create_image(listParent, "pm dev menu row", kHit);
      if (r.hit) {
        ui::place(r.hit, anchor, {0.5f, 0.5f}, where, {kViewWide, kPitch});
        // The game's selection is a GameObject, not the bar's component, so remember the object the bar
        // made its button on: that is what followSelection() looks for.
        r.hitGo = phasmo::Component::get_gameObject(r.hit);
      }
    }
    r.text = ui::create_text(listParent, "pm dev menu row text", 26.0f);
    if (r.text) {
      ui::set_color(r.text, r.kind == Read ? kDim : kText);
      ui::place(r.text, anchor, {0.5f, 0.5f}, where, {kViewWide, kPitch});
    }
  }
  // Every clickable row becomes a game button: the Button on its bar gives the game's own hover, press,
  // selection and navigation, and the game's onClick carries the click to rowClicked.
  for (size_t i = 0; i < g_rows.size(); ++i) {
    Row& r = g_rows[i];
    if (r.kind == Read || !r.hit) continue;
    if (!api::buttons::attach(r.hit) || !api::buttons::on_click(r.hit, &rowClicked, toUser(i)))
      PM_LOG("dev-menu: the row \"%s\" could not be made a button", r.label);
  }
  // Every section exists; only the current one is shown.
  for (int s = 0; s < kSections; ++s) setSectionShown(s, s == g_section);
  fitList();  // the scroll range is the section showing, not every section's rows
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
  returnInput();
  if (g_wasCursorLocked) game::lock_cursor();
  g_active = false;
  g_built = false;
  g_canvas = nullptr;
  g_rows.clear();
  g_readFailed.clear();
  g_title = g_hint = g_refusalText = nullptr;
  g_view = ui::ScrollView{};  // the canvas died, and the list with it
  g_refusal = Why::None;
  releasePlayer();  // last, as in close()
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
    case Action::PickRetype: {
      const int n = actions::type_options();
      if (n > 0) g_retypePick = ((g_retypePick + r.arg) % n + n) % n;
      break;
    }
    case Action::Retype:
      if (!actions::retype(actions::type_option(g_retypePick))) g_refusal = Why::NoGhost;
      break;
    case Action::PickWeather: {
      const int n = actions::weather_count();
      if (n > 0) g_weatherPick = ((g_weatherPick + r.arg) % n + n) % n;
      break;
    }
    case Action::ApplyWeather:
      if (!actions::apply_weather(g_weatherPick)) g_refusal = Why::NoDifficulty;
      break;
    case Action::EndContract: {
      // The game's own truck path, refused honestly when the scene has no truck to ask. Its coroutine
      // waits on the game's clock, so the menu steps out of the way: close() unpauses and the ending runs.
      if (!actions::end_contract()) {
        g_refusal = Why::NoTruck;
        break;
      }
      close();
      break;
    }
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
    case Action::BumpPlayerSpeed: actions::bump_speed_multiplier(r.arg); break;
    case Action::ResetPlayerSpeed: actions::reset_speed_multiplier(); break;
    case Action::SetGhostBlind: actions::set_ghost_blind(!actions::ghost_blind()); break;
    case Action::SetDebugHud: s.debugHud = !s.debugHud; break;  // overlay.cpp draws it
    case Action::SetEspGhost: s.espGhost = !s.espGhost; break;
    case Action::SetEspRooms: s.espRooms = !s.espRooms; break;
    case Action::SetUnlockAll: s.unlockAll = !s.unlockAll; break;
    case Action::SpawnDummy:
      if (!dummy::spawn()) g_refusal = Why::NoPlayer;
      break;
    case Action::DummyAction:
      switch (r.arg) {
        case 0: dummy::to_me(); break;
        case 1:
          if (!dummy::make_banshee_target()) g_refusal = Why::NoGhost;
          break;
        case 2: dummy::revive(); break;
        default: dummy::despawn(); break;
      }
      break;  // actions.cpp's store hooks read it
    case Action::SetFreeShop: s.freeShop = !s.freeShop; break;
    case Action::FireState: {
      // The three named attempts are the game's own states, fired unforced like the event picker's.
      static constexpr const char* kStates[] = {"interact", "GhostAbility", "throwing"};
      if (r.arg < 0 || r.arg >= 3) break;
      for (const actions::EventKind& k : actions::event_kinds())
        if (std::string_view(k.name) == kStates[r.arg]) actions::fire_event(k.value);
      break;
    }
    case Action::GhostToMe:
      if (!actions::ghost_to_me()) g_refusal = Why::NoRig;
      break;
    case Action::SetNoclip:
      if (!actions::set_noclip(!actions::noclip())) g_refusal = Why::NoRig;
      break;
    case Action::MeToGhost:
      if (!actions::me_to_ghost()) g_refusal = Why::NoRig;
      break;
    case Action::MeToGhostRoom:
      if (!actions::me_to_ghost_room()) g_refusal = Why::NoRig;
      break;
    case Action::SetLockSanity: s.lockSanity = !s.lockSanity; break;  // actions::tick holds it
    case Action::CycleLockedSanity: s.lockedSanity = next_locked_sanity(s.lockedSanity); break;
    case Action::ToggleFreeze: actions::set_frozen(!actions::frozen()); break;
    case Action::KillLocal: actions::kill_local(); break;
    case Action::ReviveLocal:
      if (!actions::revive_local()) g_refusal = Why::NoDifficulty;
      break;
    case Action::SetGodMode: s.godMode = !s.godMode; break;
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

// A row's click, from the game's onClick. user is the row's index in g_rows: the button that fired was
// wired with its own row's index, and rows only go away with the canvas (recover/destroy), when the
// buttons go too.
void rowClicked(void* user) {
  const size_t i = fromUser(user);
  if (i >= g_rows.size()) return;
  const Row& r = g_rows[i];
  if (r.kind == Nav) {
    if (r.arg != g_section) {
      setSectionShown(g_section, false);
      g_section = r.arg;
      setSectionShown(g_section, true);
      refreshNav();
      fitList();  // the list is as long as the section now showing, and starts at its top
      refresh();  // the section's values now, not a frame late
      selectSectionRow();  // arrow keys continue from the section now showing
    }
    return;
  }
  if (r.kind == Click) run(r);
}

// The starting point for arrow-key and gamepad navigation: the section's first clickable row.
void selectSectionRow() {
  for (Row& r : g_rows)
    if (r.kind == Click && r.section == g_section && r.hit && api::buttons::select(r.hit)) return;
  for (Row& r : g_rows)  // a section with no clickable rows: its nav row still takes the selection
    if (r.kind == Nav && r.arg == g_section && r.hit && api::buttons::select(r.hit)) return;
}

// Unity's UI input module, on while the menu is open: the game runs it only while its journal is open
// (JournalController.OpenCloseJournal), and a disabled module routes no clicks and no arrow keys.
void borrowInput() {
  g_module = api::buttons::input_module();
  if (!g_module) {
    PM_LOG("dev-menu: Unity's UI input module is unavailable; the menu can take no clicks or arrow keys");
    return;
  }
  g_moduleWas = api::buttons::input_on(g_module);
  api::buttons::input_set(g_module, true);
}

void returnInput() {
  if (g_module && game::alive(g_module)) api::buttons::input_set(g_module, g_moduleWas);
  g_module = nullptr;
  g_moduleWas = false;
}

// Gives back the hold open() took. Last in every closing path: the game's release can throw (its map
// note), and nothing the menu changed may stay changed because of that.
void releasePlayer() {
  if (!g_held) return;
  g_held = false;
  actions::pause_player(false);
}

// A row's value, read now. A read the game throws on shows nothing, and is logged once with its row's
// name: one failing read must not stop the frame, which would leave every later row and the rest of the
// tick unrun (the live test of 2026-11-11, where the payout read threw on every frame at the main menu).
std::string safeValueText(size_t i) {
  if (g_readFailed.size() != g_rows.size()) g_readFailed.assign(g_rows.size(), false);
  if (g_readFailed[i]) return {};
  try {
    return valueText(g_rows[i]);
  } catch (const Il2CppExceptionWrapper&) {
    g_readFailed[i] = true;
    PM_LOG("dev-menu: reading the row \"%s\" threw a managed exception; it shows nothing from now on",
           g_rows[i].label);
  }
  return {};
}

// Reread the shown section's values, and the refusal line. Hidden sections are read when they show.
void refresh() {
  for (size_t i = 0; i < g_rows.size(); ++i) {
    Row& r = g_rows[i];
    if (r.kind == Nav || !r.text || r.section != g_section) continue;
    const std::string value = safeValueText(i);
    const std::string line = value.empty() ? i18n::text(r.label) : i18n::text(r.label) + "  " + value;
    ui::set_text(r.text, line.c_str());
  }
  if (g_refusalText)
    ui::set_text(g_refusalText, g_refusal == Why::None ? "" : whyText(g_refusal).c_str());
  followSelection();  // the arrow keys and a gamepad moved the selection inside the game
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
  borrowInput();
  selectSectionRow();
  g_held = actions::pause_player(true);  // the journal's own hold: no walking and no looking behind the menu
  // The clock is what the live tests of 2026-11-11 and 2026-11-12 blame for the picture after a close, so
  // the log says plainly what this open did to it (see sdk/systems/media.md).
  PM_LOG("dev-menu: open at timeScale %s, the player %s", number(scale).c_str(), g_held ? "held" : "not held");
}

void close() {
  if (!is_open()) return;
  show(false);
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  PM_LOG("dev-menu: closed at timeScale %s", number(actions::time_scale()).c_str());
  returnInput();
  if (g_wasCursorLocked) game::lock_cursor();
  releasePlayer();
}

void tick() {
  if (!g_active) return;
  if (!g_canvas || !game::alive(g_canvas)) {
    recover();  // the scene changed under the menu: the game goes back to how open() found it
    return;
  }
  refresh();
  // Re-assert what open() asked the game for: closing the journal mid-menu switches the UI input module
  // off and releases the player's hold while the menu is still up, and both calls skip a state that
  // already matches, so an unchanged frame does nothing.
  if (g_module && !api::buttons::input_on(g_module)) api::buttons::input_set(g_module, true);
  if (g_held) actions::pause_player(true);
}

void destroy() {
  // pmmod_stop must not leave the game frozen, the cursor loose, the UI input switched or the player
  // held because the menu was open.
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  returnInput();
  if (g_active && g_wasCursorLocked) game::lock_cursor();
  if (g_canvas) ui::destroy(g_canvas);
  g_canvas = nullptr;
  g_built = false;
  g_active = false;
  g_rows.clear();
  g_readFailed.clear();
  g_title = g_hint = g_refusalText = nullptr;
  g_view = ui::ScrollView{};  // destroyed with the canvas above
  g_refusal = Why::None;
  g_pause = Pause{};
  releasePlayer();  // last: the game's release may throw, and everything above is already put back
}

}  // namespace pm::devmenu::menu
