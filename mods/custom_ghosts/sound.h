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
void play_at(void* clip, phasmo::Vector3 at, float volume);

}  // namespace pm::custom::sound
