#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "text_table.h"

// A contract's start closes the room before it goes (user, 2026-11-03). The room closes only on the server's echo of
// ServerManager.LoadScene, so a join could land in the round trip after the master's send, and no kick reaches a player who
// got in there (sdk/systems/photon.md, "Nobody joins a running contract"). So the master closes the room first, holds the
// game's send, and lets it go only once the server has confirmed the close and a fresh reading of the room finds everyone
// verified. Otherwise the room opens again and the host is told why, through the game's own popup. Nobody gets in late, so no
// kick is needed and a contract is always fully modded: every guarantee a mod relies on about who is in a contract (a voice
// kept only of players who agreed to the notice, a custom value only to players who have the content) rests on this, and no
// mod checks it again.
//
// How the close is confirmed: right after PhotonRoom.set_IsOpen(false) the master raises a marker on the host channel to
// ReceiverGroup.All, reliably, the way the game's own send of ServerManager.LoadScene reaches everyone and comes back to the
// master. Other clients ignore it. That it comes back is what the game itself relies on. That the server handles one client's
// operations in order, so the marker comes back after the close was applied and any join the server took before the close
// reaches the master first, rests on Photon's ordering, which the game's code cannot prove: it is an assumption the smoke list
// checks. If the marker never came back, every start would reopen after kConfirmMs.
//
// Pure logic: host/suite.cpp makes the calls (the room, the marker, the held send) and asks this what to do. Main thread.
namespace pm::startclose {

// How long the master waits for its marker before it gives up, opens the room again and tells the host. A round trip takes
// well under a second; this only bounds a lost or refused marker, so the wait always ends.
constexpr uint64_t kConfirmMs = 5000;

// What the host does with the game's send of ServerManager.LoadScene.
enum class Send {
  Hold,     // nothing goes out now: the room is being closed (the caller keeps the send and closes the room)
  Refused,  // nothing goes out, and nothing is kept: a start is already being closed for
  AsSent,   // as the game sent it: this is the held send, let go
};

// What the host does this frame.
enum class Step {
  None,
  Release,     // send the held LoadScene as the game asked, then call released() or release_failed()
  Reopen,      // open the room again, drop the held send, and tell the host (why() says what)
  Drop,        // drop the held send: this client is no longer in that room, so there is nothing to open
};

enum class Why {
  None,
  Waiting,         // someone in the room is not verified: the start's popup names them
  NotConfirmed,    // the server never confirmed the close in kConfirmMs
};

class Closer {
 public:
  /// The game is sending LoadScene, and the start's check found everyone verified. `room` names the room it was pressed in
  /// (any value that changes when this client leaves one room and joins another, 0 for none).
  Send on_send(uint64_t now, uint64_t room) {
    switch (phase_) {
      case Phase::Idle:
        phase_ = Phase::Closing;
        marker_ = ++serial_;
        if (marker_ == 0) marker_ = ++serial_;  // 0 is never a marker
        deadline_ = now + kConfirmMs;
        room_ = room;
        return Send::Hold;
      case Phase::Closing:
      case Phase::Confirmed:
      case Phase::Sent: return Send::Refused;
      case Phase::Releasing: return Send::AsSent;
    }
    return Send::Refused;
  }

  /// Whether a press of the start goes on: not while a start is being closed for, so pressing again starts nothing twice.
  bool press_goes_on() const { return phase_ == Phase::Idle; }

  /// The marker to raise through the server, right after the room is closed; 0 when none is awaited.
  uint32_t marker() const { return phase_ == Phase::Closing ? marker_ : 0; }

  /// A marker came back from the server. One from this client, with the awaited value, confirms the close; anything else is
  /// ignored (another client's, an old one's, a forged one's).
  void heard(int32_t sender, int32_t localActor, uint32_t marker) {
    if (phase_ == Phase::Closing && sender != 0 && sender == localActor && marker == marker_) phase_ = Phase::Confirmed;
  }

  /// Every frame. `room` is the room this client is in now (as on_send took it), `master` whether it is the room's master, `lobby`
  /// whether this frame is the lobby's, and `verified` whether a fresh reading of the room finds everyone verified (read only when
  /// it matters).
  template <typename Verified>
  Step tick(uint64_t now, uint64_t room, bool master, bool lobby, Verified verified) {
    why_ = Why::None;
    if (phase_ == Phase::Idle || phase_ == Phase::Releasing) return Step::None;
    if (phase_ == Phase::Sent) {
      // The game's own LoadScene ends this, by leaving the lobby. Until then a second press would start a second close.
      if (room != room_ || !lobby) return finish(Step::None);
      if (now < deadline_) return Step::None;
      why_ = Why::NotConfirmed;  // sent, and nothing came of it: the host must not be left with a closed room and a dead button
      return finish(Step::Reopen);
    }
    if (room != room_) return finish(Step::Drop);    // left the room: there is nothing to open, nor anyone to start
    if (!lobby) return finish(Step::Drop);           // a contract is on its way: opening the room now would let a player into it
    if (!master) return finish(Step::Reopen);        // a new master takes over: the room it inherits is open, as before the press
    if (phase_ == Phase::Closing) {
      if (now < deadline_) return Step::None;
      why_ = Why::NotConfirmed;
      return finish(Step::Reopen);
    }
    if (verified()) {
      phase_ = Phase::Releasing;
      return Step::Release;
    }
    why_ = Why::Waiting;
    return finish(Step::Reopen);
  }

  /// The held send went out. The room stays closed, and no press or send starts anything until the game's LoadScene takes this
  /// client out of the lobby, one round trip away; `now` bounds that wait as on_send's does.
  void released(uint64_t now) {
    if (phase_ != Phase::Releasing) return;
    phase_ = Phase::Sent;
    deadline_ = now + kConfirmMs;
  }

  /// The held send could not go: the game's start is then as a refused one, the caller opens the room, and the host may press again.
  void release_failed() {
    if (phase_ == Phase::Releasing) phase_ = Phase::Idle;
  }

  /// The mods stop, or the host goes: true when the room was closed for a start and must be opened again now.
  bool abandon() {
    const bool closed = phase_ == Phase::Closing || phase_ == Phase::Confirmed;
    phase_ = Phase::Idle;
    return closed;
  }

  /// Why the last tick reopened the room.
  Why why() const { return why_; }

  bool idle() const { return phase_ == Phase::Idle; }

 private:
  enum class Phase { Idle, Closing, Confirmed, Releasing, Sent };

  Step finish(Step step) {
    phase_ = Phase::Idle;
    return step;
  }

  Phase phase_ = Phase::Idle;
  uint32_t serial_ = 0, marker_ = 0;
  uint64_t deadline_ = 0, room_ = 0;
  Why why_ = Why::None;
};

/// The marker's wire format, after the host channel's byte: 'H' 'S' version, then the marker, little-endian.
constexpr uint8_t kVersion = 1;
constexpr size_t kMarkerBytes = 7;

inline void write_marker(uint32_t marker, uint8_t out[kMarkerBytes]) {
  out[0] = 'H';
  out[1] = 'S';
  out[2] = kVersion;
  for (int i = 0; i < 4; ++i) out[3 + i] = static_cast<uint8_t>(marker >> (8 * i));
}

/// The marker a host-channel message carries, or 0 when it is no marker.
inline uint32_t read_marker(const uint8_t* data, size_t size) {
  if (!data || size != kMarkerBytes || data[0] != 'H' || data[1] != 'S' || data[2] != kVersion) return 0;
  uint32_t marker = 0;
  for (int i = 0; i < 4; ++i) marker |= static_cast<uint32_t>(data[3 + i]) << (8 * i);
  return marker;
}

// The two messages below are the user's to write (AI_POLICY.md): each is a PM_TEXT_TODO placeholder in the loader's table.

/// What the host reads when the server never confirmed the close: the contract did not start, the lobby is open again, and
/// pressing Start again tries again.
inline std::string not_confirmed_notice() {
  return i18n::text("start.not-confirmed");  // PM_TEXT_TODO in the loader's table: the user writes this message
}

/// What the host reads when the lobby could not be closed for the start at all: the contract did not start, and pressing
/// Start again tries again.
inline std::string not_closed_notice() {
  return i18n::text("start.not-closed");  // PM_TEXT_TODO in the loader's table: the user writes this message
}

}  // namespace pm::startclose
