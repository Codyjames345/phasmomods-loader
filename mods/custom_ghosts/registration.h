#pragma once

#include <cstdint>
#include <cstring>

#include "content.h"
#include "ghosts.h"
#include "squatter.h"
#include "text_table.h"

// What Custom Ghosts gives the content layer from pmmod_start (mod.cpp): each ghost of ghosts.h as a
// custom ghost, with its English text from the mod's table, its evidence, its ability, its callbacks and its rules. The host
// numbers them, puts them into the game and runs their rules only while this client's mods match the host's list
// (sdk/README.md, "The host's list"); nothing here decides that.
namespace pm::custom {

/** How the ghost `user` (its Type) changes the look of the evidence `evidenceId`: its flame look for
 *  Spectral Flame (the Hydra's "green"), else null for the evidence's own. */
inline const char* evidence_look(const char* evidenceId, void* user) {
  const Ghost* g = find(static_cast<int>(reinterpret_cast<intptr_t>(user)));
  return g && evidenceId && std::strcmp(evidenceId, kSpectralFlame) == 0 ? g->flameLook : nullptr;
}

/** Whether the ghost `user` (its Type) may be rolled now (PmGhostDef.rollable): every ghost but the Squatter always,
 *  the Squatter only while its figure is ready (squatter::available). */
inline bool rollable(void* user) {
  return static_cast<int>(reinterpret_cast<intptr_t>(user)) != kSquatter || squatter::available();
}

/** The definition of `g`: its id, English text and evidence (null an empty slot), its ability's text when it has
 *  one, `user` its Type, and the callbacks of `callbacks` (useAbility only for a ghost with an ability), with
 *  evidence_look for its evidence's look and rollable for the type roll. */
inline pm::api::content::GhostDef definition(const Ghost& g, const pm::api::content::GhostDef& callbacks) {
  pm::api::content::GhostDef def;
  def.id = id_of(g);
  // Its text is the mod's table's (mods/custom_ghosts/i18n/default.json), in English.
  def.name = pm::api::content::Text("en", i18n::text(title_key(g)));
  def.description = pm::api::content::Text("en", i18n::text(g.description));
  def.strength = pm::api::content::Text("en", i18n::text(g.strength));
  def.weakness = pm::api::content::Text("en", i18n::text(g.weakness));
  for (int i = 0; i < 3; ++i) def.evidence[i] = g.evidence[i] ? g.evidence[i] : "";  // null: an empty slot
  if (g.ability) {
    def.abilityName = pm::api::content::Text("en", i18n::text(g.ability));
    def.abilityDescription = pm::api::content::Text("en", i18n::text(g.abilityDescription));
    def.useAbility = callbacks.useAbility;
  }
  def.user = reinterpret_cast<void*>(static_cast<intptr_t>(g.type));
  def.hudState = callbacks.hudState;
  def.heldBack = callbacks.heldBack;
  def.playerView = callbacks.playerView;
  def.target = callbacks.target;
  def.hudLine = callbacks.hudLine;
  def.playerMove = callbacks.playerMove;
  def.interactionLost = callbacks.interactionLost;
  def.evidenceLook = &evidence_look;
  def.rollable = &rollable;
  if (g.keepsLastEvidence) def.flags |= PM_GHOST_KEEPS_LAST_EVIDENCE;
  if (g.blindInHunts) def.flags |= PM_GHOST_BLIND_IN_HUNTS;
  if (g.noMimicCopy) def.flags |= PM_GHOST_NO_MIMIC_COPY;
  return def;
}

/** Registers every ghost (definition) and attaches `rules` to each (`user` becomes its Type). False
 *  when the host refused any of them (logged by the host). */
inline bool register_content(const pm::api::content::GhostDef& callbacks, const pm::api::content::GhostRules& rules) {
  for (const Ghost& g : kGhosts) {
    if (pm::api::content::register_ghost(definition(g, callbacks)) <= 0) return false;
    pm::api::content::GhostRules r = rules;
    r.user = reinterpret_cast<void*>(static_cast<intptr_t>(g.type));
    if (pm::api::content::attach_rules(id_of(g), r) <= 0) return false;
  }
  return true;
}

}  // namespace pm::custom
