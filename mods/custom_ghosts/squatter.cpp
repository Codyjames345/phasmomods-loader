#include "squatter.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <random>
#include <set>
#include <type_traits>
#include <vector>

#include "ClosetZone.h"
#include "Difficulty.h"
#include "Door.h"
#include "GhostAI.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "game.h"
#include "game_api.h"
#include "ghost_sight.h"
#include "ghosts.h"
#include "net.h"
#include "players.h"
#include "runtime.h"
#include "squatter_chase.h"
#include "squatter_difficulty.h"
#include "squatter_door.h"
#include "squatter_figure.h"
#include "squatter_voice.h"

namespace pm::custom::squatter {
namespace {

// What the master keeps of its chase (squatter_chase.h), forgotten with the phase.
struct Chase {
  Tracker tracker;
  int target = -1;           // the target's id (id_of), -1 for none
  bool hidingKnown = false;  // it knows the target's hiding place (knows_hiding)
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
  std::optional<Point> openingDoor, openingForward;  // the door being opened quietly, for its clip
  float sprint = 0.0f;                       // the sprint speed it last chased at (chase_sprint)
  float seed = 0.0f;                         // offsets the move cycle per chase
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
};
State g;

uint64_t now() { return GetTickCount64(); }

// Every client shows its own figure where the contract's ghost is, facing the way it faces.
void show(void* ghost) {
  void* t = phasmo::Component::get_transform(ghost);
  if (!t) return;
  const phasmo::Vector3 at = phasmo::Transform::get_position(t);
  if (!figure::ready()) figure::create(at);
  const phasmo::Vector3 f = phasmo::Transform::get_forward(t);
  figure::place(at, std::atan2(f.x, f.z));
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

// Where to walk to, renewed every kSteerMs; at most every frame's speed is set after the idle change (tick).
void steer(void* agent, std::optional<Point> to, float speed, uint64_t t) {
  phasmo::NavMeshAgent::set_speed(agent, speed);
  Chase& c = g.chase;
  if (!to || t < c.nextSteer) return;
  c.nextSteer = t + kSteerMs;
  if (!game::set_destination(agent, vector(*to))) c.searchAt.reset();  // off the navigation mesh: pick another
}

// The shut door of the ghost's room it has reached, if one may be opened quietly (squatter_door.h may_open).
void* door_to_open(Point ghostAt) {
  void* level = game::level_controller();
  void* room = level ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  const api::RefArray doors{room ? phasmo::LevelRoom::doors.get(room) : nullptr};
  for (size_t i = 0; i < doors.size(); ++i) {
    void* d = doors[i];
    if (d && api::alive(d) && at_door(ghostAt, at_of(d)) && door::may_open(d)) return d;
  }
  return nullptr;
}

// The master's quiet door opening on the chase: it stops at a shut door, opens it with the game's quiet opener and
// plays the clip for its side (squatter_chase.h door_act). True while it waits for the door.
bool open_doors(Point ghostAt, uint64_t t) {
  Chase& c = g.chase;
  door::Opening o = door::open_step(t);
  if (o == door::Opening::Idle && t >= c.pauseUntil) {
    if (void* d = door_to_open(ghostAt)) {
      c.openingDoor = at_of(d);
      c.openingForward = point(game::forward(game::transform_of(d)));
      o = door::open_quietly(d, t);
    }
  }
  if (o == door::Opening::Opened && c.openingDoor && c.openingForward)
    act(door_act(ghostAt, *c.openingDoor, *c.openingForward), kDoorPauseMs, t);
  if (o != door::Opening::Waiting) c.openingDoor.reset();
  return o == door::Opening::Waiting;
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
  for (const Seen& s : players) {
    const bool sensed = c.seen.contains(s.id) || c.heard.contains(s.id);
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
  const bool targetSensed = target && (c.seen.contains(target->id) || c.heard.contains(target->id));
  if (targetSensed) c.tracker.sensed(target->id, target->at, t);
  c.hidingKnown = target && knows_hiding(target->closet != nullptr, targetSensed || c.tracker.tracks(t), c.hidingKnown);

  bool anySeen = false;
  for (const Seen& s : players) anySeen = anySeen || (s.living && s.inside && c.seen.contains(s.id));
  const Phase now = *g.phase;
  // Nothing is chased or kicked by a figure nobody can see (one that failed to build is logged by squatter_figure).
  const bool shown = figure::ready();
  const Facts f{friendlyGhost, sees_for_phase(now, anySeen, targetSensed, shown),
                shown && c.target >= 0 && c.tracker.tracks(t), c.hidingKnown, door::living_inside(),
                t >= c.pauseUntil};
  if (doors_lock(now, f)) door::lock_all();
  const Phase next = next_phase(now, f);
  g.phase = next;

  // A new phase: its first line at once, its stance.
  if (c.was != next) {
    c.phaseSince = t;
    c.lines.reset();
    c.searchAt.reset();
    c.searchFrom.reset();
    if (const std::optional<Act> stance = stance_for(next)) {
      act(*stance, 0, t, false);  // held for the whole phase
    } else if (const std::optional<Act> rise = entry_act(c.was, next)) {
      act(*rise, kGetUpMs, t);
    } else if (c.out.act == static_cast<int8_t>(Act::Sit)) {
      c.out.act = -1;
    }
    if (next == Phase::Chasing && c.was == Phase::Lurking) {
      c.seed = static_cast<float>(roll() % 1000u) / 100.0f;
      c.nextBump = t + kBumpGapMinMs + roll() % (kBumpGapMaxMs - kBumpGapMinMs);
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
  switch (next) {
    case Phase::Chasing: {
      const float seconds = static_cast<float>(t - c.phaseSince) / 1000.0f + c.seed;
      const Move m = move_at(seconds);
      if (m != c.out.move) {
        c.out.move = m;
        c.out.variant = static_cast<uint8_t>(roll() & 0xFF);
      }
      speed = move_speed(c.sprint, m);
      to = targetSensed ? std::optional<Point>(target->at) : c.tracker.last_at();
      // The catch: a kick to the face, a line, and the game's own kill (kick).
      if (target && !paused &&
          // may_kick's distance is given as 0 or past reach: within_kick measures it in three dimensions.
          may_kick(next, friendlyGhost, target->living, within_kick(ghostAt, target->at) ? 0.0f : kKickReach + 1.0f,
                   kKickReach) &&
          shown && kick(target->player, target->id)) {
        act(Act::Kick, kKickMs, t);
        say(Lines::Catch, t);
        c.tracker.forget();
        c.target = -1;
      }
      break;
    }
    case Phase::Searching: {
      c.out.move = Move::Run;
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
      void* stand = target && target->closet ? phasmo::ClosetZone::standOutsideEntrancePoint.get(target->closet)
                                             : nullptr;
      const std::optional<Point> spot = stand ? std::optional<Point>(point(game::position(stand))) : std::nullopt;
      const bool there = spot && std::hypot(spot->x - ghostAt.x, spot->z - ghostAt.z) < kStandReach;
      c.out.move = Move::Run;
      speed = there ? 0.0f : move_speed(c.sprint, Move::Run);
      to = there ? std::optional<Point>(ghostAt) : spot;
      if (there && t >= c.nextBang && !paused) {  // demands they come out, kicking the door over and over
        act(Act::Bang, kBangGapMs / 2, t);
        c.nextBang = t + kBangGapMs;
      }
      break;
    }
    case Phase::Following: {  // friendly: after its target, waiting and talking at a hiding place, never banging
      void* stand = c.hidingKnown && target && target->closet
                        ? phasmo::ClosetZone::standOutsideEntrancePoint.get(target->closet)
                        : nullptr;
      const bool known = target && (targetSensed || c.tracker.tracks(t));
      const Step step = follow_step(ghostAt, known ? std::optional<Point>(target->at) : std::nullopt,
                                    stand ? std::optional<Point>(point(game::position(stand))) : std::nullopt,
                                    c.tracker.last_at());
      c.out.move = Move::Run;
      speed = move_speed(c.sprint, Move::Run) * step.pace;
      to = step.to ? step.to : std::optional<Point>(ghostAt);
      break;
    }
    default:  // Lurking, LockedOut, Moping and Excited: held where it sits or gets up
      to = ghostAt;
      break;
  }

  // Moving about: shut doors opened quietly, stalls and clumsy bumps.
  if (next == Phase::Chasing || next == Phase::Searching || next == Phase::Following) {
    if (open_doors(ghostAt, t)) speed = 0.0f;
    phasmo::Vector3 v = phasmo::NavMeshAgent::get_velocity(agent);
    const float pace = std::hypot(v.x, v.z);
    if (!paused && c.stuck.update(pace, speed, t)) act(Act::Bump, kBumpPauseMs, t);
    if (c.nextBump == 0)  // a chase this master did not start (a host migration): the first bump comes later
      c.nextBump = t + kBumpGapMinMs + roll() % (kBumpGapMaxMs - kBumpGapMinMs);
    if (next == Phase::Chasing && !paused && t >= c.nextBump) {
      act(Act::Bump, kBumpPauseMs, t);
      c.nextBump = t + kBumpGapMinMs + roll() % (kBumpGapMaxMs - kBumpGapMinMs);
    }
  }
  if (t < c.pauseUntil) speed = 0.0f;
  if (agent) steer(agent, to, speed, t);

  c.out.phase = next;
  if (share(c.out)) c.lastSent = t;
  else if (t - c.lastSent >= kShownResendMs) {  // the same Shown again, for a client that missed it
    const std::vector<uint8_t> message = shown_message(c.out);
    net::send(message.data(), message.size(), net::To::Others, true);
    c.lastSent = t;
  }
}

// Every client plays the Shown: the master's own on the master, the master's last word elsewhere.
void play(void* ghost) {
  if (!g.shown.last || !figure::ready()) return;
  const Shown& s = *g.shown.last;
  const Cues c = g.cues.take(s);
  if (c.act && s.act >= 0) figure::play_act(static_cast<Act>(s.act));
  if (c.move) figure::play(s.move, s.variant);
  if (c.line && s.lineSet >= 0 && s.lineIndex >= 0)
    voice::play_index(static_cast<Lines>(s.lineSet), s.lineIndex, vector(at_of(ghost)));
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
    g.chase = Chase{};
    g.kicked.clear();
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
  g.steered = master && steering();
  tell_sprint(master);
  if (master) {
    g.sprints.drop_stale(now());
    // The chase, after the idle change above, since idling's start sets the agent's speed its own way.
    if (g.phase) run(haunting, friendly(), now());
    door::relock(now());  // locks again any held exit a raced Monkey Paw wish reopened
  } else {
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
  g = State{};
  figure::destroy();
  ghost_sight::drop();
  door::set_haunting(false);
  door::reset();
}

}  // namespace pm::custom::squatter
