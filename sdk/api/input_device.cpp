#include "input_device.h"

#include "Input.h"
#include "Player.h"
#include "game_api.h"
#include "runtime.h"

namespace pm::api::input {

std::optional<bool> keyboard_in_use() {
  if (!all_resolved<phasmo::Network::get_Instance, phasmo::Network::localRig, phasmo::LocalPlayer::playerInput,
                    phasmo::InputDevices::UsesKeyboard>())
    return std::nullopt;
  void* net = phasmo::Network::get_Instance();
  void* rig = net && alive(net) ? phasmo::Network::localRig.get(net) : nullptr;
  void* playerInput = rig && alive(rig) ? phasmo::LocalPlayer::playerInput.get(rig) : nullptr;
  if (!playerInput || !alive(playerInput)) return std::nullopt;
  return phasmo::InputDevices::UsesKeyboard(playerInput);
}

}  // namespace pm::api::input
