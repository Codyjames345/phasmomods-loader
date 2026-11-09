// Offline tests for the third-person view and the shown body (sdk/api/third_person.h, orbit_rule.h): the pure rules of
// where the camera sits, how it eases and how a wall pulls it in, and over the fake backend a view that copies the
// player's camera, draws the body, follows the head, and on its end gives the player their own camera back exactly as
// it was. The game's side is in sdk/systems/media.md. Run: build\third_person_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "Camera.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "UnityWorld.h"
#include "check.h"
#include "fake_game.h"
#include "orbit_rule.h"
#include "runtime.h"
#include "third_person.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace view = pm::api::view;
namespace map = pm::map;
namespace fake = pm::fake;
using view::Point;

bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }

// ---- The pure rules ----

void the_camera_starts_straight_behind() {
  const Point head{1.0f, 1.6f, 2.0f}, fwd{0.0f, 0.0f, 1.0f};
  const Point at0 = view::orbit_point(head, fwd, 0.0f, 0.0f);
  CHECK(near(at0.x, 1.0f) && near(at0.y, 1.6f) && near(at0.z, 2.0f));
  const Point back = view::orbit_point(head, fwd, 2.0f, 0.25f);
  CHECK(near(back.x, 1.0f) && near(back.y, 1.85f) && near(back.z, 0.0f));  // behind, along -forward, raised
  const Point side = view::orbit_point(head, {1.0f, 0.0f, 0.0f}, 2.0f, 0.0f);
  CHECK(near(side.x, -1.0f) && near(side.z, 2.0f));
}

void a_wall_pulls_the_camera_in() {
  CHECK(near(view::pulled_in(2.2f, 1.0f), 0.8f));
  CHECK(near(view::pulled_in(2.2f, 100.0f), 2.2f));
  CHECK(near(view::pulled_in(2.2f, 0.1f), 0.0f));  // a hit inside the margin: at the head
  CHECK(near(view::pulled_in(0.0f, 0.1f), 0.0f));
}

void the_ease_is_smooth_and_bounded() {
  CHECK(view::ease(0.0f) == 0.0f && view::ease(1.0f) == 1.0f);
  CHECK(view::ease(-1.0f) == 0.0f && view::ease(2.0f) == 1.0f);
  CHECK(near(view::ease(0.5f), 0.5f));
  float last = 0.0f;
  for (int i = 1; i <= 100; ++i) {
    const float e = view::ease(i / 100.0f);
    CHECK(e >= last);
    last = e;
  }
}

// ---- Over the fake game ----

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x80]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}
// The fake's own layout: a component, its GameObject and its Transform are one fake object.
constexpr int32_t kPos = 0x10, kFwd = 0x1C, kEnabled = 0x28, kMask = 0x2C, kLayer = 0x30, kPost = 0x34,
                  kVolMask = 0x38, kCopied = 0x40;
constexpr int32_t kNetRig = 0x08, kNetPlayer = 0x48, kRigCamera = 0x08, kPlayerRenderers = 0x08;

Obj g_net, g_rig, g_player, g_camera, g_gameData, g_bodyA, g_bodyB, g_cameraClass, g_dataClass, g_goClass, g_hitClass;
std::vector<Obj*> g_made;
std::vector<void*> g_destroyed;
float g_wallAt = 1000.0f;  // metres behind the head a wall lies

Obj* make(void* klass) {
  auto* o = new Obj{};
  o->klass = klass;
  g_made.push_back(o);
  return o;
}

void* fake_network(void*) { return &g_net; }
void* fake_self(void* o, void*) { return o; }
phasmo::Vector3 fake_position(void* t, void*) { return at<phasmo::Vector3>(t, kPos); }
phasmo::Vector3 fake_forward(void* t, void*) { return at<phasmo::Vector3>(t, kFwd); }
void fake_set_position(void* t, const phasmo::Vector3* p, void*) { at<phasmo::Vector3>(t, kPos) = *p; }
phasmo::Quaternion fake_rotation(void*, void*) { return {0, 0, 0, 1}; }
void fake_set_rotation(void*, const phasmo::Quaternion*, void*) {}
void fake_go_ctor(void*, void*, void*) {}
void* fake_add_component(void*, void* type, void*) {
  return make(type == phasmo::Camera::type.type_object() ? static_cast<void*>(&g_cameraClass)
                                                          : static_cast<void*>(&g_dataClass));
}
void fake_copy_from(void* cam, void* other) {
  at<int32_t>(cam, kMask) = at<int32_t>(other, kMask);
  at<bool>(cam, kCopied) = true;
}
void* fake_get_component(void* c, void*, void*) { return c == &g_camera ? &g_gameData : nullptr; }
bool fake_get_post(void* d, void*) { return at<bool>(d, kPost); }
void fake_set_post(void* d, bool on, void*) { at<bool>(d, kPost) = on; }
int32_t fake_get_vol(void* d, void*) { return at<int32_t>(d, kVolMask); }
void fake_set_vol(void* d, int32_t m, void*) { at<int32_t>(d, kVolMask) = m; }
int32_t fake_get_mask(void* c, void*) { return at<int32_t>(c, kMask); }
void fake_set_mask(void* c, int32_t m, void*) { at<int32_t>(c, kMask) = m; }
void fake_set_enabled(void* b, bool on, void*) { at<bool>(b, kEnabled) = on; }
bool fake_get_enabled(void* b, void*) { return at<bool>(b, kEnabled); }
int32_t fake_layer(void* go, void*) { return at<int32_t>(go, kLayer); }
bool fake_raycast(const phasmo::Ray*, void* hit, float maxDistance, int32_t, int32_t, void*) {
  if (g_wallAt > maxDistance) return false;
  *static_cast<float*>(hit) = g_wallAt;
  return true;
}
float fake_hit_distance(void* hit, void*) { return *static_cast<float*>(hit); }
void fake_destroy(void* o, float) {
  g_destroyed.push_back(o);
  fake::destroy(o);
}

void give() {
  fake::reset();
  for (Obj* o : g_made) delete o;
  g_made.clear();
  g_destroyed.clear();
  g_wallAt = 1000.0f;
  fake::type(map::Camera, &g_cameraClass);
  fake::type(map::UniversalAdditionalCameraData, &g_dataClass);
  fake::type(map::GameObject, &g_goClass);
  fake::type(map::RaycastHit, &g_hitClass);
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localRig, kNetRig);
  fake::field(map::Network_localPlayer, kNetPlayer);
  fake::field(map::LocalPlayer_camera, kRigCamera);
  fake::field(map::Player_renderers, kPlayerRenderers);
  fake::method(map::Component_get_transform, reinterpret_cast<void*>(&fake_self));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_self));
  fake::method(map::GameObject_get_transform, reinterpret_cast<void*>(&fake_self));
  fake::method(map::Transform_get_position, reinterpret_cast<void*>(&fake_position));
  fake::method(map::Transform_get_forward, reinterpret_cast<void*>(&fake_forward));
  fake::method(map::Transform_set_position, reinterpret_cast<void*>(&fake_set_position));
  fake::method(map::Transform_get_rotation, reinterpret_cast<void*>(&fake_rotation));
  fake::method(map::Transform_set_rotation, reinterpret_cast<void*>(&fake_set_rotation));
  fake::method(map::GameObject_ctor, reinterpret_cast<void*>(&fake_go_ctor));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&fake_add_component));
  fake::icall(map::Camera_CopyFrom, reinterpret_cast<void*>(&fake_copy_from));
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::UniversalAdditionalCameraData_get_renderPostProcessing, reinterpret_cast<void*>(&fake_get_post));
  fake::method(map::UniversalAdditionalCameraData_set_renderPostProcessing, reinterpret_cast<void*>(&fake_set_post));
  fake::method(map::UniversalAdditionalCameraData_get_volumeLayerMask, reinterpret_cast<void*>(&fake_get_vol));
  fake::method(map::UniversalAdditionalCameraData_set_volumeLayerMask, reinterpret_cast<void*>(&fake_set_vol));
  fake::method(map::Camera_get_cullingMask, reinterpret_cast<void*>(&fake_get_mask));
  fake::method(map::Camera_set_cullingMask, reinterpret_cast<void*>(&fake_set_mask));
  fake::method(map::Behaviour_set_enabled, reinterpret_cast<void*>(&fake_set_enabled));
  fake::method(map::Behaviour_get_enabled, reinterpret_cast<void*>(&fake_get_enabled));
  fake::method(map::GameObject_get_layer, reinterpret_cast<void*>(&fake_layer));
  fake::method(map::Physics_RaycastHit, reinterpret_cast<void*>(&fake_raycast));
  fake::method(map::RaycastHit_get_distance, reinterpret_cast<void*>(&fake_hit_distance));
  fake::value(map::QueryTriggerInteraction_Ignore, 1);
  fake::icall(map::UnityObject_Destroy, reinterpret_cast<void*>(&fake_destroy));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  at<void*>(&g_net, kNetRig) = &g_rig;
  at<void*>(&g_net, kNetPlayer) = &g_player;
  at<void*>(&g_rig, kRigCamera) = &g_camera;
  at<phasmo::Vector3>(&g_camera, kPos) = {0.0f, 1.6f, 0.0f};
  at<phasmo::Vector3>(&g_camera, kFwd) = {0.0f, 0.0f, 1.0f};
  at<bool>(&g_camera, kEnabled) = true;
  at<int32_t>(&g_camera, kMask) = 0x0F;  // the body's layers (4 and 5) left out, as the game does
  at<bool>(&g_gameData, kPost) = true;
  at<int32_t>(&g_gameData, kVolMask) = 0x100;
  at<int32_t>(&g_bodyA, kLayer) = 4;
  at<int32_t>(&g_bodyB, kLayer) = 5;
  void* renderers = fake::array(sizeof(void*), 2);
  static_cast<void**>(pm::api::ValueArray{renderers}.at(0))[0] = &g_bodyA;
  static_cast<void**>(pm::api::ValueArray{renderers}.at(1))[0] = &g_bodyB;
  at<void*>(&g_player, kPlayerRenderers) = renderers;
}

Obj* our_camera() {
  for (Obj* o : g_made)
    if (o->klass == &g_cameraClass) return o;
  return nullptr;
}

void the_view_copies_the_camera_and_draws_the_body() {
  give();
  CHECK(view::begin_third_person());
  CHECK(!view::begin_third_person());  // already on
  Obj* cam = our_camera();
  CHECK(cam && at<bool>(cam, kCopied));
  if (!cam) return;
  CHECK(at<int32_t>(cam, kMask) == (0x0F | 0x10 | 0x20));  // the body's layers added
  CHECK(at<int32_t>(&g_camera, kMask) == 0x0F);           // the player's camera untouched
  CHECK(!at<bool>(&g_camera, kEnabled));                  // and off while the view is on
  // The URP settings copied onto the view's own data.
  bool copied = false;
  for (Obj* o : g_made)
    if (o->klass == &g_dataClass) copied = at<bool>(o, kPost) && at<int32_t>(o, kVolMask) == 0x100;
  CHECK(copied);
  // It eases out straight back from the head, to the full distance.
  view::tick_third_person(view::kEaseSeconds / 2);
  const float mid = -at<phasmo::Vector3>(cam, kPos).z;
  CHECK(mid > 0.1f && mid < view::kOrbitDistance);
  view::tick_third_person(view::kEaseSeconds);
  CHECK(near(at<phasmo::Vector3>(cam, kPos).z, -view::kOrbitDistance, 1e-3f));
  CHECK(near(at<phasmo::Vector3>(cam, kPos).y, 1.6f + view::kOrbitHeight, 1e-3f));
  // A wall a metre behind pulls it in.
  g_wallAt = 1.0f;
  view::tick_third_person(0.016f);
  CHECK(near(at<phasmo::Vector3>(cam, kPos).z, -(1.0f - view::kWallMargin), 1e-3f));
  // The head turns: the view follows.
  g_wallAt = 1000.0f;
  at<phasmo::Vector3>(&g_camera, kFwd) = {1.0f, 0.0f, 0.0f};
  view::tick_third_person(0.016f);
  CHECK(near(at<phasmo::Vector3>(cam, kPos).x, -view::kOrbitDistance, 1e-3f));
  CHECK(near(view::view_yaw(), 90.0f, 1e-2f));
  view::stop_third_person();
  CHECK(at<bool>(&g_camera, kEnabled));  // stopped at once: the player's camera on again
}

void the_ease_ends_where_it_started() {
  give();
  view::begin_third_person();
  view::tick_third_person(view::kEaseSeconds);
  Obj* cam = our_camera();
  view::end_third_person();
  CHECK(view::in_third_person());  // easing back
  CHECK(!at<bool>(&g_camera, kEnabled));
  view::tick_third_person(view::kEaseSeconds / 2);
  CHECK(view::in_third_person());
  view::tick_third_person(view::kEaseSeconds);
  CHECK(!view::in_third_person());
  CHECK(at<bool>(&g_camera, kEnabled));  // the player's camera on again
  CHECK(at<int32_t>(&g_camera, kMask) == 0x0F);
  CHECK(cam && std::find(g_destroyed.begin(), g_destroyed.end(), static_cast<void*>(cam)) != g_destroyed.end());
}

void a_new_start_while_easing_back_turns_out_again() {
  give();
  view::begin_third_person();
  view::tick_third_person(view::kEaseSeconds);
  Obj* cam = our_camera();
  view::end_third_person();
  view::tick_third_person(view::kEaseSeconds / 2);  // half way back in
  const float halfWay = cam ? -at<phasmo::Vector3>(cam, kPos).z : 0.0f;
  CHECK(view::begin_third_person());  // a new emote: out again from where it is
  view::tick_third_person(0.016f);
  CHECK(cam && -at<phasmo::Vector3>(cam, kPos).z >= halfWay - 1e-3f);
  view::tick_third_person(view::kEaseSeconds);
  CHECK(view::in_third_person());
  CHECK(cam && near(at<phasmo::Vector3>(cam, kPos).z, -view::kOrbitDistance, 1e-3f));
  CHECK(!at<bool>(&g_camera, kEnabled));
  view::stop_third_person();
}

void a_lost_camera_ends_the_view_at_once() {
  give();
  view::begin_third_person();
  fake::destroy(&g_camera);
  view::tick_third_person(0.016f);
  CHECK(!view::in_third_person());
}

void the_body_shows_on_the_players_camera() {
  give();
  view::show_own_body(true);
  CHECK(at<int32_t>(&g_camera, kMask) == (0x0F | 0x10 | 0x20));
  view::show_own_body(true);  // twice is once
  view::show_own_body(false);
  CHECK(at<int32_t>(&g_camera, kMask) == 0x0F);
  view::show_own_body(true);
  view::stop_third_person();
  CHECK(at<int32_t>(&g_camera, kMask) == 0x0F);
}

}  // namespace

int main() {
  the_camera_starts_straight_behind();
  a_wall_pulls_the_camera_in();
  the_ease_is_smooth_and_bounded();
  the_view_copies_the_camera_and_draws_the_body();
  the_ease_ends_where_it_started();
  a_new_start_while_easing_back_turns_out_again();
  a_lost_camera_ends_the_view_at_once();
  the_body_shows_on_the_players_camera();
  std::printf("%s\n", g_failures ? "third_person tests FAILED" : "third_person tests passed");
  return g_failures ? 1 : 0;
}
