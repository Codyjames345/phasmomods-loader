#pragma once

// The ghost player's PlayerSpot, out of the game's player list (Network.playerSpots) on every client while
// their body is parked (src/body.h), so no rule or screen that walks the list sees them: the dead count,
// the paw, the cards, the all-dead test, team sanity, the hunts and the truck's sanity monitor.
//
// Each client takes the spot out of its own list; no game RPC and no message of the mod's own. A Photon
// player with no spot is not invisible to the game, though (sdk/systems/players.md, "A player with no
// spot"), so while the spot is out:
//  - Network.GetSpotPlayerFromPhotonPlayer still answers the held spot for the ghost player's Photon player,
//    as it would with the spot in the list; Network.GetPlayerFromPhotonPlayer calls it, so it still answers
//    the spot's Player (the parked body). Every caller of either lookup, the game's own checks among them,
//    gets the answer it got with the spot in the list. Every other Photon player goes straight to the game's
//    own lookup.
//  - Network.OnPlayerLeftRoom finds the spot back in the list when the ghost player leaves, so the game's
//    own departure runs for them.
//  - While KrampusPresent.GetFurthestSpawnPositionFromPlayers runs, that lookup answers another player's spot for
//    the ghost player's Photon player, so the Winter's Jest present is placed as in an AI ghost's contract.
//  - PlayerCharacter.SetPlayerBodyType ignores -1 while a spot is held: that is the held spot's position, and it
//    would change every character's body type.
// Every later player moves down one place in the list while the spot is out (provisional, sdk/systems/players.md).
// A client takes the spot out only once it has heard the ghost player stepped out (session HELLO, re-announced every 2 s;
// stepped-out peers are dropped only when they leave the room). Until a lost or late HELLO comes, that client keeps the
// spot and a living, visible body, and if it is the master the all-dead end waits with it.
// On investigators' clients the spot comes back on the first tick after the level is gone, so lobby code in that one
// frame reads the shorter list. None was found: LobbyController.Start only subscribes, and Network.SetSpotPlayer waits.
// put_back returns the spot in the order the game sorts the list (Photon actor number), while this client
// is still in a room; outside one the game has emptied its list (Network.OnLeftRoom), so the spot is forgotten.
namespace pm::outlist {

// Takes the spot of the Photon player with actor number `actor` out of Network.playerSpots and holds it.
// True when that spot is out, already or now; false when no spot holds that actor.
bool take_out(int actor);

// Puts the held spot back (see above) and lets it go. Nothing when no spot is held.
void put_back();

// The actor whose spot is held, or 0.
int actor();

// The held spot's networked Player (the parked body), or null.
void* player();

bool create_hooks();

}  // namespace pm::outlist
