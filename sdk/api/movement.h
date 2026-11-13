#pragma once

/// @file
/// @brief How the local investigator moves, as the game decides it: whether it sprints now.
///
/// The game works out a player's speed on that player's own client (sdk/systems/players.md, "Speed and stamina"):
/// LocalPCPlayer.UpdatePlayerSpeed gives the rig's controller the sprint speed while the player asks to sprint
/// (FirstPersonController.running: Run held, or toggled on in the game's toggle sprint mode), sprinting is allowed
/// (Difficulty.sprinting of LevelValues.difficulty is not 0, or no GameController.instance exists) and the stamina may
/// sprint (PlayerStamina.canSprint of LocalPlayer.stamina). sprinting() reads those three, so a mod follows the game's
/// own decision rather than measuring the speed.
///
/// Main thread. Local only: no other client holds these values.
/// @ingroup runtime
/// @since Contract 2.6

#include <optional>

namespace pm::api::movement {

/// The game's sprint rule, LocalPCPlayer.UpdatePlayerSpeed's: the sprint speed while all three hold.
/// @param running FirstPersonController.running: the player asks to sprint.
/// @param canSprint PlayerStamina.canSprint: the stamina may sprint.
/// @param inContract Whether GameController.instance exists.
/// @param sprintingSetting Difficulty.sprinting: 0 off, 1 on, 2 infinite. Read only in a contract.
/// @return Whether the game gives the sprint speed.
constexpr bool sprints(bool running, bool canSprint, bool inContract, int sprintingSetting) {
  return running && canSprint && (!inContract || sprintingSetting != 0);
}

/// Whether this client's investigator sprints now, by the game's rule (sprints) on its own values.
/// @return The answer on the PC rig. Nothing on the VR rig (its stamina applies its speed another way,
///   VRStamina.Update, which was not traced), with no local rig, outside a level, or where an entry it reads did not
///   resolve.
std::optional<bool> sprinting();

}  // namespace pm::api::movement
