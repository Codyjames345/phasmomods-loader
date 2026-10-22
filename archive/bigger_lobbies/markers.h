#pragma once

// The journal's marks for the other players' choices (each ghost and evidence button shows, by a
// small mark per player, who has picked it). The game keeps four marks per button and finds a
// player's by their place in the room, so players 5-8 had none: each button gets four more, copied
// from its fourth and set further along the same line. Should a button still lack one, the game's
// own update for that player is skipped rather than failing. Main thread.
namespace pg::lobbies::markers {

bool resolve();
bool create_hooks();  // before hooks::enable_all
void tick();

}  // namespace pg::lobbies::markers
