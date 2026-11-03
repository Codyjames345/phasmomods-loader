#pragma once

#include <algorithm>
#include <cstdint>
#include <span>
#include <vector>

// The Draugr's rules that make no game call, so they can be tested offline (behaviour.cpp applies them).
// Its things are the house's own props: whatever can be carried and lay in its room when its rules
// started. Equipment is never one of them, since players bring it into the contract.
//  - While anything of its things is out of its room it is angered.
//  - Outside hunts it takes them back, one at a time, to the very place each lay, never one a player
//    is carrying.
//  - A thing that leaves its room carried by a player makes that player its thief; while it is angered its
//    hunts go only for the most recent living thief (target), until they are in the truck, dead or killed.
//  - While it is angered its room is kCold degrees colder than the game's own temperature system makes
//    it, a tell that may read as freezing whatever the evidence (RoomCold).
namespace pm::custom::draugr {

constexpr float kCold = 3.0f;  // degrees Celsius its room loses while it is angered

struct Point {
  float x = 0, y = 0, z = 0;
};

/**
 * One of its things, as it keeps track of it. `Player` is how it keeps a player: a pointer here, a
 * pm::api::Held in the game, which keeps the player across frames.
 */
template <class Player = void*>
struct Thing {
  Point home;             // where it lay when its rules started
  bool out = false;       // out of its room at the last survey
  Player thief{};         // the player carrying it as it left the room; null for none
  uint64_t takenAt = 0;   // when it left the room
  Player lastCarrier{};   // the player last seen carrying it while it lay in the room; null for none
};

/**
 * Every frame, who carries each of its things now (`carriers` in the same order, null for nobody): a
 * carrier is remembered, so a thing carried out and set down between two surveys still names its thief.
 */
template <class Player>
void note_carriers(std::vector<Thing<Player>>& things, const std::vector<void*>& carriers) {
  for (size_t i = 0; i < things.size() && i < carriers.size(); ++i)
    if (carriers[i]) things[i].lastCarrier = carriers[i];
}

/** What a survey sees of a thing now. */
struct Seen {
  bool alive = true;       // still in the world
  bool out = false;        // out of its room
  void* holder = nullptr;  // the player carrying it now (in their inventory); null for none
};

/**
 * One survey of its things, `seen` in the same order. A thing that has just left its room takes whoever
 * carries it now as its thief, or else whoever carried it last while it lay in the room (note_carriers),
 * none when nobody did (thrown, pushed); one back in the room, or gone from the world, has none, and one
 * lying in the room uncarried forgets its last carrier. Returns whether it is angered: something of its
 * still in the world is out.
 */
template <class Player>
bool survey(std::vector<Thing<Player>>& things, const std::vector<Seen>& seen, uint64_t now) {
  bool angered = false;
  for (size_t i = 0; i < things.size() && i < seen.size(); ++i) {
    Thing<Player>& t = things[i];
    const bool out = seen[i].alive && seen[i].out;
    if (out && !t.out) {
      if (seen[i].holder) t.thief = seen[i].holder;
      else t.thief = static_cast<void*>(t.lastCarrier);
      t.takenAt = now;
    } else if (!out) {
      t.thief = nullptr;
      t.lastCarrier = seen[i].holder;
    }
    t.out = out;
    angered |= out;
  }
  return angered;
}

/**
 * What it keeps: the room its things were surveyed in, its things and its anger. Its room is its
 * favourite room as the game holds it now (the room its speed and anger follow): when a Monkey Paw's wish
 * moves that room, as it moves a Goryo's, the old room's things are no longer its.
 */
template <class Player = void*, class Room = void*>
struct Hoard {
  Room room{};  // the room its things were surveyed in; null before the first survey
  std::vector<Thing<Player>> things;
  bool angered = false;

  /**
   * Its favourite room is `now`. When its things were surveyed in another room (or not yet), they and
   * their thieves are forgotten and it is calm: returns true, and the caller surveys `now` for its things.
   */
  bool follow(void* now) {
    if (room && static_cast<void*>(room) == now) return false;
    room = now;
    things.clear();
    angered = false;
    return true;
  }
};

/** The thing it takes back now: the one out of its room longest that is still in the world and that nobody carries; -1 for none. */
template <class Player>
int to_take_back(const std::vector<Thing<Player>>& things, const std::vector<Seen>& seen) {
  int best = -1;
  for (size_t i = 0; i < things.size() && i < seen.size(); ++i)
    if (things[i].out && seen[i].alive && !seen[i].holder && (best < 0 || things[i].takenAt < things[best].takenAt))
      best = static_cast<int>(i);
  return best;
}

/**
 * Of the thieves of its things still out, the most recent one `may_go_for(player)` allows; null for none.
 */
template <class Player, class MayGoFor>
void* thief_to_hunt(const std::vector<Thing<Player>>& things, MayGoFor may_go_for) {
  const Thing<Player>* best = nullptr;
  for (const Thing<Player>& t : things)
    if (t.out && t.thief && (!best || t.takenAt > best->takenAt) && may_go_for(t.thief)) best = &t;
  return best ? static_cast<void*>(best->thief) : nullptr;
}

/**
 * Its one target (PmGhostDef.target), to which the host holds its hunts as the Banshee's are held, its AI's
 * and a player's alike: while it is angered, the most recent thief of a thing still out that `living(player)` says is
 * alive and who is not `excluded` (where the game re-targets, the dying player); null when it is calm or
 * has no such thief, for no restriction.
 */
template <class Player, class Living>
void* target(const std::vector<Thing<Player>>& things, bool angered, Living living,
                    std::span<const void* const> excluded = {}) {
  return angered ? thief_to_hunt(things, [&](const Player& p) {
    void* const player = static_cast<void*>(p);
    return std::find(excluded.begin(), excluded.end(), player) == excluded.end() && living(player);
  })
                 : nullptr;
}

/**
 * Its state for its player's HUD (PmGhostDef.hudState, hud_line.h): 1 angered, 0 calm. The host answers
 * from its own things (`own`; -1 while it runs no Draugr), another client from the host's last word on
 * the anger (`heard`).
 */
inline int32_t hud_anger(bool master, int32_t own, bool heard) {
  if (master) return own;
  return heard ? 1 : 0;
}

/**
 * Its room's temperature while it is angered. The game's own temperature system (LevelRoom.Update) keeps
 * running on the room's own value: before each update the room is given its own value back, and after
 * it the room shows that value less kCold. The cold is a tell, not evidence (user, 2026-10-30), as the
 * Mimic's orbs show on any evidence: it goes below the floor the game keeps a room above for a ghost
 * without Freezing, so a freezing reading may point to a Draugr even when the contract withholds
 * Freezing. A value the game wrote between updates (the room's temperature is not the one it showed)
 * becomes its own. When the anger ends the room goes back to its own value at once.
 */
struct RoomCold {
  bool on = false;   // the room shows a colder temperature than its own
  float own = 0;     // the room's own temperature, as the game's system made it
  float shown = 0;   // what the room shows while `on`

  /** The temperature the game's update starts from, given the room's temperature `now`. */
  float before_update(float now) const { return on && now == shown ? own : now; }

  /** After the game's update made the room's own temperature `ownNow`: what the room shows. */
  float after_update(float ownNow, bool angered) {
    own = ownNow;
    on = angered;
    if (!on) return ownNow;
    shown = ownNow - kCold;
    return shown;
  }

  /** The anger is over (or its rules stopped): the temperature the room goes back to, given its temperature `now`. */
  float end(float now) {
    const float back = before_update(now);
    on = false;
    return back;
  }
};

}  // namespace pm::custom::draugr
