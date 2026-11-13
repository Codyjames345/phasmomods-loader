// Offline tests for playing, sharing and ending emotes (mods/emotes/emote_session.h): the decisions the mod's game glue
// carries out each frame, on the emoter's client and on everyone else's. The design is
// docs/superpowers/specs/2026-11-10-emotes-design.md, "Playing an emote" and "What ends an emote". Run:
// build\emotes_play_test.exe
#include <cmath>
#include <cstdio>
#include <vector>

#include "check.h"
#include "emote_session.h"

namespace {

using namespace pm::emotes;

// A table of `n` emotes, each looping after `seconds`, keyed 1000 + index as a pack's ids hash to.
EmoteTable table_of(int n, bool loops = true, float seconds = 2.0f) {
  EmoteTable t(static_cast<size_t>(n));
  for (int i = 0; i < n; ++i) {
    t[i].clipSeconds = seconds;
    t[i].loops = loops;
    t[i].song = SongLoop{0.0f, seconds};
    t[i].key = static_cast<uint32_t>(1000 + i);
  }
  return t;
}

EmoteTable table() {
  EmoteTable t = table_of(kSegments);
  t[5] = {3.0f, false, SongLoop{0.0f, 3.0f}, 1005, false};       // a one-shot
  t[2] = {1.6f, true, SongLoop{0.0f, 16.7f}, 1002, false};       // a song that loops on its own (Floss)
  return t;
}

LocalFrame here(double clip = 0.0) {
  LocalFrame f;
  f.present = true;
  f.pos = {1.0f, 0.0f, 1.0f};
  f.viewYaw = 0.0f;
  f.clipTime = clip;
  return f;
}

bool has(const std::vector<EmoteMsg>& sent, Kind k) {
  for (const EmoteMsg& m : sent)
    if (m.kind == k) return true;
  return false;
}

// A table of one emote with a walking variant (the user's ruling, 2026-11-12: forward only walks).
EmoteTable walking_table() {
  EmoteTable t = table_of(1);
  t[0].hasMove = true;
  return t;
}

// A frame: the investigator at (x, z), this move input (x right, y forward) and this dt.
LocalFrame frame_at(double clip, float x, float z, float moveX = 0.0f, float moveY = 0.0f, float dt = 0.1f) {
  LocalFrame f = here(clip);
  f.pos = {x, 0.0f, z};
  f.moveX = moveX;
  f.moveY = moveY;
  f.dt = dt;
  return f;
}

// A Move message was sent with this walking flag.
bool sent_move(const std::vector<EmoteMsg>& sent, bool moving) {
  for (const EmoteMsg& m : sent)
    if (m.kind == Kind::Move && m.moving == moving) return true;
  return false;
}

// ---- The emoter's client ----

void a_pick_starts_and_tells_everyone() {
  const EmoteTable t = table();
  LocalSession s(t);
  LocalFrame f = here(-1.0);
  f.viewYaw = 30.0f;
  const LocalOut o = s.pick(4, f);
  CHECK(o.start == 4 && !o.stop);
  CHECK(o.send.size() == 1 && o.send[0].kind == Kind::Start && o.send[0].key == t[4].key);
  CHECK(o.send.size() == 1 && o.send[0].x == 1.0f && o.send[0].yaw == 30.0f);
  CHECK(s.active() && s.emote() == 4 && s.yaw() == 30.0f);
  // Nothing without a local investigator (the ghost player of Playable Ghost has none).
  LocalSession none(table());
  LocalFrame gone = here();
  gone.present = false;
  CHECK(none.pick(1, gone).start == -1 && !none.active());
}

void moving_ends_the_emote() {
  LocalSession s(table());
  s.pick(0, here());
  LocalFrame f = here(0.5);
  f.pos.x += 0.4f;
  CHECK(!s.tick(f).stop);  // 0.4 m: still dancing
  f.pos.x += 0.2f;         // 0.6 m from the start
  const LocalOut o = s.tick(f);
  CHECK(o.stop && has(o.send, Kind::End));
  CHECK(!s.active());
  CHECK(!s.tick(f).stop);  // once
}

void each_listed_action_ends_it() {
  LocalSession s(table());
  s.pick(1, here());
  LocalFrame f = here(0.5);
  f.endAction = true;  // use, place, pick up, drop, throw, a door, a light switch, crouch
  CHECK(s.tick(f).stop);
}

void switching_items_does_not() {
  // Switching is not among the actions the glue reports (Task 4: InventorySwap, scroll and slots are not listed), so
  // the session sees an ordinary frame and the emote goes on.
  LocalSession s(table());
  s.pick(1, here());
  CHECK(!s.tick(here(0.5)).stop && s.active());
}

void a_one_shot_ends_by_itself() {
  LocalSession s(table());
  s.pick(5, here());
  CHECK(!s.tick(here(2.9)).stop);
  CHECK(s.tick(here(3.0)).stop);
}

// The song loops itself in the game (cut to its loop, AudioSource.loop): the session never restarts it, which cut it
// mid-wave a frame late (the sixth in-game run).
void a_loop_keeps_going() {
  LocalSession s(table());
  s.pick(0, here());
  CHECK(!s.tick(here(1.9)).stop);
  CHECK(!s.tick(here(2.05)).stop);  // the clip looped
  CHECK(!s.tick(here(30.0)).stop && s.active());
}

void a_new_pick_cuts_in() {
  LocalSession s(table());
  s.pick(0, here());
  s.tick(here(0.5));
  const LocalOut o = s.pick(3, here(0.6));
  CHECK(o.start == 3 && !o.stop);  // one clip and one song: the new one replaces the old, no stop between
  CHECK(o.send.size() == 1 && o.send[0].kind == Kind::Start && o.send[0].key == 1003u);
}

void death_ends_the_emote_first() {
  LocalSession s(table());
  s.pick(0, here());
  LocalFrame f = here(0.5);
  f.dying = true;  // the kill has begun: the game's death animation has the body from this frame
  const LocalOut o = s.tick(f);
  CHECK(o.stop && o.atOnce && has(o.send, Kind::End));
  // Nothing is picked while the death animation plays (the user, 2026-11-10).
  LocalSession falling(table());
  CHECK(falling.pick(1, f).start == -1 && !falling.active());
  // A player whose death is over may dance: they are only down to the living. Their own mark is no fresh death.
  LocalFrame down = here(0.5);
  down.dead = true;
  LocalSession ghost(table());
  ghost.pick(1, down);
  CHECK(!ghost.tick(down).stop);
  // The game's mark of the death arriving with no start seen (on a build whose entry did not resolve) ends the dance
  // too, and at once: the game has that body either way.
  LocalSession late(table());
  late.pick(0, here());
  LocalFrame over = here(0.5);
  over.dead = true;
  const LocalOut fallback = late.tick(over);
  CHECK(fallback.stop && fallback.atOnce);
}

void the_body_turns_and_is_shared() {
  LocalSession s(table());
  s.pick(0, here());
  LocalFrame f = here(0.1);
  f.viewYaw = 80.0f;
  LocalOut o = s.tick(f);
  CHECK(o.bodyYaw && *o.bodyYaw == 0.0f && !has(o.send, Kind::Facing));  // within ninety: no turn, nothing sent
  f.viewYaw = 120.0f;
  o = s.tick(f);
  CHECK(o.bodyYaw && std::fabs(*o.bodyYaw - 30.0f) < 1e-3f && has(o.send, Kind::Facing));
  f.viewYaw = 122.0f;  // two degrees more: under the send threshold
  o = s.tick(f);
  CHECK(!has(o.send, Kind::Facing));
  f.viewYaw = 130.0f;
  o = s.tick(f);
  CHECK(has(o.send, Kind::Facing));
}

// The user's ruling (2026-11-10): walking within the half metre turns the body smoothly to the way it goes, so it does
// not drift sideways or backwards.
void walking_turns_the_body_to_the_way_it_goes() {
  LocalSession s(table());
  s.pick(0, here());  // looking along +z, the body with it
  LocalFrame f = here(0.5);
  f.dt = 0.1f;
  f.pos.x += 0.1f;  // walking right at 1 m/s
  LocalOut o = s.tick(f);
  const float step = kTurnDegreesPerSecond * 0.1f;
  CHECK(o.bodyYaw && std::fabs(*o.bodyYaw - step) < 1e-3f);  // turning, not at once
  f.pos.x += 0.1f;
  o = s.tick(f);
  CHECK(o.bodyYaw && std::fabs(*o.bodyYaw - std::min(90.0f, 2 * step)) < 1e-3f);
  for (int i = 0; i < 5; ++i) o = s.tick(f);  // standing: it keeps the way it went (within ninety of the view)
  CHECK(o.bodyYaw && std::fabs(*o.bodyYaw - 90.0f) < 1e-3f && s.active());
  // Walking back (toward the view) turns it round, the short way.
  LocalSession back(table());
  back.pick(0, here());
  LocalFrame b = here(0.5);
  b.dt = 0.1f;
  for (int i = 0; i < 3; ++i) {
    b.pos.z -= 0.1f;
    o = back.tick(b);
  }
  CHECK(o.bodyYaw && std::fabs(std::fabs(wrap_degrees(*o.bodyYaw)) - std::min(180.0f, 3 * step)) < 1e-3f);
  // Too slow to be walking (a sway of the body): the view's rule holds.
  LocalSession sway(table());
  sway.pick(0, here());
  LocalFrame w = here(0.5);
  w.dt = 0.1f;
  w.pos.x += 0.01f;  // 0.1 m/s
  CHECK(sway.tick(w).bodyYaw.value_or(-1.0f) == 0.0f);
}

// ---- Walking emotes and intros (the user's ruling, 2026-11-12: W alone walks, a diagonal does not) ----

void w_alone_is_forward() { CHECK(walking_forward(0.0f, 1.0f)); }

void stick_roughly_up_is_forward() { CHECK(walking_forward(0.3f, 0.8f)); }

void a_diagonal_is_not_forward() { CHECK(!walking_forward(0.707f, 0.707f)); }

void backward_and_sideways_are_not_forward() {
  CHECK(!walking_forward(0.0f, -1.0f));
  CHECK(!walking_forward(1.0f, 0.0f));
}

void walking_forward_switches_to_the_moving_clip() {
  LocalSession s(walking_table());
  const LocalOut start = s.pick(0, here(-1.0));
  CHECK(start.phase && *start.phase == Phase::Loop);  // no intro: the loop clip plays at once
  const LocalOut o = s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f));
  CHECK(o.phase && *o.phase == Phase::Moving && s.phase() == Phase::Moving);
  CHECK(sent_move(o.send, true) && !sent_move(o.send, false));
  const LocalOut n = s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 1.0f));  // still walking: no second switch
  CHECK(!n.phase.has_value() && !has(n.send, Kind::Move));
}

void a_short_stop_does_not_switch_back() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f));  // Walking
  const LocalOut o = s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 0.0f, 0.1f));  // let go for only 0.1 s
  CHECK(s.phase() == Phase::Moving && !o.phase.has_value() && !has(o.send, Kind::Move));
}

void a_stop_longer_than_the_hold_switches_back() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f));  // Walking
  const LocalOut o = s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 0.0f, 0.25f));  // let go for 0.25 s
  CHECK(o.phase && *o.phase == Phase::Loop && s.phase() == Phase::Loop);
  CHECK(sent_move(o.send, false));
}

void walking_forward_past_half_a_metre_does_not_end_it() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  float z = 1.0f;
  for (int i = 0; i < 12; ++i) {  // 3 m forward, the half-metre start point moving with each step
    z += 0.25f;
    CHECK(!s.tick(frame_at(0.5 + 0.25 * i, 1.0f, z, 0.0f, 1.0f, 0.25f)).stop);
  }
  CHECK(s.active());
}

void sideways_after_walking_measures_from_the_stop() {
  LocalSession s(walking_table());
  LocalFrame pick = here(-1.0);
  pick.pos = {0.0f, 0.0f, 0.0f};
  s.pick(0, pick);
  s.tick(frame_at(0.5, 0.0f, 1.0f, 0.0f, 1.0f));  // walking forward
  s.tick(frame_at(0.6, 0.0f, 2.0f, 0.0f, 1.0f));  // to z = 2, still going
  for (int i = 0; i < 3; ++i) s.tick(frame_at(0.7, 0.0f, 2.0f, 0.0f, 0.0f, 0.1f));  // stop for 0.3 s
  CHECK(s.phase() == Phase::Loop);
  CHECK(!s.tick(frame_at(0.8, 0.4f, 2.0f, 1.0f, 0.0f)).stop);  // 0.4 m from the stop: still dancing
  CHECK(s.tick(frame_at(0.9, 0.6f, 2.0f, 1.0f, 0.0f)).stop);   // 0.6 m from it: over
  CHECK(!s.active());
}

void backward_past_half_a_metre_ends_it() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  LocalFrame f = frame_at(0.5, 1.0f, 0.6f, 0.0f, -1.0f);  // holding S, 0.4 m back
  CHECK(!s.tick(f).stop);
  f.pos.z = 0.4f;  // 0.6 m behind the start
  CHECK(s.tick(f).stop);
}

void sprinting_ends_a_walking_emote() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f));  // Walking
  LocalFrame f = frame_at(0.6, 1.0f, 1.0f, 0.0f, 1.0f);
  f.sprinting = true;
  const LocalOut o = s.tick(f);
  CHECK(o.stop && has(o.send, Kind::End) && !s.active());
}

void a_standing_emote_ignores_walking_forward() {
  LocalSession s(table());  // t[0] has no walking variant
  const LocalOut start = s.pick(0, here(-1.0));
  CHECK(start.phase && *start.phase == Phase::Loop);
  const LocalOut o = s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f));  // walking means nothing to it
  CHECK(!o.phase.has_value() && !has(o.send, Kind::Move) && s.phase() == Phase::Loop);
  CHECK(s.tick(frame_at(0.6, 1.6f, 1.0f, 0.0f, 1.0f)).stop);  // the half-metre rule stands as it was
}

void song_clock_runs_on_across_a_clip_switch() {
  LocalSession s(walking_table());
  s.pick(0, here(-1.0));
  CHECK(s.elapsed() == 0.0);
  s.tick(frame_at(0.1, 1.0f, 1.0f, 0.0f, 0.0f, 0.5f));
  CHECK(std::fabs(s.elapsed() - 0.5) < 1e-6);
  s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 1.0f, 0.1f));  // switch to the walking clip
  CHECK(std::fabs(s.elapsed() - 0.6) < 1e-6);
  s.tick(frame_at(0.7, 1.0f, 1.0f, 0.0f, 0.0f, 0.25f));  // and back
  CHECK(std::fabs(s.elapsed() - 0.85) < 1e-6);
}

void an_intro_plays_once_then_loops() {
  EmoteTable t = table_of(1);
  t[0].introSeconds = 1.5f;
  LocalSession s(t);
  const LocalOut start = s.pick(0, here(-1.0));
  CHECK(start.phase && *start.phase == Phase::Intro && s.phase() == Phase::Intro);
  CHECK(!s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 0.0f, 0.7f)).phase.has_value());  // 0.7 s in
  CHECK(!s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 0.0f, 0.7f)).phase.has_value());  // 1.4 s in
  const LocalOut o = s.tick(frame_at(0.7, 1.0f, 1.0f, 0.0f, 0.0f, 0.2f));         // 1.6 s in
  CHECK(o.phase && *o.phase == Phase::Loop && s.phase() == Phase::Loop);
  CHECK(o.send.empty());  // every client times the intro itself: nothing is sent for it
}

void walking_forward_during_the_intro_skips_to_moving() {
  EmoteTable t = table_of(1);
  t[0].introSeconds = 1.5f;
  t[0].hasMove = true;
  LocalSession s(t);
  const LocalOut start = s.pick(0, here(-1.0));
  CHECK(start.phase && *start.phase == Phase::Intro);
  const LocalOut o = s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f, 0.5f));  // walking 0.5 s into the intro
  CHECK(o.phase && *o.phase == Phase::Moving && sent_move(o.send, true));
  CHECK(!s.tick(frame_at(1.0, 1.0f, 1.0f, 0.0f, 1.0f, 0.5f)).phase.has_value());
  CHECK(!s.tick(frame_at(1.6, 1.0f, 1.0f, 0.0f, 1.0f, 0.6f)).phase.has_value());  // past 1.5 s: the intro stays skipped
  CHECK(s.phase() == Phase::Moving);
}

void stopping_after_a_skipped_intro_goes_to_the_loop() {
  EmoteTable t = table_of(1);
  t[0].introSeconds = 1.5f;
  t[0].hasMove = true;
  LocalSession s(t);
  s.pick(0, here(-1.0));
  s.tick(frame_at(0.5, 1.0f, 1.0f, 0.0f, 1.0f, 0.5f));  // Intro straight to Moving
  const LocalOut o = s.tick(frame_at(0.6, 1.0f, 1.0f, 0.0f, 0.0f, 0.25f));
  CHECK(o.phase && *o.phase == Phase::Loop);  // never back to the Intro
  CHECK(s.phase() == Phase::Loop);
}

void a_lost_investigator_ends_it() {
  LocalSession s(table());
  s.pick(0, here());
  LocalFrame f = here(0.5);
  f.present = false;  // a scene change, or leaving the room
  CHECK(s.tick(f).stop);
}

// ---- Everyone else's clients ----

bool did(const std::vector<RemoteOut>& out, RemoteOut::What w, int actor) {
  for (const RemoteOut& o : out)
    if (o.what == w && o.actor == actor) return true;
  return false;
}

void a_peer_dances_where_told() {
  RemoteSessions r(table());
  auto out = r.receive(7, EmoteMsg{Kind::Start, 1002u, false, 0, 0, 0, 45.0f}, false, false);
  CHECK(did(out, RemoteOut::Start, 7) && did(out, RemoteOut::Yaw, 7));
  CHECK(r.dancing(7));
  out = r.receive(7, EmoteMsg{Kind::Facing, 1002u, false, 0, 0, 0, 90.0f}, false, false);
  CHECK(did(out, RemoteOut::Yaw, 7) && out.back().yaw == 90.0f);
  out = r.receive(7, EmoteMsg{Kind::End, 1002u, false, 0, 0, 0, 0}, false, false);
  CHECK(did(out, RemoteOut::Stop, 7) && !r.dancing(7));
  // A Facing or End for a peer not dancing is passed over.
  CHECK(r.receive(9, EmoteMsg{Kind::Facing, 1000u, false, 0, 0, 0, 10.0f}, false, false).empty());
}

void the_living_never_see_the_dead_dance() {
  RemoteSessions r(table());
  CHECK(r.receive(3, EmoteMsg{Kind::Start, 1001u, false, 0, 0, 0, 0}, true, false).empty());  // a dead emoter, a living viewer
  CHECK(!r.dancing(3));
  CHECK(!r.receive(3, EmoteMsg{Kind::Start, 1001u, false, 0, 0, 0, 0}, true, true).empty());  // the dead see the dead
  // A living emoter who dies mid-dance stops for the living.
  RemoteSessions live(table());
  live.receive(4, EmoteMsg{Kind::Start, 1001u, false, 0, 0, 0, 0}, false, false);
  const auto out = live.tick({RemoteFrame{4, true, true, false, 0.5}}, false);
  CHECK(did(out, RemoteOut::Stop, 4) && !live.dancing(4));
  // A peer whose death has begun, before the game marks it, comes off at once: their body is the game's death
  // animation, not the dance (the user, 2026-11-10).
  RemoteSessions fall(table());
  fall.receive(6, EmoteMsg{Kind::Start, 1001u, false, 0, 0, 0, 0}, false, false);
  const auto down = fall.tick({RemoteFrame{6, true, false, true, 0.5}}, false);
  CHECK(did(down, RemoteOut::Stop, 6) && !fall.dancing(6));
  for (const RemoteOut& o : down)
    if (o.what == RemoteOut::Stop) CHECK(o.atOnce);
  // The dying are down to the living as the dead are: a start from one is passed over.
  RemoteSessions late(table());
  CHECK(late.receive(7, EmoteMsg{Kind::Start, 1001u, false, 0, 0, 0, 0}, true, false).empty() && !late.dancing(7));
}

void a_vanished_emoter_is_cleaned_up() {
  RemoteSessions r(table());
  r.receive(5, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  const auto out = r.tick({RemoteFrame{5, false, false, false, -1.0}}, false);  // left the room, or the scene changed
  CHECK(did(out, RemoteOut::Stop, 5) && !r.dancing(5));
  // One not in the frame at all is gone too.
  r.receive(6, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  CHECK(did(r.tick({}, false), RemoteOut::Stop, 6));
}

void a_peers_one_shot_ends_here_too() {
  RemoteSessions r(table());
  // A one-shot peer's clip that played out ends for this client too.
  r.receive(8, EmoteMsg{Kind::Start, 1005u, false, 0, 0, 0, 0}, false, false);
  CHECK(did(r.tick({RemoteFrame{8, true, false, false, 3.1}}, false), RemoteOut::Stop, 8));
}

// A peer naming an emote this client's list does not hold (its packs differ, or the wheel changed mid-contract) is
// passed over: nothing shows and nothing is remembered.
void an_unknown_key_from_a_peer_is_ignored() {
  RemoteSessions r(table());
  const auto out = r.receive(7, EmoteMsg{Kind::Start, 999999u, false, 0, 0, 0, 45.0f}, false, false);
  CHECK(out.empty());
  CHECK(!r.dancing(7));
  // And a Facing afterwards does nothing either: no peer was made.
  CHECK(r.receive(7, EmoteMsg{Kind::Facing, 1000u, false, 0, 0, 0, 90.0f}, false, false).empty());
}

// One blend rule for a clip switch on every client (the review, 2026-11-13: a peer's walk ended over 0.1 s where the
// dancer's own ended over 0.25 s): an intro ending blends over kIntroBlendSeconds, every walk switch over
// kMoveBlendSeconds.
void a_switch_blends_alike_on_every_client() {
  CHECK(phase_blend_seconds(Phase::Loop, true) == kIntroBlendSeconds);
  CHECK(phase_blend_seconds(Phase::Loop, false) == kMoveBlendSeconds);
  CHECK(phase_blend_seconds(Phase::Moving, true) == kMoveBlendSeconds);
  CHECK(phase_blend_seconds(Phase::Moving, false) == kMoveBlendSeconds);
}

// A peer already dancing who starts an emote this client lacks: their old dance stops here, rather than playing on
// until their End (the review, 2026-11-13).
void an_unknown_key_stops_a_peers_old_dance() {
  RemoteSessions r(table());
  r.receive(7, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  CHECK(r.dancing(7));
  const auto out = r.receive(7, EmoteMsg{Kind::Start, 999999u, false, 0, 0, 0, 45.0f}, false, false);
  CHECK(out.size() == 1 && out[0].what == RemoteOut::Stop && out[0].actor == 7);
  CHECK(!r.dancing(7));
}

// More emotes than one page: the index goes into the whole table, and a peer matches the message's key to its own
// table and gets the same index.
void a_table_of_twenty_emotes_plays_the_last() {
  const EmoteTable big = table_of(20);
  LocalSession s(big);
  LocalFrame f = here(-1.0);
  const LocalOut o = s.pick(19, f);
  CHECK(o.start == 19 && o.send.size() == 1 && o.send[0].kind == Kind::Start && o.send[0].key == big[19].key);
  // Past the end of this table, not past one page of eight.
  CHECK(s.pick(20, here(-1.0)).start == -1 && s.active() && s.emote() == 19);
  RemoteSessions r(big);
  const auto out = r.receive(4, o.send[0], false, false);
  CHECK(did(out, RemoteOut::Start, 4));
  bool sawLast = false;
  for (const auto& [actor, emote] : r.dancers())
    if (actor == 4 && emote == 19) sawLast = true;
  CHECK(sawLast);
}

// ---- Walking and intros for peers (2026-11-12) ----

void a_peer_intro_switches_on_their_own_clock() {
  EmoteTable t = table_of(1);
  t[0].introSeconds = 1.0f;
  RemoteSessions r(t);
  r.receive(7, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  CHECK(r.elapsed(7) == 0.0);
  CHECK(r.tick({RemoteFrame{7, true, false, false, 0.4}}, false, 0.6f).empty());  // 0.6 s in: still the intro
  const auto out = r.tick({RemoteFrame{7, true, false, false, 0.5}}, false, 0.5f);  // 1.1 s in
  CHECK(out.size() == 1 && out[0].what == RemoteOut::Phase && out[0].phase == Phase::Loop && out[0].actor == 7);
  CHECK(r.tick({RemoteFrame{7, true, false, false, 0.6}}, false, 0.5f).empty());  // once only
}

void a_peer_move_message_switches_their_clip() {
  RemoteSessions r(table_of(1));
  r.receive(7, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  auto out = r.receive(7, EmoteMsg{Kind::Move, 1000u, true, 0, 0, 0, 0}, false, false);
  CHECK(out.size() == 1 && out[0].what == RemoteOut::Phase && out[0].phase == Phase::Moving);
  out = r.receive(7, EmoteMsg{Kind::Move, 1000u, false, 0, 0, 0, 0}, false, false);
  CHECK(out.size() == 1 && out[0].what == RemoteOut::Phase && out[0].phase == Phase::Loop);
  // A Move from a peer not dancing is passed over.
  CHECK(r.receive(9, EmoteMsg{Kind::Move, 1000u, true, 0, 0, 0, 0}, false, false).empty());
}

void a_peer_elapsed_follows_dt() {
  RemoteSessions r(table_of(1));
  CHECK(r.elapsed(3) == 0.0);  // a peer not dancing
  r.receive(3, EmoteMsg{Kind::Start, 1000u, false, 0, 0, 0, 0}, false, false);
  CHECK(r.elapsed(3) == 0.0);
  r.tick({RemoteFrame{3, true, false, false, 0.1}}, false, 0.25f);
  r.tick({RemoteFrame{3, true, false, false, 0.2}}, false, 0.25f);
  CHECK(std::fabs(r.elapsed(3) - 0.5) < 1e-9);
}

}  // namespace

int main() {
  a_pick_starts_and_tells_everyone();
  moving_ends_the_emote();
  each_listed_action_ends_it();
  switching_items_does_not();
  a_one_shot_ends_by_itself();
  a_loop_keeps_going();
  a_new_pick_cuts_in();
  death_ends_the_emote_first();
  the_body_turns_and_is_shared();
  walking_turns_the_body_to_the_way_it_goes();
  w_alone_is_forward();
  stick_roughly_up_is_forward();
  a_diagonal_is_not_forward();
  backward_and_sideways_are_not_forward();
  walking_forward_switches_to_the_moving_clip();
  a_short_stop_does_not_switch_back();
  a_stop_longer_than_the_hold_switches_back();
  walking_forward_past_half_a_metre_does_not_end_it();
  sideways_after_walking_measures_from_the_stop();
  backward_past_half_a_metre_ends_it();
  sprinting_ends_a_walking_emote();
  a_standing_emote_ignores_walking_forward();
  song_clock_runs_on_across_a_clip_switch();
  an_intro_plays_once_then_loops();
  walking_forward_during_the_intro_skips_to_moving();
  stopping_after_a_skipped_intro_goes_to_the_loop();
  a_lost_investigator_ends_it();
  a_peer_dances_where_told();
  the_living_never_see_the_dead_dance();
  a_vanished_emoter_is_cleaned_up();
  a_peers_one_shot_ends_here_too();
  an_unknown_key_from_a_peer_is_ignored();
  an_unknown_key_stops_a_peers_old_dance();
  a_switch_blends_alike_on_every_client();
  a_table_of_twenty_emotes_plays_the_last();
  a_peer_intro_switches_on_their_own_clock();
  a_peer_move_message_switches_their_clip();
  a_peer_elapsed_follows_dt();
  std::printf("%s\n", g_failures ? "emotes_play tests FAILED" : "emotes_play tests passed");
  return g_failures ? 1 : 0;
}
