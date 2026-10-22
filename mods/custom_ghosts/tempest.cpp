#include "tempest.h"

#include <windows.h>

#include <random>
#include <vector>

#include "apparition.h"
#include "flame_net.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"
#include "tempest_rule.h"

namespace pg::custom::tempest {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. From the class dump
// (LightningController, RandomWeather, AzureEffectsController, the weather type enum) and the
// decompiles of PlayLocalLightning, PlayLightningNetworked and HoopCounter.OnTriggerExit (2026-10-22).
constexpr size_t kLevelValuesWeather = 0x98;  // LevelValues: the contract's WeatherProfile
constexpr size_t kWeatherType = 0x20;         // WeatherProfile: its weather type (tempest_rule.h)
constexpr int kWeatherDead = 8;               // the weather type of the dead players' profile, never the contract's
constexpr size_t kGhostHunting = 0xF3;        // GhostAI: hunting, on every client
constexpr size_t kModelRenderers = 0x30;      // GhostModel: Renderer[] (SetVisibility switches these)
constexpr size_t kLightningEffects = 0x28;    // LightningController.effects (AzureEffectsController): null in a contract
constexpr size_t kWeatherEffects = 0x38;      // RandomWeather.effects: the AzureEffectsController its storm strikes from
constexpr size_t kEffectsThunder = 0xA8;      // AzureEffectsController.thunderSettingsList: List<AzureThunderSettings>
constexpr size_t kListSize = 0x18;            // List<T>._size
constexpr uintptr_t kLocalLightningRva = 0x231A070;  // LightningController.PlayLocalLightning(): a strike on this client only
constexpr uintptr_t kStormLightningRva = 0x2319F40;  // LightningController.PlayLightningNetworked(info): the game's storm strike

using StormLightning_t = void (*)(void*, void*, const void*);
StormLightning_t g_stormLightning = nullptr;

struct Api {
  il2cpp::Class* weatherClass = nullptr;
  il2cpp::Object* lightningType = nullptr;
  void* (*findObject)(il2cpp::Object*, const void*) = nullptr;
  void (*lightning)(void*, const void*) = nullptr;
  bool ok = false;
} api;

struct State {
  bool haunting = false;   // a Tempest haunts this contract
  ULONGLONG lastStrike = 0;
  bool shown = false;      // a strike showed it in a hunt last frame
  il2cpp::Held lightning;  // the scene's LightningController
  bool lightningFailed = false;  // logged once: no lightning to play
  bool lightningThrew = false;   // the game's lightning threw: none is played again this contract
  // host
  StrikeClock clock;
  bool eventStrike = false;  // one of its events began: the next tick strikes
  int strikes = 0;           // in the current hunt
  bool skyKnown = false;
  Sky sky = Sky::Clear;
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

int contract_weather() {
  void* values = game::level_values();
  void* weather = values ? il2cpp::field<void*>(values, kLevelValuesWeather) : nullptr;
  return weather ? il2cpp::field<int>(weather, kWeatherType) : -1;
}

/** The contract's sky, read once it is there (never the dead players' profile). */
Sky sky() {
  if (!g.skyKnown) {
    const int type = contract_weather();
    if (type < 0 || type == kWeatherDead) return Sky::Clear;
    g.sky = tempest_sky(type);
    g.skyKnown = true;
    PG_LOG("tempest: weather %d, so it strikes %s", type,
           g.sky == Sky::HeavyRain ? "often" : g.sky == Sky::Rain ? "now and then" : "rarely");
  }
  return g.sky;
}

float since_strike() { return g.lastStrike ? (GetTickCount64() - g.lastStrike) / 1000.0f : -1.0f; }

void* lightning_controller() {
  if (!g.lightning.get() || !game::alive(g.lightning.get())) {
    void* found = api.findObject(api.lightningType, nullptr);
    g.lightning = il2cpp::Held(found && game::alive(found) ? found : nullptr);
  }
  return g.lightning.get();
}

void cannot_strike(const char* why) {
  if (!g.lightningFailed) PG_LOG("tempest: no lightning to play (%s)", why);
  g.lightningFailed = true;
}

/**
 * The game's own lightning on this client. PlayLocalLightning (the lobby hoop's) strikes from its
 * controller's effects, which no contract scene sets, so it throws there; the game's storm strike
 * uses the weather's own effects, so the controller is given those before it first strikes.
 */
void play_lightning() {
  if (g.lightningThrew) return;
  void* controller = lightning_controller();
  if (!controller) {
    cannot_strike("no LightningController");
    return;
  }
  if (!il2cpp::field<void*>(controller, kLightningEffects)) {
    void* weather = il2cpp::static_field_by_type(api.weatherClass, "RandomWeather");
    void* effects = weather && game::alive(weather) ? il2cpp::field<void*>(weather, kWeatherEffects) : nullptr;
    if (!effects || !game::alive(effects)) {
      cannot_strike("no weather effects");
      return;
    }
    il2cpp::set_ref_field(controller, kLightningEffects, effects);
  }
  void* effects = il2cpp::field<void*>(controller, kLightningEffects);
  void* thunder = il2cpp::field<void*>(effects, kEffectsThunder);
  if (!thunder || il2cpp::field<int>(thunder, kListSize) < 1) {  // PlayLocalLightning would throw on an empty list
    cannot_strike("no thunder settings");
    return;
  }
  // Its other throws (a null entry or prefab, a prefab without the thunder component) are the scene's;
  // the ghost still shows and speeds by its strikes without the lightning.
  try {
    api.lightning(controller, nullptr);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("tempest: the game's lightning threw, so none is played again this contract", e.ex);
    g.lightningThrew = true;
  } catch (...) {
    PG_LOG("tempest: the game's lightning threw, so none is played again this contract");
    g.lightningThrew = true;
  }
}

/** A strike on this client: the moment it lights the Tempest up, and the game's lightning unless it already played. */
void strike_here(bool play) {
  g.lastStrike = GetTickCount64();
  if (play) play_lightning();
}

/** The host: a strike on every client. Reliable, so no client misses the moment the ghost shows. */
void strike_everywhere() {
  const std::vector<uint8_t> message = net_msg::strike();
  net::send(message.data(), message.size(), net::To::Others, true);
  strike_here(true);
}

/**
 * The host: every hunt opens with a strike, more follow as often as the weather says, and one strikes
 * as each of its events begins. All its state moves on before it strikes.
 */
void host_tick(void* ghost, ULONGLONG now) {
  const bool hunting = game::ghost_state(ghost) == game::kStateHunting;
  const bool opens = hunting && !g.clock.hunting;
  if (!hunting && g.clock.hunting) {
    PG_LOG("tempest: its hunt is over after %d strikes", g.strikes);
    g.strikes = 0;
  }
  const float gap = tempest_next_strike(sky(), std::uniform_real_distribution<float>(0, 1)(g_random));
  bool strike = tempest_strike_now(g.clock, hunting, now, static_cast<uint64_t>(gap * 1000));
  if (strike) {
    if (opens) PG_LOG("tempest: its hunt opens with a strike, the next in %.1f s", gap);
    ++g.strikes;
  }
  strike |= g.eventStrike;
  g.eventStrike = false;
  if (strike) strike_everywhere();
}

void set_model(void* ghost, bool visible) {
  void* model = apparition::model_of(ghost);
  auto* renderers = model ? il2cpp::field<il2cpp::Array*>(model, kModelRenderers) : nullptr;
  for (uintptr_t i = 0; renderers && i < renderers->max_length; ++i)
    if (void* r = renderers->data<void*>()[i]; r && game::alive(r)) game::set_renderer_enabled(r, visible);
}

/** Every client: in a hunt it shows only while a strike lights it up, and then in full, past the game's flicker. */
void show_by_strikes(void* ghost) {
  if (!il2cpp::field<bool>(ghost, kGhostHunting)) {
    if (g.shown) set_model(ghost, false);  // the hunt ended in a strike: hidden, as after any hunt
    g.shown = false;
    return;
  }
  g.shown = tempest_striking(since_strike());
  set_model(ghost, g.shown);
}

// The game's storm strike, an RPC every client runs: in heavy rain it lights the Tempest up as its own do.
// Counted first, since the game's own handler throws where a client has no local Player (the ghost player's).
void detour_storm_lightning(void* controller, void* info, const void* method) {
  hooks::Guard guard;
  if (g.haunting && contract_weather() == kWeatherHeavyRain) strike_here(false);
  g_stormLightning(controller, info, method);
}

}  // namespace

bool resolve() {
  api.weatherClass = il2cpp::cls("Assembly-CSharp.dll", "", "RandomWeather");
  il2cpp::Class* lightning = il2cpp::cls("Assembly-CSharp.dll", "", "LightningController");
  api.lightningType = lightning ? il2cpp::type_object(lightning) : nullptr;
  bool ok = api.weatherClass && api.lightningType;
  ok &= il2cpp::bind(api.findObject, il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectOfType", 1);
  api.lightning = reinterpret_cast<decltype(api.lightning)>(game::game_assembly_address(kLocalLightningRva));
  api.ok = ok && api.lightning;
  PG_LOG("tempest: resolve %s", api.ok ? "ok" : "unavailable (no strikes)");
  return true;  // optional
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kStormLightningRva), reinterpret_cast<void*>(&detour_storm_lightning),
                       reinterpret_cast<void**>(&g_stormLightning), "LightningController.PlayLightningNetworked");
}

void tick(void* ghost, int type) {
  if (!api.ok) return;
  if (!ghost || type != kTempest || !game::alive(ghost)) {
    if (g.haunting) reset();
    return;
  }
  g.haunting = true;
  if (game::is_master()) host_tick(ghost, GetTickCount64());
  show_by_strikes(ghost);
}

// Called inside the game's state change: the strike waits for the tick, so no game call runs in it.
void event_begins() {
  if (api.ok && g.haunting) g.eventStrike = true;
}

bool available() { return api.ok; }

bool striking() { return g.haunting && tempest_striking(since_strike()); }

void receive(int sender, const uint8_t* data, size_t size) {
  if (sender != game::master_actor() || !net_msg::read_strike(data, size)) return;
  if (api.ok) strike_here(true);
}

void reset() { g = State{}; }

}  // namespace pg::custom::tempest
