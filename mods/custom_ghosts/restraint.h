#pragma once

// What a custom ghost may not do right now, whoever drives it. Runs on the host. Main thread.
//  - Echo: while dormant (nobody heard near it for a minute) its interactions and events mostly fail;
//    the AI loses 3 in 4 of them, a player cannot act at all until a voice wakes it. Through the same hook
//    its AI repeats a player's use of a switch or door in its room once its interaction sensor detects it
//    (echo.h repeat); a repeat is never lost to dormancy, every other interaction is as before.
//  - Kikimora: never touches anything in a room with the lights on.
//  - Lithobolia: its throws go at an investigator in sight (lithobolia.h), through the same hook.
//  - Gremlin: some of its interactions switch off running electronics instead (gremlin.h).
//  - Brownie: nothing while an investigator or a running video camera has it in view (brownie.h).
//  - Tempest: lightning strikes as each of its events begins (tempest.h), through the same hook.
//  - Bogeyman: its AI's hunts begin from a closet (bogeyman.h), through the same hook, and outside hunts
//    most of its AI's idle door interactions are on a closet door its door sensor detects, when there is
//    one (GhostActivity.GetDoorToOpen).
// The game's own AI is held back through GhostActivity.TryInteract and GhostAI.ChangeState; a player
// controlling the ghost asks held_back() through the service (core/services.h) before acting.
namespace pg::custom::restraint {

enum Kind { kInteract = 0, kEvent = 1 };

bool create_hooks();  // before hooks::enable_all
// The custom ghost haunting the contract (on the host; null otherwise) and its type, every frame.
// `steered`: a player controls it, so the Lithobolia's hunt throws stay the game's own, the Bogeyman's
// hunts begin where it stands and its doors are the ones the player picks, and the Echo repeats nothing.
void tick(void* ghost, int type, bool steered);
// The rule holds now (no roll): the action would not happen.
bool held_back(int type, Kind kind);

}  // namespace pg::custom::restraint
