#include "sound.h"

#include <algorithm>

#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::custom::sound {

bool resolve() { return true; }

bool available() { return true; }

void play_at(void* clip, phasmo::Vector3 at, float volume) {
  if (!api::alive(clip)) return;
  void* go = phasmo::GameObject::type.make();
  if (!go) return;
  phasmo::GameObject::ctor(go, api::make_string("One shot audio"));
  const phasmo::Vector3 where{at.x, at.y, at.z};
  phasmo::Transform::set_position(phasmo::GameObject::get_transform(go), &where);
  void* source = phasmo::GameObject::AddComponent(go, phasmo::AudioSource::type.type_object());
  if (!source) {
    phasmo::Object::Destroy(go, 0.0f);
    return;
  }
  phasmo::AudioSource::set_clip(source, clip);
  phasmo::AudioSource::set_spatialBlend(source, 1.0f);
  phasmo::AudioSource::set_volume(source, volume);
  phasmo::AudioSource::Play(source, 0);
  phasmo::Object::Destroy(go, phasmo::AudioClip::get_length(clip) * std::max(phasmo::Time::get_timeScale(), 0.01f));
}

}  // namespace pm::custom::sound
