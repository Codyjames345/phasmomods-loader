#pragma once

// What a custom ghost may not do right now, whoever drives it. Runs on the host. Main thread.
//  - Echo: while dormant (nobody heard near it for a minute) 3 in 4 of its interactions and events are lost,
//    and it does not use its ability, whoever steers it. Through the same hook it repeats a player's use of a
//    switch or door in its room once its interaction sensor detects it (echo.h repeat); a repeat is never lost to
//    dormancy, every other interaction is as before. A ghost player's aimed interaction makes the same repeat
//    (GhostActivity.NormalInteraction), and pays for what it loses (interaction_lost).
//  - Kikimora: never touches anything in a room with the lights on.
//  - Lithobolia: its throws go at an investigator in sight (lithobolia.h), through the same hook.
//  - Gremlin: some of its interactions switch off running electronics instead (gremlin.h).
//  - Brownie: nothing while an investigator or a running video camera has it in view (brownie.h).
//  - Tempest: lightning strikes as each of its events begins (tempest.h), through the same hook.
//  - Bogeyman: its hunts begin from a closet (bogeyman.h), through the same hook, and outside hunts most of
//    its idle door interactions are on a closet door its door sensor detects, when there is one
//    (GhostActivity.GetDoorToOpen); its AI's and a ghost player's alike.
//  - Squatter: attempts nothing at all, forced or not: no interaction from its roll, no pulse of its interaction
//    sensor (a voodoo pin's forced one included), and no state change but idling, wandering and its favourite room
//    (squatter_rule.h keeps_quiet), so no event, ability, interaction, cursed pull or hunt begins, whatever starts it;
//    while the mod steers its agent (squatter.h steering) only idling. Its rules hold back every kind a player may ask.
// Its attempts are held back through GhostActivity.TryInteract and GhostAI.ChangeState, where a roll a player
// takes (sdk/api/content.h roll_taken) goes straight on, as it attempts nothing yet; a player controlling the
// ghost asks held_back() before acting (the ghost's PmGhostDef.heldBack, which Playable Ghost asks through the
// content layer, sdk/api/content.h held_back). No hook here takes who steers it as an input.
namespace pm::custom::restraint {

// The kinds of PmGhostDef.heldBack: kHunt and kFuseBox only the Squatter's rules hold back.
enum Kind { kInteract = 0, kEvent = 1, kHunt = 2, kAbility = 3, kFuseBox = 4 };

bool create_hooks();  // before hooks::enable_all
// The custom ghost haunting the contract (on the host; null otherwise) and its type, every frame.
void tick(void* ghost, int type);
// The rule holds now (no roll): the action would not happen.
bool held_back(int type, Kind kind);
// An interaction a player has paid for and chosen (PmGhostDef.interactionLost): whether a dormant Echo loses
// this one, 3 in 4 by a roll of its own, as its AI's roll does; never its repeat. False for every other ghost.
bool interaction_lost(int type);

}  // namespace pm::custom::restraint
