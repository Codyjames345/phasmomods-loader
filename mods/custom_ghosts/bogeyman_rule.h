#pragma once

#include <cmath>
#include <tuple>
#include <vector>

// The Bogeyman's rules, kept free of game calls so they can be tested offline. Each hunt begins from the
// closet nearest the youngest living player (lowest prestige, then lowest level), and in a hunt it goes
// for the youngest of the players it sees. With nobody in sight it searches the hiding places, nearest
// first, each once per hunt; in an open room it is slow and loses sight of its prey sooner. Outside
// hunts, most of its interactions near a closet open the closet's door.
namespace pg::custom {

constexpr float kBogeymanOpenArea = 30.0f;  // square metres of floor: a room this big is open
constexpr float kBogeymanOpenSpeed = 0.8f;  // times the game's hunt speed in an open room
constexpr float kBogeymanOpenSight = 7.0f;  // metres: in an open room it loses prey farther than this
constexpr float kBogeymanArrived = 0.8f;    // metres from a hiding place's spot: it searches it
constexpr float kBogeymanGiveUp = 8.0f;     // seconds on one hiding place it cannot reach before the next
constexpr float kBogeymanClosetDoorShare = 0.75f;  // of its interactions with a closet within reach, the share that open its door
constexpr float kBogeymanCreakReach = 4.0f;   // metres
constexpr float kBogeymanLurkReach = 10.0f;   // metres from the chosen player a hiding place may be (hiding_distance)

struct Place {
  float x, y, z;
};

// A player it may come for: their progress as the lobby shows it, and how far they are from the ghost.
struct Candidate {
  int prestige;
  int level;
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

/** The index of the hiding place nearest the player, if within kBogeymanLurkReach (hiding_distance); -1 otherwise. */
inline int lurk_place(const std::vector<Place>& places, Place player) {
  int best = -1;
  float bestDistance = 0;
  for (size_t i = 0; i < places.size(); ++i) {
    const float d = hiding_distance(places[i], player);
    if (d <= kBogeymanLurkReach && (best < 0 || d < bestDistance)) {
      best = static_cast<int>(i);
      bestDistance = d;
    }
  }
  return best;
}

}  // namespace pg::custom
