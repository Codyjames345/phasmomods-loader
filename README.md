# PhasmoMods Loader

## AI Disclosure

This project makes heavy use of AI for reverse engineering and C++ programming. I acknowledge that I cannot call this wholly "my" project, and that code readability is likely poor despite my best efforts at making a robust SDK. Hence I have decided to make this project fully open source and allow derivative works This is only a hobby and I have no ill intentions towards the community or Kinetic Games. I understand a
subset of the community will be upset with this decision, but reverse engineering is incredibly tedious and not feasible for a solo developer working out of passion alone.

<u>AI was not used for any of the following:</b>
- this README
- ideas for custom ghosts/spectral flame
- journal text

## Important Disclaimers

As Phasmophobia does not have any official way to load mods, I had to bypass the game's anti-cheat. This brings up some <b>very important</b> points:

- I have no intention to facilitate cheating. Please read below for details on steps I've taken to be careful about this.
- There is no guarantee your account won't be flagged for cheating. Use of any mods is cheating, regardless of what those mods do. You have been warned.
- This project is still in development, and I'm only doing it as a hobby. Please expect bugs.

The most important takeaway if you read nothing else: <b>Do not attempt to use mods in public lobbies</b>

## Overview

The PhasmoMods Loader is my solution to a modding system for Phasmophobia. It provides an SDK with a comprehensive suite of functions to interface with the game in the way of modifications or additions. The loader handles mod matching between players in a lobby, version tracking, dependencies, networking, and more.

The goal of this is to provide a way for developers to create their own mods without the need for reverse engineering, substantially lowering the barrier of entry.

<u>In an effort to protect against misuse, I have made the following decisions</u>
- Mods are only allowed in private lobbies. The loader will forcefully disable all mods in public lobbies.
- The anti-cheat is not documented, nor are any of its functions available in the SDK.
- I plan to disable Steam achievements when mods are available, but this still needs work.
- A consent banner is presented that must be agreed to before mods are enabled.

The mod loader attaches to the game through DLL injection, which may be picked up as malicious by your antivirus. If you encounter any issues, you may need to add the executable as an exception. If this makes you suspicious, please read the source code for yourself or compile your own version from source - I have nothing to hide. This is just a consequence of how this type of mod loader works.

If you prefer automatic injection so you can launch straight from Steam, the loader provides a launch argument you can copy and paste into your Phasmophobia game settings. This only needs to be done once. After that, the game will start with the mods button visible on the main menu without any manual steps.

Each mod is its own DLL with a manifest containing its version, ID, author, display name, dependencies, and description. You can toggle any mod from within the mods menu in-game.

## Using Mods

As mentioned above, mods will only be loaded in private lobbies (for obvious reasons). Please don't attempt to get around this as I don't wish to catch attention from Kinetic Games. This whole project is in good faith and that requires users (you) to use it in good faith.

When the host creates a lobby, a mod list will be created based on their enabled mods. This mod list will remain fixed for the duration of that lobby. When players attempt to join, a few cases can happen:

| Case | How is it handled? |
| --- | --- |
| Player joining isn't running PhasmoMods | Player is disconnected and shown a generic network error |
| Player joining has a subset of the required mods | Player is disconnected and shown a list of the required mods, with missing mods highlighted |
| Player joining has a superset of the required mods | Extra mods are disabled (this is shown to the player), and the player joins successfully |

## Bundled Mods

I have included four mods to demonstrate the capabilities of PhasmoMods. They are as follows:

| Mod | Source | What it does |
| --- | --- | --- |
| Playable Ghost | `mods/playable_ghost` | One player plays as the ghost. All vanilla tells (speed, abilities, special rules) are maintained to the best of my knowledge, though do expect bugs. |
| Spectral Flame | `mods/spectral_flame` | An eighth evidence: firelights will slowly turn blue when the ghost is in the same room ([details](docs/spectral-flame.md)). |
| Custom Ghosts | `mods/custom_ghosts` | Fifteen new ghost types, each with their own lore and unique behaviour; requires Spectral Flame ([details](docs/custom-ghosts.md)). |
| Ghost Tells | `mods/ghost_tells` | Additional unique behaviour for six vanilla ghosts, to make zero evidence a bit more manageable (targets ones that are a pain to identify) ([details](docs/ghost-tells.md)). |

`archive/` holds mods that were being developed but stopped for one reason or another. The most promising one was Bigger Lobbies but unfortunately a server-side change a few years ago has made this dream impossible without a custom server implementation. The 20 player lobby dream may return if someone is willing to investigate this, but it's out of my scope for now.

### Custom Ghosts

| Ghost | Description | Evidence |
| --- | --- | --- |
| Echo  | A restless nymph known for its copycat behaviour. It often repeats actions of those around it, and talks in a familiar voice... be careful who you trust. |  |
| Dullahan | The headless rider of Irish legend. Trading agility for speed, this ghost is a menace on straightaways but easily bested by corners. Don't let its slow turns lull you into feeling safe. |  |
| Kikimora A Slavic household spirit with a compulsion to tidy up clutter left in the dark. Keep an eye on your equipment - this thief will keep you on your toes. |   |
| Draugr | The jealous revenant of Valheim, strongly bonded to its favourite room. It is angered by material leaving its room, but its tether slows it down the further away it chases you. |   |
Black Shuck: The phantom hound of the East Anglian roads, wielding a powerful sense of smell to trace your steps. It can't see or hear you, but hiding from its nose proves difficult. Salt, running water or rain breaks your scent trail.
| Lithobolia | The ancient stone-throwing devil, a spiteful spirit that seeks a living target. It throws objects exclusively at players, and will wait if no-one is in sight. |   |
| Gremlin | The saboteur often blamed for failing technology. This critter will often turn off equipment that has been set down and left running. |   |
Brownie: A Scottish house spirit that only works unwatched. In a hunt it is incredibly swift and quiet while nobody watches - but a glance brings it to a crawl.
| Bogeyman | The classic monster in the closet, it searches hiding places before anywhere else. Its hunts begin from the closet or locker nearest the youngest player (lowest XP level/prestige), and it goes for them first. Slow in open rooms and loses sight of its prey sooner. |   |
| Lethe | With its namesake being the underworld river of forgetfulness, this ghost has the power to make players forget - much like it does. Photos, videos and recordings of it fade from the journal a few minutes after they're taken, stopped only by players taking care to remember them (viewing the media page of the journal). During a hunt it will also lose interest quickly after breaking line of sight. |   |
| Mesmer | An illusionist with powerful deceptive abilities. Its physical form appears differently for each player, creating confusion and panic that throws off the truck's sanity monitor - which shows the team saner than they are. It can't fool a camera: video, photos and the D.O.T.S. projector show its true form and location. |   |
| Knell | The slow toll of a funeral bell, its footsteps mirror the rhythm. Every couple seconds it alternates between powerful surges of speed and a slow crawl, held back by the inevitability of death. |   |
| Asterion | The birth name of the Minotaur, it is cursed to navigate the living realm as a labyrinth - unable to pass through doors. Though it lacks the ability to phase, its raging fury won't keep it trapped for long. Its sheer strength is enough to blast open doors after a few seconds and destroy their hinges, making them unusable for the rest of the contract. |   |
| Tempest | A weather-based ghost, gifted ludicrous speed when supercharged by its conjured lightning strikes. Its power amplifies with raging weather - strongest in the rain, and weakest when skies are clear. This demonic presence is completely invisible to the naked eye, only revealed by its lightning... |   |
| Hydra | Cut off one head, and two more take its place... the serpent of Lerna proves formidable against smudges. The true copy is difficult to identify, though a keen ear may help. Beware, a sickly green spectral flame provides warning to those who dare fight it. |   |

### Spectral Flame

The addition of more than a few ghosts mandated a new type of evidence, as all possible combinations of three ran out. The idea came from soul flame (Minecraft) - a flame enraged by trapped souls, giving it a haunting blue colour.

This evidence type gives more purpose to firelights as I felt they were underused.

When the ghost is present in the same room as a firelight, it will slowly begin to shift its hue towards blue. This change takes about a full minute but differs based on the firelight tier. When the ghost walks out of the room, its colour fades back to orange at half this speed.

For ghosts without spectral flame evidence: the colour will shift to a pale blue and remain steady, progressing no further.
For ghosts with spectral flame evidence: the colour will shift to a deep blue in the same length of time. I decided to make the time period the same so that you can potentially get the evidence early by noticing the faster rate of colour change, adding a bit of skill into the mix. The hardest part is balancing the colour change so it doesn't reveal the ghost's location too easily, but isn't annoyingly long. Will need to test.
To make things a bit more fair, ghosts with spectral flame will not be able to blow out firelights until they are fully blue. This was because I kept getting them blown out before they were noticeably blue, and it also doubles as a quick rule-out if a non-blue candle is ever blown out (similar to how a ghost throwing the writing book rules out ghost writing).

Lastly, the Hydra has a unique tell of shifting firelights to green instead of blue. Thought it'd be fun to spice things up a bit.

## Install

Requires Windows x64 and Phasmophobia on Steam. Mods have been tested on the latest version as of 2/11/26, but future versions may break some mods. The mod loader is designed with this in mind, and (hopefully) only minor changes to the loader is all that will be required (mods don't need to change).

1. Download `PhasmoModsLoader-win-Setup.exe` from the [latest release](../../releases/latest) and run it.
2. Windows SmartScreen warns about the file the first time, because the release is not
   code signed. Choose **More info**, then **Run anyway**.
3. Tick the mods in PhasmoMods Loader, start Phasmophobia through Steam, and choose
   **Attach to running game** at the main menu. Answer the notice in the game.

PhasmoMods Loader installs into `%LOCALAPPDATA%\PhasmoModsLoader`, checks for updates
each time it starts, and keeps your settings in `%LOCALAPPDATA%\PhasmoMods`. See the
[installation guide](docs/installation.md) for the Steam launch option, removal and troubleshooting.

## Build from source

Install Visual Studio's C++ x64 tools, CMake, Ninja, PowerShell and the .NET SDK
(for the pinned `vpk` packer). MinHook 1.3.4 and the Velopack C++ library are vendored
in `third_party`. From the source folder:

```powershell
.\validate.ps1   # builds the native targets and runs the native and packaging tests
.\package.ps1    # validates, then packs dist/releases with Velopack's vpk
```

`dist/releases` holds `PhasmoModsLoader-win-Setup.exe` and the update packages.
Every build also replaces `dist/PhasmoMods` with the unpacked payload, so the program
there runs the build just made. Close it before building.

## Source layout

`core/` is shared by the host and every mod (IL2CPP access, game bindings, UI built from
game classes, and the host contract in `core/mod_api.h`); `host/` is the host; `tools/`
holds the PhasmoMods Loader and the injector. `archive/` holds archived mods: Bigger Lobbies, Replay, and Afterlife.

Agents and contributors wish to create their own mods start at [sdk/README.md](sdk/README.md):
it explains the game SDK (the map of game types and methods, the game API, the offline check
`tools/gamecheck.ps1`) and where reverse-engineering findings are recorded.

See the [design](docs/design.md), [the suite plan](docs/superpowers/plans/2026-10-20-mod-suite.md), and
[loading evidence](docs/loading-investigation.md) for implementation and verification limits.
