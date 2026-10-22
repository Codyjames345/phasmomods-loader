#include "markers.h"

#include <windows.h>

#include <vector>

#include "expand.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "paging.h"

namespace pg::lobbies::markers {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load).
constexpr size_t kOtherMarks = 0x60;                // NetworkThreeStateButton: ThreeStateButton[], one per player slot
constexpr uintptr_t kSetOtherStateRva = 0x10D0920;  // NetworkThreeStateButton.SetOtherPlayerButtonState(Player, state)
constexpr ULONGLONG kScanMs = 2000;

using SetOther_t = void (*)(void*, void*, int, const void*);
SetOther_t g_setOther = nullptr;

struct Api {
  il2cpp::Object* buttonType = nullptr;
  il2cpp::Array* (*findAll)(il2cpp::Object*, bool, const void*) = nullptr;
} api;

ULONGLONG g_nextScan = 0;
int g_grownCount = 0;

void detour_set_other(void* button, void* player, int state, const void* method) {
  hooks::Guard guard;
  try {
    g_setOther(button, player, state, method);
  } catch (const Il2CppExceptionWrapper&) {
    // A player with no mark on this button (one not grown yet): their choice is not shown here.
  }
}

bool grow(void* button) {
  auto* marks = il2cpp::field<il2cpp::Array*>(button, kOtherMarks);
  if (!marks || marks->max_length != static_cast<uintptr_t>(kVanilla)) return false;
  void* third = marks->data<void*>()[kVanilla - 2];
  void* last = marks->data<void*>()[kVanilla - 1];
  if (!third || !last || !game::alive(last)) return false;
  const Vector3 a = game::local_position(game::transform_of(third)), b = game::local_position(game::transform_of(last));
  const Vector3 step{b.x - a.x, b.y - a.y, b.z - a.z};
  std::vector<void*> more;
  for (int k = kVanilla; k < kPlayers; ++k) {
    void* copy = expand::clone(last);
    if (!copy) return false;
    const float n = static_cast<float>(k - kVanilla + 1);
    game::set_local_position(game::transform_of(copy), {b.x + step.x * n, b.y + step.y * n, b.z + step.z * n});
    more.push_back(copy);
  }
  il2cpp::Array* all = expand::grown(marks, more);
  if (!all) return false;
  il2cpp::set_ref_field(button, kOtherMarks, all);
  return true;
}

}  // namespace

bool resolve() {
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  api.buttonType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "NetworkThreeStateButton"));
  return api.buttonType && il2cpp::bind(api.findAll, object, "FindObjectsOfType", 2);
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kSetOtherStateRva), reinterpret_cast<void*>(&detour_set_other),
                       reinterpret_cast<void**>(&g_setOther), "NetworkThreeStateButton.SetOtherPlayerButtonState");
}

void tick() {
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextScan) return;
  g_nextScan = now + kScanMs;  // journals (and the Custom Ghosts mod's buttons) come and go with scenes
  il2cpp::Array* buttons = api.findAll(api.buttonType, true, nullptr);
  int grownNow = 0;
  for (uintptr_t i = 0; buttons && i < buttons->max_length; ++i)
    if (void* b = buttons->data<void*>()[i]; b && grow(b)) ++grownNow;
  if (grownNow) {
    g_grownCount += grownNow;
    PG_LOG("markers: %d journal buttons now mark %d players (%d in all)", grownNow, kPlayers, g_grownCount);
  }
}

}  // namespace pg::lobbies::markers
