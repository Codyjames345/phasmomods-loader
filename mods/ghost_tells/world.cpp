#include "world.h"

#include <algorithm>
#include <cmath>

#include "game.h"
#include "players.h"

namespace pg::tells::world {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (class dump).
constexpr size_t kGhostInfo = 0x38;           // GhostAI: GhostInfo
constexpr size_t kGhostAppeared = 0xA8;       // GhostAI: showing itself
constexpr size_t kGhostHunting = 0xF3;        // GhostAI: hunting (the Hunting RPC, every client)
constexpr size_t kInfoBehaviour = 0x2C;       // GhostInfo: the type whose rules its AI follows
constexpr size_t kInfoFavouriteRoom = 0x70;   // GhostInfo: favourite LevelRoom
constexpr size_t kHuntTarget = 0x48;          // hunt state: the player it has detected, or null
constexpr size_t kHuntSearch = 0x50;          // hunt state: the player it searches for, or null
constexpr size_t kLevelGhostRoom = 0x38;      // LevelController: the room the ghost is in (synced)
constexpr size_t kLevelRooms = 0x50;          // LevelController: LevelRoom[]
constexpr float kChest = 1.2f, kEye = 1.6f;   // metres above the feet
constexpr float kFloor = 2.5f;                // metres up or down still counted as the same floor

il2cpp::Array* rooms() {
  void* level = game::level_controller();
  return level && game::alive(level) ? il2cpp::field<il2cpp::Array*>(level, kLevelRooms) : nullptr;
}

}  // namespace

int behaviour_type(void* ai) {
  void* info = ai && game::alive(ai) ? il2cpp::field<void*>(ai, kGhostInfo) : nullptr;
  return info ? il2cpp::field<int>(info, kInfoBehaviour) : -1;
}

bool hunting(void* ai) { return ai && game::alive(ai) && il2cpp::field<bool>(ai, kGhostHunting); }
bool appeared(void* ai) { return ai && game::alive(ai) && il2cpp::field<bool>(ai, kGhostAppeared); }

void* favourite_room(void* ai) {
  void* info = ai && game::alive(ai) ? il2cpp::field<void*>(ai, kGhostInfo) : nullptr;
  void* room = info ? il2cpp::field<void*>(info, kInfoFavouriteRoom) : nullptr;
  return room && game::alive(room) ? room : nullptr;
}

void* hunt_state(void* ai) {
  return ai && game::alive(ai) && game::ghost_state(ai) == game::kStateHunting ? game::ghost_state_object(ai) : nullptr;
}

void* hunt_target(void* ai) {
  void* state = hunt_state(ai);
  void* target = state ? il2cpp::field<void*>(state, kHuntTarget) : nullptr;
  return target && game::alive(target) ? target : nullptr;
}

void* ghost_room() {
  void* level = game::level_controller();
  void* room = level && game::alive(level) ? il2cpp::field<void*>(level, kLevelGhostRoom) : nullptr;
  return room && game::alive(room) ? room : nullptr;
}

void* hunt_search(void* ai) {
  void* state = hunt_state(ai);
  void* player = state ? il2cpp::field<void*>(state, kHuntSearch) : nullptr;
  return player && game::alive(player) ? player : nullptr;
}

void* room_at(Vector3 p) {
  il2cpp::Array* all = rooms();
  for (uintptr_t i = 0; all && i < all->max_length; ++i)
    if (void* room = all->data<void*>()[i]; room && game::alive(room) && game::in_room(room, p)) return room;
  return nullptr;
}

int room_index(void* room) {
  il2cpp::Array* all = rooms();
  for (uintptr_t i = 0; room && all && i < all->max_length; ++i)
    if (all->data<void*>()[i] == room) return static_cast<int>(i);
  return -1;
}

void* room_by_index(int index) {
  il2cpp::Array* all = rooms();
  if (!all || index < 0 || static_cast<uintptr_t>(index) >= all->max_length) return nullptr;
  void* room = all->data<void*>()[index];
  return room && game::alive(room) ? room : nullptr;
}

float distance_to_room(void* room, Vector3 p) {
  float best = 1e9f;
  for (const game::Bounds& b : game::room_bounds(room)) {
    const float dx = std::max(0.0f, std::fabs(p.x - b.center.x) - b.extents.x);
    const float dy = std::max(0.0f, std::fabs(p.y - b.center.y) - b.extents.y);
    const float dz = std::max(0.0f, std::fabs(p.z - b.center.z) - b.extents.z);
    best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return best;
}

bool clear_line(Vector3 from, Vector3 to) {
  const float dx = to.x - from.x, dy = to.y - from.y, dz = to.z - from.z;
  const float d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (d < 0.5f) return true;
  return !game::raycast(from, {dx / d, dy / d, dz / d}, d - 0.4f);  // stops short of what it looks at
}

Vector3 feet(void* component) { return game::position(game::transform_of(component)); }

Vector3 chest(void* ai) {
  const Vector3 f = feet(ai);
  return {f.x, f.y + kChest, f.z};
}

Vector3 eye(void* player) {
  const Vector3 f = feet(player);
  return {f.x, f.y + kEye, f.z};
}

bool same_floor(Vector3 a, Vector3 b) { return std::fabs(a.y - b.y) < kFloor; }

int living_within(Vector3 p, float range) {
  int n = 0;
  game::each_player([&](void* player) {
    if (!game::living(player)) return;
    const Vector3 at = feet(player);
    if (same_floor(at, p) && std::hypot(at.x - p.x, at.z - p.z) <= range) ++n;
  });
  return n;
}

}  // namespace pg::tells::world
