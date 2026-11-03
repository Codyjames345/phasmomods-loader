#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <tuple>
#include <vector>

// The Bogeyman's rules, kept free of game calls so they can be tested offline. Each hunt begins from the
// closet nearest the youngest living player (lowest prestige, then lowest level, then who joined first),
// and goes only for that player, its target (bogeyman_target), until they are in the truck, dead or killed,
// as the game's Banshee keeps to its one (host/content_target.h). With no target it searches the hiding
// places, each once per hunt, its AI walking to the nearest first and a player searching those their route
// reaches; in an open room it is slow and loses sight of its prey sooner. Outside hunts, most of its door
// interactions are on a closet door when one is in reach. Its AI and a player controlling it follow every one of
// these alike.
namespace pm::custom {

constexpr float kBogeymanOpenArea = 30.0f;  // square metres of floor: a room this big is open
constexpr float kBogeymanOpenSpeed = 0.8f;  // times the game's hunt speed in an open room
constexpr float kBogeymanOpenSight = 7.0f;  // metres: in an open room it loses prey farther than this
constexpr float kBogeymanArrived = 0.8f;    // metres from a hiding place's spot: it searches it
constexpr float kBogeymanGiveUp = 8.0f;     // seconds on one hiding place it cannot reach before the next
constexpr float kBogeymanClosetDoorShare = 0.75f;  // of its door interactions with a closet door in reach, the share on one
constexpr float kBogeymanLurkReach = 10.0f;   // metres from the chosen player a closet it starts a hunt from may be
constexpr float kBogeymanSameFloor = 2.0f;    // metres of height between a closet and the player: on their floor

struct Place {
  float x, y, z;
};

// The prestige and level of a player whose progress cannot be read: after every progress that can be.
constexpr int kUnknownProgress = std::numeric_limits<int>::max();

// A player it may come for, as every client knows them alike: their progress from the lobby's spots (a
// buffered RPC each player sends) and their Photon actor (lower joined first).
struct Candidate {
  int prestige;  // kUnknownProgress when it cannot be read
  int level;     // kUnknownProgress when it cannot be read
  int actor;
  bool living;   // alive, and for its hunts somewhere the hunt may go for them
};

/** Metres between two points, a difference in height counting three times: another floor is far. */
inline float hiding_distance(Place a, Place b) {
  const float dx = a.x - b.x, dy = (a.y - b.y) * 3, dz = a.z - b.z;
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

/** The index of the nearest hiding place not yet searched (`searched[i]`), or -1 for none left. */
inline int next_hiding_place(const std::vector<Place>& places, const std::vector<bool>& searched, Place from) {
  int best = -1;
  float bestDistance = 0;
  for (size_t i = 0; i < places.size(); ++i) {
    if (i < searched.size() && searched[i]) continue;
    const float d = hiding_distance(places[i], from);
    if (best < 0 || d < bestDistance) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  return best;
}

/**
 * The index of the hiding place not yet searched that a player's route has brought it to, the nearest within
 * kBogeymanArrived of `at` (true distance, as its AI's arrival); -1 for none.
 */
inline int reached_hiding_place(const std::vector<Place>& places, const std::vector<bool>& searched, Place at) {
  int best = -1;
  float bestDistance = kBogeymanArrived;
  for (size_t i = 0; i < places.size(); ++i) {
    if (i < searched.size() && searched[i]) continue;
    const float dx = places[i].x - at.x, dy = places[i].y - at.y, dz = places[i].z - at.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (d < bestDistance) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  return best;
}

/** Whether a room with this floor area (square metres) is open. */
inline bool open_room(float floorArea) { return floorArea >= kBogeymanOpenArea; }

/** Whether it has lost sight of a player this many metres away: only in an open room, beyond kBogeymanOpenSight. */
inline bool open_sight_lost(bool inOpenRoom, float distance) { return inOpenRoom && distance > kBogeymanOpenSight; }

/**
 * The index of the youngest living candidate: the lowest prestige, then the lowest level within it,
 * then the one who joined first (the lowest actor); -1 for none. "Youngest" is the least progressed, as
 * the lobby board shows players. The last tie goes by join order, never by distance, so every client
 * names the same player whatever the ghost's position there.
 */
inline int youngest(const std::vector<Candidate>& c) {
  int best = -1;
  for (size_t i = 0; i < c.size(); ++i) {
    if (!c[i].living) continue;
    if (best < 0 ||
        std::tie(c[i].prestige, c[i].level, c[i].actor) < std::tie(c[best].prestige, c[best].level, c[best].actor))
      best = static_cast<int>(i);
  }
  return best;
}

/**
 * Its one target (PmGhostDef.target), to which the host holds its hunts as the Banshee's are held: the
 * youngest living player (youngest()), so the next youngest once they die, leaving out the players of
 * `excludedActors` (where the game re-targets, the dying one); -1 for none.
 */
inline int bogeyman_target(std::vector<Candidate> players, std::span<const int32_t> excludedActors = {}) {
  for (Candidate& c : players)
    if (std::find(excludedActors.begin(), excludedActors.end(), c.actor) != excludedActors.end()) c.living = false;
  return youngest(players);
}

/**
 * Whether its hunt steps out of a closet now (bogeyman.h hunt_begins): on the host (`master`), right after a
 * change of its own ghost to the hunting state (`own`, `toHunting`) that found it not hunting (`wasHunting`)
 * and left it hunting in a new hunt state (`newHunt`; the difficulty may turn the hunt into idling, and a
 * change the game ignores keeps the old state). Who steers the ghost plays no part: a player's hunt begins
 * from the closet as its AI's does.
 */
inline bool bogeyman_steps_out(bool own, bool master, bool toHunting, bool wasHunting, bool newHunt) {
  return own && master && toHunting && !wasHunting && newHunt;
}

/**
 * Whether its door rule may turn a door pick into a closet door (bogeyman.h closet_door): its own ghost's
 * idle door chance asked for the door (`chancePick`; a player's aimed interaction runs the same chance), the
 * pick found one (`found`), outside a hunt (`hunting`), on the host (`master`). Who steers the ghost plays no
 * part.
 */
inline bool bogeyman_turns_door(bool own, bool master, bool chancePick, bool found, bool hunting) {
  return own && master && chancePick && found && !hunting;
}

/**
 * Which of the `count` closet doors in reach it takes, for a share roll in [0, 1) and an index roll (any
 * number, taken modulo `count`): kBogeymanClosetDoorShare of the time one of them; -1 when there is none or
 * the share roll fails, and the game's own pick stands.
 */
inline int bogeyman_closet_door(size_t count, float shareRoll, size_t indexRoll) {
  if (!count || shareRoll >= kBogeymanClosetDoorShare) return -1;
  return static_cast<int>(indexRoll % count);
}

/**
 * The index of the hiding place nearest the player that lies within kBogeymanLurkReach of them (true
 * distance) and on their floor (within kBogeymanSameFloor of their height); -1 for none.
 */
inline int lurk_place(const std::vector<Place>& places, Place player) {
  int best = -1;
  float bestDistance = 0;
  for (size_t i = 0; i < places.size(); ++i) {
    const float dx = places[i].x - player.x, dy = places[i].y - player.y, dz = places[i].z - player.z;
    const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
    if (std::fabs(dy) <= kBogeymanSameFloor && d <= kBogeymanLurkReach && (best < 0 || d < bestDistance)) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  return best;
}

}  // namespace pm::custom
