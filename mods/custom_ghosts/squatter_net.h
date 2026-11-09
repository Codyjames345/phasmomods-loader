#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "flame_net.h"
#include "squatter_figure.h"
#include "squatter_rule.h"

// The Squatter's messages on Custom Ghosts' channel (flame_net.h kinds kSquatter, kSquatterTry, kSquatterSprint) and
// the rules each end keeps, free of game calls so they can be tested offline. Only the master runs the Squatter; the
// other clients show what it says. The sender of every message is Photon's actor number from the event itself
// (core/net.h), never a value in the message, so no message here names a player. Times are milliseconds on any
// steady clock. Main thread.
namespace pm::custom::squatter {

/** How many Phase values there are: a byte at or above it is no Phase (read_shown). */
inline constexpr int kPhaseValues = static_cast<int>(Phase::Following) + 1;
/** How many Move values there are: a byte at or above it is no Move (read_shown). */
inline constexpr int kMoveValues = static_cast<int>(Move::Sneak) + 1;
/** How many Lines sets there are: a set at or above it is none (read_shown). */
inline constexpr int kLinesValues = static_cast<int>(Lines::Steps) + 1;
/** How many Act values there are (squatter_figure.h): an act at or above it is none (read_shown). */
inline constexpr int kActValues = static_cast<int>(Act::OpenDoorInwards) + 1;

/**
 * What the master shows every client, so all of them see and hear the same Squatter (the spec: "All players see the
 * same move at the same time"). The master sends it to the others whenever it changes; each client plays a line or an
 * act once per new cue, so the same line twice is two cues.
 */
struct Shown {
  Phase phase = Phase::Lurking;
  Move move = Move::Run;
  uint8_t variant = 0;    // which of the move's clips plays (squatter_figure.h state_for)
  int8_t lineSet = -1;    // a Lines set, or -1 for no line
  int8_t lineIndex = -1;  // the line within the set (squatter_voice.h play_index), or -1 for none
  uint8_t lineCue = 0;    // counts up, wrapping, each time the master starts a line
  int8_t act = -1;        // an Act, or -1 for none
  uint8_t actCue = 0;     // counts up, wrapping, each time the master starts an act
  uint8_t lookAt = 0;     // the Photon actor number of the player it is after, whom its head turns to; 0 for none
  bool operator==(const Shown&) const = default;
};

/** The kSquatter message for `s`: the header, then each field of Shown in order, a byte each. */
inline std::vector<uint8_t> shown_message(const Shown& s) {
  std::vector<uint8_t> out = net_msg::header(net_msg::kSquatter);
  out.insert(out.end(), {static_cast<uint8_t>(s.phase), static_cast<uint8_t>(s.move), s.variant,
                         static_cast<uint8_t>(s.lineSet), static_cast<uint8_t>(s.lineIndex), s.lineCue,
                         static_cast<uint8_t>(s.act), s.actCue, s.lookAt});
  return out;
}

/**
 * Reads a kSquatter message into `out`. False, leaving `out` alone, for any other kind, a message of another length,
 * or a value no client knows: a phase, move, line set or act out of range, a line below -1, or a line with no set.
 */
inline bool read_shown(const uint8_t* data, size_t size, Shown& out) {
  if (net_msg::kind_of(data, size) != net_msg::kSquatter || size != 13) return false;
  const auto lineSet = static_cast<int8_t>(data[7]), lineIndex = static_cast<int8_t>(data[8]);
  const auto act = static_cast<int8_t>(data[10]);
  if (data[4] >= kPhaseValues || data[5] >= kMoveValues || lineSet < -1 || lineSet >= kLinesValues || lineIndex < -1 ||
      (lineSet < 0 && lineIndex >= 0) || act < -1 || act >= kActValues)
    return false;
  out = Shown{static_cast<Phase>(data[4]), static_cast<Move>(data[5]), data[6], lineSet, lineIndex, data[9], act,
              data[11], data[12]};
  return true;
}

/** The last Shown this client has taken from the master. */
struct ShownInbox {
  std::optional<Shown> last;
  /**
   * Takes a kSquatter message from `sender` when it is the master (`master`, Photon's master actor; 0 while it is
   * unknown takes nothing) and the message reads whole. True when taken; otherwise the last one stands.
   */
  bool take(int sender, int master, const uint8_t* data, size_t size) {
    Shown s;
    if (master == 0 || sender != master || !read_shown(data, size, s)) return false;
    last = s;
    return true;
  }
};

/**
 * The phase this client keeps: on the master its own (`kept`, which the master's tick moves), on any other client the
 * last Shown's once one has come, and `kept` until then. A client that becomes the master, after a host migration,
 * keeps the phase it last followed, so the chase goes on from the last Shown: Chasing stays Chasing, and LockedOut
 * stays LockedOut, which never locks again (squatter_rule.h doors_lock).
 */
inline std::optional<Phase> kept_phase(std::optional<Phase> kept, const std::optional<Shown>& last, bool master) {
  if (!master && last) return last->phase;
  return kept;
}

// Starting values, to be tuned with the user in game.

// Milliseconds within which a second report of a try at a locked exit, from one player, is the same input: the game's
// interact path and grab path can both report one press (squatter_door.h on_attempt).
inline constexpr uint64_t kTryWindowMs = 300;
// Milliseconds after which a client tells the master its sprint again though nothing changed.
inline constexpr uint64_t kSprintResendMs = 1000;
// Milliseconds after which the master drops a player's sprint report: three resends missed.
inline constexpr uint64_t kSprintStaleMs = 3 * kSprintResendMs;
// The fastest sprint a report may carry, in metres a second; a faster one is no reading. A bound on the message, not
// a game value.
inline constexpr float kMaxReportedSpeed = 20.0f;

/** The kSquatterTry message: the header alone. Who tried is its sender. */
inline std::vector<uint8_t> try_message() { return net_msg::header(net_msg::kSquatterTry); }

/** Whether this client's try at a locked exit goes to the master now: one per input, the first in each kTryWindowMs. */
class TryTeller {
 public:
  /**
   * Whether a try reported at `now` (milliseconds) goes to the master: true for the first, and for each one at least
   * kTryWindowMs after the last that went; true also records it as the last that went.
   */
  bool due(uint64_t now) {
    if (sent_ && now - last_ < kTryWindowMs) return false;
    sent_ = true;
    last_ = now;
    return true;
  }

 private:
  bool sent_ = false;
  uint64_t last_ = 0;
};

/**
 * Whether a message from `sender` is a try the master takes: this client is the master, the message is a kSquatterTry
 * with no body, and the sender is one of the room's players (`room`, their actor numbers), so 0 (unknown) is not.
 */
inline bool try_from(int sender, bool master, const std::vector<int>& room, const uint8_t* data, size_t size) {
  return master && sender != 0 && net_msg::kind_of(data, size) == net_msg::kSquatterTry && size == 4 &&
         std::find(room.begin(), room.end(), sender) != room.end();
}

/** The master's queue of tries, by actor number, a double report from one player within kTryWindowMs taken once. */
class TryInbox {
 public:
  /**
   * Queues a try by `actor` (0 for the master's own player when it has no actor number, in single player). False when
   * the same player tried within kTryWindowMs.
   */
  bool report(int actor, uint64_t now) {
    const auto it = last_.find(actor);
    if (it != last_.end() && now - it->second < kTryWindowMs) return false;
    last_[actor] = now;
    queue_.push_back(actor);
    return true;
  }
  /** The tries queued since the last take, oldest first, emptying the queue. */
  std::vector<int> take() {
    std::vector<int> out;
    out.swap(queue_);
    return out;
  }
  /** Forgets every queued try and every player's last one, as when the Squatter stops haunting. */
  void clear() { *this = TryInbox{}; }

 private:
  std::map<int, uint64_t> last_;
  std::vector<int> queue_;
};

/** One player's sprint, as their own client reads it. */
struct Sprint {
  bool canSprint = false;  // can_sprint below
  float speed = 0.0f;      // sprint_speed_if_on below, metres a second
  bool operator==(const Sprint&) const = default;
};

/**
 * The speed, in metres a second, a player's rig would sprint at in a contract with sprinting on, as the game works it
 * out on that player's client: LocalPlayer.sprintSpeed times LocalPlayer.speedMultiplier, but for a dead rig
 * (LocalPlayer.isDead), whose speed skips the multiplier. That is LocalPCPlayer.UpdatePlayerSpeed on PC; on VR,
 * VRStamina.Update does the same on the branch a flag of VRStamina picks, and its other branch was not traced
 * (sdk/systems/players.md, "Speed and stamina").
 */
inline float sprint_speed_if_on(float sprintSpeed, float multiplier, bool rigDead) {
  return rigDead ? sprintSpeed : sprintSpeed * multiplier;
}

/**
 * Whether a player may sprint now in a contract, the sprint input aside, as LocalPCPlayer.UpdatePlayerSpeed decides:
 * sprinting is allowed (`sprinting`, Difficulty.sprinting, is not 0) and the stamina can sprint
 * (PlayerStamina.canSprint, which the out-of-breath step clears on PC and VR alike).
 */
inline bool can_sprint(bool staminaCanSprint, int32_t sprinting) { return staminaCanSprint && sprinting != 0; }

/** The kSquatterSprint message: the header, canSprint as a byte, then the speed. */
inline std::vector<uint8_t> sprint_message(const Sprint& s) {
  std::vector<uint8_t> out = net_msg::header(net_msg::kSquatterSprint);
  out.push_back(s.canSprint ? 1 : 0);
  net_msg::put(out, s.speed);
  return out;
}

/** Reads a kSquatterSprint message into `out`; false, leaving it alone, for any other kind or length, a flag not 0 or
 *  1, or a speed that is not a finite number from 0 to kMaxReportedSpeed. */
inline bool read_sprint(const uint8_t* data, size_t size, Sprint& out) {
  if (net_msg::kind_of(data, size) != net_msg::kSquatterSprint || size != 9 || data[4] > 1) return false;
  size_t at = 5;
  float speed = 0;
  if (!net_msg::take(data, size, at, speed) || !std::isfinite(speed) || speed < 0.0f || speed > kMaxReportedSpeed)
    return false;
  out = Sprint{data[4] == 1, speed};
  return true;
}

/**
 * Reads a sprint report from `sender` into `out` when the master takes it: this client is the master, the message
 * reads whole (read_sprint), and the sender is one of the room's players (`room`), so 0 (unknown) is not. False,
 * leaving `out` alone, otherwise.
 */
inline bool sprint_from(int sender, bool master, const std::vector<int>& room, const uint8_t* data, size_t size,
                        Sprint& out) {
  return master && sender != 0 && std::find(room.begin(), room.end(), sender) != room.end() &&
         read_sprint(data, size, out);
}

/** Whether this client tells the master its sprint now: on any change, and every kSprintResendMs while none comes. */
class SprintTeller {
 public:
  /**
   * Whether this client sends `now`, its player's sprint read at `ms` (milliseconds), to the master: true for the
   * first, for one that differs from the last sent, and for an unchanged one kSprintResendMs or more after the last
   * send; true also records it as the last sent.
   */
  bool due(const Sprint& now, uint64_t ms) {
    if (sent_ && *sent_ == now && ms - at_ < kSprintResendMs) return false;
    sent_ = now;
    at_ = ms;
    return true;
  }

 private:
  std::optional<Sprint> sent_;
  uint64_t at_ = 0;
};

/** The master's last word from each player on their sprint, by actor number, each good for kSprintStaleMs. */
class SprintBook {
 public:
  /** Records `s` for `actor` (0 for the master's own player when it has no actor number) at `now`. */
  void report(int actor, const Sprint& s, uint64_t now) { book_[actor] = Entry{s, now}; }
  /** `actor`'s last report, or none when they have not reported within kSprintStaleMs of `now`. */
  std::optional<Sprint> of(int actor, uint64_t now) const {
    const auto it = book_.find(actor);
    if (it == book_.end() || now - it->second.at > kSprintStaleMs) return std::nullopt;
    return it->second.sprint;
  }
  /** Drops every report gone stale by `now`. */
  void drop_stale(uint64_t now) {
    std::erase_if(book_, [now](const auto& e) { return now - e.second.at > kSprintStaleMs; });
  }
  /** How many players' reports the book holds, stale ones not yet dropped included. */
  size_t size() const { return book_.size(); }
  /** Forgets every report, as when the Squatter stops haunting. */
  void clear() { book_.clear(); }

 private:
  struct Entry {
    Sprint sprint;
    uint64_t at = 0;
  };
  std::map<int, Entry> book_;
};

}  // namespace pm::custom::squatter
