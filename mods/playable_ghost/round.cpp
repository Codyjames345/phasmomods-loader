#include "round.h"

#include <windows.h>

#include <algorithm>
#include <iterator>
#include <random>
#include <utility>
#include <vector>

#include "Difficulty.h"
#include "GameController.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "GhostWanderState.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "LevelStats.h"
#include "LocalisationSystem.h"
#include "Photon.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "notice.h"
#include "runtime.h"
#include "text_table.h"

namespace pm::round {
namespace {

using phasmo::GhostController;
using phasmo::LevelController;
using phasmo::LiftButton;

enum class Phase { Idle, Picking, Spawned };

session::Session* g_session = nullptr;
std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};
Phase g_phase = Phase::Idle;
api::Held g_heldController;  // GhostController whose spawn is held
uint64_t g_pickDeadline = 0;
bool g_typeFixed = false;

// Spawn probe and forced rolls (host, inside the native spawn).
bool g_probing = false;
std::vector<uint8_t> g_probeTypes;
int g_forcedType = -1;
int g_forcedRoom = -1;
bool g_resuming = false;

// Favourite-room claim window (host).
api::Held g_roomController;
uint64_t g_roomDeadline = 0;

// The held spawn run through on purpose: detour_spawn lets it through while this lives, and an exception
// out of the spawn clears it as well as a return does.
struct Resuming {
  Resuming() { g_resuming = true; }
  ~Resuming() { g_resuming = false; }
  Resuming(const Resuming&) = delete;
  Resuming& operator=(const Resuming&) = delete;
};

// Runs the held spawn through, as the game's own spawn.
void run_held_spawn() {
  const Resuming resuming;
  GhostController::Spawn(g_heldController);
}

Clock g_clock = &GetTickCount64;

int random_below(int n) { return std::uniform_int_distribution<int>(0, n - 1)(g_random); }

api::RefArray level_rooms(void* levelController) {
  return api::RefArray{levelController ? LevelController::rooms.get(levelController) : nullptr};
}

int room_index(const api::RefArray& rooms, void* room) {
  for (size_t i = 0, n = rooms.size(); room && i < n; ++i) {
    if (rooms[i] == room) return static_cast<int>(i);
  }
  return -1;
}

int forced_type_override() {
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = values ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  const int none = phasmo::GhostType::None();
  const int type = difficulty ? phasmo::Difficulty::ghostTypeOverride.get(difficulty) : none;
  return type == none ? -1 : type;
}

bool ghost_player_round() { return g_session && g_session->ghost_actor() > 0 && game::private_room(); }

unsigned seconds_left(uint64_t deadline, uint64_t now) {
  return deadline > now ? static_cast<unsigned>((deadline - now + 999) / 1000) : 0;
}

// The ghost types the native weighted roll can produce from this dictionary: those with a positive
// weight, or every key when no weight is positive (the roll's own fallback).
std::vector<uint8_t> allowed_types(void* dict) {
  std::vector<uint8_t> positive, all;
  PmIntEntry entries[256];
  const size_t count = api::dictionary_entries(dict, entries, std::size(entries));
  for (size_t i = 0; i < count; ++i) {
    const PmIntEntry& e = entries[i];
    if (e.key < 0 || e.key >= session::kNoType) continue;
    all.push_back(static_cast<uint8_t>(e.key));
    if (e.value > 0) positive.push_back(static_cast<uint8_t>(e.key));
  }
  return positive.empty() ? all : positive;
}

int32_t detour_pick_type(api::Original<GhostController::PickType> original, void* dict) {
  const int32_t rolled = original(dict);  // always consume the native draw
  if (!api::inside<GhostController::Spawn>() || !dict) return rolled;
  if (g_probing) {
    g_probeTypes = allowed_types(dict);
    // Out of the native spawn before it writes or sends anything; caught in probe().
    GhostController::abandon_spawn();
  }
  if (g_forcedType >= 0) {
    const std::vector<uint8_t> allowed = allowed_types(dict);
    if (std::find(allowed.begin(), allowed.end(), g_forcedType) != allowed.end()) return g_forcedType;
    PM_LOG("round: chosen type %d is no longer allowed natively; native roll %d stands", g_forcedType, rolled);
  }
  return rolled;
}

// The spawn rolls the favourite room into the traits and reads it nowhere after; its CreateGhost RPC runs
// here at once and copies the traits into the ghost. Replacing the rolled index first gives the chosen
// room, with the native roll consumed as before.
void detour_create_ghost(api::Original<GhostController::CreateGhost> original, void* controller, bool flag,
                         int32_t model, void* info) {
  if (g_forcedRoom >= 0 && controller == g_heldController && api::inside<GhostController::Spawn>() &&
      static_cast<size_t>(g_forcedRoom) < level_rooms(GhostController::levelController.get(controller)).size())
    GhostController::favouriteRoomIndex.set(controller, g_forcedRoom);
  original(controller, flag, model, info);
}

// Runs the native spawn until its type roll and returns the types that roll could produce. The
// probe is unwound by the host from the roll, before any state is written or any RPC sent. Empty
// when the spawn returned without reaching the roll (it then ran natively).
std::vector<uint8_t> probe(void* controller) {
  g_probeTypes.clear();
  g_probing = true;
  try {
    GhostController::Spawn(controller);  // detour_spawn lets it through while g_probing
  } catch (...) {
    // Expected: the unwinding GhostController::abandon_spawn raises, from detour_pick_type.
  }
  g_probing = false;
  return std::exchange(g_probeTypes, {});
}

void start_picking(void* controller) {
  std::vector<uint8_t> types = probe(controller);
  if (types.empty()) {
    PM_LOG("round: native spawn finished without a type roll; nothing to pick");
    g_phase = Phase::Spawned;
    return;
  }
  const int fixed = forced_type_override();
  const api::RefArray rooms = level_rooms(GhostController::levelController.get(controller));
  session::PickOffer offer;
  offer.types = types;
  offer.fixedType = fixed >= 0 ? static_cast<uint8_t>(fixed) : session::kNoType;
  offer.rooms = static_cast<uint16_t>(std::min<size_t>(rooms.size(), 0xFFFF));  // pm:not-game-data: the message's field
  offer.deadline = g_clock() + kPickMs;
  g_heldController = controller;
  g_pickDeadline = offer.deadline;
  g_typeFixed = fixed >= 0;
  g_phase = Phase::Picking;
  g_session->offer_pick(offer);
  g_session->set_lock(offer.deadline);
  PM_LOG("round: spawn held for actor %d (%zu types, %u rooms, type %s)", g_session->ghost_actor(), types.size(),
         offer.rooms, g_typeFixed ? "fixed by difficulty" : "chosen");
}

void resume_spawn(const session::Choice& c) {
  g_forcedType = g_typeFixed ? -1 : c.type;
  g_forcedRoom = c.room;
  run_held_spawn();
  void* controller = g_heldController;
  PM_LOG("round: ghost spawned (type %d, chosen type %d room %d)", GhostController::ghostType.get(g_heldController),
         g_forcedType, g_forcedRoom);
  g_forcedType = g_forcedRoom = -1;
  g_heldController = nullptr;
  g_phase = Phase::Spawned;
  g_session->close_pick();
  g_session->set_lock(0);
  // Vanilla leaves the new ghost at the level origin until the truck first opens, when it warps to
  // its favourite room. The ghost player is already playing, so start them there now.
  if (game::alive(controller) && game::alive(game::ghost_ai())) {
    try {
      GhostController::WarpGhostToFavouriteRoom(controller);
    } catch (...) {
      PM_LOG("round: warping the new ghost to its favourite room threw; it stays where it spawned");
    }
  }
}

// The truck's first use warps the ghost to its favourite room (vanilla). In a ghost-player round the
// ghost already started there and has since been moved by the ghost player, so leave it be.
void detour_warp(api::Original<GhostController::WarpGhostToFavouriteRoom> original, void* controller) {
  if (ghost_player_round() && api::inside<LiftButton::NetworkedUse>()) {
    PM_LOG("round: truck opened; the ghost player's ghost is not warped back to its favourite room");
    return;
  }
  original(controller);
}

void detour_spawn(api::Original<GhostController::Spawn> original, void* controller) {
  if (g_resuming || g_probing || !g_session || !g_session->local_is_master() || !game::private_room()) {
    original(controller);
    return;
  }
  if (!g_session->in_round()) g_session->start_round();
  if (g_phase == Phase::Picking) return;  // already held; a second trigger is ignored
  if (g_phase == Phase::Idle && g_session->ghost_actor() > 0) {
    start_picking(controller);
    return;
  }
  original(controller);
}

void detour_change_room(api::Original<GhostController::ChangeFavouriteRoomTo> original, void* controller, void* room) {
  const bool fromGhostState = api::inside<phasmo::GhostWanderState::Exit>();
  const int before = favourite_room();
  original(controller, room);  // the change happens exactly as in vanilla
  if (!fromGhostState || !ghost_player_round() || !g_session->local_is_master() || g_phase != Phase::Spawned)
    return;
  // Another mod may keep the room (the Custom Ghosts mod's Draugr never changes it): nothing to claim.
  if (favourite_room() == before) return;
  // The native rules changed the favourite room; for a short while the ghost player may choose
  // another room instead.
  g_roomController = controller;
  g_roomDeadline = g_clock() + kRoomClaimMs;
  g_session->offer_room(room_index(level_rooms(game::level_controller()), room), g_roomDeadline);
  PM_LOG("round: favourite room changed natively; the ghost player may claim another");
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
  GhostController::ChangeFavouriteRoomTo(controller, room);
  if (favourite_room() == before) return;  // kept by another mod: no change was counted
  if (void* stats = phasmo::LevelStats::get_Instance()) {
    phasmo::LevelStats::roomsChanged.set(stats, phasmo::LevelStats::roomsChanged.get(stats) - 1);
  }
}

// LiftButton.AttemptUse, run by the host for whoever pressed the truck's door button. While the
// ghost player is still choosing, the closed truck stays closed and the presser is told why.
void detour_truck_button(api::Original<LiftButton::AttemptUse> original, void* button, void* info) {
  const uint64_t now = g_clock();
  const bool opening = LiftButton::isClosed.get(button);
  if (opening && g_session && g_session->local_is_master() && g_session->locked(now)) {
    void* presser = info ? phasmo::PhotonMessageInfo::Sender.get(info) : nullptr;
    if (presser) g_session->notify_wait(game::actor_number(presser));
    return;
  }
  original(button, info);
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
    const api::RefArray rooms = level_rooms(game::level_controller());
    while (s.take_room_claim(claimed)) {
      // Any room on the map (the ghost player's journal), as long as it is a new one.
      if (claimed >= 0 && static_cast<size_t>(claimed) < rooms.size() && claimed != favourite_room()) {
        PM_LOG("round: ghost player claimed room %d", claimed);
        claim_room(rooms[static_cast<size_t>(claimed)]);
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

bool create_hooks() {
  // The canonical bodies, as the raw hooks by address were.
  constexpr auto kOne = api::Twins::CanonicalOnly;
  return api::hook<GhostController::Spawn>(&detour_spawn, "GhostController.Spawn", kOne) &&
         api::hook<GhostController::PickType>(&detour_pick_type, "GhostController.PickType", kOne) &&
         api::hook<GhostController::CreateGhost>(&detour_create_ghost, "GhostController.CreateGhost", kOne) &&
         api::hook<GhostController::ChangeFavouriteRoomTo>(&detour_change_room, "GhostController.ChangeFavouriteRoomTo", kOne) &&
         api::hook<LiftButton::AttemptUse>(&detour_truck_button, "LiftButton.AttemptUse", kOne) &&
         api::hook<GhostController::WarpGhostToFavouriteRoom>(&detour_warp, "GhostController.WarpGhostToFavouriteRoom", kOne);
}

void tick(session::Session& s) {
  if (!g_session) s.set_random(random_below);
  g_session = &s;
  const uint64_t now = g_clock();
  s.set_contract(game::level_controller() != nullptr);  // every client, so a new master knows a contract runs
  if (!game::level_controller()) {
    if (s.in_round()) s.end_round();
    if (g_phase != Phase::Idle) reset_round();
    return;
  }
  if (s.local_is_master() && game::private_room()) tick_host(s, now);
  const unsigned left = seconds_left(s.lock_deadline(), now);
  const std::string wait = i18n::text("truck.wait", {{"time", "0:" + std::string(left < 10 ? "0" : "") + std::to_string(left)}});
  if (s.take_wait_notice() && s.locked(now)) notice::show_wait(left, i18n::text("truck.title"), wait);
  notice::tick_wait(left, wait);
}

void teardown() {
  if (g_heldController && game::alive(g_heldController)) {
    run_held_spawn();  // never leave a contract without its ghost
  }
  if (g_roomController) close_room_window();
  reset_round();
}

void set_clock(Clock clock) { g_clock = clock ? clock : &GetTickCount64; }

int room_count() { return static_cast<int>(level_rooms(game::level_controller()).size()); }

void* room(int index) {
  const api::RefArray rooms = level_rooms(game::level_controller());
  return index >= 0 && static_cast<size_t>(index) < rooms.size() ? rooms[static_cast<size_t>(index)] : nullptr;
}

std::string room_name(int index) {
  void* found = room(index);
  return found ? api::to_utf8(phasmo::LevelRoom::name.get(found)).substr(0, 95) : std::string{};
}

int current_ghost_room() {
  void* lc = game::level_controller();
  return lc ? room_index(level_rooms(lc), LevelController::currentGhostRoom.get(lc)) : -1;
}

int favourite_room() {
  void* ghost = game::ghost_ai();
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  return info ? room_index(level_rooms(game::level_controller()), phasmo::GhostInfo::favouriteRoom.get(info)) : -1;
}

bool ghost_in_favourite_room() {
  void* lc = game::level_controller();
  void* ghost = game::ghost_ai();
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  void* here = lc ? LevelController::currentGhostRoom.get(lc) : nullptr;
  return info && game::alive(here) && here == phasmo::GhostInfo::favouriteRoom.get(info);
}

std::string ghost_type_name(int type) {
  void* controller = GhostController::instance.get();
  if (!game::alive(controller)) controller = nullptr;  // the static outlives its level
  void* collection = controller ? GhostController::allGhostsCollection.get(controller) : nullptr;
  const api::List ghosts{collection ? phasmo::GhostCollection::ghosts.get(collection) : nullptr};
  for (size_t i = 0, n = ghosts.size(); i < n; ++i) {
    void* data = ghosts[i];
    if (!data || phasmo::GhostData::ghostType.get(data) != type) continue;
    return api::to_utf8(phasmo::LocalisationSystem::GetLocalisedValue(phasmo::GhostData::ghostNameLocalisationKey.get(data), true))
        .substr(0, 95);
  }
  return i18n::text("round.ghost-fallback", {{"type", std::to_string(type)}});
}

}  // namespace pm::round
