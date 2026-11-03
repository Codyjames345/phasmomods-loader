#include "consent.h"

#include <windows.h>

#include "contained.h"
#include "log.h"
#include "menu_panel.h"
#include "text_table.h"

namespace pm::consent {
namespace {

// Copying the menu's popup searches the scene: at most once a second.
constexpr ULONGLONG kTryEveryMs = 1000;
// How many times, with the main menu in view, the popup may be not found before the notice gives up. The menu reads as
// in view only once its scene is up, the popup's own scene; this bounds the wait should a build's popup never appear.
constexpr int kTriesInView = 10;

struct State {
  Notice notice;
  std::string fingerprint;
  std::wstring path;
  bool canRemember = false;
  Answer answer = Answer::Pending;
  ULONGLONG nextTry = 0;
  int failedInView = 0;     // times the popup could not be made with the main menu in view, for the whole run
  menu_panel::Panel panel;  // the notice, as the game's own popup
  // As last written to mods.log, where a notice nobody sees is diagnosed: why the main menu was out
  // of view (null: in view; "" before the first tick), whether copying the menu's popup failed, and
  // whether the popup was drawn.
  const char* waitingFor = "";
  bool cannotShow = false;  // it answered Declined for the player
  bool copyFailed = false;
  bool drawn = false;
} g;

void answer(Answer a) {
  g.answer = a;
  if (a == Answer::Agreed && !g.canRemember)
    PM_LOG("consent: this game build's hash could not be read, so the answer is not remembered (the notice will be shown "
           "again next time)");
  else if (a == Answer::Agreed && !remember(g.path, g.fingerprint))
    PM_LOG("consent: could not remember the answer (the notice will be shown again next time)");
  PM_LOG("consent: %s (%s)", a == Answer::Agreed ? "agreed" : "declined; mods stay off", g.fingerprint.c_str());
  hide();
}

// The notice can never be shown in this run: it answers Declined, so it never blocks the game.
void cannot_show(const char* why) {
  PM_LOG("consent: the notice cannot be shown as the menu's popup (%s); the game runs without mods", why);
  g.cannotShow = true;
  answer(Answer::Declined);
}

// open, with whatever it throws stopped here: a half-built popup would have no working button, and at the frame's boundary
// nothing would ever answer. The same code on the same scene would throw again, so a throw lasts.
bool open_contained(menu_panel::OpenFailure& failure) {
  const bool opened = host::contained("consent", "building the menu's popup", false, [&] {
    failure = {true, "building it threw"};
    const bool made = menu_panel::open(g.panel, g.notice.title, g.notice.body, {i18n::text("consent.agree"), i18n::text("consent.decline")}, &failure);
    if (made) menu_panel::fit_body(g.panel);  // the mod list and the paragraphs a build adds must not run over the buttons
    if (made) failure = {};
    return made;
  });
  return opened;
}

// The notice as the game's own popup over the main menu, while the menu is in view.
void tick_menu() {
  if (!menu_panel::is_open(g.panel)) {
    const ULONGLONG now = GetTickCount64();
    if (now < g.nextTry) return;
    g.nextTry = now + kTryEveryMs;
    menu_panel::OpenFailure failure;
    if (!open_contained(failure)) {
      if (failure.lasting || ++g.failedInView >= kTriesInView) {
        cannot_show(failure.why);
        return;
      }
      if (!g.copyFailed) PM_LOG("consent: the menu's popup could not be made yet (%s); trying again each second", failure.why);
      g.copyFailed = true;
      return;
    }
    g.copyFailed = false;
    g.drawn = false;
    PM_LOG("consent: shown as the menu's popup");
  }
  const int clicked = menu_panel::tick(g.panel);
  if (!menu_panel::is_open(g.panel)) {
    const char* why = menu_panel::why_menu_hidden();
    PM_LOG("consent: the menu's popup closed (%s)", why ? why : "it was switched off");
    return;
  }
  if (menu_panel::drawn(g.panel) != g.drawn) {
    g.drawn = !g.drawn;
    PM_LOG(g.drawn ? "consent: the menu's popup is drawn" : "consent: the menu's popup is not drawn (it or its canvas is switched off)");
  }
  if (clicked >= 0) answer(clicked == 0 ? Answer::Agreed : Answer::Declined);
}

}  // namespace

void begin(const Notice& notice, const build::Hashes& game, const std::wstring& rememberedPath) {
  g = {};
  g.notice = notice;
  g.fingerprint = fingerprint(notice, game);
  g.path = rememberedPath;
  g.canRemember = can_remember(game);
  if (g.canRemember && remembered(g.path, g.fingerprint)) {
    g.answer = Answer::Agreed;
    PM_LOG("consent: agreed earlier to this notice (%s)", g.fingerprint.c_str());
  } else if (!menu_panel::listeners_available()) {
    // The notice's buttons take a click through a listener on the game's event: without one it could never be
    // answered, and it never blocks the game.
    g.answer = Answer::Declined;
    g.cannotShow = true;
    PM_LOG("consent: this game build gives no listener on a button to answer the notice with; the game runs without "
           "mods");
  } else if (!menu_panel::menu_available()) {
    // The notice waits for the main menu, which the host could never see: it could never be shown.
    g.answer = Answer::Declined;
    g.cannotShow = true;
    PM_LOG("consent: this game build does not let the host read the main menu to show the notice in; the game runs "
           "without mods");
  } else if (!menu_panel::can_copy()) {
    // The notice is the game's popup, which this build gives the host no way to copy: it could never be shown.
    g.answer = Answer::Declined;
    g.cannotShow = true;
    PM_LOG("consent: this game build lacks an entry a copy of the menu's popup needs; the game runs without mods");
  }
}

Answer tick() {
  if (g.answer != Answer::Pending) return g.answer;
  const char* hidden = menu_panel::why_menu_hidden();
  if (hidden != g.waitingFor) {
    if (hidden) PM_LOG("consent: waiting for the main menu (%s)", hidden);
    else PM_LOG("consent: the main menu is in view");
    g.waitingFor = hidden;
  }
  if (hidden) {  // however long it takes: the mods wait for the answer
    hide();
    g.nextTry = 0;
    return g.answer;
  }
  tick_menu();
  return g.answer;
}

void hide() {
  const bool closed = host::contained("consent", "taking the menu's popup away", false, [] {
    menu_panel::close(g.panel);
    return true;
  });
  if (!closed) g.panel = {};  // it took away what it could: nothing here points at the copy any more
}

std::string off_cause() {
  return g.cannotShow ? i18n::text("off.notice-cannot-show") : std::string();
}

}  // namespace pm::consent
