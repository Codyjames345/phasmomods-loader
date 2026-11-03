#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>

#include "text_table.h"

// A game ghost type's own state, as its player feels it: the host reads it from the game and sends two
// bytes (Status::trait); this says what they mean. A type is named by its content id ("vanilla.gallu"),
// so its number is the game's own (sdk/api/content.h type_of). A registered ghost's state is its
// definition's (PmGhostDef.hudState), whose own hudLine reads it (Status's kCustomState). The ghost
// player learns only about the ghost itself, never about the investigators or their equipment. Pure
// text, testable offline.
namespace pm::typestate {

/** The Deildegast's speed for how many things it has moved (the game's rule: 3 m/s, 0.1 less per thing). */
inline float deildegast_speed(int moved) { return std::max(0.4f, 3.0f - 0.1f * static_cast<float>(moved)); }

/** The HUD line for a game ghost type's state, by the type's id; empty when its type has none to show. */
inline std::string describe(std::string_view ghostId, const uint8_t trait[2]) {
  char number[16];
  if (ghostId == "vanilla.gallu") {
    const int state = std::min<int>(trait[0], 2);
    return i18n::text("state.gallu", {{"state", state == 0 ? i18n::text("state.gallu.calm")
                                                : state == 1 ? i18n::text("state.gallu.enraged")
                                                             : i18n::text("state.gallu.exhausted")}});
  }
  if (ghostId == "vanilla.dayan") {
    const int state = std::min<int>(trait[0], 2);
    return i18n::text("state.dayan", {{"state", state == 0 ? i18n::text("state.dayan.steady")
                                                : state == 1 ? i18n::text("state.dayan.empowered")
                                                             : i18n::text("state.dayan.weakened")}});
  }
  if (ghostId == "vanilla.obambo") {
    std::snprintf(number, sizeof number, "%d:%02d", trait[1] / 60, trait[1] % 60);
    return i18n::text("state.obambo",
                      {{"mood", trait[0] ? i18n::text("state.obambo.aggressive") : i18n::text("state.obambo.calm")}, {"time", number}});
  }
  if (ghostId == "vanilla.thaye")
    return i18n::text("state.thaye", {{"age", std::to_string(trait[0])}, {"sanity", std::to_string(trait[1])}});
  if (ghostId == "vanilla.deildegast") {
    std::snprintf(number, sizeof number, "%.1f", deildegast_speed(trait[0]));
    return i18n::text("state.deildegast", {{"speed", number}, {"moved", std::to_string(trait[0])}});
  }
  if (ghostId == "vanilla.raiju") return trait[0] ? i18n::text("state.raiju") : std::string();
  return {};
}

}  // namespace pm::typestate
