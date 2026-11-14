#pragma once

// Photon PUN and Realtime, the game's networking: the room's master, its players' actor numbers, and
// the PhotonView every networked object carries. Not obfuscated, so every entry is by real name.
// Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "UnityObject.h"

namespace pm::map {

/// Photon's static network class (PhotonUnityNetworking, namespace Photon.Pun). Inferred: Photon PUN's
/// documented API.
inline constexpr pm::sdk::EntryDesc PhotonNetwork{.key = "PhotonNetwork", .name = "PhotonNetwork",
                                                  .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};

/// Whether this client is the room's master. A fact for DifficultyUIManager.RefreshButtons. Inferred:
/// Photon PUN's documented property getter (LevelController.SyncCurrentGhostRoom asks it before it
/// syncs the ghost's room, read in its decompiled code).
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_IsMasterClient{
    .key = "PhotonNetwork.get_IsMasterClient", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "get_IsMasterClient"};

/// The room's master client, a Photon.Realtime.Player: this client's own player in offline mode, null
/// when there is no client or no current room, else the current room's player of the master's actor
/// number. Static. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_MasterClient{
    .key = "PhotonNetwork.get_MasterClient", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "PhotonNetwork",
    .name = "get_MasterClient"};

/// The room's players, a Photon.Realtime.Player[] sorted by actor number; empty outside a room.
/// Static. Inferred: Photon PUN's documented property getter.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_PlayerList{
    .key = "PhotonNetwork.get_PlayerList", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "get_PlayerList"};

/// This client's own Photon.Realtime.Player. Static. Inferred: Photon PUN's documented property
/// getter.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_LocalPlayer{
    .key = "PhotonNetwork.get_LocalPlayer", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "get_LocalPlayer"};

/// Whether this client is in a room: its client state is Joined. Static. The game's check of an RPC's
/// sender (ExitLevel.Exit and GhostController.CreateGhost call it) lets every sender through outside a
/// room. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_InRoom{
    .key = "PhotonNetwork.get_InRoom", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "PhotonNetwork",
    .name = "get_InRoom"};

/// Whether PUN runs offline (a single-player game), the class's static flag. The game's check of an
/// RPC's sender reads that flag and lets every sender through while it is set. Static. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_OfflineMode{
    .key = "PhotonNetwork.get_OfflineMode", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "PhotonNetwork",
    .name = "get_OfflineMode"};

/// Whether Photon hands this client its incoming RPCs and events, the class's static flag. Static. The game clears it for a
/// scene load: ServerManager.LoadScene, a contract's start on every client, clears it first, while the lobby's scene is
/// still up. PhotonNetwork.NewSceneLoaded, Player.Start and the main menu set it again. While it is clear nothing arrives,
/// so what was sent then waits in order (sdk/systems/photon.md). Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_IsMessageQueueRunning{
    .key = "PhotonNetwork.get_IsMessageQueueRunning", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "PhotonNetwork",
    .name = "get_IsMessageQueueRunning"};

/// A player of a Photon room (PhotonRealtime, namespace Photon.Realtime), not the game's own Player.
/// Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc PhotonPlayer{.key = "PhotonPlayer", .name = "Player", .assembly = "PhotonRealtime",
                                                 .ns = "Photon.Realtime"};

/// The player's actor number in the room, the number the host's messages name their sender by.
/// Inferred: Photon Realtime's documented property getter.
inline constexpr pm::sdk::EntryDesc PhotonPlayer_get_ActorNumber{
    .key = "PhotonPlayer.get_ActorNumber", .kind = pm::sdk::Kind::Method, .owner = "PhotonPlayer",
    .name = "get_ActorNumber"};

/// The base class of the networked MonoBehaviours (PhotonUnityNetworking, namespace Photon.Pun); every
/// FireSource is one. Inferred: Photon PUN's documented API.
inline constexpr pm::sdk::EntryDesc MonoBehaviourPun{.key = "MonoBehaviourPun", .name = "MonoBehaviourPun",
                                                     .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun",
                                                     .base = &MonoBehaviour};

/// The PhotonView of the behaviour's GameObject: FireSource.BlowOutFire sends its RPC through it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc MonoBehaviourPun_get_photonView{
    .key = "MonoBehaviourPun.get_photonView", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "MonoBehaviourPun", .name = "get_photonView"};

/// The network identity of a GameObject (PhotonUnityNetworking, namespace Photon.Pun). Inferred:
/// Photon PUN's documented API.
inline constexpr pm::sdk::EntryDesc PhotonView{.key = "PhotonView", .name = "PhotonView",
                                               .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun",
                                               .base = &MonoBehaviour};

/// The view's id, the same number on every client of the room. Inferred: Photon PUN's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc PhotonView_get_ViewID{.key = "PhotonView.get_ViewID", .kind = pm::sdk::Kind::Method,
                                                          .owner = "PhotonView", .name = "get_ViewID"};

/// The player who owns the view, a Photon.Realtime.Player: Owner's backing field. A received RPC's
/// sender is compared with it (JournalController.PageSync, build 25811160). For a view with no owner number,
/// such as a scene object's, it is the master client (PhotonView.set_OwnerActorNr). Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonView_owner{.key = "PhotonView.owner", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "PhotonView", .name = "<Owner>k__BackingField"};

inline constexpr const char* PhotonView_set_OwnerActorNr_params[] = {"int"};

/// Sets the view's owner number and owner: it asks the current room for the player with that number with
/// PhotonRoom.GetPlayer's master lookup on, so number 0 makes the master client the owner (PhotonView.owner).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonView_set_OwnerActorNr{
    .key = "PhotonView.set_OwnerActorNr", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonView", .name = "set_OwnerActorNr", .params = PhotonView_set_OwnerActorNr_params};

/// A Photon room (PhotonRealtime, namespace Photon.Realtime). Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc PhotonRoom{.key = "PhotonRoom", .name = "Room", .assembly = "PhotonRealtime",
                                               .ns = "Photon.Realtime"};

inline constexpr const char* PhotonRoom_GetPlayer_params[] = {"int", "bool"};

/// The room's player with an actor number; with the flag set, number 0 means the master client. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonRoom_GetPlayer{
    .key = "PhotonRoom.GetPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonRoom", .name = "GetPlayer", .params = PhotonRoom_GetPlayer_params};

/// What PUN tells an RPC method about its call (PhotonUnityNetworking, namespace Photon.Pun), a struct
/// the method receives by address. Inferred: Photon PUN's documented API.
inline constexpr pm::sdk::EntryDesc PhotonMessageInfo{.key = "PhotonMessageInfo", .name = "PhotonMessageInfo",
                                                      .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};

/// The player who sent the RPC, a Photon.Realtime.Player. JournalController.PageSync compares it with
/// its view's owner (build 25811160). Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonMessageInfo_Sender{.key = "PhotonMessageInfo.Sender",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "PhotonMessageInfo", .name = "Sender"};

/// Whether this client owns the view (the local player's own, for a player's objects). Inferred: Photon
/// PUN's documented property getter; JournalController.GoToPage reads its
/// backing field to decide whether to send the page sync to every client.
inline constexpr pm::sdk::EntryDesc PhotonView_get_IsMine{.key = "PhotonView.get_IsMine", .kind = pm::sdk::Kind::Method,
                                                          .owner = "PhotonView", .name = "get_IsMine"};

/// Asks every client to make this client the view's owner; under OwnershipOption.Takeover the
/// request is granted when it comes back, and only then is IsMine set. Inferred: Photon PUN's
/// documented method.
inline constexpr pm::sdk::EntryDesc PhotonView_RequestOwnership{.key = "PhotonView.RequestOwnership",
                                                                .kind = pm::sdk::Kind::Method, .owner = "PhotonView",
                                                                .name = "RequestOwnership"};

/// How the view's observed components are sent (a ViewSynchronization number). Inferred: Photon PUN's
/// documented field.
inline constexpr pm::sdk::EntryDesc PhotonView_Synchronization{.key = "PhotonView.Synchronization",
                                                               .kind = pm::sdk::Kind::Field, .owner = "PhotonView",
                                                               .name = "Synchronization"};

/// Who may take the view over (an OwnershipOption number). Inferred: Photon PUN's documented field.
inline constexpr pm::sdk::EntryDesc PhotonView_OwnershipTransfer{.key = "PhotonView.OwnershipTransfer",
                                                                 .kind = pm::sdk::Kind::Field, .owner = "PhotonView",
                                                                 .name = "OwnershipTransfer"};

/// Disconnects this client from Photon, the game's own way (PauseMenuController's leave ends with it). It
/// does nothing once the client is fully disconnected. Static. Inferred: Photon PUN's documented method.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_Disconnect{.key = "PhotonNetwork.Disconnect",
                                                             .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
                                                             .owner = "PhotonNetwork", .name = "Disconnect"};

/// PUN's cleanup of the room this client leaves, run from PUN's handler of the client's state change while
/// it disconnects, before the client's peer disconnects and Photon's socket closes, and from
/// PhotonNetwork.set_OfflineMode. It reads the GameObject of every PhotonView PUN holds registered from a
/// network instantiation and destroys those objects: a registered view whose object Unity destroyed without
/// its OnDestroy (one never activated) makes it throw, and on a disconnect the peer then never disconnects
/// and Photon's receive thread keeps the socket open. A private static void(). The host watches it
/// (host/room_watch.h). Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_LeftRoomCleanup{
    .key = "PhotonNetwork.LeftRoomCleanup", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "PhotonNetwork",
    .name = "LeftRoomCleanup"};

/// The PhotonViews PUN holds registered, as a new PhotonView[]. Static. Inferred: Photon PUN's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_PhotonViews{
    .key = "PhotonNetwork.get_PhotonViews", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "get_PhotonViews"};

/// Photon PUN's synchronisation options. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc ViewSynchronization{.key = "ViewSynchronization", .name = "ViewSynchronization",
                                                        .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};

/// Sent unreliably, and only when it changed: what the game's own prop throw sets before it throws.
/// Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc ViewSynchronization_UnreliableOnChange{
    .key = "ViewSynchronization.UnreliableOnChange", .kind = pm::sdk::Kind::EnumValue,
    .owner = "ViewSynchronization", .name = "UnreliableOnChange"};

/// Not sent at all: the view's observed components stay on this client. Inferred: from the value's
/// name.
inline constexpr pm::sdk::EntryDesc ViewSynchronization_Off{.key = "ViewSynchronization.Off",
                                                            .kind = pm::sdk::Kind::EnumValue,
                                                            .owner = "ViewSynchronization", .name = "Off"};

/// Photon PUN's ownership options. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc OwnershipOption{.key = "OwnershipOption", .name = "OwnershipOption",
                                                    .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};

/// Anyone may take the view over. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc OwnershipOption_Takeover{.key = "OwnershipOption.Takeover",
                                                             .kind = pm::sdk::Kind::EnumValue,
                                                             .owner = "OwnershipOption", .name = "Takeover"};

/// The value get_IsMine (and get_AmController, the same code) returns: whether this client controls the
/// view. PUN sets it from the view's owner and controller, and recomputes it on ownership and master
/// changes. Inferred: from the class dump (IsMine's backing field).
inline constexpr pm::sdk::EntryDesc PhotonView_isMine{.key = "PhotonView.isMine", .kind = pm::sdk::Kind::Field,
                                                      .owner = "PhotonView", .name = "<IsMine>k__BackingField",
                                                      .type = "bool"};

inline constexpr const char* PhotonView_RPC_params[] = {"string", "RpcTarget", "object[]"};

/// Sends an RPC of the view, by its method's name, to the targets an RpcTarget names, with its
/// arguments: the overload with an RpcTarget (the other takes one player). Inferred: Photon PUN's
/// documented method.
inline constexpr pm::sdk::EntryDesc PhotonView_RPC{.key = "PhotonView.RPC", .kind = pm::sdk::Kind::Method,
                                                   .owner = "PhotonView", .name = "RPC",
                                                   .params = PhotonView_RPC_params};

inline constexpr const char* PhotonView_RPC_toPlayer_params[] = {"string", "Player", "object[]"};

/// Sends an RPC of the view to one player, never buffered. When that player is this client it runs here at once and nothing
/// goes out (PhotonNetwork's RPC runs ExecuteRpc), so it is the way to run an RPC on this client alone with the
/// game's own call. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonView_RPC_toPlayer{
    .key = "PhotonView.RPC(Player)", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonView", .name = "RPC", .params = PhotonView_RPC_toPlayer_params};

/// Who an RPC goes to (PhotonUnityNetworking, namespace Photon.Pun). A buffered value is kept by the server and replayed
/// to every later joiner: see sdk/systems/photon.md before sending anything custom. Inferred: PUN's documented enum.
inline constexpr pm::sdk::EntryDesc RpcTarget{.key = "RpcTarget", .name = "RpcTarget",
                                              .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun"};

/// Everyone, this client at once, and not kept by the server. Inferred: PUN's documented enum.
inline constexpr pm::sdk::EntryDesc RpcTarget_All{.key = "RpcTarget.All", .kind = pm::sdk::Kind::EnumValue,
                                                  .owner = "RpcTarget", .name = "All"};

/// A Photon player's name, as the game's own lists show it, chosen by that player. Inferred: Photon's documented property.
inline constexpr pm::sdk::EntryDesc PhotonPlayer_get_NickName{.key = "PhotonPlayer.get_NickName", .kind = pm::sdk::Kind::Method,
                                                              .owner = "PhotonPlayer", .name = "get_NickName"};

/// Sets how many players the room takes, for everyone. ServerManager.LoadScene sets it to the players there as it closes the
/// room. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonRoom_set_MaxPlayers{
    .key = "PhotonRoom.set_MaxPlayers", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonRoom", .name = "set_MaxPlayers"};

inline constexpr const char* PhotonNetwork_RemoveRPCs_params[] = {"Player"};

/// Removes a Photon player's buffered RPCs from the server. Network.OnPlayerLeftRoom calls it for a leaver, on the
/// master. Inferred: Photon's documented API.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_RemoveRPCs{
    .key = "PhotonNetwork.RemoveRPCs", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "RemoveRPCs", .params = PhotonNetwork_RemoveRPCs_params};

inline constexpr const char* PhotonNetwork_LocalCleanPhotonView_params[] = {"PhotonView"};

/// Removes a view from PUN's registry on this client alone. PhotonNetwork.LeftRoomCleanup reads every view left registered, so a
/// kept view whose object is destroyed without its OnDestroy must be cleaned (sdk/systems/photon.md). A public static. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_LocalCleanPhotonView{
    .key = "PhotonNetwork.LocalCleanPhotonView", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "LocalCleanPhotonView", .params = PhotonNetwork_LocalCleanPhotonView_params};

/// Whether this Photon player is this client's own rather than another client's. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonPlayer_IsLocal{
    .key = "PhotonPlayer.IsLocal", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonPlayer", .name = "IsLocal", .type = "bool"};

inline constexpr const char* PhotonNetwork_Destroy_params[] = {"GameObject"};

/// Destroys a networked object on every client. A kept copy's views that it misses stay registered, with the cleanup's
/// risk (sdk/systems/photon.md).
inline constexpr pm::sdk::EntryDesc PhotonNetwork_Destroy{
    .key = "PhotonNetwork.Destroy", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "PhotonNetwork", .name = "Destroy", .params = PhotonNetwork_Destroy_params};

/// Sets offline mode. It is PhotonNetwork.LeftRoomCleanup's second caller.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_set_OfflineMode{
    .key = "PhotonNetwork.set_OfflineMode", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "PhotonNetwork", .name = "set_OfflineMode"};

/// Photon's client changed state: it runs PhotonNetwork.LeftRoomCleanup as the client leaves a room, which is how a
/// disconnect reaches the cleanup (sdk/systems/photon.md).
inline constexpr pm::sdk::EntryDesc PhotonNetwork_OnClientStateChanged{
    .key = "PhotonNetwork.OnClientStateChanged", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "PhotonNetwork", .name = "OnClientStateChanged"};


/// The room this client is in, a PhotonRoom, or null outside one. Static. Inferred: Photon PUN's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_get_CurrentRoom{
    .key = "PhotonNetwork.get_CurrentRoom", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic,
    .owner = "PhotonNetwork", .name = "get_CurrentRoom"};

/// Whether the room is listed in the lobby's room browser (a public room). Room declares its own getter
/// beside RoomInfo's. A public room reads false from a contract's start until the lobby is back, because
/// ServerManager.LoadScene unlists every room (sdk/systems/photon.md). Inferred: Photon Realtime's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc PhotonRoom_get_IsVisible{.key = "PhotonRoom.get_IsVisible",
                                                             .kind = pm::sdk::Kind::Method, .owner = "PhotonRoom",
                                                             .name = "get_IsVisible"};

/// Lists or unlists the room in the room browser, for everyone, and keeps the new value locally. The game calls it in two
/// places, both as the master: ServerManager.LoadScene unlists the room as a contract starts, and the main menu's return
/// from a contract lists it again if this client created it public (sdk/systems/photon.md). Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonRoom_set_IsVisible{
    .key = "PhotonRoom.set_IsVisible", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonRoom", .name = "set_IsVisible"};

/// Opens or closes the room to joiners, for everyone. The game calls it where it calls PhotonRoom.set_IsVisible: closed
/// for a contract, open again in the lobby. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonRoom_set_IsOpen{
    .key = "PhotonRoom.set_IsOpen", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PhotonRoom", .name = "set_IsOpen"};

/// PUN's per-frame driver (PhotonUnityNetworking, namespace Photon.Pun), a MonoBehaviour that lives for
/// the whole process. Inferred: Photon PUN's documented API.
inline constexpr pm::sdk::EntryDesc PhotonHandler{.key = "PhotonHandler", .name = "PhotonHandler",
                                                  .assembly = "PhotonUnityNetworking", .ns = "Photon.Pun",
                                                  .base = &MonoBehaviour};

/// PUN's LateUpdate, run once a frame on the main thread in every scene, the menus included. The host hooks
/// it as its frame boundary; a mod is ticked from there and needs no hook of its own. Inferred: Unity's
/// message method on PhotonHandler.
inline constexpr pm::sdk::EntryDesc PhotonHandler_LateUpdate{.key = "PhotonHandler.LateUpdate",
                                                             .kind = pm::sdk::Kind::Method, .owner = "PhotonHandler",
                                                             .name = "LateUpdate"};

inline constexpr const char* PhotonNetwork_RaiseEvent_params[] = {"byte", "object", "RaiseEventOptions", "SendOptions"};

/// Raises a custom event with the room: the code, the content, who it goes to and how it is delivered. It refuses, sending
/// nothing and answering false, a code above 199 or a client not joined to a room. In offline mode it sends nothing and
/// raises the event on this client only when RaiseEventOptions.Receivers is not ReceiverGroup.Others. Static. Decompiled.
inline constexpr pm::sdk::EntryDesc PhotonNetwork_RaiseEvent{
    .key = "PhotonNetwork.RaiseEvent", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "PhotonNetwork", .name = "RaiseEvent", .params = PhotonNetwork_RaiseEvent_params};

/// Who a raised event goes to, and whether the server caches it (PhotonRealtime, namespace Photon.Realtime). Inferred:
/// Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc RaiseEventOptions{.key = "RaiseEventOptions", .name = "RaiseEventOptions",
                                                      .assembly = "PhotonRealtime", .ns = "Photon.Realtime"};

/// The options' constructor: sent to the room's other players, not cached. Inferred: Photon Realtime's source.
inline constexpr pm::sdk::EntryDesc RaiseEventOptions_ctor{.key = "RaiseEventOptions.ctor", .kind = pm::sdk::Kind::Method,
                                                           .owner = "RaiseEventOptions", .name = ".ctor"};

/// The actor numbers the event goes to, an int[]; when set, RaiseEventOptions.Receivers is not read. Inferred: Photon
/// Realtime's documented field.
inline constexpr pm::sdk::EntryDesc RaiseEventOptions_TargetActors{.key = "RaiseEventOptions.TargetActors",
                                                                   .kind = pm::sdk::Kind::Field,
                                                                   .owner = "RaiseEventOptions", .name = "TargetActors"};

/// The group the event goes to, a ReceiverGroup, one byte. Decompiled: PhotonNetwork.RaiseEvent reads it as a byte.
inline constexpr pm::sdk::EntryDesc RaiseEventOptions_Receivers{
    .key = "RaiseEventOptions.Receivers", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RaiseEventOptions", .name = "Receivers"};

/// The groups of a room's players (PhotonRealtime, namespace Photon.Realtime). Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc ReceiverGroup{.key = "ReceiverGroup", .name = "ReceiverGroup",
                                                  .assembly = "PhotonRealtime", .ns = "Photon.Realtime"};

/// Everyone in the room but this client. Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc ReceiverGroup_Others{.key = "ReceiverGroup.Others", .kind = pm::sdk::Kind::EnumValue,
                                                         .owner = "ReceiverGroup", .name = "Others"};

/// The room's master client alone. Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc ReceiverGroup_MasterClient{.key = "ReceiverGroup.MasterClient",
                                                               .kind = pm::sdk::Kind::EnumValue,
                                                               .owner = "ReceiverGroup", .name = "MasterClient"};

/// Everyone in the room, this client too, through the server. Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc ReceiverGroup_All{.key = "ReceiverGroup.All", .kind = pm::sdk::Kind::EnumValue,
                                                      .owner = "ReceiverGroup", .name = "All"};

/// How a message is delivered (Photon3Unity3D, namespace ExitGames.Client.Photon). Inferred: Photon's documented API.
inline constexpr pm::sdk::EntryDesc DeliveryMode{.key = "DeliveryMode", .name = "DeliveryMode",
                                                 .assembly = "Photon3Unity3D", .ns = "ExitGames.Client.Photon"};

/// May be lost, and arrives in sequence. Inferred: Photon's documented API.
inline constexpr pm::sdk::EntryDesc DeliveryMode_Unreliable{.key = "DeliveryMode.Unreliable",
                                                            .kind = pm::sdk::Kind::EnumValue, .owner = "DeliveryMode",
                                                            .name = "Unreliable"};

/// Arrives, in sequence. Inferred: Photon's documented API.
inline constexpr pm::sdk::EntryDesc DeliveryMode_Reliable{.key = "DeliveryMode.Reliable",
                                                          .kind = pm::sdk::Kind::EnumValue, .owner = "DeliveryMode",
                                                          .name = "Reliable"};

inline constexpr const char* SendOptions_fieldTypes[] = {"DeliveryMode", "bool", "byte"};

/// How PhotonNetwork.RaiseEvent delivers an event, a struct it takes by value (Photon3Unity3D, namespace
/// ExitGames.Client.Photon). Inferred: Photon's documented API. Its fields and their order are exact: they fix the
/// struct at 8 bytes, which fixes how it is passed, so a build that changes it leaves the entry unresolved.
inline constexpr pm::sdk::EntryDesc SendOptions{.key = "SendOptions", .flags = pm::sdk::kExactFields | pm::sdk::kFieldOrder,
                                                .hint = "SendOptions", .assembly = "Photon3Unity3D",
                                                .ns = "ExitGames.Client.Photon", .fieldTypes = SendOptions_fieldTypes};

/// Its delivery mode, a DeliveryMode. Inferred: Photon's documented field.
inline constexpr pm::sdk::EntryDesc SendOptions_DeliveryMode{.key = "SendOptions.DeliveryMode",
                                                             .kind = pm::sdk::Kind::Field, .owner = "SendOptions",
                                                             .name = "DeliveryMode", .type = "@DeliveryMode"};

/// A message Photon received for the client (Photon3Unity3D, namespace ExitGames.Client.Photon). Inferred: Photon's
/// documented API.
inline constexpr pm::sdk::EntryDesc EventData{.key = "EventData", .name = "EventData", .assembly = "Photon3Unity3D",
                                              .ns = "ExitGames.Client.Photon"};

/// The event's code, the one PhotonNetwork.RaiseEvent was given. Inferred: Photon's documented field.
inline constexpr pm::sdk::EntryDesc EventData_Code{.key = "EventData.Code", .kind = pm::sdk::Kind::Field,
                                                   .owner = "EventData", .name = "Code", .type = "byte"};

/// The actor number that sent the event. Inferred: Photon's documented property getter.
inline constexpr pm::sdk::EntryDesc EventData_get_Sender{.key = "EventData.get_Sender", .kind = pm::sdk::Kind::Method,
                                                         .owner = "EventData", .name = "get_Sender"};

/// What the event carries, the content PhotonNetwork.RaiseEvent was given. Inferred: Photon's documented property getter.
inline constexpr pm::sdk::EntryDesc EventData_get_CustomData{.key = "EventData.get_CustomData",
                                                             .kind = pm::sdk::Kind::Method, .owner = "EventData",
                                                             .name = "get_CustomData"};

/// Photon Realtime's client (PhotonRealtime, namespace Photon.Realtime). Inferred: Photon Realtime's documented API.
inline constexpr pm::sdk::EntryDesc LoadBalancingClient{.key = "LoadBalancingClient", .name = "LoadBalancingClient",
                                                        .assembly = "PhotonRealtime", .ns = "Photon.Realtime"};

inline constexpr const char* LoadBalancingClient_OnEvent_params[] = {"EventData"};

/// Every event the client receives comes through here, before PUN or the game sees it. The host hooks it for the suite's
/// own event code (host/net.h). Inferred: Photon Realtime's documented method.
inline constexpr pm::sdk::EntryDesc LoadBalancingClient_OnEvent{
    .key = "LoadBalancingClient.OnEvent", .kind = pm::sdk::Kind::Method, .owner = "LoadBalancingClient",
    .name = "OnEvent", .params = LoadBalancingClient_OnEvent_params};

/// .NET's byte (mscorlib, System.Byte): the element type of the byte[] an event carries. Inferred: the .NET class.
inline constexpr pm::sdk::EntryDesc Byte{.key = "Byte", .name = "Byte", .assembly = "mscorlib", .ns = "System"};

/// .NET's int (mscorlib, System.Int32): the element type of RaiseEventOptions.TargetActors. Inferred: the .NET class.
inline constexpr pm::sdk::EntryDesc Int32{.key = "Int32", .name = "Int32", .assembly = "mscorlib", .ns = "System"};

}  // namespace pm::map

/// @file
/// @brief The game API for Photon (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// Photon's static network class.
struct PhotonNetwork {
  /// get_IsMasterClient(): whether this client is the room's master. Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_IsMasterClient, bool()> get_IsMasterClient{};
  /// get_MasterClient(): the room's master, a PhotonPlayer, or null outside a room. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_MasterClient, pm::api::Object()> get_MasterClient{};
  /// get_PlayerList(): the room's players, a PhotonPlayer[], read as a pm::api::RefArray. Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_PlayerList, pm::api::Object()> get_PlayerList{};
  /// get_LocalPlayer(): this client's PhotonPlayer. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_LocalPlayer, pm::api::Object()> get_LocalPlayer{};
  /// get_InRoom(): whether this client is in a room. Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_InRoom, bool()> get_InRoom{};
  /// get_OfflineMode(): whether PUN runs offline (single player). Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_OfflineMode, bool()> get_OfflineMode{};
  /// get_IsMessageQueueRunning(): false from a contract's start until the game turns the queue on again. Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_IsMessageQueueRunning, bool()> get_IsMessageQueueRunning{};
  /// Disconnect(): this client leaves Photon, the game's own way. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_Disconnect, void()> Disconnect{};
  /// get_PhotonViews(): the registered PhotonViews, a new PhotonView[] read as a pm::api::RefArray. An
  /// element may be a view whose engine object is gone: check it with pm::api::alive. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_PhotonViews, pm::api::Object()> get_PhotonViews{};
  /// get_CurrentRoom(): the room this client is in, a PhotonRoom, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_get_CurrentRoom, pm::api::Object()> get_CurrentRoom{};
  /// RaiseEvent(code, content, options, sendOptions): raises an event with the room; `options` is a RaiseEventOptions and
  /// `sendOptions` a SendOptions, 8 bytes by value. False when PUN refuses it. Unresolved: false, nothing sent.
  static constexpr pm::api::Method<pm::map::PhotonNetwork_RaiseEvent, bool(uint8_t, pm::api::Object, pm::api::Ref<pm::map::RaiseEventOptions>, uint64_t)> RaiseEvent{};
};

/// A player of a Photon room. `player` below is the Photon.Realtime.Player.
struct PhotonPlayer {
  /// get_ActorNumber(player). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::PhotonPlayer_get_ActorNumber, int32_t(pm::api::Ref<pm::map::PhotonPlayer>)> get_ActorNumber{};
  /// get_NickName(player): the name that player chose, a managed string. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhotonPlayer_get_NickName, pm::api::Object(pm::api::Ref<pm::map::PhotonPlayer>)> get_NickName{};
};

/// A networked MonoBehaviour. `behaviour` below is one.
struct MonoBehaviourPun {
  /// get_photonView(behaviour): its PhotonView. Unresolved: null.
  static constexpr pm::api::Method<pm::map::MonoBehaviourPun_get_photonView, pm::api::Object(pm::api::Ref<pm::map::MonoBehaviourPun>)> get_photonView{};
};

/// A GameObject's network identity. `view` below is the PhotonView.
struct PhotonView {
  /// The type: klass(), is(obj), type_object() (for GetComponents). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PhotonView> type{};
  /// get_ViewID(view). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::PhotonView_get_ViewID, int32_t(pm::api::Ref<pm::map::PhotonView>)> get_ViewID{};
  /// get_IsMine(view). Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonView_get_IsMine, bool(pm::api::Ref<pm::map::PhotonView>)> get_IsMine{};
  /// The owner, a PhotonPlayer, or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonView_owner, pm::api::Object> owner{};
  /// RequestOwnership(view). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhotonView_RequestOwnership, void(pm::api::Ref<pm::map::PhotonView>)> RequestOwnership{};
  /// Its ViewSynchronization: Synchronization.get(view), .set(view, v). Unresolved: 0, writes nothing.
  static constexpr pm::api::Field<pm::map::PhotonView_Synchronization, int32_t> Synchronization{};
  /// Its OwnershipOption: OwnershipTransfer.get(view). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PhotonView_OwnershipTransfer, int32_t> OwnershipTransfer{};
  /// What get_IsMine returns: isMine.get(view), .set(view, mine). PUN may set it again on an ownership
  /// or master change. Unresolved: false, writes nothing.
  static constexpr pm::api::Field<pm::map::PhotonView_isMine, bool> isMine{};
  /// RPC(view, methodName, target, parameters): methodName a managed string, target an RpcTarget
  /// number, parameters an object[]. A mod hooks it to see the RPCs a view sends. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::PhotonView_RPC, void(pm::api::Ref<pm::map::PhotonView>, pm::api::Object, int32_t, pm::api::Object)> RPC{};
  /// RPC_toPlayer(view, methodName, player, parameters): to that player alone, never buffered; on this client when the player
  /// is this client, and then nothing goes out. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhotonView_RPC_toPlayer, void(pm::api::Ref<pm::map::PhotonView>, pm::api::Object, pm::api::Object, pm::api::Object)> RPC_toPlayer{};
};

/// Who an RPC goes to. Unresolved: 0.
namespace RpcTarget {
inline constexpr pm::api::EnumValue<pm::map::RpcTarget_All> All{};  ///< All(): the value's number in this game build
}  // namespace RpcTarget

/// Photon PUN's synchronisation options. Unresolved: 0.
namespace ViewSynchronization {
inline constexpr pm::api::EnumValue<pm::map::ViewSynchronization_UnreliableOnChange> UnreliableOnChange{};  ///< UnreliableOnChange(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::ViewSynchronization_Off> Off{};  ///< Off(): the value's number in this game build
}  // namespace ViewSynchronization

/// Photon PUN's ownership options. Unresolved: 0.
namespace OwnershipOption {
inline constexpr pm::api::EnumValue<pm::map::OwnershipOption_Takeover> Takeover{};  ///< Takeover(): the value's number in this game build
}  // namespace OwnershipOption

/// What an RPC method is told about its call, a struct it receives by address. `info` below is that
/// address.
struct PhotonMessageInfo {
  /// The sender, a PhotonPlayer, or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::PhotonMessageInfo_Sender, pm::api::Object> Sender{};
};

/// A Photon room. `room` below is the PhotonRoom.
struct PhotonRoom {
  /// get_IsVisible(room): whether the lobby's room browser lists it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::PhotonRoom_get_IsVisible, bool(pm::api::Ref<pm::map::PhotonRoom>)> get_IsVisible{};
  /// set_IsOpen(room, open): opens or closes the room to joiners. The game closes it for a contract. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhotonRoom_set_IsOpen, void(pm::api::Ref<pm::map::PhotonRoom>, bool)> set_IsOpen{};
};

/// Who a raised event goes to. `options` below is the RaiseEventOptions.
struct RaiseEventOptions {
  /// The type: make() for a new one, then ctor. Unresolved: null.
  static constexpr pm::api::Type<pm::map::RaiseEventOptions> type{};
  /// ctor(options): runs the constructor of options made with type.make(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RaiseEventOptions_ctor, void(pm::api::Ref<pm::map::RaiseEventOptions>)> ctor{};
  /// The actor numbers it goes to, an int[]: TargetActors.set(options, actors). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::RaiseEventOptions_TargetActors, pm::api::Object> TargetActors{};
  /// The group it goes to, a ReceiverGroup value: Receivers.set(options, group). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::RaiseEventOptions_Receivers, uint8_t> Receivers{};
};

/// The groups of a room's players. Unresolved: 0.
namespace ReceiverGroup {
inline constexpr pm::api::EnumValue<pm::map::ReceiverGroup_Others> Others{};  ///< Others(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::ReceiverGroup_MasterClient> MasterClient{};  ///< MasterClient(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::ReceiverGroup_All> All{};  ///< All(): the value's number in this game build
}  // namespace ReceiverGroup

/// How a message is delivered. Unresolved: 0.
namespace DeliveryMode {
inline constexpr pm::api::EnumValue<pm::map::DeliveryMode_Unreliable> Unreliable{};  ///< Unreliable(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::DeliveryMode_Reliable> Reliable{};  ///< Reliable(): the value's number in this game build
}  // namespace DeliveryMode

/// How PhotonNetwork.RaiseEvent delivers an event. `options` below is a struct of 8 bytes, zeroed.
struct SendOptions {
  /// The struct's own entry, which proves its size: name it in pm::api::all_resolved. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SendOptions> type{};
  /// Its delivery mode: DeliveryMode.set(&options, mode). Unresolved: writes nothing.
  static constexpr pm::api::Field<pm::map::SendOptions_DeliveryMode, int32_t> DeliveryMode{};
};

/// A message Photon received for the client. `event` below is the EventData.
struct EventData {
  /// Its code: Code.get(event). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::EventData_Code, uint8_t> Code{};
  /// get_Sender(event): the actor that sent it. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::EventData_get_Sender, int32_t(pm::api::Ref<pm::map::EventData>)> get_Sender{};
  /// get_CustomData(event): what it carries, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::EventData_get_CustomData, pm::api::Object(pm::api::Ref<pm::map::EventData>)> get_CustomData{};
};

/// Photon Realtime's client. `client` below is the LoadBalancingClient.
struct LoadBalancingClient {
  /// OnEvent(client, event): hook it to see every event first. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LoadBalancingClient_OnEvent, void(pm::api::Ref<pm::map::LoadBalancingClient>, pm::api::Ref<pm::map::EventData>)> OnEvent{};
};

/// .NET's byte.
struct Byte {
  /// The type: make_array(n) for a byte[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Byte> type{};
};

/// .NET's int.
struct Int32 {
  /// The type: make_array(n) for an int[]. Unresolved: null.
  static constexpr pm::api::Type<pm::map::Int32> type{};
};

/// @}
}  // namespace phasmo
