#pragma once

#include "Transform.h"

// A clip played once at a point in the world, as AudioSource.PlayClipAtPoint does: a new object
// there with a fully 3D AudioSource, destroyed when the clip ends. This game build strips that
// method, so its steps are taken here from the calls the build keeps, through the game API
// (sdk/map/UnityAudio.h). The mod requires those entries, so it does not start without them. Main
// thread.
namespace pm::custom::sound {

bool resolve();    // true: its entries are the mod's requirements
bool available();  // true while the mod runs
// fullVolumeWithin: metres within which it plays at full volume, falling off beyond (AudioSource.minDistance); 0 or less
// keeps Unity's own 1 m. silentBeyond: metres at which it falls to silence, evenly from fullVolumeWithin
// (AudioRolloffMode.Linear, AudioSource.maxDistance); 0 or less keeps Unity's logarithmic fall, which never reaches
// silence and carries through walls and floors.
void play_at(void* clip, phasmo::Vector3 at, float volume, float fullVolumeWithin = 0.0f, float silentBeyond = 0.0f);
// The same, from an object made a child of the Transform `parent`, at its origin, so the sound moves with it (a voice
// with the one speaking). It ends with the clip, or with `parent` when that is destroyed first.
void play_on(void* clip, void* parent, float volume, float fullVolumeWithin = 0.0f, float silentBeyond = 0.0f);

}  // namespace pm::custom::sound
