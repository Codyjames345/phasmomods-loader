#pragma once

#include "gate_rule.h"

// What the told ghost may not do right now, whoever drives it, through the game's own entry points:
// GhostActivity.TryInteract (the AI's interactions) and GhostAI.ChangeState (interactions, events,
// abilities and hunts, the AI's and a Playable Ghost player's alike; never a forced change or a cursed
// possession's pull, gate_rule.h). Each ghost's file adds the rule that holds it back. Runs on the host.
// Main thread.
namespace pg::tells::gate {

using Kind = GateKind;
constexpr Kind kInteract = kGateInteract, kEvent = kGateEvent, kHunt = kGateHunt;

using Rule = bool (*)(Kind kind);
/** A rule asked before every attempt; true holds the attempt back. Added once, at start. */
void add_rule(Rule rule);

/** Told before (`before`) and after every state change of the told ghost on the host. */
using StateListener = void (*)(void* ai, int state, bool before);
void around_state(StateListener listener);

bool create_hooks();  // before hooks::enable_all
// The told ghost haunting the contract (on the host while the tells are on; null otherwise).
void tick(void* ghost);
bool held_back(Kind kind);
/** GhostAI.ChangeState(state, null, false), through every mod's detours. */
void change_state(void* ai, int state);
/** GhostActivity.TryInteract on the ghost's activity, through every mod's detours. */
void try_interact(void* ai);

}  // namespace pg::tells::gate
