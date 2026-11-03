#pragma once

#include <cstdint>

// What the contract says about Spectral Flame, read through the content layer (sdk/api/content.h) and
// the game map: the evidence's registration and its game value, whether a ghost type of the contract
// carries it, whether the flames are on, whether the ghost gives it, and the ghost's look for it. Main
// thread.
namespace pm::flame::contract {

/// Spectral Flame's content id, which the host numbers.
inline constexpr const char* kEvidenceId = "phasmomods.spectral-flame.flame";

/// Registers the evidence with its journal row's text, from pmmod_start only. False when the host
/// refused it (the host logs why).
bool register_evidence();

/// The evidence's game value while content is active or a contract that loaded with content holds it,
/// else -1.
int32_t evidence();

/// The contract's LevelController while a contract is loaded, else null.
void* level();

/// The contract's ghost, its GhostAI, once it has spawned, else null.
void* ghost();

/// Whether a ghost type of the contract's ghost collection lists `evidence` (Custom Ghosts adds
/// those). False for a negative `evidence` and outside a contract.
bool carried(int32_t evidence);

/// Whether the ghost `ghost` gives `evidence`, as the difficulty left its evidence (the given evidence
/// every client's GhostInfo holds; empty when the difficulty gives none).
bool ghost_gives(void* ghost, int32_t evidence);

/// The evidence's game value while the flames are on in the loaded contract, else -1. They are on while
/// a contract is loaded, the evidence has a number and a ghost type of the contract carries it. The
/// number comes from the content layer, which decides a contract's content when it loads and holds it
/// until it ends whatever the room does meanwhile (sdk/api/content.h), so the flames stay on through a
/// lapse in the room's content state (a hitch, a late manifest) that would otherwise wipe every flame's
/// blueness. Main thread.
int32_t flame_evidence();

/// Whether the ghost's flames turn green: its look for the evidence is "green" (the Hydra's,
/// content::evidence_look); false for any other look, none, and no ghost.
bool green(void* ghost);

}  // namespace pm::flame::contract
