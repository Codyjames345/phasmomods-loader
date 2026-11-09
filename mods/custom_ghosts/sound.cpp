#include "sound.h"

#include <algorithm>

#include "Player.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "runtime.h"

namespace pm::custom::sound {

bool resolve() { return true; }

bool available() { return true; }

namespace {

// Plays `clip` from the new object `go` at `at`, destroying it with the clip. It plays through the game's mixer group for
// that place, as the game's own noises do (Noise.PlaySound with Noise.changeMixerOnPlay: SoundController's
// GetFloorAudioSnapshot), so the game muffles it as it does them, on another floor or from outdoors (user, 2026-11-09:
// the Squatter was heard on other floors and from outside, its sounds having gone past the mixer).
void play_from(void* go, void* clip, phasmo::Vector3 at, float volume, float fullVolumeWithin, float silentBeyond) {
  void* source = phasmo::GameObject::AddComponent(go, phasmo::AudioSource::type.type_object());
  if (!source) {
    phasmo::Object::Destroy(go, 0.0f);
    return;
  }
  phasmo::AudioSource::set_clip(source, clip);
  void* sounds = phasmo::SoundController::instance.get();
  if (sounds && api::alive(sounds))
    if (void* group = phasmo::SoundController::GetFloorAudioSnapshot(sounds, &at))
      phasmo::AudioSource::set_outputAudioMixerGroup(source, group);
  phasmo::AudioSource::set_spatialBlend(source, 1.0f);
  phasmo::AudioSource::set_volume(source, volume);
  if (fullVolumeWithin > 0.0f) phasmo::AudioSource::set_minDistance(source, fullVolumeWithin);
  if (silentBeyond > 0.0f) {
    phasmo::AudioSource::set_rolloffMode(source, phasmo::AudioRolloffMode::Linear());
    phasmo::AudioSource::set_maxDistance(source, silentBeyond);
  }
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

void play_at(void* clip, phasmo::Vector3 at, float volume, float fullVolumeWithin, float silentBeyond) {
  if (!api::alive(clip)) return;
  void* go = one_shot();
  if (!go) return;
  const phasmo::Vector3 where{at.x, at.y, at.z};
  phasmo::Transform::set_position(phasmo::GameObject::get_transform(go), &where);
  play_from(go, clip, where, volume, fullVolumeWithin, silentBeyond);
}

void play_on(void* clip, void* parent, float volume, float fullVolumeWithin, float silentBeyond) {
  if (!api::alive(clip) || !api::alive(parent)) return;
  void* go = one_shot();
  if (!go) return;
  void* t = phasmo::GameObject::get_transform(go);
  phasmo::Transform::SetParent(t, parent, false);
  const phasmo::Vector3 origin{0, 0, 0};
  phasmo::Transform::set_localPosition(t, &origin);
  play_from(go, clip, phasmo::Transform::get_position(parent), volume, fullVolumeWithin, silentBeyond);
}

}  // namespace pm::custom::sound
