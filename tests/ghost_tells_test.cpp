// Offline tests for the Ghost Tells rules (mods/ghost_tells/*.h).
// Run: build\ghost_tells_test.exe
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "gate_rule.h"
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

using namespace pg::tells;

namespace {

constexpr float kFrame = 1.0f / 30;

void the_six_ghosts_are_told_and_no_other() {
  for (int type : {kSpirit, kMare, kShade, kYurei, kGoryo, kOnryo}) CHECK(told(type));
  CHECK(kSpirit == 0 && kMare == 6 && kShade == 8 && kYurei == 10 && kGoryo == 14 && kOnryo == 16);
  CHECK(!told(1));   // the Wraith
  CHECK(!told(24));  // GhostType.None, which every custom ghost's AI follows
  CHECK(!told(-1));
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

void the_journal_names_the_six_pages_only() {
  CHECK(journal_type(u"Journal2_Spirit") == kSpirit);
  CHECK(journal_type(u"Journal2_Mare") == kMare);
  CHECK(journal_type(u"Journal2_Shade") == kShade);
  CHECK(journal_type(u"Journal2_Yurei") == kYurei);
  CHECK(journal_type(u"Journal2_Goryo") == kGoryo);
  CHECK(journal_type(u"Journal2_Onryo") == kOnryo);
  CHECK(journal_type(u"Journal2_Jinn") == -1);
  CHECK(journal_type(u"Journal_GoryoTitle") == -1);
  CHECK(journal_type(u"Journal2_Goryo2") == -1);
  CHECK(journal_type(u"") == -1);
}

void every_told_ghost_has_a_journal_line() {
  for (int type : {kSpirit, kMare, kShade, kYurei, kGoryo, kOnryo}) {
    const char* line = tell_line(type);
    CHECK(line && std::strlen(line) > 20);
    if (!line) continue;
    const std::string s = line;
    CHECK(s.find("\xE2\x80\x94") == std::string::npos);  // no em dash in player-facing copy
    CHECK(s.find("investigator") == std::string::npos);  // players, not investigators
  }
  CHECK(tell_line(1) == nullptr);
  CHECK(tell_line(24) == nullptr);
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
  q.step(true, false, kFrame);
  CHECK(q.quiet());
  frames(59.0f, [&] { return q.step(true, false, kFrame); });  // the flag stays set for its 180 s
  CHECK(q.quiet());
  frames(1.1f, [&] { return q.step(true, false, kFrame); });
  CHECK(!q.quiet());  // a minute is over, though the game's own flag is still set
  Quiet inHunt;
  inHunt.step(true, true, kFrame);
  CHECK(!inHunt.quiet());  // incense in a hunt stops it instead
  inHunt.step(true, false, kFrame);
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
  CHECK(gate_kind(2) == kGateHunt);
  CHECK(gate_kind(14) == kGateEvent && gate_kind(15) == kGateEvent && gate_kind(8) == kGateEvent);
  CHECK(gate_kind(9) == kGateInteract && gate_kind(4) == kGateInteract && gate_kind(7) == kGateInteract);
  CHECK(gate_kind(0) == -1 && gate_kind(1) == -1 && gate_kind(3) == -1);  // idle, wander, favourite room
  CHECK(gate_kind(19) == -1);  // the summoning circle's answer
  CHECK(gate_kind(20) == -1);  // the music box's pull
  CHECK(gate_may_hold(2, false, false));
  CHECK(!gate_may_hold(2, true, false));   // the Death card's forced hunt
  CHECK(!gate_may_hold(14, true, false));  // the Devil card's forced event
  CHECK(!gate_may_hold(9, false, true));   // nothing reaches into a running hunt
  CHECK(!gate_may_hold(20, false, false));
  CHECK(cursed_pull(19) && cursed_pull(20));  // a possession made it show itself: no tell changes that
  CHECK(!cursed_pull(14) && !cursed_pull(8) && !cursed_pull(0));
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

int main() {
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
  if (g_failures) std::printf("%d failure(s)\n", g_failures);
  return g_failures ? 1 : 0;
}
