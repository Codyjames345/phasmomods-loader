#pragma once

// The lobby's own managers: MainManager (one per menu scene), the lobby computer GhostOS it keeps, and
// the computer's InventoryManager (the shared pool the truck is stocked from) and LoadoutManager (the
// player's saved loadouts). Their names are real; the fields that link them are obfuscated, so they
// are found by their types. Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "Photon.h"
#include "UnityObject.h"

namespace pm::map {

/// The menu scene's manager (Assembly-CSharp, global namespace), a MonoBehaviourPunCallbacks.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc MainManager{.key = "MainManager", .name = "MainManager",
                                                .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviourPun};

/// The menu scene's MainManager: the class's one static field of its own type. Null, or a destroyed
/// one, outside the menu scene. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc MainManager_instance{.key = "MainManager.instance", .kind = pm::sdk::Kind::Field,
                                                         .flags = pm::sdk::kStatic, .owner = "MainManager",
                                                         .type = "@MainManager"};

/// The lobby computer (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostOS{.key = "GhostOS", .name = "GhostOS", .assembly = "Assembly-CSharp",
                                            .ns = "", .base = &MonoBehaviour};

/// The lobby's computer: MainManager's one GhostOS field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc MainManager_ghostOS{.key = "MainManager.ghostOS", .kind = pm::sdk::Kind::Field,
                                                        .owner = "MainManager", .type = "@GhostOS"};

/// The lobby's shared equipment pool (Assembly-CSharp, global namespace), a MonoBehaviour: each
/// player's loadout is added to it, and the truck is stocked from it. Inferred: from its methods' names.
inline constexpr pm::sdk::EntryDesc InventoryManager{.key = "InventoryManager", .name = "InventoryManager",
                                                     .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// The computer's equipment pool: GhostOS's one InventoryManager field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostOS_inventory{.key = "GhostOS.inventory", .kind = pm::sdk::Kind::Field,
                                                      .owner = "GhostOS",
                                                      .type = "@InventoryManager"};

/// Adds a whole loadout to the lobby's pool, one parameter (the Loadout). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc InventoryManager_AddLoadoutToLobby{
    .key = "InventoryManager.AddLoadoutToLobby", .kind = pm::sdk::Kind::Method, .owner = "InventoryManager",
    .name = "AddLoadoutToLobby"};

/// Adds one loadout item to the lobby's pool, one parameter (the item). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc InventoryManager_AddLoadoutItemToLobby{
    .key = "InventoryManager.AddLoadoutItemToLobby", .kind = pm::sdk::Kind::Method, .owner = "InventoryManager",
    .name = "AddLoadoutItemToLobby"};

/// Withdraws this player's own contribution from the lobby's pool: InventoryEquipment.RemoveCommittedItemsFromLobby
/// on each of the pool's items, which returns the committed amount to the player's own stock only where the item
/// allows it (sdk/internals/contract.md, "The death status and the saved equipment"). Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryManager_RemoveMyEquipmentFromLobby{
    .key = "InventoryManager.RemoveMyEquipmentFromLobby", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryManager", .name = "RemoveMyEquipmentFromLobby"};

/// Whether this player may add the item to the lobby's pool: the store's unlock check (StoreItem.IsUnlocked) and the
/// amount the player owns. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_CanAddToLobby{
    .key = "InventoryEquipment.CanAddToLobby", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryEquipment", .name = "CanAddToLobby"};

/// The reward screen's loss of the brought equipment: InventoryEquipment.ResetItem(true) on each of the pool's items.
/// The screen calls it instead of ReturnItemstoInventory when its copy of the death status is set and the
/// contract's Difficulty.loseItemsOnDeath is set. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryManager_ResetAllEquipment{
    .key = "InventoryManager.ResetAllEquipment", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryManager", .name = "ResetAllEquipment"};

/// The reward screen's return of the brought equipment: InventoryEquipment.ReturnCommittedItem on each of the pool's
/// items, then the loadouts' amounts are refreshed. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryManager_ReturnItemstoInventory{
    .key = "InventoryManager.ReturnItemstoInventory", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryManager", .name = "ReturnItemstoInventory"};

/// A piece of equipment the player owns (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc EquipmentInfo{.key = "EquipmentInfo", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .name = "EquipmentInfo", .assembly = "Assembly-CSharp", .ns = "",
                                                  .base = &MonoBehaviourPun};

inline constexpr const char* EquipmentInfo_int[] = {"int"};

/// Takes an amount off the player's saved stock of the item's current tier, kept between 0 and 1000, and saves it; a
/// negative amount adds. Decompiled.
inline constexpr pm::sdk::EntryDesc EquipmentInfo_RemoveAmountOwned{
    .key = "EquipmentInfo.RemoveAmountOwned", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "EquipmentInfo", .name = "RemoveAmountOwned", .params = EquipmentInfo_int};

/// One item of the lobby's pool on this client (Assembly-CSharp, global namespace), an EquipmentInfo. Besides the
/// saved stock it keeps a saved committed amount: what this player put in the truck. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment{.key = "InventoryEquipment",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "InventoryEquipment", .assembly = "Assembly-CSharp",
                                                       .ns = "", .base = &EquipmentInfo};

inline constexpr const char* InventoryEquipment_bool[] = {"bool"};

/// Loses the committed amount: with the flag it adds it to a LevelStats count, then saves the committed amount as 0
/// without returning it to the stock. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_ResetItem{
    .key = "InventoryEquipment.ResetItem", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "InventoryEquipment", .name = "ResetItem", .params = InventoryEquipment_bool};

/// Returns the committed amount to the stock (EquipmentInfo's AddAmountOwned) and saves the committed amount as 0.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_ReturnCommittedItem{
    .key = "InventoryEquipment.ReturnCommittedItem", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryEquipment", .name = "ReturnCommittedItem"};

/// Whether this player may take the item out of the pool: in a room, outside one kind of contract, with an amount of
/// it in their PlayerSpot's equipment, and then either a store-wide state or the item's own saved flag. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_CanRemoveFromLobby{
    .key = "InventoryEquipment.CanRemoveFromLobby", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryEquipment", .name = "CanRemoveFromLobby"};

/// Withdraws the whole committed amount: only when CanRemoveFromLobby allows it, it sends AddToLobbyNetworked with
/// minus that amount, to every client through the server, buffered. Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_RemoveCommittedItemsFromLobby{
    .key = "InventoryEquipment.RemoveCommittedItemsFromLobby", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryEquipment", .name = "RemoveCommittedItemsFromLobby",
    .params = InventoryEquipment_bool};

inline constexpr const char* InventoryEquipment_AddToLobbyNetworked_params[] = {"int", "int", "PhotonMessageInfo"};

/// An amount of a tier added to (or, negative, taken from) the sender's share of the pool, an RPC whose coroutine
/// every client runs. It moves the sender's PlayerSpot equipment count by the amount (a withdrawal by at most that
/// count, an addition by at most what the item has left over), and on the sender's own client saves the new count as
/// the committed amount and takes the amount off the stock (EquipmentInfo.RemoveAmountOwned, so a withdrawal adds it
/// back). Decompiled.
inline constexpr pm::sdk::EntryDesc InventoryEquipment_AddToLobbyNetworked{
    .key = "InventoryEquipment.AddToLobbyNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "InventoryEquipment", .name = "AddToLobbyNetworked",
    .params = InventoryEquipment_AddToLobbyNetworked_params};

/// The player's saved loadouts (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc LoadoutManager{.key = "LoadoutManager", .name = "LoadoutManager",
                                                   .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// The computer's loadouts: GhostOS's one LoadoutManager field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GhostOS_loadouts{.key = "GhostOS.loadouts", .kind = pm::sdk::Kind::Field,
                                                     .owner = "GhostOS", .type = "@LoadoutManager"};

/// Equips the player's saved loadout again, adding it to the lobby's pool. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc LoadoutManager_CheckEquipped{.key = "LoadoutManager.CheckEquipped",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .owner = "LoadoutManager", .name = "CheckEquipped"};

}  // namespace pm::map

/// @file
/// @brief The game API for the lobby's managers (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// The menu scene's manager.
struct MainManager {
  /// The menu scene's MainManager: instance.get(), or null; check it with pm::api::alive, since a
  /// destroyed one stays after the scene. Reading it may run the class's static constructor, so read it
  /// while the game plays. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::MainManager_instance, void*> instance{};
  /// Its lobby computer, a GhostOS: ghostOS.get(main). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MainManager_ghostOS, void*> ghostOS{};
};

/// The lobby computer. `os` below is the GhostOS.
struct GhostOS {
  /// Its InventoryManager: inventory.get(os). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostOS_inventory, void*> inventory{};
  /// Its LoadoutManager: loadouts.get(os). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostOS_loadouts, void*> loadouts{};
};

/// The lobby's shared equipment pool. `inventory` below is the InventoryManager.
struct InventoryManager {
  /// AddLoadoutToLobby(inventory, loadout). A mod hooks it to refuse a loadout. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::InventoryManager_AddLoadoutToLobby, void(void*, void*)>
      AddLoadoutToLobby{};
  /// AddLoadoutItemToLobby(inventory, item). A mod hooks it to refuse an item. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::InventoryManager_AddLoadoutItemToLobby, void(void*, void*)>
      AddLoadoutItemToLobby{};
  /// RemoveMyEquipmentFromLobby(inventory): withdraws this player's contribution. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::InventoryManager_RemoveMyEquipmentFromLobby, void(void*)>
      RemoveMyEquipmentFromLobby{};
  /// ResetAllEquipment(inventory): the reward screen's loss of the brought equipment, on a death with the
  /// difficulty's lose-items set. Local, and saved. A mod hooks it to keep the equipment. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::InventoryManager_ResetAllEquipment, void(void*)> ResetAllEquipment{};
  /// ReturnItemstoInventory(inventory): the screen's return of the brought equipment to the player's stock, the
  /// branch the game takes when nothing is lost (and on leaving a room). Local, and saved. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::InventoryManager_ReturnItemstoInventory, void(void*)>
      ReturnItemstoInventory{};
};

/// One item type's saved stock. `info` below is the EquipmentInfo.
struct EquipmentInfo {
  /// RemoveAmountOwned(info, amount): takes an amount off the saved stock of the item's current tier,
  /// kept between 0 and 1000, and saves it; a negative amount adds. A mod hooks it to refuse a
  /// decrement. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::EquipmentInfo_RemoveAmountOwned, void(void*, int32_t)>
      RemoveAmountOwned{};
};

/// The player's saved loadouts. `loadouts` below is the LoadoutManager.
struct LoadoutManager {
  /// CheckEquipped(loadouts): equips the saved loadout again. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LoadoutManager_CheckEquipped, void(void*)> CheckEquipped{};
};

/// @}
}  // namespace phasmo
