#include "outlist.h"

#include <climits>

#include "Photon.h"
#include "Player.h"
#include "game_api.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::outlist {
namespace {

namespace api = pm::api;
using phasmo::PlayerSpot;

struct Out {
  api::Held spot;    // the PlayerSpot taken out
  api::Held photon;  // its Photon player, as the game's lookups compare it: by reference
  int actor = 0;
} g;

api::List spots() {
  void* net = phasmo::Network::get_Instance();
  return api::List{net ? phasmo::Network::playerSpots.get(net) : nullptr};
}

// The key Network.OnPlayerLeftRoom sorts the list by: the spot's Photon actor number, or int.MaxValue
// without a Photon player (decompiled).
int sort_key(void* spot) {
  void* photon = spot ? PlayerSpot::photonPlayer.get(spot) : nullptr;
  return photon ? phasmo::PhotonPlayer::get_ActorNumber(photon) : INT_MAX;
}

// The held spot back in `list`, after every spot whose key is not above its own, as the game's stable
// sort leaves it. Only the List's own Add and RemoveAt, as the game's code uses. False when it could not go back.
bool insert(const api::List& list) {
  if (!list.raw) return false;
  if (list.contains(g.spot.get())) return true;
  const int key = sort_key(g.spot.get());
  size_t at = 0;
  const size_t n = list.size();
  while (at < n && sort_key(list[at]) <= key) ++at;
  if (!list.add(g.spot.get())) return false;
  for (size_t moved = 0; moved < n - at; ++moved) {
    void* item = list[at];
    if (!list.remove_at(at) || !list.add(item)) return false;
  }
  return true;
}

// Inside the Winter's Jest present's search for a spawn point. Its walk of the room is the only reader that must not
// find the parked body: the AI ghost's contract has no such body to stay away from.
int g_inPresentSearch = 0;

// The spot of another Photon player in the room's walk, or null. A duplicate position never changes the search's
// minimum distances (KrampusPresent.GetFurthestSpawnPositionFromPlayers), so the walk gives what a walk over the
// investigators alone gives.
void* other_players_spot(api::Original<phasmo::Network::GetSpotPlayerFromPhotonPlayer> original, void* net) {
  const api::RefArray room{phasmo::PhotonNetwork::get_PlayerList()};
  for (size_t i = 0, n = room.size(); i < n; ++i) {
    void* other = room[i];
    if (!other || other == g.photon.get()) continue;
    void* spot = original(net, other);
    if (spot && api::alive(PlayerSpot::player.get(spot))) return spot;
  }
  return nullptr;
}

void* detour_spot_from_photon(api::Original<phasmo::Network::GetSpotPlayerFromPhotonPlayer> original, void* net,
                              void* photon) {
  if (photon && g.spot.get() && photon == g.photon.get()) {
    if (g_inPresentSearch > 0)
      if (void* other = other_players_spot(original, net)) return other;
    // What the game's own lookup would find with the spot in the list: that spot.
    return g.spot.get();
  }
  return original(net, photon);
}

phasmo::Vector3 detour_present_search(api::Original<phasmo::KrampusPresent::GetFurthestSpawnPositionFromPlayers> original,
                                      void* present) {
  struct InSearch {
    InSearch() { ++g_inPresentSearch; }
    ~InSearch() { --g_inPresentSearch; }
    InSearch(const InSearch&) = delete;
    InSearch& operator=(const InSearch&) = delete;
  } in;
  return original(present);
}

// A held spot has position -1 (Network.GetIndexOfPlayer), which to this method means this client's own body type
// for every character. The game raises -1 itself only from the customisation screen, in the lobby, where no spot
// is held. So in a level it can only be the held spot's, and with the spot in the list it would have touched the
// parked body alone.
void detour_body_type(api::Original<phasmo::PlayerCharacter::SetPlayerBodyType> original, void* character,
                      int32_t position) {
  if (position == -1 && g.spot.get()) return;
  original(character, position);
}

void detour_player_left(api::Original<phasmo::Network::OnPlayerLeftRoom> original, void* net, void* photon) {
  if (photon && g.spot.get() && photon == g.photon.get()) {
    // The game's departure finds the spot where it looks for it, and does the rest itself.
    if (insert(spots())) PM_LOG("outlist: the ghost player left the room; their spot is back for the game's own departure");
    else PM_LOG("outlist: the ghost player left the room, and their spot could not go back; the game's departure finds none");
    g = Out{};
  }
  original(net, photon);
}

}  // namespace

bool take_out(int actor) {
  if (actor <= 0) return false;
  if (g.spot.get() && g.actor == actor) return true;
  put_back();
  const api::List list = spots();
  for (size_t i = 0, n = list.size(); i < n; ++i) {
    void* spot = list[i];
    void* photon = spot ? PlayerSpot::photonPlayer.get(spot) : nullptr;
    if (!photon || phasmo::PhotonPlayer::get_ActorNumber(photon) != actor) continue;
    g.spot = spot;
    g.photon = photon;
    g.actor = actor;
    if (!list.remove_at(i)) {
      g = Out{};
      return false;
    }
    PM_LOG("outlist: the ghost player's spot is out of the player list");
    return true;
  }
  return false;
}

void put_back() {
  if (!g.spot.get()) return;
  if (phasmo::PhotonNetwork::get_InRoom()) {
    if (insert(spots())) PM_LOG("outlist: the ghost player's spot is back in the player list");
    else PM_LOG("outlist: the ghost player's spot could not go back in the player list");
  } else {
    PM_LOG("outlist: out of the room; the ghost player's spot is let go");  // Network.OnLeftRoom emptied the list
  }
  g = Out{};
}

int actor() { return g.spot.get() ? g.actor : 0; }

void* player() { return g.spot.get() ? PlayerSpot::player.get(g.spot.get()) : nullptr; }

bool create_hooks() {
  // Every body: the copies are reached only from methods nothing calls (the entry's note), but any that answered
  // differently would be a lookup that no longer finds the spot.
  return api::hook<phasmo::Network::GetSpotPlayerFromPhotonPlayer>(&detour_spot_from_photon, nullptr,
                                                                   api::Twins::All) &&
         api::hook<phasmo::PlayerCharacter::SetPlayerBodyType>(&detour_body_type, nullptr,
                                                               api::Twins::CanonicalOnly) &&
         api::hook<phasmo::KrampusPresent::GetFurthestSpawnPositionFromPlayers>(&detour_present_search, nullptr,
                                                                                api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Network::OnPlayerLeftRoom>(&detour_player_left, nullptr, api::Twins::CanonicalOnly);
}

}  // namespace pm::outlist
