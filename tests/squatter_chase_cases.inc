// The Squatter's chase rules (mods/custom_ghosts/squatter_chase.h), included into squatter_test.cpp.

// It hears a player as the hunt's voice test does: within 9 m on its own floor, or within 1000 m of a wished player on
// any floor, never one outdoors; only then is the player's loudness asked (asking clears it).
void hears_as_the_hunts_voice_test() {
  CHECK(within_hearing(8.9f, false, true, false));
  CHECK(!within_hearing(9.1f, false, true, false));
  CHECK(!within_hearing(2.0f, false, false, false));  // another floor
  CHECK(within_hearing(500.0f, true, false, false));  // wished: any floor, 1000 m
  CHECK(!within_hearing(1001.0f, true, true, false));
  CHECK(!within_hearing(1.0f, false, true, true));  // outdoors
  CHECK(!within_hearing(1.0f, true, true, true));   // outdoors, wished too
  CHECK(!within_hearing(std::numeric_limits<float>::quiet_NaN(), false, true, false));
}

// It goes for the nearest player it senses who is alive and inside; with nobody sensed it keeps the one it still
// tracks; a player hiding in a known place stays its target, and one it never saw go in is not sensed.
void the_target_is_the_nearest_sensed_player_inside() {
  const std::vector<Candidate> c = {
      {1, 6.0f, true, true, true, false},   // sensed, inside
      {2, 3.0f, true, true, true, false},   // nearer
      {3, 1.0f, false, true, true, false},  // dead
      {4, 0.5f, true, false, true, false},  // outside
      {5, 2.0f, true, true, false, false},  // not sensed
  };
  CHECK(pick_target(c, 1, true) == 2);
  CHECK(pick_target(c, -1, false) == 2);
  const std::vector<Candidate> none = {{5, 2.0f, true, true, false, false}};
  CHECK(pick_target(none, 5, true) == 5);    // still tracked
  CHECK(pick_target(none, 5, false) == -1);  // lost
  CHECK(pick_target(none, 9, true) == -1);   // tracked, but gone from the house
  const std::vector<Candidate> hiding = {{7, 4.0f, true, true, false, true}};
  CHECK(pick_target(hiding, 7, false) == 7);  // a known hiding place keeps its target
  const std::vector<Candidate> outside = {{8, 1.0f, true, false, false, false}};
  CHECK(pick_target(outside, 8, true) == -1);  // never past the exits
}

// It knows where a target is for kTrackMs after it last sensed them, then loses them.
void tracking_lapses_after_the_window() {
  Tracker t;
  CHECK(!t.tracks(0));
  t.sensed(3, {1, 0, 2}, 1000);
  CHECK(t.tracks(1000 + kTrackMs) && t.target() == 3);
  CHECK(!t.tracks(1001 + kTrackMs));
  CHECK(t.last_at() && t.last_at()->x == 1 && t.last_at()->z == 2);
  t.sensed(4, {5, 0, 5}, 2000);
  CHECK(t.target() == 4 && t.last_at()->x == 5);
  t.forget();
  CHECK(!t.tracks(2000) && t.target() == -1 && !t.last_at());
}

// A target in a hiding place is known there when it went in while tracked, or is sensed in it; once known it stays
// known while they stay in, and they are forgotten there once they step out.
void a_hiding_place_is_known_from_tracking() {
  CHECK(knows_hiding(true, true, false));
  CHECK(knows_hiding(true, false, true));
  CHECK(!knows_hiding(true, false, false));  // went in unseen
  CHECK(!knows_hiding(false, true, true));   // stepped out
}

// Lines come at a random gap between kLineGapMinMs and kLineGapMaxMs.
void lines_come_at_a_random_gap() {
  LineClock c;
  CHECK(c.due(0));
  c.spoke(0, 0);
  CHECK(!c.due(kLineGapMinMs - 1));
  CHECK(c.due(kLineGapMinMs));
  c.spoke(10000, 0xFFFFFFFFu);
  CHECK(!c.due(10000 + kLineGapMinMs));
  CHECK(c.due(10000 + kLineGapMaxMs));
  // Not chatty (user, 2026-11-10: lines played too often): at least 12 s between two, up to 24 s.
  CHECK(kLineGapMinMs == 12000 && kLineGapMaxMs == 24000);
}

// It is stuck when it moves at under kStuckFraction of the pace it wants for kStuckMs, which plays a bump.
void stuck_for_a_while_is_a_bump() {
  StuckClock s;
  CHECK(!s.update(0.1f, 5.0f, 0));
  CHECK(!s.update(0.1f, 5.0f, kStuckMs - 1));
  CHECK(s.update(0.1f, 5.0f, kStuckMs));
  CHECK(!s.update(0.1f, 5.0f, kStuckMs + 1));  // once per stall
  CHECK(!s.update(4.0f, 5.0f, kStuckMs + 2));  // moving again
  CHECK(!s.update(0.0f, 0.0f, 10 * kStuckMs));  // standing still on purpose is no stall
  CHECK(!s.update(0.1f, 5.0f, 20 * kStuckMs));
  CHECK(s.update(0.1f, 5.0f, 20 * kStuckMs + kStuckMs));
}

// A search point lies within kSearchRadius of where it last knew the target was, on the same height.
void search_points_stay_near_the_last_place() {
  const Point last{10, 2, -4};
  for (uint32_t roll : {0u, 1u, 12345u, 0xFFFFFFFFu, 77777u}) {
    const Point p = search_point(last, roll);
    const float d = std::hypot(p.x - last.x, p.z - last.z);
    CHECK(d <= kSearchRadius + 1e-4f && d >= 1.0f - 1e-4f);
    CHECK(p.y == last.y);
  }
  CHECK(search_point(last, 1).x != search_point(last, 2).x || search_point(last, 1).z != search_point(last, 2).z);
}

// Held still after moving (the trip's fall, a bump, a kick, a door), it stops where it stands rather than sliding to a
// halt (user, 2026-11-09: it moved along the floor after tripping); held still already, or moving on, nothing more.
void a_fall_stops_it_where_it_stands() {
  CHECK(stops_dead(4.0f, 0.0f));
  CHECK(!stops_dead(0.0f, 0.0f));
  CHECK(!stops_dead(4.0f, 3.0f));
  CHECK(!stops_dead(0.0f, 4.0f));
  CHECK(move_speed(4.0f, Move::Stumble) == 0.0f);  // the trip holds it still
}

// Its walking and running keep pace with the ground (user, 2026-11-09: research how games animate humanoids and
// copy it): each paced clip plays at the ground speed over the speed it travels on its own, the Run blend's own speed
// being the ground speed between Jogging's and Running's, within kMinPlayRate and kMaxPlayRate; nothing else is paced.
void walking_and_running_keep_pace_with_the_ground() {
  CHECK(std::fabs(play_rate(Move::Walk, kWalkSpeed) - 1.0f) < 1e-5f);
  CHECK(std::fabs(play_rate(Move::Walk, kWalkSpeed / 2) - 0.5f) < 1e-5f);
  CHECK(std::fabs(play_rate(Move::Sneak, kSneakSpeed * 1.5f) - 1.5f) < 1e-5f);
  CHECK(std::fabs(play_rate(Move::Run, 3.0f) - 1.0f) < 1e-5f);  // within the blend: the blend's own pace
  CHECK(std::fabs(play_rate(Move::Run, kRunSpeed * 1.2f) - 1.2f) < 1e-4f);  // past Running: played faster
  CHECK(std::fabs(play_rate(Move::Run, kJogSpeed / 2) - 0.5f) < 1e-5f);   // under Jogging: played slower
  CHECK(play_rate(Move::Walk, 100.0f) == kMaxPlayRate && play_rate(Move::Walk, 0.01f) == kMinPlayRate);
  CHECK(play_rate(Move::Walk, 0.0f) == 1.0f && play_rate(Move::Roll, 3.0f) == 1.0f && play_rate(Move::Stand, 1.0f) == 1.0f);
  CHECK(play_rate(Move::Walk, std::numeric_limits<float>::quiet_NaN()) == 1.0f);
  // The rolls, the trip and the jump carry it by their clips; a pace carries every other move.
  CHECK(moves_by_clip(Move::Roll) && moves_by_clip(Move::Stumble) && moves_by_clip(Move::Parkour));
  CHECK(!moves_by_clip(Move::Run) && !moves_by_clip(Move::Walk) && !moves_by_clip(Move::Sneak) &&
        !moves_by_clip(Move::Stand));
  // It searches at a sneak, which loops and stands when still, as a walk does.
  CHECK(std::string(states_of(Move::Sneak)[0]) == "Sneak" && !plays_once(Move::Sneak));
  CHECK(rest_move(Move::Sneak, 0.0f) == Move::Stand && rest_move(Move::Sneak, 1.0f) == Move::Sneak);
}

// Its head turns to the player it is after while it chases, waits at their hiding place, follows them or is excited
// to see them, and to nobody otherwise; the IK weights blend toward their goal a step at a time, never past it (user,
// 2026-11-09: copy how games animate humanoids).
void its_head_turns_to_the_player_it_is_after() {
  CHECK(look_target(Phase::Chasing, 3) == 3 && look_target(Phase::AtHidingPlace, 3) == 3);
  CHECK(look_target(Phase::Following, 7) == 7 && look_target(Phase::Excited, 7) == 7);
  CHECK(look_target(Phase::Lurking, 3) == 0 && look_target(Phase::Searching, 3) == 0 &&
        look_target(Phase::Moping, 3) == 0 && look_target(Phase::LockedOut, 3) == 0);
  CHECK(look_target(Phase::Chasing, -1) == 0 && look_target(Phase::Chasing, 0) == 0 &&
        look_target(Phase::Chasing, 256) == 0);
  CHECK(approach(0.0f, 1.0f, 0.25f) == 0.25f && approach(0.9f, 1.0f, 0.25f) == 1.0f);
  CHECK(approach(1.0f, 0.0f, 0.25f) == 0.75f && approach(0.1f, 0.0f, 0.25f) == 0.0f);
  CHECK(approach(0.5f, 1.0f, 0.0f) == 0.5f && approach(0.5f, 1.0f, std::numeric_limits<float>::quiet_NaN()) == 0.5f);
}

// Turning on the spot as games do it (user, 2026-11-09: integrate the turning clips): standing, it keeps its facing
// until its heading pulls kTurnStartDegrees away, then turns left or right by a clip, and past kTurnAroundDegrees it
// turns round; moving, it turns toward its heading at up to a rate, the shorter way round.
void it_turns_on_the_spot_by_its_clips() {
  constexpr float deg = std::numbers::pi_v<float> / 180.0f;
  CHECK(turn_for(30 * deg) == Turn::None && turn_for(-59 * deg) == Turn::None);
  CHECK(turn_for(70 * deg) == Turn::Right && turn_for(-70 * deg) == Turn::Left);
  CHECK(turn_for(150 * deg) == Turn::Around && turn_for(-170 * deg) == Turn::Around);
  CHECK(turn_for(350 * deg) == Turn::None);  // ten degrees the other way round
  CHECK(std::fabs(std::fabs(wrap_angle(3 * std::numbers::pi_v<float>)) - std::numbers::pi_v<float>) < 1e-4f);
  CHECK(wrap_angle(std::numeric_limits<float>::quiet_NaN()) == 0.0f);
  CHECK(std::fabs(turn_toward(0, 90 * deg, 10 * deg) - 10 * deg) < 1e-5f);
  CHECK(std::fabs(turn_toward(0, -90 * deg, 10 * deg) + 10 * deg) < 1e-5f);
  CHECK(std::fabs(turn_toward(0, 5 * deg, 10 * deg) - 5 * deg) < 1e-5f);  // never past it
  CHECK(std::fabs(wrap_angle(turn_toward(170 * deg, -170 * deg, 10 * deg) - 180 * deg)) < 1e-4f);  // across the back
  CHECK(turn_toward(1.0f, 2.0f, 0.0f) == 1.0f);
  // The clips each client plays of its own are in the bundle's table, and a jump runs on into its landing.
  for (const char* state : kLocalStates) CHECK(seconds_of(state) > 0.0f);
  // The jump runs on into the forward roll, and holds for both clips.
  CHECK(std::string(state_for(Move::Parkour, 7)) == "Parkour_A" && seconds_of("Parkour_A") > seconds_of("Roll_A"));
}

// The clumsy moves come from the way ahead (user, 2026-11-09): a dive at a close target first, then a jump down a drop or
// over a knee-high prop, a trip over an ankle-high one, a weave through a doorway or gap, and a duck past furniture,
// stepping away from it; nothing ahead, it runs. A duck or weave goes side-on so its sidestep follows its way.
void the_way_ahead_picks_its_clumsy_move() {
  Surroundings s;
  CHECK(move_for(s).move == Move::Run);
  s.furniture = 1;  // on its right: it steps left (Duck_A)
  CHECK(move_for(s).move == Move::Duck && move_for(s).variant == 0);
  s.furniture = -1;
  CHECK(move_for(s).move == Move::Duck && move_for(s).variant == 1);
  s.gap = true;
  CHECK(move_for(s).move == Move::Weave);
  s.ankleProp = true;
  CHECK(move_for(s).move == Move::Stumble);
  s.kneeProp = true;
  CHECK(move_for(s).move == Move::Parkour);
  s = Surroundings{};
  s.dropAhead = true;
  CHECK(move_for(s).move == Move::Parkour);
  // What lies at its feet comes before a dive at its target: it trips over the clutter between them, or jumps it, rather
  // than rolling through it (user, 2026-11-09: it rolled where it should have tripped).
  s.diveAt = true;
  CHECK(move_for(s).move == Move::Parkour);
  s = Surroundings{};
  s.diveAt = true;
  s.ankleProp = true;
  CHECK(move_for(s).move == Move::Stumble);
  s.ankleProp = false;
  CHECK(move_for(s).move == Move::Roll && std::string(state_for(Move::Roll, move_for(s).variant)) == "Roll_A");
  s.gap = true;  // a dive still comes before squeezing through a gap
  CHECK(move_for(s).move == Move::Roll);
  for (const Move m : {Move::Roll, Move::Parkour, Move::Stumble, Move::Duck, Move::Weave}) CHECK(moves_by_clip(m));
  constexpr float quarter = std::numbers::pi_v<float> / 2;
  CHECK(std::fabs(wrap_angle(move_facing(Move::Weave, 0, 0.3f) - (0.3f + quarter))) < 1e-5f);  // steps left: faces right
  CHECK(std::fabs(wrap_angle(move_facing(Move::Duck, 1, 0.3f) - (0.3f - quarter))) < 1e-5f);
  CHECK(move_facing(Move::Run, 0, 0.3f) == 0.3f && move_facing(Move::Roll, 1, -2.0f) == -2.0f);
}

// The door clip follows the leaf's swing (door::swings_toward): it pulls a door that comes toward it (Opening Door
// Inwards) and pushes one that goes away (Open Door Outwards).
void the_door_clip_follows_the_side() {
  CHECK(door_act(true) == Act::OpenDoorInwards);
  CHECK(door_act(false) == Act::OpenDoorOutwards);
}

// It only reaches a door within kDoorReach of its feet, on the flat.
void a_door_is_reached_within_reach() {
  CHECK(at_door({0, 0, 0}, {kDoorReach - 0.1f, 0.5f, 0}));
  CHECK(!at_door({0, 0, 0}, {kDoorReach + 0.1f, 0, 0}));
  CHECK(!at_door({0, 0, 0}, {0, 3.0f, 0}));  // another floor
}

// A player who can't sprint when the chase turns to them is taunted, as one whose sprint lapses is.
void a_target_who_cannot_sprint_is_taunted() {
  TauntBook b;
  CHECK(b.due(1, false, 0));          // can't sprint from the start (sprinting off)
  CHECK(!b.due(1, false, 1000));      // once per lapse
  CHECK(!b.due(1, true, 2000));       // sprinting again
  CHECK(!b.due(1, false, 3000));      // lapsed again, within the cooldown
  CHECK(!b.due(1, true, 4000));
  CHECK(b.due(1, false, 4000 + static_cast<uint64_t>(kTauntCooldown * 1000)));
  CHECK(!b.due(2, true, 0));          // a fresh target who can sprint
}

// Every client plays each new cue once; the first Shown a client takes joins the act under way (or plays the move)
// but never an old line.
void cues_play_once_and_never_replay_on_joining() {
  CuePlayer p;
  Shown s;
  s.lineSet = static_cast<int8_t>(Lines::Chase);
  s.lineIndex = 2;
  s.lineCue = 5;
  s.act = static_cast<int8_t>(Act::Sit);
  s.actCue = 3;
  Cues c = p.take(s);
  CHECK(c.act && !c.line && !c.move);
  c = p.take(s);
  CHECK(!c.move && !c.line && !c.act);
  s.lineCue = 6;
  c = p.take(s);
  CHECK(c.line && !c.act && !c.move);
  s.actCue = 4;
  c = p.take(s);
  CHECK(c.act && !c.line && !c.move);
  s.act = -1;  // the act is over: back to the move
  c = p.take(s);
  CHECK(c.move && !c.act);
  s.variant = 1;
  c = p.take(s);
  CHECK(c.move);
  p.reset();
  c = p.take(s);
  CHECK(c.move && !c.line && !c.act);
}

// Friendly mode (Task 9). On a friendly ghost's difficulty it starts seated (and sits while lurking or locked out).
void friendly_starts_seated_in_the_favourite_room() {
  CHECK(first_phase(true) == Phase::Moping);
  CHECK(stance_for(Phase::Moping) == Act::Sit);
  CHECK(!stance_for(Phase::Lurking) && !stance_for(Phase::LockedOut));  // they stand (Move::Stand, user 2026-11-09)
  CHECK(!stance_for(Phase::Chasing) && !stance_for(Phase::Following) && !stance_for(Phase::Excited));
  CHECK(steers(Phase::Moping));  // held where it sits, never wandering off
}

// On first sight it gets up, and its excited line comes once, at the start of getting up.
void gets_up_on_first_sight_and_says_its_line_once() {
  const Facts sees{true, true, false, false, 2, false};
  CHECK(next_phase(Phase::Moping, sees) == Phase::Excited);
  CHECK(entry_act(Phase::Moping, Phase::Excited) == Act::GetUp);
  CHECK(!entry_act(Phase::Lurking, Phase::Chasing));  // it lurks standing, so nothing to get up from
  CHECK(!entry_act(Phase::Chasing, Phase::Searching) && !entry_act(std::nullopt, Phase::Excited));
  CHECK(lines_for(Phase::Excited) == Lines::Excited);
  CHECK(kGetUpMs < kLineGapMinMs);  // the next line is due only after it has got up and moved on
  const Facts up{true, false, false, false, 2, true};
  CHECK(next_phase(Phase::Excited, up) == Phase::Following);
  CHECK(lines_for(Phase::Following) == Lines::Friendly);
}

// Following: after the nearest living player inside, stopping a step from them; never after one outside, so never past
// the exits; with nobody, where it last knew them.
void follows_the_nearest_living_inside_and_stops_at_exits() {
  const Point ghost{0, 0, 0};
  const Step far = follow_step(ghost, Point{5, 0, 0}, std::nullopt, std::nullopt);
  CHECK(far.to && far.to->x == 5 && far.pace == kFollowPace && !far.bang);
  const Step near = follow_step(ghost, Point{kFollowGap - 0.1f, 0, 0}, std::nullopt, std::nullopt);
  CHECK(near.pace == 0.0f && !near.bang);
  const Step lost = follow_step(ghost, std::nullopt, std::nullopt, Point{3, 0, 3});
  CHECK(lost.to && lost.to->x == 3 && lost.pace > 0.0f);
  const Step none = follow_step(ghost, std::nullopt, std::nullopt, std::nullopt);
  CHECK(none.pace == 0.0f);
  const std::vector<Candidate> c = {{1, 2.0f, true, false, true, false}, {2, 6.0f, true, true, true, false}};
  CHECK(pick_target(c, -1, false) == 2);  // the one outside is never followed
}

// At a hiding place a friendly Squatter waits at the door and talks, and never bangs.
void waits_and_talks_at_a_hiding_place_without_banging() {
  const Point ghost{0, 0, 0};
  const Step going = follow_step(ghost, Point{4, 0, 0}, Point{4, 0, 1}, std::nullopt);
  CHECK(going.to && going.to->z == 1 && going.pace == kFollowPace && !going.bang);
  const Step there = follow_step(ghost, Point{0.2f, 0, 0}, Point{0.2f, 0, 0.2f}, std::nullopt);
  CHECK(there.pace == 0.0f && !there.bang);
  CHECK(next_phase(Phase::Following, Facts{true, false, false, true, 2, true}) == Phase::Following);
  CHECK(!doors_lock(Phase::Following, Facts{true, false, false, false, 0, true}));
  CHECK(!may_kick(Phase::Following, true, true, 0.1f, kKickReach));
}

// The kick's reach is measured in three dimensions, as the hunt's kill measures it: never through a floor or ceiling.
void the_kick_never_reaches_through_a_floor() {
  CHECK(within_kick({0, 0, 0}, {kKickReach - 0.1f, 0, 0}));
  CHECK(!within_kick({0, 0, 0}, {kKickReach + 0.1f, 0, 0}));
  CHECK(!within_kick({0, 0, 0}, {0.3f, 2.8f, 0.2f}));  // the floor above, right over it
  CHECK(!within_kick({0, 0, 0}, {0, std::numeric_limits<float>::quiet_NaN(), 0}));
}

// With no last place for its target (after a kick, a death, a disconnect or a new master) it searches around itself
// rather than standing still.
void with_no_last_place_it_searches_around_itself() {
  CHECK(search_origin(std::nullopt, Point{3, 1, 4}).x == 3 && search_origin(std::nullopt, Point{3, 1, 4}).z == 4);
  CHECK(search_origin(Point{7, 0, 7}, Point{3, 1, 4}).x == 7);
}

// While its figure cannot be shown, it neither starts a chase nor kicks: nobody is chased by what they cannot see.
void an_unseen_figure_never_starts_a_chase() {
  CHECK(sees_for_phase(Phase::Lurking, true, true, true) && !sees_for_phase(Phase::Lurking, true, true, false));
  CHECK(sees_for_phase(Phase::Moping, true, false, true) && !sees_for_phase(Phase::Moping, true, false, false));
  CHECK(sees_for_phase(Phase::Chasing, false, true, true) && !sees_for_phase(Phase::Chasing, true, false, true));
  CHECK(!sees_for_phase(Phase::Chasing, true, true, false));
}

// A step plays as each foot plants in the figure's own clip (user, 2026-11-10: the steps did not match the
// animation): a foot that has lifted kFootLift above the lowest it has been comes back down within kFootPlant of it.
// A foot that never lifts (standing, sliding) plays nothing, and a jitter under the lift plays nothing.
void a_step_plays_as_each_foot_plants() {
  constexpr float kPi = 3.14159265f;
  FootPlants feet;
  int steps = 0;
  // A walk cycle of a second: each foot lifts 0.15 m once, half a cycle apart, over a planted height of 0.1 m.
  for (int i = 0; i <= 300; ++i) {
    const float t = static_cast<float>(i) / 100.0f;
    const auto lift = [&](float phase) { return 0.1f + 0.15f * std::max(0.0f, std::sin(2.0f * kPi * (t + phase))); };
    steps += feet.update(lift(0.0f), lift(0.5f), t);
  }
  CHECK(steps >= 5 && steps <= 6);  // two a second for three seconds, the first plant of each foot arming it
  FootPlants still;
  int idle = 0;
  for (int i = 0; i <= 300; ++i) {
    const float t = static_cast<float>(i) / 100.0f;
    const float jitter = 0.1f + 0.4f * kFootLift * std::sin(13.0f * t);
    idle += still.update(jitter, 0.1f, t);
  }
  CHECK(idle == 0);
  FootPlants once;
  CHECK(once.update(0.1f, 0.1f, 0.0f) == 0);
  CHECK(once.update(0.1f + kFootLift + 0.01f, 0.1f, 0.1f) == 0);  // lifted: armed
  CHECK(once.update(0.1f + kFootPlant / 2, 0.1f, 0.2f) == 1);     // down again: a step
  CHECK(once.update(0.1f, 0.1f, 0.3f) == 0);                       // and only one
  once.reset();
  CHECK(once.update(0.1f + kFootPlant / 2, 0.1f, 0.4f) == 0);     // forgotten: not armed
  CHECK(kFootPlant < kFootLift);
  FootPlants broken;  // a height that is not a number plays nothing and arms nothing
  CHECK(broken.update(std::numeric_limits<float>::quiet_NaN(), 0.1f, 0.0f) == 0);
  // Each step plays the players' own sound for the surface under it (user, 2026-11-09): each tag a player's footsteps
  // know names its surface, and any other tag none.
  CHECK(surface_of("Carpet") == 0 && surface_of("Heavyconcrete") == 12 && surface_of("Stairs") == kStairsSurface);
  CHECK(std::string(kStepSurfaces[kStairsSurface]) == "Stairs");
  CHECK(surface_of("Untagged") == -1 && surface_of("") == -1 && surface_of("carpet") == -1 && surface_of("Dirt") == -1);
  for (int i = 0; i < static_cast<int>(std::size(kStepSurfaces)); ++i) CHECK(surface_of(kStepSurfaces[i]) == i);
}

// A moping Squatter never sits inside a hiding place the difficulty closed (a blocked pantry on Willow Street, user
// 2026-11-09): it moves to the closest place outside it, nearest side first.
void moping_moves_to_the_closest_place_outside_a_closed_hiding_place() {
  CHECK(inside_box({1, 0, 1}, {0, 0, 0}, {2, 2, 2}));
  CHECK(!inside_box({3, 0, 0}, {0, 0, 0}, {2, 2, 2}));
  CHECK(!inside_box({0, 3, 0}, {0, 0, 0}, {2, 2, 2}));
  const Point centre{0, 0, 0}, extents{2, 2, 1};
  const std::vector<Point> out = ways_out({1.5f, 0.3f, 0.2f}, centre, extents);
  CHECK(out.size() == 4);
  if (out.size() != 4) return;
  // The near side: +x at 2.5 is 1.0 m away, +z at 1.5 is 1.3 m.
  CHECK(std::fabs(out[0].x - (2 + kOutsideMargin)) < 1e-4f && out[0].z == 0.2f && out[0].y == 0.3f);
  float last = 0.0f;
  for (const Point& p : out) {
    CHECK(!inside_box(p, centre, extents));
    const float d = std::hypot(p.x - 1.5f, p.z - 0.2f);
    CHECK(d >= last);
    last = d;
  }
}

// The user's standing and walking clips (2026-11-09): it stands rather than running on the spot, and walks to search,
// follow and find a seat.
void it_stands_still_and_walks_with_its_own_clips() {
  CHECK(std::string(states_of(Move::Stand)[0]) == "Stand" && std::string(states_of(Move::Walk)[0]) == "Walk");
  CHECK(move_speed(5.0f, Move::Stand) == 0.0f);
  CHECK(move_speed(5.0f, Move::Walk) > 0.0f && move_speed(5.0f, Move::Walk) < move_speed(5.0f, Move::Run));
  CHECK(rest_move(Move::Run, 0.0f) == Move::Stand && rest_move(Move::Walk, 0.0f) == Move::Stand);
  CHECK(rest_move(Move::Run, 3.0f) == Move::Run && rest_move(Move::Walk, 1.0f) == Move::Walk);
  CHECK(rest_move(Move::Stumble, 0.0f) == Move::Stumble);  // a clumsy move plays its own clip, whatever its pace
  CHECK(move_for({}).move == Move::Run);  // the chase runs where nothing calls for a clumsy move, never strolls
}

// A door counts only when it walks into it, never one it passes beside or leaves behind.
void it_opens_only_the_door_it_walks_into() {
  CHECK(heading_into({0, 0, 0}, {1, 0, 0}, {2, 0, 0.1f}));
  CHECK(!heading_into({0, 0, 0}, {1, 0, 0}, {-2, 0, 0}));       // walking away
  CHECK(!heading_into({0, 0, 0}, {1, 0, 0}, {0, 0, 2}));        // passing beside
  CHECK(!heading_into({0, 0, 0}, {1, 0, 0}, {0.1f, 0, 0}));     // standing still
}

// The tunables keep the chase sane: a search is slower than the chase, and pauses are short.
void the_chase_tunables_are_sane() {
  CHECK(kSearchPace > 0.0f && kSearchPace < 1.0f);
  CHECK(kBumpPauseMs > 0 && kClumsyGapMs > 0 && kSenseMs > 0 && kDiveMin < kDiveMax);
  CHECK(kBangGapMs > 0 && kDoorPauseMs > 0 && kGetUpMs > 0 && kSightMs > 0 && kShownResendMs > 0);
}

// A roll, a jump, a trip, a duck or a weave carries the figure by its clip the way the figure faces, so one begins only
// once the figure faces the way it runs (user, 2026-11-09: it rolled before it had turned to the player, away from them,
// then came back).
void a_clumsy_move_waits_until_it_faces_its_way() {
  constexpr float deg = std::numbers::pi_v<float> / 180.0f;
  CHECK(faces_ahead(0.0f, 0.0f));
  CHECK(faces_ahead(0.0f, (kFaceAheadDegrees - 1.0f) * deg));
  CHECK(faces_ahead(0.0f, -(kFaceAheadDegrees - 1.0f) * deg));
  CHECK(!faces_ahead(0.0f, (kFaceAheadDegrees + 1.0f) * deg));
  CHECK(!faces_ahead(0.0f, 180.0f * deg));
  CHECK(faces_ahead(179.0f * deg, -179.0f * deg));  // across the wrap
  CHECK(!faces_ahead(std::numeric_limits<float>::quiet_NaN(), 0.0f));
  CHECK(kFaceAheadDegrees > 0.0f && kFaceAheadDegrees <= kDiveConeDegrees);
}

// Its kick at a hiding place sounds as the door being slammed, as the door's own slam RPC picks the sound
// (Door.PlayDoorSlamNoise): one of the door's slam clips, or the game's shared ones when the door has none, at the
// game's volume. Every client plays it from the kick's cue, at the moment the foot lands.
void a_kick_at_a_hiding_place_sounds_its_door() {
  const std::optional<SlamPick> own = slam_pick(3, 5, 7);
  CHECK(own && own->own && own->index == 7 % 3);
  const std::optional<SlamPick> shared = slam_pick(0, 5, 7);
  CHECK(shared && !shared->own && shared->index == 7 % 5);
  CHECK(!slam_pick(0, 0, 7));
  CHECK(kSlamVolume == 0.15f);
  CHECK(kSlamDelayMs > 0 && kSlamDelayMs < kBangGapMs);
}

// It reaches a player in a hiding place it has a way into as the game's hunt does: once it stands at the closet's spot
// (ClosetZone.navSpot on the mesh), where the hunt's closet search kills whoever hides there, or within its kick's
// reach of them; never from another floor.
void it_reaches_a_hider_at_the_closets_spot() {
  const Point spot{0, 0, 0}, hider{0, 0, 2.0f};
  CHECK(at_spot({0.5f, 0, 0}, spot));
  CHECK(!at_spot({kStandReach + 0.1f, 0, 0}, spot));
  CHECK(!at_spot({0, kDoorRise + 0.5f, 0}, spot));  // another floor
  CHECK(!at_spot({0, 0, 0}, std::nullopt));
  CHECK(reaches_hider({0.5f, 0, 0}, hider, spot));                  // at the spot, 2 m from them
  CHECK(!reaches_hider({0, 0, -3.0f}, hider, spot));                // on its way
  CHECK(reaches_hider({0, 0, 1.2f}, hider, std::nullopt));         // within the kick
  CHECK(!reaches_hider({0, 0, -3.0f}, hider, std::nullopt));
}

// A closet's inside is kept off the ghost's routes by its own obstacle (ClosetZone.internalAreaObstacle), so it opens
// the obstacle (DynamicNavMeshObstacle.targetActivity cleared) while it walks in for a hider, and puts it back only once
// it no longer goes in and stands clear of the obstacle, so the game never switches it on under it.
void a_closets_obstacle_opens_only_while_it_goes_in() {
  CHECK(keeps_closet_open(true, false) && keeps_closet_open(true, true));
  CHECK(keeps_closet_open(false, true));  // done, but still inside: not yet
  CHECK(!keeps_closet_open(false, false));
}

// A door line is said at the exit nearest the listener, loud within kDoorLineFullVolume, so a player shut outside hears
// it through the door (user, 2026-11-09).
void a_door_line_is_said_at_the_nearest_exit() {
  const std::vector<Point> exits = {{0, 0, 0}, {10, 0, 0}, {4, 3, 0}};
  CHECK(nearest_point(exits, {9, 0, 1}) == 1);
  CHECK(nearest_point(exits, {4, 0, 0}) == 2);  // on the flat: height does not count
  CHECK(nearest_point({}, {0, 0, 0}) == -1);
  CHECK(kDoorLineFullVolume >= 5.0f);
}

// How fast the ghost moves on this client, for the clips' pace. A frame in which no time passes keeps its distance for
// the next (user, 2026-11-09: the walk played too slowly and the feet slid; frames inside one tick of a coarse clock
// dropped their distance, so the pace read low).
void the_pace_keeps_the_distance_of_a_frame_with_no_time() {
  PaceMeter split, whole;
  split.update({0, 0, 0}, 0.0);
  whole.update({0, 0, 0}, 0.0);
  split.update({0.05f, 0, 0}, 0.0);  // a frame inside the same tick
  const float a = split.update({0.1f, 0, 0}, 0.1);
  const float b = whole.update({0.1f, 0, 0}, 0.1);
  CHECK(std::abs(a - b) < 1e-6f && a > 0.0f);
  // Held long enough, it reads the true speed: 1 m/s.
  PaceMeter steady;
  for (int i = 0; i <= 200; ++i) steady.update({0.01f * i, 0, 0}, 0.01 * i);
  CHECK(std::abs(steady.pace() - 1.0f) < 0.01f);
  steady.update({100, 0, 0}, 1.0);  // time went backwards: ignored, nothing read
  CHECK(std::abs(steady.pace() - 1.0f) < 0.01f);
  steady.reset();
  CHECK(steady.pace() == 0.0f);
}

// The way ahead is read across the body's width, as its feet sweep it, not down one line (user, 2026-11-09: it never
// tripped; a ray down its middle missed what its feet met). The rays are spread evenly, symmetric about its line.
void the_way_ahead_is_read_across_its_width() {
  CHECK(kSweepRays >= 3 && kSweepRays % 2 == 1);  // one down the middle
  CHECK(kKneeHeight < kChestHeight);
  CHECK(std::abs(sweep_offset(0) + kSweepHalfWidth) < 1e-6f);
  CHECK(std::abs(sweep_offset(kSweepRays - 1) - kSweepHalfWidth) < 1e-6f);
  CHECK(std::abs(sweep_offset(kSweepRays / 2)) < 1e-6f);
  for (int i = 0; i < kSweepRays; ++i) CHECK(std::abs(sweep_offset(i) + sweep_offset(kSweepRays - 1 - i)) < 1e-6f);
  // What called a clumsy move, for the log: the same order move_for picks in.
  Surroundings s;
  CHECK(clumsy_reason(s) == nullptr);
  s.furniture = 1;
  CHECK(std::string(clumsy_reason(s)) == "furniture beside");
  s.ankleProp = true;
  CHECK(std::string(clumsy_reason(s)) == "its feet met clutter" && move_for(s).move == Move::Stumble);
  s.kneeProp = true;
  CHECK(std::string(clumsy_reason(s)) == "drop or knee-high prop" && move_for(s).move == Move::Parkour);
  s.diveAt = true;  // at its feet first
  CHECK(std::string(clumsy_reason(s)) == "drop or knee-high prop" && move_for(s).move == Move::Parkour);
  s = Surroundings{};
  s.diveAt = true;
  CHECK(std::string(clumsy_reason(s)) == "dive at the target" && move_for(s).move == Move::Roll);
  // The log lists every condition it read, not only the one that won.
  s.ankleProp = true;
  s.furniture = -1;
  CHECK(sensed_list(s) == "feet, dive, furniture left");
  CHECK(sensed_list(Surroundings{}).empty());
}

// It trips when its feet meet clutter, as a body does, not when a ray ahead finds some (user, 2026-11-09: it never
// tripped, and was not to be bandaged): a box around its feet, as wide as its stance, from under the navigation mesh it
// stands on (which may lie above the floor) to its shins, reaching a stride ahead and a little behind, turned to the
// way it moves.
void its_feet_meet_what_it_trips_over() {
  constexpr float pi = std::numbers::pi_v<float>;
  const FootBox north = foot_box({1, 0, 2}, 0.0f);  // heading +z
  CHECK(std::abs(north.half.x - kFootHalfWidth) < 1e-6f);
  CHECK(std::abs(north.half.z - (kFootAhead + kFootBehind) / 2) < 1e-6f);
  CHECK(std::abs(north.centre.x - 1.0f) < 1e-5f);
  CHECK(std::abs(north.centre.z - (2.0f + (kFootAhead - kFootBehind) / 2)) < 1e-5f);  // more ahead than behind
  CHECK(std::abs(north.centre.y - (kFootHeight - kFootBelow) / 2) < 1e-5f);  // under its feet to shin height
  CHECK(std::abs(north.half.y - (kFootHeight + kFootBelow) / 2) < 1e-5f);
  CHECK(std::abs(north.centre.y - north.half.y + kFootBelow) < 1e-5f);    // its bottom kFootBelow under the feet
  const FootBox east = foot_box({0, 0, 0}, pi / 2);  // heading +x
  CHECK(std::abs(east.centre.x - (kFootAhead - kFootBehind) / 2) < 1e-5f && std::abs(east.centre.z) < 1e-5f);
  CHECK(std::abs(east.yaw - pi / 2) < 1e-6f);
  // Low enough for a shoe (about 0.1 m) and equipment lying flat, wherever the mesh lies over the floor.
  CHECK(kFootBelow >= 0.1f && kFootHeight >= 0.15f && kFootHeight < kKneeHeight);
  // Its other lines carry only a little way; the door lines carry further (a_door_line_is_said_at_the_nearest_exit).
  CHECK(kVoiceFullVolume > 0.0f && kVoiceFullVolume < kVoiceRange && kVoiceRange < 15.0f);
}

// One line at a time (user, 2026-11-09: the door lines played over each other): a line waits until the one playing has
// finished, at most kVoiceQueueMax wait, and one that has waited kVoiceWaitMs is dropped, so the voice never runs behind
// what is happening.
void voice_lines_never_talk_over_each_other() {
  CHECK(kVoiceQueueMax >= 1 && kVoiceQueueMax <= 3 && kVoiceWaitMs > 0);
  VoiceQueue q;
  CHECK(!q.next(0));
  q.push({Lines::Door, 1}, 0);
  const std::optional<QueuedLine> first = q.next(0);
  CHECK(first && first->set == Lines::Door && first->index == 1);
  q.playing(0, 2000);           // a 2 s line
  q.push({Lines::Door, 2}, 100);
  CHECK(!q.next(1999));         // waits for the line playing
  const std::optional<QueuedLine> second = q.next(2000);
  CHECK(second && second->index == 2);
  q.playing(2000, 1000);
  // A burst: only kVoiceQueueMax wait, the newest kept.
  for (int i = 0; i < 5; ++i) q.push({Lines::Door, 10 + i}, 2100);
  CHECK(q.waiting() == static_cast<size_t>(kVoiceQueueMax));
  const std::optional<QueuedLine> after = q.next(3000);
  CHECK(after && after->index == 10 + 5 - kVoiceQueueMax);
  // One that waited too long is dropped, not played late.
  VoiceQueue late;
  late.push({Lines::Chase, 0}, 0);
  late.playing(0, 10000);
  late.push({Lines::Chase, 1}, 100);
  CHECK(!late.next(100 + kVoiceWaitMs + 1) && late.waiting() == 0);
  late.reset();
  CHECK(!late.next(0) && late.waiting() == 0);
}

// A door line is said from just behind the door, its far side from the listener: inside, to a player shut outside.
void a_door_line_comes_from_behind_the_door() {
  const Point door{0, 1, 0};
  const Point behind = behind_door(door, {0, 0, -3});  // the listener south of it
  CHECK(std::abs(behind.z - kBehindDoor) < 1e-5f && std::abs(behind.x) < 1e-5f && behind.y == 1.0f);
  const Point same = behind_door(door, door);  // a listener on the door: the door itself
  CHECK(same.x == 0.0f && same.z == 0.0f);
  CHECK(kBehindDoor > 0.0f && kBehindDoor < 2.0f);
}

// A fall settles once its bodies have slowed below kSettleSpeed for kSettleMs, or after kFallMaxMs whatever they do,
// and never before; it is up again once the get-up's clip has played.
void a_fall_settles_then_gets_up() {
  FallState f;
  CHECK(f.phase() == FallPhase::Up);
  CHECK(f.update(5, 0.0f, 1000) == FallPhase::Up);  // nothing falls before a fall begins
  f.begin(1000);
  CHECK(f.phase() == FallPhase::Falling);
  CHECK(f.update(1100, 3.0f, 1000) == FallPhase::Falling);
  CHECK(f.update(1200, kSettleSpeed / 2, 1000) == FallPhase::Falling);  // slow, but not for long enough
  CHECK(f.update(1200 + kSettleMs - 1, kSettleSpeed / 2, 1000) == FallPhase::Falling);
  CHECK(f.update(1200 + kSettleMs, kSettleSpeed / 2, 1000) == FallPhase::GettingUp);
  const uint64_t rose = 1200 + kSettleMs;
  CHECK(f.update(rose + 999, 0.0f, 1000) == FallPhase::GettingUp);
  CHECK(f.update(rose + 1000, 0.0f, 1000) == FallPhase::Up);
  // A bounce restarts the settling.
  f.begin(0);
  CHECK(f.update(100, 0.0f, 1000) == FallPhase::Falling);
  CHECK(f.update(200, 1.0f, 1000) == FallPhase::Falling);  // the bounce
  CHECK(f.update(250, 0.0f, 1000) == FallPhase::Falling);  // still again from here
  CHECK(f.update(100 + kSettleMs, 0.0f, 1000) == FallPhase::Falling);
  CHECK(f.update(250 + kSettleMs - 1, 0.0f, 1000) == FallPhase::Falling);
  CHECK(f.update(250 + kSettleMs, 0.0f, 1000) == FallPhase::GettingUp);
  // A body that never settles (wedged, rocking) gets up at the time limit.
  f.begin(0);
  CHECK(f.update(kFallMaxMs - 1, 2.0f, 1000) == FallPhase::Falling);
  CHECK(f.update(kFallMaxMs, 2.0f, 1000) == FallPhase::GettingUp);
  f.reset();
  CHECK(f.phase() == FallPhase::Up);
  CHECK(kSettleSpeed > 0.0f && kSettleMs < kFallMaxMs && kRecoverSeconds > 0.0f);
}

// Face down gets up with Standing Up, face up with Getting Up; a body on its side keeps the choice it had, so it never
// flips between them (Review Focus 5).
void the_get_up_follows_how_it_landed() {
  CHECK(get_up_for({0, -1, 0}, GetUp::FaceUp) == GetUp::FaceDown);
  CHECK(get_up_for({0, 1, 0}, GetUp::FaceDown) == GetUp::FaceUp);
  CHECK(get_up_for({0.9f, -kSideBand - 0.01f, 0}, GetUp::FaceUp) == GetUp::FaceDown);
  CHECK(get_up_for({0.9f, -kSideBand + 0.01f, 0}, GetUp::FaceUp) == GetUp::FaceUp);    // on its side: kept
  CHECK(get_up_for({0.9f, kSideBand - 0.01f, 0}, GetUp::FaceDown) == GetUp::FaceDown);  // on its side: kept
  CHECK(get_up_for({1, 0, 0}, GetUp::FaceDown) == GetUp::FaceDown);
  CHECK(get_up_for({1, 0, 0}, GetUp::FaceUp) == GetUp::FaceUp);
  CHECK(std::string(get_up_state(GetUp::FaceDown)) == kStandUpState);
  CHECK(std::string(get_up_state(GetUp::FaceUp)) == kTripGetUpState);
}

// It gets up where it lies: the root at the hips on the flat, turned so the get-up clip's own head lies along the
// landed body's hips to head.
void it_gets_up_where_it_lies() {
  constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;
  const RootPose down = get_up_root({2, 0.2f, 3}, {2, 0.25f, 4.5f}, GetUp::FaceDown);  // head toward +z
  CHECK(down.at.x == 2.0f && down.at.y == 0.2f && down.at.z == 3.0f);
  CHECK(std::abs(wrap_angle(down.yaw - (0.0f - kGetUpHeadYaw[0] * kDeg))) < 1e-4f);
  const RootPose up = get_up_root({0, 0, 0}, {1, 0, 0}, GetUp::FaceUp);  // head toward +x: 90 degrees clockwise
  CHECK(std::abs(wrap_angle(up.yaw - (90.0f * kDeg - kGetUpHeadYaw[1] * kDeg))) < 1e-4f);
  // A head straight above the hips (it never lies that way, but a wedged body might): no turn is read from it.
  const RootPose standing = get_up_root({0, 0, 0}, {0, 1, 0}, GetUp::FaceDown);
  CHECK(std::isfinite(standing.yaw));
}

// What knocks it over: a trip and a drop always; a bump, a door's swing and a thrown object only from their speed up.
void what_knocks_it_over() {
  CHECK(knocks_over(FallCause::Trip, 0.0f) && knocks_over(FallCause::Drop, 0.0f));
  const std::pair<FallCause, float> paced[] = {
      {FallCause::Bump, kBumpFallPace}, {FallCause::Door, kDoorKnockSpeed}, {FallCause::Thrown, kThrownKnockSpeed}};
  for (const auto& [cause, speed] : paced) {
    CHECK(!knocks_over(cause, speed - 0.01f));
    CHECK(knocks_over(cause, speed));
    CHECK(knocks_over(cause, speed + 0.01f));
    CHECK(!knocks_over(cause, std::numeric_limits<float>::quiet_NaN()));
  }
  CHECK(kBumpFallPace > 0.0f && kDoorKnockReach > 0.0f && kThrownReach > 0.0f);
  // Each cause pushes it its own way and no harder than the ragdoll's cap; a trip and a drop only carry its run.
  for (int cause = 0; cause < kFallCauseValues; ++cause) CHECK(kFallPush[cause] >= 0.0f && kFallPush[cause] <= 6.0f);
  CHECK(kFallPush[static_cast<int>(FallCause::Trip)] == 0.0f && kFallPush[static_cast<int>(FallCause::Drop)] == 0.0f);
  const Point push = fall_push(FallCause::Door, 0.0f);  // heading +z
  CHECK(std::abs(push.z - kFallPush[static_cast<int>(FallCause::Door)]) < 1e-5f && std::abs(push.x) < 1e-5f);
}

// A knock never lands while it is down, mid-act or already falling: no fall stacks on another or restarts it
// (Review Focus 2).
void a_fall_never_stacks() {
  CHECK(may_fall(Move::Run, -1, false));
  CHECK(may_fall(Move::Stumble, -1, false));  // a trip's own fall comes mid-clip
  CHECK(!may_fall(Move::Run, -1, true));       // down
  CHECK(!may_fall(Move::Fall, -1, false));     // already falling
  CHECK(!may_fall(Move::Run, static_cast<int8_t>(Act::Kick), false));
  CHECK(!may_fall(Move::Run, static_cast<int8_t>(Act::OpenDoorInwards), false));
  CHECK(!may_fall(Move::Run, static_cast<int8_t>(Act::Bang), false));
}

// A door's leaf meets the body at the point of its hinge-to-edge line nearest the chest, on the flat.
void a_leaf_meets_the_body_at_its_nearest_point() {
  const Point hinge{0, 0, 0}, edge{1, 2, 0};
  const Point mid = nearest_on_flat(hinge, edge, {0.4f, 1.2f, 0.5f});
  CHECK(std::abs(mid.x - 0.4f) < 1e-6f && mid.z == 0.0f && mid.y == 1.2f);
  CHECK(nearest_on_flat(hinge, edge, {-3, 0, 1}).x == 0.0f);  // past the hinge: the hinge
  CHECK(nearest_on_flat(hinge, edge, {5, 0, -1}).x == 1.0f);  // past the edge: the edge
  CHECK(nearest_on_flat(hinge, hinge, {5, 0, 5}).x == 0.0f);  // no length: the hinge
}

// A clip that carries the body is bent to land where the world needs it (motion warping): each frame's step turns toward
// the target and stretches or shrinks so the clip's remaining travel reaches it, within limits; with no travel left the
// step plays as recorded.
void a_move_lands_where_the_world_needs_it() {
  const Point ahead = warp_step({0, 0, 0.1f}, 2.0f, {2.0f, 0, 0});  // the target due +x, as far as the clip has left
  CHECK(std::abs(ahead.x - 0.1f) < 1e-5f && std::abs(ahead.z) < 1e-5f);
  const Point farther = warp_step({0, 0, 0.1f}, 2.0f, {0, 0, 2.6f});  // 1.3 times the travel left: stretched
  CHECK(std::abs(farther.z - 0.13f) < 1e-5f);
  const Point nearer = warp_step({0, 0, 0.1f}, 2.0f, {0, 0, 1.6f});  // 0.8 times: shrunk
  CHECK(std::abs(nearer.z - 0.08f) < 1e-5f);
  CHECK(std::abs(warp_step({0, 0, 0.1f}, 2.0f, {0, 0, 10.0f}).z - 0.1f * kWarpMaxScale) < 1e-5f);  // clamped
  CHECK(std::abs(warp_step({0, 0, 0.1f}, 2.0f, {0, 0, 0.1f}).z - 0.1f * kWarpMinScale) < 1e-5f);
  const Point asIs = warp_step({0.03f, 0, 0.1f}, 0.0f, {5, 0, 0});  // no travel left
  CHECK(asIs.x == 0.03f && asIs.z == 0.1f);
  const Point noWay = warp_step({0.03f, 0, 0.1f}, 2.0f, {0, 0, 0});  // on the target already
  CHECK(noWay.x == 0.03f && noWay.z == 0.1f);
  CHECK(kWarpMinScale == 0.5f && kWarpMaxScale == 1.5f);
}

// Every clip that carries the body has its travel, as the bundle's build measured it, but the trips (their ragdoll
// takes them) and the jump (its landing is a drop's or a prop's far side, as recorded).
void the_clips_travel_as_built() {
  for (const StateTravel& t : kClipTravel) CHECK(t.metres > 0.0f && seconds_of(t.state) > 0.0f);
  for (const Move m : kAllMoves) {
    if (!moves_by_clip(m) || m == Move::Stumble || m == Move::Parkour) continue;
    for (const char* s : states_of(m)) CHECK(travel_of(s) > 0.0f);
  }
  CHECK(std::abs(travel_of("Roll_A") - 5.52f) < 1e-4f && travel_of("Run") == 0.0f && travel_of(nullptr) == 0.0f);
}

// A jump down a drop falls as its take-off ends, where the bundle runs it on into a roll: the ragdoll tumbles down the
// drop in place of the roll, never after it.
void a_drop_falls_as_the_jump_lands() {
  CHECK(kJumpTakeOffSeconds > 0.0f && kJumpTakeOffSeconds < seconds_of(state_for(Move::Parkour, 0)));
  CHECK(std::abs(kJumpTakeOffSeconds + seconds_of("Roll_A") - seconds_of(state_for(Move::Parkour, 0))) < 1e-3f);
}

// Its steps play as loud as a player's on the same surface and at the same pace (FootstepController.AttemptPlaySound's
// volumes, decompiled): walking on concrete 0.4, running 0.3; old wood 0.05 and 0.075; stairs 0.6 either way. Never
// louder than 1, and nothing for a surface no step knows.
void its_steps_are_as_loud_as_a_players() {
  CHECK(step_volume(surface_of("Concrete"), false) == 0.4f && step_volume(surface_of("Concrete"), true) == 0.3f);
  CHECK(step_volume(surface_of("OldWood"), false) == 0.05f && step_volume(surface_of("OldWood"), true) == 0.075f);
  CHECK(step_volume(surface_of("Grass"), false) == 0.6f && step_volume(surface_of("Grass"), true) == 0.3f);
  CHECK(step_volume(kStairsSurface, false) == 0.6f && step_volume(kStairsSurface, true) == 0.6f);
  for (int s = 0; s < static_cast<int>(std::size(kStepSurfaces)); ++s)
    for (bool run : {false, true}) CHECK(step_volume(s, run) > 0.0f && step_volume(s, run) <= 1.0f);
  CHECK(step_volume(-1, false) == 0.0f && step_volume(static_cast<int>(std::size(kStepSurfaces)), true) == 0.0f);
}

// Its voice is muffled as the game muffles a sound where it is spoken (SoundController.GetFloorAudioSnapshot), unless
// nothing solid lies between it and the listener: an open doorway is open space, so the line plays as heard where the
// listener is (user, 2026-11-10: lines through an open door still sounded muffled). A shut door or a wall keeps it
// muffled.
void an_open_doorway_does_not_muffle_its_voice() {
  const Point spoken{0, 1.5f, 0}, listener{5, 1.6f, 0};
  const Point open = mixer_point(spoken, listener, false);
  CHECK(open.x == listener.x && open.y == listener.y && open.z == listener.z);
  const Point shut = mixer_point(spoken, listener, true);
  CHECK(shut.x == spoken.x && shut.y == spoken.y && shut.z == spoken.z);
}

// It stands from a get-up facing the way its body faces (Animator's body rotation), not the way its root was turned as
// it lay, so it turns to its way from there rather than spinning (user, 2026-11-10: it did not turn cleanly to run on).
void it_stands_facing_the_way_its_body_faces() {
  constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;
  for (const float degrees : {0.0f, 119.0f, -150.0f, 179.0f}) {
    const float h = degrees * kDeg / 2;
    CHECK(std::abs(wrap_angle(body_heading({0, std::sin(h), 0, std::cos(h)}) - degrees * kDeg)) < 1e-4f);
  }
  // A body pitched forward 60 degrees, turned 90: its heading is still 90.
  const phasmo::Quaternion yaw{0, std::sin(45 * kDeg), 0, std::cos(45 * kDeg)}, pitch{std::sin(30 * kDeg), 0, 0,
                                                                                        std::cos(30 * kDeg)};
  CHECK(std::abs(wrap_angle(body_heading(pm::custom::qmul(yaw, pitch)) - 90 * kDeg)) < 1e-4f);
}

// The second dive (Stand To Roll) stands still for its last half second, upright from 1.9 s (the bundle's probe), so it
// is trimmed at 1.8 s, while it still comes up out of the roll, and the run takes over from there (user, 2026-11-10:
// a dive ended standing still instead of running on).
void the_standing_roll_hands_back_to_the_run() {
  CHECK(std::abs(seconds_of("Roll_B") - 1.8f) < 1e-4f);
  CHECK(travel_of("Roll_B") > 4.0f && travel_of("Roll_B") < 4.4f);
}

// Its voice plays pitched down a little (user, 2026-11-10: the lines sounded out of place). Unity's pitch is also the
// playback rate, so a line lasts its length over the pitch: the queue waits that long and the sound outlives it.
void its_voice_is_pitched_down() {
  CHECK(voice::kVoicePitch < 1.0f && voice::kVoicePitch >= 0.8f);
  CHECK(std::abs(voice::played_seconds(1.8f, 0.9f) - 2.0f) < 1e-5f);
  CHECK(voice::played_seconds(2.0f, 1.0f) == 2.0f);
  CHECK(voice::played_seconds(2.0f, 0.0f) == 2.0f && voice::played_seconds(-1.0f, 0.9f) == 0.0f);
}

// The whole skeleton blends between clips, not only the humanoid bones: the investigator's rig has bones between them
// (Spine02 above the chest, a second neck twist) that would jump to the new clip's pose and carry the head, the arms and
// the sheet hung from the head with them (user, 2026-11-10: the sheet teleported as the clips changed).
void every_bone_under_the_hips_blends() {
  // A small tree: 0 has 1 and 2, 1 has 3, 3 has 4.
  const std::vector<std::vector<int>> children = {{1, 2}, {3}, {}, {4}, {}};
  const std::vector<int> all = figure::all_under(0, [&](int n) { return children[static_cast<size_t>(n)]; });
  CHECK(all.size() == 5);
  std::set<int> seen(all.begin(), all.end());
  CHECK(seen.size() == 5 && all[0] == 0);
  const auto at = [&](int n) { return std::find(all.begin(), all.end(), n) - all.begin(); };
  CHECK(at(1) < at(3) && at(3) < at(4));  // each before what hangs from it
  CHECK(figure::all_under(-1, [&](int) { return std::vector<int>{}; }).size() == 1);
}

// Standing up after a fall it turns to the player it is after with its turn clips before it runs: done once no turn clip
// plays and none would start (its figure is within its turn clips' start angle of them), or at the latest kRiseTurnMs
// after it stood.
void it_turns_with_its_clips_before_running_on() {
  constexpr float kDeg = std::numbers::pi_v<float> / 180.0f;
  CHECK(!rise_turn_done(1000, 1000 + kRiseTurnMs, 0.0f, 90 * kDeg, false));  // facing away: a turn clip plays
  CHECK(!rise_turn_done(1000, 1000 + kRiseTurnMs, 89 * kDeg, 90 * kDeg, true));  // a turn clip still plays
  CHECK(rise_turn_done(1000, 1000 + kRiseTurnMs, 89 * kDeg, 90 * kDeg, false));
  CHECK(rise_turn_done(1000 + kRiseTurnMs, 1000 + kRiseTurnMs, 0.0f, 180 * kDeg, true));  // waited long enough
  CHECK(rise_turn_done(1000, 1000 + kRiseTurnMs, 179 * kDeg, -179 * kDeg, false));         // across the wrap
  // Past its turn clips' start angle and short of facing them no clip plays, and it stood frozen there for the whole
  // kRiseTurnMs (user, 2026-11-10: getting up after a trip left it still). The run's own turning finishes the alignment.
  CHECK(rise_turn_done(1000, 1000 + kRiseTurnMs, 45 * kDeg, 90 * kDeg, false));
  CHECK(rise_turn_done(1000, 1000 + kRiseTurnMs, kFaceAheadDegrees * kDeg, 0.0f, false));
  CHECK(rise_turn_done(1000, 1000 + kRiseTurnMs, 0.0f, (kTurnStartDegrees - 1.0f) * kDeg, false));  // just short
  CHECK(!rise_turn_done(1000, 1000 + kRiseTurnMs, 0.0f, kTurnStartDegrees * kDeg, false));          // at it
  CHECK(kRiseTurnMs >= 1000 && kRiseTurnMs <= 4000);
}

// Every fall says its trip line (user, 2026-11-10: a fall in friendly mode played no sound): a trip says it as it begins
// (line_for_move), and a fall with any other cause (a bump, a drop, a door, a thrown object) as it is knocked over.
void every_fall_says_its_line() {
  CHECK(!line_for_fall(FallCause::Trip));  // already said as the trip began
  for (const FallCause c : {FallCause::Bump, FallCause::Drop, FallCause::Door, FallCause::Thrown})
    CHECK(line_for_fall(c) == std::optional<Lines>(Lines::Trip));
  CHECK(line_for_move(Move::Stumble) == std::optional<Lines>(Lines::Trip));
}


// A root that jumps further than any clip can carry it in a frame is a warp, not a step: the cloth's anchor moved under
// it, so what the cloth accumulated is cleared (Unity's ClearTransformMotion) instead of being flung across the room.
void a_root_jump_is_a_warp_not_a_step() {
  CHECK(!root_jumped(0.0f));
  CHECK(!root_jumped(kFrameJumpMetres - 0.01f));
  CHECK(root_jumped(kFrameJumpMetres));
  // Its furthest-travelling clip, at twenty frames a second, is still a step: the threshold sits above the fastest the
  // body ever moves itself, so a warp is the only thing that clears the cloth's motion.
  float fastest = 0.0f;
  for (const StateTravel& t : kClipTravel) fastest = std::max(fastest, t.metres / seconds_of(t.state));
  CHECK(fastest / 20.0f < kFrameJumpMetres);
}

// A trip's frames are timed from the knock-over to a moment after the get-up clip ends. The window that closed with the
// clip logged no slow frame at all across the three falls of the 2026-11-10 run, and the user's spike is the frame just
// after it, so the get-up's length is not where the watching stops.
void the_frames_of_a_trip_are_timed_past_the_get_up() {
  CHECK(fall_watch(true, 0.0f, 0.0f));           // under physics, no get-up timed yet
  CHECK(fall_watch(false, 0.5f, 3.0f));          // a get-up still being watched
  CHECK(fall_watch(false, 2.99f, 3.0f));
  CHECK(!fall_watch(false, 3.0f, 3.0f));         // the moment after is the last one
  CHECK(!fall_watch(false, 1.0f, 0.0f));         // never down: nothing to watch
  CHECK(fall_watch(true, 9.0f, 3.0f));           // physics is always worth timing
  CHECK(kFallWatchTailSeconds >= 0.5f && kFallWatchTailSeconds <= 3.0f);
}

// What a clip change leaves to smooth, by turn and by place, is what the user sees as two clips that do not mesh. None
// of the 2026-11-10 run's changes reached 60 degrees, so that threshold asked the wrong question; this one asks about
// the changes a player can see, and counts a joint moving as well as a joint turning.
void a_clip_change_is_a_jump_by_turn_or_by_place() {
  CHECK(!pose_jumps(0.0f, 0.0f));
  CHECK(!pose_jumps(kPoseTurnWarnDegrees - 0.01f, kPoseShiftWarnMetres - 0.01f));
  CHECK(pose_jumps(kPoseTurnWarnDegrees, 0.0f));
  CHECK(pose_jumps(0.0f, kPoseShiftWarnMetres));
  CHECK(kPoseTurnWarnDegrees >= 5.0f && kPoseTurnWarnDegrees <= 45.0f);  // a snap a player sees, not a pose difference
  CHECK(kPoseShiftWarnMetres >= 0.01f && kPoseShiftWarnMetres <= 0.1f);
}

