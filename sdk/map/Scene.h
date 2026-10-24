#pragma once

// Scene and asset facts: GameObject paths, layers and Animator parameters, which live in the game's
// scene and asset files rather than in its code. The offline check reads them from those files
// (tools/assetcheck.py); the running game checks a path when a mod looks it up, a layer when the host
// activates and a parameter when a mod first asks for it. sdk/systems/scene.md says which files hold
// what and how they are read. Facts read from the files are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pg::map {

/// The journal's book: the pages and tabs under Journal2.0Canvas, the object that carries the
/// JournalController (get it with the controller's transform). Under it are the evidence pages
/// ("Evidence.1", "Evidence.2/All Ghosts"), the ghost pages and "Ghost Select Page". Read from the
/// PCPlayer prefab's files with assetcheck; VRPlayer (OpenXR) and OldPCPlayer have it as well, and
/// OldVRPlayer's journal lacks it. Decompiled: read from the asset files, not seen in the game.
inline constexpr pg::sdk::EntryDesc Scene_JournalBookContent{
    .key = "Scene.JournalBookContent", .kind = pg::sdk::Kind::Scene, .confidence = pg::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent", .component = "JournalController"};

/// The layer the game puts the open journal's canvas and tabs on while the journal is open. Index 11
/// in the tag manager. Decompiled: read from the asset files.
inline constexpr pg::sdk::EntryDesc Layer_PlayerView{.key = "Layer.PlayerView", .kind = pg::sdk::Kind::Layer,
                                                     .confidence = pg::sdk::Confidence::Decompiled,
                                                     .name = "PlayerView"};

}  // namespace pg::map

// The game API for the scene facts (sdk/api/game_api.h). Every member is called on Unity's main thread
// while the game plays. One that did not resolve logs "<key> is not available in this game build"
// once; a path missing in the running scene is logged once per scene load. Either way the lookup
// answers null or -1 and touches nothing.
namespace phasmo {

/// The journal: `canvas` below is the Transform of the object that carries the JournalController.
struct Journal {
  /// The book's pages and tabs: BookContent.find(canvas). Null when it is not there.
  static constexpr pg::api::SceneObject<pg::map::Scene_JournalBookContent> BookContent{};
};

/// Layers by name.
struct Layers {
  /// The open journal's layer: PlayerView.index(), -1 when it did not resolve.
  static constexpr pg::api::Layer<pg::map::Layer_PlayerView> PlayerView{};
};

}  // namespace phasmo
