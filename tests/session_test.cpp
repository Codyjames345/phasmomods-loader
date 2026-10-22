// Offline tests for pg::session (no game, no Photon). Run: build\session_test.exe
#include "session.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

using namespace pg::session;

namespace {

int g_failures = 0;
#define CHECK(cond)                                                   \
  do {                                                                \
    if (!(cond)) {                                                    \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);     \
      ++g_failures;                                                   \
    }                                                                 \
  } while (0)

// A room of sessions; actor numbers start at 1 and the first is master.
struct Room {
  std::vector<Session> s;
  std::vector<int> actors;
  std::vector<int> unmodded;  // in the room without the mod: they never observe, send or receive
  int master = 1;
  uint64_t now = 1000;

  explicit Room(int n) : s(n) {
    for (int i = 0; i < n; ++i) actors.push_back(i + 1);
  }
  Session& at(int actor) { return s[actor - 1]; }
  bool modded(int actor) const {
    for (int a : unmodded)
      if (a == actor) return false;
    return true;
  }

  // One frame: everyone observes, then every queued message is delivered as Photon would.
  void frame(uint64_t dt = 16) {
    now += dt;
    for (int a : actors)
      if (modded(a)) at(a).observe(a, master, actors, now);
    deliver();
  }
  void deliver() {
    for (int from : actors) {
      if (!modded(from)) continue;
      for (Outgoing& o : at(from).take_outgoing()) {
        for (int to : actors) {
          if (to == from || !modded(to)) continue;
          bool hit = o.to == To::Others || (o.to == To::Master && to == master) || (o.to == To::Actor && to == o.actor);
          if (hit) at(to).receive(from, o.bytes.data(), o.bytes.size(), now);
        }
      }
    }
  }
  void settle() {
    for (int i = 0; i < 3; ++i) frame(kAnnounceMs);
  }
};

std::vector<uint8_t> packet(Type t, std::vector<uint8_t> body) {
  std::vector<uint8_t> b = {'P', 'G', kVersion, t};
  b.insert(b.end(), body.begin(), body.end());
  return b;
}

template <class T>
void append(std::vector<uint8_t>& b, T v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  b.insert(b.end(), p, p + sizeof v);
}

std::vector<uint8_t> input_packet(uint16_t seq, float dx, float dz) {
  std::vector<uint8_t> body;
  append(body, seq);
  append(body, dx);
  append(body, dz);
  return packet(kInput, body);
}

void volunteer_and_assignment_reach_everyone() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).volunteers() == std::vector<int>{2});
  for (int a : r.actors) CHECK(r.at(a).ghost_actor() == 2);
  CHECK(r.at(2).local_is_ghost());
  CHECK(!r.at(3).local_is_ghost());
}

void only_the_master_assigns() {
  Room r(3);
  r.at(3).set_volunteer(true);
  r.settle();
  CHECK(r.at(3).ghost_actor() == 3);  // drawn by the host in the lobby
  std::vector<uint8_t> body;
  append<int32_t>(body, 2);
  std::vector<uint8_t> spoof = packet(kAssign, body);
  r.at(3).receive(2, spoof.data(), spoof.size(), r.now);  // forged by a non-master peer
  CHECK(r.at(3).ghost_actor() == 3);
}

void host_receives_only_the_ghosts_fresh_ordered_input() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.settle();
  Input in;
  r.at(2).send_input(0.6f, 0.8f);
  r.deliver();
  CHECK(r.at(1).fresh_input(r.now, in) && in.dx == 0.6f && in.dz == 0.8f);

  std::vector<uint8_t> other = input_packet(500, 1.0f, 0.0f);
  r.at(1).receive(3, other.data(), other.size(), r.now);  // a hunter is not the ghost
  CHECK(r.at(1).fresh_input(r.now, in) && in.dx == 0.6f);

  std::vector<uint8_t> old = input_packet(0, 0.0f, 1.0f);  // older sequence, arrives late
  r.at(1).receive(2, old.data(), old.size(), r.now);
  CHECK(r.at(1).fresh_input(r.now, in) && in.dx == 0.6f);

  std::vector<uint8_t> nan = input_packet(9, std::numeric_limits<float>::quiet_NaN(), 0.0f);
  r.at(1).receive(2, nan.data(), nan.size(), r.now);
  std::vector<uint8_t> big = input_packet(10, 3.0f, 0.0f);
  r.at(1).receive(2, big.data(), big.size(), r.now);
  CHECK(r.at(1).fresh_input(r.now, in) && in.dx == 0.6f);

  CHECK(!r.at(1).fresh_input(r.now + kInputTimeoutMs + 1, in));  // stale: native AI resumes
}

void ghost_leaving_clears_the_role() {
  Room r(3);
  r.at(3).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).ghost_actor() == 3);
  r.actors = {1, 2};
  r.frame();
  CHECK(r.at(1).ghost_actor() == kNobody);  // nobody else volunteers
  r.settle();
  CHECK(r.at(2).ghost_actor() == kNobody);
}

const float kForward[3] = {0, 0, 1};

void triggers_and_status_flow_between_ghost_and_host() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.settle();
  const float aim[3] = {0.6f, 0.0f, 0.8f};
  r.at(2).send_trigger(3, aim);
  r.at(2).send_trigger(5, aim);   // the disguise, the last kind
  r.at(2).send_trigger(6, aim);   // out of range, never sent
  r.at(2).send_trigger(99, aim);
  r.deliver();
  std::vector<Trigger> got = r.at(1).take_triggers();
  CHECK(got.size() == 2 && got[0].kind == 3 && got[0].aim[0] == 0.6f && got[0].aim[2] == 0.8f && got[1].kind == 5);
  std::vector<uint8_t> beyond = {6};  // a kind past the last one, sent by hand
  append(beyond, 0.0f);
  append(beyond, 0.0f);
  append(beyond, 1.0f);
  std::vector<uint8_t> forged = packet(kTrigger, beyond);
  r.at(1).receive(2, forged.data(), forged.size(), r.now);
  CHECK(r.at(1).take_triggers().empty());

  // Whole charges survive the 1/255 encoding.
  CHECK(whole_charges(encode_charge(1.0f, 2.0f), 2) == 1);
  CHECK(whole_charges(encode_charge(2.0f, 2.0f), 2) == 2);
  CHECK(whole_charges(encode_charge(1.99f, 2.0f), 2) == 1);
  CHECK(whole_charges(encode_charge(0.99f, 2.0f), 2) == 0);
  CHECK(whole_charges(encode_charge(-3.0f, 2.0f), 2) == 0);  // owed after an expensive interaction

  Status s;
  s.interact = 200;
  s.events = 1;
  s.abilities = 2;
  s.fuses = 1;
  s.flags = kHuntAvailable;
  s.more = kEventAllowed | kFuseTurnOff;
  s.sanity = 47;
  s.huntBelow = 70;
  s.type = 24;
  s.room = 3;
  s.identity = 31;
  r.at(1).set_status(s);
  r.frame(kStatusMs);
  CHECK(r.at(2).status() == s);
  std::vector<uint8_t> body = {0, 0, 0, 0, 0, kHuntAvailable | kHunting, 0, 10, 50, 1, 2, 0, 1};
  std::vector<uint8_t> fake = packet(kStatus, body);
  r.at(2).receive(3, fake.data(), fake.size(), r.now);  // a hunter, not the master
  CHECK(r.at(2).status() == s);
  std::vector<uint8_t> shortBody = {0, 0, 0, 0, 0, 0, 0, 10, 50, 1, 2, 0};  // the previous version's STATUS, without identity
  std::vector<uint8_t> old = packet(kStatus, shortBody);
  r.at(2).receive(1, old.data(), old.size(), r.now);
  CHECK(r.at(2).status() == s);
}

void trigger_aim_must_be_a_direction() {
  Room r(2);
  r.at(2).set_volunteer(true);
  r.settle();
  std::vector<uint8_t> body = {0};
  append(body, 5.0f);
  append(body, 0.0f);
  append(body, 0.0f);
  std::vector<uint8_t> bad = packet(kTrigger, body);
  r.at(1).receive(2, bad.data(), bad.size(), r.now);
  CHECK(r.at(1).take_triggers().empty());
}

void host_as_ghost_needs_no_network() {
  Room r(2);
  r.at(1).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).local_is_ghost());
  r.at(1).send_trigger(0, kForward);
  CHECK(r.at(1).take_triggers().size() == 1);
  r.at(1).send_input(1.0f, 0.0f);  // applied locally by the caller, not sent
  bool sentInput = false;
  for (Outgoing& o : r.at(1).take_outgoing()) sentInput |= o.bytes[3] == kInput;
  CHECK(!sentInput);
}

void malformed_packets_are_ignored() {
  Room r(2);
  r.settle();
  const uint8_t shortPacket[] = {'P', 'G'};
  r.at(1).receive(2, shortPacket, sizeof shortPacket, r.now);
  const uint8_t wrongVersion[] = {'P', 'G', 99, kHello, 1};
  r.at(1).receive(2, wrongVersion, sizeof wrongVersion, r.now);
  CHECK(r.at(1).volunteers().empty());
  std::vector<uint8_t> truncated = packet(kInput, {1, 0});
  r.at(1).receive(2, truncated.data(), truncated.size(), r.now);
  Input in;
  CHECK(!r.at(1).fresh_input(r.now, in));
}

void master_migration_keeps_the_assignment() {
  Room r(3);
  r.at(1).set_random([](int n) { return n - 1; });
  r.at(2).set_volunteer(true);
  r.at(3).set_volunteer(true);
  r.settle();
  CHECK(r.at(3).ghost_actor() == 3);  // two volunteers, so a redraw on migration could pick actor 2
  r.actors = {2, 3};
  r.master = 2;
  r.settle();
  CHECK(r.at(2).ghost_actor() == 3);
  CHECK(r.at(3).ghost_actor() == 3);
  Input in;
  r.at(3).send_input(0.0f, 1.0f);
  r.deliver();
  CHECK(r.at(2).fresh_input(r.now, in) && in.dz == 1.0f);
}

void random_volunteer_is_drawn_in_the_lobby_with_fair_rotation() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.at(3).set_volunteer(true);
  r.settle();
  for (int a : r.actors) CHECK(r.at(a).ghost_actor() == 2);  // known before the contract loads
  CHECK(r.at(2).local_is_ghost());
  r.at(1).start_round();
  r.at(1).end_round();
  r.settle();
  // Actor 2 has been the ghost once, so only actor 3 is in the pool now.
  for (int a : r.actors) CHECK(r.at(a).ghost_actor() == 3);
  r.at(1).start_round();
  int drawnCount = -1;
  r.at(1).set_random([&](int n) { drawnCount = n; return n - 1; });
  r.at(1).end_round();
  CHECK(drawnCount == 2);  // both have been the ghost once: a fair draw between them
  CHECK(r.at(1).ghost_actor() == 3);
}

// Ticking first gives no advantage: each new volunteer in the lobby triggers a fresh draw.
void a_later_volunteer_can_be_drawn_for_the_first_contract() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).ghost_actor() == 2);
  int drawnCount = -1;
  r.at(1).set_random([&](int n) { drawnCount = n; return n - 1; });
  r.at(3).set_volunteer(true);
  r.settle();
  CHECK(drawnCount == 2);  // drawn again, between both volunteers
  for (int a : r.actors) CHECK(r.at(a).ghost_actor() == 3);
  drawnCount = -1;
  r.settle();
  CHECK(drawnCount == -1);  // no newcomer, no redraw
}

// Actors 2 and 3 volunteer; the default draw picks actor 2.
Room two_volunteers() {
  Room r(3);
  r.at(2).set_volunteer(true);
  r.at(3).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).ghost_actor() == 2);
  return r;
}

void a_volunteer_who_withdraws_is_replaced() {
  Room r = two_volunteers();
  r.at(2).set_volunteer(false);
  r.settle();
  for (int a : r.actors) CHECK(r.at(a).ghost_actor() == 3);
}

// While a contract loads Photon pauses its message queue, so the host hears nothing for a while.
void a_silent_contract_load_keeps_the_drawn_ghost() {
  Room r = two_volunteers();
  for (int i = 0; i < 10; ++i) {  // longer than kPeerTimeoutMs without delivering anything
    r.now += kAnnounceMs;
    for (int a : r.actors) {
      r.at(a).observe(a, r.master, r.actors, r.now);
      r.at(a).take_outgoing();
    }
  }
  CHECK(r.at(1).ghost_actor() == 2);
  r.at(1).set_random([](int n) { return n - 1; });  // a redraw would now pick actor 3
  r.settle();
  CHECK(r.at(1).ghost_actor() == 2);
  CHECK(r.at(3).ghost_actor() == 2);
}

// A ghost-player round needs the mod on every client: a player without it never sees the ghost
// player's hidden body handled, so the contract stays vanilla until everyone has it.
void a_player_without_the_mod_means_the_native_ghost() {
  Room r(3);
  r.unmodded = {3};
  r.at(2).set_volunteer(true);
  r.settle();
  CHECK(!r.at(1).everyone_has_mod());
  CHECK(!r.at(2).everyone_has_mod());
  CHECK(r.at(1).ghost_actor() == kNobody);
  CHECK(r.at(2).ghost_actor() == kNobody);
  r.unmodded.clear();  // actor 3 starts the mod
  r.settle();
  CHECK(r.at(1).everyone_has_mod());
  CHECK(r.at(1).ghost_actor() == 2);
  CHECK(r.at(3).ghost_actor() == 2);
}

void a_player_without_the_mod_joining_the_lobby_cancels_the_ghost_player() {
  Room r(2);
  r.at(2).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).ghost_actor() == 2);
  r.s.emplace_back();
  r.actors.push_back(3);
  r.unmodded = {3};
  r.frame();
  CHECK(r.at(1).ghost_actor() == kNobody);
  CHECK(r.at(2).ghost_actor() == kNobody);
}

// A client on another version of the mod speaks another wire format and handles the body another
// way: it counts as not having the mod.
void a_player_on_another_mod_version_means_the_native_ghost() {
  Room r(3);
  r.unmodded = {3};
  r.at(2).set_volunteer(true);
  r.settle();
  const std::vector<uint8_t> hello = {'P', 'G', static_cast<uint8_t>(kVersion - 1), kHello, 0};
  for (int to : {1, 2}) r.at(to).receive(3, hello.data(), hello.size(), r.now);
  r.settle();
  CHECK(!r.at(1).everyone_has_mod());
  CHECK(r.at(1).ghost_actor() == kNobody);
}

void no_volunteers_means_the_native_ghost() {
  Room r(2);
  r.settle();
  r.at(1).start_round();
  r.deliver();
  CHECK(r.at(1).ghost_actor() == kNobody);
  CHECK(r.at(2).ghost_actor() == kNobody);
}

void round_with_ghost(Room& r, int ghost) {
  r.at(ghost).set_volunteer(true);
  r.settle();
  CHECK(r.at(1).ghost_actor() == ghost);
  r.at(1).start_round();
  r.deliver();
}

// Everyone parks the ghost player's body only once the ghost player's own client has stepped out of
// it (src/body.h): until then it is still their investigator.
void the_ghost_players_step_out_reaches_everyone() {
  Room r(3);
  round_with_ghost(r, 2);
  CHECK(!r.at(1).ghost_stepped_out() && !r.at(3).ghost_stepped_out());
  r.at(3).set_stepped_out(true);  // only the ghost player's word counts
  r.frame();
  CHECK(!r.at(1).ghost_stepped_out());
  r.at(2).set_stepped_out(true);
  r.frame();
  CHECK(r.at(1).ghost_stepped_out() && r.at(3).ghost_stepped_out() && r.at(2).ghost_stepped_out());
  r.at(2).set_stepped_out(false);
  r.frame();
  CHECK(!r.at(1).ghost_stepped_out() && !r.at(3).ghost_stepped_out());
}

// A ghost player's client that goes quiet (a stall, a minimised window, a contract's load) has not
// stepped back into its investigator: only its own HELLO, or its leaving, says so.
void a_silent_ghost_player_stays_stepped_out() {
  Room r(3);
  round_with_ghost(r, 2);
  r.at(2).set_stepped_out(true);
  r.frame();
  for (int i = 0; i < 10; ++i) {  // longer than kPeerTimeoutMs without delivering anything
    r.now += kAnnounceMs;
    for (int a : r.actors) {
      r.at(a).observe(a, r.master, r.actors, r.now);
      r.at(a).take_outgoing();
    }
  }
  CHECK(r.at(1).ghost_stepped_out() && r.at(3).ghost_stepped_out());
  r.at(2).send_input(0.5f, 0);  // traffic other than HELLO resumes first
  r.deliver();
  CHECK(r.at(1).ghost_stepped_out());
  r.actors = {1, 3};  // the ghost player left the room
  r.frame();
  CHECK(!r.at(1).ghost_stepped_out() && !r.at(3).ghost_stepped_out());
}

void truck_wait_notice_reaches_only_the_investigator() {
  Room r(3);
  round_with_ghost(r, 2);
  r.at(1).notify_wait(3);
  r.deliver();
  CHECK(r.at(3).take_wait_notice());
  CHECK(!r.at(3).take_wait_notice());
  CHECK(!r.at(2).take_wait_notice());
  r.at(1).notify_wait(1);  // the host pressed the button itself
  CHECK(r.at(1).take_wait_notice());
  std::vector<uint8_t> forged = packet(kWait, {});
  r.at(3).receive(2, forged.data(), forged.size(), r.now);  // only the host may send it
  CHECK(!r.at(3).take_wait_notice());
}

void pick_offer_reaches_only_the_ghost_and_choices_are_validated() {
  Room r(3);
  round_with_ghost(r, 2);
  PickOffer offer;
  offer.types = {1, 5};
  offer.rooms = 10;
  offer.deadline = r.now + 30000;
  r.at(1).offer_pick(offer);
  r.deliver();
  CHECK(r.at(2).pick_offer().valid && r.at(2).pick_offer().types == offer.types && r.at(2).pick_offer().rooms == 10);
  CHECK(r.at(2).pick_offer().deadline == r.now + 30000);
  CHECK(!r.at(3).pick_offer().valid);

  r.at(2).send_pick({5, 12});  // room out of range
  r.deliver();
  CHECK((r.at(1).choice() == Choice{5, -1}));
  r.at(2).send_pick({7, 3});  // type not offered
  r.deliver();
  CHECK((r.at(1).choice() == Choice{5, 3}));

  std::vector<uint8_t> body = {1};
  append<int16_t>(body, 0);
  std::vector<uint8_t> forged = packet(kPick, body);
  r.at(1).receive(3, forged.data(), forged.size(), r.now);  // an investigator is not the ghost
  CHECK((r.at(1).choice() == Choice{5, 3}));

  r.at(1).close_pick();
  r.deliver();
  CHECK(!r.at(2).pick_offer().valid);
  r.at(2).send_pick({1, 1});  // too late
  r.deliver();
  CHECK((r.at(1).choice() == Choice{5, 3}));
}

void forced_type_is_not_choosable() {
  Room r(2);
  round_with_ghost(r, 2);
  PickOffer offer;
  offer.types = {1, 5};
  offer.fixedType = 5;
  offer.rooms = 4;
  offer.deadline = r.now + 30000;
  r.at(1).offer_pick(offer);
  r.deliver();
  CHECK(r.at(2).pick_offer().fixedType == 5);
  r.at(2).send_pick({1, 2});
  r.deliver();
  CHECK((r.at(1).choice() == Choice{-1, 2}));
}

void truck_lock_reaches_everyone_and_expires() {
  Room r(3);
  round_with_ghost(r, 2);
  r.at(1).set_lock(r.now + 30000);
  r.deliver();
  for (int a : r.actors) {
    CHECK(r.at(a).locked(r.now));
    CHECK(!r.at(a).locked(r.now + 30000));
  }
  std::vector<uint8_t> body;
  append<uint32_t>(body, 0);
  std::vector<uint8_t> forged = packet(kLock, body);
  r.at(3).receive(2, forged.data(), forged.size(), r.now);  // only the host can unlock
  CHECK(r.at(3).locked(r.now));
  r.at(1).set_lock(0);
  r.deliver();
  for (int a : r.actors) CHECK(!r.at(a).locked(r.now));
}

void lock_ends_with_the_round() {
  Room r(2);
  round_with_ghost(r, 2);
  r.at(1).set_lock(r.now + 30000);
  r.deliver();
  r.at(1).end_round();
  r.deliver();
  CHECK(!r.at(1).locked(r.now));
  CHECK(!r.at(2).locked(r.now));
}

void room_offer_and_claim() {
  Room r(3);
  round_with_ghost(r, 3);
  r.at(1).offer_room(4, r.now + 20000);
  r.deliver();
  CHECK(r.at(3).room_offer().valid && r.at(3).room_offer().nativeRoom == 4);
  CHECK(!r.at(2).room_offer().valid);
  r.at(3).send_room_claim(7);
  r.deliver();
  int room = -1;
  CHECK(r.at(1).take_room_claim(room) && room == 7);
  CHECK(!r.at(1).take_room_claim(room));
  r.at(1).close_room_offer();
  r.deliver();
  CHECK(!r.at(3).room_offer().valid);
  r.at(3).send_room_claim(2);  // closed: not sent
  r.deliver();
  CHECK(!r.at(1).take_room_claim(room));
}

void host_as_ghost_picks_without_network() {
  Room r(2);
  round_with_ghost(r, 1);
  CHECK(r.at(1).local_is_ghost());
  PickOffer offer;
  offer.types = {3};
  offer.rooms = 2;
  offer.deadline = r.now + 30000;
  r.at(1).offer_pick(offer);
  CHECK(r.at(1).pick_offer().valid);
  r.at(1).send_pick({3, 1});
  CHECK((r.at(1).choice() == Choice{3, 1}));
  bool sent = false;
  for (Outgoing& o : r.at(1).take_outgoing()) sent |= o.bytes[3] == kPickOffer || o.bytes[3] == kPick;
  CHECK(!sent);
}

}  // namespace

void guesses_reach_the_ghost_and_the_score_reaches_the_investigators() {
  Room r(4);
  round_with_ghost(r, 2);
  r.at(3).send_guess({31, false});
  r.at(4).send_guess({9, true});
  r.deliver();
  CHECK(r.at(2).guesses().size() == 2);
  CHECK((r.at(2).guesses().at(3) == pg::score::Guess{31, false}));
  CHECK((r.at(2).guesses().at(4) == pg::score::Guess{9, true}));
  CHECK(r.at(1).guesses().empty());  // only the ghost player collects them
  r.at(3).send_guess({31, false});   // unchanged: nothing is sent again
  CHECK(r.at(3).take_outgoing().empty());
  r.at(3).send_guess({4, false});
  r.deliver();
  CHECK(r.at(2).guesses().at(3).type == 4);
  r.at(2).send_guess({1, false});    // the ghost player does not guess
  CHECK(r.at(2).take_outgoing().empty());
  const pg::score::Tally t = pg::score::tally(r.at(2).guesses(), 31);
  r.at(2).send_score(t);
  r.deliver();
  pg::score::Tally got;
  CHECK(r.at(3).take_score(got) && got == t && !r.at(3).take_score(got));
  CHECK(r.at(1).take_score(got) && got == t);
  std::vector<uint8_t> forged = packet(kScore, {9, 9, 9});
  r.at(3).receive(4, forged.data(), forged.size(), r.now);  // only the ghost player sends the score
  CHECK(!r.at(3).take_score(got));
}

void the_mimic_offer_reaches_the_ghost_and_only_offered_picks_reach_the_host() {
  Room r(3);
  round_with_ghost(r, 2);
  r.at(1).offer_mimic({3, 9, 12});
  r.deliver();
  CHECK((r.at(2).mimic_offer() == std::vector<uint8_t>{3, 9, 12}));
  CHECK(r.at(3).mimic_offer().empty());  // only the ghost player is offered
  r.at(1).offer_mimic({3, 9, 12});       // unchanged: nothing is sent again
  CHECK(r.at(1).take_outgoing().empty());
  r.at(2).send_mimic_pick(9);
  r.deliver();
  uint8_t pick = 0;
  CHECK(r.at(1).take_mimic_pick(pick) && pick == 9 && !r.at(1).take_mimic_pick(pick));
  r.at(2).send_mimic_pick(kNoType);  // back to the game's own choice
  r.deliver();
  CHECK(r.at(1).take_mimic_pick(pick) && pick == kNoType);
  r.at(2).send_mimic_pick(5);  // not offered: refused by the host
  r.deliver();
  CHECK(!r.at(1).take_mimic_pick(pick));
  std::vector<uint8_t> forged = packet(kMimicPick, {3});
  r.at(1).receive(3, forged.data(), forged.size(), r.now);  // only the ghost player picks
  CHECK(!r.at(1).take_mimic_pick(pick));
  r.at(3).send_mimic_pick(3);  // an investigator does not pick
  CHECK(r.at(3).take_outgoing().empty());
}

void host_as_ghost_picks_the_mimic_copy_without_network() {
  Room r(2);
  round_with_ghost(r, 1);
  r.at(1).offer_mimic({4, 6});
  r.at(1).send_mimic_pick(6);
  uint8_t pick = 0;
  CHECK(r.at(1).take_mimic_pick(pick) && pick == 6);
  r.at(1).send_mimic_pick(7);
  CHECK(!r.at(1).take_mimic_pick(pick));
  CHECK(r.at(1).take_outgoing().empty());
}

void marks_and_alerts_reach_only_the_ghost_player() {
  Room r(3);
  round_with_ghost(r, 2);
  using pg::perceive::Alert;
  using pg::perceive::Mark;
  const std::vector<Mark> marks = {{pg::perceive::kPrey, 1, 0, 2}, {pg::perceive::kStepSprint, -3, 0.5f, 4}};
  r.at(1).send_marks(marks);
  r.deliver();
  CHECK(r.at(2).marks() == marks && r.at(2).marks_at() == r.now);
  CHECK(r.at(3).marks().empty());
  r.at(1).send_marks({});  // the marks go: an empty list is sent once
  r.deliver();
  CHECK(r.at(2).marks().empty());
  r.at(1).send_marks({});
  CHECK(r.at(1).take_outgoing().empty());
  std::vector<Mark> many(40, Mark{pg::perceive::kScent, 0, 0, 0});
  r.at(1).send_marks(many);
  r.deliver();
  CHECK(r.at(2).marks().size() == static_cast<size_t>(pg::perceive::kMaxMarks));
  r.at(3).send_marks(marks);  // only the host perceives for the ghost
  CHECK(r.at(3).take_outgoing().empty());

  Alert a;
  a.kind = pg::perceive::kOuija;
  a.located = true;
  a.x = 4;
  a.z = -2;
  a.text = std::string(60, 'x');  // longer than an alert carries
  r.at(1).send_alert(a);
  r.deliver();
  Alert got;
  CHECK(r.at(2).take_alert(got) && got.kind == a.kind && got.located && got.x == 4 && got.z == -2);
  CHECK(got.text.size() == pg::perceive::kMaxAlertText);
  CHECK(!r.at(2).take_alert(got));
  CHECK(!r.at(3).take_alert(got));
  std::vector<uint8_t> forged = packet(kAlert, {pg::perceive::kSalt, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  r.at(2).receive(3, forged.data(), forged.size(), r.now);  // only the host sends alerts
  CHECK(!r.at(2).take_alert(got));
}

void host_as_ghost_perceives_without_network() {
  Room r(2);
  round_with_ghost(r, 1);
  r.at(1).send_marks({{pg::perceive::kCharge, 1, 1, 1}});
  pg::perceive::Alert a;
  a.kind = pg::perceive::kCrucifix;
  r.at(1).send_alert(a);
  CHECK(r.at(1).marks().size() == 1);
  pg::perceive::Alert got;
  CHECK(r.at(1).take_alert(got) && got.kind == pg::perceive::kCrucifix);
  CHECK(r.at(1).take_outgoing().empty());
}

void the_form_reaches_everyone_but_the_ghost_player() {
  Room r(3);
  round_with_ghost(r, 2);
  r.at(1).send_form({3, 5000});
  r.deliver();
  Form f;
  CHECK(r.at(1).take_form(f) && (f == Form{3, 5000}));  // the host shows it too
  CHECK(r.at(3).take_form(f) && f.actor == 3 && !r.at(3).take_form(f));
  CHECK(!r.at(2).take_form(f));  // the ghost player is the form
  r.at(3).send_form({1, 100});   // only the host decides
  CHECK(r.at(3).take_outgoing().empty());
  std::vector<uint8_t> forged = packet(kForm, {1, 0, 0, 0, 0x88, 0x13});
  r.at(3).receive(2, forged.data(), forged.size(), r.now);
  CHECK(!r.at(3).take_form(f));
}

/** A custom ghost and the Mimic are named by what they are, not by the type their AI follows. */
void the_ghost_is_named_by_what_it_is() {
  Status st;
  CHECK(named_type(st) == kNoType);
  st.type = 24;  // a custom ghost's AI follows no vanilla type
  CHECK(named_type(st) == 24);
  st.identity = 43;
  CHECK(named_type(st) == 43);
}

int main() {
  the_ghost_is_named_by_what_it_is();
  volunteer_and_assignment_reach_everyone();
  only_the_master_assigns();
  host_receives_only_the_ghosts_fresh_ordered_input();
  ghost_leaving_clears_the_role();
  triggers_and_status_flow_between_ghost_and_host();
  trigger_aim_must_be_a_direction();
  host_as_ghost_needs_no_network();
  malformed_packets_are_ignored();
  master_migration_keeps_the_assignment();
  random_volunteer_is_drawn_in_the_lobby_with_fair_rotation();
  a_later_volunteer_can_be_drawn_for_the_first_contract();
  a_volunteer_who_withdraws_is_replaced();
  a_silent_contract_load_keeps_the_drawn_ghost();
  no_volunteers_means_the_native_ghost();
  a_player_without_the_mod_means_the_native_ghost();
  a_player_without_the_mod_joining_the_lobby_cancels_the_ghost_player();
  a_player_on_another_mod_version_means_the_native_ghost();
  the_ghost_players_step_out_reaches_everyone();
  a_silent_ghost_player_stays_stepped_out();
  truck_wait_notice_reaches_only_the_investigator();
  pick_offer_reaches_only_the_ghost_and_choices_are_validated();
  forced_type_is_not_choosable();
  truck_lock_reaches_everyone_and_expires();
  lock_ends_with_the_round();
  room_offer_and_claim();
  host_as_ghost_picks_without_network();
  guesses_reach_the_ghost_and_the_score_reaches_the_investigators();
  the_mimic_offer_reaches_the_ghost_and_only_offered_picks_reach_the_host();
  host_as_ghost_picks_the_mimic_copy_without_network();
  marks_and_alerts_reach_only_the_ghost_player();
  host_as_ghost_perceives_without_network();
  the_form_reaches_everyone_but_the_ghost_player();
  std::printf(g_failures ? "%d check(s) failed\n" : "all session tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
