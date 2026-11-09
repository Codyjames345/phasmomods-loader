#include "sound.h"

#include <algorithm>

#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::custom::sound {

bool resolve() { return true; }

bool available() { return true; }

namespace {

// Plays `clip` from the new object `go`, destroying it with the clip.
void play_from(void* go, void* clip, float volume, float fullVolumeWithin) {
  void* source = phasmo::GameObject::AddComponent(go, phasmo::AudioSource::type.type_object());
  if (!source) {
    phasmo::Object::Destroy(go, 0.0f);
    return;
  }
  phasmo::AudioSource::set_clip(source, clip);
  phasmo::AudioSource::set_spatialBlend(source, 1.0f);
  phasmo::AudioSource::set_volume(source, volume);
  if (fullVolumeWithin > 0.0f) phasmo::AudioSource::set_minDistance(source, fullVolumeWithin);
  phasmo::AudioSource::Play(source, 0);
  phasmo::Object::Destroy(go, phasmo::AudioClip::get_length(clip) * std::max(phasmo::Time::get_timeScale(), 0.01f));
}

// A new object for one sound, or null.
void* one_shot() {
  void* go = phasmo::GameObject::type.make();
  if (go) phasmo::GameObject::ctor(go, api::make_string("One shot audio"));
  return go;
}

}  // namespace

void play_at(void* clip, phasmo::Vector3 at, float volume, float fullVolumeWithin) {
  if (!api::alive(clip)) return;
  void* go = one_shot();
  if (!go) return;
  const phasmo::Vector3 where{at.x, at.y, at.z};
  phasmo::Transform::set_position(phasmo::GameObject::get_transform(go), &where);
  play_from(go, clip, volume, fullVolumeWithin);
}

void play_on(void* clip, void* parent, float volume, float fullVolumeWithin) {
  if (!api::alive(clip) || !api::alive(parent)) return;
  void* go = one_shot();
  if (!go) return;
  void* t = phasmo::GameObject::get_transform(go);
  phasmo::Transform::SetParent(t, parent, false);
  const phasmo::Vector3 origin{0, 0, 0};
  phasmo::Transform::set_localPosition(t, &origin);
  play_from(go, clip, volume, fullVolumeWithin);
}

}  // namespace pm::custom::sound
