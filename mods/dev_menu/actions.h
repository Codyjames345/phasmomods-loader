#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Transform.h"
#include "menu_rule.h"

// The dev menu's game side: its three hooks and the rows' actions. Every action is the game's own method,
// on the client the game would call it from (the mod never sends an RPC by name or runs an RPC body); the
// rules in menu_rule.h decide before a click, and the game's own guards still decide after. Main thread.
namespace pm::devmenu::actions {

using phasmo::Vector3;

/** Creates the progression-suppression, god-mode and noclip hooks. False when any cannot be made: the mod
 *  stays off, since a dev session without progression suppression is refused (fail closed). */
bool start();
/** Puts back everything the menu left in the game (the ghost's sight, the rig's physical step) and clears the
 *  switches. */
void stop();
/** Every frame, menu open or not: the sanity lock, and noclip's switch going off with a rig a scene took. */
void tick();

/** Marks the dev session on, once, when the menu first opens: from then the contract can earn no progress. */
void note_session();
bool session_on();

Switches& switches();

/** This client's offline state: PhotonNetwork.get_OfflineMode; false when it cannot be read (fail closed). */
bool offline();
/** Whether the menu may open now: the game's single player mode (offline mode, which the game pairs with a
 *  joined room), or no room at all (the main menu). False, fail closed, when either probe is unreadable. */
bool can_open();
/** The game as the refusal rules want it, read now. */
Can snapshot();

// ---- Read-outs: every value comes from the running game now, never a cache ----

/** The name of a GhostState number, from the game's own enum values (map names); empty for one the map
 *  does not name. */
std::string state_name(int32_t state);
/** One line: the ghost's state name and its flags (hunting, appeared, cooldown, hunts held off). */
std::string ghost_line();
/** One line: the ghost type number and, for a custom type, its content id. */
std::string ghost_identity();
std::string favourite_room_name();
float base_speed();
int room_count();
std::string room_name(int index);
/** The room index the ghost's favourite is, or -1. */
int favourite_room_index();
bool events_blocked();
bool friendly();
bool lose_equipment();
int evidence_given();
int forced_type();
std::string forced_type_name();
/** How many forced-type options the row offers, the game's offered collection plus Random. */
int forced_type_options();
int forced_type_value(int option);  // option 0 is the game's Random (no override)
float reward_multiplier();
int bonus_reward();
float sanity();
bool dead();
bool frozen();
float time_scale();
/** The local rig's speed multiplier as the game holds it (LocalPlayer.speedMultiplier: the contract's
 *  Difficulty.playerSpeed at the start, a Monkey Paw wish later). 0 with no rig. */
float speed_multiplier();
/** Whether this client's rig is the PC rig whose speed the game can set. False for a VR rig and off
 *  contract; the speed rows say so rather than doing nothing quietly. */
bool speed_settable();
/** The room the hunt is set on, for the debug read-out; empty with no hunt, no target or an unknown room. */
std::string hunt_target_room();
/** Whether the ghost sees nobody now: what its line-of-sight sensor feeds on detects no layer at all. False
 *  when there is no ghost, no vision sensor, or the layers cannot be read. */
bool ghost_blind();

// ---- The rows' actions; the click already passed menu_rule.h's refuse() ----

void force_hunt();
void end_hunt();
void clear_cooldown();
void appear();
void unappear();
/** The game's own favourite-room move, on the master, plus the game's own warp: index is a
 *  snapshot-style index into LevelController.rooms. */
void apply_room(int index);
int random_room();  // an index into the level's rooms, -1 with none
void bump_base_speed(float delta);
void fire_event(int32_t state);
void set_events_blocked(bool value);
void set_friendly(bool value);
void set_lose_equipment(bool value);
void bump_evidence(int delta);
void set_forced_type(int value);
void set_sanity(int percent);
void set_frozen(bool value);
/** Steps the local PC rig's speed through the game's own setter (LocalPCPlayer.SetPlayerSpeedMultiplier, the
 *  call a Monkey Paw wish makes, which also re-applies it to the controller). `steps` is in quarter-steps,
 *  bounded by menu_rule.h. Nothing for a rig the game will not take it on. */
void bump_speed_multiplier(int steps);
/** Gives the rig back the speed multiplier the game kept for it at the start of the contract
 *  (LocalPCPlayer.ResetPlayerSpeedMultiplier). */
void reset_speed_multiplier();
/** Blinds the ghost or gives it its sight back, on its own line-of-sight sensor: the mask that sensor feeds on
 *  is emptied, or the mask found before emptying is put back. The ghost still hears and smells; only what it
 *  can see changes. The mod puts the mask back if it stops while the ghost is still sightless. */
void set_ghost_blind(bool blind);
/** Holds the local investigator in place while the menu is up, as the game does for its journal: no walking
 *  and no looking, crosshair away, cursor free; false gives all of it back. Asks the game only for a PC rig
 *  that has a HUD and a journal controller (the game's release throws without one), and answers whether it
 *  asked, so the menu releases only a hold it took. */
bool pause_player(bool held);
/** Kills the local investigator the way the Monkey Paw and the tarot cards do (Player.BeginDeathSequence with
 *  SnappedNeck, on the master). */
void kill_local();
/** Revives the local investigator through the game's own Player.RevivePlayer. Its Revive RPC goes on only in a
 *  contract carrying the tarot cards or the Monkey Paw, so a contract without either has the paw added to its
 *  Difficulty.actualCursedItems until the revive lands (at most 5 seconds; the author's call, 2026-11-12), and
 *  then its own array goes back. False, asking nothing, without a player or a difficulty. */
bool revive_local();
void unlock_main_door();
/** Ends the contract the way the game's own lift button does, by calling
 *  ExitLevel.StartAttemptExitLevel on the scene's ExitLevel (offline this client is the master, so this
 *  is the same call LiftButton.NetworkedUse makes). The game's coroutine then sends the game's own Exit
 *  RPC with the game's own sender: the mod never touches an RPC. False, starting nothing, when the scene
 *  has no ExitLevel (its serialized ready bool is an editor value, so in a truck-less scene the game
 *  would do nothing either). */
bool end_contract();
void leave();
void quit();
void set_time_scale(float value);

/** How many types the re-type picker offers: the contract's offered collection, custom ghosts included. */
int type_options();
/** The GhostType number of picker option `index`; -1 out of range. */
int32_t type_option(int index);
/** A type for a developer: a custom ghost's content id and its number, or the number alone. */
std::string type_name(int32_t type);
/** Turns the running ghost into `type` the way the game set it up at spawn: the controller's identity and behaviour,
 *  the game's own evidence pick, and GhostAI.Init again on those traits (the game's sync, the evidence in the world
 *  and every per-type start step); then the orb and the freezing flag the old type left are put right. The Mimic,
 *  Thaye and Obambo timers keep running from where they were. False with no ghost. */
bool retype(int32_t type);

/** How many weathers the weather picker offers: every contract weather, the Blood Moon included. */
int weather_count();
/** The game's own value name of picker weather `index`; empty out of range. */
std::string weather_name(int index);
/** The weather the contract plays now (Difficulty.actualWeather), by name; empty with no contract. */
std::string weather_now();
/** Changes the contract's weather to picker weather `index`: Difficulty.actualWeather (which the hunt's Blood Moon
 *  branch reads) and then the Monkey Paw's own RandomWeather.TransitionWeatherProfile (LevelValues.weather, the
 *  sky, the sound; local, nothing sent). Works to and from the Blood Moon. False with no contract or no scene
 *  weather. */
bool apply_weather(int index);

/** Whether noclip is on. */
bool noclip();
/** Switches the PC rig's physical step (PhysicsCharacterController) off or back on. While it is off the
 *  investigator passes through walls and floors, falls nowhere, and the walk the game asks for moves them along
 *  the camera (the mod's PhysicsCharacterController.Move hook). False, changing nothing, without a PC rig. */
bool set_noclip(bool on);
/** Teleports the investigator to the ghost, standing on the navigation mesh under it. False when the game gives
 *  no ghost, no PC rig or no mesh near it. Local: the rig's transform, as the physical step reads it. */
bool me_to_ghost();
/** Teleports the investigator into the ghost's favourite room, onto the mesh nearest the room's first collider's
 *  centre. False as me_to_ghost. */
bool me_to_ghost_room();
/** Warps the ghost onto the mesh at the investigator's feet with its own NavMeshAgent (NavMeshAgent.Warp, the
 *  call the game's own ghost teleports make). The ghost's position reaches others the way its walking does. */
bool ghost_to_me();
/** The evidence the contract gives (GhostController.givenEvidence), vanilla ones by the enum's value names and a
 *  custom one by its number; empty with no contract. */
std::string given_evidence();
/** The ghost's position in the world; false with no ghost. */
bool ghost_at(Vector3& out);
/** The speed the ghost's NavMeshAgent moves at now, in metres a second (the hunt's real speed while hunting);
 *  0 with no ghost. */
float ghost_agent_speed();
/** The name of the level room the ghost stands in now (game::room_at), empty outside every room. */
std::string ghost_room_now();
/** Metres from the local camera to the ghost; negative with no ghost or no camera. */
float ghost_distance();
/** The world centre of room `index` of the level's room list (the first of its colliders' boxes); false when
 *  the room or its colliders are missing. */
bool room_centre(int index, Vector3& out);

/** The event kinds the Ghost page can fire: the game's GhostState numbers, named by the map. Filled now;
 *  empty when the enum values did not resolve. */
struct EventKind {
  const char* name;  // the game's own state name, for a developer
  int32_t value;
};
std::vector<EventKind> event_kinds();

}  // namespace pm::devmenu::actions
