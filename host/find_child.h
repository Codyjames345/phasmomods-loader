#pragma once

namespace pm::host {

// Transform.Find from a Transform, "a/b" paths, or null: for the host's own menu controls, whose paths are not yet Scene
// entries. No mod has it: a raw path would put an asset fact back in mod code (sdk/map/Transform.h). Main thread.
void* find_child(void* transform, const char* path);

}  // namespace pm::host
