#include "squatter.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <set>
#include <type_traits>
#include <vector>

#include "ClosetZone.h"
#include "Difficulty.h"
#include "Door.h"
#include "Footstep.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "game.h"
#include "game_api.h"
#include "log.h"
#include "ghost_sight.h"
#include "ghosts.h"
#include "net.h"
#include "players.h"
#include "runtime.h"
#include "squatter_chase.h"
#include "squatter_difficulty.h"
#include "squatter_door.h"
#include "squatter_figure.h"
#include "sound.h"
#include "squatter_voice.h"

namespace pm::custom::squatter {
namespace {

// What the master keeps of its chase (squatter_chase.h), forgotten with the phase.
struct Chase {
  Tracker tracker;
  int target = -1;           // the target's id (id_of), -1 for none
  bool hidingKnown = false;  // it knows the target's hiding place (knows_hiding)
  Interest interest;         // how long it has kicked at the hiding place it waits at (kKickInterestMs)
  // The hiding place it lost interest in: whoever hides there is neither seen nor heard by it until nobody does, so the
  // player has a chance to get away (user, 2026-11-09). Compared only.
  const void* givenUp = nullptr;
  LineClock lines;
  StuckClock stuck;
  TauntBook taunts;
  std::array<int, kLinesValues> lastLine{};  // the last line of each set, so a set never repeats at once
  std::optional<Phase> was;                  // the phase last frame
  uint64_t phaseSince = 0;
  uint64_t pauseUntil = 0;                   // an act stops it until then
  bool actOneOff = false;                    // the act set is a one-off, cleared when its pause ends
  uint64_t nextBump = 0, nextBang = 0, nextSight = 0, nextSteer = 0, lastSent = 0;
  std::set<int> seen, heard;                 // whom the last sight pulse saw and who was heard talking
  std::optional<Point> searchAt;
  std::optional<Point> searchFrom;  // where the search centres (search_origin), fixed as it begins
  std::optional<Point> seat;        // where a seated phase walks to before it sits, out of a closed hiding place
  uint64_t nextSeatCheck = 0;
  std::optional<bool> openingToward;                 // the door being opened swings toward it, for its clip
  api::Held passing;                                 // the door it waits at until it can pass (hold_at_doors)
  uint64_t passDeadline = 0;
  std::optional<float> lurkSpeed;                    // the ghost's own agent speed, while a door pauses its lurk
  api::Held openAtHand;  // a hiding place's door it stands at to open this frame (hold_at_doors)
  uint64_t nextHideLog = 0;  // when it next logs what it decides at a hiding place
  // The closet whose inside it opened to walk in (ClosetZone.internalAreaObstacle), and what its obstacle wanted before.
  api::Held openObstacle;
  bool obstacleWas = true;
  float steeredSpeed = 0.0f;                         // the speed steer last set (stops_dead)
  float sprint = 0.0f;                       // the sprint speed it last chased at (chase_sprint)
  uint64_t moveUntil = 0;                    // when it next looks at the way ahead (a clumsy move's clip, or kSenseMs)
  uint64_t nextClumsy = 0, nextDive = 0;     // no clumsy move before the first, no dive before the second
  Shown out;
  std::mt19937 rng{std::random_device{}()};
  Chase() { lastLine.fill(-1); }
};

struct State {
  std::optional<Phase> phase;
  bool steered = false;  // the master steered its ghost's agent last frame (squatter_rule.h steering_begins)
  ShownInbox shown;  // .last: the master's last word on another client, the master's own on the master
  TryTeller tryTeller;
  TryInbox tries;
  SprintTeller sprintTeller;
  SprintBook sprints;
  Chase chase;      // the master's
  CuePlayer cues;   // what this client has played of the Shown
  std::set<int> kicked;  // whom the master has kicked this contract (kick): each at most once
  // Every client's footsteps (the players' own step sounds, play_step): counted from the ghost's own movement here.
  StepCounter steps;
  std::optional<Point> stepFrom;
  const void* lastStep = nullptr;  // the clip it last stepped with, compared only, never used
  std::mt19937 stepRng{std::random_device{}()};
  // How fast the ghost moves on this client, for the figure's pace (figure::set_pace).
  std::optional<Point> paceFrom;
  uint64_t paceAt = 0;
  float pace = 0.0f;
  // Its head's turn to the player it is after, and the hand it puts on a door's handle (figure::look_at, reach).
  float lookWeight = 0.0f;
  phasmo::Vector3 lookPoint{};
  api::Held reachDoor;
  phasmo::Vector3 reachLocal{};  // the handle, in the leaf's own space
  figure::Hand reachHand = figure::Hand::Right;
  uint64_t reachSince = 0;
  float reachWeight = 0.0f;
  uint64_t playAt = 0;  // the last frame play() ran, for the blends' steps
  // The figure's own facing on this client, radians clockwise from +z, and the clip it plays of its own around the
  // master's moves (turning on the spot, setting off), until localUntil.
  std::optional<float> facing;
  enum class Local : uint8_t { None, Turning, Starting } local = Local::None;
  uint64_t localUntil = 0;
  std::optional<Move> lastMove;
  // The slam of a hiding place's door, due when its kick's foot lands (kSlamDelayMs), on this client, and the hiding
  // place, taken as the kick starts: by the time the foot lands it may have lost interest and looks at nobody.
  uint64_t slamAt = 0;
  api::Held slamCloset;
  // When this client's figure began a trip, until it has stood up from it (stood_up): its facing holds while it is down.
  std::optional<uint64_t> tripSince;
};

// Over how long the ghost's speed is smoothed for the figure's pace, seconds: a starting value.
constexpr float kPaceSmoothSeconds = 0.25f;
State g;

uint64_t now() { return GetTickCount64(); }

// The way the contract's ghost faces, radians clockwise from +z seen from above.
float heading_of(void* ghost) {
  void* t = phasmo::Component::get_transform(ghost);
  const phasmo::Vector3 f = t ? phasmo::Transform::get_forward(t) : phasmo::Vector3{0, 0, 1};
  return std::atan2(f.x, f.z);
}

// Every client shows its own figure where the contract's ghost is, facing its own way (play turns it).
void show(void* ghost) {
  void* t = phasmo::Component::get_transform(ghost);
  if (!t) return;
  const phasmo::Vector3 at = phasmo::Transform::get_position(t);
  if (!figure::ready()) figure::create(at);
  if (!g.facing) g.facing = heading_of(ghost);
  figure::place(at, *g.facing);
}

template <auto& Member>
bool resolved() {
  return std::remove_cvref_t<decltype(Member)>::use::resolved();
}

// This client's own player's sprint, as its rig works it out in a contract (squatter_net.h sprint_speed_if_on,
// can_sprint); none while any of it cannot be read, so the master never takes a guess.
std::optional<Sprint> own_sprint() {
  using phasmo::LocalPlayer;
  if (!resolved<LocalPlayer::sprintSpeed>() || !resolved<LocalPlayer::speedMultiplier>() ||
      !resolved<LocalPlayer::isDead>() || !resolved<LocalPlayer::stamina>() ||
      !resolved<phasmo::PlayerStamina::canSprint>() || !resolved<phasmo::Difficulty::sprinting>())
    return std::nullopt;
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  void* stamina = rig && api::alive(rig) ? LocalPlayer::stamina.get(rig) : nullptr;
  void* values = phasmo::LevelValues::instance.get();
  void* difficulty = values && api::alive(values) ? phasmo::LevelValues::difficulty.get(values) : nullptr;
  if (!stamina || !difficulty) return std::nullopt;
  const bool can =
      can_sprint(phasmo::PlayerStamina::canSprint.get(stamina), phasmo::Difficulty::sprinting.get(difficulty));
  return Sprint{can, sprint_speed_if_on(LocalPlayer::sprintSpeed.get(rig), LocalPlayer::speedMultiplier.get(rig),
                                        LocalPlayer::isDead.get(rig))};
}

// Tells the master this client's sprint: the master records its own, any other client sends it on a change and at a
// modest rate.
void tell_sprint(bool master) {
  const std::optional<Sprint> s = own_sprint();
  if (!s) return;
  if (master) {
    g.sprints.report(game::local_actor(), *s, now());
    return;
  }
  if (!g.sprintTeller.due(*s, now())) return;
  const std::vector<uint8_t> message = sprint_message(*s);
  net::send(message.data(), message.size(), net::To::Master, true);
}

// The local player tried an exit this client holds (squatter_door.h on_attempt): one try per input to the master,
// which picks the line.
void tried(void*) {
  if (!g.tryTeller.due(now())) return;
  if (game::is_master()) {
    g.tries.report(game::local_actor(), now());
    return;
  }
  const std::vector<uint8_t> message = try_message();
  net::send(message.data(), message.size(), net::To::Master, true);
}

// What the channel brought, forgotten as the Squatter stops haunting.
void forget_channel() {
  g.shown = ShownInbox{};
  g.tryTeller = TryTeller{};
  g.tries.clear();
  g.sprintTeller = SprintTeller{};
  g.sprints.clear();
  g.cues.reset();
}

Point point(phasmo::Vector3 v) { return {v.x, v.y, v.z}; }
phasmo::Vector3 vector(Point p) { return {p.x, p.y, p.z}; }
Point at_of(void* component) { return point(game::position(game::transform_of(component))); }

uint32_t roll() { return static_cast<uint32_t>(g.chase.rng()); }

// A player's id for the chase: their Photon actor, the local player's own (0 in single player, the one player there).
int id_of(void* player) {
  if (player == game::local_player()) return game::local_actor();
  std::vector<int> actors;
  game::room_actors(actors);
  for (int a : actors)
    if (game::player_of_actor(a) == player) return a;
  return -1;
}

// One player as the master's chase reads them this frame.
struct Seen {
  void* player = nullptr;
  int id = -1;
  Point at;
  bool living = false, inside = false;
  void* room = nullptr;    // Player.currentRoom
  void* closet = nullptr;  // Player.closet, the hiding place they are in
};

// Where a ghost stands at a closet, as the game's own hunt goes to one: the closet's navSpot, taken onto the navigation
// mesh within 1 m (NavMesh.SamplePosition in the walkable area; the game's hunt chase does the same, a constant in its
// code, as Bogeyman cites). None for no closet, or none on the mesh that near. The entrance point outside its door was
// not on the mesh at every closet, so the Squatter ran at the wall beside it and never kicked (user, 2026-11-09).
std::optional<Point> closet_spot(void* closet) {
  constexpr float kNavSpotSample = 1.0f;
  constexpr int kWalkableArea = 1;
  void* nav = closet ? phasmo::ClosetZone::navSpot.get(closet) : nullptr;
  if (!nav) return std::nullopt;
  const phasmo::Vector3 from = game::position(nav);
  phasmo::NavMeshHit hit{};
  if (!phasmo::NavMesh::SamplePosition(&from, &hit, kNavSpotSample, kWalkableArea)) return std::nullopt;
  return point(hit.position);
}

// A layer mask of the layers given, those that resolved.
int32_t mask_of(std::initializer_list<int32_t> layers) {
  int32_t mask = 0;
  for (const int32_t l : layers)
    if (l >= 0 && l < 32) mask |= 1 << l;
  return mask;
}

// Whether a ray from `from` along the unit `dir` hits something on `mask` within `metres`, triggers aside.
bool ray_hits(phasmo::Vector3 from, phasmo::Vector3 dir, float metres, int32_t mask) {
  return mask && phasmo::Physics::Raycast(&from, &dir, metres, mask, phasmo::QueryTriggerInteraction::Ignore());
}

// What the chasing Squatter reads of the way ahead, along the way its agent moves (Surroundings): rays at ankle, knee
// and chest height ahead for props in its way, a ray down ahead for a drop, rays to each side at the waist for a
// doorway, a gap or furniture beside it, and `target`, the player it senses, for a dive when `mayDive`. Nothing while
// it barely moves. The layers are the game's: furniture (DefaultProps, StaticProps, Cabinets), walls and doors (Wall,
// Door, Default), and the ground (Default, Floor, DefaultNoNavigation, StaticProps).
Surroundings surroundings(void* agent, Point at, const Seen* target, bool mayDive) {
  Surroundings s;
  const phasmo::Vector3 v = agent ? phasmo::NavMeshAgent::get_velocity(agent) : phasmo::Vector3{};
  const float speed = std::hypot(v.x, v.z);
  if (speed < kStillPace) return s;
  const phasmo::Vector3 ahead{v.x / speed, 0, v.z / speed}, right{ahead.z, 0, -ahead.x}, left{-ahead.z, 0, ahead.x};
  using L = phasmo::Layers;
  const int32_t furniture = mask_of({L::DefaultProps.index(), L::StaticProps.index(), L::Cabinets.index()});
  const int32_t solid = furniture | mask_of({L::Wall.index(), L::Door.index(), L::Default.index()});
  const int32_t ground =
      mask_of({L::Default.index(), L::Floor.index(), L::DefaultNoNavigation.index(), L::StaticProps.index()});
  const auto at_height = [&](float h) { return phasmo::Vector3{at.x, at.y + h, at.z}; };
  const bool chest = ray_hits(at_height(kChestHeight), ahead, kPropLook, solid);
  const bool knee = ray_hits(at_height(kKneeHeight), ahead, kPropLook, furniture);
  s.kneeProp = knee && !chest;
  s.ankleProp = !knee && ray_hits(at_height(kAnkleHeight), ahead, kPropLook, furniture);
  const phasmo::Vector3 down{0, -1, 0};
  const phasmo::Vector3 there{at.x + ahead.x * kDropAhead, at.y + kDropDepth, at.z + ahead.z * kDropAhead};
  s.dropAhead = ground && !ray_hits(there, down, 2.0f * kDropDepth, ground);
  const bool onLeft = ray_hits(at_height(kWaistHeight), left, kGapHalf, solid);
  const bool onRight = ray_hits(at_height(kWaistHeight), right, kGapHalf, solid);
  s.gap = onLeft && onRight;
  if (!s.gap) {
    if (ray_hits(at_height(kWaistHeight), right, kBesideReach, furniture)) s.furniture = 1;
    else if (ray_hits(at_height(kWaistHeight), left, kBesideReach, furniture)) s.furniture = -1;
  }
  if (mayDive && target) {
    const float dx = target->at.x - at.x, dz = target->at.z - at.z, metres = std::hypot(dx, dz);
    const float cosine = metres > 0 ? (dx * ahead.x + dz * ahead.z) / metres : 0.0f;
    s.diveAt = metres >= kDiveMin && metres <= kDiveMax &&
               cosine >= std::cos(kDiveConeDegrees * std::numbers::pi_v<float> / 180.0f);
  }
  return s;
}

// The middle of a door's collider box: the door itself, whichever way it is built. None for no door or no collider.
std::optional<phasmo::Vector3> door_middle(void* d) {
  if (!d || !api::alive(d)) return std::nullopt;
  void* collider = phasmo::Component::GetComponentInChildren(phasmo::Component::get_transform(d),
                                                             phasmo::Collider::type.type_object(), false);
  if (!collider) return std::nullopt;
  return phasmo::Collider::get_bounds(collider).center;
}

// Where the Squatter stands to kick a closet's door, and the door's middle it faces (user, 2026-11-09: it kicked the
// wall beside the door): a little way out in front of the middle of its doors, out being from the closet's own zone
// toward its doors, taken onto the navigation mesh within 1 m. The closet's spot (closet_spot) when its doors or the
// mesh there cannot be read.
struct KickSpot {
  Point stand, face;
};
std::optional<KickSpot> kick_spot(void* closet, Point from) {
  constexpr float kStandOut = 0.75f;  // metres out from the doors' middle: a kick's reach, a starting value
  if (!closet) return std::nullopt;
  int n = 0;
  phasmo::Vector3 mid{};
  for (void* d : {phasmo::ClosetZone::leftClosetDoor.get(closet), phasmo::ClosetZone::rightClosetDoor.get(closet)})
    if (const std::optional<phasmo::Vector3> m = door_middle(d)) {
      mid = {mid.x + m->x, mid.y + m->y, mid.z + m->z};
      ++n;
    }
  if (n == 0) {
    const std::optional<Point> spot = closet_spot(closet);
    return spot ? std::optional<KickSpot>({*spot, *spot}) : std::nullopt;
  }
  mid = {mid.x / n, mid.y / n, mid.z / n};
  const phasmo::Vector3 inside = game::position(phasmo::Component::get_transform(closet));
  float ox = mid.x - inside.x, oz = mid.z - inside.z;
  if (std::hypot(ox, oz) < 0.1f) ox = from.x - mid.x, oz = from.z - mid.z;  // a zone at its doors: out toward it
  const float out = std::hypot(ox, oz);
  if (out <= 0.0f) return std::nullopt;
  const phasmo::Vector3 want{mid.x + ox / out * kStandOut, from.y, mid.z + oz / out * kStandOut};
  phasmo::NavMeshHit hit{};
  if (!phasmo::NavMesh::SamplePosition(&want, &hit, 1.0f, 1)) return std::nullopt;
  return KickSpot{point(hit.position), point(mid)};
}

// Its way into the hiding place `closet` from `ghostAt` (way_in), and the door it opens for Open. It never passes
// through a door (user, 2026-11-09): a door not shut but too narrow it opens with the opening clip first, and a shut one
// it never opens. Kick for no closet or no door read.
struct HidingWay {
  Entry entry;
  api::Held door;  // Open: the door to open
};
HidingWay hiding_way_in(void* closet, Point ghostAt) {
  if (!closet || !api::alive(closet)) return {};
  void* doors[2] = {phasmo::ClosetZone::leftClosetDoor.get(closet), phasmo::ClosetZone::rightClosetDoor.get(closet)};
  std::vector<HidingDoor> read;
  std::vector<void*> of;
  for (void* d : doors) {
    if (!d || !api::alive(d)) continue;
    const std::optional<phasmo::Vector3> m = door_middle(d);
    const float metres = m ? std::hypot(m->x - ghostAt.x, m->z - ghostAt.z) : 0.0f;
    read.push_back({door::shut(d), door::lets_through(d), door::may_open(d), metres});
    of.push_back(d);
  }
  const Entry e = way_in(read);
  return {e, e.how == WayIn::Open ? of[static_cast<size_t>(e.door)] : nullptr};
}

// Whether the master's ghost at `at` stands inside the closet obstacle `obstacle` (DynamicNavMeshObstacle): within its
// trigger's box (the collider its Start adds), widened by kObstacleMargin; with no collider read, within
// kObstacleFallback of it.
bool inside_obstacle(void* obstacle, Point at) {
  constexpr float kObstacleMargin = 0.5f, kObstacleFallback = 2.0f;  // metres, starting values
  if (!obstacle || !api::alive(obstacle)) return false;
  if (void* collider = phasmo::Component::GetComponent(obstacle, phasmo::Collider::type.type_object())) {
    const phasmo::Bounds b = phasmo::Collider::get_bounds(collider);
    return inside_box(at, point(b.center),
                      {b.extents.x + kObstacleMargin, b.extents.y + kObstacleMargin, b.extents.z + kObstacleMargin});
  }
  const Point o = at_of(obstacle);
  return std::hypot(o.x - at.x, o.z - at.z) < kObstacleFallback;
}

// Puts back the closet obstacle the master opened, as it wanted before.
void close_obstacle() {
  Chase& c = g.chase;
  if (api::alive(c.openObstacle)) phasmo::DynamicNavMeshObstacle::targetActivity.set(c.openObstacle.get(), c.obstacleWas);
  c.openObstacle = nullptr;
}

// The master's closet obstacle this frame (user, 2026-11-09: it could not walk into Willow Street's pantry with its doors
// open; its route stopped at the doorway): a closet's inside is kept off a ghost's routes while its obstacle is on, so
// while it walks in for a hider (`wanted`, that closet's obstacle) the obstacle is wanted off, its Update switching it
// off; the one it opened goes back once it no longer goes in and stands clear of it (keeps_closet_open).
void keep_closet_open(void* wanted, Point ghostAt) {
  Chase& c = g.chase;
  if (c.openObstacle && c.openObstacle.get() != wanted &&
      !keeps_closet_open(false, inside_obstacle(c.openObstacle.get(), ghostAt)))
    close_obstacle();
  if (wanted && api::alive(wanted) && !c.openObstacle) {
    c.obstacleWas = phasmo::DynamicNavMeshObstacle::targetActivity.get(wanted);
    phasmo::DynamicNavMeshObstacle::targetActivity.set(wanted, false);
    c.openObstacle = wanted;
  }
}

// Turns the master's ghost to face `p` on the flat, for a kick or a wait at a door; every client's figure follows its
// heading (play's turn on the spot).
void face_toward(void* ghost, Point at, Point p) {
  const float yaw = std::atan2(p.x - at.x, p.z - at.z);
  const phasmo::Quaternion turn{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)};
  if (void* t = phasmo::Component::get_transform(ghost)) phasmo::Transform::set_rotation(t, &turn);
}

// The box of the hiding place the difficulty closed (BlockedZone.blocked) that `p` lies in, by the zone's own trigger
// collider; none when it lies in none.
std::optional<phasmo::Bounds> closed_hiding_place_at(Point p) {
  void* level = game::level_controller();
  const api::RefArray zones{level ? phasmo::LevelController::blockedZones.get(level) : nullptr};
  for (size_t i = 0; i < zones.size(); ++i) {
    void* z = zones[i];
    if (!z || !api::alive(z) || !phasmo::BlockedZone::blocked.get(z)) continue;
    void* collider = phasmo::Component::GetComponent(z, phasmo::Collider::type.type_object());
    if (!collider) continue;
    const phasmo::Bounds b = phasmo::Collider::get_bounds(collider);
    if (inside_box(p, point(b.center), point(b.extents))) return b;
  }
  return std::nullopt;
}

bool inside_at(Point p);

// The closest valid place to mope from `ghostAt` inside the closed hiding place `zone` (user, 2026-11-09): the nearest
// way out of its box (squatter_chase.h ways_out) that lies in the house and in no closed hiding place; none when no
// side leads anywhere valid.
std::optional<Point> closest_valid_place(Point ghostAt, const phasmo::Bounds& zone) {
  for (const Point& p : ways_out(ghostAt, point(zone.center), point(zone.extents)))
    if (inside_at(p) && !closed_hiding_place_at(p)) return p;
  return std::nullopt;
}

// Whether `p` lies in a room of the house: a room the map knows (game::room_at) that is not RoomKind.outsideMap.
bool inside_at(Point p) {
  void* room = game::room_at(vector(p));
  return room && phasmo::LevelRoom::kind.get(room) != phasmo::RoomKind::outsideMap();
}

std::vector<Seen> read_players() {
  std::vector<Seen> out;
  const int32_t outside = phasmo::RoomKind::outsideMap();
  game::each_player([&](void* p) {
    Seen s;
    s.player = p;
    s.id = id_of(p);
    s.at = at_of(p);
    s.living = game::living(p);
    s.room = phasmo::Player::currentRoom.get(p);
    // A player whose room cannot be read counts as outside: never a target, so never followed past an exit.
    s.inside = s.room && phasmo::LevelRoom::kind.get(s.room) != outside;
    s.closet = phasmo::Player::closet.get(p);
    if (s.id >= 0) out.push_back(s);
  });
  return out;
}

// Whom the ghost sees through the shared sight copies, and who is heard talking within the hunt's hearing
// (squatter_chase.h within_hearing), every kSightMs. Asking a player's loudness clears it, as the hunt's own test does.
void sense(void* ghost, const std::vector<Seen>& players, uint64_t t) {
  Chase& c = g.chase;
  if (t < c.nextSight) return;
  c.nextSight = t + kSightMs;
  c.seen.clear();
  c.heard.clear();
  const bool sight = ghost_sight::pulse(ghost);
  void* level = game::level_controller();
  void* ghostRoom = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  void* head = phasmo::GhostAI::head.get(ghost);
  const Point ear = head ? point(game::position(head)) : at_of(ghost);
  for (const Seen& s : players) {
    if (!s.living || !s.inside) continue;
    if (sight && ghost_sight::sees(s.player)) c.seen.insert(s.id);
    void* body = phasmo::Player::body.get(s.player);
    const Point b = body ? point(game::position(game::object_transform(body))) : s.at;
    const bool sameFloor =
        ghostRoom && s.room && phasmo::LevelRoom::floor.get(ghostRoom) == phasmo::LevelRoom::floor.get(s.room);
    const float metres = std::hypot(b.x - ear.x, b.y - ear.y, b.z - ear.z);
    if (!within_hearing(metres, phasmo::Player::wished.get(s.player), sameFloor, !s.inside)) continue;
    void* audio = phasmo::Player::audio.get(s.player);
    if (audio && phasmo::PlayerAudio::IsTalkingLoudly(audio, 0.1f)) c.heard.insert(s.id);
  }
}

// Starts act `a` on every client; a one-off is cleared once its pause of `ms` ends, a lasting one (sitting) stays.
void act(Act a, uint64_t ms, uint64_t t, bool oneOff = true) {
  Chase& c = g.chase;
  c.out.act = static_cast<int8_t>(a);
  ++c.out.actCue;
  c.pauseUntil = t + ms;
  c.actOneOff = oneOff;
}

// Starts a line of `set` on every client, never the one that set last played when it has another.
void say(Lines set, uint64_t t) {
  Chase& c = g.chase;
  const int i = static_cast<int>(set);
  const int index = pick_line(voice::count(set), c.lastLine[i], roll());
  c.lines.spoke(t, roll());
  if (index < 0) return;
  c.lastLine[i] = index;
  c.out.lineSet = static_cast<int8_t>(i);
  c.out.lineIndex = static_cast<int8_t>(index);
  ++c.out.lineCue;
}

// Where to walk to, renewed every kSteerMs; at most every frame's speed is set after the idle change (tick). Brought to a
// stop (a fall, a bump, a kick, a door), it stops where it stands, as its clip does: the agent's speed alone would let
// it slide to a halt at its acceleration (user, 2026-11-09: it moved along the floor after tripping). The game's own
// ghost teleports stop an agent with NavMeshAgent.Warp, which leaves it standing with no path; it walks on once the
// next destination is set.
void steer(void* agent, std::optional<Point> to, float speed, uint64_t t) {
  Chase& c = g.chase;
  if (stops_dead(c.steeredSpeed, speed)) {
    phasmo::Vector3 here = phasmo::Transform::get_position(phasmo::Component::get_transform(agent));
    phasmo::NavMeshAgent::Warp(agent, &here);
    c.nextSteer = 0;
  }
  c.steeredSpeed = speed;
  phasmo::NavMeshAgent::set_speed(agent, speed);
  if (!to || t < c.nextSteer) return;
  c.nextSteer = t + kSteerMs;
  if (!game::set_destination(agent, vector(*to))) c.searchAt.reset();  // off the navigation mesh: pick another
}

// The shut door of the ghost's room it has reached, if one may be opened quietly (squatter_door.h may_open).
void* door_to_open(Point ghostAt, Point velocity) {
  void* level = game::level_controller();
  void* room = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  const api::RefArray doors{room ? phasmo::LevelRoom::doors.get(room) : nullptr};
  for (size_t i = 0; i < doors.size(); ++i) {
    void* d = doors[i];
    if (!d || !api::alive(d)) continue;
    const Point at = at_of(d);
    if (at_door(ghostAt, at) && heading_into(ghostAt, at, velocity) && door::may_open(d)) return d;
  }
  return nullptr;
}

// The master's doors (user, 2026-11-09: it never passes through a door): at a door it walks into whose gap is too narrow
// to pass, it stops, plays the opening-door clip for the way the leaf swings (door_act) and swings the door open with the
// game's own HingeDoor.SwingDoor, then waits until the gap fits or kPassWaitMs passes. True while it waits there.
bool hold_at_doors(void* agent, Point ghostAt, uint64_t t) {
  Chase& c = g.chase;
  if (c.passing) {
    if (!door::lets_through(c.passing.get()) && t < c.passDeadline) return true;
    c.passing = nullptr;
  }
  door::Opening o = door::open_step(t);
  if (o == door::Opening::Idle && t >= c.pauseUntil) {
    const phasmo::Vector3 v = agent ? phasmo::NavMeshAgent::get_velocity(agent) : phasmo::Vector3{};
    if (void* d = c.openAtHand ? c.openAtHand.get() : door_to_open(ghostAt, point(v))) {
      const std::optional<door::Leaf> leaf = door::leaf_of(d);
      c.openingToward = leaf && door::swings_toward(*leaf, {ghostAt.x, ghostAt.y, ghostAt.z});
      c.passing = d;
      c.passDeadline = t + kPassWaitMs;
      o = door::open_quietly(d, t);
    }
  }
  if (o == door::Opening::Opened && c.openingToward) act(door_act(*c.openingToward), kDoorPauseMs, t);
  if (o != door::Opening::Waiting) c.openingToward.reset();
  if (o == door::Opening::Refused) c.passing = nullptr;  // nothing it may open: the game's own way
  return static_cast<bool>(c.passing);
}

// The master's Squatter, every frame it haunts with a phase: reads who it sees and hears, moves the phase on, locks
// the exits once everyone is out, steers the agent and fills the Shown every client plays.
void run(void* ghost, bool friendlyGhost, uint64_t t) {
  Chase& c = g.chase;
  // A client that has just become the master goes on from the last Shown it followed, so no client replays a cue.
  if (!c.was && g.shown.last) {
    c.out = *g.shown.last;
    c.was = c.out.phase;
  }
  void* agent = game::ghost_agent(ghost);
  const Point ghostAt = at_of(ghost);
  const std::vector<Seen> players = read_players();
  sense(ghost, players, t);

  // Its target: the nearest player it senses inside, else the one it still tracks or knows is hiding.
  std::vector<Candidate> candidates;
  const Seen* target = nullptr;
  // Once nobody hides in the hiding place it gave up on, it may go for whoever hides there again.
  if (c.givenUp && std::none_of(players.begin(), players.end(), [&](const Seen& s) { return s.closet == c.givenUp; }))
    c.givenUp = nullptr;
  const auto unnoticed = [&](const Seen& s) { return c.givenUp && s.closet == c.givenUp; };
  for (const Seen& s : players) {
    const bool sensed = !unnoticed(s) && (c.seen.contains(s.id) || c.heard.contains(s.id));
    const bool hiding = s.id == c.target && c.hidingKnown && s.closet;
    candidates.push_back({s.id, std::hypot(s.at.x - ghostAt.x, s.at.z - ghostAt.z), s.living, s.inside, sensed,
                          hiding});
  }
  const int picked = pick_target(candidates, c.target, c.tracker.tracks(t));
  if (picked != c.target) {
    c.hidingKnown = false;
    if (picked < 0) c.tracker.forget();
  }
  c.target = picked;
  for (const Seen& s : players)
    if (s.id == c.target) target = &s;
  const bool targetSensed =
      target && !unnoticed(*target) && (c.seen.contains(target->id) || c.heard.contains(target->id));
  if (targetSensed) c.tracker.sensed(target->id, target->at, t);
  c.hidingKnown = target && knows_hiding(target->closet != nullptr, targetSensed || c.tracker.tracks(t), c.hidingKnown);
  // A hiding place it has a way into hides nobody from it: it knows where they are and goes in for them, opening the
  // door first when one stands ajar (hiding_way_in).
  const HidingWay way = c.hidingKnown ? hiding_way_in(target->closet, ghostAt) : HidingWay{};
  if (c.hidingKnown && way.entry.how != WayIn::Kick) c.tracker.sensed(target->id, target->at, t);
  keep_closet_open(!friendlyGhost && c.hidingKnown && way.entry.how == WayIn::Walk
                       ? phasmo::ClosetZone::internalAreaObstacle.get(target->closet)
                       : nullptr,
                   ghostAt);

  bool anySeen = false;
  for (const Seen& s : players) anySeen = anySeen || (s.living && s.inside && !unnoticed(s) && c.seen.contains(s.id));
  const Phase now = *g.phase;
  // Nothing is chased or kicked by a figure nobody can see (one that failed to build is logged by squatter_figure).
  const bool shown = figure::ready();
  const Facts f{friendlyGhost,
                sees_for_phase(now, anySeen, targetSensed, shown),
                shown && c.target >= 0 && c.tracker.tracks(t),
                waits_at_hiding_place(c.hidingKnown, way.entry.how),
                door::living_inside(),
                t >= c.pauseUntil,
                c.interest.lost(t)};
  if (doors_lock(now, f)) door::lock_all();
  const Phase next = next_phase(now, f);
  g.phase = next;

  // A new phase: its first line at once, its stance.
  if (c.was != next) {
    // Lost interest at a hiding place, it forgets whoever hides there and lurks, as any ghost wanders (user, 2026-11-09).
    if (c.was == Phase::AtHidingPlace && next == Phase::Lurking) {
      c.givenUp = target ? target->closet : nullptr;
      c.tracker.forget();
      c.target = -1;
      c.hidingKnown = false;
    }
    c.interest.reset();
    c.phaseSince = t;
    c.lines.reset();
    c.searchAt.reset();
    c.searchFrom.reset();
    c.seat.reset();
    if (const std::optional<Act> stance = stance_for(next)) {
      act(*stance, 0, t, false);  // held for the whole phase
    } else if (const std::optional<Act> rise = entry_act(c.was, next)) {
      act(*rise, kGetUpMs, t);
    } else if (c.out.act == static_cast<int8_t>(Act::Sit)) {
      c.out.act = -1;
    }
    if (next == Phase::Chasing && c.was == Phase::Lurking) {
      c.moveUntil = 0;
      c.nextClumsy = 0;
    }
    c.was = next;
  }
  if (c.actOneOff && t >= c.pauseUntil && c.out.act >= 0) {
    c.out.act = -1;
    c.actOneOff = false;
  }

  // Its pace: just under the target's sprint, whatever their stamina (squatter_chase.h chase_sprint).
  const std::optional<Sprint> targetSprint = c.target >= 0 ? g.sprints.of(c.target, t) : std::nullopt;
  c.sprint = chase_sprint(targetSprint, g.sprints.of(game::local_actor(), t), c.sprint);
  const bool paused = t < c.pauseUntil;

  // Lines: the phase's own at a random gap; the taunt for a tired target; a line for each try at a locked exit.
  if (next != Phase::LockedOut && c.lines.due(t)) say(lines_for(next), t);
  if (next == Phase::Chasing && c.target >= 0 && targetSprint && c.taunts.due(c.target, targetSprint->canSprint, t))
    say(Lines::Tired, t);
  if (!take_tries().empty() && next == Phase::LockedOut) say(Lines::Door, t);

  float speed = 0.0f;
  std::optional<Point> to;
  c.openAtHand = nullptr;
  switch (next) {
    case Phase::Chasing: {
      // It runs, and reads the way ahead every kSenseMs: a clumsy move plays where the way calls for one (move_for), for
      // its whole clip, and the next waits kClumsyGapMs after it.
      if (t >= c.moveUntil) {
        Pick pick;
        // A clumsy move carries its figure the way the figure faces, so it waits until this client's figure, the
        // master's own, has turned to the way the agent runs and is not turning on the spot (faces_ahead).
        const phasmo::Vector3 v = agent ? phasmo::NavMeshAgent::get_velocity(agent) : phasmo::Vector3{};
        const bool facesAhead =
            g.facing && g.local != State::Local::Turning && faces_ahead(*g.facing, std::atan2(v.x, v.z));
        if (t >= c.nextClumsy && !paused && facesAhead)
          // Never a dive at a player in a hiding place: it would roll through the door.
          pick = move_for(
              surroundings(agent, ghostAt, targetSensed && !c.hidingKnown ? target : nullptr, t >= c.nextDive));
        if (pick.move != c.out.move || plays_once(pick.move)) {
          c.out.move = pick.move;
          c.out.variant = pick.variant;
        }
        if (plays_once(pick.move)) {
          const float hold = move_hold_seconds(pick.move, seconds_of(state_for(pick.move, pick.variant)));
          c.moveUntil = t + static_cast<uint64_t>(hold * 1000.0f);
          c.nextClumsy = c.moveUntil + kClumsyGapMs;
          if (pick.move == Move::Roll) c.nextDive = t + kDiveGapMs;
        } else {
          c.moveUntil = t + kSenseMs;
        }
      }
      const Move m = c.out.move;
      speed = moves_by_clip(m) ? 0.0f : move_speed(c.sprint, m);  // a roll or a trip carries it by its clip
      to = targetSensed ? std::optional<Point>(target->at) : c.tracker.last_at();
      // A player in a hiding place it has a way into: it heads for the closet's spot (ClosetZone.navSpot on the mesh),
      // where the game's hunt goes, opening a door that stands ajar with the opening clip as it reaches it or the spot
      // (hold_at_doors), never walking through it; at the spot it has them, as the hunt's closet search does.
      bool hiderReached = false;
      if (c.hidingKnown && target && way.entry.how != WayIn::Kick) {
        const std::optional<Point> spot = closet_spot(target->closet);
        if (way.entry.how == WayIn::Open) {
          const std::optional<phasmo::Vector3> mid = door_middle(way.door.get());
          const bool atDoor = mid && std::hypot(mid->x - ghostAt.x, mid->z - ghostAt.z) <= kDoorReach;
          if (at_spot(ghostAt, spot) || atDoor || !spot) {
            if (mid) face_toward(ghost, ghostAt, point(*mid));
            speed = 0.0f;
            to = ghostAt;
            c.openAtHand = way.door.get();
          } else {
            to = spot;
          }
        } else {
          hiderReached = reaches_hider(ghostAt, target->at, spot);
          to = spot ? spot : std::optional<Point>(target->at);
        }
        if (t >= c.nextHideLog) {  // what it decides at a hiding place, for the user's tests (once a second)
          c.nextHideLog = t + 1000;
          PM_LOG("squatter: hiding place way=%d ghost=(%.2f,%.2f,%.2f) spot=%s(%.2f,%.2f,%.2f) to=(%.2f,%.2f,%.2f) "
                 "left=%.2f pending=%d",
                 static_cast<int>(way.entry.how), ghostAt.x, ghostAt.y, ghostAt.z, spot ? "" : "none ",
                 spot ? spot->x : 0.0f, spot ? spot->y : 0.0f, spot ? spot->z : 0.0f, to ? to->x : 0.0f,
                 to ? to->y : 0.0f, to ? to->z : 0.0f, agent ? phasmo::NavMeshAgent::get_remainingDistance(agent) : -1.0f,
                 agent ? static_cast<int>(phasmo::NavMeshAgent::get_pathPending(agent)) : -1);
        }
      }
      // The catch: a kick to the face, a line, and the game's own kill (kick).
      if (target && !paused &&
          // may_kick's distance is given as 0 or past reach: within_kick measures it in three dimensions.
          may_kick(next, friendlyGhost, target->living,
                   hiderReached || within_kick(ghostAt, target->at) ? 0.0f : kKickReach + 1.0f, kKickReach) &&
          shown && kick(target->player, target->id)) {
        act(Act::Kick, kKickMs, t);
        say(Lines::Catch, t);
        c.tracker.forget();
        c.target = -1;
      }
      break;
    }
    case Phase::Searching: {
      c.out.move = Move::Sneak;  // creeping about, the user's Crouched Walking clip (2026-11-09)
      speed = move_speed(c.sprint, Move::Run) * kSearchPace;
      // Around where it last sensed its target, else around where the search began, so it never stands still; a point
      // not inside the house is dropped and another tried next frame, so it never searches past an exit.
      if (!c.searchFrom) c.searchFrom = search_origin(c.tracker.last_at(), ghostAt);
      if (c.searchAt && std::hypot(c.searchAt->x - ghostAt.x, c.searchAt->z - ghostAt.z) < kStandReach)
        c.searchAt.reset();
      if (!c.searchAt) {
        const Point p = search_point(*c.searchFrom, roll());
        if (inside_at(p)) c.searchAt = p;
      }
      to = c.searchAt;
      break;
    }
    case Phase::AtHidingPlace: {
      const std::optional<KickSpot> kickAt = target ? kick_spot(target->closet, ghostAt) : std::nullopt;
      const std::optional<Point> spot = kickAt ? std::optional<Point>(kickAt->stand) : std::nullopt;
      const bool there = spot && std::hypot(spot->x - ghostAt.x, spot->z - ghostAt.z) < kStandReach;
      if (there) face_toward(ghost, ghostAt, kickAt->face);  // squarely at the doors, so the kick lands on them
      c.out.move = Move::Run;
      speed = there ? 0.0f : move_speed(c.sprint, Move::Run);
      to = there ? std::optional<Point>(ghostAt) : spot;
      if (there && t >= c.nextBang && !paused) {  // demands they come out, kicking the door over and over
        act(Act::Bang, kBangGapMs / 2, t);
        c.nextBang = t + kBangGapMs;
        c.interest.kicked(t);  // it loses interest kKickInterestMs after its first kick here
      }
      break;
    }
    case Phase::Following: {  // friendly: after its target, waiting and talking at a hiding place, never banging
      const std::optional<Point> stand = c.hidingKnown && target ? closet_spot(target->closet) : std::nullopt;
      const bool known = target && (targetSensed || c.tracker.tracks(t));
      const Step step = follow_step(ghostAt, known ? std::optional<Point>(target->at) : std::nullopt, stand,
                                    c.tracker.last_at());
      c.out.move = Move::Walk;
      speed = move_speed(c.sprint, Move::Run) * step.pace;
      to = step.to ? step.to : std::optional<Point>(ghostAt);
      break;
    }
    default:  // Lurking, LockedOut, Moping and Excited
      if (next == Phase::Lurking) {
        // The ghost's own states move it, as any ghost's: the figure walks while it moves and stands while it stops.
        const phasmo::Vector3 v = agent ? phasmo::NavMeshAgent::get_velocity(agent) : phasmo::Vector3{};
        c.out.move = std::hypot(v.x, v.z) >= kStillPace ? Move::Walk : Move::Stand;
        // Never through a door: its own movement waits, its speed kept, while it opens one too narrow to pass.
        if (agent && hold_at_doors(agent, ghostAt, t)) {
          if (!c.lurkSpeed) c.lurkSpeed = phasmo::NavMeshAgent::get_speed(agent);
          phasmo::NavMeshAgent::set_speed(agent, 0.0f);
          c.out.move = Move::Stand;
        } else if (agent && c.lurkSpeed) {
          phasmo::NavMeshAgent::set_speed(agent, *c.lurkSpeed);
          c.lurkSpeed.reset();
        }
        break;
      }
      c.out.move = Move::Stand;
      to = ghostAt;
      // A moping Squatter never sits inside a hiding place the difficulty closed, where nobody could reach it (user,
      // 2026-11-09): it walks at the ghost's own base speed to the closest valid place, and sits there.
      if (next == Phase::Moping && !c.seat && t >= c.nextSeatCheck) {
        c.nextSeatCheck = t + 500;
        if (const std::optional<phasmo::Bounds> zone = closed_hiding_place_at(ghostAt)) {
          c.seat = closest_valid_place(ghostAt, *zone);
          if (c.seat && c.out.act == static_cast<int8_t>(Act::Sit)) c.out.act = -1;
        }
      }
      if (c.seat) {
        if (std::hypot(c.seat->x - ghostAt.x, c.seat->z - ghostAt.z) >= kStandReach) {
          c.out.move = Move::Walk;
          speed = phasmo::GhostAI::baseSpeed.get(ghost);
          to = c.seat;
        } else {
          c.seat.reset();
          act(Act::Sit, 0, t, false);
        }
      }
      break;
  }

  // Moving about: shut doors opened quietly, stalls and clumsy bumps.
  if (next == Phase::Chasing || next == Phase::Searching || next == Phase::Following) {
    if (hold_at_doors(agent, ghostAt, t)) speed = 0.0f;
    phasmo::Vector3 v = phasmo::NavMeshAgent::get_velocity(agent);
    const float pace = std::hypot(v.x, v.z);
    if (!paused && c.stuck.update(pace, speed, t)) act(Act::Bump, kBumpPauseMs, t);
  }
  if (t < c.pauseUntil) speed = 0.0f;
  // Only a phase the mod steers sets the agent: while it lurks the ghost's own states do.
  if (steers(next)) {
    c.out.move = rest_move(c.out.move, speed);  // standing, never running on the spot
    if (agent) steer(agent, to, speed, t);
    // A roll, a trip or a landing carries it as far as its clip travels (root motion), on the navigation mesh.
    if (agent && next == Phase::Chasing && moves_by_clip(c.out.move) && c.out.act < 0) {
      const phasmo::Vector3 step = figure::clip_step();
      if (step.x != 0.0f || step.z != 0.0f) {
        phasmo::Vector3 there = phasmo::Transform::get_position(phasmo::Component::get_transform(agent));
        there.x += step.x;
        there.z += step.z;
        phasmo::NavMeshAgent::Warp(agent, &there);
        c.nextSteer = 0;
      }
    }
  }

  c.out.phase = next;
  c.out.lookAt = look_target(next, c.target);
  if (share(c.out)) c.lastSent = t;
  else if (t - c.lastSent >= kShownResendMs) {  // the same Shown again, for a client that missed it
    const std::vector<uint8_t> message = shown_message(c.out);
    net::send(message.data(), message.size(), net::To::Others, true);
    c.lastSent = t;
  }
}

// The hinged door nearest the ghost within kReachDoorMetres, among its room's doors, as door_to_open looks, and the
// doors of `closet`, the hiding place of the player it is after (hiding_way_in): the one a door clip opens, on this
// client. Null when none is that near.
void* door_near(Point ghostAt, void* closet) {
  void* level = game::level_controller();
  void* room = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  const api::RefArray roomDoors{room ? phasmo::LevelRoom::doors.get(room) : nullptr};
  std::vector<void*> doors;
  for (size_t i = 0; i < roomDoors.size(); ++i) doors.push_back(roomDoors[i]);
  if (closet && api::alive(closet)) {
    doors.push_back(phasmo::ClosetZone::leftClosetDoor.get(closet));
    doors.push_back(phasmo::ClosetZone::rightClosetDoor.get(closet));
  }
  void* best = nullptr;
  float nearest = kReachDoorMetres;
  for (void* d : doors) {
    const std::optional<door::Leaf> leaf = door::leaf_of(d);
    if (!leaf) continue;
    const float metres = std::hypot(leaf->centre.x - ghostAt.x, leaf->centre.z - ghostAt.z);
    if (metres < nearest) {
      nearest = metres;
      best = d;
    }
  }
  return best;
}

// As a door clip starts, the hand nearer the door's handle takes it: the handle near the leaf's far edge at handle
// height on the Squatter's side, kept in the leaf's own space so it swings with the door.
void start_reach(Point ghostAt, void* closet) {
  g.reachDoor = nullptr;
  void* d = door_near(ghostAt, closet);
  const std::optional<door::Leaf> leaf = d ? door::leaf_of(d) : std::nullopt;
  if (!leaf) return;
  const float along = 2.0f * kHandleAlong;
  phasmo::Vector3 handle{leaf->pivot.x + (leaf->centre.x - leaf->pivot.x) * along, ghostAt.y + kHandleHeight,
                         leaf->pivot.z + (leaf->centre.z - leaf->pivot.z) * along};
  const float dx = ghostAt.x - handle.x, dz = ghostAt.z - handle.z, toward = std::hypot(dx, dz);
  if (toward > 0.0f) {
    handle.x += dx / toward * kHandleOut;
    handle.z += dz / toward * kHandleOut;
  }
  g.reachDoor = d;
  g.reachLocal = qrot(qinv(leaf->rotation), {handle.x - leaf->pivot.x, handle.y - leaf->pivot.y,
                                              handle.z - leaf->pivot.z});
  g.reachHand = figure::nearer_hand(handle);
  g.reachSince = now();
}

// The step clips a player's FootstepController holds for surface `surface` of kStepSurfaces, running or walking: an
// AudioClip[], or null.
void* step_clips(void* steps, int surface, bool running) {
  using F = phasmo::FootstepController;
  switch (surface) {
    case 0: return running ? F::runCarpetAudioClips.get(steps) : F::carpetAudioClips.get(steps);
    case 1: return running ? F::runConcreteAudioClips.get(steps) : F::concreteAudioClips.get(steps);
    case 2: return running ? F::runWoodAudioClips.get(steps) : F::woodAudioClips.get(steps);
    case 3: return running ? F::runOldWoodAudioClips.get(steps) : F::oldwoodAudioClips.get(steps);
    case 4: return running ? F::runGrassAudioClips.get(steps) : F::grassAudioClips.get(steps);
    case 5: return running ? F::runSnowAudioClips.get(steps) : F::snowAudioClips.get(steps);
    case 6: return running ? F::runTruckAudioClips.get(steps) : F::truckAudioClips.get(steps);
    case 7: return running ? F::runWetAudioClips.get(steps) : F::wetAudioClips.get(steps);
    case 8: return running ? F::runGlassAudioClips.get(steps) : F::glassAudioClips.get(steps);
    case 9: return running ? F::runMetalAudioClips.get(steps) : F::metalAudioClips.get(steps);
    case 10: return running ? F::runGravelAudioClips.get(steps) : F::gravelAudioClips.get(steps);
    case 11: return running ? F::runVinylAudioClips.get(steps) : F::vinylAudioClips.get(steps);
    case 12: return running ? F::runHeavyconcreteAudioClips.get(steps) : F::heavyconcreteAudioClips.get(steps);
    case kStairsSurface: return F::stairsAudioClips.get(steps);
    default: return nullptr;
  }
}

// One footstep of the Squatter at `at`, as a player's footsteps choose theirs (FootstepController.AttemptPlaySound): the
// tag of what lies under it on the footsteps' own layers picks the surface, and one of that surface's clips plays, the
// running ones while it runs (user, 2026-11-09: the players' own step sounds, kept per surface). The clips and layers
// are this client's own player's; nothing plays on a surface the game's footsteps do not know. Played through the mod's
// own sound, never the player's PlayerNoise, so no ghost hears it as a player.
void play_step(Point at, bool running, uint32_t roll) {
  void* player = game::local_player();
  void* steps = player ? phasmo::FootstepController::footstepController.get(player) : nullptr;
  if (!steps || !api::alive(steps)) return;
  void* under = game::raycast({at.x, at.y + 0.5f, at.z}, {0, -1, 0}, 1.5f, phasmo::FootstepController::mask.get(steps));
  void* tag = under ? phasmo::Component::get_tag(under) : nullptr;
  const int surface = tag ? surface_of(api::to_utf8(tag)) : -1;
  const api::RefArray clips{surface >= 0 ? step_clips(steps, surface, running) : nullptr};
  if (clips.size() == 0) return;
  size_t pick = roll % clips.size();
  if (clips.size() > 1 && clips[pick] == g.lastStep) pick = (pick + 1) % clips.size();  // never the same twice running
  void* clip = clips[pick];
  if (!clip) return;
  g.lastStep = clip;
  sound::play_at(clip, vector(at), 1.0f);
}

// The sound of the door of hiding place `closet` nearer `ghostAt` being slammed, on this client, as the door's own slam
// RPC's body plays it (Door.PlayDoorSlamNoise): one of the door's slam clips, or of the game's shared ones when it has
// none (slam_pick), through the door's Noise at the game's volume (kSlamVolume). Every client plays it from the kick's
// cue rather than the master sending the RPC, which only the room's first actor may (sdk/systems/photon.md).
void slam(void* closet, Point ghostAt, uint32_t roll) {
  if (!closet || !api::alive(closet)) return;
  void* d = nullptr;
  float nearest = 0.0f;
  for (void* each : {phasmo::ClosetZone::leftClosetDoor.get(closet), phasmo::ClosetZone::rightClosetDoor.get(closet)}) {
    const std::optional<phasmo::Vector3> m = door_middle(each);
    if (!m) continue;
    const float metres = std::hypot(m->x - ghostAt.x, m->z - ghostAt.z);
    if (!d || metres < nearest) {
      d = each;
      nearest = metres;
    }
  }
  void* noise = d ? phasmo::Door::noise.get(d) : nullptr;
  if (!noise || !api::alive(noise)) return;
  const api::RefArray own{phasmo::Door::slamClips.get(d)};
  void* sounds = phasmo::SoundController::instance.get();
  const api::RefArray shared{sounds && api::alive(sounds) ? phasmo::SoundController::doorSlamClips.get(sounds)
                                                          : nullptr};
  const std::optional<SlamPick> pick = slam_pick(own.size(), shared.size(), roll);
  void* clip = pick ? (pick->own ? own[pick->index] : shared[pick->index]) : nullptr;
  if (clip) phasmo::Noise::PlaySound(noise, clip, kSlamVolume, false, false);
}

// Every client plays the Shown: the master's own on the master, the master's last word elsewhere.
void play(void* ghost) {
  if (!g.shown.last || !figure::ready()) return;
  const Shown& s = *g.shown.last;
  const Cues c = g.cues.take(s);
  const uint64_t t = now();
  if (c.act && s.act >= 0) {
    figure::play_act(static_cast<Act>(s.act));
    g.tripSince.reset();  // an act ends the get-up where it is
  }
  // A trip plays on into getting up (the bundle's Stumble); a move the master sends while the figure is still down waits
  // until it stands.
  if (c.move && !(g.tripSince && s.move != Move::Stumble)) figure::play(s.move, s.variant);
  if (c.move && s.move == Move::Stumble && s.act < 0) g.tripSince = t;
  // Its walking and running keep pace with how fast the ghost moves here (figure::set_pace), smoothed over a moment.
  const Point here = at_of(ghost);
  if (g.paceFrom && t > g.paceAt) {
    const float seconds = static_cast<float>(t - g.paceAt) / 1000.0f;
    const float raw = std::hypot(here.x - g.paceFrom->x, here.z - g.paceFrom->z) / seconds;
    g.pace += (raw - g.pace) * std::min(1.0f, seconds / kPaceSmoothSeconds);
  }
  g.paceFrom = here;
  g.paceAt = t;
  figure::set_pace(s.move, g.pace);

  const float dt = g.playAt && t > g.playAt ? static_cast<float>(t - g.playAt) / 1000.0f : 0.0f;
  g.playAt = t;

  // Its head turns to the player it is after (Shown.lookAt), at their eyes, blending in and out.
  void* watched = nullptr;
  if (s.lookAt) watched = s.lookAt == game::local_actor() ? game::local_player() : game::player_of_actor(s.lookAt);
  if (watched) {
    const Point p = at_of(watched);
    g.lookPoint = {p.x, p.y + kEyeHeight, p.z};
  }
  g.lookWeight = approach(g.lookWeight, watched ? 1.0f : 0.0f, dt / kLookBlendSeconds);
  figure::look_at(g.lookPoint, g.lookWeight);

  // Its kick at a hiding place slams the door of the hiding place the player it is after hides in, as the foot lands.
  if (c.act && s.act == static_cast<int8_t>(Act::Bang)) {
    g.slamAt = t + kSlamDelayMs;
    g.slamCloset = watched ? phasmo::Player::closet.get(watched) : nullptr;
  }
  if (g.slamAt && t >= g.slamAt) {  // the kick's foot lands, whatever the phase has become since
    g.slamAt = 0;
    slam(g.slamCloset.get(), here, static_cast<uint32_t>(g.stepRng()));
    g.slamCloset = nullptr;
  }

  // In a door clip, the nearer hand takes the door's handle and follows it as the leaf swings, blending in and out.
  const bool doorAct =
      s.act == static_cast<int8_t>(Act::OpenDoorOutwards) || s.act == static_cast<int8_t>(Act::OpenDoorInwards);
  if (c.act && doorAct) start_reach(here, watched ? phasmo::Player::closet.get(watched) : nullptr);
  const bool reaching = doorAct && api::alive(g.reachDoor) &&
                        t - g.reachSince >= static_cast<uint64_t>(kReachDelaySeconds * 1000.0f);
  g.reachWeight = approach(g.reachWeight, reaching ? 1.0f : 0.0f, dt / kReachBlendSeconds);
  const std::optional<door::Leaf> leaf = g.reachWeight > 0.0f ? door::leaf_of(g.reachDoor.get()) : std::nullopt;
  if (leaf) {
    const phasmo::Vector3 turned = qrot(leaf->rotation, g.reachLocal);
    figure::reach(g.reachHand, {leaf->pivot.x + turned.x, leaf->pivot.y + turned.y, leaf->pivot.z + turned.z},
                  g.reachWeight);
  } else {
    figure::reach(g.reachHand, {}, 0.0f);
  }
  if (!doorAct && g.reachWeight <= 0.0f) g.reachDoor = nullptr;

  // Its own clips around the master's moves, as games blend locomotion: setting off from standing plays Start Walking
  // before the walk, and standing it turns on the spot with a turn clip once its heading pulls far enough away. A new
  // act or move from the master ends either.
  if (c.act || c.move) g.local = State::Local::None;
  if (c.move && s.act < 0 && s.move == Move::Walk && g.lastMove == Move::Stand && !g.tripSince) {
    figure::play_clip(kStartWalkState);
    g.local = State::Local::Starting;
    g.localUntil = t + static_cast<uint64_t>(kStartWalkSeconds * 1000.0f);
  }
  if (c.move) g.lastMove = s.move;
  if (g.local != State::Local::None && t >= g.localUntil) {
    g.local = State::Local::None;
    if (s.act < 0) figure::play(s.move, s.variant);
  }
  const float heading = heading_of(ghost);
  if (!g.facing) g.facing = heading;
  // Stood up from a trip: the get-up stands it facing kStandUpTurnDegrees round from its root, so the root turns as far
  // and the figure snaps to standing in the same frame, the body staying as it stands; then it plays the master's move,
  // and turns back to its way as it always turns.
  if (g.tripSince && stood_up(*g.tripSince, t)) {
    g.tripSince.reset();
    g.facing = risen_facing(*g.facing);
    show(ghost);
    figure::snap_to(state_for(Move::Stand, 0));
    if (s.act < 0 && s.move != Move::Stumble) figure::play(s.move, s.variant);
  }
  if (g.tripSince) {
    // Down from a trip: it lies the way it fell, whichever way the ghost turns.
  } else if (g.local == State::Local::Turning) {
    g.facing = wrap_angle(*g.facing + figure::clip_turn());  // the clip's own turn
  } else if (g.pace >= kStillPace || s.act >= 0 || s.move != Move::Stand || g.local == State::Local::Starting) {
    // A duck or a weave goes side-on, quickly, so its sidestep carries it along its way (move_facing).
    const bool sideOn = s.act < 0 && (s.move == Move::Duck || s.move == Move::Weave);
    const float rate = kTurnRateDegrees * (sideOn ? 2.0f : 1.0f) * std::numbers::pi_v<float> / 180.0f;
    g.facing = turn_toward(*g.facing, move_facing(s.move, s.variant, heading), rate * dt);
  } else if (const Turn turn = turn_for(heading - *g.facing); turn != Turn::None) {
    const char* clip = turn == Turn::Around ? kTurnAroundState : turn == Turn::Right ? kTurnRightState : kTurnLeftState;
    figure::play_clip(clip);
    g.local = State::Local::Turning;
    g.localUntil = t + static_cast<uint64_t>(seconds_of(clip) * 1000.0f);
  }
  if (c.line && s.lineSet >= 0 && s.lineIndex >= 0)
    voice::play_index(static_cast<Lines>(s.lineSet), s.lineIndex, vector(at_of(ghost)));
  // A footstep each stride it moves, on this client, from its own view of the ghost's movement.
  const Point at = at_of(ghost);
  if (g.stepFrom && g.steps.moved(std::hypot(at.x - g.stepFrom->x, at.z - g.stepFrom->z)))
    play_step(at, s.move == Move::Run || plays_once(s.move), static_cast<uint32_t>(g.stepRng()));
  g.stepFrom = at;
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  door::on_attempt(&tried);
  return door::create_hooks();
}

void tick(void* haunting, int type, bool /*steered: a player's own lines come in a later task*/) {
  const bool squatter = haunting && type == kSquatter;
  door::set_haunting(squatter);
  if (!squatter) {
    g.phase.reset();
    g.steered = false;
    close_obstacle();  // a closet it opened goes back as it was
    g.chase = Chase{};
    g.kicked.clear();
    g.steps.reset();
    g.stepFrom.reset();
    g.paceFrom.reset();
    g.pace = 0.0f;
    g.lookWeight = 0.0f;
    g.reachDoor = nullptr;
    g.reachWeight = 0.0f;
    g.facing.reset();
    g.local = State::Local::None;
    g.lastMove.reset();
    g.slamAt = 0;
    g.slamCloset = nullptr;
    g.tripSince.reset();
    forget_channel();
    figure::destroy();
    if (!haunting) ghost_sight::drop();  // shared (ghost_sight.h): kept while the contract's ghost lives
    // The exits' lock is kept: it belongs to the map, and squatter_door forgets it once the map's LevelController
    // is gone, so a tick without the ghost mid-contract never locks them twice.
    return;
  }
  const bool master = game::is_master();
  // The first phase waits for the difficulty: guessed once, it would hold for the whole contract.
  if (!g.phase)
    if (const std::optional<bool> friendlyGhost = friendly_if_read()) g.phase = first_phase(*friendlyGhost);
  g.phase = kept_phase(g.phase, g.shown.last, master);
  // The first frame the mod steers the master's ghost, it leaves wandering or the favourite room for idling, which
  // restraint lets through; the agent's speed is set after this change, since idling's start sets its own.
  if (master && steering_begins(g.steered, g.phase))
    phasmo::GhostAI::ChangeState(haunting, phasmo::GhostState::idle(), nullptr, false);
  // The frame it stops steering (it lost interest at a hiding place and lurks), the master idles the ghost again: the
  // state's start gives the agent the ghost's own speed in place of the mod's, and its roll moves on to wandering.
  if (master && steering_ends(g.steered, g.phase))
    phasmo::GhostAI::ChangeState(haunting, phasmo::GhostState::idle(), nullptr, false);
  g.steered = master && steering();
  tell_sprint(master);
  if (master) {
    g.sprints.drop_stale(now());
    // The chase, after the idle change above, since idling's start sets the agent's speed its own way.
    if (g.phase) run(haunting, friendly(), now());
    door::relock(now());  // locks again any held exit a raced Monkey Paw wish reopened
  } else {
    close_obstacle();   // a closet this client opened while it was the master goes back as it was
    g.chase = Chase{};  // a client that becomes the master starts its chase afresh from the last Shown's phase
  }
  show(haunting);
  play(haunting);
}

std::optional<Phase> phase() { return g.phase; }

bool steering() { return g.phase && steers(*g.phase); }

void receive(int sender, const uint8_t* data, size_t size) {
  switch (net_msg::kind_of(data, size)) {
    case net_msg::kSquatter: g.shown.take(sender, game::master_actor(), data, size); break;
    case net_msg::kSquatterTry: {
      std::vector<int> room;
      game::room_actors(room);
      if (try_from(sender, game::is_master(), room, data, size)) g.tries.report(sender, now());
      break;
    }
    case net_msg::kSquatterSprint: {
      std::vector<int> room;
      game::room_actors(room);
      if (Sprint s; sprint_from(sender, game::is_master(), room, data, size, s)) g.sprints.report(sender, s, now());
      break;
    }
    default: break;
  }
}

std::optional<Shown> shown() { return g.shown.last; }

bool share(const Shown& s) {
  if (!game::is_master() || (g.shown.last && *g.shown.last == s)) return false;
  g.shown.last = s;
  const std::vector<uint8_t> message = shown_message(s);
  net::send(message.data(), message.size(), net::To::Others, true);
  return true;
}

std::vector<int> take_tries() { return game::is_master() ? g.tries.take() : std::vector<int>{}; }

std::optional<Sprint> sprint_of(int actor) { return g.sprints.of(actor, now()); }

bool kick(void* player, int id) {
  if (!game::is_master() || !player || !api::alive(player)) return false;
  // Unreadable counts as friendly: the kill's animation refuses on a friendly ghost's difficulty, with a report.
  const std::optional<bool> friendlyGhost = friendly_if_read();
  if (!friendlyGhost || *friendlyGhost) return false;
  if (phasmo::Player::isDead.get(player) || g.kicked.contains(id)) return false;
  g.kicked.insert(id);
  phasmo::Player::BeginDeathSequence(player, phasmo::DeathType::SnappedNeck());
  return true;
}

void reset() {
  close_obstacle();
  g = State{};
  figure::destroy();
  ghost_sight::drop();
  door::set_haunting(false);
  door::reset();
}

}  // namespace pm::custom::squatter
