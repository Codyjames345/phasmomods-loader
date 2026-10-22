#pragma once

// The rules that set the Dullahan, the Draugr and the Black Shuck apart, applied on the host (which
// runs the ghost) on top of the game's own AI. Main thread.
//  - Dullahan: its body turns slowly; its speed is twice the game's base when facing its path and falls
//    to a quarter of it when it must turn.
//  - Draugr: never changes favourite room; in a hunt it runs at 2.8 m/s in its room and slows by 0.15 m/s
//    per metre away from it, to a crawl of 0.3 m/s; while anything taken from its room is outside it, it may hunt from 65% sanity.
//  - Black Shuck: blind and deaf in a hunt. It follows one player's scent trail (where they walked in the
//    last 30 s) step by step to them, the game's own chase and kill acting on them, and loses them where
//    salt or running water cuts the trail; outdoors in rain no trail is laid. A trail that ends at a
//    hiding place hands them to the game's own closet search. A smudge stops it as it stops any ghost.
//  - Knell: in a hunt it surges to 1.75 times the base speed and crawls at 0.35 times it, on a strict
//    two-second beat (knell_rule.h).
//  - Asterion: it stands still while it batters a door down (asterion.h).
//  - Bogeyman: it is slow in an open room, and goes for the youngest of the players it sees (bogeyman.h).
//  - Brownie: slower than a walk while a player looks at it, very fast while nobody does,
//    and no speed-up on sight of its prey (brownie.h).
//  - Tempest: very fast for the moment lightning lights it up, slow in the dark between, and no
//    speed-up on sight of its prey (tempest.h).
// Each speed rule writes the inputs of the game's own hunt speed (the hunt state's base and line-of-sight
// speed-up, speed_rule.h OwnSpeed), every frame and again right after the game's two-second base refresh, so
// the game builds the speed, applies its multipliers (the difficulty's ghost speed, Blood Moon) once, and
// footsteps, run sound and walk animation follow. No rule caps a speed.
namespace pg::custom::behaviour {

bool resolve();
bool create_hooks();  // before hooks::enable_all

// Every frame on the host. ghost is the custom ghost's GhostAI (null for none or a vanilla ghost);
// steered is true while a player controls it (the playable ghost), who then steers in hunts.
void tick(void* ghost, int type, bool steered);
void teardown();
// The ghost's own state for its player (core/services.h sense): the Dullahan's speed in percent of the
// game's while hunting (0 otherwise), the Draugr's anger (1 or 0); -1 for any other type.
int sense(int type);
// The freshest points of the scent trails (the Black Shuck's), newest first, as x, y, z triples.
int scent(float* xyz, int maxPoints);
// The hunt's target policy on the host. proposed is the player the game is asking about or about to set
// as its target; returns the player to keep, null for none. The hunt's sight test, which every target it
// sets or keeps passes, is detoured through it (its voice and electronics tests answer false for the
// Black Shuck). Every custom ghost's target rule, the Bogeyman's among them, belongs here, so each game
// function keeps exactly one detour. The Black Shuck keeps only the player it follows by scent, and only
// while the game may go for them (alive, not in the truck, not already killed); in a Search, once their
// trail has led it to their hiding place, it keeps nobody, so the game's own closet search finds them
// unseen (shuck_rule.h shuck_target). The Bogeyman's AI sees as the game does, and of the players the game's
// sight test passes it keeps only the youngest when it detects more than one (bogeyman.h choose_target).
// Any other type keeps what the game proposed.
void* choose_target(int type, void* ghost, void* proposed);
// Whether the ghost's hunt may go for this player, by the game's own test: alive, not in the truck, not
// already killed in this hunt. False while it is not hunting.
bool may_go_for(void* ghost, void* player);

}  // namespace pg::custom::behaviour
