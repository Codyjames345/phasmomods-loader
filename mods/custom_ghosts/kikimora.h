#pragma once

// The Kikimora's rule: it carries off equipment lying loose near it and hides it in another dark,
// empty room, leaving EMF where it was as a ghost's throw does. It never touches anything in a room
// with the lights on. Runs on the host, which takes the item over through its PhotonView as a player
// picking it up does, so every client sees it move. Main thread.
namespace pg::custom::kikimora {

bool resolve();
// Every frame on the host while a Kikimora haunts the contract; it hides something by itself every
// so often unless a player controls it (who hides things with hide()).
void tick(void* ghost, bool steered);
// Hide `item` (a PhotonObjectInteract) now, if the Kikimora's rules allow it; nearest() finds the
// loose equipment closest to the ghost that it may take (null for none).
bool hide(void* ghost, void* item);
void* nearest(void* ghost);
// The ghost stands in a room with its lights on, where the Kikimora touches nothing.
bool in_lit_room(void* ghost);
void reset();

}  // namespace pg::custom::kikimora
