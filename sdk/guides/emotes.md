# Emotes and keys of your own

What the SDK gives a mod that makes players dance, or that needs a key players rebind: the Emotes mod (`mods/emotes`) is
built from these and nothing else. No reverse engineering is needed. The game's side of each is in the systems pages
linked below.

## A key in the game's keybinds menu

`sdk/api/keybinds.h`. Declare a key with `begin_keys(modId, pm::mod::shown_name(), file)` and `add_key(ModKey)` in `pmmod_start`, call
`tick_keys()` every frame, and read it with `key_state(id)`.

- A `ModKey` has a default path per device (keyboard, gamepad, VR, in Input System syntax: `"<Keyboard>/x"`,
  `"<Gamepad>/dpad/up"`, `"<XRController>{LeftHand}/{PrimaryButton}"`) and fallbacks for each. A default the game
  already binds falls back to the first free one; the game's menu-navigation actions (named `UI...`) do not count as
  taken. VR defaults are not checked against the game.
- The keyboard and gamepad bindings each get a row in the game's keybinds menu, labelled with your text, and the game's
  own rebinding runs on it. Your rows sit at the end of each list, under a heading with your mod's name (the manifest's,
  which `shown_name` reads from the host), with the list's own dividers between them; the list's scroll grows to reach
  them. Rows are copied only from the local player's own keybinds menu, never from a map's training whiteboards. While
  a row is on screen the key's actions are off, so the game's rebinding can run on them; `key_state` reads nothing then. A rebinding is kept as the game keeps its own:
  the page's Apply saves it to the file you name, Reset takes it off, and leaving the page without Apply drops it.
  `begin_keys` hooks the page for this, so enable your hooks after it.
- The key is ready (`keys_ready`) once the game's bindings can be read: the keybinds menu or a local player is loaded.

How the game's actions and the menu work: [menus](../systems/menus.md#key-bindings).

## A setting on the audio page

`sdk/api/settings.h`. Declare a slider with `begin_settings(modId, pm::mod::shown_name(), file)` and `add_slider(Slider)`
in `pmmod_start`, call `tick_settings()` every frame, and read it with `value(id)`. Its row is a copy of the master
volume's, under your mod's heading on the journal's audio page. Its value counts on the page's Apply and is saved to your
file; leaving the page without Apply puts the slider back. The whole page reads with one line between every pair of rows
the player can see, yours and the game's alike: the SDK puts the page's own dividers away and copies one line in
between every pair of rows, the line taken from the keybinds page when one is loaded
([menus](../systems/menus.md#the-audio-page)). The Emotes mod's volume is one.

## A clip on a player's own body

`sdk/api/player_anim.h`. `play_on_player(player, clip, blendIn)` plays an `AnimationClip` (from your bundle,
[custom animation](custom-animation.md)) over the animation the game gives any player's body, on your client only;
`stop_on_player(player, blendOut)` gives the body back exactly as it was. A second clip blends in over the first.
`clip_time(player)` is the clip's own time, which a song can follow. Call `tick_player_anims(dt)` every frame. Humanoid
clips play on the investigator whatever skeleton they were made for. Each client plays what you tell it through your
channel. A kill takes the body: hook `Player.NotifyOfDeathStart` and call `stop_on_player(player, 0.0f)` there, or your
clip plays over the game's death animation and hides it
([players](../systems/players.md#the-mark)). How the body is animated: [players](../systems/players.md#the-body).

## A third-person view, and the player's own body shown

`sdk/api/third_person.h`. `begin_third_person()` eases a camera of your own out behind the local player's head, looking
where they look, and `end_third_person()` eases it back; the mouse still turns it. `show_own_body(true)` shows the local
player their own body on their own camera instead, for VR. Call `tick_third_person(dt)` every frame. How it renders as
the game does: [media](../systems/media.md#a-camera-of-a-mods-own-and-screen-effects).

## A sound the ghost hears as talking

`sdk/api/talk_loudness.h`. The ghost hears players through their talking loudness. Measure your sound in the game's
scale (`loudness_of` from samples, or `envelope_of_rms` from RMS values measured beforehand) and raise it on the master
with `add_talk_loudness(player, loudness)` once a second (`Pace`), as the game refreshes a voice. Every rule of the
ghost's voice checks then applies to it. How the checks work: [voice](../systems/voice.md#how-loud-a-player-talks).

## A blur and a vignette

`sdk/api/screen_effects.h`. `set_screen_effects(blur, vignette)` puts a blur and a vignette over the game, under
overlay canvases; `set_screen_effects(0, 0)` removes them. A canvas of your own (`core/ui.h`) is a new window, which
the project's vanilla-UI rule leaves to the author's design; ask before drawing one.

## An emote pack

Other mods' dances come in as *emote packs*: content packs for the Emotes mod, mod folders of data alone (SDK
README, "Content packs"). A pack has no DLL and no code: the loader lists it in the Mods panel, needs the Emotes mod
(a required dependency), starts it right after that mod and stops it with it, and the lobby's list includes it, so a
joiner without it is removed. What the pack holds is the Emotes mod's own format: an `emotes.json` catalogue and an
asset bundle, read by the mod with `pm::api::packs::owned()` (wrapper: `sdk/api/packs.h`). The worked example is
`sdk/examples/example_emote_pack/`, laid out and built exactly as below (its README.md walks through both tool steps).

### The folder

- `manifest.json` says who the pack is data for: `"contentPackFor": { "id": "phasmomods.emotes", "minimumVersion":
  "<version you need>" }`, which becomes a required dependency. A manifest that names both `contentPackFor` and a
  `dll` is refused: a pack is data only (`core/mod_manifest.h`).
- `assets/emotes.json` is the catalogue, in the format below (`mods/emotes/catalogue.h`).
- `assets/<bundle>` is the asset bundle, under the plain file name the catalogue gives. Name it after your pack id:
  the engine loads a bundle once, so two packs whose bundles share a name cannot both load (`sdk/api/bundles.h`).
- `assets/loudness/<name>.txt` holds each song's measured loudness ("A sound the ghost hears as talking" above);
  the tool's Build step writes these files, and the mod reads them at start.
- `i18n/default.json` holds each emote's name under the label key its catalogue entry gives; the wheel shows it, and
  `tests/text_table_test.cpp` checks every catalogue label against the table.

The mod reads the catalogue, the bundle and the loudness files from the copy the launcher makes of the pack's
`assets` folder for each game run, never from the installed pack folder.

### The catalogue

`emotes.json` is one JSON object; `//` comments are allowed. The top level:

| Field | Rule |
| --- | --- |
| `format` | required: `1`. Any other number, or none, refuses the pack. |
| `bundle` | required: one file name in the pack's assets folder, ending `.bundle`, with no folder or drive in it. |
| `emotes` | required: 1 to 64 emote objects. The wheel keeps them in this file order. |

Each emote:

| Field | Required | What it is, and its rule |
| --- | --- | --- |
| `name` | yes | its short id: 1 to 40 lower case letters, digits and dashes, unique in the pack. The wheel knows it as `"<pack id>.<name>"`. |
| `clip` | yes | the dancing clip: a project path under `Assets/` ending `.anim`, the path it has in the bundle. |
| `song` | yes | its song: a path under `Assets/` ending `.wav`, `.ogg` or `.mp3`. The bundle's copy is cut to its loop (see the tool below); the engine loops it sample for sample. |
| `icon` | yes | its wheel icon: a path under `Assets/` ending `.png`. |
| `label` | yes | the key of its name in the pack's `i18n/default.json`. |
| `loudness` | yes | the song's measured loudness: a `.txt` under the pack's assets folder by a plain relative path (no `..`, no `\`, no drive). |
| `moveClip` | no | its walking variant, a `.anim` under `Assets/`; it may equal `clip` when the clip walks as it is. |
| `introClip` | no | a `.anim` under `Assets/`, played once before the loop. |
| `introSong` | no | a song under `Assets/`, played once before the looping song. |
| `introLoudness` | no | the intro song's loudness file: set it exactly when `introSong` is set. |
| `loops` | no | `true` by default. `false` makes a one-shot, which ends when played out and cannot set `moveClip` or `introClip`. |
| `songLoop` | no | `{ "start": ..., "seconds": ... }`, two non-negative numbers of seconds: where the song repeats, as the tool measured it, for the ghost's hearing. When it is absent, or its seconds are 0, the mod takes the clip's own length as the loop. |
| `partner` | no | makes the emote synced, danced by two: see "A synced emote" below. |
| `clipSource`, `clipLoopGuess`, `songSource`, `songLoopGuess` | no | the tool's notes: the source clip and song to measure, and the loop length to search near. The mod reads them silently; only the tool acts on them. |

An unknown field is not a failure: the pack loads and the mod names every unknown field in its log. A catalogue that
breaks one of these rules is refused whole, with one reason for the log and nothing read from it.

### A synced emote

An emote with a `partner` object is danced by two: the player who picks it dances `clip`, and another player can join
and dance the partner's clip beside them (`docs/emotes.md`, "Dancing together"). The object:

| Field | Required | What it is, and its rule |
| --- | --- | --- |
| `clip` | yes | the second dancer's clip: a `.anim` under `Assets/`. It loops as the emote does, and every client starts it at the first dancer's point, wrapped to its own length. |
| `offset` | yes | `{ "right": ..., "forward": ... }`, two numbers of metres: where the second dancer stands, to the first dancer's right (negative for their left) and in front of them, facing the same way. Sway's is `-1.3` and `0`. |
| `song` | no | a synced song under `Assets/` (`.wav`, `.ogg` or `.mp3`), which replaces the emote's song from the same point when someone joins. |
| `loudness` | no | the synced song's loudness file: set it exactly when `song` is set. The ghost hears the synced song from it. |
| `introClip` | no | a `.anim` under `Assets/`, allowed only when the emote has its own `introClip`. The mod reads it but does not play it yet: a join waits for the first dancer's intro to end. |

A synced emote must loop and cannot set `moveClip`: two dancers never walk together, and a one-shot is never danced
together. An unknown field inside `partner` is named in the log as `emotes[i].partner.<field>`. The emote pack tool
checks the partner's paths as it checks the emote's own, sets the partner's clip to loop as the emote does, and measures
a synced song into its loudness file.

### What the mod plays

- An emote with an `introClip` starts on it and runs on its `clip` when the intro ends; each client times the intro's
  end on its own clock, so no message marks it. Walking forward during an intro skips the rest of it, straight to
  the walking clip.
- A `moveClip` plays while the game's move input points forward: at least `kForwardInput` forward and less than that
  sideways, forward only (`mods/emotes/emote_rule.h`). While walking forward, the half-metre cancel point travels
  with the dancer, so walking forward never ends the emote. Letting go of forward for `kStopHoldSeconds` (0.2 s)
  blends the standing clip back, never the intro; sprinting ends a walking emote, and a sideways or backward step
  cancels from where the walking stopped. VR rigs give no move input, so a VR dancer treats a walking emote as a
  standing one.
- An `introSong` plays once, and the looping song starts on the sample the intro ends; both begin together. The ghost
  hears the intro from its own loudness file and the loop from the song's.
- A `loops: false` emote ends the frame its clip plays out.

### The emote pack tool

The Unity package `sdk/unity/com.phasmomods.emote-tools` finds the loops and builds the bundle; its `package.json`
targets Unity 2022.3. Add it to your project by a `file:` line in the project's `Packages/manifest.json` that points
at the package folder, the way a local package is referenced:

```json
"com.phasmomods.emote-tools": "file:<path to the SDK copy>/sdk/unity/com.phasmomods.emote-tools"
```

From the editor's menu, `PhasmoMods/Emotes/Find loops` and `PhasmoMods/Emotes/Build pack` each ask you for the pack
folder, which is the folder holding `emotes.json` (a pack's `assets` folder). Or run both in batch through
`tools/emote_pack.ps1 -Unity <Unity.exe> -Project <Unity project> -Pack <pack folder> [-FindLoops] [-Build]`: it runs
one batch-mode Unity per step, `FindLoops` first, prints the `EMLOOP`, `EM` and `EMFAIL` lines from the run, and
stops with the log's path when a step fails.

`FindLoops` measures and cuts each emote in turn, writing its song and clip as it goes, then writes the measured loops
into the catalogue; if anything fails it leaves the catalogue untouched, though the emotes before the failure are
already written:

- With a `songSource` and a positive `songLoopGuess`, it finds the sample lag near the guess at which the song best
  repeats itself, and writes a loop wav at the emote's `song` path. The loop is cut from its second time round, clear
  of the decoder's start padding, so the song must run at least two of its loops. It records the measured `songLoop`
  in the catalogue, keeping the file's comments and layout.
- With no positive `songLoopGuess` (the song at the `song` path is a loop already), that song's whole length
  becomes `songLoop`.
- With a `clipSource` and a positive `clipLoopGuess`, it finds the frame near the guess at which the animation best
  returns to its start, by pose and by motion. When that frame's cost is at most a quarter of the median over all
  the clip's frames (`ClipCutShare`), the clip is cut there. Otherwise it is crossfaded to the guess: its tail past
  `clipLoopGuess` blends into its start, so the guess must be shorter than the clip (its length less half a second
  works). A guess of the clip's whole length keeps the whole clip instead, with Unity's Loop Pose
  (`AnimationClipSettings.loopBlend`) easing its end into its start. The result replaces the `clip` path, with
  `loopTime` on. A source must be another asset than the `clip` or `song` the tool writes from it.

`Build` checks and builds: every named clip must exist and be humanoid, and gets its `loopTime` as the catalogue's
`loops` says (intro clips never loop); it writes each song's loudness file (RMS amplitude per 0.1 s window) under
the pack folder and sets the song and icon import settings the bundle builds with; then it gives every named asset
the bundle's name, builds for Windows x64, refuses when the bundle holds other than exactly the named assets, and
copies the bundle into the pack folder.

Every path the catalogue names stays where the tool may read or write: `PackPaths.cs` refuses an asset path (clip,
song, icon, source) not under the project's `Assets/`, a loudness path not under the pack folder, and a bundle name
that is not a plain file name, before anything is read or written. The mod checks loudness paths the same way when it
loads them.

The `EMLOOP` lines report each measurement: one per song with its loop in samples and its repeat correlation (1 is a
perfect repeat), marked `POOR` below 0.99; for clips the best frame anywhere past half a second, then the best
frame near the guess with its cost and its share of the median, and the cost at the clip's own end, then whether the
clip was cut or crossfaded. A crossfaded clip returns to its
start through the blend rather than at a clean frame, so watch how it loops.

### What fails, and where a player sees it

- A pack whose catalogue breaks a rule, whose bundle is missing or will not load, or whose bundle lacks a named clip
  is left out as a whole: one reason goes to the mod's log, and the pack is reported with `pm::api::packs::failed()`,
  which keeps it off and shows the reason (the Emotes mod's `emotes.pack-unusable` text) in the Mods panel. The mod's
  own emotes and the other packs still play, and the mod stays on. A broken catalogue of the mod's own keeps the
  whole mod off.
- A missing song, intro song or icon is only a log line: that emote plays silent, or shows a bare icon. A pack with
  no readable string table shows its emotes' label keys.
- Two packs whose bundles share a name: the engine loads a bundle once, so the second pack's bundle will not load and
  that pack is refused. Naming each bundle after its pack id keeps this from happening.
- Two emotes that share a wheel key (the same pack id and name, or a hash collision between two ids): rather than
  guess which emote was meant, the mod keeps its own and leaves every pack out, each reported off in the Mods panel.
