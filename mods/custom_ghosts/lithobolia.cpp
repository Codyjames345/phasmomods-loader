#include "lithobolia.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "Photon.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "Props.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghost_sight.h"
#include "ghosts.h"
#include "held.h"
#include "lithobolia_rule.h"
#include "log.h"
#include "net.h"
#include "players.h"
#include "runtime.h"
#include "sight_rule.h"

namespace pm::custom::lithobolia {
namespace {

using phasmo::Vector3;

// Decompiles of GhostActivity.InteractWithAProp/GetPropToThrow, the prop throw and
// PlayerSanity.ChangeSanity (build 25811160, 2026-10-20); the ownership calls as kikimora.cpp.
constexpr ULONGLONG kOwnershipWaitMs = 1500;
constexpr ULONGLONG kNoteMs = 5000;  // the throw diagnostics: one line at most this often
// The launch is readied as the game's own throw readies the object, in its order: the Rigidbody freed
// with PhotonObjectInteract.FreezeRigidbody(false, true) (the RPC that does the same on every client goes
// out only while the view's IsMine is set), then its PhotonView synchronised (UnreliableOnChange), then the
// force.
// It sees outside hunts through the shared copies of the ghost's vision (ghost_sight.h).

struct Throw {
  api::Held item;          // PhotonObjectInteract
  api::Held target;        // Player
  int actor = 0;           // the target's Photon actor
  ULONGLONG deadline = 0;  // waiting for ownership until then
  ULONGLONG landed = 0;    // launched: watched for a hit until then
} g;

ULONGLONG g_nextNote = 0;
int g_unnoted = 0;  // throws decided since the last diagnostic line

/** Whether a throw's diagnostic line may be written now (one per kNoteMs, never per frame). */
bool note_now() {
  ++g_unnoted;
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextNote) return false;
  g_nextNote = now + kNoteMs;
  return true;
}

Vec vec(Vector3 v) { return {v.x, v.y, v.z}; }

Vec chest_of(void* player) {
  const Vector3 p = game::position(game::transform_of(player));
  return {p.x, p.y + kLithoboliaChest, p.z};
}

/** The Photon actor of a networked Player, or 0. */
int actor_of(void* player) {
  std::vector<int> actors;
  game::room_actors(actors);
  for (int a : actors)
    if (game::player_of_actor(a) == player) return a;
  return 0;
}

/** Metres from the ghost's feet to a player's chest. */
float chest_distance(void* ghost, void* player) {
  const Vector3 at = game::position(game::transform_of(ghost));
  const Vec chest = chest_of(player);
  return length({chest.x - at.x, chest.y - at.y, chest.z - at.z});
}

/** The nearest living player the ghost's own vision sensor detects within range, and their actor; null for none. */
void* visible_target(void* ghost, int& actor, const char* why) {
  std::vector<void*> seen;
  std::vector<float> distances;
  int living = 0;
  // In a hunt the ghost's own sensor pulses (its sight test reads it); outside, the copies are pulsed now.
  const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
  const bool copied = !hunting && ghost_sight::pulse(ghost);
  game::each_player([&](void* player) {
    if (game::living(player)) ++living;
    if (!(hunting ? game::ghost_sees(ghost, player) : copied && ghost_sight::sees(player))) return;
    seen.push_back(player);
    distances.push_back(chest_distance(ghost, player));
  });
  const int best = nearest_in_range(distances, kLithoboliaRange);
  void* player = best >= 0 ? seen[best] : nullptr;
  actor = player ? actor_of(player) : 0;
  if (note_now()) {
    float nearest = -1;
    for (float d : distances) nearest = nearest < 0 ? d : std::min(nearest, d);
    PM_LOG("lithobolia: %s (%d since the last line, sight %s): %d living, %zu seen, nearest seen %.1f m -> %s", why,
           g_unnoted, hunting ? "the ghost's own" : copied ? "the copies" : "none", living, seen.size(), nearest,
           player ? "throws at them" : "nothing (nobody seen within 8 m)");
    g_unnoted = 0;
  }
  return player;
}

bool throwable(void* item) {
  if (!api::alive(item) || phasmo::PhotonObjectInteract::door.get(item)) return false;
  return api::alive(phasmo::PhotonObjectInteract::body.get(item)) && phasmo::PhotonObjectInteract::view.get(item);
}

void clear() { g = Throw{}; }

/** Aim and launch the taken item, with the EMF of a ghost's throw where it left. */
void launch(void* ghost) {
  void* rb = phasmo::PhotonObjectInteract::body.get(g.item);
  const bool wasKinematic = phasmo::Rigidbody::get_isKinematic(rb);
  // Readied as the game's throw readies it: freed (for every client while it is ours), then
  // synchronised. A velocity set on a kinematic Rigidbody does nothing, which is what 3f59df4..5a7bfc3 did.
  phasmo::PhotonObjectInteract::FreezeRigidbody(g.item, false, true);
  phasmo::PhotonView::Synchronization.set(phasmo::PhotonObjectInteract::view.get(g.item),
                                          phasmo::ViewSynchronization::UnreliableOnChange());
  const Vec from = vec(game::position(game::transform_of(g.item)));
  const Vec aim = aim_velocity(from, chest_of(g.target));
  const phasmo::Vector3 velocity{aim.x, aim.y, aim.z};
  phasmo::Rigidbody::set_velocity(rb, &velocity);
  if (void* interaction = phasmo::GhostAI::interaction.get(ghost))
    phasmo::GhostInteraction::CreateThrowingEMF(interaction, g.item.get(), phasmo::EMFType::objectMoved(), false,
                                                20.0f);
  g.landed = GetTickCount64() + static_cast<ULONGLONG>(kLithoboliaFlight * 1000);
  PM_LOG("lithobolia: launched at a player (kinematic before %d, after %d; aimed %.1f m/s, has %.1f m/s)",
         wasKinematic ? 1 : 0, phasmo::Rigidbody::get_isKinematic(rb) ? 1 : 0, length(aim),
         length(vec(phasmo::Rigidbody::get_velocity(rb))));
}

/** Apply a hit to this client's own player. */
void hit_me(float sanity) {
  void* me = game::local_player();
  void* mine = api::alive(me) ? phasmo::Player::sanity.get(me) : nullptr;
  if (api::alive(mine)) phasmo::PlayerSanity::ChangeSanity(mine, sanity);
}

}  // namespace

bool resolve() { return true; }

/** Take `prop` (a PhotonObjectInteract) over to throw it at `target` once the host owns it. */
void begin(void* prop, void* target, int actor) {
  g.item = prop;
  g.target = target;
  g.actor = actor;
  void* view = phasmo::PhotonObjectInteract::view.get(prop);
  if (!phasmo::PhotonView::get_IsMine(view)) phasmo::PhotonView::RequestOwnership(view);  // moved once it is the host's
  g.deadline = GetTickCount64() + kOwnershipWaitMs;
}

/** Throw `prop` at the nearest player in sight; false (nothing begun) when nobody is. */
bool throw_at_visible(void* ghost, void* prop, const char* why) {
  int actor = 0;
  void* target = visible_target(ghost, actor, why);
  if (target) begin(prop, target, actor);
  return target != nullptr;
}

bool intercept(void* ghost, void* prop) {
  if (!ghost || !throwable(prop)) return false;
  if (g.item) return true;  // one throw at a time: the game's own, possibly at nobody, is swallowed too
  throw_at_visible(ghost, prop, "a throw");
  return true;  // nobody in sight: the throw is swallowed
}

bool intercept_hunt_throw(void* ghost, void* prop) {
  void* interact = api::alive(prop) ? phasmo::Prop::interact.get(prop) : nullptr;
  if (!ghost || !throwable(interact)) return false;
  void* state = game::ghost_state(ghost) == phasmo::GhostState::hunting() ? game::ghost_state_object(ghost) : nullptr;
  void* target = state ? phasmo::GhostHuntState::target.get(state) : nullptr;
  if (g.item) return true;  // one throw at a time
  // None at a target the ghost does not see within range.
  const bool sees = game::ghost_sees(ghost, target);
  const float distance = api::alive(target) ? chest_distance(ghost, target) : -1;
  const bool throws = sees && distance <= kLithoboliaRange;
  if (note_now()) {
    PM_LOG("lithobolia: a hunt throw (%d since the last line): target %s, seen %d, %.1f m -> %s", g_unnoted,
           target ? "set" : "none", sees ? 1 : 0, distance, throws ? "throws at them" : "nothing");
    g_unnoted = 0;
  }
  if (throws) begin(interact, target, actor_of(target));
  return true;
}

bool throw_now(void* ghost) {
  void* activity = ghost ? game::ghost_activity(ghost) : nullptr;
  void* prop = activity ? phasmo::GhostActivity::GetPropToThrow(activity) : nullptr;
  void* interact = api::alive(prop) ? phasmo::Prop::interact.get(prop) : nullptr;
  return !g.item && throwable(interact) && throw_at_visible(ghost, interact, "the ability");
}

void tick(void* ghost, int type) {
  if (!ghost) ghost_sight::drop();  // shared (ghost_sight.h): kept while the contract's ghost lives
  if (!g.item) return;
  if (!ghost || type != kLithobolia || !api::alive(g.item) || !api::alive(g.target)) {
    clear();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (!g.landed) {
    if (phasmo::PhotonView::get_IsMine(phasmo::PhotonObjectInteract::view.get(g.item))) launch(ghost);
    else if (now >= g.deadline) {
      PM_LOG("lithobolia: the object never became the host's; no throw");
      clear();
    }
    return;
  }
  void* rb = phasmo::PhotonObjectInteract::body.get(g.item);
  const Vec at = vec(game::position(game::transform_of(g.item)));
  if (lithobolia_hit(at, length(vec(phasmo::Rigidbody::get_velocity(rb))), chest_of(g.target))) {
    PM_LOG("lithobolia: the throw hits");
    if (g.actor == game::local_actor()) {
      hit_me(kLithoboliaHit);
    } else if (g.actor) {
      const std::vector<uint8_t> message = net_msg::hit(kLithoboliaHit);
      net::send(message.data(), message.size(), net::To::Actor, true, g.actor);
    }
    clear();
  } else if (now >= g.landed) {
    clear();
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  float sanity = 0;
  if (sender != game::master_actor() || !net_msg::read_hit(data, size, sanity)) return;
  hit_me(std::clamp(sanity, 0.0f, kLithoboliaHit));
}

void reset() {
  clear();
  ghost_sight::drop();
}

}  // namespace pm::custom::lithobolia
