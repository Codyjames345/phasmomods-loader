#include "tempest.h"

#include <windows.h>

#include <random>
#include <vector>

#include "Difficulty.h"
#include "GhostAI.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "Weather.h"
#include "apparition.h"
#include "flame_net.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "managed_exception.h"  // Il2CppExceptionWrapper: the C++ type a managed exception is thrown as
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "runtime.h"
#include "tempest_rule.h"

namespace pm::custom::tempest {
namespace {

// The lightning (sdk/map/Weather.h, decompiles of build 25811160, 2026-10-22): PlayLocalLightning is the
// lobby hoop's strike, from its controller's own effects, which no contract scene sets; the game's storm
// strike (PlayLightningNetworked) strikes from the weather's (RandomWeather.instance's) effects.

struct State {
  bool haunting = false;   // a Tempest haunts this contract
  ULONGLONG lastStrike = 0;
  bool shown = false;      // a strike showed it in a hunt last frame
  api::Held lightning;     // the scene's LightningController
  bool lightningFailed = false;  // logged once: no lightning to play
  bool lightningThrew = false;   // the game's lightning threw: none is played again this contract
  // host
  StrikeClock clock;
  bool eventStrike = false;  // one of its events began: the next tick strikes
  int strikes = 0;           // in the current hunt
  SkyWatch sky;              // the contract's weather, which a Monkey Paw wish can change
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

int contract_weather() {
  void* values = phasmo::LevelValues::get_Instance();
  void* weather = values ? phasmo::LevelValues::weather.get(values) : nullptr;
  return weather ? phasmo::WeatherProfile::weatherType.get(weather) : -1;
}

/**
 * The contract's sky as it is now, read again each time: a Monkey Paw weather wish changes it mid-contract
 * (sdk/systems/weather.md). Logged when it is first read and whenever it changes.
 */
Sky sky() {
  const int type = contract_weather();
  if (g.sky.see(type, phasmo::WeatherType::rain(), phasmo::WeatherType::heavyRain(), phasmo::WeatherType::dead()))
    PM_LOG("tempest: weather %d, so it strikes %s", type,
           g.sky.sky == Sky::HeavyRain ? "often" : g.sky.sky == Sky::Rain ? "now and then" : "rarely");
  return g.sky.sky;
}

float since_strike() { return g.lastStrike ? (GetTickCount64() - g.lastStrike) / 1000.0f : -1.0f; }

void* lightning_controller() {
  if (!api::alive(g.lightning)) {
    void* found = phasmo::Object::FindObjectOfType(phasmo::LightningController::type.type_object(), false);
    g.lightning = api::alive(found) ? found : nullptr;
  }
  return g.lightning.get();
}

void cannot_strike(const char* why) {
  if (!g.lightningFailed) PM_LOG("tempest: no lightning to play (%s)", why);
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
  if (!phasmo::LightningController::effects.get(controller)) {
    void* weather = phasmo::RandomWeather::instance.get();
    void* effects = api::alive(weather) ? phasmo::RandomWeather::effects.get(weather) : nullptr;
    if (!api::alive(effects)) {
      cannot_strike("no weather effects");
      return;
    }
    phasmo::LightningController::effects.set(controller, effects);
  }
  void* effects = phasmo::LightningController::effects.get(controller);
  const api::List thunder{phasmo::AzureEffectsController::thunderSettingsList.get(effects)};
  if (thunder.size() < 1) {  // PlayLocalLightning would throw on an empty list
    cannot_strike("no thunder settings");
    return;
  }
  // Its other throws (a null entry or prefab, a prefab without the thunder component) are the scene's;
  // the ghost still shows and speeds by its strikes without the lightning.
  try {
    phasmo::LightningController::PlayLocalLightning(controller);
  } catch (const Il2CppExceptionWrapper& e) {
    mod::host().log_exception("tempest: the game's lightning threw, so none is played again this contract", e.ex);
    g.lightningThrew = true;
  } catch (...) {
    PM_LOG("tempest: the game's lightning threw, so none is played again this contract");
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
  const bool hunting = game::ghost_state(ghost) == phasmo::GhostState::hunting();
  const bool opens = hunting && !g.clock.hunting;
  if (!hunting && g.clock.hunting) {
    PM_LOG("tempest: its hunt is over after %d strikes", g.strikes);
    g.strikes = 0;
  }
  const float gap = tempest_next_strike(sky(), std::uniform_real_distribution<float>(0, 1)(g_random));
  bool strike = tempest_strike_now(g.clock, hunting, now, static_cast<uint64_t>(gap * 1000));
  if (strike) {
    if (opens) PM_LOG("tempest: its hunt opens with a strike, the next in %.1f s", gap);
    ++g.strikes;
  }
  strike |= g.eventStrike;
  g.eventStrike = false;
  if (strike) strike_everywhere();
}

void set_model(void* ghost, bool visible) {
  void* model = apparition::model_of(ghost);
  const api::RefArray renderers{model ? phasmo::GhostModel::renderers.get(model) : nullptr};
  for (size_t i = 0; i < renderers.size(); ++i)
    if (void* r = renderers[i]; api::alive(r)) phasmo::Renderer::set_enabled(r, visible);
}

/** Every client: in a hunt it shows only while a strike lights it up, and then in full, past the game's flicker. */
void show_by_strikes(void* ghost) {
  if (!phasmo::GhostAI::isHunting.get(ghost)) {
    if (g.shown) set_model(ghost, false);  // the hunt ended in a strike: hidden, as after any hunt
    g.shown = false;
    return;
  }
  g.shown = tempest_striking(since_strike());
  set_model(ghost, g.shown);
}

// The game's storm strike, an RPC every client runs: in heavy rain it lights the Tempest up as its own do.
// Counted first, since the game's own handler throws where a client has no local Player.
void detour_storm_lightning(api::Original<phasmo::LightningController::PlayLightningNetworked> original,
                            void* controller, const void* info) {
  if (g.haunting && contract_weather() == phasmo::WeatherType::heavyRain()) strike_here(false);
  original(controller, info);
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::LightningController::PlayLightningNetworked>(
      &detour_storm_lightning, "LightningController.PlayLightningNetworked", api::Twins::CanonicalOnly);
}

void tick(void* ghost, int type) {
  if (!ghost || type != kTempest || !api::alive(ghost)) {
    if (g.haunting) reset();
    return;
  }
  g.haunting = true;
  if (game::is_master()) host_tick(ghost, GetTickCount64());
  show_by_strikes(ghost);
}

// Called inside the game's state change: the strike waits for the tick, so no game call runs in it.
void event_begins() {
  if (g.haunting) g.eventStrike = true;
}

bool available() { return true; }

bool striking() { return g.haunting && tempest_striking(since_strike()); }

void receive(int sender, const uint8_t* data, size_t size) {
  if (sender != game::master_actor() || !net_msg::read_strike(data, size)) return;
  strike_here(true);
}

void reset() { g = State{}; }

}  // namespace pm::custom::tempest
