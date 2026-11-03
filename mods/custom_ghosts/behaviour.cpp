#include "behaviour.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <deque>
#include <map>
#include <random>
#include <vector>

#include "Difficulty.h"
#include "Equipment.h"
#include "GhostAI.h"
#include "GhostHuntState.h"
#include "GhostIdleState.h"
#include "GhostWanderState.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "Props.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "Weather.h"
#include "asterion.h"
#include "bogeyman.h"
#include "bogeyman_rule.h"
#include "brownie.h"
#include "brownie_rule.h"
#include "carry.h"
#include "draugr_rule.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "goryo_way.h"
#include "held.h"
#include "hunt.h"
#include "knell_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"
#include "runtime.h"
#include "shuck_rule.h"
#include "speed_rule.h"
#include "tempest.h"
#include "tempest_rule.h"
#include "trail.h"

namespace pm::custom::behaviour {
namespace {

using phasmo::Vector3;
using phasmo::GhostHuntState;

constexpr ULONGLONG kTrailStepMs = 500;
constexpr float kSameSink = 0.1f;                 // metres: a running sink this close to one of the last step's is the same
constexpr float kStandingStill = 0.01f;           // metres: a destination this close to the ghost stops it where it is
constexpr ULONGLONG kSurveyMs = 1000;
constexpr float kRoomMargin = 1.0f;               // metres outside a room's colliders still counted as in it
constexpr ULONGLONG kAngerResendMs = 5000;        // the host says the Draugr's anger again this often

struct Scent {
  api::Held player;  // held while its trail lasts, so a player who left reads as gone
  trail::Trail trail;
  ULONGLONG ignoredUntil = 0;  // the hound lost them: it casts about for other trails until then
};

struct State {
  api::Held ghost;
  api::Held agent;
  int type = 0;
  OwnSpeed own = flat_speed(0);  // the hunt speed inputs the rule last computed (speed_rule.h)
  bool ruled = false;          // a rule applies to this hunt, so hunt_speed answers own
  bool hunting = false;
  bool steered = false;        // a player controls it: where it walks in a hunt is theirs
  float dullahanHeading = 0;   // degrees, the direction its body faces
  bool headingKnown = false;
  float nativeTurn = -1;       // the agent's angular speed before the Dullahan changed it
  // Draugr (draugr_rule.h)
  std::vector<api::Held> roomItems;     // its things: the house's own props that lay in its room when surveyed
  draugr::Hoard<api::Held, api::Held> hoard;  // its room, what it knows of each thing (same order) and its anger
  ULONGLONG nextTakeBack = 0, nextAngerSend = 0;
  carry::Carry takingBack;              // the thing it is taking back
  // Black Shuck
  std::map<void*, Scent> trails;  // per player
  trail::Breaks breaks;           // what breaks scent now, refreshed each trail step
  std::vector<trail::Point> running;  // the sinks running at the last trail step
  api::Held prey;                 // the player it follows by scent in this hunt, or null
  ShuckMove move = ShuckMove::Native;
  trail::Point steer;             // where Follow and Sniff send it this frame
  api::Held path;                 // the NavMeshPath its player's corridor asks its agent's way into, made once
  trail::WayCache way;            // the last way asked into it
  api::Held killHeld;             // the hunt state whose kill reach it holds at 0, to give back
  float killReach = 0, slowKillReach = 0;  // that state's own reach (killReach, slowKillReach) before it held them
  // Knell
  ULONGLONG huntStart = 0;  // when the current hunt began (0: not hunting)
  ULONGLONG nextTrailStep = 0, nextSurvey = 0, lastTick = 0;
} g;

// The Draugr's room while it is angered, on every client: each client's room runs its own temperature
// system, and its readings read it.
struct Cold {
  api::Held room;  // the room it makes colder; null while it does not
  draugr::RoomCold rule;
} g_cold;
bool g_heardAnger = false;  // a client's: whether the host last said the Draugr is angered

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

bool hunting_now(void* ghost) { return game::ghost_state(ghost) == phasmo::GhostState::hunting(); }

/** The ghost's hunt state while it hunts, null otherwise (another state's fields are not a hunt's). */
void* hunt_of(void* ghost) { return ghost && hunting_now(ghost) ? game::ghost_state_object(ghost) : nullptr; }

/** The game's base speed for this ghost (GhostAI.baseSpeed), 1.7 m/s unless something changed it. */
float game_base() { return g.ghost ? phasmo::GhostAI::baseSpeed.get(g.ghost) : 0.0f; }

void detour_idle_insanity(api::Original<phasmo::GhostIdleState::HuntInsanity> original, void* idle) {
  original(idle);
  if (g.type == kDraugr && g.hoard.angered && idle && phasmo::GhostIdleState::ghost.get(idle) == g.ghost)
    phasmo::GhostIdleState::insanity.set(idle, phasmo::GhostIdleState::insanity.get(idle) + kDraugrAngeredBonus);
}

// The wander's destination picker, on the master: the Draugr's picks skip the long roam as the Goryo's do
// (goryo_way.h), so its wander never changes its favourite room; a Monkey Paw's wish still can, as it can
// a Goryo's.
bool detour_pick_destination(api::Original<phasmo::GhostWanderState::PickDestination> original, void* wander,
                             phasmo::Vector3* to) {
  void* ghost = wander ? phasmo::GhostWanderState::ghost.get(wander) : nullptr;
  if (g.type != kDraugr || !ghost || ghost != g.ghost.get()) return original(wander, to);
  return goryo_way::pick_as_goryo(ghost, [&] { return original(wander, to); });
}

// Every room's temperature system, every frame on every client (LevelRoom.Update). For the Draugr's room
// while it is angered, the game's update runs on the room's own temperature, and the room then shows it
// less the Draugr's cold (draugr_rule.h RoomCold), the thermometer and breath reading that.
void detour_room_update(api::Original<phasmo::LevelRoom::Update> original, void* room) {
  using phasmo::LevelRoom;
  if (!room || room != g_cold.room.get()) return original(room);
  LevelRoom::temperature.set(room, g_cold.rule.before_update(LevelRoom::temperature.get(room)));
  original(room);
  LevelRoom::temperature.set(room, g_cold.rule.after_update(LevelRoom::temperature.get(room), true));
}

/** The room's temperature goes back to the game's own, and nothing is made colder. */
void end_cold() {
  void* room = g_cold.room;
  if (room && api::alive(room))
    phasmo::LevelRoom::temperature.set(room, g_cold.rule.end(phasmo::LevelRoom::temperature.get(room)));
  g_cold = Cold{};
}

/** Whether this hunt state is the hunt of the ghost of `type` on the host, whoever steers it. */
bool hunts(int type, void* hunt) {
  return g.type == type && g.hunting && hunt && g.ghost && GhostHuntState::ghost.get(hunt) == g.ghost.get();
}

// The hunt's sight test, which every target it sets or keeps passes. The Black Shuck is blind: it
// detects only the player the target policy keeps, whom it follows by scent. The Bogeyman sees as the
// game does, but not beyond its short sight in an open room. Both whoever steers it.
bool detour_hunt_sees(api::Original<GhostHuntState::Sees> original, void* hunt, void* player, bool skipDeogen) {
  if (hunt::own_sight()) return original(hunt, player, skipDeogen);  // hunt_sees asks the game's own test
  if (hunts(kBogeyman, hunt))
    return original(hunt, player, skipDeogen) && choose_target(g.type, g.ghost, player) == player;
  if (!hunts(kBlackShuck, hunt)) return original(hunt, player, skipDeogen);
  if (!player || choose_target(g.type, g.ghost, player) != player) return false;
  hunt::record_seen(hunt, player);
  return true;
}

// The hunt's voice test, which sends a ghost to a player talking near it. The Black Shuck is deaf.
bool detour_hunt_hears(api::Original<GhostHuntState::Hears> original, void* hunt, void* player) {
  return !hunts(kBlackShuck, hunt) && original(hunt, player);
}

// The hunt's electronics test, which sends a ghost to a player whose equipment is on near it. The
// Black Shuck senses no equipment.
bool detour_hunt_senses_gear(api::Original<GhostHuntState::SensesGear> original, void* hunt, void* player) {
  return !hunts(kBlackShuck, hunt) && original(hunt, player);
}

// Every NavMeshAgent's SetDestination. While the Black Shuck's AI follows or sniffs a trail, its agent
// goes to the trail's next step whatever the game's chase or wander asks, so it walks the path its prey
// walked. The kill's own stop (a destination where the ghost stands) passes, and every other move and
// every other agent goes where the game sends it.
bool detour_set_destination(api::Original<phasmo::NavMeshAgent::SetDestination> original, void* agent,
                            const phasmo::Vector3* target) {
  if (agent && target && agent == g.agent.get() && !g.steered &&
      (g.move == ShuckMove::Follow || g.move == ShuckMove::Sniff) && hunting_now(g.ghost) &&
      game::distance({target->x, target->y, target->z}, game::position(game::transform_of(g.ghost))) > kStandingStill) {
    const phasmo::Vector3 step{g.steer.x, g.steer.y, g.steer.z};
    return original(agent, &step);
  }
  return original(agent, target);
}

/** Send the agent to `p` through the game's own SetDestination (and so through its detour above). */
void go_to(void* agent, trail::Point p) {
  const phasmo::Vector3 to{p.x, p.y, p.z};
  phasmo::NavMeshAgent::SetDestination(agent, &to);
}

float yaw_of(phasmo::Vector3 v) { return std::atan2(v.x, v.z) * 180.0f / 3.14159265f; }

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
  void* info = phasmo::GhostAI::info.get(g.ghost);
  void* room = info ? phasmo::GhostInfo::favouriteRoom.get(info) : nullptr;
  return room && api::alive(room) ? room : nullptr;
}

/** Every loaded, active object of a map type (Object.FindObjectsOfType); empty when there is none. */
api::RefArray find_all(void* typeObject) {
  return api::RefArray{typeObject ? phasmo::Object::FindObjectsOfType(typeObject) : nullptr};
}

/**
 * The Draugr's things, as its rules start and whenever its favourite room has moved: everything that can be
 * carried and lies in its room, and where it lies, other than equipment, which players bring into the
 * contract.
 */
void survey_room(void* room) {
  g.roomItems.clear();
  g.hoard.things.clear();
  const api::RefArray items = find_all(phasmo::PhotonObjectInteract::type.type_object());
  for (size_t i = 0; i < items.size(); ++i) {
    void* item = items[i];
    if (!item || !phasmo::PhotonObjectInteract::body.get(item) || phasmo::PhotonObjectInteract::door.get(item) ||
        phasmo::Equipment::type.is(phasmo::PhotonObjectInteract::prop.get(item)))
      continue;
    const Vector3 at = game::position(game::transform_of(item));
    if (!game::in_room(room, at, 0)) continue;
    g.roomItems.emplace_back(item);
    g.hoard.things.push_back({.home = {at.x, at.y, at.z}});
  }
  PM_LOG("behaviour: the Draugr's room holds %zu things", g.roomItems.size());
}

/** The investigator carrying `item` (in a hand or put away, NetworkedPropGrab.inventory), or null. */
void* carrier_of(void* item) {
  void* carrier = nullptr;
  game::each_player([&](void* player) {
    void* grab = phasmo::Player::propGrab.get(player);
    const api::List carried{grab ? phasmo::NetworkedPropGrab::inventory.get(grab) : nullptr};
    if (!carrier && carried.contains(item)) carrier = player;
  });
  return carrier;
}

/** Who carries each of its things now (draugr_rule.h note_carriers), in the same order: each player's inventory read once. */
std::vector<void*> carriers_now() {
  std::vector<void*> out(g.roomItems.size(), nullptr);
  game::each_player([&](void* player) {
    void* grab = phasmo::Player::propGrab.get(player);
    const api::List carried{grab ? phasmo::NetworkedPropGrab::inventory.get(grab) : nullptr};
    for (size_t i = 0; i < g.roomItems.size(); ++i)
      if (!out[i] && carried.contains(g.roomItems[i].get())) out[i] = player;
  });
  return out;
}

/** Whether the Draugr may still move a thing it is taking back: no hunt has begun and nobody has picked it up. */
bool may_take_back(void* item) { return !g.hunting && !carrier_of(item); }

/** The host says the Draugr's anger to every client when it changes, and again every so often for a client that missed it. */
void tell_anger(ULONGLONG now, bool changed) {
  if (!changed && now < g.nextAngerSend) return;
  g.nextAngerSend = now + kAngerResendMs;
  const std::vector<uint8_t> message = net_msg::anger(g.hoard.angered);
  net::send(message.data(), message.size(), net::To::Others, true);
}

/**
 * Once a second: which of its things are out of its room and who carries them (draugr_rule.h survey), and
 * so whether it is angered. Outside hunts, while it is angered, it takes one back to where it lay, at the
 * Kikimora's cadence and never one a player carries.
 */
void survey_things(void* room, bool hunting, ULONGLONG now) {
  std::vector<draugr::Seen> seen;
  for (const api::Held& item : g.roomItems) {
    const bool alive = api::alive(item.get());
    const bool out = alive && !game::in_room(room, game::position(game::transform_of(item.get())), kRoomMargin);
    seen.push_back({alive, out, alive ? carrier_of(item) : nullptr});
  }
  const bool angered = draugr::survey(g.hoard.things, seen, now);
  const bool changed = angered != g.hoard.angered;
  if (changed) PM_LOG(angered ? "behaviour: something has left the Draugr's room" : "behaviour: the Draugr's room is whole again");
  g.hoard.angered = angered;
  tell_anger(now, changed);
  if (!g.nextTakeBack) g.nextTakeBack = now + carry::kMinGapMs;
  if (hunting || !angered || g.takingBack.busy() || now < g.nextTakeBack) return;
  g.nextTakeBack = now + carry::kMinGapMs +
                   std::uniform_int_distribution<ULONGLONG>(0, carry::kMaxGapMs - carry::kMinGapMs)(g_random);
  const int i = draugr::to_take_back(g.hoard.things, seen);
  if (i < 0) return;
  const draugr::Point home = g.hoard.things[i].home;
  if (g.takingBack.take(g.roomItems[i], {home.x, home.y, home.z}, now)) PM_LOG("behaviour: the Draugr takes something back");
}

/** A speed rule applies to this hunt this frame: `s` is what hunt_speed answers. */
void rule(OwnSpeed s) {
  g.own = s;
  g.ruled = true;
}

void tick_draugr(bool hunting, ULONGLONG now) {
  void* room = favourite_room();
  if (!room) return;
  const bool wasAngered = g.hoard.angered;
  if (g.hoard.follow(room)) {  // its first room, or a Monkey Paw's wish moved it: its things are what lies there
    g.takingBack.reset();
    survey_room(room);
    if (wasAngered) {
      PM_LOG("behaviour: the Draugr's room has moved; it is calm until something leaves the new one");
      tell_anger(now, true);
    }
  }
  draugr::note_carriers(g.hoard.things, carriers_now());
  const carry::Carry::Step step = g.takingBack.step(now, &may_take_back);
  if (step == carry::Carry::Step::Set) PM_LOG("behaviour: the Draugr put something back where it lay");
  if (step == carry::Carry::Step::Dropped) PM_LOG("behaviour: the Draugr could not take something back; left where it was");
  if (now >= g.nextSurvey) {
    g.nextSurvey = now + kSurveyMs;
    survey_things(room, hunting, now);
  }
  // Its hunts go only for its thief (target), whom the host holds them to (host/content_target.h).
  if (hunting) rule(flat_speed(draugr_speed(game::distance_to_room(room, game::position(game::transform_of(g.ghost))))));
}

void tick_dullahan(bool hunting, float dt) {
  if (!hunting) {
    if (g.nativeTurn >= 0) phasmo::NavMeshAgent::set_angularSpeed(g.agent, g.nativeTurn);
    g.nativeTurn = -1;
    g.headingKnown = false;
    return;
  }
  if (g.nativeTurn < 0) {
    g.own = flat_speed(dullahan_speed(game_base(), 0));  // until it moves, as if facing its path
    g.nativeTurn = phasmo::NavMeshAgent::get_angularSpeed(g.agent);
    phasmo::NavMeshAgent::set_angularSpeed(g.agent, kDullahanTurn);
  }
  phasmo::Vector3 v = phasmo::NavMeshAgent::get_velocity(g.agent);
  if (v.x * v.x + v.z * v.z < 0.01f) phasmo::NavMeshAgent::get_desiredVelocity_Injected(g.agent, &v);
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
 * first Use has run (Sink.used) and the fill tween that Use started is playing: each later Use pauses or
 * resumes it (DOTween.TogglePause on the water surface), and it ends when the basin is full.
 */
trail::Breaks scent_breaks() {
  trail::Breaks breaks;
  const api::RefArray salt = find_all(phasmo::SaltSpot::type.type_object());
  for (size_t i = 0; i < salt.size(); ++i)
    if (void* s = salt[i]) breaks.salt.push_back(to_point(game::position(game::transform_of(s))));
  const api::RefArray sinks = find_all(phasmo::Sink::type.type_object());
  for (size_t i = 0; i < sinks.size(); ++i) {
    void* sink = sinks[i];
    void* water = sink && phasmo::Sink::used.get(sink) ? phasmo::Sink::water.get(sink) : nullptr;
    if (water && api::alive(water) && phasmo::DOTween::IsTweening(water, true))
      breaks.water.push_back(to_point(game::position(game::transform_of(sink))));
  }
  return breaks;
}

/** Whether it rains on this contract: the weather the trail cannot be laid in outdoors. */
bool raining() {
  void* values = phasmo::LevelValues::get_Instance();
  void* weather = values ? phasmo::LevelValues::weather.get(values) : nullptr;
  if (!weather) return false;
  const int type = phasmo::WeatherProfile::weatherType.get(weather);
  return type == phasmo::WeatherType::rain() || type == phasmo::WeatherType::heavyRain();
}

/** Whether p is outdoors, by the game's own test; false on a level with no outside room, where that test throws. */
bool outdoors(Vector3 p) {
  void* level = game::level_controller();
  void* outside = level ? phasmo::LevelController::outsideRoom.get(level) : nullptr;
  if (!outside || !api::alive(outside) || !phasmo::LevelRoom::roomColliders.get(outside)) return false;
  const phasmo::Vector3 point{p.x, p.y, p.z};
  return phasmo::LevelController::IsPointInOutsideRoom(level, &point);
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
    if (added) it->second.player = api::Held(player);
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
  if (GhostHuntState::target.get(hunt) == player) GhostHuntState::target.set(hunt, nullptr);
  if (GhostHuntState::lastPlayer.get(hunt) == player) GhostHuntState::lastPlayer.set(hunt, nullptr);
  if (GhostHuntState::searchTarget.get(hunt) == player) {
    GhostHuntState::searchTarget.set(hunt, nullptr);
    GhostHuntState::SetCloset(hunt, nullptr);
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
    GhostHuntState::killReach.set(g.killHeld, g.killReach);
    GhostHuntState::slowKillReach.set(g.killHeld, g.slowKillReach);
    g.killHeld = nullptr;
  }
  if (!held) return;
  if (!g.killHeld) {  // the reach the game gave it, to give back
    g.killReach = GhostHuntState::killReach.get(hunt);
    g.slowKillReach = GhostHuntState::slowKillReach.get(hunt);
    g.killHeld = hunt;
  }
  GhostHuntState::killReach.set(hunt, 0.0f);
  GhostHuntState::slowKillReach.set(hunt, 0.0f);
}

/**
 * Runs fn(player, scent) for each trail the hunt `hunt` may follow now: those of the living players it may go for, but
 * for one it lost in the last kTrailMs. Its AI, its player's corridor and its player's view all go by these.
 */
template <class Fn>
void each_followed(void* hunt, ULONGLONG now, Fn fn) {
  for (const auto& [player, kept] : g.trails)
    if (now >= kept.ignoredUntil && game::living(player) && hunt::may_go_for(hunt, player)) fn(player, kept);
}

/**
 * The Black Shuck's hunt on the host (shuck_rule.h), whoever steers it. It keeps one prey while the game may go
 * for them (hunt::may_go_for) and their trail leads to them (trail::follow), and loses them when salt or
 * water cuts it, ignoring their trail for kTrailMs; with none its AI picks up the nearest trail it can follow,
 * and a player's takes the trail it walks (trail::walked). Follow makes the prey the game's own target, so the
 * game's chase and kill act on them while the SetDestination detour keeps its AI on the trail; Sniff walks its
 * AI along the nearest other trail in reach with no target; Search hands a hidden prey to the game's own closet
 * search; Native leaves the game alone. A player walks it themselves (player_move): nothing here sends them
 * anywhere.
 */
void tick_shuck(bool hunting, bool steered, ULONGLONG now) {
  record_trails(now);
  g.move = ShuckMove::Native;
  void* state = hunting ? game::ghost_state_object(g.ghost) : nullptr;
  if (!state || !hunt::begun(state)) {  // until the start delay ends, the game's own hunt does not move it either
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
    followable = game::living(prey) && hunt::may_go_for(state, prey) && followed != g.trails.end() &&
                 trail::follow(followed->second.trail, at, g.breaks, toPrey);
    if (!followable) lose_prey(state, prey, now);
  }
  std::vector<void*> players;  // those whose trails it may pick up or sniff, and their trails
  std::vector<const trail::Trail*> trails;
  each_followed(state, now, [&](void* player, const Scent& s) {
    players.push_back(player);
    trails.push_back(&s.trail);
  });
  // Just lost: its AI and a player alike cast about from the next frame. A player may otherwise move on to
  // another's trail by walking it; its AI keeps its prey until it loses them.
  const bool mayChoose = steered ? !prey || followable : !prey;
  if (mayChoose) {
    const int current = static_cast<int>(std::find(players.begin(), players.end(), prey) - players.begin());
    const int chosen = steered ? trail::walked(trails, current < static_cast<int>(players.size()) ? current : -1, at, g.breaks)
                               : trail::pick_up(trails, at, g.breaks);
    if (chosen >= 0) {
      prey = players[chosen];
      followable = trail::follow(*trails[chosen], at, g.breaks, toPrey);
    }
  }
  void* closet = followable ? phasmo::Player::closet.get(prey) : nullptr;
  const bool hidden = closet && api::alive(closet) &&
                      game::distance(here, game::position(game::transform_of(prey))) <= kShuckSearchReach;
  const bool otherTrail = trail::next_step(trails, at, g.breaks, toOther);
  const bool smudged = api::alive(phasmo::GhostAI::smudge.get(g.ghost));
  g.move = shuck_move(smudged, prey != nullptr, followable, hidden, otherTrail);
  if (followable) g.prey = prey;
  hold_kill(state, g.move == ShuckMove::Follow && !hunt_sees(g.ghost, prey));
  switch (g.move) {
    case ShuckMove::Follow:
      if (GhostHuntState::target.get(state) != prey) GhostHuntState::SetTarget(state, prey);
      g.steer = toPrey;
      if (!steered) go_to(g.agent, toPrey);
      break;
    case ShuckMove::Sniff:
      g.steer = toOther;
      if (!steered) go_to(g.agent, toOther);
      break;
    case ShuckMove::Search:
      // The sight test now misses them, so the game drops its target and searches the closet it was
      // given: ClosetZone.CheckCloset first, then on its next pass its own kill of whoever hides there.
      if (GhostHuntState::searchCloset.get(state) != closet) GhostHuntState::SetTarget(state, prey);
      break;
    case ShuckMove::Native:
      break;
  }
}

/** Whether the map holds all the agent's path needs (each entry is optional: without it, nothing is asked). */
bool path_resolved() {
  using api::Use;
  namespace map = pm::map;
  return Use<map::NavMeshPath>::resolved() && Use<map::NavMeshPath_ctor>::resolved() &&
         Use<map::NavMeshPath_get_status>::resolved() && Use<map::NavMeshPath_get_corners>::resolved() &&
         Use<map::NavMeshPathStatus_PathComplete>::resolved() && Use<map::NavMeshAgent_CalculatePath>::resolved();
}

/**
 * The way the Shuck's agent would walk from where it stands to `to` (trail.h Way), asked of the game as its wander asks
 * it (GhostWanderState.PickDestination: a NavMeshPath, the agent's CalculatePath, the path's status): its next corner,
 * the first past where it stands. Unknown without the entries, a path or a complete one.
 */
trail::Way way_to(trail::Point to) {
  constexpr float kSamePlace = 0.1f;  // metres: a corner this close to the agent is where it stands
  if (!g.agent || !path_resolved()) return {};
  if (!g.path) {
    void* path = phasmo::NavMeshPath::type.make();
    if (!path) return {};
    phasmo::NavMeshPath::ctor(path);
    g.path = path;
  }
  const phasmo::Vector3 target{to.x, to.y, to.z};
  if (!phasmo::NavMeshAgent::CalculatePath(g.agent, &target, g.path) ||
      phasmo::NavMeshPath::get_status(g.path) != phasmo::NavMeshPathStatus::PathComplete())
    return {};
  const api::Array<phasmo::Vector3> corners{phasmo::NavMeshPath::get_corners(g.path)};
  if (corners.size() < 2) return {};
  const trail::Point from{corners[0].x, corners[0].y, corners[0].z};
  for (size_t i = 1; i < corners.size(); ++i) {
    const trail::Point corner{corners[i].x, corners[i].y, corners[i].z};
    if (trail::distance(from, corner) > kSamePlace) return {true, corner, i + 1 == corners.size()};
  }
  return {};
}

void reset(void* ghost, int type) {
  if (g.agent && g.nativeTurn >= 0 && api::alive(g.agent)) phasmo::NavMeshAgent::set_angularSpeed(g.agent, g.nativeTurn);
  hold_kill(nullptr, false);
  hunt::clear_own_sight();
  g = State{};
  g.ghost = ghost;
  g.type = type;
  g.agent = ghost ? phasmo::GhostAI::agent.get(ghost) : nullptr;
  if (ghost) PM_LOG("behaviour: a %s haunts this contract", find(type) ? find(type)->name : "custom ghost");
}

}  // namespace

void* choose_target(int type, void* ghost, void* proposed) {
  if (!ghost || ghost != g.ghost.get()) return proposed;
  if (type == kBogeyman) return bogeyman::choose_target(ghost, proposed);
  if (type != kBlackShuck) return proposed;
  void* kept = shuck_target(g.move, g.prey, proposed);
  return kept && hunt::may_go_for(game::ghost_state_object(ghost), kept) ? kept : nullptr;
}

bool may_go_for(void* ghost, void* player) {
  void* hunt = hunt_of(ghost);
  return hunt && game::living(player) && hunt::may_go_for(hunt, player);
}

bool hunt_sees(void* ghost, void* player) {
  if (!may_go_for(ghost, player) || !phasmo::Player::losTargets.get(player)) return false;
  return hunt::own_sees(game::ghost_state_object(ghost), player);  // the game's own test: never this mod's policy
}

bool hunt_begun(void* ghost) { return hunt::begun(hunt_of(ghost)); }

void forget(void* ghost, void* player) {
  void* hunt = hunt_of(ghost);
  if (hunt && player) forget_in(hunt, player);
}

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::GhostIdleState::HuntInsanity>(&detour_idle_insanity, "GhostIdleState.HuntInsanity",
                                                          api::Twins::CanonicalOnly) &&
         api::hook<phasmo::GhostWanderState::PickDestination>(&detour_pick_destination,
                                                               "GhostWanderState.PickDestination",
                                                               api::Twins::CanonicalOnly) &&
         api::hook<GhostHuntState::Sees>(&detour_hunt_sees, "GhostHuntState.Sees", api::Twins::CanonicalOnly) &&
         api::hook<GhostHuntState::Hears>(&detour_hunt_hears, "GhostHuntState.Hears", api::Twins::CanonicalOnly) &&
         api::hook<GhostHuntState::SensesGear>(&detour_hunt_senses_gear, "GhostHuntState.SensesGear",
                                               api::Twins::CanonicalOnly) &&
         api::hook<phasmo::NavMeshAgent::SetDestination>(&detour_set_destination, "NavMeshAgent.SetDestination",
                                                         api::Twins::CanonicalOnly) &&
         api::hook<phasmo::LevelRoom::Update>(&detour_room_update, "LevelRoom.Update", api::Twins::CanonicalOnly);
}

void tick(void* ghost, int type, bool steered) {
  if (ghost != g.ghost || type != g.type) reset(ghost, type);
  if (!g.ghost || !g.agent || !api::alive(g.agent)) return;
  const ULONGLONG now = GetTickCount64();
  const float dt = g.lastTick ? static_cast<float>(now - g.lastTick) / 1000.0f : 0.0f;
  g.lastTick = now;
  const bool hunting = hunting_now(g.ghost);
  g.hunting = hunting;
  g.steered = steered;
  g.ruled = false;
  if (type == kDullahan) tick_dullahan(hunting, dt);
  else if (type == kDraugr) tick_draugr(hunting, now);
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
}

bool hunt_speed(float* base, float* losScale) {
  if (!g.ruled || !g.hunting || !g.ghost || !base || !losScale) return false;
  give_speed(g.own, base, losScale);
  return true;
}

void tick_room(void* haunting, int type) {
  void* info = haunting && type == kDraugr ? phasmo::GhostAI::info.get(haunting) : nullptr;
  void* room = info ? phasmo::GhostInfo::favouriteRoom.get(info) : nullptr;
  if (room && !api::alive(room)) room = nullptr;
  if (!room) g_heardAnger = false;
  // The host's own anger, or what the host last said of it.
  const bool angered = room && (game::is_master() ? g.hoard.angered && g.ghost.get() == haunting : g_heardAnger);
  void* cold = angered ? room : nullptr;
  if (g_cold.room.get() == cold) return;
  end_cold();
  g_cold.room = cold;
  if (cold) PM_LOG("behaviour: the Draugr's room grows colder");
}

void* target(int type, std::span<const void* const> excluded) {
  if (type != kDraugr || g.type != kDraugr || !g.ghost || !game::is_master()) return nullptr;
  return draugr::target(
      g.hoard.things, g.hoard.angered, [](void* player) { return game::living(player); }, excluded);
}

void receive(int sender, const uint8_t* data, size_t size) {
  bool angered = false;
  if (sender != game::master_actor() || !net_msg::read_anger(data, size, angered)) return;
  g_heardAnger = angered;
}

void teardown() {
  end_cold();
  g_heardAnger = false;
  reset(nullptr, 0);
}

int scent(float* xyz, int maxPoints) {
  if (g.type != kBlackShuck || !g.ghost || !g.hunting || !xyz || maxPoints <= 0) return 0;  // it follows scent in hunts
  // What the hunt goes by: the steps it may pick up of the trails it may follow.
  const trail::Point at = to_point(game::position(game::transform_of(g.ghost)));
  std::vector<trail::Trail::Step> steps;
  each_followed(hunt_of(g.ghost), GetTickCount64(), [&](void*, const Scent& kept) {
    for (const trail::Trail::Step& s : kept.trail.steps())
      if (trail::smells(at, s.at, g.breaks)) steps.push_back(s);
  });
  std::sort(steps.begin(), steps.end(), [](const auto& a, const auto& b) { return a.time > b.time; });
  const int n = std::min(maxPoints, static_cast<int>(steps.size()));
  for (int i = 0; i < n; ++i) {
    xyz[3 * i] = steps[i].at.x;
    xyz[3 * i + 1] = steps[i].at.y;
    xyz[3 * i + 2] = steps[i].at.z;
  }
  return n;
}

void player_move(const float* at, float* dx, float* dz) {
  if (g.type != kBlackShuck || !g.ghost || !at || !dx || !dz) return;
  void* hunt = hunt_of(g.ghost);
  const ULONGLONG now = GetTickCount64();
  std::vector<const trail::Trail*> trails;
  each_followed(hunt, now, [&](void*, const Scent& kept) { trails.push_back(&kept.trail); });
  const trail::Point here{at[0], at[1], at[2]};
  trail::confine(hunt != nullptr, trails, here, g.breaks, *dx, *dz,
                 [&](trail::Point step) { return g.way.get(here, step, now, &way_to); });
}

int sense(int type) {
  if (type == kDraugr) {
    const int32_t own = type == g.type && g.ghost ? (g.hoard.angered ? 1 : 0) : -1;
    return draugr::hud_anger(game::is_master(), own, g_heardAnger);
  }
  if (type != g.type || !g.ghost) return -1;
  if (type == kDullahan) return g.hunting && game_base() > 0 ? static_cast<int>(100.0f * g.own.base / game_base() + 0.5f) : 0;
  return -1;
}

}  // namespace pm::custom::behaviour
