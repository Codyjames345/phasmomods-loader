#include "restraint.h"

#include <windows.h>

#include <random>

#include "GhostAI.h"
#include "Props.h"
#include "bogeyman.h"
#include "bogeyman_rule.h"
#include "content.h"
#include "brownie.h"
#include "echo.h"
#include "echo_rule.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "gremlin.h"
#include "held.h"
#include "hydra.h"
#include "kikimora.h"
#include "lithobolia.h"
#include "runtime.h"
#include "squatter.h"
#include "squatter_rule.h"
#include "tempest.h"

namespace pm::custom::restraint {
namespace {

using phasmo::GhostAI;
using phasmo::GhostActivity;

api::Held g_ghost;
int g_type = 0;
bool g_repeating = false;  // the Echo's repeat is running: the state change it makes is never lost
// Set while the ghost's idle door chance runs and has not yet asked for its door: the next
// GetDoorToOpen is that chance's own pick (it calls GetDoorToOpen once, before it acts on the door).
bool g_doorChancePick = false;
std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

/** A roll in 0..99 for the rules that lose a share of the AI's attempts. */
int percent_roll() { return std::uniform_int_distribution<int>(0, 99)(g_random); }

/** The attempt fails: always where the rule is absolute, 3 in 4 for a dormant Echo (echo_rule.h). */
bool skip(Kind kind) {
  if (g_type == kEcho) return echo::dormant_now() && echo_dormant_loses(percent_roll());
  return held_back(g_type, kind);
}

/**
 * The Squatter refuses the change to `state` (squatter_rule.h keeps_quiet), by the running game's numbers, and only
 * idling while the mod steers its agent (squatter.h steering).
 */
bool squatter_refuses(int32_t state) {
  namespace GhostState = phasmo::GhostState;
  return squatter::keeps_quiet({GhostState::idle(), GhostState::wander(), GhostState::favouriteRoom()}, state,
                               squatter::steering());
}

void detour_try_interact(api::Original<GhostActivity::TryInteract> original, void* activity) {
  const bool own = g_ghost && activity == game::ghost_activity(g_ghost);
  // The Squatter interacts with nothing, whoever steers it: no roll is granted or tried.
  if (own && g_type == kSquatter) return;
  // A roll a player takes is granted to them (Playable Ghost): nothing is attempted yet, whichever hook runs
  // first. Their attempt comes later (detour_normal_interaction below).
  if (own && api::content::roll_taken()) return original(activity);
  if (own && g_type == kEcho) {
    // The Echo uses the switch or door a player just used in its room, once its sensor, pulsed where it
    // stands, detects it (the ghost is the host's: game::is_master), with no dormancy roll; otherwise a dormant
    // Echo loses 3 in 4.
    const bool repeated = echo_tries_repeat(true, game::is_master(), false) && echo::repeat(g_ghost);
    if (echo_roll(repeated, echo::dormant_now(), percent_roll) != EchoRoll::Game) return;
  } else if (own && skip(kInteract)) {
    return;
  }
  // The Gremlin's interaction may be sabotage instead (the ghost is the host's: game::is_master).
  if (own && g_type == kGremlin && game::is_master() && gremlin::interact(g_ghost)) return;
  original(activity);
}

// One pulse of the ghost's interaction sensor outside its roll: for the Echo, a ghost player's aimed
// interaction (Playable Ghost runs it through NormalInteraction) is the switch or door a player just used in
// its room, by the same repeat its AI's roll makes (echo.h repeat), once in reach; otherwise, and for a
// forced pulse (the voodoo doll's), the game's own.
void detour_normal_interaction(api::Original<GhostActivity::NormalInteraction> original, void* activity, bool force) {
  // No pulse of the Squatter's sensor: not its AI's, a player's aimed one, nor a voodoo pin's forced one, which
  // jumps straight here (sdk/systems/ghost-ai.md, "Every route to a state change").
  if (g_ghost && g_type == kSquatter && activity == game::ghost_activity(g_ghost)) return;
  const bool own = g_ghost && g_type == kEcho && activity == game::ghost_activity(g_ghost);
  if (echo_tries_repeat(own, game::is_master(), force)) {
    struct Repeating {  // cleared however the repeat ends, a managed exception from the game's call included
      Repeating() { g_repeating = true; }
      ~Repeating() { g_repeating = false; }
    } repeating;
    if (echo::repeat(g_ghost)) return;
  }
  original(activity, force);
}

// The ghost's idle door chance (GhostActivity.doorChance): its door pick is the next GetDoorToOpen.
bool detour_door_chance(api::Original<GhostActivity::doorChance> original, void* activity) {
  const bool outer = g_doorChancePick;
  g_doorChancePick = true;
  const bool acted = original(activity);
  g_doorChancePick = outer;
  return acted;
}

// The door the game's door pick returns. When it is the Bogeyman's idle door interaction on the host (the
// door chance's own pick, outside hunts) and the pick found a door, 3 in 4 times the ghost takes a closet
// door its door sensor detects instead (bogeyman.h closet_door), and the game interacts with that as with any
// door it picks. A ghost player's aimed door is the same door chance's pick, so the same rule turns it
// (bogeyman_rule.h bogeyman_turns_door). No pick, another caller (the hunt state's) or any other ghost: the
// game's own pick, unchanged.
void* detour_get_door_to_open(api::Original<GhostActivity::GetDoorToOpen> original, void* activity) {
  const bool chancePick = g_doorChancePick;
  g_doorChancePick = false;
  void* door = original(activity);
  const bool own = g_ghost && g_type == kBogeyman && activity == game::ghost_activity(g_ghost);
  if (bogeyman_turns_door(own, game::is_master(), chancePick, api::alive(door),
                          own && game::ghost_state(g_ghost) == phasmo::GhostState::hunting()))
    if (void* closet = bogeyman::closet_door(g_ghost)) return closet;
  return door;
}

/**
 * Whether the contract's ghost acts as the Bogeyman now, by the host's word: a Mimic's copy made inside the game's
 * state change shows at once, before this mod's next frame.
 */
bool acts_as_bogeyman() {
  const Ghost* bogeyman = find(kBogeyman);
  return bogeyman && api::content::id_of(api::content::acting_as()) == id_of(*bogeyman);
}

void detour_change_state(api::Original<GhostAI::ChangeState> original, void* ai, int32_t state, void* target,
                         bool flag) {
  const int event = phasmo::GhostState::randomEvent();
  const bool own = ai && ai == g_ghost;
  // The Squatter's refusal comes first, forced or not and whatever asks: its AI, a player, a cursed possession, a
  // tarot card, a phrase. On a friendly ghost's difficulty a forced hunt never reaches the game's change either, so
  // the difficulty is never asked to turn it into idling and no kill can follow.
  if (own && g_type == kSquatter && squatter_refuses(state)) return;
  // A roll a player takes only grants them the event: nothing begins yet, whichever hook runs first. Their chosen
  // event comes through here as the AI's does.
  const bool attempt = own && !api::content::roll_taken();
  if (attempt && state == event && skip(kEvent)) return;
  if (attempt && g_type == kTempest && game::is_master() && (state == event || state == phasmo::GhostState::appear()))
    tempest::event_begins();  // lightning as each of its events begins
  // The Lithobolia throws at someone it can see instead (game::is_master: the ghost is the host's).
  if (own && g_type == kLithobolia && state == phasmo::GhostState::throwing() && game::is_master() &&
      lithobolia::intercept(ai, target))
    return;
  // A hunt begins only from another state: the game ignores a second ChangeState(hunting) while it hunts.
  const int hunting = phasmo::GhostState::hunting();
  const bool toHunting = ai && state == hunting;
  const bool wasHunting = toHunting && game::ghost_state(ai) == hunting;
  void* stateBefore = toHunting ? game::ghost_state_object(ai) : nullptr;
  original(ai, state, target, flag);
  // The Bogeyman steps out of the closet nearest the youngest player, once the game has really made a new hunt
  // state: its AI's hunts and a player's alike (a ghost player's hunt is the same ChangeState, which Playable
  // Ghost sends through every mod's hooks). What it acts as is asked after the change, which a Mimic's copy may
  // have made at its top.
  if (toHunting && bogeyman_steps_out(acts_as_bogeyman(), game::is_master(), true, wasHunting,
                                      game::ghost_state(ai) == hunting &&
                                      static_cast<void*>(game::ghost_state_object(ai)) != stateBefore))
    bogeyman::hunt_begins(ai);
}

/**
 * The hunt's throw (Prop.TryThrow: the hunt's tick is its only caller while a ghost hunts, though the
 * Hydra's heads call it too) flies at the Lithobolia's target, and a Hydra's heads leave alone what the
 * ghost throws this tick (hydra.h ghost_threw: it tells the ghost's throws from the heads' own).
 */
void detour_try_throw(api::Original<phasmo::Prop::TryThrow> original, void* prop, bool a, bool b) {
  const int hunting = phasmo::GhostState::hunting();
  if (g_ghost && g_type == kHydra && game::is_master() && game::ghost_state(g_ghost) == hunting)
    hydra::ghost_threw(g_ghost, prop);
  if (g_ghost && g_type == kLithobolia && game::is_master() && game::ghost_state(g_ghost) == hunting &&
      lithobolia::intercept_hunt_throw(g_ghost, prop))
    return;
  original(prop, a, b);
}

}  // namespace

bool create_hooks() {
  return api::hook<phasmo::Prop::TryThrow>(&detour_try_throw, "Prop.TryThrow", api::Twins::CanonicalOnly) &&
         api::hook<GhostActivity::TryInteract>(&detour_try_interact, "GhostActivity.TryInteract",
                                              api::Twins::CanonicalOnly) &&
         api::hook<GhostAI::ChangeState>(&detour_change_state, "GhostAI.ChangeState", api::Twins::CanonicalOnly) &&
         api::hook<GhostActivity::GetDoorToOpen>(&detour_get_door_to_open, "GhostActivity.GetDoorToOpen",
                                                api::Twins::CanonicalOnly) &&
         api::hook<GhostActivity::doorChance>(&detour_door_chance, "GhostActivity.doorChance",
                                             api::Twins::CanonicalOnly) &&
         api::hook<GhostActivity::NormalInteraction>(&detour_normal_interaction, "GhostActivity.NormalInteraction",
                                                    api::Twins::CanonicalOnly);
}

void tick(void* ghost, int type) {
  g_ghost = ghost;
  g_type = ghost ? type : 0;
}

bool held_back(int type, Kind kind) {
  if (!g_ghost || type != g_type) return false;
  if (type == kSquatter) return true;  // every kind: it attempts nothing
  if (type == kEcho) return kind == kAbility && echo::dormant_now();
  if (type == kKikimora) return kind == kInteract && kikimora::in_lit_room(g_ghost);
  if (type == kBrownie) return brownie::watched();  // never in view of an investigator or a camera
  return false;
}

bool interaction_lost(int type) { return g_ghost && type == g_type && type == kEcho && !g_repeating && skip(kInteract); }

}  // namespace pm::custom::restraint
