#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "lobby_gate.h"
#include "start_close.h"

// What becomes of a contract's start around the closer (host/start_close.h). host/suite.cpp fills a Calls with this
// client's room, marker, held send, popup and log and calls these two; the offline tests fill the same struct with
// fakes and check that every situation calls exactly what the host must, in the same order, with the same log lines
// and popups (tests/start_flow_test.cpp). These decisions lived in suite.cpp itself until the final review (M12):
// it is not built offline, which is how two bugs got through here. Nothing here calls the game. Main thread.
namespace pm::startflow {

// Everything the two functions touch outside the closer. `clock` is the clock the closer's timings are measured on;
// `room` names the room this client is in, as the closer tells rooms apart. `start_answer` reads the room afresh,
// fills `answer` and returns whether everyone in it is verified; `tell_start_waits` tells the host, from that same
// answer, who the start waits for. `popup` shows the game's popup titled by the text-table key; `log` writes a line
// exactly as pm::log::write does.
template <typename Answer>
struct Calls {
  uint64_t (*clock)() = nullptr;
  uint64_t (*room)() = nullptr;
  int (*local_actor)() = nullptr;   // 0 when this client is not in a room
  bool (*is_master)() = nullptr;    // whether this client is the room's master
  bool (*in_lobby)() = nullptr;     // whether this frame is the lobby's
  bool (*close_room)() = nullptr;   // close the room for the start; false when it cannot be
  bool (*send_marker)(const uint8_t* marker, size_t size) = nullptr;  // raise the closer's marker through the server
  void (*reopen_room)() = nullptr;  // open the room again, and say so when it cannot
  bool (*release_held)() = nullptr;  // let the held send go as the game sent it; false when it could not
  void (*drop_held)() = nullptr;     // forget the held send: nothing goes out
  bool (*start_answer)(Answer& answer) = nullptr;
  void (*tell_start_waits)(const Answer& answer) = nullptr;
  void (*popup)(const char* title_key, const std::string& text) = nullptr;
  void (*log)(const char* fmt, ...) = nullptr;
};

// What becomes of an allowed start's send (lobby_gate::Config::startSend), answering a lobby_gate::Send value. The
// first is held while the room closes, and this client's marker comes back to it through the server, behind the
// close. If the room cannot be closed or the marker cannot be sent, nothing is held: the room opens again and the
// host is told to press again. A second press while one is being closed for, or after its release, is refused: one
// start, one held send (the final review's R2).
template <typename Answer>
int start_send(startclose::Closer& closer, const Calls<Answer>& calls) {
  const startclose::Send send = closer.on_send(calls.clock(), calls.room());
  if (send == startclose::Send::AsSent) return static_cast<int>(lobby_gate::Send::AsSent);
  if (send == startclose::Send::Refused) return static_cast<int>(lobby_gate::Send::Refused);
  uint8_t marker[startclose::kMarkerBytes];
  startclose::write_marker(closer.marker(), marker);
  const int local = calls.local_actor();
  if (local != 0 && calls.close_room() && calls.send_marker(marker, sizeof marker)) {
    calls.log("suite: the room is closed for a contract's start; the start goes once the server confirms it");
    return static_cast<int>(lobby_gate::Send::Held);
  }
  calls.log("suite: the room could not be closed for a contract's start; it does not go ahead");
  closer.abandon();
  calls.reopen_room();
  calls.popup("popup.contract-waiting", startclose::not_closed_notice());
  return static_cast<int>(lobby_gate::Send::Refused);
}

// Every frame: a start whose room is closed goes once the server confirms the close and everyone is still verified;
// otherwise the room opens again and the host hears why.
template <typename Answer>
void tick_start(startclose::Closer& closer, const Calls<Answer>& calls) {
  if (closer.idle()) return;
  Answer answer;
  const startclose::Step step = closer.tick(calls.clock(), calls.room(), calls.is_master(), calls.in_lobby(), [&] {
    return calls.start_answer(answer);
  });
  switch (step) {
    case startclose::Step::None: return;
    case startclose::Step::Release:
      calls.log("suite: the server confirmed the room closed and everyone is verified; the contract starts");
      if (calls.release_held()) {
        closer.released(calls.clock());
      } else {
        calls.log("suite: a contract's start could not be sent; the room is open again");
        closer.release_failed();
        calls.reopen_room();
        calls.popup("popup.contract-waiting", startclose::not_closed_notice());
      }
      return;
    case startclose::Step::Drop:
      calls.drop_held();
      return;
    case startclose::Step::Reopen:
      calls.drop_held();
      calls.reopen_room();
      if (closer.why() == startclose::Why::Waiting) calls.tell_start_waits(answer);
      if (closer.why() == startclose::Why::NotConfirmed) {
        calls.log("suite: the server did not confirm the room closed in %llu ms; the room is open again",
                  static_cast<unsigned long long>(startclose::kConfirmMs));
        calls.popup("popup.contract-waiting", startclose::not_confirmed_notice());
      }
      return;
  }
}

}  // namespace pm::startflow
