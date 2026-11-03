#include "steering.h"

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "game_api.h"
#include "hooks.h"
#include "log.h"
#include "runtime.h"

extern "C" {
extern unsigned char pm_steer_begin[], pm_steer_end[], pm_steer_replaced[], pm_steer_replaced_end[];
extern unsigned char pm_steer_target[], pm_steer_active[], pm_steer_dx[], pm_steer_dz[];
}

namespace pm::steering {
namespace {

using pm::api::host_only::game_entry;

unsigned char* g_page = nullptr;  // the stub the hooks layer copied, whose data this writes
bool g_refused = false;           // the last patch was refused: nothing tries again until reset

template <class T>
T* field(const unsigned char* label) {
  return reinterpret_cast<T*>(g_page + (label - pm_steer_begin));
}

const PmGameEntry* site() { return game_entry(pm::map::UnityPlayer_SteerSite.key); }

bool in_place() {
  const PmGameEntry* s = site();
  return g_page && s && hooks::site_patched(s->code);
}

bool install() {
  g_page = nullptr;
  if (const std::string lacking = missing(); !lacking.empty()) {
    PM_LOG("steering: these game entries did not resolve: %s; steering unavailable", lacking.c_str());
    return false;
  }
  if (static_cast<size_t>(pm_steer_replaced_end - pm_steer_replaced) != kReplaced.size()) {
    PM_LOG("steering: the stub has no room for the instruction it replaces; steering unavailable");
    return false;
  }
  // The stub as the game's build wants it: the map's frame slot and the instruction the jump replaces.
  std::vector<unsigned char> stub(pm_steer_begin, pm_steer_end);
  const size_t at = static_cast<size_t>(pm_steer_replaced - pm_steer_begin);
  std::memcpy(stub.data() + at, kReplaced.data(), kReplaced.size());
  stub[at - 1] = kDirectionZ;
  void* page = nullptr;
  if (!hooks::patch_site(site()->code, kReplaced.data(), kReplaced.size(), stub.data(), stub.size(), &page,
                         pm::map::UnityPlayer_SteerSite.key)) {
    PM_LOG("steering: the steering site could not be patched; steering unavailable until the mods start again");
    return false;
  }
  g_page = static_cast<unsigned char*>(page);
  PM_LOG("steering: installed, stub %p (%zu bytes)", page, stub.size());
  return true;
}

// The CrowdAgent the crowd steers for the managed NavMeshAgent `agent`, or null: for a destroyed object, one of another
// class (whose native object the engine's function must never be given), an agent the crowd holds no entry for, and
// when an entry it reads did not resolve: the patch in place does not prove them.
void* crowd_agent_of(void* agent) {
  const PmGameEntry* cachedPtr = game_entry(pm::map::UnityObject_m_CachedPtr.key);
  const PmGameEntry* internalAgent = game_entry(pm::map::UnityPlayer_GetInternalAgent.key);
  if (!cachedPtr || !internalAgent || !internalAgent->code) return nullptr;
  if (!pm::api::alive(agent) || !phasmo::NavMeshAgent::type.is(agent)) return nullptr;
  void* native = nullptr;
  std::memcpy(&native, static_cast<const char*>(agent) + cachedPtr->offset, sizeof native);
  if (!native) return nullptr;
  return reinterpret_cast<void* (*)(void*)>(internalAgent->code)(native);
}

}  // namespace

std::string missing() {
  std::string out;
  for (const pm::sdk::EntryDesc* d : pm::map::kSteeringEntries) {
    const PmGameEntry* e = game_entry(d->key);
    if (!pm::map::steering_entry_usable(*d, e != nullptr, e && e->code))
      out += (out.empty() ? "" : ", ") + std::string(d->key);
  }
  return out;
}

bool steer(void* agent, float dx, float dz) {
  if (!in_place()) {
    if (g_refused) return false;
    g_refused = !install();
    if (g_refused) return false;
  }
  void* crowd = agent ? crowd_agent_of(agent) : nullptr;
  if (!crowd) return true;
  const float m2 = dx * dx + dz * dz;
  if (!std::isfinite(m2) || m2 > 1.0001f) {
    expire();
    return true;
  }
  *field<float>(pm_steer_dx) = dx;
  *field<float>(pm_steer_dz) = dz;
  *field<void*>(pm_steer_target) = crowd;
  std::atomic_thread_fence(std::memory_order_release);
  *field<int32_t>(pm_steer_active) = 1;
  return true;
}

void expire() {
  if (g_page) *field<int32_t>(pm_steer_active) = 0;
}

void reset() { g_refused = false; }

}  // namespace pm::steering
