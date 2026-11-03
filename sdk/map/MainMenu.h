#pragma once

// The main menu's board: ConnectToServer, which runs its buttons and decoration, the game's error popup, and whether
// the camera is on a board. The host copies the popup and the News tape for its own (host/menu_panel.h). The menu
// objects' class and the fields ConnectToServer and ErrorManager keep them in are obfuscated, so they are found by
// their types. The menu's taped buttons are Doozy's (sdk/map/Doozy.h). Notes read from decompiled code are from build
// 25811160.

#include "UnityRendering.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The main menu (Assembly-CSharp, global namespace), a MonoBehaviour on the menu's board. Decompiled.
inline constexpr pm::sdk::EntryDesc ConnectToServer{.key = "ConnectToServer", .name = "ConnectToServer",
                                                    .assembly = "Assembly-CSharp", .ns = ""};

/// The main menu in this scene, or null. Static. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc ConnectToServer_get_Instance{
    .key = "ConnectToServer.get_Instance", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "ConnectToServer", .name = "get_Instance"};

/// The board's decoration, a List<GameObject>: what ConnectToServer.EnableMenuDecor switches. Decompiled.
inline constexpr pm::sdk::EntryDesc ConnectToServer_menuDecor{
    .key = "ConnectToServer.menuDecor", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ConnectToServer", .type = "List<GameObject>"};

inline constexpr const char* MenuObjects_fieldTypes[] = {"GameObject[]", "GameObject", "GameObject", "GameObject",
                                                         "Boolean[]", "bool"};

/// The menu's buttons and the reasons they are off, a class of ConnectToServer's (Assembly-CSharp, name obfuscated).
/// ConnectToServer.TurnMenuObjects records one reason in its bool[] and switches the buttons from all of them. Decompiled.
inline constexpr pm::sdk::EntryDesc MenuObjects{.key = "MenuObjects", .confidence = pm::sdk::Confidence::Decompiled,
                                                .flags = pm::sdk::kExactFields | pm::sdk::kFieldOrder,
                                                .assembly = "Assembly-CSharp",
                                                .fieldTypes = MenuObjects_fieldTypes};

/// ConnectToServer's menu objects. Decompiled.
inline constexpr pm::sdk::EntryDesc ConnectToServer_menuObjects{
    .key = "ConnectToServer.menuObjects", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ConnectToServer", .type = "@MenuObjects"};

/// Whether every reason says on, so the game shows the menu's buttons: the class's one bool, which
/// ConnectToServer.TurnMenuObjects stores. Decompiled.
inline constexpr pm::sdk::EntryDesc MenuObjects_enabled{.key = "MenuObjects.enabled", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "MenuObjects", .type = "bool"};

/// The game's error popup (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc ErrorManager{.key = "ErrorManager", .name = "ErrorManager",
                                                 .assembly = "Assembly-CSharp", .ns = ""};

/// The popup it shows and hides, a GameObject: its one GameObject field. Inferred: from the class dump and the scene.
inline constexpr pm::sdk::EntryDesc ErrorManager_errorParent{.key = "ErrorManager.errorParent",
                                                             .kind = pm::sdk::Kind::Field, .owner = "ErrorManager",
                                                             .type = "GameObject"};

/// Shows the popup with the next message of the game's error queue. The queue's own class, whose name is obfuscated, shows
/// it the same way as a message is added, on any paper of the corkboard (sdk/systems/menus.md). Decompiled.
inline constexpr pm::sdk::EntryDesc ErrorManager_ShowErrors{
    .key = "ErrorManager.ShowErrors", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ErrorManager", .name = "ShowErrors"};

/// The popup's button: moves the game's error queue on, and hides the popup when the queue is empty. Decompiled.
inline constexpr pm::sdk::EntryDesc ErrorManager_NextMessageButton{
    .key = "ErrorManager.NextMessageButton", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ErrorManager", .name = "NextMessageButton"};

/// A board the camera can look at (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc CameraFocusUI{.key = "CameraFocusUI", .name = "CameraFocusUI",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// Whether the camera is on this board now. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc CameraFocusUI_get_isFocused{.key = "CameraFocusUI.get_isFocused",
                                                                .kind = pm::sdk::Kind::Method, .owner = "CameraFocusUI",
                                                                .name = "get_isFocused"};

}  // namespace pm::map

// The game API for the main menu (sdk/api/game_api.h). Every member is called on Unity's main thread. One that did not
// resolve in the running game build logs "<key> is not available in this game build" once and returns its type's default
// without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// The main menu. `menu` below is the ConnectToServer.
struct ConnectToServer {
  /// get_Instance(): the main menu in this scene, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::ConnectToServer_get_Instance, void*()> get_Instance{};
  /// The board's decoration, a List<GameObject>: menuDecor.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ConnectToServer_menuDecor, void*> menuDecor{};
  /// The menu's buttons and their reasons: menuObjects.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ConnectToServer_menuObjects, void*> menuObjects{};
};

/// The menu's buttons and the reasons they are off. `objects` below is one.
struct MenuObjects {
  /// Whether the game shows the buttons: enabled.get(objects). Unresolved: false.
  static constexpr pm::api::Field<pm::map::MenuObjects_enabled, bool> enabled{};
};

/// The game's error popup. `manager` below is the ErrorManager.
struct ErrorManager {
  /// The type: type_object() for Object.FindObjectOfType, is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::ErrorManager> type{};
  /// The popup, a GameObject: errorParent.get(manager). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ErrorManager_errorParent, void*> errorParent{};
};

/// A board the camera can look at. `board` below is the CameraFocusUI.
struct CameraFocusUI {
  /// The type: type_object() for GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::CameraFocusUI> type{};
  /// get_isFocused(board). Unresolved: false.
  static constexpr pm::api::Method<pm::map::CameraFocusUI_get_isFocused, bool(void*)> get_isFocused{};
};

/// @}
}  // namespace phasmo
