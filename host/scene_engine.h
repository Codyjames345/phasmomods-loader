#pragma once

#include "scene_facts.h"

// The scene facts' engine in the running game (sdk/runtime/scene_facts.h), built on the map's own
// Unity entries: Transform.Find, LayerMask.NameToLayer, SceneManager.GetActiveScene and the Animator's
// parameter list. A call whose entries did not resolve answers as for a miss. Main thread.
namespace pg::scene_engine {

// The engine, valid for the process lifetime.
const sdk::SceneEngine* engine();

}  // namespace pg::scene_engine
