#pragma once

#include <cstddef>
#include <cstdint>

// A mod's messages over the game's own Photon room connection, on the mod's own channel of the
// host's event (host/net.cpp). Main thread.
namespace pg::net {

enum class To { Others, Master, Actor };

// Handler for a payload on this mod's channel; `sender` is Photon's actor number for the sending
// client, taken from the event itself and never from the payload.
using Handler = void (*)(int sender, const uint8_t* data, size_t size);
void set_handler(Handler handler);

bool send(const uint8_t* data, size_t size, To to, bool reliable, int actor = 0);

}  // namespace pg::net
