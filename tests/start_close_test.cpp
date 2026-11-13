// A contract's start closes the room first (host/start_close.h): the plan's Task 13d tests for the user's ruling of 2026-11-03.
#include "start_close.h"

#include <fstream>
#include <sstream>
#include <string>

#include "check.h"
#include "text_fixture.h"

namespace {

using pm::startclose::Closer;
using pm::startclose::Send;
using pm::startclose::Step;
using pm::startclose::Why;

constexpr uint64_t kRoom = 77;
constexpr int32_t kLocal = 1;

bool yes() { return true; }
bool no() { return false; }

// The send is held until the server confirms the close and everyone is still verified; only then does it go, once, as sent.
void the_room_closes_and_is_confirmed_before_the_start_goes() {
  Closer c;
  CHECK(c.on_send(1000, kRoom) == Send::Hold);
  const uint32_t marker = c.marker();
  CHECK(marker != 0);
  // Not confirmed yet: nothing goes, whatever the room reads.
  CHECK(c.tick(1100, kRoom, true, true, yes) == Step::None);
  // Another client's marker, or a stale one, confirms nothing.
  c.heard(2, kLocal, marker);
  c.heard(kLocal, kLocal, marker + 1);
  c.heard(0, 0, marker);
  CHECK(c.tick(1200, kRoom, true, true, yes) == Step::None);
  // This client's own marker comes back: the start goes, as the game sent it.
  c.heard(kLocal, kLocal, marker);
  CHECK(c.tick(1300, kRoom, true, true, yes) == Step::Release);
  CHECK(c.on_send(1300, kRoom) == Send::AsSent);  // the held send passes its own hook
  c.released(1300);
  // Sent: the game's LoadScene ends it by leaving the lobby, and the room stays closed meanwhile.
  CHECK(c.tick(1400, kRoom, true, false, yes) == Step::None);
  CHECK(c.idle() && c.press_goes_on());
  // The next start closes afresh, with a new marker.
  CHECK(c.on_send(2000, kRoom) == Send::Hold && c.marker() != marker);
}

// A player who got in before the close shows up on the fresh reading: the room opens again and the host is told who it waits for.
void a_player_who_got_in_first_reopens_the_room_and_waits() {
  Closer c;
  CHECK(c.on_send(1000, kRoom) == Send::Hold);
  c.heard(kLocal, kLocal, c.marker());
  CHECK(c.tick(1100, kRoom, true, true, no) == Step::Reopen);
  CHECK(c.why() == Why::Waiting);
  CHECK(c.idle() && c.press_goes_on());
  CHECK(c.on_send(1200, kRoom) == Send::Hold);  // pressing again later closes again
}

// No confirmation within kConfirmMs: the room opens again and the host is told, so the wait always ends.
void no_confirmation_reopens_the_room_and_says_so() {
  Closer c;
  CHECK(c.on_send(1000, kRoom) == Send::Hold);
  CHECK(c.tick(1000 + pm::startclose::kConfirmMs - 1, kRoom, true, true, yes) == Step::None);
  CHECK(c.tick(1000 + pm::startclose::kConfirmMs, kRoom, true, true, yes) == Step::Reopen);
  CHECK(c.why() == Why::NotConfirmed);
  CHECK(c.idle());
  // A marker arriving after that confirms nothing.
  c.heard(kLocal, kLocal, 1);
  CHECK(c.tick(9000, kRoom, true, true, yes) == Step::None);
  // The loader's table has both messages: a missing key would show as "[key]".
  CHECK(pm::startclose::not_confirmed_notice() != "[start.not-confirmed]" && !pm::startclose::not_confirmed_notice().empty());
  CHECK(pm::startclose::not_closed_notice() != "[start.not-closed]" && !pm::startclose::not_closed_notice().empty());
}

// Every way out of a closed room but the start itself opens it again, or leaves a room this client is no longer in. A master who
// left in that moment leaves it closed for the next, who can still start or leave; the next contract's return opens it.
void a_room_closed_for_a_start_opens_again_unless_its_master_left() {
  {  // this client stops being the master: the room it closed opens again
    Closer c;
    c.on_send(1000, kRoom);
    CHECK(c.tick(1100, kRoom, false, true, yes) == Step::Reopen);
    CHECK(c.idle());
  }
  {  // confirmed, then the master changes before the start goes
    Closer c;
    c.on_send(1000, kRoom);
    c.heard(kLocal, kLocal, c.marker());
    CHECK(c.tick(1100, kRoom, false, true, yes) == Step::Reopen);
  }
  {  // this client left the room: nothing to open, and the held send is dropped
    Closer c;
    c.on_send(1000, kRoom);
    CHECK(c.tick(1100, 0, false, true, yes) == Step::Drop);
    CHECK(c.idle());
  }
  {  // the mods stop with the room closed: the caller opens it
    Closer c;
    c.on_send(1000, kRoom);
    CHECK(c.abandon());
    CHECK(c.idle() && !c.abandon());
  }
  {  // nothing closed, nothing to open
    Closer c;
    CHECK(!c.abandon());
    CHECK(c.tick(1000, kRoom, true, true, yes) == Step::None);
  }
  // The verified check is read only once confirmed: a reading that cannot be made never keeps the room closed.
  Closer c;
  c.on_send(1000, kRoom);
  int reads = 0;
  CHECK(c.tick(1100, kRoom, true, true, [&] { ++reads; return true; }) == Step::None);
  CHECK(reads == 0);
}

// While a start is being closed for, a press and another send start nothing: one start, one held send.
void pressing_again_while_closing_starts_nothing_twice() {
  Closer c;
  CHECK(c.press_goes_on());
  CHECK(c.on_send(1000, kRoom) == Send::Hold);
  CHECK(!c.press_goes_on());
  CHECK(c.on_send(1050, kRoom) == Send::Refused);
  c.heard(kLocal, kLocal, c.marker());
  CHECK(!c.press_goes_on());
  CHECK(c.on_send(1100, kRoom) == Send::Refused);
  CHECK(c.tick(1200, kRoom, true, true, yes) == Step::Release);
  CHECK(!c.press_goes_on());
  c.released(1200);
  CHECK(!c.press_goes_on());
}

// A double-click: the second press comes after the release and before the server's echo. It must start nothing, or a second close
// would reopen the room during the contract's load, or send a second start.
void a_second_press_after_the_release_starts_nothing() {
  Closer c;
  CHECK(c.on_send(1000, kRoom) == Send::Hold);
  c.heard(kLocal, kLocal, c.marker());
  CHECK(c.tick(1100, kRoom, true, true, yes) == Step::Release);
  c.released(1100);
  CHECK(!c.idle() && !c.press_goes_on());
  CHECK(c.on_send(1150, kRoom) == Send::Refused);
  CHECK(c.tick(1200, kRoom, true, true, yes) == Step::None);
  // The contract loads: nothing opens the room, now or at any later time.
  CHECK(c.tick(1300, kRoom, true, false, yes) == Step::None);
  CHECK(c.tick(1300 + pm::startclose::kConfirmMs * 4, kRoom, true, false, yes) == Step::None);
  CHECK(c.idle());
}

// A start being closed for when the lobby's frame ends never opens the room and never sends: a contract is on its way.
void outside_the_lobby_a_closing_start_is_dropped() {
  {
    Closer c;
    c.on_send(1000, kRoom);
    CHECK(c.tick(1100, kRoom, true, false, yes) == Step::Drop);
    CHECK(c.idle());
  }
  {
    Closer c;
    c.on_send(1000, kRoom);
    c.heard(kLocal, kLocal, c.marker());
    CHECK(c.tick(1100, kRoom, true, false, yes) == Step::Drop);
  }
  {  // past the deadline too
    Closer c;
    c.on_send(1000, kRoom);
    CHECK(c.tick(1000 + pm::startclose::kConfirmMs, kRoom, true, false, yes) == Step::Drop);
  }
}

// The host is never left with a closed room and a dead button: a send that came to nothing opens it again, and one that could not
// go at all lets them press again.
void a_start_that_comes_to_nothing_opens_the_room() {
  {
    Closer c;
    c.on_send(1000, kRoom);
    c.heard(kLocal, kLocal, c.marker());
    CHECK(c.tick(1100, kRoom, true, true, yes) == Step::Release);
    c.released(1100);
    CHECK(c.tick(1100 + pm::startclose::kConfirmMs - 1, kRoom, true, true, yes) == Step::None);
    CHECK(c.tick(1100 + pm::startclose::kConfirmMs, kRoom, true, true, yes) == Step::Reopen);
    CHECK(c.why() == Why::NotConfirmed && c.idle() && c.press_goes_on());
  }
  {
    Closer c;
    c.on_send(1000, kRoom);
    c.heard(kLocal, kLocal, c.marker());
    CHECK(c.tick(1100, kRoom, true, true, yes) == Step::Release);
    c.release_failed();
    CHECK(c.idle() && c.press_goes_on());
  }
}

// host/suite.cpp has no test of its own, so these pin what it must hand over: the lobby's answer and the master to the
// start flow (host/start_flow.h, tested in tests/start_flow_test.cpp), and the room's answer to the gate.
void the_suite_hands_over_the_lobby_and_the_room() {
  std::ifstream in(std::string(PM_TEXT_DIR) + "/host/suite.cpp", std::ios::binary);
  std::ostringstream read;
  read << in.rdbuf();
  const std::string suite = read.str();
  CHECK(suite.find("calls.is_master = &hostgame::local_is_master") != std::string::npos);
  CHECK(suite.find("calls.in_lobby = &frame_in_lobby") != std::string::npos);
  CHECK(suite.find("gate.privateRoom =") != std::string::npos);
  const size_t asked = suite.find("consent::missing_notice_text(i18n::table())"), shown = suite.find("consent::begin(");
  CHECK(asked != std::string::npos && shown != std::string::npos && asked < shown);
}

void the_marker_reads_back_and_nothing_else_does() {
  uint8_t bytes[pm::startclose::kMarkerBytes];
  pm::startclose::write_marker(0xA1B2C3D4u, bytes);
  CHECK(pm::startclose::read_marker(bytes, sizeof bytes) == 0xA1B2C3D4u);
  CHECK(pm::startclose::read_marker(bytes, sizeof bytes - 1) == 0);
  bytes[1] = 'M';  // a presence announcement's tag
  CHECK(pm::startclose::read_marker(bytes, sizeof bytes) == 0);
  CHECK(pm::startclose::read_marker(nullptr, 7) == 0);
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  the_room_closes_and_is_confirmed_before_the_start_goes();
  a_player_who_got_in_first_reopens_the_room_and_waits();
  no_confirmation_reopens_the_room_and_says_so();
  a_room_closed_for_a_start_opens_again_unless_its_master_left();
  pressing_again_while_closing_starts_nothing_twice();
  a_second_press_after_the_release_starts_nothing();
  outside_the_lobby_a_closing_start_is_dropped();
  a_start_that_comes_to_nothing_opens_the_room();
  the_suite_hands_over_the_lobby_and_the_room();
  the_marker_reads_back_and_nothing_else_does();
  return g_failures == 0 ? 0 : 1;
}
