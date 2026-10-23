#include "haunt.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "bindings.h"
#include "game.h"
#include "haunt_rules.h"
#include "il2cpp.h"
#include "log.h"
#include "ui.h"

namespace pg::afterlife {
namespace {

using il2cpp::Vector3;

constexpr size_t kPlayerDead = 0x28;          // Player: dead
constexpr ULONGLONG kActionsMs = 250;         // how often the switched-off actions are checked
constexpr const char* kLocked[] = {"Interact", "Primary Use", "Secondary Use"};

struct Api {
  il2cpp::Object* cameraType = nullptr;
  void (*setFov)(void*, float, const void*) = nullptr;
  int (*cullingMask)(void*, const void*) = nullptr;
  void (*setCullingMask)(void*, int, const void*) = nullptr;
  float (*nearClip)(void*, const void*) = nullptr;
  void (*setNearClip)(void*, float, const void*) = nullptr;
  float (*farClip)(void*, const void*) = nullptr;
  void (*setFarClip)(void*, float, const void*) = nullptr;
} api;

Rules g_rules;
il2cpp::Held g_camera;     // the mod's camera while haunting
il2cpp::Held g_ownCamera;  // the investigator's, switched off while haunting
il2cpp::Held g_frozen;     // the Player frozen in place
std::vector<il2cpp::Held> g_offActions;
ULONGLONG g_nextActions = 0;
bindings::Key g_key;
il2cpp::Held g_keyOwner;
il2cpp::Held g_canvas;
il2cpp::Held g_hint;
std::string g_hintShown;

bool dead(void* player) { return player && game::alive(player) && il2cpp::field<bool>(player, kPlayerDead); }

void set_hint(const std::string& text) {
  if (text.empty() && !g_canvas) return;
  if (!g_canvas) {
    g_canvas = ui::create_canvas("AfterlifeHint", 40);
    g_hint = ui::create_text(g_canvas, "Hint", 24);
    ui::place(g_hint, {0.5f, 0}, {0.5f, 0.5f}, {0, 40}, {1400, 36});
    ui::set_color(g_hint, {1, 1, 1, 0.75f});
  }
  if (text == g_hintShown) return;
  g_hintShown = text;
  ui::set_text(g_hint, text.c_str());
}

void lock_actions(bool locked) {
  if (!locked) {
    for (void* a : g_offActions) game::set_action_enabled(a, true);
    g_offActions.clear();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextActions) return;
  g_nextActions = now + kActionsMs;  // the game may switch them back on (menus): switched off again
  void* lp = game::local_player_component();
  if (!lp) return;
  game::for_each_input_action(lp, [](void* action, const char*, const char* name) {
    for (const char* n : kLocked) {
      if (std::strcmp(name, n) != 0 || !game::action_enabled(action)) continue;
      game::set_action_enabled(action, false);
      if (std::find(g_offActions.begin(), g_offActions.end(), action) == g_offActions.end()) g_offActions.push_back(action);
    }
  });
}

void start_haunting(void* player) {
  g_ownCamera = game::local_camera();
  void* go = game::new_game_object("Afterlife camera");
  g_camera = go ? game::add_component(go, api.cameraType) : nullptr;
  if (!g_camera) return;
  if (g_ownCamera && game::alive(g_ownCamera)) {
    api.setFov(g_camera, game::field_of_view(g_ownCamera), nullptr);
    api.setCullingMask(g_camera, api.cullingMask(g_ownCamera, nullptr), nullptr);
    api.setNearClip(g_camera, api.nearClip(g_ownCamera, nullptr), nullptr);
    api.setFarClip(g_camera, api.farClip(g_ownCamera, nullptr), nullptr);
    game::set_enabled(g_ownCamera, false);
  }
  game::freeze_player_movement(player, true);
  g_frozen = player;
  PG_LOG("afterlife: haunting with the ghost");
}

void stop_haunting() {
  if (g_camera && game::alive(g_camera)) game::destroy(game::game_object_of(g_camera));
  g_camera = nullptr;
  if (g_ownCamera && game::alive(g_ownCamera)) game::set_enabled(g_ownCamera, true);
  g_ownCamera = nullptr;
  if (g_frozen && game::alive(g_frozen)) game::freeze_player_movement(g_frozen, false);
  g_frozen = nullptr;
  PG_LOG("afterlife: back with the investigator's body");
}

void follow(void* ghost) {
  void* gt = game::transform_of(ghost);
  void* ct = game::transform_of(g_camera);
  const Vector3 p = game::position(gt);
  game::set_position(ct, {p.x, p.y + game::kGhostEyeHeight, p.z});
  game::set_rotation(ct, game::rotation(gt));
}

}  // namespace

bool resolve() {
  il2cpp::Class* camera = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Camera");
  bool ok = camera != nullptr;
  ok &= il2cpp::bind(api.setFov, camera, "set_fieldOfView", 1);
  ok &= il2cpp::bind(api.cullingMask, camera, "get_cullingMask", 0);
  ok &= il2cpp::bind(api.setCullingMask, camera, "set_cullingMask", 1);
  ok &= il2cpp::bind(api.nearClip, camera, "get_nearClipPlane", 0);
  ok &= il2cpp::bind(api.setNearClip, camera, "set_nearClipPlane", 1);
  ok &= il2cpp::bind(api.farClip, camera, "get_farClipPlane", 0);
  ok &= il2cpp::bind(api.setFarClip, camera, "set_farClipPlane", 1);
  api.cameraType = camera ? il2cpp::type_object(camera) : nullptr;
  ok &= api.cameraType != nullptr;
  PG_LOG("afterlife: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

void tick() {
  const ULONGLONG now = GetTickCount64();
  void* level = game::level_controller();
  void* player = level ? game::local_player() : nullptr;
  void* ghost = level ? game::ghost_ai() : nullptr;
  const bool ghostPresent = ghost && game::alive(ghost);
  const bool isDead = dead(player);
  void* lp = player ? game::local_player_component() : nullptr;
  if (lp != g_keyOwner) {
    g_keyOwner = lp;
    g_key = lp ? bindings::for_action(lp, "Crouch") : bindings::Key{};
  }
  const bool keyDown = isDead && game::cursor_locked() && bindings::down(g_key);
  if (g_rules.step(isDead, ghostPresent, keyDown, now)) g_rules.haunting() ? start_haunting(player) : stop_haunting();
  if (!level) g_rules.reset();
  if (g_rules.haunting() && g_camera && game::alive(g_camera)) follow(ghost);
  lock_actions(level && g_rules.touch_locked(now));

  std::string hint;
  if (g_rules.haunting()) hint = "You see what the ghost sees. Hold [" + g_key.label + "] to return to your body";
  else if (const unsigned left = g_rules.lock_seconds(now); left && level)
    hint = "Back from the ghost: you can touch nothing for " + std::to_string(left) + " s";
  else if (isDead && ghostPresent && !g_key.label.empty()) hint = "Hold [" + g_key.label + "] to watch through the ghost's eyes";
  set_hint(hint);
}

void teardown() {
  if (g_rules.haunting()) stop_haunting();
  g_rules.reset();
  lock_actions(false);
  if (g_canvas) ui::destroy(g_canvas);
  g_canvas = nullptr;
  g_hint = nullptr;
  g_hintShown.clear();
}

}  // namespace pg::afterlife
