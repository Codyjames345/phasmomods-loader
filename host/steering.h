#pragma once

// Steering an agent's direction inside Unity's crowd simulation, the host contract's steer_agent. The host owns the
// native side: its stub (steer_stub.asm), what the stub assumes at the map's UnityPlayer.SteerSite, and the managed
// agent's way to its CrowdAgent. It reads those entries itself (pm::map::kSteeringEntries), which a mod never names
// or requires, so how the host steers can change without a mod rebuild. The patch is one of hooks.h's site patches,
// so it is restored as they are. Main thread, outside the crowd update: the host's LateUpdate, or a mod's start or
// tick in it.

#include <array>
#include <cstddef>
#include <string>

#include "UnityPlayer.h"

namespace pm::steering {

namespace detail {

constexpr int hex_value(char c) {
  return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
}

// `N` bytes of the site's pattern from its byte `from`, at compile time. A wildcard among them, or a pattern too short,
// does not compile.
template <size_t N>
consteval std::array<unsigned char, N> pinned(size_t from = 0) {
  std::array<unsigned char, N> out{};
  const char* p = pm::map::UnityPlayer_SteerSite.pattern;
  for (size_t i = 0; i < from + N; ++i) {
    while (*p == ' ') ++p;
    const int high = hex_value(p[0]), low = high < 0 ? -1 : hex_value(p[1]);
    if (low < 0) throw "a pinned byte is a wildcard, or lies past the end of the pattern";
    if (i >= from) out[i - from] = static_cast<unsigned char>(high * 16 + low);
    p += 2;
  }
  return out;
}

}  // namespace detail

// The instruction the jump replaces, which the stub runs itself: the bytes the patch expects at the site.
inline constexpr std::array<unsigned char, 8> kReplaced = detail::pinned<8>();
// Where in the stack frame, at the site, the agent's direction's Z lies: the displacement of the pattern's third
// instruction, `movss xmm1, [rsp+disp8]`.
inline constexpr unsigned char kDirectionZ = detail::pinned<1>(17)[0];

// What the stub assumes of the pattern. A map change that breaks one fails the host's build rather than building a
// wrong stub. The replaced instruction is `movss xmm0, [rbx+disp32]`, 8 bytes with nothing RIP-relative, so the stub may
// run it at another address and RBX is the agent. The third instruction is `movss xmm1, [rsp+disp8]`, so its last byte
// is the frame slot of the direction's Z.
static_assert(detail::pinned<4>(0) == std::array<unsigned char, 4>{0xF3, 0x0F, 0x10, 0x83},
              "the replaced instruction is no longer movss xmm0, [rbx+disp32]");
static_assert(detail::pinned<5>(12) == std::array<unsigned char, 5>{0xF3, 0x0F, 0x10, 0x4C, 0x24},
              "the pattern's third instruction is no longer movss xmm1, [rsp+disp8]");

// Steers `agent` until the next expire, putting the patch in place first when it is not.
//  - `agent` is a managed NavMeshAgent. Null, destroyed or of another class: nothing changes.
//  - (dx, dz) replaces its CrowdAgent's direction, |d| <= 1.
//  - A direction that is not one (longer than 1, not finite) ends this frame's steering of any agent, until a valid
//    ask after it.
// Returns whether the host can steer: false (logged) when an entry did not resolve or the patch was refused, and
// nothing is written then. After a refusal it tries the patch no more until reset(), so asking every frame never
// stops every thread every frame.
bool steer(void* agent, float dx, float dz);

// Ends what was asked, so the stub passes straight on. The host calls it before the mods' ticks, so an agent stays
// steered only while a mod keeps asking. It also calls it when the mods stop. The crowd update of the frame ahead runs
// after those ticks.
void expire();

// Forgets a refused patch, so the next ask tries it once more. The host calls it as the mods start afresh.
void reset();

// The steering entries (pm::map::kSteeringEntries) that did not resolve, separated by ", "; empty when none. The host
// logs it at startup.
std::string missing();

}  // namespace pm::steering
