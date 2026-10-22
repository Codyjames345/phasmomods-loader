// Offline tests for the vanilla rules the controlled ghost keeps (src/tether.h): who moves it, how
// far a wander lets it go, when a hunt's search ends, when an offered hunt still stands, and the
// gates on its actions. Run: build\tether_test.exe
#include "tether.h"

#include <cmath>

#include "check.h"

using namespace pg::tether;

namespace {

Ghost in(int state) { return {state, false, false}; }

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void the_player_moves_the_ghost_only_where_a_vanilla_ghost_moves() {
  CHECK(reason(in(kWander)) == kRoaming);
  CHECK(reason(in(kRoam)) == kRoaming);
  CHECK(reason(in(kHunting)) == kSteer);
  // Between wanders a vanilla ghost stands where it is: idle and its interactions.
  CHECK(reason(in(kIdle)) == kResting);
  CHECK(reason(in(4)) == kResting);  // a light switch
  CHECK(reason(in(kFuseBox)) == kResting);
  CHECK(!native_moves(kSteer) && !native_moves(kRoaming) && !native_moves(kResting));
}

void a_favourite_room_roll_walks_the_ghost_home_natively() {
  CHECK(reason(in(kFavouriteRoom)) == kDrawnHome);
  Ghost home = in(kFavouriteRoom);
  home.inFavouriteRoom = true;
  CHECK(reason(home) == kActing);  // the walk to its spot in the room, still native
  CHECK(native_moves(kDrawnHome) && native_moves(kActing));
}

void events_abilities_and_smoke_move_the_ghost_natively() {
  CHECK(reason(in(kRandomEvent)) == kEvent);
  CHECK(reason(in(kAbility)) == kActing);
  Ghost g = in(kHunting);
  g.smudged = true;
  CHECK(reason(g) == kBlinded);
  CHECK(native_moves(kEvent) && native_moves(kBlinded));
}

void a_wander_reaches_its_radius_or_its_chosen_point() {
  const Reach hop = reach_of(1, 1, 3, true, 2, 1);  // the usual three-metre hop, point one metre off
  CHECK(near(hop.radius, 3) && near(hop.x, 1) && near(hop.z, 1));
  const Reach banshee = reach_of(0, 0, 3, true, 8, 6);  // towards its target, ten metres away
  CHECK(near(banshee.radius, 10));
  CHECK(near(reach_of(0, 0, 5, false, 40, 40).radius, 5));  // no point: the radius alone
}

void at_the_edge_of_its_reach_the_ghost_only_slides_along_it() {
  const Reach r{0, 0, 3};
  float dx = 1, dz = 0;
  keep_within(r, 1, 0, dx, dz);  // inside: free
  CHECK(near(dx, 1) && near(dz, 0));
  dx = 1, dz = 0;
  keep_within(r, 3, 0, dx, dz);  // at the edge heading out: stopped
  CHECK(near(dx, 0) && near(dz, 0));
  dx = std::sqrt(0.5f), dz = std::sqrt(0.5f);
  keep_within(r, 3, 0, dx, dz);  // diagonal: the sideways part stays
  CHECK(near(dx, 0) && near(dz, std::sqrt(0.5f)));
  dx = -1, dz = 0;
  keep_within(r, 3.5f, 0, dx, dz);  // heading back in: free
  CHECK(near(dx, -1));
}

void a_lost_targets_search_ends_after_the_vanilla_walk() {
  SearchPace pace;
  // Ten metres of path at 2 m/s: five seconds.
  CHECK(!pace.tick(true, 0, 0, 8, 0, 10, 2, 1));
  CHECK(!pace.tick(true, 0, 0, 8, 0, 10, 2, 3));
  CHECK(pace.tick(true, 0, 0, 8, 0, 10, 2, 1));
  // The end is set where the ghost is: that is not a new search point.
  CHECK(!pace.tick(true, 0, 0, 0, 0, -1, 2, 10));
  // A new search point starts a new walk; an unknown path is measured straight.
  CHECK(!pace.tick(true, 0, 0, 0, 6, -1, 3, 1));
  CHECK(pace.tick(true, 0, 0, 0, 6, -1, 3, 1));
  // Seeing a target again (no search) resets it.
  CHECK(!pace.tick(false, 0, 0, 0, 6, -1, 3, 1));
  CHECK(!pace.tick(true, 0, 0, 0, 6, 4, 1, 1));
}

void a_search_path_that_arrives_late_replaces_the_straight_guess() {
  SearchPace pace;
  // The path is still being worked out: measured straight, five metres.
  CHECK(!pace.tick(true, 0, 0, 5, 0, -1, 1, 2));
  // It turns out to be twelve metres from here, with two already walked: fourteen in all.
  CHECK(!pace.tick(true, 0, 0, 5, 0, 12, 1, 4));   // six walked
  CHECK(!pace.tick(true, 0, 0, 5, 0, 8, 1, 7));    // thirteen walked; later lengths change nothing
  CHECK(pace.tick(true, 0, 0, 5, 0, 1, 1, 1));     // fourteen
}

void actions_start_only_from_idle_or_a_wander() {
  CHECK(may_act_from(kIdle) && may_act_from(kWander) && may_act_from(kRoam));
  CHECK(!may_act_from(kHunting) && !may_act_from(kFavouriteRoom) && !may_act_from(kRandomEvent));
  CHECK(!may_act_from(kAbility) && !may_act_from(kFuseBox) && !may_act_from(5));  // a door in progress
}

void a_throw_with_nothing_to_throw_costs_nothing() {
  CHECK(aimed_state_counts(5, true));  // a door
  CHECK(aimed_state_counts(kThrow, true));
  CHECK(!aimed_state_counts(kThrow, false));
  CHECK(!aimed_state_counts(kIdle, true));
}

void trying_to_act_takes_the_roll_back_from_restlessness() {
  CHECK(!restless(29999, 0));
  CHECK(restless(30000, 0));
  CHECK(!restless(30000, 30000));  // a try at 30 s, even one that failed, ends it
  CHECK(restless(60001, 30000));
}

void native_blocks_take_back_an_offered_hunt() {
  HuntGate g;
  g.insanity = 60;
  g.haveModifier = true;
  g.modifier = 0;
  CHECK(hunt_allowed(g));
  HuntGate smudged = g;
  smudged.smudged = true;
  CHECK(!hunt_allowed(smudged));
  HuntGate cooling = g;
  cooling.cooldown = true;
  CHECK(!hunt_allowed(cooling));
  HuntGate hunting = g;
  hunting.hunting = true;
  CHECK(!hunt_allowed(hunting));
}

void sanity_recovered_above_the_threshold_takes_back_an_offered_hunt() {
  HuntGate g;
  g.haveModifier = true;
  g.modifier = 15;  // e.g. a Demon's or a hard difficulty's head start
  g.insanity = 35.9f;
  CHECK(hunt_allowed(g));  // (int)35.9 + 15 = 50, as the native check truncates
  g.insanity = 34.5f;
  CHECK(!hunt_allowed(g));
  g.thaye = true;  // the Thaye's check is by age, not sanity
  CHECK(hunt_allowed(g));
  g.thaye = false;
  g.haveModifier = false;  // not learned yet: the native roll's own verdict stands
  CHECK(hunt_allowed(g));
  g.haveModifier = true;
  g.insanity = -1;  // nobody alive to measure
  CHECK(hunt_allowed(g));
}

void actions_keep_the_rolls_vanilla_gates() {
  ActionGate g;
  CHECK(!may_interact(g));  // the front door is still locked
  g.mainDoorOpen = true;
  CHECK(may_interact(g) && may_use_ability(g) && may_cause_event(g));
  ActionGate shade = g;
  shade.shadeWatched = true;
  CHECK(!may_use_ability(shade) && !may_cause_event(shade) && may_interact(shade));
  ActionGate home = g;
  home.stayHome = true;
  CHECK(!may_use_ability(home) && !may_cause_event(home));
  ActionGate quiet = g;
  quiet.noEvents = true;
  CHECK(may_use_ability(quiet) && !may_cause_event(quiet));
}

void the_fuse_box_keeps_each_types_rules() {
  CHECK(fuse_action(true, false, false, true, 0) == kFuseTurnOff);
  CHECK(fuse_action(true, false, false, false, 0) == kFuseTurnOn);
  CHECK(fuse_action(true, false, false, true, kJinn) == kFuseNone);  // a Jinn never turns it off
  CHECK(fuse_action(true, false, false, false, kJinn) == kFuseTurnOn);
  CHECK(fuse_action(true, false, false, false, kHantu) == kFuseNone);  // a Hantu never turns it on
  CHECK(fuse_action(true, false, false, true, kHantu) == kFuseTurnOff);
  CHECK(fuse_action(false, false, false, true, 0) == kFuseNone);
  CHECK(fuse_action(true, true, false, true, 0) == kFuseNone);
  CHECK(fuse_action(true, false, true, true, 0) == kFuseNone);
}

}  // namespace

int main() {
  the_player_moves_the_ghost_only_where_a_vanilla_ghost_moves();
  a_favourite_room_roll_walks_the_ghost_home_natively();
  events_abilities_and_smoke_move_the_ghost_natively();
  a_wander_reaches_its_radius_or_its_chosen_point();
  at_the_edge_of_its_reach_the_ghost_only_slides_along_it();
  a_lost_targets_search_ends_after_the_vanilla_walk();
  a_search_path_that_arrives_late_replaces_the_straight_guess();
  actions_start_only_from_idle_or_a_wander();
  a_throw_with_nothing_to_throw_costs_nothing();
  trying_to_act_takes_the_roll_back_from_restlessness();
  native_blocks_take_back_an_offered_hunt();
  sanity_recovered_above_the_threshold_takes_back_an_offered_hunt();
  actions_keep_the_rolls_vanilla_gates();
  the_fuse_box_keeps_each_types_rules();
  std::printf(g_failures ? "%d check(s) failed\n" : "all tether tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
