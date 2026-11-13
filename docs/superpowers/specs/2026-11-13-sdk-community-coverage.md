# SDK community coverage (stage 2b Task 17, Step 1)

What the SDK must hold so that a community author writes a mod with no reverse engineering, and so that the Playable
Ghost revamp (user, 2026-11-13) has every game fact it needs. For each area: what the map has now, what is still to
map, the reverse engineering that needs, the small `sdk/api` helpers worth adding, and the questions an author will
ask, each with the snippet that answers it. It ends with the mapping rounds in order and the questions only the user can
answer. **This spec is the user's to approve before any mapping starts** (Task 17 Step 1).

Written against `development` at c1bd983 (2026-11-13): 2,091 map entries in 59 headers of `sdk/map/`, notes read from
Steam build 25811160. Nothing here was reverse engineered for this spec; every fact about the game comes from
`sdk/systems/*.md`, `sdk/guides/*.md` and the map's `///` notes, cited by map key. Where a behaviour is known from
playing but not mapped, it says **game knowledge, to verify by RE**, and must not be taken as fact.

## How to read this spec

- **Mapped already** names map keys (`GhostAI.isHunting`) or a whole header where it covers an area (all of `Door.h`).
  Confidence matters: an entry marked Inferred in its note (from a name only) is listed as mapped, with "(inferred)",
  and its round upgrades it to Decompiled when a mod is to rely on it.
- **To map** lists each missing member or behaviour, with what a mod does with it: **read**, **call**, **hook** or
  **change**. A change always goes through the game's own method, never a field write that skips the game's behaviour
  (CLAUDE.md). An RPC is only ever **hooked**: no mod sends a game RPC by name or calls an RPC's body
  (`sdk/systems/photon.md`, "The game's checks on an RPC").
- **RE needed** says what the round must find or read. It is not done here.
- **Helpers** are small `sdk/api` functions built only from map entries, where a common question joins several. The
  shared ones are listed once, in "The shared helpers" below; an area names any of its own.
- **Snippets** use the typed handles of Task 14 Step 0b (`pm::api::Ref<D>`, so `phasmo::GhostAI::Ref`), which are being
  built now. A call that is not mapped yet carries `// to map`; a proposed helper carries `// helper`. Snippets are
  sketches of the answer, written to become `tests/coverage_questions_test.cpp` cases in Step 3, where they compile
  against the fake backend.

## What changed since the plan was written

Much of the plan's Task 17 list was mapped by later tasks, so it is not listed as missing below:

- **Doors** (all of `Door.h`, `sdk/systems/doors.md`), **navigation** basics (`UnityWorld.h`, `sdk/systems/navigation.md`),
  **menus and UI widgets** (`UnityUI.h`, `Doozy.h`, `UnityEvents.h`, `Input.h`, `sdk/systems/menus.md`,
  `sdk/api/buttons.h`, `menu_rows.h`, `keybinds.h`), **settings sliders** (`sdk/api/settings.h`), **a mod's own assets**
  (`sdk/api/assets.h`, `bundles.h`, `models.h`, `packs.h`), **emotes and the body** (`Playables.h`, `FinalIK.h`,
  `sdk/api/player_anim.h`, `third_person.h`), **weather** (`Weather.h`, `sdk/systems/weather.md`), **voice**
  (`PlayerVoice.h`, `sdk/systems/voice.md`), **the contract's ends** (`GameController.h`, `sdk/systems/contract.md`),
  **death and revival** (`Player.h`, `sdk/systems/players.md`), **objectives** (`Rewards.h`), and **the Squatter's
  entries** (`ClosetZone.h`, `PhysicsCharacterController`, `LocalPlayer.TeleportPlayer`).
- **The current ghost** is reachable now: `LevelController.instance` and `LevelController.currentGhost` are mapped, and
  `core/game.cpp`'s `ghost_ai()` reads them through the map. The plan's workaround (a raw read of the level controller)
  is gone. What is missing is a documented, typed helper in `sdk/api/` for it: `core/game.h` is shared code of the
  bundled mods, typed `void*`, and not part of the SDK's reference.
- **Docs that lag the map** (fixed in Round 1, no RE): `sdk/systems/players.md` says the dying flag is not mapped, but
  `Player.dying` is; `sdk/systems/vanilla-ghosts.md` says no Yokai rule is mapped, but `GhostIdleState.TypeHuntRules`
  (its hunt line of 80 while players talk) and `GhostHuntState.Hears` (its 2.5 m hearing) are, and the same page omits
  the hunt lines `TypeHuntRules` gives the Mare, Shade, Demon, Onryo, Raiju, Deogen, Gallu, Dayan, Obambo and Kormos;
  `sdk/README.md`'s "What exists today" still counts GhostAI at 17 entries.

## The shared helpers

Proposed for a new `sdk/api` header (its name is a question for the user, below; `sdk/api/world.h` is used here). Each is
a few lines over map entries, documented to the Task 14 Step 1e standard, returns a typed handle, and answers a null
handle (never throws) outside a contract or when an entry did not resolve. `core/game.h` then calls these instead of
holding its own copies, so the bundled mods and community mods share one implementation.

| Helper | Built from | Answers |
| --- | --- | --- |
| `phasmo::level()` | `LevelController.instance`, `pm::api::alive` | the level's controller, null outside a contract (the static outlives its level) |
| `phasmo::current_ghost()` | `level()`, `LevelController.currentGhost` | the contract's ghost, a `GhostAI::Ref`, on every client |
| `phasmo::ghost_controller()` | `GhostController.instance`, `alive` | the contract's ghost controller |
| `phasmo::ghost_info()` | `current_ghost()`, `GhostAI.info` | the ghost's `GhostInfo` on this client |
| `phasmo::is_hunting()` | `current_ghost()`, `GhostAI.isHunting` | whether a hunt is on |
| `phasmo::local_player()` | `Network.get_Instance`, `Network.localPlayer` | this client's investigator |
| `phasmo::each_player(f)` | `Network.playerSpots`, `PlayerSpot.player`, `alive` | every investigator in the room's spots, dead ones included |
| `phasmo::living(p)` | `Player.isDead`, `Player.dying` | alive and not dying |
| `phasmo::player_of_actor(n)`, `phasmo::actor_of(p)` | `PlayerSpot.photonPlayer`, `PhotonPlayer.get_ActorNumber` | a player by Photon actor number and back |
| `phasmo::position_of(obj)` | `Component.get_transform`, `Transform.get_position` | where a component's object stands |
| `phasmo::room_of(p)` | `Player.currentRoom` | the room a player is in (the last room trigger their camera entered) |
| `phasmo::room_at(point)` | `LevelController.rooms`, `LevelRoom.roomColliders`, `Collider.get_bounds` | the first room whose colliders hold a point |
| `phasmo::ghost_room()` | `LevelController.currentGhostRoom` | the room the ghost is in, on every client |
| `phasmo::favourite_room()` | `GhostController.favouriteRoomIndex`, `LevelController.rooms` (or `GhostInfo.favouriteRoom` once decompiled) | the ghost's favourite room |
| `phasmo::sanity_of(p)` | `Player.sanity`, `PlayerSanity.insanity` | a player's sanity, 0 to 100, as the players read it (100 less insanity) |
| `phasmo::average_sanity()` | `GameController.instance`, `GameController.GetAveragePlayerInsanity` | the team's average, as the game's checks use it |
| `phasmo::difficulty()` | `LevelValues.get_Instance`, `LevelValues.difficulty` | the difficulty this contract plays |

`room_at` and `room_of` can disagree: `Player.currentRoom` keeps the last room entered, `room_at` tests the point now.
The helper notes say so, as `LevelRoom.OnTriggerEnter`'s note does.

### The lifecycle events

Hook points as thin wrappers over hooks of the game's own methods, in a second new header (`sdk/api/events.h` here).
A mod hooks a method once (`pm::api::hook`), so two parts of one mod cannot both hook `GhostAI.Hunting`; the events
layer owns one hook per method and calls every callback the mod registered, in order. Each event's note says on which
clients it fires and on which it does not.

| Event | Hooked entry (mapped) | Fires |
| --- | --- | --- |
| `on_ghost_spawned(ghost)` | `GhostAI.Init` (from the `GhostController.CreateGhost` RPC) | every client, once a contract |
| `on_hunt(ghost, started)` | `GhostAI.Hunting` (RPC body, hooked only) | every client, at each start and end |
| `on_hunt_state_exit(hunt)` | `GhostHuntState.Exit` | the master, as the hunt state ends |
| `on_state_change(ghost, state, object, forced)` | `GhostAI.ChangeState` | the master; a callback may refuse, as Custom Ghosts' restraint does |
| `on_death_start(player)` | `Player.NotifyOfDeathStart` (RPC body, hooked only) | every client, as a kill begins |
| `on_death(player)` | `Player.CompleteDeathSequenceNetworked` (RPC body, hooked only) | every client, as a kill completes |
| `on_revive(player)` | `Player.RevivePlayer`'s `Revive` RPC: **to map** | every client |
| `on_contract_loaded()` | the set of `GameController.allPlayersLoaded`: **to map** (the loaded event it raises) | every client |
| `on_contract_started()` | **to map**: what "started" means in the game (the truck's first use, `LiftButton.NetworkedUse`, which calls `GhostController.WarpGhostToFavouriteRoom`; or the setup phase's end) | every client |
| `on_contract_ending(kind)` | `ExitLevel.Exit`, `GameController.Exit`, `PauseMenuController.Leave`, `PauseMenuController.Quit` | every client (each end's own) |
| `on_favourite_room_changed(room)` | `GhostController.ChangeFavouriteRoomTo` | the master |
| `on_event(ghost, kind)`, `on_appear(ghost)` | **to map** (ghost events, below) | |
| `on_sanity_changed(player, amount, cause)` | `PlayerSanity.ChangeSanity`, `PlayerSanity.SetInsanity`; the cause is **to map** | |

---

## 1. The ghost

**Mapped already.**
- The ghost and its brain: all of `GhostAI.h` (`sdk/systems/ghost-ai.md`): `GhostAI.state`, `ChangeState`, `stateMachine`,
  `GhostStateMachine.current`, `agent`, `info`, `isHunting`, `Hunting`, `postHuntCooldown`, `huntsHeldOff`,
  `StopGhostFromHunting`, `smudge`, `Appear`, `MakeGhostAppear`, `isAppeared`, `UnAppear`, `LookAtPlayer`,
  `TemporarilyStopWander`, `canWander`, `stayHome`, `interactionBoost`, `GetNearestPlayer`, `vision`, `hearing`,
  `sightTargets`, `head`, `model`, `activity`, `interaction`, `EventPick`, the speed inputs (`baseSpeed`,
  `huntSpeedFactor1` to `3`, `speedFlag`) and the `GhostState` values by name (`idle` to `ignite`, `summoningCircle`,
  `musicBox`, `GhostAbility`, `randomEvent`).
- Its type and identity: all of `GhostController.h`, `GhostInfo.h`, `GhostTypes.h`, `GhostCollection.h`
  (`sdk/systems/ghost-types.md`): `GhostInfo.identityType`, `behaviourType`, `name`, `givenEvidence`; the roll
  (`GhostController.Spawn`, `PickType`, `PickEvidence`, `CreateGhost`); the Mimic (`mimicCopies`, `mimicNext`,
  `MimicCopy`, `AttemptGhostTypeStateChange`).
- Its states: `GhostIdleState.h` (the hunt check, `HuntInsanity`, `TypeHuntRules`), `GhostWanderState.h`
  (`PickDestination`, `Exit`), `GhostDotsState`, `GhostHuntState.h` (below, "Hunts").
- Its rooms: `LevelController.currentGhostRoom`, `GhostController.favouriteRoomIndex`, `ChangeFavouriteRoomTo`,
  `WarpGhostToFavouriteRoom`, `GhostInfo.favouriteRoom` (inferred).
- Its interactions: `GhostActivity.TryInteract` (inferred), `NormalInteraction`, `GetDoorToOpen`, `doorChance`,
  `lightChance`, `InteractWithARandomProp`, `GetPropToThrow`, `throwable`, `TwinInteraction`, `GhostAbility`,
  `interactionSensor`, `sensorBox`; its traces `GhostInteraction.CreateThrowingEMF`, `CreateInteractionEMF`,
  `CreateAppearedEMF`.
- Its sensors: `Sensor.h` (`RangeSensor`, `LOSSensor`, `GhostHuntingProcessor`, `PlayerProcessor`), `GhostSenses.h`
  (`GhostHearing`, `PlayerNoise`, `NoiseKind`), its footsteps in `Footstep.h`.
- Its body: `GhostModel.animator`, `renderers` (inferred), `sounds` (inferred), `Setup`.
- Steering it: `pm::api::AgentSteering` (`NavMeshAgent::steer`), and the quiet route ("Moving the ghost outside a hunt").

**To map.**
- The ghost events, each kind, and appearing in each form: see "Ghost events" in the revamp section. **Hook**, **call**.
- The ghost's own sounds: the GhostAudio component (`GhostModel.Setup`'s note names `GhostAudio.SetHuntingSound`), its
  growls, knocks and paranormal sounds. **Hook** (to see a sound), **call** (to play one through the game).
- `GhostInfo.favouriteRoom`: upgrade from inferred, and say how it relates to `GhostController.favouriteRoomIndex`.
  **Read**.
- The salt state value (`GhostState.salt`, named in `sdk/systems/ghost-ai.md` but with no entry). **Read**.
- The ghost's speed outside hunts, if the game sets one by type (only `GhostAI.Awake` and the idle state's entry are
  known to set it). **Read**.

**RE needed.** `GhostAI.EventPick` and the event states it starts; the GhostAudio class and who calls its plays; the
`GhostInfo.favouriteRoom` writers; the `GhostState` value for salt (by `SaltSpot.SyncSalt`'s change).

**Helpers.** `current_ghost()`, `ghost_info()`, `is_hunting()`, `ghost_room()`, `favourite_room()`; plus
`phasmo::ghost_type()` (`GhostInfo.identityType`) and `phasmo::acting_as()` (`GhostInfo.behaviourType`, which a
Mimic's copy changes), so an author never confuses the two.

**Questions an author asks.**

```cpp
// Is the ghost hunting?
bool hunting = phasmo::is_hunting();  // helper

// What is the ghost, and what is it acting as (a Mimic copies others)?
phasmo::GhostInfo::Ref info = phasmo::ghost_info();  // helper
int32_t identity = phasmo::GhostInfo::identityType.get(info);
int32_t actingAs = phasmo::GhostInfo::behaviourType.get(info);
bool isMimic = identity == phasmo::GhostType::Mimic();

// Which room is the ghost in, and which is its favourite?
phasmo::LevelRoom::Ref now = phasmo::ghost_room();         // helper
phasmo::LevelRoom::Ref home = phasmo::favourite_room();    // helper
std::string name = pm::api::to_utf8(phasmo::LevelRoom::name.get(now));

// Send the ghost to its favourite room, as the game does (master only).
phasmo::GhostAI::ChangeState(phasmo::current_ghost(), phasmo::GhostState::favouriteRoom(), {}, false);

// Play a growl through the ghost's own audio.
phasmo::GhostAudio::PlaySound(audio, kind);  // to map
```

## 2. Players

**Mapped already.**
- Who is there: `Network.get_Instance`, `Network.playerSpots`, `PlayerSpot.player`, `PlayerSpot.photonPlayer`,
  `PlayerSpot.playerReady`, `PlayerSpot.level`, `PlayerSpot.prestige`, `Network.localPlayer`, `Network.localRig`,
  `Network.GetPlayerFromPhotonPlayer`, `Network.GetSpotPlayerFromPhotonPlayer`, `Network.GetIndexOfPlayer`,
  `PhotonPlayer.get_ActorNumber`, `PhotonPlayer.get_NickName`, `PhotonPlayer.IsLocal` (`sdk/systems/players.md`,
  `photon.md`).
- Alive or dead: `Player.isDead`, `Player.dying`, `Player.BeginDeathSequence`, `NotifyOfDeathStart`,
  `CompleteDeathSequenceNetworked`, `RevivePlayer`, `DeathType`, `LocalPlayer.isDead`, `GameController.PlayerDied`,
  `AmountOfDeadPlayers`.
- Where: the body (`Player.body`, its transform), `Player.currentRoom`, `Player.closet`, `Player.losTargets`,
  `LocalPlayer.TeleportPlayer`, `VRPlayer.Teleport`, `PhysicsCharacterController` (capsule and masks).
- What they carry: `Player.propGrab`, `NetworkedPropGrab.inventory`, `Player.keys`, `PCPropGrab.GetItemInCameraSlot`
  (local only).
- How they move: `LocalPlayer.walkSpeed`, `sprintSpeed`, `speedMultiplier`, `LocalPCPlayer.SetPlayerSpeedMultiplier`,
  `ResetPlayerSpeedMultiplier`, `PlayerStamina.canSprint`, `NetworkedStamina.moving`, `FirstPersonController.moveInput`,
  `running`, `sdk/api/movement.h`, `Player.ToggleFreezePlayerMovement` (inferred), `PCCanvas.PausePlayerInPlace`.
- Their voice and noise: `Player.audio`, `PlayerAudio.loudness`, `IsTalkingLoudly`, `PlayerNoise`.
- Their body and animation: `Player.animator`, `renderers`, `characterAnimations`, `PlayerCharacter`, `PlayerCosmetics`.
- Their journal: `Player.journal`; their stats: `Player.stats`, `PlayerStats.SyncStats`.
- The truck: `ExitLevel.ThereAreAlivePlayersOutsideTheTruck`, `ExitLevel.trigger`, `ExitLevelTrigger`.

**To map.**
- The item in a player's hand, on every client (not only the local camera slot). **Read**.
- Whether a player is in the truck, per player: what `ExitLevel.ThereAreAlivePlayersOutsideTheTruck` reads, or the
  trigger's own occupants. **Read**.
- Whether a player is talking now (the push-to-talk and walkie presses, `PlayerVoice`'s voice mode, which
  `sdk/systems/voice.md` lists as unmapped). **Read**.
- Whether a player is crouching. **Read**. (Game knowledge, to verify by RE: crouching changes footstep noise, as
  `NoiseKind.CrouchFootstep` suggests.)
- The revive's `Revive` RPC body, for `on_revive`. **Hook**.

**RE needed.** The held-item field of `NetworkedPropGrab` (which of the inventory is in hand) and its sync; the readers of
`ExitLevelTrigger`; `PlayerVoice`'s press fields and the walkie's; the crouch state and where it is synced; the
`Revive` body.

**Helpers.** `local_player()`, `each_player()`, `living()`, `player_of_actor()`, `actor_of()`, `position_of()`,
`room_of()`; plus `phasmo::held_item(p)` once mapped and `phasmo::in_truck(p)`.

**Questions an author asks.**

```cpp
// Who is in the contract, alive, and where?
phasmo::each_player([](phasmo::Player::Ref p) {      // helper
  if (!phasmo::living(p)) return;                      // helper
  phasmo::Vector3 at = phasmo::position_of(p);         // helper
  phasmo::LevelRoom::Ref room = phasmo::room_of(p);    // helper
});

// Which Photon actor is this player (to message them through the SDK's channel)?
int actor = phasmo::actor_of(player);  // helper

// Is this player hiding in a closet?
bool hiding = phasmo::Player::closet.get(player) != nullptr;

// What is this player holding?
phasmo::PhotonObjectInteract::Ref held = phasmo::NetworkedPropGrab::held.get(  // to map
    phasmo::Player::propGrab.get(player));

// Is this player in the truck?
bool safe = phasmo::in_truck(player);  // helper, to map
```

## 3. Sanity

**Mapped already.** `Player.sanity`, `PlayerSanity.insanity` (0 to 100, 100 the most insane), `ChangeSanity`
(adds to insanity within what the difficulty allows), `SetInsanity` (inferred), `Update` (the drain in the dark),
`CandleIsOn`, `GameController.GetAveragePlayerInsanity`, `HeartRateData.UpdateUI` (inferred), `Wristwatch.UpdateSanity`,
`Difficulty.startingSanity`, `sanityDrain`, `sanityPillRestorationAmount`, `sanityMonitor`; the idle hunt check's use of
it (`GhostIdleState.HuntInsanity`).

**To map.**
- **Every way sanity drops or rises**, each as a cause a hook can tell apart: the darkness drain, a ghost event, seeing
  the ghost appear, a hunt, a cursed possession's use (the mirror, the Ouija board, the tarot cards, the monkey paw's
  weather wish takes 7 per `sdk/systems/weather.md`), a ghost's ability (the Jinn, the Yurei, the Poltergeist), the
  Phantom when seen, the Moroi's curse, the sanity pill, and the Winter's Jest RPC (`WintersJestManager.RPC_MakeKrampusStronger`
  acts on sanity). **Hook** (to see fear as it happens).
- The sanity medication's use (an item; not mapped). **Hook**.
- Where insanity is synced to other clients (`SetInsanity` sends it; how often the drain reaches others). **Read**.

**RE needed.** Every caller of `PlayerSanity.ChangeSanity` and `PlayerSanity.SetInsanity`, sorted by what they are
(each will be an `inside<>` context or a cause on `on_sanity_changed`); `SetInsanity`'s body (upgrade from inferred) and
its RPC; the pill's class.

**Helpers.** `sanity_of()`, `average_sanity()`, and the event `on_sanity_changed(player, amount, cause)`, whose cause is
the caller found by RE (the host's `inside<>` already tells callers apart).

**Questions an author asks.**

```cpp
// How sane is each player, and the team?
float mine = phasmo::sanity_of(phasmo::local_player());  // helper
float team = phasmo::average_sanity();                    // helper

// Take 10 sanity from a player, as the game does (the difficulty's limits apply).
phasmo::PlayerSanity::ChangeSanity(phasmo::Player::sanity.get(player), 10.0f);

// Tell me whenever someone loses sanity, and why.
pm::api::events::on_sanity_changed([](phasmo::Player::Ref p, float amount, int cause) {  // helper, cause to map
});
```

## 4. Map data

The map, the truck, the fuse box, doors, lights, hiding places and the cursed possession.

**Mapped already.**
- Doors: all of `Door.h` (`sdk/systems/doors.md`): finding, the leaf and its angle, locks and keys (`Door.locked`,
  `LockDoor`, `UnlockDoor`, `ToggleDoorLock`, `LockDoorForTime`, `KeyInfo`, `Key.GrabbedKey`), the ghost's force
  (`GhostForceDoor`, `CheckDoorDuringHunt`, `GhostOpenDoorEvent`), slams (`CreateDoorSlamNoise`, `PlayDoorSlamNoise`),
  the exits (`DoorKind`, `LevelController.huntingDoors`, `LockHuntingDoors`, `UnlockHuntingDoors`, `SmallMapVariation`),
  a quiet open (`HingeDoor.OpenDoor`).
- Lights: all of `LightSwitch.h` (`isOn`, `UseLightSwitch`, `Use`, `FlickerNetworked`, `room`, `sources`,
  `ghostUseRefused`, `isFuseboxSwitch`), `LevelRoom.lightSwitches` (inferred), `LevelRoom.AreRoomLightsOn` (inferred).
- The fuse box: `LevelController.fuseBox`, `FuseBox.powerOn`, `outOfOrder`, `BreakFuseBox`, `GhostUse`;
  `Difficulty.fuseBoxStartPower`, `fuseBoxVisibleOnMap`; `GhostState.fusebox`.
- Hiding places: all of `ClosetZone.h` (`ClosetZone`, `occupants`, `navSpot`, `CheckCloset`, `HidingSpot`),
  `BlockedZone`, `LevelController.blockedZones`, `Difficulty.amountOfHidingPlaces`.
- The truck and the exits: `LiftButton`, `ExitLevel`, `SetupPhaseController.mainDoorOpen`, `UnlockMainDoor`.
- The map's floors: `MapController` (all optional and inferred), `FloorType`, `LevelRoom.floor`.
- Outdoors: `LevelController.outsideRoom`, `IsPointInOutsideRoom`, `RoomKind`.
- The cursed possession present: `Difficulty.actualCursedItems`, `cursedPossessionsQuantity`, `CursedItemType` (two
  values: `tarotCards`, `monkeyPaw`); each possession in `CursedItems.h` (section 6).
- Props: all of `Props.h` (`PhotonObjectInteract`, `Prop.TryThrow`, `Sink`, `SaltSpot`).

**To map.**
- **The map itself**: which map this contract is on, its name as the game shows it, and its size class (small, medium,
  large; `GhostActivity.sensorBox` and the open doors are set "by the map's size", per their notes). **Read**.
- The player's own use of the fuse box (the switch a player flips) and the power going off in a hunt or by the ghost,
  as one event. **Hook**.
- Every `CursedItemType` value by name, not only two. **Read**.
- `LevelRoom.lightSwitches` and `AreRoomLightsOn`: upgrade from inferred. **Read**.
- Light bulbs breaking (game knowledge, to verify by RE: a ghost may burst a bulb). **Hook**.
- The truck's own state: whether the truck's doors are closing (`LiftButton.isClosed` is mapped; the rest of the
  leave sequence is in `contract.md`). Nothing more is needed unless a round finds a gap.

**RE needed.** Where the game holds the contract's map (a LevelValues or scene field) and its size enum; `FuseBox`'s
player use and its RPC; `CursedItemType`'s other values; `LevelRoom`'s light members' readers.

**Helpers.** `phasmo::current_map()` (an id and a localised name), `phasmo::power_on()` (`level()`,
`LevelController.fuseBox`, `FuseBox.powerOn`).

**Questions an author asks.**

```cpp
// Is the power on?
bool power = phasmo::power_on();  // helper

// Which doors lead out, and are they locked?
phasmo::LevelController::Ref lc = phasmo::level();  // helper
// Step 0b gives typed element handles for mapped element types; today it is a RefArray of raw references.
const pm::api::RefArray exits{phasmo::LevelController::huntingDoors.get(lc)};
for (size_t i = 0; i < exits.size(); ++i)
  bool locked = phasmo::Door::locked.get(phasmo::as<phasmo::Door>(exits[i]));

// Which map is this?
phasmo::MapId map = phasmo::current_map();  // helper, to map

// Are this room's lights on?
bool flickering = false;
bool lit = phasmo::LevelRoom::AreRoomLightsOn(room, &flickering);
```

## 5. Rooms

**Mapped already.** `LevelController.rooms`, `LevelRoom.roomColliders`, `name` (inferred), `kind`, `floor`, `doors`
(inferred), `lightSwitches` (inferred), `temperature`, `Update` (the temperature system), `ResetTemperature`,
`OnTriggerEnter` (sets `Player.currentRoom`), `AnyPlayersInRoom` (inferred), `ArePlayersTalkingInRoom` (inferred),
`LevelRoom.GetPlayersInRoom`, `LevelController.currentGhostRoom`,
`SyncCurrentGhostRoomNetworked`, `Collider.get_bounds`; the room's floor sound (`SoundController.GetFloorAudioSnapshot`).

**To map.**
- **Temperature, whole**: how `LevelRoom.Update` moves a room's temperature (its target, rate and what it reads: the
  ghost's room, the power, the Freezing evidence `EvidenceController.freezing`), and the readings a thermometer shows
  (`LevelRoom.temperature`'s note says the reading is it, or a heat source's, plus a random spread). **Read**; the
  numbers go in the entry notes.
- Which room the ghost counts as in when it stands outdoors (Task 17's navigation item; `LevelRoom.OnTriggerEnter`
  skips `RoomKind.outsideMap`). **Read**.
- `LevelRoom.name` as the player reads it (localised) and its key. **Read**.

**RE needed.** `LevelRoom.Update`'s branches and the fields it reads (the "few flags" it returns on); `ResetTemperature`'s
difficulty case; `LevelRoom.name`'s writers; the readers of `LevelRoom.GetPlayersInRoom`.

**Helpers.** `room_of()`, `room_at()`, `ghost_room()`, `favourite_room()`; `phasmo::each_room(f)`;
`phasmo::players_in(room)` over `LevelRoom.GetPlayersInRoom`.

**Questions an author asks.**

```cpp
// Which room is the local player in, and how cold is it?
phasmo::LevelRoom::Ref room = phasmo::room_of(phasmo::local_player());  // helper
float celsius = phasmo::LevelRoom::temperature.get(room);

// Is anyone in the ghost's room?
bool company = phasmo::LevelRoom::AnyPlayersInRoom(phasmo::ghost_room());

// What room is this point in?
phasmo::LevelRoom::Ref at = phasmo::room_at(point);  // helper

// Is the room freezing because of the ghost's evidence?
bool freezes = phasmo::EvidenceController::freezing.get(evidence);
float target = phasmo::LevelRoom::targetTemperature.get(room);  // to map
```

## 6. Equipment

Each class, held or placed, on or off, each sensor's reading, and the evidence it reveals. Registering new equipment
stays documented only (`sdk/guides/equipment.md`).

**Mapped already.** Shared: `Equipment.isOn`, `Use` (virtual), `placed`, `PlaceNetworked`, `GrabNetworked`,
`storeItem`, `StoreItem.tier`, `itemName`, the store (`StoreItem.IsUnlocked`, `MeetsLevel`, `CanAfford`,
`StoreEquipment.CanBuy`, `Buy`), the pools (`ObjectPooler.SpawnFromEquipmentPool`, `TierEquipmentPool`,
`LevelValues.equipmentLoadout`, `GameEquipmentLoadout`), the lobby's pool (`InventoryManager`, `InventoryEquipment`,
`LoadoutManager`; `sdk/guides/equipment.md`). Items: `Candle`, `CandleObject`, `Candlebra`, `FireSource` (flames.md);
`CCTV` (inferred) and `CCTVController`; `Crucifix.CrucifixUseNetworked` (inferred), `useCount`; `WhiteSage.UseNetworked`
(inferred), `YureiTrap`; `EVPRecorder.PlayLocationSound`, `PlayAboutSound`, `PlayDifficultySound` (all inferred);
`EMFReader`, `Thermometer`, `MotionSensor`, `SoundSensor`, `ParabolicMicrophone`, `SoundRecorder`, `IRLightSensor`,
`Torch` (types only, inferred); `SaltSpot`; the EMF reading object `EMF` and `EMFType` (inferred).

**To map.**

| Item | To map | Use |
| --- | --- | --- |
| EMF reader | its level now (1 to 5) and the reading it shows from; an `EMF` reading's level, place and life | read, hook |
| Thermometer | its shown reading | read |
| Spirit box | whether it is on and listening; the question heard; the game's choice to answer, and the answer; the breath sound (the Deogen) | hook, read |
| Ghost writing book | placed, written in, which page | hook, read |
| D.O.T.S. projector | on, placed, its area; whether the ghost shows in it (`GhostDotsState`) | read, hook |
| UV light and glowsticks | on, charge; which prints it shows | read |
| Video camera | on, night vision (media.md: not mapped), who views it (mapped through `CCTVController.viewers`) | read |
| Photo camera | a photo taken, its subject and its stars; the photo's flash on the ghost | hook |
| Parabolic microphone | its reading, the sounds it picks up (the Banshee's scream, the Myling) | read, hook |
| Sound sensor, motion sensor | each trigger, with where and when | hook |
| Sound recorder | a recording's subject | hook |
| Crucifix | its range by ghost type, a burn (a prevented hunt), upgrade `CrucifixUseNetworked` from inferred | hook, read |
| Incense | a smudge, its target (the ghost, in a hunt or not), upgrade `UseNetworked` from inferred | hook |
| Salt | a pile laid; a step in it (`SaltSpot.SyncSalt`, mapped); the footprints | hook |
| Firelight, igniter | mapped (`FireSource`, `IgniteFire`) | |
| Sanity medication | a use and the amount restored | hook |
| Head gear, tripod, torches | on or off (`Equipment.isOn`) is enough unless a round finds more | read |
| Any item | which player holds it, in hand or put away (section 2) | read |

**RE needed.** Each item class's own `Use` override, its networked use and its fields (the EMF reader's level, the
thermometer's shown value, the spirit box's answer roll and its phrase check, the camera's photo and its subject
detection, the sensors' triggers); `EMF`'s fields; the crucifix's range test; the media's star rating.

**Helpers.** `phasmo::each_item(f)` (every `Equipment` in the level, through `Object.FindObjectsOfType`, with the costly
scan kept as `core/game.h`'s `each_running_camera` keeps it); `phasmo::holder_of(item)`.

**Questions an author asks.**

```cpp
// Is this item placed and switched on?
bool placedOn = phasmo::Equipment::placed.get(item) && phasmo::Equipment::isOn.get(item);

// Is this item a crucifix?
if (phasmo::Crucifix::Ref crucifix = phasmo::as<phasmo::Crucifix>(item))
  int uses = phasmo::Crucifix::useCount.get(crucifix);

// What does this EMF reader show?
int level = phasmo::EMFReader::level.get(reader);  // to map

// Tell me when someone takes a photo of the ghost.
pm::api::events::on_photo([](phasmo::Player::Ref by, int subject, int stars) {});  // helper, to map
```

## 7. Weather

**Mapped already.** All of `Weather.h` and the weather members of `Difficulty.h` (`sdk/systems/weather.md`):
`WeatherType`, `WeatherValueType`, `Difficulty.actualWeather`, `selectedWeather`, `LevelValues.weather`,
`RandomWeather.TransitionWeatherProfile`, `RandomWeather.effects`, `LightningController.PlayLocalLightning`,
`PlayLightningNetworked` (hook only), `AzureEffectsController.thunderSettingsList`; the Blood Moon's hunt speed.

**To map.** What each weather changes in play beyond the sky (rain and wind sound, fog, snow), only where a mod would
read it: the readers of `LevelValues.weather` and `Difficulty.actualWeather` besides the storm and the hunt speed.
**Read**. Nice-to-have: no revamp need names weather.

**RE needed.** The callers of the two weather fields' readers.

**Helpers.** `phasmo::weather()`: `Difficulty.actualWeather` paired with the profile by name, per `weather.md`'s "Two
numberings".

**Questions an author asks.**

```cpp
// What weather is the contract, as the hunt reads it?
int32_t weather = phasmo::Difficulty::actualWeather.get(phasmo::difficulty());  // helper
bool bloodMoon = weather == phasmo::WeatherValueType::BloodMoon();

// What does the sky show now (a monkey paw may have changed it)?
void* profile = phasmo::LevelValues::weather.get(phasmo::LevelValues::get_Instance());
```

## 8. The journal

**Mapped already.** All of `JournalController.h` (`sdk/systems/journal.md`, `evidence.md`): pages, tabs, the ghost list
and the guess (`GetGhostType`, `ghostButtons`, `ghostSync`, `UnselectGhost`), evidence marks (`evidenceButtons`,
`AddEvidence`, `FilterOutEvidence`, `RemoveEvidence`, `SetGhostTypes`, `evidenceSync`), opening (`OpenCloseJournal`,
`open`, `JournalRigController`), the photo page (`Journal_PhotoPage.photos`, `videos`, `sounds`, `JournalMedia.AddMedia`,
`RemoveMedia`, `media`, `filled`, `Media.value`, `MediaValues.evidenceType`, every `MediaSubject` value),
`PlayersList`; the scene objects in `Scene.h`; texts (`LocalisationSystem.h`); `sdk/api/journal.h` (open or not);
the host's content journal lines (`PmHostApi::journal_lines`).

**To map.** A photo's reward (its stars and money) and the photo page's total; who took each media item; upgrade
`Media`, `JournalMedia`, `MediaValues` from inferred. **Read**, **hook**.

**RE needed.** The photo camera's capture and its subject scoring; the reward screen's photo line (`RewardManager`
builds rows).

**Helpers.** `phasmo::guessed_ghost()` (`local_player()`, `Player.journal`, `JournalController.GetGhostType`);
`phasmo::marked_evidence()` (the evidence buttons' states).

**Questions an author asks.**

```cpp
// Which ghost did the local player pick in the journal?
int32_t guess = phasmo::guessed_ghost();  // helper

// Is the journal open? (nothing when there is no local investigator)
std::optional<bool> open = pm::api::journal::open();

// How many photos has the team taken, and of what?
phasmo::Journal_PhotoPage::Ref page = phasmo::JournalController::photoPage.get(journal);
// each JournalMedia's Media.value gives MediaValues.evidenceType, a MediaSubject number
```

## 9. Voice chat and the game's speech recognition

**Mapped already.** All of `PlayerVoice.h` (`sdk/systems/voice.md`): `PlayerVoice.localTap`, `globalTap`,
`CreateGlobalAudioTap`, `VivoxParticipantTap.DoAudioFilterRead`, `VivoxServiceInternal.MuteInputDevice`,
`UnmuteInputDevice`, `VoiceVolume.deathMultiplier`; `PlayerAudio.loudness`, `IsTalkingLoudly`,
`GetPlayerVoiceAudioSource`; `sdk/api/talk_loudness.h`; the recognition's handler
`PhraseListenerController.OnPhraseRecognized(string)` and `NetworkedReaction` (inferred); `GhostInfo.name`.

**To map.**
- Push-to-talk, the voice mode, and the walkie's press, per player (voice.md lists them as unmapped). **Read**.
- The speech recognition itself: the recognizer's phrase lists (the Ouija board's questions, the spirit box's
  questions, the ghost's name and the provoking phrases) and the controller that runs it (`SpeechRecognitionController`,
  named in `OnPhraseRecognized`'s note). The Echo's revamp loop shows heard phrases as text to pick from (user,
  ledger, 2026-11-13), so the phrase text and its list are a revamp need. **Read**, **hook**.
- The spirit box's and the Ouija board's own phrase handlers (which phrase they act on). **Hook**.
- `NetworkedReaction`: upgrade from inferred. **Hook**.

**RE needed.** `SpeechRecognitionController` and its grammar or keyword list; the readers of recognised phrases besides
`PhraseListenerController`; `PlayerVoice`'s press fields.

**Helpers.** `pm::api::events::on_phrase(player, text)` (local player only: recognition runs on each client for its own
player, per voice.md). Anything that keeps speech sets `recordsVoice` in the manifest.

**Questions an author asks.**

```cpp
// What did the local player just say?
pm::api::events::on_phrase([](std::string_view text) {});  // helper, over PhraseListenerController.OnPhraseRecognized

// How loud is this player talking?
float loudness = phasmo::PlayerAudio::loudness.get(phasmo::Player::audio.get(player));

// Is this player pressing push-to-talk?
bool talking = phasmo::PlayerVoice::pressed.get(voice);  // to map
```

## 10. Everything the bundled mods use

Every entry the bundled mods (Ghost Tells, Spectral Flame, Custom Ghosts, Playable Ghost, Emotes, Dev Menu) use is a
public map entry today: no bundled mod declares an entry of its own (a grep for `EntryDesc` in `mods/` finds none). The
only entries a mod cannot name are the host's own steering entries (`pm::map::kSteeringEntries`), by design, behind
`pm::api::AgentSteering`.

What is not public is the bundled mods' shared helper layer, `core/game.h` and `core/players.h`: about 60 functions
(`alive`, `transform_of`, `position`, `ghost_ai`, `local_player`, `each_player`, `room_at`, `level_rooms`, `raycast`,
`ghost_sees`, `each_running_camera` and more), typed `void*` and documented as plain comments.

**To map.** Nothing in the map. **To do:** move the helpers a community author needs into `sdk/api` with typed handles
(the shared helpers above, plus `raycast`, `ghost_sees`, `distance_to_room`), leaving `core/game.h` as thin calls to them.
The Unity basics (`set_position`, `set_active`, `child` and the like) stay plain wrappers an author can call through
`Transform.h` and `UnityObject.h` directly.

**RE needed.** None.

**Questions an author asks.**

```cpp
// Does the ghost's own vision see this player now?
bool seen = phasmo::ghost_sees(phasmo::current_ghost(), player);  // helper (from core/game.h)

// What does a ray from the camera hit?
phasmo::Transform::Ref hit = phasmo::raycast(origin, direction, 3.0f);  // helper (from core/game.h)
```

## 11. The contract's lifecycle and the hook points

**Mapped already.** `GameController.allPlayersLoaded`, `ExitLevel.Exit`, `ExitLevel.StartAttemptExitLevel`,
`GameController.Exit`, `GameController.LoadBackToMenu`, `PauseMenuController.Leave`, `Quit`, `LoadingController`,
`Network.sceneChanging`, `LevelValues.completed`, `eventsBlocked` (`sdk/systems/contract.md`); `GhostController.Spawn`,
`CreateGhost`, `GhostAI.Init`; `GhostAI.Hunting`, `GhostHuntState.Started`, `Exit`, `GameController.ResetHuntEffects`;
`Player.NotifyOfDeathStart`, `CompleteDeathSequenceNetworked`; `LiftButton.NetworkedUse`.

**To map.**
- The loaded event `GameController.allPlayersLoaded`'s coroutine raises, and GameController's two hunt events
  (`ghost-ai.md`: `Hunting` "invokes one of GameController's events" at a start and another at an end). **Hook**: a mod
  subscribes to the game's own events where the game has them, rather than hooking a method.
- What "the contract started" is: the setup phase's end, or the truck's first use. **Read**.
- Ghost events and appearing as hook points: section 17.

**RE needed.** GameController's events and their fields; `SetupPhaseController`'s timer and what ends the setup phase.

**Helpers.** The events header above (`sdk/api/events.h`). The example mod `sdk/examples/hunt_watch/` (Task 17 Step 4)
is built on it.

**Questions an author asks.**

```cpp
// Log each hunt's start and end, with every player's room and sanity at the start.
pm::api::events::on_hunt([](phasmo::GhostAI::Ref ghost, bool started) {  // helper
  if (!started) return;
  phasmo::each_player([](phasmo::Player::Ref p) {                          // helper
    log(phasmo::room_of(p), phasmo::sanity_of(p));                         // helper
  });
});

// Run once every player has loaded into the contract.
pm::api::events::on_contract_loaded([] {});  // helper, to map

// Know when a player is killed (the kill begins a second or two before the mark).
pm::api::events::on_death_start([](phasmo::Player::Ref victim) {});  // helper
```

## 12. Difficulty and contract settings

**Mapped already.** All of `Difficulty.h` (`sdk/systems/difficulty.md`): the `Difficulty` object, its preset
(`Difficulty.difficulty`), every custom setting (`sprinting`, `playerSpeed`, `startingSanity`, `sanityDrain`,
`sanityPillRestorationAmount`, `flashlights`, `changingFavouriteRoom`, `interactionAmount`, `eventFrequency`,
`huntDuration`, `gracePeriod`, `fingerprintChance`, `fingerprintDuration`, `killsExtendHuntDuration`, `roamingFrequency`,
`ghostSpeed`, `setupTime`, `selectedWeather`, `actualWeather`, `amountOfDoorsOpen`, `amountOfHidingPlaces`,
`sanityMonitor`, `activityMonitor`, `fuseBoxVisibleOnMap`, `fuseBoxStartPower`, `cursedPossessionsQuantity`,
`actualCursedItems`, `loseItemsOnDeath`, `evidenceGiven`, `friendlyGhost`, `ghostTypeOverride`, `overrideMultiplier`),
`GetRewardMultiplier`, the menu (`DifficultyUIManager`), the sync (`SyncDifficultyValues`, hook only), `LevelValues`,
`DifficultySettings.AddHuntDuration`, `ResetHuntGracePeriod`.

**To map.** The value enums' names where a setting is a number with meanings (for example `eventFrequency` low to high:
`DifficultyValueType` is mapped; which settings use it is in each note or **to map**); the map choice (section 4).
**Read**.

**RE needed.** For each setting, its readers in play, only where a round needs a number (most are already cited by the
rule that reads them, such as `Difficulty.ghostSpeed` in `GhostHuntState.UpdateSpeed`).

**Helpers.** `difficulty()`.

**Questions an author asks.**

```cpp
// How long does a hunt last, and how many evidence does this contract give?
phasmo::Difficulty::Ref d = phasmo::difficulty();  // helper
int32_t hunt = phasmo::Difficulty::huntDuration.get(d);
int32_t evidence = phasmo::Difficulty::evidenceGiven.get(d);

// Is this a friendly ghost's difficulty (no kills, no hunts)?
bool friendly = phasmo::Difficulty::friendlyGhost.get(d);
```

## 13. UI widgets mods reuse

**Mapped already.** `UnityUI.h` (Button, Toggle, Selectable, Image, RawImage, TMP_Text, layout groups, ScrollRect,
Canvas, CanvasGroup, EventSystem), `Doozy.h` (UIButton and its behaviours), `UnityEvents.h`, `Input.h` (actions,
bindings, cursor, mouse), `MainMenu.h` (`ErrorManager`, the popup), `ServerManager.h` (the lobby board),
`sdk/systems/menus.md`; `sdk/api/buttons.h`, `menu_rows.h`, `keybinds.h`, `settings.h`, `input_device.h`; the popup
through `pm::mod::show_message`.

**To map.** Only what a round needs for the revamp's surfaces (the truck monitor's pages, the journal's), under the
vanilla-UI-only rule. Nice-to-have, last.

**RE needed.** None now.

**Questions an author asks.**

```cpp
// Show the game's own popup.
pm::mod::show_message(pm::i18n::text("title"), pm::i18n::text("body"));

// Make a graphic a button that hovers and clicks as the game's do, and handle its click.
void* button = pm::api::buttons::attach(graphic);
pm::api::buttons::on_click(button, &on_pressed, this);
```

## 14. The loader's services

Checked against `core/mod_api.h` (contract 2.6) and `sdk/api`:

| Service | Exists? | What there is | To do |
| --- | --- | --- | --- |
| Settings | Partly | `sdk/api/settings.h`: float sliders on the audio page, saved to a file the mod names, local only | typed settings (bool, choice, key) in the BepInEx `Config.Bind` / SMAPI `ReadConfig` shape, saved per mod; **and the lobby question below** |
| Data folder | Partly | `PmHostApi::dataDirectory` is `%LOCALAPPDATA%\PhasmoMods`, shared; mods build `<data>\<mod id>\` themselves (Dev Menu, Emotes) | a helper `pm::mod::data_folder()` giving `<data>\<mod id>\`, made on first ask, as SMAPI's `DirectoryPath`; the plan's path was `data\<mod id>\`, which the user picks between |
| Frame time | No | `pmmod_tick` takes no time; `Time.get_frameCount` and `Time.get_timeScale` are mapped, `Time.get_deltaTime` and `Time.get_unscaledDeltaTime` are not | map `Time.get_deltaTime`, `get_unscaledDeltaTime`, `get_time`, `get_unscaledTime` (Unity's documented API), and a helper `pm::api::frame_time()`; the plan's "new contract member" is then unneeded, since the map gives it (a question for the user) |
| Log levels | No | `PmHostApi::log(line)` and `log_exception` | a new contract member `log_level(level, line)` at the end of `PmHostApi`, levels trace, debug, info, warn, error, alert (SMAPI's `LogLevel`); `log` stays info |

**The lobby question** (the plan asks for the user's answer with this spec): a setting that changes game rules (a
ghost's speed, a tell's strength) must agree across a lobby, or two clients play different rules. The options:

- **A. The host's value holds in the host's lobby.** A setting declares itself `rules` (it changes what other players
  see) or `local` (volume, keys). The loader sends the host's `rules` values to each verified player on joining and
  again when the contract starts, as the game sends `LevelValues.difficulty` to a joiner and applies the difficulty to
  everyone; a client plays with them while in that room and goes back to its own after leaving. The Mods panel shows the
  host's values greyed while in a room. This follows the game's own difficulty and the rule that the host's list is the
  lobby's list.
- **B. Mismatch is a mismatch.** A `rules` value that differs from the host's counts as another build of the mod: the
  joiner is shown the host's values and removed, as for a missing mod. Simple, but players must copy settings by hand.
- **C. No rules settings.** Settings may only be `local`; a mod that wants a rules choice exposes it through the game's
  custom difficulty or not at all.

A follows the user's 2026-11-03 rules most closely and keeps one mechanism (host decides). It needs a loader message
type, which the loader already owns. The user chooses.

## 15. The reverse-engineering catch-up the plan names

| Name in the plan | Mapped now? | What is there |
| --- | --- | --- |
| `Loadout.AddToLobby` | No | `Loadout` and `Loadout.EquipLoadout` are mapped; the lobby's pool side is (`InventoryManager.AddLoadoutToLobby`, `AddLoadoutItemToLobby`, both inferred; `InventoryEquipment.AddToLobbyNetworked`, `CanAddToLobby`). Map it only if a round relies on it. |
| `Loadout.AddToEquippedLoadout` | No | Nothing. Map only if relied on. |
| `PhotonNetwork.RPC` | No | `PhotonView.RPC` and `PhotonView.RPC(Player)` are mapped. No mod may send a game RPC by name, so this is mapped only if the host's own checks need it; the logistics scan already treats it as a send path (1abaae6). |
| `PhotonNetwork.NetworkingClient` | No | `LoadBalancingClient` and `LoadBalancingClient.OnEvent` are mapped, reached another way. Map the getter only if relied on. |
| `NetworkedStamina.NetworkSyncSprint` | No | `NetworkedStamina`, `moving` and `ForceOutOfBreatheLocal` are mapped; `GhostSenses.h`'s comment names the sync method without an entry. Map it as a hook point if the revamp's Dayan or Kormos loop needs "a player started sprinting" on other clients. |
| The ~169 addresses cited in the task reports and reviews (about 100 unmapped) | Unknown | The reports are gitignored maintainer history. One audit round sorts them: each address a mod relies on gets an entry; the rest are listed as not needed. |

## 16. Navigation for the ghost that leaves the building (Task 18, held)

**Mapped already.** `NavMesh.SamplePosition`, `CalculatePath`, `NavMeshPath`, `NavMeshAgent` (`SetDestination`,
`CalculatePath`, `Warp`, speeds, `get_remainingDistance`, `get_pathPending`, `get_destination`, `get_velocity`),
`DynamicNavMeshObstacle`, `RoomKind.exterior`, `exteriorRoom`, `insideRoom`, `LevelRoom.OnTriggerEnter`,
`LevelController.outsideRoom`, `IsPointInOutsideRoom` (`sdk/systems/navigation.md`, `doors.md`).

**To map** (the plan's Task 17 list for Task 18): `NavMeshSurface` (constructor, `BuildNavMesh`, `AddData`,
`RemoveData`, `navMeshData`, the setters for `collectObjects`, `center`, `size`, `useGeometry`, `layerMask`,
`agentTypeID`, `defaultArea`); `NavMeshLink` (constructor, `startPoint`, `endPoint`, `width`, `bidirectional`,
`agentTypeID`, `area`, `UpdateLink`); `NavMeshAdder`; `NavMesh.RemoveNavMeshData`, `Raycast`, `FindClosestEdge`;
`NavMeshAgent.get_pathStatus`, `ResetPath`, `set_isStopped`, `set_updatePosition`; `LevelController.SyncCurrentGhostRoom`;
and the hunt-outdoors checks. **Call**, **read**.

**RE needed.** As the plan says; and which clients run the ghost's agent.

This is held with Task 18 until the user says to start it, so it is the last round.

---

## 17. The Playable Ghost revamp's needs

The revamp (user, 2026-11-13) rebuilds Playable Ghost as a per-ghost gameplay loop once the SDK is fully mapped. Each
type keeps the tells investigators use, and its personality, derived from its vanilla rules, shapes its loop. So the
revamp needs every vanilla rule of every type, and every lever a ghost pulls. One row per need. "Mapped" gives the keys;
"to map" gives the RE.

### Vanilla type rules that `vanilla-ghosts.md` says are not mapped

| Type | Need | Status | Keys, or RE needed |
| --- | --- | --- | --- |
| Wraith (1) | teleports to a player | to map | `GhostActivity.GhostAbility`'s branch for the Wraith (its target pick, the warp, the EMF it leaves) |
| Wraith (1) | never steps in salt | to map | `SaltSpot.SyncSalt` is mapped and says "unless the ghost's type ignores salt": read which type its test names, and the salt sensor that calls it |
| Phantom (2) | sanity drain while a player looks at it | to map | the looking test (which of a player's cameras, what range) and its `PlayerSanity.ChangeSanity` call; game knowledge, to verify by RE |
| Phantom (2) | a photo makes it vanish | to map | the photo camera's capture and its call to `GhostAI.UnAppear` or the event's end |
| Phantom (2) | blinks less in hunts | to map | `GhostAI.Update`'s blink timer while `GhostAI.isHunting` (its note says the model blinks on a timer), by type |
| Jinn (5) | faster with the power on and its target far | to map | `GhostHuntState.UpdateSpeed`'s Jinn branch (distance, `FuseBox.powerOn`) |
| Jinn (5) | its sanity-drop ability | to map | `GhostActivity.GhostAbility`'s Jinn branch and the drain it applies (game knowledge, to verify by RE: near the fuse box) |
| Jinn (5) | never turns the power off | to map | the type test before `GhostState.fusebox` (in the idle roll or `FuseBox.GhostUse`) |
| Revenant (7) | fast with sight, slow without | to map | `GhostHuntState.UpdateSpeed`'s Revenant branch (`target`, `searchTarget`) |
| Yokai (12) | voices provoke it | mapped | `GhostIdleState.TypeHuntRules` (hunt line 80 while `LevelRoom.ArePlayersTalkingInRoom` in its room, with a chance bonus) |
| Yokai (12) | hears only near it in hunts | mapped | `GhostHuntState.Hears` (2.5 m); its electronics range in `GhostHuntState.SensesGear` is to verify by RE |
| Hantu (13) | speed by temperature | to map | `GhostHuntState.UpdateSpeed`'s Hantu branch (`LevelRoom.temperature` of `LevelController.currentGhostRoom`) |
| Hantu (13) | freezing breath with the power off | to map | the breath effect's class and its test of `FuseBox.powerOn`; game knowledge, to verify by RE |
| Hantu (13) | never turns the power on | to map | the type test before `GhostState.fusebox` |
| Demon (9) | its hunt line | mapped | `GhostIdleState.TypeHuntRules` (70) |
| Demon (9) | its shorter hunt cooldown | to map | who reads `GhostAI.postHuntCooldown` and how long it lasts by type (only its set is mapped) |
| Demon (9) | its wider crucifix range | to map | the crucifix's range test by type (section 6) |
| Demon (9) | its smudge hold | mapped | `GhostAI.StopGhostFromHunting` |
| Oni (11) | never the airball event | to map | `GhostAI.EventPick`'s event list and the Oni's remap (`GhostState.randomEvent`'s note: the Oni and Shade remap events 1, 3 and 4) |
| Banshee (4) | its singing event | to map | the singing event state among `GhostAI.EventPick`'s five |
| Banshee (4) | its scream on the parabolic microphone | to map | the paranormal sound pick by type and the parabolic microphone's pickup |
| Moroi (21) | the spirit box curse | to map | the spirit box's answer path and the curse's drain multiplier on `PlayerSanity.Update` |
| Deogen (22) | speed by distance to its target | to map | `GhostHuntState.RefreshBase` reads "a field of its GhostAI" for the Deogen: map that field and its writer |
| Deogen (22) | breathing on the spirit box | to map | the spirit box's answer branch by type (`MediaSubject.spiritBoxBreath` is mapped as a media kind) |
| Obake (19) | six-finger prints | to map | `Fingerprint.AttemptToSpawnFingerprints` (mapped) and the print variant pick (`MediaSubject.obakeFingerprint` is mapped) |
| Obake (19) | shapeshift | to map | the model swap in an event or hunt (`MediaSubject.obakeShapeshift` is mapped; `GhostAI`'s two GhostModel arrays have no entries, per ghost-ai.md) |
| Raiju (18) | electronics range | partly | `GhostActivity.electronicsSensor`, `nearElectronics`, `UpdateElectronics` (inferred), its hunt line in `TypeHuntRules` (65 near electronics); to map: the sensor's range setting and the speed branch in `UpdateSpeed` |
| The Twins (17) | two speeds | partly | `GhostHuntState.UpdateSpeed`'s offset of 0.2 or -0.2 set as the Twins' hunt begins (ghost-ai.md, "Hunt speed"); to map: the offset's field and which twin hunts; `GhostActivity.TwinInteraction` is mapped |
| Myling (15) | more sounds on the parabolic microphone | to map | the paranormal sound frequency by type; `Footstep.Play` (its quieter steps) is mapped |
| Mimic (20) | ghost orbs | mapped | `EvidenceController.SpawnAllGhostTypeEvidence`, `ForceRespawnGhostOrb`, `ghostOrb`, `EvidenceType.GhostOrb` |
| Poltergeist (3) | throws many props at once | to map | `GhostActivity.GhostAbility`'s Poltergeist branch; its throw rules (`InteractWithARandomProp`, `GhostHuntState.throwTimer`) are mapped |
| Poltergeist (3) | sanity drop per prop thrown | to map | the drain in that branch (game knowledge, to verify by RE) |
| Yurei (10) | its ability's sanity drain | to map | `GhostActivity.GhostAbility`'s Yurei branch (game knowledge, to verify by RE: a door shut and a drain nearby) |
| Gallu (25) | what moves its rage | to map | the writers of `GhostController.galluState` (game knowledge, to verify by RE: protections used against it) |
| Dayan (26) | beyond what is mapped | partly | `GhostController.dayanState`, `GhostAI.DayanPlayerChecks`, `NetworkedStamina.moving`, its hunt line; to map: its speeds in `UpdateSpeed`, what `DayanPlayerChecks` measures (range, which player) |
| Obambo (27) | beyond what is mapped | partly | `GhostAI.obamboAggressive`, its hunt lines (65, 10); to map: its switch timer and its speeds |
| Kormos (28) | beyond what is mapped | partly | its event array, chase, `PlayerProcessor`, hearing (`GhostHearing`, footsteps in hunts); to map: the player flag `PlayerProcessor` reads, and the room condition in `TypeHuntRules` |
| Deildegast (30) | beyond what is mapped | partly | `GhostActivity.lightChance`, `InteractWithARandomProp`, `GhostAI.movedProps` (inferred); to map: the speed by moved props (inferred only) |
| Aswang (29) | everything | to map | no rule is mapped: its branches in `TypeHuntRules`, `UpdateSpeed`, `RefreshBase`, `GhostAbility`, `EventPick`, `PickDestination`, `GhostHuntState.Sees`, the evidence pick; the user's loop for it waits on this (ledger, 2026-11-13) |

### Sanity and players

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| Per-player sanity | mapped | `Player.sanity`, `PlayerSanity.insanity` |
| Every way it drops, as it happens | to map | the callers of `PlayerSanity.ChangeSanity` and `SetInsanity` (section 3) |
| Per-player position | mapped | the body's transform (`Component.get_transform`, `Transform.get_position`), `Player.body` |
| Per-player room | mapped | `Player.currentRoom`, `LevelController.rooms` with `LevelRoom.roomColliders` |

### Ghost events and appearing

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| The events, each kind | to map | `GhostAI.EventPick` "starts one of five event states" (its note): each state's class, by what it does, and its kind number |
| Start an event of a chosen kind | to map | `EventPick(bool, int)` takes an event number, which `ChangeState` passes as -1: read whether a chosen number is honoured and whether calling it outside `ChangeState` keeps the game's state bookkeeping; if not, a chosen kind needs `ChangeState(randomEvent)` with a hook on `EventPick` that picks the number |
| Appearing | mapped | `GhostAI.Appear(int form)`, `MakeGhostAppear` (RPC, hook only), `isAppeared`, `UnAppear`, `GhostState.appear` |
| The forms of an appearance | to map | the `int` `Appear` takes (the Shade and Oni coin may change it): its values by what the ghost shows |
| The singing event | to map | one of the five event states (Banshee row above) |
| The game's player-shaped event | to map | `GhostController.GhostEventPlayer` (ghost-ai.md: an agent, a noise, particles, a photo evidence, a target; spawned by RPC; none mapped); and whether any game code runs it (community notes say not) |
| The sanity cost of an event | to map | the event states' drain calls (section 3) |

### Interactions

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| Door slam | mapped | `Door.GhostForceDoor`, `CreateDoorSlamNoise`, `PlayDoorSlamNoise`, `GhostOpenDoorEvent`; a slam is a ghost interaction with its traces (doors.md) |
| Door hold | mapped | `Door.GhostForceDoor` keeps the push "there" (its note), `Door.forceRoutine` |
| Door lock | mapped | `Door.LockDoor`, `LockDoorForTime`, `LevelController.LockHuntingDoors` (doors.md, "Locking a door yourself") |
| Lights per room, and whether a room is lit | mapped | `LevelRoom.lightSwitches` (inferred), `LevelRoom.AreRoomLightsOn` (inferred), `LightSwitch.get_isOn`, `UseLightSwitch`, `FlickerNetworked` |
| The breaker | partly | `FuseBox.powerOn`, `GhostUse`, `BreakFuseBox`, `outOfOrder`, `GhostState.fusebox`; to map: a player's use and the event of the power changing |
| Electronics: radios, TVs, car alarms, phones | to map | each class and the ghost's use (`GhostState.carAlarm` is the only mapped value; `GhostActivity.UpdateElectronics` is the Raiju's sensor) |
| Sinks | mapped | `Sink`, `Sink.water`, `Sink.used`, `MediaSubject.dirtyWater` |
| Knocks and ghost sounds | to map | `GhostState.doorKnock`, `windowKnock` values are mapped; the sound plays (GhostAudio) and the paranormal sound pick are not |
| Windows: prints, breath, knocks | to map | the window class, its fingerprints (`Fingerprint` is the general class), its knock (`GhostState.windowKnock`, `EMFType` has windowKnock per its note) |
| Throw a chosen prop | mapped | `Prop.TryThrow(bool, bool)`, `GhostActivity.GetPropToThrow`, `throwable`, `GhostInteraction.CreateThrowingEMF` |
| With a chosen force | to map | the force inside `Prop.TryThrow` (its two flags; whether a strength field exists) |
| Move a prop as the ghost (possession) | mapped | the pattern in navigation.md, "Moving a thing along the floor" (`PhotonView.RequestOwnership`, `Rigidbody.AddForceAtPosition`, `Prop.PlayImpactSound`), with `GhostInteraction.CreateThrowingEMF` for the trace; whether the game counts it as an interaction (`LevelStats`) is to map |

### Cursed possessions, one by one

| Possession | Status | Keys, or RE needed |
| --- | --- | --- |
| Ouija board | partly | `OuijaBoard.PlayMessageSequence` (inferred), `BreakBoard`; to map: the question heard, the answer chosen (the "answer coroutine" players.md mentions), the sanity cost |
| Music box | partly | `MusicBox.UseNetworked` (inferred), `tween`, `BreakMusicBox`, `EnterMusicBoxState`, `GhostState.musicBox`; to map: the ghost's singing and walk to the box, the hunt trigger's range |
| Tarot cards | partly | `TarotCards.ChangeState` (inferred), `HighPriestess`, `HangedMan`; to map: the card numbers by name and each card's effect |
| Haunted mirror | partly | `HauntedMirror.UseNetworked` (inferred), `BreakItem`; to map: the sanity drain while used, the room it shows (the favourite room, game knowledge, to verify by RE) |
| Monkey paw | partly | `MonkeyPaw.MakeWishNetworked` (inferred), `ReviveWish`, `OpenExitsWish`; to map: each wish's handler by wish ("which handler is which wish was not traced", ghost-ai.md) and the wish list |
| Summoning circle | partly | `SummoningCircle` (inferred), `GhostState.summoningCircle`; to map: the candles lit, the teleport, the forced hunt |
| Voodoo doll | partly | `VoodooDoll.NetworkedUse` (inferred), `StartHunt`; to map: the pin numbers and their interactions (`GhostActivity.NormalInteraction` with force) |
| What the ghost may answer | to map | for each: whether the ghost's side (`GhostAI.ChangeState` or `NormalInteraction`) is reached, so a player ghost can answer; the Ouija board's answer is a string chosen by the game |

### Evidence placing, and its timing

| Evidence | Status | Keys, or RE needed |
| --- | --- | --- |
| EMF 5 | partly | `GhostInteraction.CreateThrowingEMF`, `CreateInteractionEMF`, `CreateAppearedEMF`, `EMF`, `EMFType` (inferred); to map: the level each reading gets and the chance of 5 when the ghost gives EMF |
| Ultraviolet (prints) | partly | `Fingerprint.AttemptToSpawnFingerprints`, `EvidenceController.IsFingerPrintEvidence`, `Difficulty.fingerprintChance`, `fingerprintDuration`; to map: where prints spawn (doors, switches, windows, keyboards) and the salt footprints |
| Ghost writing | to map | the book's class and the ghost's write (an interaction?) |
| D.O.T.S. | partly | `GhostState.dots`, `GhostDotsState` (`GoToNearest`, `radius`, `destination`); to map: what shows the ghost in the projector's light and for how long |
| Ghost orbs | mapped | `EvidenceController.SpawnAllGhostTypeEvidence`, `ghostOrb`, `ForceRespawnGhostOrb` |
| Freezing | partly | `EvidenceController.freezing`, `LevelRoom.temperature`; to map: the room cooling rule (section 5) |
| Spirit box answer | to map | the spirit box's answer roll and its conditions (lights, distance, players), section 6 |
| Timing | to map | when each evidence can first appear (setup, hunts), and the game's rate for each; the revamp lets the ghost player choose when to give evidence within a time limit (CLAUDE.md), so the vanilla rates are the baseline |

### Protections, photos and who looks

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| Crucifix used | partly | `Crucifix.CrucifixUseNetworked` (inferred), `useCount`; to map: the burn that prevents a hunt and its range |
| Smudge | mapped | `WhiteSage.UseNetworked` (inferred), `GhostAI.StopGhostFromHunting`, `smudge`, `huntsHeldOff`, `WhiteSage.YureiTrap` |
| Salt | mapped | `SaltSpot.SyncSalt` (hook only), `SaltSpot.flatSalt` |
| Flames | mapped | `FireSource.BlowOutFire`, `IgniteFire`, `IgniteFireNetworked`, `CountGhostBlowOut`, `GhostAI.flamesBlownOut` (flames.md) |
| A photo of the ghost | to map | the photo camera's capture and its subject (`MediaSubject.ghost` is mapped) |
| Video of the ghost | partly | `CCTVController.viewers`, `GetCCTVCamerasByCurrentCameraType`, `core/game.h`'s `each_running_camera`; to map: whether a camera films the ghost (the game's test, if it has one) |
| Who is looking at the ghost | to map | the game's own test, if any (the Phantom's drain needs one); otherwise a helper over `Camera.WorldToViewportPoint` and `Physics.Raycast`, marked as not the game's |

### Hunts

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| Start | mapped | `GhostAI.ChangeState(hunting)`, `GhostAI.Hunting` (hook only), `GhostIdleState.Execute`, the hunt lines, `GhostHuntState.Started`, `startDelay` |
| End | partly | `GhostHuntState.Exit`, `GameController.ResetHuntEffects`, `LevelController.UnlockHuntingDoors`; to map: the time limit `Started` tests ("when its time is up"), `Difficulty.huntDuration`'s use, `killsExtendHuntDuration` |
| Target | mapped | `GhostHuntState.target`, `searchTarget`, `SetTarget`, `SightPick`, `TargetPick`, `StartPick`, `Chase`, `KeepNearest`, `TargetAllows`, `lastPlayer`, `seen` |
| Speed | mapped (base) | `GhostHuntState.UpdateSpeed`, `RefreshBase`, `baseSpeed`, `sightSpeedUp`, the content `huntSpeed` rule; each type's branch is in the type table above |
| Kill | mapped | `GhostHuntState.Kill`, `killReach`, `slowKillReach`, `killed`, `Player.BeginDeathSequence` |
| Hiding-place search | mapped | `GhostHuntState.SetCloset`, `searchCloset`, `ClosetZone.CheckCloset`, `navSpot`, `standOutsideEntrancePoint`, `Player.closet` |
| Holding a caught player | to map | what the kill does with the ghost and the victim between the catch and the death (the choke, ghost-ai.md "A hunt's choke is the ghost's model during its kill"): the animation and its length |
| Grace period | partly | `DifficultySettings.ResetHuntGracePeriod`, `Difficulty.gracePeriod`; to map: the grace's own field and reader |

### Dead players, objectives, temperature

| Need | Status | Keys, or RE needed |
| --- | --- | --- |
| A dead player's state | mapped | `Player.isDead`, `dying`, `LocalPlayer.isDead`, `VoiceVolume.deathMultiplier`, `WeatherType.dead`, `Layer.DeadPlayer` |
| What a dead player can do | to map | the dead player's movement and what they may use (`LightSwitch.UseLightSwitch`'s second argument "lets a dead local player use it", per its note); game knowledge, to verify by RE: dead players see the ghost and walk through walls |
| The optional objectives: their list | partly | `LevelValues.sideObjectives`, `mainObjective`, `Objective`, `ObjectiveValues.type` (read as an int); to map: the objective kinds by name |
| Their progress | partly | `Objective.state`, `ObjectiveManager.CompleteObjective`, `FailObjective`, `HasSetupObjectives`; to map: progress counts (for example a smudge or a photo objective's own test) |
| Temperature per room | mapped (read) | `LevelRoom.temperature`; the cooling rule is to map (section 5) |

---

## Mapping rounds, in order

Each round is one implementer task: map, prove by RE, test against the fake backend, document in `sdk/systems/<area>.md`,
add its question snippets to `tests/coverage_questions_test.cpp`, run `.\validate.ps1` and
`.\tools\gamecheck.ps1 -IfConfigured -Expect`, commit. The basic lookups come first, then the revamp's needs, then the
nice-to-haves. Rounds 1 and 2 need Task 14 Step 0b (typed handles) merged first.

1. **Basic lookups.** The shared helpers (`sdk/api/world.h`), typed, documented; `core/game.h` and `core/players.h` call
   them; promote `raycast`, `ghost_sees`, `distance_to_room`; fix the three lagging docs. No RE.
2. **Lifecycle events.** `sdk/api/events.h` over the mapped hooks; RE GameController's loaded and hunt events, the
   setup phase's end, and the `Revive` body; `sdk/examples/hunt_watch/` and `sdk/guides/reading-the-game.md` can follow.
3. **Sanity.** Every caller of `ChangeSanity` and `SetInsanity` as a cause; `SetInsanity` decompiled; the pill;
   `on_sanity_changed`.
4. **Ghost events and appearing.** The five event states, the kind numbers, a chosen kind, the forms `Appear` takes,
   the singing event, the Oni's airball rule, `GhostEventPlayer`.
5. **Hunts, complete.** The time limit, hunt duration and kills extending it, the grace period, the hold of a caught
   player, the cooldown by type (the Demon), the blink timer by type (the Phantom).
6. **Hunt speed by type.** Every branch of `GhostHuntState.UpdateSpeed` and `RefreshBase`: Jinn, Revenant, Hantu,
   Deogen's field, the Twins' offset, Raiju, Dayan, Obambo, Deildegast, Aswang.
7. **Abilities by type.** Every branch of `GhostActivity.GhostAbility`: Wraith, Jinn, Yurei, Poltergeist, Obake, and
   the fuse box gates (Jinn, Hantu), the salt test (Wraith), the Gallu's rage writers.
8. **Evidence placing.** EMF levels, prints and their places, the Obake's prints, the writing book, D.O.T.S., freezing
   and the room's cooling (with section 5's temperature), the Hantu's breath, timing.
9. **Equipment, readings and use events.** Section 6's table: the EMF reader, thermometer, spirit box (answers, the
   Deogen's breath, the Moroi's curse), parabolic microphone (the Banshee, the Myling), sensors, crucifix (range, burn),
   incense, photo camera (photos of the ghost, the Phantom's vanish), held item per player.
10. **Cursed possessions.** Each possession's questions, answers, wishes, cards, pins, costs and hunts, and every
    `CursedItemType` value.
11. **Interactions in the world.** Electronics (radio, TV, car alarm, phone), windows, knocks and the ghost's sounds
    (GhostAudio), the breaker's player use and power events, bulbs, the throw's force, the possession pattern counted
    as an interaction, who looks at the ghost (the Phantom's test).
12. **Players and the dead.** In the truck, talking and push-to-talk, crouching, the sprint sync
    (`NetworkSyncSprint`), the Kormos's player flag, what a dead player can do.
13. **The remaining types.** Whatever rounds 4 to 12 left of the type table: the Aswang's remaining rules, the Kormos,
    Obambo and Dayan beyond, the Deildegast's speed.
14. **Objectives and rewards.** Objective kinds by name, progress, the photo rewards and stars.
15. **Voice and speech recognition.** The recognizer's phrase lists and controller, the spirit box's and the Ouija
    board's phrase handlers, `on_phrase`. (Round 10 or 9 may pull this forward if their questions need it.)
16. **Map data II.** The contract's map, its name and size, `GhostInfo.favouriteRoom`, `LevelRoom.name` localised,
    the inferred room members upgraded.
17. **The loader's services.** The data folder helper, frame time, log levels, typed settings, and the lobby answer
    the user chooses.
18. **The catch-up audit.** Section 15's five names (each mapped only if relied on) and the ~169 cited addresses.
19. **Nice-to-haves.** Weather effects, the journal's remainder, UI widgets for the revamp's surfaces.
20. **Navigation for Task 18.** Held until the user starts Task 18.

Then Task 17's Steps 3 to 6: the coverage gate, the example and guide (if not done after Round 2), the smoke list,
the commits.

## Questions for the user

1. **The lobby question for settings** (section 14): A, B or C.
2. **Header names** for the shared helpers and the events (`sdk/api/world.h`, `sdk/api/events.h` here), and whether
   the helpers live in `phasmo::` (the game's namespace, as the plan's `phasmo::current_ghost()`) or `pm::api::`.
3. **The data folder**: `%LOCALAPPDATA%\PhasmoMods\<mod id>\` (what the bundled mods use now) or the plan's
   `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`.
4. **Frame time**: map Unity's `Time` getters (no contract change) as proposed, or add the plan's contract member.
5. **Order**: is it right that the revamp's needs (Rounds 3 to 15) come before the loader's services (Round 17)? The
   services are small and a community author needs them before the revamp does.
6. **The revamp's loops** may name needs this list misses (the per-ghost page's 45 loops). Should one round read the
   recorded loops and add any missing need before Round 3 starts?
