#pragma once

#include <vector>

// Which favourite-room candidate a click on the map picker lands in, kept free of game calls so it
// can be tested offline. Each candidate carries its floor and the top-down (x, z) boxes of its room
// colliders; the click is a world-space (x, z) point on the floor the map is showing.
namespace pg::room_pick {

struct Box {
  float minX, minZ, maxX, maxZ;
};

struct Room {
  int floor = 0;           // LevelRoom floor (basement 0, first floor 1, ...)
  std::vector<Box> boxes;  // its colliders' bounds
};

// Index of the room containing (x, z) on `floor`, or -1. Where rooms overlap (a cupboard inside a
// bedroom's bounds) the smallest box wins, as the more specific room.
inline int at(const std::vector<Room>& rooms, int floor, float x, float z) {
  int best = -1;
  float bestArea = 0;
  for (size_t i = 0; i < rooms.size(); ++i) {
    if (rooms[i].floor != floor) continue;
    for (const Box& b : rooms[i].boxes) {
      if (x < b.minX || x > b.maxX || z < b.minZ || z > b.maxZ) continue;
      const float area = (b.maxX - b.minX) * (b.maxZ - b.minZ);
      if (best < 0 || area < bestArea) {
        best = static_cast<int>(i);
        bestArea = area;
      }
    }
  }
  return best;
}

}  // namespace pg::room_pick
