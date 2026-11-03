// Offline tests for the vanilla rules the controlled ghost keeps (src/tether.h): who moves it, how
// far a wander lets it go, when a hunt's search ends, when an offered hunt still stands, and the
// gates on its actions. Run: build\tether_test.exe
#include "tether.h"

#include <cmath>

#include "check.h"
#include "text_fixture.h"
#include "roll.h"

using namespace pm::tether;

namespace {

// Two numberings, neither the game's: the rules read every state's number from the game, so nothing here may know
// them. Numbers neither names (kOther) stand for the game's other states.
constexpr States kStates{.idle = 40, .wander = 41, .dots = 47, .hunting = 42, .favouriteRoom = 43, .throwing = 46,
                         .fuseBox = 48, .randomEvent = 44, .ability = 45};
constexpr int kOther = 30, kOtherToo = 31;
constexpr States kMoved{.idle = 3, .wander = 9, .dots = 5, .hunting = 0, .favouriteRoom = 1, .throwing = 2,
                        .fuseBox = 11, .randomEvent = 4, .ability = 6};

Ghost in(int state) { return {state, false, false}; }

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

void the_player_moves_the_ghost_only_where_a_vanilla_ghost_moves() {
  for (const States& s : {kStates, kMoved}) {
    CHECK(reason(in(s.wander), s) == kRoaming);
    CHECK(reason(in(s.dots), s) == kRoaming);
    CHECK(reason(in(s.hunting), s) == kSteer);
    // Between wanders a vanilla ghost stands where it is: idle and its interactions.
    CHECK(reason(in(s.idle), s) == kResting);
    CHECK(reason(in(s.throwing), s) == kResting);
    CHECK(reason(in(s.fuseBox), s) == kResting);
  }
  CHECK(reason(in(kOther), kStates) == kResting);  // another state, a light switch
  CHECK(!native_moves(kSteer) && !native_moves(kRoaming) && !native_moves(kResting));
}

void a_favourite_room_roll_walks_the_ghost_home_natively() {
  for (const States& s : {kStates, kMoved}) {
    CHECK(reason(in(s.favouriteRoom), s) == kDrawnHome);
    Ghost home = in(s.favouriteRoom);
    home.inFavouriteRoom = true;
    CHECK(reason(home, s) == kActing);  // the walk to its spot in the room, still native
  }
  CHECK(native_moves(kDrawnHome) && native_moves(kActing));
}

void events_abilities_and_smoke_move_the_ghost_natively() {
  for (const States& s : {kStates, kMoved}) {
    CHECK(reason(in(s.randomEvent), s) == kEvent);
    CHECK(reason(in(s.ability), s) == kActing);
    Ghost g = in(s.hunting);
    g.smudged = true;
    CHECK(reason(g, s) == kBlinded);
  }
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
  for (const States& s : {kStates, kMoved}) {
    CHECK(may_act_from(s.idle, s) && may_act_from(s.wander, s) && may_act_from(s.dots, s));
    CHECK(!may_act_from(s.hunting, s) && !may_act_from(s.favouriteRoom, s) && !may_act_from(s.randomEvent, s));
    CHECK(!may_act_from(s.ability, s) && !may_act_from(s.fuseBox, s));
  }
  CHECK(!may_act_from(kOtherToo, kStates));  // another state, a door in progress
}

void a_throw_with_nothing_to_throw_costs_nothing() {
  for (const States& s : {kStates, kMoved}) {
    CHECK(aimed_state_counts(s.fuseBox, true, s));
    CHECK(aimed_state_counts(s.fuseBox, false, s));
    CHECK(aimed_state_counts(s.throwing, true, s));
    CHECK(!aimed_state_counts(s.throwing, false, s));
    CHECK(!aimed_state_counts(s.idle, true, s));
  }
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
  CHECK(fuse_action(true, false, false, true, "vanilla.spirit") == kFuseTurnOff);
  CHECK(fuse_action(true, false, false, false, "vanilla.spirit") == kFuseTurnOn);
  CHECK(fuse_action(true, false, false, true, "vanilla.jinn") == kFuseNone);  // a Jinn never turns it off
  CHECK(fuse_action(true, false, false, false, "vanilla.jinn") == kFuseTurnOn);
  CHECK(fuse_action(true, false, false, false, "vanilla.hantu") == kFuseNone);  // a Hantu never turns it on
  CHECK(fuse_action(true, false, false, true, "vanilla.hantu") == kFuseTurnOff);
  // A registered ghost, or a type the content layer cannot name, follows the rule for any other type.
  CHECK(fuse_action(true, false, false, true, "custom.ghost") == kFuseTurnOff);
  CHECK(fuse_action(true, false, false, false, "") == kFuseTurnOn);
  CHECK(fuse_action(false, false, false, true, "vanilla.spirit") == kFuseNone);
  CHECK(fuse_action(true, true, false, true, "vanilla.spirit") == kFuseNone);
  CHECK(fuse_action(true, false, true, true, "vanilla.spirit") == kFuseNone);
}

}  // namespace

// The idle roll's draws, told apart by their scope as the decompiled GhostIdleState.Execute makes them.
void the_idle_roll_tells_its_own_draws() {
  using pm::roll::Idle;
  Idle roll;
  roll.draw(0, 2, 1, true);  // a state change's draw before the coin is not the coin
  CHECK(roll.coin == -1);
  roll.draw(0, 2, 1, false);
  CHECK(roll.coin == 1 && !roll.decided);
  roll.draw(0, 4, 1, false);  // the hunt branch's chance, passed
  CHECK(roll.decided && roll.huntRolled && !roll.passed_without_hunting());
  roll.draw(0, 11, 1, false);  // nothing after it counts
  CHECK(roll.huntRolled);
  roll.begin();
  roll.draw(0, 2, 1, false);
  roll.draw(0, 4, 0, false);  // the chance failed
  CHECK(roll.decided && !roll.huntRolled && roll.passed_without_hunting());
  roll.begin();
  roll.draw(0, 2, 1, false);
  roll.draw(0, 11, 1, false);  // a check failed: the next draw is the activity roll's
  CHECK(!roll.huntRolled && roll.passed_without_hunting());
  roll.begin();
  roll.draw(0, 2, 0, false);  // the coin says no hunt branch
  roll.draw(0, 4, 1, false);
  CHECK(!roll.huntRolled && !roll.passed_without_hunting());
}

// A chance's own roll becomes 0 in an aimed interaction, and only that draw, however it continues.
void a_chance_answers_only_its_own_roll() {
  pm::roll::Chance c;
  CHECK(c.draw(0, 100, 57, true) == 57);  // no chance began: the game's own
  c.due = true;
  CHECK(c.draw(0, 4, 3, true) == 3);       // another draw is the game's
  CHECK(c.draw(0, 100, 57, true) == 0);    // its roll
  CHECK(!c.due && c.draw(0, 100, 58, true) == 58);  // what it runs after that draws for itself
  c.due = true;
  CHECK(c.draw(0, 100, 57, false) == 57 && !c.due);  // not aimed: the roll stands, and is spent
}

// A chance's scope puts the roll back as it found it when the chance ends, whatever it threw, and a chance
// inside another leaves the outer one's state alone; the state change count likewise.
void a_chances_scope_restores_on_unwind_and_nests() {
  using pm::roll::Chance;
  using pm::roll::ChanceScope;
  Chance c;
  {
    ChanceScope outer{c};
    CHECK(c.due);
    {
      ChanceScope inner{c};  // a chance run before the outer one drew its roll
      CHECK(c.draw(0, 100, 40, true) == 0 && !c.due);
    }
    CHECK(c.due && c.draw(0, 100, 41, true) == 0);  // the outer one's roll is still its own
    {
      ChanceScope inner{c};  // a chance run after it drew
      CHECK(c.due);
    }
    CHECK(!c.due && c.draw(0, 100, 42, true) == 42);
  }
  CHECK(!c.due);
  try {
    ChanceScope thrown{c};
    throw 1;
  } catch (int) {
  }
  CHECK(!c.due);  // the roll is not left over for a draw outside the chance

  int running = 0;
  try {
    pm::roll::Running outer{running};
    pm::roll::Running inner{running};
    CHECK(running == 2);
    throw 2;
  } catch (int) {
  }
  CHECK(running == 0);
}

// An aimed interaction costs the charge by how much rarer the type's own chance makes its kind.
void an_aimed_interaction_costs_by_the_types_chance() {
  using pm::roll::Aimed;
  using pm::roll::interaction_cost;
  CHECK(interaction_cost("vanilla.spirit", Aimed::Door) == 1.0f);
  CHECK(interaction_cost("", Aimed::Light) == 1.0f && interaction_cost("", Aimed::Prop) == 1.0f);
  CHECK(std::fabs(interaction_cost("vanilla.deogen", Aimed::Door) - 25.0f / 30.0f) < 1e-6f);
  CHECK(interaction_cost("vanilla.thaye", Aimed::Door) == interaction_cost("vanilla.deogen", Aimed::Door));
  CHECK(interaction_cost("vanilla.deildegast", Aimed::Door) == 2.5f);
  CHECK(interaction_cost("vanilla.deildegast", Aimed::Light) == 2.5f);
  CHECK(std::fabs(interaction_cost("vanilla.deildegast", Aimed::Prop) - 100.0f / 85.0f) < 1e-6f);
  CHECK(interaction_cost("vanilla.poltergeist", Aimed::Prop) == 5.0f);
  CHECK(interaction_cost("vanilla.poltergeist", Aimed::Door) == 1.0f);
}

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  the_idle_roll_tells_its_own_draws();
  a_chance_answers_only_its_own_roll();
  a_chances_scope_restores_on_unwind_and_nests();
  an_aimed_interaction_costs_by_the_types_chance();
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
