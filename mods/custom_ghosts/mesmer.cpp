#include "mesmer.h"

#include <windows.h>

#include <cmath>
#include <vector>

#include "Camera.h"
#include "GhostAI.h"
#include "GhostInfo.h"
#include "LevelRoom.h"
#include "LightSwitch.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "Transform.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "apparition.h"
#include "game.h"
#include "game_api.h"
#include "ghosts.h"
#include "held.h"
#include "log.h"
#include "mesmer_rule.h"
#include "players.h"
#include "runtime.h"
#include "sound.h"

namespace pm::custom::mesmer {
namespace {

using phasmo::Vector3;

// The class dump, the scene's TagManager (layer 3 is the only unnamed layer) and decompiles of
// GhostAI.Appear and GhostModel.SetVisibility (build 25811160, 2026-10-20).

struct Dimmed {
  api::Held light;
  float original;
};

struct State {
  bool active = false;          // a Mesmer haunts this contract (the monitor lies)
  bool wasAppeared = false;
  uint32_t appearances = 0;
  apparition::Copy fake;
  api::Held camera;             // this player's camera while its mask is changed
  int savedMask = 0;
  std::vector<Dimmed> lights;
  ULONGLONG began = 0;
  bool warnedLayers = false;
  bool wasHunting = false;
  api::Held room;               // its favourite room, outside hunts (compared only)
  api::Held dimmedRoom;         // the room `lights` were gathered from (compared only)
} g;

void restore_camera() {
  if (api::alive(g.camera)) phasmo::Camera::set_cullingMask(g.camera, g.savedMask);
  g.camera = nullptr;
}

void restore_lights() {
  for (Dimmed& d : g.lights)
    if (api::alive(d.light)) phasmo::Light::set_intensity(d.light, d.original);
  g.lights.clear();
}

/**
 * Every form the AI may wear: the models in each of GhostAI's GhostModel arrays, in the order the class
 * lays the arrays out. Every GhostAI method that reads one array reads the other (InitGhostModel picks
 * between them by its bool), so no map entry can name either: GhostAI's fields of that type are read
 * from the game's metadata.
 */
std::vector<void*> forms_of(void* ghost) {
  std::vector<void*> forms;
  void* arrays[4] = {};
  const size_t n = phasmo::GhostAI::type.fields_of<phasmo::GhostModel::type>(ghost, true, arrays, 4);
  for (size_t a = 0; a < n && a < 4; ++a) {
    const api::RefArray models{arrays[a]};
    for (size_t i = 0; i < models.size(); ++i)
      if (void* m = models[i]; api::alive(m)) forms.push_back(m);
  }
  return forms;
}

/** No camera but a player's eye draws the fake's layer (photos, video cameras: the truth). */
void keep_cameras_true(void* playerCamera) {
  const api::RefArray cameras{phasmo::Camera::get_allCameras()};
  for (size_t i = 0; i < cameras.size(); ++i) {
    void* c = cameras[i];
    if (!c || c == playerCamera || !api::alive(c)) continue;
    const int mask = phasmo::Camera::get_cullingMask(c);
    if (mask & (1 << kMesmerFakeLayer)) phasmo::Camera::set_cullingMask(c, mask & ~(1 << kMesmerFakeLayer));
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
  void* real = phasmo::GhostAI::model.get(ghost);
  std::vector<void*> forms = forms_of(ghost);
  int realIndex = -1;
  for (size_t i = 0; i < forms.size(); ++i)
    if (forms[i] == real) realIndex = static_cast<int>(i);
  if (forms.empty() || !real) return;
  // The true form is taken out of this player's eye by its layers; a shared layer (Default, the
  // fake's own) would take everything on it out too, so then the appearance stays as it is.
  void* camera = game::local_camera();
  const api::RefArray renderers{phasmo::GhostModel::renderers.get(real)};
  uint32_t realLayers = 0;
  for (size_t i = 0; i < renderers.size(); ++i)
    if (void* r = renderers[i]; api::alive(r)) realLayers |= mesmer_layer_bit(game::layer(game::game_object_of(r)));
  if (!api::alive(camera)) return;  // no eye of this player's to show it to yet
  if (!mesmer_layers_safe(realLayers)) {
    if (!g.warnedLayers) PM_LOG("mesmer: its form shares a layer with the scene (0x%X); appearances are its own", realLayers);
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
    const phasmo::Vector3 guess{spot.x, spot.y, spot.z};
    phasmo::NavMeshHit hit{};
    if (phasmo::NavMesh::SamplePosition(&guess, &hit, 1.5f, -1) && hit.hit)
      spot = {hit.position.x, hit.position.y, hit.position.z};
    // It faces whoever sees it, from the camera they see through: a ghost player's body is parked elsewhere.
    const Vector3 eye = game::position(game::transform_of(camera));
    yaw = mesmer_facing(spot.x, spot.z, eye.x, eye.z);
  }
  apparition::place(g.fake, spot, yaw);
  // This player's eye: the fake's layer on, the true form's layers off, for as long as it shows.
  g.camera = camera;
  g.savedMask = phasmo::Camera::get_cullingMask(camera);
  phasmo::Camera::set_cullingMask(
      camera, static_cast<int>((static_cast<uint32_t>(g.savedMask) | (1u << kMesmerFakeLayer)) & ~realLayers));
  keep_cameras_true(camera);
  if (hunt) return;  // a hunt keeps the game's own sounds
  // And a sound of its own, from where this player sees it.
  const api::List sounds{phasmo::GhostModel::sounds.get(form)};
  if (sounds.size() > 0)
    if (void* clip = sounds[seed % static_cast<uint32_t>(sounds.size())]) sound::play_at(clip, spot, 1.0f);
}

void hide_fake() {
  apparition::destroy(g.fake);
  restore_camera();
}

void* favourite_room(void* ghost) {
  void* info = phasmo::GhostAI::info.get(ghost);
  void* room = info ? phasmo::GhostInfo::favouriteRoom.get(info) : nullptr;
  return api::alive(room) ? room : nullptr;
}

/** The lights of its favourite room, dimming and coming back on a slow wave. */
void dim_room(void* room, ULONGLONG now) {
  if (room != g.dimmedRoom) {  // its favourite room moved: the old room's lights back as they were
    restore_lights();
    g.dimmedRoom = room;
  }
  if (!room) return;
  if (g.lights.empty()) {
    const api::List switches{phasmo::LevelRoom::lightSwitches.get(room)};
    for (size_t i = 0; i < switches.size(); ++i) {
      void* sw = switches[i];
      const api::List sources{api::alive(sw) ? phasmo::LightSwitch::sources.get(sw) : nullptr};
      for (size_t k = 0; k < sources.size(); ++k) {
        void* source = sources[k];
        void* light = api::alive(source) ? phasmo::LightSource::lightSource.get(source) : nullptr;
        if (api::alive(light)) g.lights.push_back({api::Held(light), phasmo::Light::get_intensity(light)});
      }
    }
  }
  const float share = mesmer_dim((now - g.began) / 1000.0f);
  for (Dimmed& d : g.lights)
    if (api::alive(d.light)) phasmo::Light::set_intensity(d.light, d.original * share);
}

// The truck monitor reads every player's sanity: for that one call, each is a fifth saner.
void detour_update_ui(api::Original<phasmo::HeartRateData::UpdateUI> original, void* monitor) {
  if (!g.active) {
    original(monitor);
    return;
  }
  struct Saved {
    void* sanity;
    float insanity;
  };
  std::vector<Saved> saved;
  game::each_player([&](void* player) {
    void* sanity = phasmo::Player::sanity.get(player);
    if (!api::alive(sanity)) return;
    const float insanity = phasmo::PlayerSanity::insanity.get(sanity);
    saved.push_back({sanity, insanity});
    phasmo::PlayerSanity::insanity.set(sanity, mesmer_shown_insanity(insanity));
  });
  struct Restore {
    std::vector<Saved>& saved;
    ~Restore() {
      for (const Saved& s : saved) phasmo::PlayerSanity::insanity.set(s.sanity, s.insanity);
    }
  } restore{saved};
  original(monitor);
}

// Outside hunts its room's lights dim instead of flickering: the flicker a ghost's interaction sends is
// not shown there (each client receives it as an RPC; it starts a visual coroutine and nothing else).
void detour_flicker(api::Original<phasmo::LightSwitch::FlickerNetworked> original, void* lightSwitch, const void* info) {
  if (g.active && g.room && lightSwitch && phasmo::LightSwitch::room.get(lightSwitch) == g.room) return;
  original(lightSwitch, info);
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::HeartRateData::UpdateUI>(&detour_update_ui, "HeartRateData.UpdateUI",
                                                    api::Twins::CanonicalOnly) &&
         api::hook<phasmo::LightSwitch::FlickerNetworked>(&detour_flicker, "LightSwitch.FlickerNetworked",
                                                          api::Twins::CanonicalOnly);
}

void tick(void* ghost, int type) {
  const bool mesmer = ghost && type == kMesmer && api::alive(ghost);
  if (!mesmer) {
    if (g.active || g.fake.root || !g.lights.empty()) reset();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (!g.active) {
    g.active = true;
    g.began = now;
  }
  const bool hunting = phasmo::GhostAI::isHunting.get(ghost);
  const bool appeared = phasmo::GhostAI::isAppeared.get(ghost) && !hunting;
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

}  // namespace pm::custom::mesmer
