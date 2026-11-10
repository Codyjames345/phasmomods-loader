// Offline tests for the Emotes mod's rules (mods/emotes/emote_rule.h), its message (emote_net.h) and its emotes' table
// (emote_assets.h): the wheel's pick by direction and its key, the body turning past ninety degrees as the user's
// examples say, the half-metre cancel, a looping song restarting with its clip, the message's bytes and the dead's rule.
// The design is docs/superpowers/specs/2026-11-10-emotes-design.md. Run: build\emotes_test.exe
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "check.h"
#include "emote_assets.h"
#include "emote_net.h"
#include "emote_rule.h"

namespace {

using namespace pm::emotes;

bool near(float a, float b) { return std::fabs(a - b) < 1e-3f; }

// ---- The wheel ----

void the_wheel_picks_by_direction() {
  const float far = 300.0f;
  CHECK(segment_for(0, far, kPickDeadZone) == 0);      // up
  CHECK(segment_for(far, far, kPickDeadZone) == 1);    // up-right
  CHECK(segment_for(far, 0, kPickDeadZone) == 2);      // right
  CHECK(segment_for(0, -far, kPickDeadZone) == 4);     // down
  CHECK(segment_for(-far, 0, kPickDeadZone) == 6);     // left
  CHECK(segment_for(-far, far, kPickDeadZone) == 7);   // up-left
  CHECK(segment_for(5, 5, kPickDeadZone) == -1);       // inside the dead zone
  CHECK(segment_for(0, 0, kPickDeadZone) == -1);
  // The neutral zone in the middle is large, so a pick is easy to let go of by moving back toward the centre (the user,
  // 2026-11-10, twice): a move of 200 counts picks nothing.
  CHECK(segment_for(200.0f, 0.0f, kPickDeadZone) == -1);
  CHECK(segment_for(-140.0f, 140.0f, kPickDeadZone) == -1);
  // The boundary between up and up-right, at 22.5 degrees, picks the clockwise one.
  const float b = 22.5f / 57.29577951f;
  CHECK(segment_for(far * std::sin(b), far * std::cos(b), kPickDeadZone) == 1);
  CHECK(segment_for(far * std::sin(b - 0.01f), far * std::cos(b - 0.01f), kPickDeadZone) == 0);
  // A stick's dead zone works the same way.
  CHECK(segment_for(0.6f, 0.0f, kStickDeadZone) == 2);
  CHECK(segment_for(0.3f, 0.0f, kStickDeadZone) == -1);
}

// The mouse's movement since the wheel opened is held within kPickReach of the centre (the user, 2026-11-10), so after
// any flick the way back to the neutral zone is only the gap between them.
void the_mouse_reach_is_capped() {
  CHECK(kPickReach > kPickDeadZone);
  float x = 0.0f, y = 0.0f;
  add_capped(x, y, 5000.0f, 0.0f);  // a long flick right
  CHECK(near(x, kPickReach) && near(y, 0.0f));
  CHECK(segment_for(x, y, kPickDeadZone) == 2);
  // Back left by a little more than the gap: neutral again.
  add_capped(x, y, -(kPickReach - kPickDeadZone) - 1.0f, 0.0f);
  CHECK(segment_for(x, y, kPickDeadZone) == -1);
  // A diagonal flick keeps its direction.
  x = y = 0.0f;
  add_capped(x, y, -3000.0f, 3000.0f);
  CHECK(near(std::hypot(x, y), kPickReach) && near(x, -y));
  CHECK(segment_for(x, y, kPickDeadZone) == 7);
  // Inside the cap nothing changes.
  x = y = 0.0f;
  add_capped(x, y, 10.0f, -20.0f);
  CHECK(near(x, 10.0f) && near(y, -20.0f));
}

void release_plays_the_pick() {
  WheelInput w;
  CHECK(!wheel_step(w, true, false, false, -1).has_value());  // the key goes down: the wheel opens
  CHECK(w.open);
  CHECK(!wheel_step(w, true, false, false, 2).has_value());   // the mouse picks the right segment
  CHECK(w.picked == 2);
  const auto played = wheel_step(w, false, true, false, 2);   // released on it
  CHECK(played.has_value() && *played == 2);
  CHECK(!w.open);
}

void release_with_nothing_picked_plays_nothing() {
  WheelInput w;
  wheel_step(w, true, false, false, -1);
  wheel_step(w, true, false, false, 3);
  wheel_step(w, true, false, false, -1);  // back to neutral
  CHECK(!wheel_step(w, false, true, false, -1).has_value());
  CHECK(!w.open);
}

void an_interrupted_wheel_plays_nothing() {
  WheelInput w;
  wheel_step(w, true, false, false, -1);
  wheel_step(w, true, false, false, 5);
  CHECK(!wheel_step(w, true, false, true, 5).has_value());  // a menu took the keyboard
  CHECK(!w.open);
  // Released afterwards: still nothing, and it does not reopen until the key goes down again.
  CHECK(!wheel_step(w, false, true, false, 5).has_value());
  CHECK(!w.open);
  // The key lost without a release (the window lost focus): closed, nothing played.
  wheel_step(w, true, false, false, -1);
  wheel_step(w, true, false, false, 1);
  CHECK(!wheel_step(w, false, false, false, 1).has_value());
  CHECK(!w.open);
}

// ---- The body's facing (the user's examples) ----

void the_body_follows_past_ninety() {
  float body = 0.0f;
  body = body_yaw(body, 60.0f);
  CHECK(near(body, 0.0f));  // within ninety: the body stays
  body = body_yaw(body, 120.0f);
  CHECK(near(body, 30.0f));  // past ninety to the right: the body turns to keep it at ninety
  body = body_yaw(body, 0.0f);
  CHECK(near(body, 30.0f));  // turning back does not swing the body back
  // Turned right round to 180, by the right: the body faces 90.
  body = 0.0f;
  for (float v = 0.0f; v <= 180.0f; v += 5.0f) body = body_yaw(body, v);
  CHECK(near(body, 90.0f));
  // By the left: -90.
  body = 0.0f;
  for (float v = 0.0f; v >= -180.0f; v -= 5.0f) body = body_yaw(body, v);
  CHECK(near(body, -90.0f));
  // On across the 180/-180 wrap, still ninety behind on the right.
  body = 90.0f;
  body = body_yaw(body, -170.0f);  // 190 degrees to the right of 0
  CHECK(near(body, 100.0f));
}

void only_a_move_past_half_a_metre_ends_it() {
  CHECK(!moved_too_far({0, 0, 0}, {0.5f, 0, 0}));
  CHECK(moved_too_far({0, 0, 0}, {0.51f, 0, 0}));
  CHECK(moved_too_far({1, 0, 1}, {1.4f, 0, 1.4f}));  // 0.566 m on the diagonal
  CHECK(!moved_too_far({0, 0, 0}, {0, 3.0f, 0}));     // straight up a stair: no move
}

// ---- A song with its clip (the user's rule) ----

void a_looping_song_restarts_with_its_clip() {
  const float length = 2.0f;
  CHECK(!loop_restarted(0.0, 1.9, length));
  CHECK(loop_restarted(1.9, 2.05, length));  // crossed the clip's end: the song starts again
  CHECK(!loop_restarted(2.05, 3.0, length));
  CHECK(loop_restarted(3.9, 4.1, length));
  CHECK(loop_restarted(1.0, 5.0, length));   // a long frame crossing two: one restart
  CHECK(!loop_restarted(1.0, 1.0, length));
  CHECK(!loop_restarted(-1.0, 0.5, length));  // no clip before
  CHECK(!loop_restarted(1.9, 2.1, 0.0f));
  CHECK(!one_shot_done(1.99, length));
  CHECK(one_shot_done(2.0, length));
}

void a_song_loops_at_its_own_points() {
  // No loop points given: the song's loop is the clip's (start 0, the clip's length).
  const SongLoop clipLoop{0.0f, 2.0f};
  CHECK(song_position(1.5, clipLoop) == 1.5);
  CHECK(std::fabs(song_position(2.5, clipLoop) - 0.5) < 1e-9);
  // Its own points: an intro of 1 s, then a 4 s loop from 1 s to 5 s, whatever the clip's length.
  const SongLoop own{1.0f, 4.0f};
  CHECK(song_position(3.0, own) == 3.0);             // still in the first pass
  CHECK(std::fabs(song_position(5.5, own) - 1.5) < 1e-9);  // back to the loop point, half a second on
  CHECK(std::fabs(song_position(9.5, own) - 1.5) < 1e-9);
  // No loop at all (length 0): the song plays on from the clip's start.
  CHECK(song_position(50.0, SongLoop{0.0f, 0.0f}) == 50.0);
  // An emote's loop: its own points when given, else its clip's length.
  EmoteAsset e = kEmotes[0];
  e.songLoopStart = e.songLoopSeconds = 0.0f;  // none given
  CHECK(song_loop_of(e, 9.0f).start == 0.0f && song_loop_of(e, 9.0f).length == 9.0f);
  e.songLoopStart = 2.0f;
  e.songLoopSeconds = 3.0f;
  CHECK(song_loop_of(e, 9.0f).start == 2.0f && song_loop_of(e, 9.0f).length == 3.0f);
}

// ---- The message ----

void messages_round_trip() {
  const EmoteMsg sent{Kind::Start, 7, 1.5f, -2.25f, 3.0f, 135.0f};
  const std::vector<uint8_t> bytes = pack(sent);
  CHECK(bytes.size() == kMessageBytes && bytes[0] == kMagic);
  const auto got = unpack(bytes);
  CHECK(got.has_value());
  if (got) {
    CHECK(got->kind == Kind::Start && got->emote == 7);
    CHECK(got->x == 1.5f && got->y == -2.25f && got->z == 3.0f && got->yaw == 135.0f);
  }
  for (Kind k : {Kind::Facing, Kind::End}) {
    const auto again = unpack(pack(EmoteMsg{k, 0, 0, 0, 0, -45.0f}));
    CHECK(again && again->kind == k);
  }
}

void bad_messages_are_dropped() {
  std::vector<uint8_t> good = pack(EmoteMsg{Kind::Start, 3, 0, 0, 0, 0});
  CHECK(!unpack({}).has_value());
  std::vector<uint8_t> b = good;
  b[0] = 'C';
  CHECK(!unpack(b).has_value());  // another mod's tag
  b = good;
  b[1] = 0;
  CHECK(!unpack(b).has_value());
  b[1] = 4;
  CHECK(!unpack(b).has_value());  // unknown kinds
  b = good;
  b[2] = kSegments;
  CHECK(!unpack(b).has_value());  // past the wheel
  b = good;
  b.pop_back();
  CHECK(!unpack(b).has_value());  // short
  b = good;
  b.push_back(0);
  CHECK(!unpack(b).has_value());  // long
  EmoteMsg nan{Kind::Start, 0, 0, 0, 0, 0};
  nan.x = std::nanf("");
  CHECK(!unpack(pack(nan)).has_value());
  nan.x = 0;
  nan.yaw = INFINITY;
  CHECK(!unpack(pack(nan)).has_value());
}

void the_dead_dance_for_the_dead() {
  CHECK(shows_to(false, false));  // a living emoter, to the living
  CHECK(shows_to(false, true));   // and to the dead
  CHECK(!shows_to(true, false));  // a dead one, never to the living
  CHECK(shows_to(true, true));    // but to the dead
}

// ---- The emotes ----

void every_slot_has_its_names() {
  std::set<std::string> folders, clips, keys;
  for (const EmoteAsset& e : kEmotes) {
    folders.insert(e.folder);
    clips.insert(e.clip);
    keys.insert(e.nameKey);
    CHECK(clip_path(e).starts_with("assets/emotes/clips/") && clip_path(e).ends_with(".anim"));
    CHECK(clip_path(e).find_first_of("ABCDEFGHIJKLMNOPQRSTUVWXYZ") == std::string::npos);  // bundle paths are lower case
    CHECK(song_path(e) == std::string("assets/emotes/loops/") + e.folder + ".wav");  // cut to its loop (cut_loops.ps1)
    CHECK(icon_path(e) == std::string("assets/emotes/icons/") + e.folder + ".png");
    CHECK(loudness_path(e) == std::string(e.folder) + "/loudness.txt");
  }
  CHECK(folders.size() == kSegments && clips.size() == kSegments && keys.size() == kSegments);
  CHECK(std::string(kEmotes[0].clip) == "DiscoFever" && std::string(kEmotes[7].clip) == "TakeTheL");
  CHECK(clip_path(kEmotes[7]) == "assets/emotes/clips/takethel.anim");
  // The two songs whose loop is not their clip's (the author, 2026-11-10), measured from the tracks: both from 0 s.
  CHECK(kEmotes[2].songLoopStart == 0.0f && std::fabs(kEmotes[2].songLoopSeconds - 16.692f) < 1e-4f);  // Floss
  CHECK(kEmotes[7].songLoopStart == 0.0f && std::fabs(kEmotes[7].songLoopSeconds - 12.632f) < 1e-4f);  // Take The L
  // The rest, from the author's rough points (2026-11-10), measured on the waveform as Floss was.
  const float measured[] = {8.542f, 16.0f, 0, 15.118f, 18.935f, 32.542f, 19.2f};
  for (int i : {0, 1, 3, 4, 5, 6})
    CHECK(kEmotes[i].songLoopStart == 0.0f && std::fabs(kEmotes[i].songLoopSeconds - measured[i]) < 1e-4f);
}

}  // namespace

// The journal hangs from the rig's JournalParent, placed from the game's pose; the dance plays over the rig, so the mod
// keeps it where it was on the waist bone when the dance began.
void the_journal_keeps_its_place_on_the_waist() {
  const float h = 0.70710678f;
  const Pose waist{{0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 0.0f, 1.0f}};
  const Pose journal{{0.2f, 0.9f, 0.1f}, {0.0f, h, 0.0f, h}};  // turned 90 degrees about up
  const Pose held = relative_to(waist, journal);
  // The waist moves up 10 cm and turns 90 degrees about up: the journal moves and turns with it.
  const Pose danced{{0.0f, 1.1f, 0.0f}, {0.0f, h, 0.0f, h}};
  const Pose now = placed_on(danced, held);
  const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
  CHECK(near(now.p.x, 0.1f) && near(now.p.y, 1.0f) && near(now.p.z, -0.2f));  // (0.2, -0.1, 0.1) turned, then raised
  CHECK(near(std::fabs(now.q.y), 1.0f) && near(now.q.w, 0.0f));               // 180 degrees about up in all
  // Unmoved, it stays put.
  const Pose same = placed_on(waist, held);
  CHECK(near(same.p.x, 0.2f) && near(same.p.y, 0.9f) && near(same.p.z, 0.1f));
}

int main() {
  the_journal_keeps_its_place_on_the_waist();
  the_wheel_picks_by_direction();
  the_mouse_reach_is_capped();
  release_plays_the_pick();
  release_with_nothing_picked_plays_nothing();
  an_interrupted_wheel_plays_nothing();
  the_body_follows_past_ninety();
  only_a_move_past_half_a_metre_ends_it();
  a_looping_song_restarts_with_its_clip();
  a_song_loops_at_its_own_points();
  messages_round_trip();
  bad_messages_are_dropped();
  the_dead_dance_for_the_dead();
  every_slot_has_its_names();
  std::printf("%s\n", g_failures ? "emotes tests FAILED" : "emotes tests passed");
  return g_failures ? 1 : 0;
}
