// Offline tests for wearing head-mounted gear through an emote (mods/emotes/gear.h): over the fake game, a worn
// head camera comes off the game's attach point onto the game's own point under the head bone while a body dances,
// a fresh mount mid-dance is found and ridden, a take off is left off until the game hangs gear out again, a dance
// that ends puts watched gear back under the parent the game had it under, and a body with no socket at all is left
// alone. The same rule runs for this client's own body and for every other dancer's, so both are watched side by
// side here. The game's side is in sdk/systems/players.md, "Head-mounted gear". Run: build\emotes_gear_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>

#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "gear.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace fake = pm::fake;
namespace gear = pm::emotes::gear;
namespace map = pm::map;

// The fake's own layout. A component and its Transform are one fake object, as elsewhere.
constexpr int32_t kSocketAttach = 0x10, kSocketSpot = 0x18, kSocketItem = 0x20;
constexpr int32_t kParent = 0x10;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}
template <class T>
const T& at(const void* o, int32_t offset) {
  return *reinterpret_cast<const T*>(static_cast<const unsigned char*>(o) + offset);
}

Obj g_socketClass;
Obj g_playerA, g_socketA, g_attachA, g_spotA, g_gearA;      // one dancer's body
Obj g_playerB, g_socketB, g_attachB, g_spotB, g_gearB;      // another's, on the same client

void* fake_self(void* o, void*) { return o; }
void* fake_get_parent(void* t, void*) { return at<void*>(t, kParent); }
void fake_set_parent(void* t, void* parent, bool, void*) { at<void*>(t, kParent) = parent; }
void* fake_in_children(void* c, void* type, bool, void*) {
  if (type != phasmo::NetworkedHeadMountedSocket::type.type_object()) return nullptr;
  if (c == &g_playerA) return &g_socketA;
  if (c == &g_playerB) return &g_socketB;
  return nullptr;
}

void dress(Obj& socket, Obj& attach, Obj& spot, Obj& gearItem) {
  at<void*>(&socket, kSocketAttach) = &attach;
  at<void*>(&socket, kSocketSpot) = &spot;
  at<void*>(&socket, kSocketItem) = &gearItem;
  at<void*>(&gearItem, kParent) = &attach;  // where the game's mount hung it on PC
}

void give() {
  fake::reset();
  for (Obj* o : {&g_playerA, &g_socketA, &g_attachA, &g_spotA, &g_gearA, &g_playerB, &g_socketB, &g_attachB,
                 &g_spotB, &g_gearB})
    for (int32_t b = 0; b < 0x40; ++b) o->bytes[b] = 0;
  g_socketClass.klass = nullptr;
  g_socketA.klass = &g_socketClass;
  g_socketB.klass = &g_socketClass;
  fake::type(map::NetworkedHeadMountedSocket, &g_socketClass);
  fake::field(map::NetworkedHeadMountedSocket_attachTransform, kSocketAttach);
  fake::field(map::NetworkedHeadMountedSocket_modelHeadSpot, kSocketSpot);
  fake::field(map::NetworkedHeadMountedSocket_mountedEquipment, kSocketItem);
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_self));
  fake::method(map::Transform_get_parent, reinterpret_cast<void*>(&fake_get_parent));
  fake::method(map::Transform_SetParent, reinterpret_cast<void*>(&fake_set_parent));
  fake::method(map::Component_GetComponentInChildren, reinterpret_cast<void*>(&fake_in_children));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  dress(g_socketA, g_attachA, g_spotA, g_gearA);
  dress(g_socketB, g_attachB, g_spotB, g_gearB);
}

void worn_gear_rides_the_head_through_the_dance() {
  give();
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);  // onto the head bone for the dance
  gear::follow(w);                                  // a frame of the dance: it stays put
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);  // back where the game had it
  CHECK(!w.item);                                     // the watch is spent
}

void a_fresh_mount_mid_dance_goes_back_onto_the_head() {
  give();
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  at<void*>(&g_gearA, kParent) = &g_attachA;  // the game mounted fresh gear during the dance
  gear::follow(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);
}

void gear_the_game_took_off_mid_dance_stays_off() {
  give();
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  at<void*>(&g_gearA, kParent) = nullptr;  // the un-hang: the parent goes to none
  gear::follow(w);
  CHECK(!w.item);                          // the watch forgets gear that came off
  CHECK(at<void*>(&g_gearA, kParent) == nullptr);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == nullptr);
}

void a_take_off_and_remount_mid_dance_ends_back_home() {
  give();
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  at<void*>(&g_gearA, kParent) = nullptr;  // taken off
  gear::follow(w);
  at<void*>(&g_gearA, kParent) = &g_attachA;  // and hung out again by the game
  gear::follow(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);
}

void a_dancer_wearing_nothing_is_still_watched() {
  give();
  at<void*>(&g_socketA, kSocketItem) = nullptr;
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));  // a body with a socket is watched even while it wears nothing
  CHECK(!w.item);
  gear::follow(w);  // a frame with nothing worn does nothing
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);  // the game's own hang is untouched
}

void a_mount_during_a_dance_is_found_and_ridden() {
  give();
  at<void*>(&g_socketA, kSocketItem) = nullptr;
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  at<void*>(&g_socketA, kSocketItem) = &g_gearA;  // the player mounts the head camera mid-dance
  at<void*>(&g_gearA, kParent) = &g_attachA;
  gear::follow(w);
  CHECK(w.item);
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);
}

void a_watcher_on_a_body_with_no_socket_does_nothing() {
  give();
  gear::Watch w;
  CHECK(!gear::begin(w, nullptr));  // a body that is gone
  gear::follow(w);
  gear::end(w);
}

void gear_already_on_the_head_never_moves() {
  give();
  at<void*>(&g_gearA, kParent) = &g_spotA;  // hung on the head bone to begin with (the other hang point)
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  gear::end(w);
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
}

void the_local_dancer_and_the_remote_one_are_watched_apart() {
  give();
  gear::Watch local, remote;
  CHECK(gear::begin(local, &g_playerA));
  CHECK(gear::begin(remote, &g_playerB));
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  CHECK(at<void*>(&g_gearB, kParent) == &g_spotB);
  at<void*>(&g_gearB, kParent) = &g_attachB;  // this dancer remounted; the other did not
  gear::follow(local);
  gear::follow(remote);
  CHECK(at<void*>(&g_gearA, kParent) == &g_spotA);
  CHECK(at<void*>(&g_gearB, kParent) == &g_spotB);
  gear::end(local);
  CHECK(at<void*>(&g_gearA, kParent) == &g_attachA);
  CHECK(at<void*>(&g_gearB, kParent) == &g_spotB);  // the other dance still runs
  gear::end(remote);
  CHECK(at<void*>(&g_gearB, kParent) == &g_attachB);
}

void a_gone_body_ends_watched_gear_back_home() {
  give();
  gear::Watch w;
  CHECK(gear::begin(w, &g_playerA));
  fake::destroy(&g_attachA);  // the body's hang point is gone (a torn-down body)
  gear::end(w);               // refuses to touch it, and spends the watch
  CHECK(!w.item);
}

}  // namespace

int main() {
  worn_gear_rides_the_head_through_the_dance();
  a_fresh_mount_mid_dance_goes_back_onto_the_head();
  gear_the_game_took_off_mid_dance_stays_off();
  a_take_off_and_remount_mid_dance_ends_back_home();
  a_dancer_wearing_nothing_is_still_watched();
  a_mount_during_a_dance_is_found_and_ridden();
  a_watcher_on_a_body_with_no_socket_does_nothing();
  gear_already_on_the_head_never_moves();
  the_local_dancer_and_the_remote_one_are_watched_apart();
  a_gone_body_ends_watched_gear_back_home();
  std::printf("%s\n", g_failures ? "emotes_gear tests FAILED" : "emotes_gear tests passed");
  return g_failures ? 1 : 0;
}
