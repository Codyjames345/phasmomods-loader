#pragma once

// Scene and asset facts: GameObject paths, layers and Animator parameters, which live in the game's
// scene and asset files rather than in its code. The offline check reads them from those files
// (tools/assetcheck.py); the running game checks a path when a mod looks it up, a layer when the host
// activates and a parameter when a mod first asks for it. sdk/systems/scene.md says which files hold
// what and how they are read. Facts read from the files are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The journal's book: the pages and tabs under Journal2.0Canvas, the object that carries the
/// JournalController (get it with the controller's transform). Under it are the evidence pages
/// ("Evidence.1", "Evidence.2/All Ghosts"), the ghost pages and "Ghost Select Page". Read from the
/// PCPlayer prefab's files with assetcheck; VRPlayer (OpenXR) and OldPCPlayer have it as well, and
/// OldVRPlayer's journal lacks it. Decompiled: read from the asset files, not seen in the game.
inline constexpr pm::sdk::EntryDesc Scene_JournalBookContent{
    .key = "Scene.JournalBookContent", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent", .component = "JournalController"};

/// The evidence spread's ghost page's grid of guess buttons, one GhostButton each, alphabetical, laid
/// out by its GridLayoutGroup. Read from the PCPlayer prefab's files with assetcheck. Decompiled: read
/// from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostGrid{
    .key = "Scene.JournalGhostGrid", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.2/All Ghosts",
    .component = "JournalController"};

/// The Spirit's guess button in that grid: the one the host clones for a custom ghost. It holds a
/// TextLocaliserUI for its name and the other players' marks. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostButton{
    .key = "Scene.JournalGhostButton", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.2/All Ghosts/Spirit",
    .component = "JournalController"};

/// The Ghosts tab's list of every ghost: its entries (a Button and a TextLocaliserUI each) are placed
/// by hand, alphabetical, and its GridLayoutGroup ships switched off. Decompiled: read from the asset
/// files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostList{
    .key = "Scene.JournalGhostList", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Ghost Select Page/Ghost Types List",
    .component = "JournalController"};

/// The Spirit's entry in that list: the one the host clones. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostEntry{
    .key = "Scene.JournalGhostEntry", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer",
    .path = "Content/GhostHuntingBookContent/Ghost Select Page/Ghost Types List/Spirit",
    .component = "JournalController"};

/// The Home tab's first page's contents: a row per section of the book, then a row per ghost from the
/// first alphabetically, placed by a VerticalLayoutGroup that doesn't size them. A row holds a
/// TextLocaliserUI for its name and one child, a TextMeshProUGUI with the page number, typed into the
/// prefab; both texts size themselves to the row. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalContentsRows{
    .key = "Scene.JournalContentsRows", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Contents A/Contents Group",
    .component = "JournalController"};

/// The Home tab's second page's contents: the rest of the ghosts, rows as on the first. Decompiled: read
/// from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalContentsGhostRows{
    .key = "Scene.JournalContentsGhostRows", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Contents B/Ghost Types",
    .component = "JournalController"};

/// The Spirit's row on that page: the one the host clones. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalContentsGhostRow{
    .key = "Scene.JournalContentsGhostRow", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Contents B/Ghost Types/Spirit",
    .component = "JournalController"};

/// The Spirit's page, one of the book's pages (the journal's pages array): the one the host clones.
/// Its children below each carry a TextLocaliserUI; every ghost's page has them at the same places.
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPage{
    .key = "Scene.JournalGhostPage", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Spirit", .component = "JournalController"};

/// The page's title: the ghost's name key. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPageTitle{
    .key = "Scene.JournalGhostPageTitle", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Spirit/Title",
    .component = "JournalController"};

/// The page's description: one text, the ghost's description with its strength and weakness.
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPageDescription{
    .key = "Scene.JournalGhostPageDescription", .kind = pm::sdk::Kind::Scene,
    .confidence = pm::sdk::Confidence::Decompiled, .scene = "Resources/PCPlayer",
    .path = "Content/GhostHuntingBookContent/Spirit/Description", .component = "JournalController"};

/// The page's first evidence. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPageEvidence1{
    .key = "Scene.JournalGhostPageEvidence1", .kind = pm::sdk::Kind::Scene,
    .confidence = pm::sdk::Confidence::Decompiled, .scene = "Resources/PCPlayer",
    .path = "Content/GhostHuntingBookContent/Spirit/Evidence 1", .component = "JournalController"};

/// The page's second evidence (the object's name ends in a space). Decompiled: read from the asset
/// files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPageEvidence2{
    .key = "Scene.JournalGhostPageEvidence2", .kind = pm::sdk::Kind::Scene,
    .confidence = pm::sdk::Confidence::Decompiled, .scene = "Resources/PCPlayer",
    .path = "Content/GhostHuntingBookContent/Spirit/Evidence 2 ", .component = "JournalController"};

/// The page's third evidence. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostPageEvidence3{
    .key = "Scene.JournalGhostPageEvidence3", .kind = pm::sdk::Kind::Scene,
    .confidence = pm::sdk::Confidence::Decompiled, .scene = "Resources/PCPlayer",
    .path = "Content/GhostHuntingBookContent/Spirit/Evidence 3", .component = "JournalController"};

/// The evidence spread's evidence page's rows: one EvidenceButton each, laid out in a column by its
/// VerticalLayoutGroup, all seven active, in the order EMF, DOTS, Fingerprints, Freezing, Ghost Orbs,
/// Ghost Writing, Spirit Box. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalEvidenceRows{
    .key = "Scene.JournalEvidenceRows", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.1/Layout Group",
    .component = "JournalController"};

/// The Spirit Box's row in them: the one the host clones for a custom evidence. Its children are the
/// other players' marks, a box, the name (ThreeStateButton.textLabel), the line under it
/// (EvidenceButton.descriptionLabel), a strike and an image named "Monkey Paw Scratched Image".
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalEvidenceButton{
    .key = "Scene.JournalEvidenceButton", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.1/Layout Group/Spirit Box",
    .component = "JournalController"};

/// The evidence page, the left of the first spread. Its rows are EvidenceRows. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalEvidencePage{
    .key = "Scene.JournalEvidencePage", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.1",
    .component = "JournalController"};

/// The evidence page's heading. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalEvidenceTitle{
    .key = "Scene.JournalEvidenceTitle", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.1/Title",
    .component = "JournalController"};

/// The page of the ghost guess, the first spread's right. Its grid is GhostGrid. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostsPage{
    .key = "Scene.JournalGhostsPage", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.2",
    .component = "JournalController"};

/// That page's heading. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostsTitle{
    .key = "Scene.JournalGhostsTitle", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.2/Title",
    .component = "JournalController"};

/// That page's line above the ghost it is decided to be. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalGhostsLine{
    .key = "Scene.JournalGhostsLine", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Evidence.2/Weve decided its a :",
    .component = "JournalController"};

/// The book's Home tab. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalHomeTab{
    .key = "Scene.JournalHomeTab", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Home Tab",
    .component = "JournalController"};

/// The contents pages, left. Each page near x 0 in the book. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalContentsA{
    .key = "Scene.JournalContentsA", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Contents A",
    .component = "JournalController"};

/// The contents pages, right. Each right page near x 948 in the book. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalContentsB{
    .key = "Scene.JournalContentsB", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Contents B",
    .component = "JournalController"};

/// The payments page, a left page. Its heading, rule and paragraph are the book's page style. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalPayments{
    .key = "Scene.JournalPayments", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Payments",
    .component = "JournalController"};

/// That page's heading, a text to copy for a page in the book's style. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalPaymentsTitle{
    .key = "Scene.JournalPaymentsTitle", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Payments/Title",
    .component = "JournalController"};

/// That page's rule under the heading. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalPaymentsRule{
    .key = "Scene.JournalPaymentsRule", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Payments/------------ (2)",
    .component = "JournalController"};

/// That page's paragraph, a text to copy for a page in the book's style. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalPaymentsBody{
    .key = "Scene.JournalPaymentsBody", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Payments/Text (1)",
    .component = "JournalController"};

/// The first page of the cursed possessions, a right page. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalCursedPossessions{
    .key = "Scene.JournalCursedPossessions", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Cursed Possessions A",
    .component = "JournalController"};

/// The overview page, a left page. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalOverview{
    .key = "Scene.JournalOverview", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Overview",
    .component = "JournalController"};

/// The page on capturing media, a right page. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalCapturingMedia{
    .key = "Scene.JournalCapturingMedia", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Content/GhostHuntingBookContent/Capturing Media",
    .component = "JournalController"};

/// The journal's 3D book, under the Animator that opens and closes it, with the canvas beside it (from the rig's Transform).
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Scene_JournalModel{
    .key = "Scene.JournalModel", .kind = pm::sdk::Kind::Scene, .confidence = pm::sdk::Confidence::Decompiled,
    .scene = "Resources/PCPlayer", .path = "Journal_anim/Journal_geo", .component = "JournalRigController"};

/// Whether the journal's book is open: the BookController's bool JournalRigController's Animator is set by.
/// Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc AnimatorParam_JournalOpened{
    .key = "AnimatorParam.JournalOpened", .kind = pm::sdk::Kind::AnimatorParam,
    .confidence = pm::sdk::Confidence::Decompiled, .name = "isOpened", .scene = "Resources/PCPlayer",
    .component = "BookController"};

/// The layer the game puts the open journal's canvas and tabs on while the journal is open. Index 11
/// in the tag manager. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_PlayerView{.key = "Layer.PlayerView", .kind = pm::sdk::Kind::Layer,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .name = "PlayerView"};

/// A layer named LaserGhost in the tag manager. Ghost Tells' Goryo puts its camera-only figure on it,
/// as the approved design names it the camera-only render layer
/// (docs/superpowers/specs/2026-10-21-ghost-tells-design.md). Inferred: from the layer's name and that
/// design's RE.
inline constexpr pm::sdk::EntryDesc Layer_LaserGhost{.key = "Layer.LaserGhost", .kind = pm::sdk::Kind::Layer,
                                                     .name = "LaserGhost"};

/// Unity's built-in UI layer: the closed journal's pages canvas is on it, and the investigator's
/// LocalJournalController puts the canvas back on it as the book closes. Inferred: Unity's built-in
/// layer, and the layer the journal's canvas leaves the PlayerView layer for.
inline constexpr pm::sdk::EntryDesc Layer_UI{.key = "Layer.UI", .kind = pm::sdk::Kind::Layer, .name = "UI"};

// The ground a living player stands on: of the layers the game's physics matrix lets a living player (AlivePlayer)
// collide with, those a floor, a stair or a prop is on rather than a wall, a door, a camera or an invisible wall.
// Decompiled: the layer names and the physics matrix read from the asset files.

/// Unity's Default layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Default{.key = "Layer.Default", .kind = pm::sdk::Kind::Layer,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .name = "Default"};
/// The game's Floor layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Floor{.key = "Layer.Floor", .kind = pm::sdk::Kind::Layer,
                                                .confidence = pm::sdk::Confidence::Decompiled, .name = "Floor"};
/// The game's DefaultNoNavigation layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_DefaultNoNavigation{.key = "Layer.DefaultNoNavigation",
                                                              .kind = pm::sdk::Kind::Layer,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .name = "DefaultNoNavigation"};
/// The game's StaticProps layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_StaticProps{.key = "Layer.StaticProps", .kind = pm::sdk::Kind::Layer,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .name = "StaticProps"};

// What a body bumps into: the furniture layers (DefaultProps, Cabinets) and the walls and doors (Wall, Door), of the
// layers the game's physics matrix lets a living player collide with. Decompiled: the layer names and the physics
// matrix read from the asset files.

/// The game's DefaultProps layer: props a player collides with. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_DefaultProps{.key = "Layer.DefaultProps", .kind = pm::sdk::Kind::Layer,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "DefaultProps"};
/// The game's Item layer: the equipment players carry, place and drop (on Willow Street the thermometer, tripod,
/// crucifix and photo camera bodies, among 252 objects). Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Item{.key = "Layer.Item", .kind = pm::sdk::Kind::Layer,
                                               .confidence = pm::sdk::Confidence::Decompiled, .name = "Item"};
/// The game's Cabinets layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Cabinets{.key = "Layer.Cabinets", .kind = pm::sdk::Kind::Layer,
                                                   .confidence = pm::sdk::Confidence::Decompiled, .name = "Cabinets"};
/// The game's Wall layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Wall{.key = "Layer.Wall", .kind = pm::sdk::Kind::Layer,
                                               .confidence = pm::sdk::Confidence::Decompiled, .name = "Wall"};
/// The game's Door layer. Decompiled: read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_Door{.key = "Layer.Door", .kind = pm::sdk::Kind::Layer,
                                               .confidence = pm::sdk::Confidence::Decompiled, .name = "Door"};

/// Unity's built-in Ignore Raycast layer. The game's physics matrix lets it collide only with a layer the game leaves
/// unnamed, so no body or character controller of the game's bumps into a collider on it; Cloth still collides with
/// the colliders it is given, whatever their layer, so a mod puts a cloth's colliders here (sdk/api/models.h). The
/// matrix does not govern queries: Unity's default ray mask leaves this layer out, but a ray or overlap query of the
/// game's with a mask that includes it (all layers, say) can still hit such a collider, and the game's own masks have
/// not been audited. Decompiled: the layer's name and the physics matrix read from the asset files.
inline constexpr pm::sdk::EntryDesc Layer_IgnoreRaycast{.key = "Layer.IgnoreRaycast", .kind = pm::sdk::Kind::Layer,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .name = "Ignore Raycast"};

}  // namespace pm::map

/// @file
/// @brief The game API for the scene facts (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread while the game plays. One that did not resolve logs "<key> is not
/// available in this game build" once; a path missing in the running scene is logged once per scene load. Either
/// way the lookup answers null or -1 and touches nothing.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// The journal: `canvas` below is the Transform of the object that carries the JournalController.
struct Journal {
  /// The evidence page, the left of the first spread. Its rows are EvidenceRows. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalEvidencePage> EvidencePage{};
  /// The evidence page's heading. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalEvidenceTitle> EvidenceTitle{};
  /// The page of the ghost guess, the first spread's right. Its grid is GhostGrid. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostsPage> GhostsPage{};
  /// That page's heading. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostsTitle> GhostsTitle{};
  /// That page's line above the ghost it is decided to be. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostsLine> GhostsLine{};
  /// The book's Home tab. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalHomeTab> HomeTab{};
  /// The contents pages, left. Each page near x 0 in the book. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalContentsA> ContentsA{};
  /// The contents pages, right. Each right page near x 948 in the book. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalContentsB> ContentsB{};
  /// The payments page, a left page. Its heading, rule and paragraph are the book's page style. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalPayments> Payments{};
  /// That page's heading, a text to copy for a page in the book's style. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalPaymentsTitle> PaymentsTitle{};
  /// That page's rule under the heading. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalPaymentsRule> PaymentsRule{};
  /// That page's paragraph, a text to copy for a page in the book's style. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalPaymentsBody> PaymentsBody{};
  /// The first page of the cursed possessions, a right page. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalCursedPossessions> CursedPossessions{};
  /// The overview page, a left page. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalOverview> Overview{};
  /// The page on capturing media, a right page. find(canvas).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalCapturingMedia> CapturingMedia{};
  /// The 3D book: find(the rig's Transform).
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalModel> Model{};
  /// Whether the book is open, for the Animator's by-id setter: Opened.hash(animator), on JournalRigController's Animator.
  static constexpr pm::api::AnimatorParam<pm::map::AnimatorParam_JournalOpened> Opened{};
  /// The book's pages and tabs: BookContent.find(canvas). Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalBookContent> BookContent{};
  /// The evidence spread's grid of guess buttons. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostGrid> GhostGrid{};
  /// The Spirit's guess button in it. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostButton> GhostButton{};
  /// The Ghosts tab's list. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostList> GhostList{};
  /// The Spirit's entry in it. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostEntry> GhostEntry{};
  /// The Home tab's first page's contents. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalContentsRows> ContentsRows{};
  /// The Home tab's second page's contents. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalContentsGhostRows> ContentsGhostRows{};
  /// The Spirit's row on the second. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalContentsGhostRow> ContentsGhostRow{};
  /// The Spirit's page. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPage> GhostPage{};
  /// That page's title text. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPageTitle> GhostPageTitle{};
  /// That page's description text. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPageDescription> GhostPageDescription{};
  /// That page's first evidence text. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPageEvidence1> GhostPageEvidence1{};
  /// That page's second evidence text. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPageEvidence2> GhostPageEvidence2{};
  /// That page's third evidence text. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalGhostPageEvidence3> GhostPageEvidence3{};
  /// The evidence page's rows. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalEvidenceRows> EvidenceRows{};
  /// The Spirit Box's row in them. Null when it is not there.
  static constexpr pm::api::SceneObject<pm::map::Scene_JournalEvidenceButton> EvidenceButton{};
};

/// Layers by name.
struct Layers {
  /// The open journal's layer: PlayerView.index(), -1 when it did not resolve.
  static constexpr pm::api::Layer<pm::map::Layer_PlayerView> PlayerView{};
  /// Unity's UI layer, where the closed journal's canvas sits: UI.index(), -1 when it did not resolve.
  static constexpr pm::api::Layer<pm::map::Layer_UI> UI{};
  /// The ground a living player stands on (Layer_Default's note in the map). Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_Default> Default{};
  static constexpr pm::api::Layer<pm::map::Layer_Floor> Floor{};  ///< the game's Floor layer, part of that ground. Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_DefaultNoNavigation> DefaultNoNavigation{};  ///< the game's DefaultNoNavigation layer, part of that ground. Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_StaticProps> StaticProps{};  ///< the game's StaticProps layer, part of that ground. Unresolved: -1.
  /// What a body bumps into (Layer_DefaultProps's note in the map). Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_DefaultProps> DefaultProps{};
  static constexpr pm::api::Layer<pm::map::Layer_Cabinets> Cabinets{};  ///< the game's Cabinets layer. Unresolved: -1.
  /// The equipment players carry, place and drop (Layer_Item's note in the map). Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_Item> Item{};
  static constexpr pm::api::Layer<pm::map::Layer_Wall> Wall{};  ///< the game's Wall layer. Unresolved: -1.
  static constexpr pm::api::Layer<pm::map::Layer_Door> Door{};  ///< the game's Door layer. Unresolved: -1.
  /// The LaserGhost layer: LaserGhost.index(), -1 when it did not resolve.
  static constexpr pm::api::Layer<pm::map::Layer_LaserGhost> LaserGhost{};
  /// The Ignore Raycast layer, which collides with nothing of the game's: IgnoreRaycast.index(), -1 when it did not
  /// resolve.
  static constexpr pm::api::Layer<pm::map::Layer_IgnoreRaycast> IgnoreRaycast{};
};

/// @}
}  // namespace phasmo
