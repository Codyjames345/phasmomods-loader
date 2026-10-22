# Findings

Problems found while doing something else, and not fixed at the time. The format and the
rule behind it are in `standards/repository.md`. Each entry carries a prompt written to be
pasted into a session that was not here for the conversation that produced it.

An entry is deleted in the change that fixes it. `/fix` takes a number or a few words from
a heading and does both halves; `/finding` writes a new entry; `harness-findings verify`
reports an entry whose problem is already gone.

<!-- next: 31 -->

## 30. The reverse-engineering scripts point at setup steps that do not exist

  re/decomp.mjs:3

  decomp.mjs says the one-time Ghidra import is described in re/README.md, and that file has never
  existed. callers.mjs, decomp.mjs and slimpe.mjs all read re/names.tsv, which is git-ignored because
  it is derived from the game's binaries, so a fresh clone has no way to rebuild what they need
  except from notes kept outside the repository.

  Prompt:
  > re/decomp.mjs:3 refers to re/README.md for the one-time Ghidra import, and that file does not
  > exist. Write re/README.md describing how to set up the reverse-engineering tools from a clean
  > clone: generating a Cpp2IL DiffableCs dump from copies of the game files (never inside the game
  > folder), building re/names.tsv with re/names.mjs, the headless Ghidra import into re/ghidra that
  > decomp.mjs expects, and the GHIDRA_INSTALL_DIR variable. re/ghidra, re/out, re/names.tsv and
  > re/import.log are git-ignored (see .gitignore). Name no path that exists on only one machine;
  > read the install locations from variables or arguments instead. Run each documented command once.

## 29. The game's storm lightning throws on the ghost player's client in heavy rain

  mods/playable_ghost/body.cpp:214

  body.cpp clears Network+0x28 (the local Player) on the ghost player's client after stepping out.
  LightningController.PlayLightningNetworked (0x2319F40), the RPC LightningController.Update sends
  to every client every 0 to 30 s while the contract's weather is heavy rain, reads Network+0x28
  in its heavy-rain branch whenever GameController's instance exists, and raises a
  NullReferenceException when it is null (decompiled 2026-10-22). So in any heavy-rain contract with
  a ghost player, that client throws on every storm strike and sees none of the game's lightning,
  whatever the ghost type. Not seen in a log yet. The Tempest counts the strike before the game's
  handler runs (tempest.cpp detour_storm_lightning), so its speed does not depend on this. The
  fix belongs with the other null-local-Player cases (senses.cpp's VoiceVolume hook).

  Prompt:
  > In this repository, the ghost player's client has no local Player (mods/playable_ghost/body.cpp:214
  > clears Network+0x28), and LightningController.PlayLightningNetworked (GameAssembly RVA 0x2319F40,
  > decompile it with re\decomp.mjs) throws a NullReferenceException on it in heavy rain. Confirm it in
  > a heavy-rain contract with a ghost player (the game log shows the exception at each strike), then
  > make that client play the strike as an investigator's client does, the way senses.cpp handles
  > VoiceVolume.Update, without changing what any other client sees. Never send the lightning RPC
  > yourself: a client whose weather is not heavy rain runs the game's cheat report on its sender.
  > Run `env -u PSModulePath harness-verify`.

## 28. Ghost Tells copies four private helpers of Custom Ghosts instead of sharing them from core

  mods/ghost_tells/world.cpp:85

  jscpd finds Ghost Tells repeating helpers that live privately inside Custom Ghosts files:
  distance_to_room (mods/custom_ghosts/behaviour.cpp, as world.cpp's), the LevelController room list
  (kikimora.cpp rooms(), as world.cpp's), the running-CCTV scan and view test (brownie.cpp tick_host,
  as goryo.cpp on_camera), and the TryInteract/ChangeState hook creation (restraint.cpp, as gate.cpp).
  Each mod also repeats the same everyone-has-this-mod check (spectral_flame, custom_ghosts,
  ghost_tells mod.cpp). Two copies of each can drift: a fix to one room or camera test misses the other.
  The fix is to move each helper into core (game.h or players.h) and call it from every mod; it was left
  because it rewrites Custom Ghosts files that other sessions were changing at the time.

  Prompt:
  > In this repository, move these helpers into core and use them from every mod: distance_to_room
  > (mods/custom_ghosts/behaviour.cpp, mods/ghost_tells/world.cpp), the LevelController +0x50 room list
  > and room-at-point lookup (mods/custom_ghosts/kikimora.cpp, mods/ghost_tells/world.cpp), the running
  > placed-video-camera scan and its view test (mods/custom_ghosts/brownie.cpp, mods/ghost_tells/goryo.cpp),
  > and the everyone-runs-this-mod check (mods/spectral_flame/mod.cpp, mods/custom_ghosts/mod.cpp,
  > mods/ghost_tells/mod.cpp). Keep behaviour identical, run `env -u PSModulePath harness-verify`, and
  > check `jscpd --format cpp --min-tokens 50 mods core` reports none of these pairs.

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

  mods/custom_ghosts/difficulty.cpp:252

  Both come from REA decompiles, the class dump and the scene files, not from a running game, and
  pass only offline tests (tests/pages_test.cpp, tests/difficulty_test.cpp, tests/registry_test.cpp).
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
  > %LOCALAPPDATA%\PhasmoMods\custom_difficulty.txt must hold "6=<type>". The other client's option
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
  - Asterion: doors closed by Door+0xB0 against the leaf's local angle on HingeDoor+0x194's axis;
  GhostForceDoor leaves UV prints only for a ghost with Ultraviolet (the game's check is assumed);
  CreateDoorSlamNoise as the bang; DisableOrEnableDoor(false) keeps a jammed door open; the leaf
  Rigidbody's worldCenterOfMass is the middle of the leaf (a door is in its way when its heading
  crosses the hinge-to-middle line, asterion_rule.h door_ahead).
  - Lithobolia: the throwing state is 6; ownership taken over before the aimed launch; the hit
  message and PlayerSanity.ChangeSanity(+5) on the hit player's client. No throw with nobody in
  sight (the game's sensor, GhostAI+0xA0, via Sensor.IsDetected); hunt throws (Prop.TryThrow from
  the hunt state, only while hunting) fly at the hunt target (+0x48) when the sensor sees it within 8 m, else the throw is swallowed; the AI only.
  - Lethe: it drops its target one second after the game's sensor loses them.
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
  PhotonObjectInteract, Door +0x98) opens or closes it, with prints.
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

## 20. Spectral Flame has never run inside the game

  mods/spectral_flame/flames.cpp:1

  Everything in mods/spectral_flame and the mod requirement (requires= in mod.ini, enforced by the
  host, the launcher and the Mods panel) comes from REA decompiles, the class dump and the scene files,
  not from a contract.

  First contract (2026-10-21, solo host, Brownie, Tier 2 Firelights; the host's 5-second "flames:"
  report in mods.log): "resolve ok", "on for this contract" and the journal row confirmed;
  LevelController+0xC0 lists the Firelights' fire sources and +0x38 follows the ghost from room to
  room. No flame turned blue: "placed" was read from Candle+0x410, which only the Tier 3 lantern
  switches (now Equipment+0x19C, set by PlaceNetworked and cleared by GrabNetworked).

  Second contract (2026-10-21, same setup, the fix): placed confirmed; a Tier 2 Firelight in the
  ghost's room built 4% -> 23% in about 10 s of calm ghost time, faded once a hunt began, and found
  1 material and 1 light to paint. The given evidence read empty in both contracts because the
  preset used (Custom 1, "evidenceGiven" 0 in the save) gives no evidence, so the flame stayed under
  the false-positive cap; GhostController+0x30 (host) and GhostInfo+0x30 (every client, now read)
  both hold the given list.

  Third contract (2026-10-21, evidence given): the tint shows, but the old full blue (the warm HDR
  brightness kept) rendered white/faint blue. Rebalanced the same day at the user's request (docs/
  spectral-flame.md): orange, white-hot, then a pale sky blue at a false cap of one half, the game's
  "Gas Flame Blue" (a lantern's own _Color_1) at full; 120/100/80 s by tier with a 20 s warm-up;
  hunts count; 240 s fade; a ghost giving Spectral Flame cannot blow out a set-down flame until it
  has been fully blue (FireSource.BlowOutFire detour, host). The report now lists each lit flame's
  warm-up and remaining burn seconds. The colours are checked offline only through an approximate
  tone curve (tests/spectral_flame_test.cpp on_screen); the game's own curve may differ.

  Fourth contract (2026-10-21, Knell, two Tier 2 Firelights): the warm-up, 100 s to full blue and
  the BlowOutFire detour confirmed ("stays lit" lines, flames kept burning); a Tier 2 burns about
  600 s. On screen "Gas Flame Blue" showed teal, and the light and the orange around the wax stayed
  warm: a Tier 2 has three flames, so only each fire source's own light was painted, never the
  candle's own "Point light.1" (a direct child of the Candle, range 2.5, on by default, beside the
  wax). Now the deep blue has less green (kFlameDeep), and every light of a candle is painted per
  candle with its bluest flame, at the flame's hue. (LightPulse's emission tween is no route to the
  wax: every Firelight LightPulse has a null renderer in the assets. If the wax still shows orange,
  the holders' orange _EmissionColor, "Candle 2 Holder" and "Candle Holder", is next, though neither
  material has the _EMISSION keyword.)

  Still unverified: the colours on screen (the pale stop a light blue, not white; distinct from the
  deep blue; the lantern's deep blue bright enough to see; the room lit as blue as the flame, the
  blue light being darker on purpose; the orange around the wax gone); a Tier 1 candle lasting a
  full test at the faster burn (about 170 s of burn; Candle+0x464 in the report); that renderers
  found under each fire source and candle are the flame meshes; the sanity detour (PlayerSanity+0x60
  holds the candles near the player); the message on channel 5; and the journal row's sync for
  every player. The "flames:" report (flames.cpp report, only while a set-down flame burns) goes
  once this entry is closed.

  2026-10-22: the user found the ghost rarely stayed in one room long enough for a flame to change;
  the 20 s warm-up (which drained while the ghost was away) is removed, so short visits add up. In
  the last three contracts the ghost spent 54%, 74% and 70% of its time in its main room. Then, at
  the user's request: full blue in 60/50/40 s of ghost time by tier, the false flame halfway in the
  same time, and every flame drops back to yellow at half the speed it turns (a false flame is half
  a true one in everything), so a Tier 1 turns fully blue in about 120 / (3p − 1) s of real time.

  Prompt:
  > In this repository, play private contracts in Phasmophobia build 25811160 with Spectral Flame and
  > Custom Ghosts on every client (two clients), a custom difficulty giving 3 evidence, the ghost type
  > forced to a flame ghost (Draugr, Kikimora or Brownie), then a vanilla ghost. With mods.log's
  > "flames:" report: a Tier 1 Firelight set down in the ghost room turns white-hot, pale sky blue
  > after about half a minute of ghost time there and fully deep blue after a minute (Tier 2 50 s,
  > Tier 3 lantern 40 s; longer in real time as the ghost comes and goes, and while it is away the
  > flame drops back at half speed), hunts included, with the light on the walls following;
  > it stays yellow in your hand and starts over when picked up; it does not go out from the ghost
  > before it has been fully blue ("stays lit" lines), and may afterwards; it does not burn out before
  > it is fully blue. With the vanilla ghost it reaches the pale blue only after a minute of ghost time,
  > goes no further, and the ghost may blow it out. A fully blue candle no longer eases sanity drain. The other client sees the
  > same colours. Then a contract with Custom Ghosts off: no row and no tint.

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

## 15. Afterlife has never run inside the game

  mods/afterlife/haunt.cpp:1

  Switching off the dead investigator's camera and rendering from a new Camera at the ghost's head
  (URP may need its additional camera data, and the copied culling mask may not suit a dead player),
  freezing the dead player's movement, and switching off Interact, Primary Use and Secondary Use for
  the lock are unverified, as is whether Crouch reaches a dead player's input and whether the game
  switches those actions back on by itself.

  Prompt:
  > In this repository, die in a private contract with Afterlife running.
  > Check the hint, hold Crouch: the view is the ghost's, turning with it; you cannot move or touch
  > anything. Hold Crouch again: you are back where you were, and for a minute the hint counts down and
  > nothing can be picked up or thrown; afterwards you can. Check mods.log for "afterlife: haunting
  > with the ghost" and fix what is wrong.

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

  mods/custom_ghosts/registry.cpp:1

  GhostInfo.SyncValuesNetworked invokes a UnityEvent and calls GetFullEvidence before returning, and
  the registry's detour sets the behaviour type (GhostType.None) only after it returns. Anything
  listening to that event sees +0x2C still holding 31-35 for that moment, which the game's type
  checks do not know. A review of the custom ghosts found it; it was not traced to a visible fault.

  Prompt:
  > In this repository, find the listeners of the UnityEvent that
  > GhostInfo.SyncValuesNetworked invokes (decompile with re/decomp.mjs) and check whether any reads the
  > behaviour type (+0x2C). If one does, set the behaviour type before calling the original (and keep
  > GetFullEvidence working, which needs the custom entry in the collection), then test a custom ghost.

## 12. The custom ghosts have never run inside the game

  mods/custom_ghosts/registry.cpp:1

  Everything in mods/custom_ghosts is verified against decompiled code and offline tests only: the
  GhostData entries made with il2cpp_object_new (no Unity object behind them), behaviour type
  GhostType.None set in GhostAI.Init and GhostInfo.SyncValuesNetworked, the localisation keys, the
  journal clones (ghost buttons, Ghosts-tab entries and pages, the grid cell height, the journal's
  lists, map, page array and the ghost sync controller's button array), the NavMeshAgent.set_speed
  hook, the Draugr's room survey, the Kikimora's ownership takeover and teleport, and the Echo's
  Vivox tap capture, clip transfer and playback. LevelStats.SendData reports the ghost type to the
  game's analytics as text, so a custom ghost is reported as "31" to "35"; the mod does not touch
  telemetry. The Black Shuck's hunt rests on detours of the hunt state's sense tests by RVA (sight
  0x214BB40, voice 0x214DDA0, electronics 0x21554D0), its set-target 0x2150AB0 handing a hidden
  player to its closet search 0x213B840, and NavMeshAgent.SetDestination; on the game's own
  eligibility test 0x214FD80 and closet setter 0x213FA10; on its writes to the hunt state's target,
  search player, last player and seen set (+0x48, +0x50, +0x58, +0x68); and on holding the hunt's
  kill reach (+0xBC, +0xC0) at 0 while the game's vision sensor does not see its prey. A sink counts
  as running water while DOTween.IsTweening reports its fill tween playing (a tap turned on and off
  again should no longer break scent), and whether baths and showers are Sinks at all is unknown.

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
  > client), Echo repeating a phrase in a teammate's voice from the ghost.

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

## 9. A client that inherits the master role mid-contract thinks it is in the lobby

  mods/playable_ghost/session.cpp:63

  Only the master's start_round sets inRound_, so a client that becomes master during a contract
  has inRound_ false and runs the lobby draw on its first observe. An inherited ghost player is now
  kept (drawnAmong_ is seeded from it), but if the old master was itself the ghost player, the role
  is redrawn mid-contract among the remaining volunteers.

  Prompt:
  > In mods/playable_ghost/session.cpp, a new master after migration has inRound_ == false even when a
  > contract is running (start_round is master-only, called from mods/playable_ghost/round.cpp tick). Make the
  > inherited contract state known to every client (for example from the ASSIGN or LOCK messages or
  > a LevelController check) so draw_ghost never runs mid-contract, and add a session test where the
  > master who is the ghost player leaves during a round.

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
