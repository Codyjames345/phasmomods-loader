# Findings

Problems found while doing something else, and not fixed at the time. The format and the
rule behind it are in `standards/repository.md`. Each entry carries a prompt written to be
pasted into a session that was not here for the conversation that produced it.

An entry is deleted in the change that fixes it. `/fix` takes a number or a few words from
a heading and does both halves; `/finding` writes a new entry; `harness-findings verify`
reports an entry whose problem is already gone.

<!-- next: 33 -->

## 32. A community mod has only the install folder to live in, which an update may replace

  tools/launcher.cpp:525

  The launcher discovers mods only in the `mods` folder beside it (discover_mods with Place::InstallFolder), and the
  host reads the same folder when the launcher did not start it. core/mod_list.h already names a second place
  (Place::Elsewhere, "such as one in %LOCALAPPDATA%\PhasmoMods"), and the reserved-author rule is built around it, but
  nothing reads one. The installer and updater are Velopack's (tools/update_velopack.cpp), whose install keeps the
  program in a `current` folder that each update replaces, so a community mod a player copied there is likely lost on
  the next update. Not checked on a real update. sdk/README.md ("Does not exist yet") plans that folder in
  %LOCALAPPDATA%\PhasmoMods\mods for stage 2; the stage 2b plan names it only as an example in the reserved-author
  rule, and no task builds it. Found while writing sdk/guides/adding-a-ghost.md (stage 2b Task 14 Step 2).

  Prompt:
  > PhasmoMods Loader finds mods only in the `mods` folder beside the launcher (tools/launcher.cpp discover_mods, and
  > host/mods.cpp when the launcher did not start the game). core/mod_list.h's Place::Elsewhere names a mods folder
  > outside the install folder that nothing reads yet. First confirm on a Velopack install whether an update keeps a
  > folder added under `current\mods`. Then confirm with the author that %LOCALAPPDATA%\PhasmoMods\mods, which
  > sdk/README.md plans, is still where community mods should live, before adding it to discovery, the launcher's copy
  > step and the Mods panel. Update sdk/guides/adding-a-ghost.md, docs/installation.md and this entry together.

## 31. Five mod-drawn windows remain, against the vanilla-UI rule

  core/notice.h:1

  The vanilla-UI rule forbids windows and overlays a mod draws itself (CLAUDE.md, "Vanilla UI only"). Stage 2b Task 14
  Step 0a audited every ui::create_canvas caller. The deception score and the ghost's payout moved to the game's own popup
  through the host contract's show_message. These have no mapped game surface in a contract, so the author chose
  (2026-11-03) to rework what could be and mark the rest for the author to design: each carries a PM_UI_TODO comment.

  - core/notice.cpp: the "truck stays shut" countdown an investigator sees while the ghost is chosen.
  - mods/playable_ghost/hud.cpp: the ghost player's HUD strip (restlessness, actions and their keys, the object under the
    ghost's gaze, a notice line, the investigators' average sanity).
  - mods/playable_ghost/panorama.cpp: the cover drawn over the ghost player's view while the panorama camera moves.
  - mods/playable_ghost/perceive_view.cpp: the per-type senses drawn where they are on screen, and the alert line.
  - mods/playable_ghost/picker.cpp: the pick screen's own text over the journal and the truck's map.

  Prompt:
  > Each file above draws its own canvas with ui::create_canvas, which the vanilla-UI rule forbids; the author decides what
  > game surface each should use (CLAUDE.md, "Vanilla UI only"). For the one the author names, reverse engineer the game
  > surface chosen (its scene path, its widgets and how the game itself fills them), map what a mod needs as entries with
  > notes, and move the window onto it, keeping the game's own components and click events. Remove the PM_UI_TODO comment
  > and this entry's line for it in the same change. Never invent a window in its place; ask the author when no game
  > surface fits.

## 27. Ghost Tells has never run inside the game

  mods/ghost_tells/mod.cpp:1

  The eighteen tells come from REA and Ghidra decompiles, the class dump and the ISIL
  listing of build 25811160, not from a contract. Each rule's arithmetic is tested offline
  (tests/ghost_tells_test.cpp); everything below is unverified in the game:
  - The switch: a private room in which every client runs the mod turns it on for the contract
  ("GhostTells: on for this contract").
  - Journal: LocalisationSystem.GetLocalisedValue (0x8D1EF0) is asked for "Journal2_<Type>" when a
  ghost page is shown (not once at load, before the mod has decided), and the added line fits the page.
  - Spirit: GhostAI +0xF8 is set when incense reaches it in a hunt and +0xF5 outside one; the hunt
  state's base speed (+0xB0) held at 0 stops it, its footsteps and its walk; PhraseListenerController
  .OnPhraseRecognized (0x2370E00) is called for a phrase outside a hunt when voice recognition is on,
  GhostInfo +0x48 is its name as the recogniser hears it, and ChangeState(9) knocks on a door near it.
  - Mare: the switch-off coroutine's MoveNext (LightSwitch nested, 0x26FFD10, bool(this), hooked to mark
  its step) makes the Random.RandomRangeInt(0, 7) roll; a withheld appearance's event ends with
  ChangeState(0) on the next frame; GhostAI.Appear (0x2020FB0) is called on the host for an
  event's appearance, with the ghost already where it will show itself (its room is read there); UseLightSwitch(false, true, true) from the host switches a lit switch off for
  everyone outside a hunt (it refuses while GhostAI +0xF4 is set: believed to be the Alan Wake
  event's flag, unconfirmed); LightSwitch +0x68 and +0xE0 mark a switch it cannot use.
  - Shade: GhostAI +0xA8 is set while it shows itself; ChangeState(0) ends its event cleanly and
  UnAppear (0x202E140) hides it; the hunt state's +0x48 is a Player; +0xB4 back to 1 resets the
  line-of-sight speed-up.
  - Yurei: the WhiteSage handler at 0x8256B0 is the one that runs (not a decoy) and runs on the client
  whose incense it is, or the host; LevelRoom +0x28 is the room's Door[] its ability picks from with
  Random.Range(0, length), its ability's entry (0x201F3D0) changing the ghost to idle (whose entry rolls
  Random.Range(0, 4)) before it rolls over LevelController +0x38's doors, so the steer answers only the
  roll after that nested ChangeState(0); its ability forces its door with Door.GhostForceDoor inside
  ChangeState(15); tarot cards pass ChangeState's forced flag and the gate lets them through;
  Door.HuntingCloseDoor locks a door so a player trying it gets the hunt's locked response, and
  ToggleDoorLock(true, true) unlocks it; ChangeState(15) from the host runs the Yurei's ability.
  - Goryo: the copy of its model on layer "LaserGhost" shows on a placed video camera's feed and on the
  truck monitor, never to a player's eyes or in a photo; the CCTV's transform faces where it films;
  GhostActivity.TryInteract from the host acts as its idle roll's interaction; hunt state +0x50 holds the
  player it searches for after losing or hearing them (it heads home only with +0x48 and +0x50 empty); set_destination every
  second steers a hunting ghost home without the hunt state overriding it at once.
  - Onryo: FireSource.IgniteFire (0x6528C0) is what a player's lighter calls, IgniteFireNetworked
  (0x6526A0) is the RPC every client runs, so refusing IgniteFire alone keeps a flame dark everywhere; GhostAI
  +0x120 is its count of blown-out flames on the host; FireSource +0x38 Light intensity and +0x30
  flame scale change how a flame burns (a vanilla flicker script may set the intensity every frame).
  - Playable Ghost: a player controlling the Spirit or Shade sees interactions, events, hunts and the fuse
  box refused, without losing the charge, while the tells hold the ghost back (custom_link::held_back
  asks the ghost-tells service).

  Prompt:
  > In this repository, test in Phasmophobia build 25811160 with Ghost Tells on every client (two
  > clients, private lobby), on a custom difficulty that forces each of the six
  > ghosts in turn (Spirit, Mare, Shade, Yurei, Goryo, Onryo). Check mods.log for "GhostTells: on for
  > this contract" and each ghost's lines (spirit:, mare:, shade:, yurei:, goryo:, onryo:). Spirit:
  > incense it in a hunt (it must stand still about 3 s, silent), incense it outside one (nothing for a
  > minute), say its first name within 10 m (a knock 1 to 3 s later, not again within 20 s). Mare:
  > switch a light on near it again and again (off within a second about half the time); wait for an
  > event in a lit room (the lights go out first); after a hunt, the rooms it passed are dark. Shade:
  > stare at it during an event (gone within half a second); stand two players beside it (nothing
  > happens); in a hunt, stand together in its view (it stops 1.5 s). Yurei: incense its room (its
  > doors shut, and shut again 3 s after being opened, for 90 s); after its ability, try the door (locked
  > 3 s); leave a door of its room open (its ability within about a minute). Goryo: a video camera on it
  > in its room, everyone 10 m away, watch the truck monitor (it stands visible now and then; never to
  > the eye or in a photo); in a hunt, break its sight 15 m from its room (it walks home). Onryo: light a
  > candle within 4 m (blown out half the time, and it will not relight for 15 s); after two blow-outs
  > every flame burns low. Read the journal page of each. Fix what differs and update this entry.

## 26. The Ghosts tab grid fix and the custom difficulty's custom ghosts have never run inside the game

  host/content_difficulty.cpp:206

  Since stage 2b the host's content layer does both (host/content_journal.cpp,
  host/content_difficulty.cpp, host/content_types.cpp) in place of Custom Ghosts' own pages.cpp,
  difficulty.cpp and registry.cpp, which the items below name; the same game facts stand behind it.
  Both come from REA decompiles, the class dump and the scene files, not from a running game, and
  pass only offline tests (tests/content_journal_test.cpp, tests/content_difficulty_test.cpp,
  tests/content_test.cpp).
  A wrong offset or calling convention here does not fail quietly: a custom ghost type could reach
  the save file or a player without the mod, whose menu then throws and whose contract spawns a ghost
  with no data. Unverified in the game:
  - pages.cpp use_grid: the Ghosts tab list's GridLayoutGroup switched on keeps the 30 vanilla
  entries where they were and lays out all 45 readably (each name fits a 65.8-unit row).
  - difficulty.cpp: FindObjectOfType(Type, includeInactive) finds the DifficultyUIManager while the
  game hides its Difficulty Selection board (seen live 2026-10-21: the plain overload missed it, so a
  contract started without opening the board rolled a ghost instead of Custom 1's Brownie); the Ghost Type
  option's change handler is 0x96DD20 and its count is DifficultyOption+0x70; EasySave3's shared
  generic code at 0x2ECD840 (Save: key, value) and 0x2ECCA40 (Serialize: value, settings) takes the
  arguments the ISIL call sites pass; ForceSyncDifficulty re-sends to every client (SyncDifficultyValues
  0x95C760 makes the lobby's difficulty anew); the contract's spawn forces a custom type picked in the
  lobby; a player without the mod who joins gets Random; switching the mod off in the lobby leaves
  every difficulty at Random; show_pick's call of the handler labels the option and changes nothing
  else; registry.cpp's List<GhostData>.Insert (shared code 0x2FB9780) puts each custom ghost in its
  alphabetical place; the summary 0x95E010 boxes GhostType with the class in the metadata slot at
  0x644D320, and the Enum.ToString (0x41FFBC0) detour renames only that. MultiplierTooltip calls DifficultyUIManager.GetGhostType (0x958E80), which throws when the
  menu's collection lacks the lobby's ghost: hovering the reward multiplier in the moments before
  the mod offers the custom ghosts (a client the host counted first; the lobby after a contract,
  up to a second) may log a NullReferenceException.

  Prompt:
  > In this repository, test in Phasmophobia build 25811160 with Spectral Flame and Custom Ghosts on
  > every client (two clients, private lobby). Open a journal's Ghosts tab: all 45 ghosts must show in
  > three columns with no overlap. In Custom 1, step through the Ghost Type option: all 45 ghosts
  > must come in alphabetical order (Asterion first, Black Shuck after Banshee, Yurei last). Pick a
  > custom ghost: the difficulty summary's first line must read "Ghost: <its name>", not a number.
  > Pick a vanilla ghost, have a player without the mod join and leave: the option must keep naming
  > it. Pick the custom ghost again, apply, and start a contract: that ghost must haunt it, and
  > %LOCALAPPDATA%\PhasmoMods\custom_difficulty.txt must hold "6=<ghost id>". The other client's option
  > must name that ghost too, and so must both after the contract, back in the lobby. Restart the
  > game: Custom 1 must show the pick again. Restart the game once more, open a lobby and start a
  > contract without ever opening the difficulty board: the pick must haunt it. Have a player without the mod join: the option must fall
  > back to Random on every client, and back to the pick when they leave. Switch Custom Ghosts off in the Mods panel
  > and check the difficulty reads Random. Read mods.log for "difficulty:" lines and the save file's
  > Difficulty entries (EasySave3) for any ghost type above 30. Fix what differs and update this entry.

## 24. The ten flame ghosts have never run inside the game

  mods/custom_ghosts/mod.cpp:1

  The Lithobolia, Gremlin, Brownie, Bogeyman, Lethe, Mesmer, Knell, Asterion, Tempest and Hydra come
  from REA decompiles, the class dump and the scene files, not from a contract. Each rule's arithmetic
  is tested offline (tests/custom_ghosts_test.cpp); everything below is unverified in the game:
  - Mesmer: the one-shot sound (sound.cpp repeats PlayClipAtPoint's steps, which this build strips).
  - Hydra: the stripped model copies (apparition.cpp: scripts, colliders, rigidbodies and sounds
  destroyed before waking) render, animate from the copied Animator parameters and flicker with
  the real model; the smudge read at GhostAI+0xF8; the false heads' NavMeshAgents.
  - Asterion: no door closes behind it; each closed door in its way costs it 3 s and stays jammed
  open. Doors closed by Door+0xB0 against the leaf's local angle on HingeDoor+0x194's axis;
  GhostForceDoor leaves UV prints only for a ghost with Ultraviolet (the game's check is assumed);
  CreateDoorSlamNoise as the bang; DisableOrEnableDoor(false) keeps a jammed door open; the leaf
  Rigidbody's worldCenterOfMass is the middle of the leaf (a door is in its way when its heading
  crosses the hinge-to-middle line, asterion_rule.h door_ahead).
    Live, 2026-11-03 (user's first test sheet): the bangs and the 3 s are right, but the door stays shut
    after "asterion: a door is broken open" (logged 4 times), so the ghost passes through it as any ghost
    does; and of a double door only one leaf is battered. Decompiled 2026-11-05: GhostForceDoor (3 args)
    starts its push only while Door+0x180 (Door.forceRoutine) is null, and the push switches the door
    back on as it ends, undoing a jam sent with it (fixed: DoorJam sends the jam after the push). It
    always pushes with physics, while GhostOpenDoorAnimated picks the physics push only for a door with
    useForceInsteadOfAnimations (Door+0xC8) set, else the animated move. Why the leaf stays shut is not
    known yet: each burst now logs whether a push was already running, the door's physics flag and
    kinematic state, and how far the leaf has turned 1 s and 3 s later. Open: which leaves make a double
    door (Door+0xF8 and +0x100 hold Doors; Door.Convert copies both; the hunt's own push skips a door
    whose +0xF8 is set and whose +0x21 is set).
  - Lithobolia: the throwing state is 6; ownership taken over before the aimed launch; the hit
  message and PlayerSanity.ChangeSanity(+5) on the hit player's client. No throw with nobody in
  sight (the game's sensor, GhostAI+0xA0, via Sensor.IsDetected); hunt throws (Prop.TryThrow from
  the hunt state, only while hunting) fly at the hunt target (+0x48) when the sensor sees it within 8 m, else the throw is swallowed; the AI only.
  - Lethe: one second after the hunt's own sight test (0x214BB40, through the trampoline) stops passing
  for the player its hunt still looks for (its last player, +0x58, kept when its chase drops its target
  +0x48), it clears them there and from +0x50, as the game's kill does, and no longer wanders where it
  lost them ("lethe: it forgets its prey").
  - Gremlin: Equipment.Use through each class's override switches a device off for every client;
  IRLightSensor is the D.O.T.S. projector (its prefab says so).
  - Bogeyman: ClosetZone.CheckCloset searches a closet as the game's hunt does; clearing the hunt
  target (+0x48) makes it lose prey. Its AI's hunts start with a closet door opening next to the
  lowest-level player, never the closet they hide in (Player +0x30): Network.PlayerSpot +0x2C (level) and
  +0x30 (prestige) hold every player's progress on the host (ServerManager.UpdatePlayerLevelInformation);
  ChangeState(2) has made a new hunt state, current, when it returns; NavMesh.SamplePosition (0x518F100)
  finds the NavMesh within 1 m of a ClosetZone's nav spot (+0x48), NavMeshAgent.Warp moves the ghost there,
  and every client sees it there. It goes for them when two are in sight: the game's own sight test
  (0x214BB40, called for each player through the trampoline) passes only the youngest of those it sees,
  leaving out those beyond 7 m in an open room, and the game's first-seen pick (0x214D480) then targets
  them. Most of its door interactions are closets: when GhostActivity.GetDoorToOpen (0x2005FC0), called
  from the idle door chance (0x2007C60, told apart by return address), has picked a door, 3 in 4 times
  it returns instead a closet door that the door sensor (GhostActivity +0x20) detects, as Sensor.IsDetected
  on the door's GameObject reports, and the game's door interaction (ChangeState(5) on the door's
  PhotonObjectInteract, Door +0x98) opens or closes it, with prints. A lone player beyond 7 m in an open
  room fails its sight test, so it does not chase them. Its walk to the hiding places, like the Black
  Shuck's trail steering, waits out the hunt-start delay (hunt state +0x8C below 0, as the hunt's gate
  0x213F6F0 reads it). When the delay ends the gate sends the ghost towards GhostAI +0xDC (through
  0x2155C00): check that the Bogeyman still stands by the closet it stepped out of then, and that the
  youngest player has that delay to react.
  - Speed rules (behaviour.cpp, speed_rule.h): no speed blip every 2 s in any custom hunt (the base
  refresh detour); at 50% and 150% ghost speed and in Blood Moon each custom ghost's speed scales
  once, as a vanilla ghost's does.
  - Brownie: the Player transform's forward is where the player looks (no pitch). Its steps reach 12 m
  in hunts and 20 m outside, heard from a client that is not the host too (Footstep play hook,
  0x243E380, then Noise.SetMaxDistance). A Brownie, Knell and Dullahan's step rate and running sound
  follow their real speed (steps quicken in a Knell surge); no Knell step plays apart from the game's own.
  - Lethe: replays warp the agent and call GhostAI.Appear with the recorded form; JournalMedia.
  RemoveMedia(false) on every journal removes a slot everywhere and from the reward lists; Media+0x10
  -> MediaValues+0x1C is the photo's subject (lethe_activity lists which subjects fade).
  - Tempest: LightningController.PlayLocalLightning strikes in a contract once its effects (+0x28,
  null there) are given RandomWeather's (+0x38); PlayLightningNetworked runs on every client for the
  game's own heavy-rain strikes; setting the model's renderers in the LateUpdate tick wins over the
  game's hunt flicker (hidden between strikes, shown in full in one); the strike speed holds.
  - Mesmer: layer 3 is unused by every camera but players' own; the ghost models render on a layer of
  their own (else the fake is skipped, logged); Camera.allCameras holds the CCTV and photo cameras;
  HeartRateData.UpdateUI reads PlayerSanity+0x30 for the monitor; the hunt fake animates from the
  Animator parameters its form shares with the true one; skipping LightSwitch.FlickerNetworked in
  its room hides the interaction flicker and nothing else (LightFlickerEventNetworked is untouched).
  - The journal's ghost page and the Ghosts tab's list with 45 entries: every name fits its button.

  Prompt:
  > In this repository, play private contracts in Phasmophobia build 25811160 with Spectral Flame and
  > Custom Ghosts on every client (two clients), forcing each flame ghost in turn (custom difficulty,
  > or Playable Ghost's pick). For each, check its strength, weakness and tell as docs/custom-ghosts.md
  > describes, and mods.log for each module's "resolve ok" and its own lines. Fix what differs, and
  > update this entry with what was confirmed.

## 23. Test fakes bind any method name, so a method missing from the game passes every test

  tests/menu_panel_test.cpp:96

  Each test's il2cpp::method_ptr fake returns a function for whatever name the code asks for. The Mods
  panel shipped asking for CanvasRenderer.SetAlpha, which this game build strips: the suite passed and
  every panel open failed in the game ("method SetAlpha/1 not found on CanvasRenderer" in mods.log,
  fixed 2026-10-20 with GetColor/SetColor). Nothing compares the names the host and the mods resolve
  (il2cpp::method_ptr, il2cpp::method, bindings) with re/names.tsv, so the next renamed or stripped
  method is found the same way, in a live session.

  Prompt:
  > In this repository, add an offline check that every (class, method, argument count) the host and the
  > mods resolve through il2cpp::method_ptr / il2cpp::method (core/il2cpp.h) exists in re/names.tsv,
  > the method table of the supported game build. Decide whether to scan the sources for the literal
  > calls or to record the lookups each test's fake receives; overloads by argument count must match.
  > Point the check at a deliberately misspelt name first and watch it fail, then add it to the test
  > command so `env -u PSModulePath harness-verify` runs it.

## 19. The Mods panel, the themed notice and the join guard have never run inside the game

  host/mods_panel.cpp:1

  The Mods button (a copy of the main menu's hidden News tape), the popups copied from ErrorManager's
  ErrorParent and moved to where the menu camera looks, restarting mods from the panel (every mod
  stopped, its hooks rolled back, the choice started again), and the join guard's leave
  (ServerManager.LeaveServer) and kick (ServerManager.KickPlayer by ID card) are verified only against
  the scene file and decompiled code.

  Prompt:
  > In this repository, play through host/mods_panel.cpp, host/menu_panel.cpp, host/consent.cpp and
  > host/join_guard.h in Phasmophobia build 25811160 with two clients. Check: the notice appears as the
  > game's pinned popup over the main menu; the Mods tape sits where News would and opens the panel;
  > switching a mod and Apply restarts the mods (mods.log: "restarting the mods", each mod stopped and
  > started, no exceptions) and the mod works afterwards, including Playable Ghost's steering; a
  > joiner with a different mod list leaves with the note naming the difference; a client without
  > the mod stays (every current mod has PG_MOD_VANILLA_PEERS).

## 14. The ghost's senses, alerts, Mimic choice and disguise have never run inside the game

  mods/playable_ghost/marks.cpp:1

  Verified against decompiles and the class dump only: GhostHearing's detected list (16-byte
  entries of PlayerNoise and distance) and PlayerNoise +0x48/+0x50; the Dayan's sight list
  (GhostAI +0x138, nearest first, filled by DayanPlayerChecks) and NetworkedStamina +0x4C; the Raiju's
  electronics RangeSensor and Sensor.GetDetections(null); the Yokai's voice taps muted by distance;
  the marks' projection (Camera.WorldToScreenPoint and the canvas scale factor); the thirteen alert
  hooks (signatures from the dump, PhotonMessageInfo by pointer; which of them run on the ghost
  player's own client is unknown, so the host relays); the summoning detected as a jump onto a circle;
  the Mimic's override written before AttemptGhostTypeStateChange; and the disguise's baked meshes
  (SkinnedMeshRenderer.BakeMesh, materials, layer 11 left out), which assume the investigator's body
  is under their Player object.

  Prompt:
  > In this repository, play the ghost in a private contract as a Kormos,
  > a Deogen, a Dayan, a Raiju and a Yokai in turn (pick the type), and check each sense against what
  > the ghost should know; then as a Mimic pick copies with Crouch and check the copy changes when the
  > game's timer fires. With an investigator, use a crucifix, salt, the music box, a tarot card, the
  > Ouija board, sage near the ghost, and say the ghost's name: each alert should show once, with a
  > sensible direction. Hold Crouch and tap Primary Use near an investigator: everyone else should see
  > their body standing where the ghost is for five seconds. Check mods.log for exceptions; fix.

## 13. A custom ghost's type is briefly visible to listeners of the ghost's sync

  host/content_types.cpp:366

  GhostInfo.SyncValuesNetworked invokes a UnityEvent and calls GetFullEvidence before returning, and
  the host's content layer (ContentTypes::after_sync_values, which replaced Custom Ghosts' own
  registry) sets the behaviour type (GhostType.None) only after it returns. Anything listening to that
  event sees +0x2C still holding a custom type for that moment, which the game's type checks do not know. A review of the custom ghosts found it; it was not traced to a visible fault.

  Prompt:
  > In this repository, find the listeners of the UnityEvent that
  > GhostInfo.SyncValuesNetworked invokes (decompile with re/decomp.mjs) and check whether any reads the
  > behaviour type (+0x2C). If one does, set the behaviour type before calling the original (and keep
  > GetFullEvidence working, which needs the custom entry in the collection), then test a custom ghost.

## 12. The custom ghosts have never run inside the game

  mods/custom_ghosts/mod.cpp:1

  Everything in mods/custom_ghosts is verified against decompiled code and offline tests only (since
  stage 2b the entries, behaviour type, keys, journal and hunt speed writes below are the host's content
  layer's, which writes the hunt speed before every speed update instead): the
  GhostData entries made with il2cpp_object_new (no Unity object behind them), behaviour type
  GhostType.None set in GhostAI.Init and GhostInfo.SyncValuesNetworked, the localisation keys, the
  journal clones (ghost buttons, Ghosts-tab entries and pages, the grid cell height, the journal's
  lists, map, page array and the ghost sync controller's button array), the hunt speed written into
  the hunt state (base +0xB0, line-of-sight speed-up +0xB4, put back after the game's two-second base
  refresh 0x214C2A0), the Draugr's room survey, the Kikimora's ownership takeover and teleport, and
  the Echo's Vivox tap capture, clip transfer and playback. LevelStats.SendData reports the ghost type
  to the game's analytics as text, so a custom ghost is reported as its number ("31" on); the mod does not
  touch telemetry. The Black Shuck's hunt rests on detours of the hunt state's sense tests by RVA (sight
  0x214BB40, voice 0x214DDA0, electronics 0x21554D0), its set-target 0x2150AB0 handing a hidden
  player to its closet search 0x213B840, and NavMeshAgent.SetDestination; on the game's own
  eligibility test 0x214FD80 and closet setter 0x213FA10; on its writes to the hunt state's target,
  search player, last player and seen set (+0x48, +0x50, +0x58, +0x68); and on holding the hunt's
  kill reach (+0xBC, +0xC0) at 0 while the game's vision sensor does not see its prey. A sink counts
  as running water while DOTween.IsTweening reports its fill tween playing (a tap turned on and off
  again should no longer break scent), and whether baths and showers are Sinks at all is unknown.
  The Echo's repeat rests on detours of the RPCs every client runs for a switch's use
  (LightSwitch.Use 0x26E5940, a player's when its byGhost and counted flags are both clear) and a
  player's door grab (Door.NetworkedGrabbedDoor 0x264D150), on LightSwitch +0xD0 and LevelRoom +0x28
  for its room, on the ghost's interaction sensor (GhostActivity +0x20) detecting the object when the
  repeat pulses it where the ghost stands, as TryInteract does (RangeSensor.SetBoxShape 0x3E99F90, then
  the sensor class's vtable entry +0x178), with its OnPulsed delegate (+0x128, the ghost's whole
  interaction) set aside for that pulse and put back, and on the game's own light and door states
  (GhostAI.ChangeState 4 and 5) started from TryInteract.

  Prompt:
  > In this repository, run a private contract where every client runs
  > Custom Ghosts, with a custom type (pick it as the playable ghost). Check mods.log for "registry:
  > 5 custom ghosts added" and "pages: custom ghosts added to a journal" on each client; open the
  > journal: 35 ghost buttons in alphabetical order that fit the page, Ghosts-tab entries that open
  > their pages, evidence filtering crosses them out, other players see your choice. Name the ghost
  > correctly and check the results pay for it. Then check each ghost's rule: Dullahan speed in
  > corridors and at corners, Draugr hunting early after an item leaves its room, Black Shuck (AI)
  > walking past a visible, noisy player whose trail is cut by salt and casting about elsewhere
  > rather than going to where they stood, following a walked path round corners to the player and
  > killing them but never through a thin wall, dropping a followed player who reaches the truck,
  > crediting the escape objective to a player who escapes its hunt, finding a player hiding in a
  > closet by their trail (the closet rattles open, then the kill), stopping for a smudge, and losing
  > a trail at a running tap and outdoors in rain but not at a tap turned on and off again (try a
  > bath and a shower too and note whether they break it), and a ghost player seeing the darkened
  > hunt view with the scent marks, Kikimora moving dark-room equipment (seen moving on every
  > client), Echo repeating a phrase in a teammate's voice from the ghost, and with voice chat off,
  > a light switched or a door moved in the Echo's room is the next thing it touches within a minute
  > ("echo: a player used a" and "echo: repeats a player's" in the host's log; EMF at that switch or
  > door), also when a client other than the host used it, never before the ghost comes near it,
  > still within the minute while the Echo is dormant (nobody talking), with no other interaction
  > (prints, EMF, a door or switch moving) at the moment of each repeat's check, and that a dormant
  > Echo still loses most of its other interactions.

## 11. The mod host, its consent notice and mod loading have never run inside the game

  host/suite.cpp:1

  The split into ModHost.dll and mod DLLs (host/, core/mod_glue.cpp) is verified offline only:
  the consent banner's look and its blocking of the main menu's clicks (a GraphicRaycaster on an
  overlay canvas), hooks created on the main thread after start-up and chained through the host,
  mod channels on event 197, presence announcements, the launcher's mod list and session file.

  Prompt:
  > In this repository, build, open dist\PhasmoMods\ModLauncher.exe,
  > tick Playable Ghost and attach to a running game at the main menu. Check in
  > %LOCALAPPDATA%\PhasmoMods\mods.log: "ModHost active", "mods: loaded playable-ghost", the
  > banner covering the menu with clicks not reaching it, "consent: agreed" after I agree, then
  > "[playable-ghost] PlayableGhost active". Restart and check the answer is remembered; choose Play
  > without mods once and check no mod starts. In a private lobby with two modded clients, check
  > each logs the other's presence (peer_has_mod) and the playable ghost's lobby tape still works.

## 10. The vanilla action and movement rules have never run inside the game

  mods/playable_ghost/actions.cpp:1

  The rewritten action layer (typed charges from the native idle roll, the TryInteract hook, the
  unforced hunt chance, restlessness by attempts, the Twins follow-up), the wander reach and
  search pacing in mods/playable_ghost/control.cpp, and the fuse box on the "Torch" action are verified against
  decompiled code and offline tests only. The input action name "Torch" is a guess from the
  game's Controls_Torch text; the key falls back to T.

  Prompt:
  > In this repository, host a contract as the ghost player in a private lobby and watch
  > %LOCALAPPDATA%\PhasmoMods\mods.log. Check: "native roll granted" lines appear for each kind
  > (interaction, event, ability, fuse box) and native interactions stop while you act; a hunt is
  > offered only after a passed roll and is withdrawn at the next hunt check; you can move only
  > during a wander and within its reach; after losing a target in a hunt the search ends where
  > you are and the speed-up then decays; the torch key toggles the fuse box (log the input action
  > names if it does not); a Twins ghost's twin interacts after some aimed interactions; and 30 s
  > without trying to act lets the native AI act until you press any action key.

## 8. Duplication remains across gameplay bindings, UI helpers and tests

  core/game.cpp:90

  jscpd reports 26 clones, 192 duplicated lines across 107 C++ source and test files. Examples are
  the method-binding helper repeated in core/game.cpp, core/ui.cpp, host/net.cpp and senses, UI
  helpers in hud and notice, and the payout and session tests' scaffolding. The zero-duplication
  scan fails.

  Prompt:
  > Run jscpd on core, host, mods, tools and tests. Review all reported
  > clones, including test scaffolding, and consolidate meaningful duplication without changing
  > gameplay. Do not hide the findings with exclusions or a higher threshold.

## 4. A held ghost spawn leaves every client without a GhostAI for up to 30 seconds

  mods/playable_ghost/round.cpp:231

  While the ghost player picks, the host holds GhostController's spawn, so LevelController.ghost is
  null on every client. Vanilla tolerates this while players load, but systems started after the
  all-loaded event may throw every frame until the ghost spawns, which costs frame time and floods
  Player.log.

  Prompt:
  > In mods/playable_ghost/round.cpp, detour_spawn holds the native ghost spawn for up to kPickMs.
  > Run a contract with a ghost player, let the pick run its full 30 seconds, and read Player.log
  > for exceptions during that window. If any system throws per frame, find what it needs and
  > decide whether to shorten the hold or hold something narrower than the whole spawn.

## 3. The ghost payout base may already include the difficulty multiplier

  mods/playable_ghost/reward.cpp:86

  contract_ended reads ObjectiveValues.moneyReward (LevelValues+0x50, then +0x10, then +0x30) as
  the unscaled main-objective reward. If the game stores it already multiplied by the difficulty,
  payout::ghost cancels the multiplier out and pays roughly the same on every difficulty instead of
  less on harder ones.

  Prompt:
  > In mods/playable_ghost/reward.cpp, log mainReward and the difficulty multiplier at the end of a
  > contract on two different difficulties. If mainReward changes with the difficulty, base the
  > ghost payout on mainReward divided by the multiplier (or find the unscaled value) so
  > mods/playable_ghost/payout.h's inverse scaling holds; extend tests/payout_test.cpp to match.

## 2. Favourite-room indices assume every client orders the level's rooms the same way

  mods/playable_ghost/picker.cpp:65

  The picker sends the chosen favourite room as an index into LevelController+0x50, and the host
  applies it to its own array (mods/playable_ghost/round.cpp detour_random_index). If clients ordered that array
  differently, the ghost player would pick one room and get another. The native SetGhostRoom RPC
  also sends indices, which suggests the order is shared, but it is not verified.

  Prompt:
  > In mods/playable_ghost/picker.cpp, the picker builds the room list from round::room_name(i) over
  > LevelController+0x50 and sends indices; mods/playable_ghost/round.cpp forces the host's roll to that index.
  > Confirm in game, on two clients, that index i names the same room on both (log the names). If
  > not, send room names and resolve them on the host instead.

## 1. The round flow has never run inside the game

  mods/playable_ghost/round.cpp:1

  Every hook added for the round flow (spawn hold and probe, type and room rolls,
  LiftButton.AttemptUse, ExitLevel.Exit, AddXPAndFunds, the lobby equipment calls, parking the ghost
  player's investigator out of play) is verified only against decompiled code. Native
  activation has succeeded. The lobby toggle was rebuilt on 2026-10-19 (a half-size Ready tape on the
  player's own ID card) after it never appeared, and has not been seen in game yet. Nor have the
  journal picker (mods/playable_ghost/journal.cpp, mods/playable_ghost/mapview.cpp, mods/playable_ghost/picker.cpp) or the ghost's pause overlay
  (the pause handling in mods/playable_ghost/journal.cpp and leave.cpp). No full contract has been watched working; an incorrect offset or signature may still
  fail during gameplay.

  Prompt:
  > PlayableGhost (mods/playable_ghost/round.cpp, mods/playable_ghost/body.cpp, mods/playable_ghost/equipment.cpp, mods/playable_ghost/reward.cpp) has never run
  > through a complete contract inside Phasmophobia build 25811160. Play one contract as the ghost
  > player and one as an investigator with a second client, watch %LOCALAPPDATA%\PhasmoMods\mods.log
  > and Player.log, and fix whatever the hooks get wrong. Check: the picker appears, the truck door
  > refuses to open with the popup until the pick ends, the house key appears in the truck for every
  > client, the ghost player's investigator is invisible to everyone and takes no part (left out of
  > the average sanity hunts use, never hunted), the ghost player's journal shows the investigators'
  > choices, equipment leaves and returns, and the payout applies. Leave from the pause menu as the
  > ghost player: mods.log shows one "stepped out" line for the contract, and no torch light or
  > sound stays on in the truck where the body is parked.
