#include "hooks.h"

#include <windows.h>

#include <MinHook.h>

#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "log.h"

namespace pg::hooks {

std::atomic<int> g_inflight{0};

namespace {

// Each hook is one link of its target's chain. The link's relay jumps to its detour, and MinHook
// patches the relay, not the detour, when a newer link chains on the same target.
struct Created {
  void* target;  // the game function
  void* at;      // what MinHook patched: the target, or the relay of the link before this one
  void* detour;
  bool enabled;
};

bool g_initialised = false;
std::mutex g_mutex;
// Each hooked game function and the relay of the link that currently runs first for it. A second
// hook on the same function hooks that relay, so the newest detour runs first and its trampoline
// reaches the one before it.
std::map<void*, void*> g_head;
std::vector<Created> g_created;  // in creation order
size_t g_groupStart = 0;         // hooks from here on belong to the open group

// The relays: `jmp [rip+0]` and the detour's address, 16 bytes each, in executable blocks of the
// host's own. A detour may run on several targets (hook_entry's all-twins hook does that), so
// patching the detour to chain on one target would put the newer hook on all of them; patching the
// link's own relay puts it on that target's chain alone. A relay is never freed or reused: a call
// may still be passing through one after its link is removed.
constexpr size_t kRelayBytes = 16;
constexpr size_t kRelayBlock = 0x10000;
uint8_t* g_relays = nullptr;  // the current block
size_t g_relaysUsed = kRelayBlock / kRelayBytes;

// Writes a relay to `detour` in the next free slot, which stays free until take_relay.
void* write_relay(void* detour) {
  if (g_relaysUsed == kRelayBlock / kRelayBytes) {
    void* block = VirtualAlloc(nullptr, kRelayBlock, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READ);
    if (!block) return nullptr;
    g_relays = static_cast<uint8_t*>(block);
    g_relaysUsed = 0;
  }
  uint8_t* relay = g_relays + g_relaysUsed * kRelayBytes;
  DWORD old = 0;
  // The page keeps execute access throughout: other relays on it may be running.
  if (!VirtualProtect(relay, kRelayBytes, PAGE_EXECUTE_READWRITE, &old)) return nullptr;
  const uint8_t jump[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp [rip+0]: the address that follows
  std::memcpy(relay, jump, sizeof jump);
  std::memcpy(relay + sizeof jump, &detour, sizeof detour);
  std::memset(relay + sizeof jump + sizeof detour, 0xCC, kRelayBytes - sizeof jump - sizeof detour);
  VirtualProtect(relay, kRelayBytes, old, &old);
  FlushInstructionCache(GetCurrentProcess(), relay, kRelayBytes);
  return relay;
}

void take_relay() { ++g_relaysUsed; }

void* head(void* target) {
  auto first = g_head.find(target);
  return first == g_head.end() ? nullptr : first->second;
}

bool repeated(void* target, void* detour, const char* name) {
  for (const Created& c : g_created) {
    if (c.target == target && c.detour == detour) {
      PG_LOG("hooks: %s is already hooked with this detour", name);
      return true;
    }
  }
  return false;
}

void remove_since(size_t start);

}  // namespace

bool init() {
  MH_STATUS s = MH_Initialize();
  if (s != MH_OK) {
    PG_LOG("hooks: MH_Initialize failed: %s", MH_StatusToString(s));
    return false;
  }
  g_initialised = true;
  return true;
}

bool create(void* target, void* detour, void** original, const char* name) {
  if (!target || !detour) {
    PG_LOG("hooks: %s has no target", name);
    return false;
  }
  std::lock_guard lock(g_mutex);
  if (repeated(target, detour, name)) return false;
  void* relay = write_relay(detour);
  if (!relay) {
    PG_LOG("hooks: create %s at %p failed: no memory for its relay", name, target);
    return false;
  }
  void* earlier = head(target);
  void* at = earlier ? earlier : target;
  MH_STATUS s = MH_CreateHook(at, relay, original);
  if (s != MH_OK) {
    PG_LOG("hooks: create %s at %p failed: %s", name, at, MH_StatusToString(s));
    return false;
  }
  take_relay();
  g_head[target] = relay;
  g_created.push_back({target, at, detour, false});
  if (at == target) PG_LOG("hooks: created %s at %p", name, target);
  else PG_LOG("hooks: created %s at %p, chained ahead of an earlier detour", name, target);
  return true;
}

bool enable_all() {
  std::lock_guard lock(g_mutex);
  bool queued = false;
  for (size_t i = g_groupStart; i < g_created.size(); ++i) {
    if (g_created[i].enabled) continue;
    if (MH_QueueEnableHook(g_created[i].at) != MH_OK) return false;
    queued = true;
  }
  MH_STATUS s = queued ? MH_ApplyQueued() : MH_OK;
  if (s != MH_OK) {
    PG_LOG("hooks: enable failed: %s", MH_StatusToString(s));
    return false;
  }
  for (size_t i = g_groupStart; i < g_created.size(); ++i) g_created[i].enabled = true;
  return true;
}

void begin_group() {
  std::lock_guard lock(g_mutex);
  g_groupStart = g_created.size();
}

namespace {

// Newest first, so a detour chained onto another is removed before the one it chained onto.
void remove_since(size_t start) {
  while (g_created.size() > start) {
    const Created c = g_created.back();
    g_created.pop_back();
    if (c.enabled) MH_DisableHook(c.at);
    MH_RemoveHook(c.at);
    // The link before this one leads again: its relay is what this one patched.
    if (c.at == c.target) g_head.erase(c.target);
    else g_head[c.target] = c.at;
  }
  if (g_groupStart > start) g_groupStart = start;
}

}  // namespace

void rollback_group() {
  std::lock_guard lock(g_mutex);
  remove_since(g_groupStart);  // only the open group adds hooks, so nothing has chained onto these
  PG_LOG("hooks: removed the hooks of a mod that did not start");
}

size_t mark() {
  std::lock_guard lock(g_mutex);
  return g_created.size();
}

void rollback_to(size_t mark) {
  std::lock_guard lock(g_mutex);
  const size_t removed = g_created.size() > mark ? g_created.size() - mark : 0;
  remove_since(mark);
  PG_LOG("hooks: removed %zu hook(s)", removed);
}

void shutdown() {
  if (!g_initialised) return;
  MH_DisableHook(MH_ALL_HOOKS);
  // A detour that entered before disabling may still be running on the main thread.
  for (int i = 0; i < 500 && g_inflight.load(std::memory_order_acquire) > 0; ++i) Sleep(10);
  Sleep(100);  // let returning detours leave their epilogues
  MH_Uninitialize();
  g_initialised = false;
  PG_LOG("hooks: shut down (inflight=%d)", g_inflight.load());
}

}  // namespace pg::hooks
