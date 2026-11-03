#pragma once

#include <cstdint>
#include <string>

#include "ghosts.h"
#include "text_table.h"

// The line the HUD of a player controlling a custom ghost shows for the ghost's own state
// (PmGhostDef.hudLine): what hudState answered on the host (mod.cpp hud_state), turned into text on that
// player's client. Pure text, tested offline.
namespace pm::custom {

/**
 * The HUD line of the ghost `type` in the state `state`: the Echo's dormancy (1 dormant, 0 hearing
 * voices), the Dullahan's charge (its speed in percent of the game's base; 0 says nothing), the
 * Kikimora's lit room (1 lit) and the Draugr's anger (1 angered, 0 calm). Empty for any other ghost or
 * state.
 */
inline std::string hud_line(int type, int32_t state) {
  switch (type) {
    case kEcho:
      if (state == 1) return "<alpha=#77>" + i18n::text("hud.echo-dormant");
      return state == 0 ? i18n::text("hud.echo-hearing") : "";
    case kDullahan:
      if (state <= 0) return "";
      return i18n::text("hud.dullahan-charge", {{"speed", std::to_string(static_cast<int>(state))}});
    case kKikimora:
      return state == 1 ? "<alpha=#77>" + i18n::text("hud.kikimora-lit") : "";
    case kDraugr:
      if (state == 1) return i18n::text("hud.draugr-angered");
      return state == 0 ? i18n::text("hud.draugr-whole") : "";
    default:
      return "";
  }
}

}  // namespace pm::custom
