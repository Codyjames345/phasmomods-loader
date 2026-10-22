#include <windows.h>

#include <cstring>
#include <vector>

#include "check.h"
#include "game.h"

namespace {
char managerClass[] = "ServerManager";
char objectClass[] = "Object";
char profileClass[] = "MultiplayerProfile";
char spotClass[] = "PlayerSpot";  // Network/PlayerSpot, unavailable through a top-level class lookup.
char listClass[] = "List`1";
int photonField;
il2cpp::Object managerType{};
struct Node {
  il2cpp::Object object{};
  char fields[0x100]{};
  void* player = nullptr;
  int actor = 0;
};
Node sceneManager, firstCard, secondCard, firstSpot, secondSpot, firstPlayer, secondPlayer;
struct CardArray {
  il2cpp::Array array{};
  void* entries[2]{};
} cardArray;
il2cpp::List fixtureCards{};
bool fieldAvailable = true;
const char* missingMethod = "";
int topLevelSpotLookups = 0, fieldLookups = 0, kicks = 0, leaves = 0;
void* kickedCard = nullptr;
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}

/** Supply deterministic time for the production scene-search throttle. */
ULONGLONG WINAPI clock_ms() { return 1000; }
/** Return the fixture's manager through the production discovery boundary. */
void* find_manager(il2cpp::Object* type, const void*) {
  CHECK(type == &managerType);
  return &sceneManager;
}
/** Record only calls made with the manager resolved by production code. */
void leave_server(void* manager, const void*) {
  CHECK(manager == &sceneManager);
  ++leaves;
}
/** Record which ID card the production actor lookup selected. */
void kick_player(void* manager, void* card, const void*) {
  CHECK(manager == &sceneManager);
  ++kicks;
  kickedCard = card;
}
}

#define GetTickCount64 clock_ms
#include "../host/room_actions.cpp"
#undef GetTickCount64

namespace il2cpp {
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
/** Model top-level metadata without exposing the nested Network/PlayerSpot class. */
Class* cls(const char*, const char*, const char* name) {
  if (std::strcmp(name, "ServerManager") == 0) return managerClass;
  if (std::strcmp(name, "Object") == 0) return objectClass;
  if (std::strcmp(name, "PlayerSpot") == 0) ++topLevelSpotLookups;
  return nullptr;
}
/** Resolve only the manager type used by scene discovery. */
Object* type_object(Class* klass) { return klass == managerClass ? &managerType : nullptr; }
/** Bind the native entry points while allowing required-method failures. */
void* method_ptr(Class* klass, const char* name, int argc) {
  if (std::strcmp(name, missingMethod) == 0) return nullptr;
  if (klass == objectClass && std::strcmp(name, "FindObjectOfType") == 0 && argc == 1)
    return reinterpret_cast<void*>(&find_manager);
  if (klass == managerClass && std::strcmp(name, "LeaveServer") == 0 && argc == 0)
    return reinterpret_cast<void*>(&leave_server);
  if (klass == managerClass && std::strcmp(name, "KickPlayer") == 0 && argc == 1)
    return reinterpret_cast<void*>(&kick_player);
  return nullptr;
}
/** Read runtime class identity from the same object header as the real bindings. */
Class* class_of(const void* object) {
  return object ? static_cast<const Object*>(object)->klass : nullptr;
}
/** Reject null objects and incompatible offset contents before reading their fields. */
bool is_class(const void* object, const char* name) {
  auto* klass = class_of(object);
  return klass && std::strcmp(static_cast<const char*>(klass), name) == 0;
}
/** Expose photonPlayer only on the nested runtime class. */
Field* field_info(Class* klass, const char* name) {
  ++fieldLookups;
  return fieldAvailable && klass == spotClass && std::strcmp(name, "photonPlayer") == 0
      ? &photonField : nullptr;
}
/** Model the runtime field API's null-metadata guard. */
void* reference_field(void* object, Field* field) {
  return object && field == &photonField ? static_cast<Node*>(object)->player : nullptr;
}
/** Consume managed exception reports without an external log file. */
void log_exception(const char*, const void*) {}
}

namespace pg::game {
/** All fixture Unity objects remain alive for the duration of a case. */
bool alive(void* object) { return object != nullptr; }
/** Null references have no actor, matching the game's binding. */
int actor_number(void* player) { return player ? static_cast<Node*>(player)->actor : 0; }
}
namespace pg::log {
/** Keep offline tests independent of the user's game log. */
void write(const char*, ...) {}
}

namespace {
/** Reset production discovery state and create two cards with distinct actors. */
void reset() {
  pg::room_actions::api = {};
  pg::room_actions::g_manager = nullptr;
  pg::room_actions::g_nextSearch = 0;
  sceneManager = {}; firstCard = {}; secondCard = {};
  firstSpot = {}; secondSpot = {}; firstPlayer = {}; secondPlayer = {};
  sceneManager.object.klass = managerClass;
  firstCard.object.klass = secondCard.object.klass = profileClass;
  firstSpot.object.klass = secondSpot.object.klass = spotClass;
  firstPlayer.actor = 4;
  secondPlayer.actor = 9;
  firstSpot.player = &firstPlayer;
  secondSpot.player = &secondPlayer;
  fixtureCards = {};
  fixtureCards.obj.klass = listClass;
  fixtureCards.items = &cardArray.array;
  fixtureCards.size = 2;
  cardArray.array.max_length = 2;
  cardArray.entries[0] = &firstCard;
  cardArray.entries[1] = &secondCard;
  il2cpp::set_field(&sceneManager, 0xC8, &fixtureCards);
  il2cpp::set_field(&firstCard, 0x60, &firstSpot);
  il2cpp::set_field(&secondCard, 0x60, &secondSpot);
  fieldAvailable = true;
  missingMethod = "";
  topLevelSpotLookups = fieldLookups = kicks = leaves = 0;
  kickedCard = nullptr;
}
}

/** Exercise startup and actor selection through the real room-actions implementation. */
int main() {
  reset();
  CHECK(pg::room_actions::resolve());
  CHECK(topLevelSpotLookups == 0);
  CHECK(fieldLookups == 0);
  CHECK(pg::room_actions::kick(9));
  CHECK(kicks == 1 && kickedCard == &secondCard);
  CHECK(fieldLookups == 1);
  CHECK(!pg::room_actions::kick(27));
  CHECK(kicks == 1);
  CHECK(pg::room_actions::leave_lobby());
  CHECK(leaves == 1);

  reset();
  fieldAvailable = false;
  CHECK(pg::room_actions::resolve());
  CHECK(!pg::room_actions::kick(9));
  CHECK(!pg::room_actions::kick(0));
  CHECK(kicks == 0);

  reset();
  CHECK(pg::room_actions::resolve());
  firstSpot.object.klass = objectClass;
  secondSpot.object.klass = objectClass;
  CHECK(!pg::room_actions::kick(9));
  CHECK(kicks == 0 && fieldLookups == 0);

  reset();
  CHECK(pg::room_actions::resolve());
  firstSpot.player = secondSpot.player = nullptr;
  CHECK(!pg::room_actions::kick(9));
  CHECK(!pg::room_actions::kick(0));
  CHECK(kicks == 0);

  for (const char* method : {"FindObjectOfType", "LeaveServer", "KickPlayer"}) {
    reset();
    missingMethod = method;
    CHECK(!pg::room_actions::resolve());
  }

  reset();  // the ServerManager kept between frames stays reachable for the GC until it is dropped
  CHECK(pg::room_actions::resolve());
  CHECK(pg::room_actions::leave_lobby());
  CHECK(held(&sceneManager));
  reset();
  CHECK(!held(&sceneManager));
  return g_failures ? 1 : 0;
}
