#pragma once

#include <cmath>
#include <limits>
#include <tuple>
#include <vector>

// The Bogeyman's rules, kept free of game calls so they can be tested offline. Each hunt begins from the
// closet nearest the youngest living player (lowest prestige, then lowest level), and in a hunt it goes
// for the youngest of the players it sees. With nobody in sight it searches the hiding places, nearest
// first, each once per hunt; in an open room it is slow and loses sight of its prey sooner. Outside
// hunts, most of its door interactions are on a closet door when one is in reach.
namespace pg::custom {

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

// A player it may come for: their progress as the lobby shows it, and how far they are from the ghost.
struct Candidate {
  int prestige;  // kUnknownProgress when it cannot be read
  int level;     // kUnknownProgress when it cannot be read
  float distance;  // metres from the ghost
  bool living;     // alive and somewhere the hunt may go for them
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

/** Whether a room with this floor area (square metres) is open. */
inline bool open_room(float floorArea) { return floorArea >= kBogeymanOpenArea; }

/** Whether it has lost sight of a player this many metres away: only in an open room, beyond kBogeymanOpenSight. */
inline bool open_sight_lost(bool inOpenRoom, float distance) { return inOpenRoom && distance > kBogeymanOpenSight; }

enum class BogeymanPick { Lost, Proposed, Youngest };  // what its hunt's sight test answers for a player

/**
 * What its hunt's sight test answers for a player the game's own test sees, `distance` metres away: lost
 * beyond its short sight in an open room, whoever else is in sight; else that player while fewer than two are
 * in its sight (`inSight()`, counted only when needed); else only the youngest of those in sight.
 */
template <class InSight>
BogeymanPick bogeyman_pick(bool inOpenRoom, float distance, InSight inSight) {
  if (open_sight_lost(inOpenRoom, distance)) return BogeymanPick::Lost;
  return inSight() < 2 ? BogeymanPick::Proposed : BogeymanPick::Youngest;
}

/**
 * The index of the youngest living candidate: the lowest prestige, then the lowest level within it,
 * then the nearest; -1 for none. "Youngest" is the least progressed, as the lobby board shows players.
 */
inline int youngest(const std::vector<Candidate>& c) {
  int best = -1;
  for (size_t i = 0; i < c.size(); ++i) {
    if (!c[i].living) continue;
    if (best < 0 || std::tie(c[i].prestige, c[i].level, c[i].distance) <
                        std::tie(c[best].prestige, c[best].level, c[best].distance))
      best = static_cast<int>(i);
  }
  return best;
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

}  // namespace pg::custom
