#include "restraint.h"

#include <windows.h>

#include <random>

#include "echo.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "bogeyman.h"
#include "brownie.h"
#include "gremlin.h"
#include "kikimora.h"
#include "lithobolia.h"
#include "tempest.h"

namespace pg::custom::restraint {
namespace {

constexpr int kStateRandomEvent = 14;      // GhostAI.State: an event
constexpr int kStateThrowing = 6;          // GhostAI.State: throwing the prop it is given
constexpr int kStateHunting = 2;           // GhostAI.State: a hunt begins
constexpr int kStateAppear = 8;            // GhostAI.State: it shows itself (an event)
constexpr uintptr_t kTryThrowRva = 0x2701C50;  // Prop.TryThrow(bool, bool): the hunt state's throw (state 6 never reaches it)
// GhostActivity.GetDoorToOpen(): a door its door sensor (+0x20) detects, at random, never the main door
// (in a hunt only one marked at Door +0x82), or null. The idle roll's door chance (0x2007C60) then runs
// ChangeState(5) on its PhotonObjectInteract (Door +0x98).
constexpr uintptr_t kGetDoorToOpenRva = 0x2005FC0;
constexpr int kDormantSkipPercent = 75;    // the AI's interactions and events lost while the Echo is dormant

using TryInteract_t = void (*)(void*, const void*);
using ChangeState_t = void (*)(void*, int, void*, bool, const void*);
using TryThrow_t = void (*)(void*, bool, bool, const void*);
using GetDoorToOpen_t = void* (*)(void*, const void*);
TryInteract_t g_tryInteract = nullptr;
ChangeState_t g_changeState = nullptr;
TryThrow_t g_tryThrow = nullptr;
GetDoorToOpen_t g_getDoorToOpen = nullptr;

il2cpp::Held g_ghost;
int g_type = 0;
bool g_steered = false;  // a player controls the ghost
std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

/** The AI's attempt fails: always where the rule is absolute, 3 in 4 for a dormant Echo. */
bool skip(Kind kind) {
  if (!held_back(g_type, kind)) return false;
  return g_type != kEcho || std::uniform_int_distribution<int>(0, 99)(g_random) < kDormantSkipPercent;
}

void detour_try_interact(void* activity, const void* method) {
  hooks::Guard guard;
  if (g_ghost && activity == game::ghost_activity(g_ghost) && skip(kInteract)) return;
  // The Gremlin's interaction may be sabotage instead (the ghost is the host's: game::is_master).
  if (g_ghost && g_type == kGremlin && activity == game::ghost_activity(g_ghost) && game::is_master() &&
      gremlin::interact(g_ghost))
    return;
  g_tryInteract(activity, method);
}

// The door the AI's door interactions use (its idle roll's door chance; the hunt state asks too). Outside
// hunts the Bogeyman's AI on the host often takes a closet's door instead (bogeyman.h closet_door), and the
// game interacts with it as with any door it picks. A ghost player's aimed door stays theirs.
void* detour_get_door_to_open(void* activity, const void* method) {
  hooks::Guard guard;
  void* door = g_getDoorToOpen(activity, method);
  if (g_ghost && g_type == kBogeyman && activity == game::ghost_activity(g_ghost) && game::is_master() && !g_steered &&
      game::ghost_state(g_ghost) != kStateHunting)
    if (void* closet = bogeyman::closet_door(g_ghost)) return closet;
  return door;
}

void detour_change_state(void* ai, int state, void* target, bool flag, const void* method) {
  hooks::Guard guard;
  if (ai && ai == g_ghost && state == kStateRandomEvent && skip(kEvent)) return;
  if (ai && ai == g_ghost && g_type == kTempest && game::is_master() &&
      (state == kStateRandomEvent || state == kStateAppear))
    tempest::event_begins();  // lightning as each of its events begins
  // The Lithobolia throws at someone it can see instead (game::is_master: the ghost is the host's).
  if (ai && ai == g_ghost && g_type == kLithobolia && state == kStateThrowing && game::is_master() &&
      lithobolia::intercept(ai, target))
    return;
  // A hunt begins only from another state: the game ignores a second ChangeState(2) while it hunts.
  const bool bogeymanHunt = ai && ai == g_ghost && g_type == kBogeyman && state == kStateHunting && game::is_master() &&
                            !g_steered && game::ghost_state(ai) != kStateHunting;
  void* stateBefore = bogeymanHunt ? game::ghost_state_object(ai) : nullptr;
  g_changeState(ai, state, target, flag, method);
  // The Bogeyman's AI steps out of the closet nearest the youngest player, once the game has really made a
  // new hunt state (the difficulty may turn the hunt into idling, and a change it ignores keeps the old state).
  if (bogeymanHunt && game::ghost_state(ai) == kStateHunting && game::ghost_state_object(ai) != stateBefore)
    bogeyman::hunt_begins(ai);
}

/** The hunt's throw (the only Prop.TryThrow caller while a ghost hunts) flies at the Lithobolia's target. */
void detour_try_throw(void* prop, bool a, bool b, const void* method) {
  hooks::Guard guard;
  if (g_ghost && g_type == kLithobolia && game::is_master() && !g_steered &&
      game::ghost_state(g_ghost) == kStateHunting &&
      lithobolia::intercept_hunt_throw(g_ghost, prop))
    return;
  g_tryThrow(prop, a, b, method);
}

}  // namespace

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kTryThrowRva), reinterpret_cast<void*>(&detour_try_throw),
                       reinterpret_cast<void**>(&g_tryThrow), "Prop.TryThrow") &&
         hooks::create(game::try_interact(), reinterpret_cast<void*>(&detour_try_interact),
                       reinterpret_cast<void**>(&g_tryInteract), "GhostActivity.TryInteract") &&
         hooks::create(game::change_state(), reinterpret_cast<void*>(&detour_change_state),
                       reinterpret_cast<void**>(&g_changeState), "GhostAI.ChangeState") &&
         hooks::create(game::game_assembly_address(kGetDoorToOpenRva), reinterpret_cast<void*>(&detour_get_door_to_open),
                       reinterpret_cast<void**>(&g_getDoorToOpen), "GhostActivity.GetDoorToOpen");
}

void tick(void* ghost, int type, bool steered) {
  g_steered = steered;
  g_ghost = ghost;
  g_type = ghost ? type : 0;
}

bool held_back(int type, Kind kind) {
  if (!g_ghost || type != g_type) return false;
  if (type == kEcho) return echo::dormant_now();
  if (type == kKikimora) return kind == kInteract && kikimora::in_lit_room(g_ghost);
  if (type == kBrownie) return brownie::watched();  // never in view of an investigator or a camera
  return false;
}

}  // namespace pg::custom::restraint
