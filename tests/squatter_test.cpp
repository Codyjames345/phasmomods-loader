// Offline tests for the Squatter's rules (mods/custom_ghosts/squatter_rule.h).
// Run: build\squatter_test.exe
#include <cstdint>

#include "squatter_rule.h"

#include "check.h"

using namespace pm::custom::squatter;

namespace {

constexpr Phase kChaseStates[] = {Phase::Chasing, Phase::Searching, Phase::AtHidingPlace};
constexpr Phase kAllPhases[] = {Phase::Lurking,  Phase::Chasing,   Phase::Searching, Phase::AtHidingPlace,
                                Phase::LockedOut, Phase::Moping,   Phase::Excited,   Phase::Following};

// The facts of a frame that is not friendly, with `inside` living players in the house and nothing else going on.
Facts hostile(int inside = 1) {
  Facts f{};
  f.livingInside = inside;
  return f;
}

void lurks_until_it_sees_someone() {
  Facts f = hostile();
  CHECK(next_phase(Phase::Lurking, f) == Phase::Lurking);
  f.seesPlayer = true;
  CHECK(next_phase(Phase::Lurking, f) == Phase::Chasing);
  CHECK(first_phase(false) == Phase::Lurking);
}

void search_and_chase_alternate() {
  Facts f = hostile();
  f.tracksPlayer = true;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Chasing);
  f.tracksPlayer = false;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Searching);

  Facts seen = hostile();
  seen.seesPlayer = true;
  CHECK(next_phase(Phase::Searching, seen) == Phase::Chasing);
  Facts hiding = hostile();
  hiding.targetHiding = true;
  CHECK(next_phase(Phase::Searching, hiding) == Phase::AtHidingPlace);
  CHECK(next_phase(Phase::Searching, hostile()) == Phase::Searching);

  CHECK(next_phase(Phase::AtHidingPlace, hiding) == Phase::AtHidingPlace);
  CHECK(next_phase(Phase::AtHidingPlace, seen) == Phase::Chasing);
  CHECK(next_phase(Phase::AtHidingPlace, hostile()) == Phase::Searching);  // stepped out and out of sight
}

void door_locks_only_when_everyone_is_out() {
  for (const Phase p : kChaseStates) {
    CHECK(!doors_lock(p, hostile(1)));
    CHECK(!doors_lock(p, hostile(3)));
    CHECK(doors_lock(p, hostile(0)));
  }
  CHECK(!doors_lock(Phase::Lurking, hostile(0)));  // nobody was chased
}

void walking_back_in_keeps_the_door_open() {
  for (const Phase p : kChaseStates) {
    CHECK(!doors_lock(p, hostile(1)));
    CHECK(next_phase(p, hostile(1)) != Phase::LockedOut);
  }
  Facts f = hostile(1);
  f.seesPlayer = true;
  f.tracksPlayer = true;
  const Phase p = next_phase(Phase::Chasing, f);
  f.livingInside = 2;
  CHECK(!doors_lock(p, f));
  CHECK(next_phase(p, f) == Phase::Chasing);
}

void a_dropped_target_does_not_stop_the_door() {
  // The chased player disconnecting is only livingInside falling to 0.
  Facts f = hostile(1);
  f.seesPlayer = true;
  f.tracksPlayer = true;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Chasing);
  f.seesPlayer = false;
  f.tracksPlayer = false;
  f.livingInside = 0;
  CHECK(doors_lock(Phase::Chasing, f));
  CHECK(next_phase(Phase::Chasing, f) == Phase::LockedOut);
}

void chase_ends_locked_out_when_everyone_is_out() {
  for (const Phase p : kChaseStates) {
    const Facts f = hostile(0);
    CHECK(next_phase(p, f) == Phase::LockedOut);
    CHECK(doors_lock(p, f));
  }
}

void locked_out_is_final() {
  for (int mask = 0; mask < 16; ++mask) {
    for (const int inside : {0, 1, 4}) {
      Facts f{};
      f.seesPlayer = mask & 1;
      f.tracksPlayer = mask & 2;
      f.targetHiding = mask & 4;
      f.actFinished = mask & 8;
      f.livingInside = inside;
      CHECK(next_phase(Phase::LockedOut, f) == Phase::LockedOut);
      CHECK(!doors_lock(Phase::LockedOut, f));
    }
  }
}

void friendly_never_chases_or_kicks() {
  CHECK(first_phase(true) == Phase::Moping);
  Facts f{};
  f.friendly = true;
  f.livingInside = 1;
  CHECK(next_phase(Phase::Moping, f) == Phase::Moping);
  f.seesPlayer = true;
  CHECK(next_phase(Phase::Moping, f) == Phase::Excited);
  CHECK(next_phase(Phase::Excited, f) == Phase::Excited);  // the get-up clip has not ended
  f.actFinished = true;
  CHECK(next_phase(Phase::Excited, f) == Phase::Following);

  // Under every combination of facts a friendly ghost stays among the friendly phases, whatever phase
  // it is handed, and never locks a door.
  for (int mask = 0; mask < 16; ++mask) {
    for (const int inside : {0, 1, 3}) {
      Facts g{};
      g.friendly = true;
      g.seesPlayer = mask & 1;
      g.tracksPlayer = mask & 2;
      g.targetHiding = mask & 4;
      g.actFinished = mask & 8;
      g.livingInside = inside;
      for (const Phase p : kAllPhases) {
        const Phase n = next_phase(p, g);
        CHECK(n == Phase::Moping || n == Phase::Excited || n == Phase::Following);
        CHECK(!doors_lock(p, g));
      }
    }
  }
  for (const Phase p : kAllPhases) CHECK(!may_kick(p, true, true, 0.0f, 1.0f));
}

void kick_needs_a_living_target_in_reach() {
  CHECK(!may_kick(Phase::Chasing, false, false, 0.0f, 1.0f));
  CHECK(!may_kick(Phase::Chasing, false, true, 1.5f, 1.0f));
  CHECK(may_kick(Phase::Chasing, false, true, 0.5f, 1.0f));
  CHECK(!may_kick(Phase::Lurking, false, true, 0.5f, 1.0f));
  CHECK(!may_kick(Phase::AtHidingPlace, false, true, 0.5f, 1.0f));  // it cannot open the door
  CHECK(!may_kick(Phase::LockedOut, false, true, 0.5f, 1.0f));
}

void speed_stays_just_under_sprint() {
  for (const float s : {2.0f, 3.0f, 4.0f}) {
    CHECK(average_chase_speed(s) < s);
    CHECK(average_chase_speed(s) > 0.85f * s);
    CHECK(move_speed(s, Move::Stumble) == 0.0f);
    CHECK(move_speed(s, Move::Roll) > move_speed(s, Move::Run));
    CHECK(move_speed(s, Move::Duck) < move_speed(s, Move::Run));
  }
  // The cycle holds every move, Run most of all, and repeats; a negative time reads as the start.
  int seen[6] = {};
  for (float t = 0; t < kMoveCycleSeconds; t += 0.25f) ++seen[static_cast<int>(move_at(t))];
  for (const int n : seen) CHECK(n > 0);
  for (const int n : seen) CHECK(n <= seen[static_cast<int>(Move::Run)]);
  CHECK(move_at(kMoveCycleSeconds) == move_at(0.0f));
  CHECK(move_at(-3.0f) == move_at(0.0f));
}

void tired_taunt_fires_once_per_lapse() {
  const float cooled = kTauntCooldown + 1.0f;
  CHECK(tired_taunt_due(false, true, cooled));                   // the edge, after a cooldown
  CHECK(!tired_taunt_due(false, false, cooled));                 // still tired: no new edge
  CHECK(!tired_taunt_due(true, true, cooled));                   // can sprint
  CHECK(!tired_taunt_due(true, false, cooled));
  CHECK(!tired_taunt_due(false, true, kTauntCooldown - 1.0f));   // too soon after the last one
}

void lines_match_the_phase() {
  CHECK(lines_for(Phase::Lurking) == Lines::Lurk);
  CHECK(lines_for(Phase::Chasing) == Lines::Chase);
  CHECK(lines_for(Phase::Searching) == Lines::Search);
  CHECK(lines_for(Phase::AtHidingPlace) == Lines::Hiding);
  CHECK(lines_for(Phase::LockedOut) == Lines::Door);
  CHECK(lines_for(Phase::Moping) == Lines::Mope);
  CHECK(lines_for(Phase::Excited) == Lines::Excited);
  CHECK(lines_for(Phase::Following) == Lines::Friendly);
}

void picks_never_repeat_and_handle_nothing() {
  for (const uint32_t r : {0u, 1u, 7u, 0xFFFFFFFFu}) {
    CHECK(pick_line(0, -1, r) == -1);
    CHECK(pick_line(0, 3, r) == -1);
    CHECK(pick_line(1, 0, r) == 0);
    CHECK(pick_line(1, -1, r) == 0);
  }
  for (int count = 2; count <= 6; ++count) {
    for (int last = -1; last < count; ++last) {
      for (uint32_t r = 0; r < 40; ++r) {
        const int pick = pick_line(count, last, r);
        CHECK(pick >= 0 && pick < count);
        CHECK(pick != last);
      }
    }
  }
}

}  // namespace

int main() {
  lurks_until_it_sees_someone();
  search_and_chase_alternate();
  door_locks_only_when_everyone_is_out();
  walking_back_in_keeps_the_door_open();
  a_dropped_target_does_not_stop_the_door();
  chase_ends_locked_out_when_everyone_is_out();
  locked_out_is_final();
  friendly_never_chases_or_kicks();
  kick_needs_a_living_target_in_reach();
  speed_stays_just_under_sprint();
  tired_taunt_fires_once_per_lapse();
  lines_match_the_phase();
  picks_never_repeat_and_handle_nothing();
  return g_failures ? 1 : 0;
}
