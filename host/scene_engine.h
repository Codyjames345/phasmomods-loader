#pragma once

#include "scene_facts.h"

// The scene facts' engine in the running game (sdk/runtime/scene_facts.h), built on the map's own
// Unity entries: Transform.Find, LayerMask.NameToLayer, SceneManager.GetActiveScene, the Animator's
// parameter list and Shader.PropertyToID. A call whose entries did not resolve answers as for a miss.
// Main thread.
namespace pm::scene_engine {

// The engine, valid for the process lifetime.
const sdk::SceneEngine* engine();

}  // namespace pm::scene_engine
