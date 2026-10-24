#pragma once

// UnityEngine.SceneManagement.SceneManager: Unity's scene loading. Not obfuscated, so every entry is
// by real name. The host reads the active scene to know when a scene loaded, which ends what it
// remembers of the Scene and AnimatorParam entries (Scene.h); a mod gets no wrapper.

#include "entry.h"

namespace pg::map {

/// Unity's scene manager (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc SceneManager{.key = "SceneManager", .name = "SceneManager",
                                                 .assembly = "UnityEngine.CoreModule",
                                                 .ns = "UnityEngine.SceneManagement"};

/// The active scene. Static. Its result is a Scene, a struct whose only field is the scene's handle
/// (an int), so it comes back as that int; every scene load gives a new handle, the same scene loaded
/// again included. Inferred: Unity's documented API; the struct's one int field is in the dump of
/// build 25811160.
inline constexpr pg::sdk::EntryDesc SceneManager_GetActiveScene{
    .key = "SceneManager.GetActiveScene", .kind = pg::sdk::Kind::Method, .flags = pg::sdk::kStatic,
    .owner = "SceneManager", .name = "GetActiveScene"};

}  // namespace pg::map
