#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <vector>

#include "check.h"
#include "consent.h"
#include "game.h"
#include "menu_panel.h"

namespace {
struct Element {
  std::string name;
  bool alive = false;
};
std::array<Element, 32> elements;
size_t nextElement = 0;
bool menuVisible = false, nativeAvailable = true, cursorLocked = true;
bool pointerPressed = false;
bool popupDrawn = true, closeOnTick = false;
std::vector<std::string> logged;  // mods.log lines, in order
int count_logged(const std::string& line) { return static_cast<int>(std::count(logged.begin(), logged.end(), line)); }
int nativeSelection = -1;
std::string overlaySelection;
ULONGLONG fixtureTime = 1000;
Element nativePopup;
std::wstring fixturePath;
const pg::consent::Notice testNotice{"Fixture notice", "Fixture consent body"};

/** Advance production consent time without depending on the wall clock. */
ULONGLONG WINAPI clock_ms() { return fixtureTime; }

/** Allocate stable fixture objects, just as Unity retains component identities. */
void* make_element(const char* name) {
  CHECK(nextElement < elements.size());
  if (nextElement >= elements.size()) return nullptr;
  auto& element = elements[nextElement++];
  element = {name, true};
  return &element;
}

/** Observe whether an overlay exists through the fixture scene. */
bool overlay_visible() {
  for (const auto& element : elements)
    if (element.alive && element.name == "PhasmoMods notice") return true;
  return false;
}
}

#define GetTickCount64 clock_ms
#include "../host/consent.cpp"
#undef GetTickCount64

namespace il2cpp {
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
/** The live fixture element named `name` is held. */
bool held(const char* name) {
  for (void* object : holds)
    if (object && static_cast<Element*>(object)->alive && static_cast<Element*>(object)->name == name) return true;
  return false;
}
int live_holds() {
  int count = 0;
  for (void* object : holds) count += object != nullptr;
  return count;
}
}
namespace pg::menu_panel {
/** Expose the game's current menu focus independently of popup availability. */
const char* why_menu_hidden() { return menuVisible ? nullptr : "the fixture menu is out of view"; }
bool drawn(const Panel&) { return popupDrawn; }
/** Create a native popup even outside menu focus, so consent must enforce its own gate. */
bool open(Panel& panel, const std::string& title, const std::string& body,
          const std::vector<std::string>& buttons) {
  if (!nativeAvailable) return false;
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
  panel = {};
}
/** Observe scene lifetime through the same public boundary as production. */
bool is_open(const Panel& panel) {
  return panel.root && static_cast<Element*>(panel.root)->alive;
}
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
}

namespace pg::game {
bool alive(void* object) { return object && static_cast<Element*>(object)->alive; }
bool cursor_locked() { return cursorLocked; }
void free_cursor() { cursorLocked = false; }
bool mouse_position(float& x, float& y) { x = y = 0; return true; }
bool mouse_left_pressed() { return pointerPressed; }
}

namespace pg::ui {
void* create_canvas(const char* name, int) { return make_element(name); }
void* create_image(void*, const char* name, Color) { return make_element(name); }
void* create_text(void*, const char* name, float) { return make_element(name); }
void destroy(void* object) { static_cast<Element*>(object)->alive = false; }
void place(void*, Vec2, Vec2, Vec2, Vec2) {}
void block_pointer(void*) {}
void set_raycast_target(void*, bool) {}
void set_text(void*, const char*) {}
void set_word_wrap(void*, bool) {}
void set_alignment(void*, Align) {}
void set_color(void*, Color) {}
bool contains(void* object, Vec2, void*) {
  return object && static_cast<Element*>(object)->name == overlaySelection;
}
}

namespace pg::log {
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  logged.emplace_back(line);
}
}

namespace {
using pg::consent::Answer;

/** Start an independent pending notice without reaching into controller state. */
void reset() {
  pg::consent::hide();
  DeleteFileW(fixturePath.c_str());
  elements = {};
  nextElement = 0;
  nativePopup = {};
  menuVisible = false;
  nativeAvailable = true;
  cursorLocked = true;
  pointerPressed = false;
  nativeSelection = -1;
  overlaySelection.clear();
  fixtureTime = 1000;
  popupDrawn = true;
  closeOnTick = false;
  pg::consent::begin(testNotice, fixturePath);
  logged.clear();
}

/** A notice nobody sees is diagnosed from mods.log: every change in what it waits for and in how
    it is shown is written there once, not every frame. */
void logs_what_the_notice_waits_for() {
  reset();
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(count_logged("consent: waiting for the main menu (the fixture menu is out of view)") == 1);
  menuVisible = true;
  pg::consent::tick();
  pg::consent::tick();
  CHECK(count_logged("consent: the main menu is in view") == 1);
  CHECK(count_logged("consent: shown as the menu's popup") == 1);
  CHECK(count_logged("consent: the menu's popup is drawn") == 1);
  popupDrawn = false;
  pg::consent::tick();
  pg::consent::tick();
  CHECK(count_logged("consent: the menu's popup is not drawn (it or its canvas is switched off)") == 1);
  closeOnTick = true;
  pg::consent::tick();
  CHECK(count_logged("consent: the menu's popup closed (it was switched off)") == 1);
  nativeAvailable = false;
  for (int second = 0; second < 3; ++second) {
    fixtureTime += 1000;
    pg::consent::tick();
  }
  CHECK(count_logged("consent: the menu's popup could not be copied; trying again each second") == 1);
  fixtureTime += 20001;
  pg::consent::tick();
  pg::consent::tick();
  CHECK(overlay_visible() && count_logged("consent: shown as the mod's own overlay") == 1);
  menuVisible = false;
  pg::consent::tick();
  CHECK(count_logged("consent: waiting for the main menu (the fixture menu is out of view)") == 2);
}

/** Pending consent must leave lobby exploration and its cursor untouched. */
void stays_hidden_outside_main_menu() {
  for (bool available : {true, false}) {
    reset();
    nativeAvailable = available;
    CHECK(pg::consent::tick() == Answer::Pending);
    CHECK(!nativePopup.alive && !overlay_visible() && cursorLocked);
    fixtureTime += 20001;
    CHECK(pg::consent::tick() == Answer::Pending);
    CHECK(!nativePopup.alive && !overlay_visible() && cursorLocked);
  }
}

/** Main-menu focus owns popup lifetime, including fast exit and reentry. */
void follows_main_menu_focus() {
  reset();
  menuVisible = true;
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive && !overlay_visible() && !cursorLocked);
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive);

  menuVisible = false;
  cursorLocked = true;
  nativeSelection = 0;
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(!nativePopup.alive && !overlay_visible() && cursorLocked);
  CHECK(!pg::consent::remembered(fixturePath, pg::consent::fingerprint(testNotice)));

  nativeSelection = -1;
  menuVisible = true;
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(nativePopup.alive && !overlay_visible());
}

/** Missing native UI may fall back only while the player is looking at the menu. */
void fallback_follows_main_menu_focus() {
  reset();
  nativeAvailable = false;
  menuVisible = true;
  CHECK(pg::consent::tick() == Answer::Pending);
  fixtureTime += 20001;
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(overlay_visible() && !nativePopup.alive && !cursorLocked);
  // The overlay and its buttons, kept between frames, stay reachable for the GC until it is hidden.
  CHECK(il2cpp::held("PhasmoMods notice") && il2cpp::held("I agree") && il2cpp::held("Play without mods"));

  menuVisible = false;
  cursorLocked = true;
  pointerPressed = true;
  overlaySelection = "I agree";
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(!overlay_visible() && !nativePopup.alive && cursorLocked);
  CHECK(il2cpp::live_holds() == 0);
  CHECK(!pg::consent::remembered(fixturePath, pg::consent::fingerprint(testNotice)));

  pointerPressed = false;
  menuVisible = true;
  fixtureTime += 20001;
  CHECK(pg::consent::tick() == Answer::Pending);
  CHECK(overlay_visible() && !nativePopup.alive);
}

/** Both presentations preserve agreement, decline, and persisted consent semantics. */
void answers_and_remembered_notices() {
  for (bool native : {true, false}) {
    for (int selection : {0, 1}) {
      reset();
      menuVisible = true;
      nativeAvailable = native;
      fixtureTime += 20001;
      CHECK(pg::consent::tick() == Answer::Pending);
      if (native) nativeSelection = selection;
      else {
        pointerPressed = true;
        overlaySelection = selection == 0 ? "I agree" : "Play without mods";
      }
      const Answer expected = selection == 0 ? Answer::Agreed : Answer::Declined;
      CHECK(pg::consent::tick() == expected);
      CHECK(!nativePopup.alive && !overlay_visible());
      CHECK(pg::consent::tick() == expected);
      const auto print = pg::consent::fingerprint(testNotice);
      CHECK(pg::consent::remembered(fixturePath, print) == (selection == 0));

      menuVisible = false;
      cursorLocked = true;
      pointerPressed = false;
      pg::consent::begin(testNotice, fixturePath);
      CHECK(pg::consent::tick() == (selection == 0 ? Answer::Agreed : Answer::Pending));
      CHECK(!nativePopup.alive && !overlay_visible() && cursorLocked);
    }
  }
}
}

/** Exercise the production controller with deterministic game and UI boundaries. */
int main() {
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  fixturePath = std::wstring(temporary) + L"pg-consent-ui-" +
      std::to_wstring(GetCurrentProcessId()) + L".txt";
  stays_hidden_outside_main_menu();
  follows_main_menu_focus();
  fallback_follows_main_menu_focus();
  answers_and_remembered_notices();
  logs_what_the_notice_waits_for();
  pg::consent::hide();
  DeleteFileW(fixturePath.c_str());
  return g_failures ? 1 : 0;
}
