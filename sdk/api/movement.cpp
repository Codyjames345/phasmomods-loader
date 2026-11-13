#include "movement.h"

#include "Difficulty.h"
#include "GameController.h"
#include "Player.h"
#include "game_api.h"
#include "runtime.h"

namespace pm::api::movement {

std::optional<bool> sprinting() {
  using phasmo::LocalPlayer;
  if (!all_resolved<phasmo::Network::get_Instance, phasmo::Network::localRig, phasmo::LocalPCPlayer::type,
                    phasmo::LocalPCPlayer::firstPersonController, phasmo::FirstPersonController::running,
                    LocalPlayer::stamina, phasmo::PlayerStamina::canSprint, phasmo::GameController::instance,
                    phasmo::LevelValues::instance, phasmo::LevelValues::difficulty, phasmo::Difficulty::sprinting>())
    return std::nullopt;
  void* net = phasmo::Network::get_Instance();
  void* rig = net && alive(net) ? phasmo::Network::localRig.get(net) : nullptr;
  // The VR rig's stamina applies its speed another way (VRStamina.Update), not traced.
  if (!rig || !alive(rig) || !phasmo::LocalPCPlayer::type.is(rig)) return std::nullopt;
  void* controller = phasmo::LocalPCPlayer::firstPersonController.get(rig);
  void* stamina = LocalPlayer::stamina.get(rig);
  if (!controller || !alive(controller) || !stamina || !alive(stamina)) return std::nullopt;
  const bool running = phasmo::FirstPersonController::running.get(controller);
  const bool canSprint = phasmo::PlayerStamina::canSprint.get(stamina);
  // In a contract (a GameController exists) the difficulty's setting decides too. LocalPCPlayer.UpdatePlayerSpeed
  // reads it through LevelValues.get_Instance, which makes a LevelValues where none exists; the static is read here
  // instead, so asking never makes one.
  void* game = phasmo::GameController::instance.get();
  const bool inContract = game && alive(game);
  int setting = 0;
  if (inContract) {
    void* values = phasmo::LevelValues::instance.get();
    void* difficulty = values && alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
    if (!difficulty) return std::nullopt;
    setting = phasmo::Difficulty::sprinting.get(difficulty);
  }
  return sprints(running, canSprint, inContract, setting);
}

}  // namespace pm::api::movement
