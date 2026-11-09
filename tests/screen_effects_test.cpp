// Offline tests for a mod's screen blur and vignette (sdk/api/screen_effects.h) over the fake backend: a URP volume of
// the mod's own on a layer the player's camera takes volumes from, with a Gaussian depth of field and a vignette set to
// the strengths asked for, and nothing left behind when both go to 0. The game's side is in sdk/systems/media.md.
// Run: build\screen_effects_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "Camera.h"
#include "Player.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "check.h"
#include "fake_game.h"
#include "runtime.h"
#include "screen_effects.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace fx = pm::api::fx;
namespace map = pm::map;
namespace fake = pm::fake;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x90]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}
constexpr int32_t kNetRig = 0x08, kRigCamera = 0x08, kVolMask = 0x10, kLayer = 0x14;
constexpr int32_t kGlobal = 0x18, kPriority = 0x1C, kWeight = 0x20, kProfile = 0x28;
constexpr int32_t kP1 = 0x30, kP2 = 0x38, kP3 = 0x40, kP4 = 0x48;  // a fake component's parameters

Obj g_net, g_rig, g_camera, g_cameraData, g_goClass, g_volumeClass, g_profileClass, g_vignetteClass, g_dofClass,
    g_dataClass;
std::vector<Obj*> g_made;
std::vector<void*> g_destroyed;
std::map<void*, float> g_floats;  // a parameter's float value
std::map<void*, int32_t> g_modes;

Obj* make(void* klass) {
  auto* o = new Obj{};
  o->klass = klass;
  g_made.push_back(o);
  return o;
}

void* fake_network(void*) { return &g_net; }
void fake_go_ctor(void*, void*, void*) {}
void* fake_add_component(void*, void*, void*) { return make(&g_volumeClass); }
void* fake_create_instance(void*, void*) { return make(&g_profileClass); }
void* fake_add(void*, void* type, bool overrides, void*) {
  CHECK(overrides);
  Obj* c = make(type == phasmo::Vignette::type.type_object() ? static_cast<void*>(&g_vignetteClass)
                                                             : static_cast<void*>(&g_dofClass));
  for (int32_t off : {kP1, kP2, kP3, kP4}) at<void*>(c, off) = make(nullptr);
  return c;
}
void fake_set_float(void* p, float v, void*) { g_floats[p] = v; }
void fake_set_mode(void* p, int32_t v, void*) { g_modes[p] = v; }
void* fake_get_component(void*, void*, void*) { return &g_cameraData; }
int32_t fake_vol_mask(void* d, void*) { return at<int32_t>(d, kVolMask); }
int32_t g_layerSet = -1;
void fake_set_layer(void*, int32_t l, void*) { g_layerSet = l; }
void* fake_self(void* o, void*) { return o; }
void fake_destroy(void* o, float) {
  g_destroyed.push_back(o);
  fake::destroy(o);
}

void give() {
  fake::reset();
  for (Obj* o : g_made) delete o;
  g_made.clear();
  g_destroyed.clear();
  g_floats.clear();
  g_modes.clear();
  g_layerSet = -1;
  fake::type(map::GameObject, &g_goClass);
  fake::type(map::Volume, &g_volumeClass);
  fake::type(map::VolumeProfile, &g_profileClass);
  fake::type(map::Vignette, &g_vignetteClass);
  fake::type(map::DepthOfField, &g_dofClass);
  fake::type(map::UniversalAdditionalCameraData, &g_dataClass);
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localRig, kNetRig);
  fake::field(map::LocalPlayer_camera, kRigCamera);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&fake_get_component));
  fake::method(map::UniversalAdditionalCameraData_get_volumeLayerMask, reinterpret_cast<void*>(&fake_vol_mask));
  fake::method(map::GameObject_ctor, reinterpret_cast<void*>(&fake_go_ctor));
  fake::method(map::GameObject_set_layer, reinterpret_cast<void*>(&fake_set_layer));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&fake_add_component));
  fake::method(map::Component_get_gameObject, reinterpret_cast<void*>(&fake_self));
  fake::method(map::ScriptableObject_CreateInstance, reinterpret_cast<void*>(&fake_create_instance));
  fake::method(map::VolumeProfile_Add, reinterpret_cast<void*>(&fake_add));
  fake::method(map::VolumeParameter_float_set_value, reinterpret_cast<void*>(&fake_set_float));
  fake::method(map::VolumeParameter_DepthOfFieldMode_set_value, reinterpret_cast<void*>(&fake_set_mode));
  fake::value(map::DepthOfFieldMode_Gaussian, 1);
  fake::field(map::Volume_m_IsGlobal, kGlobal);
  fake::field(map::Volume_priority, kPriority);
  fake::field(map::Volume_weight, kWeight);
  fake::field(map::Volume_sharedProfile, kProfile);
  fake::field(map::Vignette_intensity, kP1);
  fake::field(map::Vignette_smoothness, kP2);
  fake::field(map::DepthOfField_mode, kP1);
  fake::field(map::DepthOfField_gaussianStart, kP2);
  fake::field(map::DepthOfField_gaussianEnd, kP3);
  fake::field(map::DepthOfField_gaussianMaxRadius, kP4);
  fake::icall(map::UnityObject_Destroy, reinterpret_cast<void*>(&fake_destroy));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  at<void*>(&g_net, kNetRig) = &g_rig;
  at<void*>(&g_rig, kRigCamera) = &g_camera;
  at<int32_t>(&g_cameraData, kVolMask) = 0b1000100;  // layers 2 and 6
}

Obj* only(void* klass) {
  Obj* found = nullptr;
  for (Obj* o : g_made)
    if (o->klass == klass) found = o;
  return found;
}

void effects_come_and_go() {
  give();
  CHECK(fx::set_screen_effects(0.5f, 0.4f));
  Obj* volume = only(&g_volumeClass);
  CHECK(volume != nullptr);
  if (!volume) return;
  CHECK(at<bool>(volume, kGlobal) && at<float>(volume, kWeight) == 1.0f && at<float>(volume, kPriority) > 0.0f);
  CHECK(g_layerSet == 2);  // the lowest layer the camera takes volumes from
  Obj* vignette = only(&g_vignetteClass);
  Obj* dof = only(&g_dofClass);
  CHECK(vignette && dof);
  if (!vignette || !dof) return;
  CHECK(g_floats[at<void*>(vignette, kP1)] == 0.4f);
  CHECK(g_modes[at<void*>(dof, kP1)] == 1);  // Gaussian
  const float radius = g_floats[at<void*>(dof, kP4)];
  CHECK(radius >= 0.5f && radius <= 1.5f);
  // A change sets the values again on the same volume.
  CHECK(fx::set_screen_effects(1.0f, 0.2f));
  CHECK(only(&g_volumeClass) == volume);
  CHECK(g_floats[at<void*>(vignette, kP1)] == 0.2f);
  CHECK(g_floats[at<void*>(dof, kP4)] > radius);
  CHECK(fx::screen_effects_on());
  // Both at 0: the volume is gone.
  CHECK(fx::set_screen_effects(0.0f, 0.0f));
  CHECK(!fx::screen_effects_on());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), static_cast<void*>(volume)) != g_destroyed.end());
}

void a_blur_alone_has_no_vignette() {
  give();
  fx::set_screen_effects(0.5f, 0.0f);
  Obj* vignette = only(&g_vignetteClass);
  CHECK(vignette && g_floats[at<void*>(vignette, kP1)] == 0.0f);
  fx::clear_screen_effects();
  CHECK(!fx::screen_effects_on());
}

void no_camera_no_effects() {
  give();
  at<void*>(&g_rig, kRigCamera) = nullptr;
  CHECK(!fx::set_screen_effects(0.5f, 0.5f));
  CHECK(only(&g_volumeClass) == nullptr);
}

}  // namespace

int main() {
  effects_come_and_go();
  a_blur_alone_has_no_vignette();
  no_camera_no_effects();
  std::printf("%s\n", g_failures ? "screen_effects tests FAILED" : "screen_effects tests passed");
  return g_failures ? 1 : 0;
}
