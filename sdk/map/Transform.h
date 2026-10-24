#pragma once

// UnityEngine.Transform: Unity's transform component, which holds the scene's hierarchy. Not
// obfuscated, so every entry is by real name. The host uses these to find Scene entries (Scene.h); a
// mod gets no wrapper for them, since a lookup by a raw path string would put asset facts back into
// mod code.

#include "entry.h"

namespace pg::map {

inline constexpr const char* Transform_string[] = {"string"};

/// Unity's transform component (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc Transform{.key = "Transform", .name = "Transform",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Finds a child by its name, or a descendant by a path of names separated by '/', inactive ones
/// included; null when there is none. Where several children share a name it takes the first that
/// leads to the rest of the path. The host's scene_find calls it. Inferred: Unity's documented API.
inline constexpr pg::sdk::EntryDesc Transform_Find{.key = "Transform.Find", .kind = pg::sdk::Kind::Method,
                                                   .owner = "Transform", .name = "Find", .params = Transform_string};

}  // namespace pg::map
