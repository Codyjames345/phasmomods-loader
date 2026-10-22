#include "steer.h"

#include <windows.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>

#include "log.h"

extern "C" {
extern unsigned char pg_steer_begin[], pg_steer_end[];
extern unsigned char pg_steer_target[], pg_steer_active[], pg_steer_dx[], pg_steer_dz[], pg_steer_resume[];
}

namespace pg::steer {
namespace {

constexpr uintptr_t kSiteRva = 0xA4FB7A;
constexpr uintptr_t kGetInternalAgentRva = 0xA432F0;
constexpr unsigned char kOriginal[8] = {0xF3, 0x0F, 0x10, 0x83, 0x90, 0x02, 0x00, 0x00};
// NavMeshAgent::GetInternalAgent starts by reading the agent handle at +0x60.
constexpr unsigned char kGetInternalAgentPrologue[4] = {0x48, 0x8B, 0x51, 0x60};

unsigned char* g_site = nullptr;
unsigned char* g_page = nullptr;
using GetInternalAgent_t = void* (*)(void* nativeAgent);
GetInternalAgent_t g_getInternalAgent = nullptr;

template <class T>
T* field(const unsigned char* label) {
  return reinterpret_cast<T*>(g_page + (label - pg_steer_begin));
}

bool write_code(unsigned char* at, const void* bytes, size_t n) {
  DWORD old = 0;
  if (!VirtualProtect(at, n, PAGE_EXECUTE_READWRITE, &old)) return false;
  std::memcpy(at, bytes, n);
  VirtualProtect(at, n, old, &old);
  FlushInstructionCache(GetCurrentProcess(), at, n);
  return true;
}

unsigned char* alloc_near(unsigned char* target) {
  SYSTEM_INFO si;
  GetSystemInfo(&si);
  const uintptr_t gran = si.dwAllocationGranularity;
  const uintptr_t t = reinterpret_cast<uintptr_t>(target);
  for (uintptr_t delta = gran; delta < 0x70000000; delta += gran) {
    for (uintptr_t candidate : {(t - delta) & ~(gran - 1), (t + delta) & ~(gran - 1)}) {
      void* p = VirtualAlloc(reinterpret_cast<void*>(candidate), 0x1000, MEM_COMMIT | MEM_RESERVE,
                             PAGE_EXECUTE_READWRITE);
      if (p) return static_cast<unsigned char*>(p);
    }
  }
  return nullptr;
}

}  // namespace

bool install(void* unityPlayer) {
  if (g_site) return true;
  auto* base = static_cast<unsigned char*>(unityPlayer);
  unsigned char* site = base + kSiteRva;
  if (std::memcmp(site, kOriginal, sizeof kOriginal) != 0 ||
      std::memcmp(base + kGetInternalAgentRva, kGetInternalAgentPrologue, sizeof kGetInternalAgentPrologue) != 0) {
    PG_LOG("steer: UnityPlayer instructions differ from the audited build; steering unavailable");
    return false;
  }
  unsigned char* page = g_page ? g_page : alloc_near(site);  // the page outlives an uninstall: reused when started again
  if (!page) {
    PG_LOG("steer: no executable page within rel32 range");
    return false;
  }
  const size_t size = static_cast<size_t>(pg_steer_end - pg_steer_begin);
  std::memcpy(page, pg_steer_begin, size);
  g_page = page;
  *field<void*>(pg_steer_target) = nullptr;
  *field<int32_t>(pg_steer_active) = 0;
  *field<void*>(pg_steer_resume) = site + sizeof kOriginal;

  const intptr_t rel = reinterpret_cast<intptr_t>(page) - reinterpret_cast<intptr_t>(site + 5);
  unsigned char patch[8] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90};
  const int32_t rel32 = static_cast<int32_t>(rel);
  std::memcpy(patch + 1, &rel32, 4);
  if (!write_code(site, patch, sizeof patch)) {
    VirtualFree(page, 0, MEM_RELEASE);
    g_page = nullptr;
    PG_LOG("steer: patch write failed");
    return false;
  }
  g_site = site;
  g_getInternalAgent = reinterpret_cast<GetInternalAgent_t>(base + kGetInternalAgentRva);
  PG_LOG("steer: installed at %p, stub %p (%zu bytes)", site, page, size);
  return true;
}

void uninstall() {
  if (!g_site) return;
  clear();
  write_code(g_site, kOriginal, sizeof kOriginal);
  g_site = nullptr;
  // A crowd worker cannot still be inside the stub: this runs on the main thread after the
  // crowd update for the frame has completed. Keep the page anyway; it is 4 KB.
  PG_LOG("steer: uninstalled");
}

bool installed() { return g_site != nullptr; }

void* crowd_agent_for(void* managedAgent) {
  if (!g_getInternalAgent || !managedAgent) return nullptr;
  void* native = *reinterpret_cast<void**>(static_cast<char*>(managedAgent) + 0x10);  // m_CachedPtr
  return native ? g_getInternalAgent(native) : nullptr;
}

void set(void* crowdAgent, float dx, float dz) {
  if (!g_page || !crowdAgent) return;
  const float m2 = dx * dx + dz * dz;
  if (!std::isfinite(m2) || m2 > 1.0001f) {
    clear();
    return;
  }
  *field<float>(pg_steer_dx) = dx;
  *field<float>(pg_steer_dz) = dz;
  *field<void*>(pg_steer_target) = crowdAgent;
  std::atomic_thread_fence(std::memory_order_release);
  *field<int32_t>(pg_steer_active) = 1;
}

void clear() {
  if (!g_page) return;
  *field<int32_t>(pg_steer_active) = 0;
}

}  // namespace pg::steer
