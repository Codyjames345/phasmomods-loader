# Equipment

This page describes the game's equipment system as far as the map covers it: an item in the world, who carries it,
its use and sync, the store's description of it, the lobby's pool and the saved stock, and how evidence reaches the
world. It ends with what a custom item, the dowsing rods, would need. The SDK has no function that registers equipment,
and none is planned in stage 2b. The equipment classes are to be mapped in full before the SDK's first public release
(the stage 2b content design, "The first public SDK release waits for full mapping"), and this page grows with them.

Each claim cites the map key it was read from. An entry's confidence matters here: **Decompiled** entries were read
from the game's code; **Inferred** entries are known only from a class or method name, and their notes say so. Where
the map has nothing, this page says "not mapped" rather than guess.

## An item in the world

`Equipment` (`sdk/map/Equipment.h`) is the base class of every item a player carries, sets down and uses. A `Candle`
is one, and so are the classes the map knows by name: `CCTV`, `MotionSensor`, `SoundSensor`, `EMFReader`,
`ParabolicMicrophone`, `SoundRecorder`, `Thermometer`, `EVPRecorder` (the spirit box), `IRLightSensor` (the D.O.T.S.
projector), `Torch`, `Crucifix` and `WhiteSage` (incense). Only `Equipment`, `Candle` and `WhiteSage` are decompiled;
the rest are Inferred.

| Key | What it is |
| --- | --- |
| `Equipment.placed` | Whether the item is set down, the same on every client. Live: read in the game. |
| `Equipment.PlaceNetworked` | The RPC that sets the item down, on every client: it sets `placed`, takes the item out of the player's hand and lays it at the position and rotation it is given. |
| `Equipment.GrabNetworked` | The RPC of a player picking the item up, on every client: it clears `placed` and frees the item from any socket it was in. |
| `Equipment.isOn` | Whether the item is switched on. Read for candles (`PlayerSanity.CandleIsOn`, `Candle.Update`). |
| `Equipment.Use` | The item's use. For electronics it switches the item on or off, which it sends to every client as the class's `UseNetworked`. Virtual: each kind overrides it, so call it with `virtual_call`. |
| `Equipment.storeItem` | The store's `StoreItem` for the item, or null for an item the store does not sell (a level's own candles). |

[Flames](../systems/flames.md) follows a candle through all of these.

## Who carries what

Each `Player` has a `NetworkedPropGrab` (`Player.propGrab`). Its `NetworkedPropGrab.inventory` lists every item the
investigator picked up and has not dropped, in a hand or put away. The grab RPC adds to it and the drop RPC removes
from it on every client, so every client knows who carries what ([ghost AI](../systems/ghost-ai.md), "Who carries
what"). The grab and drop RPCs themselves have no map entries.

The ghost cares what a player carries: `GhostHuntState.SensesGear` answers whether a player it has not killed,
within its range, carries equipment that is switched on, and the hunt's `Listen` sends the hunt to such a player.

## Use and sync

The map holds a few items' own networked uses, each decompiled or inferred as its note says:

| Key | What it does |
| --- | --- |
| `Crucifix.CrucifixUseNetworked`, `Crucifix.useCount` | The crucifix is used, on every client; a use the game accepts adds one to `useCount`. Inferred. |
| `WhiteSage.UseNetworked` | The incense is used, on every client. Inferred. |
| `WhiteSage.YureiTrap` | The incense's handler that stops the Yurei's wander (`GhostAI.TemporarilyStopWander`). Decompiled. |
| `EVPRecorder.PlayLocationSound`, `PlayAboutSound`, `PlayDifficultySound` | The spirit box plays an answer about where the ghost is, what it is, or how hard the contract is. Inferred. |

A mod hooks these rather than calls them. Never send a game RPC by name or call an RPC's body: the game checks who
sent an RPC, and a refused one can harm the player's account ([Photon](../systems/photon.md), "The game's checks on an
RPC").

Not mapped: the other items' readings (the EMF reader's level, the thermometer's temperature, the sensors' triggers),
and the spirit box's choice to answer.

## The store's description

`StoreItem` is the store's description of an item, a ScriptableObject. The map holds one of its fields:
`StoreItem.tier`, a `StoreItemTier` (`One`, `Two`, `Three`). The game reads it in two places: `PlayerSanity.Update`
scales the sanity drain near a lit candle by its tier, and `Candle.Update` leaves the wax of a tier Three candle (the
lantern) unmelted.

Not mapped: the store's listing, an item's price, its description, its unlock level, the shop screen and the purchase.

## The lobby's pool and the saved stock

`InventoryManager` (`sdk/map/MainManager.h`) is the lobby's shared equipment pool, reached through the lobby computer
(`GhostOS`). Each player's loadout is added to it, and the truck is stocked from it (Inferred, from its methods' names).
Each item of the pool is an `InventoryEquipment`, which is an `EquipmentInfo`: a piece of equipment the player owns.

| Key | What it does |
| --- | --- |
| `InventoryEquipment.AddToLobbyNetworked` | The RPC that moves an amount of a tier into (or, negative, out of) the sender's share of the pool. Every client moves the sender's `PlayerSpot` equipment count; on the sender's own client it saves the new share as the committed amount and takes it off the saved stock. Decompiled. |
| `EquipmentInfo.RemoveAmountOwned` | Takes an amount off the saved stock of the item's current tier, kept between 0 and 1000, and saves it; a negative amount adds. Decompiled. |
| `InventoryEquipment.CanRemoveFromLobby`, `RemoveCommittedItemsFromLobby` | Whether, and how, a player takes their share back out. Decompiled. |
| `InventoryManager.RemoveMyEquipmentFromLobby` | Withdraws this player's share of every item. Decompiled. |
| `InventoryManager.ResetAllEquipment`, `ReturnItemstoInventory` | At the reward screen, the committed equipment is lost (a death on a difficulty that loses items, `Difficulty.loseItemsOnDeath`) or returned to the stock. Decompiled. |
| `InventoryManager.AddLoadoutToLobby`, `AddLoadoutItemToLobby` | Adds a whole loadout, or one of its items, to the pool. Inferred. |
| `LoadoutManager` | The player's saved loadouts. Inferred. |

[The contract](../internals/contract.md), "The death status and the saved equipment", walks through a contract's start
and end. A spot's equipment list gets its entries when the game adds the spot ([players](../systems/players.md)).

Not mapped: how the truck lays out what the pool holds (its slots and how an item spawns there), the loadout's format
and save, and the item list the pool is built from.

## How evidence reaches the world

The master picks the ghost's evidence, and `GhostInfo.SyncEvidence` gives every client the same `givenEvidence`
numbers, then calls `EvidenceController.SpawnAllGhostTypeEvidence` ([evidence](../systems/evidence.md), "How what is
given reaches every client"). A custom evidence's number travels the same way. The map holds one of the controller's
own moves: the ghost orb follows the favourite room only when the given evidence holds Ghost Orbs, or the ghost is a
Mimic.

An item shows an evidence only through the code of its own class, and that code is not mapped for any vanilla item.
Spectral Flame shows how a mod reveals its own evidence: it reads the given numbers on each client and changes the
look of the game's own flames, and a ghost whose given list lacks the flame never shows it
([flames](../systems/flames.md)).

## Worked example: dowsing rods

The user's example of custom equipment is a set of dowsing rods: an item with a place in the truck, a shop listing with
a price and a description, a use in the investigation, and perhaps an evidence type. This section lists what a mod would
need for each part. It designs none of the rods' behaviour: that is the author's to decide, and each mechanic the rods
mirror needs its own reverse engineering first.

What the map already gives such a mod:

- the item in the world: `Equipment`, with `placed`, `isOn`, `Use` and `storeItem`;
- who carries it: `Player.propGrab` and `NetworkedPropGrab.inventory`;
- the ghost's interest in switched-on equipment: `GhostHuntState.SensesGear`;
- an evidence of its own: `pm::api::content::register_evidence`, which already puts a custom evidence on the journal,
  into the game's evidence pick and to every client as a number (the SDK's README, "The host contract, version 2.3").

What is missing, each to map before a registration function could exist:

- **The item's object.** How the game makes an item: its prefab, its `PhotonView`, and the components an `Equipment`
  needs to be grabbed, placed and used. A custom item needs a model, which is an asset, so the rods' art comes from
  their author.
- **The truck.** How the truck lays out the pool's items and spawns each, so a custom item gets a place.
- **The store.** The listing, price, description and unlock of a `StoreItem`, and the shop screen that shows them, so a
  custom item appears on the game's own screen (vanilla UI only).
- **The pool and the save.** The list the pool is built from and the save of each item's stock, committed amount and
  loadouts.
- **The use.** How an item's `UseNetworked` reaches every client, so the rods' use syncs as a vanilla item's does.

What a registration function would then need, as `register_ghost` and `register_evidence` have:

- a stable id, `"<mod id>.<name>"`, and text from the mod's string table, English required;
- a place in the store and the truck, read from the game's own lists at runtime, never fixed in code;
- callbacks for its use, run on the main thread, and for its held and placed states.

The rules it would have to follow:

- **Saves and other players.** A custom item never reaches a save file or a player without the content, as for a custom
  ghost type. The saved stock and the loadouts are saves, so a custom item could not be stored there as a vanilla item
  is; that needs a design of its own.
- **Currency.** Kinetic Games allows mods in private lobbies as long as they don't add excessive amounts of in-game currency or
  collectables, which would break progression. Whether a custom item may cost in-game money is a question for the project's author before any
  price is built.
- **Lobbies.** Every player in a contract runs the host's mods, so every client has the item's code. The loader decides
  this; the mod checks nothing (the SDK's README, "The lobby's rules").
