#pragma once

#include <string>
#include <vector>

// The in-game mod list. A "Mods" button on the main menu (a copy of the game's own News tape, which
// the game keeps switched off, in its place) opens a panel drawn as the game's popup
// (host/menu_panel.h): every installed mod with its description and whether it is on. Clicking a
// row switches it; Apply hands the new choice to the host, which restarts the mods with it. Mods
// cannot be changed in a room. Main thread only.
namespace pm::mods_panel {

// What this build lacks, in the log: at activation, once the host's game API is bound.
void report();
// Every frame once the player has agreed to the notice. `canChange`: not in a room.
void tick(bool canChange);
bool take_choice(std::vector<std::string>& ids);  // the choice applied in the panel, once
// The host restarted the mods with it (`ok`, or some could not start) and tried to save it (`saved`): the panel says so.
void applied(bool ok, bool saved = true);
// Instead of tick, when no mod runs for a reason the player did not choose: the notice could not be shown, or this build
// lacks what the lobby's rules need. The game's own UI then names `cause`: the Mods button opens the panel with it as the
// status, every mod's row unticked and greyed, and only a Close button. Nothing can be switched on here. Where this build
// cannot make the button or the panel, which is usual when the notice could not be shown, nothing shows, and the log
// and the player guide are all there is: the loader draws no window of its own.
void tick_off(const std::string& cause);
void teardown();

}  // namespace pm::mods_panel
