#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "presence.h"

// Who may stay in a modded private lobby. A lobby is modded when its host runs mods; everyone in it
// must then run the same mods, in the same builds, as the host, and players without the mod may stay
// only when every mod the host runs allows them (PG_MOD_VANILLA_PEERS).
//  - A joiner whose mods differ from the host's leaves by itself, and once out of the room tells its
//    player why (a note shown in the lobby would go with the lobby).
//  - The host removes a modded player whose mods still differ kKickDelayMs after it learned of them
//    (their own leave did not start), and a player never heard from kGraceMs after it first saw
//    them when a running mod needs everyone to have it. A client on an older mod suite speaks an
//    older presence format and is never heard, so it counts as having no mod.
// A leave or kick the game could not carry out is asked for again kRetryMs later. Players are
// judged only while the lobby's screen is up, where Photon delivers messages and the game's lobby
// steps exist: a contract's load is silent. A player once heard keeps their mods until they leave
// the room. A client that started no mods (its player declined the notice) neither checks nor
// enforces anything. Pure logic, tested offline.
namespace pg::joinguard {

constexpr uint64_t kGraceMs = 10000;     // for a joiner's mod to agree to the notice and say hello
constexpr uint64_t kKickDelayMs = 4000;  // for a mismatched modded joiner to leave by itself
constexpr uint64_t kRetryMs = 3000;      // before a leave or kick the game could not carry out is tried again

struct Frame {
  int local = 0;  // 0 outside a (private) room
  int master = 0;
  std::vector<int> actors;
  bool lobby = false;  // the lobby's screen is up (not loading or playing a contract)
  uint64_t now = 0;
};

struct Verdict {
  bool leave = false;     // this client must leave the lobby (report with Guard::left)
  std::vector<int> kick;  // the host: players to remove now (report each with Guard::kicked)
  std::string show;       // why this client left a lobby, now that it is out of the room
};

using NameOf = std::function<std::string(const std::string& id)>;  // a mod's name for its player

/** Whether two clients run the same mods in the same builds, in whatever order. */
inline bool same_mods(std::vector<presence::Mod> a, std::vector<presence::Mod> b) {
  auto byId = [](const presence::Mod& x, const presence::Mod& y) { return x.id < y.id || (x.id == y.id && x.build < y.build); };
  std::sort(a.begin(), a.end(), byId);
  std::sort(b.begin(), b.end(), byId);
  return a == b;
}

/** What a joiner is told when the host's mods differ from its own. */
inline std::string mismatch_message(const std::vector<presence::Mod>& host, const std::vector<presence::Mod>& mine,
                                    const NameOf& name) {
  std::string onlyHost, onlyMine, other;
  auto add = [&](std::string& list, const std::string& id) { list += (list.empty() ? "" : ", ") + name(id); };
  auto find = [](const std::vector<presence::Mod>& in, const std::string& id) -> const presence::Mod* {
    for (const presence::Mod& m : in)
      if (m.id == id) return &m;
    return nullptr;
  };
  for (const presence::Mod& m : host) {
    const presence::Mod* match = find(mine, m.id);
    if (!match) add(onlyHost, m.id);
    else if (match->build != m.build) add(other, m.id);
  }
  for (const presence::Mod& m : mine)
    if (!find(host, m.id)) add(onlyMine, m.id);
  std::string text = "This lobby's host runs different mods, so you have left it.\n";
  if (!onlyHost.empty()) text += "\nOnly the host has: " + onlyHost + ".";
  if (!onlyMine.empty()) text += "\nOnly you have: " + onlyMine + ".";
  if (!other.empty()) text += "\nDifferent versions: " + other + ".";
  return text + "\n\nSwitch on the same mods from the Mods button, or join another lobby.";
}

class Guard {
 public:
  explicit Guard(NameOf name) : name_(std::move(name)) {}

  Verdict judge(const Frame& f, const presence::Presence& p) {
    Verdict v;
    now_ = f.now;
    if (f.local == 0) {
      if (leaving_) v.show = std::move(message_);
      seen_.clear();
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
    const std::vector<presence::Mod>& mine = p.local();
    if (!f.lobby || mine.empty()) return v;
    if (f.local != f.master) {
      auto host = seen_.find(f.master);
      if (!leaving_ && f.now >= leaveRetryAt_ && host != seen_.end() && host->second.heard &&
          !same_mods(host->second.mods, mine)) {
        leaving_ = true;
        v.leave = true;
        message_ = mismatch_message(host->second.mods, mine, name_);
      }
      return v;
    }
    for (auto& [actor, s] : seen_) {
      if (s.kicked || f.now < s.retryAt) continue;
      bool remove = false;
      if (s.heard) {
        if (same_mods(s.mods, mine)) s.mismatchSince = 0;
        else if (s.mismatchSince == 0) s.mismatchSince = f.now ? f.now : 1;
        else remove = f.now - s.mismatchSince >= kKickDelayMs;
      } else {
        remove = !p.local_vanilla_peers() && f.now - s.first >= kGraceMs;
      }
      if (remove) {
        s.kicked = true;
        v.kick.push_back(actor);
      }
    }
    return v;
  }

  /** Whether the leave asked for started; if not, it is asked for again kRetryMs later. */
  void left(bool started) {
    if (started) return;
    leaving_ = false;
    leaveRetryAt_ = now_ + kRetryMs;
  }

  /** Whether the kick asked for was carried out; if not, it is asked for again kRetryMs later. */
  void kicked(int actor, bool done) {
    auto it = seen_.find(actor);
    if (done || it == seen_.end()) return;
    it->second.kicked = false;
    it->second.retryAt = now_ + kRetryMs;
  }

 private:
  struct Seen {
    uint64_t first = 0;  // when this client first saw them in the room
    bool heard = false;
    std::vector<presence::Mod> mods;  // as last announced
    uint64_t mismatchSince = 0;       // the host: when their mods were first seen to differ (0: they match)
    bool kicked = false;
    uint64_t retryAt = 0;  // after a kick that was not carried out
  };
  NameOf name_;
  std::map<int, Seen> seen_;
  uint64_t now_ = 0;
  bool leaving_ = false;      // this client asked to leave a mismatched lobby
  std::string message_;       // why, for when it is out of the room
  uint64_t leaveRetryAt_ = 0;
};

}  // namespace pg::joinguard
