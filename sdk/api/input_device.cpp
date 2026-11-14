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
  const auto net = phasmo::Network::get_Instance();
  const auto rig = net && alive(net.raw()) ? phasmo::Network::localRig.get(net) : nullptr;
  const auto playerInput = rig && alive(rig.raw()) ? phasmo::LocalPlayer::playerInput.get(rig) : nullptr;
  if (!playerInput || !alive(playerInput.raw())) return std::nullopt;
  return phasmo::InputDevices::UsesKeyboard(playerInput);
}

}  // namespace pm::api::input
