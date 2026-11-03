// Offline tests for who may stay in a modded lobby and which mods run there (host/join_guard.h). Run: build\join_guard_test.exe
#include "join_guard.h"

#include <string>
#include <vector>

#include "check.h"
#include "consent_text.h"
#include "mod_agreement.h"
#include "mod_deps.h"
#include "text_fixture.h"

using namespace pm;
using joinguard::Frame;
using joinguard::Guard;
using joinguard::Verdict;
using presence::Mod;
using presence::Presence;

namespace {

const Mod kGhost{"playable-ghost", "1111111111111111", "Playable Ghost", "1.0.0"};
const Mod kGhostOther{"playable-ghost", "2222222222222222", "Playable Ghost", "1.0.1"};
const Mod kCustom{"custom-ghosts", "3333333333333333", "Custom Ghosts", "2.0.0"};
const Mod kReplay{"replay", "4444444444444444", "Replay", "3.0.0"};

std::string name_of(const std::string& id) { return id; }

// What `listener` hears when `speaker` announces `mods`.
void hear(Presence& listener, int speaker, std::vector<Mod> mods, uint64_t now) {
  Presence p;
  p.set_local(std::move(mods));
  p.observe(speaker, {speaker}, now);
  for (const std::vector<uint8_t>& part : p.take_announcement()) listener.receive(speaker, part.data(), part.size(), now);
}

// What `listener` hears when `speaker` stops every mod it ran: one announcement listing none.
void hear_none(Presence& listener, int speaker, uint64_t now) {
  Presence p;
  p.set_local({kGhost});
  p.observe(speaker, {speaker}, now);
  p.take_announcement();
  p.set_local({});
  for (const std::vector<uint8_t>& part : p.take_announcement()) listener.receive(speaker, part.data(), part.size(), now);
  const std::vector<Mod>* heard = listener.mods_of(speaker);
  CHECK(heard && heard->empty());
}

Frame lobby(int local, int master, std::vector<int> actors, uint64_t now) { return {local, master, std::move(actors), true, now}; }
Frame public_lobby(int local, int master, uint64_t now) {
  Frame f{local, master, {}, true, now};
  f.publicRoom = true;
  return f;
}
Frame outside(uint64_t now) { return {0, 0, {}, false, now}; }

bool contains(const std::vector<int>& v, int a) {
  for (int x : v)
    if (x == a) return true;
  return false;
}
using Ids = std::vector<std::string>;

void a_joiner_with_other_mods_leaves_and_is_told_why_once_out() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(2, {1, 2}, 0);
  Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(!v.leave);  // the host has not been heard from yet
  hear(p, 1, {kGhostOther, kCustom}, 100);
  v = guard.judge(lobby(2, 1, {1, 2}, 100), p);
  CHECK(v.leave && v.show.empty());  // told once out of the room, where the note survives
  guard.left(true);
  CHECK(!guard.judge(lobby(2, 1, {1, 2}, 200), p).leave);  // asked once
  v = guard.judge(outside(300), p);
  CHECK(v.show.find("Custom Ghosts") != std::string::npos);   // only the host has it
  CHECK(v.show.find("Playable Ghost") != std::string::npos);  // another build
  CHECK(guard.judge(outside(400), p).show.empty());           // shown once
}

void a_leave_that_did_not_start_is_asked_again() {
  Presence q;
  Guard again(&name_of);
  q.set_local({kGhost});
  q.observe(2, {1, 2}, 0);
  hear(q, 1, {kCustom}, 0);
  CHECK(again.judge(lobby(2, 1, {1, 2}, 0), q).leave);
  again.left(false);  // the lobby's Leave was not there
  hear(q, 1, {kCustom}, joinguard::kRetryMs - 1);
  CHECK(!again.judge(lobby(2, 1, {1, 2}, joinguard::kRetryMs - 1), q).leave);
  hear(q, 1, {kCustom}, joinguard::kRetryMs);
  CHECK(again.judge(lobby(2, 1, {1, 2}, joinguard::kRetryMs), q).leave);
}

void mismatch_message_tells_how_to_update() {
  const std::string text = joinguard::mismatch_message({kGhost}, {kGhostOther}, &name_of);
  CHECK(text.substr(text.rfind('\n') + 1) == "Restart the game through PhasmoMods Loader to update, then join again.");
}

// The removal lists ALL the lobby's required mods: every mod the host runs, with its version, marking the ones the joiner
// lacks or has in another build. A name and version never decide a match: the build does.
void mismatch_message_lists_every_required_mod() {
  const Mod hostTells{"phasmomods.ghost-tells", "1111111111111111", "Ghost Tells", "1.0.1"};
  const Mod myTells{"phasmomods.ghost-tells", "2222222222222222", "Ghost Tells", "1.0.0"};
  const Mod hostOnly{"someone.new-ghosts", "3333333333333333", "New Ghosts", "2.0.0"};
  const Mod shared{"someone.shared", "5555555555555555", "Shared", "3.1.0"};
  const Mod sameVersionOtherBuild{"someone.rebuilt", "6666666666666666", "Rebuilt", "1.0.0"};
  const Mod mine{"someone.rebuilt", "7777777777777777", "Rebuilt", "1.0.0"};
  const Mod mineOnly{"old-mod", "4444444444444444", "Old Mod", ""};
  const std::string text =
      joinguard::mismatch_message({hostTells, hostOnly, shared, sameVersionOtherBuild}, {myTells, shared, mine, mineOnly}, &name_of);
  CHECK(text.find("New Ghosts 2.0.0: you don't have it") != std::string::npos);
  CHECK(text.find("Ghost Tells 1.0.1: you have 1.0.0") != std::string::npos);
  CHECK(text.find("Rebuilt 1.0.0: you have another build") != std::string::npos);
  CHECK(text.find("\nShared 3.1.0\n") != std::string::npos);  // required too, and the joiner has it
  // What rules the joiner out is highlighted (the user, 2026-11-03), and a mod it has is not.
  CHECK(text.find("<b>New Ghosts 2.0.0: you don't have it</b>") != std::string::npos);
  CHECK(text.find("<b>Ghost Tells 1.0.1: you have 1.0.0</b>") != std::string::npos);
  CHECK(text.find("<b>Shared") == std::string::npos);
  CHECK(text.find("Old Mod") == std::string::npos);           // an extra mod is not what the lobby needs
  Mod renamed = myTells;
  renamed.build = hostTells.build;
  renamed.name = "Another Name";
  renamed.version = "9.9.9";
  CHECK(joinguard::same_mods({hostTells}, {renamed}) && !joinguard::same_mods({hostTells}, {myTells}));
}

// A hostile host's list costs a bounded message; what the joiner must fix comes first, so the bound never hides it.
void mismatch_message_is_bounded() {
  std::vector<Mod> host, mine;
  for (size_t i = 0; i < presence::kMaxMods; ++i) {
    host.push_back({"m" + std::to_string(i), "0011223344556677", std::string(60, 'N'), "1.0.0"});
    if (i + 1 < presence::kMaxMods) mine.push_back(host.back());
  }
  host.back().name = "The Last One";
  const std::string text = joinguard::mismatch_message(host, mine, &name_of);
  CHECK(text.find("The Last One 1.0.0: you don't have it") != std::string::npos);
  CHECK(text.find("And " + std::to_string(presence::kMaxMods - joinguard::kMaxListedMods) + " more.") != std::string::npos);
  CHECK(text.size() < 4096);
}

void a_joiner_with_the_same_mods_stays() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kReplay, kGhost});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost, kReplay}, 0);  // the same set, in another order
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(!v.leave && v.suspend.empty() && !v.resume);
}

// A host never heard runs no mods (or an older suite that speaks another presence): every mod here goes off, and nobody
// leaves.
void vanilla_host_suspends_after_grace() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(2, {1, 2}, 0);
  CHECK(guard.judge(lobby(2, 1, {1, 2}, 0), p).suspend.empty());
  CHECK(guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs - 1), p).suspend.empty());
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs), p);
  CHECK(v.suspend == (Ids{"playable-ghost", "custom-ghosts"}) && !v.leave && !v.resume);
  CHECK(v.notice == "This lobby's host isn't running PhasmoMods, so your mods are off until you leave it.");
  p.set_local({});
  CHECK(guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs + 1), p).suspend.empty());  // said once
}

// A mod's old flag no longer lets anyone without the mods stay.
void vanilla_peers_flag_is_ignored() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});  // whatever their PmModInfo.flags say: neither presence nor the guard reads the flag
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs - 1), p).kick.empty());  // still saying hello
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), p).kick, 2));
}

void joiner_with_extra_mods_suspends_only_extras() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost, kCustom}, 0);
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(v.suspend == Ids{"replay"} && !v.leave && v.kick.empty());
}

void extras_notice_names_them() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost}, 0);
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(v.notice == "This lobby's host doesn't run Custom Ghosts, Replay, so those mods are off until you leave it.");
}

// A suspended mod takes the mods that require it with it, and the player is told which: as extras, or as what rules them out.
void suspension_cascades_to_dependents() {
  pm::modlist::Manifest custom, replay, ghost;
  custom.id = kCustom.id;
  custom.dependencies = {{kReplay.id, "", true}};
  replay.id = kReplay.id;
  ghost.id = kGhost.id;
  ghost.dependencies = {{kCustom.id, "", false}};  // optional: it stays
  const std::vector<pm::modlist::Manifest> installed{ghost, custom, replay};
  int asked = 0;
  const auto dependents = [&](const Ids& ids) {
    ++asked;
    return moddeps::dependents(ids, installed);
  };
  // The normal case: the host lacks both, so the mod that requires the other is an extra too, named once as one.
  Presence q;
  Guard plain(&name_of, dependents);
  q.set_local({kGhost, kCustom, kReplay});
  q.observe(2, {1, 2}, 0);
  hear(q, 1, {kGhost}, 0);
  Verdict v = plain.judge(lobby(2, 1, {1, 2}, 0), q);
  CHECK(v.suspend == (Ids{"custom-ghosts", "replay"}) && !v.leave && v.kick.empty());
  CHECK(v.notice == "This lobby's host doesn't run Custom Ghosts, Replay, so those mods are off until you leave it.");
  // With them off, the next judgements ask for nothing more and read the manifests no more, and leaving brings both back.
  CHECK(asked == 1);
  q.set_local({kGhost});
  for (uint64_t at = 100; at < 2000; at += 100) {
    v = plain.judge(lobby(2, 1, {1, 2}, at), q);
    CHECK(v.suspend.empty() && !v.resume && !v.leave);
  }
  CHECK(asked == 1);
  CHECK(plain.judge(outside(2000), q).resume);

  // The host runs Custom Ghosts without Replay, which this client's Custom Ghosts requires (a host whose manifest says
  // otherwise). That mod cannot run here, which is as good as lacking it: this client leaves, and once out of the room it
  // is told what the lobby requires, rather than running short of the host's list until the host's kick.
  Presence p;
  Guard guard(&name_of, dependents);
  p.set_local({kGhost, kCustom, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost, kCustom}, 0);
  v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(v.leave && v.suspend.empty() && v.notice.empty());
  guard.left(true);
  v = guard.judge(outside(100), p);
  CHECK(v.show.find("Custom Ghosts 2.0.0: yours needs a mod this lobby doesn't run") != std::string::npos);
  CHECK(v.show.find("Playable Ghost 1.0.0") != std::string::npos);
  CHECK(v.show.find("Custom Ghosts") < v.show.find("Playable Ghost"));  // what rules it out comes first

  // The extras changing asks again, and only then.
  Presence r;
  Guard again(&name_of, dependents);
  r.set_local({kGhost, kCustom, kReplay});
  r.observe(2, {1, 2}, 0);
  hear(r, 1, {kGhost, kCustom, kReplay}, 0);
  asked = 0;
  for (uint64_t at = 0; at < 500; at += 100) CHECK(!again.judge(lobby(2, 1, {1, 2}, at), r).leave);
  CHECK(asked == 0);  // no extras: nothing to ask
  hear(r, 1, {kGhost, kCustom}, 500);
  CHECK(again.judge(lobby(2, 1, {1, 2}, 500), r).leave);
  CHECK(asked == 1);
}

void joiner_missing_host_mod_is_removed() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost, kCustom}, 0);
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(v.leave && v.suspend.empty());  // a removal is never a suspension
}

void joiner_with_other_build_is_removed() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhostOther, kCustom});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost}, 0);
  const Verdict v = guard.judge(lobby(2, 1, {1, 2}, 0), p);
  CHECK(v.leave && v.suspend.empty());
}

void extras_resume_on_leaving() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost}, 0);
  CHECK(guard.judge(lobby(2, 1, {1, 2}, 0), p).suspend == Ids{"replay"});
  p.set_local({kGhost});
  CHECK(!guard.judge(lobby(2, 1, {1, 2}, 1), p).resume);
  CHECK(guard.judge(outside(2), p).resume);
  CHECK(!guard.judge(outside(3), p).resume);  // once
}

// This client's presence then names only what still runs, so the room's mod agreement sees matching lists, and the host
// sees a player with the same mods.
void suspended_client_announces_only_running_mods() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(2, {1, 2}, 0);
  hear(p, 1, {kGhost}, 0);
  CHECK(guard.judge(lobby(2, 1, {1, 2}, 0), p).suspend == Ids{"replay"});
  p.set_local({kGhost});
  Presence host;
  host.observe(1, {1, 2}, 0);
  for (const std::vector<uint8_t>& part : p.take_announcement()) host.receive(2, part.data(), part.size(), 0);
  CHECK(host.mods_of(2) && *host.mods_of(2) == std::vector<Mod>{kGhost});
}

void leaving_resumes() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(2, {1, 2}, 0);
  CHECK(guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs), p).suspend.empty());  // first seen now
  CHECK(guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs * 2), p).suspend == Ids{"playable-ghost"});
  p.set_local({});
  const Verdict v = guard.judge(outside(joinguard::kGraceMs * 2 + 1), p);
  CHECK(v.resume && v.suspend.empty() && v.show.empty());
  // The next room starts afresh.
  p.set_local({kGhost});
  p.observe(5, {4, 5}, joinguard::kGraceMs * 2 + 2);
  CHECK(guard.judge(lobby(5, 4, {4, 5}, joinguard::kGraceMs * 2 + 2), p).suspend.empty());
}

// Frames up to the grace, with the host first seen at 0: every mod off.
Verdict past_the_grace(Guard& guard, Presence& p) {
  guard.judge(lobby(2, 1, {1, 2}, 0), p);
  return guard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs), p);
}

// A host heard after the grace: mods come back in the lobby, then the matching rules apply.
void late_host_presence_resumes_then_matches() {
  Presence p;
  Guard matching(&name_of);
  p.set_local({kGhost});
  p.observe(2, {1, 2}, 0);
  CHECK(past_the_grace(matching, p).suspend == Ids{"playable-ghost"});
  p.set_local({});
  hear(p, 1, {kGhost}, joinguard::kGraceMs + 1);
  const Verdict back = matching.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs + 1), p);
  CHECK(back.resume && back.suspend.empty() && !back.leave);
  p.set_local({kGhost});
  const Verdict after = matching.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs + 2), p);
  CHECK(!after.resume && !after.leave && after.suspend.empty());

  // A host with fewer mods: only the rest stay off.
  Presence q;
  Guard subset(&name_of);
  q.set_local({kGhost, kReplay});
  q.observe(2, {1, 2}, 0);
  CHECK(past_the_grace(subset, q).suspend.size() == 2);
  q.set_local({});
  hear(q, 1, {kGhost}, joinguard::kGraceMs + 1);
  const Verdict partly = subset.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs + 1), q);
  CHECK(partly.suspend == Ids{"replay"} && !partly.resume && !partly.leave);

  // A host that lacks a match leads to the existing leave, told against every mod this client would run.
  Presence r;
  Guard mismatching(&name_of);
  r.set_local({kGhost});
  r.observe(2, {1, 2}, 0);
  CHECK(past_the_grace(mismatching, r).suspend == Ids{"playable-ghost"});
  r.set_local({});
  hear(r, 1, {kGhostOther}, joinguard::kGraceMs + 1);
  CHECK(mismatching.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs + 1), r).leave);
  const Verdict left = mismatching.judge(outside(joinguard::kGraceMs + 2), r);
  CHECK(left.resume && left.show.find("Playable Ghost 1.0.1: you have 1.0.0") != std::string::npos);
}

// A contract's load and the contract itself: nothing changes the running mods, however long the host is silent.
void never_suspends_outside_the_lobby_screen() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(2, {1, 2}, 0);
  guard.judge({2, 1, {1, 2}, false, 0}, p);
  CHECK(guard.judge({2, 1, {1, 2}, false, joinguard::kGraceMs * 5}, p).suspend.empty());
  hear(p, 1, {kGhost}, joinguard::kGraceMs * 5);
  CHECK(guard.judge({2, 1, {1, 2}, false, joinguard::kGraceMs * 5 + 1}, p).suspend.empty());
  Frame listed{2, 1, {1, 2}, false, joinguard::kGraceMs * 5 + 2};
  listed.publicRoom = true;
  CHECK(guard.judge(listed, p).suspend.empty());
}

void host_never_suspends_itself() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kReplay});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  hear(p, 2, {kGhost}, 1);
  for (uint64_t now : {uint64_t{1}, joinguard::kGraceMs, joinguard::kGraceMs * 3}) {
    const Verdict v = guard.judge(lobby(1, 1, {1, 2}, now), p);
    CHECK(v.suspend.empty() && !v.leave);
  }
  // In a public lobby it does: below.
}

// Nobody is removed from a public lobby over mods, because none run: for the host, and for a joiner.
void public_lobby_suspends_every_mod_and_removes_nobody() {
  for (const int local : {1, 2}) {
    Presence p;
    Guard guard(&name_of);
    p.set_local({kGhost, kCustom});
    const Verdict v = guard.judge(public_lobby(local, 1, 0), p);
    CHECK(v.suspend == (Ids{"playable-ghost", "custom-ghosts"}) && !v.leave && v.kick.empty());
    CHECK(v.notice == "Mods only run in private lobbies, so your mods are off until you leave this one.");
    p.set_local({});
    for (uint64_t now : {uint64_t{1}, joinguard::kGraceMs * 3}) {
      const Verdict later = guard.judge(public_lobby(local, 1, now), p);
      CHECK(later.suspend.empty() && !later.leave && later.kick.empty() && later.notice.empty());
    }
  }
}

void single_player_counts_as_private() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1}, 0);
  const Verdict v = guard.judge(lobby(1, 1, {1}, joinguard::kGraceMs * 3), p);
  CHECK(v.suspend.empty() && !v.leave && v.kick.empty());
}

// A public lobby unlists itself for a contract: reading as unlisted then does not bring the mods back, only leaving does.
void leaving_a_public_lobby_resumes() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  CHECK(guard.judge(public_lobby(1, 1, 0), p).suspend == Ids{"playable-ghost"});
  p.set_local({});
  p.observe(1, {1, 2}, 1);
  const Verdict contract = guard.judge({1, 1, {1, 2}, false, 1}, p);
  CHECK(!contract.resume && contract.suspend.empty());
  const Verdict lobbyAgain = guard.judge(lobby(1, 1, {1, 2}, 2), p);
  CHECK(!lobbyAgain.resume && lobbyAgain.suspend.empty());
  CHECK(guard.judge(outside(3), p).resume);
}

// A new master lists the room from its own stale value on coming back from a contract: suspended at once, for every mod.
void lobby_turning_public_suspends() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(1, {1, 2}, 0);
  hear(p, 2, {kGhost, kCustom}, 0);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 0), p).suspend.empty());
  const Verdict v = guard.judge(public_lobby(1, 1, 1), p);
  CHECK(v.suspend == (Ids{"playable-ghost", "custom-ghosts"}) && !v.notice.empty());
}

// A room this client cannot read as out or in changes nothing: no resume into a room it cannot judge.
void an_unreadable_room_changes_nothing() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  CHECK(guard.judge(public_lobby(1, 1, 0), p).suspend.size() == 1);
  p.set_local({});
  Frame unknown = outside(1);
  unknown.roomUnknown = true;
  const Verdict v = guard.judge(unknown, p);
  CHECK(!v.resume && v.show.empty());
  CHECK(guard.judge(outside(2), p).resume);  // it still owes the resume
}

// The game's kick refuses a sender that is not the room's first actor, on the removed player's client: this client's
// master flag is not enough, or the game flags this player for it.
void only_the_first_actor_kicks() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(3, {2, 3}, 0);
  guard.judge(lobby(3, 3, {2, 3}, 0), p);
  CHECK(guard.judge(lobby(3, 3, {2, 3}, joinguard::kGraceMs * 2), p).kick.empty());  // master, but not the lowest actor
  // The lower actor leaves: this client is first now, and a player unheard for the grace goes.
  p.observe(3, {3, 4}, joinguard::kGraceMs * 2);
  guard.judge(lobby(3, 3, {3, 4}, joinguard::kGraceMs * 2), p);
  CHECK(contains(guard.judge(lobby(3, 3, {3, 4}, joinguard::kGraceMs * 3), p).kick, 4));
  // An actor number that cannot be read trusts nothing.
  Guard unread(&name_of);
  unread.judge(lobby(3, 3, {0, 3, 5}, 0), p);
  CHECK(unread.judge(lobby(3, 3, {0, 3, 5}, joinguard::kGraceMs * 2), p).kick.empty());
}

void the_host_kicks_a_mismatched_modder_who_does_not_leave() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  hear(p, 2, {kGhostOther}, 500);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500), p).kick.empty());  // time to leave by itself
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kKickDelayMs - 1), p).kick.empty());
  hear(p, 2, {kGhostOther}, 500 + joinguard::kKickDelayMs);
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kKickDelayMs), p).kick, 2));
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 600 + joinguard::kKickDelayMs), p).kick.empty());  // once
}

// A joiner with extras has as long as anyone to say hello before it can have switched them off; if it never does, it goes.
void the_host_gives_a_joiner_with_extras_time_to_suspend_them() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  hear(p, 2, {kGhost, kReplay}, 500);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500), p).kick.empty());
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kKickDelayMs), p).kick.empty());
  hear(p, 2, {kGhost, kReplay}, 500 + joinguard::kGraceMs);
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, 500 + joinguard::kGraceMs), p).kick, 2));
  // Having switched them off, it matches and stays.
  Guard fine(&name_of);
  Presence q;
  q.set_local({kGhost});
  q.observe(1, {1, 2}, 0);
  fine.judge(lobby(1, 1, {1, 2}, 0), q);
  hear(q, 2, {kGhost, kReplay}, 500);
  fine.judge(lobby(1, 1, {1, 2}, 500), q);
  hear(q, 2, {kGhost}, 600);
  CHECK(fine.judge(lobby(1, 1, {1, 2}, 600), q).kick.empty());
  CHECK(fine.judge(lobby(1, 1, {1, 2}, 600 + joinguard::kGraceMs), q).kick.empty());
}

void a_kick_that_failed_is_tried_again() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), p).kick, 2));
  guard.kicked(2, false);  // no ID card yet
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs - 1), p).kick.empty());
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs), p).kick, 2));
  guard.kicked(2, true);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + joinguard::kRetryMs + joinguard::kKickConfirmMs - 1), p).kick.empty());
}

// The game's kick can return without doing anything, and a player's client can ignore it: a kick counts only when the player
// leaves. One still there is kicked again, a bounded number of times, and then the host is told, once. A kick the game could
// not even try uses up no kick of the player.
void a_kick_the_game_skipped_is_tried_again_then_reported() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2, 3}, 0);
  guard.judge(lobby(1, 1, {1, 2, 3}, 0), p);
  uint64_t now = joinguard::kGraceMs;
  const auto judge = [&](uint64_t at) { return guard.judge(lobby(1, 1, {1, 2, 3}, at), p); };
  Verdict v = judge(now);
  CHECK(contains(v.kick, 2) && contains(v.kick, 3));
  guard.kicked(2, true);
  guard.kicked(3, false);  // no card for this one yet: tried again without using up a kick
  v = judge(now + joinguard::kKickConfirmMs - 1);
  CHECK(!contains(v.kick, 2));
  if (contains(v.kick, 3)) guard.kicked(3, false);
  int kicksOf2 = 1;
  bool toldOnce = false;
  for (uint64_t at = now + joinguard::kKickConfirmMs; at < now + joinguard::kKickConfirmMs * 8; at += 100) {
    v = judge(at);
    if (contains(v.kick, 2)) {
      ++kicksOf2;
      guard.kicked(2, true);
    }
    if (contains(v.kick, 3)) guard.kicked(3, true);  // this one's card came, and the kick ran
    if (contains(v.unremoved, 2)) {
      CHECK(!toldOnce);
      toldOnce = true;
    }
  }
  CHECK(kicksOf2 == joinguard::kMaxKicks);
  CHECK(toldOnce);
  // Told, it asks no more, for as long as they stay.
  now += joinguard::kKickConfirmMs * 8;
  CHECK(!contains(judge(now).kick, 2) && judge(now).unremoved.empty());
  // A player whose mods come to match is fine, and starts afresh if they change again.
  hear(p, 2, {kGhost}, now);
  CHECK(!contains(judge(now + 100).kick, 2));
  hear(p, 2, {kCustom}, now + 200);
  judge(now + 300);
  CHECK(contains(judge(now + 300 + joinguard::kKickDelayMs).kick, 2));
  // A kicked player who leaves is simply gone: nothing is reported.
  Presence q;
  Guard gone(&name_of);
  q.set_local({kGhost});
  q.observe(1, {1, 2}, 0);
  gone.judge(lobby(1, 1, {1, 2}, 0), q);
  CHECK(contains(gone.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), q).kick, 2));
  gone.kicked(2, true);
  q.observe(1, {1}, joinguard::kGraceMs + 100);
  v = gone.judge(lobby(1, 1, {1}, joinguard::kGraceMs + joinguard::kKickConfirmMs * 8), q);
  CHECK(v.kick.empty() && v.unremoved.empty());
}

// A player the game has no ID card for can't be kicked. That is asked for again, but not for ever: past the bound they count as
// not removed, the host is told, once, and the start names them as stuck. A kick that runs starts the count again. A player whose
// mods come to match starts afresh.
void a_kick_the_game_can_never_try_is_given_up_on() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  uint64_t now = joinguard::kGraceMs;
  int tries = 0;
  std::vector<int> told;
  for (int step = 0; step < 100 && told.empty(); ++step, now += joinguard::kRetryMs) {
    const Verdict v = guard.judge(lobby(1, 1, {1, 2}, now), p);
    told = v.unremoved;
    if (contains(v.kick, 2)) {
      ++tries;
      guard.kicked(2, false);
      CHECK(guard.start_answer(lobby(1, 1, {1, 2}, now), p).go == false);
    }
  }
  CHECK(tries == joinguard::kMaxUntriedKicks);
  CHECK(told == std::vector<int>{2});
  joinguard::StartAnswer a = guard.start_answer(lobby(1, 1, {1, 2}, now), p);
  CHECK(!a.go && a.stuck == std::vector<int>{2} && a.waiting.empty() && !a.notFirstActor);
  for (int step = 0; step < 5; ++step, now += joinguard::kRetryMs) {
    const Verdict v = guard.judge(lobby(1, 1, {1, 2}, now), p);
    CHECK(v.kick.empty() && v.unremoved.empty());
  }
  hear(p, 2, {kGhost}, now);
  guard.judge(lobby(1, 1, {1, 2}, now), p);
  CHECK(guard.start_answer(lobby(1, 1, {1, 2}, now), p).go);
  // A run of them broken by a kick that ran is not the same run.
  Presence q;
  Guard broken(&name_of);
  q.set_local({kGhost});
  q.observe(1, {1, 2}, 0);
  broken.judge(lobby(1, 1, {1, 2}, 0), q);
  now = joinguard::kGraceMs;
  for (int i = 0; i < joinguard::kMaxUntriedKicks - 1; ++i, now += joinguard::kRetryMs) {
    CHECK(contains(broken.judge(lobby(1, 1, {1, 2}, now), q).kick, 2));
    broken.kicked(2, false);
  }
  CHECK(contains(broken.judge(lobby(1, 1, {1, 2}, now), q).kick, 2));
  broken.kicked(2, true);
  now += joinguard::kKickConfirmMs;
  CHECK(contains(broken.judge(lobby(1, 1, {1, 2}, now), q).kick, 2));
  broken.kicked(2, false);
  CHECK(broken.judge(lobby(1, 1, {1, 2}, now + joinguard::kRetryMs), q).unremoved.empty());
}

// A joiner that has not heard a late host switches every mod off and says so. Heard to run none is not heard to differ: the
// host gives it the full grace from then, and it stays when the host's presence reaches it and its mods come back.
void a_late_host_does_not_kick_a_joiner_that_switched_every_mod_off() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  hear(p, 2, {kGhost}, 100);  // it ran the same mods
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 200), p).kick.empty());
  hear_none(p, 2, 10000);  // its farewell: it heard nothing from this host for its grace
  guard.judge(lobby(1, 1, {1, 2}, 10000), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 10000 + joinguard::kKickDelayMs), p).kick.empty());
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 10000 + joinguard::kGraceMs - 1), p).kick.empty());
  hear(p, 2, {kGhost}, 10000 + joinguard::kGraceMs - 1);  // this host's presence arrived: its mods are back
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 10000 + joinguard::kGraceMs * 3), p).kick.empty());
  // One that stays switched off is removed once the grace is over.
  hear_none(p, 2, 50000);
  guard.judge(lobby(1, 1, {1, 2}, 50000), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, 50000 + joinguard::kGraceMs - 1), p).kick.empty());
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, 50000 + joinguard::kGraceMs), p).kick, 2));
  // Going off and on again does not hold a kick off for ever: the grace runs from the first time it was heard so, until its
  // mods match.
  Presence q;
  Guard flapping(&name_of);
  q.set_local({kGhost});
  q.observe(1, {1, 2}, 0);
  flapping.judge(lobby(1, 1, {1, 2}, 0), q);
  hear_none(q, 2, 100);
  flapping.judge(lobby(1, 1, {1, 2}, 100), q);
  hear(q, 2, {kCustom}, 200);
  flapping.judge(lobby(1, 1, {1, 2}, 200), q);
  hear_none(q, 2, 300);
  CHECK(contains(flapping.judge(lobby(1, 1, {1, 2}, 100 + joinguard::kGraceMs), q).kick, 2));
}

// A player who becomes the host judges at once those it has long known: the grace runs from when
// it first saw them, not from when it became the host.
void a_new_host_enforces_at_once() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(2, {1, 2, 3}, 0);
  hear(p, 1, {kGhost, kCustom}, 0);
  guard.judge(lobby(2, 1, {1, 2, 3}, 0), p);
  CHECK(guard.judge(lobby(2, 1, {1, 2, 3}, joinguard::kGraceMs), p).kick.empty());  // not the host
  p.observe(2, {2, 3}, joinguard::kGraceMs + 1);  // the host left; actor 2 took over
  CHECK(contains(guard.judge(lobby(2, 2, {2, 3}, joinguard::kGraceMs + 1), p).kick, 3));
}

void the_host_kicks_a_vanilla_player() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs - 1), p).kick.empty());  // still saying hello
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), p).kick, 2));
}

// A player who chose "Play without mods" has no mod running, so its loader says nothing, whoever is in the room. To a
// modded host it reads as a vanilla player, and the host removes it like one, which is correct.
void a_declined_player_sends_nothing_and_is_removed_as_vanilla() {
  Presence declined;
  declined.observe(2, {1, 2}, 0);
  CHECK(declined.take_announcement().empty());
  declined.observe(2, {1, 2}, presence::kAnnounceMs);
  CHECK(declined.take_announcement().empty());
  Guard itsGuard(&name_of);
  const Verdict own = itsGuard.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs), declined);
  CHECK(!own.leave && own.kick.empty() && own.suspend.empty() && own.notice.empty());

  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost});
  host.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), host);
  CHECK(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs - 1), host).kick.empty());
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), host).kick, 2));
}

// A contract's load pauses Photon's messages: everyone falls silent, and nobody is judged for it.
void a_silent_contract_load_judges_nobody() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(1, {1, 2}, 0);
  hear(p, 2, {kGhost, kCustom}, 0);
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
  hear(q, 1, {kCustom}, 0);
  const Verdict v = joiner.judge(lobby(2, 1, {1, 2}, joinguard::kGraceMs), q);
  CHECK(!v.leave && v.suspend.empty());
  CHECK(joiner.judge(public_lobby(2, 1, joinguard::kGraceMs), q).suspend.empty());
}

void leaving_the_room_forgets_everything() {
  Presence p;
  Guard guard(&name_of);
  p.set_local({kGhost, kCustom});
  p.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), p);
  guard.judge(outside(1), p);
  p.observe(1, {1, 3}, 2);
  guard.judge(lobby(1, 1, {1, 3}, 2), p);
  CHECK(guard.judge(lobby(1, 1, {1, 3}, joinguard::kGraceMs), p).kick.empty());  // first seen at 2
  CHECK(contains(guard.judge(lobby(1, 1, {1, 3}, joinguard::kGraceMs + 2), p).kick, 3));
}

// One client: what presence heard, the answer it reads from that, and the guard, as the host runs them.
struct Client {
  content::Agreement agreement;
  Presence presence;
  Guard guard{&name_of};
  int local = 0, master = 0;
  Client(int self, int host) : local(self), master(host) {}
  void mine(const std::vector<Mod>& mods) {
    presence.set_local(mods);
    std::vector<content::ModBuild> builds;
    for (const Mod& m : mods) builds.push_back({m.id, m.build});
    agreement.set_local(std::move(builds));
  }
  // `actors` holds this client too.
  void observe(const std::vector<int>& actors, bool contract = false, bool isPrivate = true) {
    std::vector<int> others;
    for (int a : actors)
      if (a != local) others.push_back(a);
    agreement.observe(isPrivate, contract, local, master, others, presence.revision(), [this](int actor) -> std::optional<std::vector<content::ModBuild>> {
      const std::vector<Mod>* mods = presence.mods_of(actor);
      if (!mods) return std::nullopt;
      std::vector<content::ModBuild> out;
      for (const Mod& m : *mods) out.push_back({m.id, m.build});
      return out;
    });
  }
};

// The list is what the master runs. Nobody else's mods come into the host's answer, and each other client asks about the
// master alone: what a third player runs is no part of its answer.
void host_list_is_the_lobbys_list() {
  Client host(1, 1), joiner(2, 1);
  host.mine({kGhost, kCustom});
  joiner.mine({kCustom, kGhost});  // the same set, in another order
  host.observe({1});
  CHECK(host.agreement.agreed());  // from the moment the lobby opens
  host.presence.observe(1, {1, 2}, 0);
  host.observe({1, 2});
  CHECK(host.agreement.agreed());  // with no waiting on anyone
  joiner.presence.observe(2, {1, 2}, 0);
  joiner.observe({1, 2});
  CHECK(!joiner.agreement.agreed());  // it has not heard the host
  hear(joiner.presence, 3, {kGhost, kCustom}, 0);
  joiner.observe({1, 2, 3});
  CHECK(!joiner.agreement.agreed());  // another player saying the right list is not the host saying it
  hear(joiner.presence, 1, {kGhost, kCustom}, 0);
  joiner.observe({1, 2, 3});
  CHECK(joiner.agreement.agreed());
  hear(joiner.presence, 3, {kReplay}, 0);
  joiner.observe({1, 2, 3});
  CHECK(joiner.agreement.agreed());  // a third player's mods change nothing here
  // Outside a private room there is no list to match, on the host either.
  host.observe({}, false, false);
  CHECK(!host.agreement.agreed());
}

// A vanilla joiner, a mismatched joiner and a never-heard joiner each leave the host's and the other players' mods running.
void joiner_cannot_switch_off_the_hosts_mods() {
  for (const std::vector<Mod>& said : {std::vector<Mod>{}, {kReplay}, {kGhostOther, kCustom}, {kGhost}, {kGhost, kCustom, kReplay}}) {
    Client host(1, 1), other(2, 1);
    host.mine({kGhost, kCustom});
    other.mine({kGhost, kCustom});
    host.presence.observe(1, {1, 2, 3}, 0);
    other.presence.observe(2, {1, 2, 3}, 0);
    hear(host.presence, 2, {kGhost, kCustom}, 0);
    hear(other.presence, 1, {kGhost, kCustom}, 0);
    host.observe({1, 2, 3});  // player 3 has joined and not been heard: the never-heard case
    other.observe({1, 2, 3});
    CHECK(host.agreement.agreed() && other.agreement.agreed());
    const unsigned hostDecisions = host.agreement.decisions(), otherDecisions = other.agreement.decisions();
    CHECK(host.guard.judge(lobby(1, 1, {1, 2, 3}, 0), host.presence).suspend.empty());
    if (said.empty()) hear_none(host.presence, 3, 0);
    else hear(host.presence, 3, said, 0);
    if (said.empty()) hear_none(other.presence, 3, 0);
    else hear(other.presence, 3, said, 0);
    host.observe({1, 2, 3});
    other.observe({1, 2, 3});
    CHECK(host.agreement.agreed() && other.agreement.agreed());
    CHECK(host.agreement.decisions() == hostDecisions && other.agreement.decisions() == otherDecisions);
    CHECK(host.agreement.unverified({1, 2, 3}) == std::vector<int>{3});  // who is held back from, and nothing more
    CHECK(other.agreement.unverified({1, 2, 3}) == std::vector<int>{3});
    const Verdict v = host.guard.judge(lobby(1, 1, {1, 2, 3}, joinguard::kKickDelayMs), host.presence);
    CHECK(v.suspend.empty() && !v.leave && v.notice.empty());
    CHECK(!contains(v.kick, 2));
  }
}

void joiner_with_host_mods_runs_them() {
  Client joiner(2, 1);
  joiner.mine({kGhost, kCustom});
  joiner.presence.observe(2, {1, 2}, 0);
  hear(joiner.presence, 1, {kGhost, kCustom}, 0);
  joiner.observe({1, 2});
  const Verdict v = joiner.guard.judge(lobby(2, 1, {1, 2}, 0), joiner.presence);
  CHECK(joiner.agreement.agreed() && !v.leave && v.suspend.empty());
  CHECK(joiner.agreement.peer_runs(1, kGhost.id) && !joiner.agreement.peer_runs(1, kReplay.id));
}

// Its extras go off and it stays, and then it matches, as the host then finds too.
void joiner_extras_off_and_it_stays() {
  Client joiner(2, 1), host(1, 1);
  host.mine({kGhost});
  joiner.mine({kGhost, kReplay});
  joiner.presence.observe(2, {1, 2}, 0);
  host.presence.observe(1, {1, 2}, 0);
  hear(joiner.presence, 1, {kGhost}, 0);
  hear(host.presence, 2, {kGhost, kReplay}, 0);
  joiner.observe({1, 2});
  host.observe({1, 2});
  CHECK(!joiner.agreement.agreed() && host.agreement.unverified({1, 2}) == std::vector<int>{2});
  const Verdict v = joiner.guard.judge(lobby(2, 1, {1, 2}, 0), joiner.presence);
  CHECK(v.suspend == Ids{kReplay.id} && !v.leave);
  joiner.mine({kGhost});  // what the host runs of apply_to_room
  joiner.observe({1, 2});
  CHECK(joiner.agreement.agreed());
  hear(host.presence, 2, {kGhost}, 1000);
  host.observe({1, 2});
  CHECK(host.agreement.unverified({1, 2}).empty());
  CHECK(host.guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs + 1000), host.presence).kick.empty());
}

void vanilla_joiner_is_kicked_after_grace() {
  Client host(1, 1);
  host.mine({kGhost});
  host.presence.observe(1, {1, 2}, 0);
  host.guard.judge(lobby(1, 1, {1, 2}, 0), host.presence);
  CHECK(host.guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs - 1), host.presence).kick.empty());
  CHECK(contains(host.guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), host.presence).kick, 2));
}

void mismatched_joiner_leaves_with_the_list() {
  Client joiner(2, 1);
  joiner.mine({kGhostOther});
  joiner.presence.observe(2, {1, 2}, 0);
  hear(joiner.presence, 1, {kGhost, kCustom}, 0);
  joiner.observe({1, 2});
  CHECK(!joiner.agreement.agreed());
  const Verdict v = joiner.guard.judge(lobby(2, 1, {1, 2}, 0), joiner.presence);
  CHECK(v.leave);
  joiner.guard.left(true);
  const Verdict out = joiner.guard.judge(outside(1), joiner.presence);
  CHECK(out.show.find("Custom Ghosts 2.0.0: you don't have it") != std::string::npos);
  CHECK(out.show.find("Playable Ghost 1.0.0: you have 1.0.1") != std::string::npos);
}

// Whatever a player says of themselves touches only that player: saying the host's list gets them treated as verified, and
// they have only themselves to blame for what arrives. Saying anything else, or nonsense, changes nothing for anyone else.
void hostile_announcement_changes_nothing_for_others() {
  Client host(1, 1), other(2, 1);
  host.mine({kGhost});
  other.mine({kGhost});
  host.presence.observe(1, {1, 2, 3}, 0);
  other.presence.observe(2, {1, 2, 3}, 0);
  hear(host.presence, 2, {kGhost}, 0);
  hear(other.presence, 1, {kGhost}, 0);
  host.observe({1, 2, 3});
  other.observe({1, 2, 3});
  const unsigned before = host.agreement.decisions();
  std::vector<Mod> huge;
  for (size_t i = 0; i < 40; ++i) huge.push_back({"hostile.m" + std::to_string(i), "0011223344556677", "Hostile", "1.0.0"});
  hear(host.presence, 3, huge, 0);
  const std::vector<uint8_t> nonsense{'H', 'M', presence::kVersion, 9, 0, 1, 200, 1, 2, 3};
  host.presence.receive(3, nonsense.data(), nonsense.size(), 0);
  other.presence.receive(3, nonsense.data(), nonsense.size(), 0);
  // Nor can it speak for the host: a sender is Photon's word, and what it says is kept under its own number.
  hear(other.presence, 3, {kReplay}, 0);
  host.observe({1, 2, 3});
  other.observe({1, 2, 3});
  CHECK(host.agreement.agreed() && other.agreement.agreed() && host.agreement.decisions() == before);
  CHECK(host.agreement.verified(2) && !host.agreement.verified(3));
  CHECK(other.agreement.verified(1) && !other.agreement.verified(3));
  const content::ModBuild ghost{kGhost.id, kGhost.build};
  CHECK(!host.agreement.delivers(3, 1, ghost) && host.agreement.delivers(2, 1, ghost));
  // Saying the very list: verified, and no more than that.
  hear(host.presence, 3, {kGhost}, 1);
  host.observe({1, 2, 3});
  CHECK(host.agreement.verified(3) && host.agreement.agreed() && host.agreement.decisions() == before);
}

// A contract holds every answer as it loaded, whoever leaves, whatever arrives, and the lobby after it decides afresh.
void no_decision_changes_during_a_contract() {
  const content::ModBuild ghost{kGhost.id, kGhost.build};
  Client a(1, 1), b(2, 1);
  a.mine({kGhost});
  b.mine({kGhost});
  a.presence.observe(1, {1, 2, 3}, 0);
  b.presence.observe(2, {1, 2, 3}, 0);
  hear(a.presence, 2, {kGhost}, 0);
  hear(a.presence, 3, {kGhost}, 0);
  hear(b.presence, 1, {kGhost}, 0);
  hear(b.presence, 3, {kGhost}, 0);
  a.observe({1, 2, 3});
  b.observe({1, 2, 3});
  CHECK(a.agreement.agreed() && b.agreement.agreed());
  a.observe({1, 2, 3}, true);
  b.observe({1, 2, 3}, true);
  const unsigned decisions = b.agreement.decisions();
  // The host's list seems to change, a player leaves, and the host leaves: nothing moves.
  hear(b.presence, 1, {kReplay}, 1000);
  b.observe({1, 2, 3}, true);
  CHECK(b.agreement.agreed());
  b.observe({1, 2}, true);
  CHECK(b.agreement.agreed());
  b.master = 2;
  b.observe({2}, true);
  CHECK(b.agreement.agreed() && b.agreement.decisions() == decisions);
  CHECK(a.agreement.agreed() && a.agreement.delivers(2, 1, ghost));

  // Loaded with no: no it stays. Nothing starts partway through a contract, the race that lets a player in at the last moment
  // included. The lobby after decides afresh.
  Client late(3, 1);
  late.mine({kGhost});
  late.presence.observe(3, {1, 3}, 0);
  late.observe({1, 3}, true);
  CHECK(!late.agreement.agreed());
  const unsigned lateDecisions = late.agreement.decisions();
  hear(late.presence, 1, {kGhost}, 2000);
  late.observe({1, 3}, true);
  CHECK(!late.agreement.agreed() && late.agreement.decisions() == lateDecisions && !late.agreement.delivers(1, 3, ghost));
  late.observe({1, 3}, false);
  CHECK(late.agreement.agreed() && late.agreement.decisions() == lateDecisions + 1);
}

// The host presses start while a player is unverified: the start does not go, and it names who it waits for.
void start_waits_for_an_unverified_player() {
  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost});
  host.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), host);
  joinguard::StartAnswer a = guard.start_answer(lobby(1, 1, {1, 2}, 1), host);
  CHECK(!a.go && a.waiting == std::vector<int>{2} && a.stuck.empty());
  // Asked before the guard has seen them at all: a join that arrived this very frame.
  a = guard.start_answer(lobby(1, 1, {1, 2, 7}, 1), host);
  CHECK(!a.go && a.waiting == (std::vector<int>{2, 7}));
  // Announcing anything else is no better.
  hear(host, 2, {kGhost, kReplay}, 2);
  a = guard.start_answer(lobby(1, 1, {1, 2}, 3), host);
  CHECK(!a.go && a.waiting == std::vector<int>{2});
  // What it could not read holds it too, with nobody to name.
  joinguard::Frame unread = lobby(1, 1, {}, 3);
  unread.listRead = false;
  a = guard.start_answer(unread, host);
  CHECK(!a.go && a.listUnread && a.waiting.empty());
  CHECK(joinguard::start_wait_notice({}, {}, true).find("can't read who's in this lobby") != std::string::npos);
  const std::string text = joinguard::start_wait_notice({"Alice", "Bob"}, {}, false);
  CHECK(text == "Waiting for Alice and Bob. The loader is still checking their mods. Press Start again once that's done.");
}

void start_goes_once_everyone_is_verified() {
  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost, kCustom});
  host.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), host);
  CHECK(!guard.start_answer(lobby(1, 1, {1, 2}, 0), host).go);
  hear(host, 2, {kCustom, kGhost}, 1);
  CHECK(guard.start_answer(lobby(1, 1, {1, 2}, 1), host).go);  // at once, with the guard yet to look
  guard.judge(lobby(1, 1, {1, 2}, 2), host);
  // An announcement that lapses still counts while they stay, as the guard keeps it.
  host.observe(1, {1, 2}, presence::kTimeoutMs + 100);
  CHECK(!host.mods_of(2));
  CHECK(guard.start_answer(lobby(1, 1, {1, 2}, presence::kTimeoutMs + 100), host).go);
  // Alone, in single player or an empty lobby, there is nobody to wait for.
  Presence alone;
  Guard aloneGuard(&name_of);
  alone.set_local({kGhost});
  CHECK(aloneGuard.start_answer(lobby(1, 1, {1}, 0), alone).go);
  // Not the host's call elsewhere: a client, a client with no mods, a public lobby, and out of a room.
  CHECK(guard.start_answer(lobby(2, 1, {1, 2, 9}, 0), host).go);
  Presence none;
  CHECK(guard.start_answer(lobby(1, 1, {1, 9}, 0), none).go);
  CHECK(guard.start_answer(public_lobby(1, 1, 0), host).go);
  CHECK(guard.start_answer(outside(0), host).go);
}

// Asking changes nothing: a press removes nobody and moves no player's time on. The kick still falls when it would have.
void pressing_start_kicks_nobody() {
  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost});
  host.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), host);
  for (uint64_t now = 100; now < joinguard::kGraceMs; now += 100) {
    CHECK(!guard.start_answer(lobby(1, 1, {1, 2}, now), host).go);
    CHECK(guard.judge(lobby(1, 1, {1, 2}, now), host).kick.empty());
  }
  CHECK(contains(guard.judge(lobby(1, 1, {1, 2}, joinguard::kGraceMs), host).kick, 2));
}

// The wait always ends. A player verified, gone or removed ends it; one the game will not remove keeps it refused while they
// stay, names them, and goes when they do. The host is told, and nothing else stands in its way.
void wait_ends_when_the_player_is_removed_or_leaves() {
  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost});
  host.observe(1, {1, 2}, 0);
  guard.judge(lobby(1, 1, {1, 2}, 0), host);
  CHECK(!guard.start_answer(lobby(1, 1, {1, 2}, 0), host).go);
  // It leaves.
  host.observe(1, {1}, 1000);
  guard.judge(lobby(1, 1, {1}, 1000), host);
  CHECK(guard.start_answer(lobby(1, 1, {1}, 1000), host).go);
  // The next is kicked after the grace and goes.
  host.observe(1, {1, 3}, 2000);
  uint64_t now = 2000;
  guard.judge(lobby(1, 1, {1, 3}, now), host);
  now += joinguard::kGraceMs;
  CHECK(contains(guard.judge(lobby(1, 1, {1, 3}, now), host).kick, 3));
  CHECK(guard.start_answer(lobby(1, 1, {1, 3}, now), host).waiting == std::vector<int>{3});
  host.observe(1, {1}, now + 100);
  guard.judge(lobby(1, 1, {1}, now + 100), host);
  CHECK(guard.start_answer(lobby(1, 1, {1}, now + 100), host).go);
  // The next stays through every kick: stuck, named as such, and refused until it goes.
  host.observe(1, {1, 4}, now + 200);
  now += 200;
  guard.judge(lobby(1, 1, {1, 4}, now), host);
  now += joinguard::kGraceMs;
  std::vector<int> unremoved;
  for (int tries = 0; tries < 20 && unremoved.empty(); ++tries) {
    unremoved = guard.judge(lobby(1, 1, {1, 4}, now), host).unremoved;
    if (unremoved.empty()) now += joinguard::kKickConfirmMs;
  }
  CHECK(unremoved == std::vector<int>{4});
  joinguard::StartAnswer a = guard.start_answer(lobby(1, 1, {1, 4}, now), host);
  CHECK(!a.go && a.stuck == std::vector<int>{4} && a.waiting.empty());
  CHECK(joinguard::start_wait_notice({}, {"Mallory"}, false) ==
        "The game didn't remove Mallory, whose mods differ from yours. A contract can't start while they're here. Ask them to leave, or start a new lobby.");
  host.observe(1, {1}, now + 100);
  guard.judge(lobby(1, 1, {1}, now + 100), host);
  CHECK(guard.start_answer(lobby(1, 1, {1}, now + 100), host).go);
  // A master who is not the first actor can kick nobody, so nothing it could wait for will happen: it says so.
  Presence second;
  Guard secondGuard(&name_of);
  second.set_local({kGhost});
  second.observe(5, {3, 5}, 0);
  a = secondGuard.start_answer(lobby(5, 5, {3, 5}, 0), second);
  CHECK(!a.go && a.stuck == std::vector<int>{3} && a.notFirstActor);
  CHECK(joinguard::start_wait_notice({}, {"Mallory"}, false, true) ==
        "The loader can't verify Mallory, and the game only lets the player who's been here longest remove someone. A contract can't start while they're here. Ask them to leave, or start a new lobby.");
  CHECK(!a.go && secondGuard.start_answer(lobby(3, 3, {3, 5}, 0), second).notFirstActor == false);
}

// A master that can't be read could be this client: the start waits, and says it can't read the room.
void an_unread_master_refuses_the_start() {
  Presence host;
  Guard guard(&name_of);
  host.set_local({kGhost});
  host.observe(1, {1}, 0);
  const joinguard::StartAnswer a = guard.start_answer(lobby(1, 0, {1}, 0), host);
  CHECK(!a.go && a.listUnread);
  Presence none;
  CHECK(guard.start_answer(lobby(1, 0, {1}, 0), none).go);  // no mods here: nothing to hold
  CHECK(guard.start_answer(outside(0), host).go);           // out of a room: no master to read
}

// A player's name is theirs to choose, and the popup draws rich text.
void a_players_name_is_made_safe_to_show() {
  CHECK(joinguard::shown_player("Alice", 2) == "Alice");
  CHECK(joinguard::shown_player("<b>Al\nice</b>", 2) == "bAlice/b");
  CHECK(joinguard::shown_player("", 7) == "Player 7");
  CHECK(joinguard::shown_player("<>", 7) == "Player 7");
  CHECK(joinguard::shown_player(std::string(100, 'x'), 7) == std::string(32, 'x'));
  // Non-Latin names keep every whole character: what is cut is the part of the one the limit split, and no more.
  const std::string cyrillic = "\xD0\x90";  // two bytes
  std::string sixteen, twenty;
  for (int i = 0; i < 16; ++i) sixteen += cyrillic;
  twenty = sixteen + cyrillic + cyrillic + cyrillic + cyrillic;
  CHECK(joinguard::shown_player(sixteen, 7) == sixteen);
  CHECK(joinguard::shown_player(twenty, 7) == sixteen);
  CHECK(joinguard::shown_player("x" + twenty, 7) == "x" + sixteen.substr(0, 30));
  const std::string kanji = "\xE6\xBC\xA2";  // three bytes
  std::string ten, twelve;
  for (int i = 0; i < 10; ++i) ten += kanji;
  twelve = ten + kanji + kanji;
  CHECK(joinguard::shown_player(ten, 7) == ten);
  CHECK(joinguard::shown_player(twelve, 7) == ten);
  CHECK(joinguard::shown_player("<" + ten + ">", 7) == ten);  // nothing is cut, so nothing else goes
  const std::string emoji = "\xF0\x9F\x91\xBB";  // four bytes, and the limit lands right after one
  std::string eight;
  for (int i = 0; i < 8; ++i) eight += emoji;
  CHECK(joinguard::shown_player(eight + emoji, 7) == eight);
  CHECK(joinguard::shown_player("xy" + eight, 7) == "xy" + eight.substr(0, 28));
  CHECK(joinguard::joined_names({"A", "B", "C"}) == "A, B and C");
}

// When the master leaves the next takes over with its own list. A client whose mods equal it is unmoved; one whose mods differ
// follows the same rule as a joiner.
void new_host_list_takes_over() {
  Client stays(2, 1), mismatched(3, 1);
  stays.mine({kGhost});
  mismatched.mine({kGhost, kReplay});
  stays.presence.observe(2, {1, 2, 3}, 0);
  mismatched.presence.observe(3, {1, 2, 3}, 0);
  hear(stays.presence, 1, {kGhost}, 0);
  hear(stays.presence, 3, {kGhost, kReplay}, 0);
  hear(mismatched.presence, 1, {kGhost, kReplay}, 0);
  hear(mismatched.presence, 2, {kGhost}, 0);
  stays.observe({1, 2, 3});
  mismatched.observe({1, 2, 3});
  CHECK(stays.agreement.agreed() && mismatched.agreement.agreed());
  CHECK(stays.agreement.unverified({1, 2, 3}) == std::vector<int>{3});
  // Player 1 leaves. Player 2 is the host now: yes at once, with no word from anyone, and its list decides who is verified.
  stays.master = 2;
  mismatched.master = 2;
  stays.observe({2, 3});
  mismatched.observe({2, 3});
  CHECK(stays.agreement.agreed() && stays.agreement.unverified({2, 3}) == std::vector<int>{3});
  CHECK(!mismatched.agreement.agreed());
  const Verdict v = mismatched.guard.judge(lobby(3, 2, {2, 3}, 0), mismatched.presence);
  CHECK(v.suspend == Ids{kReplay.id} && !v.leave);
  // A new host whose list is the player's own, as one verified always was, moves nothing.
  mismatched.mine({kGhost});
  mismatched.observe({2, 3});
  CHECK(mismatched.agreement.agreed());
}

// A private room's player list that cannot be read says nothing about who is there. It forgets nobody, and it is no room with
// nobody else in it; the answer about the host stands, since the host's word and this client's list are read apart from it.
void an_unreadable_player_list_changes_little() {
  const content::Agreement::ModsOf nobody = [](int) -> std::optional<std::vector<content::ModBuild>> { return std::nullopt; };
  content::Agreement alone;
  alone.set_local({{kGhost.id, kGhost.build}});
  alone.observe(true, false, 1, 0, {}, 1, nobody, false);  // the master unread too: nothing proves this client hosts
  CHECK(!alone.agreed());
  alone.observe(true, false, 1, 0, {}, 1, nobody, true);
  CHECK(alone.agreed());  // alone, readably
  Client s(1, 2);
  s.mine({kGhost});
  s.presence.observe(1, {1, 2}, 0);
  hear(s.presence, 2, {kGhost}, 0);
  s.observe({1, 2});
  CHECK(s.agreement.agreed());
  s.agreement.observe(true, false, 1, 2, {}, s.presence.revision(), nobody, false);
  CHECK(s.agreement.agreed());  // the host's list is not forgotten by one
  s.agreement.observe(true, false, 1, 2, {2}, s.presence.revision(), nobody, true);
  CHECK(s.agreement.agreed());
  // An unread number proves nothing.
  CHECK(!s.agreement.verified(0));
  s.agreement.observe(true, false, 1, 0, {2}, s.presence.revision(), nobody, true);
  CHECK(!s.agreement.agreed() && !s.agreement.verified(2));
}

// A mod's message comes only from a verified player, to a client that matches the host.
void a_mods_channel_takes_only_verified_players() {
  const content::ModBuild ghost{kGhost.id, kGhost.build};
  const content::ModBuild custom{kCustom.id, kCustom.build};
  Client room(1, 1);
  room.mine({kGhost, kCustom});
  room.presence.observe(1, {1, 2}, 0);
  room.observe({1, 2});
  CHECK(!room.agreement.delivers(2, 1, ghost));  // not heard yet

  // A v0.1.0 peer: its version 2 announcement reads as nothing here, so it is never heard, whatever it sends.
  const std::vector<uint8_t> version2{'H', 'M', 2, 2, 14, 'p', 'l', 'a', 'y', 'a', 'b', 'l', 'e', '-', 'g', 'h', 'o', 's', 't',
                                      1, 1, 1, 1, 1, 1, 1, 1};
  room.presence.receive(2, version2.data(), version2.size(), 0);
  room.observe({1, 2});
  CHECK(!room.presence.mods_of(2) && room.agreement.agreed());
  CHECK(!room.agreement.delivers(2, 1, ghost) && !room.agreement.delivers(2, 1, custom));

  // The mod in another build, or fewer mods: not verified, so nothing passes from them. The host's own answer stands.
  hear(room.presence, 2, {kGhostOther, kCustom}, 0);
  room.observe({1, 2});
  CHECK(room.agreement.agreed() && !room.agreement.delivers(2, 1, ghost) && !room.agreement.delivers(2, 1, custom));
  hear(room.presence, 2, {kGhost}, 0);
  room.observe({1, 2});
  CHECK(!room.agreement.delivers(2, 1, ghost));

  // The same mods in the same builds: each mod's channel opens, for that player only.
  hear(room.presence, 2, {kCustom, kGhost}, 0);
  room.observe({1, 2});
  CHECK(room.agreement.delivers(2, 1, ghost) && room.agreement.delivers(2, 1, custom));
  CHECK(!room.agreement.delivers(3, 1, ghost));                                // nobody heard of
  CHECK(!room.agreement.delivers(2, 1, {kReplay.id, kReplay.build}));          // a mod this client does not run
  CHECK(!room.agreement.delivers(2, 1, {kGhost.id, kGhostOther.build}));       // not in this client's build

  // A third player who differs shuts nothing, for the players who match.
  hear(room.presence, 3, {kReplay}, 0);
  room.observe({1, 2, 3});
  CHECK(room.agreement.delivers(2, 1, ghost) && !room.agreement.delivers(3, 1, ghost));

  // A player who has gone gets nothing through, and a public room passes nothing.
  room.observe({1});
  CHECK(!room.agreement.delivers(2, 1, ghost));
  room.observe({1, 2}, false, false);
  CHECK(!room.agreement.delivers(2, 1, ghost));

  // A client that does not match the host hears nobody, the host included.
  Client joiner(2, 1);
  joiner.mine({kGhost, kReplay});
  joiner.presence.observe(2, {1, 2}, 0);
  hear(joiner.presence, 1, {kGhost}, 0);
  joiner.observe({1, 2});
  CHECK(!joiner.agreement.agreed() && !joiner.agreement.delivers(1, 2, ghost));

  // Single player: a client's own message, raised for itself, passes; it names no other sender.
  Client alone(1, 1);
  alone.mine({kGhost});
  alone.presence.observe(1, {1}, 0);
  alone.observe({1});
  CHECK(alone.agreement.delivers(1, 1, ghost));
  CHECK(!alone.agreement.delivers(1, 0, ghost));  // this client's number cannot be read: nothing proved
  CHECK(!alone.agreement.delivers(2, 1, ghost));
}

void notice_says_private_lobbies() {
  const consent::Notice n = consent::notice({{"Ghost Tells", false}});
  CHECK(n.body.find("Mods only run in private lobbies and single player, and switch off in public lobbies. Play only "
                    "with friends who also agreed to this.") != std::string::npos);
  CHECK(n.body.find("Play with mods only in private lobbies") == std::string::npos);
  CHECK(n.body.find("achievement") == std::string::npos);  // the loader leaves Steam's achievements alone
  // The notice as it read before this rule, so everyone who agreed to that one is asked again.
  consent::Notice old = n;
  old.body =
      "These mods change Phasmophobia in ways Kinetic Games does not support. Read this before you agree.\n\n"
      "<b>Your account is at risk.</b> The game's anti-cheat can report a modified game to Kinetic Games, which may ban "
      "the account. Play with mods only in private lobbies, and only with friends who also agreed to this.\n\n"
      "<b>Your progress is at your own risk.</b> Mods can break saves, progress, money and items. Nobody who made these "
      "mods is responsible for anything you lose.\n\nMods installed: <b>Ghost Tells</b>. Choose which run from the Mods "
      "button on the main menu.";
  const pm::build::Hashes game{"aaaa", "bbbb"};
  CHECK(consent::fingerprint(n, game) != consent::fingerprint(old, game));
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  a_joiner_with_other_mods_leaves_and_is_told_why_once_out();
  a_leave_that_did_not_start_is_asked_again();
  a_kick_that_failed_is_tried_again();
  a_new_host_enforces_at_once();
  mismatch_message_tells_how_to_update();
  mismatch_message_lists_every_required_mod();
  mismatch_message_is_bounded();
  a_joiner_with_the_same_mods_stays();
  vanilla_host_suspends_after_grace();
  vanilla_peers_flag_is_ignored();
  joiner_with_extra_mods_suspends_only_extras();
  extras_notice_names_them();
  suspension_cascades_to_dependents();
  joiner_missing_host_mod_is_removed();
  joiner_with_other_build_is_removed();
  extras_resume_on_leaving();
  suspended_client_announces_only_running_mods();
  leaving_resumes();
  late_host_presence_resumes_then_matches();
  never_suspends_outside_the_lobby_screen();
  host_never_suspends_itself();
  public_lobby_suspends_every_mod_and_removes_nobody();
  single_player_counts_as_private();
  leaving_a_public_lobby_resumes();
  lobby_turning_public_suspends();
  an_unreadable_room_changes_nothing();
  only_the_first_actor_kicks();
  the_host_kicks_a_mismatched_modder_who_does_not_leave();
  the_host_gives_a_joiner_with_extras_time_to_suspend_them();
  the_host_kicks_a_vanilla_player();
  a_declined_player_sends_nothing_and_is_removed_as_vanilla();
  a_kick_the_game_skipped_is_tried_again_then_reported();
  a_kick_the_game_can_never_try_is_given_up_on();
  a_late_host_does_not_kick_a_joiner_that_switched_every_mod_off();
  a_silent_contract_load_judges_nobody();
  a_client_without_started_mods_neither_checks_nor_kicks();
  leaving_the_room_forgets_everything();
  host_list_is_the_lobbys_list();
  joiner_cannot_switch_off_the_hosts_mods();
  joiner_with_host_mods_runs_them();
  joiner_extras_off_and_it_stays();
  vanilla_joiner_is_kicked_after_grace();
  mismatched_joiner_leaves_with_the_list();
  hostile_announcement_changes_nothing_for_others();
  no_decision_changes_during_a_contract();
  start_waits_for_an_unverified_player();
  start_goes_once_everyone_is_verified();
  pressing_start_kicks_nobody();
  wait_ends_when_the_player_is_removed_or_leaves();
  an_unread_master_refuses_the_start();
  a_players_name_is_made_safe_to_show();
  new_host_list_takes_over();
  an_unreadable_player_list_changes_little();
  a_mods_channel_takes_only_verified_players();
  notice_says_private_lobbies();
  std::printf(g_failures ? "%d check(s) failed\n" : "all join guard tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
