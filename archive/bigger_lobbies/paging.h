#pragma once

#include <cstdint>
#include <string>

// Eight players on screens made for four: the lobby's ID cards and the truck's sanity monitor keep
// their size and show four players at a time, players 1-4 or 5-8. Pure, testable offline.
namespace pg::lobbies {

constexpr int kPlayers = 8;     // the room's limit in a private lobby
constexpr int kVanilla = 4;     // the game's
constexpr int kPageSize = 4;
constexpr uint64_t kMonitorPageMs = 6000;  // the truck's monitor turns its page by itself

inline int pages(int players) { return players > kPageSize ? 2 : 1; }
/** Whether the player in `slot` (0-based, the game's spot order) is shown on `page`. */
inline bool on_page(int slot, int page) { return slot / kPageSize == page; }

/** The lobby's page tab: what clicking it shows next (empty while everyone fits on one page). */
inline std::string tab_label(int page, int players) {
  if (pages(players) < 2) return {};
  return page == 0 ? "Players 5-8 >" : "< Players 1-4";
}

/** A page that turns by itself every kMonitorPageMs while there are two. */
class AutoPager {
 public:
  int page(int players, uint64_t now) {
    if (pages(players) < 2) {
      page_ = 0;
      next_ = 0;
      return 0;
    }
    if (!next_) next_ = now + kMonitorPageMs;
    if (now >= next_) {
      page_ = 1 - page_;
      next_ = now + kMonitorPageMs;
    }
    return page_;
  }

 private:
  int page_ = 0;
  uint64_t next_ = 0;
};

/**
 * The room's limit for a value the game sets: its four becomes eight in a private room, unless the
 * game is closing the room to the players already in it (it sets the limit to their number when a
 * contract starts; `lockedRecently` says such a call came moments ago).
 */
inline int room_limit(int requested, bool privateRoom, int playersInRoom, bool lockedRecently) {
  if (!privateRoom || requested != kVanilla) return requested;
  if (playersInRoom == kVanilla && lockedRecently) return requested;
  return kPlayers;
}

}  // namespace pg::lobbies
