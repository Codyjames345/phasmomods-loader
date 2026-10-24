#pragma once

// Native sites in UnityPlayer.dll, the engine's own code. It has no metadata and exports only
// UnityMain, so each site is found by a byte pattern unique in its .text section: the last resort,
// and the only one there. The patterns span the instructions each site's users rely on, so a build
// that changes them leaves the entry unresolved rather than found somewhere wrong. Only the host
// patches a site (the host contract's patch_site); a mod asks for the effect. The steering stub that
// uses both moves here from Playable Ghost in stage 2b (mods/playable_ghost/steer_stub.asm).

#include "entry.h"

namespace pg::map {

/// The crowd update's read of a CrowdAgent's desired speed, `movss xmm0, [rbx+0x290]` (8 bytes,
/// F3 0F 10 83 90 02 00 00), where steering replaces the agent's direction: RBX is the CrowdAgent,
/// XMM14 and XMM15 hold its normalised direction's X and Y and [RSP+0x24] its Z, and the next
/// instructions scale all three by the speed. The pattern covers those uses through the three
/// multiplies. Live: Playable Ghost's stub at UnityPlayer+0xA4FB7A steered the ghost in game,
/// 2026-10-17.
inline constexpr pg::sdk::EntryDesc UnityPlayer_SteerSite{
    .key = "UnityPlayer.SteerSite", .kind = pg::sdk::Kind::NativeSite, .confidence = pg::sdk::Confidence::Live,
    .assembly = "UnityPlayer",
    .pattern = "F3 0F 10 83 90 02 00 00 45 0F 28 D3 F3 0F 10 4C 24 24 41 0F 28 E4 F3 44 0F 10 8B 48 01 00 00 "
               "41 0F 28 EC F3 44 0F 59 0D ?? ?? ?? ?? 4C 8B FE 4C 63 B3 E0 01 00 00 F3 44 0F 59 F0 F3 44 0F "
               "59 F8 F3 0F 59 C8",
    .within = ".text"};

/// NavMeshAgent::GetInternalAgent: from a native NavMeshAgent (a managed one's m_CachedPtr), the
/// CrowdAgent the crowd steers, or null. It reads the agent handle at +0x60 (prologue 48 8B 51 60)
/// and returns the crowd's agent array entry (0x2C0 bytes each) the handle names. A look-alike at
/// +0xA43260 reads the same handle but returns another table's entry, so the pattern runs through
/// the 0x2C0 stride. Live: Playable Ghost called it at UnityPlayer+0xA432F0 for the agent it
/// steered in game, 2026-10-17.
inline constexpr pg::sdk::EntryDesc UnityPlayer_GetInternalAgent{
    .key = "UnityPlayer.NavMeshAgent.GetInternalAgent", .kind = pg::sdk::Kind::NativeSite,
    .confidence = pg::sdk::Confidence::Live, .assembly = "UnityPlayer",
    .pattern = "48 8B 51 60 48 8B 05 ?? ?? ?? ?? 48 8B CA 48 C1 E9 04 4C 8B 80 90 00 00 00 8B C2 24 0F 3C 01 "
               "74 05 33 D2 8B C2 C3 41 3B 88 80 00 00 00 73 F2 8B C1 48 69 C8 C0 02 00 00 49 8B 80 A0 00 00 "
               "00 48 03 C1",
    .within = ".text"};

}  // namespace pg::map
