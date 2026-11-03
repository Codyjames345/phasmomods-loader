#pragma once

// Native sites in UnityPlayer.dll, the engine's own code. It has no metadata and exports only
// UnityMain, so each site is found by a byte pattern unique in its .text section: the last resort,
// and the only one there. The patterns span the instructions each site's users rely on, so a build
// that changes them leaves the entry unresolved rather than found somewhere wrong. Only the host uses them, and
// the host contract's game_entry never gives one to a mod: its steering (host/steering.cpp, host/steer_stub.asm)
// patches one and calls the other (kSteeringEntries below). A mod asks for the effect,
// phasmo::NavMeshAgent::steer (UnityWorld.h), and requires none of them. They are the map's most fragile, and the
// offline check reports them as native.

#include "UnityObject.h"
#include "UnityWorld.h"
#include "entry.h"

namespace pm::map {

/// The crowd update's read of a CrowdAgent's desired speed into XMM0, an 8-byte instruction, where steering
/// replaces the agent's direction: RBX is the CrowdAgent, XMM14 and XMM15 hold its normalised direction's X
/// and Y and a slot of the stack frame its Z, and the next instructions scale all three by the speed. The
/// pattern covers those uses through the three multiplies. Live: the steering stub at this site steered
/// the ghost in game, 2026-10-17.
inline constexpr pm::sdk::EntryDesc UnityPlayer_SteerSite{
    .key = "UnityPlayer.SteerSite", .kind = pm::sdk::Kind::NativeSite, .confidence = pm::sdk::Confidence::Live,
    .assembly = "UnityPlayer",
    .pattern = "F3 0F 10 83 90 02 00 00 45 0F 28 D3 F3 0F 10 4C 24 24 41 0F 28 E4 F3 44 0F 10 8B 48 01 00 00 "
               "41 0F 28 EC F3 44 0F 59 0D ?? ?? ?? ?? 4C 8B FE 4C 63 B3 E0 01 00 00 F3 44 0F 59 F0 F3 44 0F "
               "59 F8 F3 0F 59 C8",
    .within = ".text"};

/// NavMeshAgent::GetInternalAgent: from a native NavMeshAgent (a managed one's Object.m_CachedPtr), the
/// CrowdAgent the crowd steers, or null. It reads the agent's handle and returns the entry that handle names
/// in the crowd's agent array. A look-alike reads the same handle but returns another table's entry, so the
/// pattern runs through the array's stride. It begins a function. Live: steering called it for the agent
/// it steered in game, 2026-10-17.
inline constexpr pm::sdk::EntryDesc UnityPlayer_GetInternalAgent{
    .key = "UnityPlayer.NavMeshAgent.GetInternalAgent", .kind = pm::sdk::Kind::NativeSite,
    .confidence = pm::sdk::Confidence::Live, .assembly = "UnityPlayer",
    .pattern = "48 8B 51 60 48 8B 05 ?? ?? ?? ?? 48 8B CA 48 C1 E9 04 4C 8B 80 90 00 00 00 8B C2 24 0F 3C 01 "
               "74 05 33 D2 8B C2 C3 41 3B 88 80 00 00 00 73 F2 8B C1 48 69 C8 C0 02 00 00 49 8B 80 A0 00 00 "
               "00 48 03 C1",
    .within = ".text"};

/// What the host's steering needs, its own required list (no mod names these): the site it patches, the function
/// that gives a native agent's CrowdAgent, the field that gives a managed agent's native one, and the type every
/// agent it is given must be. The host logs the unresolved ones at startup, and the offline check prints them on its
/// "HOST steering" line.
inline constexpr const pm::sdk::EntryDesc* kSteeringEntries[] = {&UnityPlayer_SteerSite, &UnityPlayer_GetInternalAgent,
                                                                  &UnityObject_m_CachedPtr, &NavMeshAgent};

/// Whether a steering entry can be used, the one judgement the host (host/steering.cpp) and the offline check's "HOST
/// steering" line make: it resolved and, for a native site, it has an address.
constexpr bool steering_entry_usable(const pm::sdk::EntryDesc& d, bool resolved, bool hasAddress) {
  return resolved && (d.kind != pm::sdk::Kind::NativeSite || hasAddress);
}

}  // namespace pm::map
