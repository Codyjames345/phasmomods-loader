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

// The door clip: outwards when it stands on the side the door's forward points to, inwards otherwise.
void the_door_clip_follows_the_side() {
  CHECK(door_act({0, 0, 1}, {0, 0, 0}, {0, 0, 1}) == Act::OpenDoorOutwards);
  CHECK(door_act({0, 0, -1}, {0, 0, 0}, {0, 0, 1}) == Act::OpenDoorInwards);
}

// It only reaches a door within kDoorReach of its feet, on the flat.
void a_door_is_reached_within_reach() {
  CHECK(at_door({0, 0, 0}, {kDoorReach - 0.1f, 0.5f, 0}));
  CHECK(!at_door({0, 0, 0}, {kDoorReach + 0.1f, 0, 0}));
  CHECK(!at_door({0, 0, 0}, {0, 3.0f, 0}));  // another floor
}

// The speed it chases the target at: their own sprint as reported, else the master's own player's, else the last.
void the_chase_speed_comes_from_the_target() {
  CHECK(chase_sprint(Sprint{true, 5.0f}, Sprint{true, 4.0f}, 3.0f) == 5.0f);
  CHECK(chase_sprint(std::nullopt, Sprint{true, 4.0f}, 3.0f) == 4.0f);
  CHECK(chase_sprint(std::nullopt, std::nullopt, 3.0f) == 3.0f);
  CHECK(chase_sprint(Sprint{false, 5.0f}, std::nullopt, 3.0f) == 5.0f);  // a tired target: the same speed
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
  CHECK(stance_for(Phase::Lurking) == Act::Sit && stance_for(Phase::LockedOut) == Act::Sit);
  CHECK(!stance_for(Phase::Chasing) && !stance_for(Phase::Following) && !stance_for(Phase::Excited));
  CHECK(steers(Phase::Moping));  // held where it sits, never wandering off
}

// On first sight it gets up, and its excited line comes once, at the start of getting up.
void gets_up_on_first_sight_and_says_its_line_once() {
  const Facts sees{true, true, false, false, 2, false};
  CHECK(next_phase(Phase::Moping, sees) == Phase::Excited);
  CHECK(entry_act(Phase::Moping, Phase::Excited) == Act::GetUp);
  CHECK(entry_act(Phase::Lurking, Phase::Chasing) == Act::GetUp);
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

// The tunables keep the chase sane: a search is slower than the chase, and pauses are short.
void the_chase_tunables_are_sane() {
  CHECK(kSearchPace > 0.0f && kSearchPace < 1.0f);
  CHECK(kBumpGapMinMs < kBumpGapMaxMs && kBumpPauseMs < kBumpGapMinMs);
  CHECK(kBangGapMs > 0 && kDoorPauseMs > 0 && kGetUpMs > 0 && kSightMs > 0 && kShownResendMs > 0);
}
