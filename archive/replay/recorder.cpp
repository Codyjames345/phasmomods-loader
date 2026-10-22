#include "recorder.h"

#include <windows.h>

#include <vector>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "services.h"

namespace pg::replay::recorder {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kNetworkSpots = 0x38;       // Network: List<PlayerSpot>
constexpr size_t kSpotPhotonPlayer = 0x18;   // PlayerSpot: Photon.Realtime.Player
constexpr size_t kSpotPlayer = 0x38;         // PlayerSpot: networked Player
constexpr size_t kPlayerDead = 0x28;         // Player: dead
constexpr size_t kLevelGhostRoom = 0x38;     // LevelController: the room the ghost is in
constexpr size_t kLevelRooms = 0x50;         // LevelController: LevelRoom[]
constexpr size_t kRoomFloor = 0x68;          // LevelRoom: its floor
constexpr int kStateHunting = 2, kStateEvent = 14;
constexpr uintptr_t kCrucifixRva = 0x5DC690; // Crucifix.CrucifixUseNetworked(bool, PhotonMessageInfo)
constexpr size_t kMaxFrames = 2 * 60 * 90;   // 90 minutes
constexpr int kChunksPerFrame = 4;

// Messages on this mod's channel: 'R' 'P' version type, then:
//   CHUNK  id:u16 index:u16 count:u16 bytes   (the host to all: a piece of the replay blob)
constexpr uint8_t kVersion = 1;
constexpr uint8_t kChunk = 1;

using Crucifix_t = void (*)(void*, bool, void*, const void*);
Crucifix_t g_crucifix = nullptr;

struct State {
  bool recording = false;
  ULONGLONG start = 0, next = 0;
  Recording rec;
  std::vector<Box> rooms;
  int ghostActor = 0;
  std::vector<std::vector<uint8_t>> outgoing;  // chunks still to send, once back in the lobby
  size_t outgoingNext = 0;
  uint16_t blobId = 1;
  Assembler assembler;
  std::vector<Replay> ready;
} g;

Vector3 where(void* component) { return game::position(game::transform_of(component)); }
Point point(Vector3 v) { return {v.x, v.y, v.z}; }

void snapshot_rooms(void* level) {
  g.rooms.clear();
  auto* rooms = il2cpp::field<il2cpp::Array*>(level, kLevelRooms);
  for (uintptr_t i = 0; rooms && i < rooms->max_length; ++i) {
    void* room = rooms->data<void*>()[i];
    if (!room || !game::alive(room)) continue;
    const int8_t floor = static_cast<int8_t>(il2cpp::field<int>(room, kRoomFloor));
    for (const game::Bounds& b : game::room_bounds(room))
      g.rooms.push_back({floor, b.center.x - b.extents.x, b.center.z - b.extents.z, b.center.x + b.extents.x, b.center.z + b.extents.z});
  }
}

int slot_of(int actor) {
  for (size_t i = 0; i < g.rec.actors.size(); ++i)
    if (g.rec.actors[i] == actor) return static_cast<int>(i);
  if (g.rec.actors.size() >= static_cast<size_t>(kMaxPlayers)) return -1;
  g.rec.actors.push_back(actor);
  return static_cast<int>(g.rec.actors.size()) - 1;
}

void record(void* level) {
  Frame f;
  void* ghost = game::ghost_ai();
  if (ghost && game::alive(ghost)) {
    f.flags |= kGhostPresent;
    f.ghost = point(where(ghost));
    const int state = game::ghost_state(ghost);
    if (state == kStateHunting) f.flags |= kHunting;
    if (state == kStateEvent) f.flags |= kEvent;
    void* room = il2cpp::field<void*>(level, kLevelGhostRoom);
    if (room && game::alive(room)) f.floor = static_cast<int8_t>(il2cpp::field<int>(room, kRoomFloor));
  }
  const auto* pg = static_cast<const PgPlayableGhostService*>(mod::host().service(PG_SERVICE_PLAYABLE_GHOST));
  if (pg && pg->version >= 2 && pg->ghostActor) {
    if (const int actor = pg->ghostActor()) g.ghostActor = actor;  // the lobby draws the next one: keep this one's
  }
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    if (!player || !game::alive(player)) continue;
    const int actor = game::actor_number(il2cpp::field<void*>(spot, kSpotPhotonPlayer));
    // The ghost player's investigator is parked out of play in the truck (playable_ghost body.h): not
    // an investigator in the replay, neither present nor killed.
    if (actor == g.ghostActor) continue;
    const int s = slot_of(actor);
    if (s < 0) continue;
    f.present |= static_cast<uint8_t>(1 << s);
    if (il2cpp::field<bool>(player, kPlayerDead)) f.dead |= static_cast<uint8_t>(1 << s);
    f.players[s] = point(where(player));
  }
  if (g.rec.frames.size() < kMaxFrames) g.rec.frames.push_back(f);
}

void finish() {
  g.recording = false;
  const Replay replay = make_replay(g.rec, g.ghostActor, g.rooms);
  PG_LOG("replay: contract over, %zu frames, %zu moments", g.rec.frames.size(), replay.moments.size());
  if (!replay.moments.empty()) {
    g.outgoing = chunks(encode(replay));
    g.outgoingNext = 0;
    ++g.blobId;
    g.ready.push_back(replay);  // the host sees it too
  }
  g.rec = Recording{};
  g.rooms.clear();
  g.ghostActor = 0;
}

void flush() {
  if (g.outgoingNext >= g.outgoing.size() || !game::in_room()) return;
  for (int n = 0; n < kChunksPerFrame && g.outgoingNext < g.outgoing.size(); ++n, ++g.outgoingNext) {
    const std::vector<uint8_t>& c = g.outgoing[g.outgoingNext];
    std::vector<uint8_t> b = {'R', 'P', kVersion, kChunk};
    const uint16_t index = static_cast<uint16_t>(g.outgoingNext), count = static_cast<uint16_t>(g.outgoing.size());
    b.insert(b.end(), reinterpret_cast<const uint8_t*>(&g.blobId), reinterpret_cast<const uint8_t*>(&g.blobId) + 2);
    b.insert(b.end(), reinterpret_cast<const uint8_t*>(&index), reinterpret_cast<const uint8_t*>(&index) + 2);
    b.insert(b.end(), reinterpret_cast<const uint8_t*>(&count), reinterpret_cast<const uint8_t*>(&count) + 2);
    b.insert(b.end(), c.begin(), c.end());
    net::send(b.data(), b.size(), net::To::Others, true);
  }
  if (g.outgoingNext >= g.outgoing.size()) g.outgoing.clear(), g.outgoingNext = 0;
}

void detour_crucifix(void* self, bool flag, void* info, const void* method) {
  hooks::Guard guard;
  g_crucifix(self, flag, info, method);
  if (g.recording) g.rec.crucifixes.push_back(static_cast<uint32_t>(GetTickCount64() - g.start));
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kCrucifixRva), reinterpret_cast<void*>(&detour_crucifix),
                       reinterpret_cast<void**>(&g_crucifix), "Crucifix.CrucifixUseNetworked");
}

void tick() {
  const ULONGLONG now = GetTickCount64();
  void* level = game::level_controller();
  const bool inContract = level && game::alive(level) && game::private_room();
  if (inContract && game::is_master()) {
    if (!g.recording) {
      g.recording = true;
      g.rec = Recording{};
      g.start = g.next = now;
      snapshot_rooms(level);
      PG_LOG("replay: recording the contract (%zu room outlines)", g.rooms.size());
    }
    if (now >= g.next) {
      g.next += kStepMs;
      if (now > g.next + 2 * kStepMs) g.next = now + kStepMs;  // after a long hitch: no burst of frames
      record(level);
    }
  } else if (g.recording && !inContract) {
    finish();
  } else if (g.recording) {
    g.recording = false;  // no longer the host: the new host has no full recording either
    g.rec = Recording{};
  }
  if (!level) flush();
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (size < 10 || data[0] != 'R' || data[1] != 'P' || data[2] != kVersion || data[3] != kChunk) return;
  if (sender != game::master_actor()) return;  // only the host records
  uint16_t id, index, count;
  std::memcpy(&id, data + 4, 2);
  std::memcpy(&index, data + 6, 2);
  std::memcpy(&count, data + 8, 2);
  std::vector<uint8_t> blob;
  if (!g.assembler.add(id, index, count, data + 10, size - 10, blob)) return;
  Replay r;
  if (decode(blob.data(), blob.size(), r)) g.ready.push_back(std::move(r));
  else PG_LOG("replay: a replay from the host could not be read");
}

bool take(Replay& out) {
  if (g.ready.empty()) return false;
  out = std::move(g.ready.front());
  g.ready.erase(g.ready.begin());
  return true;
}

void reset() { g = State{}; }

}  // namespace pg::replay::recorder
