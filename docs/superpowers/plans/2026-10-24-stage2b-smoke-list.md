# Stage 2b in-game test list

The checks the user runs in the game for stage 2b (plan Task 14 Step 3), gathered from the task reports, the ledger, the plan and FINDINGS. The custom ghosts and Ghost Tells' ghosts, and their journal text, are read from the code and the string tables. The published page of this list keeps each result and note; this file is its source in the repository.

## Before you start

- Read the ban risk again before every session: the game has a startup anti-cheat that has reported accounts for a loader DLL in the game folder. Nothing of the loader goes in the game folder; start the game only through PhasmoMods Loader, and only you launch it.
- Use a local build the updater won't replace: run `.\build.ps1`, then start `dist\PhasmoMods\PhasmoMods Loader.exe` (Setup did not install it, so it never updates). Use **Attach to running game**, or its Steam launch option while testing, and restore that option afterwards. For an installed copy, set `PM_UPDATE_SOURCE` to an existing empty folder in the shell you start the launcher from (a user environment variable for a Steam launch; remove it afterwards).
- Private lobbies only. Single player counts as private.
- Keep `%LOCALAPPDATA%\PhasmoMods\mods.log` (and the game's `Player.log`) from every session; most checks below name the line to look for.
- Force a ghost with Custom 1's Ghost Type option; set the evidence given to 0 for the zero-evidence checks. Two clients are needed wherever a step says host and client.
- Forcing an unseen build: close the game, move `dist\PhasmoMods\builds\<record>.txt` out of that folder (or change one digit of its `gameassembly=` line), start through the launcher. Put it back, or run `.\build.ps1`, to restore. Never edit `sdk\builds`.

## Start-up and the consent notice

Tasks 12e, 12f, 13, 13b, 13c, 13d and 14 (1c).

- Log after start
  - [ ] `game map: ... resolved` with no MISSING key, `game map: 2/2 generic instances resolved on the main thread`.
  - [ ] `menu: available`, `mods panel: available`; no `suite: this game build lacks what the join guard needs`, no `consent: this game build gives no pointer`, no `consent: this game build gives no listener`, no `not in the scene` line.
- [ ] Before answering the notice: `mods.log` has no `mods: loaded` line, and only `PhotonHandler.LateUpdate` is listed under `hooks: created`. Check in Process Explorer (or a debugger's module list) that no mod DLL from `bin` is mapped, only the `host-` copy.
- The notice
  - [ ] It shows as the menu's own popup and asks again on the first run of this build (a player who agreed on v0.1.0 is asked again).
  - [ ] It has the "Mods are programs" paragraph (your words) after the account paragraph, the voice paragraph naming Custom Ghosts, and no achievements paragraph.
  - [ ] The whole notice fits its popup at the game's own text size. If it does not, note how: the fix follows the game's popup (scrolling or size), never a smaller font.
  - [ ] Hovering "I agree" and "Play without mods" lights them exactly as the error popup's OK does; pressing shows the pressed colour; each click gives its answer once. Nothing else happens (no error queue advance, no training).
  - [ ] Press "I agree", drag off, release, press Enter: nothing is answered.
- [ ] "Play without mods": `mods.log` has `no hook of the loader's stays in the game`, the game runs on with no crash at that frame, there is no Mods button and no `mods: loaded` line. It asks again next start. **Try this first.**
- [ ] After "I agree": `mods: loaded` then `mods: <id> started` for each ticked mod, in the order Spectral Flame, Custom Ghosts, Ghost Tells, Playable Ghost; they stop in reverse. The other hooks now appear under `hooks: created`. Every bundled mod starts.
- [ ] Agree, restart: `consent: agreed earlier`, no notice.
- A new game build (forced unseen, see Before you start)
  - [ ] `mods.log` has both `build:` hash lines and `build: the offline check has not seen this game build`.
  - [ ] The notice shows the update paragraph straight after "Your account is at risk.", and still fits, with the voice paragraph.
  - [ ] Agree and restart: no notice. Put the record back: the notice asks once more, with no update paragraph.
  - [ ] Forced unseen and "Play without mods": nothing from a mod in `mods.log`, and it asks again next start.
- [ ] The Mods tape highlights, presses and clicks exactly like News (compare with the Singleplayer tape beside it). It opens the Mods panel and never starts the training. After a scene change back to the menu, and after Apply, the tape reappears and works.
- [ ] The "OK" message popup's button closes it. The error popup copy's own buttons do nothing (no error queue advance).
- [ ] The overlay never appears on a normal start.

## Launcher and the Mods panel

Tasks 12g, 13b, 13c and 14 (string tables).

- [ ] Each bundled mod's row reads "<Name> 1.0.0" and "By PhasmoMods. <description>"; the launcher's ticks show the versions. The launcher window, the Mods panel and every popup show text, not `[key]`.
- [ ] Edit one entry of `i18n/default.json` in `dist\PhasmoMods` and see it change on the next start.
- [ ] Rows' boxes tint like the lobby's checkboxes and tick on click. Apply and Close behave as the tapes.
- [ ] Custom Ghosts needs Spectral Flame: its row says so. Switch Spectral Flame off: Custom Ghosts goes off with it and the status names it. Switch Custom Ghosts on: Spectral Flame comes on too.
- [ ] Remove Spectral Flame's folder and start: Custom Ghosts' row reads "Needs phasmomods.spectral-flame. By PhasmoMods. ...", the rest run, and `mods.log` has `mods: phasmomods.custom-ghosts stays off: ...`.
- [ ] Saved choice: in `dist\PhasmoMods\mods\spectral-flame` move `SpectralFlame.dll` aside and put a small text file under that name. Start and agree: its row reads "Its DLL could not be loaded", Custom Ghosts' reads "Needs Spectral Flame, which did not start". Toggle Ghost Tells, Apply, and check `launcher.ini` still lists both. Restore the DLL and restart: both run.
- [ ] Untick a mod, restart, agree: no `mods: loaded` line for it. Tick it and Apply: it loads then. Untick and Apply: it stops, and stays mapped.
- [ ] Toggle Ghost Tells off and on, then Apply: `mods.log` shows it starting last. Apply with a mod left off and the panel open: its reason goes when it starts.
- [ ] A long reason ("This game build lacks <key> and 3 more") fits the row's description column.
- [ ] A folder with a broken `manifest.json`, and one with a DLL only: the launcher names both, the game starts, the panel shows greyed rows saying why, and the other mods run.
- [ ] A corrupt or wrong-architecture DLL as a mod: a greyed row with a reason, **no Windows dialog**, and the other mods run.
- [ ] A copy of a bundled mod's folder with `"author": "PhasmoMods"` and another id beside a host copy in `bin`: a greyed row says the author is reserved, and the real mods run.
- [ ] With a v0.1.0-era `launcher.ini` (`enabled=ghost-tells,custom-ghosts,spectral-flame`): the same mods start, and `launcher.ini` now holds the `phasmomods.` ids.
- [ ] A saved custom difficulty pick survives the id change: a `custom_difficulty.txt` line `6=custom_ghosts.echo` becomes `6=phasmomods.custom-ghosts.echo`, and Custom 1's pick is still the Echo.
- [ ] In a room the rows are locked (greyed, a click changes nothing) and the panel says to leave first, the host's too.

## Development builds

- [ ] Installed from the release, the development box starts unticked. Ticking it moves to the latest development build and restarts, with the box still ticked.
- [ ] Unticking it moves back to the latest release, a lower version, and restarts, with the box unticked and the mods and the notice's answer kept.
- [ ] With the box ticked and a new development build published, the next start updates to it. With the box unticked, a player sees no development build.
- [ ] The box can't be changed once mods are attached, and works again after the program restarts.
- [ ] A player on a development build joining a host on the release is removed with the list of what the lobby needs, and the reverse too.
- [ ] Started through the Steam launch option, the program updates along the channel the box chose.

## Lobbies and players

Tasks 13, 13b, 13c and 13d, with the closed-room start.

- [ ] Two players, same mods, private lobby: both stay, `mods.log` has `room's mod lists match` and each sees the other's full list; a contract plays as before.
- The closed-room start
  - [ ] Host presses Start with a verified friend: `mods.log` shows `the room is closed for a contract's start`, then `the server confirmed the room closed`, and the contract starts within a second.
  - [ ] A friend joins in the moment Start is pressed: the room reopens and "The contract is waiting" names them, or Photon refuses their join; the contract never holds them unverified. Note what the joiner sees.
  - [ ] Host leaves the lobby during the close: the others can still start a contract or leave, with no error. A new player can't join until the next contract's return (known, and rare).
  - [ ] Double-click Start with a friend in the lobby: one contract starts, with no popup. In a public lobby (mods off), Start works as in the unmodded game.
  - [ ] A friend in the lobby during a start sees and hears nothing from the closed room's marker, and their `Player.log` is clean.
  - [ ] If the marker never returns, every start reopens after 5 s with the not-confirmed popup (its text is `PM_TEXT_TODO` until you write it). This would show on the first start.
- [ ] Press Start just as someone joins: "The contract is waiting" names them, nobody is kicked for it, and once they're verified and all ready again the contract starts fully modded. `mods.log` has `a contract's start waits`.
- [ ] A vanilla friend joins a modded lobby: the host's tells and custom ghosts stay on for everyone else, the difficulty menu still offers the custom ghosts on the host, and the friend is kicked within 10 s and sees the game's own kick.
- [ ] With a vanilla friend who pressed Ready: Start shows the popup, and within 10 s the friend is kicked and the host can start.
- [ ] A joiner lacking a host mod, and one with another build of one: it leaves, and the game's popup lists every required mod with versions, the missing ones highlighted. Force it to stay (or use an old build) and the host's kick removes it.
- [ ] A joiner with one extra mod: within a moment the extra's name appears in the game's popup **on the lobby's paper**, OK closes it, nobody is kicked, the extra's effects are gone, and it is back after leaving. Walk away from the board and back: the popup returns if not yet clicked.
- [ ] A joiner with an extra mod that another of its extras requires (needs a test mod pair): both go off, the notice names both as extras, both come back on leaving.
- [ ] A modded joiner under a vanilla host: the vanilla notice after 10 s, nothing custom runs, and mods return on leaving.
- [ ] Host a public lobby: the public notice for the host and a modded joiner, nobody removed, mods off through a contract and the return to the lobby, and on after leaving. Then single player: mods on, no notice.
- [ ] Host a private lobby, then leave so that a player who last created a public room becomes master, and return from a contract: the room turns public and every mod goes off with the notice.
- [ ] Master leaves, in the lobby and in a contract: the next host's list takes over, nothing starts or stops mid-contract, and a client that differs follows the joiner rule back in the lobby.
- [ ] Back in the lobby after a contract, a joiner with an extra mod is still handled (`mods.log` shows the guard acting after each return).
- [ ] Start a contract within a second or two of a third player joining: `mods.log` on each client ends on `match` early in the contract, and the ghost player's HUD updates.
- [ ] During a joiner's grace, as host: turn to a custom page in the lobby journal and mark a custom ghost's guess. They work on the host, with no exception in the vanilla friend's `Player.log`. After a modded friend joins later, your custom page doesn't throw on theirs.
- [ ] Apply a custom difficulty during the grace: the host still shows the pick, a modded friend gets it once verified, and the vanilla friend's `Player.log` is clean.
- [ ] A full contract with two modded players, custom ghost included, then back to the lobby and a second contract.
- [ ] If a skipped kick can be arranged: three `asked the game to kick` lines, then the popup on the host.
- [ ] A v0.1.0 peer in a lobby: no mod of this build reacts to its messages.
- [ ] A public room: no mod sends anything. A non-host client: no difficulty sync.
- [ ] Suspend and resume in a lobby (a vanilla host, then leave): `mods.log` shows the same start order after resuming.
- [ ] Steam achievements are the game's own: with mods on, an easy one (the training achievement) unlocks.

## The SDK's platform (stage 2a's untested parts)

Watch `mods.log` through the sessions above; each item is a line to find, or to never find.

- [ ] Scene facts (`scene_engine`): the journal's book and the PlayerView layer resolve; no `game map: the scene facts could not be resolved` and no `scene facts: exception` line, in the menu, a lobby and a contract.
- [ ] The steering site (`patch_site`): the ghost steers in a hunt and a wander. Restart the mods from the Mods panel (out of a room), then steering still works: a new page, and the site restored by the host (`hooks: restored the N bytes at ...` at the stop). At game exit no `stays writable` line.
- [ ] A static field whose class constructor throws (`static_access`): if a `suite: static field <key>: ...; it answers false` line ever appears, the game carries on with no crash. Note the key.
- [ ] Wrong-kind service calls: if a `suite: a mod passed <service> an object that is not <kind>` line appears, nothing breaks. Note the service.
- [ ] The Enum.ToString hook (hot), the write hook during EasySave's cache save, the send right after ApplyDifficulty, and the one-shot board handler after a contract: no exception or hitch.

## Content in the journal and the difficulty

Tasks 5, 6, 7, 9b and FINDINGS 26.

- [ ] A journal's Ghosts tab shows all 45 ghosts in three columns with no overlap; the 30 vanilla entries stay where they were, and each name fits its row.
- [ ] A journal's Home tab lists all 45 ghosts alphabetically over its two pages, the numbers counting up with no gap; turning to a number shows that ghost's page, including a vanilla ghost's from the Ghosts tab's list, no row runs off its page, and the text is readable.
- [ ] Two players in a modded lobby: one turns to a ghost's page, and the other, looking at their book, sees the same ghost's page.
- [ ] Clicks open each custom ghost's page; a guess on a custom ghost's button syncs between players.
- [ ] Each custom ghost's page is laid out as the game's own: description, then strength and weakness under the game's labels, then the ability.
- [ ] The Spectral Flame row appears after the game's rows while a ghost that gives it is shown; its name and line read right; it is marked, synced and filters the ghost buttons as the game's rows do; the rows' spacing fits.
- [ ] A non-English journal: switch the game's language; vanilla text follows it, custom pages fall back to English, and the labels are in the chosen language.
- The lobby journal
  - [ ] Custom ghost pages, list entries, evidence rows and lines (Ghost Tells' too) show in the lobby before a contract and after one.
  - [ ] A player with other mods joins the lobby: every journal is put back as the game made it, the Ghosts tab's entries where they were, until they leave; the local reader's book turns to its Ghosts tab if it was on a page that went.
- The custom difficulty
  - [ ] In Custom 1, step through the Ghost Type option: all 45 ghosts in alphabetical order (Asterion first, Black Shuck after Banshee, Yurei last).
  - [ ] Pick a custom ghost: the summary's first line reads "Ghost: <its name>", not a number.
  - [ ] Pick a vanilla ghost, have a player without the mod join and leave: the option keeps naming it.
  - [ ] Pick the custom ghost again, apply, start a contract: that ghost haunts it, and `custom_difficulty.txt` holds `6=<ghost id>`. The other client's option names it too, and so do both after the contract.
  - [ ] Restart the game: Custom 1 shows the pick again. Restart once more and start a contract without opening the difficulty board: the pick haunts it.
  - [ ] A player without the mod joins: the option falls back to Random on every client, and back to the pick when they leave. Switch Custom Ghosts off: the difficulty reads Random.
  - [ ] Hovering the reward multiplier in the moment before the custom ghosts are offered may log a NullReferenceException (known, FINDINGS 26); note it if it does.
  - [ ] Read the save file's Difficulty entries (EasySave3) for any ghost type above 30: there must be none.
- [ ] The Mimic copying a custom ghost: its rules start on every client while it acts as the copy and stop at the switch; the Mimic keeps its own evidence and flame look.

## Spectral Flame

Task 8. `docs/spectral-flame.md` describes every look.

- [ ] A lit Firelight set down, and a candelabra's candles, in the ghost's room: the colour builds late (white-hot, then pale blue, then deep blue for a ghost that gives it), and fades evenly when the ghost is away. A flame in a hand never changes.
- [ ] The false positive: any ghost stops at the pale blue.
- [ ] The Hydra's green, from the content layer: pale green with the evidence withheld (zero evidence too), deep green when given.
- [ ] Sanity: a flame more than half blue no longer eases the drain (the CandleIsOn hook on the game's cached delegate).
- [ ] Blow-outs: a ghost that gives it never blows out a set-down flame before it has been fully blue.
- [ ] The flames stay on through a room lapse mid-contract.

## Custom ghosts

One section for each custom ghost, in the order Custom Ghosts registers them (`mods/custom_ghosts/ghosts.h`). For each: set it up with Custom 1's Ghost Type option, once with evidence and once on zero evidence, in single player. The journal text is quoted from the table; read it for accuracy and tone. The player-controlled checks need Playable Ghost and a second client.

### Echo

**Evidence:** Spirit Box, Freezing Temperatures, D.O.T.S.

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Echo; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Echo can be told from every other ghost by its tells below.

**Tells**

- [ ] Within a minute of you using a light switch or a door in its favourite room, its next interaction once its own sensor detects that switch or door uses it (only the latest counts), dormant or not.
- [ ] It calls out in the voice of a player it has heard, from wherever it is, a shade lower: a phrase of 0.5 to 3 s, played where the ghost was, on every client.
- [ ] With nobody heard near it for a minute it falls quiet: no speech, and 3 in 4 of its interactions and events do not happen, though a repeat always does.
- [ ] A voice whose audio stopped (a player who died or left) no longer wakes it. Recordings go when the contract ends.
- [ ] Which tap carries positional voice: check the voice is heard where the ghost was.

**Told apart**

- [ ] Its voice needs another player, since it hears only other players' voices; in single player it is dormant all along, so check there that the repeats still show and most other actions are lost, on zero evidence.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Echo  
> **Description:** The nymph of Greek myth, cursed to repeat only what others say. A lonely mimic, it stays quiet unless someone speaks near it, then answers in their voice.  
> **Strength:** An Echo can call out in the voice of anyone it has heard, and it repeats what players do in its room: the switch or door you just used is the next thing it touches.  
> **Weakness:** With nobody speaking near it for a while, an Echo falls quiet and rarely acts.  
> **Ability:** Speak: Call out with the last words a player said, in their own voice.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Ability "Speak": used on demand at the vanilla rate of the ability roll (allowed difference 2); no Speak while dormant.
- [ ] HUD: "Voices" while it hears the living near it, the faded "Silence" line while dormant.
- [ ] Its interaction is a repeat of the switch or door a player just used in its room, once in reach, whatever the aim hits; the repeat's charge cost.
- [ ] Dormant: a lost interaction stays paid for; a repeat while dormant still happens. A restless Echo's own roll repeats too.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Dullahan

**Evidence:** EMF 5, Ghost Orbs, Ghost Writing

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Dullahan; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Dullahan can be told from every other ghost by its tells below.

**Tells**

- [ ] In a hunt, very fast on long straight runs (3.4 m/s before the shared multipliers).
- [ ] Turns slowly (45 degrees a second) and loses most of its speed at every corner (0.43 m/s facing away from its path).

**Told apart**

- [ ] Needs a hunt with a straight run and a corner. Check on a small map too.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Dullahan  
> **Description:** The headless rider of Irish legend, which charges down whatever lies straight ahead. Its mount cannot turn, so in the house it is fast on a long run and all but stops at a corner.  
> **Strength:** A Dullahan is very fast on long, straight runs.  
> **Weakness:** A Dullahan turns slowly and loses most of its speed at every corner.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] HUD: "Charge: N% speed".
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Kikimora

**Evidence:** Ultraviolet, Ghost Writing, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Kikimora; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Kikimora can be told from every other ghost by its tells below.

**Tells**

- [ ] Carries off loose equipment near it and hides it in another dark, empty room, leaving EMF where it was (the takeover and move).
- [ ] Never touches anything in a room with the lights on.

**Told apart**

- [ ] Needs equipment left loose and a dark room. Check with all lights on in its room: nothing moves.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Kikimora  
> **Description:** The Slavic household spirit behind the stove, fussy about other people's mess. It tidies away whatever the living leave lying around in the dark, somewhere they will not think to look.  
> **Strength:** A Kikimora carries off equipment left on the floor and hides it in another room.  
> **Weakness:** A Kikimora will not touch anything in a room with the lights on.  
> **Ability:** Tidy away: Carry off the nearest equipment lying in a dark room and hide it in another dark, empty room.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Ability "Tidy away": on demand (allowed difference 2).
- [ ] HUD: the faded "The lights are on here" line in a lit room, where it can touch nothing.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Draugr

**Evidence:** EMF 5, Spectral Flame, Freezing Temperatures. A difficulty that withholds evidence always leaves Freezing (as the Goryo's D.O.T.S.); on zero evidence none is given.

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Draugr; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Draugr can be told from every other ghost by its tells below.

**Tells**

- [ ] Its things are the house's own props in its room when its rules started (never equipment). Carry one out: it is angered.
- [ ] Angered, it may hunt from 65% sanity (the idle state's insanity bonus), and its hunts go only for the living player who last carried one out, until they are in the truck, dead or killed; then the game's own hunt.
- [ ] Angered, outside hunts, it takes its things back one at a time to the very place each lay, every 25 to 45 s, never one a player carries.
- [ ] Angered, its room is 3 °C colder on the host and on a client (thermometer and breath); with Freezing withheld its readings may still go below freezing (a tell). The room's own temperature comes back when the anger ends.
- [ ] It keeps its room: no long roam that changes rooms (a Monkey Paw wish can still move it; its things are then the new room's, anger and thieves forgotten).
- [ ] In a hunt: 2.8 m/s in its room, slowing 0.15 m/s per metre away, to 0.3 m/s from about 17 m.

**Told apart**

- [ ] Needs a player to carry a prop out of its room. Check on zero evidence in single player, and the cold with Freezing withheld.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Draugr  
> **Description:** The restless dead of the Norse barrows, jealous guardians of their resting place and all that lies in it. It keeps to its room, and whatever is taken from there, it wants back from the thief alone.  
> **Strength:** Carry anything out of a Draugr's room and it may hunt while your sanity is still high, going only for the thief until they reach the truck or die.  
> **Weakness:** A Draugr takes back what was taken, and its room turns colder until it does. In a hunt it ignores all but the thief and slows far from its room.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] HUD: "Something has left your room" (angered) or "Your room is whole", on a non-host controlling client too.
- [ ] Its hunts are held to the same one target; the target's HUD after a kill mid-hunt.
- [ ] Steered and standing on its target with another player nearer and in sight: it kills the target. With the target in the truck: Listen goes for anyone, and sight still does not.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Black Shuck

**Evidence:** Ultraviolet, Freezing Temperatures, D.O.T.S.

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Black Shuck; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Black Shuck can be told from every other ghost by its tells below.

**Tells**

- [ ] Blind and deaf in a hunt: it never sees players, hears voices or senses equipment.
- [ ] It picks up a scent trail within 8 m (where a player walked in the last 30 s) and follows it step for step to them; the game's own kill takes them.
- [ ] Closets do not hide you: a trail ending at a closet or locker leads it there, and the game's closet search finds you.
- [ ] Salt or running water breaks a trail (a running sink; baths and showers only where the game makes them sinks), and rain outdoors lays none. Smudges stun it.
- [ ] No hitch at a doorway with salt on the way round a wall, or with a running sink; it comes free when scent gives out.

**Told apart**

- [ ] Needs a hunt and a trail. **Rain:** outdoors it lays no trail. Check indoors on zero evidence in every weather, and with salt in a doorway.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Black Shuck  
> **Description:** The phantom hound of the East Anglian roads, blind but never lost. It cannot see or hear you; it follows your scent, step for step, until salt, running water or rain breaks it.  
> **Strength:** In a hunt a Black Shuck follows the exact path its prey walked, round every corner and into hiding places, though it cannot see or hear.  
> **Weakness:** Salt, running water or rain breaks a Black Shuck's scent, and it loses anyone on the other side.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Blind in hunts (the darkened hunt view): its player sees only the scent view, including on a floor below a trail and after losing a prey.
- [ ] Prey and kill; a kill through a thin wall held off; closet search at a trail's end; the switch when walking onto another's trail; a lost trail ignored for 30 s.
- [ ] The corridor (allowed difference 4): steering stays within 1 m of the trails its AI may follow; salt, running water and rain leave gaps; free steering by a running sink. Ghost and player heights agree within 1 m.
- [ ] Frame time in a hunt with scent in reach (unmeasured): note any hitch.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Lithobolia

**Evidence:** Spirit Box, Freezing Temperatures, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Lithobolia; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Lithobolia can be told from every other ghost by its tells below.

**Tells**

- [ ] Throws objects only at a player its vision sensor detects within 8 m; a hit costs 5% sanity on the hit player's client.
- [ ] With nobody in sight it throws nothing at all.
- [ ] In a hunt what it throws flies at its target if it sees them within 8 m, else the throw is swallowed.

**Told apart**

- [ ] Check in single player on zero evidence: no throw with nobody in sight, throws at you in sight.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Lithobolia  
> **Description:** Named for the stone-throwing devil that pelted a New Hampshire house in 1682. A spiteful thrower that wants a target, it throws only at players it can see, never at an empty room.  
> **Strength:** A Lithobolia throws only at players it can see, and a hit costs more than a fright.  
> **Weakness:** With nobody in sight a Lithobolia throws nothing at all.  
> **Ability:** Throw: Throw the nearest loose object at a player in sight.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Ability "Throw": the nearest loose object at a player in sight, aimed and paid for.
- [ ] Its hunt throw flies at someone it sees, as its AI's does.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Gremlin

**Evidence:** EMF 5, D.O.T.S., Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Gremlin; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Gremlin can be told from every other ghost by its tells below.

**Tells**

- [ ] Half of its interactions with running electronics in its room switch one off, for every client (EMF where it stands).
- [ ] Never touches equipment within 2 m of a living player.

**Told apart**

- [ ] Needs running electronics in its room, with nobody within 2 m.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Gremlin  
> **Description:** The saboteur airmen blamed for every failing engine in the Second World War. It cannot leave a running machine alone, unless someone is standing by it.  
> **Strength:** A Gremlin switches off equipment that has been set down and left running.  
> **Weakness:** A Gremlin will not touch equipment someone is holding or standing beside.  
> **Ability:** Short out: Switch off the nearest placed equipment that nobody is holding or standing beside.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Ability "Short out": on demand (allowed difference 2); never equipment held or with someone beside it.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Brownie

**Evidence:** EMF 5, Ghost Writing, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Brownie; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Brownie can be told from every other ghost by its tells below.

**Tells**

- [ ] Unwatched in a hunt: 3.83 m/s, and its steps make no sound: players hear nothing, and the sound sensor and parabolic microphone never detect them (no reading, no radar point, no playback, no sound-sensor challenge progress), while its UV prints still appear.
- [ ] Watched in a hunt: 1.02 m/s, no speed-up on sight, heard only up close (12 m instead of 20 m) and detected as usual.
- [ ] Seen on the truck's monitor in a hunt, whichever grid page or full-screen camera the viewer has open (any switched-on camera of the kind on show counts): with a player at the monitor and a switched-on camera of the kind it shows pointing at the Brownie, it slows to 1.02 m/s and its steps are heard up close, as if a player looked at it. With nobody at the monitor, that camera does not slow it.
- [ ] Outside hunts its steps are the game's own, and it never interacts or causes an event while a player or a running video camera can see it.
- [ ] A non-Brownie ghost's steps after a Brownie contract, and after switching the mod off mid-hunt, are detected again.

**Told apart**

- [ ] Check on the host and on a client, AI and player-controlled, with a sound sensor and a parabolic microphone.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Brownie  
> **Description:** The Scottish house spirit that works only while nobody is watching. Unseen it is quick and silent in a hunt; watched, it slows to a walk, and outside a hunt it does nothing in view.  
> **Strength:** A Brownie is very fast in a hunt while nobody looks at it, and then its footsteps make no sound, even to equipment.  
> **Weakness:** A Brownie slows to less than a walk while anyone looks at it, and then its footsteps can be heard, if only up close.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same silence and speeds; it touches nothing while watched.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Bogeyman

**Evidence:** Ultraviolet, Ghost Orbs, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Bogeyman; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Bogeyman can be told from every other ghost by its tells below.

**Tells**

- [ ] Each hunt begins from the closet or locker nearest the youngest living player (lowest prestige, then level; ties to who joined first), not one they hide in, within 10 m on their floor: the game's teleport moves it there and the door opens as it steps out. It still stands by that closet when the hunt-start delay ends, and the youngest player has that delay to react.
- [ ] Its hunts go only for that player: while they are in the truck it goes for anyone it hears and its sight stays on them; when they die it takes the next youngest. With no target it searches closets and hiding spots, nearest first (8 s at most on one it cannot reach).
- [ ] Outside hunts, 3 in 4 of its door interactions are on a closet door its door sensor detects (with prints).
- [ ] Slower in open rooms, where it loses prey beyond 7 m.

**Told apart**

- [ ] **Needs a closet or locker** within 10 m of the youngest player on their floor. Check on a map without one near; **a second player** shows the youngest rule.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Bogeyman  
> **Description:** The thing in the closet, which knows every place a frightened child hides. It lurks there, hunts only the youngest player, and starts each hunt from the closet nearest them.  
> **Strength:** A Bogeyman's hunts begin from the closet or locker nearest the youngest player, and they go only for that player until they reach the truck or die.  
> **Weakness:** A Bogeyman is slow in open rooms and loses sight of its prey there sooner, and while it hunts the youngest player it ignores everyone else.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] Its hunt warps to the closet, with the door opening; held to the same one target; an AI Bogeyman ignores a nearer seen player with the target in the house, then the truck.
- [ ] With no target, a hiding place it reaches is searched (closets through the game's own search).
- [ ] An aimed door turns into a closet door 3 times in 4.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Lethe

**Evidence:** Ghost Orbs, Freezing Temperatures, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Lethe; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Lethe can be told from every other ghost by its tells below.

**Tells**

- [ ] Photos, videos and recordings of its activity fade from the journal after three minutes unless someone looks at them first.
- [ ] In a hunt it forgets prey out of its sight for 1 s, and no longer wanders where it lost them (`lethe: it forgets its prey`).
- [ ] Its appearance replays through the hooked Appear (Task 11b).

**Told apart**

- [ ] Needs a photo or recording of its activity and three minutes.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Lethe  
> **Description:** Named for the river of forgetting in the underworld. It has forgotten who it was and makes the living forget too: photos fade, and in a hunt it loses you a second after you leave its sight.  
> **Strength:** Photos, videos and recordings of a Lethe fade from the journal a few minutes after they are taken, unless someone looks at them first.  
> **Weakness:** A Lethe forgets its prey: once it loses sight of someone in a hunt, it gives up the chase within a second.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same forgetting in its hunts.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Mesmer

**Evidence:** Spirit Box, D.O.T.S., Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Mesmer; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Mesmer can be told from every other ghost by its tells below.

**Tells**

- [ ] Each player sees its appearances as another form, elsewhere, and its hunts as another form where it truly is.
- [ ] A player sees the same form in every appearance and hunt of a contract, and another contract usually deals another form.
- [ ] Two or more players each see a different form, and find it by telling each other what they saw.
- [ ] When it walks during an appearance, each player's form walks too, at their own distance from the true ghost, animating as it goes. It faces where it walks, and turns to the player when it stops. It doesn't pass through walls or sink through floors.
- [ ] The truck monitor shows the team a fifth saner.
- [ ] The lights in its room dim slowly instead of flickering.
- [ ] Video cameras, photos and the D.O.T.S. projector show it as it is.
- [ ] Its one-shot sound plays.

**Told apart**

- [ ] Check the camera and D.O.T.S. view against the eye, on zero evidence in single player.
- [ ] On zero evidence in single player with no camera: the favourite room's lights slowly dim and brighten instead of flickering, and the truck monitor reads higher than the wristwatch's sanity bar.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Mesmer  
> **Description:** Named for the showman whose audiences saw whatever he wished. An illusionist with a different face for everyone, no two players agree on what a Mesmer showed them.  
> **Strength:** Each player sees a Mesmer differently, and the truck's sanity monitor shows the team saner than they are.  
> **Weakness:** A Mesmer cannot fool a camera: video, photos and the D.O.T.S. projector show it as it is.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same forms for the investigators while a player controls it.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Knell

**Evidence:** Spirit Box, Ghost Orbs, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Knell; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Knell can be told from every other ghost by its tells below.

**Tells**

- [ ] In a hunt it surges to 1.75 times the base speed for one second in every two, and crawls at 0.35 times the base in between.
- [ ] The game's own footsteps quicken in every surge and slow in every crawl.

**Told apart**

- [ ] Needs a hunt. Every map, every weather.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Knell  
> **Description:** The slow toll of a funeral bell, on a beat that never changes. Relentless in a hunt, it surges and crawls on a two-second beat you can hear in its steps.  
> **Strength:** In a hunt a Knell surges to great speed on every other beat.  
> **Weakness:** Between surges a Knell crawls at about a third of the base speed; run on its slow beat.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same beat while a player steers it.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Asterion

**Evidence:** Ghost Writing, D.O.T.S., Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Asterion; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Asterion can be told from every other ghost by its tells below.

**Tells**

- [ ] A closed door never stops it for good: it batters it down in 3 s, heard far away, and the door stays broken open.
- [ ] It never closes a door; no door closes behind it.
- [ ] UV prints on a battered door only where the ghost gives Ultraviolet.

**Told apart**

- [ ] **Needs a closed door** in its way in a hunt.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Asterion  
> **Description:** The Minotaur of the labyrinth, by its own name. A battering brute that makes a maze of any house, doors only slow it, and it never closes one.  
> **Strength:** A closed door never stops an Asterion for good: it batters it down, and the door stays broken open.  
> **Weakness:** Battering a door down takes an Asterion a few seconds, and it can be heard far away.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The player Asterion's door battering while Playable Ghost steers it.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Tempest

**Evidence:** Spirit Box, Ultraviolet, Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Tempest; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Tempest can be told from every other ghost by its tells below.

**Tells**

- [ ] In a hunt it cannot be seen except for 1.5 s after each lightning strike, when it shows in full and runs at 3.83 m/s.
- [ ] Every hunt opens with a strike; more follow every 3 to 6 s in heavy rain (the game's storm lightning counts too), 6 to 10 s in rain, 9 to 14 s in any other weather.
- [ ] In the dark between strikes it crawls at 1.02 m/s and never speeds up on sight of its prey.
- [ ] It never changes the weather; outside hunts, lightning flashes as each of its events begins.

**Told apart**

- [ ] Needs a hunt. **Check in each weather**: the strikes' rhythm changes with the rain.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Tempest  
> **Description:** A storm spirit of the old weather lore. It moves with the lightning, and the wilder the night, the more often it strikes.  
> **Strength:** In a hunt a Tempest cannot be seen, except when lightning strikes; then it is very fast.  
> **Weakness:** Between strikes a Tempest is slow, and the clearer the night, the rarer its lightning.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same visibility and speeds. In heavy rain, the storm's lightning on the ghost player's client (FINDINGS 29).
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

### Hydra

**Evidence:** Ghost Orbs, D.O.T.S., Spectral Flame

**Set-up**

- [ ] With evidence: Custom 1, Ghost Type Hydra; the journal's evidence and its guess button work.
- [ ] On zero evidence, in single player: Hydra can be told from every other ghost by its tells below.

**Tells**

- [ ] Each smudge in a hunt grows a false head (at most three) that walks and flickers as it does and throws objects as the hunting ghost does; no object is thrown twice in one tick.
- [ ] The false heads are harmless and silent: they never kill and make no footsteps.
- [ ] Its Spectral Flame burns green: pale green at the false-positive limit even when withheld, deep green when given.

**Told apart**

- [ ] The green flame names it on zero evidence. The heads need a smudge in a hunt.
- [ ] On every map and in every weather, apart from any condition named above.

**Journal text** (from the table):

> **Name:** Hydra  
> **Description:** The many-headed serpent of Lerna: cut off one head and two grow back. Its flame burns green as marsh fire, and every smudge grows it another head.  
> **Strength:** Smudge a Hydra in a hunt and it splits into apparitions, only one of them real; smudge it again and there are more.  
> **Weakness:** A Hydra's false heads are harmless and make no sound as they walk.

**Journal**

- [ ] The journal text reads right: accurate to the tells as built, and in your tone.

**Player-controlled**

- [ ] The same heads while a player controls it.
- [ ] Its AI and its player follow the same rules: targets, kill limits, speeds, held-back rules and tells, apart from the allowed differences named here.

## Ghost Tells

One section for each vanilla ghost Ghost Tells changes (`mods/ghost_tells/tells.h`). Force each with Custom 1, on two clients in a private lobby, and check `mods.log` for each ghost's lines (`spirit:`, `mare:` and so on). The line its journal page gains is quoted from the table.

### Spirit

**Set-up**

- [ ] Custom 1, Ghost Type Spirit, two clients: `mods.log` has `spirit:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Spirit shows them too.

**Tells**

- [ ] Incense in a hunt stops it where it stands for 3 s, silent (footsteps and walk stop too).
- [ ] Incense outside a hunt silences it for a minute: no interactions, events or ability.
- [ ] With voice recognition on, say its first name within 10 m: a knock on a door 1 to 3 s later, not again within 20 s.

**Told apart**

- [ ] Needs incense; the knock needs voice recognition.

**Line its journal page gains** (from the table):

> **Line:** Tells: incense stops it dead in a hunt and silences it for a minute; it knocks when you say its name.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] While a tell holds it back, the player sees interactions, events, hunts and the fuse box refused, without losing the charge.

### Mare

**Set-up**

- [ ] Custom 1, Ghost Type Mare, two clients: `mods.log` has `mare:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Mare shows them too.

**Tells**

- [ ] It never shows itself in a lit room: it switches the room's lights off first, or stays unseen when it cannot.
- [ ] The rooms it hunted through are dark when the hunt ends.
- [ ] It switches off a light just turned on near it 1 time in 2 (vanilla 1 in 7): switch one on near it again and again.

**Told apart**

- [ ] Needs a light switch.

**Line its journal page gains** (from the table):

> **Line:** Tells: it puts the lights out before it shows itself, leaves the rooms it hunted through dark, and often switches off a light you have just turned on.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] It cannot show itself in a lit room.

### Shade

**Set-up**

- [ ] Custom 1, Ghost Type Shade, two clients: `mods.log` has `shade:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Shade shows them too.

**Tells**

- [ ] Stared at for 0.4 s while it shows itself, it vanishes (within half a second).
- [ ] With two players within 5 m it does nothing at all.
- [ ] In a hunt it stops for 1.5 s when its prey has another player beside them.

**Told apart**

- [ ] **Needs a second player** for two of its tells; the stare works alone.

**Line its journal page gains** (from the table):

> **Line:** Tells: it vanishes the moment you stare at it, does nothing with two of you nearby, and hesitates when it sees you together.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] Held back with two players near: interactions, events and hunts refused without losing the charge.

### Yurei

**Set-up**

- [ ] Custom 1, Ghost Type Yurei, two clients: `mods.log` has `yurei:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Yurei shows them too.

**Tells**

- [ ] When incense traps it, it shuts the doors of its room and shuts any opened one again 3 s later, for the 90 s trap.
- [ ] The door its ability slams is locked for 3 s with the hunt's lock (try it).
- [ ] A door left open in its room for 20 s draws its ability to that door (within about a minute).
- [ ] Tarot cards still pass the gate.

**Told apart**

- [ ] **Needs a door** in its room, and incense.

**Line its journal page gains** (from the table):

> **Line:** Tells: smudged, it shuts itself in; the door its ability slams stays locked for a moment; a door left open in its room draws its ability.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] The same doors and locks.

### Goryo

**Set-up**

- [ ] Custom 1, Ghost Type Goryo, two clients: `mods.log` has `goryo:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Goryo shows them too.

**Tells**

- [ ] In its room with nobody within 10 m and a running video camera on it, it stands visible to cameras alone for 4 s now and then: on the camera's feed and the truck monitor, never to the eye or in a photo.
- [ ] In those conditions it makes an extra interaction every 20 s.
- [ ] In a hunt, having lost everyone more than 12 m from its room, it walks home; it chases anyone it detects, anywhere. It has no leash.

**Told apart**

- [ ] Needs a video camera in its room.

**Line its journal page gains** (from the table):

> **Line:** Tells: a video camera can catch it standing in its room while nobody is near, where it also acts more; it heads home when it loses you far from there.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] With no prey a player may steer it anywhere (allowed difference 4).

### Onryo

**Set-up**

- [ ] Custom 1, Ghost Type Onryo, two clients: `mods.log` has `onryo:` lines and the tells run on both.
- [ ] The tells run on every contract, with evidence and on zero evidence; a Mimic copying the Onryo shows them too.

**Tells**

- [ ] A flame lit within 4 m is blown out 1 time in 2, counting toward its third-flame hunt.
- [ ] Every flame gutters as its count rises and burns low at two.
- [ ] A flame it blew out will not catch for 15 s, on every client.

**Told apart**

- [ ] Needs a flame and a lighter.

**Line its journal page gains** (from the table):

> **Line:** Tells: it snuffs a flame lit near it, every flame gutters as its count of snuffed flames rises, and a flame it snuffed will not relight for a while.

**Journal**

- [ ] The line reads right and fits the page, in the lobby journal too.

**Player-controlled**

- [ ] The same blow-outs.

## Playable Ghost

Tasks 12a to 12d and 12c's rounds. The ghost player is one of two or more clients.

- The parked body stays the ghost player's local player (0.3.0)
  - [ ] An investigator opens the main door with the ghost player as host, and again as a client: no crash, and no `StateMachine ChangeState Error` in the ghost player's game log.
  - [ ] The ghost appears in an event and hunts: no crash, and its sounds play for the ghost player and the investigators.
  - [ ] The ghost player's game log after a contract: `FootstepController.AttemptPlaySound`, `EVPRecorder.Update` and `VoiceVolume.Update` no longer repeat, and no other `at <Class>.Update ()` line repeats in their place.
  - [ ] The ghost player hears investigators' footsteps and the house's ambience as they would standing where the ghost is, not from the truck.
  - [ ] The ghost player sees the whole house from inside it, with its lights, with no part of it switched off because the parked body is in the truck.
  - [ ] No sanity effect (camera or sound) reaches the ghost player's view as the parked body sits in the truck.
  - [ ] In heavy rain the ghost player sees and hears the game's lightning, with no error at each strike.
  - [ ] The truck's exit, a leave and a quit all work for the ghost player, and the truck doesn't leave or count the parked body by itself.
  - [ ] Team sanity counts investigators only: the ghost player's HUD average matches the investigators' readings on the truck's monitor, which has no row for the ghost player, and a drained team reaches the hunt threshold though the parked body's sanity stays full.
  - [ ] A Lethe with a ghost player: the ghost player opening their own journal on the media pages keeps nothing from fading; an investigator looking at them does.
  - [ ] A Mesmer with a ghost player: its false appearance turns to face the ghost player on their own screen, and each investigator on theirs.

- The lobby
  - [ ] The "Play as ghost" tape sits on the player's own ID card under the XP bar, in Ready's clickable colour, follows the player to another card, and clicking it volunteers without touching Ready.
  - [ ] Lobby equipment: the ghost player's loadout leaves the pool and comes back when they stop being the ghost.
- The pick
  - [ ] The pick screen shows the types the weights allow; the truck stays shut (its countdown is a mod-drawn window, `PM_UI_TODO`). The chosen type and room are the ghost's. With no pick in time, the native rolls stand. A fixed-type difficulty shows its type fixed.
  - [ ] The panorama is lit, and salt and crucifixes stay hidden for the ghost player.
  - [ ] Map view: the floors change, the room outlines show, a click picks a room, the basement is named.
  - [ ] No error and no stuck ghost after `abandon_ghost_spawn`.
  - [ ] The Mimic: the player's pick becomes the next copy at the game's own switch; copying between custom ghosts mid-hunt, and a Bogeyman at the hunt's own change.
- Stepping out
  - [ ] The camera leaves the rig only once everyone has loaded; the body is parked, unseen and not solid, on every client; a Monkey Paw revive skips it; leaving gives the rig back.
  - [ ] The investigator form: the baked body appears and follows the ghost.
  - [ ] Monitor: the truck's sanity monitor shows the investigators only, on every client, from the moment everyone has loaded (`outlist: the ghost player's spot is out of the player list` once per client).
- Controls and senses
  - [ ] Controls follow the player's bindings and rebinds (Move, Interact, Primary/Secondary Use, Crouch, Pause, Journal, Torch); mouse look; the cursor locks while steering and frees in the mod's menus; the aim label names Door, Light switch, Fuse box, Object.
  - [ ] Aimed door, light and prop interactions at their cost; an aimed interaction narrows to the aimed object; the Twins' follow-up after an aimed one.
  - [ ] The hunt offer; the Tower card's boost; a Yokai's far voices (allowed difference 1); the Yokai with a Monkey Paw wisher.
  - [ ] The microphone stays muted, the dead stay silent to the ghost, walkie-talkie taps are muted, and the microphone works again after the contract. The ghost player talks, and no investigator hears them.
  - [ ] Alerts: each of the crucifix, mirror, incense, music box, tarot, voodoo doll, monkey paw, Ouija board, name, spirit box, salt and summoning circle raises its alert.
  - [ ] The kill restriction (sight, hearing, gear and kill hooks); the target's HUD on a non-host controlling client.
  - [ ] As master, pick up and use items an investigator has not touched yet (lighter, matchbox, camera): nothing happens to the investigator's own hands or animation.
  - [ ] The journal opens and closes on the ghost player's client: open, close, the pause menu with its player list, the evidence spread, hover, the sounds, with its pages written.
  - [ ] The ghost player turns the journal's pages and marks a guess: no page-turn sound or change reaches the other clients' parked body in the truck. Now that the journal's own RPC hook is gone, this rests on the game's check of `IsMine` alone.
  - [ ] Every mod-drawn UI (HUD, notices, picker, map view, panorama) is in the game's font, alignment and colours; each is marked `PM_UI_TODO` for your design (FINDINGS 31).
- Investigators after the ghost player (provisional difference 6)
  - [ ] With 3 or more players and the ghost player not last, compare for each investigator: lobby card colour against their evidence mark colours on another player's journal, their vote icon on a photo, and their bar on the watch. Check the last sanity bar on the watch (does one stop moving?) and each pause card's photo. Record what differs, for your ruling.
  - [ ] No investigator's model changes through the contract.
  - [ ] Winter's Jest: with the body parked far away, the present lands far from the investigators, with no exception on any client.
- Paw and cards
  - [ ] Paw, nobody dead: a revive wish twitches and keeps its use. With one investigator dead: they are revived, then the wisher dies half the time; no `a revive skipped the ghost player's parked body` line.
  - [ ] High Priestess: with nobody dead the card's flag is set; with one dead they are revived.
- The contract's end
  - [ ] Truck ending, ghost player as master and as a client: no `no investigator came back` line, no NullReferenceException; the camera returns to the truck briefly, then the loading screen (judge the flash); the investigators get their normal results.
  - [ ] All dead: the contract ends when the last investigator dies. No death row and no halved totals on the ghost player's board (`contract end: no death status for the ghost player`); on a difficulty that loses items, their owned amounts are unchanged. Repeat with A dies and B quits, and with everyone quitting: full pay, once, and the line names the quitters.
  - [ ] Partial pay: three investigators, one killed, one wrong, one right: "2 of 3" and two thirds of full pay.
  - [ ] The score shows as the game's own popup in the lobby, for the ghost player and the investigators, once the lobby is back (not over the reward screen); it replaces any message waiting.
  - [ ] Money: the reward screen's total counts to the ghost's amount and the saved money rises by exactly that (`reward: the reward screen's total is the ghost's` once); the level rises by the ghost's experience; investigators' money as usual. Repeat as an investigator in the next contract: the game's own total.
  - [ ] The wait: reach the reward screen before 4 s are up: `reward: the reward screen's results wait for the final score`; Skip and Next do nothing until it is in. Press Skip repeatedly from the start: no double pay.
  - [ ] Challenges: note the ghost player's progress, end a contract as the ghost player: unchanged, with `reward: the reward screen's own challenge progress is not the ghost player's` once.
  - [ ] Seasonal event: no event row on the ghost player's screen (`reward: the reward screen sends no seasonal event score for the ghost player`); an investigator's has it.
  - [ ] Back in the lobby after each end, a leave and a quit: every ID card, the ghost player's included, is present on every client from the first frame (`outlist: the ghost player's spot is back in the player list` once per client).
  - [ ] The master is the ghost player and leaves mid-contract (three or more players, another volunteer ticked): nobody becomes the ghost player for the rest of the contract, the ghost plays on as the game's AI, and back in the lobby the new host draws a ghost player as usual.
  - [ ] The ghost player quits mid-contract: the others' logs show `outlist: the ghost player left the room; their spot is back for the game's own departure`, and the contract goes on.
  - [ ] The black cover over the truck shows again in the session's next contract.
- Leaving and quitting
  - [ ] Note the time from Quit to process exit and keep `mods.log`; if a step hangs, run `cdb -pv -p <pid> -c "~*k 40; q"` from an elevated prompt before killing it.
  - [ ] Quit as the ghost player, stepped out, from the pause menu: `leave: quitting the game`, `body: ... investigator again`, the cleanup line, an exit; no `no investigator came back`.
  - [ ] Leave as the ghost player: `leave: leaving the contract` with no `without an investigator`, the main menu loads, then the room cleanup line. **If it stays in the level, report it.** Then Quit from the lobby.
  - [ ] Close the window (Alt+F4) as the ghost player, stepped out: the cleanup line, then an exit.
  - [ ] Lobby, investigator and the volunteer: Quit from the main menu or the lobby; an exit within a few seconds and `net: Photon's room cleanup finished`.
  - [ ] Play without mods: quit from the lobby and from a contract; no `net: Photon's room cleanup` line and no `context:` line.

## Squatter

The figure (Task 5). Before the test, build `squatter.bundle` (sdk/guides/custom-animation.md) with every state of `squatter_figure.h`'s table and put it in the staged `mods/custom-ghosts/assets/` folder by hand: the release payload gains it only in Task 11, so without it Custom Ghosts does not start.

- Start
  - [ ] With the bundle: Custom Ghosts starts, and `mods.log` has no `CustomGhosts inactive` line.
  - [ ] Without the bundle, and with a bundle whose controller lacks one state: Custom Ghosts is off, its log line names the file or the missing state, and the other mods run.
- The figure, in a private contract with the Squatter forced in a Custom difficulty
  - [ ] A figure stands where the ghost is: the investigator's body under a white sheet, lit like the room, not black. Look at it from all sides and from below: the sheet shows from inside too.
  - [ ] The sheet drapes over the head and shoulders and hangs, with two eye holes in front of the eyes, side by side, with sheet between them. Judge the look: shape, length, stiffness, hole size.
  - [ ] The sheet follows the figure as the ghost wanders and turns, and settles when it stops; nothing flies off or spins.
  - [ ] No `the Squatter's figure could not be built` line; if there is one, report its reason.
  - [ ] Each clip plays: Run, Duck_A, Duck_B, Weave_A, Weave_B, Roll_A, Roll_B, Parkour_A, Parkour_B, Stumble, Sit, GetUp, Kick, Bang, Bump, OpenDoorOutwards, OpenDoorInwards (checkable once Task 7 drives the moves; until then only the controller's default state plays). Travelling clips stay in place and do not snap back each loop.
  - [ ] All clients see the same figure in the same place, facing the same way.
  - [ ] Walking into the figure, shining a torch at it and throwing an item at it: nothing collides with its sheet spheres.
  - [ ] Leave the contract and start another: the old figure is gone, a new one appears, and the log shows no error.

The way out (Task 6). The lock is the game's hunt lock (`Door.HuntingCloseDoor`) on every exit (sdk/systems/doors.md). The
chase that calls it comes in Task 7, so these are checkable once Task 7 is in; a private contract with the Squatter
forced in a Custom difficulty, two players where it says so.

- Each kind of map, the doors locking once every living player is out
  - [ ] A house with one exit (Tanglewood, Edgefield, Ridgeview or Willow Street): run out of the front door; it swings shut and locks, `mods.log` has `squatter: 1 exit(s) held`, and no refused-RPC or exception line follows.
  - [ ] A map with several exits: Brownstone High School (6), Nell's Diner (3) or Bleasdale Farmhouse (2). Every exit shuts and locks at once, not only the one used, and the log names the count the doors page gives.
  - [ ] A map without a house: Camp Woodwind or Maple Lodge Campsite. Both leaves of the campsite's gate shut and lock once everyone is past it; a player at a campfire or in a tent still counts as inside (the gate stays open).
  - [ ] A map with variations: Prison or Point Hope. Every exit locks, including on a smaller variation.
  - [ ] With two players, one still inside (or hiding where it never looks): the exits stay open until that player is out too. A dead player inside does not keep them open.
  - [ ] As a client, not the host: the exits lock for you too, and your log has no refused-RPC line.
- The locked exits
  - [ ] Grabbing a locked exit does not open it, on the host and on a client.
  - [ ] The lock's click as it locks, and the rattle on a try, come only when the host owns the exit's view: a player who grabbed that exit last owns it, and then nobody hears the click and only that player's tries rattle (`sdk/systems/doors.md`, "When a player owns the exit"). Their absence there is not a failure; note who opened the exit last.
  - [ ] With the host owning the exit (nobody but the host opened it) and without the house key picked up, trying a locked exit plays the game's locked rattle, each time.
  - [ ] Having picked up the house key, try a locked exit: report whether it plays the unlock click, and check it still cannot be grabbed open (the code says it stays shut; `sdk/systems/doors.md`, "Locking the exits outside a hunt").
  - [ ] After the exits have locked, make each Monkey Paw wish that reaches them (`sdk/systems/doors.md`, "The Monkey Paw"), and check every exit stays shut and locked on the host and on a client, with `squatter: a Monkey Paw wish ...` in `mods.log`:
    - [ ] the 6 s wish (it also asks for the summoning circle, which the Squatter refuses): no unlock, and the exits stay shut after 6 s;
    - [ ] the 120 s wish: the fuse box still breaks, and the exits stay shut through and after the 120 s;
    - [ ] the trap wish (the ghost sent to its room): its room's doors and yours are timed as the game does, but an exit among them stays shut;
    - [ ] the wish that opens every exit (`MonkeyPaw.OpenExitsWish`): no unlock click, and every exit stays shut for the rest of the contract.
  - [ ] Before the exits lock (a player still inside), each of those wishes does what the game does to the doors.
  - [ ] Each try at a locked exit, by the host or a client, reaches the mod (once Task 7 plays a line for it, each try plays one); a try at an ordinary door plays nothing.
  - [ ] On a friendly ghost's difficulty nothing locks, and the log has no lock line.
  - [ ] Leave and start another contract on the same map: its exits start as the map's own (locked until the key is used), not still switched off.

The mod channel and the exits' hold (Task 7b). Squatter messages go on Custom Ghosts' channel (`mods/custom_ghosts/squatter_net.h`); most of what they carry shows only once Task 7c's chase uses it. A private contract with the Squatter forced in a Custom difficulty, two players (one the host) unless it says otherwise.

- The exits' hold follows the host's lock
  - [ ] As the client, when the host's lock shuts the exits: your `mods.log` has `squatter: N exit(s) held` as they swing shut, with no refused-RPC or exception line, and your count matches the map's exits.
  - [ ] As the client, make a Monkey Paw wish on the exits as soon as they lock (the 6 s wish, then the one that opens every exit): every exit stays shut for both players.
  - [ ] On a contract with another ghost (not the Squatter) that hunts: after the hunt, a Monkey Paw wish on the exits does what the game does; nothing in the log says the exits are held.
- Tries and sprint reach the host
  - [ ] As the client, try a locked exit once by the interact key and once by grabbing: each press reaches the host once (once 7c plays a line for it, one line per press, never two).
  - [ ] As the client, run out of breath while chased: the host's Squatter taunts you (needs 7c). On a Custom difficulty with sprinting off, the taunt plays without running.
  - [ ] As the client, take a Monkey Paw wish that changes your speed: the Squatter's pace follows your new speed (needs 7c).
  - [ ] With a VR player as the client: running out of breath reaches the host as on PC (VR's stamina clears the same flag).
- Host migration
  - [ ] The host leaves mid-chase: the new host's Squatter carries on chasing, with no restart in its room.
  - [ ] The host leaves after the exits have locked: they stay locked, with no second lock click and no refused-RPC line, and later tries still get a line.
- Doors on maps other than Grafton
  - [ ] On Brownstone High School or Nell's Diner, the Squatter's agent passes doors as on Grafton: only Grafton's doors were checked for a NavMeshObstacle (`sdk/systems/doors.md`, "A ghost walking through"); report a door it stops at.

Restraint while steering and the host's re-lock (Task 7c1). A private contract with the Squatter forced in a Custom difficulty, two players (one the host) unless it says otherwise.

- The door push: not built. `Door.GhostForceDoor` leaves a door-moved EMF reading, the ghost's door sound, a count in the results' "doors moved" and ghost interactions and, for an Ultraviolet ghost, fingerprints (`sdk/systems/doors.md`, "A ghost's force"). The user chose the quiet opener instead (below).
- The quiet opener (`squatter_door.h` `open_quietly`; the chase calls it from Task 7c2, so these wait on 7c2)
  - [ ] When the Squatter reaches a shut door on the chase, the door swings open part of the way for both players at the same angle, and the host's `mods.log` has `squatter: a door opened quietly`. Test once with a door the host grabbed last and once with a door the client grabbed last (the host takes the client's door's view first, so that one opens a moment later).
  - [ ] No EMF reading appears at an opened door (hold an EMF reader at it), no ghost door sound or rattle plays, and the results screen's "doors moved" and ghost interactions do not count it.
  - [ ] The opened door creaks and closes normally when a player then pushes it shut, and either player can grab it.
  - [ ] A door the client is holding when the Squatter reaches it stays in the client's hand: the Squatter does not open it.
  - [ ] The exits never open this way: with an exit shut and unlocked before the lock, the Squatter passes it without opening it, and after the lock the exits stay locked.
  - [ ] A late or failed takeover (a laggy client owning the door) gives up after about two seconds: the door stays shut, and nothing in the log says it opened.
- Steering
  - [ ] On a friendly ghost's difficulty the Squatter mopes where it starts: it never wanders off or walks to its favourite room, and the log has no exception line.
  - [ ] On a normal difficulty, while it lurks, it stays where it started in the ghost room, sitting, and never wanders off (Task 7c2 holds it there).
- The host's re-lock
  - [ ] As the client, make the Monkey Paw wish that opens every exit at the moment the exits lock (or the 6 s wish just before): if an exit reopens, the host's `mods.log` has `squatter: 1 reopened exit(s) locked again` and the exit shuts and locks again within about two seconds, for both players. Repeat a few times; a wish that lands after the lock leaves the exits shut with no re-lock line.
  - [ ] After the lock, on an exit nobody grabbed but the host (so the host's lock clicked), a player holding the house key tries it: it unlocks with the game's click, then the host locks it again (a second click), and it never opens. On an exit the client grabbed last, the key does nothing new and the host's log has no re-lock line.
  - [ ] With the exits locked and nobody wishing, the host's log has no re-lock line, also when the client was the last to grab an exit before the lock.

The chase (Task 7c2). A private contract with the Squatter forced in a Custom difficulty, two players (one the host) unless it says otherwise. The voice folders must hold recordings for lines to play.

- Lurking
  - [ ] It sits in the ghost room saying lurk lines every few seconds, and both players hear the same line at the same time from where it sits.
  - [ ] It never wanders out of the room before it sees someone.
  - [ ] Walking into its sight starts the chase: it gets up, then runs at the nearest player, saying chase lines. Talking near it on its floor without being seen does not start it.
- No hunt
  - [ ] During the chase no light flickers, no electronics interfere, no hunt sound plays, and no door locks before everyone is out (7a could not trace the flicker code; report any flicker).
- The chase
  - [ ] A player sprinting away barely outruns it; a walking player is caught up with. Tune `kSprintMargin` and the move multipliers with the user.
  - [ ] Its run is broken by ducks, weaves, rolls, parkour and stumbles, the roll a burst forward and the stumble a stop, and both players see the same move at the same moment.
  - [ ] It bumps into things every so often, and when it stalls against something: it plays the bump clip and stops briefly.
  - [ ] It goes for the nearest player when two are inside, and switches when the other comes nearer.
  - [ ] A player whose sprint runs out hears a tired line, at most once every twenty seconds; with sprinting off in the Custom difficulty, the chased player hears it as the chase turns to them. Its speed is the same either way.
  - [ ] As the client, being chased gives the same pace as the host being chased (the client's sprint reaches the host).
- Losing track and searching
  - [ ] Breaking line of sight and staying quiet loses it after about four seconds: it searches around the last place it saw you, saying search lines, slower than its chase.
  - [ ] Talking loudly near it on its floor, or being seen again, starts the chase again.
- Hiding
  - [ ] Running into a closet or locker in its sight: it goes to stand outside, says hiding lines and kicks the door over and over. The door never opens and the player is never caught there.
  - [ ] Hiding unseen: it searches and never finds you (unless you talk loudly).
  - [ ] Stepping out: it chases at once.
- Doors on the way: the quiet opener entries above (Task 7c1), now live.
  - [ ] The opening-door clip plays as it opens one, outwards or inwards by the side it stands on, and it stops briefly.
- Out of the house
  - [ ] It never follows a player past an exit. Once every living player is out, the exits lock, and it sits down inside.
  - [ ] Each try at a locked exit gets a door line, once per try.
  - [ ] One player out and one inside: the exits stay unlocked and it keeps after the one inside.
- Host migration
  - [ ] The host leaving mid-chase: the new host's Squatter carries on chasing, no line or clip replays, and after the lock nothing locks twice.
- Sight
  - [ ] A Lithobolia contract still throws at players it sees outside hunts (its sight copies are now shared, `ghost_sight.h`).


The kick (Task 8). A private contract with the Squatter forced in a Custom difficulty, two players (one the host) unless it says otherwise.

- [ ] Caught within about a metre in the chase, the victim is kicked: the Squatter plays the kick, a catch line plays for both players, and the victim dies with the game's own snapped-neck death (no ghost choke, no ghost model). Note what the victim's camera does, and what the other player sees of the body.
- [ ] The victim is dead for the game: the truck's sanity monitor shows "?", and a Monkey Paw revive brings them back.
- [ ] Both players caught one after the other: each is kicked once, and with everyone dead the contract ends as vanilla.
- [ ] As the client being caught: the same kick and death, and the host's `mods.log` has no exception line.
- [ ] On a friendly ghost's difficulty nothing is ever kicked (Task 9 checks the rest of friendly mode).

Friendly mode (Task 9). A private contract with the Squatter forced in a Custom difficulty with the friendly ghost on, two players (one the host) unless it says otherwise.

- [ ] It starts seated in the ghost room, saying mope lines every few seconds, and stays there until it sees someone.
- [ ] On first sight it gets up, says one excited line, then follows the nearest player around inside at a walking pace, stopping a step from them and saying friendly lines.
- [ ] It opens shut doors on the way quietly, as in the chase, and bumps into things now and then.
- [ ] It never follows anyone outside: leaving the house, it waits inside. No exit ever locks, and nobody is ever kicked or killed.
- [ ] Hiding in a closet in its sight: it waits at the door and talks, and never bangs.
- [ ] Losing it: it goes to where it last saw you and waits there.

Not playable (Task 10). A private contract with Custom Ghosts and Playable Ghost, a ghost player chosen in the lobby.

- [ ] The ghost player's journal pick never lists the Squatter, and its type is never the Squatter however many contracts are started without choosing.
- [ ] Without a ghost player, a Random contract can still be the Squatter.
