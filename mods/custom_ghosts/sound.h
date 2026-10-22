#pragma once

#include "il2cpp.h"

// A clip played once at a point in the world, as AudioSource.PlayClipAtPoint does: a new object
// there with a fully 3D AudioSource, destroyed when the clip ends. This game build strips that
// method, so its steps are taken here from the calls the build keeps. Main thread.
namespace pg::custom::sound {

bool resolve();    // optional: false leaves available() false
bool available();
void play_at(void* clip, il2cpp::Vector3 at, float volume);

}  // namespace pg::custom::sound
