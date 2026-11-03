#include "content_target.h"

#include "GhostAI.h"
#include "GhostHuntState.h"
#include "game_api.h"
#include "guarded.h"
#include "runtime.h"

namespace pm::host {
namespace {

using phasmo::GhostAI;
using phasmo::GhostHuntState;
using pm::api::Use;

ContentTarget* g_self = nullptr;  // the instance whose hooks are made

bool detour_start_pick(pm::api::Original<GhostHuntState::StartPick> original, void* hunt) {
  static bool logged = false;
  if (g_self && hunt) guarded(logged, "holding a hunt to its target", [&] { g_self->before_start_pick(hunt); });
  return original(hunt);
}

// The chase skips this step for the Banshee's type.
void detour_keep_nearest(pm::api::Original<GhostHuntState::KeepNearest> original, void* hunt) {
  if (!g_self || g_self->keeps_nearest(hunt)) original(hunt);
}

// For the Banshee's type the game's look in sight finds nobody: it clears its out argument and answers false.
bool detour_sight_pick(pm::api::Original<GhostHuntState::SightPick> original, void* hunt, void** found) {
  if (!g_self || g_self->sight_picks(hunt)) return original(hunt, found);
  if (found) *found = nullptr;  // an out argument of the game's: a local, so no write barrier
  return false;
}

void detour_new_banshee_target(pm::api::Original<GhostAI::SetNewBansheeTarget> original, void* ghost, void* dying) {
  static bool logged = false;
  bool given = false;
  if (g_self && ghost)
    guarded(logged, "re-targeting a held hunt", [&] { given = g_self->new_target(ghost, dying); });
  if (!given) original(ghost, dying);
}

void detour_exit(pm::api::Original<GhostHuntState::Exit> original, void* hunt) {
  static bool logged = false;
  original(hunt);
  if (g_self && hunt) guarded(logged, "ending a held hunt", [&] { g_self->after_exit(hunt); });
}

}  // namespace

std::string ContentTarget::missing() const {
  struct Need {
    const char* key;
    bool resolved;
  };
  const Need needs[] = {
      {"GhostHuntState.ghost", Use<pm::map::GhostHuntState_ghost>::resolved()},
      {"GhostHuntState.StartPick", Use<pm::map::GhostHuntState_StartPick>::resolved()},
      {"GhostHuntState.KeepNearest", Use<pm::map::GhostHuntState_KeepNearest>::resolved()},
      {"GhostHuntState.SightPick", Use<pm::map::GhostHuntState_SightPick>::resolved()},
      {"GhostHuntState.Exit", Use<pm::map::GhostHuntState_Exit>::resolved()},
      {"GhostAI.bansheeTarget", Use<pm::map::GhostAI_bansheeTarget>::resolved()},
      {"GhostAI.SetNewBansheeTarget", Use<pm::map::GhostAI_SetNewBansheeTarget>::resolved()},
  };
  std::string out;
  for (const Need& n : needs)
    if (!n.resolved) out += (out.empty() ? "" : ", ") + std::string(n.key);
  return out;
}

bool ContentTarget::create_hooks() {
  g_self = this;
  using pm::api::hook;
  return hook<GhostHuntState::StartPick>(&detour_start_pick, "content: a hunt held to its target") &&
         hook<GhostHuntState::KeepNearest>(&detour_keep_nearest, "content: a held hunt keeps its player") &&
         hook<GhostHuntState::SightPick>(&detour_sight_pick, "content: a held hunt's sight") &&
         hook<GhostAI::SetNewBansheeTarget>(&detour_new_banshee_target, "content: a held hunt's next target") &&
         hook<GhostHuntState::Exit>(&detour_exit, "content: a held hunt's end");
}

bool ContentTarget::held(void* hunt) const { return hunt && set_ && GhostHuntState::ghost.get(hunt) == set_.get(); }

bool ContentTarget::holds(const void* ghost) const { return ghost && ghost == set_.get(); }

void ContentTarget::release() {
  if (set_ && pm::api::alive(set_.get())) GhostAI::bansheeTarget.set(set_.get(), nullptr);
  set_ = nullptr;
  heldFor_ = 0;
}

void ContentTarget::before_start_pick(void* hunt) {
  void* ghost = GhostHuntState::ghost.get(hunt);
  if (!ghost) return;
  if (ghost == set_.get()) release();  // a hunt that ended without its exit
  if (void* target = acting_.target(nullptr, 0)) {
    // The hunt's ghostType stays the ghost's own (no type swap): every other reader of the field acts only
    // for the Banshee's type, so the field reaches nothing but the hunt's own steps.
    GhostAI::bansheeTarget.set(ghost, target);
    set_ = ghost;
    const content::Ghost* acting = acting_.acting_ghost();
    heldFor_ = acting ? acting->handle : 0;
  }
}

bool ContentTarget::keeps_nearest(void* hunt) const { return !held(hunt); }

bool ContentTarget::sight_picks(void* hunt) const { return !held(hunt); }

// The game's re-target for the Banshee's type, which the kill and the Hanged Man card call with the player
// about to die, before their death begins: for the held ghost its definition answers instead, with only that
// player left out, so its own rule decides, keeping its target while they are not the one dying if it so
// chooses. An answer of nobody keeps the target, as the game's does.
bool ContentTarget::new_target(void* ghost, void* dying) {
  if (!ghost || ghost != set_.get()) return false;
  const void* const excluded[] = {dying};
  void* next = acting_.target(excluded, 1);
  if (next && next != dying) GhostAI::bansheeTarget.set(ghost, next);
  return true;
}

void ContentTarget::after_exit(void* hunt) {
  if (held(hunt)) release();
}

void ContentTarget::tick() {
  if (!set_) return;
  if (!pm::api::alive(set_.get())) {
    set_ = nullptr;
    return;
  }
  // The ghost no longer acts as the type this hold was made for (a Mimic's copy, of a game type or another
  // custom one): from now on the hunt is the game's own.
  const content::Ghost* acting = acting_.acting_ghost();
  if (!acting || acting->handle != heldFor_) {
    release();
    return;
  }
  // The target left the game mid-hunt: as the game re-targets a Banshee whose target is gone, the definition
  // names the next one, with nobody left out; nobody, and the field stays.
  void* target = GhostAI::bansheeTarget.get(set_.get());
  if (target && !pm::api::alive(target))
    if (void* next = acting_.target(nullptr, 0)) GhostAI::bansheeTarget.set(set_.get(), next);
}

void ContentTarget::withdraw() { release(); }

}  // namespace pm::host
