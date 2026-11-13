#include "mimic.h"

#include <algorithm>

#include "GhostAI.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "LevelController.h"
#include "log.h"
#include "runtime.h"

namespace pm::mimic {
namespace {

using phasmo::GhostController;

uint8_t g_pick = session::kNoType;

bool is_mimic(void* controller) {
  return controller && api::alive(controller) && GhostController::ghostType.get(controller) == phasmo::GhostType::Mimic();
}

// The type the Mimic is copying now: the ghost's behaviour type, which MimicCopy rewrites at every
// switch while GhostController.traits stays the Mimic's. The game reaches it through the same chain
// (GhostController.levelController, LevelController.currentGhost, GhostAI.info); kNoType when the chain
// is not there or its type is not one the session can send.
uint8_t current_copy(void* controller) {
  void* level = controller ? GhostController::levelController.get(controller) : nullptr;
  void* ghost = level ? phasmo::LevelController::currentGhost.get(level) : nullptr;
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  if (!info) return session::kNoType;
  const int32_t t = phasmo::GhostInfo::behaviourType.get(info);
  return t >= 0 && t < session::kNoType ? static_cast<uint8_t>(t) : session::kNoType;
}

void detour_attempt(api::Original<GhostController::AttemptGhostTypeStateChange> original, void* controller) {
  if (place_pick(controller, g_pick)) {
    PM_LOG("mimic: copying the type its player chose (%d)", g_pick);
    g_pick = session::kNoType;
  }
  original(controller);
}

}  // namespace

bool place_pick(void* controller, uint8_t pick) {
  // Only at the game's own moment to switch, and only to a type it would itself copy.
  if (pick == session::kNoType || !is_mimic(controller) || !GhostController::typeChangeDue.get(controller) ||
      GhostController::mimicNext.get(controller).hasValue)
    return false;
  if (pick == current_copy(controller)) return false;  // never the type it is copying now, stale offer or not
  const std::vector<uint8_t> offered = copies(controller);
  if (std::find(offered.begin(), offered.end(), pick) == offered.end()) return false;
  GhostController::mimicNext.set(controller, {true, pick});
  return true;
}

std::vector<uint8_t> copies(void* controller) {
  std::vector<uint8_t> types;
  if (!is_mimic(controller)) return types;
  void* list = GhostController::mimicCopies.get(controller);
  if (!list) return types;
  const api::List copies{list};
  const api::Array<int32_t> values{copies.items()};
  for (size_t i = 0, n = copies.size(); i < n; ++i)
    if (values[i] >= 0 && values[i] < session::kNoType) types.push_back(static_cast<uint8_t>(values[i]));
  return types;
}

bool create_hooks() {
  return api::hook<GhostController::AttemptGhostTypeStateChange>(&detour_attempt,
                                                                  "GhostController.AttemptGhostTypeStateChange",
                                                                  api::Twins::CanonicalOnly);
}

void tick(session::Session& s, void* ghost) {
  uint8_t pick;
  if (s.take_mimic_pick(pick)) g_pick = pick;
  if (!ghost) {
    g_pick = session::kNoType;
    return;
  }
  void* controller = GhostController::instance.get();
  if (!is_mimic(controller)) return;
  s.offer_mimic(offer(copies(controller), current_copy(controller)));
}

uint8_t next() { return g_pick; }

}  // namespace pm::mimic
