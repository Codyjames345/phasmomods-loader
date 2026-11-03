#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "presence.h"
#include "text_table.h"

// Who may stay in a lobby, and which mods run there. The host's list is the lobby's list: every mod on it is required of every
// player, and nothing a joiner runs, says or fails to say changes it, or what any other player's mods do
// (sdk/runtime/mod_agreement.h). (PM_MOD_VANILLA_PEERS is reserved and ignored: a mod that sets it still loads, and it changes
// nothing.)
//  - A joiner lacking a host mod, or running one in another build, leaves by itself, and once out of the room is told every
//    mod the lobby requires (a note shown in the lobby would go with the lobby).
//  - A joiner whose mods are more than the host's, each of the host's in the same build, stays with the extras off. A mod of
//    its own that requires an extra (host/mod_deps.h) is normally an extra too. One the host runs cannot run here, which
//    counts as lacking it, so the joiner leaves and is told.
//  - A host never heard kGraceMs after this client first saw it runs no mods for this purpose: every mod here goes off. A
//    client on an older mod suite (v0.1.0 speaks presence version 2) is never heard, so it counts as running none, and it
//    counts this one so too.
//  - The host removes a modded player whose mods still differ kKickDelayMs after it learned of them (kGraceMs for one who
//    has only extras, which it can switch off), and a player never heard from kGraceMs after it first saw them. A player
//    heard to run no mods counts as not heard: kGraceMs from then. A joiner that has not heard this host yet switches every
//    mod off and says so, and it starts them again when the host's presence arrives.
//  - The game's kick can do nothing without a word, so a kick counts only when the player leaves the room. One still there
//    kKickConfirmMs later is kicked again, kMaxKicks in all, and then the host is told, and nobody is kicked again until
//    that player's mods match or they leave. A kick the game could not even try (no ID card yet) is asked for again, up to
//    kMaxUntriedKicks in a row, and then the same applies.
//  - A contract's start waits until every player is verified (start_answer; host/lobby_gate.h refuses it). Pressing start
//    changes no timing here: the wait ends as the player is verified, leaves or is removed. One the game would not remove keeps it
//    refused while they stay, and the host can still leave.
//  - A lobby listed publicly, or not readable as private, switches every mod off, for the host as well, and removes nobody,
//    because no mod runs. The game unlists every room for a contract, so public lasts until the room is left.
// Mods come back on leaving the room, or in the lobby when the host's presence arrives late (the matching rules then apply).
// A leave or kick the game could not carry out is asked for again kRetryMs later. Players are judged, and mods switched off,
// only while the lobby's screen is up, where Photon delivers messages and the game's lobby steps exist: a contract's load is
// silent, and nothing about the mods changes during a contract. A player once heard keeps their mods until they leave the
// room. A client that started no mods (its player declined the notice) neither checks nor enforces anything. Only the room's
// first actor kicks, since the game's kick is refused from anyone else (sdk/systems/photon.md). Pure logic, tested offline.
namespace pm::joinguard {

constexpr uint64_t kGraceMs = 10000;     // for a joiner's mod to agree to the notice and say hello
constexpr uint64_t kKickDelayMs = 4000;  // for a mismatched modded joiner to leave by itself
constexpr uint64_t kRetryMs = 3000;      // before a leave or kick the game could not carry out is tried again
constexpr uint64_t kKickConfirmMs = 5000;  // for a kicked player to be gone from the room
constexpr int kMaxKicks = 3;             // of one player that the game took and who stayed
constexpr int kMaxUntriedKicks = 10;     // of one player in a row that the game could not try: kRetryMs apart, 30 seconds
constexpr size_t kMaxListedMods = 24;    // named in a removal's message; a host's list can be far longer than a popup

inline std::string vanilla_host_notice() { return i18n::text("lobby.vanilla-host"); }
inline std::string public_notice() { return i18n::text("lobby.public"); }

struct Frame {
  int local = 0;  // 0 outside a room, or when this client's number cannot be read
  int master = 0;
  std::vector<int> actors;  // the room's players (empty in a public room, which nobody is judged in)
  bool lobby = false;       // the lobby's screen is up (not loading or playing a contract)
  uint64_t now = 0;
  bool publicRoom = false;   // this client's room is listed, or cannot be read as private
  bool roomUnknown = false;  // local is 0 and this client cannot be shown out of a room: nothing changes on this frame
  bool listRead = true;      // `actors` was read whole; false says nothing about who is there
};

struct Verdict {
  bool leave = false;     // this client must leave the lobby (report with Guard::left)
  std::vector<int> kick;  // the host: players to remove now (report each with Guard::kicked)
  std::vector<int> unremoved;  // the host: players still here after kMaxKicks, each once: log it and tell the player
  std::string show;       // why this client left a lobby, now that it is out of the room
  std::vector<std::string> suspend;  // the ids of this client's mods to have off in this room now: every other mod runs
  bool resume = false;               // every mod this client switched off for this room starts again
  std::string notice;                // tells the player what `suspend` switched off
};

// Whether a contract may start now, and if not who it is waiting for.
struct StartAnswer {
  bool go = true;
  bool listUnread = false;   // who is here could not be read
  std::vector<int> waiting;  // players the normal timing will settle: verified, gone or removed, within kGraceMs
  std::vector<int> stuck;    // players the game did not remove, or that this client cannot remove: it waits while they stay
  bool notFirstActor = false;  // this host cannot remove anyone (the game's kick needs the room's first actor)
};

using NameOf = std::function<std::string(const std::string& id)>;  // a mod's name, for one that announced none
// This client's installed mods that require one of `ids`, directly or through another (host/mod_deps.h dependents).
using DependentsOf = std::function<std::vector<std::string>(const std::vector<std::string>& ids)>;

/** Whether two clients run the same mods in the same builds, in whatever order. Names and versions are for players. */
inline bool same_mods(const std::vector<presence::Mod>& a, const std::vector<presence::Mod>& b) {
  const auto builds = [](const std::vector<presence::Mod>& mods) {
    std::vector<std::pair<std::string, std::string>> out;
    for (const presence::Mod& m : mods) out.emplace_back(m.id, m.build);
    std::sort(out.begin(), out.end());
    return out;
  };
  return builds(a) == builds(b);
}

/** The mod called `id` in `in`, or null. */
inline const presence::Mod* find_mod(const std::vector<presence::Mod>& in, const std::string& id) {
  for (const presence::Mod& m : in)
    if (m.id == id) return &m;
  return nullptr;
}

/** Whether `mine` runs every mod of `host`, each in the same build. */
inline bool covers(const std::vector<presence::Mod>& mine, const std::vector<presence::Mod>& host) {
  return std::all_of(host.begin(), host.end(), [&](const presence::Mod& h) {
    const presence::Mod* m = find_mod(mine, h.id);
    return m && m->build == h.build;
  });
}

/**
 * What a joiner is told once a host mod it lacks, or runs in another build, has removed it: every mod the lobby requires,
 * with its version, marking those two, which come first so that the list's limit never hides them. `mine` holds every mod
 * this client would run, the suspended ones too. `cantRun` holds those of `mine` that require a mod this lobby does not run,
 * which also rule this client out.
 */
inline std::string mismatch_message(const std::vector<presence::Mod>& host, const std::vector<presence::Mod>& mine,
                                    const NameOf& name, const std::vector<presence::Mod>& cantRun = {}) {
  std::vector<std::string> marked, fine;
  for (const presence::Mod& m : host) {
    // The name its client announced, which this client may not have; the id when it announced none.
    const std::string shownName = m.name.empty() ? name(m.id) : m.name;
    const std::string shown = m.version.empty() ? shownName : shownName + " " + m.version;
    const presence::Mod* match = find_mod(mine, m.id);
    if (find_mod(cantRun, m.id)) marked.push_back(i18n::text("lobby.required.needs-missing", {{"mod", shown}}));
    else if (!match) marked.push_back(i18n::text("lobby.required.missing", {{"mod", shown}}));
    else if (match->build != m.build)
      marked.push_back(match->version.empty() || match->version == m.version
                           ? i18n::text("lobby.required.other-build", {{"mod", shown}})
                           : i18n::text("lobby.required.other-version", {{"mod", shown}, {"version", match->version}}));
    else fine.push_back(shown);
  }
  // The mods that rule this client out are highlighted (the user, 2026-11-03), in the style the loader's table gives.
  for (std::string& line : marked) line = i18n::text("lobby.required.highlight", {{"line", line}});
  marked.insert(marked.end(), fine.begin(), fine.end());
  std::string text = i18n::text("lobby.required.title") + "\n";
  for (size_t i = 0; i < marked.size() && i < kMaxListedMods; ++i) text += "\n" + marked[i];
  if (marked.size() > kMaxListedMods)
    text += "\n" + i18n::text("lobby.required.more", {{"count", std::to_string(marked.size() - kMaxListedMods)}});
  return text + "\n\n" + i18n::text("lobby.required.advice");
}

/** Tells a joiner which of its mods the host does not run. */
inline std::string extras_notice(const std::vector<presence::Mod>& extras, const NameOf& name) {
  std::string names;
  for (const presence::Mod& m : extras) names += (names.empty() ? "" : i18n::text("list.separator")) + (m.name.empty() ? name(m.id) : m.name);
  return i18n::text("lobby.extras", {{"mods", names}});
}

/** A player's own choice of name, as the popup may show it: no control characters or rich text, and bounded. */
inline std::string shown_player(std::string_view name, int actor) {
  constexpr size_t kMaxShown = 32;
  std::string out;
  bool cut = false;
  for (const char c : name) {
    if (static_cast<unsigned char>(c) < 0x20 || c == 0x7F || c == '<' || c == '>') continue;
    if (out.size() >= kMaxShown) {
      cut = true;
      break;
    }
    out += c;
  }
  if (cut) {
    // The cut can leave part of a UTF-8 character at the end: that part goes, and every whole character stays.
    size_t lead = out.size();
    while (lead > 0 && (static_cast<unsigned char>(out[lead - 1]) & 0xC0) == 0x80) --lead;
    if (lead > 0) {
      const unsigned char first = static_cast<unsigned char>(out[lead - 1]);
      const size_t whole = first >= 0xF0 ? 4 : first >= 0xE0 ? 3 : first >= 0xC0 ? 2 : 1;
      if (out.size() - (lead - 1) < whole) out.resize(lead - 1);
    }
  }
  return out.empty() ? i18n::text("player.unnamed", {{"number", std::to_string(actor)}}) : out;
}

/** "A", "A and B" or "A, B and C". */
inline std::string joined_names(const std::vector<std::string>& names) {
  std::string out;
  for (size_t i = 0; i < names.size(); ++i)
    out += (i == 0 ? std::string() : i + 1 == names.size() ? i18n::text("list.last-separator") : i18n::text("list.separator")) + names[i];
  return out;
}

/** Tells the host a player stays whom the game's kick did not remove. */
inline std::string unremoved_notice(const std::vector<std::string>& names) {
  return i18n::text("lobby.unremoved", {{"players", joined_names(names)}});
}

/** Tells a host who cannot remove anyone, which the game leaves to the player in the room longest. */
inline std::string not_first_actor_notice(const std::vector<std::string>& names) {
  return i18n::text("lobby.not-first-actor", {{"players", joined_names(names)}});
}

/** Tells the host why the contract did not start, naming who it is waiting for. */
inline std::string start_wait_notice(const std::vector<std::string>& waiting, const std::vector<std::string>& stuck, bool listUnread,
                                     bool notFirstActor = false) {
  std::string out;
  if (listUnread) out = i18n::text("start.list-unread");
  if (!waiting.empty()) out = i18n::text("start.waiting", {{"players", joined_names(waiting)}});
  if (!stuck.empty()) out += (out.empty() ? "" : "\n\n") + (notFirstActor ? not_first_actor_notice(stuck) : unremoved_notice(stuck));
  return out;
}

class Guard {
 public:
  // `dependents` says which of this client's mods cannot run without a suspended one; without it, none is taken to.
  explicit Guard(NameOf name, DependentsOf dependents = {}) : name_(std::move(name)), dependents_(std::move(dependents)) {}

  Verdict judge(const Frame& f, const presence::Presence& p) {
    Verdict v;
    now_ = f.now;
    if (f.local == 0) {
      if (f.roomUnknown) return v;
      if (leaving_) v.show = std::move(message_);
      v.resume = !suspended_.empty();
      seen_.clear();
      suspended_.clear();
      publicRoom_ = false;
      leaving_ = false;
      message_.clear();
      leaveRetryAt_ = 0;
      return v;
    }
    std::erase_if(seen_, [&](const auto& entry) { return std::find(f.actors.begin(), f.actors.end(), entry.first) == f.actors.end(); });
    for (int a : f.actors) {
      if (a == f.local) continue;
      Seen& s = seen_.try_emplace(a, Seen{f.now}).first->second;
      if (const std::vector<presence::Mod>* mods = p.mods_of(a)) {
        s.heard = true;
        s.mods = *mods;
      }
    }
    // Every mod this client would run here: those running, and those it switched off for this room.
    std::vector<presence::Mod> whole = p.local();
    whole.insert(whole.end(), suspended_.begin(), suspended_.end());
    if (!f.lobby || whole.empty()) return v;
    publicRoom_ = publicRoom_ || f.publicRoom;
    if (publicRoom_) {
      switch_off(v, whole, public_notice());
      return v;
    }
    if (f.local != f.master) {
      judge_host(v, f, whole);
      return v;
    }
    const std::vector<presence::Mod>& mine = p.local();
    if (!mine.empty()) judge_players(v, f, mine);
    return v;
  }

  /**
   * Whether a contract's start may go ahead now, on a fresh reading of the room: on the host, once everyone there has announced
   * exactly its mods. It asks nothing of a client that is not the host, runs no mods, or is in a public lobby, where no
   * mod runs, so a start goes as in the game there. It changes nothing: pressing start kicks nobody.
   */
  StartAnswer start_answer(const Frame& f, const presence::Presence& p) const {
    StartAnswer a;
    const std::vector<presence::Mod>& mine = p.local();
    if (f.local == 0 || mine.empty() || publicRoom_ || f.publicRoom) return a;
    if (f.master != 0 && f.local != f.master) return a;
    if (f.master == 0 || !f.listRead) {  // in a room, so a master of 0 is one that could not be read: this may be the host
      a.go = false;
      a.listUnread = true;
      return a;
    }
    const bool mayKick = !f.actors.empty() && *std::min_element(f.actors.begin(), f.actors.end()) == f.local;
    a.notFirstActor = !mayKick;
    for (int actor : f.actors) {
      if (actor == f.local) continue;
      const auto seen = seen_.find(actor);
      const std::vector<presence::Mod>* mods = p.mods_of(actor);
      if (!mods && seen != seen_.end() && seen->second.heard) mods = &seen->second.mods;  // kept while they stay
      if (mods && same_mods(*mods, mine)) continue;
      a.go = false;
      if (!mayKick || (seen != seen_.end() && seen->second.gaveUp)) a.stuck.push_back(actor);
      else a.waiting.push_back(actor);
    }
    return a;
  }

  /** Whether the leave asked for started; if not, it is asked for again kRetryMs later. */
  void left(bool started) {
    if (started) return;
    leaving_ = false;
    leaveRetryAt_ = now_ + kRetryMs;
  }

  /**
   * Whether the game's kick ran; if not, it is asked for again kRetryMs later, and does not count towards kMaxKicks, up to
   * kMaxUntriedKicks in a row. That it ran proves nothing about the player: only their leaving the room does.
   */
  void kicked(int actor, bool done) {
    auto it = seen_.find(actor);
    if (it == seen_.end()) return;
    Seen& s = it->second;
    if (done) {
      s.untried = 0;
      return;
    }
    s.kicked = false;
    --s.kicks;
    s.retryAt = now_ + kRetryMs;
    if (++s.untried >= kMaxUntriedKicks) {
      s.gaveUp = true;
      s.tellGaveUp = true;
    }
  }

 private:
  struct Seen {
    uint64_t first = 0;  // when this client first saw them in the room
    bool heard = false;
    std::vector<presence::Mod> mods;  // as last announced
    uint64_t mismatchSince = 0;       // the host: when their mods were first seen to differ (0: they match)
    uint64_t emptySince = 0;          // the host: when they were first heard to run no mods (0: they match since)
    bool kicked = false;              // a kick is waiting to be proved by their leaving
    uint64_t kickedAt = 0;
    int kicks = 0;         // kicks the game took while they stayed
    int untried = 0;       // kicks in a row that the game could not try
    bool gaveUp = false;   // no more kicks of them
    bool tellGaveUp = false;  // the host has yet to hear that
    uint64_t retryAt = 0;  // after a kick that was not carried out
  };

  // Asks for exactly `off` to be off, once: nothing when they already are, and every mod back when none need be.
  void switch_off(Verdict& v, const std::vector<presence::Mod>& off, const std::string& notice) {
    if (same_mods(off, suspended_)) return;
    suspended_ = off;
    if (off.empty()) {
      v.resume = true;
      return;
    }
    for (const presence::Mod& m : off) v.suspend.push_back(m.id);
    v.notice = notice;
  }

  // This client is not the host: its mods against the host's.
  void judge_host(Verdict& v, const Frame& f, const std::vector<presence::Mod>& whole) {
    const auto host = seen_.find(f.master);
    if (host == seen_.end()) return;  // the master is unknown, or not in the list: nothing is proved about it
    if (!host->second.heard) {
      if (f.now - host->second.first >= kGraceMs) switch_off(v, whole, vanilla_host_notice());
      return;
    }
    const std::vector<presence::Mod>& hostMods = host->second.mods;
    std::vector<presence::Mod> extras;
    for (const presence::Mod& m : whole)
      if (!find_mod(hostMods, m.id)) extras.push_back(m);
    // A mod that requires an extra goes off with it. Normally the host lacks it too, so it is an extra and is named as one.
    // One the host does run cannot run here, which is as good as lacking it: this client cannot match the room.
    std::vector<presence::Mod> cantRun;
    for (const std::string& id : dependents_of(extras))
      if (const presence::Mod* m = find_mod(whole, id); m && !find_mod(extras, id)) cantRun.push_back(*m);
    if (!covers(whole, hostMods) || !cantRun.empty()) {
      if (!leaving_ && f.now >= leaveRetryAt_) {
        leaving_ = true;
        v.leave = true;
        message_ = mismatch_message(hostMods, whole, name_, cantRun);
      }
      return;
    }
    switch_off(v, extras, extras.empty() ? std::string() : extras_notice(extras, name_));
  }

  // What requires `extras`. It takes every installed manifest to answer and the guard runs every frame, so it is asked again
  // only when the extras change, which is whenever the host's mods change what they are. This assumes the manifests do not
  // change while the game runs: the host reads them once, at its start (host/mods.h).
  const std::vector<std::string>& dependents_of(const std::vector<presence::Mod>& extras) {
    std::vector<std::string> ids;
    for (const presence::Mod& m : extras) ids.push_back(m.id);
    if (ids != dependentsFor_) {
      dependentsFor_ = std::move(ids);
      dependentsOfExtras_ = dependents_ && !dependentsFor_.empty() ? dependents_(dependentsFor_) : std::vector<std::string>{};
    }
    return dependentsOfExtras_;
  }

  // This client is the host: everyone else's mods against its own. The game's kick is refused unless it comes from the
  // room's first actor, the lowest number in the list; a number that could not be read is 0, so nobody is kicked then.
  void judge_players(Verdict& v, const Frame& f, const std::vector<presence::Mod>& mine) {
    const bool mayKick = !f.actors.empty() && *std::min_element(f.actors.begin(), f.actors.end()) == f.local;
    for (auto& [actor, s] : seen_) {
      const bool matches = s.heard && same_mods(s.mods, mine);
      if (matches) {
        s.kicks = 0;
        s.untried = 0;
        s.gaveUp = false;
        s.tellGaveUp = false;
        s.emptySince = 0;
      }
      if (s.gaveUp) {
        if (std::exchange(s.tellGaveUp, false)) v.unremoved.push_back(actor);
        continue;
      }
      if (s.kicked) {
        if (f.now - s.kickedAt < kKickConfirmMs) continue;
        s.kicked = false;  // still in the list: the game skipped it, or the player's client did nothing
        if (s.kicks >= kMaxKicks) {
          s.gaveUp = true;
          v.unremoved.push_back(actor);
          continue;
        }
      }
      if (f.now < s.retryAt) continue;
      bool remove = false;
      if (!s.heard) {
        remove = f.now - s.first >= kGraceMs;
      } else if (matches) {
        s.mismatchSince = 0;
      } else if (s.mods.empty()) {
        if (s.emptySince == 0) s.emptySince = f.now ? f.now : 1;
        remove = f.now - s.emptySince >= kGraceMs;
      } else if (s.mismatchSince == 0) {
        s.mismatchSince = f.now ? f.now : 1;
      } else {
        remove = f.now - s.mismatchSince >= (covers(s.mods, mine) ? kGraceMs : kKickDelayMs);
      }
      if (remove && mayKick) {
        s.kicked = true;
        s.kickedAt = f.now;
        ++s.kicks;
        v.kick.push_back(actor);
      }
    }
  }

  NameOf name_;
  DependentsOf dependents_;
  std::vector<std::string> dependentsFor_, dependentsOfExtras_;  // the extras last asked about, and the answer
  std::map<int, Seen> seen_;
  uint64_t now_ = 0;
  bool leaving_ = false;      // this client asked to leave a mismatched lobby
  std::string message_;       // why, for when it is out of the room
  uint64_t leaveRetryAt_ = 0;
  std::vector<presence::Mod> suspended_;  // what this client switched off for this room
  bool publicRoom_ = false;               // this room has read as public in the lobby
};

}  // namespace pm::joinguard
