// Offline tests for the map picker's room hit test (src/room_pick.h). Run: build\room_pick_test.exe
#include "room_pick.h"

#include "check.h"

using namespace pg::room_pick;

namespace {

std::vector<Room> house() {
  std::vector<Room> r(4);
  r[0] = {1, {{0, 0, 10, 10}}};            // living room, ground floor
  r[1] = {1, {{2, 2, 4, 4}}};              // cupboard inside the living room's bounds
  r[2] = {2, {{0, 0, 10, 10}}};            // bedroom directly above
  r[3] = {1, {{10, 0, 14, 4}, {10, 4, 20, 6}}};  // L-shaped hallway from two colliders
  return r;
}

void picks_the_room_on_the_shown_floor() {
  const auto r = house();
  CHECK(at(r, 1, 8, 8) == 0);
  CHECK(at(r, 2, 8, 8) == 2);
  CHECK(at(r, 0, 8, 8) == -1);  // nothing in the basement here
}

void smallest_overlapping_room_wins() {
  const auto r = house();
  CHECK(at(r, 1, 3, 3) == 1);
  CHECK(at(r, 2, 3, 3) == 2);  // the cupboard is on another floor
}

void any_collider_of_a_room_counts() {
  const auto r = house();
  CHECK(at(r, 1, 12, 2) == 3);
  CHECK(at(r, 1, 18, 5) == 3);
  CHECK(at(r, 1, 18, 2) == -1);  // inside the L's bounding box but in neither collider
}

void outside_every_room_is_nothing() {
  const auto r = house();
  CHECK(at(r, 1, -1, 5) == -1);
  CHECK(at({}, 1, 5, 5) == -1);
}

}  // namespace

int main() {
  picks_the_room_on_the_shown_floor();
  smallest_overlapping_room_wins();
  any_collider_of_a_room_counts();
  outside_every_room_is_nothing();
  std::printf(g_failures ? "%d check(s) failed\n" : "all room pick tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
