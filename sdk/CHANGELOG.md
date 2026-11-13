# Changelog

Changes to the host contract (`core/mod_api.h`) and the SDK a mod builds against. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/). The contract's versions are `PMMOD_API_MAJOR.PMMOD_API_MINOR`:
a host runs a mod built for the same major version and a minor no newer than its own (`pm_api_compatible`). Within a
major version `PmHostApi` and every published struct only grow at the end, and every content struct begins with its
`size`, so a mod built for an older minor keeps loading.

## [2.5] - Unreleased

### Added

- **Typed handles, the foundation.** `pm::api::Ref<D>` (`sdk/api/ref.h`) is a handle to an object of the map's type
  `D`, one pointer like the `void*` it will replace; it converts by itself to the handle of each base type the map
  records and to `pm::api::Object`, a handle of an unmapped type, and `pm::api::as<D>(x)` (`game_api.h`) converts down
  after asking the runtime. `pm::api::HeldRef<D>` (`held.h`) keeps one across frames as `Held` does. Each Type entry
  gains `EntryDesc::base`, its nearest mapped base class, which `gamecheck` proves (`BASE` lines). No wrapper takes or
  gives a handle yet, so nothing that compiled changes; Steamworks' `CSteamID` and Win32's `HWND` are the precedent.

- **Every installed mod, read-only.** `PmHostApi::mod_entry(index, out)` gives the installed mod at an index (id, name,
  version, author, whether it runs, whether it is developer-only or records voice, and why it is off), filling a
  `PmModEntry` up to the `size` the mod sets; SMAPI's `IModRegistry.GetAll` is the precedent. `pm::mod::mod_entries()`
  (`core/mod_glue.h`) copies them all out. The dev menu's Loader page reads it.
- **Objects of a generic instance.** `PmHostApi::object_new` also takes an Inflated entry's key and makes an object of
  its generic instance; `Method::new_object()` on an Inflated wrapper calls it (`List<UInt32>.ctor` makes a
  `List<UInt32>`). An older host answers null.
- **Cloth self-collision.** `Cloth.set_selfCollisionDistance`, `Cloth.set_selfCollisionStiffness` and
  `Cloth.SetSelfAndInterCollisionIndices`, with `List<UInt32>.ctor` and `.Add` to build its list; `add_cloth`'s
  `ClothSettings::selfCollision` turns it on for chosen points (`sdk/api/models.h`).
- **The hunt's speed factors.** `GhostAI.huntSpeedFactor1` to `3`, the three factors every hunt's speed is multiplied by
  ([ghost AI](systems/ghost-ai.md), "Hunt speed").

## [2.4] - Unreleased

### Added

- **A game event's listener.** `PmHostApi::unity_action_new(target, listener)` makes a UnityAction bound to a managed
  target, the way the game makes its own listeners, so a mod's button takes the game's own click
  (`sdk/api/buttons.h`).

## [2.3] - Unreleased

### Added

- **Emotes and keys of your own** ([guide](guides/emotes.md)). Helpers in the mod's own code (the host contract does not
  change): a rebindable key with a row in the game's keybinds menu (`sdk/api/keybinds.h`, `binding_rule.h`); a clip
  played on a player's own body and handed back (`sdk/api/player_anim.h`); an eased third-person view of the local
  player and their body shown to them (`sdk/api/third_person.h`, `orbit_rule.h`); a sound counted as the player talking
  (`sdk/api/talk_loudness.h`); and a screen blur and vignette (`sdk/api/screen_effects.h`). `core/ui.h` gains a
  world-space canvas and textures drawn from pixels. The map gains the entries behind them: the player's Animator,
  renderers, hand anchors, voice loudness and VR hands; the held item's camera slot; Unity's Playables, the Input
  System's action building, overrides and keybinds rows, the gamepad's stick; `Camera.CopyFrom`; and URP's camera data,
  volumes, vignette and depth of field.
- **Settings on the audio page.** `sdk/api/settings.h`: a mod's sliders on the journal's audio page, copies of the
  master volume's row under the mod's heading, counting on the page's Apply as the game's do and saved to the mod's
  file. `sdk/api/menu_rows.h` holds what the keybinds and settings rows share. Mod keys (`keybinds.h`) now follow the
  keybinds page's Apply, Reset and closing as the game's keys do. The map gains the pages' buttons
  (`ControlsManager.*`, `GameMyAudioManager.*`), the game's slider (`XRSlider`, `SliderEvent`) and
  `InputActionRebindingExtensions.RemoveAllBindingOverrides`.
- **One line between rows.** `rows::space_out` and `rows::line_divider` (`sdk/api/menu_rows.h`) lay a journal menu's
  list out with exactly one line between every pair of visible rows, the mod's and the game's alike: the list's own
  dividers are put away, a line is copied in between every pair of rows, and `line_divider` takes the line to copy from another journal menu's
  list when one is loaded, since the audio page's own dividers draw as double rules.
- **The head the game hides.** `PlayerCharacter.ChangeHeadObjectsShadowCastingModeShadowsOnly`, which the photo camera
  and the focus UI call on the client's own player; the third-person view now draws the head again each frame, so a
  photo taken by anyone no longer leaves the dancer headless ([players](systems/players.md)).
- **The start of a kill.** `Player.NotifyOfDeathStart` (`sdk/map/Player.h`), the RPC every client gets when a kill
  begins: it marks the player dying, turns their collider off and wakes their ragdoll, a second or two before
  `Player.isDead` is set. A mod hooks it to let go of a body the game is taking back for its death animation, and never
  calls it ([players](systems/players.md#the-mark)).
- **A mod's own name.** `PmHostApi.mod_name` (added at the end) gives the name an installed mod's manifest gives it,
  the one the Mods panel shows, and `pm::mod::shown_name()` gives the running mod its own. Its keys' section in the
  keybinds menu is headed with it.
- **Assets kept across scenes.** `load_bundle_asset` marks what it loads `HideFlags.DontUnloadUnusedAsset`: an asset
  loaded at the menu is in no scene, and the next scene load would otherwise destroy it.
- **Figures of a mod's own.** Helpers in the mod's own code (the host contract does not change): `load_bundle`,
  `load_controller`, `load_bundle_asset`, `bundle_asset_names` and `unload_bundle` (`sdk/api/bundles.h`) load an
  AssetBundle from the mod's assets folder; `copy_investigator_body`, `use_controller`, `play_state`,
  `controller_has_states`, `two_sided_material`, `lit_material_of`, `add_cloth` and `destroy_cloth`
  (`sdk/api/models.h`) copy the investigator's body without the game's scripts, play a controller's states on it and
  drape Unity Cloth on a mesh the mod builds ([custom animation](guides/custom-animation.md)). The map gains the
  entries behind them: Cloth, the Mesh and SkinnedMeshRenderer setters, `Material(Shader)`, the Lit shader's base
  colour and culling, the investigator's body, Resources and AssetBundle loading, and Animator states and bones.
- **Optional assemblies.** A build may lack an assembly that only `kOptional` Type entries name; those entries then
  stay unresolved instead of the whole map failing. The Cloth entries are such.
- **Cloth after a teleport.** The map gains `Cloth.ClearTransformMotion` (`phasmo::Cloth::ClearTransformMotion`): it
  forgets the motion a cloth has accumulated from its GameObject moving through the world, so calling it right after
  moving that GameObject a long way in one frame (a warp, a teleport, a body set down where it landed) carries the
  cloth with it instead of dragging it.
- **The fake game sizes arrays of a struct.** `pm::fake::element_size` sets the element size `array_new` gives a type.
- **A ghost the Mimic never copies.** `PM_GHOST_NO_MIMIC_COPY`, a new bit of `PmGhostDef.flags` (`GhostDef::flags`):
  the host leaves the ghost off the Mimic's copy list for good.
- **A ghost no player plays.** `PM_GHOST_NOT_PLAYABLE`, a new bit of `PmGhostDef.flags`, and `content::flags_of(type)`
  (`PmHostApi.content_flags_of`, added at the end) to read any type's flags. Playable Ghost never offers such a ghost
  to its ghost player, draws the type roll again past it in a contract with one, and never keeps it from a pick.
- **A ghost with no evidence.** Every evidence slot of a `PmGhostDef` may be empty; the game then gives only its own
  "no evidence" for it ([evidence](systems/evidence.md#a-ghost-that-gives-no-evidence)). Allowed since 2.2, now stated
  and tested.
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
- **[The vanilla ghosts, type by type](systems/vanilla-ghosts.md):** every mapped rule of each vanilla ghost type, with
  the entries that hold it.
- **The Onryo's hunt:** `GhostIdleState.Execute` hunts once `GhostAI.flamesBlownOut` passes 2, without the sanity
  check. [Ghost AI](systems/ghost-ai.md) has a new section, "How a hunt starts".
- **Equipment, item by item:** a table of what the map holds for each item and cursed possession
  ([equipment](guides/equipment.md)).
- **A door's slam sound** ([doors](systems/doors.md#a-doors-slam-sound)): `Door.slamClips`, `Door.noise`,
  `Door.PlayDoorSlamNoise` (documented, never to be sent), `SoundController.instance` and
  `SoundController.doorSlamClips`, and a `Noise.PlaySound` wrapper, so a mod can play a door's slam on one client.
- **A closet's inside on the ghost's routes:** `ClosetZone.internalAreaObstacle` and `DynamicNavMeshObstacle`
  (`targetActivity`, `navMeshObstacle`, `Update`). Clearing `targetActivity` switches the obstacle that keeps ghost
  routes out of the closet off.
- **Layers:** `Layer.Item`, the equipment players carry, place and drop.
- **A ragdoll from Unity's parts** ([custom animation](guides/custom-animation.md#a-ragdoll-from-unitys-parts)):
  `Joint` (`set_connectedBody`, `set_axis`), `CharacterJoint` (`set_swingAxis` and its four limits, with
  `phasmo::SoftJointLimit`), `BoxCollider` (`set_size`, `set_center`), `SphereCollider` (`set_radius`, `set_center`),
  `CapsuleCollider.set_center` and `set_direction`, `Rigidbody.set_mass`, `set_useGravity` and `get_angularVelocity`,
  `Collider.get_attachedRigidbody`, `Physics.OverlapSphere`, and `Layer.DeadPlayer`, the layer that meets only the
  house's fixed world.
- **Heavier cloth and its simulated shape:** `ClothSettings` gains `worldVelocityScale`, `worldAccelerationScale` and
  `virtualParticles`, set only when a mod asks; the map gains `Cloth.get_vertices` and `Cloth.get_normals` (the cloth
  as simulated, through the engine's functions), `Mesh.RecalculateBounds`, `MeshUpdateFlags` and `Mesh.MarkDynamic`,
  so a mod can draw a mesh that follows a cloth each frame (the Squatter draws its sheet's thickness so).
- **A matte material:** `ShaderProperty.Lit.Smoothness` and `set_smoothness` (`sdk/api/models.h`), which sets a Lit
  material's smoothness, 0 being matte as the game's own untextured fabric.
- **A pass after the Animator:** `IKExecutionOrder.LateUpdate`, where Final IK runs its solvers, for a hook that changes
  the animated pose before any IK bends it ([custom animation](guides/custom-animation.md#a-pass-after-the-animator)).
- **Sounds as the game plays them:** `SoundController.GetFloorAudioSnapshot`, the mixer group a sound at a point plays
  through (what `Noise.PlaySound` gives a noise with `Noise.changeMixerOnPlay`), and `AudioSource.set_outputAudioMixerGroup`,
  so a mod's sound is muffled on another floor or from outdoors as the game's are; `AudioSource.set_rolloffMode` with
  `AudioRolloffMode.Linear` for a sound that falls to silence; `Physics.CheckBox`.
- **Costumes:** `hide_renderers` takes the names of the parts to keep drawing; [scene](systems/scene.md) names each
  renderer of the investigator's body.

### Changed

- `Door.CreateDoorSlamNoise` is decompiled: it only sends `Door.PlayDoorSlamNoise`, whose body accepts the room's first
  actor alone, so no other client may call it. Its Door argument is never read.

- **No mod runs with part of itself missing.** The host keeps off a mod whose `pmmod_start` met any failure of the
  host's, whatever `pmmod_start` returned: a registration it refused (`ghost_register`, `evidence_register`,
  `ghost_rules`, `journal_lines`), a hook that could not be made or switched on (`create_hook`, `hook_entry`,
  `hook_body`, `enable_hooks`), or a service name already taken (`register_service`). It stops the mod, removes its
  hooks and content, and logs why, so a mod need not check what those calls returned. Once every mod has started, a
  mod whose ghost or rules the host drops (an evidence or a ghost no started mod registered) stays off for that start
  too, and the others start again without it, until nothing is dropped.
  While custom content is off on a client (the game's enums unreadable, or a game entry or hook the host's content needs
  missing) every ghost and evidence is refused, so Custom Ghosts and Spectral Flame are off there instead of running with
  nothing in the game. Rules and journal lines on the game's own ghosts still register.
- `Door.ToggleDoorLock`, `Door.GhostForceDoor` and `Door.DisableOrEnableDoor` are decompiled. `Door.ToggleDoorLock`
  switches a door on or off for players and unlocks a locked door at once; switched off, it also starts the hunt's close
  coroutine on the door's owner, which shuts and locks the door again as it ends. Its earlier note, read from the name
  alone, said that `false` locks the door.
- **Mapping the game moved** from this README to [its own page](internals/mapping.md), with its sections (an entry, the
  resolver, confidence, commands, game builds, adding an entry, moving code onto the SDK).
- **Entry notes name ghost types:** "the Onryo (16)", where a note gave only the number. Notes say what a member does,
  and how an entry is found moved to the `//` comments above it.

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
