#include "hooks.h"

#include <windows.h>

#include <MinHook.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

#include "hde64.h"
#include "log.h"

namespace pg::hooks {

std::atomic<int> g_inflight{0};
#ifdef PG_HOOKS_TEST
bool g_noPageNear = false;
#endif

namespace {

// A site patch: a 5-byte jmp rel32 to the stub's page over 5 to 32 bytes of whole instructions.
constexpr size_t kJumpBytes = 5;
constexpr size_t kMaxSiteBytes = 32;
// The jump back after a stub: jmp [rip+0] and the address it reads.
constexpr size_t kResumeBytes = 14;
// A stub's page: the stub and its jump back fit in one.
constexpr size_t kStubPage = 0x1000;
// How far from its site a stub's page is looked for: a rel32 reaches 2 GB either way.
constexpr uintptr_t kNear = 0x7FF00000;

// Each hook is one link of its target's chain. The link's relay jumps to its detour, and MinHook
// patches the relay, not the detour, when a newer link chains on the same target. A site patch
// (patch_site) is recorded among the hooks so that it unwinds with them, in order.
struct Created {
  void* target;  // the game function, or a patched site
  // What MinHook patched: the target, or the relay of the link before this one. A site patch's site.
  void* at;
  void* detour;  // null for a site patch
  bool enabled;  // for a site patch, whether its jump is in place
  // A site patch's size (0 for a hook), the bytes it replaced and the bytes it wrote.
  size_t siteBytes = 0;
  std::array<uint8_t, kMaxSiteBytes> original{}, written{};
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
  if (!VirtualProtect(relay, kRelayBytes, old, &old))
    PG_LOG("hooks: the relay at %p stays writable: restoring its protection failed (%lu)", relay, GetLastError());
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

// Writes `size` bytes of code at `at`, making them writable for the write and restoring their
// protection after it. False, logged and nothing written, when they cannot be made writable.
bool write_code(void* at, const void* bytes, size_t size) {
  DWORD old = 0;
  if (!VirtualProtect(at, size, PAGE_EXECUTE_READWRITE, &old)) {
    PG_LOG("hooks: the code at %p cannot be made writable (%lu)", at, GetLastError());
    return false;
  }
  std::memcpy(at, bytes, size);
  if (!VirtualProtect(at, size, old, &old))
    PG_LOG("hooks: the code at %p stays writable: restoring its protection failed (%lu)", at, GetLastError());
  FlushInstructionCache(GetCurrentProcess(), at, size);
  return true;
}

// Whether `size` bytes at `at` are committed code this process may read.
bool readable_code(const void* at, size_t size) {
  constexpr DWORD kReadableCode = PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  for (uintptr_t p = reinterpret_cast<uintptr_t>(at), end = p + size; p < end;) {
    MEMORY_BASIC_INFORMATION region{};
    if (!VirtualQuery(reinterpret_cast<const void*>(p), &region, sizeof region) || region.State != MEM_COMMIT ||
        (region.Protect & PAGE_GUARD) || !(region.Protect & kReadableCode))
      return false;
    p = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
  }
  return true;
}

// Whether `bytes` are whole instructions, as HDE64 decodes them. VEX and EVEX forms (C4, C5, 62),
// which HDE64 does not know, are not.
bool whole_instructions(const uint8_t* bytes, size_t size) {
  for (size_t at = 0; at < size;) {
    uint8_t buffer[32] = {};  // HDE64 reads past the instruction, so it reads a zero-padded copy
    std::memcpy(buffer, bytes + at, std::min<size_t>(size - at, 16));
    hde64s hs{};
    hde64_disasm(buffer, &hs);
    if ((hs.flags & F_ERROR) || hs.len == 0 || hs.len > size - at || hs.opcode == 0xC4 || hs.opcode == 0xC5 ||
        hs.opcode == 0x62)
      return false;
    at += hs.len;
  }
  return true;
}

// Whether `size` bytes at `site` overlap a site patch in place or the 5 bytes before or after a
// hook's patched address (MinHook may patch either side).
bool overlaps(const uint8_t* site, size_t size) {
  const auto begin = reinterpret_cast<uintptr_t>(site), end = begin + size;
  for (const Created& c : g_created) {
    const auto at = reinterpret_cast<uintptr_t>(c.at);
    const bool hit = c.siteBytes ? c.enabled && at < end && begin < at + c.siteBytes
                                 : at + kJumpBytes > begin && at < end + kJumpBytes;
    if (hit) return true;
  }
  return false;
}

// A page of kStubPage bytes, readable, writable and executable, within rel32 range of `site`, or null.
uint8_t* page_near(uintptr_t site) {
#ifdef PG_HOOKS_TEST
  if (g_noPageNear) return nullptr;
#endif
  SYSTEM_INFO system{};
  GetSystemInfo(&system);
  const uintptr_t granule = system.dwAllocationGranularity;
  const auto lowest = reinterpret_cast<uintptr_t>(system.lpMinimumApplicationAddress);
  const auto highest = reinterpret_cast<uintptr_t>(system.lpMaximumApplicationAddress);
  for (uintptr_t delta = granule; delta < kNear; delta += granule) {
    for (const bool below : {true, false}) {
      if (below ? site < lowest + delta : site > highest - delta) continue;
      const uintptr_t candidate = (below ? site - delta : site + delta) & ~(granule - 1);
      MEMORY_BASIC_INFORMATION region{};
      if (!VirtualQuery(reinterpret_cast<void*>(candidate), &region, sizeof region) || region.State != MEM_FREE)
        continue;
      if (void* page = VirtualAlloc(reinterpret_cast<void*>(candidate), kStubPage, MEM_COMMIT | MEM_RESERVE,
                                    PAGE_EXECUTE_READWRITE))
        return static_cast<uint8_t*>(page);
    }
  }
  return nullptr;
}

// Writes a site patch's replaced bytes back, unless something else has overwritten its jump since.
void unpatch(Created& c) {
  if (!c.enabled) return;
  c.enabled = false;
  if (std::memcmp(c.at, c.written.data(), c.siteBytes) != 0) {
    PG_LOG("hooks: the patch at %p was overwritten since it was written; its bytes are left as they are", c.at);
    return;
  }
  if (write_code(c.at, c.original.data(), c.siteBytes)) PG_LOG("hooks: restored the %zu bytes at %p", c.siteBytes, c.at);
  else PG_LOG("hooks: the patch at %p stays: its bytes could not be written back", c.at);
}

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
    PG_LOG("hooks: %s has no %s", name, target ? "detour" : "target");
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

bool patch_site(void* site, const void* expected, size_t expectedSize, const void* stub, size_t stubSize, void** page,
                const char* name) {
  if (page) *page = nullptr;
  if (!name) name = "a site patch";
  if (!site || !expected || !stub || !page) {
    PG_LOG("hooks: %s: no site, expected bytes, stub or page; nothing was written", name);
    return false;
  }
  if (expectedSize < kJumpBytes || expectedSize > kMaxSiteBytes) {
    PG_LOG("hooks: %s: a patch replaces %zu to %zu bytes, not %zu; nothing was written", name, kJumpBytes, kMaxSiteBytes,
           expectedSize);
    return false;
  }
  if (stubSize == 0 || stubSize > kStubPage - kResumeBytes) {
    PG_LOG("hooks: %s: a stub has 1 to %zu bytes, not %zu; nothing was written", name, kStubPage - kResumeBytes,
           stubSize);
    return false;
  }
  if (!whole_instructions(static_cast<const uint8_t*>(expected), expectedSize)) {
    PG_LOG("hooks: %s: the expected bytes are not whole instructions; nothing was written", name);
    return false;
  }
  std::lock_guard lock(g_mutex);
  auto* const at = static_cast<uint8_t*>(site);
  if (overlaps(at, expectedSize)) {
    PG_LOG("hooks: %s: the site at %p is already patched or hooked; nothing was written", name, at);
    return false;
  }
  if (!readable_code(at, expectedSize)) {
    PG_LOG("hooks: %s: the site at %p is not readable code; nothing was written", name, at);
    return false;
  }
  if (std::memcmp(at, expected, expectedSize) != 0) {
    PG_LOG("hooks: %s: the %zu bytes at %p differ from the expected ones; nothing was written", name, expectedSize, at);
    return false;
  }
  uint8_t* const stubPage = page_near(reinterpret_cast<uintptr_t>(at));
  if (!stubPage) {
    PG_LOG("hooks: %s: no page within rel32 range of %p; nothing was written", name, at);
    return false;
  }
  const intptr_t distance = reinterpret_cast<intptr_t>(stubPage) - reinterpret_cast<intptr_t>(at + kJumpBytes);
  if (distance < INT32_MIN || distance > INT32_MAX) {
    VirtualFree(stubPage, 0, MEM_RELEASE);
    PG_LOG("hooks: %s: the page at %p is out of rel32 range of %p; nothing was written", name, stubPage, at);
    return false;
  }
  // The stub, then the jump back to the instruction after the replaced ones.
  std::memcpy(stubPage, stub, stubSize);
  const uint8_t jumpBack[6] = {0xFF, 0x25, 0, 0, 0, 0};  // jmp [rip+0]: the address that follows
  const uint8_t* const resume = at + expectedSize;
  std::memcpy(stubPage + stubSize, jumpBack, sizeof jumpBack);
  std::memcpy(stubPage + stubSize + sizeof jumpBack, &resume, sizeof resume);
  FlushInstructionCache(GetCurrentProcess(), stubPage, stubSize + kResumeBytes);

  Created patch{site, site, nullptr, true, expectedSize};
  std::memcpy(patch.original.data(), at, expectedSize);
  patch.written.fill(0x90);  // nop
  patch.written[0] = 0xE9;   // jmp rel32
  const auto rel32 = static_cast<int32_t>(distance);
  std::memcpy(patch.written.data() + 1, &rel32, sizeof rel32);
  g_created.reserve(g_created.size() + 1);  // so the record cannot fail once the jump is written
  if (!write_code(at, patch.written.data(), expectedSize)) {
    VirtualFree(stubPage, 0, MEM_RELEASE);
    PG_LOG("hooks: %s: the jump at %p could not be written; nothing was written", name, at);
    return false;
  }
  g_created.push_back(patch);
  *page = stubPage;
  PG_LOG("hooks: patched %s at %p, stub at %p (%zu bytes)", name, at, stubPage, stubSize);
  return true;
}

bool enable_all() {
  std::lock_guard lock(g_mutex);
  bool queued = false;
  for (size_t i = g_groupStart; i < g_created.size(); ++i) {
    if (g_created[i].enabled || g_created[i].siteBytes) continue;  // a site patch is live as written
    if (MH_QueueEnableHook(g_created[i].at) != MH_OK) return false;
    queued = true;
  }
  MH_STATUS s = queued ? MH_ApplyQueued() : MH_OK;
  if (s != MH_OK) {
    PG_LOG("hooks: enable failed: %s", MH_StatusToString(s));
    return false;
  }
  for (size_t i = g_groupStart; i < g_created.size(); ++i)
    if (!g_created[i].siteBytes) g_created[i].enabled = true;
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
    Created c = g_created.back();
    g_created.pop_back();
    if (c.siteBytes) {
      unpatch(c);
      continue;
    }
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
  {
    std::lock_guard lock(g_mutex);
    for (auto c = g_created.rbegin(); c != g_created.rend(); ++c)
      if (c->siteBytes) unpatch(*c);
  }
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
