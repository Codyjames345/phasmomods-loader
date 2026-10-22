#pragma once

// Makes the custom ghosts real ghost types for the game while a contract runs: an entry for each in
// the game's GhostCollection (the type roll, evidence, the journal's lists and names all read it),
// their names for the game's localisation keys, and GhostType.None as the behaviour type of a custom
// ghost on every client. Main thread.
namespace pg::custom::registry {

bool resolve();
bool create_hooks();  // before hooks::enable_all

// Every frame. `enabled`: this is a private room in which every client runs the custom ghosts.
// Whether a contract has them is decided from `enabled` when the contract's collection appears, and
// holds until the contract unloads. A contract without them loses any the lobby left behind.
void tick(bool enabled);
bool decided();      // a contract is loaded and whether it has them is settled
bool active();       // the contract's collection holds the entries now
void teardown();     // take the entries out

// The lobby's custom difficulty menu reads a GhostCollection of its own (perhaps the same one): the
// custom ghosts are offered in it while its Ghost Type option may offer them (difficulty.h).
void offer(void* collection);     // add the entries it lacks
void withdraw(void* collection);  // take every custom entry out of it

// The ghost's GhostAI when it is a custom ghost (null otherwise), and its custom type.
void* custom_ghost(int& type);

}  // namespace pg::custom::registry
