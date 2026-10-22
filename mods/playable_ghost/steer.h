#pragma once

// Ghost direction steering inside Unity's crowd simulation (see steer_stub.asm).
// install/uninstall/set must run on the main thread, outside the crowd update.
namespace pg::steer {

bool install(void* unityPlayer);
void uninstall();
bool installed();

// Native CrowdAgent for a managed UnityEngine.AI.NavMeshAgent, or null.
void* crowd_agent_for(void* managedAgent);

// Substitute the steering direction of `crowdAgent` with horizontal (dx, dz), |d| <= 1.
// A zero direction holds the agent still with native deceleration.
void set(void* crowdAgent, float dx, float dz);
void clear();

}  // namespace pg::steer
