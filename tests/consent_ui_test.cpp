// Offline tests for how the consent notice is shown and answered (host/consent.cpp), over a fake main menu and popup.
// Run: build\consent_ui_test.exe
#include <windows.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.h"
#include "text_fixture.h"
#include "consent.h"
#include "il2cpp.h"
#include "menu_panel.h"
#include "mod_session.h"

namespace {
struct Element {
  std::string name;
  bool alive = false;
};
bool menuVisible = false, cursorLocked = true;
bool listenersAvailable = true, menuAvailable = true, canCopy = true;
bool nativeAvailable = true;  // the game's popup can be copied now
bool popupLasting = false;    // when it cannot: whether that lasts (open's OpenFailure)
int openCalls = 0;
int fitCalls = 0;  // the notice may shrink to fit the paper, so a long list cannot run over its buttons
enum class OpenThrows { Never, BeforeACopy, AfterACopy };
OpenThrows openThrows = OpenThrows::Never;  // the point at which building the popup throws
bool managedThrow = false;                  // what it throws: the runtime's exception, or anything else
bool closeThrows = false;
bool popupDrawn = true, closeOnTick = false;
std::vector<std::string> logged;  // mods.log lines, in order
int count_logged(const std::string& line) { return static_cast<int>(std::count(logged.begin(), logged.end(), line)); }
int nativeSelection = -1;
ULONGLONG fixtureTime = 1000;
Element nativePopup;
std::wstring fixturePath;
const pm::consent::Notice testNotice{"Fixture notice", "Fixture consent body"};
const pm::build::Hashes testBuild{"aaaa", "bbbb"};

/** Advance production consent time without depending on the wall clock. */
ULONGLONG WINAPI clock_ms() { return fixtureTime; }
}  // namespace

#define GetTickCount64 clock_ms
#include "../host/consent.cpp"
#undef GetTickCount64

namespace pm::menu_panel {
void fit_body(const Panel&) { ++fitCalls; }
bool menu_available() { return menuAvailable; }
bool listeners_available() { return listenersAvailable; }
bool can_copy() { return canCopy; }
/** Expose the game's current menu focus independently of popup availability. */
const char* why_menu_hidden() { return menuVisible ? nullptr : "the fixture menu is out of view"; }
bool drawn(const Panel&) { return popupDrawn; }
/** Create a native popup even outside menu focus, so consent must enforce its own gate. */
bool open(Panel& panel, const std::string& title, const std::string& body, const std::vector<std::string>& buttons,
          OpenFailure* failure) {
  ++openCalls;
  const auto throw_now = [] {
    if (managedThrow) throw Il2CppExceptionWrapper{nullptr};
    throw std::runtime_error("fixture");
  };
  if (openThrows == OpenThrows::BeforeACopy) throw_now();
  if (openThrows == OpenThrows::AfterACopy) {
    nativePopup = {"Half built", true};
    panel.root = &nativePopup;
    throw_now();
  }
  if (!nativeAvailable) {
    if (failure) *failure = {popupLasting, popupLasting ? "the fixture popup has no button" : "the fixture popup is not in the scene"};
    return false;
  }
  CHECK(!nativePopup.alive);
  CHECK(title == testNotice.title && body == testNotice.body);
  CHECK(buttons == std::vector<std::string>({"I agree", "Play without mods"}));
  nativePopup = {"Native consent", true};
  panel.root = &nativePopup;
  return true;
}
/** Remove the popup from the fixture scene. */
void close(Panel& panel) {
  if (panel.root) static_cast<Element*>(panel.root)->alive = false;
  if (closeThrows) {
    closeThrows = false;
    throw std::runtime_error("fixture");
  }
  panel = {};
}
/** Observe scene lifetime through the same public boundary as production. */
bool is_open(const Panel& panel) { return panel.root && static_cast<Element*>(panel.root)->alive; }
/** Simulate the native popup taking cursor control and receiving a user answer. */
int tick(Panel& panel) {
  if (closeOnTick) {  // the game switched the copy off: the panel closes itself
    close(panel);
    return -1;
  }
  cursorLocked = false;
  const int selection = nativeSelection;
  nativeSelection = -1;
  return selection;
}
}  // namespace pm::menu_panel

namespace il2cpp {
uint32_t gc_hold(void*) { return 0; }
void gc_release(uint32_t) {}
void log_exception(const char* where, const void*) { logged.emplace_back(where); }
}  // namespace il2cpp

namespace pm::log {
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  logged.emplace_back(line);
}
}  // namespace pm::log

namespace {
using pm::consent::Answer;

/** Start an independent pending notice without reaching into controller state. */
void reset() {
  pm::consent::hide();
  DeleteFileW(fixturePath.c_str());
  nativePopup = {};
  menuVisible = false;
  nativeAvailable = true;
  popupLasting = false;
  openCalls = 0;
  fitCalls = 0;
  cursorLocked = true;
  listenersAvailable = menuAvailable = canCopy = true;
  nativeSelection = -1;
  fixtureTime = 1000;
  popupDrawn = true;
  closeOnTick = false;
  openThrows = OpenThrows::Never;
  managedThrow = closeThrows = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  logged.clear();
}

/** A notice nobody sees is diagnosed from mods.log: every change in what it waits for and in how
    it is shown is written there once, not every frame. */
void logs_what_the_notice_waits_for() {
  reset();
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(count_logged("consent: waiting for the main menu (the fixture menu is out of view)") == 1);
  menuVisible = true;
  pm::consent::tick();
  pm::consent::tick();
  CHECK(count_logged("consent: the main menu is in view") == 1);
  CHECK(count_logged("consent: shown as the menu's popup") == 1);
  CHECK(count_logged("consent: the menu's popup is drawn") == 1);
  popupDrawn = false;
  pm::consent::tick();
  pm::consent::tick();
  CHECK(count_logged("consent: the menu's popup is not drawn (it or its canvas is switched off)") == 1);
  closeOnTick = true;
  pm::consent::tick();
  CHECK(count_logged("consent: the menu's popup closed (it was switched off)") == 1);
  nativeAvailable = false;
  for (int second = 0; second < 3; ++second) {
    fixtureTime += 1000;
    pm::consent::tick();
  }
  CHECK(count_logged("consent: the menu's popup could not be made yet (the fixture popup is not in the scene); trying "
                     "again each second") == 1);
  menuVisible = false;
  pm::consent::tick();
  CHECK(count_logged("consent: waiting for the main menu (the fixture menu is out of view)") == 2);
}

/** Pending consent must leave lobby exploration and its cursor untouched. */
void stays_hidden_outside_main_menu() {
  for (bool available : {true, false}) {
    reset();
    nativeAvailable = available;
    CHECK(pm::consent::tick() == Answer::Pending);
    CHECK(!nativePopup.alive && cursorLocked);
    fixtureTime += 20001;
    CHECK(pm::consent::tick() == Answer::Pending);
    CHECK(!nativePopup.alive && cursorLocked && openCalls == 0);
  }
}

/** Main-menu focus owns popup lifetime, including fast exit and reentry. */
void follows_main_menu_focus() {
  reset();
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive && !cursorLocked);
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive);

  menuVisible = false;
  cursorLocked = true;
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(!nativePopup.alive && cursorLocked);
  CHECK(!pm::consent::remembered(fixturePath, pm::consent::fingerprint(testNotice, testBuild)));

  nativeSelection = -1;
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive);
}

/** The popup keeps agreement, decline, and persisted consent as they were. */
void answers_and_remembered_notices() {
  for (int selection : {0, 1}) {
    reset();
    menuVisible = true;
    CHECK(pm::consent::tick() == Answer::Pending);
    nativeSelection = selection;
    const Answer expected = selection == 0 ? Answer::Agreed : Answer::Declined;
    CHECK(pm::consent::tick() == expected);
    CHECK(!nativePopup.alive);
    CHECK(pm::consent::tick() == expected);
    const auto print = pm::consent::fingerprint(testNotice, testBuild);
    CHECK(pm::consent::remembered(fixturePath, print) == (selection == 0));

    menuVisible = false;
    cursorLocked = true;
    pm::consent::begin(testNotice, testBuild, fixturePath);
    CHECK(pm::consent::tick() == (selection == 0 ? Answer::Agreed : Answer::Pending));
    CHECK(!nativePopup.alive && cursorLocked);
  }
}

// The notice waits for the main menu however long the game takes to show it (a slow load, a long time in a lobby), and
// is shown the moment it comes into view: there is no time limit, and nothing else is ever drawn in its place.
void waits_for_the_menu_without_a_time_limit() {
  reset();
  for (int minute = 0; minute < 120; ++minute) {
    fixtureTime += 60000;
    CHECK(pm::consent::tick() == Answer::Pending);
  }
  CHECK(!nativePopup.alive && openCalls == 0 && cursorLocked);
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive && openCalls == 1 && fitCalls == 1);
}

// A popup that can never be made answers Declined, saying why, rather than wait for good: at begin when this build
// cannot copy it at all, and at the first try when open() says the failure lasts. One that is only not in the scene yet
// is tried again each second, and those tries count only while the menu is in view (the menu still loading spends none
// of them), so it cannot be tried for ever: the tenth in view answers Declined.
void popup_that_cannot_be_copied_answers_declined() {
  reset();
  canCopy = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  CHECK(pm::consent::tick() == Answer::Declined && openCalls == 0);
  CHECK(count_logged("consent: this game build lacks an entry a copy of the menu's popup needs; the game runs without "
                     "mods") == 1);

  reset();
  menuVisible = true;
  nativeAvailable = false;
  popupLasting = true;
  CHECK(pm::consent::tick() == Answer::Declined && openCalls == 1);
  CHECK(count_logged("consent: the notice cannot be shown as the menu's popup (the fixture popup has no button); the game "
                     "runs without mods") == 1);
  CHECK(!pm::consent::remembered(fixturePath, pm::consent::fingerprint(testNotice, testBuild)));

  reset();
  nativeAvailable = false;
  menuVisible = true;
  for (int tryInView = 1; tryInView < pm::consent::kTriesInView; ++tryInView) {
    CHECK(pm::consent::tick() == Answer::Pending);
    CHECK(pm::consent::tick() == Answer::Pending);  // not again within the second
    // The menu goes out of view for a long time between tries: none is spent while it is.
    menuVisible = false;
    for (int second = 0; second < 30; ++second) {
      fixtureTime += 1000;
      CHECK(pm::consent::tick() == Answer::Pending);
    }
    menuVisible = true;
  }
  CHECK(openCalls == pm::consent::kTriesInView - 1);
  CHECK(pm::consent::tick() == Answer::Declined && openCalls == pm::consent::kTriesInView);
  CHECK(count_logged("consent: the notice cannot be shown as the menu's popup (the fixture popup is not in the scene); the "
                     "game runs without mods") == 1);
  CHECK(pm::consent::tick() == Answer::Declined && openCalls == pm::consent::kTriesInView);  // asked no more

  // A popup that turns up within the tries is shown as ever.
  reset();
  nativeAvailable = false;
  menuVisible = true;
  for (int i = 0; i < 5; ++i) {
    CHECK(pm::consent::tick() == Answer::Pending);
    fixtureTime += 1000;
  }
  nativeAvailable = true;
  CHECK(pm::consent::tick() == Answer::Pending && nativePopup.alive);
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Agreed);

  // Building the popup throws, before a copy is made or with one half built: the same code on the same scene would throw
  // again, so it answers Declined at the first try, with one line, whatever it threw. Nothing stays on screen, even when
  // taking the half-built popup away throws too.
  const char* const kThrew = "consent: the notice cannot be shown as the menu's popup (building it threw); the game runs "
                             "without mods";
  for (OpenThrows point : {OpenThrows::BeforeACopy, OpenThrows::AfterACopy})
    for (bool managed : {false, true})
      for (bool closing : {false, true}) {
        reset();
        menuVisible = true;
        openThrows = point;
        managedThrow = managed;
        closeThrows = closing;
        CHECK(pm::consent::tick() == Answer::Declined && openCalls == 1);
        CHECK(!nativePopup.alive);
        CHECK(count_logged(kThrew) == 1);
        CHECK(count_logged("consent: managed exception while building the menu's popup") == (managed ? 1 : 0));
        CHECK(!pm::consent::remembered(fixturePath, pm::consent::fingerprint(testNotice, testBuild)));
        CHECK(pm::consent::tick() == Answer::Declined && openCalls == 1);  // asked no more
      }
}

// Declined, by the player or because the notice cannot be shown, starts no mod session: the host makes no hook and
// starts no mod, so the game runs as it would without the loader. Nothing is left on screen and nothing is remembered,
// so the next start asks again.
void declined_runs_the_game_without_mods() {
  reset();
  menuVisible = true;
  nativeAvailable = false;
  popupLasting = true;
  const Answer answered = pm::consent::tick();
  CHECK(answered == Answer::Declined && !nativePopup.alive);
  int steps = 0;
  static int* counted = nullptr;
  counted = &steps;
  const pm::mod_session::Steps count{[] { return ++*counted, true; }, [] { return ++*counted, size_t{0}; },
                                     [] { ++*counted; }};
  size_t modHooks = 3;
  CHECK(!pm::mod_session::begin(answered, count, modHooks) && steps == 0 && modHooks == 3);
  CHECK(!pm::consent::remembered(fixturePath, pm::consent::fingerprint(testNotice, testBuild)));
  pm::consent::begin(testNotice, testBuild, fixturePath);
  nativeAvailable = true;
  CHECK(pm::consent::tick() == Answer::Pending && nativePopup.alive);
}

// A notice that cannot be shown says so to the Mods panel, in each way it can fail. The player's own answers do not, nor a
// notice still waiting: "Play without mods" is a choice, and needs no explaining.
void off_cause_is_only_for_a_notice_that_could_not_be_shown() {
  constexpr char kCause[] = "The notice can't be shown on this game build, so the mods stay off. mods.log says why.";
  reset();
  CHECK(pm::consent::off_cause().empty());
  menuVisible = true;
  pm::consent::tick();
  CHECK(pm::consent::off_cause().empty());
  nativeSelection = 1;
  CHECK(pm::consent::tick() == Answer::Declined);
  CHECK(pm::consent::off_cause().empty());
  reset();
  menuVisible = true;
  pm::consent::tick();
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Agreed);
  CHECK(pm::consent::off_cause().empty());

  reset();
  canCopy = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  CHECK(pm::consent::off_cause() == kCause);
  reset();
  listenersAvailable = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  CHECK(pm::consent::off_cause() == kCause);
  reset();
  menuAvailable = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  CHECK(pm::consent::off_cause() == kCause);
  reset();
  menuVisible = true;
  nativeAvailable = false;
  popupLasting = true;
  CHECK(pm::consent::tick() == Answer::Declined);
  CHECK(pm::consent::off_cause() == kCause);
  reset();
  menuVisible = true;
  openThrows = OpenThrows::BeforeACopy;
  CHECK(pm::consent::tick() == Answer::Declined);
  CHECK(pm::consent::off_cause() == kCause);
}

// "Play without mods" on a build the offline check has not passed is as on any other: the session makes no hook and starts
// no mod, nothing is remembered, and the next start asks again. So a build that was agreed to is asked only once, and a
// new one always asks.
void declined_on_an_unseen_build_stays_vanilla() {
  const pm::build::Hashes unseen{"cccc", "dddd"};
  reset();
  pm::consent::begin(testNotice, unseen, fixturePath);
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Pending);
  nativeSelection = 1;
  const Answer answered = pm::consent::tick();
  CHECK(answered == Answer::Declined && !nativePopup.alive);
  int steps = 0;
  static int* counted = nullptr;
  counted = &steps;
  const pm::mod_session::Steps count{[] { return ++*counted, true; }, [] { return ++*counted, size_t{0}; }, [] { ++*counted; }};
  size_t modHooks = 3;
  CHECK(!pm::mod_session::begin(answered, count, modHooks) && steps == 0 && modHooks == 3);
  CHECK(!pm::consent::remembered(fixturePath, pm::consent::fingerprint(testNotice, unseen)));
  pm::consent::begin(testNotice, unseen, fixturePath);
  CHECK(pm::consent::tick() == Answer::Pending);
  // Agreed, it holds for that build, and a further build asks again.
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Agreed);
  pm::consent::begin(testNotice, unseen, fixturePath);
  CHECK(pm::consent::tick() == Answer::Agreed);
  pm::consent::begin(testNotice, {"eeee", "dddd"}, fixturePath);
  nativeSelection = -1;
  CHECK(pm::consent::tick() == Answer::Pending);
}

// A build whose hash could not be read could be any build: the notice asks, as on an unseen one, and asks again next start
// however it was answered, even over an agreement a fingerprint of the same text happens to hold.
void an_unreadable_hash_asks_every_time() {
  const pm::build::Hashes unreadable{"", "bbbb"};
  reset();
  CHECK(pm::consent::remember(fixturePath, pm::consent::fingerprint(testNotice, unreadable)));
  pm::consent::begin(testNotice, unreadable, fixturePath);
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Pending);
  nativeSelection = 0;
  logged.clear();
  CHECK(pm::consent::tick() == Answer::Agreed);  // the mods run for this start
  CHECK(count_logged("consent: this game build's hash could not be read, so the answer is not remembered (the notice will "
                     "be shown again next time)") == 1);
  DeleteFileW(fixturePath.c_str());
  nativeSelection = -1;
  nativePopup = {};
  pm::consent::begin(testNotice, unreadable, fixturePath);
  CHECK(pm::consent::tick() == Answer::Pending);
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Agreed);
  CHECK(GetFileAttributesW(fixturePath.c_str()) == INVALID_FILE_ATTRIBUTES);  // nothing written
}

// The notice's buttons take a click through the loader's listener on the game's event. A build without one could never
// answer the notice, and the notice never blocks the game, so it answers Declined at once and says so; a notice agreed
// to earlier needs no answer.
void no_listener_declines_the_notice() {
  reset();
  listenersAvailable = false;
  logged.clear();
  pm::consent::begin(testNotice, testBuild, fixturePath);
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Declined);
  CHECK(!nativePopup.alive);
  CHECK(count_logged("consent: this game build gives no listener on a button to answer the notice with; the game runs "
                     "without mods") == 1);
  CHECK(pm::consent::tick() == Answer::Declined);

  reset();
  menuVisible = true;
  pm::consent::tick();
  nativeSelection = 0;
  CHECK(pm::consent::tick() == Answer::Agreed);  // remembered
  listenersAvailable = false;
  pm::consent::begin(testNotice, testBuild, fixturePath);
  CHECK(pm::consent::tick() == Answer::Agreed);
}

// A build whose main-menu entries did not resolve never shows the menu, so the notice would wait for good: it answers
// Declined at once and says so.
void no_menu_declines_the_notice() {
  reset();
  menuAvailable = false;
  logged.clear();
  pm::consent::begin(testNotice, testBuild, fixturePath);
  menuVisible = true;
  CHECK(pm::consent::tick() == Answer::Declined);
  CHECK(!nativePopup.alive);
  CHECK(count_logged("consent: this game build does not let the host read the main menu to show the notice in; the "
                     "game runs without mods") == 1);
}
}  // namespace

int main() {
  use_text({"i18n/default.json"});
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  fixturePath = std::wstring(temporary) + L"pg-consent-ui-" + std::to_wstring(GetCurrentProcessId()) + L".txt";
  stays_hidden_outside_main_menu();
  follows_main_menu_focus();
  answers_and_remembered_notices();
  waits_for_the_menu_without_a_time_limit();
  popup_that_cannot_be_copied_answers_declined();
  declined_runs_the_game_without_mods();
  declined_on_an_unseen_build_stays_vanilla();
  off_cause_is_only_for_a_notice_that_could_not_be_shown();
  an_unreadable_hash_asks_every_time();
  no_listener_declines_the_notice();
  no_menu_declines_the_notice();
  logs_what_the_notice_waits_for();
  pm::consent::hide();
  DeleteFileW(fixturePath.c_str());
  std::printf(g_failures ? "%d check(s) failed\n" : "all consent UI tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
