// Head-mounted gear (the head camera) while a body dances (the user, 2026-11-15, from the live run: the worn
// head camera stayed where the standing head is while the dance moved the head). The game hangs mounted gear from
// its attach point, which no body animation moves (sdk/systems/players.md, "Head-mounted gear"). While this mod
// animates a body, that body's gear goes onto the game's own point under the head bone, and back to the parent the
// game had it under when the dance ends. The move keeps the gear's local place and turn, which the game's mounting
// sets to the same constants for either point, so it lands exactly where the game would hang it on the head.
// A dancer who mounts gear mid-dance is found by `follow` and ridden too.
#pragma once

#include "held.h"

namespace pm::emotes::gear {

/// One dancing body's gear watch. Empty means this body has no gear socket at all, so nothing can be done for it.
struct Watch {
  pm::api::Held socket;       ///< the body's NetworkedHeadMountedSocket
  pm::api::Held spot;         ///< the game's own point under the body's head bone (modelHeadSpot)
  pm::api::Held item;         ///< the Equipment worn on the head, once seen; empty while the body wears none
  pm::api::Held original;     ///< the transform the game had that gear under when it was first seen
};

/// Starts watching `player`'s body: finds its socket and its point under the head bone, and if gear is worn,
/// remembers the parent the game hangs it under and hangs it on the head point. Returns false, leaving `watch`
/// empty, only when the body has no socket or no head point at all: a body that wears nothing is still watched, so
/// gear mounted later is found by `follow`.
bool begin(Watch& watch, void* player);

/// A frame of the dance: gear not seen yet (a fresh mount) is found and remembered; gear the game hung out again
/// goes back onto the head point. Gear taken off (a null parent) is left off.
void follow(Watch& watch);

/// The dance ended: if watched gear is still on the head point, back under the parent the game had it under. Gear
/// the game took off during the dance is left off. Clears `watch` either way.
void end(Watch& watch);

}  // namespace pm::emotes::gear
