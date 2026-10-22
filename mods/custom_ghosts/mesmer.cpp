#include "mesmer.h"

#include <windows.h>

#include <cmath>
#include <vector>

#include "apparition.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "mesmer_rule.h"
#include "players.h"
#include "sound.h"

namespace pg::custom::mesmer {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. Class dump, the scene's
// TagManager (layer 3 is the only unnamed layer) and REA (GhostAI.Appear / GhostModel.SetVisibility),
// 2026-10-20.
constexpr size_t kGhostAppeared = 0xA8;      // GhostAI: showing itself, on every client
constexpr size_t kGhostHunting = 0xF3;       // GhostAI: hunting, on every client
constexpr size_t kGhostModel = 0x60;         // GhostAI: the GhostModel it wears
constexpr size_t kGhostForms[2] = {0x70, 0x78};  // GhostAI: GhostModel[] it may wear
constexpr size_t kGhostInfo = 0x38;          // GhostAI: GhostInfo
constexpr size_t kInfoFavouriteRoom = 0x70;  // GhostInfo: favourite LevelRoom (synced to every client)
constexpr size_t kModelRenderers = 0x30;     // GhostModel: Renderer[]
constexpr size_t kModelSounds = 0x70;        // GhostModel: List<AudioClip>, its sounds
constexpr size_t kRoomSwitches = 0x20;       // LevelRoom: List<LightSwitch>
constexpr size_t kSwitchSources = 0x40;      // LightSwitch: List<LightSource>
constexpr size_t kSourceLight = 0x20;        // LightSource: Light
constexpr size_t kPlayerSanity = 0x90;       // Player: PlayerSanity
constexpr size_t kInsanity = 0x30;           // PlayerSanity: insanity, 0..100
constexpr size_t kSwitchRoom = 0xD0;         // LightSwitch: its LevelRoom
constexpr uintptr_t kUpdateUiRva = 0x12184F0;  // HeartRateData.UpdateUI(): the truck monitor
constexpr uintptr_t kFlickerRva = 0x26DC1D0;   // LightSwitch.FlickerNetworked(info): RPC, a visual flicker only (REA)
constexpr uintptr_t kSampleRva = 0x518F100;    // NavMesh.SamplePosition(Vector3, out hit, float, int)

struct NavMeshHit {
  Vector3 position, normal;
  float distance;
  int mask;
  bool hit;
};

struct Api {
  int (*cullingMask)(void*, const void*) = nullptr;
  void (*setCullingMask)(void*, int, const void*) = nullptr;
  il2cpp::Array* (*allCameras)(const void*) = nullptr;
  float (*intensity)(void*, const void*) = nullptr;
  void (*setIntensity)(void*, float, const void*) = nullptr;
  bool (*sample)(Vector3, NavMeshHit*, float, int, const void*) = nullptr;
  bool ok = false;
} api;

using UpdateUi_t = void (*)(void*, const void*);
UpdateUi_t g_updateUi = nullptr;
using Flicker_t = void (*)(void*, void*, const void*);  // PhotonMessageInfo travels by reference
Flicker_t g_flicker = nullptr;

struct Dimmed {
  il2cpp::Held light;
  float original;
};

struct State {
  bool active = false;          // a Mesmer haunts this contract (the monitor lies)
  bool wasAppeared = false;
  uint32_t appearances = 0;
  apparition::Copy fake;
  il2cpp::Held camera;          // this player's camera while its mask is changed
  int savedMask = 0;
  std::vector<Dimmed> lights;
  ULONGLONG began = 0;
  bool warnedLayers = false;
  bool wasHunting = false;
  il2cpp::Held room;            // its favourite room, outside hunts (compared only)
  il2cpp::Held dimmedRoom;      // the room `lights` were gathered from (compared only)
} g;

void restore_camera() {
  if (g.camera.get() && game::alive(g.camera.get())) api.setCullingMask(g.camera.get(), g.savedMask, nullptr);
  g.camera = il2cpp::Held();
}

void restore_lights() {
  for (Dimmed& d : g.lights)
    if (game::alive(d.light.get())) api.setIntensity(d.light.get(), d.original, nullptr);
  g.lights.clear();
}

std::vector<void*> forms_of(void* ghost) {
  std::vector<void*> forms;
  for (size_t at : kGhostForms) {
    auto* array = il2cpp::field<il2cpp::Array*>(ghost, at);
    for (uintptr_t i = 0; array && i < array->max_length; ++i)
      if (void* m = array->data<void*>()[i]; m && game::alive(m)) forms.push_back(m);
  }
  return forms;
}

/** No camera but a player's eye draws the fake's layer (photos, video cameras: the truth). */
void keep_cameras_true(void* playerCamera) {
  il2cpp::Array* cameras = api.allCameras(nullptr);
  for (uintptr_t i = 0; cameras && i < cameras->max_length; ++i) {
    void* c = cameras->data<void*>()[i];
    if (!c || c == playerCamera || !game::alive(c)) continue;
    const int mask = api.cullingMask(c, nullptr);
    if (mask & (1 << kMesmerFakeLayer)) api.setCullingMask(c, mask & ~(1 << kMesmerFakeLayer), nullptr);
  }
}

/** This client's own view of an appearance that just began. */
float yaw_of(void* transform) {
  const Vector3 f = game::forward(transform);
  return std::atan2(f.x, f.z) * 180.0f / 3.14159265f;
}

/**
 * This client's own view of an appearance (or, `hunt`, a hunt) that just began: another form, a few
 * metres off with a sound of its own; in a hunt where the ghost truly is, following it.
 */
void hide_fake();

void show_fake(void* ghost, bool hunt) {
  if (g.fake.root || g.camera.get()) hide_fake();  // one fake at a time, and the mask saved unchanged
  void* real = il2cpp::field<void*>(ghost, kGhostModel);
  std::vector<void*> forms = forms_of(ghost);
  int realIndex = -1;
  for (size_t i = 0; i < forms.size(); ++i)
    if (forms[i] == real) realIndex = static_cast<int>(i);
  if (forms.empty() || !real) return;
  // The true form is taken out of this player's eye by its layers; a shared layer (Default, the
  // fake's own) would take everything on it out too, so then the appearance stays as it is.
  void* camera = game::local_camera();
  auto* renderers = il2cpp::field<il2cpp::Array*>(real, kModelRenderers);
  uint32_t realLayers = 0;
  for (uintptr_t i = 0; renderers && i < renderers->max_length; ++i)
    if (void* r = renderers->data<void*>()[i]; r && game::alive(r)) realLayers |= mesmer_layer_bit(game::layer(game::game_object_of(r)));
  if (!camera || !game::alive(camera)) return;  // no eye of this player's to show it to yet
  if (!mesmer_layers_safe(realLayers)) {
    if (!g.warnedLayers) PG_LOG("mesmer: its form shares a layer with the scene (0x%X); appearances are its own", realLayers);
    g.warnedLayers = true;
    return;
  }
  const uint32_t seed = mesmer_seed(++g.appearances, static_cast<uint32_t>(game::local_actor()));
  void* form = forms[mesmer_form(seed, static_cast<int>(forms.size()), realIndex)];
  g.fake = apparition::make(form);
  if (!g.fake.root) return;
  apparition::set_layer(g.fake, kMesmerFakeLayer);
  const Vector3 at = game::position(game::transform_of(ghost));
  Vector3 spot = at;
  float yaw = yaw_of(game::transform_of(ghost));
  if (!hunt) {
    float dx = 0, dz = 0;
    mesmer_offset(seed, dx, dz);
    spot = {at.x + dx, at.y, at.z + dz};
    NavMeshHit hit{};
    if (api.sample(spot, &hit, 1.5f, -1, nullptr) && hit.hit) spot = hit.position;
    if (void* me = game::local_player(); me && game::alive(me)) {
      const Vector3 p = game::position(game::transform_of(me));
      yaw = std::atan2(p.x - spot.x, p.z - spot.z) * 180.0f / 3.14159265f;  // it faces whoever sees it
    }
  }
  apparition::place(g.fake, spot, yaw);
  // This player's eye: the fake's layer on, the true form's layers off, for as long as it shows.
  g.camera = il2cpp::Held(camera);
  g.savedMask = api.cullingMask(camera, nullptr);
  api.setCullingMask(camera, static_cast<int>((static_cast<uint32_t>(g.savedMask) | (1u << kMesmerFakeLayer)) & ~realLayers), nullptr);
  keep_cameras_true(camera);
  if (hunt) return;  // a hunt keeps the game's own sounds
  // And a sound of its own, from where this player sees it.
  auto* sounds = il2cpp::field<il2cpp::List*>(form, kModelSounds);
  if (sounds && sounds->items && sounds->size > 0)
    if (void* clip = sounds->items->data<void*>()[seed % static_cast<uint32_t>(sounds->size)])
      sound::play_at(clip, spot, 1.0f);
}

void hide_fake() {
  apparition::destroy(g.fake);
  restore_camera();
}

void* favourite_room(void* ghost) {
  void* info = il2cpp::field<void*>(ghost, kGhostInfo);
  void* room = info ? il2cpp::field<void*>(info, kInfoFavouriteRoom) : nullptr;
  return room && game::alive(room) ? room : nullptr;
}

/** The lights of its favourite room, dimming and coming back on a slow wave. */
void dim_room(void* room, ULONGLONG now) {
  if (room != g.dimmedRoom) {  // its favourite room moved: the old room's lights back as they were
    restore_lights();
    g.dimmedRoom = room;
  }
  if (!room) return;
  if (g.lights.empty()) {
    auto* switches = il2cpp::field<il2cpp::List*>(room, kRoomSwitches);
    for (int i = 0; switches && switches->items && i < switches->size; ++i) {
      void* sw = switches->items->data<void*>()[i];
      auto* sources = sw && game::alive(sw) ? il2cpp::field<il2cpp::List*>(sw, kSwitchSources) : nullptr;
      for (int k = 0; sources && sources->items && k < sources->size; ++k) {
        void* source = sources->items->data<void*>()[k];
        void* light = source && game::alive(source) ? il2cpp::field<void*>(source, kSourceLight) : nullptr;
        if (light && game::alive(light)) g.lights.push_back({il2cpp::Held(light), api.intensity(light, nullptr)});
      }
    }
  }
  const float share = mesmer_dim((now - g.began) / 1000.0f);
  for (Dimmed& d : g.lights)
    if (game::alive(d.light.get())) api.setIntensity(d.light.get(), d.original * share, nullptr);
}

// The truck monitor reads every player's sanity: for that one call, each is a fifth saner.
void detour_update_ui(void* monitor, const void* method) {
  hooks::Guard guard;
  if (!g.active) {
    g_updateUi(monitor, method);
    return;
  }
  struct Saved {
    void* sanity;
    float insanity;
  };
  std::vector<Saved> saved;
  game::each_player([&](void* player) {
    void* sanity = il2cpp::field<void*>(player, kPlayerSanity);
    if (!sanity || !game::alive(sanity)) return;
    const float insanity = il2cpp::field<float>(sanity, kInsanity);
    saved.push_back({sanity, insanity});
    il2cpp::set_field<float>(sanity, kInsanity, mesmer_shown_insanity(insanity));
  });
  struct Restore {
    std::vector<Saved>& saved;
    ~Restore() {
      for (const Saved& s : saved) il2cpp::set_field<float>(s.sanity, kInsanity, s.insanity);
    }
  } restore{saved};
  g_updateUi(monitor, method);
}

// Outside hunts its room's lights dim instead of flickering: the flicker a ghost's interaction sends is
// not shown there (each client receives it as an RPC; it starts a visual coroutine and nothing else).
void detour_flicker(void* lightSwitch, void* info, const void* method) {
  hooks::Guard guard;
  if (g.active && g.room && lightSwitch && il2cpp::field<void*>(lightSwitch, kSwitchRoom) == g.room) return;
  g_flicker(lightSwitch, info, method);
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* camera = il2cpp::cls(kCore, "UnityEngine", "Camera");
  il2cpp::Class* light = il2cpp::cls(kCore, "UnityEngine", "Light");
  bool ok = il2cpp::bind(api.cullingMask, camera, "get_cullingMask", 0);
  ok &= il2cpp::bind(api.setCullingMask, camera, "set_cullingMask", 1);
  ok &= il2cpp::bind(api.allCameras, camera, "get_allCameras", 0);
  ok &= il2cpp::bind(api.intensity, light, "get_intensity", 0);
  ok &= il2cpp::bind(api.setIntensity, light, "set_intensity", 1);
  ok &= sound::available();
  api.sample = reinterpret_cast<decltype(api.sample)>(game::game_assembly_address(kSampleRva));
  api.ok = ok && api.sample;
  PG_LOG("mesmer: resolve %s", api.ok ? "ok" : "unavailable (it shows itself as any ghost)");
  return true;  // optional
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kUpdateUiRva), reinterpret_cast<void*>(&detour_update_ui),
                       reinterpret_cast<void**>(&g_updateUi), "HeartRateData.UpdateUI") &&
         hooks::create(game::game_assembly_address(kFlickerRva), reinterpret_cast<void*>(&detour_flicker),
                       reinterpret_cast<void**>(&g_flicker), "LightSwitch.FlickerNetworked");
}

void tick(void* ghost, int type) {
  const bool mesmer = api.ok && ghost && type == kMesmer && game::alive(ghost);
  if (!mesmer) {
    if (g.active || g.fake.root || !g.lights.empty()) reset();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (!g.active) {
    g.active = true;
    g.began = now;
  }
  const bool hunting = il2cpp::field<bool>(ghost, kGhostHunting);
  const bool appeared = il2cpp::field<bool>(ghost, kGhostAppeared) && !hunting;
  // What ended is hidden before what began is shown (a hunt can end on the frame an appearance shows).
  if ((!appeared && g.wasAppeared) || (!hunting && g.wasHunting)) hide_fake();
  if (appeared && !g.wasAppeared) show_fake(ghost, false);
  if (hunting && !g.wasHunting) show_fake(ghost, true);  // a new form for each hunt
  g.wasAppeared = appeared;
  g.wasHunting = hunting;
  if (g.fake.root && hunting) {
    void* t = game::transform_of(ghost);
    apparition::place(g.fake, game::position(t), yaw_of(t));
    apparition::follow(g.fake, apparition::model_of(ghost));  // flickers and walks as the true one
  } else if (g.fake.root) {
    apparition::show(g.fake, true);
  }
  // Hunts flicker the lights as any ghost's do (a warning players rely on); otherwise its room dims.
  g.room = hunting ? nullptr : favourite_room(ghost);
  if (hunting) restore_lights();
  else dim_room(g.room, now);
}

void reset() {
  hide_fake();
  restore_lights();
  g = State{};
}

}  // namespace pg::custom::mesmer
