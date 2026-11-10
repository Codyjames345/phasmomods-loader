#include "screen_effects.h"

#include <algorithm>

#include "Camera.h"
#include "Player.h"
#include "UnityObject.h"
#include "UnityPostProcessing.h"
#include "held.h"
#include "log.h"
#include "runtime.h"

namespace pm::api::fx {

namespace {

// The Gaussian blur's reach: from the camera, full by this far, in metres. Starting values, judged in game.
constexpr float kBlurStart = 0.0f, kBlurEnd = 0.5f;
// Its radius at the weakest and strongest blur (URP's own range, 0.5 to 1.5).
constexpr float kRadiusLow = 0.5f, kRadiusHigh = 1.5f;
// The vignette's edge softness. A starting value, judged in game.
constexpr float kVignetteSmoothness = 0.4f;

struct State {
  Held volume;  // the Volume; its GameObject goes with it
  Held vignette, dof;
};
State g;

void* local_camera() {
  void* net = phasmo::Network::get_Instance();
  void* rig = net ? phasmo::Network::localRig.get(net) : nullptr;
  void* cam = rig && alive(rig) ? phasmo::LocalPlayer::camera.get(rig) : nullptr;
  return cam && alive(cam) ? cam : nullptr;
}

// The lowest layer the camera takes volumes from; 0 (Default) when it says none.
// A parameter's value, set by running its own constructor on it again with its own range and the override on: the
// generic VolumeParameter<T>.set_value has no code in this game build (sdk/map/UnityPostProcessing.h).
void set_clamped(void* param, float value) {
  if (!param) return;
  const float lo = phasmo::ClampedFloatParameter::min.get(param), hi = phasmo::ClampedFloatParameter::max.get(param);
  phasmo::ClampedFloatParameter::ctor(param, std::clamp(value, lo, std::max(lo, hi)), lo, hi, true);
}
void set_min(void* param, float value) {
  if (!param) return;
  const float lo = phasmo::MinFloatParameter::min.get(param);
  phasmo::MinFloatParameter::ctor(param, std::max(value, lo), lo, true);
}
void set_mode(void* param, int32_t mode) {
  if (param) phasmo::DepthOfFieldModeParameter::ctor(param, mode, true);
}

int32_t volume_layer(void* camera) {
  void* dataType = phasmo::UniversalAdditionalCameraData::type.type_object();
  void* data = dataType ? phasmo::Component::GetComponent(camera, dataType) : nullptr;
  const int32_t mask = data ? phasmo::UniversalAdditionalCameraData::get_volumeLayerMask(data) : 0;
  for (int32_t layer = 0; layer < 32; ++layer)
    if (mask & (1 << layer)) return layer;
  return 0;
}

bool make_volume(void* camera) {
  if (!phasmo::VolumeProfile::Add.available() || !phasmo::ClampedFloatParameter::ctor.available()) {
    PM_LOG("fx: the URP volume entries did not resolve in this game build: no blur or vignette");
    return false;
  }
  void* go = phasmo::GameObject::type.make();
  if (!go) return false;
  phasmo::GameObject::ctor(go, make_string("pm screen effects"));
  phasmo::GameObject::set_layer(go, volume_layer(camera));
  void* volume = phasmo::GameObject::AddComponent(go, phasmo::Volume::type.type_object());
  void* profileType = phasmo::VolumeProfile::type.type_object();
  void* profile = profileType ? phasmo::ScriptableObject::CreateInstance(profileType) : nullptr;
  void* dof = profile ? phasmo::VolumeProfile::Add(profile, phasmo::DepthOfField::type.type_object(), true) : nullptr;
  void* vignette = profile ? phasmo::VolumeProfile::Add(profile, phasmo::Vignette::type.type_object(), true) : nullptr;
  if (!volume || !dof || !vignette) {
    phasmo::Object::Destroy(go, 0.0f);
    return false;
  }
  phasmo::Volume::m_IsGlobal.set(volume, true);
  phasmo::Volume::priority.set(volume, kPriority);
  phasmo::Volume::weight.set(volume, 1.0f);
  phasmo::Volume::sharedProfile.set(volume, profile);
  set_mode(phasmo::DepthOfField::mode.get(dof), static_cast<int32_t>(phasmo::DepthOfFieldMode::Gaussian()));
  set_min(phasmo::DepthOfField::gaussianStart.get(dof), kBlurStart);
  set_min(phasmo::DepthOfField::gaussianEnd.get(dof), kBlurEnd);
  set_clamped(phasmo::Vignette::smoothness.get(vignette), kVignetteSmoothness);
  g.volume = volume;
  g.dof = dof;
  g.vignette = vignette;
  return true;
}

}  // namespace

bool set_screen_effects(float blur, float vignette) {
  blur = std::clamp(blur, 0.0f, 1.0f);
  vignette = std::clamp(vignette, 0.0f, 1.0f);
  if (blur <= 0.0f && vignette <= 0.0f) {
    clear_screen_effects();
    return true;
  }
  if (!g.volume || !alive(g.volume)) {
    void* camera = local_camera();
    if (!camera || !make_volume(camera)) return false;
  }
  // No blur at all is the Gaussian off: its radius at the low end and a start beyond reach.
  set_clamped(phasmo::DepthOfField::gaussianMaxRadius.get(g.dof),
                                    kRadiusLow + (kRadiusHigh - kRadiusLow) * blur);
  set_min(phasmo::DepthOfField::gaussianStart.get(g.dof), blur > 0.0f ? kBlurStart : 1000.0f);
  set_clamped(phasmo::Vignette::intensity.get(g.vignette), vignette);
  return true;
}

bool screen_effects_on() { return g.volume && alive(g.volume); }

void clear_screen_effects() {
  if (g.volume && alive(g.volume))
    if (void* go = phasmo::Component::get_gameObject(g.volume)) phasmo::Object::Destroy(go, 0.0f);
  g = State{};
}

}  // namespace pm::api::fx
