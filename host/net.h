#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "mod_api.h"

// The suite's messages over the game's own Photon room connection: PhotonNetwork.RaiseEvent with
// event code 197 and a byte[] payload whose first byte is the channel (0 for the host, a mod's
// effective channel otherwise: the one it declared or, for a mod declaring 0, the one the loader
// derived from its id), received in LoadBalancingClient.OnEvent. Main thread.
namespace pm::hostnet {

constexpr uint8_t kEventCode = 197;
constexpr uint8_t kHostChannel = 0;

// Once the player agreed, before hooks::enable_all: the hook on LoadBalancingClient.OnEvent. An entry that did not resolve
// makes no hook and is no failure (available stays false); false only when a resolved method could not be hooked.
bool create_hooks();

// Whether this build gives everything a send and a receive need, with the hook made. False before the host's bind and
// before the hook. Until then a message of the suite's stays the game's to ignore, as on a client without the loader.
bool available();
// What this build lacks for it, for the log, which is asked before the hook is made: the keys of the entries that did not
// resolve, separated by ", "; empty when it lacks nothing.
std::string missing();

void set_receiver(uint8_t channel, PmReceiver receiver);
// Who a mod's channel takes a message from, asked for each one that arrives: `gate` says whether `sender`'s message on
// `channel` is delivered, and contains what it needs to. With no gate, or a gate that throws, a mod's message is dropped.
// The host's own channel has no gate: its receiver reads what any client says about itself.
using Gate = bool (*)(uint8_t channel, int32_t sender);
void set_gate(Gate gate);
// False, with nothing sent, while not available(): an entry that did not resolve never leaves a default deciding who a
// message goes to.
bool send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor);
// A message for the other players when not all of them are verified: to each of `verified`, one by one, so nobody else gets it.
// With `everyone` true all of them are, and it goes as one message to the others. True when every send was taken.
bool send_to_verified(uint8_t channel, const uint8_t* data, size_t size, bool reliable, const std::vector<int>& verified, bool everyone);
// A reliable message to everyone in the room through the server, this client too: the way the game's own RpcTarget's
// via-server sends come back to their sender, which ServerManager.LoadScene relies on. Not open to mods.
bool send_through_server_to_all(uint8_t channel, const uint8_t* data, size_t size);

}  // namespace pm::hostnet
