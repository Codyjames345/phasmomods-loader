#pragma once

// The custom difficulty's Ghost Type option offers the custom ghosts among the game's own, in
// alphabetical order, in a private room where every player runs them, and the contract's spawn forces
// the pick as it forces any type (Difficulty.ghostTypeOverride); the difficulty's summary names it. A
// custom pick never leaves the game: the save file holds Random instead, and so does the lobby sync
// while anyone in the room is without the custom ghosts. The pick each custom preset forces is kept
// in a file of the mod's own and put back wherever the game holds that preset at Random. Main thread.
namespace pg::custom::difficulty {

bool resolve();
// `picksFile`: where each preset's pick is kept. `everyone`: whether this is a private room in which
// every player runs the custom ghosts, asked as each lobby sync is sent, so a player who has only
// just joined is already counted.
bool create_hooks(const wchar_t* picksFile, bool (*everyone)());  // before hooks::enable_all
void tick();      // every frame, after registry::tick
void teardown();  // the mod stops: no custom pick is left where the game would save or send it

}  // namespace pg::custom::difficulty
