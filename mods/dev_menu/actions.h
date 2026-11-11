#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "menu_rule.h"

// The dev menu's game side: its three hooks and the rows' actions. Every action is the game's own method,
// on the client the game would call it from (the mod never sends an RPC by name or runs an RPC body); the
// rules in menu_rule.h decide before a click, and the game's own guards still decide after. Main thread.
namespace pm::devmenu::actions {

/** Creates the progression-suppression, god-mode and consumable hooks. False when any cannot be made: the
 *  mod stays off, since a dev session without progression suppression is refused (fail closed). */
bool start();
void stop();

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
/** Holds the local investigator in place while the menu is up, as the game does for its journal: no walking
 *  and no looking, crosshair away, cursor free; false gives all of it back. Does nothing at the main menu. */
void pause_player(bool held);
void kill_local();
void revive_local();
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

/** The event kinds the Ghost page can fire: the game's GhostState numbers, named by the map. Filled now;
 *  empty when the enum values did not resolve. */
struct EventKind {
  const char* name;  // the game's own state name, for a developer
  int32_t value;
};
std::vector<EventKind> event_kinds();

}  // namespace pm::devmenu::actions
