#include "round.h"

#include <windows.h>

#include <intrin.h>

#include <algorithm>
#include <random>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "notice.h"

#pragma intrinsic(_ReturnAddress)

namespace pg::round {
namespace {

// GameAssembly build 25811160 (verified at load). Obfuscated methods are taken by address; the
// ranges are used to recognise calls made from inside them.
struct Range {
  uintptr_t rva, length;
};
constexpr Range kSpawn = {0x22B4050, 0x540};           // GhostController: host ghost setup
constexpr uintptr_t kPickTypeRva = 0x22B6BF0;          // GhostController: weighted GhostType roll
constexpr uintptr_t kRandomIndexRva = 0x2F02000;       // static int RandomIndex<T>(T[]), shared
constexpr Range kNativeRoomChange = {0x21A5B30, 0x21BA7B0 + 0x214 - 0x21A5B30};  // ghost state exits
constexpr uintptr_t kLocalisedValueRva = 0x8D1EF0;     // LocalisationSystem.GetLocalisedValue(string, bool)
constexpr Range kTruckNetworkedUse = {0x122C3B0, 0x1F4};  // LiftButton.NetworkedUse: first use warps the ghost
constexpr uintptr_t kTruckButtonRva = 0x122C0E0;       // LiftButton.AttemptUse(PhotonMessageInfo), host RPC

// Field offsets (build 25811160).
constexpr size_t kControllerLevel = 0x78;       // GhostController.levelController
constexpr size_t kControllerCollection = 0x90;  // GhostController.allGhostsCollection
constexpr size_t kControllerType = 0x28;        // GhostController traits: GhostType
constexpr size_t kCollectionGhosts = 0x18;      // GhostCollection.ghosts (List<GhostData>)
constexpr size_t kGhostDataType = 0x18;
constexpr size_t kGhostDataNameKey = 0x20;
constexpr size_t kLevelGhostRoom = 0x38;        // LevelController: room the ghost is in
constexpr size_t kLevelRooms = 0x50;            // LevelController: favourite-room candidates
constexpr size_t kRoomName = 0x70;              // LevelRoom name
constexpr size_t kGhostInfo = 0x38;             // GhostAI.GhostInfo
constexpr size_t kInfoFavouriteRoom = 0x70;     // GhostInfo favourite LevelRoom
constexpr size_t kLevelValuesDifficulty = 0x38;
constexpr size_t kDifficultyTypeOverride = 0x7C;
constexpr int kNoTypeOverride = 24;             // GhostType.None
constexpr size_t kTruckDoorsClosed = 0x55;      // LiftButton.isClosed
constexpr size_t kStatsRoomChanges = 0x98;      // LevelStats: favourite-room changes (one per change)

struct DictEntry {  // Dictionary<GhostType, int>.Entry
  int hashCode, next, key, value;
};

struct PhotonMessageInfo {  // Photon.Pun.PhotonMessageInfo, passed by reference to RPC methods
  int timeInt;
  void* sender;  // Photon.Realtime.Player
  void* photonView;
};

using Spawn_t = void (*)(void*, const void*);
using PickType_t = int (*)(void*, const void*);
using RandomIndex_t = int (*)(il2cpp::Array*, const void*);
using ChangeRoom_t = void (*)(void*, void*, const void*);
using TruckButton_t = void (*)(void*, PhotonMessageInfo*, const void*);
Spawn_t g_spawn = nullptr;
PickType_t g_pickType = nullptr;
RandomIndex_t g_randomIndex = nullptr;
ChangeRoom_t g_changeRoom = nullptr;
TruckButton_t g_truckButton = nullptr;
using Warp_t = void (*)(void*, const void*);
Warp_t g_warp = nullptr;  // GhostController.WarpGhostToFavouriteRoom (original), when bound

struct Api {
  il2cpp::Class* ghostController = nullptr;
  il2cpp::Class* levelValues = nullptr;
  il2cpp::String* (*localised)(il2cpp::String*, bool, const void*) = nullptr;
  void* (*levelStats)(const void*) = nullptr;
  void (*warpToFavouriteRoom)(void*, const void*) = nullptr;  // GhostController, master only
} api;

enum class Phase { Idle, Picking, Spawned };

session::Session* g_session = nullptr;
std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};
Phase g_phase = Phase::Idle;
il2cpp::Held g_heldController;  // GhostController whose spawn is held
uint64_t g_pickDeadline = 0;
bool g_typeFixed = false;

// Spawn probe and forced rolls (host, inside the native spawn).
bool g_probing = false;
std::vector<uint8_t> g_probeTypes;
int g_forcedType = -1;
int g_forcedRoom = -1;
bool g_resuming = false;

// Favourite-room claim window (host).
il2cpp::Held g_roomController;
uint64_t g_roomDeadline = 0;

int random_below(int n) { return std::uniform_int_distribution<int>(0, n - 1)(g_random); }

il2cpp::Array* level_rooms(void* levelController) {
  return levelController ? il2cpp::field<il2cpp::Array*>(levelController, kLevelRooms) : nullptr;
}

int room_index(il2cpp::Array* rooms, void* room) {
  for (uintptr_t i = 0; rooms && room && i < rooms->max_length; ++i) {
    if (rooms->data<void*>()[i] == room) return static_cast<int>(i);
  }
  return -1;
}

int forced_type_override() {
  void* values = il2cpp::static_field_by_type(api.levelValues, "LevelValues");
  void* difficulty = values ? il2cpp::field<void*>(values, kLevelValuesDifficulty) : nullptr;
  const int type = difficulty ? il2cpp::field<int>(difficulty, kDifficultyTypeOverride) : kNoTypeOverride;
  return type == kNoTypeOverride ? -1 : type;
}

bool ghost_player_round() { return g_session && g_session->ghost_actor() > 0 && game::private_room(); }

unsigned seconds_left(uint64_t deadline, uint64_t now) {
  return deadline > now ? static_cast<unsigned>((deadline - now + 999) / 1000) : 0;
}

// The ghost types the native weighted roll can produce from this dictionary: those with a positive
// weight, or every key when no weight is positive (the roll's own fallback).
std::vector<uint8_t> allowed_types(void* dict) {
  std::vector<uint8_t> positive, all;
  auto* entries = il2cpp::field<il2cpp::Array*>(dict, 0x18);
  const int count = il2cpp::field<int>(dict, 0x20);
  for (int i = 0; entries && i < count && i < static_cast<int>(entries->max_length); ++i) {
    const DictEntry& e = entries->data<DictEntry>()[i];
    if (e.hashCode < 0 || e.key < 0 || e.key > 0xFE) continue;
    all.push_back(static_cast<uint8_t>(e.key));
    if (e.value > 0) positive.push_back(static_cast<uint8_t>(e.key));
  }
  return positive.empty() ? all : positive;
}

int detour_pick_type(void* dict, const void* method) {
  hooks::Guard guard;
  const int rolled = g_pickType(dict, method);  // always consume the native draw
  if (!game::in_function(_ReturnAddress(), kSpawn.rva, kSpawn.length) || !dict) return rolled;
  if (g_probing) {
    g_probeTypes = allowed_types(dict);
    // Unwind out of the native spawn before it writes or sends anything; caught in probe().
    il2cpp::raise_argument_null("PlayableGhost.probe");
  }
  if (g_forcedType >= 0) {
    const std::vector<uint8_t> allowed = allowed_types(dict);
    if (std::find(allowed.begin(), allowed.end(), g_forcedType) != allowed.end()) return g_forcedType;
    PG_LOG("round: chosen type %d is no longer allowed natively; native roll %d stands", g_forcedType, rolled);
  }
  return rolled;
}

int detour_random_index(il2cpp::Array* array, const void* method) {
  hooks::Guard guard;
  const int rolled = g_randomIndex(array, method);
  if (g_forcedRoom < 0 || !g_heldController || !game::in_function(_ReturnAddress(), kSpawn.rva, kSpawn.length))
    return rolled;
  il2cpp::Array* rooms = level_rooms(il2cpp::field<void*>(g_heldController, kControllerLevel));
  if (array == rooms && g_forcedRoom < static_cast<int>(array->max_length)) return g_forcedRoom;
  return rolled;
}

// Runs the native spawn until its type roll and returns the types that roll could produce. The
// probe is unwound by a managed exception thrown from the roll, before any state is written or any
// RPC sent. Empty when the spawn returned without reaching the roll (it then ran natively).
std::vector<uint8_t> probe(void* controller) {
  g_probeTypes.clear();
  g_probing = true;
  try {
    g_spawn(controller, nullptr);
  } catch (...) {
    // Expected: the probe's own exception, raised from detour_pick_type.
  }
  g_probing = false;
  return std::exchange(g_probeTypes, {});
}

void start_picking(void* controller) {
  std::vector<uint8_t> types = probe(controller);
  if (types.empty()) {
    PG_LOG("round: native spawn finished without a type roll; nothing to pick");
    g_phase = Phase::Spawned;
    return;
  }
  const int fixed = forced_type_override();
  il2cpp::Array* rooms = level_rooms(il2cpp::field<void*>(controller, kControllerLevel));
  session::PickOffer offer;
  offer.types = types;
  offer.fixedType = fixed >= 0 ? static_cast<uint8_t>(fixed) : session::kNoType;
  offer.rooms = static_cast<uint16_t>(rooms ? std::min<uintptr_t>(rooms->max_length, 0xFFFF) : 0);
  offer.deadline = GetTickCount64() + kPickMs;
  g_heldController = controller;
  g_pickDeadline = offer.deadline;
  g_typeFixed = fixed >= 0;
  g_phase = Phase::Picking;
  g_session->offer_pick(offer);
  g_session->set_lock(offer.deadline);
  PG_LOG("round: spawn held for actor %d (%zu types, %u rooms, type %s)", g_session->ghost_actor(), types.size(),
         offer.rooms, g_typeFixed ? "fixed by difficulty" : "chosen");
}

void resume_spawn(const session::Choice& c) {
  g_forcedType = g_typeFixed ? -1 : c.type;
  g_forcedRoom = c.room;
  g_resuming = true;
  g_spawn(g_heldController, nullptr);
  g_resuming = false;
  void* controller = g_heldController;
  PG_LOG("round: ghost spawned (type %d, chosen type %d room %d)", il2cpp::field<int>(g_heldController, kControllerType),
         g_forcedType, g_forcedRoom);
  g_forcedType = g_forcedRoom = -1;
  g_heldController = nullptr;
  g_phase = Phase::Spawned;
  g_session->close_pick();
  g_session->set_lock(0);
  // Vanilla leaves the new ghost at the level origin until the truck first opens, when it warps to
  // its favourite room. The ghost player is already playing, so start them there now.
  if (g_warp && game::alive(controller) && game::alive(game::ghost_ai())) {
    try {
      g_warp(controller, nullptr);
    } catch (...) {
      PG_LOG("round: warping the new ghost to its favourite room threw; it stays where it spawned");
    }
  }
}

// The truck's first use warps the ghost to its favourite room (vanilla). In a ghost-player round the
// ghost already started there and has since been moved by the ghost player, so leave it be.
void detour_warp(void* controller, const void* method) {
  hooks::Guard guard;
  if (ghost_player_round() && game::in_function(_ReturnAddress(), kTruckNetworkedUse.rva, kTruckNetworkedUse.length)) {
    PG_LOG("round: truck opened; the ghost player's ghost is not warped back to its favourite room");
    return;
  }
  g_warp(controller, method);
}

void detour_spawn(void* controller, const void* method) {
  hooks::Guard guard;
  if (g_resuming || g_probing || !g_session || !g_session->local_is_master() || !game::private_room()) {
    g_spawn(controller, method);
    return;
  }
  if (!g_session->in_round()) g_session->start_round();
  if (g_phase == Phase::Picking) return;  // already held; a second trigger is ignored
  if (g_phase == Phase::Idle && g_session->ghost_actor() > 0) {
    start_picking(controller);
    return;
  }
  g_spawn(controller, method);
}

void detour_change_room(void* controller, void* room, const void* method) {
  hooks::Guard guard;
  const bool fromGhostState = game::in_function(_ReturnAddress(), kNativeRoomChange.rva, kNativeRoomChange.length);
  const int before = favourite_room();
  g_changeRoom(controller, room, method);  // the change happens exactly as in vanilla
  if (!fromGhostState || !ghost_player_round() || !g_session->local_is_master() || g_phase != Phase::Spawned)
    return;
  // Another mod may keep the room (the Custom Ghosts mod's Draugr never changes it): nothing to claim.
  if (favourite_room() == before) return;
  // The native rules changed the favourite room; for a short while the ghost player may choose
  // another room instead.
  g_roomController = controller;
  g_roomDeadline = GetTickCount64() + kRoomClaimMs;
  g_session->offer_room(room_index(level_rooms(game::level_controller()), room), g_roomDeadline);
  PG_LOG("round: favourite room changed natively; the ghost player may claim another");
}

void close_room_window() {
  g_roomController = nullptr;
  g_session->close_room_offer();
}

// The claimed room replaces the native one through the same native change. That counts a second
// change in LevelStats, so the count is put back: the contract still had one change.
void claim_room(void* room) {
  void* controller = g_roomController;
  close_room_window();
  if (!game::alive(controller) || !game::alive(room)) return;
  const int before = favourite_room();
  g_changeRoom(controller, room, nullptr);
  if (favourite_room() == before) return;  // kept by another mod: no change was counted
  if (void* stats = api.levelStats(nullptr)) {
    il2cpp::set_field<int>(stats, kStatsRoomChanges, il2cpp::field<int>(stats, kStatsRoomChanges) - 1);
  }
}

// LiftButton.AttemptUse, run by the host for whoever pressed the truck's door button. While the
// ghost player is still choosing, the closed truck stays closed and the presser is told why.
void detour_truck_button(void* button, PhotonMessageInfo* info, const void* method) {
  hooks::Guard guard;
  const uint64_t now = GetTickCount64();
  const bool opening = il2cpp::field<bool>(button, kTruckDoorsClosed);
  if (opening && g_session && g_session->local_is_master() && g_session->locked(now)) {
    void* presser = info ? info->sender : nullptr;
    if (presser) g_session->notify_wait(game::actor_number(presser));
    return;
  }
  g_truckButton(button, info, method);
}

void tick_host(session::Session& s, uint64_t now) {
  if (!game::level_controller()) return;
  if (!s.in_round()) s.start_round();
  if (g_phase == Phase::Picking) {
    const session::Choice& c = s.choice();
    const bool chosen = (g_typeFixed || c.type >= 0) && c.room >= 0;
    if (chosen || now >= g_pickDeadline || s.ghost_actor() <= 0 || !game::alive(g_heldController)) {
      if (game::alive(g_heldController)) {
        resume_spawn(c);
      } else {
        g_phase = Phase::Idle;
        s.set_lock(0);
      }
    }
  }
  if (g_roomController) {
    int claimed = -1;
    il2cpp::Array* rooms = level_rooms(game::level_controller());
    while (s.take_room_claim(claimed)) {
      // Any room on the map (the ghost player's journal), as long as it is a new one.
      if (rooms && claimed >= 0 && static_cast<uintptr_t>(claimed) < rooms->max_length && claimed != favourite_room()) {
        PG_LOG("round: ghost player claimed room %d", claimed);
        claim_room(rooms->data<void*>()[claimed]);
        return;
      }
    }
    if (now >= g_roomDeadline || s.ghost_actor() <= 0) close_room_window();
  }
}

void reset_round() {
  g_phase = Phase::Idle;
  g_heldController = nullptr;
  g_roomController = nullptr;
  g_forcedType = g_forcedRoom = -1;
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  api.ghostController = il2cpp::cls(kGame, "", "GhostController");
  api.levelValues = il2cpp::cls(kGame, "", "LevelValues");
  api.localised = reinterpret_cast<decltype(api.localised)>(game::game_assembly_address(kLocalisedValueRva));
  api.levelStats = reinterpret_cast<decltype(api.levelStats)>(
      il2cpp::method_ptr(il2cpp::cls(kGame, "", "LevelStats"), "get_Instance", 0));
  api.warpToFavouriteRoom = reinterpret_cast<decltype(api.warpToFavouriteRoom)>(
      il2cpp::method_ptr(api.ghostController, "WarpGhostToFavouriteRoom", 0));
  const bool ok = api.ghostController && api.levelValues && api.localised && api.levelStats;
  PG_LOG("round: resolve %s%s", ok ? "ok" : "FAILED", api.warpToFavouriteRoom ? "" : " (no favourite-room warp: ghost starts where vanilla spawns it)");
  return ok;
}

bool create_hooks() {
  void* changeRoom = il2cpp::method_ptr(api.ghostController, "ChangeFavouriteRoomTo", 1);
  auto create = [](void* target, auto detour, auto& original, const char* name) {
    return hooks::create(target, reinterpret_cast<void*>(detour), reinterpret_cast<void**>(&original), name);
  };
  return create(game::game_assembly_address(kSpawn.rva), &detour_spawn, g_spawn, "GhostController.SpawnGhost") &&
         create(game::game_assembly_address(kPickTypeRva), &detour_pick_type, g_pickType, "GhostController.PickType") &&
         create(game::game_assembly_address(kRandomIndexRva), &detour_random_index, g_randomIndex, "RandomIndex<T[]>") &&
         create(changeRoom, &detour_change_room, g_changeRoom, "GhostController.ChangeFavouriteRoomTo") &&
         create(game::game_assembly_address(kTruckButtonRva), &detour_truck_button, g_truckButton, "LiftButton.AttemptUse") &&
         (!api.warpToFavouriteRoom ||
          create(reinterpret_cast<void*>(api.warpToFavouriteRoom), &detour_warp, g_warp, "GhostController.WarpGhostToFavouriteRoom"));
}

void tick(session::Session& s) {
  if (!g_session) s.set_random(random_below);
  g_session = &s;
  const uint64_t now = GetTickCount64();
  if (!game::level_controller()) {
    if (s.in_round()) s.end_round();
    if (g_phase != Phase::Idle) reset_round();
    return;
  }
  if (s.local_is_master() && game::private_room()) tick_host(s, now);
  if (s.take_wait_notice() && s.locked(now)) notice::show_wait(seconds_left(s.lock_deadline(), now));
  notice::tick_wait(seconds_left(s.lock_deadline(), now));
}

void teardown() {
  if (g_heldController && game::alive(g_heldController)) {
    g_spawn(g_heldController, nullptr);  // never leave a contract without its ghost
  }
  if (g_roomController) close_room_window();
  reset_round();
}

int room_count() {
  il2cpp::Array* rooms = level_rooms(game::level_controller());
  return rooms ? static_cast<int>(rooms->max_length) : 0;
}

void* room(int index) {
  il2cpp::Array* rooms = level_rooms(game::level_controller());
  return rooms && index >= 0 && static_cast<uintptr_t>(index) < rooms->max_length ? rooms->data<void*>()[index] : nullptr;
}

std::string room_name(int index) {
  il2cpp::Array* rooms = level_rooms(game::level_controller());
  if (!rooms || index < 0 || index >= static_cast<int>(rooms->max_length)) return {};
  void* room = rooms->data<void*>()[index];
  char buf[96];
  il2cpp::to_utf8(room ? il2cpp::field<il2cpp::String*>(room, kRoomName) : nullptr, buf, sizeof buf);
  return buf;
}

int current_ghost_room() {
  void* lc = game::level_controller();
  return lc ? room_index(level_rooms(lc), il2cpp::field<void*>(lc, kLevelGhostRoom)) : -1;
}

int favourite_room() {
  void* ghost = game::ghost_ai();
  void* info = ghost ? il2cpp::field<void*>(ghost, kGhostInfo) : nullptr;
  return info ? room_index(level_rooms(game::level_controller()), il2cpp::field<void*>(info, kInfoFavouriteRoom)) : -1;
}

bool ghost_in_favourite_room() {
  void* lc = game::level_controller();
  void* ghost = game::ghost_ai();
  void* info = ghost ? il2cpp::field<void*>(ghost, kGhostInfo) : nullptr;
  void* here = lc ? il2cpp::field<void*>(lc, kLevelGhostRoom) : nullptr;
  return info && game::alive(here) && here == il2cpp::field<void*>(info, kInfoFavouriteRoom);
}

std::string ghost_type_name(int type) {
  void* controller = il2cpp::static_field_by_type(api.ghostController, "GhostController");
  if (!game::alive(controller)) controller = nullptr;  // the static outlives its level
  void* collection = controller ? il2cpp::field<void*>(controller, kControllerCollection) : nullptr;
  auto* ghosts = collection ? il2cpp::field<il2cpp::List*>(collection, kCollectionGhosts) : nullptr;
  for (int i = 0; ghosts && ghosts->items && i < ghosts->size; ++i) {
    void* data = ghosts->items->data<void*>()[i];
    if (!data || il2cpp::field<int>(data, kGhostDataType) != type) continue;
    char buf[96];
    il2cpp::to_utf8(api.localised(il2cpp::field<il2cpp::String*>(data, kGhostDataNameKey), true, nullptr), buf, sizeof buf);
    return buf;
  }
  return "Ghost " + std::to_string(type);
}

}  // namespace pg::round
