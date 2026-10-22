#pragma once

// Which of the told ghost's state changes a tell may hold back, kept free of game calls so it can be
// tested offline. Only the ghost's own attempts are held back: never a forced change (the tarot cards
// pass `forced`: Death's hunt, the Devil's event, the others), never a cursed possession's pull (the
// summoning circle, the music box), and never anything inside a running hunt.
namespace pg::tells {

enum GateKind { kGateInteract = 0, kGateEvent = 1, kGateHunt = 2 };

/** What a change to GhostAI.State `state` is, or -1 for one no tell holds back. */
inline int gate_kind(int state) {
  switch (state) {
    case 2: return kGateHunt;                     // hunting
    case 8: case 14: case 15: return kGateEvent;  // appear, randomEvent, GhostAbility
    // light, door, throwing, fuse box, door knock, window knock, car alarm, flicker, CCTV, mannequin,
    // teleport object, interact, ignite
    case 4: case 5: case 6: case 7: case 9: case 10: case 11: case 12: case 13: case 16: case 17: case 18: case 23:
      return kGateInteract;
    default: return -1;  // idle, wander, favourite room, summoning circle, music box, D.O.T.S., salt
  }
}

/** A cursed possession's pull (the summoning circle, the music box): what the ghost shows then is left alone. */
inline bool cursed_pull(int state) { return state == 19 || state == 20; }

/** Whether a tell may hold back GhostAI.ChangeState(state, _, forced) while the ghost does or does not hunt. */
inline bool gate_may_hold(int state, bool forced, bool hunting) { return gate_kind(state) >= 0 && !forced && !hunting; }

}  // namespace pg::tells
