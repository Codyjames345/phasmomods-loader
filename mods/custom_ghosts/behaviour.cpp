#include "behaviour.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <vector>

#include "asterion.h"
#include "bogeyman.h"
#include "bogeyman_rule.h"
#include "brownie.h"
#include "brownie_rule.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "knell_rule.h"
#include "log.h"
#include "players.h"
#include "shuck_rule.h"
#include "speed_rule.h"
#include "tempest.h"
#include "tempest_rule.h"
#include "trail.h"

namespace pg::custom::behaviour {
namespace {

using il2cpp::bind;
using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kGhostInfo = 0x38;               // GhostAI.GhostInfo
constexpr size_t kInfoFavouriteRoom = 0x70;       // GhostInfo: favourite LevelRoom
constexpr size_t kHuntGhost = 0x10;               // hunt state: its GhostAI
constexpr size_t kHuntTarget = 0x48;              // hunt state: the player it has detected, or null
constexpr size_t kHuntSearched = 0x50;            // hunt state: the player it goes to look for (heard, hidden), or null
constexpr size_t kHuntLastPlayer = 0x58;          // hunt state: the player it last went for; it wanders near +0x78 while set
constexpr size_t kHuntSeen = 0x68;                // hunt state: the players it has seen (its end reads it for the escape objective)
constexpr size_t kHuntKilled = 0x70;              // hunt state: the players it has killed
constexpr size_t kHuntKillReach = 0xBC;           // hunt state: metres within which its kill takes a player at once (1.1)
constexpr size_t kHuntSlowKillReach = 0xC0;       // hunt state: metres within which it takes them after +0xC4 s (1.5)
constexpr size_t kHuntCloset = 0xD8;              // hunt state: the ClosetZone its closet search is for, or null
// Hunt state: seconds left of the hunt-start delay. Its Execute (0x213FE90) runs no chase, speed or kill
// while its gate (0x213F6F0) counts this down; the gate passes once it is below 0.
constexpr size_t kHuntStartDelay = 0x8C;
constexpr size_t kPlayerRoom = 0x50;              // Player: the LevelRoom they are in
constexpr size_t kGhostSmudge = 0xF8;             // GhostAI: the WhiteSage while smudged in a hunt
constexpr size_t kPlayerCloset = 0x30;            // Player: the ClosetZone they hide in, or null
constexpr size_t kPlayerSightParts = 0x80;        // Player: the components the hunt's sight test looks for (it throws on null)
constexpr size_t kSinkWater = 0x20;               // Sink: the water surface Transform its fill tween moves
constexpr size_t kSinkUsed = 0x74;                // Sink: set by its first Use, which shows the water and starts the fill
constexpr size_t kLevelValuesWeather = 0x98;      // LevelValues: the contract's weather settings
constexpr size_t kWeatherType = 0x20;             // those settings: weather type
constexpr int kRain = 2, kHeavyRain = 3;          // weather types
constexpr size_t kLevelOutsideRoom = 0x70;        // LevelController: the outside LevelRoom IsPointInOutsideRoom reads
constexpr size_t kRoomColliders = 0x30;           // LevelRoom: the colliders IsPointInOutsideRoom reads
constexpr size_t kGhostBaseSpeed = 0xC8;          // GhostAI: the 1.7 m/s GhostAI.Awake gives every ghost
constexpr size_t kHuntBase = 0xB0;                // hunt state: base speed (speed_rule.h OwnSpeed)
constexpr size_t kHuntSight = 0xB4;               // hunt state: line-of-sight speed-up
constexpr size_t kIdleGhost = 0x18;               // idle state: its GhostAI
constexpr size_t kIdleInsanity = 0x48;            // idle state: the insanity its hunt check uses
constexpr size_t kInteractRigidbody = 0x38;       // PhotonObjectInteract: Rigidbody (things that can be carried)
constexpr size_t kInteractDoor = 0x58;            // PhotonObjectInteract: Door (doors are not carried)
constexpr uintptr_t kRefreshBaseRva = 0x214C2A0;  // hunt state: copies the base from GhostAI +0xC8 every 2 s
constexpr uintptr_t kIdleInsanityRva = 0x215FFC0; // idle state: writes the hunt check's insanity
// The hunt's senses (hunt state methods). Every target the hunt sets or keeps passes the sight test,
// and so does its kill, which its chase makes only on a target that test sees.
constexpr uintptr_t kHuntSeesRva = 0x214BB40;      // bool(Player, bool skipDeogen): the vision sensor, GhostAI +0xA0
constexpr uintptr_t kHuntHearsRva = 0x214DDA0;     // bool(Player): PlayerAudio.IsTalkingLoudly within range
constexpr uintptr_t kHuntSensesGearRva = 0x21554D0;  // bool(Player): equipment switched on within range
constexpr uintptr_t kHuntSetTargetRva = 0x2150AB0;   // void(Player): its target, and the closet its search is for
constexpr uintptr_t kHuntSetClosetRva = 0x213FA10;   // void(ClosetZone): the closet its search is for (null: none)
// bool(Player): a player it may go for: alive, not in a room whose +0x58 is 1 (the truck), not killed
// already. It throws for a player with no room or a hunt with no killed set, so callers check both.
constexpr uintptr_t kHuntEligibleRva = 0x214FD80;
constexpr ULONGLONG kTrailStepMs = 500;
constexpr float kSameSink = 0.1f;                 // metres: a running sink this close to one of the last step's is the same
constexpr float kStandingStill = 0.01f;           // metres: a destination this close to the ghost stops it where it is
constexpr ULONGLONG kSurveyMs = 1000;
constexpr float kRoomMargin = 1.0f;               // metres outside a room's colliders still counted as in it

struct Api {
  il2cpp::Class* ghostController = nullptr;
  il2cpp::Object* interactType = nullptr;
  il2cpp::Object* saltType = nullptr;
  il2cpp::Object* sinkType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  bool (*isTweening)(void*, bool, const void*) = nullptr;  // DOTween.IsTweening(target, alsoCheckIfIsPlaying)
  bool (*isOutside)(void*, Vector3, const void*) = nullptr;  // LevelController.IsPointInOutsideRoom
  void (*setTarget)(void*, void*, const void*) = nullptr;    // the hunt's own set-target (kHuntSetTargetRva)
  void (*setCloset)(void*, void*, const void*) = nullptr;    // kHuntSetClosetRva
  bool (*eligible)(void*, void*, const void*) = nullptr;     // kHuntEligibleRva
  Vector3 (*velocity)(void*, const void*) = nullptr;
  // Icalls: this build strips the managed desiredVelocity and angularSpeed getters.
  void (*desiredVelocity)(void*, Vector3*) = nullptr;
  float (*angularSpeed)(void*) = nullptr;
  void (*setAngularSpeed)(void*, float, const void*) = nullptr;
  // The hunt's seen set's runtime class (a List of players) and its Contains and Add, found on first use.
  il2cpp::Class* seenClass = nullptr;
  const il2cpp::Method* seenContains = nullptr;
  const il2cpp::Method* seenAdd = nullptr;
} api;

using RefreshBase_t = void (*)(void*, const void*);
using IdleInsanity_t = void (*)(void*, const void*);
using ChangeRoom_t = void (*)(void*, void*, const void*);
using HuntSees_t = bool (*)(void*, void*, bool, const void*);
using HuntSense_t = bool (*)(void*, void*, const void*);
using SetDestination_t = bool (*)(void*, Vector3, const void*);
RefreshBase_t g_refreshBase = nullptr;
IdleInsanity_t g_idleInsanity = nullptr;
ChangeRoom_t g_changeRoom = nullptr;
HuntSees_t g_huntSees = nullptr;
HuntSense_t g_huntHears = nullptr;
HuntSense_t g_huntSensesGear = nullptr;
SetDestination_t g_setDestination = nullptr;

struct Scent {
  il2cpp::Held player;  // held while its trail lasts, so a player who left reads as gone
  trail::Trail trail;
  ULONGLONG ignoredUntil = 0;  // the hound lost them: it casts about for other trails until then
};

struct State {
  il2cpp::Held ghost;
  il2cpp::Held agent;
  int type = 0;
  OwnSpeed own = flat_speed(0);  // the hunt speed inputs the rule last computed (speed_rule.h)
  bool ruled = false;          // a rule applies to this hunt, so own is written
  bool hunting = false;
  bool steered = false;        // a player controls it: the game's own senses and steering stand
  float dullahanHeading = 0;   // degrees, the direction its body faces
  bool headingKnown = false;
  float nativeTurn = -1;       // the agent's angular speed before the Dullahan changed it
  // Draugr
  std::vector<il2cpp::Held> roomItems;  // things that were in its room when it appeared
  bool surveyed = false;
  bool angered = false;
  // Black Shuck
  std::map<void*, Scent> trails;  // per player
  trail::Breaks breaks;           // what breaks scent now, refreshed each trail step
  std::vector<trail::Point> running;  // the sinks running at the last trail step
  il2cpp::Held prey;              // the player it follows by scent in this hunt, or null
  ShuckMove move = ShuckMove::Native;
  trail::Point steer;             // where Follow and Sniff send it this frame
  il2cpp::Held killHeld;          // the hunt state whose kill reach it holds at 0, to give back
  float killReach = 0, slowKillReach = 0;  // that state's own reach (+0xBC, +0xC0) before it held them
  // Knell
  ULONGLONG huntStart = 0;  // when the current hunt began (0: not hunting)
  ULONGLONG nextTrailStep = 0, nextSurvey = 0, lastTick = 0;
} g;

/** The game's base speed for this ghost (GhostAI +0xC8), 1.7 m/s unless something changed it. */
float game_base() { return g.ghost ? il2cpp::field<float>(g.ghost, kGhostBaseSpeed) : 0.0f; }

/** Write the rule's speed into the hunt state: its base, and the line-of-sight speed-up held at 1 unless kept. */
void apply(void* huntState, OwnSpeed s) {
  il2cpp::set_field<float>(huntState, kHuntBase, s.base);
  if (!s.keepSight) il2cpp::set_field<float>(huntState, kHuntSight, 1.0f);
}

// The game copies the base back from GhostAI every two seconds; put the rule's back at once, so there is no blip.
void detour_refresh_base(void* huntState, const void* method) {
  hooks::Guard guard;
  g_refreshBase(huntState, method);
  if (g.ruled && huntState && g.ghost && game::ghost_state_object(g.ghost) == huntState) apply(huntState, g.own);
}

void detour_idle_insanity(void* idle, const void* method) {
  hooks::Guard guard;
  g_idleInsanity(idle, method);
  if (g.type == kDraugr && g.angered && idle && il2cpp::field<void*>(idle, kIdleGhost) == g.ghost)
    il2cpp::set_field<int>(idle, kIdleInsanity, il2cpp::field<int>(idle, kIdleInsanity) + kDraugrAngeredBonus);
}

void detour_change_room(void* controller, void* room, const void* method) {
  hooks::Guard guard;
  if (g.type == kDraugr && g.ghost) {
    PG_LOG("behaviour: the Draugr keeps its room");
    return;
  }
  g_changeRoom(controller, room, method);
}

/** Whether this hunt state's start delay is over (kHuntStartDelay below 0), so the game's chase, speed and kill run. */
bool begun(void* hunt) { return il2cpp::field<float>(hunt, kHuntStartDelay) < 0; }

/** Whether a ghost of `type` rules the senses of this hunt state: its AI hunts on the host, nobody steering it. */
bool ai_hunts(int type, void* hunt) {
  return g.type == type && g.hunting && !g.steered && hunt && g.ghost &&
         il2cpp::field<void*>(hunt, kHuntGhost) == g.ghost.get();
}

/** Whether the hunt may go for `player`, by its own test (kHuntEligibleRva), guarded where that test throws. */
bool eligible(void* hunt, void* player) {
  if (!hunt || !player || !il2cpp::field<void*>(player, kPlayerRoom) || !il2cpp::field<void*>(hunt, kHuntKilled)) return false;
  return api.eligible(hunt, player, nullptr);
}

/** Call a one-argument bool method of a hunt state's player set (Contains, Add); false without it. */
bool set_call(void* set, const il2cpp::Method* m, void* player) {
  return m && reinterpret_cast<bool (*)(void*, void*, const void*)>(*reinterpret_cast<void* const*>(m))(set, player, m);
}

/** Record `player` in the hunt's seen set, as its own sight test does for each player it sees. */
void record_seen(void* hunt, void* player) {
  void* seen = il2cpp::field<void*>(hunt, kHuntSeen);
  if (!seen) return;
  if (il2cpp::Class* klass = il2cpp::class_of(seen); klass != api.seenClass) {  // once: every hunt's set is one class
    api.seenClass = klass;
    api.seenContains = il2cpp::method(klass, "Contains", 1);
    api.seenAdd = il2cpp::method(klass, "Add", 1);
  }
  if (!set_call(seen, api.seenContains, player)) set_call(seen, api.seenAdd, player);
}

// The hunt's sight test, which every target it sets or keeps passes. The Black Shuck is blind: it
// detects only the player the target policy keeps, whom it follows by scent. The Bogeyman sees as the
// game does, then detects only those the policy keeps, so the game's own target code goes for the
// youngest of them.
bool detour_hunt_sees(void* hunt, void* player, bool skipDeogen, const void* method) {
  hooks::Guard guard;
  if (ai_hunts(kBogeyman, hunt))
    return g_huntSees(hunt, player, skipDeogen, method) && choose_target(g.type, g.ghost, player) == player;
  if (!ai_hunts(kBlackShuck, hunt)) return g_huntSees(hunt, player, skipDeogen, method);
  if (!player || choose_target(g.type, g.ghost, player) != player) return false;
  record_seen(hunt, player);
  return true;
}

// The hunt's voice test, which sends a ghost to a player talking near it. The Black Shuck is deaf.
bool detour_hunt_hears(void* hunt, void* player, const void* method) {
  hooks::Guard guard;
  return !ai_hunts(kBlackShuck, hunt) && g_huntHears(hunt, player, method);
}

// The hunt's electronics test, which sends a ghost to a player whose equipment is on near it. The
// Black Shuck senses no equipment.
bool detour_hunt_senses_gear(void* hunt, void* player, const void* method) {
  hooks::Guard guard;
  return !ai_hunts(kBlackShuck, hunt) && g_huntSensesGear(hunt, player, method);
}

// Every NavMeshAgent's SetDestination. While the Black Shuck's AI follows or sniffs a trail, its agent
// goes to the trail's next step whatever the game's chase or wander asks, so it walks the path its prey
// walked. The kill's own stop (a destination where the ghost stands) passes, and every other move and
// every other agent goes where the game sends it.
bool detour_set_destination(void* agent, Vector3 target, const void* method) {
  hooks::Guard guard;
  if (agent && agent == g.agent.get() && !g.steered && (g.move == ShuckMove::Follow || g.move == ShuckMove::Sniff) &&
      game::ghost_state(g.ghost) == game::kStateHunting &&
      game::distance(target, game::position(game::transform_of(g.ghost))) > kStandingStill)
    target = {g.steer.x, g.steer.y, g.steer.z};
  return g_setDestination(agent, target, method);
}

float yaw_of(Vector3 v) { return std::atan2(v.x, v.z) * 180.0f / 3.14159265f; }

float angle_between(float a, float b) {
  float d = std::fmod(std::fabs(a - b), 360.0f);
  return d > 180.0f ? 360.0f - d : d;
}

/** Turn `from` towards `to` by at most `step` degrees. */
float turn_towards(float from, float to, float step) {
  float d = std::fmod(to - from + 540.0f, 360.0f) - 180.0f;
  if (std::fabs(d) <= step) return to;
  return from + (d > 0 ? step : -step);
}

void* favourite_room() {
  void* info = il2cpp::field<void*>(g.ghost, kGhostInfo);
  void* room = info ? il2cpp::field<void*>(info, kInfoFavouriteRoom) : nullptr;
  return room && game::alive(room) ? room : nullptr;
}

/** Distance from p to the nearest of a room's collider boxes (0 inside one). */
float distance_to_room(void* room, Vector3 p) {
  float best = 1e9f;
  for (const game::Bounds& b : game::room_bounds(room)) {
    const float dx = std::max(0.0f, std::fabs(p.x - b.center.x) - b.extents.x);
    const float dy = std::max(0.0f, std::fabs(p.y - b.center.y) - b.extents.y);
    const float dz = std::max(0.0f, std::fabs(p.z - b.center.z) - b.extents.z);
    best = std::min(best, std::sqrt(dx * dx + dy * dy + dz * dz));
  }
  return best;
}

il2cpp::Array* find_all(il2cpp::Object* type) { return type ? api.findObjects(type, nullptr) : nullptr; }

/** The Draugr's room as it appeared: everything that can be carried and lies in it. */
void survey_room(void* room) {
  g.roomItems.clear();
  il2cpp::Array* items = find_all(api.interactType);
  for (uintptr_t i = 0; items && i < items->max_length; ++i) {
    void* item = items->data<void*>()[i];
    if (!item || !il2cpp::field<void*>(item, kInteractRigidbody) || il2cpp::field<void*>(item, kInteractDoor)) continue;
    if (game::in_room(room, game::position(game::transform_of(item)), 0)) g.roomItems.emplace_back(item);
  }
  g.surveyed = true;
  PG_LOG("behaviour: the Draugr's room holds %zu things", g.roomItems.size());
}

/** A speed rule applies to this hunt this frame: `s` is what to write. */
void rule(OwnSpeed s) {
  g.own = s;
  g.ruled = true;
}

void tick_draugr(bool hunting) {
  void* room = favourite_room();
  if (!room) return;
  if (!g.surveyed) survey_room(room);
  const ULONGLONG now = GetTickCount64();
  if (now >= g.nextSurvey) {
    g.nextSurvey = now + kSurveyMs;
    bool out = false;
    for (const il2cpp::Held& item : g.roomItems)
      if (game::alive(item.get()) && !game::in_room(room, game::position(game::transform_of(item.get())), kRoomMargin)) out = true;
    if (out != g.angered) PG_LOG(out ? "behaviour: something has left the Draugr's room" : "behaviour: the Draugr's room is whole again");
    g.angered = out;
  }
  if (hunting) rule(flat_speed(draugr_speed(distance_to_room(room, game::position(game::transform_of(g.ghost))))));
}

void tick_dullahan(bool hunting, float dt) {
  if (!hunting) {
    if (g.nativeTurn >= 0) api.setAngularSpeed(g.agent, g.nativeTurn, nullptr);
    g.nativeTurn = -1;
    g.headingKnown = false;
    return;
  }
  if (g.nativeTurn < 0) {
    g.own = flat_speed(dullahan_speed(game_base(), 0));  // until it moves, as if facing its path
    g.nativeTurn = api.angularSpeed(g.agent);
    api.setAngularSpeed(g.agent, kDullahanTurn, nullptr);
  }
  Vector3 v = api.velocity(g.agent, nullptr);
  if (v.x * v.x + v.z * v.z < 0.01f) api.desiredVelocity(g.agent, &v);
  if (v.x * v.x + v.z * v.z < 0.01f) {  // standing still: nothing to turn towards, so the last speed stands
    rule(g.own);
    return;
  }
  const float want = yaw_of(v);
  if (!g.headingKnown) g.dullahanHeading = yaw_of(game::forward(game::transform_of(g.ghost)));
  g.headingKnown = true;
  g.dullahanHeading = turn_towards(g.dullahanHeading, want, kDullahanTurn * dt);
  rule(flat_speed(dullahan_speed(game_base(), angle_between(g.dullahanHeading, want))));
}

// The Knell surges and crawls on a strict two-second beat through every hunt (knell_rule.h).
void tick_knell(bool hunting, ULONGLONG now) {
  if (!hunting) {
    g.huntStart = 0;
    return;
  }
  if (!g.huntStart) g.huntStart = now;
  rule(flat_speed(knell_speed(game_base(), (now - g.huntStart) / 1000.0f)));
}

/** A game position as a trail point. */
trail::Point to_point(Vector3 v) { return {v.x, v.y, v.z}; }

/**
 * What breaks scent now: salt, and every sink whose water is flowing. A sink's water flows while its
 * first Use has run (+0x74) and the fill tween that Use started is playing: each later Use pauses or
 * resumes it (DOTween.TogglePause on the water surface), and it ends when the basin is full.
 */
trail::Breaks scent_breaks() {
  trail::Breaks breaks;
  il2cpp::Array* salt = find_all(api.saltType);
  for (uintptr_t i = 0; salt && i < salt->max_length; ++i)
    if (void* s = salt->data<void*>()[i]) breaks.salt.push_back(to_point(game::position(game::transform_of(s))));
  il2cpp::Array* sinks = find_all(api.sinkType);
  for (uintptr_t i = 0; sinks && i < sinks->max_length; ++i) {
    void* sink = sinks->data<void*>()[i];
    void* water = sink && il2cpp::field<bool>(sink, kSinkUsed) ? il2cpp::field<void*>(sink, kSinkWater) : nullptr;
    if (water && game::alive(water) && api.isTweening(water, true, nullptr))
      breaks.water.push_back(to_point(game::position(game::transform_of(sink))));
  }
  return breaks;
}

/** Whether it rains on this contract: the weather the trail cannot be laid in outdoors. */
bool raining() {
  void* values = game::level_values();
  void* weather = values ? il2cpp::field<void*>(values, kLevelValuesWeather) : nullptr;
  const int type = weather ? il2cpp::field<int>(weather, kWeatherType) : -1;
  return type == kRain || type == kHeavyRain;
}

/** Whether p is outdoors, by the game's own test; false on a level with no outside room, where that test throws. */
bool outdoors(Vector3 p) {
  void* level = game::level_controller();
  void* outside = level ? il2cpp::field<void*>(level, kLevelOutsideRoom) : nullptr;
  if (!outside || !game::alive(outside) || !il2cpp::field<void*>(outside, kRoomColliders)) return false;
  return api.isOutside(level, p, nullptr);
}

/** Once per trail step: wash out the steps near water that has started running, then lay each player's next step. */
void record_trails(ULONGLONG now) {
  if (now < g.nextTrailStep) return;
  g.nextTrailStep = now + kTrailStepMs;
  g.breaks = scent_breaks();
  std::vector<trail::Point> started;
  for (const trail::Point& w : g.breaks.water)
    if (std::none_of(g.running.begin(), g.running.end(), [&](const trail::Point& r) { return trail::distance(r, w) < kSameSink; }))
      started.push_back(w);
  g.running = g.breaks.water;
  if (!started.empty())
    for (auto& [player, s] : g.trails) s.trail.wash(started);
  const bool rain = raining();
  game::each_player([&](void* player) {
    if (!game::living(player)) return;
    const Vector3 p = game::position(game::transform_of(player));
    auto [it, added] = g.trails.try_emplace(player);
    if (added) it->second.player = il2cpp::Held(player);
    it->second.trail.add(to_point(p), now, g.breaks, rain && outdoors(p));
  });
  // Every trail fades, including those of players who died or left.
  for (auto it = g.trails.begin(); it != g.trails.end();) {
    it->second.trail.forget_older_than(now);
    it = it->second.trail.steps().empty() ? g.trails.erase(it) : std::next(it);
  }
}

/**
 * The hunt's chase of, search for and wander around `player` end as the game's own clearings end them (its
 * kill clears its target, search player and last player; its closet search gives up by clearing the search
 * player, then the closet its search was for).
 */
void forget_in(void* hunt, void* player) {
  if (il2cpp::field<void*>(hunt, kHuntTarget) == player) il2cpp::set_ref_field(hunt, kHuntTarget, nullptr);
  if (il2cpp::field<void*>(hunt, kHuntLastPlayer) == player) il2cpp::set_ref_field(hunt, kHuntLastPlayer, nullptr);
  if (il2cpp::field<void*>(hunt, kHuntSearched) == player) {
    il2cpp::set_ref_field(hunt, kHuntSearched, nullptr);
    api.setCloset(hunt, nullptr, nullptr);
  }
}

/**
 * It has lost its prey: the hunt forgets them (forget_in), and their trail is ignored for kTrailMs, so it
 * casts about for other trails instead of waiting at the break.
 */
void lose_prey(void* hunt, void* player, ULONGLONG now) {
  forget_in(hunt, player);
  if (const auto it = g.trails.find(player); it != g.trails.end()) it->second.ignoredUntil = now + trail::kTrailMs;
  g.prey = nullptr;
}

/**
 * Hold the hunt's unforced kill off (both its reaches at 0), or give the game's reach back. That kill
 * measures distance only, which spans a thin wall, and the Shuck's sight test passes wherever its prey
 * is; so it reaches only a prey the game's own vision sensor sees, as every ghost's kill does. The closet
 * search's forced kill measures no reach.
 */
void hold_kill(void* hunt, bool held) {
  if (g.killHeld && (!held || g.killHeld.get() != hunt)) {
    il2cpp::set_field<float>(g.killHeld, kHuntKillReach, g.killReach);
    il2cpp::set_field<float>(g.killHeld, kHuntSlowKillReach, g.slowKillReach);
    g.killHeld = nullptr;
  }
  if (!held) return;
  if (!g.killHeld) {  // the reach the game gave it, to give back
    g.killReach = il2cpp::field<float>(hunt, kHuntKillReach);
    g.slowKillReach = il2cpp::field<float>(hunt, kHuntSlowKillReach);
    g.killHeld = hunt;
  }
  il2cpp::set_field<float>(hunt, kHuntKillReach, 0.0f);
  il2cpp::set_field<float>(hunt, kHuntSlowKillReach, 0.0f);
}

/**
 * The Black Shuck's hunt, for its AI on the host (shuck_rule.h). It keeps one prey while the game may go
 * for them (kHuntEligibleRva) and their trail leads to them (trail::follow), and loses them when salt or
 * water cuts it; with none it picks up the nearest trail it can follow. Follow makes the prey the game's
 * own target, so the game's chase and kill act on them while the SetDestination detour keeps it on the
 * trail; Sniff walks the nearest other trail in reach with no target; Search hands a hidden prey to the
 * game's own closet search; Native leaves the game alone.
 */
void tick_shuck(bool hunting, bool steered, ULONGLONG now) {
  record_trails(now);
  g.move = ShuckMove::Native;
  void* state = hunting && !steered ? game::ghost_state_object(g.ghost) : nullptr;
  if (!state || !begun(state)) {  // until the start delay ends, the game's own hunt does not move it either
    g.prey = nullptr;
    hold_kill(nullptr, false);
    return;
  }
  const Vector3 here = game::position(game::transform_of(g.ghost));
  const trail::Point at = to_point(here);
  trail::Point toPrey, toOther;
  void* prey = g.prey.get();
  bool followable = false;
  if (prey) {
    const auto followed = g.trails.find(prey);
    followable = game::living(prey) && eligible(state, prey) && followed != g.trails.end() &&
                 trail::follow(followed->second.trail, at, g.breaks, toPrey);
    if (!followable) lose_prey(state, prey, now);
  }
  std::vector<void*> players;  // those whose trails it may pick up or sniff, and their trails
  std::vector<const trail::Trail*> trails;
  for (auto& [player, s] : g.trails)
    if (now >= s.ignoredUntil && game::living(player) && eligible(state, player)) {
      players.push_back(player);
      trails.push_back(&s.trail);
    }
  if (!prey) {
    const int picked = trail::pick_up(trails, at, g.breaks);
    if (picked >= 0) {
      prey = players[picked];
      followable = trail::follow(*trails[picked], at, g.breaks, toPrey);
    }
  }
  void* closet = followable ? il2cpp::field<void*>(prey, kPlayerCloset) : nullptr;
  const bool hidden = closet && game::alive(closet) &&
                      game::distance(here, game::position(game::transform_of(prey))) <= kShuckSearchReach;
  const bool otherTrail = trail::next_step(trails, at, g.breaks, toOther);
  const bool smudged = game::alive(il2cpp::field<void*>(g.ghost, kGhostSmudge));
  g.move = shuck_move(smudged, prey != nullptr, followable, hidden, otherTrail);
  if (followable) g.prey = prey;
  hold_kill(state, g.move == ShuckMove::Follow && !hunt_sees(g.ghost, prey));
  switch (g.move) {
    case ShuckMove::Follow:
      if (il2cpp::field<void*>(state, kHuntTarget) != prey) api.setTarget(state, prey, nullptr);
      g.steer = toPrey;
      game::set_destination(g.agent, {toPrey.x, toPrey.y, toPrey.z});
      break;
    case ShuckMove::Sniff:
      g.steer = toOther;
      game::set_destination(g.agent, {toOther.x, toOther.y, toOther.z});
      break;
    case ShuckMove::Search:
      // The sight test now misses them, so the game drops its target and searches the closet it was
      // given: ClosetZone.CheckCloset first, then on its next pass its own kill of whoever hides there.
      if (il2cpp::field<void*>(state, kHuntCloset) != closet) api.setTarget(state, prey, nullptr);
      break;
    case ShuckMove::Native:
      break;
  }
}

void reset(void* ghost, int type) {
  if (g.agent && g.nativeTurn >= 0 && game::alive(g.agent)) api.setAngularSpeed(g.agent, g.nativeTurn, nullptr);
  hold_kill(nullptr, false);
  g = State{};
  g.ghost = ghost;
  g.type = type;
  g.agent = ghost ? game::ghost_agent(ghost) : nullptr;
  if (ghost) PG_LOG("behaviour: a %s haunts this contract", find(type) ? find(type)->name : "custom ghost");
}

}  // namespace

void* choose_target(int type, void* ghost, void* proposed) {
  if (!ghost || ghost != g.ghost.get()) return proposed;
  if (type == kBogeyman) return bogeyman::choose_target(ghost, proposed);
  if (type != kBlackShuck) return proposed;
  void* kept = shuck_target(g.move, g.prey, proposed);
  return kept && eligible(game::ghost_state_object(ghost), kept) ? kept : nullptr;
}

bool may_go_for(void* ghost, void* player) {
  if (!ghost || game::ghost_state(ghost) != game::kStateHunting) return false;  // another state's fields are not a hunt's
  return game::living(player) && eligible(game::ghost_state_object(ghost), player);
}

bool hunt_sees(void* ghost, void* player) {
  if (!may_go_for(ghost, player) || !il2cpp::field<void*>(player, kPlayerSightParts)) return false;
  return g_huntSees(game::ghost_state_object(ghost), player, false, nullptr);  // the trampoline: never this mod's detour
}

bool hunt_begun(void* ghost) {
  void* hunt = ghost && game::ghost_state(ghost) == game::kStateHunting ? game::ghost_state_object(ghost) : nullptr;
  return hunt && begun(hunt);
}

void forget(void* ghost, void* player) {
  void* hunt = ghost && game::ghost_state(ghost) == game::kStateHunting ? game::ghost_state_object(ghost) : nullptr;
  if (hunt && player) forget_in(hunt, player);
}

bool resolve() {
  constexpr const char* kAi = "UnityEngine.AIModule.dll";
  il2cpp::Class* agent = il2cpp::cls(kAi, "UnityEngine.AI", "NavMeshAgent");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  api.ghostController = il2cpp::cls("Assembly-CSharp.dll", "", "GhostController");
  api.interactType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PhotonObjectInteract"));
  api.saltType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "SaltSpot"));
  api.sinkType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "Sink"));
  bool ok = api.ghostController && api.interactType && api.saltType && api.sinkType;
  ok &= bind(api.findObjects, object, "FindObjectsOfType", 1);
  ok &= bind(api.isTweening, il2cpp::cls("DOTween.dll", "DG.Tweening", "DOTween"), "IsTweening", 2);
  ok &= bind(api.isOutside, il2cpp::cls("Assembly-CSharp.dll", "", "LevelController"), "IsPointInOutsideRoom", 1);
  api.setTarget = reinterpret_cast<decltype(api.setTarget)>(game::game_assembly_address(kHuntSetTargetRva));
  api.setCloset = reinterpret_cast<decltype(api.setCloset)>(game::game_assembly_address(kHuntSetClosetRva));
  api.eligible = reinterpret_cast<decltype(api.eligible)>(game::game_assembly_address(kHuntEligibleRva));
  ok &= api.setTarget && api.setCloset && api.eligible;
  ok &= bind(api.velocity, agent, "get_velocity", 0);
  api.desiredVelocity = reinterpret_cast<decltype(api.desiredVelocity)>(
      il2cpp::resolve_icall("UnityEngine.AI.NavMeshAgent::get_desiredVelocity_Injected"));
  api.angularSpeed = reinterpret_cast<decltype(api.angularSpeed)>(il2cpp::resolve_icall("UnityEngine.AI.NavMeshAgent::get_angularSpeed"));
  ok &= api.desiredVelocity && api.angularSpeed;
  ok &= bind(api.setAngularSpeed, agent, "set_angularSpeed", 1);
  PG_LOG("behaviour: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  void* changeRoom = il2cpp::method_ptr(api.ghostController, "ChangeFavouriteRoomTo", 1);
  void* setDestination = il2cpp::method_ptr(il2cpp::cls("UnityEngine.AIModule.dll", "UnityEngine.AI", "NavMeshAgent"),
                                            "SetDestination", 1);
  return hooks::create(game::game_assembly_address(kRefreshBaseRva), reinterpret_cast<void*>(&detour_refresh_base),
                       reinterpret_cast<void**>(&g_refreshBase), "GhostHunt.RefreshBase") &&
         hooks::create(game::game_assembly_address(kIdleInsanityRva), reinterpret_cast<void*>(&detour_idle_insanity),
                       reinterpret_cast<void**>(&g_idleInsanity), "GhostIdle.HuntInsanity") &&
         hooks::create(changeRoom, reinterpret_cast<void*>(&detour_change_room), reinterpret_cast<void**>(&g_changeRoom),
                       "GhostController.ChangeFavouriteRoomTo") &&
         hooks::create(game::game_assembly_address(kHuntSeesRva), reinterpret_cast<void*>(&detour_hunt_sees),
                       reinterpret_cast<void**>(&g_huntSees), "GhostHunt.Sees") &&
         hooks::create(game::game_assembly_address(kHuntHearsRva), reinterpret_cast<void*>(&detour_hunt_hears),
                       reinterpret_cast<void**>(&g_huntHears), "GhostHunt.HearsVoice") &&
         hooks::create(game::game_assembly_address(kHuntSensesGearRva), reinterpret_cast<void*>(&detour_hunt_senses_gear),
                       reinterpret_cast<void**>(&g_huntSensesGear), "GhostHunt.SensesElectronics") &&
         hooks::create(setDestination, reinterpret_cast<void*>(&detour_set_destination),
                       reinterpret_cast<void**>(&g_setDestination), "NavMeshAgent.SetDestination");
}

void tick(void* ghost, int type, bool steered) {
  if (ghost != g.ghost || type != g.type) reset(ghost, type);
  if (!g.ghost || !g.agent || !game::alive(g.agent)) return;
  const ULONGLONG now = GetTickCount64();
  const float dt = g.lastTick ? static_cast<float>(now - g.lastTick) / 1000.0f : 0.0f;
  g.lastTick = now;
  const bool hunting = game::ghost_state(g.ghost) == game::kStateHunting;
  g.hunting = hunting;
  g.steered = steered;
  g.ruled = false;
  if (type == kDullahan) tick_dullahan(hunting, dt);
  else if (type == kDraugr) tick_draugr(hunting);
  else if (type == kBlackShuck) {
    tick_shuck(hunting, steered, now);
    if (hunting) rule(flat_speed(game_base()));  // the sight speed-up never reaches it
  } else if (type == kKnell) tick_knell(hunting, now);
  else if (type == kBrownie) {
    if (hunting)  // slow while watched, very fast unseen, the game's own without its bindings
      rule(speed_when(brownie::available(), flat_speed(brownie_speed(game_base(), brownie::seen())), game_base()));
  } else if (type == kBogeyman) {
    if (hunting)  // slow out in the open
      rule(speed_when(bogeyman::in_open_room(), scaled_speed(game_base(), kBogeymanOpenSpeed), game_base()));
  } else if (type == kAsterion) {
    if (hunting)  // it stands still while it batters a door down
      rule(speed_when(asterion::battering(), scaled_speed(game_base(), 0.0f), game_base()));
  } else if (type == kTempest) {
    if (hunting)  // very fast while lightning lights it up, slow in the dark between, the game's own without its bindings
      rule(speed_when(tempest::available(), flat_speed(tempest_speed(game_base(), tempest::striking())), game_base()));
  }
  void* state = g.ruled ? game::ghost_state_object(g.ghost) : nullptr;
  if (state) apply(state, g.own);
}

void teardown() { reset(nullptr, 0); }

int scent(float* xyz, int maxPoints) {
  if (g.type != kBlackShuck || !g.ghost || !xyz || maxPoints <= 0) return 0;
  // Only the scent close enough for it to pick up, as the AI hound would (trail::kPickUp).
  const Vector3 at = game::position(game::transform_of(g.ghost));
  std::vector<trail::Trail::Step> steps;
  for (auto& [player, kept] : g.trails) {
    if (!game::living(player)) continue;
    for (const trail::Trail::Step& s : kept.trail.steps())
      if (std::hypot(s.at.x - at.x, s.at.z - at.z) <= trail::kPickUp && std::fabs(s.at.y - at.y) < 2.5f) steps.push_back(s);
  }
  std::sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) { return a.time > b.time; });
  const int n = std::min(maxPoints, static_cast<int>(steps.size()));
  for (int i = 0; i < n; ++i) {
    xyz[3 * i] = steps[i].at.x;
    xyz[3 * i + 1] = steps[i].at.y;
    xyz[3 * i + 2] = steps[i].at.z;
  }
  return n;
}

int sense(int type) {
  if (type != g.type || !g.ghost) return -1;
  if (type == kDraugr) return g.angered ? 1 : 0;
  if (type == kDullahan) return g.hunting && game_base() > 0 ? static_cast<int>(100.0f * g.own.base / game_base() + 0.5f) : 0;
  return -1;
}

}  // namespace pg::custom::behaviour
