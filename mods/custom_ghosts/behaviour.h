#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

// The rules that set the Dullahan, the Draugr and the Black Shuck apart, applied on the host (which
// runs the ghost) on top of the game's own AI. Main thread.
//  - Dullahan: its body turns slowly; its speed is twice the game's base when facing its path and falls
//    to a quarter of it when it must turn.
//  - Draugr: keeps its room the Goryo way (its wander never takes the long roam, goryo_way.h); in a hunt
//    it runs at 2.8 m/s in its room and slows by 0.15 m/s per metre away from it, to a crawl of 0.3 m/s.
//    Its things are the house's own props that lay in its
//    room when its rules started (draugr_rule.h). While any of them is outside the room it is angered: it
//    may hunt from 65% sanity, outside hunts it takes them back one at a time to where they lay (at the
//    Kikimora's cadence, never one a player carries, carry.h), its hunts go only for the most recent player
//    who carried one out (its target, below; host/content_target.h), and its room is 3 degrees colder on
//    every client (tick_room). When a
//    Monkey Paw's wish moves its room, its things are surveyed anew there and it is calm (draugr_rule.h Hoard).
//  - Black Shuck: blind and deaf in a hunt, whoever steers it. It follows one player's scent trail (where they
//    walked in the last 30 s) step by step to them, the game's own chase and kill acting on them, and loses them
//    where salt or running water cuts the trail, ignoring their trail for 30 s; outdoors in rain no trail is
//    laid. A trail that ends at a hiding place hands them to the game's own closet search. A smudge stops it as
//    it stops any ghost. A player walks it themselves within the trails' corridor (player_move), and their prey
//    is whoever's trail they walk (trail.h walked).
//  - Knell: in a hunt it surges to 1.75 times the base speed and crawls at 0.35 times it, on a strict
//    two-second beat (knell_rule.h).
//  - Asterion: it stands still while it batters a door down (asterion.h).
//  - Bogeyman: it is slow in an open room, and its hunts go only for the youngest player (bogeyman.h).
//  - Brownie: slower than a walk while a player looks at it, very fast while nobody does,
//    and no speed-up on sight of its prey (brownie.h).
//  - Tempest: very fast for the moment lightning lights it up, slow in the dark between, and no
//    speed-up on sight of its prey (tempest.h).
// Each speed rule gives the inputs of the game's own hunt speed (its base and line-of-sight speed-up,
// speed_rule.h OwnSpeed) through hunt_speed, the ghost's content rule (PmGhostRules.huntSpeed), which the
// host writes into the hunt state before every speed update, so the game builds the speed, applies its
// multipliers (the difficulty's ghost speed, Blood Moon) once, and footsteps, run sound and walk animation
// follow. No rule caps a speed.
namespace pm::custom::behaviour {

bool resolve();
bool create_hooks();  // before hooks::enable_all

// Every frame on the host. ghost is the custom ghost's GhostAI (null for none, a vanilla ghost or another
// client), type the custom ghost it acts as (ghosts.h); steered is true while a player controls it (the
// playable ghost), who then walks it in hunts: its rules otherwise run as for its AI.
void tick(void* ghost, int type, bool steered);
// The hunt speed the rule of the last tick sets (PmGhostRules.huntSpeed): `base` becomes its base and
// `losScale` 1 when it holds the line-of-sight speed-up off; false (both untouched) when no rule applies,
// for the game's own.
bool hunt_speed(float* base, float* losScale);
// Every frame on every client, after tick: haunting is the contract's ghost (null for none), type the
// custom ghost it acts as. While a Draugr is angered (the host's own anger, or the last the host said, which
// receive takes) its favourite room's temperature runs kCold below the game's own (draugr_rule.h RoomCold,
// through the room's own LevelRoom.Update); otherwise, and once it is no longer angered, the room shows the
// game's own temperature.
void tick_room(void* haunting, int type);
// The ghost's one target (PmGhostDef.target), to which the host holds its hunts, its AI's and a player's
// alike: for the Draugr while it is angered, the most recent living thief of one of its things still out
// (draugr_rule.h target), never one of `excluded`; null otherwise, and off the host, which alone keeps its things.
void* target(int type, std::span<const void* const> excluded);
// A message on the mod's channel: the host's word on the Draugr's anger (flame_net.h kAnger), taken only from
// the master client.
void receive(int sender, const uint8_t* data, size_t size);
void teardown();
// The ghost's own state for its player (PmGhostDef.hudState): the Dullahan's speed in percent of the
// game's while hunting (0 otherwise), on the host; the Draugr's anger (1 or 0), on the host from its own
// things and on another client from the host's word (receive, draugr_rule.h hud_anger); -1 for any other
// type.
int sense(int type);
// The freshest points of the scent trails (the Black Shuck's), newest first, as x, y, z triples, while it
// hunts (none otherwise).
int scent(float* xyz, int maxPoints);
// Host, every frame a player steers the Black Shuck: in a hunt, confines the direction (dx, dz) they steer
// in from `at` (x, y, z) to the corridor around the same scent trails its AI follows, those of the living
// players the hunt may go for, but for one it lost in the last kTrailMs (trail.h confine); with none of their
// scent reaching it, as its AI then roams by the game's hunt, outside a hunt, or for another ghost, it leaves it
// as it is.
void player_move(const float* at, float* dx, float* dz);
// The hunt's target policy on the host. proposed is the player the game is asking about or about to set
// as its target; returns the player to keep, null for none. The hunt's sight test, which every target it
// sets or keeps passes, is detoured through it (its voice and electronics tests answer false for the
// Black Shuck). Every custom ghost's target rule, the Bogeyman's among them, belongs here, so each game
// function keeps exactly one detour. The Black Shuck keeps only the player it follows by scent, and only
// while the game may go for them (alive, not in the truck, not already killed); in a Search, once their
// trail has led it to their hiding place, it keeps nobody, so the game's own closet search finds them
// unseen (shuck_rule.h shuck_target). The Bogeyman, its AI or a player's, sees as the game does, but never
// beyond its short sight in an open room (bogeyman.h choose_target). Any other type keeps what the game
// proposed.
void* choose_target(int type, void* ghost, void* proposed);
// Whether the ghost's hunt may go for this player, as the game's own test decides (hunt.h may_go_for):
// alive, not in the truck, not already killed in this hunt. False while it is not hunting.
bool may_go_for(void* ghost, void* player);
// Whether the game's own sight test (GhostHuntState.Sees, which this mod's detour passes straight on for
// this call, so no custom policy and no re-entry) passes for this player in the ghost's hunt; false for a
// player the hunt may not go for. Like the game's, a pass records the player in the hunt's seen list.
bool hunt_sees(void* ghost, void* player);
// Whether the ghost's hunt is past its start delay (GhostHuntState.startDelay below 0): until then the
// game's own hunt runs no chase, speed or kill, so no custom rule steers it either. False while it is not
// hunting.
bool hunt_begun(void* ghost);
// The ghost's hunt stops chasing, searching for and wandering around this player, as the game's own kill and
// closet search clear them (its target, lastPlayer, searchTarget and, with SetCloset(null), the closet it
// was searching). Nothing while it is not hunting.
void forget(void* ghost, void* player);

}  // namespace pm::custom::behaviour
