# SDK community coverage (stage 2b Task 17, Step 1)

What the SDK must hold so that a community author writes a mod with no reverse engineering, and so that the Playable
Ghost revamp (user, 2026-11-13) has every game fact it needs. For each area: what the map has now, what is still to
map, the reverse engineering that needs, the small `sdk/api` helpers worth adding, and the questions an author will
ask, each with the snippet that answers it. It ends with the mapping rounds in order and the decisions the user and the
controller made on the spec's questions (2026-11-14). **This spec is the user's to approve before any mapping starts**
(Task 17 Step 1).

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

In a new header, `sdk/api/world.h`, in namespace `phasmo::` (Decisions, 2). Typed handles land first (Task 14 Step 0b),
and Round 1 starts after them (Decisions, 5). Each is
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

Hook points as thin wrappers over hooks of the game's own methods, in a second new header, `sdk/api/events.h` (Decisions, 2).
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
| `on_room_entered(player, room)` | `LevelRoom.OnTriggerEnter` (sets `Player.currentRoom`) | every client, as a player's camera collider enters a room |
| `on_light_used(switch, on, byGhost)` | `LightSwitch.Use` (RPC body, hooked only) | every client, for each use of a switch |
| `on_smudge(incense)` | `WhiteSage.UseNetworked` (inferred: upgrade in Round 16) | every client |
| `on_salt_step(spot)` | `SaltSpot.SyncSalt` (RPC body, hooked only) | every client |
| `on_flame_out(flame, byGhost)` | `FireSource.BlowOutFire` | the master |
| `on_photo(by, subject, stars)` | the photo camera's capture: **to map** (Round 18) | |
| `on_phrase(text)` | `PhraseListenerController.OnPhraseRecognized` | the client whose own player spoke, for every phrase, in a hunt or not (`sdk/systems/voice.md`) |
| `on_item_grabbed(player, item)`, `on_item_dropped(player, item)` | `NetworkedPropGrab`'s grab and drop RPCs: **to map** (Round 17) | every client |

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
- A sound, knock or footstep of the ghost's at a chosen point away from the ghost (revamp, "Sounds at a chosen point").
  **Call**.
- A hunt's warning effects without a hunt (revamp, "Hunts"). **Call**.
- How long an appearance lasts, and ending or extending it (revamp, "Ghost events and appearing"). **Read**, **change**.

**RE needed.** `GhostAI.EventPick` and the event states it starts; the GhostAudio class and who calls its plays; the
`GhostInfo.favouriteRoom` writers; the `GhostState` value for salt (by `SaltSpot.SyncSalt`'s change); where a
`Noise` or a `Footstep` can be placed (both are mapped in `Footstep.h`, the ghost's own); the appearance's timer.

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
  (local only), `PhotonObjectInteract.inUse` (whether something uses an object now, on this client; for a door,
  set by `Door.NetworkedGrabbedDoor` as a player grabs it).
- How they move: `LocalPlayer.walkSpeed`, `sprintSpeed`, `speedMultiplier`, `LocalPCPlayer.SetPlayerSpeedMultiplier`,
  `ResetPlayerSpeedMultiplier`, `PlayerStamina.canSprint`, `NetworkedStamina.moving`, `FirstPersonController.moveInput`,
  `running`, `sdk/api/movement.h`, `Player.ToggleFreezePlayerMovement` (inferred), `PCCanvas.PausePlayerInPlace`.
- Their voice and noise: `Player.audio`, `PlayerAudio.loudness`, `IsTalkingLoudly`, `PlayerNoise`.
- Their body and animation: `Player.animator`, `renderers`, `characterAnimations`, `PlayerCharacter`, `PlayerCosmetics`.
- Their journal: `Player.journal`; their stats: `Player.stats`, `PlayerStats.SyncStats`.
- The truck: `ExitLevel.ThereAreAlivePlayersOutsideTheTruck`, `ExitLevel.trigger`, `ExitLevelTrigger`.

**To map.**
- The item in a player's hand, on every client (not only the local camera slot). **Read**.
- A player picking up, holding and dropping any prop, not only equipment: the grab and drop RPCs of
  `NetworkedPropGrab` (players.md: "The grab and drop RPCs themselves have no map entries"), the holder of a prop, and
  a drop forced through the game's own method (for a door, `Door.ForceDropDoorNetworked` is mapped and hook only).
  **Hook**, **read**, **call**.
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
- **Every way sanity drops or rises**, each as a cause a hook can tell apart. Mapped causes: the darkness drain
  (`PlayerSanity.Update`), the monkey paw's weather wish (7, `sdk/systems/weather.md`) and the Winter's Jest RPC
  (`WintersJestManager.RPC_MakeKrampusStronger` acts on sanity). Game knowledge, to verify by RE: a ghost event, seeing
  the ghost appear, a hunt, the other cursed possessions (the mirror, the Ouija board, the tarot cards), the Jinn's,
  Yurei's and Poltergeist's abilities, the Phantom when seen, the Moroi's curse and the sanity pill. **Hook** (to see
  fear as it happens).
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

// Take 10 sanity from a player, as the game does (the difficulty's limits apply). Which client may change which
// player's sanity, and how the change reaches the others, is to map (Round 6); until then, only on that player's own
// client.
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
- Lights: all of `LightSwitch.h` (`get_isOn`, `get_IsSwitchOn`, `UseLightSwitch`, `Use`, `FlickerNetworked`, `room`, `sources`,
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
- **The map itself**: which map this contract is on, its name as the game shows it, and its size (`GhostActivity.sensorBox`
  and the open doors are set "by the map's size", per their notes; that the size is a class of small, medium and large
  is game knowledge, to verify by RE). **Read**.
- The truck's activity monitor: what feeds it, and how a ghost action shows on it (only the difficulty switch
  `Difficulty.activityMonitor` is mapped). **Read**, **hook**.
- The player's own use of the fuse box (the switch a player flips) and the power going off in a hunt or by the ghost,
  as one event. **Hook**.
- Every `CursedItemType` value by name, not only two. **Read**.
- `LevelRoom.lightSwitches` and `AreRoomLightsOn`: upgrade from inferred. **Read**.
- Light bulbs breaking (game knowledge, to verify by RE: a ghost may burst a bulb). **Hook**.
- Windows and mirrors as objects the ghost acts on: a window's knock (`GhostState.windowKnock` is mapped), its prints,
  and breath fogging a window or a mirror (game knowledge, to verify by RE: whether the game has a fog or breath effect
  on glass at all). If it has none, breath on glass stays in the revamp's design and the user designs its look later;
  the round then maps the glass's materials and any decal surface the game has, so that effect can be applied
  (Decisions, 10). **Call**, **hook**, **change**.
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
- **Changing a room's temperature through the game**: making a room colder (or giving Freezing in it on demand)
  through whatever the game's own cooling goes through, never a write to `LevelRoom.temperature` that the next
  `LevelRoom.Update` overwrites. **Change**.

**RE needed.** `LevelRoom.Update`'s branches and the fields it reads (the "few flags" it returns on), and which of its
inputs a mod can set through a game method; `ResetTemperature`'s difficulty case; `LevelRoom.name`'s writers; the
readers of `LevelRoom.GetPlayersInRoom`.

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
| Spirit box | whether it is on and listening; the question heard; the game's choice to answer, and the answer; a breath sound (`MediaSubject.spiritBoxBreath` is mapped; that it is the Deogen's is game knowledge, to verify by RE); playing audio through its speaker as an answer | hook, read, call |
| Ghost writing book | placed, written in, which page | hook, read |
| D.O.T.S. projector | on, placed, its area; whether the ghost shows in it (`GhostDotsState`) | read, hook |
| UV light and glowsticks | on, charge; which prints it shows | read |
| Video camera | on, night vision (media.md: not mapped), who views the monitor (mapped: `CCTVController.viewers`); which room or point each camera covers, and whether it films the ghost | read |
| Photo camera | a photo taken, its subject and its stars; the photo's flash on the ghost | hook |
| Parabolic microphone | its reading, the sounds it picks up (`MediaSubject.bansheeWail` and `paranormalSound` are mapped media kinds; the Banshee's scream and the Myling's extra sounds on it are game knowledge, to verify by RE) | read, hook |
| Sound sensor, motion sensor | each trigger, with where and when | hook |
| Sound recorder | a recording's subject | hook |
| Crucifix | its range test, a burn (`MediaSubject.usedCrucifix` is a mapped media kind), upgrade `CrucifixUseNetworked` from inferred; that a burn prevents a hunt and that the range differs by type are game knowledge, to verify by RE | hook, read |
| Incense | a smudge, its target (the ghost, in a hunt or not), upgrade `UseNetworked` from inferred | hook |
| Salt | a pile laid; a step in it (`SaltSpot.SyncSalt`, mapped); the footprints | hook |
| Firelight, igniter | mapped (`FireSource`, `IgniteFire`) | |
| Sanity medication | a use and the amount restored | hook |
| Head gear, tripod, torches | on or off (`Equipment.isOn`) is enough unless a round finds more | read |
| Any item | which player holds it, in hand or put away (section 2); which way a held item points, to test whether it is aimed at the ghost | read |
| Any electronic item | the ghost upsetting it outside a hunt: a torch flickering, an item reacting, a placed or held item switched off as the ghost would (`sdk/systems/ghost-ai.md`: "which code flickers the lights and upsets the electronics in a hunt was not traced"; what the ghost does to equipment outside a hunt is game knowledge, to verify by RE) | call, hook |

**RE needed.** Each item class's own `Use` override, its networked use and its fields (the EMF reader's level, the
thermometer's shown value, the spirit box's answer roll, its phrase check and its speaker's AudioSource, the camera's
photo and its subject detection, the sensors' triggers); `EMF`'s fields; the crucifix's range test; the media's star
rating; how a CCTV camera's view is set (its Camera, for a coverage test against `LevelController.rooms`); the hunt's
electronics effect (shared with the hunt-effects round).

**Helpers.** `phasmo::each_item(f)` (every `Equipment` in the level, through `Object.FindObjectsOfType`, with the costly
scan kept as `core/game.h`'s `each_running_camera` keeps it); `phasmo::holder_of(item)`; `phasmo::aimed_at(item, point)`
(the held item's transform against a point, once the held item is mapped; a geometric test, not the game's, said so in
its note); `phasmo::cameras_covering(room)`.

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

**To map.**
- A photo's reward (its stars and money) and the photo page's total; who took each media item; upgrade `Media`,
  `JournalMedia`, `MediaValues` from inferred. **Read**, **hook**.
- **The team's guess**: each player's pick on every client (whether `JournalController.ghostSync`, the guess sync
  `journal.md` describes, carries every pick or one shared pick), and the guess the results judge
  (`ExitLevel.CheckMissions` completes the identify objective "when the journal names the ghost", per `contract.md`:
  whose journal is to map). **Read**.

**RE needed.** The photo camera's capture and its subject scoring; the reward screen's photo line (`RewardManager`
builds rows); `JournalGhostSyncController`'s values and owners; `ExitLevel.CheckMissions`' read of the guess.

**Helpers.** `phasmo::guessed_ghost()` (`local_player()`, `Player.journal`, `JournalController.GetGhostType`), the local
player's pick; `phasmo::guess_of(player)` and `phasmo::judged_guess()` once the team's guess is mapped;
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

**To map.** The surfaces the revamp's screens will use (revamp section, "The ghost player's screens"): every kept idea
tagged as a screen (the minigames, the hiding-place guess, the escape check, the ghost's objectives, loadout and
recap, and the energy every loop spends) is built with `PM_UI_TODO` until the user designs it (Decisions, 7). Round 32
maps only what each screen's logic needs from the game, and records which game surface each screen could use (the
truck monitor's pages, the journal's pages, the lobby board, a cloned widget, the game's popup) as options for the
user. Other UI work stays a nice-to-have.

**RE needed.** For each candidate surface: enough of how the game builds and pages it to say whether a screen fits, as
`menus.md` and `journal.md` do for theirs.

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
| Settings | Partly | `sdk/api/settings.h`: float sliders on the audio page, saved to a file the mod names, local only | typed settings (bool, choice, key) in the BepInEx `Config.Bind` / SMAPI `ReadConfig` shape, saved per mod; **and the lobby rule below** (Decisions, 1) |
| Data folder | Partly | `PmHostApi::dataDirectory` is `%LOCALAPPDATA%\PhasmoMods`, shared; mods build `<data>\<mod id>\` themselves (Dev Menu, Emotes) | a helper `pm::mod::data_folder()` giving `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`, made on first ask, as SMAPI's `DirectoryPath` (Decisions, 3); the Dev Menu and Emotes move their files there, with a one-time move of each player's existing files from `<data>\<mod id>\` |
| Frame time | No | `pmmod_tick` takes no time; `Time.get_frameCount` and `Time.get_timeScale` are mapped, `Time.get_deltaTime` and `Time.get_unscaledDeltaTime` are not | map `Time.get_deltaTime`, `get_unscaledDeltaTime`, `get_time`, `get_unscaledTime` (Unity's documented API), and a helper `pm::api::frame_time()`; no contract member: the map gives it (Decisions, 4) |
| Log levels | No | `PmHostApi::log(line)` and `log_exception` | a new contract member `log_level(level, line)` at the end of `PmHostApi`, levels trace, debug, info, warn, error, alert (SMAPI's `LogLevel`); `log` stays info |

**The lobby rule** (the plan asked for the user's answer with this spec; the user chose A on 2026-11-14, Decisions, 1):
a setting that changes game rules (a ghost's speed, a tell's strength) must agree across a lobby, or two clients play
different rules. The options were:

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
type, which the loader already owns. **A is the rule** (user, 2026-11-14); Round 5 builds it.

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
type keeps the tells investigators use, and its personality, derived from its vanilla rules, shapes its loop. This
section lists every game fact and lever the revamp needs, from three sources: the brief's list; the user's ideas page
(100 ideas, of which the 86 the user marked keep or maybe count, cited by id: e01 to e10 the economy, m01 to m12 the
minigames, v movement, s senses, h haunts, t traps, d evidence, u hunts, y per type, g goals, p other players); and the
user's per-ghost page (one loop per ghost, 45 loops, with the Aswang, Draugr and Hydra as the user revised them; the
ledger of 2026-11-13 holds both pages' links). Ideas that need nothing from the game (energy sums, combos, progression
kept by the loader) have no row.

A row is **mapped** (keys given), **partly** (the mapped keys, then what is missing) or **to map** (the RE needed).
**Round** is the mapping round below that closes it. A behaviour not in a map note or an `sdk/systems` page is written as
"game knowledge, to verify by RE", even where the user's loop states it: the loops describe the game as players know it,
and the SDK records only what the code proves.

### The vanilla types' rules

| Type | Need, and the loop or idea that needs it | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- | --- |
| Spirit (0) | its smudge hold on hunts (Spirit loop) | mapped | `GhostAI.StopGhostFromHunting` (a length of its own) | |
| Spirit (0) | its name heard (Spirit loop, e04) | mapped | `PhraseListenerController.OnPhraseRecognized` on the speaker's client, compared in a hunt with `GhostInfo.name`; `NetworkedReaction` (inferred) on the ghost's client | 24 |
| Wraith (1) | game knowledge, to verify by RE: it teleports to a player and leaves EMF there (Wraith loop) | to map | where the teleport lives (a branch of `GhostActivity.GhostAbility` is the first place to look), its target pick, the warp, the EMF (`GhostInteraction.CreateInteractionEMF` is mapped) | 14 |
| Wraith (1) | game knowledge, to verify by RE: it never steps in salt | partly | `SaltSpot.SyncSalt` "unless the ghost's type ignores salt": read which type the test names, and the salt sensor that calls it | 14 |
| Phantom (2) | game knowledge, to verify by RE: a player looking at it loses sanity faster (Phantom loop, e02) | to map | the looking test (its camera, range and cone) and its sanity call | 28 |
| Phantom (2) | game knowledge, to verify by RE: a photo of it ends its appearance | to map | the photo camera's capture and what it calls on the ghost (`GhostAI.UnAppear` is mapped) | 18 |
| Phantom (2) | game knowledge, to verify by RE: it shows less often in a hunt | to map | `GhostAI.Update` makes the model blink on a timer while `isHunting` (ghost-ai.md): read the timer and any branch by type | 9 |
| Poltergeist (3) | its throws (Poltergeist loop) | mapped | `GhostActivity.InteractWithARandomProp` (20 in 100, a throw), `GhostHuntState.throwTimer` (never skips) | |
| Poltergeist (3) | game knowledge, to verify by RE: a throw of many props at once that costs nearby players sanity | to map | its ability's branch and the drain | 14 |
| Poltergeist (3) | game knowledge, to verify by RE: it throws harder than other types (m01, the user's note) | to map | the throw's strength and any branch by type (Throws, below) | 17 |
| Banshee (4) | one target, hunts only them, its hunt check reads the target (Banshee loop) | mapped | `GhostAI.bansheeTarget`, `GhostAI.SetNewBansheeTarget`, `GhostHuntState.SightPick`, `Chase`, `KeepNearest`, `TargetAllows`, `GhostIdleState.HuntInsanity` | |
| Banshee (4) | the target alone (y01) | mapped | `phasmo::each_player`, `position_of` (helpers), `Player.currentRoom` | 1 |
| Banshee (4) | game knowledge, to verify by RE: its singing event, aimed at the target, on demand (y01) | to map | the event states of `GhostAI.EventPick` (Ghost events, below) | 8 |
| Banshee (4) | game knowledge, to verify by RE: its scream on the parabolic microphone | to map | `MediaSubject.bansheeWail` is a mapped media kind; what plays it and what picks it up | 29 |
| Jinn (5) | game knowledge, to verify by RE: fast at a distance with the power on (y05) | to map | `GhostHuntState.UpdateSpeed`'s branch for the Jinn, if it has one | 13 |
| Jinn (5) | game knowledge, to verify by RE: its ability drops nearby sanity while the power is on | to map | its ability's branch and the drain | 14 |
| Jinn (5) | game knowledge, to verify by RE: it never turns the power off | to map | the type test before `GhostState.fusebox` or in `FuseBox.GhostUse` | 14 |
| Mare (6) | darkness: never flickers, switches lights off, reads lit rooms, its hunt line (y06) | mapped | `GhostActivity.lightChance`, `GhostWanderState.PickDestination`, `GhostIdleState.TypeHuntRules` (40 with a light on in its room, 60 otherwise) | |
| Mare (6) | a light switched on near it; switching a chosen light off as the ghost (the rolling blackout) | mapped | `LightSwitch.Use` (RPC body, hook only: on, prints, byGhost, counted), `LightSwitch.UseLightSwitch` (its third argument is the ghost's use), `LightSwitch.room`, `LevelRoom.floor` | 2 |
| Revenant (7) | game knowledge, to verify by RE: slow with nobody in sight, fast on sight (y07) | to map | `GhostHuntState.UpdateSpeed`'s branch for the Revenant | 13 |
| Shade (8) | its form coin, event remap, hunt line 35, a larger activity roll | mapped | `GhostAI.Appear`, `GhostState.randomEvent`, `GhostIdleState.TypeHuntRules`, `GhostIdleState.rollRange` | |
| Shade (8) | game knowledge, to verify by RE: little activity with players near it (Shade loop, y02) | to map | any branch by type in the idle roll or `GhostActivity.TryInteract` that reads players near the ghost | 14 |
| Demon (9) | hunt line 70, its own smudge hold | mapped | `GhostIdleState.TypeHuntRules`, `GhostAI.StopGhostFromHunting` | |
| Demon (9) | game knowledge, to verify by RE: a shorter wait between hunts | to map | the readers of `GhostAI.postHuntCooldown` (only its set is mapped) and any length by type | 7 |
| Demon (9) | game knowledge, to verify by RE: a wider crucifix range | to map | the crucifix's range test (Protections, below) | 16 |
| Yurei (10) | the incense traps it | mapped | `WhiteSage.YureiTrap`, `GhostAI.TemporarilyStopWander`; a door locked for a time: `Door.LockDoorForTime` | |
| Yurei (10) | game knowledge, to verify by RE: its ability slams a door and drains sanity (y04) | to map | its ability's branch | 14 |
| Oni (11) | its form coin and event remap | mapped | `GhostAI.Appear`, `GhostState.randomEvent` | |
| Oni (11) | game knowledge, to verify by RE: very active near players; never the airball event; a touch in an event drains more (y03) | to map | the idle roll's branches by type (activity), `GhostAI.EventPick`'s event list (airball), the event states' drain (touch) | 14, 8, 29 |
| Yokai (12) | voices provoke it; it hears only within 2.5 m in a hunt | mapped | `GhostIdleState.TypeHuntRules` (80 while players talk in its room), `GhostHuntState.Hears` (2.5 m); its electronics range in `GhostHuntState.SensesGear` is to verify | 13 |
| Hantu (13) | its last evidence is kept | mapped | `GhostController.PickEvidence` | |
| Hantu (13) | game knowledge, to verify by RE: faster in cold rooms, slower in warm | to map | `GhostHuntState.UpdateSpeed`'s branch for the Hantu (`LevelRoom.temperature` is mapped) | 13 |
| Hantu (13) | game knowledge, to verify by RE: visible breath in hunts with the power off | to map | the breath effect and its test of `FuseBox.powerOn` | 22 |
| Hantu (13) | game knowledge, to verify by RE: it never turns the power on | to map | the type test before `GhostState.fusebox` | 14 |
| Goryo (14) | never roams far, never moves its room | mapped | `GhostWanderState.PickDestination`, `GhostWanderState.Exit` | |
| Goryo (14) | game knowledge, to verify by RE: its D.O.T.S. shows only on camera; a camera watching its room (Goryo loop) | to map | the D.O.T.S. showing's camera test (Evidence, below); which camera covers which room (section 6) | 22, 18 |
| Myling (15) | quieter steps in a hunt | mapped | `Footstep.Play` (8 m less), `Footstep.reach` | |
| Myling (15) | game knowledge, to verify by RE: more sounds on the parabolic microphone | to map | the paranormal sound pick by type (`MediaSubject.paranormalSound` is mapped) | 29 |
| Onryo (16) | three flames, then a hunt; flames blown out | mapped | `GhostAI.flamesBlownOut`, `FireSource.CountGhostBlowOut`, `FireSource.BlowOutFire`, `GhostIdleState.Execute`, `GameController.AmountOfDeadPlayers` | |
| Twins (17) | a second interaction around the twin | mapped | `GhostActivity.TwinInteraction` | |
| Twins (17) | the second twin's place, and acting there at will (y11) | to map | the twin's own box: its position and how it moves; whether starting `GhostActivity.TwinInteraction` (a coroutine, `MonoBehaviour.StartCoroutine` is mapped) is a lawful path for a chosen action | 15 |
| Twins (17) | two hunt speeds, and which twin hunts | partly | `GhostHuntState.UpdateSpeed` adds 0.2 or -0.2 at random as the Twins' hunt begins (ghost-ai.md); to map: the offset's field and which body hunts | 13 |
| Raiju (18) | electronics near it; its hunt line | partly | `GhostActivity.electronicsSensor`, `nearElectronics`, `UpdateElectronics` (inferred), `GhostIdleState.TypeHuntRules` (65 near electronics); to map: the sensor's range and the speed branch | 13 |
| Raiju (18) | game knowledge, to verify by RE: it upsets electronics from farther | to map | the electronics effect's range by type (Electronics, below) | 23 |
| Obake (19) | its last evidence is kept; print and shapeshift media kinds | mapped | `GhostController.PickEvidence`, `MediaSubject.obakeFingerprint`, `MediaSubject.obakeShapeshift` | |
| Obake (19) | game knowledge, to verify by RE: six-fingered prints, prints that fade sooner, choosing when to leave prints (y14) | to map | the print variant pick in `Fingerprint.AttemptToSpawnFingerprints`; the print's life by type (`Difficulty.fingerprintDuration` is mapped) | 21 |
| Obake (19) | game knowledge, to verify by RE: a shapeshift in hunts | to map | the model swap (GhostAI's two GhostModel arrays have no entries, per ghost-ai.md) | 9 |
| Mimic (20) | copies, picked at each switch; ghost orbs | mapped | `GhostController.MimicCopy`, `mimicNext`, `mimicCopies`, `AttemptGhostTypeStateChange`; `EvidenceController.SpawnAllGhostTypeEvidence`, `ghostOrb`, `ForceRespawnGhostOrb` | |
| Moroi (21) | hunt speed from the team's insanity | mapped | `GhostHuntState.RefreshBase` | |
| Moroi (21) | game knowledge, to verify by RE: whoever hears it on the spirit box is cursed and loses sanity faster; sanity pills lift the curse (y13) | to map | the spirit box's answer path and where the curse is held and read; the pill's use | 20, 16 |
| Deogen (22) | sees everyone, slow kill | mapped | `GhostHuntState.Sees`, `Kill`, `slowKillReach` | |
| Deogen (22) | game knowledge, to verify by RE: fast far from its target, slow up close | to map | `GhostHuntState.RefreshBase` reads "a field of its GhostAI" for the Deogen: that field and its writer | 13 |
| Deogen (22) | game knowledge, to verify by RE: heavy breathing on the spirit box | to map | the spirit box's answer branch (`MediaSubject.spiritBoxBreath` is a mapped media kind) | 20 |
| Thaye (23) | its age, its falling hunt line, its events read the age | mapped | `GhostController.thaye`, `thayeStep`, `thayeThreshold`, `GhostIdleState.TypeHuntRules`, `GhostState.randomEvent` | |
| Thaye (23) | ageing a step when the player spends (y12) | to map | the ageing is `ThayeAgeChangeNetworked` (named in `GhostController.thayeStep`'s note; an RPC, so hook only); the game asks for it through `GhostController.typeChangeDue`, which its timer sets and `AttemptGhostTypeStateChange` clears as it acts: read whether setting that flag and letting the next state change run is the game's own path, or what else is | 15 |
| Gallu (25) | its rage state | mapped | `GhostController.galluState`, its hunt lines in `TypeHuntRules` | |
| Gallu (25) | game knowledge, to verify by RE: protections used against it enrage it, then it tires; spending its rage (Gallu loop) | to map | the writers of `galluState` (an RPC or a method), and a lawful way to set it through them | 15 |
| Dayan (26) | its strength, empowered by movement near it | mapped | `GhostController.dayanState`, `GhostAI.DayanPlayerChecks`, `NetworkedStamina.moving`, its hunt lines | |
| Dayan (26) | its hunt speeds, and what `DayanPlayerChecks` measures | to map | its `UpdateSpeed` branch; `DayanPlayerChecks`' range and players | 29 |
| Obambo (27) | its moods, flipped at its switches; hunts only while aggressive | mapped | `GhostAI.obamboAggressive`, `GhostController.AttemptGhostTypeStateChange`, `typeChangeDue` (Update sets it before calling the switch), its hunt lines (65 aggressive, 10 calm) | |
| Obambo (27) | its switch timer and speeds | to map | its timer in `GhostController`'s Update; its `UpdateSpeed` branch | 29 |
| Kormos (28) | fixed events, never switches prey, sensors within a distance, hears footsteps | mapped | `GhostState.randomEvent`, `GhostHuntState.KeepNearest`, `PlayerProcessor`, `GhostHearing` | |
| Kormos (28) | the player flag `PlayerProcessor` reads; the room condition of its hunt line | to map | `PlayerProcessor`'s player read; `TypeHuntRules`' Kormos branch | 28 |
| Aswang (29) | everything (its loop waits on this) | to map | As decompiled, `TypeHuntRules` gives it the 50 of every type without a branch and `RefreshBase` the base speed of every type but the Moroi and Deogen, so its rules are not there. RE: find where its type number is compared (a whole-assembly scan for comparisons with its `GhostType` value, as `audited_ghost_types()` gives it), then map each rule found | 29 |
| Deildegast (30) | fewer light and more prop interactions | mapped | `GhostActivity.lightChance`, `InteractWithARandomProp` | |
| Deildegast (30) | its speed by props moved | partly | `GhostAI.movedProps` (inferred, and the speed rule only inferred from it); to map: the reader in `UpdateSpeed` | 29 |

### The bundled custom ghosts' loops

Each custom ghost's AI rules are already built on public map entries (`mods/custom_ghosts/`). Only what their player
loops add beyond those rules is listed.

| Loop | Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- | --- |
| Echo | the game's speech recognition shows heard phrases as text to pick from (the user's note) | partly | `PhraseListenerController.OnPhraseRecognized(string)`; to map: the recognizer's phrase lists and controller | 24 |
| Draugr | a player carrying its thing (the thief) | to map | a prop's holder (Props in hands, below) | 17 |
| Draugr | its room growing colder | to map | changing a room's temperature (section 5) | 22 |
| Lithobolia | aimed throws at a player it sees; a hit costs sanity | to map | a throw's aim and its hit (Throws, below) | 17 |
| Gremlin | switching running electronics off as the ghost | to map | the ghost upsetting equipment (Electronics, below) | 23 |
| Brownie | watched, the truck monitor included | partly | `CCTVController.viewers`, `GetCCTVCamerasByCurrentCameraType`; to map: whether a camera films the ghost | 18 |
| Lethe | its media fades unless viewed | partly | `JournalMedia.RemoveMedia`, `Journal_PhotoPage`, `JournalController.currentPage`, `open`; to map: who took the media and when a player views it | 18 |
| Mesmer | a different form per player; cameras show the truth | partly | Wearing an investigator's form, below | 30 |
| Hydra | haunts echoed from a second spot; heads that linger and touch things | to map | sounds and interactions at a chosen point (below); the twin's box mechanism may serve (Twins row) | 11, 15 |
| Bogeyman | closets as waypoints; a cursed hiding place found first | mapped | `ClosetZone` (`navSpot`, `occupants`), `Player.closet`, `GhostHuntState.SetTarget`, `SetCloset`, `NavMeshAgent.Warp` | |
| Black Shuck | running water and rain break trails | mapped | `Sink.used`, `Sink.water`, `Difficulty.actualWeather`, `LevelValues.weather` | |
| Tempest | lightning paces it | mapped | `LightningController.PlayLocalLightning`, `PlayLightningNetworked` (hook only), `LevelValues.weather` | |
| Kikimora, Dullahan, Knell, Asterion | their loops reuse their rules and the shared mechanics below | mapped | their rules' entries | |

### Sanity and players

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Per-player sanity, the team's average (e02, e05, s01) | mapped | `Player.sanity`, `PlayerSanity.insanity`, `GameController.GetAveragePlayerInsanity` | 1 |
| Every way it drops, as it happens (e01, e02) | to map | the callers of `PlayerSanity.ChangeSanity` and `SetInsanity` (section 3) | 6 |
| Per-player position and room | mapped | `Player.body`, `Component.get_transform`, `Transform.get_position`, `Player.currentRoom` | 1 |
| A player entering a room or a doorway (t02, t04) | mapped | `LevelRoom.OnTriggerEnter` (`on_room_entered`); a doorway as a point near `Door.body` | 2 |
| Players moving near the ghost (Dayan) | mapped | `NetworkedStamina.moving` | |
| Players fleeing to the truck (g02) | to map | whether a player is in the truck (section 2) | 27 |

### Ghost events and appearing

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| The events, each kind | to map | `GhostAI.EventPick` "starts one of five event states" (its note): each state's class, by what it does, and its kind number | 8 |
| Start an event of any chosen kind (the Banshee's song on demand, y01; any kind, Decisions, 9) | to map | `EventPick(bool, int)` takes an event number, which `ChangeState` passes as -1: read whether a chosen number is honoured and whether the state bookkeeping holds when it is; otherwise `ChangeState(randomEvent)` with a hook on `EventPick` that picks the number | 8 |
| An event's target and place (y01 "aimed at the target", t04 a room) | to map | each event state's target player and where it runs | 8 |
| Appearing | mapped | `GhostAI.Appear(int)`, `MakeGhostAppear` (RPC, hook only), `isAppeared`, `UnAppear` (ends an appearance on every client when called on the host) | |
| `GhostState.appear` | mapped, not usable | `ChangeState` stores it but starts no state object for it and logs an error (its note); a mod never changes state to it | |
| The forms of an appearance | to map | the `int` `Appear` takes (the Shade's and Oni's coin may change it): its values by what the ghost shows | 9 |
| How long an appearance lasts; extending it (m04, the Phantom loop) | to map | the appearance's timer and who ends it; ending early is mapped (`GhostAI.UnAppear`) | 9 |
| The game's player-shaped event | to map | `GhostController.GhostEventPlayer` (ghost-ai.md: an agent, a noise, particles, a photo evidence and a target; spawning it warps the agent and starts it on every client by RPC; an obfuscated state class calls it; its prefab has no model, so it is a particle cloud that walks to a player, not a figure in their form; community notes say the game never uses it, which was not checked in a run). It cannot serve "wear a face" (below) | 30 |
| The sanity cost of an event, and of a touch | to map | the event states' sanity calls (section 3) | 6, 8 |

### Interactions

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Door slam, with its strength (m02, h02) | mapped | `Door.GhostForceDoor(open, strength, skipRattle)`, `CreateDoorSlamNoise`, `PlayDoorSlamNoise` (hook only), `GhostOpenDoorEvent`; the leaf's angle: `Door.closedAngle`, `HingeDoor.openAngle`, `Door.body`; the doors of a floor: `LevelRoom.doors` (inferred), `LevelRoom.floor` | |
| Door hold (h03) | partly | `Door.GhostForceDoor` drops the door from players' hands as it starts (`Door.ForceDropDoorNetworked`, `Door.grabbable`) and keeps the push there; to map: how long it keeps it and whether it can be extended | 17 |
| Door lock | mapped | `Door.LockDoor`, `LockDoorForTime`, `LevelController.LockHuntingDoors` (doors.md, "Locking a door yourself") | |
| Lights per room, whether a room is lit, flickering a chosen light (m03, e08) | partly | `LevelRoom.lightSwitches` (inferred), `AreRoomLightsOn` (inferred), `LightSwitch.get_isOn`, `UseLightSwitch`; a flicker is `GhostAI.ChangeState(GhostState.flicker, ...)` (the value is inferred) and arrives as `LightSwitch.FlickerNetworked` (hook only); to map: the flicker state's object and length | 23 |
| The breaker, tripped or turned on as the ghost; a fumble's EMF (m06, the Jinn and Hantu loops) | partly | `FuseBox.powerOn`, `GhostUse`, `BreakFuseBox`, `outOfOrder`, `GhostState.fusebox`, `GhostInteraction.CreateInteractionEMF`; to map: a player's use, the power-changed event; sparks are game knowledge, to verify by RE | 23 |
| Electronics: radios, TVs, car alarms, phones, set off one at a time or a room at once (h04, h05, m07) | to map | each class and the ghost's use (`GhostState.carAlarm` is the only mapped value) | 23 |
| The ghost upsetting equipment outside a hunt (h04 "also make it affect equipment"; the Raiju, Jinn and Gremlin loops) | to map | section 6, "Any electronic item" | 23 |
| Sinks | mapped | `Sink`, `Sink.water`, `Sink.used`, `MediaSubject.dirtyWater` | |
| Knocks and ghost sounds | to map | `GhostState.doorKnock`, `windowKnock` (values, inferred) are mapped; the sound plays (GhostAudio) and the paranormal sound pick are not | 11 |
| Windows and mirrors: prints, knocks, breath on glass (h11) | to map | section 4; whether the game has a breath effect on glass is game knowledge, to verify by RE; with none, the round maps the glass's materials and any decal surface so the effect the user designs later can be applied (Decisions, 10) | 22, 23 |
| A random action when pressure is full (e07) | mapped | `GhostActivity.TryInteract` (inferred), `NormalInteraction`, `GhostAI.ChangeState` | |
| A room that flickers on its own and turns colder (e08) | partly | the flicker above; a room's temperature (section 5) | 22, 23 |
| The truck's activity monitor (e10) | to map | section 4 | 23 |

### Throws, props and possession

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| A throw in a hunt | mapped | `Prop.TryThrow(bool, bool)`, `GhostActivity.GetPropToThrow`, `throwable`, `GhostHuntState.throwTimer`, `GhostInteraction.CreateThrowingEMF` | |
| A throw outside a hunt (the Poltergeist and Lithobolia loops, m01) | to map | `Prop.TryThrow`'s note: "The hunt's tick is its only caller while a ghost hunts; the ghost's throwing state never reaches it." The throwing state's own throw (`GhostState.throwing`, inferred) is unmapped | 17 |
| A throw's strength, by type (m01, the user's note) | to map | the force inside each throw path, and any branch by type (game knowledge, to verify by RE: the Poltergeist throws harder) | 17 |
| A throw's aim and its hit (the Lithobolia's aimed throws at a player it sees, whose hits cost sanity; the Poltergeist's throw near someone) | to map | whether either throw path takes a direction; how the game detects a thrown prop hitting a player, if it does; else a mod aims through the prop's `Rigidbody` once it owns it (`PhotonView.RequestOwnership`, `Rigidbody.set_velocity`) | 17 |
| A player picking up, holding and dropping any prop; a drop forced through the game (m05, t01, the Draugr) | to map | section 2 | 17 |
| Moving a prop as the ghost, possession (v02, v03) | partly | the pattern in navigation.md, "Moving a thing along the floor" (`PhotonView.RequestOwnership`, `Rigidbody.AddForceAtPosition`, `Prop.PlayImpactSound`), with `GhostInteraction.CreateThrowingEMF` for the trace; `GhostAI.movedProps` (inferred) for the Deildegast; to map: whether the game counts it as an interaction (`LevelStats.amountOfGhostInteractions` sums counters such as `doorsMoved`) | 17 |

### Sounds at a chosen point, and who heard them

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| A ghost sound, knock or footstep at a chosen point away from the ghost (h05, t05, t06, v09, m08, m10, the Hydra's echoes, the Myling's sounds) | partly | the step and its sound are mapped (`Footstep.Play`, `Footstep.noise`, `Footstep.reach`, `Noise.PlaySound`, `Noise.SetVolume`, `Noise.SetMaxDistance`, `ObjectPooler.SpawnFromPool`); to map: whether a `Noise` or `Footstep` can be placed or pooled away from the ghost, the GhostAudio sounds, and whether the ghost's steps play outside hunts (game knowledge, to verify by RE) | 11 |
| Quieter steps in a hunt (u02) | mapped | `Noise.SetVolume`, `Noise.SetMaxDistance`, `Footstep.reach` | |
| Who heard a sound (e01, the Myling loop) | to map | the game has no "heard" of its own that is mapped; a helper over the sound's reach (`Footstep.reach`, the AudioSource's max distance) and the players' positions, documented in the helper as ours, not the game's (Decisions, 8) | 11 |

### Seeing and being seen

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Who is looking at the ghost (the Phantom, Shade, Oni and Brownie loops, y02, y03) | to map | the game's own test, which the Phantom's drain needs and Round 28 finds; where the game has none, a helper over `Camera.WorldToViewportPoint` and `Physics.Raycast`, documented as ours, not the game's (Decisions, 8) | 28 |
| How many see it at once (the Oni loop) | to map | as above, counted | 28 |
| Who witnessed an action (e01) | to map | as above, for a point and a moment | 28 |
| Whether the ghost sees a player | mapped | `GhostAI.vision`, `Player.losTargets`, `core/game.h`'s `ghost_sees` (promoted in Round 1) | 1 |
| Equipment aimed at the ghost (s03) | to map | the held item and its facing (section 6) | 19 |
| A camera watching a room, filming the ghost (the Goryo and Brownie loops) | to map | section 6, "Video camera" | 18 |
| Wearing an investigator's form, seen differently per player, true on cameras (h12, the Mesmer loop) | partly | Playable Ghost already draws an investigator's body (`mods/playable_ghost/form.h`) from `PlayerCharacter`, `PlayerCosmetics.SetPlayerCosmeticsNetworked`, `Player.renderers` and `sdk/api/models.h` (a local copy of the body, so per client); `Camera.get_cullingMask` and the `Layer` entries are mapped; to map: which layers the CCTV and photo cameras draw, so a form can hide from them | 30 |

### Cursed possessions, one by one

| Possession | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Ouija board (h10) | partly | `OuijaBoard.PlayMessageSequence` (inferred), `BreakBoard`; to map: the question heard, the answer chosen ("the Ouija board's answer coroutine", players.md), its sanity cost, and when a ghost may answer | 25 |
| Music box (h10) | partly | `MusicBox.UseNetworked` (inferred), `tween`, `BreakMusicBox`, `EnterMusicBoxState`, `GhostState.musicBox`; to map: what the ghost does in the music box's state and the box's hunt condition | 25 |
| Haunted mirror | partly | `HauntedMirror.UseNetworked` (inferred), `BreakItem`; to map: its sanity cost while used and what it shows (game knowledge, to verify by RE: the favourite room) | 25 |
| Summoning circle | partly | `SummoningCircle` (inferred: "lit, it teleports the ghost onto itself"), `GhostState.summoningCircle`, ghost-ai.md's routes (a forced hunt among them); to map: its candles and how they light it, the teleport | 25 |
| Tarot cards (h10) | partly | `TarotCards.ChangeState` (inferred), `HighPriestess`, `HangedMan`; to map: the card numbers by name and each card's effect | 26 |
| Monkey paw | partly | `MonkeyPaw.MakeWishNetworked` (inferred), `ReviveWish`, `OpenExitsWish`; to map: each wish's handler by wish ("which handler is which wish was not traced", ghost-ai.md) | 26 |
| Voodoo doll | partly | `VoodooDoll.NetworkedUse` (inferred), `StartHunt`; to map: the pin numbers and the interaction each forces (`GhostActivity.NormalInteraction` with force) | 26 |
| Which possession the contract has | partly | `Difficulty.actualCursedItems`, `CursedItemType` (two values); to map: every value | 26 |

### Evidence: placing it, holding it back, giving it now

The revamp lets the ghost player choose when to give each evidence of its type, within a time limit, and holding it
back costs energy (CLAUDE.md; d01, d02, m12, the Obake loop). So each evidence needs the game's own path that places
it, a hook that holds it back, and a lawful way to give it now; where giving it now has no game path, the AI's own
behaviour gives it once the hold ends ("the game gives it on its own", d02).

| Evidence | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| EMF 5 | partly | `GhostInteraction.CreateThrowingEMF`, `CreateInteractionEMF`, `CreateAppearedEMF`, `EMF`, `EMFType` (inferred); to map: the level each reading gets, the chance of 5 when the ghost gives EMF, and where to hold a 5 back | 21 |
| Ultraviolet | partly | `Fingerprint.AttemptToSpawnFingerprints` (on the master; a roll, then an RPC), `EvidenceController.IsFingerPrintEvidence`, `Difficulty.fingerprintChance`, `fingerprintDuration`; to map: where prints can appear, a print without the roll, the salt footprints | 21 |
| Ghost writing (m12) | to map | the book's class, the ghost's write and its timing | 21 |
| D.O.T.S. | partly | `GhostState.dots`, `GhostDotsState` (`GoToNearest`, `radius`, `destination`); to map: what shows the ghost in the projector's light, for how long, and the Goryo's camera-only rule | 22 |
| Ghost orbs | mapped | `EvidenceController.SpawnAllGhostTypeEvidence`, `ghostOrb`, `ForceRespawnGhostOrb`; holding orbs back: to map whether the orb can be hidden through the game | 22 |
| Freezing | partly | `EvidenceController.freezing`, `LevelRoom.temperature`; to map: the room's cooling (section 5), and how it is held back or given | 22 |
| Spirit box answer | to map | section 6 | 20 |
| A voice through the spirit box (p02, the Moroi loop) | to map | the spirit box's speaker (its AudioSource) and its answer event; the ghost player's voice is in other clients' taps (`PlayerVoice.localTap`, `VivoxParticipantTap.DoAudioFilterRead`), so playback goes through those; a mod that keeps the voice sets `recordsVoice` | 20 |
| Timing | to map | when each evidence can first appear and the game's rate for each, the baseline for the time limit | 21, 22 |

### Protections, photos and media

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Crucifix used, a burn, its range (the Demon and Gallu loops) | partly | `Crucifix.CrucifixUseNetworked` (inferred), `useCount`, `MediaSubject.usedCrucifix`, `burningChapelCrucifix`; to map: the range test and what a burn stops (game knowledge, to verify by RE: a hunt) | 16 |
| Smudge, in a hunt or out (the Spirit, Hydra and Gallu loops) | partly | `GhostAI.StopGhostFromHunting`, `smudge`, `huntsHeldOff`, `WhiteSage.YureiTrap`, `WhiteSage.UseNetworked` (inferred: upgrade, and map whom it smudged) | 16 |
| Salt | mapped | `SaltSpot.SyncSalt` (hook only), `SaltSpot.flatSalt` | |
| Flames | mapped | `FireSource.BlowOutFire`, `IgniteFire`, `IgniteFireNetworked`, `CountGhostBlowOut`, `GhostAI.flamesBlownOut` (flames.md) | |
| Sanity pills | to map | section 3 | 16 |
| A photo of the ghost (g01, the Phantom loop) | to map | the photo camera's capture and its subject (`MediaSubject.ghost` is mapped) | 18 |
| Media that fades unviewed (the Lethe loop) | partly | Lethe row above | 18 |

### Hunts

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| Each type's hunt line, read at runtime (e05 and the user's note, u01, m09, the Mare and Thaye loops) | partly | the line is readable now through mapped entries: make an idle state, give it the three controllers, run `GhostIdleState.HuntInsanity` and compare `GhostIdleState.insanity` with the insanity it started from (ghost-ai.md, "Each type's hunt line"); `TypeHuntRules`' offset field itself has no entry (no fact tells it from the chance bonus beside it). A helper `phasmo::hunt_line()` wraps the recipe | 7 |
| May it hunt now, without the roll (u01, m09) | to map | the hunt branch's conditions: `LevelValues.eventsBlocked`, `GhostAI.postHuntCooldown`, `GhostAI.huntsHeldOff` (mapped) and "the type's, sanity and setup checks" (ghost-ai.md), whose setup checks are unmapped; a helper `phasmo::may_hunt_now()` | 7 |
| Start | mapped | `GhostAI.ChangeState(hunting)`, `GhostAI.Hunting` (hook only), `GhostIdleState.Execute`, `GhostHuntState.Started`, `startDelay` (a shorter warning, u02) | |
| A hunt's warning effects without a hunt: lights flicker, electronics upset, the hunt sound (u04, m09, the Spirit, Lethe and Hydra loops) | to map | ghost-ai.md: "Which code flickers the lights and upsets the electronics in a hunt was not traced"; the hunt sound plays through the ghost's GhostAudio noise from `GhostAI.Hunting`'s start (an RPC, so a bluff never calls it): map the effects' own methods so a mod plays them without a hunt | 10 |
| End, length, a longer hunt (u02, u07) | partly | `GhostHuntState.Exit`, `GameController.ResetHuntEffects`, `LevelController.UnlockHuntingDoors`, `DifficultySettings.AddHuntDuration`; to map: the time limit `Started` tests ("when its time is up"), `Difficulty.huntDuration`'s and `killsExtendHuntDuration`'s readers | 12 |
| Target | mapped | `GhostHuntState.target`, `searchTarget`, `SetTarget`, `SightPick`, `TargetPick`, `StartPick`, `Chase`, `KeepNearest`, `TargetAllows`, `lastPlayer`, `seen` | |
| Speed | mapped (base) | `GhostHuntState.UpdateSpeed`, `RefreshBase`, `baseSpeed`, `sightSpeedUp`, the content `huntSpeed` rule; each type's branch is in the type table | 13 |
| Kill | mapped | `GhostHuntState.Kill`, `killReach`, `slowKillReach`, `killed`, `Player.BeginDeathSequence` | |
| Hiding-place search, guessing where a player hides (u05, t03) | mapped | `GhostHuntState.SetCloset`, `searchCloset`, `ClosetZone.CheckCloset`, `navSpot`, `standOutsideEntrancePoint`, `occupants`, `Player.closet` | |
| Holding a caught player | to map | what happens between the catch and the death: `GhostHuntState.Kill` stops the ghost, clears the hunt's target and begins the death at once (its note), and the choke is the ghost's model during its kill (players.md); the animation and its length | 12 |
| A caught investigator breaking free (u08) | to map | where the catch can be refused or ended before `Player.BeginDeathSequence` without breaking the death sequence (a detour on `GhostHuntState.Kill` that skips the original refuses the whole kill, which is mapped) | 12 |
| The grace period | partly | `DifficultySettings.ResetHuntGracePeriod`, `Difficulty.gracePeriod`; to map: the grace's own field and reader | 12 |
| Moving fast, returning home, warping (v01, v04, v05, v06) | mapped | `NavMeshAgent.set_speed`, `Warp`, `GhostController.WarpGhostToFavouriteRoom`, `pm::api::AgentSteering`; to map: `LevelController.SyncCurrentGhostRoom` so the ghost's room follows a warp | 33 |

### Dead players, objectives, temperature

| Need | Status | Keys, or RE needed | Round |
| --- | --- | --- | --- |
| A dead player's state | mapped | `Player.isDead`, `dying`, `LocalPlayer.isDead`, `VoiceVolume.deathMultiplier`, `WeatherType.dead`, `Layer.DeadPlayer` | |
| What a dead player can do; the dead acting for the ghost (p01) | to map | the dead player's movement and what they may use (`LightSwitch.UseLightSwitch`'s second argument lets a dead local player use it, per its note); game knowledge, to verify by RE: dead players see the ghost and pass through walls. A dead player's action for the ghost reaches the master through the SDK's channel, which then acts through the game | 27 |
| The optional objectives: their list | partly | `LevelValues.sideObjectives`, `mainObjective`, `Objective`, `ObjectiveValues.type` (read as an int); to map: the objective kinds by name | 31 |
| Their progress | partly | `Objective.state`, `ObjectiveManager.CompleteObjective`, `FailObjective`, `HasSetupObjectives`; to map: progress counts | 31 |
| The team's guess (g03, d03, the Mesmer loop) | to map | section 8 | 31 |
| Temperature per room | mapped (read) | `LevelRoom.temperature`; changing it is to map (section 5) | 22 |

### The ghost player's screens

Every kept idea tagged as a screen (the minigames m01 to m12, the hiding-place guess u05, the escape check u08, the
ghost's objectives g01, loadout g04 and recap g06, and the energy every loop spends) is built with `PM_UI_TODO` until
the user designs it (Decisions, 7). Round 32 maps only what each screen's logic needs from the game, and records which
surface the game has that each could use (the truck monitor, journal pages, the lobby board, a cloned widget, the
game's popup) as options for the user. The warning investigators get when the ghost spends big (p04) is player-facing
too, so it carries `PM_UI_TODO` the same way.

---

## Mapping rounds, in order

Each round is one implementer task: map, prove by RE, test against the fake backend, document in `sdk/systems/<area>.md`,
add its question snippets to `tests/coverage_questions_test.cpp`, run `.\validate.ps1` and
`.\tools\gamecheck.ps1 -IfConfigured -Expect`, commit. The basic lookups and events come first, then the loader's
services (Decisions, 6), then the revamp's needs, then the nice-to-haves. A round whose RE finds more than one task's
work stops and reports the split, rather than growing. Round 1 starts once Task 14 Step 0b's typed handles land
(Decisions, 5).

1. **Basic lookups.** The shared helpers in `sdk/api/world.h`, in `phasmo::`, typed and documented; `core/game.h` and
   `core/players.h` call them; promote `raycast`, `ghost_sees`, `distance_to_room`; fix the three lagging docs. No RE.
2. **Events over mapped hooks.** `sdk/api/events.h`: spawn, hunt, state change, death start and end, favourite room,
   room entered, light used, salt, flames, phrase. No RE.
3. **Events that need RE.** GameController's loaded and hunt events, the setup phase's end ("started"), the `Revive`
   body. Then `sdk/examples/hunt_watch/` and `sdk/guides/reading-the-game.md` can follow.
4. **Loader services I.** `pm::mod::data_folder()` at `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`, with the Dev Menu and
   Emotes moved to it and a one-time move of each player's existing files; frame time through Unity's `Time` getters
   (`Time.get_deltaTime`, `get_unscaledDeltaTime`, `get_time`, `get_unscaledTime`) and `pm::api::frame_time()`; log
   levels as a new `PmHostApi` member at its end.
5. **Loader services II.** Typed settings (bool, choice, key, besides the sliders) declared `rules` or `local`, and the
   loader sending the host's `rules` values to every verified player on joining and at the contract's start (section
   14, option A).
6. **Sanity causes.** Every caller of `ChangeSanity` and `SetInsanity` as a cause; `SetInsanity` decompiled; which
   client may change whose sanity; `on_sanity_changed`.
7. **The hunt check.** `phasmo::hunt_line()` over the documented recipe; the hunt branch's setup checks and
   `phasmo::may_hunt_now()`; the readers of `postHuntCooldown` and any length by type.
8. **Ghost events.** `EventPick`'s five event states, their kind numbers, starting any chosen kind (Decisions, 9), each
   event's target and place, the singing event, the event list by type (the Oni's airball), the events' sanity calls.
9. **Appearing.** The forms `Appear` takes, an appearance's length and its extension, the blink timer in hunts by type,
   the model swap (the Obake's shapeshift, the GhostModel arrays).
10. **A hunt's effects without a hunt.** The code that flickers lights, upsets electronics and plays the hunt sound in a
    hunt, mapped so a mod plays each alone.
11. **Sounds at a chosen point.** GhostAudio and its sounds, knocks, the paranormal sound pick, placing a `Noise` or a
    `Footstep` away from the ghost, the ghost's steps outside hunts, and the "who heard" helper (ours, not the game's:
    Decisions, 8).
12. **Hunts, complete.** The time limit, hunt duration and kills extending it, the grace period, the hold of a caught
    player, a lawful break-free point.
13. **Hunt speed by type.** Every branch of `GhostHuntState.UpdateSpeed` and `RefreshBase`: the Jinn, Revenant, Hantu,
    the Deogen's field, the Twins' offset, the Raiju, the Yokai's electronics range.
14. **Abilities and the idle roll by type.** The branches of `GhostActivity.GhostAbility` (Wraith, Jinn, Yurei,
    Poltergeist), the idle roll's branches by type (the Shade's and Oni's activity near players), the fuse box gates
    (Jinn, Hantu), the salt test (Wraith).
15. **Type states.** The Thaye's ageing and a lawful step, the Gallu's rage writers and a lawful change, the Twins'
    second place and acting there.
16. **Protections.** The crucifix's range and burn, the incense (decompiled, its target), the sanity pill, the Moroi's
    curse being lifted.
17. **Props, hands and throws.** A player's grab and drop of any prop, the held item, a forced drop, the door hold's
    length, the throw outside a hunt, a throw's strength by type, its aim and hit, possession counted as an interaction.
18. **Photos and video.** The photo camera's capture, subject and stars (`on_photo`, the Phantom's vanish), which room a
    CCTV camera covers and whether it films the ghost, media viewed and its taker, the photo rewards.
19. **Readings.** The EMF reader's level, the thermometer's shown value, the parabolic microphone, the sound and motion
    sensors, the held item's facing (`aimed_at`, ours, not the game's).
20. **The spirit box.** Its question, answer roll and answer, the Deogen's breath, the Moroi's curse, its speaker for a
    voice.
21. **Evidence I.** EMF 5, prints (places, the Obake's variant and life), the writing book; holding each back and
    giving it now; their timing.
22. **Evidence II and temperature.** D.O.T.S. (with the Goryo's camera rule), orbs, freezing and the room's cooling,
    changing a room's temperature through the game, the Hantu's breath; holding each back; their timing. Breath on
    glass: whether the game has a breath or fog effect on windows and mirrors; if it has, map it as the
    way to show breath; if it has none, map the surfaces such an effect would use (the glass's renderers and materials,
    any decal or projector the game uses) so the effect the user designs later can be applied (Decisions, 10).
23. **Electronics and power.** Radios, TVs, car alarms and phones, the ghost upsetting equipment, the flicker state, the
    breaker's player use and the power events, bulbs, the activity monitor, window knocks.
24. **Speech recognition.** The recognizer's phrase lists and controller, the spirit box's and Ouija board's phrase
    handlers, `NetworkedReaction` decompiled.
25. **Cursed possessions I.** The Ouija board, music box, haunted mirror and summoning circle.
26. **Cursed possessions II.** The tarot cards, monkey paw wishes, voodoo doll pins, every `CursedItemType` value.
27. **Players and the dead.** In the truck, talking and push-to-talk, crouching, the sprint sync
    (`NetworkSyncSprint`), what a dead player can do.
28. **Seeing.** The game's own looking test (the Phantom's, found whatever stands in elsewhere), who sees the ghost and
    how many, who witnessed an action (ours, not the game's, where the game has no test: Decisions, 8), the Kormos's
    player flag and room condition.
29. **The remaining type rules.** The Aswang (the whole-assembly scan, then each rule), the Banshee's scream, the
    Myling's sounds, the Oni's touch, the Dayan's, Obambo's and Deildegast's speeds and timers.
30. **The ghost player's form.** `GhostEventPlayer` mapped and documented as what it is; the CCTV's and photo camera's
    layers, so a drawn form hides from them.
31. **Journal and objectives.** The team's guess and the judged guess, objective kinds by name, progress.
32. **The revamp's screens.** What each screen's logic needs from the game, and, for each screen, the game surfaces it
    could use, recorded as options for the user; every screen itself stays `PM_UI_TODO` until the user designs it
    (Decisions, 7).
33. **Map data II.** The contract's map, its name and size, `GhostInfo.favouriteRoom`, `LevelRoom.name` localised,
    the inferred room members upgraded, `LevelController.SyncCurrentGhostRoom` (the ghost's room after a warp; also on
    Task 18's list).
34. **The catch-up names.** Section 15's five names, each mapped only if relied on.
35. **The address audit.** The ~169 cited addresses. A group of tasks, not one: its first task lists them and splits
    them into tasks of about 40 addresses each.
36. **Nice-to-haves.** Weather effects, the journal's remainder, UI widgets beyond the revamp's surfaces.
37. **Navigation for Task 18.** Held until the user starts Task 18.

Then Task 17's Steps 3 to 6: the coverage gate, the example and guide (if not done after Round 3), the smoke list, the
commits.

## Decisions

The spec's questions, answered on 2026-11-14.

1. **Settings in a lobby** (user): option A. A setting declares itself `rules` or `local`; the host's `rules` values
   hold in the host's lobby, sent by the loader to each verified player on joining and at the contract's start, and a
   client goes back to its own values after leaving (section 14). Round 5.
2. **Where the helpers live** (controller, following the plan): in namespace `phasmo::`, as the plan's
   `phasmo::current_ghost()`, in `sdk/api/world.h` (the lookups) and `sdk/api/events.h` (the events).
3. **The data folder** (user): `%LOCALAPPDATA%\PhasmoMods\data\<mod id>\`. The Dev Menu and Emotes move their files
   there, with a one-time move of each player's existing files from `%LOCALAPPDATA%\PhasmoMods\<mod id>\`. Round 4.
4. **Frame time** (controller): map Unity's `Time` getters, which are Unity's documented API; no new contract member.
   Round 4.
5. **Typed handles first** (controller): Task 14 Step 0b is being built now, and Round 1 starts after it lands, so the
   helpers and events are typed from the start.
6. **Order** (user): the loader's services move to right after the events, as Rounds 4 and 5; every later round is
   renumbered (this spec's numbers are the new ones).
7. **The revamp's screens** (user): every screen is built with `PM_UI_TODO` until the user designs it. Round 32 maps
   only what each screen's logic needs and records which game surface each could use, as options for the user.
8. **Stand-ins for tests the game lacks** (user): allowed where the game has no test of its own (who heard a sound, who
   witnessed an action, an item aimed at the ghost, who looks at the ghost outside the Phantom's rule), each helper
   documented as ours, not the game's. The Phantom's own looking test is still found (Round 28).
9. **Ghost events** (user): map starting any chosen event kind, not only the Banshee's song. Round 8.
10. **Breath on glass** (user): keep it. If RE finds no vanilla effect, it stays in the design and the user designs the
    effect's look later (art is the user's); Round 22 maps whatever surfaces exist (materials, decals) so the effect
    can be applied.
