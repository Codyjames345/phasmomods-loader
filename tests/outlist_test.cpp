// Offline test of the ghost player's spot out of the game's player list (mods/playable_ghost/outlist.cpp)
// over the fake game. The paw, the High Priestess and the all-dead test are this file's own models, so they pin only
// that the spot is out of the list and what the game's rule then gives, not the game's code. body.cpp's gate on
// GameController.allPlayersLoaded and the ghost player's own client have no seam here (smoke steps).
// The game's own readers of the list are modelled here on their decompiled bodies
// (sdk/systems/players.md): the dead count, the Monkey Paw's revive wish, the High Priestess, the all-dead
// test, the lookups of a Photon player's spot and Player (Network.GetSpotPlayerFromPhotonPlayer, which
// Network.GetPlayerFromPhotonPlayer calls) and Photon's departure callback (Network.OnPlayerLeftRoom), the spot
// lookup and the departure reached through the mod's hooks as the game reaches them. Run: build\outlist_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Photon.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "outlist.h"
#include "runtime.h"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

// The test's own offsets; the fake backend reads them in the object itself.
constexpr int32_t kSpots = 0x08, kLocal = 0x10;  // Network
constexpr int32_t kPhoton = 0x08, kPlayer = 0x10;  // PlayerSpot
constexpr int32_t kDead = 0x08;                    // Player

struct Object {
  void* klass = nullptr;
  alignas(8) unsigned char fields[0x20]{};
};
template <class T>
void put(Object& o, int32_t offset, T value) {
  *reinterpret_cast<T*>(reinterpret_cast<char*>(&o) + offset) = value;
}
template <class T>
T get(const Object& o, int32_t offset) {
  return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(&o) + offset);
}

struct Photon {
  void* klass = nullptr;
  int actor = 0;
};

Object network;
Photon pWisher{nullptr, 1}, pGhost{nullptr, 2}, pAlice{nullptr, 3}, pBob{nullptr, 4}, pGone{nullptr, 5};
Object wisher, ghostBody, alice, bob;
Object sWisher, sGhost, sAlice, sBob;
void* spotList = nullptr;
bool inRoom = true;
std::vector<Photon*> room;  // PhotonNetwork.get_PlayerList
std::vector<std::string> events;
std::map<std::string, void*> detours;
PmHostApi capturing{};
bool (*fakeHookBody)(const char*, uint32_t, void*, void**, const char*) = nullptr;

bool capture_hook_body(const char* key, uint32_t index, void* detour, void** original, const char* name) {
  if (!fakeHookBody(key, index, detour, original, name)) return false;
  detours[key] = detour;
  return true;
}

void* get_instance(const void*) { return &network; }
bool get_in_room(const void*) { return inRoom; }
int32_t actor_number(void* photon, const void*) { return static_cast<Photon*>(photon)->actor; }

std::vector<void*> spots() { return fake::list_items_of(spotList); }
Object& spot(void* s) { return *static_cast<Object*>(s); }
bool dead(Object& p) { return get<bool>(p, kDead); }
void set_dead(Object& p, bool d) { put<bool>(p, kDead, d); }
bool in_room(void* photon) { return std::find(room.begin(), room.end(), photon) != room.end(); }

std::string name_of(void* p) {
  if (p == &wisher) return "wisher";
  if (p == &alice) return "alice";
  if (p == &bob) return "bob";
  if (p == &ghostBody) return "body";
  return p ? "?" : "null";
}

// Network.GetSpotPlayerFromPhotonPlayer: the first spot holding that Photon player, by reference, else null.
void* spot_from_photon_body(void*, void* photon, const void*) {
  for (void* s : spots())
    if (get<void*>(spot(s), kPhoton) == photon) return s;
  return nullptr;
}

void* spot_lookup(void* photon);

// Network.GetPlayerFromPhotonPlayer: it calls the spot lookup directly, so the game's call reaches the mod's
// hook; that spot's Player, else localPlayer.
void* player_from_photon_body(void* net, void* photon, const void*) {
  void* s = spot_lookup(photon);
  return s ? get<void*>(spot(s), kPlayer) : get<void*>(*static_cast<Object*>(net), kLocal);
}

// Network.OnPlayerLeftRoom: nothing without a spot; otherwise the spot goes, and the event the all-dead test
// runs from is raised.
void player_left_body(void*, void* photon, const void*) {
  const std::vector<void*> list = spots();
  for (size_t i = 0; i < list.size(); ++i) {
    if (get<void*>(spot(list[i]), kPhoton) != photon) continue;
    fake::list_remove_at(spotList, static_cast<int32_t>(i));
    events.push_back("departure " + std::to_string(static_cast<Photon*>(photon)->actor));
    return;
  }
  events.push_back("no departure");
}

// PlayerCharacter.SetPlayerBodyType: the positions that reached the game.
std::vector<int32_t> bodyTypes;
void body_type_body(void*, int32_t position, const void*) { bodyTypes.push_back(position); }

// The game reaching its own methods, through the mod's hooks.
void* spot_lookup(void* photon) {
  return reinterpret_cast<void* (*)(void*, void*, const void*)>(detours.at(
      map::Network_GetSpotPlayerFromPhotonPlayer.key))(&network, photon, nullptr);
}
void* lookup(void* photon) { return player_from_photon_body(&network, photon, nullptr); }

// KrampusPresent.GetFurthestSpawnPositionFromPlayers' rule over given positions: the spawn point with the largest
// smallest distance. Each Player's position is its x in positionOf.
std::map<void*, float> positionOf;
const float kSpawnPoints[] = {5.0f, 15.0f, 50.0f, 90.0f};
float furthest_spawn(const std::vector<Object*>& players) {
  float best = -1.0f, at = 0.0f;
  for (const float spawn : kSpawnPoints) {
    float nearest = 1e9f;
    for (Object* p : players) nearest = std::min(nearest, std::abs(spawn - positionOf[p]));
    if (nearest > best) {
      best = nearest;
      at = spawn;
    }
  }
  return at;
}
// The method's body: it walks the room through the game's lookup, and throws on a null.
bool searchSawNull = false;
phasmo::Vector3 present_search_body(void*, const void*) {
  std::vector<Object*> players;
  for (Photon* p : room) {
    void* player = lookup(p);
    if (!player) searchSawNull = true;
    else players.push_back(static_cast<Object*>(player));
  }
  return phasmo::Vector3{furthest_spawn(players), 0.0f, 0.0f};
}
void* get_player_list(const void*) {
  void* players = fake::array(sizeof(void*), room.size());
  for (size_t i = 0; i < room.size(); ++i) pm::api::RefArray{players}.set(i, room[i]);
  return players;
}
void leaves(Photon& p) {
  room.erase(std::remove(room.begin(), room.end(), &p), room.end());
  reinterpret_cast<void (*)(void*, void*, const void*)>(detours.at(map::Network_OnPlayerLeftRoom.key))(&network, &p,
                                                                                                    nullptr);
}

// GameController.AmountOfDeadPlayers: the spots whose Photon player is in the room and whose Player is dead.
int dead_count() {
  int n = 0;
  for (void* s : spots())
    if (in_room(get<void*>(spot(s), kPhoton)) && dead(*static_cast<Object*>(get<void*>(spot(s), kPlayer)))) ++n;
  return n;
}
// The paw's wish: at a count of 0 it twitches and keeps the use; otherwise MonkeyPaw.ReviveWish revives the
// first dead player who is not the wisher.
std::string revive_wish() {
  if (dead_count() == 0) return "twitch, use kept";
  for (void* s : spots()) {
    void* p = get<void*>(spot(s), kPlayer);
    if (p != &wisher && dead(*static_cast<Object*>(p))) return "revive " + name_of(p);
  }
  return "twitch, use spent";
}
// TarotCards.HighPriestess: the first dead player is revived; with none, its flag.
std::string high_priestess() {
  for (void* s : spots()) {
    void* p = get<void*>(spot(s), kPlayer);
    if (dead(*static_cast<Object*>(p))) return "revive " + name_of(p);
  }
  return "flag";
}
// GameController.PlayerDied's all-dead test: every spot's Player is dead.
bool all_dead() {
  for (void* s : spots())
    if (!dead(*static_cast<Object*>(get<void*>(spot(s), kPlayer)))) return false;
  return true;
}
std::vector<std::string> order() {
  std::vector<std::string> names;
  for (void* s : spots()) names.push_back(name_of(get<void*>(spot(s), kPlayer)));
  return names;
}
using Names = std::vector<std::string>;

void reset() {
  pm::outlist::put_back();  // nothing held from the last case, while its list still exists
  fake::reset();
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&get_instance));
  fake::method(map::PhotonNetwork_get_InRoom, reinterpret_cast<void*>(&get_in_room));
  fake::method(map::PhotonPlayer_get_ActorNumber, reinterpret_cast<void*>(&actor_number));
  fake::method(map::Network_GetSpotPlayerFromPhotonPlayer, reinterpret_cast<void*>(&spot_from_photon_body));
  fake::method(map::Network_GetPlayerFromPhotonPlayer, reinterpret_cast<void*>(&player_from_photon_body));
  fake::method(map::Network_OnPlayerLeftRoom, reinterpret_cast<void*>(&player_left_body));
  fake::method(map::PlayerCharacter_SetPlayerBodyType, reinterpret_cast<void*>(&body_type_body));
  bodyTypes.clear();
  fake::method(map::PhotonNetwork_get_PlayerList, reinterpret_cast<void*>(&get_player_list));
  fake::method(map::KrampusPresent_GetFurthestSpawnPositionFromPlayers, reinterpret_cast<void*>(&present_search_body));
  positionOf = {{&wisher, 0.0f}, {&alice, 10.0f}, {&bob, 20.0f}, {&ghostBody, 100.0f}};
  searchSawNull = false;
  fake::field(map::Network_playerSpots, kSpots);
  fake::field(map::Network_localPlayer, kLocal);
  fake::field(map::PlayerSpot_photonPlayer, kPhoton);
  fake::field(map::PlayerSpot_player, kPlayer);
  capturing = *fake::host();
  fakeHookBody = capturing.hook_body;
  capturing.hook_body = &capture_hook_body;
  detours.clear();
  std::string missing;
  CHECK(pm::api::bind(&capturing, missing));
  CHECK(pm::outlist::create_hooks());
  for (Object* p : {&wisher, &ghostBody, &alice, &bob}) *p = Object{};
  const std::pair<Object*, std::pair<Photon*, Object*>> table[] = {
      {&sWisher, {&pWisher, &wisher}}, {&sGhost, {&pGhost, &ghostBody}}, {&sAlice, {&pAlice, &alice}},
      {&sBob, {&pBob, &bob}}};
  for (const auto& [s, pp] : table) {
    *s = Object{};
    put<void*>(*s, kPhoton, pp.first);
    put<void*>(*s, kPlayer, pp.second);
  }
  spotList = fake::list({&sWisher, &sGhost, &sAlice, &sBob});  // sorted by actor, as the game keeps it
  network = Object{};
  put<void*>(network, kSpots, spotList);
  put<void*>(network, kLocal, &alice);  // this client is Alice's
  room = {&pWisher, &pGhost, &pAlice, &pBob};
  inRoom = true;
  events.clear();
}

void the_spot_goes_out_and_the_others_keep_their_order() {
  reset();
  CHECK(pm::outlist::take_out(2));
  CHECK((order() == Names{"wisher", "alice", "bob"}));
  CHECK(pm::outlist::actor() == 2);
  CHECK(pm::outlist::player() == &ghostBody);
  CHECK(pm::outlist::take_out(2));  // already out: nothing more
  CHECK((order() == Names{"wisher", "alice", "bob"}));
  reset();
  CHECK(!pm::outlist::take_out(9));  // nobody holds that actor
  CHECK(!pm::outlist::take_out(0));
  CHECK(order().size() == 4);
}

// Nobody dead: the paw twitches and keeps its use, as in vanilla. The body is not dead-marked, and even a
// dead-marked one would not count, being out of the list.
void the_paw_with_nobody_dead_keeps_its_use() {
  reset();
  pm::outlist::take_out(2);
  CHECK(revive_wish() == "twitch, use kept");
  set_dead(ghostBody, true);
  CHECK(dead_count() == 0);
  CHECK(revive_wish() == "twitch, use kept");
}

void the_paw_revives_the_dead_investigator() {
  reset();
  pm::outlist::take_out(2);
  set_dead(bob, true);
  CHECK(dead_count() == 1);
  CHECK(revive_wish() == "revive bob");
}

void the_high_priestess_revives_an_investigator_or_sets_its_flag() {
  reset();
  pm::outlist::take_out(2);
  set_dead(ghostBody, true);  // ahead of Alice in the room's order, were it in the list
  set_dead(alice, true);
  CHECK(high_priestess() == "revive alice");
  set_dead(alice, false);
  CHECK(high_priestess() == "flag");
}

// The all-dead test ends the contract once every investigator is dead: with the ghost player's spot out it
// is the same test as a room without one, and with the spot in, their living body would hold it back.
void the_all_dead_test_counts_investigators_only() {
  reset();
  pm::outlist::take_out(2);
  for (Object* p : {&wisher, &alice}) set_dead(*p, true);
  CHECK(!all_dead());
  set_dead(bob, true);
  CHECK(all_dead());
  reset();  // the spot in the list: the living body holds the end back
  for (Object* p : {&wisher, &alice, &bob}) set_dead(*p, true);
  CHECK(!all_dead());
}

// The spot lookup answers the held spot for the ghost player, exactly what it found in the list before; every
// other Photon player is the game's own lookup, unchanged.
void the_spot_lookup_finds_the_held_spot_and_passes_everyone_else_through() {
  reset();
  CHECK(detours.count(map::Network_GetPlayerFromPhotonPlayer.key) == 0);  // reached through the spot lookup only
  CHECK(spot_lookup(&pGhost) == &sGhost);  // in the list: the game's own answer
  pm::outlist::take_out(2);
  CHECK(spot_lookup(&pGhost) == &sGhost);  // out: the same answer
  CHECK(spot_lookup(&pBob) == &sBob);
  CHECK(spot_lookup(&pWisher) == &sWisher);
  CHECK(spot_lookup(&pGone) == nullptr);  // a departed player: no spot, as the game answers
  CHECK(spot_lookup(nullptr) == nullptr);
  Photon twin{nullptr, 2};  // the same number, another Photon player: compared by reference, as the game does
  CHECK(spot_lookup(&twin) == nullptr);
  pm::outlist::put_back();
  CHECK(spot_lookup(&pGhost) == &sGhost);  // back in the list: the game's own answer again
}

// Every caller of Network.GetPlayerFromPhotonPlayer gets the Player it got with the spot in the list, for every
// Photon player the game can pass: the room's, a departed one, null and a look-alike.
void every_player_lookup_answers_as_with_the_spot_in() {
  reset();
  Photon twin{nullptr, 2};
  void* const photons[] = {&pWisher, &pGhost, &pAlice, &pBob, &pGone, nullptr, &twin};
  std::vector<void*> before;
  for (void* p : photons) before.push_back(lookup(p));
  CHECK(before[1] == &ghostBody);
  CHECK(before[4] == &alice);  // no spot: the game's fallback, the local investigator
  pm::outlist::take_out(2);
  for (size_t i = 0; i < before.size(); ++i) CHECK(lookup(photons[i]) == before[i]);
}

// The Winter's Jest present lands where a walk over the investigators alone puts it, though the parked body stands
// where it would change the answer.
void the_present_is_placed_as_over_the_investigators_alone() {
  reset();
  pm::outlist::take_out(2);
  const auto call = [] {
    return reinterpret_cast<phasmo::Vector3 (*)(void*, const void*)>(
        detours.at(map::KrampusPresent_GetFurthestSpawnPositionFromPlayers.key))(&network, nullptr);
  };
  const float investigatorsOnly = furthest_spawn({&wisher, &alice, &bob});
  CHECK(investigatorsOnly == 90.0f);
  CHECK(furthest_spawn({&wisher, &ghostBody, &alice, &bob}) == 50.0f);  // the control: the body would change it
  CHECK(call().x == investigatorsOnly);
  CHECK(lookup(&pGhost) == &ghostBody);  // past the search, the parked body again
  // With no local investigator, it finds no null: the search would throw on one.
  put<void*>(network, kLocal, nullptr);
  CHECK(call().x == investigatorsOnly);
  CHECK(searchSawNull == false);
  // Investigators who are gone from the walk are not stood in for.
  room = {&pGhost, &pBob};
  CHECK(call().x == furthest_spawn({&bob}));
  // Alone in the room: the parked body, never null.
  room = {&pGhost};
  CHECK(call().x == furthest_spawn({&ghostBody}));
  CHECK(searchSawNull == false);
}

// While a spot is held, -1 is that spot's position and does not reach the characters; any other position does, and
// with nothing held -1 is the game's own.
void the_body_type_ignores_the_held_spots_position() {
  reset();
  auto body_type = [](int32_t position) {
    reinterpret_cast<void (*)(void*, int32_t, const void*)>(detours.at(map::PlayerCharacter_SetPlayerBodyType.key))(
        &alice, position, nullptr);
  };
  body_type(-1);
  CHECK((bodyTypes == std::vector<int32_t>{-1}));  // the customisation screen's, nothing held
  pm::outlist::take_out(2);
  body_type(-1);
  CHECK((bodyTypes == std::vector<int32_t>{-1}));
  body_type(1);
  CHECK((bodyTypes == std::vector<int32_t>{-1, 1}));
  pm::outlist::put_back();
  body_type(-1);
  CHECK((bodyTypes == std::vector<int32_t>{-1, 1, -1}));
}

// The ghost player leaving: the game's departure finds their spot and runs, and nothing is held after.
void the_ghost_players_departure_is_the_games() {
  reset();
  pm::outlist::take_out(2);
  leaves(pGhost);
  CHECK((events == std::vector<std::string>{"departure 2"}));
  CHECK((order() == Names{"wisher", "alice", "bob"}));
  CHECK(pm::outlist::actor() == 0);
  CHECK(lookup(&pGhost) == &alice);  // as for any departed player
  pm::outlist::put_back();
  CHECK(order().size() == 3);
  // An investigator leaving is the game's alone, the ghost player's spot staying out.
  reset();
  pm::outlist::take_out(2);
  leaves(pBob);
  CHECK((events == std::vector<std::string>{"departure 4"}));
  CHECK((order() == Names{"wisher", "alice"}));
  CHECK(pm::outlist::actor() == 2);
}

// Back in the list where the game's sort puts it, once, while this client is in a room.
void the_spot_comes_back_in_actor_order() {
  reset();
  pm::outlist::take_out(2);
  pm::outlist::put_back();
  CHECK((order() == Names{"wisher", "body", "alice", "bob"}));
  CHECK(pm::outlist::actor() == 0);
  pm::outlist::put_back();
  CHECK(order().size() == 4);
  // Already back in the list by the time it is put back: not twice.
  reset();
  pm::outlist::take_out(2);
  fake::list_insert(spotList, 1, &sGhost);
  pm::outlist::put_back();
  CHECK((order() == Names{"wisher", "body", "alice", "bob"}));
  // The last place, and the first.
  reset();
  put<void*>(sGhost, kPhoton, &pGone);  // actor 5
  CHECK(pm::outlist::take_out(5));
  pm::outlist::put_back();
  CHECK((order() == Names{"wisher", "alice", "bob", "body"}));
  reset();
  pm::outlist::take_out(1);
  pm::outlist::put_back();
  CHECK((order() == Names{"wisher", "body", "alice", "bob"}));
}

// Out of the room the game has cleared its list (Network.OnLeftRoom): the spot is let go, not put back.
void outside_a_room_the_spot_is_let_go() {
  reset();
  pm::outlist::take_out(2);
  fake::list_remove_at(spotList, 0);
  fake::list_remove_at(spotList, 0);
  fake::list_remove_at(spotList, 0);
  inRoom = false;
  pm::outlist::put_back();
  CHECK(order().empty());
  CHECK(pm::outlist::actor() == 0);
}

// Another actor's spot taken out puts the first back.
void a_new_ghost_player_puts_the_old_spot_back() {
  reset();
  pm::outlist::take_out(2);
  pm::outlist::take_out(4);
  CHECK((order() == Names{"wisher", "body", "alice"}));
  CHECK(pm::outlist::actor() == 4);
}

}  // namespace

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

int main() {
  the_spot_goes_out_and_the_others_keep_their_order();
  the_paw_with_nobody_dead_keeps_its_use();
  the_paw_revives_the_dead_investigator();
  the_high_priestess_revives_an_investigator_or_sets_its_flag();
  the_all_dead_test_counts_investigators_only();
  the_spot_lookup_finds_the_held_spot_and_passes_everyone_else_through();
  every_player_lookup_answers_as_with_the_spot_in();
  the_present_is_placed_as_over_the_investigators_alone();
  the_body_type_ignores_the_held_spots_position();
  the_ghost_players_departure_is_the_games();
  the_spot_comes_back_in_actor_order();
  outside_a_room_the_spot_is_let_go();
  a_new_ghost_player_puts_the_old_spot_back();
  pm::outlist::put_back();  // nothing held past the fake game
  std::printf("outlist: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
