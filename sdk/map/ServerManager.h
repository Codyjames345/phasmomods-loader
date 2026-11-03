#pragma once

// The lobby board: ServerManager, which runs the lobby's Ready button and lists the players' ID cards,
// and the cards themselves (MultiplayerProfile, a ProfileCard). Their names and ServerManager's Ready
// fields are real; the card list and the card's player spot are obfuscated, so they are found by their
// types. Notes read from decompiled code are from build 25811160.

#include "UnityRendering.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The lobby's manager (Assembly-CSharp, global namespace), a MonoBehaviourPunCallbacks: the Ready
/// button and the players' ID cards. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc ServerManager{.key = "ServerManager", .name = "ServerManager",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// The lobby's Ready button, a Toggle. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc ServerManager_readyToggle{.key = "ServerManager.readyToggle",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .owner = "ServerManager", .name = "readyToggle"};

/// The Ready button's label, a TextMeshProUGUI beside the button. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc ServerManager_readyText{.key = "ServerManager.readyText",
                                                            .kind = pm::sdk::Kind::Field, .owner = "ServerManager",
                                                            .name = "readyText"};

/// The Ready label's colour while the button can be clicked, a Color; ServerManager greys the label
/// otherwise. Optional: without it a copy keeps the label's own colour. Inferred: from the field's name.
inline constexpr pm::sdk::EntryDesc ServerManager_enabledColour{
    .key = "ServerManager.enabledColour", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kOptional,
    .owner = "ServerManager", .name = "enabledColour", .type = "Color"};

/// A player's lobby ID card (Assembly-CSharp, global namespace), a ProfileCard. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc MultiplayerProfile{.key = "MultiplayerProfile", .name = "MultiplayerProfile",
                                                       .assembly = "Assembly-CSharp", .ns = ""};

/// The lobby's ID cards, a List<MultiplayerProfile>: ServerManager's one list of them. Inferred: from
/// the class dump.
inline constexpr pm::sdk::EntryDesc ServerManager_cards{.key = "ServerManager.cards", .kind = pm::sdk::Kind::Field,
                                                        .owner = "ServerManager",
                                                        .type = "List<@MultiplayerProfile>"};

/// A player's card (Assembly-CSharp, global namespace), a MonoBehaviourPun: the lobby's ID card and
/// the others. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc ProfileCard{.key = "ProfileCard", .name = "ProfileCard",
                                                .assembly = "Assembly-CSharp", .ns = ""};

/// The room's player the card shows, a PlayerSpot, or null for a card with no player: ProfileCard's
/// one PlayerSpot field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc ProfileCard_spot{.key = "ProfileCard.spot", .kind = pm::sdk::Kind::Field,
                                                     .owner = "ProfileCard",
                                                     .type = "@PlayerSpot"};

/// The lobby's own Leave: takes this client's equipment out of the lobby and leaves the room through the game's own
/// steps. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_LeaveServer{.key = "ServerManager.LeaveServer",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "ServerManager", .name = "LeaveServer"};

inline constexpr const char* ServerManager_KickPlayer_params[] = {"@MultiplayerProfile"};

/// The lobby's kick of the player a card shows. The game's method returns without a word unless the card allows a kick (a
/// flag on the card whose writer was not found) and this client is the room's master (PhotonNetwork.get_IsMasterClient).
/// Otherwise it adds the player's id to the lobby's ban list, sends that list's RPC to every client, and sends the player
/// the kick's RPC. Called again it does the same again, and the list gains the id again. So returning proves nothing: see
/// the player leave the room. The player it removes runs ServerManager.KickPlayerNetworked, which refuses a sender that is
/// not the room's first actor. It works in the lobby only: both ends need the ServerManager's view, and a contract's load replaces
/// its scene (sdk/systems/photon.md). Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_KickPlayer{
    .key = "ServerManager.KickPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "KickPlayer", .params = ServerManager_KickPlayer_params};

/// The kick as the removed player receives it, an RPC. It refuses a sender that is not the room's first actor, the lowest
/// actor number (sdk/systems/photon.md), and flags that sender. Otherwise it puts the game's kicked message in the game's
/// error popup, adds this room to the rooms this client was kicked from, and leaves through ServerManager.LeaveServer.
/// Never call it: a kick is ServerManager.KickPlayer on the master. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_KickPlayerNetworked{
    .key = "ServerManager.KickPlayerNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ServerManager", .name = "KickPlayerNetworked"};

/// A contract's start on every client, an RPC. It records the level values, on the master unlists and closes the room for
/// the contract (PhotonRoom.set_IsVisible, PhotonRoom.set_IsOpen), and stops Photon's queue for the load
/// (PhotonNetwork.get_IsMessageQueueRunning reads false from here on). The lobby's scene and this manager are still up
/// until the level activates. Never call it. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_LoadScene{
    .key = "ServerManager.LoadScene", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "LoadScene"};

/// The lobby's setup as the main menu comes back from a contract, from the main menu's start coroutine while still in the
/// room. That coroutine then lists the room again for the master, if this client created it public. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_InitBackFromGame{
    .key = "ServerManager.InitBackFromGame", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ServerManager", .name = "InitBackFromGame"};

/// The lobby's Start button, a Button: the host's way into a contract. Its click runs a listener the compiler made that
/// inlines ServerManager.StartGame, so that method's own code never runs for a click. Online the game lights it only once
/// every player is ready (ServerManager.NetworkedReady); offline ServerManager.UpdateUI lights it once a contract is voted.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_startGameButton{
    .key = "ServerManager.startGameButton", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "startGameButton"};

/// A contract's start on the master: picks the voted contract and the weather, then sends ServerManager.LoadScene to every
/// client through the server, buffered. Nothing calls it: a click on ServerManager.startGameButton runs an inlined copy, and
/// a hook here sees nothing (sdk/systems/photon.md). Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_StartGame{
    .key = "ServerManager.StartGame", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "StartGame"};

/// This client's click on the Ready toggle: sends ServerManager.NetworkedReady to every client through the server, buffered.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_Ready{
    .key = "ServerManager.Ready", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "Ready"};

/// A player's Ready as every client runs it, an RPC. It writes PlayerSpot.playerReady for the sender. On the master it then
/// sets ServerManager.startGameButton's interactable: on only when every Network.playerSpots entry is ready. This is the only
/// place online that lights the button. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_NetworkedReady{
    .key = "ServerManager.NetworkedReady", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "NetworkedReady"};

/// The lobby's Ready and Start state. With its argument true it clears every player's PlayerSpot.playerReady and switches
/// the Start button and the Ready toggle off, then, once a contract is voted, on again as this mode asks: online the Ready
/// toggle, offline the Start button. A player joining or leaving runs it with true, so everyone has to ready again.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_UpdateUI{
    .key = "ServerManager.UpdateUI", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "UpdateUI"};

/// A joiner's spot and card, a coroutine LobbyController.OnPlayerEnteredRoomEvent starts on every client. On the master it
/// first sends the joiner a kick if their id is on the lobby's ban list. It adds their Network.playerSpots entry, waits for a cloud
/// check of their name (with a deadline), then gives them a card and runs ServerManager.UpdateUI with true. So a player is in
/// the room a while before the lobby shows them. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_AssignNewPlayerSpot{
    .key = "ServerManager.AssignNewPlayerSpot", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "AssignNewPlayerSpot"};

/// A leaver's card comes off, and the rest close up, on every client; ends by running ServerManager.UpdateUI's reset, so
/// everyone has to ready again. Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_RemovePlayerCard{
    .key = "ServerManager.RemovePlayerCard", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "RemovePlayerCard"};

/// The room browser's controller (Assembly-CSharp, global namespace), on the main menu's Main Controller. Decompiled.
inline constexpr pm::sdk::EntryDesc LobbyController{.key = "LobbyController",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "LobbyController", .assembly = "Assembly-CSharp", .ns = ""};

/// Creates a room from the Create Lobby paper's room options, and keeps whether they list it, for the master's return from
/// a contract. A player's choice of a public or private room reaches a room only here (sdk/systems/photon.md). Decompiled.
inline constexpr pm::sdk::EntryDesc LobbyController_CreateServer{
    .key = "LobbyController.CreateServer", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LobbyController", .name = "CreateServer"};

}  // namespace pm::map

// The game API for the lobby board (sdk/api/game_api.h).
// Every member is called on Unity's main thread. One that did not resolve in the running game
// build logs "<key> is not available in this game build" once and returns its type's default
// without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// The lobby's manager. `manager` below is the ServerManager.
struct ServerManager {
  /// The type: type_object() for Object.FindObjectOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::ServerManager> type{};
  /// The Ready button, a Toggle: readyToggle.get(manager). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ServerManager_readyToggle, void*> readyToggle{};
  /// The Ready label: readyText.get(manager). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ServerManager_readyText, void*> readyText{};
  /// The Ready label's clickable colour: enabledColour.get(manager). Optional; check it with
  /// pm::api::all_resolved. Unresolved: all zero.
  static constexpr pm::api::Field<pm::map::ServerManager_enabledColour, Color> enabledColour{};
  /// The ID cards, a List<MultiplayerProfile>: cards.get(manager), read as a pm::api::List, or null
  /// before the lobby lists any. Unresolved: null.
  static constexpr pm::api::Field<pm::map::ServerManager_cards, void*> cards{};
  /// LeaveServer(manager): the lobby's own Leave. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ServerManager_LeaveServer, void(void*)> LeaveServer{};
  /// KickPlayer(manager, card): the lobby's kick; the game itself refuses it on a client that is not the master.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ServerManager_KickPlayer, void(void*, void*)> KickPlayer{};
  /// The Start button, a Button: startGameButton.get(manager). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ServerManager_startGameButton, void*> startGameButton{};
  /// A contract's start: hook nothing here for a click, which runs an inlined copy. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ServerManager_StartGame, void*(void*)> StartGame{};
  /// Ready(manager, ready): this client's Ready, sent to everyone. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ServerManager_Ready, void(void*, bool)> Ready{};
  /// UpdateUI(manager, reset): refreshes the lobby's Ready and Start state, clearing every player's Ready when `reset`.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ServerManager_UpdateUI, void(void*, bool)> UpdateUI{};
};

/// A player's lobby ID card.
struct MultiplayerProfile {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::MultiplayerProfile> type{};
};

/// A player's card. `card` below is the ProfileCard.
struct ProfileCard {
  /// The room's player it shows, a PlayerSpot: spot.get(card), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::ProfileCard_spot, void*> spot{};
};

/// @}
}  // namespace phasmo
