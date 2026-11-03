#include "contract.h"

#include <vector>

#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "content.h"
#include "flame_rules.h"
#include "runtime.h"
#include "text_table.h"

namespace pm::flame::contract {

bool register_evidence() {
  api::content::EvidenceDef def;
  def.id = kEvidenceId;
  def.name = {"en", i18n::text("evidence.name")};  // the mod's table (mods/spectral_flame/i18n/default.json)
  def.journalLine = {"en", i18n::text("evidence.journal-line")};
  return api::content::register_evidence(def) > 0;
}

int32_t evidence() { return api::content::evidence_of(kEvidenceId); }

void* level() {
  // The static instance outlives its level: after a contract it still points at the destroyed one.
  void* level = phasmo::LevelController::instance.get();
  return api::alive(level) ? level : nullptr;
}

void* ghost() {
  void* ghost = phasmo::LevelController::currentGhost.get(level());
  return api::alive(ghost) ? ghost : nullptr;
}

bool carried(int32_t evidence) {
  if (evidence < 0 || !level()) return false;
  void* controller = phasmo::GhostController::instance.get();
  if (!api::alive(controller)) return false;
  void* collection = phasmo::GhostController::allGhostsCollection.get(controller);
  const api::List ghosts{phasmo::GhostCollection::ghosts.get(collection)};
  std::vector<int> triples;
  for (size_t i = 0; i < ghosts.size(); ++i)
    if (void* data = ghosts[i])
      for (int value : {phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
                        phasmo::GhostData::evidence3.get(data)})
        triples.push_back(value);
  return active_for(triples.data(), static_cast<int>(triples.size() / 3), evidence);
}

bool ghost_gives(void* ghost, int32_t evidence) {
  void* info = phasmo::GhostAI::info.get(ghost);
  if (evidence < 0 || !api::alive(info)) return false;
  const api::List given{phasmo::GhostInfo::givenEvidence.get(info)};
  const api::Array<int32_t> values{given.items()};
  for (size_t i = 0; i < given.size(); ++i)
    if (values[i] == evidence) return true;
  return false;
}

int32_t flame_evidence() {
  const int32_t value = evidence();
  return carried(value) ? value : -1;
}

bool green(void* ghost) { return ghost && api::content::evidence_look(kEvidenceId) == "green"; }

}  // namespace pm::flame::contract
