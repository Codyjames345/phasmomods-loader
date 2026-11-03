#pragma once

// The scene and asset facts in the running game: layers and shader properties by name, paths found
// from an object a mod holds, and Animator parameters checked against the Animator. The host
// (host/game_map.cpp) runs this over the running game, and the fake game backend (sdk/fake) over its
// own, so the tests exercise the code the host runs. Main thread only.

#include <cstdint>
#include <string>
#include <unordered_map>

#include "entry.h"

namespace pm::sdk {

// What the facts ask of the engine. Each call contains what it throws and answers as for a miss. Where
// a call misses for a reason other than the fact being absent (an argument of the wrong type, an entry
// it needs that did not resolve), it points `why` at a static text saying so ("the root is not a
// Transform", "Transform.Find unresolved"); otherwise it leaves `why` alone.
struct SceneEngine {
  // The active scene's handle, which changes with every scene load; 0 when it cannot be read.
  int32_t (*active_scene)();
  // The Transform at `path` under the Transform `root` (Transform.Find), or null.
  void* (*find)(void* root, const char* path, const char** why);
  // The index of the layer named `name` (LayerMask.NameToLayer), or -1.
  int32_t (*name_to_layer)(const char* name, const char** why);
  // Whether the Animator `animator` lists a parameter named `name`, read from its parameter list, with
  // its hash into `hash`.
  bool (*parameter)(const void* animator, const char* name, int32_t* hash, const char** why);
  // UnityEngine.Object's implicit bool.
  bool (*alive)(const void* obj);
  // A strong GC handle on `obj` (0 for null), and its release: what is kept across frames is held.
  uint32_t (*hold)(void* obj);
  void (*release)(uint32_t handle);
  // The id of the shader property named `name` (Shader.PropertyToID) into `id`; false when it could not
  // be asked.
  bool (*property_id)(const char* name, int32_t* id, const char** why);
};

class SceneFacts {
 public:
  SceneFacts() = default;
  SceneFacts(const SceneFacts&) = delete;
  SceneFacts& operator=(const SceneFacts&) = delete;
  // Releases nothing: at process exit the runtime the handles belong to may be gone. Call forget()
  // while the game runs.
  ~SceneFacts() = default;

  // The engine the facts ask; null forgets what is kept and answers as for a miss from then on.
  void set_engine(const SceneEngine* engine);
  // Releases everything kept and forgets what was logged. Main thread, while the game runs.
  void forget();

  // The index of the Layer entry `d`'s layer, or -1, logged with why (no layer has its name, or the
  // engine could not ask).
  int32_t layer(const EntryDesc& d);
  // The id of the ShaderProperty entry `d`'s property into `id`; false, logged with why, when the engine
  // could not ask. Any name has an id: whether a material's shader has the property is the material's
  // to answer (Material.HasProperty).
  bool shader_property(const EntryDesc& d, int32_t& id);
  // The Transform at the Scene entry `d`'s path under `root`. Kept until the active scene changes,
  // the object found is destroyed or another root is given, so only a call with a new root searches.
  // A null `root` answers null at once: no search, nothing logged, the kept answer untouched. Null,
  // logged once per scene with why, when nothing is there or `root` is destroyed or not a Transform.
  void* find(const EntryDesc& d, void* root);
  // Whether `animator` has the AnimatorParam entry `d`'s parameter, with its hash. Checked once per
  // Animator and scene; a miss is logged once per scene.
  bool parameter(const EntryDesc& d, const void* animator, int32_t& hash);

 private:
  // What one entry last answered, and for which root or Animator, in which scene.
  struct Kept {
    int32_t scene = 0;
    bool valid = false;  // `scene`, `of` and the answer belong together
    void* of = nullptr;  // the root or Animator asked about, held
    uint32_t ofHandle = 0;
    void* found = nullptr;  // a path's Transform, held
    uint32_t foundHandle = 0;
    bool has = false;  // a parameter: whether the Animator has it
    int32_t hash = 0;
    const char* why = nullptr;  // a parameter: why the Animator was refused, when the engine said
    int32_t missLogged = 0;  // the scene a miss was last logged in
    bool missLoggedOnce = false;
  };

  void keep(Kept& k, int32_t scene, void* of, void* found);
  void release(Kept& k);
  // Whether a miss of the entry is due to be logged: once per scene.
  bool miss_once(Kept& k, int32_t scene);

  const SceneEngine* engine_ = nullptr;
  std::unordered_map<std::string, Kept> kept_;  // by key
};

}  // namespace pm::sdk
