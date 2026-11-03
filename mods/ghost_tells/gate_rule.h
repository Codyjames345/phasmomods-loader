#pragma once

#include <algorithm>
#include <array>

// Which of the told ghost's state changes a tell may hold back, kept free of game calls so it can be
// tested offline. Only the ghost's own attempts are held back: never a forced change (the tarot cards
// pass `forced`: Death's hunt, the Devil's event, the others), never a cursed possession's pull (the
// summoning circle, the music box), and never anything inside a running hunt. The state numbers are
// the game's, read from its GhostState enum when the mod starts (gate.cpp).
namespace pm::tells {

enum GateKind { kGateInteract = 0, kGateEvent = 1, kGateHunt = 2 };

/**
 * What a kind of PmGhostDef.heldBack (core/mod_api.h) asks of the gate, or -1 for none it answers: its own three, and
 * the fuse box (kind 4), which the gate holds as an interaction (gate_kind), for its AI and so for a player.
 */
inline int gate_held_kind(int heldBackKind) {
  if (heldBackKind == 4) return kGateInteract;
  return heldBackKind == kGateInteract || heldBackKind == kGateEvent || heldBackKind == kGateHunt ? heldBackKind : -1;
}

/** The GhostState numbers the gate sorts, as the running game numbers them. */
struct GateStates {
  int hunting = -1;
  std::array<int, 3> events{-1, -1, -1};  // appear, randomEvent, GhostAbility
  // light, door, throwing, fusebox, doorKnock, windowKnock, carAlarm, flicker, cctv, mannequin,
  // teleportObject, interact, ignite
  std::array<int, 13> interactions{-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
  std::array<int, 2> pulls{-1, -1};  // summoningCircle, musicBox
};

/** What a change to GhostAI.State `state` is, or -1 for one no tell holds back (idle, wander, favourite
 *  room, the cursed pulls, D.O.T.S., salt, and anything the enum adds). */
inline int gate_kind(const GateStates& s, int state) {
  if (state < 0) return -1;
  if (state == s.hunting) return kGateHunt;
  if (std::find(s.events.begin(), s.events.end(), state) != s.events.end()) return kGateEvent;
  if (std::find(s.interactions.begin(), s.interactions.end(), state) != s.interactions.end()) return kGateInteract;
  return -1;
}

/** A cursed possession's pull (the summoning circle, the music box): what the ghost shows then is left alone. */
inline bool cursed_pull(const GateStates& s, int state) {
  return state >= 0 && std::find(s.pulls.begin(), s.pulls.end(), state) != s.pulls.end();
}

/** Whether a tell may hold back GhostAI.ChangeState(state, _, forced) while the ghost does or does not hunt. */
inline bool gate_may_hold(const GateStates& s, int state, bool forced, bool hunting) {
  return gate_kind(s, state) >= 0 && !forced && !hunting;
}

}  // namespace pm::tells
