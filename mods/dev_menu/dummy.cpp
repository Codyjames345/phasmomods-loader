// The dummy investigator (dummy.h). The hunt's own rules choose it; the mod only offers it where the game offers a
// player, through the three pickers the hunt runs (GhostHuntState's map notes):
//  - StartPick keeps the Banshee's target when it may, otherwise a player from the network's list. The dummy is
//    offered when it stands nearer the ghost's head than the player the game kept.
//  - KeepNearest moves the kept player to the nearest one each frame (not for the Banshee or the Kormos, whose
//    callers skip it). The dummy is offered by the same distance test.
//  - SightPick finds the first player in sight (nobody for the Banshee). The dummy is offered when the game found
//    nobody and the hunt's own sight test (GhostHuntState.Sees) passes for it.
// A Banshee goes for the dummy only when the dummy row makes it the Banshee's target, as the game's start pick keeps
// that target for the whole hunt.
#include "dummy.h"

#include <cmath>

#include "actions.h"
#include "game.h"
#include "held.h"
#include "hooks.h"
#include "log.h"
#include "runtime.h"

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "GhostSenses.h"
#include "LevelRoom.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "UnityObject.h"

namespace pm::devmenu::dummy {
namespace {

using namespace phasmo;

api::Held g_body;    // the dummy's GameObject
api::Held g_player;  // its Player component

bool alive_now() { return g_player && api::alive(g_player) && g_body && api::alive(g_body); }

// The scripts the dummy keeps: the three the ghost's tests read. Everything else of the game's on the copy goes
// before it is switched on, so none of it ever runs.
bool kept_script(void* c) {
  return Player::type.is(c) || PlayerSanity::type.is(c) || PlayerLOSTarget::type.is(c);
}

void strip(void* transform) {
  if (!transform) return;
  void* go = game::game_object_of(transform);
  void* type = MonoBehaviour::type.type_object();
  if (go && type) {
    const api::RefArray scripts{GameObject::GetComponents(go, type)};
    for (size_t i = 0; i < scripts.size(); ++i) {
      void* c = scripts[i];
      if (!c) continue;
      if (kept_script(c))
        Behaviour::set_enabled(c, false);
      else
        Object::DestroyImmediate(c, false);
    }
  }
  const int n = Transform::get_childCount(transform);
  for (int i = 0; i < n; ++i) strip(Transform::GetChild(transform, i));
}

Vector3 where(void* player) {
  void* body = player ? Player::body.get(player) : nullptr;
  void* t = body ? game::transform_of(body) : (player ? game::transform_of(player) : nullptr);
  return t ? game::position(t) : Vector3{};
}

// The ghost's head, which the game's own nearest test measures from (GhostHuntState.KeepNearest's note).
bool head(Vector3& out) {
  void* ai = game::ghost_ai();
  void* h = ai ? GhostAI::head.get(ai) : nullptr;
  void* t = h ? game::transform_of(h) : nullptr;
  if (!t) return false;
  out = game::position(t);
  return true;
}

// Whether the dummy stands nearer the ghost's head than `player` (or there is no player at all).
bool nearer_than(void* player) {
  if (!available()) return false;
  Vector3 h{};
  if (!head(h)) return false;
  if (!player) return true;
  return game::distance(h, where(g_player)) < game::distance(h, where(player));
}

bool det_start_pick(api::Original<GhostHuntState::StartPick> original, void* state) {
  const bool kept = original(state);
  if (!kept || !available()) return kept;
  void* ai = game::ghost_ai();
  if (ai && GhostAI::bansheeTarget.get(ai)) return kept;  // the game keeps the Banshee's target: leave its choice
  if (nearer_than(GhostHuntState::kept.get(state))) GhostHuntState::kept.set(state, g_player);
  return kept;
}

void det_keep_nearest(api::Original<GhostHuntState::KeepNearest> original, void* state) {
  original(state);
  if (available() && nearer_than(GhostHuntState::kept.get(state))) GhostHuntState::kept.set(state, g_player);
}

bool det_sight_pick(api::Original<GhostHuntState::SightPick> original, void* state, void** found) {
  if (original(state, found)) return true;
  if (!available() || !found) return false;
  void* ai = game::ghost_ai();
  if (ai && GhostAI::bansheeTarget.get(ai)) return false;  // the Banshee's sight goes only for its own target
  if (!GhostHuntState::Sees(state, g_player, false)) return false;
  *found = g_player;
  return true;
}

}  // namespace

bool spawn() {
  if (alive_now()) return false;
  void* me = game::local_player();
  void* go = me ? game::game_object_of(me) : nullptr;
  if (!go) return false;
  // The copy is made under a parent that is switched off, so nothing of it runs until it is stripped.
  void* hold = game::new_game_object("pm dev dummy holder");
  if (!hold) return false;
  GameObject::SetActive(hold, false);
  void* copy = game::instantiate(go, game::transform_of(hold), true);
  if (!copy) {
    Object::Destroy(hold, 0.0f);
    return false;
  }
  void* copyGo = game::game_object_of(copy);
  strip(game::transform_of(copyGo));
  void* t = game::transform_of(copyGo);
  Transform::SetParent(t, nullptr, true);
  Object::Destroy(hold, 0.0f);
  void* player = Component::GetComponent(t, Player::type.type_object());
  if (!player) {
    Object::Destroy(copyGo, 0.0f);
    return false;
  }
  g_body = copyGo;
  g_player = player;
  Player::isDead.set(player, false);
  Player::dying.set(player, false);
  GameObject::SetActive(copyGo, true);
  to_me();
  PM_LOG("dev-menu: the dummy investigator is in the contract");
  return true;
}

void despawn() {
  void* ai = game::ghost_ai();
  if (ai && g_player && GhostAI::bansheeTarget.get(ai) == static_cast<void*>(g_player))
    GhostAI::bansheeTarget.set(ai, nullptr);
  if (g_body && api::alive(g_body)) Object::Destroy(g_body, 0.0f);
  g_body = nullptr;
  g_player = nullptr;
}

void* player() { return alive_now() ? static_cast<void*>(g_player) : nullptr; }

bool available() { return alive_now() && !Player::isDead.get(g_player); }

bool to_me() {
  void* me = game::local_player();
  if (!alive_now() || !me) return false;
  void* t = game::transform_of(g_body);
  const Vector3 at = where(me);
  if (!t) return false;
  game::set_position(t, at);
  return true;
}

bool make_banshee_target() {
  void* ai = game::ghost_ai();
  if (!ai || !available()) return false;
  GhostAI::bansheeTarget.set(ai, g_player);
  return true;
}

bool on_kill(void* victim) {
  if (!victim || !alive_now() || victim != static_cast<void*>(g_player)) return false;
  Player::isDead.set(g_player, true);
  PM_LOG("dev-menu: the hunt killed the dummy investigator");
  return true;
}

void revive() {
  if (!alive_now()) return;
  Player::isDead.set(g_player, false);
  Player::dying.set(g_player, false);
}

void tick() {
  if ((g_body || g_player) && !alive_now()) {
    g_body = nullptr;  // a scene change destroyed it
    g_player = nullptr;
    return;
  }
  if (!alive_now()) return;
  // The hunt's tests read the room it stands in, and throw on a player with none (the research's readers).
  void* room = game::room_at(where(g_player));
  if (room) Player::currentRoom.set(g_player, room);
}

bool position(Vector3& out) {
  if (!alive_now()) return false;
  out = where(g_player);
  return true;
}

bool hook() {
  return api::hook<GhostHuntState::StartPick>(&det_start_pick, "Dev Menu: the dummy at the hunt's start") &&
         api::hook<GhostHuntState::KeepNearest>(&det_keep_nearest, "Dev Menu: the dummy when it is nearest") &&
         api::hook<GhostHuntState::SightPick>(&det_sight_pick, "Dev Menu: the dummy in sight");
}

}  // namespace pm::devmenu::dummy
