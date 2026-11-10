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

EmoteTable table() {
  EmoteTable t{};
  for (int i = 0; i < kSegments; ++i) t[i] = {2.0f, true, SongLoop{0.0f, 2.0f}};
  t[5] = {3.0f, false, SongLoop{0.0f, 3.0f}};  // a one-shot
  t[2] = {1.6f, true, SongLoop{0.0f, 16.7f}};  // a song that loops on its own (Floss)
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

// ---- The emoter's client ----

void a_pick_starts_and_tells_everyone() {
  LocalSession s(table());
  LocalFrame f = here(-1.0);
  f.viewYaw = 30.0f;
  const LocalOut o = s.pick(4, f);
  CHECK(o.start == 4 && !o.stop);
  CHECK(o.send.size() == 1 && o.send[0].kind == Kind::Start && o.send[0].emote == 4);
  CHECK(o.send.size() == 1 && o.send[0].x == 1.0f && o.send[0].yaw == 30.0f);
  CHECK(s.active() && s.emote() == 4 && s.yaw() == 30.0f);
  // Nothing without a local investigator (the ghost player of Playable Ghost has none).
  LocalSession none(table());
  LocalFrame gone = here();
  gone.present = false;
  CHECK(none.pick(1, gone).start == -1 && !none.active());
}

// The user's ruling (2026-11-10): the dance and its song wait for the view to be out, so the song does not start at
// the camera.
void a_pick_waits_for_the_view() {
  LocalSession s(table());
  LocalFrame f = here(-1.0);
  LocalOut o = s.pick(3, f, 0.3f);
  CHECK(o.viewOut && o.start == -1 && o.send.empty());  // the view goes out; nothing plays or is sent yet
  CHECK(s.waiting() && !s.active());
  f.dt = 0.2f;
  o = s.tick(f);
  CHECK(o.start == -1 && o.send.empty() && !o.stop);
  f.dt = 0.15f;
  o = s.tick(f);  // 0.35 s: the view is out
  CHECK(o.start == 3 && has(o.send, Kind::Start));
  CHECK(s.active() && !s.waiting());
  // With the view already out, a pick plays at once.
  o = s.pick(5, here(), 0.0f);
  CHECK(o.start == 5 && !o.viewOut);
}

void what_ends_a_dance_also_ends_the_wait() {
  LocalSession s(table());
  s.pick(1, here(-1.0), 0.3f);
  LocalFrame f = here(-1.0);
  f.dt = 0.1f;
  f.endAction = true;  // or the journal opening
  LocalOut o = s.tick(f);
  CHECK(o.stop && o.send.empty());  // the view comes back; nobody was told of a dance
  CHECK(!s.waiting() && !s.active());
  f.endAction = false;
  f.dt = 1.0f;
  CHECK(s.tick(f).start == -1);  // and it never starts
  // Moving past half a metre during the wait ends it the same way.
  s.pick(1, here(-1.0), 0.3f);
  f = here(-1.0);
  f.dt = 0.1f;
  f.pos.x += 0.6f;
  CHECK(s.tick(f).stop && !s.waiting());
  // The mod stopping ends it too.
  s.pick(1, here(-1.0), 0.3f);
  o = s.end_now();
  CHECK(o.stop && o.send.empty() && !s.waiting());
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

void a_loop_keeps_going_and_its_song_restarts() {
  LocalSession s(table());
  s.pick(0, here());
  CHECK(!s.tick(here(1.9)).restartSong);
  const LocalOut o = s.tick(here(2.05));
  CHECK(!o.stop && o.restartSong);  // the clip looped: so does the song
  CHECK(!s.tick(here(3.0)).restartSong);
  CHECK(!s.tick(here(30.0)).stop && s.active());
}

void a_song_with_its_own_loop_restarts_at_its_own_point() {
  LocalSession s(table());
  s.pick(2, here());
  for (double t = 0.1; t < 16.6; t += 0.1) CHECK(!s.tick(here(t)).restartSong);  // the clip loops ten times
  CHECK(s.tick(here(16.75)).restartSong);
}

void a_new_pick_cuts_in() {
  LocalSession s(table());
  s.pick(0, here());
  s.tick(here(0.5));
  const LocalOut o = s.pick(3, here(0.6));
  CHECK(o.start == 3 && !o.stop);  // one clip and one song: the new one replaces the old, no stop between
  CHECK(o.send.size() == 1 && o.send[0].kind == Kind::Start && o.send[0].emote == 3);
  // The song restart counts from the new clip's own time.
  CHECK(!s.tick(here(0.1)).restartSong);
}

void death_ends_the_emote_first() {
  LocalSession s(table());
  s.pick(0, here());
  LocalFrame f = here(0.5);
  f.dead = true;
  const LocalOut o = s.tick(f);
  CHECK(o.stop && has(o.send, Kind::End));
  // A dead player may emote; their own later death is no death.
  LocalSession ghost(table());
  ghost.pick(1, f);
  CHECK(!ghost.tick(f).stop);
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
  auto out = r.receive(7, EmoteMsg{Kind::Start, 2, 0, 0, 0, 45.0f}, false, false);
  CHECK(did(out, RemoteOut::Start, 7) && did(out, RemoteOut::Yaw, 7));
  CHECK(r.dancing(7));
  out = r.receive(7, EmoteMsg{Kind::Facing, 0, 0, 0, 0, 90.0f}, false, false);
  CHECK(did(out, RemoteOut::Yaw, 7) && out.back().yaw == 90.0f);
  out = r.receive(7, EmoteMsg{Kind::End, 0, 0, 0, 0, 0}, false, false);
  CHECK(did(out, RemoteOut::Stop, 7) && !r.dancing(7));
  // A Facing or End for a peer not dancing is passed over.
  CHECK(r.receive(9, EmoteMsg{Kind::Facing, 0, 0, 0, 0, 10.0f}, false, false).empty());
}

void the_living_never_see_the_dead_dance() {
  RemoteSessions r(table());
  CHECK(r.receive(3, EmoteMsg{Kind::Start, 1, 0, 0, 0, 0}, true, false).empty());  // a dead emoter, a living viewer
  CHECK(!r.dancing(3));
  CHECK(!r.receive(3, EmoteMsg{Kind::Start, 1, 0, 0, 0, 0}, true, true).empty());  // the dead see the dead
  // A living emoter who dies mid-dance stops for the living.
  RemoteSessions live(table());
  live.receive(4, EmoteMsg{Kind::Start, 1, 0, 0, 0, 0}, false, false);
  const auto out = live.tick({RemoteFrame{4, true, true, 0.5}}, false);
  CHECK(did(out, RemoteOut::Stop, 4) && !live.dancing(4));
}

void a_vanished_emoter_is_cleaned_up() {
  RemoteSessions r(table());
  r.receive(5, EmoteMsg{Kind::Start, 0, 0, 0, 0, 0}, false, false);
  const auto out = r.tick({RemoteFrame{5, false, false, -1.0}}, false);  // left the room, or the scene changed
  CHECK(did(out, RemoteOut::Stop, 5) && !r.dancing(5));
  // One not in the frame at all is gone too.
  r.receive(6, EmoteMsg{Kind::Start, 0, 0, 0, 0, 0}, false, false);
  CHECK(did(r.tick({}, false), RemoteOut::Stop, 6));
}

void a_peers_song_loops_with_its_clip() {
  RemoteSessions r(table());
  r.receive(2, EmoteMsg{Kind::Start, 0, 0, 0, 0, 0}, false, false);
  CHECK(!did(r.tick({RemoteFrame{2, true, false, 1.9}}, false), RemoteOut::RestartSong, 2));
  CHECK(did(r.tick({RemoteFrame{2, true, false, 2.05}}, false), RemoteOut::RestartSong, 2));
  // A one-shot peer's clip that played out ends for this client too.
  r.receive(8, EmoteMsg{Kind::Start, 5, 0, 0, 0, 0}, false, false);
  CHECK(did(r.tick({RemoteFrame{8, true, false, 3.1}}, false), RemoteOut::Stop, 8));
}

}  // namespace

int main() {
  a_pick_waits_for_the_view();
  what_ends_a_dance_also_ends_the_wait();
  a_pick_starts_and_tells_everyone();
  moving_ends_the_emote();
  each_listed_action_ends_it();
  switching_items_does_not();
  a_one_shot_ends_by_itself();
  a_loop_keeps_going_and_its_song_restarts();
  a_song_with_its_own_loop_restarts_at_its_own_point();
  a_new_pick_cuts_in();
  death_ends_the_emote_first();
  the_body_turns_and_is_shared();
  a_lost_investigator_ends_it();
  a_peer_dances_where_told();
  the_living_never_see_the_dead_dance();
  a_vanished_emoter_is_cleaned_up();
  a_peers_song_loops_with_its_clip();
  std::printf("%s\n", g_failures ? "emotes_play tests FAILED" : "emotes_play tests passed");
  return g_failures ? 1 : 0;
}
