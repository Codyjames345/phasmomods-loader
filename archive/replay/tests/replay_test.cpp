// Offline tests for the contract's top moments (mods/replay/moments.h). Run: build\replay_test.exe
#include "moments.h"

#include "check.h"

using namespace pg::replay;

namespace {

/** A contract of `n` frames with two investigators standing far from the ghost. */
Recording quiet(int n) {
  Recording r;
  r.actors = {1, 2};
  for (int i = 0; i < n; ++i) {
    Frame f;
    f.flags = kGhostPresent;
    f.present = 0b11;
    f.ghost = {0, 0, 0};
    f.players[0] = {30, 0, 0};
    f.players[1] = {0, 0, 30};
    r.frames.push_back(f);
  }
  return r;
}

void a_quiet_contract_has_nothing_to_show() { CHECK(find_moments(quiet(100)).empty()); }

void a_kill_is_the_best_moment() {
  Recording r = quiet(100);
  for (int i = 40; i < 100; ++i) r.frames[i].dead |= 0b10;  // the second investigator dies at frame 40
  const std::vector<Moment> m = find_moments(r);
  CHECK(m.size() == 1 && m[0].kind == kKill && m[0].subject == 1 && m[0].peak == 40);
  CHECK(m[0].first == 24 && m[0].last == 44);  // 8 s before, 2 s after
}

void someone_first_seen_dead_was_not_killed_here() {
  Recording r = quiet(100);
  for (int i = 0; i < 30; ++i) r.frames[i].present = 0b01;  // the second player appears at frame 30 ...
  for (int i = 30; i < 100; ++i) r.frames[i].dead |= 0b10;  // ... already dead
  CHECK(find_moments(r).empty());
}

void a_hunt_gives_an_escape_and_a_chase() {
  Recording r = quiet(100);
  for (int i = 10; i < 30; ++i) {
    r.frames[i].flags |= kHunting;
    r.frames[i].players[0] = {static_cast<float>(i == 20 ? 1 : 4), 0, 0};  // close all hunt, 1 m once
  }
  r.frames[20].players[1] = {0, 5.0f, 0.5f};  // right below on another floor: not close
  const std::vector<Moment> m = find_moments(r);
  CHECK(m.size() == 1);  // the escape and the chase overlap: the better one stays
  CHECK(m[0].kind == kEscape && m[0].subject == 0 && m[0].peak == 20);
}

void a_near_miss_must_be_near() {
  Recording r = quiet(100);
  for (int i = 10; i < 30; ++i) {
    r.frames[i].flags |= kHunting;
    r.frames[i].players[0] = {3, 0, 0};  // chased at 3 m, never closer
  }
  const std::vector<Moment> m = find_moments(r);
  CHECK(m.size() == 1 && m[0].kind == kChase);
}

void the_best_moments_are_shown_in_order() {
  Recording r = quiet(400);
  r.crucifixes = {150 * kStepMs};
  for (int i = 300; i < 400; ++i) r.frames[i].dead |= 0b01;
  for (int i = 50; i < 54; ++i) {
    r.frames[i].flags |= kEvent;
    r.frames[i].players[1] = {0, 0, 2};
  }
  const std::vector<Moment> m = find_moments(r);
  CHECK(m.size() == 3);
  CHECK(m[0].kind == kCloseEvent && m[1].kind == kCrucifixSave && m[2].kind == kKill);
}

void a_replay_survives_the_trip() {
  Recording r = quiet(100);
  for (int i = 60; i < 100; ++i) r.frames[i].dead |= 0b01;
  r.frames[55].ghost = {-3.25f, 1.5f, 7.75f};
  const std::vector<Box> rooms = {{0, -5, -5, 5, 5}, {1, 0, 0, 2.5f, 3}};
  const Replay sent = make_replay(r, 2, rooms);
  CHECK(sent.moments.size() == 1 && sent.moments[0].frames.size() == 21 && sent.moments[0].moment.peak == 16);
  const std::vector<uint8_t> blob = encode(sent);
  std::vector<uint8_t> whole;
  Assembler a;
  const auto parts = chunks(blob);
  for (size_t i = parts.size(); i-- > 0;)  // out of order
    CHECK(a.add(7, static_cast<uint16_t>(i), static_cast<uint16_t>(parts.size()), parts[i].data(), parts[i].size(), whole) == (i == 0));
  CHECK(whole == blob);
  Replay got;
  CHECK(decode(whole.data(), whole.size(), got));
  CHECK(got.actors == sent.actors && got.ghostActor == 2 && got.rooms.size() == 2 && got.rooms[1].x1 == 2.5f);
  CHECK(got.moments.size() == 1 && got.moments[0].frames == sent.moments[0].frames);
  CHECK(got.moments[0].moment.kind == kKill && got.moments[0].moment.peak == 16);
  std::vector<uint8_t> broken = blob;
  broken.pop_back();
  CHECK(!decode(broken.data(), broken.size(), got));
  broken = blob;
  broken.push_back(0);
  CHECK(!decode(broken.data(), broken.size(), got));
  CHECK(!a.add(1, 3, 2, blob.data(), 4, whole));  // a chunk past the count
}

void captions_name_the_investigator() {
  CHECK(caption(kKill, "Sam") == "Sam was killed by the ghost");
  CHECK(caption(kChase, "Ash") == "The ghost chased Ash");
  CHECK(caption(kCrucifixSave, "x") == "A crucifix stopped a hunt");
  CHECK(caption(99, "x").empty());
}

}  // namespace

int main() {
  a_quiet_contract_has_nothing_to_show();
  a_kill_is_the_best_moment();
  someone_first_seen_dead_was_not_killed_here();
  a_hunt_gives_an_escape_and_a_chase();
  a_near_miss_must_be_near();
  the_best_moments_are_shown_in_order();
  a_replay_survives_the_trip();
  captions_name_the_investigator();
  std::printf(g_failures ? "%d check(s) failed\n" : "all replay tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
