#pragma once

// The ghost's one target on its player's HUD (PmGhostDef.target, sdk/api/content.h target). The host holds
// a hunt to it, for the ghost's AI and a player controlling it alike, by writing it to the ghost's Banshee
// target on the master while the hunt runs (host/content_target.h); the game itself writes that field only
// for a ghost whose AI follows the Banshee's rules at the time (GhostAI.SetNewBansheeTarget acts for that type
// alone).
// Pure logic, tested offline.
namespace pm::target {

/// The player the HUD shows as the ghost's target: `hunting`, for a ghost that is not the Banshee's type, its
/// Banshee target while one is set (the host's hold: the target fixed at its start); otherwise what its
/// definition answers now (`answer`), or null.
inline void* shown(bool bansheeType, bool hunting, void* bansheeTarget, void* answer) {
  return hunting && !bansheeType && bansheeTarget ? bansheeTarget : answer;
}

/// The player whose insanity the game's hunt check measures (GhostIdleState.HuntInsanity), which reads the field
/// with no test of the type and runs outside a hunt: outside one, the ghost's Banshee target whatever its type (a
/// Mimic that has copied the Banshee keeps it through later copies; nothing clears it). `hunting`, the field of
/// another type is the host's hold and no focus of the check's, so no one (the team's), and the Banshee's type's
/// own target for that type.
inline void* hunt_focus(bool bansheeType, bool hunting, void* bansheeTarget) {
  return bansheeType || !hunting ? bansheeTarget : nullptr;
}

}  // namespace pm::target
