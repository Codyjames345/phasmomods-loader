#pragma once

#include <string>
#include <vector>

// The in-game mod list. A "Mods" button on the main menu (a copy of the game's own News tape, which
// the game keeps switched off, in its place) opens a panel drawn as the game's popup
// (host/menu_panel.h): every installed mod with its description and whether it is on. Clicking a
// row switches it; Apply hands the new choice to the host, which restarts the mods with it. Mods
// cannot be changed in a room. Main thread only.
namespace pg::mods_panel {

bool resolve();
// Every frame once the player has agreed to the notice. `canChange`: not in a room.
void tick(bool canChange);
bool take_choice(std::vector<std::string>& ids);  // the choice applied in the panel, once
void applied(bool ok);  // the host restarted the mods with it (or could not): the panel says so
void teardown();

}  // namespace pg::mods_panel
