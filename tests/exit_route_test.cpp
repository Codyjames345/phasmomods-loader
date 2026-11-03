// Offline tests for the way the contract's end goes on the ghost player's client (src/exit_route.h).
// Run: build\exit_route_test.exe
#include "exit_route.h"

#include <utility>

#include "check.h"

using namespace pm::exit_route;

namespace {

Sender in_room(int actor, std::vector<int> actors) {
  Sender s;
  s.inRoom = true;
  s.readable = true;
  s.actor = actor;
  s.actors = std::move(actors);
  return s;
}

void the_rooms_first_actor_is_let_through() {
  CHECK(accepted(in_room(1, {1, 2, 3})));
  // The first actor is the lowest number among the room's players, not actor 1 nor the list's first.
  CHECK(accepted(in_room(4, {7, 4, 9})));
}

void any_other_sender_is_refused() {
  CHECK(!accepted(in_room(2, {1, 2, 3})));
  CHECK(!accepted(in_room(7, {7, 4, 9})));  // first in the list, but not the lowest number
  CHECK(!accepted(in_room(5, {})));         // a room with no players lets nobody through
}

void outside_a_room_or_offline_every_sender_is_let_through() {
  Sender out = in_room(2, {1, 2});
  out.inRoom = false;
  CHECK(accepted(out));
  Sender offline = in_room(2, {1, 2});
  offline.offline = true;
  CHECK(accepted(offline));
  offline.readable = false;  // the check reads nothing offline, so a null does not matter
  CHECK(accepted(offline));
}

void a_null_in_a_room_is_left_to_the_game() {
  Sender s = in_room(1, {1, 2});
  s.readable = false;
  CHECK(!accepted(s));
}

void only_a_ghost_players_accepted_call_takes_another_way() {
  CHECK(route(false, true, false) == Route::Game);  // an investigator's client
  CHECK(route(false, true, true) == Route::Game);
  CHECK(route(false, false, false) == Route::Game);
  CHECK(route(true, false, false) == Route::Game);  // a refused sender reaches the game's refusal
  CHECK(route(true, false, true) == Route::Game);
  CHECK(route(true, true, true) == Route::GameAsInvestigator);  // vanilla's way to the main menu
  CHECK(route(true, true, false) == Route::GhostPlayer);
}

}  // namespace

int main() {
  the_rooms_first_actor_is_let_through();
  any_other_sender_is_refused();
  outside_a_room_or_offline_every_sender_is_let_through();
  a_null_in_a_room_is_left_to_the_game();
  only_a_ghost_players_accepted_call_takes_another_way();
  std::printf(g_failures ? "%d check(s) failed\n" : "all exit route tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
