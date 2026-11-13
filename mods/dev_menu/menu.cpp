// The dev menu on screen (menu_layout.h has the look and the geometry). One overlay canvas over the game's
// 1920x1080 layout: a slab with a header (the title and three status lamps), the sections down the left, the current
// section's groups as flat panels in two columns of a scrolling area, and a footer with the refusal and the key hint.
// Every control is a bar carrying the game's own Button component, so hover, press, selection and arrow-key navigation
// are all the game's own (the user's live-test ruling, 2026-11-11). A row is one of four kinds: a read-out, an action,
// a switch with its lamp, or a picker with its value between two step buttons. While the menu is open the game's UI
// input module runs again and the player is held in place as the journal holds them. Every value shown is reread from
// the game every frame; every click goes through menu_rule.h's refuse() before actions.cpp runs it, and a refusal
// shows its reason in the footer rather than working around a game guard.
#include "menu.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include "actions.h"
#include "dummy.h"
#include "buttons.h"
#include "menu_layout.h"
#include "mod_glue.h"
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

// The labels live in the mod's table (PM_TEXT_AI placeholders the user may rewrite). i18n::key names each one in the
// code, so the table checks count it as asked for.
constexpr const char* kTitleKey = i18n::key("dev-menu.title");
constexpr const char* kHintKey = i18n::key("dev-menu.hint");
constexpr const char* kOnKey = i18n::key("dev-menu.on");
constexpr const char* kOffKey = i18n::key("dev-menu.off");
constexpr const char* kRefusedKey = i18n::key("dev-menu.refused");
constexpr const char* kPausedOpenKey = i18n::key("dev-menu.paused-while-open");
constexpr const char* kStatusKeys[] = {i18n::key("dev-menu.status.offline"), i18n::key("dev-menu.status.session"),
                                       i18n::key("dev-menu.status.frozen")};
constexpr const char* kWhyKeys[] = {nullptr, i18n::key("dev-menu.why.not-offline"),
                                    i18n::key("dev-menu.why.no-contract"),
                                    i18n::key("dev-menu.why.no-ghost"),
                                    i18n::key("dev-menu.why.no-difficulty"),
                                    i18n::key("dev-menu.why.no-player"),
                                    i18n::key("dev-menu.why.hunting"), i18n::key("dev-menu.why.not-hunting"),
                                    i18n::key("dev-menu.why.no-rooms"), i18n::key("dev-menu.why.dead"),
                                    i18n::key("dev-menu.why.no-truck"), i18n::key("dev-menu.why.friendly"),
                                    i18n::key("dev-menu.why.no-rig"), i18n::key("dev-menu.why.alive"),
                                    i18n::key("dev-menu.why.dying"), i18n::key("dev-menu.why.no-dummy"),
                                    i18n::key("dev-menu.why.doors-shut"), i18n::key("dev-menu.why.no-threshold"), i18n::key("dev-menu.why.no-spare")};
static_assert(sizeof kWhyKeys / sizeof kWhyKeys[0] == static_cast<size_t>(Why::NoSpare) + 1,
              "every refusal reason has its text, in the order of menu_rule.h's Why");

// The sections, in the order they show down the left.
constexpr int kSections = 7;
constexpr const char* kSectionKeys[kSections] = {i18n::key("dev-menu.section.difficulty"),
                                                 i18n::key("dev-menu.section.contract"),
                                                 i18n::key("dev-menu.section.ghost"),
                                                 i18n::key("dev-menu.section.player"),
                                                 i18n::key("dev-menu.section.equipment"),
                                                 i18n::key("dev-menu.section.game"),
                                                 i18n::key("dev-menu.section.loader")};

// What a row shows beside its label. Each row names one of these; the value is read from the game now.
enum ValueKind {
  NoValue,
  NameType, NumberReward, NumberPayout, LineState, LineIdentity, NameRoom, PickRoom, OnEvents, PickEvent,
  OnFrozen, OnGod, OnPause, OnSession, OnOffline, OnBlind, OnDebugHud,
  LineEvidence, OnNoclip, OnLockSanity, OnEspGhost, OnEspRooms, NameWeather, PickWeatherValue,
  PickRetypeValue, OnUnlockAll, OnFreeShop, OnSaveFrozen, OnDummy, LineTypeDials, OnEspDoors, OnEspHiding,
  OnEspSwitches, ModName, ModState, LineContent, PickObjectiveValue, OnNeverRunOut,
  PickItemValue, NameSightHold,
  OptionOnOff, OptionChoiceValue, SliderValue, NumberThreshold,
};

// The five kinds of row, and the section column's entries.
enum Kind { Nav, Read, Act, Toggle, Picker, Slider };

// What a slider sets: a custom difficulty option, one of the actions' dials, or the game's clock.
enum Source { FromOption, FromDial, FromClock };

// A row's clickable parts: the bar (an action or a switch, or a section), and a picker's two step buttons.
enum Part { kBar = 0, kDec = 1, kInc = 2 };

struct Row {
  Kind kind = Read;
  int section = 0;
  const char* group = "";        // the group's text key: the panel it sits in
  Action action = Action::Info;  // an action's or a switch's click; a picker's step (its arg is the step's size)
  int arg = 0;                   // a row's parameter: a delta, a sanity value, a direction
  const char* label = "";        // the row's words, a key in the mod's table
  ValueKind value = NoValue;
  bool danger = false;           // an action that ends or undoes something: its words are drawn in the warning red
  Source source = FromOption;    // a slider's number, with ref
  int ref = -1;                  // the option's index or the dial's id, for the rows that set one
  // Made by build():
  void* bar = nullptr;           // the bar the game's Button sits on (actions, switches, sections)
  void* dec = nullptr;           // a picker's step buttons
  void* inc = nullptr;
  void* barGo = nullptr;         // their GameObjects, which is what the game's selection moves onto
  void* decGo = nullptr;
  void* incGo = nullptr;
  void* text = nullptr;          // the label
  void* valueText = nullptr;     // the value, or a switch's on/off
  void* lamp = nullptr;          // a switch's lamp
  void* fill = nullptr;          // a slider's filled part, from the track's left edge
  float trackLeft = 0.0f;        // a slider's track: its left edge and line in the section's container
  float trackLine = 0.0f;
  float contentTop = 0.0f;       // the row's top edge, measured down from the content's top
};

// A group's panel and its heading, one per group per section.
struct Card {
  int section = 0;
  void* panel = nullptr;
  void* heading = nullptr;
};

api::Held g_canvas;
bool g_built = false;     // the canvas and its rows exist (they are kept between openings)
bool g_active = false;    // the rows are shown
int g_section = 0;
int g_roomPick = 0;       // the room picker's cursor index
int g_eventPick = 0;      // the event picker's cursor index
int g_weatherPick = 0;    // the weather picker's cursor index
int g_retypePick = 0;     // the re-type picker's cursor index
int g_modPick = 0;        // the Loader page's mod picker
int g_objectivePick = 0;  // the objective picker
int g_itemPick = 0;       // the item picker
Pause g_pause;
bool g_wasCursorLocked = true;
// Unity's UI input module, borrowed while the menu is open: the game runs it only while its journal is
// open, and without it no canvas takes clicks and no menu takes arrow keys.
void* g_module = nullptr;
bool g_moduleWas = false;
// Whether open() asked the game to hold the player (actions::pause_player answered true): only a hold the
// menu took is re-asserted while it is open and released when it closes.
bool g_held = false;
// The slider the pointer is dragging (its row), or -1. While one is dragged the list's ScrollRect is switched off, so
// the drag moves the slider and not the list.
int g_drag = -1;
// The rows whose read has thrown once already: logged once each, then shown empty.
std::vector<bool> g_readFailed;
Why g_refusal = Why::None;  // the last refused click, shown until a click works
std::vector<Row> g_rows;
std::vector<Card> g_cards;
void* g_sectionRoot[kSections] = {};   // each section's container inside the scrolling area
float g_sectionHeight[kSections] = {};
void* g_title = nullptr;
void* g_hint = nullptr;
void* g_refusalText = nullptr;
void* g_navLit = nullptr;              // the lit bar beside the current section
void* g_statusLamp[3] = {};
// The groups' scrolling area, on the game's own ScrollRect and clip (the user's original brief asked for a
// ScrollRect). Its parts are null when the game's classes did not resolve; the menu still works, it just does not
// scroll.
ui::ScrollView g_view;

// The palette (menu_layout.h has the reasoning): the truck's dark slab and panels, bone text, fog for what is quiet,
// the flashlight's amber for what is lit, and a warning red.
constexpr ui::Color kDim{0.0f, 0.0f, 0.0f, 0.55f};         // over the game, behind the slab
constexpr ui::Color kSlab{0.114f, 0.137f, 0.157f, 0.97f};  // #1D2328
constexpr ui::Color kPanel{0.149f, 0.180f, 0.208f, 0.96f}; // #262E35
constexpr ui::Color kBone{0.902f, 0.882f, 0.827f, 1.0f};   // #E6E1D3
constexpr ui::Color kFog{0.545f, 0.576f, 0.608f, 1.0f};    // #8B939B
constexpr ui::Color kAmber{0.914f, 0.690f, 0.298f, 1.0f};  // #E9B04C
constexpr ui::Color kRed{0.831f, 0.353f, 0.306f, 1.0f};    // #D45A4E
constexpr ui::Color kLampOff{0.227f, 0.259f, 0.286f, 1.0f};// #3A4249
constexpr ui::Color kRule{0.545f, 0.576f, 0.608f, 0.30f};
constexpr ui::Color kBarTint{1.0f, 1.0f, 1.0f, 0.045f};        // a clickable bar: the game's Button brightens it on hover
constexpr ui::Color kStep{0.192f, 0.227f, 0.259f, 1.0f};   // #313A42, a picker's step button
constexpr ui::Color kClear{0.0f, 0.0f, 0.0f, 0.0f};
constexpr ui::Color kFill{0.914f, 0.690f, 0.298f, 0.55f};  // a slider's filled part: the amber, dimmed under its number
// The clock's slider: from paused to four times real time, the old presets' range.
constexpr float kClockTop = 4.0f;
constexpr float kClockStep = 0.05f;

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
  return std::string(kinds[g_eventPick].name);
}

// The installed mod the Loader page's picker is on, wrapped into the list as it is now (it can change).
bool pickedMod(pm::mod::ModEntry& out, int& index, int& count) {
  const std::vector<pm::mod::ModEntry> mods = pm::mod::mod_entries();
  if (mods.empty()) return false;
  count = static_cast<int>(mods.size());
  index = ((g_modPick % count) + count) % count;
  out = mods[static_cast<size_t>(index)];
  return true;
}

// A slider's range, step and number now, wherever its number lives.
float sliderLow(const Row& r) {
  return r.source == FromDial ? actions::dial_low(static_cast<actions::DialId>(r.ref)) : 0.0f;
}
float sliderHigh(const Row& r) {
  if (r.source == FromDial) return actions::dial_high(static_cast<actions::DialId>(r.ref));
  if (r.source == FromClock) return kClockTop;
  return actions::option_top(r.ref);
}
float sliderStep(const Row& r) {
  if (r.source == FromDial) return actions::dial_step(static_cast<actions::DialId>(r.ref));
  if (r.source == FromClock) return kClockStep;
  return actions::option_step_size(r.ref);
}
float sliderValue(const Row& r) {
  if (r.source == FromDial) return actions::dial_value(static_cast<actions::DialId>(r.ref));
  if (r.source == FromClock) return g_pause.frozen ? g_pause.saved : actions::time_scale();
  return actions::option_number(r.ref);
}
// Whether the number may be set now: a difficulty for an option, the investigator or the ghost for a dial.
Why sliderRefuses(const Row& r) {
  const Can c = actions::snapshot();
  if (r.source == FromOption) return c.difficulty ? Why::None : Why::NoDifficulty;
  if (r.source == FromClock) return Why::None;
  if (actions::dial_need(static_cast<actions::DialId>(r.ref)) == actions::Need::Player)
    return c.player ? Why::None : Why::NoPlayer;
  if (!c.contract) return Why::NoContract;
  return c.ghost ? Why::None : Why::NoGhost;
}
void sliderSet(const Row& r, float v) {
  const float low = sliderLow(r), high = sliderHigh(r), step = sliderStep(r);
  v = v < low ? low : (v > high ? high : v);
  if (step > 0.0f) v = low + std::round((v - low) / step) * step;
  if (r.source == FromDial) {
    const auto id = static_cast<actions::DialId>(r.ref);
    actions::dial_set(id, v);
    // The hold keeps whatever sanity the slider set, so a held sanity can be moved without letting go of it.
    Switches& s = actions::switches();
    if (id == actions::DialId::Sanity && s.lockSanity) s.lockedSanity = static_cast<int>(std::lround(v));
  } else if (r.source == FromClock) {
    const float scale = v <= 0.0f ? kPausedScale : v;  // never 0: menu_rule.h's kPausedScale says why
    actions::set_time_scale(scale);
    if (g_pause.frozen) {
      g_pause.saved = scale;  // closing restores the new speed, not the old one
      actions::set_time_scale(kPausedScale);
    }
  } else {
    actions::option_set(r.ref, v);
  }
}

std::string valueText(const Row& r) {
  switch (r.value) {
    case NoValue: return {};
    case NameType: return actions::forced_type_name();
    case NumberReward: return number(actions::reward_multiplier());
    case NumberPayout: return whole(actions::bonus_reward());
    case LineState: return actions::ghost_line();
    case LineIdentity: return actions::ghost_identity();
    case NameRoom: return actions::favourite_room_name();
    case PickRoom: return roomPickText();
    case OnEvents: return onOff(actions::events_blocked());
    case PickEvent: return eventPickText();
    case OnFrozen: return onOff(actions::frozen());
    case OnGod: return onOff(actions::switches().godMode);
    case OnPause: return onOff(actions::switches().pauseWhenOpen);
    case OnSession: return onOff(actions::session_on());
    case OnOffline: return onOff(actions::offline());
    case OnBlind: {
      static constexpr const char* kSightKeys[] = {i18n::key("dev-menu.ghost-sight.normal"),
                                                   i18n::key("dev-menu.ghost-sight.nobody"),
                                                   i18n::key("dev-menu.ghost-sight.everyone")};
      const int g = actions::switches().ghostSight;
      return g >= 0 && g < 3 ? i18n::text(kSightKeys[g]) : std::string{};
    }
    case OnDebugHud: return onOff(actions::switches().debugHud);
    case LineEvidence: return actions::given_evidence();
    case OnNoclip: return onOff(actions::noclip());
    case OnLockSanity: return onOff(actions::switches().lockSanity);
    case OnEspGhost: return onOff(actions::switches().espGhost);
    case OnEspRooms: return onOff(actions::switches().espRooms);
    case NameWeather: return actions::weather_now();
    case OnUnlockAll: return onOff(actions::switches().unlockAll);
    case OnDummy: return onOff(dummy::available());
    case LineTypeDials: return actions::type_dials();
    case ModName: {
      pm::mod::ModEntry m;
      int index = 0, count = 0;
      if (!pickedMod(m, index, count)) return {};
      return m.name + "  " + whole(index + 1) + "/" + whole(count);
    }
    case ModState: {
      // The host contract's mod_entry (2.5): the id and version, and whether it runs or why it is off, in the Mods
      // panel's own words.
      pm::mod::ModEntry m;
      int index = 0, count = 0;
      if (!pickedMod(m, index, count)) return {};
      std::string line = m.id + (m.version.empty() ? "" : " " + m.version) + ", ";
      line += (m.flags & PM_MOD_ENTRY_STARTED) ? i18n::text(kOnKey) : i18n::text(kOffKey);
      if (!m.offReason.empty()) line += ": " + m.offReason;
      return line;
    }
    case LineContent: return actions::content_line();
    case PickObjectiveValue: return actions::objective_line(g_objectivePick);
    case OnNeverRunOut: return onOff(actions::switches().neverRunOut);
    case PickItemValue: return actions::item_name(g_itemPick);
    case NameSightHold: {
      static constexpr const char* kHoldKeys[] = {i18n::key("dev-menu.sight-game"), i18n::key("dev-menu.sight-none"),
                                                  i18n::key("dev-menu.sight-frozen")};
      // The multiplier itself while a hunt runs, so the setting can be watched working (it climbs only during a chase).
      const int h = actions::switches().sightHold;
      const float now = actions::sight_now();
      const std::string setting = h >= 0 && h < 3 ? i18n::text(kHoldKeys[h]) : std::string{};
      return now < 0.0f ? setting : setting + " (" + number(now) + ")";
    }
    case OnEspDoors: return onOff(actions::switches().espDoors);
    case OnEspHiding: return onOff(actions::switches().espHiding);
    case OnEspSwitches: return onOff(actions::switches().espSwitches);
    case OnFreeShop: return onOff(actions::switches().freeShop);
    case OnSaveFrozen: return onOff(freeze_save(actions::session_on()));
    case PickWeatherValue: return actions::weather_name(g_weatherPick);
    case PickRetypeValue: return actions::type_name(actions::type_option(g_retypePick));
    case OptionOnOff: return onOff(actions::option_number(r.ref) > 0.5f);
    case OptionChoiceValue: return actions::option_value(r.ref);
    case SliderValue: {
      const float v = sliderValue(r);
      const float step = sliderStep(r);
      // Whole numbers for a slider that moves by whole steps, two places for the rest.
      std::string shown = step >= 1.0f && std::fabs(step - std::round(step)) < 0.001f
                              ? whole(static_cast<int>(std::lround(v)))
                              : number(v);
      if (r.source == FromClock && g_pause.frozen) shown += "  " + i18n::text(kPausedOpenKey);
      return shown;
    }
    case NumberThreshold: {
      // The sanity the ghost may hunt at or below, learned from the game's own idle check (actions::hunt_threshold).
      const int t = actions::hunt_threshold();
      if (t < 0) return i18n::text(i18n::key("dev-menu.threshold-unknown"));
      return i18n::text(i18n::key("dev-menu.threshold-value"), {{"sanity", whole(t)}});
    }
  }
  return {};
}

// ---- The sections, their groups and their rows, in the order they show ----
//
// The information architecture (the user's ask, 2026-11-12: things were hard to find): each section is split into
// groups named for what a developer is doing there, and every value with a previous/next or lower/higher pair is one
// picker row instead of three rows.
std::vector<Row> sectionRows(int section) {
  std::vector<Row> rows;
  const char* group = "";
  const auto in = [&](const char* key) { group = key; };
  const auto add = [&](Kind kind, Action action, int arg, const char* label, ValueKind value, bool danger = false) {
    Row r;
    r.kind = kind;
    r.section = section;
    r.group = group;
    r.action = action;
    r.arg = arg;
    r.label = label;
    r.value = value;
    r.danger = danger;
    rows.push_back(r);
  };
  // A slider: its label, and the number it sets.
  const auto slider = [&](const char* label, Source source, int ref) {
    add(Slider, Action::Slide, 1, label, SliderValue);
    rows.back().source = source;
    rows.back().ref = ref;
  };
  const auto dial = [&](const char* label, actions::DialId id) { slider(label, FromDial, static_cast<int>(id)); };
  switch (section) {
    case 0: {  // Difficulty
      // Every option on its own row, in the three groups the game's own custom difficulty menu uses (the user's ask,
      // 2026-11-13): a switch for an on/off, a slider for a number, a choice stepped through the game's own values.
      static constexpr const char* kGroups[] = {i18n::key("dev-menu.group.option-player"),
                                                i18n::key("dev-menu.group.option-ghost"),
                                                i18n::key("dev-menu.group.option-contract")};
      const actions::OptionGroup order[] = {actions::OptionGroup::Player, actions::OptionGroup::Ghost,
                                            actions::OptionGroup::Contract};
      for (int g = 0; g < 3; ++g) {
        in(kGroups[g]);
        for (int i = 0; i < actions::option_count(); ++i) {
          if (actions::option_group(i) != order[g]) continue;
          switch (actions::option_control(i)) {
            case actions::OptionControl::Switch:
              add(Toggle, Action::OptionToggle, 0, actions::option_label(i), OptionOnOff);
              rows.back().ref = i;
              break;
            case actions::OptionControl::Choice:
              add(Picker, Action::OptionChoice, 1, actions::option_label(i), OptionChoiceValue);
              rows.back().ref = i;
              break;
            case actions::OptionControl::Slider: slider(actions::option_label(i), FromOption, i); break;
          }
        }
      }
      in(i18n::key("dev-menu.group.next-contract"));
      add(Picker, Action::BumpType, 1, i18n::key("dev-menu.ghost-type"), NameType);
      in(i18n::key("dev-menu.group.reward"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.reward"), NumberReward);
      add(Read, Action::Info, 0, i18n::key("dev-menu.payout"), NumberPayout);
      break;
    }
    case 1:  // Contract
      in(i18n::key("dev-menu.group.contract"));
      add(Act, Action::UnlockDoor, 0, i18n::key("dev-menu.unlock-door"), NoValue);
      add(Act, Action::EndContract, 0, i18n::key("dev-menu.end-contract"), NoValue, true);
      add(Act, Action::Leave, 0, i18n::key("dev-menu.leave"), NoValue, true);
      add(Act, Action::Quit, 0, i18n::key("dev-menu.quit"), NoValue, true);
      in(i18n::key("dev-menu.group.objectives"));
      add(Picker, Action::PickObjective, 1, i18n::key("dev-menu.objective"), PickObjectiveValue);
      add(Act, Action::CompleteObjective, 0, i18n::key("dev-menu.objective-complete"), NoValue);
      add(Act, Action::FailObjective, 0, i18n::key("dev-menu.objective-fail"), NoValue, true);
      in(i18n::key("dev-menu.group.weather"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.weather"), NameWeather);
      add(Picker, Action::PickWeather, 1, i18n::key("dev-menu.weather-pick"), PickWeatherValue);
      add(Act, Action::ApplyWeather, 0, i18n::key("dev-menu.weather-apply"), NoValue);
      break;
    case 2:  // Ghost
      in(i18n::key("dev-menu.group.watch"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.state"), LineState);
      add(Read, Action::Info, 0, i18n::key("dev-menu.identity"), LineIdentity);
      add(Read, Action::Info, 0, i18n::key("dev-menu.evidence-given"), LineEvidence);
      add(Read, Action::Info, 0, i18n::key("dev-menu.room"), NameRoom);
      add(Read, Action::Info, 0, i18n::key("dev-menu.type-dials"), LineTypeDials);
      in(i18n::key("dev-menu.group.hunts"));
      add(Act, Action::ForceHunt, 0, i18n::key("dev-menu.force-hunt"), NoValue);
      add(Act, Action::EndHunt, 0, i18n::key("dev-menu.end-hunt"), NoValue);
      add(Act, Action::ClearCooldown, 0, i18n::key("dev-menu.clear-cooldown"), NoValue);
      add(Act, Action::ChaseMe, 0, i18n::key("dev-menu.chase-me"), NoValue);
      add(Act, Action::CycleSightHold, 0, i18n::key("dev-menu.sight-hold"), NameSightHold);
      add(Act, Action::SetGhostBlind, 0, i18n::key("dev-menu.ghost-blind"), OnBlind);
      dial(i18n::key("dev-menu.base-speed"), actions::DialId::GhostSpeed);
      in(i18n::key("dev-menu.group.room"));
      add(Picker, Action::PickRoom, 1, i18n::key("dev-menu.room-pick"), PickRoom);
      add(Act, Action::ApplyRoom, 0, i18n::key("dev-menu.room-apply"), NoValue);
      add(Act, Action::RandomRoom, 0, i18n::key("dev-menu.room-random"), NoValue);
      add(Act, Action::GhostToMe, 0, i18n::key("dev-menu.ghost-to-me"), NoValue);
      in(i18n::key("dev-menu.group.activity"));
      add(Toggle, Action::ToggleEvents, 0, i18n::key("dev-menu.block-events"), OnEvents);
      add(Picker, Action::FireEvent, 1, i18n::key("dev-menu.event"), PickEvent);
      add(Act, Action::FireEvent, 0, i18n::key("dev-menu.event-fire"), NoValue);
      add(Act, Action::FireState, 0, i18n::key("dev-menu.interact-now"), NoValue);
      add(Act, Action::FireState, 1, i18n::key("dev-menu.ability-now"), NoValue);
      add(Act, Action::FireState, 2, i18n::key("dev-menu.throw-now"), NoValue);
      add(Act, Action::Appear, 0, i18n::key("dev-menu.appear"), NoValue);
      add(Act, Action::UnAppear, 0, i18n::key("dev-menu.unappear"), NoValue);
      in(i18n::key("dev-menu.group.type"));
      add(Picker, Action::PickRetype, 1, i18n::key("dev-menu.retype-pick"), PickRetypeValue);
      add(Act, Action::Retype, 0, i18n::key("dev-menu.retype-apply"), NoValue);
      add(Act, Action::TypeChangeNow, 0, i18n::key("dev-menu.type-change-now"), NoValue);
      dial(i18n::key("dev-menu.flames"), actions::DialId::Flames);
      add(Act, Action::BumpFlames, 0, i18n::key("dev-menu.flames-clear"), NoValue);
      break;
    case 3:  // Player
      in(i18n::key("dev-menu.group.you"));
      // Sanity by the percent, with the ghost's own hunt line beside it and a click to sit on either side of it (the
      // user's ask, 2026-11-13: test the hunt thresholds).
      dial(i18n::key("dev-menu.sanity"), actions::DialId::Sanity);
      add(Read, Action::Info, 0, i18n::key("dev-menu.threshold"), NumberThreshold);
      add(Act, Action::SanityToThreshold, 0, i18n::key("dev-menu.sanity-on-threshold"), NoValue);
      add(Act, Action::SanityToThreshold, 1, i18n::key("dev-menu.sanity-above-threshold"), NoValue);
      add(Toggle, Action::SetLockSanity, 0, i18n::key("dev-menu.sanity-lock"), OnLockSanity);
      add(Toggle, Action::SetGodMode, 0, i18n::key("dev-menu.god-mode"), OnGod);
      add(Toggle, Action::ToggleFreeze, 0, i18n::key("dev-menu.freeze"), OnFrozen);
      add(Act, Action::KillLocal, 0, i18n::key("dev-menu.die"), NoValue, true);
      add(Act, Action::ReviveLocal, 0, i18n::key("dev-menu.revive"), NoValue);
      in(i18n::key("dev-menu.group.movement"));
      add(Toggle, Action::SetNoclip, 0, i18n::key("dev-menu.noclip"), OnNoclip);
      dial(i18n::key("dev-menu.player-speed"), actions::DialId::PlayerSpeed);
      add(Act, Action::ResetPlayerSpeed, 0, i18n::key("dev-menu.speed-reset"), NoValue);
      add(Act, Action::MeToGhost, 0, i18n::key("dev-menu.me-to-ghost"), NoValue);
      add(Act, Action::MeToGhostRoom, 0, i18n::key("dev-menu.me-to-ghost-room"), NoValue);
      add(Act, Action::MeToTruck, 0, i18n::key("dev-menu.me-to-truck"), NoValue);
      in(i18n::key("dev-menu.group.dummy"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.dummy"), OnDummy);
      add(Act, Action::SpawnDummy, 0, i18n::key("dev-menu.dummy-spawn"), NoValue);
      add(Act, Action::DummyAction, 0, i18n::key("dev-menu.dummy-to-me"), NoValue);
      add(Act, Action::DummyAction, 1, i18n::key("dev-menu.dummy-banshee"), NoValue);
      add(Act, Action::DummyAction, 2, i18n::key("dev-menu.dummy-revive"), NoValue);
      add(Act, Action::DummyAction, 3, i18n::key("dev-menu.dummy-remove"), NoValue, true);
      break;
    case 4:  // Equipment
      in(i18n::key("dev-menu.group.store"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.save-frozen"), OnSaveFrozen);
      add(Toggle, Action::SetUnlockAll, 0, i18n::key("dev-menu.unlock-all"), OnUnlockAll);
      add(Toggle, Action::SetFreeShop, 0, i18n::key("dev-menu.free-shop"), OnFreeShop);
      in(i18n::key("dev-menu.group.supplies"));
      // A dev session keeps every consumable and every brought item whatever happens (actions.cpp's hooks), so the
      // group says so instead of offering a switch that could only ever be on.
      add(Read, Action::Info, 0, i18n::key("dev-menu.equipment-protected"), OnSession);
      add(Toggle, Action::SetNeverRunOut, 0, i18n::key("dev-menu.never-run-out"), OnNeverRunOut);
      add(Picker, Action::PickItem, 1, i18n::key("dev-menu.item"), PickItemValue);
      add(Act, Action::SpawnItem, 0, i18n::key("dev-menu.item-spawn"), NoValue);
      break;
    case 5:  // Game
      in(i18n::key("dev-menu.group.time"));
      slider(i18n::key("dev-menu.tick-speed"), FromClock, 0);
      add(Toggle, Action::SetPauseWhenOpen, 0, i18n::key("dev-menu.pause-when-open"), OnPause);
      in(i18n::key("dev-menu.group.on-screen"));
      add(Toggle, Action::SetDebugHud, 0, i18n::key("dev-menu.debug-hud"), OnDebugHud);
      add(Toggle, Action::SetEspGhost, 0, i18n::key("dev-menu.esp-ghost"), OnEspGhost);
      add(Toggle, Action::SetEspRooms, 0, i18n::key("dev-menu.esp-rooms"), OnEspRooms);
      add(Toggle, Action::SetEspDoors, 0, i18n::key("dev-menu.esp-doors"), OnEspDoors);
      add(Toggle, Action::SetEspHiding, 0, i18n::key("dev-menu.esp-hiding"), OnEspHiding);
      add(Toggle, Action::SetEspSwitches, 0, i18n::key("dev-menu.esp-switches"), OnEspSwitches);
      in(i18n::key("dev-menu.group.session"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.session"), OnSession);
      add(Read, Action::Info, 0, i18n::key("dev-menu.offline-state"), OnOffline);
      break;
    case 6:  // Loader
      in(i18n::key("dev-menu.group.mods"));
      add(Picker, Action::PickMod, 1, i18n::key("dev-menu.mod"), ModName);
      add(Read, Action::Info, 0, i18n::key("dev-menu.mod-state"), ModState);
      in(i18n::key("dev-menu.group.content"));
      add(Read, Action::Info, 0, i18n::key("dev-menu.content"), LineContent);
      break;
  }
  return rows;
}

// A clickable part travels to its click handler as the wiring's user pointer: its row's index and which part it is.
void* toUser(size_t row, Part part) {
  return reinterpret_cast<void*>(static_cast<uintptr_t>(row * 4 + static_cast<size_t>(part)));
}
size_t rowOf(void* user) { return static_cast<size_t>(reinterpret_cast<uintptr_t>(user)) / 4; }
Part partOf(void* user) { return static_cast<Part>(reinterpret_cast<uintptr_t>(user) % 4); }

void rowClicked(void* user);
void selectSectionRow();
void borrowInput();
void returnInput();
void releasePlayer();
void refresh();
void fitList();
void followSelection();

using namespace layout;

// A text: its colour, its size and where it sits; null when the game's text could not be made.
void* text_at(void* parent, const char* name, float size, ui::Color colour, ui::Vec2 anchor, ui::Vec2 pivot,
              ui::Vec2 at, ui::Vec2 box) {
  void* t = ui::create_text(parent, name, size);
  if (!t) return nullptr;
  ui::set_color(t, colour);
  // The game's text centres itself; a text placed by its left edge reads from that edge, as a label should.
  if (pivot.x == 0.0f) ui::set_alignment(t, ui::Align::Left);
  ui::place(t, anchor, pivot, at, box);
  return t;
}

// A plain block of colour.
void* block_at(void* parent, const char* name, ui::Color colour, ui::Vec2 anchor, ui::Vec2 at, ui::Vec2 box) {
  void* b = ui::create_image(parent, name, colour);
  if (b) ui::place(b, anchor, {0.5f, 0.5f}, at, box);
  return b;
}

// A clickable part: the game's own Button on a bar of colour, its click carried to rowClicked.
void* button_at(void* parent, const char* name, ui::Color colour, ui::Vec2 anchor, ui::Vec2 at, ui::Vec2 box,
                size_t row, Part part, void*& go) {
  void* b = block_at(parent, name, colour, anchor, at, box);
  if (!b) return nullptr;
  if (!api::buttons::attach(b) || !api::buttons::on_click(b, &rowClicked, toUser(row, part)))
    PM_LOG("dev-menu: a control of the row \"%s\" could not be made a button", g_rows[row].label);
  // The game's selection is a GameObject, not the bar's component: followSelection() looks for this.
  go = phasmo::Component::get_gameObject(b);
  return b;
}

void refreshNav();

// The section column: a bar per section, and the lit bar beside the current one.
void build_nav(void* canvas) {
  const ui::Vec2 mid{0.5f, 0.5f};
  for (size_t i = 0; i < g_rows.size(); ++i) {
    Row& r = g_rows[i];
    if (r.kind != Nav) continue;
    const float y = navOffset(r.arg);
    r.bar = button_at(canvas, "pm dev menu section", kBarTint, mid, {kNavLeft + kNavWide / 2.0f, y},
                      {kNavWide, kNavPitch - 6.0f}, i, kBar, r.barGo);
    r.text = text_at(canvas, "pm dev menu section text", 26.0f, kFog, mid, {0.0f, 0.5f}, {kNavLeft + 22.0f, y},
                     {kNavWide - 22.0f, kNavPitch});
  }
  g_navLit = block_at(canvas, "pm dev menu section lit", kAmber, mid, {kNavLeft + kNavBar / 2.0f, navOffset(0)},
                      {kNavBar, kNavPitch - 16.0f});
}

// The header: the title on the left, three status lamps on the right, a rule under both.
void build_header(void* canvas) {
  const ui::Vec2 mid{0.5f, 0.5f};
  g_title = text_at(canvas, "pm dev menu title", 40.0f, kBone, mid, {0.0f, 0.5f}, {kSlabLeft, kHeaderY},
                    {700.0f, kHeaderBox});
  if (g_title) ui::set_text(g_title, i18n::text(kTitleKey).c_str());
  for (int i = 0; i < 3; ++i) {
    // From the right edge leftward: single player, dev session, save frozen, read in the order a developer asks.
    // Each slot closes with its own lamp, the label right up against it, so a lamp never reads as its neighbour's.
    const float right = kSlabRight - static_cast<float>(2 - i) * kStatusPitch;
    g_statusLamp[i] = block_at(canvas, "pm dev menu status lamp", kLampOff, mid, {right - kLampWide / 2.0f, kHeaderY},
                               {kLampWide, kLampHigh});
    if (void* label = text_at(canvas, "pm dev menu status", 20.0f, kFog, mid, {1.0f, 0.5f},
                              {right - kLampWide - 12.0f, kHeaderY}, {kStatusPitch - kLampWide - 40.0f, kHeaderBox}))
      ui::set_text(label, ("<align=right>" + i18n::text(kStatusKeys[i])).c_str());
  }
  block_at(canvas, "pm dev menu rule", kRule, mid, {0.0f, kRuleY}, {kSlabWide - 60.0f, 2.0f});
}

// The footer: the refusal on the left in the warning red, the key hint on the right.
void build_footer(void* canvas) {
  const ui::Vec2 mid{0.5f, 0.5f};
  g_refusalText = text_at(canvas, "pm dev menu refusal", 22.0f, kRed, mid, {0.0f, 0.5f}, {kSlabLeft, kFooterY},
                          {900.0f, kFooterBox});
  if (g_refusalText) ui::set_text(g_refusalText, "");
  g_hint = text_at(canvas, "pm dev menu hint", 19.0f, kFog, mid, {1.0f, 0.5f}, {kSlabRight, kFooterY},
                   {820.0f, kFooterBox});
  if (g_hint) ui::set_text(g_hint, ("<align=right>" + i18n::text(kHintKey)).c_str());
}

// One section's groups: each a panel with its name, its rows flowed into the shorter column so far.
void build_section(int section, void* parent, bool inList) {
  // The section's groups, in the order its rows name them.
  std::vector<const char*> groups;
  std::vector<int> counts;
  for (const Row& r : g_rows) {
    if (r.kind == Nav || r.section != section) continue;
    if (groups.empty() || std::string_view(groups.back()) != r.group) {
      groups.push_back(r.group);
      counts.push_back(0);
    }
    ++counts.back();
  }
  std::vector<float> heights;
  for (const int n : counts) heights.push_back(card_height(n));
  const std::vector<Placed> placed = flow(heights);
  g_sectionHeight[section] = content_height(heights);

  // The section's own container, so a switch of section shows or hides it whole. In the scrolling area it is pinned
  // to the content's top edge; without one it sits where the area would be.
  const ui::Vec2 top{0.5f, 1.0f};
  void* root = ui::create_image(parent, "pm dev menu section page", kClear);
  if (!root) return;
  if (inList)
    ui::place(root, top, top, {0.0f, 0.0f}, {kViewWide, g_sectionHeight[section]});
  else
    ui::place(root, {0.5f, 0.5f}, top, {kViewX, kViewTop}, {kViewWide, g_sectionHeight[section]});
  g_sectionRoot[section] = root;

  size_t groupIndex = 0;
  int rowInGroup = 0;
  for (size_t i = 0; i < g_rows.size(); ++i) {
    Row& r = g_rows[i];
    if (r.kind == Nav || r.section != section) continue;
    if (rowInGroup == counts[groupIndex]) {
      ++groupIndex;
      rowInGroup = 0;
    }
    const Placed& at = placed[groupIndex];
    const float cx = column_x(at.column);
    const float left = cx - kCardWide / 2.0f + kCardPad;
    const float right = cx + kCardWide / 2.0f - kCardPad;
    if (rowInGroup == 0) {
      Card card;
      card.section = section;
      card.panel = block_at(root, "pm dev menu group", kPanel, top, {cx, -(at.top + heights[groupIndex] / 2.0f)},
                            {kCardWide, heights[groupIndex]});
      card.heading = text_at(root, "pm dev menu group name", 21.0f, kAmber, top, {0.0f, 0.5f},
                             {left, -(at.top + kCardHead / 2.0f + 4.0f)}, {kRowWide, kCardHead});
      if (card.heading) ui::set_text(card.heading, i18n::text(r.group).c_str());
      g_cards.push_back(card);
    }
    const float y = -(at.top + row_line(rowInGroup));
    r.contentTop = at.top + row_line(rowInGroup) - kPitch / 2.0f;
    ++rowInGroup;

    // The clickable bar under an action or a switch, the width of the row.
    if (r.kind == Act || r.kind == Toggle)
      r.bar = button_at(root, "pm dev menu row", kBarTint, top, {cx, y}, {kRowWide, kPitch - 6.0f}, i, kBar, r.barGo);
    const ui::Color words = r.danger ? kRed : (r.kind == Read || r.kind == Picker ? kFog : kBone);
    // A label takes what its row leaves it: all of an action without a value, the room beside a switch's lamp, the
    // room beside a picker, and a little over half beside a read-out's value. A label too long for that shrinks.
    float labelWide = kRowWide * 0.55f;
    if (r.kind == Picker || r.kind == Slider) labelWide = kRowWide - kPickerWide - 24.0f;
    if (r.kind == Toggle) labelWide = kRowWide - kLampWide - 150.0f;
    if (r.kind == Act && r.value == NoValue) labelWide = kRowWide - 20.0f;
    r.text = text_at(root, "pm dev menu row label", 22.0f, words, top, {0.0f, 0.5f}, {left + 10.0f, y},
                     {labelWide, kPitch});
    if (r.text) {
      ui::set_auto_size(r.text, 15.0f, 22.0f);
      std::string words = i18n::text(r.label);
      // A difficulty option says on its label when the game reads it, unless that is at once: only as a contract
      // begins, partly now, or not traced to a reader yet.
      static constexpr const char* kWhenKeys[] = {nullptr, i18n::key("dev-menu.option-when.start"),
                                                  i18n::key("dev-menu.option-when.mixed"),
                                                  i18n::key("dev-menu.option-when.unknown")};
      const bool option = r.action == Action::OptionToggle || r.action == Action::OptionChoice ||
                          (r.kind == Slider && r.source == FromOption);
      const int when = option ? actions::option_when(r.ref) : 0;
      if (when >= 1 && when <= 3) words += " <color=#8B939B>(" + i18n::text(kWhenKeys[when]) + ")</color>";
      ui::set_text(r.text, words.c_str());
    }

    switch (r.kind) {
      case Toggle: {
        r.lamp = block_at(root, "pm dev menu lamp", kLampOff, top, {right - 10.0f - kLampWide / 2.0f, y},
                          {kLampWide, kLampHigh});
        r.valueText = text_at(root, "pm dev menu row value", 19.0f, kFog, top, {1.0f, 0.5f},
                              {right - 10.0f - kLampWide - 12.0f, y}, {120.0f, kPitch});
        break;
      }
      case Picker: {
        const float pickLeft = right - kPickerWide;
        r.dec = button_at(root, "pm dev menu step", kStep, top, {pickLeft + kStepWide / 2.0f, y},
                          {kStepWide, kStepHigh}, i, kDec, r.decGo);
        r.inc = button_at(root, "pm dev menu step", kStep, top, {right - kStepWide / 2.0f, y}, {kStepWide, kStepHigh},
                          i, kInc, r.incGo);
        if (void* t = text_at(root, "pm dev menu step mark", 24.0f, kBone, top, {0.5f, 0.5f},
                              {pickLeft + kStepWide / 2.0f, y}, {kStepWide, kStepHigh}))
          ui::set_text(t, "<align=center>\xE2\x80\xB9");
        if (void* t = text_at(root, "pm dev menu step mark", 24.0f, kBone, top, {0.5f, 0.5f},
                              {right - kStepWide / 2.0f, y}, {kStepWide, kStepHigh}))
          ui::set_text(t, "<align=center>\xE2\x80\xBA");
        r.valueText = text_at(root, "pm dev menu row value", 21.0f, kBone, top, {0.5f, 0.5f},
                              {right - kPickerWide / 2.0f, y}, {kPickerWide - 2.0f * kStepWide - 12.0f, kPitch});
        if (r.valueText) ui::set_auto_size(r.valueText, 13.0f, 21.0f);
        break;
      }
      case Slider: {
        // The picker's shape, with a track between the step buttons: its fill shows the number against its range,
        // a click or a drag on it sets the number there, and the step buttons move it one step.
        const float pickLeft = right - kPickerWide;
        const float trackWide = kPickerWide - 2.0f * kStepWide - 12.0f;
        r.dec = button_at(root, "pm dev menu step", kStep, top, {pickLeft + kStepWide / 2.0f, y},
                          {kStepWide, kStepHigh}, i, kDec, r.decGo);
        r.inc = button_at(root, "pm dev menu step", kStep, top, {right - kStepWide / 2.0f, y}, {kStepWide, kStepHigh},
                          i, kInc, r.incGo);
        if (void* t = text_at(root, "pm dev menu step mark", 24.0f, kBone, top, {0.5f, 0.5f},
                              {pickLeft + kStepWide / 2.0f, y}, {kStepWide, kStepHigh}))
          ui::set_text(t, "<align=center>-");
        if (void* t = text_at(root, "pm dev menu step mark", 24.0f, kBone, top, {0.5f, 0.5f},
                              {right - kStepWide / 2.0f, y}, {kStepWide, kStepHigh}))
          ui::set_text(t, "<align=center>+");
        r.trackLeft = pickLeft + kStepWide + 6.0f;
        r.trackLine = y;
        r.bar = button_at(root, "pm dev menu track", kLampOff, top, {r.trackLeft + trackWide / 2.0f, y},
                          {trackWide, kStepHigh}, i, kBar, r.barGo);
        r.fill = ui::create_image(root, "pm dev menu track fill", kFill);
        r.valueText = text_at(root, "pm dev menu row value", 20.0f, kBone, top, {0.5f, 0.5f},
                              {r.trackLeft + trackWide / 2.0f, y}, {trackWide - 8.0f, kPitch});
        if (r.valueText) ui::set_auto_size(r.valueText, 12.0f, 20.0f);
        break;
      }
      case Read:
      case Act:
        if (r.value != NoValue) {
          r.valueText = text_at(root, "pm dev menu row value", 21.0f, r.kind == Read ? kBone : kAmber, top,
                                {1.0f, 0.5f}, {right - 10.0f, y}, {kRowWide - labelWide - 24.0f, kPitch});
          if (r.valueText) ui::set_auto_size(r.valueText, 13.0f, 21.0f);
        }
        break;
      case Nav: break;
    }
  }
}

bool build() {
  void* canvas = ui::create_canvas("pm dev menu", 30000);
  if (!canvas) return false;
  g_canvas = canvas;
  api::buttons::catch_pointer(canvas);  // without a GraphicRaycaster the canvas catches no pointer at all
  const ui::Vec2 mid{0.5f, 0.5f};
  block_at(canvas, "pm dev menu dim", kDim, mid, {0, 0}, {1920, 1080});
  block_at(canvas, "pm dev menu slab", kSlab, mid, {0, 0}, {kSlabWide, kSlabHigh});

  g_rows.clear();
  g_cards.clear();
  for (int s = 0; s < kSections; ++s) {
    Row nav;
    nav.kind = Nav;
    nav.arg = s;
    nav.label = kSectionKeys[s];
    g_rows.push_back(nav);
  }
  for (int s = 0; s < kSections; ++s)
    for (const Row& r : sectionRows(s)) g_rows.push_back(r);

  build_header(canvas);
  build_nav(canvas);
  build_footer(canvas);
  // The groups live in a scroll view (the user's own ask in the original brief), so a section taller than the slab
  // scrolls instead of running under the footer. Its height follows the section showing.
  g_view = ui::create_scroll_view(canvas, "pm dev menu list", {kViewX, kViewY}, {kViewWide, kViewHigh}, kViewHigh);
  if (!g_view.content) PM_LOG("dev-menu: the game's scroll view could not be made; the groups are placed plainly");
  void* parent = g_view.content ? static_cast<void*>(g_view.content) : canvas;
  for (int s = 0; s < kSections; ++s) build_section(s, parent, g_view.content != nullptr);

  for (int s = 0; s < kSections; ++s)
    if (g_sectionRoot[s]) ui::set_active(g_sectionRoot[s], s == g_section);
  fitList();  // the scroll range is the section showing, not every section's groups
  g_built = true;
  return true;
}

// The scroll range is the section showing, and it starts from that section's top.
void fitList() {
  if (!g_view.content) return;
  ui::set_scroll_height(g_view, g_sectionHeight[g_section]);
  followSelection();
}

// Arrow keys and a gamepad move the selection inside the game, which tells the menu nothing, so follow it: the row
// the selection has reached comes back into the view. A drag leaves the area where the player put it until the
// selection needs somewhere else.
void followSelection() {
  if (!g_view.scroll) return;
  void* selected = api::buttons::selected();
  if (!selected) return;
  for (const Row& r : g_rows) {
    if (r.kind == Nav || r.section != g_section) continue;
    if (r.barGo == selected || r.decGo == selected || r.incGo == selected) {
      ui::scroll_reveal(g_view, r.contentTop, kPitch);
      return;
    }
  }
}

void refreshNav() {
  for (Row& r : g_rows) {
    if (r.kind != Nav || !r.text) continue;
    const bool sel = r.arg == g_section;
    ui::set_color(r.text, sel ? kBone : kFog);
    ui::set_text(r.text, i18n::text(r.label).c_str());
  }
  if (g_navLit) ui::place(g_navLit, {0.5f, 0.5f}, {0.5f, 0.5f}, {kNavLeft + kNavBar / 2.0f, navOffset(g_section)},
                          {kNavBar, kNavPitch - 16.0f});
}

void switchSection(int section) {
  if (section == g_section || section < 0 || section >= kSections) return;
  if (g_sectionRoot[g_section]) ui::set_active(g_sectionRoot[g_section], false);
  g_section = section;
  if (g_sectionRoot[g_section]) ui::set_active(g_sectionRoot[g_section], true);
  refreshNav();
  fitList();
  refresh();
  selectSectionRow();
}

// Closing hides the whole canvas, groups, section column and slab together, so nothing of the menu can be left over
// the game.
void show(bool on) {
  g_active = on;
  if (g_canvas) ui::set_active(g_canvas, on);
  refreshNav();
}

void forgetCanvas() {
  g_active = false;
  g_built = false;
  g_canvas = nullptr;
  g_rows.clear();
  g_cards.clear();
  g_readFailed.clear();
  g_title = g_hint = g_refusalText = g_navLit = nullptr;
  for (void*& lamp : g_statusLamp) lamp = nullptr;
  for (int s = 0; s < kSections; ++s) {
    g_sectionRoot[s] = nullptr;
    g_sectionHeight[s] = 0.0f;
  }
  g_view = ui::ScrollView{};
  g_refusal = Why::None;
}

// The canvas died under us (a scene change while the menu was open): put the game back as open() found it and forget
// the rows; the next open builds fresh ones.
void recover() {
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  returnInput();
  if (g_wasCursorLocked) game::lock_cursor();
  forgetCanvas();
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
    case Action::BumpFlames: actions::bump_flames(r.arg); break;
    case Action::TypeChangeNow: actions::type_change_now(); break;
    case Action::ChaseMe:
      if (!actions::chase_me()) g_refusal = Why::NotHunting;
      break;
    case Action::MeToTruck:
      if (!actions::me_to_truck()) g_refusal = Why::NoTruck;
      break;
    case Action::PickMod: g_modPick += r.arg; break;  // wrapped where it is read: the list can change
    case Action::PickObjective: {
      const int n = actions::objective_count();
      if (n > 0) g_objectivePick = ((g_objectivePick + r.arg) % n + n) % n;
      break;
    }
    case Action::CompleteObjective:
    case Action::FailObjective:
      if (!actions::settle_objective(g_objectivePick, r.action == Action::CompleteObjective))
        g_refusal = Why::NoContract;
      break;
    case Action::SetNeverRunOut: s.neverRunOut = !s.neverRunOut; break;  // actions::tick keeps the supplies
    case Action::PickItem: {
      const int n = actions::item_count();
      if (n > 0) g_itemPick = ((g_itemPick + r.arg) % n + n) % n;
      break;
    }
    case Action::SpawnItem:
      switch (actions::spawn_item(g_itemPick)) {
        case actions::SpawnResult::Spawned: break;
        case actions::SpawnResult::NoneLeft: g_refusal = Why::NoSpare; break;
        case actions::SpawnResult::Failed: g_refusal = Why::NoPlayer; break;
      }
      break;
    case Action::CycleSightHold: s.sightHold = next_sight_hold(s.sightHold); break;
    case Action::OptionToggle:
    case Action::OptionChoice:
      // The friendly switch keeps its own rule (menu_rule.h): never while a hunt runs.
      if (std::string_view(actions::option_label(r.ref)) == "dev-menu.option.friendly" && c.hunting) {
        g_refusal = Why::Hunting;
        break;
      }
      actions::option_step(r.ref, r.action == Action::OptionToggle ? 1 : r.arg);
      break;
    case Action::Slide:
      if (const Why w = sliderRefuses(r); w != Why::None) {
        g_refusal = w;
        break;
      }
      sliderSet(r, sliderValue(r) + static_cast<float>(r.arg) * sliderStep(r));
      break;
    case Action::SanityToThreshold: {
      const int t = actions::hunt_threshold();
      if (t < 0) {
        g_refusal = Why::NoThreshold;
        break;
      }
      // On the line the check passes (sanity at or below it); one above, it fails.
      const int want = t + r.arg;
      actions::set_sanity(want < 0 ? 0 : (want > 100 ? 100 : want));
      if (s.lockSanity) s.lockedSanity = want < 0 ? 0 : (want > 100 ? 100 : want);
      break;
    }
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
      const actions::EndContract how = actions::end_contract_by_button();
      if (how == actions::EndContract::NoTruck) {
        g_refusal = Why::NoTruck;
        break;
      }
      if (how == actions::EndContract::DoorsShut) {
        g_refusal = Why::DoorsShut;
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
    case Action::ResetPlayerSpeed: actions::reset_speed_multiplier(); break;
    case Action::SetGhostBlind:
      // Normal, then sees nobody (its sight sensor emptied too), then sees everyone anywhere (the Deogen's rule).
      s.ghostSight = next_ghost_sight(s.ghostSight);
      actions::set_ghost_blind(s.ghostSight == kGhostSightNobody);
      break;
    case Action::SetDebugHud: s.debugHud = !s.debugHud; break;  // overlay.cpp draws it
    case Action::SetEspGhost: s.espGhost = !s.espGhost; break;
    case Action::SetEspRooms: s.espRooms = !s.espRooms; break;
    case Action::SetUnlockAll: s.unlockAll = !s.unlockAll; break;
    case Action::SetEspDoors: s.espDoors = !s.espDoors; break;  // overlay.cpp draws all three
    case Action::SetEspHiding: s.espHiding = !s.espHiding; break;
    case Action::SetEspSwitches: s.espSwitches = !s.espSwitches; break;
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
    case Action::SetLockSanity:
      // Holds the sanity it has now; the slider and the threshold clicks move the held value with it.
      s.lockSanity = !s.lockSanity;
      if (s.lockSanity) s.lockedSanity = static_cast<int>(std::lround(actions::sanity()));
      break;
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
    case Action::Info: break;
  }
}

// A click, from the game's onClick. The user pointer names the row (its index in g_rows) and the part: a section's
// bar switches section, an action's or a switch's bar runs its action, and a picker's step buttons run its step
// downward or upward by the row's own size. Rows only go away with the canvas, and their buttons with them.
void rowClicked(void* user) {
  const size_t i = rowOf(user);
  if (i >= g_rows.size()) return;
  const Row& r = g_rows[i];
  if (r.kind == Nav) {
    switchSection(r.arg);
    return;
  }
  if (r.kind == Picker || (r.kind == Slider && partOf(user) != kBar)) {
    Row step = r;
    step.arg = partOf(user) == kDec ? -r.arg : r.arg;
    run(step);
  } else if (r.kind == Slider) {
    // A click on the track was the pointer's: dragSliders() has set the number where it landed.
  } else if (r.kind == Act || r.kind == Toggle) {
    run(r);
  }
  actions::save_settings();  // the menu's settings are kept between runs (the user's report of 2026-11-13)
  refresh();  // the value the click changed, now rather than a frame late
}

// The starting point for arrow-key and gamepad navigation: the section's first control.
void selectSectionRow() {
  for (Row& r : g_rows) {
    if (r.kind == Nav || r.section != g_section) continue;
    if (r.bar && api::buttons::select(r.bar)) return;
    if (r.inc && api::buttons::select(r.inc)) return;
  }
  for (Row& r : g_rows)  // a section with nothing to press: its own entry in the section column takes it
    if (r.kind == Nav && r.arg == g_section && r.bar && api::buttons::select(r.bar)) return;
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
  actions::hold_input(false);  // the journal flag open() raised, back as it was
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

// Reread the shown section's values, the lamps, the header's status lamps and the refusal. Hidden sections are read
// when they show.
void refresh() {
  const std::string on = i18n::text(kOnKey);
  for (size_t i = 0; i < g_rows.size(); ++i) {
    Row& r = g_rows[i];
    if (r.kind == Nav || r.section != g_section || !r.valueText) continue;
    const std::string value = safeValueText(i);
    if (r.kind == Toggle) {
      const bool lit = value == on;
      if (r.lamp) ui::set_color(r.lamp, lit ? kAmber : kLampOff);
      ui::set_color(r.valueText, lit ? kAmber : kFog);
      ui::set_text(r.valueText, ("<align=right>" + value).c_str());
    } else if (r.kind == Picker) {
      ui::set_text(r.valueText, ("<align=center>" + value).c_str());
    } else if (r.kind == Slider) {
      ui::set_text(r.valueText, ("<align=center>" + value).c_str());
      if (r.fill) {
        const float low = sliderLow(r), high = sliderHigh(r);
        float f = high > low ? (sliderValue(r) - low) / (high - low) : 0.0f;
        f = f < 0.0f ? 0.0f : (f > 1.0f ? 1.0f : f);
        const float trackWide = kPickerWide - 2.0f * kStepWide - 12.0f;
        ui::place(r.fill, {0.5f, 1.0f}, {0.0f, 0.5f}, {r.trackLeft, r.trackLine}, {trackWide * f, kStepHigh});
      }
    } else {
      ui::set_text(r.valueText, ("<align=right>" + value).c_str());
    }
  }
  const bool lamps[3] = {actions::offline(), actions::session_on(), freeze_save(actions::session_on())};
  for (int i = 0; i < 3; ++i)
    if (g_statusLamp[i]) ui::set_color(g_statusLamp[i], lamps[i] ? kAmber : kLampOff);
  if (g_refusalText)
    ui::set_text(g_refusalText, g_refusal == Why::None ? "" : whyText(g_refusal).c_str());
  followSelection();  // the arrow keys and a gamepad moved the selection inside the game
}

}  // namespace

namespace {
// The pointer on a slider's track: a press there starts a drag, which sets the number under the pointer every frame
// until the button comes up. The list does not scroll during it.
void endDrag() {
  if (g_drag < 0) return;
  g_drag = -1;
  if (g_view.scroll && game::alive(g_view.scroll)) game::set_enabled(static_cast<void*>(g_view.scroll), true);
  actions::save_settings();  // the held sanity, among the settings kept between runs
}

void dragSliders() {
  float mx = 0.0f, my = 0.0f;
  if (!game::mouse_left_held() || !game::mouse_position(mx, my)) {
    endDrag();
    return;
  }
  const ui::Vec2 at{mx, my};
  if (g_drag < 0) {
    if (!game::mouse_left_pressed()) return;
    // Only a track the list shows: one scrolled out of the view is hidden by the clip, not gone.
    if (g_view.viewport && !ui::contains(static_cast<void*>(g_view.viewport), at)) return;
    for (size_t i = 0; i < g_rows.size(); ++i) {
      const Row& r = g_rows[i];
      if (r.kind != Slider || r.section != g_section || !r.bar) continue;
      if (!ui::contains(r.bar, at)) continue;
      if (const Why w = sliderRefuses(r); w != Why::None) {
        g_refusal = w;
        return;
      }
      g_refusal = Why::None;
      g_drag = static_cast<int>(i);
      if (g_view.scroll) game::set_enabled(static_cast<void*>(g_view.scroll), false);
      break;
    }
    if (g_drag < 0) return;
  }
  if (static_cast<size_t>(g_drag) >= g_rows.size()) {
    endDrag();
    return;
  }
  const Row& r = g_rows[static_cast<size_t>(g_drag)];
  ui::Vec2 inside{};
  if (!ui::normalized_point(r.bar, at, inside)) {
    // Off the track: past its left or right end the number goes to that end (the step buttons sit just outside it).
    if (r.dec && ui::contains(r.dec, at)) sliderSet(r, sliderLow(r));
    else if (r.inc && ui::contains(r.inc, at)) sliderSet(r, sliderHigh(r));
    return;
  }
  const float v = sliderLow(r) + inside.x * (sliderHigh(r) - sliderLow(r));
  if (std::fabs(v - sliderValue(r)) >= sliderStep(r) * 0.5f) sliderSet(r, v);
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
  // The journal's own hold: no walking and no looking behind the menu. A free cursor means another of the game's
  // overlays (the journal, its options page) holds the player already; that hold is the overlay's to release, so
  // the menu takes none, or closing the menu would release it under the overlay and hide the mouse there (the
  // user's test of 2026-11-13). tick() takes one if that overlay closes first.
  g_held = g_wasCursorLocked && actions::pause_player(true);
  actions::hold_input(true);              // and the journal's own gate: no click reaches the world
  // The clock is what the live tests of 2026-11-11 and 2026-11-12 blame for the picture after a close, so
  // the log says plainly what this open did to it (see sdk/systems/media.md).
  PM_LOG("dev-menu: open at timeScale %s, the player %s", number(scale).c_str(), g_held ? "held" : "not held");
}

void close() {
  if (!is_open()) return;
  endDrag();
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
  dragSliders();
  refresh();
  // Re-assert what open() asked the game for: closing the journal mid-menu switches the UI input module
  // off and releases the player's hold while the menu is still up, and both calls skip a state that
  // already matches, so an unchanged frame does nothing.
  if (g_module && !api::buttons::input_on(g_module)) api::buttons::input_set(g_module, true);
  if (g_held) {
    actions::pause_player(true);
  } else if (game::cursor_locked()) {
    // The overlay the menu opened over has closed and locked the cursor, taking its hold with it: the hold, and
    // locking the cursor again on close, are now the menu's.
    g_wasCursorLocked = true;
    actions::overlay_closed();
    g_held = actions::pause_player(true);
    game::free_cursor();
  }
  actions::hold_input(true);
}

void destroy() {
  // pmmod_stop must not leave the game frozen, the cursor loose, the UI input switched or the player
  // held because the menu was open.
  float restore = 0.0f;
  if (pause_close(g_pause, restore)) actions::set_time_scale(restore);
  returnInput();
  if (g_active && g_wasCursorLocked) game::lock_cursor();
  if (g_canvas) ui::destroy(g_canvas);
  forgetCanvas();  // the canvas, and everything on it, went above
  g_pause = Pause{};
  releasePlayer();  // last: the game's release may throw, and everything above is already put back
}

}  // namespace pm::devmenu::menu
