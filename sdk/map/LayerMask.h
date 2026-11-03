#pragma once

// UnityEngine.LayerMask: Unity's layer helpers. Not obfuscated, so every entry is by real name. The
// host resolves the Layer entries (Scene.h) through NameToLayer when it activates; a mod gets no
// wrapper, since it reads a layer from its Layer entry.

#include "entry.h"

namespace pm::map {

inline constexpr const char* LayerMask_string[] = {"string"};

/// Unity's layer mask struct (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc LayerMask{.key = "LayerMask", .name = "LayerMask",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The index of the layer named `layerName` in the project's tag manager, or -1 when no layer has
/// that name. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc LayerMask_NameToLayer{
    .key = "LayerMask.NameToLayer", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "LayerMask",
    .name = "NameToLayer", .type = "int", .params = LayerMask_string};

}  // namespace pm::map
