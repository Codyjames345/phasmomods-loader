#pragma once

// The Brownie (brownie_rule.h) in the game. The host works out every frame whether a living
// player sees it (their facing, range and line of sight) and whether a running video camera does;
// behaviour.cpp sets its hunt speed by the first, restraint.cpp holds back its interactions and events by
// either. Every client's footstep sound is cut short while it hunts (a hook on the game's footstep play). Main thread.
namespace pg::custom::brownie {

bool resolve();
bool create_hooks();  // the footstep play hook; before hooks::enable_all
// Every frame on the host: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick_host(void* ghost, int type);
// Every frame on every client: records which ghost haunts, for the footstep hook.
void tick_all(void* ghost, int type);
bool available();  // its bindings resolved; without them it hunts as any ghost
bool seen();     // a living investigator looks at it now
bool watched();  // a player or a running video camera has it in view now
void reset();

}  // namespace pg::custom::brownie
