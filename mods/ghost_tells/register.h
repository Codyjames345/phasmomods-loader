#pragma once

#include <cstdint>

#include "content.h"
#include "journal_text.h"
#include "tells.h"

// What Ghost Tells gives the content layer from pmmod_start (mod.cpp): for each told ghost, its rules
// on its vanilla id and its journal line. The host runs the rules and shows the lines only while this
// client's mods match the host's list (sdk/README.md, "The host's list"); nothing here decides that.
namespace pm::tells {

/** Attaches `rules` (its functions; `user` becomes the told number, and huntSpeed is kept for the
 *  Spirit and the Shade only, the two whose tells change hunt speed) and adds the journal line, for
 *  every told ghost. False when the host refused any of them (logged by the host). */
inline bool register_content(const pm::api::content::GhostRules& rules) {
  for (int told = 0; told < kToldCount; ++told) {
    pm::api::content::GhostRules r = rules;
    r.user = reinterpret_cast<void*>(static_cast<intptr_t>(told));
    if (told != kSpirit && told != kShade) r.huntSpeed = nullptr;
    if (!pm::api::content::attach_rules(kToldIds[told], r)) return false;
    if (!pm::api::content::add_journal_lines(kToldIds[told], {pm::api::content::Text("en", tell_line(told))}))
      return false;
  }
  return true;
}

}  // namespace pm::tells
