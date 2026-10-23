// Offline tests for who may stay in a modded lobby (host/join_guard.h). Run: build\join_guard_test.exe
#include "join_guard.h"

#include <string>
#include <vector>

#include "check.h"

using namespace pg;
using joinguard::Frame;
using joinguard::Guard;
using joinguard::Verdict;
using presence::Mod;
using presence::Presence;

namespace {

const Mod kGhost{"playable-ghost", "1111111111111111"};
const Mod kGhostOther{"playable-ghost", "2222222222222222"};
const Mod kCustom{"custom-ghosts", "3333333333333333"};
const Mod kReplay{"replay", "4444444444444444"};

std::string name_of(const std::string& id) {
  if (id == "playable-ghost") return "Playable Ghost";
  if (id == "custom-ghosts") return "Custom Ghosts";
  return id;
}

// What `listener` hears when `speaker` announces `mods`.
void hear(Presence& listener, int speaker, std::vector<Mod> mods, bool vanilla, uint64_t now) {
  Presence p;
  p.set_local(std::move(mods), vanilla);
  p.observe(speaker, {speaker}, now);
  const std::vector<uint8_t> hello = p.take_announcement();
  listener.receive(speaker, hello.data(), hello.size(), now);
}

Frame lobby(int local, int master, std::vector<int> actors, uint64_t now) { return {local, master, std::move(actors), true, now}; }

bool contains(const std::vector<int>& v, int a) {
  for (int x : v)
    if (x == a) return true;
  return false;
}

void a_joiner_with_other_mods_leaves_and_is_told_why_once_out() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay}, true);
  p.observe(2, {1, 2}, 0);
  Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(!v.leave);  // the host has not been heard from yet
  hear(p, 1, {kGhostOther, kCustom}, false, 100);
  v = guard.judge(lobby(2, 1, {1, 2}, 100), p);
  CHECK(v.leave && v.show.empty());  // told once out of the room, where the note survives
  guard.left(true);
  CHECK(!guard.judge(lobby(2, 1, {1, 2}, 200), p).leave);  // asked once
  v = guard.judge({0, 0, {}, false, 300}, p);
  CHECK(v.show.find("Custom Ghosts") != std::string::npos);   // only the host has it
  CHECK(v.show.find("replay") != std::string::npos);          // only the joiner has it
  CHECK(v.show.find("Playable Ghost") != std::string::npos);  // another build
  CHECK(guard.judge({0, 0, {}, false, 400}, p).show.empty());  // shown once
}

void a_leave_that_did_not_start_is_asked_again() {
  Presence q;
  Guard again(&name_of);
  q.set_local({kGhost}, true);
  q.observe(2, {1, 2}, 0);
  hear(q, 1, {kCustom}, false, 0);
  CHECK(again.judge(lobby(2, 1, {1, 2}, 0), q).leave);
  again.left(false);  // the lobby's Leave was not there
  hear(q, 1, {kCustom}, false, joinguard::kRetryMs - 1);
  CHECK(!again.judge(lobby(2, 1, {1, 2}, joinguard::kRetryMs - 1), q).leave);
  hear(q, 1, {kCustom}, false, joinguard::kRetryMs);
  CHECK(again.judge(lobby(2, 1, {1, 2}, joinguard::kRetryMs), q).leave);
}

void mismatch_message_tells_how_to_update() {
  const std::string text = joinguard::mismatch_message({kGhost}, {kGhostOther}, &name_of);
  CHECK(text.substr(text.rfind('\n') + 1) == "Restart the game through PhasmoMods Loader to update, then join again.");
}

void a_joiner_with_the_same_mods_stays() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kReplay, kGhost}, true);
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost, kReplay}, true, 0);  // the same set, in another order
  CHECK(!guard.judge(lobby(2, 1, {1, 2}, 0), p).leave);
}

void a_joiner_in_a_vanilla_lobby_stays() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost}, true);
  p.observe(2, {1, 2}, 0);
  CHECK(!guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs * 3), p).leave);
}

void the_host_kicks_a_mismatched_modder_who_does_not_leave() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost}, true);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  hear(p, 2, {kGhostOther}, true, 500);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500), p).kick.empty());  // time to leave by itself
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kKickDelayMs - 1), p).kick.empty());
  hear(p, 2, {kGhostOther}, true, 500 + joinguard::kKickDelayMs);
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kKickDelayMs), p).kick, 2));
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 600 + joinguard::kKickDelayMs), p).kick.empty());  // once
}

void the_host_keeps_a_vanilla_player_when_every_mod_allows_it() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay}, true);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs * 2), p).kick.empty());
}

void a_kick_that_failed_is_tried_again() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost}, false);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), p).kick, 2));
  guard.kicked(2, false);  // no ID card yet
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs - 1), p).kick.empty());
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs), p).kick, 2));
  guard.kicked(2, true);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs * 3), p).kick.empty());
}

// A player who becomes the host judges at once those it has long known: the grace runs from when
// it first saw them, not from when it became the host.
void a_new_host_enforces_at_once() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom}, false);
  p.observe(2, {1, 2, 3}, 0);
  guard.judge(lobby(2, 1, {1, 2, 3}, 0), p);
  CHECK(guard.judge(lobby(2, 1, {1, 2, 3}, joinguard::kGraceMs), p).kick.empty());  // not the host
  p.observe(2, {2, 3}, joinguard::kGraceMs + 1);  // the host left; actor 2 took over
  CHECK(contains(guard.judge(lobby(2, 2, {2, 3}, joinguard::kGraceMs + 1), p).kick, 3));
}

void the_host_kicks_a_vanilla_player_when_a_mod_needs_everyone() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom}, false);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs - 1), p).kick.empty());  // still saying hello
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), p).kick, 2));
}

// A contract's load pauses Photon's messages: everyone falls silent, and nobody is judged for it.
void a_silent_contract_load_judges_nobody() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom}, false);
  p.observe(1, {1, 2}, 0);
  hear(p, 2, {kGhost, kCustom}, false, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  const uint64_t later = presence::kTimeoutMs + joinguard::kGraceMs;
  p.observe(1, {1, 2}, later);  // actor 2's announcement has timed out
  CHECK(guard.judge({1, 1, {1, 2}, false, later}, p).kick.empty());
  CHECK(guard.judge(lobby(1, 1, {1, 2}, later + 1), p).kick.empty());  // heard once, still counts
}

void a_client_without_started_mods_neither_checks_nor_kicks() {
  Presence p;
  Guard guard(&name_of);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs * 2), p).kick.empty());
  Presence q;
  Guard joiner(&name_of);
  q.observe(2, {1, 2}, 0);
  hear(q, 1, {kCustom}, false, 0);
  CHECK(!joiner.judge(lobby(2, 1, {1, 2}, 0), q).leave);
}

void leaving_the_room_forgets_everything() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom}, false);
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  guard.judge({0, 0, {}, false, 1}, p);
  p.observe(1, {1, 3}, 2);
  guard.judge(lobby(1, 1, {1, 3}, 2), p);
  CHECK(guard.judge(lobby(1, 1, {1, 3}, joinguard::kGraceMs), p).kick.empty());  // first seen at 2
  CHECK(contains(guard.judge(lobby(1, 1, {1, 3}, joinguard::kGraceMs + 2), p).kick, 3));
}

}  // namespace

int main() {
  a_joiner_with_other_mods_leaves_and_is_told_why_once_out();
  a_leave_that_did_not_start_is_asked_again();
  a_kick_that_failed_is_tried_again();
  a_new_host_enforces_at_once();
  mismatch_message_tells_how_to_update();
  a_joiner_with_the_same_mods_stays();
  a_joiner_in_a_vanilla_lobby_stays();
  the_host_kicks_a_mismatched_modder_who_does_not_leave();
  the_host_keeps_a_vanilla_player_when_every_mod_allows_it();
  the_host_kicks_a_vanilla_player_when_a_mod_needs_everyone();
  a_silent_contract_load_judges_nobody();
  a_client_without_started_mods_neither_checks_nor_kicks();
  leaving_the_room_forgets_everything();
  std::printf(g_failures ? "%d check(s) failed\n" : "all join guard tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
