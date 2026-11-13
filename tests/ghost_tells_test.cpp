// Offline tests for the Ghost Tells rules (mods/ghost_tells/*.h), and for the tells in the game over the
// fake backend (sdk/fake): the world, the gate, the hunt's inputs and the Mare's roll.
// Run: build\ghost_tells_test.exe
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "LightSwitch.h"
#include "Photon.h"
#include "Player.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "fake_game.h"
#include "game_api.h"
#include "gate.h"
#include "gate_rule.h"
#include "hold.h"
#include "mare.h"
#include "register.h"
#include "rolls.h"
#include "spirit.h"
#include "runtime.h"
#include "world.h"
#include "goryo_rule.h"
#include "journal_text.h"
#include "mare_rule.h"
#include "onryo_rule.h"
#include "shade_rule.h"
#include "spirit_rule.h"
#include "tells.h"
#include "tells_net.h"
#include "yurei_rule.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::tells;

namespace {

constexpr float kFrame = 1.0f / 30;

void the_six_ghosts_are_told_and_no_other() {
  CHECK(told_of("vanilla.spirit") == kSpirit && told_of("vanilla.mare") == kMare);
  CHECK(told_of("vanilla.shade") == kShade && told_of("vanilla.yurei") == kYurei);
  CHECK(told_of("vanilla.goryo") == kGoryo && told_of("vanilla.onryo") == kOnryo);
  CHECK(told_of("vanilla.wraith") == -1);
  CHECK(told_of("phasmomods.custom-ghosts.brownie") == -1);  // a custom ghost, whatever its AI follows
  CHECK(told_of("") == -1);
  CHECK(kToldCount == 6);
}

void an_edge_rises_once() {
  Edge e;
  CHECK(!e.rose(false));
  CHECK(e.rose(true));
  CHECK(!e.rose(true));  // still set: no second rise
  CHECK(!e.rose(false));
  CHECK(e.rose(true));
}

void a_timer_runs_for_its_seconds() {
  Timer t;
  CHECK(!t.running());
  t.start(1.0f);
  float ran = 0;
  while (t.step(kFrame)) ran += kFrame;
  CHECK(ran > 0.95f && ran < 1.05f);
  CHECK(!t.running());
  t.start(5.0f);
  t.stop();
  CHECK(!t.running());
}

void messages_round_trip() {
  {
    const std::vector<uint8_t> m = net::name_heard();
    CHECK(net::kind_of(m.data(), m.size()) == net::kName);
    CHECK(net::read_name_heard(m.data(), m.size()));
  }
  {
    const std::vector<uint8_t> m = net::trapped();
    CHECK(net::kind_of(m.data(), m.size()) == net::kTrap);
    CHECK(net::read_trapped(m.data(), m.size()));
  }
  {
    const std::vector<uint8_t> m = net::goryo_show(true);
    bool on = false;
    CHECK(net::read_goryo_show(m.data(), m.size(), on) && on);
    const std::vector<uint8_t> off = net::goryo_show(false);
    CHECK(net::read_goryo_show(off.data(), off.size(), on) && !on);
  }
  {
    const std::vector<uint8_t> m = net::onryo_count(2);
    int count = -1;
    CHECK(net::read_onryo_count(m.data(), m.size(), count) && count == 2);
  }
  {
    const std::vector<uint8_t> m = net::onryo_spite(123456, 14.95f);
    int view = 0;
    float seconds = 0;
    CHECK(net::read_onryo_spite(m.data(), m.size(), view, seconds) && view == 123456);
    CHECK(seconds > 14.8f && seconds < 15.0f);
  }
  const uint8_t flame[] = {'C', 'G', 'F', 1};
  CHECK(net::kind_of(flame, sizeof flame) == 0);  // another mod's message format
  const uint8_t shortOne[] = {'G'};
  CHECK(net::kind_of(shortOne, sizeof shortOne) == 0);
  CHECK(net::kind_of(nullptr, 0) == 0);
  // A message of one kind never reads as another, and a cut-off one never reads at all.
  const std::vector<uint8_t> count = net::onryo_count(1);
  bool on = false;
  CHECK(!net::read_goryo_show(count.data(), count.size(), on));
  std::vector<uint8_t> cut = net::onryo_spite(5, 3.0f);
  cut.pop_back();
  int view = 0;
  float seconds = 0;
  CHECK(!net::read_onryo_spite(cut.data(), cut.size(), view, seconds));
}

// Each told ghost's line goes to its own vanilla page, named by its id (the content layer finds the
// page; mod.cpp gives it the line).
void the_journal_names_the_six_pages_only() {
  for (int told = 0; told < kToldCount; ++told) CHECK(told_of(kToldIds[told]) == told);
  CHECK(std::string(kToldIds[kGoryo]) == "vanilla.goryo");
  CHECK(tell_line(kToldCount).empty() && tell_line(-1).empty());
}

void every_told_ghost_has_a_journal_line() {
  for (int told = 0; told < kToldCount; ++told) {
    const std::string s = tell_line(told);
    CHECK(s.size() > 20);
    CHECK(s.rfind("Tells: ", 0) == 0);  // after the page's own text, as before
    CHECK(s.find("\xE2\x80\x94") == std::string::npos);  // no em dash in player-facing copy
    CHECK(s.find("investigator") == std::string::npos);  // players, not investigators
  }
  CHECK(tell_line(6).empty());
  CHECK(tell_line(24).empty());
}


/** Steps a rule for `seconds` of frames; returns how many frames it reported true. */
template <class F>
int frames(float seconds, F step) {
  int n = 0;
  for (float t = 0; t + kFrame / 2 < seconds; t += kFrame) n += step() ? 1 : 0;
  return n;
}

void incense_stops_the_spirit_for_three_seconds_in_a_hunt() {
  IncenseStop stop;
  CHECK(stop.step(true, true, kFrame));  // the smudge lands mid-hunt
  const int held = frames(3.0f, [&] { return stop.step(true, true, kFrame); });
  CHECK(held >= 87 && held <= 90);  // about 3 s of frames, the first already counted
  CHECK(!stop.step(true, true, kFrame));  // 3 s on: it walks on, blind
  IncenseStop already;
  CHECK(!already.step(true, false, kFrame));  // incense outside a hunt
  CHECK(!already.step(true, true, kFrame));   // still set as the hunt begins: no new smudge in the hunt
  IncenseStop outside;
  CHECK(frames(2.0f, [&] { return outside.step(true, false, kFrame); }) == 0);
}

void incense_silences_the_spirit_for_a_minute() {
  Quiet q;
  q.incense(false, false);
  q.step(kFrame);
  CHECK(q.quiet());
  frames(59.0f, [&] { return q.step(kFrame); });  // the game's own hunt hold lasts longer
  CHECK(q.quiet());
  frames(1.1f, [&] { return q.step(kFrame); });
  CHECK(!q.quiet());  // a minute is over, though the game's hold may still be on
  q.incense(false, true);  // incense again while the game's hold is still on: no new hold, no new minute
  q.step(kFrame);
  CHECK(!q.quiet());
  q.incense(false, false);  // once the hold has ended, incense starts a new minute
  CHECK(q.quiet());
  Quiet inHunt;
  inHunt.incense(true, false);
  inHunt.step(kFrame);
  CHECK(!inHunt.quiet());  // incense in a hunt stops it instead
  inHunt.step(kFrame);
  CHECK(!inHunt.quiet());  // the same incense, after the hunt: not a new one
}

void the_spirit_hears_its_name_as_a_whole_word() {
  CHECK(says_name("hey thomas are you here", "Thomas Miller"));
  CHECK(says_name("THOMAS MILLER", "Thomas Miller"));
  CHECK(says_name("is that you miller", "Thomas Miller") == false);  // the first name, or the whole name
  CHECK(says_name("thomas miller give us a sign", "Thomas Miller"));
  CHECK(!says_name("thomasina", "Thomas Miller"));
  CHECK(!says_name("", "Thomas Miller"));
  CHECK(!says_name("thomas", ""));
}

void the_spirit_knocks_once_after_its_name() {
  Answer a;
  CHECK(a.hear(true, 0.5f));  // due 2 s after the name
  const int early = frames(1.9f, [&] { return a.step(true, kFrame); });
  CHECK(early == 0);
  const int knocks = frames(0.3f, [&] { return a.step(true, kFrame); });
  CHECK(knocks == 1);
  CHECK(!a.hear(true, 0.0f));  // 20 s have not passed since the first name
  frames(18.0f, [&] { return a.step(true, kFrame); });
  CHECK(a.hear(true, 0.0f));
  CHECK(frames(0.5f, [&] { return a.step(false, kFrame); }) == 0);  // a hunt began: no knock
  CHECK(frames(5.0f, [&] { return a.step(true, kFrame); }) == 0);   // and none later either
  Answer refused;
  CHECK(!refused.hear(false, 0.0f));
}


void the_mares_quick_switch_off_is_one_in_two() {
  CHECK(kMareSwitchOffOneIn == 2);
  CHECK(mare_roll(0) == 1);  // the coroutine switches the light off when its roll is 1
  CHECK(mare_roll(1) == 0);
  int offs = 0;
  for (int fresh = 0; fresh < kMareSwitchOffOneIn; ++fresh) offs += mare_roll(fresh) == 1 ? 1 : 0;
  CHECK(offs == 1);
}

void the_mare_never_shows_itself_in_a_lit_room() {
  CHECK(mare_appearance(false, 0, 0) == Appearance::Appear);
  CHECK(mare_appearance(true, 2, 2) == Appearance::DarkenFirst);
  CHECK(mare_appearance(true, 2, 1) == Appearance::Withhold);  // a broken switch keeps the room lit
  CHECK(mare_appearance(true, 0, 0) == Appearance::Withhold);  // lit by nothing it could switch off
}

void the_mares_trail_names_each_room_once() {
  RoomTrail t;
  t.pass(3);
  t.pass(3);
  t.pass(5);
  t.pass(3);
  t.pass(-1);  // outside every room
  CHECK((t.rooms() == std::vector<int>{3, 5}));
  t.clear();
  CHECK(t.rooms().empty());
}


void the_shade_vanishes_when_stared_at() {
  Gaze g;
  CHECK(frames(0.36f, [&] { return g.step(true, true, kFrame); }) == 0);  // a glance is not a stare
  CHECK(frames(0.1f, [&] { return g.step(true, true, kFrame); }) == 1);   // 0.4 s of staring
  CHECK(frames(1.0f, [&] { return g.step(true, true, kFrame); }) == 0);   // it is gone: once
  Gaze away;
  frames(0.3f, [&] { return away.step(true, true, kFrame); });
  away.step(true, false, kFrame);  // the player looks away
  CHECK(frames(0.3f, [&] { return away.step(true, true, kFrame); }) == 0);
  Gaze unseen;
  frames(0.3f, [&] { return unseen.step(true, true, kFrame); });
  unseen.step(false, true, kFrame);  // it stopped showing itself
  CHECK(frames(0.3f, [&] { return unseen.step(true, true, kFrame); }) == 0);
}

void the_shade_does_nothing_with_two_players_near() {
  CHECK(!crowded(0));
  CHECK(!crowded(1));
  CHECK(crowded(2));
  CHECK(crowded(4));
}

void the_shade_hesitates_at_a_group_once_in_a_while() {
  Falter f;
  CHECK(f.step(true, true, kFrame));  // a group in sight in a hunt
  CHECK(f.holding());
  frames(1.6f, [&] { return f.step(true, true, kFrame); });
  CHECK(!f.holding());  // 1.5 s, then on
  CHECK(frames(10.0f, [&] { return f.step(true, true, kFrame); }) == 0);  // not again so soon
  CHECK(frames(4.0f, [&] { return f.step(true, true, kFrame); }) == 1);   // 15 s after the first
  Falter ends;
  ends.step(true, true, kFrame);
  ends.step(false, false, kFrame);  // the hunt ends
  CHECK(!ends.holding());
  Falter alone;
  CHECK(frames(3.0f, [&] { return alone.step(true, false, kFrame); }) == 0);
}


/** Steps a door rule for `seconds` with fixed open flags; returns every index it gave, in order. */
template <class F>
std::vector<int> door_frames(float seconds, F step) {
  std::vector<int> out;
  for (float t = 0; t + kFrame / 2 < seconds; t += kFrame) step(out);
  return out;
}

void the_smudged_yurei_shuts_itself_in() {
  Seal s;
  CHECK(s.step({true, false}, kFrame).empty());  // no trap yet
  s.start();
  CHECK(s.active());
  CHECK((s.step({true, false, true}, kFrame) == std::vector<int>{0, 2}));  // every open door, at once
  const std::vector<bool> closed{false, false, false}, reopened{false, true, false};
  door_frames(1.0f, [&](std::vector<int>& out) { for (int i : s.step(closed, kFrame)) out.push_back(i); });
  // A player opens the middle door 1 s in: it is shut again 3 s later, not before.
  CHECK(door_frames(2.9f, [&](std::vector<int>& out) { for (int i : s.step(reopened, kFrame)) out.push_back(i); }).empty());
  CHECK((door_frames(0.2f, [&](std::vector<int>& out) { for (int i : s.step(reopened, kFrame)) out.push_back(i); }) ==
         std::vector<int>{1}));
  door_frames(90.0f, [&](std::vector<int>& out) { for (int i : s.step(closed, kFrame)) out.push_back(i); });
  CHECK(!s.active());
  CHECK(door_frames(5.0f, [&](std::vector<int>& out) { for (int i : s.step(reopened, kFrame)) out.push_back(i); }).empty());
}

void the_yureis_door_stays_locked_for_three_seconds() {
  Hold h;
  CHECK(!h.pending());
  h.lock();
  CHECK(h.pending());
  CHECK(frames(2.9f, [&] { return h.step(kFrame); }) == 0);
  CHECK(frames(0.2f, [&] { return h.step(kFrame); }) == 1);
  CHECK(!h.pending());
  CHECK(frames(2.0f, [&] { return h.step(kFrame); }) == 0);
}

void a_door_left_open_draws_the_yureis_ability() {
  Draw d;
  const std::vector<bool> open{false, true};
  int at = -2;
  CHECK(door_frames(19.9f, [&](std::vector<int>& out) {
          if (int i = d.step(open, true, kFrame); i >= 0) out.push_back(i);
        }).empty());
  CHECK((door_frames(0.2f, [&](std::vector<int>& out) {
          if (int i = d.step(open, true, kFrame); i >= 0) out.push_back(i);
        }) == std::vector<int>{1}));
  // Still open, but within a minute of the last draw: nothing until the minute is up.
  CHECK(door_frames(59.0f, [&](std::vector<int>& out) {
          if (int i = d.step(open, true, kFrame); i >= 0) out.push_back(i);
        }).empty());
  CHECK((door_frames(1.5f, [&](std::vector<int>& out) {
          if (int i = d.step(open, true, kFrame); i >= 0) out.push_back(i);
        }) == std::vector<int>{1}));
  Draw held;
  CHECK(door_frames(30.0f, [&](std::vector<int>& out) {
          if (int i = held.step(open, false, kFrame); i >= 0) out.push_back(i);
        }).empty());  // it may not act (trapped, hunting, an event)
  at = held.step(open, true, kFrame);
  CHECK(at == 1);  // the door was open all along
  Draw cleared;
  door_frames(15.0f, [&](std::vector<int>&) { cleared.step(open, true, kFrame); });
  cleared.reset();  // it moved to another room
  CHECK(cleared.step(open, true, 6.0f) == -1);
}


/** Steps a CameraShow for `seconds`; returns the sum of what it reported (+1 starts, -1 stops). */
int show_frames(CameraShow& c, float seconds, bool conditions, float roll) {
  int sum = 0;
  for (float t = 0; t + kFrame / 2 < seconds; t += kFrame) sum += c.step(conditions, kFrame, roll);
  return sum;
}

void the_goryo_stands_on_camera_now_and_then() {
  CameraShow c;
  CHECK(show_frames(c, 29.9f, true, 0.0f) == 0);  // 30 s of conditions first
  CHECK(show_frames(c, 0.2f, true, 0.0f) == 1);
  CHECK(c.showing());
  CHECK(show_frames(c, 3.8f, true, 0.0f) == 0);
  CHECK(show_frames(c, 0.3f, true, 0.0f) == -1);  // 4 s on camera
  CHECK(!c.showing());
  CHECK(show_frames(c, 50.0f, false, 0.0f) == 0);  // no time passes without the conditions
  CameraShow cut;
  show_frames(cut, 31.0f, true, 0.0f);
  CHECK(cut.showing());
  CHECK(cut.step(false, kFrame, 0.0f) == -1);  // a player came near: gone at once
  CameraShow late;
  CHECK(show_frames(late, 59.0f, true, 1.0f) == 0);  // the longest wait
  CHECK(show_frames(late, 1.5f, true, 1.0f) == 1);
  late.reset();
  CHECK(!late.showing());
}

void the_goryo_acts_more_on_camera() {
  Extra e;
  CHECK(frames(19.9f, [&] { return e.step(true, kFrame); }) == 0);
  CHECK(frames(20.2f, [&] { return e.step(true, kFrame); }) == 2);  // at 20 s and 40 s
  CHECK(frames(30.0f, [&] { return e.step(false, kFrame); }) == 0);
}

void the_goryo_goes_home_only_when_it_has_lost_everyone_far_away() {
  HomeSearch h;
  CHECK(!h.step(true, false, false, false, 11.9f));
  CHECK(h.step(true, false, false, false, 12.5f));
  CHECK(h.step(true, false, false, false, 5.0f));   // on its way home
  CHECK(!h.step(true, false, false, false, 3.0f));  // home: the vanilla hunt carries on
  CHECK(!h.step(true, false, false, false, 5.0f));
  for (float far : {12.5f, 30.0f, 80.0f}) CHECK(!h.step(true, true, false, false, far));  // prey: it chases, however far
  CHECK(h.step(true, false, false, false, 20.0f));
  CHECK(!h.step(true, true, false, false, 20.0f));  // someone seen on the way back: the chase is on
  CHECK(!h.step(true, false, false, true, 20.0f));  // a player steers the ghost
  CHECK(!h.step(false, false, false, false, 20.0f));  // not hunting
}


bool near(float a, float b) { return a > b - 0.02f && a < b + 0.02f; }

void the_onryo_snuffs_a_new_flame_one_time_in_two() {
  CHECK(near(snuff_delay(0.49f, 0.0f), 1.0f));
  CHECK(near(snuff_delay(0.2f, 1.0f), 2.0f));
  CHECK(snuff_delay(0.5f, 0.0f) < 0);  // the flame stays lit
  CHECK(snuff_delay(0.99f, 0.5f) < 0);
}

void every_flame_gutters_as_the_onryo_counts() {
  Flicker f;
  CHECK(near(f.light(), 1.0f) && near(f.scale(), 1.0f));
  f.count(1);  // a flame was blown out
  CHECK(near(f.light(), kGutterLow));
  frames(0.3f, [&] { f.step(kFrame); return false; });
  CHECK(f.light() > 0.5f && f.light() < 0.8f);
  frames(0.35f, [&] { f.step(kFrame); return false; });
  CHECK(near(f.light(), 1.0f) && near(f.scale(), 1.0f));  // recovered, burning as before
  f.count(2);
  frames(1.0f, [&] { f.step(kFrame); return false; });
  CHECK(near(f.light(), kBurnLowLight) && near(f.scale(), kBurnLowScale));  // two out: every flame burns low
  f.count(0);  // its count reset: no dip, back to full
  CHECK(near(f.light(), 1.0f) && near(f.scale(), 1.0f));
  f.count(2);
  f.reset();
  CHECK(near(f.light(), 1.0f) && near(f.scale(), 1.0f));
}

void a_flame_the_onryo_snuffed_will_not_catch_for_a_while() {
  Spite sp;
  CHECK(!sp.spited(7));
  sp.mark(7, kOnryoSpite);
  frames(14.9f, [&] { sp.step(kFrame); return false; });
  CHECK(sp.spited(7));
  CHECK(!sp.spited(8));
  frames(0.2f, [&] { sp.step(kFrame); return false; });
  CHECK(!sp.spited(7));
  sp.mark(9, 5.0f);
  sp.clear();
  CHECK(!sp.spited(9));
}


void the_gate_holds_back_only_the_ghosts_own_attempts() {
  // The numbers build 25811160's GhostState enum gives the states (gate.cpp reads them from the game).
  GateStates s;
  s.hunting = 2;
  s.events = {8, 14, 15};
  s.interactions = {4, 5, 6, 7, 9, 10, 11, 12, 13, 16, 17, 18, 23};
  s.pulls = {19, 20};
  CHECK(gate_kind(s, 2) == kGateHunt);
  CHECK(gate_kind(s, 14) == kGateEvent && gate_kind(s, 15) == kGateEvent && gate_kind(s, 8) == kGateEvent);
  CHECK(gate_kind(s, 9) == kGateInteract && gate_kind(s, 4) == kGateInteract && gate_kind(s, 7) == kGateInteract);
  CHECK(gate_kind(s, 0) == -1 && gate_kind(s, 1) == -1 && gate_kind(s, 3) == -1);  // idle, wander, favourite room
  // What PmGhostDef.heldBack asks of it: the fuse box as the interaction the gate holds it as, its ability alone not.
  CHECK(gate_held_kind(0) == kGateInteract && gate_held_kind(1) == kGateEvent && gate_held_kind(2) == kGateHunt);
  CHECK(gate_held_kind(4) == kGateInteract && gate_held_kind(3) == -1 && gate_held_kind(-1) == -1);
  CHECK(gate_kind(s, 19) == -1);  // the summoning circle's answer
  CHECK(gate_kind(s, 20) == -1);  // the music box's pull
  CHECK(gate_kind(s, 24) == -1);  // a state the enum may add
  CHECK(gate_may_hold(s, 2, false, false));
  CHECK(!gate_may_hold(s, 2, true, false));   // the Death card's forced hunt
  CHECK(!gate_may_hold(s, 14, true, false));  // the Devil card's forced event
  CHECK(!gate_may_hold(s, 9, false, true));   // nothing reaches into a running hunt
  CHECK(!gate_may_hold(s, 20, false, false));
  CHECK(cursed_pull(s, 19) && cursed_pull(s, 20));  // a possession made it show itself: no tell changes that
  CHECK(!cursed_pull(s, 14) && !cursed_pull(s, 8) && !cursed_pull(s, 0));
  // Before the numbers are read nothing is held back, and no unread number matches.
  const GateStates unread;
  CHECK(gate_kind(unread, 0) == -1 && gate_kind(unread, 2) == -1 && !cursed_pull(unread, 0));
}

void the_yureis_door_pick_is_steered_only_after_its_idle_change() {
  Steer st;
  st.aim(2, 4);
  st.begin();
  int r = -1;
  CHECK(!st.take(0, 4, r));  // the idle state's own animator roll, inside the nested ChangeState(0)
  st.idle_done();
  CHECK(st.take(0, 4, r) && r == 2);  // the ability's door roll
  CHECK(!st.take(0, 4, r));  // one door per ability
  st.end();
  Steer wrong;
  wrong.aim(1, 3);
  wrong.begin();
  wrong.idle_done();
  CHECK(!wrong.take(0, 4, r));  // another room's door count: left to the game
  Steer unaimed;
  unaimed.begin();
  unaimed.idle_done();
  CHECK(!unaimed.take(0, 4, r));  // a vanilla ability: never steered
}

void a_room_change_keeps_the_yureis_minute() {
  Draw d;
  const std::vector<bool> open{true};
  int drawn = 0;
  for (float t = 0; t < 21.0f; t += kFrame) drawn += d.step(open, true, kFrame) >= 0 ? 1 : 0;
  CHECK(drawn == 1);
  d.reset();  // it stepped into the hallway and back
  drawn = 0;
  for (float t = 0; t < 30.0f; t += kFrame) drawn += d.step(open, true, kFrame) >= 0 ? 1 : 0;
  CHECK(drawn == 0);  // still within a minute of the last draw
}

void a_searching_goryo_is_still_after_its_prey() {
  HomeSearch h;
  CHECK(!h.step(true, false, true, false, 20.0f));  // it is searching where its prey went
  CHECK(h.step(true, false, false, false, 20.0f));  // the search is over: home
  CHECK(!h.step(true, false, true, false, 20.0f));  // it heard someone: the search is on again
}

void the_mares_roll_is_answered_only_inside_its_coroutine() {
  CHECK(mare_roll_applies(true, 0, 7));
  CHECK(!mare_roll_applies(false, 0, 7));
  CHECK(!mare_roll_applies(true, 0, 4));
}

}  // namespace

// ---- The tells in the game, over the fake backend (sdk/fake): the game reached only through map entries.

namespace game {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;
namespace world = pm::tells::world;
namespace gate = pm::tells::gate;
namespace hold = pm::tells::hold;
namespace rolls = pm::tells::rolls;
namespace mare = pm::tells::mare;

int g_toldNow = -1;  // what told_now answers (mod.cpp answers it in the mod)

// A fake managed object: its class pointer first, as a real one's, then room for its fields.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x58]{};
};

template <class T>
void put(Obj& o, int32_t offset, T value) {
  std::memcpy(reinterpret_cast<char*>(&o) + offset, &value, sizeof value);
}

int g_hingeClass = 0;  // a fake class

// A contract: the level, its ghost in one room, two players (actors 1 and 2, the second dead), and the
// GhostState numbers of build 25811160.
struct World {
  Obj level, ghost, info, room, collider, activity, network, spot1, spot2, player1, player2, photon1, photon2, hunt;
  void* levelSlot = &level;
  std::vector<std::pair<const void*, phasmo::Vector3>> positions;
  std::vector<int32_t> changes;  // the states ChangeState was asked for, as the game ran them
  int interacts = 0;
  int32_t gameRoll = 3;  // what the game's own Random.RandomRangeInt answers
  int32_t switchRoll = -1;  // the roll the switch-off coroutine's first step got, inside UseLightSwitch
  static inline World* current = nullptr;

  World() {
    current = this;
    fake::static_field(map::LevelController_instance, &levelSlot, sizeof levelSlot, true);
    fake::field(map::LevelController_currentGhost, 0x10);
    fake::field(map::LevelController_currentGhostRoom, 0x18);
    fake::field(map::LevelController_rooms, 0x20);
    fake::field(map::GhostAI_info, 0x10);
    fake::field(map::GhostAI_state, 0x18);
    fake::field(map::GhostAI_isHunting, 0x1C);
    fake::field(map::GhostAI_isAppeared, 0x1D);
    fake::field(map::GhostAI_activity, 0x20);
    fake::field(map::GhostAI_huntsHeldOff, 0x1E);
    fake::field(map::GhostAI_smudge, 0x30);
    fake::field(map::GhostInfo_favouriteRoom, 0x10);
    fake::field(map::LevelRoom_roomColliders, 0x10);
    fake::field(map::Network_playerSpots, 0x10);
    fake::field(map::PlayerSpot_player, 0x10);
    fake::field(map::PlayerSpot_photonPlayer, 0x18);
    fake::field(map::Player_isDead, 0x10);
    fake::field(map::GhostHuntState_ghost, 0x10);
    fake::field(map::GhostHuntState_target, 0x18);
    fake::field(map::GhostHuntState_searchTarget, 0x20);
    const std::pair<const pm::sdk::EntryDesc*, int64_t> states[] = {
        {&map::GhostState_idle, 0},         {&map::GhostState_wander, 1},       {&map::GhostState_hunting, 2},
        {&map::GhostState_favouriteRoom, 3}, {&map::GhostState_light, 4},        {&map::GhostState_door, 5},
        {&map::GhostState_throwing, 6},     {&map::GhostState_fusebox, 7},      {&map::GhostState_appear, 8},
        {&map::GhostState_doorKnock, 9},    {&map::GhostState_windowKnock, 10}, {&map::GhostState_carAlarm, 11},
        {&map::GhostState_flicker, 12},     {&map::GhostState_cctv, 13},        {&map::GhostState_randomEvent, 14},
        {&map::GhostState_GhostAbility, 15}, {&map::GhostState_mannequin, 16},  {&map::GhostState_teleportObject, 17},
        {&map::GhostState_interact, 18},    {&map::GhostState_summoningCircle, 19}, {&map::GhostState_musicBox, 20},
        {&map::GhostState_ignite, 23}};
    for (const auto& [d, v] : states) fake::value(*d, v);
    fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&network_instance));
    fake::method(map::Component_get_transform, reinterpret_cast<void*>(&itself));
    fake::method(map::Transform_get_position, reinterpret_cast<void*>(&position));
    fake::method(map::Collider_get_bounds, reinterpret_cast<void*>(&bounds));
    fake::method(map::PhotonNetwork_get_IsMasterClient, reinterpret_cast<void*>(&is_master));
    fake::method(map::PhotonNetwork_get_LocalPlayer, reinterpret_cast<void*>(&local_player));
    fake::method(map::PhotonPlayer_get_ActorNumber, reinterpret_cast<void*>(&actor_number));
    fake::method(map::GhostAI_ChangeState, reinterpret_cast<void*>(&change_state));
    fake::method(map::GhostActivity_TryInteract, reinterpret_cast<void*>(&try_interact));
    fake::method(map::GhostHuntState_UpdateSpeed, reinterpret_cast<void*>(&update_speed));
    fake::method(map::Random_RandomRangeInt, reinterpret_cast<void*>(&random_range_int));
    fake::method(map::LightSwitch_UseLightSwitch, reinterpret_cast<void*>(&use_light_switch));
    fake::method(map::GhostAI_Appear, reinterpret_cast<void*>(&appear));
    fake::method(map::GhostAI_StopGhostFromHunting, reinterpret_cast<void*>(&stop_hunting));
    fake::method(map::PhraseListenerController_OnPhraseRecognized, reinterpret_cast<void*>(&phrase));

    put(level, 0x10, static_cast<void*>(&ghost));
    put(level, 0x18, static_cast<void*>(&room));
    put(ghost, 0x10, static_cast<void*>(&info));
    put(ghost, 0x20, static_cast<void*>(&activity));
    put(info, 0x10, static_cast<void*>(&room));
    put(room, 0x10, fake::list({&collider}));
    put(network, 0x10, fake::list({&spot1, &spot2}));
    put(spot1, 0x10, static_cast<void*>(&player1));
    put(spot1, 0x18, static_cast<void*>(&photon1));
    put(spot2, 0x10, static_cast<void*>(&player2));
    put(spot2, 0x18, static_cast<void*>(&photon2));
    put(player2, 0x10, true);  // dead
    put(hunt, 0x10, static_cast<void*>(&ghost));
    put(hunt, 0x18, static_cast<void*>(&player1));
    put(hunt, 0x20, static_cast<void*>(&player2));
    place(&ghost, {1, 0, 1});
    place(&player1, {2, 0, 1});
    place(&player2, {30, 0, 1});
    // The runtime services fill an array only once the game API is bound.
    std::string missing;
    api::bind(fake::host(), missing);
    void* rooms = fake::array(sizeof(void*), 1);
    api::RefArray{rooms}.set(0, &room);
    put(level, 0x20, rooms);
  }
  ~World() { current = nullptr; }
  World(const World&) = delete;
  World& operator=(const World&) = delete;

  void place(const void* o, phasmo::Vector3 at) { positions.emplace_back(o, at); }

  static void* network_instance(const void*) { return &current->network; }
  static void* itself(void* component, const void*) { return component; }
  static phasmo::Vector3 position(void* t, const void*) {
    for (const auto& [o, at] : current->positions)
      if (o == t) return at;
    return {100, 0, 100};
  }
  static phasmo::Bounds bounds(void*, const void*) { return {{0, 0, 0}, {5, 5, 5}}; }
  static bool is_master(const void*) { return true; }
  static void* local_player(const void*) { return &current->photon1; }
  static int32_t actor_number(void* p, const void*) { return p == &current->photon1 ? 1 : p == &current->photon2 ? 2 : 0; }
  static void change_state(void*, int32_t state, void*, bool, const void*) { current->changes.push_back(state); }
  static void try_interact(void*, const void*) { ++current->interacts; }
  static void update_speed(void*, const void*) {}
  static int32_t random_range_int(int32_t, int32_t, const void*) { return current->gameRoll; }
  static void appear(void*, int32_t, const void*) {}
  static void* stop_hunting(void*, const void*) { return nullptr; }  // the coroutine the caller would start
  static void phrase(void*, void*, const void*) {}
  // The switch-off coroutine's first step runs inside UseLightSwitch and rolls Random.Range(0, 7).
  static void use_light_switch(void*, bool, bool, bool, const void*) {
    current->switchRoll = hooked<phasmo::Random::RandomRangeInt, int32_t (*)(int32_t, int32_t, const void*)>()(0, 7, nullptr);
  }

  // The body the game reaches the hooked method M through, as Fn.
  template <auto& M, class Fn>
  static Fn hooked() {
    using W = std::remove_cvref_t<decltype(M)>;
    return reinterpret_cast<Fn>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0));
  }
};

void start() {
  world::reset();
  gate::tick(nullptr);
  hold::reset();
  g_toldNow = -1;
  fake::reset();
}

void bind() {
  std::string missing;
  api::bind(fake::host(), missing);
}

float g_deltaTime = 0;
float delta_time(const void*) { return g_deltaTime; }

// The tells' timers run on the frame's game time (Time.deltaTime, as pm::api::frame_time reads it), held to a quarter of a
// second after a hitch as the mod's own clock was; without the getter they stand still.
void the_tells_time_is_the_games_frame_time() {
  start();
  fake::method(map::Time_get_deltaTime, reinterpret_cast<void*>(&delta_time));
  bind();
  g_deltaTime = 0.02f;
  CHECK(world::frame_seconds() == 0.02f);
  g_deltaTime = 0.0f;  // the time scale at 0: the game's time stands still, and so do the tells'
  CHECK(world::frame_seconds() == 0.0f);
  g_deltaTime = 1.5f;  // a hitch
  CHECK(world::frame_seconds() == 0.25f);
  fake::forget(map::Time_get_deltaTime);
  bind();
  g_deltaTime = 0.02f;
  CHECK(world::frame_seconds() == 0.0f);
  // The one Time entry the tells read is the one they require: a build without the other getters still runs them.
  const char* const* keys = nullptr;
  const size_t n = api::used_required(&keys);
  bool delta = false, other = false;
  for (size_t i = 0; i < n; ++i) {
    const std::string key = keys[i];
    delta = delta || key == "Time.get_deltaTime";
    other = other || key == "Time.get_unscaledDeltaTime" || key == "Time.get_time" || key == "Time.get_unscaledTime";
  }
  CHECK(delta && !other);
}

void the_world_reads_the_ghost_its_room_and_the_players() {
  start();
  World w;
  bind();
  CHECK(world::ghost() == &w.ghost);
  CHECK(world::is_master() && world::local_actor() == 1);
  int players = 0, living = 0;
  world::each_player([&](void* p) {
    ++players;
    living += world::living(p) ? 1 : 0;
  });
  CHECK(players == 2 && living == 1);  // the second investigator is dead
  CHECK(world::player_of_actor(2) == &w.player2 && world::player_of_actor(9) == nullptr);
  CHECK(world::local_player() == &w.player1);
  CHECK(world::favourite_room(&w.ghost) == &w.room && world::ghost_room() == &w.room);
  CHECK(world::room_at({1, 0, 1}) == &w.room && world::room_at({9, 0, 9}) == nullptr);
  CHECK(world::room_index(&w.room) == 0 && world::room_by_index(0) == &w.room && world::room_by_index(1) == nullptr);
  CHECK(world::living_within({1, 0, 1}, 5) == 1);  // the living one, not the dead one far off
  CHECK(world::distance_to_room(&w.room, {8, 0, 0}) > 2.9f && world::distance_to_room(&w.room, {8, 0, 0}) < 3.1f);
  // Without the level the contract is over: no ghost.
  w.levelSlot = nullptr;
  CHECK(world::ghost() == nullptr && world::room_at({1, 0, 1}) == nullptr);
}

void the_hunt_is_the_one_the_speed_update_ran() {
  start();
  World w;
  bind();
  CHECK(world::create_hooks());
  put(w.ghost, 0x18, int32_t{2});  // hunting
  put(w.ghost, 0x1C, true);
  CHECK(world::hunt_target(&w.ghost) == nullptr);  // no speed update has run yet
  World::hooked<phasmo::GhostHuntState::UpdateSpeed, void (*)(void*, const void*)>()(&w.hunt, nullptr);
  CHECK(world::hunt_state(&w.ghost) == &w.hunt);
  CHECK(world::hunt_target(&w.ghost) == &w.player1 && world::hunt_search(&w.ghost) == &w.player2);
  put(w.ghost, 0x1C, false);  // the hunt is over
  CHECK(world::hunt_state(&w.ghost) == nullptr && world::hunt_target(&w.ghost) == nullptr);
  put(w.ghost, 0x1C, true);
  put(w.hunt, 0x10, static_cast<void*>(nullptr));  // another ghost's hunt
  CHECK(world::hunt_state(&w.ghost) == nullptr);
}

bool g_holdHunts = false, g_holdInteractions = false;
bool hold_rule(gate::Kind kind) {
  return (kind == gate::kHunt && g_holdHunts) || (kind == gate::kInteract && g_holdInteractions);
}

void the_gate_reads_the_states_and_holds_back_the_ghosts_own_attempts() {
  start();
  World w;
  bind();
  gate::resolve();
  const pm::tells::GateStates& s = gate::states();
  CHECK(s.hunting == 2 && s.events[0] == 8 && s.events[1] == 14 && s.events[2] == 15);
  CHECK(s.interactions[0] == 4 && s.interactions[12] == 23 && s.pulls[0] == 19 && s.pulls[1] == 20);
  CHECK(gate::create_hooks());
  static bool added = false;
  if (!added) gate::add_rule(&hold_rule);
  added = true;
  const auto change = World::hooked<phasmo::GhostAI::ChangeState, void (*)(void*, int32_t, void*, bool, const void*)>();
  const auto interact = World::hooked<phasmo::GhostActivity::TryInteract, void (*)(void*, const void*)>();
  g_holdHunts = g_holdInteractions = true;
  change(&w.ghost, 2, nullptr, false, nullptr);  // no told ghost yet: the game's own
  CHECK((w.changes == std::vector<int32_t>{2}));
  gate::tick(&w.ghost);
  CHECK(gate::held_back(gate::kHunt) && !gate::held_back(gate::kEvent));
  change(&w.ghost, 2, nullptr, false, nullptr);  // its own hunt: held back
  change(&w.ghost, 2, nullptr, true, nullptr);   // a forced one (the Death card): through
  change(&w.ghost, 0, nullptr, false, nullptr);  // idle: never held back
  Obj other;
  change(&other, 2, nullptr, false, nullptr);    // another ghost's: through
  CHECK((w.changes == std::vector<int32_t>{2, 2, 0, 2}));
  interact(&w.activity, nullptr);
  CHECK(w.interacts == 0);
  g_holdInteractions = false;
  interact(&w.activity, nullptr);
  CHECK(w.interacts == 1);
  g_holdHunts = false;
  gate::tick(nullptr);
}

void holding_still_gives_the_hunt_its_inputs() {
  start();
  float base = 1.7f, sight = 1.4f;
  CHECK(!hold::hunt_speed(&base, &sight));  // nothing asked: the game's own
  hold::hold_still(true);
  CHECK(hold::hunt_speed(&base, &sight) && base == 0.0f && sight == 1.4f);
  hold::reset_sight();
  base = 1.7f;
  CHECK(hold::hunt_speed(&base, &sight) && base == 0.0f && sight == 1.0f);
  sight = 1.3f;
  CHECK(hold::hunt_speed(&base, &sight) && sight == 1.3f);  // the reset is asked for once
  hold::hold_still(false);
  base = 1.7f;
  CHECK(!hold::hunt_speed(&base, &sight) && base == 1.7f);  // the game's base comes back
}

void the_mares_roll_is_answered_inside_use_light_switch() {
  start();
  World w;
  bind();
  static bool resolved = false;
  if (!resolved) mare::resolve();
  resolved = true;
  CHECK(rolls::create_hooks() && mare::create_hooks());
  const auto use = World::hooked<phasmo::LightSwitch::UseLightSwitch, void (*)(void*, bool, bool, bool, const void*)>();
  const auto roll = World::hooked<phasmo::Random::RandomRangeInt, int32_t (*)(int32_t, int32_t, const void*)>();
  Obj light;
  use(&light, true, false, false, nullptr);
  CHECK(w.switchRoll == 3);  // no Mare: the game's own roll
  g_toldNow = pm::tells::kMare;
  int ones = 0;
  for (int i = 0; i < 200; ++i) {
    use(&light, true, false, false, nullptr);
    CHECK(w.switchRoll == 0 || w.switchRoll == 1);
    ones += w.switchRoll == 1 ? 1 : 0;
  }
  CHECK(ones > 60 && ones < 140);  // 1 in 2, where the game's is 1 in 7
  CHECK(roll(0, 7, nullptr) == 3);  // the same roll outside UseLightSwitch is the game's
  CHECK(roll(0, 4, nullptr) == 3);
}

// The incense's request for the game's hunt hold starts the Spirit's quiet minute only when no hold was
// on: a second smudge while the game's hold runs starts no new hold, and no new minute.
void a_spirits_quiet_minute_starts_only_with_a_new_hold() {
  start();
  World w;
  static bool resolved = false;
  if (!resolved) pm::tells::spirit::resolve();
  resolved = true;
  CHECK(pm::tells::spirit::create_hooks());
  g_toldNow = pm::tells::kSpirit;
  pm::tells::spirit::tick_host(&w.ghost, 0.03f);
  gate::tick(&w.ghost);
  const auto stop = World::hooked<phasmo::GhostAI::StopGhostFromHunting, void* (*)(void*, const void*)>();
  put(w.ghost, 0x1E, true);  // the game's hold is already on
  stop(&w.ghost, nullptr);
  pm::tells::spirit::tick_host(&w.ghost, 0.03f);
  CHECK(!gate::held_back(gate::kInteract));
  put(w.ghost, 0x1E, false);  // no hold: this incense starts one
  stop(&w.ghost, nullptr);
  pm::tells::spirit::tick_host(&w.ghost, 0.03f);
  CHECK(gate::held_back(gate::kInteract) && gate::held_back(gate::kEvent) && !gate::held_back(gate::kHunt));
  put(w.ghost, 0x1E, true);
  for (int i = 0; i < 61 * 30; ++i) pm::tells::spirit::tick_host(&w.ghost, 1.0f / 30);
  CHECK(!gate::held_back(gate::kInteract));  // the minute is over
  stop(&w.ghost, nullptr);                    // incense again while the game's hold still runs
  pm::tells::spirit::tick_host(&w.ghost, 0.03f);
  CHECK(!gate::held_back(gate::kInteract));
  pm::tells::spirit::reset();
  gate::tick(nullptr);
}

// The mod's wiring (register.h, from pmmod_start): each told ghost gets its rules, the hunt speed on
// the Spirit and the Shade only, and its journal line. The host runs the rules and shows the lines only
// while the room's mod lists match (sdk/runtime/mod_agreement.h), which replaced the mod's own check:
// a peer without the mod means no lines and no rules; with it, both; a peer leaving mid-contract
// changes nothing; another build of the mod is not agreed.
int g_wiredStarts = 0;
void wired_start(void*) { ++g_wiredStarts; }
bool wired_speed(float*, float*, void*) { return false; }
bool wired_held(int32_t, void*) { return false; }

void the_tells_and_their_lines_run_only_while_the_room_agrees() {
  start();
  bind();
  g_wiredStarts = 0;
  pm::api::content::GhostRules rules;
  rules.start = &wired_start;
  rules.huntSpeed = &wired_speed;
  rules.heldBack = &wired_held;
  CHECK(register_content(rules));
  fake::commit_content();
  pm::content::Registry& r = fake::content_registry();
  for (int told = 0; told < kToldCount; ++told) {
    const std::vector<PmGhostRules> attached = r.rules_for(kToldIds[told]);
    CHECK(attached.size() == 1);
    if (attached.size() != 1) continue;
    CHECK(reinterpret_cast<intptr_t>(attached[0].user) == told && attached[0].heldBack == &wired_held);
    CHECK((attached[0].huntSpeed != nullptr) == (told == kSpirit || told == kShade));
    const std::vector<pm::content::Text> lines = r.lines_for(kToldIds[told]);
    CHECK(lines.size() == 1 && lines[0].size() == 1 && lines[0][0].first == "en" &&
          lines[0][0].second == tell_line(told));
  }
  CHECK(r.rules_for("vanilla.wraith").empty() && r.lines_for("vanilla.wraith").empty());

  // Behind the room's agreement, as in the host.
  const std::vector<pm::content::ModBuild> ours{{"ghost-tells", "0011223344556677"}};
  const auto announced = [](std::map<int, std::vector<pm::content::ModBuild>> lists) {
    return [lists](int actor) -> std::optional<std::vector<pm::content::ModBuild>> {
      const auto it = lists.find(actor);
      if (it == lists.end()) return std::nullopt;
      return it->second;
    };
  };
  pm::content::Agreement agreement;
  unsigned heard = 0;  // presence's revision: each observe below hears something new
  agreement.set_local(ours);
  r.use_agreement(&agreement);
  agreement.observe(true, false, 1, 2, {2}, ++heard, announced({{2, {}}}));  // player 2 runs no mods
  CHECK(r.rules_for("vanilla.mare").empty() && r.lines_for("vanilla.mare").empty());
  agreement.observe(true, false, 1, 2, {2}, ++heard,
                    announced({{2, {{"ghost-tells", "8899aabbccddeeff"}}}}));  // another build
  CHECK(r.rules_for("vanilla.mare").empty() && r.lines_for("vanilla.mare").empty());
  agreement.observe(true, false, 1, 2, {2}, ++heard, announced({{2, ours}}));
  CHECK(!r.rules_for("vanilla.mare").empty() && !r.lines_for("vanilla.mare").empty());
  agreement.observe(true, true, 1, 2, {2}, ++heard, announced({{2, ours}}));  // a contract with a Mare
  fake::contract_ghost(6, 6);
  fake::tick_rules();
  CHECK(g_wiredStarts == 1);
  agreement.observe(true, true, 1, 1, {}, ++heard, announced({}));  // the host leaves
  fake::tick_rules();
  CHECK(g_wiredStarts == 1 && !r.rules_for("vanilla.mare").empty() && !r.lines_for("vanilla.mare").empty());
  fake::contract_ghost(-1, -1);
  fake::tick_rules();
}

}  // namespace game

int pm::tells::told_now() { return game::g_toldNow; }

int main() {
  use_text({"mods/ghost_tells/i18n/default.json"});
  the_six_ghosts_are_told_and_no_other();
  an_edge_rises_once();
  a_timer_runs_for_its_seconds();
  messages_round_trip();
  the_journal_names_the_six_pages_only();
  every_told_ghost_has_a_journal_line();
  incense_stops_the_spirit_for_three_seconds_in_a_hunt();
  incense_silences_the_spirit_for_a_minute();
  the_spirit_hears_its_name_as_a_whole_word();
  the_spirit_knocks_once_after_its_name();
  the_mares_quick_switch_off_is_one_in_two();
  the_mare_never_shows_itself_in_a_lit_room();
  the_mares_trail_names_each_room_once();
  the_shade_vanishes_when_stared_at();
  the_shade_does_nothing_with_two_players_near();
  the_shade_hesitates_at_a_group_once_in_a_while();
  the_smudged_yurei_shuts_itself_in();
  the_yureis_door_stays_locked_for_three_seconds();
  a_door_left_open_draws_the_yureis_ability();
  the_goryo_stands_on_camera_now_and_then();
  the_goryo_acts_more_on_camera();
  the_goryo_goes_home_only_when_it_has_lost_everyone_far_away();
  the_onryo_snuffs_a_new_flame_one_time_in_two();
  every_flame_gutters_as_the_onryo_counts();
  a_flame_the_onryo_snuffed_will_not_catch_for_a_while();
  the_gate_holds_back_only_the_ghosts_own_attempts();
  the_yureis_door_pick_is_steered_only_after_its_idle_change();
  a_room_change_keeps_the_yureis_minute();
  a_searching_goryo_is_still_after_its_prey();
  the_mares_roll_is_answered_only_inside_its_coroutine();
  game::the_tells_time_is_the_games_frame_time();
  game::the_world_reads_the_ghost_its_room_and_the_players();
  game::the_hunt_is_the_one_the_speed_update_ran();
  game::the_gate_reads_the_states_and_holds_back_the_ghosts_own_attempts();
  game::holding_still_gives_the_hunt_its_inputs();
  game::the_mares_roll_is_answered_inside_use_light_switch();
  game::a_spirits_quiet_minute_starts_only_with_a_new_hold();
  game::the_tells_and_their_lines_run_only_while_the_room_agrees();
  game::start();  // what the tells hold goes before the fake's objects do
  if (g_failures) std::printf("%d failure(s)\n", g_failures);
  return g_failures ? 1 : 0;
}
