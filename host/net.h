#pragma once

#include <cstddef>
#include <cstdint>

#include "mod_api.h"

// The suite's messages over the game's own Photon room connection: PhotonNetwork.RaiseEvent with
// event code 197 and a byte[] payload whose first byte is the channel (0 for the host, a mod's
// PgModInfo::channel otherwise), received in LoadBalancingClient.OnEvent. Main thread.
namespace pg::hostnet {

constexpr uint8_t kEventCode = 197;
constexpr uint8_t kHostChannel = 0;

bool resolve();
bool create_hooks();  // before hooks::enable_all

void set_receiver(uint8_t channel, PgReceiver receiver);
bool send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor);

}  // namespace pg::hostnet
