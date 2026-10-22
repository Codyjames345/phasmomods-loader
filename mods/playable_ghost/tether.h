#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

// Vanilla rules that still apply to the controlled ghost, kept free of game calls so they can be
// tested offline. The facts come from GhostAI and its states (build 25811160, see the offsets in
// actions.cpp and control.cpp).
//
// Movement. A vanilla ghost only moves in a few states. A wander (state 1, and its variant 21) walks
// to a random point near the ghost: within the state's radius (+0x24, three metres, sometimes up to
// five), towards its target for a Banshee, and it ends on arrival or when its timer runs out. The
// favourite-room state walks the ghost home, a hunt chases, and events and abilities move it their
// own way. In every other state, idle included, it stands where it is, and the idle roll every 2-6 s
// chooses what comes next. The ghost player takes over the wander's walk (anywhere within that
// wander's reach) and the hunt's; the native AI keeps the rest.
//
// Smudge in a hunt (GhostAI.StopHuntingForTime): the native hunt loses its target for the incense's
// duration, so the native AI moves the ghost meanwhile instead of its player.
namespace pg::tether {

// GhostAI.State values.
constexpr int kIdle = 0;
constexpr int kWander = 1;
constexpr int kHunting = 2;
constexpr int kFavouriteRoom = 3;
constexpr int kFuseBox = 7;
constexpr int kRandomEvent = 14;
constexpr int kAbility = 15;
constexpr int kRoam = 21;  // the wander's variant (same layout: timer, radius, destination)

struct Ghost {
  int state = kIdle;
  bool inFavouriteRoom = false;  // LevelController's ghost room is the GhostInfo favourite room
  bool smudged = false;          // GhostAI +0xF8: the incense that smudged it in a hunt
};

// Who moves the ghost this frame.
//   kSteer    the player, freely (a hunt)
//   kRoaming  the player, within the current wander's reach
//   kResting  nobody: the ghost holds still, as a vanilla ghost does between wanders
//   the rest  the native AI: walking home, blinded by smoke, an event, or settling in its room / an ability
enum Reason { kSteer, kRoaming, kResting, kDrawnHome, kBlinded, kEvent, kActing };

inline Reason reason(const Ghost& g) {
  switch (g.state) {
    case kHunting: return g.smudged ? kBlinded : kSteer;
    case kWander:
    case kRoam: return kRoaming;
    case kFavouriteRoom: return g.inFavouriteRoom ? kActing : kDrawnHome;
    case kRandomEvent: return kEvent;
    case kAbility: return kActing;
    default: return kResting;
  }
}

inline bool native_moves(Reason r) { return r >= kDrawnHome; }

// How far the ghost player may walk during one wander: around where it began, as far as the native
// wander would have gone (its radius, or the point it chose when that is further, as a Banshee's is).
struct Reach {
  float x = 0, z = 0, radius = 0;
};

inline Reach reach_of(float startX, float startZ, int radius, bool hasDestination, float destX, float destZ) {
  float r = static_cast<float>(std::max(radius, 0));
  if (hasDestination) r = std::max(r, std::hypot(destX - startX, destZ - startZ));
  return {startX, startZ, r};
}

// The horizontal steering direction (dx, dz) a roaming ghost at (px, pz) may take: unchanged inside
// its reach, and without its outward part once at the edge, so it can still slide along it.
inline void keep_within(const Reach& r, float px, float pz, float& dx, float& dz) {
  const float ox = px - r.x, oz = pz - r.z;
  const float d = std::hypot(ox, oz);
  if (d < r.radius || d < 1e-4f) return;
  const float nx = ox / d, nz = oz / d;
  const float out = dx * nx + dz * nz;
  if (out <= 0) return;
  dx -= out * nx;
  dz -= out * nz;
}

// A hunt that loses sight of its target walks to where it last saw them and searches there. Its
// line-of-sight speed-up (hunt state +0xB4) holds until that search is over and only then decays.
// The ghost player may walk elsewhere, so the search ends where the ghost is once a vanilla ghost,
// walking the path at its native speed, would have reached the search point. Walking there ends it
// natively, sooner.
class SearchPace {
 public:
  // Every frame of a hunt. searching: the hunt has a search point and no target. (dx, dz) is the
  // search point (the agent's destination), pathLength the native path's length when it is known
  // (otherwise negative: measured straight until the path is known). True on the one frame the
  // search should end at (gx, gz).
  bool tick(bool searching, float gx, float gz, float dx, float dz, float pathLength, float speed, float dt) {
    if (!searching) {
      active_ = ended_ = false;
      return false;
    }
    const bool newPoint = !active_ || std::hypot(dx - destX_, dz - destZ_) > kMoved;
    const bool ours = ended_ && std::hypot(dx - endX_, dz - endZ_) <= kMoved;  // the end we set
    const bool known = pathLength >= 0 && std::isfinite(pathLength);
    if (newPoint && !ours) {
      active_ = true;
      ended_ = false;
      destX_ = dx;
      destZ_ = dz;
      walked_ = 0;
      pathKnown_ = known;
      length_ = known ? pathLength : std::hypot(dx - gx, dz - gz);
    } else if (!pathKnown_ && known && !ended_) {
      pathKnown_ = true;  // the path came in after the search began: it is the walk's length
      length_ = pathLength + walked_;
    }
    if (ended_) return false;
    walked_ += std::max(speed, 0.0f) * std::max(dt, 0.0f);
    if (walked_ < length_) return false;
    ended_ = true;
    endX_ = gx;
    endZ_ = gz;
    return true;
  }

 private:
  static constexpr float kMoved = 0.5f;  // metres: a different search point
  bool active_ = false, ended_ = false, pathKnown_ = false;
  float destX_ = 0, destZ_ = 0, endX_ = 0, endZ_ = 0, length_ = 0, walked_ = 0;
};

// A hunt the native idle roll allowed (every check and its final 1-in-3/4/5 chance passed) is offered
// to the ghost player until the roll next reaches its hunt check, and only while the native checks
// still pass.
struct HuntGate {
  bool hunting = false;   // GhostAI +0xF3
  bool cooldown = false;  // GhostAI +0xD8, set after every hunt
  bool smudged = false;   // GhostAI +0xF5, set by smudging outside a hunt
  float insanity = -1;    // the measured players' insanity now, negative when unknown
  bool haveModifier = false;
  int modifier = 0;       // the type's and difficulty's addition, learned from the idle roll
  bool thaye = false;     // the Thaye's hunt check is by its age instead
};

constexpr int kHuntInsanity = 50;  // the idle roll's hunt branch needs at least this

inline bool hunt_allowed(const HuntGate& g) {
  if (g.hunting || g.cooldown || g.smudged) return false;
  if (g.thaye || !g.haveModifier || g.insanity < 0) return true;
  return static_cast<int>(g.insanity) + g.modifier >= kHuntInsanity;
}

// The vanilla gates on the actions the idle roll chooses through GhostActivity.GhostAbility and
// TryInteract, checked again when the ghost player uses one the roll granted earlier.
struct ActionGate {
  bool mainDoorOpen = false;     // SetupPhaseController +0x44 (UnlockMainDoor): before it, no interactions
  bool noEvents = false;         // LevelValues +0x23 (set by ConnectToServer): no events, hunts or fuse box
  bool stayHome = false;         // GhostAI +0x10A: the roll sends the ghost home instead of acting
  bool shadeWatched = false;     // a Shade with an investigator in the room it is in
  bool interactionBoost = false; // GhostAI +0xD9 (the Tower card): the roll's actions run natively
};

inline bool may_interact(const ActionGate& g) { return g.mainDoorOpen; }

// A vanilla ghost only acts from its idle roll. The ghost player may also act during a wander, which
// a vanilla ghost leaves for idle as soon as it arrives, but never in the middle of another action.
inline bool may_act_from(int state) { return state == kIdle || state == kWander || state == kRoam; }

// An aimed interaction counts once the native chain chose a state for the aimed object. The throw
// state (6) is chosen from the sensor's whole detection count, so with the aim narrowed to something
// that cannot be thrown it arrives with no object: nothing would happen, and nothing is paid.
constexpr int kThrow = 6;
inline bool aimed_state_counts(int state, bool hasObject) { return state != kIdle && (state != kThrow || hasObject); }

// Restlessness: once the ghost player has not tried to act for kRestlessMs, the native roll acts on
// its own again. Any attempt counts, so a ghost player with nothing saved up takes the roll back by
// trying.
constexpr uint64_t kRestlessMs = 30000;
inline bool restless(uint64_t now, uint64_t lastAttempt) { return now - lastAttempt >= kRestlessMs; }
inline bool may_use_ability(const ActionGate& g) { return !g.stayHome && !g.shadeWatched; }
inline bool may_cause_event(const ActionGate& g) { return may_use_ability(g) && !g.noEvents; }

// The fuse box (state 7: from anywhere, with EMF on the box): power on, any type but the Jinn turns it
// off; power off, any type but the Hantu tries to turn it on (1 in 5). Never while the box is out of
// order or events are off.
enum FuseAction { kFuseNone, kFuseTurnOff, kFuseTurnOn };

constexpr int kJinn = 5;
constexpr int kHantu = 13;

inline FuseAction fuse_action(bool haveFuseBox, bool outOfOrder, bool noEvents, bool powerOn, int ghostType) {
  if (!haveFuseBox || outOfOrder || noEvents) return kFuseNone;
  if (powerOn) return ghostType == kJinn ? kFuseNone : kFuseTurnOff;
  return ghostType == kHantu ? kFuseNone : kFuseTurnOn;
}

}  // namespace pg::tether
