#pragma once

// The Black Shuck's hunt, kept free of game calls so it can be tested offline. It is blind and deaf:
// it finds players by their scent trails (trail.h) alone, follows one player's trail to them, and
// has the game search the hiding place a trail ends at. The game's smudge state wins over all of it.
namespace pg::custom {

// Metres from a hidden prey at which its trail has brought the hound to the hiding place: the reach
// of the game's own closet search in a hunt (build 25811160, the constant at RVA 0x5583290).
constexpr float kShuckSearchReach = 2.0f;

enum class ShuckMove {
  Native,  // the game's hunt moves it: smudged, or no scent in range (it roams as with no target)
  Follow,  // on along its prey's trail
  Search,  // the game's own closet search of the hiding place its prey's trail ends at
  Sniff,   // on along the nearest other trail in range
};

/**
 * What the Black Shuck does this frame of a hunt. smudged: the game's smudge state holds it. hasPrey:
 * it has a player it follows. preyFollowable: their trail is in reach with no salt or water across it.
 * preyHidden: they are in a hiding place and their trail has brought the hound within kShuckSearchReach.
 * otherTrail: some trail is in reach.
 */
inline ShuckMove shuck_move(bool smudged, bool hasPrey, bool preyFollowable, bool preyHidden, bool otherTrail) {
  if (smudged) return ShuckMove::Native;
  if (hasPrey && preyFollowable) return preyHidden ? ShuckMove::Search : ShuckMove::Follow;
  return otherTrail ? ShuckMove::Sniff : ShuckMove::Native;
}

/**
 * The Black Shuck's target policy: of the player the game's hunt asks about or would set (proposed),
 * whom it keeps. Only its prey, and only while it follows them; in a Search the game's own closet search
 * must find them unseen, and with no trail to follow it keeps nobody. Null for none.
 */
inline void* shuck_target(ShuckMove move, void* prey, void* proposed) {
  return move == ShuckMove::Follow && proposed && proposed == prey ? proposed : nullptr;
}

}  // namespace pg::custom
