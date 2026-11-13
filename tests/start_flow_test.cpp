// What the closed-room start decides around the closer (host/start_flow.h, host/start_close.h): every situation calls
// exactly what the host must call, in the same order, with the same logs and popups. These were written against
// host/suite.cpp's behaviour as it stood before the move (final review M12): suite.cpp is not built offline, which is
// how R1 and R2 got through. A public lobby's start never reaches these two functions: the gate answers it before it
// calls the suite's startSend (host/lobby_gate.cpp, decide_rpc), and lobby_gate_test's a_public_lobbys_start_goes_untouched
// pins that at the level where the decision lives.
#include "start_flow.h"

#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "text_fixture.h"

namespace {

namespace clos = pm::startclose;
namespace flow = pm::startflow;
using pm::lobby_gate::Send;

constexpr uint64_t kRoom = 77;
constexpr int32_t kLocal = 1;

// The closer's caller's answer of the join guard, in miniature: the flow only fills it, hands it back and tells from it.
struct Answer {
  bool go = true;
  int token = 0;  // which reading of the room filled this one
};

uint64_t g_now = 0, g_room = kRoom;
int g_local = kLocal;
bool g_master = true, g_lobby = true;
bool g_closeOk = true, g_markerOk = true, g_releaseOk = true, g_answerGo = true;
int g_answerToken = 0, g_answerAsks = 0;
uint32_t g_markerSent = 0;  // what the bytes handed to the marker send actually carried
std::vector<std::string> g_events, g_logs;
std::vector<std::pair<std::string, std::string>> g_popups;
Answer g_told;

void reset() {
  g_now = 0;
  g_room = kRoom;
  g_local = kLocal;
  g_master = g_lobby = true;
  g_closeOk = g_markerOk = g_releaseOk = g_answerGo = true;
  g_answerToken = g_answerAsks = 0;
  g_markerSent = 0;
  g_events.clear();
  g_logs.clear();
  g_popups.clear();
  g_told = Answer{};
}

uint64_t clock_now() { return g_now; }
uint64_t room_now() { return g_room; }
int local_actor() { return g_local; }
bool is_master() { return g_master; }
bool in_lobby() { return g_lobby; }
bool close_room() {
  g_events.push_back("close");
  return g_closeOk;
}
bool send_marker(const uint8_t* marker, size_t size) {
  g_events.push_back("marker");
  g_markerSent = clos::read_marker(marker, size);
  return g_markerOk;
}
void reopen_room() { g_events.push_back("reopen"); }
bool release_held() {
  g_events.push_back("release");
  return g_releaseOk;
}
void drop_held() { g_events.push_back("drop"); }
bool start_answer(Answer& answer) {
  g_events.push_back("ask");
  ++g_answerAsks;
  answer = Answer{g_answerGo, g_answerToken};
  return g_answerGo;
}
void tell_start_waits(const Answer& answer) {
  g_events.push_back("tell");
  g_told = answer;
}
void popup(const char* title_key, const std::string& text) {
  g_events.push_back("popup");
  g_popups.emplace_back(title_key, text);
}
void log_line(const char* fmt, ...) {
  char line[512];
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(line, sizeof line, fmt, args);
  va_end(args);
  g_logs.push_back(line);
}

flow::Calls<Answer> calls() {
  flow::Calls<Answer> c;
  c.clock = &clock_now;
  c.room = &room_now;
  c.local_actor = &local_actor;
  c.is_master = &is_master;
  c.in_lobby = &in_lobby;
  c.close_room = &close_room;
  c.send_marker = &send_marker;
  c.reopen_room = &reopen_room;
  c.release_held = &release_held;
  c.drop_held = &drop_held;
  c.start_answer = &start_answer;
  c.tell_start_waits = &tell_start_waits;
  c.popup = &popup;
  c.log = &log_line;
  return c;
}

// A first start in a private lobby: the room closes, the marker goes out through the server, and the send is held.
void the_first_start_closes_the_room_sends_the_marker_and_holds() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Held));
  CHECK((g_events == std::vector<std::string>{"close", "marker"}));
  CHECK(g_markerSent == closer.marker() && g_markerSent != 0);
  CHECK(!closer.idle() && !closer.press_goes_on());
  CHECK(g_logs.size() == 1 &&
        g_logs[0] == "suite: the room is closed for a contract's start; the start goes once the server confirms it");
  CHECK(g_popups.empty());
}

// The marker comes back and a fresh reading finds everyone verified: the held send goes, once, and nothing else does.
void a_confirmed_start_is_released_once() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Held));
  closer.heard(kLocal, kLocal, closer.marker());
  g_now = 1300;
  g_answerToken = 7;
  flow::tick_start(closer, calls());
  CHECK((g_events == std::vector<std::string>{"close", "marker", "ask", "release"}));
  CHECK(g_logs.size() == 2 &&
        g_logs[1] == "suite: the server confirmed the room closed and everyone is verified; the contract starts");
  CHECK(g_popups.empty());
  // Sent: the room stays closed and no further tick does anything until the game's own LoadScene ends the wait.
  g_now = 1400;
  flow::tick_start(closer, calls());
  CHECK(g_events.size() == 4);
  CHECK(g_logs.size() == 2);
}

// Closing fails, the marker send fails, or there is no local actor: nothing is held, the room is opened again, and
// the host is told in the game's popup that the start did not go ahead.
void a_start_that_cannot_close_or_send_fails_closed() {
  // Every way round: the same log line, and the not-closed notice in the game's popup, under the waiting title.
  const std::string closed = "suite: the room could not be closed for a contract's start; it does not go ahead";
  {  // the room cannot be closed: the marker is never raised
    reset();
    clos::Closer closer;
    g_closeOk = false;
    CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Refused));
    CHECK((g_events == std::vector<std::string>{"close", "reopen", "popup"}));
    CHECK(closer.idle() && closer.press_goes_on());
    CHECK(g_logs.size() == 1 && g_logs[0] == closed);
    CHECK(g_popups.size() == 1 && g_popups[0].first == "popup.contract-waiting" && g_popups[0].second == clos::not_closed_notice());
  }
  {  // the marker cannot be sent: the room closes and opens again
    reset();
    clos::Closer closer;
    g_markerOk = false;
    CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Refused));
    CHECK((g_events == std::vector<std::string>{"close", "marker", "reopen", "popup"}));
    CHECK(closer.idle() && closer.press_goes_on());
    CHECK(g_logs.size() == 1 && g_logs[0] == closed);
    CHECK(g_popups.size() == 1 && g_popups[0].first == "popup.contract-waiting" && g_popups[0].second == clos::not_closed_notice());
  }
  {  // no local actor: the room is not touched at all
    reset();
    clos::Closer closer;
    g_local = 0;
    CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Refused));
    CHECK((g_events == std::vector<std::string>{"reopen", "popup"}));
    CHECK(closer.idle() && closer.press_goes_on());
    CHECK(g_logs.size() == 1 && g_logs[0] == closed);
    CHECK(g_popups.size() == 1 && g_popups[0].first == "popup.contract-waiting" && g_popups[0].second == clos::not_closed_notice());
  }
}

// The held send could not go: the room is opened again, the host is told, and pressing again can try again.
void a_failed_release_reopens_with_the_not_closed_notice() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  flow::start_send(closer, calls());
  closer.heard(kLocal, kLocal, closer.marker());
  g_releaseOk = false;
  g_now = 1100;
  flow::tick_start(closer, calls());
  CHECK((g_events == std::vector<std::string>{"close", "marker", "ask", "release", "reopen", "popup"}));
  CHECK(closer.idle() && closer.press_goes_on());
  CHECK(g_logs.size() == 3 &&
        g_logs[1] == "suite: the server confirmed the room closed and everyone is verified; the contract starts" &&
        g_logs[2] == "suite: a contract's start could not be sent; the room is open again");
  CHECK(g_popups.size() == 1 && g_popups[0].first == "popup.contract-waiting" && g_popups[0].second == clos::not_closed_notice());
}

// The confirm deadline passes: the held send is dropped, the room opens again, and the host is told the server never
// confirmed. The room is not read while it is only closing.
void no_confirmation_drops_reopens_and_says_so() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  flow::start_send(closer, calls());
  g_now = 1000 + clos::kConfirmMs;
  flow::tick_start(closer, calls());
  CHECK((g_events == std::vector<std::string>{"close", "marker", "drop", "reopen", "popup"}));
  CHECK(g_answerAsks == 0);
  CHECK(closer.idle() && closer.press_goes_on());
  char expected[160];
  std::snprintf(expected, sizeof expected, "suite: the server did not confirm the room closed in %llu ms; the room is open again",
                static_cast<unsigned long long>(clos::kConfirmMs));
  CHECK(g_logs.size() == 2 && g_logs[1] == expected);
  CHECK(g_popups.size() == 1 && g_popups[0].first == "popup.contract-waiting" && g_popups[0].second == clos::not_confirmed_notice());
}

// A player the fresh reading finds unverified: the held send is dropped, the room opens again, and the host is told
// who the start waits for, from the very reading the closer asked for.
void an_unverified_player_reopens_and_names_who_it_waits_for() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  flow::start_send(closer, calls());
  closer.heard(kLocal, kLocal, closer.marker());
  g_answerGo = false;
  g_answerToken = 5;
  g_now = 1100;
  flow::tick_start(closer, calls());
  CHECK((g_events == std::vector<std::string>{"close", "marker", "ask", "drop", "reopen", "tell"}));
  CHECK(g_popups.empty());
  CHECK(!g_told.go && g_told.token == 5);
  CHECK(closer.idle() && closer.press_goes_on());
}

// Leaving the room, or the lobby's frame ending mid-close: the held send is dropped and nothing opens a room this
// client is no longer in.
void leaving_the_room_or_the_lobby_drops_without_reopening() {
  {  // this client left the room: there is nothing to open
    reset();
    clos::Closer closer;
    g_now = 1000;
    flow::start_send(closer, calls());
    closer.heard(kLocal, kLocal, closer.marker());
    g_room = 0;
    g_now = 1100;
    flow::tick_start(closer, calls());
    CHECK((g_events == std::vector<std::string>{"close", "marker", "drop"}));
    CHECK(closer.idle());
    CHECK(g_logs.size() == 1 && g_popups.empty());
  }
  {  // the lobby's frame ended: a contract is on its way, and opening the room would let a player into it
    reset();
    clos::Closer closer;
    g_now = 1000;
    flow::start_send(closer, calls());
    g_lobby = false;
    g_now = 1100;
    flow::tick_start(closer, calls());
    CHECK((g_events == std::vector<std::string>{"close", "marker", "drop"}));
    CHECK(closer.idle());
    CHECK(g_logs.size() == 1 && g_popups.empty());
  }
}

// A second press starts nothing twice: not while the room is closing (a send is refused and nothing is closed or
// sent again), and not after the release either, while the game's own LoadScene is still on its way (final review R2):
// the room stays closed for the contract rather than reopening during its load.
void a_second_press_starts_nothing() {
  reset();
  clos::Closer closer;
  g_now = 1000;
  CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Held));
  CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Refused));
  CHECK((g_events == std::vector<std::string>{"close", "marker"}));
  CHECK(g_markerSent == closer.marker());
  CHECK(g_popups.empty());
  closer.heard(kLocal, kLocal, closer.marker());
  g_now = 1100;
  flow::tick_start(closer, calls());
  CHECK((g_events == std::vector<std::string>{"close", "marker", "ask", "release"}));
  CHECK(flow::start_send(closer, calls()) == static_cast<int>(Send::Refused));
  CHECK(g_events.size() == 4);
  CHECK(g_popups.empty());
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  the_first_start_closes_the_room_sends_the_marker_and_holds();
  a_confirmed_start_is_released_once();
  a_start_that_cannot_close_or_send_fails_closed();
  a_failed_release_reopens_with_the_not_closed_notice();
  no_confirmation_drops_reopens_and_says_so();
  an_unverified_player_reopens_and_names_who_it_waits_for();
  leaving_the_room_or_the_lobby_drops_without_reopening();
  a_second_press_starts_nothing();
  return g_failures == 0 ? 0 : 1;
}
