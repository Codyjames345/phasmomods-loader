#pragma once

// The game side of a ghost's one target (core/mod_api.h, PmGhostDef.target): a ghost whose definition
// names a target hunts as the game's Banshee does, whoever steers it, its AI or a player controlling it
// (Playable Ghost). It runs the game's own Banshee code on the master, which runs the hunt
// (sdk/internals/ghost-ai.md, "The Banshee's one target"):
// - When a hunt begins (GhostHuntState.StartPick) and the definition of the type the contract's ghost
//   acts as answers a player, that player becomes the ghost's Banshee target (GhostAI.bansheeTarget).
//   The game's own start pick then keeps them as the hunt's player, and its own test of that field
//   (TargetAllows) holds Listen to them while they are alive, in the house and not killed in this hunt,
//   and lets it go for anyone once not. The target is fixed for the hunt: a definition that answers
//   another player later binds from the next hunt or re-target, and one that answers nobody at the start
//   leaves that hunt unrestricted.
// - For that hunt the two steps the game skips for the Banshee's type are skipped too: KeepNearest does
//   nothing, so the kept player stays, and SightPick finds nobody, so sight goes only for the kept player.
// - Where the game gives the Banshee's type a new target (GhostAI.SetNewBansheeTarget, which the kill and
//   the Hanged Man card call with the player about to die), the definition answers without that player,
//   so its own rule decides, and its answer becomes the Banshee target; nobody, and the target stays, as
//   the game's does. The kill itself is the game's.
// - When the target leaves the game mid-hunt, the definition names the next one at the next tick, as the
//   game re-targets a Banshee whose target is gone.
// - The hunt's end (GhostHuntState.Exit) clears the field again, and so does the ghost ceasing to act as
//   the type whose definition named the target (a Mimic's copy, of a game type or another custom one; the
//   hunt is then the game's own until the next). Who steers the ghost changes nothing: its AI and a player
//   controlling it hold the same target, so a hand-over mid-hunt keeps it.
// - The field is written on the master only, never through the game's RPC: every reader runs on the
//   master, and the RPC cannot clear it, is buffered and would go stale (sdk/systems/ghost-ai.md).
// No type swap is made: the hunt's ghostType stays the ghost's own, so the field's other readers, which
// act only for the Banshee's type, stay out. Nothing changes for a ghost whose definition names no
// target, a game type included. It reaches the game only through map entries (the game API, bound to the
// host's own contract). Main thread.

#include <cstdint>
#include <string>

#include "content_acting.h"
#include "held.h"

namespace pm::host {

class ContentTarget {
 public:
  explicit ContentTarget(const content::Acting& acting) : acting_(acting) {}

  /// The map entries this needs that did not resolve, ", "-separated; empty when every one did.
  std::string missing() const;
  /// Creates its hooks on the game's methods (not enabled), for this instance; one instance hooks at a
  /// time. False when one could not be made (the game API logs which).
  bool create_hooks();
  /// Every frame: a held target who has left the game is replaced, and a hold whose ghost no longer acts
  /// as the type it was made for ends.
  void tick();
  /// Clears a target this set, as before the mods stop.
  void withdraw();
  /// Whether the running hunt of `ghost` (a GhostAI) holds a target this set.
  bool holds(const void* ghost) const;

  // What each hook does; each may be called on its own.

  /// GhostHuntState.StartPick, before: the definition's target becomes the ghost's Banshee target.
  void before_start_pick(void* hunt);
  /// GhostHuntState.KeepNearest: whether the game's step runs (false for a held hunt).
  bool keeps_nearest(void* hunt) const;
  /// GhostHuntState.SightPick: whether the game's look runs (false for a held hunt: it finds nobody).
  bool sight_picks(void* hunt) const;
  /// GhostAI.SetNewBansheeTarget: true when it gave the held ghost its next target (the game's body is
  /// then skipped), false for any other ghost, whose game body runs.
  bool new_target(void* ghost, void* dying);
  /// GhostHuntState.Exit, after: the hold ends with the hunt.
  void after_exit(void* hunt);

 private:
  bool held(void* hunt) const;
  void release();

  const content::Acting& acting_;
  pm::api::Held set_;  // the ghost whose running hunt has a Banshee target this set, null for none
  int32_t heldFor_ = 0;  // the handle of the definition that named that target (content::Ghost::handle)
};

}  // namespace pm::host
