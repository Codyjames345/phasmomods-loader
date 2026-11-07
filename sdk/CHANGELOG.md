# Changelog

Changes to the host contract (`core/mod_api.h`) and the SDK a mod builds against. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The contract's versions are `PMMOD_API_MAJOR.PMMOD_API_MINOR`:
a host runs a mod built for the same major version and a minor no newer than its own (`pm_api_compatible`). Within a
major version `PmHostApi` and every published struct only grow at the end, and every content struct begins with its
`size`, so a mod built for an older minor keeps loading.

## [Unreleased]

### Added

- **Doors** ([doors](systems/doors.md)): `Door.locked` (the padlock), `Door.view`, `Door.otherDoor` (a double
  door's other leaf), `Door.forceRoutine`, `Door.useForceInsteadOfAnimations`, `Door.CheckOcclusionPortal`,
  `Door.GhostOpenDoorEvent`, `Door.SyncLockState` (documented, never to be sent), `HingeDoor.openAngle`,
  `HingeDoor.LockDoor` and `HingeDoor.LevelControllerOpenDoor`.
- **Doors:** `Door.loopSource` (a door's creak, and how to hush it when you lock a leaf you moved),
  `Door.pairedDoor` (the other field a double door's partner may sit in), `Door.GrabbedDoor`,
  `Door.AttemptToUnlockDoor` (what a player's grab on a locked door does) and `DoorKind.PermanentLock`.
- **Navigation** ([navigation](systems/navigation.md)): `NavMesh.CalculatePath` for a route between any two points.
- **Props:** `Prop.PlayImpactSound` and `Prop.lowImpactVelocityCutoff` (a prop's own impact sound), and
  `ObjectPooler` with `ObjectPooler.SpawnFromPool`.
- **Physics:** `Rigidbody.AddForceAtPosition` (with `kVelocityChange`), `Rigidbody.GetPointVelocity`,
  `Transform.TransformPoint` and `Transform.InverseTransformPoint`.
- **Unity:** `Rigidbody.set_isKinematic`, `Rigidbody.set_constraints` (with `kFreezeAll`) and
  `Transform.set_localEulerAngles`.
- **Weather:** `LevelValues.weather` is decompiled: a Monkey Paw weather wish changes it mid-contract
  ([weather](systems/weather.md)).

### Changed

- `Door.ToggleDoorLock`, `Door.GhostForceDoor` and `Door.DisableOrEnableDoor` are decompiled. `Door.ToggleDoorLock`
  switches a door on or off for players and unlocks a locked door; it never locks one, as its earlier note said.

## [2.2] - 2026-11-03

Released in v0.2.0.

### Added

- **Content.** `ghost_register` and `evidence_register` (`PmGhostDef`, `PmEvidenceDef`, from `pmmod_start` only), with
  `content_type`, `content_ghost_id`, `content_evidence` and `content_active`. The host numbers custom ghosts and
  evidence after the game's own enums, read at runtime, and puts them into the game: the ghost collection, the type
  roll, the Mimic's copy list, the custom difficulty's Ghost Type option, the evidence pick and every journal
  (`sdk/api/content.h`, [adding a ghost](guides/adding-a-ghost.md), [adding evidence](guides/adding-evidence.md)).
- **Rules on a ghost type.** `ghost_rules` (`PmGhostRules`: `start`, `tick`, `stop`, `huntSpeed`, `heldBack`) on a
  custom or a vanilla id, run while the contract's ghost acts as that type, with `content_acting_as` and
  `content_identity` ([tells for vanilla ghosts](guides/tells-for-vanilla-ghosts.md)).
- **Journal lines** on a vanilla ghost's page: `journal_lines`.
- **Evidence looks:** `content_evidence_look` and `PmGhostDef.evidenceLook`.
- **A ghost a player controls** (Playable Ghost), answered from any registered definition: `content_player_controlled`,
  `content_use_ability`, `content_ability_text`, `content_hud_state`, `content_hud_line`, `content_held_back`,
  `content_player_view`, `content_target`, `content_flags`, `content_player_move`, `content_interaction_lost` and
  `content_roll_taken`; `PmGhostDef` gains `flags` (`PM_GHOST_KEEPS_LAST_EVIDENCE`, `PM_GHOST_BLIND_IN_HUNTS`),
  `target`, `hudLine`, `playerMove` and `interactionLost`, each at the end.
- **Runtime services:** `list_add`, `copy_settings`, `fields_of_type`, `collection_count`, `array_element_size`,
  `list_remove_at` and `dictionary_int_entries` (`sdk/api/runtime.h`).
- **The ghost's agent and spawn:** `steer_agent` (the host keeps the native site itself) and `abandon_ghost_spawn`.
- **Dependencies:** `mod_started`, for an optional dependency ([README, "Dependencies"](README.md#dependencies)).
- **Messages to the player:** `show_message`, a one-button message in the game's own popup.
- **Shader properties** as scene facts (`PM_ENTRY_SHADER_PROPERTY`).
- **`manifest.json`** for each mod: id, name, author, version, dependencies, incompatibilities and `recordsVoice`
  ([README, "The shape of a mod"](README.md#the-shape-of-a-mod)). A folder with only the old `mod.ini` still loads.
- **The truck's monitor** in the game map: `CCTVController`, `CCTVController.instance`, `CCTVController.viewers` and
  `CCTVController.GetCCTVCamerasByCurrentCameraType` ([photos, video and screens](systems/media.md)).
- **`ReceiverGroup.All`** in the game map.
- **The setup for an API reference:** `sdk/Doxyfile`, `sdk/groups.dox`, `tools/docs.ps1` and the `docs` target. The generated
  reference itself follows in a later release. Maintainers' material from the `sdk/systems` pages is now in `sdk/internals`.

### Changed

- **The prefix is `pm`, for PhasmoMods** (it was `pg`): `pm::` namespaces, `Pm*` types, `PM_` macros and the exports
  `pmmod_info`, `pmmod_start`, `pmmod_tick`, `pmmod_stop` and `pmmod_requires`. The host still loads a mod built with
  the old `pgmod_*` export names, logging once, until the next major version. <!-- pm:old-name: the rename's record -->
  Struct layouts and values are unchanged.
- **Every mod is required by every player in a lobby.** The host's list is the lobby's list, mods run only in private
  rooms, and a contract begins only once every player's mods are confirmed
  ([lobbies, saves and players](guides/lobbies-saves-and-players.md)). `PM_MOD_VANILLA_PEERS` is reserved and ignored.
- **The host gates every mod's content, rules and journal lines** on this client matching the host's list, so a mod
  keeps no such check of its own.

### Deprecated

- `room_agrees` and `peer_has_mod`: a mod has no need of either; they stay so 2.x mods load.

### Removed

- `patch_site`, which `main`'s unreleased 2.1 had as its last member: the host keeps the one native site it patched
  (`steer_agent`), and no mod reaches a native site. No release carried 2.1, so no released mod used it.

## [2.1] - unreleased on its own

On `main` after v0.1.0; never in a release. Its members are part of 2.2.

### Added

- **Runtime services:** `string_new`, `string_utf8`, `array_length`, `array_data`, `array_new`, `list_count`,
  `list_items`, `type_object`, `object_new`, `is_instance`, `alive`, `virtual_method`, `gc_hold`, `gc_release`
  and `log_exception`. Each refuses an object of the wrong kind, answering 0 or null, and logs once.
- **Static fields:** `static_get` and `static_set`.
- **Hooks per body:** `hook_body` and `body_count`, for methods the obfuscator copied.
- **Scene and asset facts:** `scene_find` and `animator_param`.

## [2.0] - 2026-10-23

Released in PhasmoMods Loader v0.1.0.

### Added

- **The game map** handed to mods: `game_entry` and `hook_entry`, which the game API (`sdk/api/game_api.h`) is built on,
  with `set_reference` (stores through the runtime's write barrier) and `inside_entry` (whether a tracked method is
  running on this thread).
- `pmmod_requires`: the entries a mod needs, so a game build that lacks one leaves only that mod off. In 2.0 it was
  named `pgmod_requires`. <!-- pm:old-name: its name in 2.0 -->

### Kept from version 1

- `log`, `create_hook`, `enable_hooks`, `send`, `set_receiver`, `peer_has_mod`, `register_service` and `service`.

[2.2]: https://github.com/Codyjames345/phasmomods-loader/compare/v0.1.0...v0.2.0
[2.1]: https://github.com/Codyjames345/phasmomods-loader/compare/v0.1.0...v0.2.0
[2.0]: https://github.com/Codyjames345/phasmomods-loader/releases/tag/v0.1.0
