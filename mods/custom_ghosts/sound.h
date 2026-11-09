#pragma once

#include "Transform.h"

// A clip played once at a point in the world, as AudioSource.PlayClipAtPoint does: a new object
// there with a fully 3D AudioSource, destroyed when the clip ends, through the game's mixer group for
// that place (SoundController.GetFloorAudioSnapshot), as the game's own noises play. This game build strips that
// method, so its steps are taken here from the calls the build keeps, through the game API
// (sdk/map/UnityAudio.h). The mod requires those entries, so it does not start without them. Main
// thread.
namespace pm::custom::sound {

bool resolve();    // true: its entries are the mod's requirements
bool available();  // true while the mod runs
// mixAt: where the game's mixer group is read for it (SoundController.GetFloorAudioSnapshot); null for where it plays.
// pitch: 1 as recorded; lower is deeper and slower (AudioSource.pitch), and the sound lasts its length over it.
// fullVolumeWithin: metres within which it plays at full volume, falling off beyond (AudioSource.minDistance); 0 or less
// keeps Unity's own 1 m. silentBeyond: metres at which it falls to silence, evenly from fullVolumeWithin
// (AudioRolloffMode.Linear, AudioSource.maxDistance); 0 or less keeps Unity's logarithmic fall, which never reaches
// silence and carries through walls and floors.
void play_at(void* clip, phasmo::Vector3 at, float volume, float fullVolumeWithin = 0.0f, float silentBeyond = 0.0f,
             const phasmo::Vector3* mixAt = nullptr, float pitch = 1.0f);
// The same, from an object made a child of the Transform `parent`, at its origin, so the sound moves with it (a voice
// with the one speaking). It ends with the clip, or with `parent` when that is destroyed first.
void play_on(void* clip, void* parent, float volume, float fullVolumeWithin = 0.0f, float silentBeyond = 0.0f,
             const phasmo::Vector3* mixAt = nullptr, float pitch = 1.0f);

// Plays `clip` once at `at` at `volume`, falling off with distance as the game's AudioSource `like` does: its rolloff
// mode, its minimum and maximum distances and its custom rolloff curve, copied. Without `like` it falls off as play_at's
// defaults do.
void play_like(void* clip, phasmo::Vector3 at, float volume, void* like);

}  // namespace pm::custom::sound
