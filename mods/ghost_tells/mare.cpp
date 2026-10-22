#include "mare.h"

#include <random>
#include <vector>

#include "game.h"
#include "gate.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "mare_rule.h"
#include "rolls.h"
#include "tells.h"
#include "world.h"

namespace pg::tells::mare {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (class dump, REA).
constexpr size_t kRoomSwitches = 0x20;          // LevelRoom: List<LightSwitch>
constexpr size_t kSwitchBlocked = 0x68;         // LightSwitch: UseLightSwitch refuses while set
constexpr size_t kSwitchCooling = 0xE0;         // LightSwitch: refused to a ghost's use while set
constexpr uintptr_t kAppearRva = 0x2020FB0;     // GhostAI.Appear(int form): RPC to every client
constexpr uintptr_t kLightsOnRva = 0x8A9CD0;    // LevelRoom.AreRoomLightsOn(out bool flickering)
// The Mare's switch-off coroutine (LightSwitch nested MoveNext): Random.Range(0, 7) == 1, then the switch-off.
constexpr uintptr_t kSwitchOffRva = 0x26FFD10;
constexpr int kStateIdle = 0, kStateAppear = 8, kStateRandomEvent = 14;

struct Api {
  bool (*lightsOn)(void*, bool*, const void*) = nullptr;
  bool (*isOn)(void*, const void*) = nullptr;                    // LightSwitch.get_IsSwitchOn
  void (*use)(void*, bool, bool, bool, const void*) = nullptr;   // LightSwitch.UseLightSwitch
  bool ok = false;
} api;

using Appear_t = void (*)(void*, int, const void*);
using MoveNext_t = bool (*)(void*, const void*);
Appear_t g_appear = nullptr;
MoveNext_t g_switchOff = nullptr;
bool g_inSwitchOff = false;  // inside the switch-off coroutine's step: its roll is the one answered

struct State {
  il2cpp::Held ghost;
  bool mare = false;
  bool wasHunting = false;
  bool endEvent = false;  // an appearance it withheld: its event ends next frame
  float sinceLook = 0;
  RoomTrail trail;
} g;

std::mt19937 g_random{std::random_device{}()};

template <class F>
void each_switch(void* room, F visit) {
  auto* list = room ? il2cpp::field<il2cpp::List*>(room, kRoomSwitches) : nullptr;
  for (int i = 0; list && list->items && i < list->size; ++i)
    if (void* s = list->items->data<void*>()[i]; s && game::alive(s)) visit(s);
}

bool usable(void* light) { return !il2cpp::field<bool>(light, kSwitchBlocked) && !il2cpp::field<bool>(light, kSwitchCooling); }

/** Every lit, usable switch of a room off, as the Mare's own switch-off does it. */
void darken(void* room) {
  each_switch(room, [](void* light) {
    if (api.isOn(light, nullptr) && usable(light)) api.use(light, false, true, true, nullptr);
  });
}

bool detour_switch_off(void* routine, const void* method) {
  hooks::Guard guard;
  struct Inside {  // cleared however the step ends, a managed exception included
    Inside() { g_inSwitchOff = true; }
    ~Inside() { g_inSwitchOff = false; }
  } inside;
  return g_switchOff(routine, method);
}

bool roll_switch_off(int min, int max, int& result) {
  if (!api.ok || !mare_roll_applies(g_inSwitchOff, min, max)) return false;
  if (!tells_on() || world::behaviour_type(game::ghost_ai()) != kMare) return false;
  result = mare_roll(std::uniform_int_distribution<int>(0, kMareSwitchOffOneIn - 1)(g_random));
  return true;
}

void detour_appear(void* ai, int form, const void* method) {
  hooks::Guard guard;
  if (!api.ok || !g.mare || ai != g.ghost || !game::is_master() || world::hunting(ai) || cursed_pull(game::ghost_state(ai))) {
    g_appear(ai, form, method);
    return;
  }
  void* room = world::room_at(world::feet(ai));
  bool flickering = false;
  const bool lit = room && api.lightsOn(room, &flickering, nullptr);
  int litSwitches = 0, usableLit = 0;
  each_switch(room, [&](void* light) {
    if (!api.isOn(light, nullptr)) return;
    ++litSwitches;
    if (usable(light)) ++usableLit;
  });
  switch (mare_appearance(lit, litSwitches, usableLit)) {
    case Appearance::DarkenFirst:
      darken(room);
      PG_LOG("mare: the lights go out before it shows itself");
      break;
    case Appearance::Withhold:
      // Not shown at all: the event that asked for the appearance ends as the next frame begins, rather
      // than from inside the event's own call here, through the game's own change to idle.
      g.endEvent = true;
      PG_LOG("mare: it will not show itself in a room it cannot darken");
      return;
    case Appearance::Appear: break;
  }
  g_appear(ai, form, method);
}

}  // namespace

bool resolve() {
  il2cpp::Class* light = il2cpp::cls("Assembly-CSharp.dll", "", "LightSwitch");
  api.lightsOn = reinterpret_cast<decltype(api.lightsOn)>(game::game_assembly_address(kLightsOnRva));
  api.ok = api.lightsOn && il2cpp::bind(api.isOn, light, "get_IsSwitchOn", 0) && il2cpp::bind(api.use, light, "UseLightSwitch", 3);
  PG_LOG("mare: resolve %s", api.ok ? "ok" : "unavailable (it plays as in the game)");
  rolls::add_roller(&roll_switch_off);
  return true;  // optional: without it the Mare is vanilla
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kAppearRva), reinterpret_cast<void*>(&detour_appear),
                       reinterpret_cast<void**>(&g_appear), "GhostAI.Appear") &&
         hooks::create(game::game_assembly_address(kSwitchOffRva), reinterpret_cast<void*>(&detour_switch_off),
                       reinterpret_cast<void**>(&g_switchOff), "LightSwitch.MareSwitchOff");
}

void tick(void* ghost, int type, bool host, float dt) {
  if (ghost != g.ghost || (type == kMare) != g.mare) {
    reset();
    g.ghost = ghost;
    g.mare = type == kMare;
  }
  if (!g.mare || !host || !api.ok) return;
  if (g.endEvent) {
    g.endEvent = false;
    const int state = game::ghost_state(ghost);
    if (state == kStateRandomEvent || state == kStateAppear) gate::change_state(ghost, kStateIdle);
  }
  const bool hunting = world::hunting(ghost);
  if (hunting) {
    g.sinceLook += dt;
    if (g.sinceLook >= kMareTrailStep) {
      g.sinceLook = 0;
      g.trail.pass(world::room_index(world::room_at(world::feet(ghost))));
    }
  } else if (g.wasHunting) {
    // Light switches cannot be used in a hunt, so the rooms it passed through go dark as it ends.
    for (int index : g.trail.rooms()) darken(world::room_by_index(index));
    PG_LOG("mare: the hunt leaves %zu rooms dark", g.trail.rooms().size());
    g.trail.clear();
  }
  g.wasHunting = hunting;
}

void reset() { g = State{}; }

}  // namespace pg::tells::mare
