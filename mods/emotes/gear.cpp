// The gear watch itself (gear.h). Everything here is a game call through the map; the rule it follows is in the
// header's comment and tested offline over the fake game (tests/emotes_gear_test.cpp).
#include "gear.h"

#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::emotes::gear {
namespace {

// The transform of a live component, or null.
void* transform_of(void* component) {
  return component && pm::api::alive(component) ? phasmo::Component::get_transform(component) : nullptr;
}

// The transform `item`'s transform hangs from, or null when it hangs from nothing or is gone.
void* hung_under(void* item) {
  void* t = transform_of(item);
  return t ? phasmo::Transform::get_parent(t) : nullptr;
}

// Hangs `item`'s transform under `parent`, keeping its local place and turn (Unity's worldPositionStays off).
void hang_under(void* item, void* parent) {
  if (void* t = transform_of(item)) phasmo::Transform::SetParent(t, parent, false);
}

}  // namespace

bool begin(Watch& watch, void* player) {
  watch = Watch{};
  void* socket = player && pm::api::alive(player)
                     ? phasmo::Component::GetComponentInChildren(player,
                                                                  phasmo::NetworkedHeadMountedSocket::type.type_object(), true)
                     : nullptr;
  if (!socket || !pm::api::alive(socket)) return false;
  void* spot = phasmo::NetworkedHeadMountedSocket::modelHeadSpot.get(socket);
  if (!spot || !pm::api::alive(spot)) return false;
  watch.socket = socket;
  watch.spot = spot;
  if (void* item = phasmo::NetworkedHeadMountedSocket::mountedEquipment.get(socket); item && pm::api::alive(item)) {
    if (void* parent = hung_under(item); parent) {
      watch.item = item;
      watch.original = parent;
      if (parent != spot) hang_under(item, spot);
    }
  }
  return true;
}

void follow(Watch& watch) {
  if (!watch.socket || !pm::api::alive(watch.socket.get()) || !watch.spot || !pm::api::alive(watch.spot.get()))
    return;
  void* item = watch.item ? watch.item.get() : phasmo::NetworkedHeadMountedSocket::mountedEquipment.get(watch.socket.get());
  if (!item || !pm::api::alive(item)) {
    watch.item = nullptr;  // worn nothing, or taken off since the last frame
    return;
  }
  void* parent = hung_under(item);
  if (!parent) {  // taken off: forget it, so a fresh mount (this piece or another) is found afresh
    watch.item = nullptr;
    watch.original = nullptr;
    return;
  }
  if (!watch.item) {  // gear this dance has not seen yet: a fresh mount. Remember where the game hung it.
    watch.item = item;
    watch.original = parent;
  }
  if (parent != watch.spot.get()) hang_under(item, watch.spot.get());
}

void end(Watch& watch) {
  if (watch.item && pm::api::alive(watch.item.get()) && watch.original && pm::api::alive(watch.original.get())) {
    if (void* parent = hung_under(watch.item.get()); parent && parent == watch.spot.get())
      hang_under(watch.item.get(), watch.original.get());
  }
  watch = Watch{};
}

}  // namespace pm::emotes::gear
