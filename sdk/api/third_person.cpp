#include "third_person.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "Camera.h"
#include "Player.h"
#include "Transform.h"
#include "Scene.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::view {

namespace {

enum class Phase { Off, Out, On, Back };

struct State {
  Phase phase = Phase::Off;
  float t = 0.0f;  // seconds into the ease (out or back)
  Held playerCamera;
  Held camera;  // the view's own Camera; its GameObject goes with it
  Held shownOn;  // the camera show_own_body added AlivePlayer to
  int32_t shownMask = 0;
  std::vector<std::pair<Held, int32_t>> head;  // the head's renderers the view draws, each with the mode it had
  using Moved = std::vector<std::pair<Held, int32_t>>;  // GameObjects moved to AlivePlayer, each with its layer
  Moved viewMoved, shownMoved;
};
State g;

void* local_camera() {
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  void* cam = rig && alive(rig) ? phasmo::LocalPlayer::camera.get(rig) : nullptr;
  return cam && alive(cam) ? cam : nullptr;
}

void* local_player() {
  void* net = phasmo::Network::get_Instance();
  void* player = net ? phasmo::Network::localPlayer.get(net) : nullptr;
  return player && alive(player) ? player : nullptr;
}

int32_t layer_bit(void* component) {
  void* go = component && alive(component) ? phasmo::Component::get_gameObject(component) : nullptr;
  const int32_t layer = go ? phasmo::GameObject::get_layer(go) : -1;
  return layer >= 0 && layer < 32 ? 1 << layer : 0;
}

// The head's renderers (PlayerCharacter.headRenderersToTurnOff), which the game draws as shadows only on the player's
// own client.
void* head_renderers() {
  void* player = local_player();
  void* type = phasmo::PlayerCharacter::type.type_object();
  void* character = player && type ? phasmo::Component::GetComponentInChildren(player, type, true) : nullptr;
  return character ? phasmo::PlayerCharacter::headRenderersToTurnOff.get(character) : nullptr;
}

// The layer other players' bodies are drawn on (the PCPlayer prefab's), as a mask; 0 when it did not resolve.
int32_t alive_bit() {
  const int32_t layer = phasmo::Layers::AlivePlayer.index();
  return layer >= 0 && layer < 32 ? 1 << layer : 0;
}

// Puts each renderer of `renderers` (a Renderer[]) on AlivePlayer, keeping its layer in `moved`. On the player's own
// client the game puts the body on PlayerView, which its renderer draws in its first-person passes over everything, with
// their own depth and field of view; on AlivePlayer it draws as another player's does.
void move_to_alive(void* renderers, State::Moved& moved) {
  const int32_t layer = phasmo::Layers::AlivePlayer.index();
  if (layer < 0) return;
  const RefArray list{renderers};
  for (size_t i = 0; i < list.size(); ++i) {
    void* go = list[i] && alive(list[i]) ? phasmo::Component::get_gameObject(list[i]) : nullptr;
    if (!go) continue;
    if (std::any_of(moved.begin(), moved.end(), [&](const auto& m) { return m.first.get() == go; })) continue;
    moved.emplace_back(go, phasmo::GameObject::get_layer(go));
    phasmo::GameObject::set_layer(go, layer);
  }
}

// Moves each GameObject under `transform` (itself included) on PlayerView, the first-person layer, to AlivePlayer: what
// the game hangs on the body there (the watch's face and its canvas) would draw over the body from outside.
void move_view_layer(void* transform, State::Moved& moved, int depth = 0) {
  const int32_t from = phasmo::Layers::PlayerView.index(), to = phasmo::Layers::AlivePlayer.index();
  if (!transform || from < 0 || to < 0 || depth > 32) return;
  if (void* go = phasmo::Component::get_gameObject(transform); go && phasmo::GameObject::get_layer(go) == from &&
                                                               std::none_of(moved.begin(), moved.end(), [&](const auto& m) {
                                                                 return m.first.get() == go;
                                                               })) {
    moved.emplace_back(go, from);
    phasmo::GameObject::set_layer(go, to);
  }
  const int32_t count = phasmo::Transform::get_childCount(transform);
  for (int32_t i = 0; i < count; ++i) move_view_layer(phasmo::Transform::GetChild(transform, i), moved, depth + 1);
}

// Gives each moved GameObject its layer back, where it still has the one it was given: one the game moved since (an
// item dropped mid-dance) keeps the game's layer.
void restore_layers(State::Moved& moved) {
  const int32_t given = phasmo::Layers::AlivePlayer.index();
  for (auto& [go, layer] : moved)
    if (go && alive(go) && phasmo::GameObject::get_layer(go) == given) phasmo::GameObject::set_layer(go, layer);
  moved.clear();
}

// Draws the head while the view is outside, keeping each renderer's mode to give back.
void show_head() {
  if (!g.head.empty()) return;
  const RefArray renderers{head_renderers()};
  const int32_t on = static_cast<int32_t>(phasmo::ShadowCastingMode::On());
  for (size_t i = 0; i < renderers.size(); ++i) {
    void* r = renderers[i];
    if (!r || !alive(r)) continue;
    g.head.emplace_back(r, phasmo::Renderer::get_shadowCastingMode(r));
    phasmo::Renderer::set_shadowCastingMode(r, on);
  }
}

// Draws the head again where the game hid it while the view is outside, keeping the game's latest mode to give back.
// The game sets the local head's renderers to shadows only whenever a photo reaches this client
// (HandCamera.AddPhotoToJournal, a PunRPC coroutine, shows Network.localPlayer's head for the photo's render, then hides
// it) and on CameraFocusUI.Focus and UnFocus, so the head would vanish mid-view (the user, 2026-11-10).
void keep_head() {
  const int32_t on = static_cast<int32_t>(phasmo::ShadowCastingMode::On());
  for (auto& [r, mode] : g.head) {
    if (!r || !alive(r)) continue;
    const int32_t now = phasmo::Renderer::get_shadowCastingMode(r);
    if (now == on) continue;
    mode = now;
    phasmo::Renderer::set_shadowCastingMode(r, on);
  }
}

void restore_head() {
  for (auto& [r, mode] : g.head)
    if (r && alive(r)) phasmo::Renderer::set_shadowCastingMode(r, mode);
  g.head.clear();
}

// The layers of the local player's own colliders and rig: the wall check passes through them. The head's colliders
// follow the camera a frame late, so walking forward leaves them just behind it, where the check looks.
int32_t own_layers() {
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  return layer_bit(local_player()) | layer_bit(rig);
}

// The layers of the local player's body's renderers, as a mask.
int32_t body_layers() {
  void* player = local_player();
  if (!player) return 0;
  const RefArray renderers{phasmo::Player::renderers.get(player)};
  int32_t mask = 0;
  for (size_t i = 0; i < renderers.size(); ++i) {
    void* go = renderers[i] && alive(renderers[i]) ? phasmo::Component::get_gameObject(renderers[i]) : nullptr;
    const int32_t layer = go ? phasmo::GameObject::get_layer(go) : -1;
    if (layer >= 0 && layer < 32) mask |= 1 << layer;
  }
  return mask;
}

Point to_point(phasmo::Vector3 v) { return {v.x, v.y, v.z}; }

void finish() {
  if (g.camera) {
    void* go = alive(g.camera) ? phasmo::Component::get_gameObject(g.camera) : nullptr;
    if (go) phasmo::Object::Destroy(go, 0.0f);
  }
  if (g.playerCamera && alive(g.playerCamera)) phasmo::Behaviour::set_enabled(g.playerCamera, true);
  restore_head();
  restore_layers(g.viewMoved);
  g.camera = nullptr;
  g.playerCamera = nullptr;
  g.phase = Phase::Off;
  g.t = 0.0f;
}

}  // namespace

bool begin_third_person() {
  if (g.phase == Phase::Back) {  // easing back in: out again from where it is
    g.t = kEaseSeconds - g.t;
    g.phase = Phase::Out;
    return true;
  }
  if (g.phase != Phase::Off) return false;
  void* playerCam = local_camera();
  if (!playerCam) return false;
  if (!phasmo::Camera::CopyFrom.available() || !phasmo::GameObject::AddComponent.available()) {
    PM_LOG("view: the camera entries did not resolve in this game build: no third-person view");
    return false;
  }
  void* go = phasmo::GameObject::type.make();
  if (!go) return false;
  phasmo::GameObject::ctor(go, make_string("pm third-person view"));
  void* cam = phasmo::GameObject::AddComponent(go, phasmo::Camera::type.type_object());
  if (!cam) {
    phasmo::Object::Destroy(go, 0.0f);
    return false;
  }
  phasmo::Camera::CopyFrom(cam, playerCam);
  phasmo::Camera::set_cullingMask(cam, phasmo::Camera::get_cullingMask(cam) | alive_bit());
  void* player = local_player();
  move_to_alive(player ? phasmo::Player::renderers.get(player) : nullptr, g.viewMoved);
  move_to_alive(head_renderers(), g.viewMoved);
  void* animator = player ? phasmo::Player::animator.get(player) : nullptr;
  move_view_layer(animator && alive(animator) ? phasmo::Component::get_transform(animator) : nullptr, g.viewMoved);
  show_head();
  void* dataType = phasmo::UniversalAdditionalCameraData::type.type_object();
  void* playerData = dataType ? phasmo::Component::GetComponent(playerCam, dataType) : nullptr;
  if (playerData) {
    void* data = phasmo::GameObject::AddComponent(go, dataType);
    if (data) {
      phasmo::UniversalAdditionalCameraData::set_renderPostProcessing(
          data, phasmo::UniversalAdditionalCameraData::get_renderPostProcessing(playerData));
      phasmo::UniversalAdditionalCameraData::set_volumeLayerMask(
          data, phasmo::UniversalAdditionalCameraData::get_volumeLayerMask(playerData));
    }
  }
  phasmo::Behaviour::set_enabled(playerCam, false);
  g.playerCamera = playerCam;
  g.camera = cam;
  g.phase = Phase::Out;
  g.t = 0.0f;
  tick_third_person(0.0f);
  return true;
}

void end_third_person() {
  if (g.phase == Phase::Off || g.phase == Phase::Back) return;
  // Back from wherever the ease out had reached.
  g.t = g.phase == Phase::Out ? kEaseSeconds - g.t : 0.0f;
  g.phase = Phase::Back;
}

void tick_third_person(float dt) {
  if (g.phase == Phase::Off) return;
  if (!g.playerCamera || !alive(g.playerCamera) || !g.camera || !alive(g.camera)) {
    finish();
    return;
  }
  keep_head();
  g.t += dt;
  float amount = 1.0f;  // how far out, 0 at the head and 1 at the full distance
  if (g.phase == Phase::Out) {
    amount = ease(g.t / kEaseSeconds);
    if (g.t >= kEaseSeconds) g.phase = Phase::On;
  } else if (g.phase == Phase::Back) {
    amount = 1.0f - ease(g.t / kEaseSeconds);
    if (g.t >= kEaseSeconds) {
      finish();
      return;
    }
  }
  void* from = phasmo::Component::get_transform(g.playerCamera);
  void* to = phasmo::Component::get_transform(g.camera);
  if (!from || !to) return;
  const Point head = to_point(phasmo::Transform::get_position(from));
  const Point forward = to_point(phasmo::Transform::get_forward(from));
  float distance = kOrbitDistance * amount;
  // A ray from the head toward the view: a wall within reach pulls it in.
  const phasmo::Ray ray{{head.x, head.y, head.z}, {-forward.x, -forward.y, -forward.z}};
  pm::api::ValueArray hits{phasmo::RaycastHit::type.make_array(1)};
  void* hit = hits.size() ? hits.at(0) : nullptr;
  const int32_t notPlayer = ~(body_layers() | own_layers());
  if (hit && phasmo::Physics::RaycastHit(&ray, hit, distance + kWallMargin, notPlayer,
                                         static_cast<int32_t>(phasmo::QueryTriggerInteraction::Ignore())))
    distance = pulled_in(distance, phasmo::RaycastHit::get_distance(hit));
  const Point at = orbit_point(head, forward, distance, kOrbitHeight * amount);
  const phasmo::Vector3 place{at.x, at.y, at.z};
  const phasmo::Quaternion turn = phasmo::Transform::get_rotation(from);
  phasmo::Transform::set_position(to, &place);
  phasmo::Transform::set_rotation(to, &turn);
}

bool in_third_person() { return g.phase != Phase::Off; }

void draw_as_seen(void* object) {
  if (g.phase == Phase::Off || !object || !alive(object)) return;
  void* t = phasmo::GameObject::type.is(object) ? phasmo::GameObject::get_transform(object)
                                                : phasmo::Component::get_transform(object);
  move_view_layer(t, g.viewMoved);
}

float seconds_until_out() {
  switch (g.phase) {
    case Phase::Off:
      return kEaseSeconds;
    case Phase::Out:
      return std::max(0.0f, kEaseSeconds - g.t);
    case Phase::On:
      return 0.0f;
    case Phase::Back:
      return std::min(kEaseSeconds, g.t);  // begin_third_person turns it out again from kEaseSeconds - t
  }
  return kEaseSeconds;
}

float view_yaw() {
  void* cam = local_camera();
  void* t = cam ? phasmo::Component::get_transform(cam) : nullptr;
  if (!t) return 0.0f;
  const phasmo::Vector3 f = phasmo::Transform::get_forward(t);
  return std::atan2(f.x, f.z) * 57.29577951f;
}

void show_own_body(bool on) {
  if (on) {
    if (g.shownOn && alive(g.shownOn)) return;
    void* cam = local_camera();
    if (!cam) return;
    g.shownMask = phasmo::Camera::get_cullingMask(cam);
    phasmo::Camera::set_cullingMask(cam, g.shownMask | alive_bit());
    void* player = local_player();
    move_to_alive(player ? phasmo::Player::renderers.get(player) : nullptr, g.shownMoved);
    g.shownOn = cam;
    return;
  }
  if (g.shownOn && alive(g.shownOn)) phasmo::Camera::set_cullingMask(g.shownOn, g.shownMask);
  restore_layers(g.shownMoved);
  g.shownOn = nullptr;
}

void stop_third_person() {
  finish();
  show_own_body(false);
}

}  // namespace pm::api::view
