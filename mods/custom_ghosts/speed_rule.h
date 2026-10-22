#pragma once

#include <cstdint>
#include <map>

// Bookkeeping for the custom ghosts' speed rules and the Echo, kept free of game calls so it can be tested
// offline.
namespace pg::custom {

// A custom ghost's speed rule sets the inputs of the game's own hunt speed, never the speed it produces. The
// hunt update (RVA 0x2153C20) builds (base x sight + bonus) x the multipliers every type shares (the
// difficulty's ghost speed, Blood Moon, three of GhostAI's), so a rule that writes the base (hunt state +0xB0)
// and, to drop the line-of-sight speed-up, holds sight (+0xB4) at 1 gets the multipliers once and leaves the
// footsteps, the run sound and the walk animation reading the real speed.
struct OwnSpeed {
  float base;      // the hunt base to write, metres per second before the multipliers
  bool keepSight;  // whether the game's line-of-sight speed-up still multiplies it (false: it is held at 1)
};

/** A speed of the rule's own: `speed` m/s, no line-of-sight speed-up. */
inline OwnSpeed flat_speed(float speed) { return {speed, false}; }

/** The game's base `gameBase` scaled by `factor`, still sped up by line of sight as a vanilla ghost's is. */
inline OwnSpeed scaled_speed(float gameBase, float factor) { return {gameBase * factor, true}; }

/**
 * `rule` while it `applies`, else the game's own default for base `gameBase`. A rule that stops mid-hunt must
 * write this: the game rewrites the base only every two seconds, so a stale one would linger.
 */
inline OwnSpeed speed_when(bool applies, OwnSpeed rule, float gameBase) {
  return applies ? rule : scaled_speed(gameBase, 1.0f);
}

// Which client can send an investigator's last phrase for the Echo: each client reports, every few
// seconds, whose phrases it holds; the host picks a client whose report is fresh, itself first.
class Holders {
 public:
  void report(int client, int actor, uint64_t now) { reports_[actor][client] = now; }
  void clear() { reports_.clear(); }

  /** A client that freshly reported holding `actor`'s phrase (never the actor), `self` first; 0 for none. */
  template <class Usable>
  int holder_of(int actor, int self, uint64_t now, uint64_t freshMs, Usable usable) const {
    auto it = reports_.find(actor);
    if (it == reports_.end()) return 0;
    int found = 0;
    for (const auto& [client, at] : it->second) {
      if (now - at > freshMs || client == actor || !usable(client)) continue;
      if (client == self) return client;
      if (!found) found = client;
    }
    return found;
  }

 private:
  std::map<int, std::map<int, uint64_t>> reports_;  // actor -> client -> when it reported
};

}  // namespace pg::custom
