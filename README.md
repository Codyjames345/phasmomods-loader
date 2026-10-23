# PhasmoMods Loader

Mods for Phasmophobia, started together by one program. PhasmoMods Loader attaches to
the running game, `ModHost.dll` verifies the game build and asks every player to agree
to a notice, and then the mods you ticked start. The **Mods** button on the game's main
menu switches mods on and off without restarting, and a modded lobby admits only
players with the same mods. Each mod is its own DLL under `mods/`:

| Mod | Source | What it does |
| --- | --- | --- |
| Playable Ghost | `mods/playable_ghost` | One player plays the ghost; the game supplies speeds, evidence, abilities and hunt rules. |
| Spectral Flame | `mods/spectral_flame` | An eighth evidence: a candle, lantern or candelabra burns blue where the ghost lingers ([details](docs/spectral-flame.md)). |
| Custom Ghosts | `mods/custom_ghosts` | Fifteen new ghost types with their own evidence, journal pages and rules; needs Spectral Flame ([details](docs/custom-ghosts.md)). |
| Ghost Tells | `mods/ghost_tells` | Three new tells each for six vanilla ghosts ([details](docs/ghost-tells.md)). |

## Read this first

- These mods are for private lobbies among friends. Do not use them in public matchmaking.
- Phasmophobia's anti-cheat reports modified clients, and an account can be banned for
  it. You use these mods at your own risk. The authors cannot undo a ban.
- This is a development build. Native activation works, but a complete multiplayer round
  has not been verified. See [outstanding findings](FINDINGS.md).
- Nothing is written to the game folder, and no game file is replaced.

## Install

Requires Windows x64 and Phasmophobia on Steam, at the game build the release names.
The mods stay inactive on any other build.

1. Download `PhasmoModsLoader-win-Setup.exe` from the [latest release](../../releases/latest) and run it.
2. Windows SmartScreen warns about the file the first time, because the release is not
   code signed. Choose **More info**, then **Run anyway**.
3. Tick the mods in PhasmoMods Loader, start Phasmophobia through Steam, and choose
   **Attach to running game** at the main menu. Answer the notice in the game.

PhasmoMods Loader installs into `%LOCALAPPDATA%\PhasmoModsLoader`, checks for updates
each time it starts, and keeps your settings in `%LOCALAPPDATA%\PhasmoMods`. See the
[installation guide](docs/installation.md) for the Steam launch option, moving over from
the old portable folder, removal and troubleshooting.

## Build from source

Install Visual Studio's C++ x64 tools, CMake, Ninja, PowerShell and the .NET SDK
(for the pinned `vpk` packer). MinHook 1.3.4 and the Velopack C++ library are vendored
in `third_party`. From the source folder:

```powershell
.\validate.ps1   # builds the native targets and runs the native and packaging tests
.\package.ps1    # validates, then packs dist/releases with Velopack's vpk
```

`dist/releases` then holds `PhasmoModsLoader-win-Setup.exe` and the update packages.
Every build also replaces `dist/PhasmoMods` with the unpacked payload, so the program
there runs the build just made. Close it before building.

## Source layout

`core/` is shared by the host and every mod (IL2CPP access, game bindings, UI built from
game classes, and the host contract in `core/mod_api.h`); `host/` is the host; `tools/`
holds PhasmoMods Loader and the injector. `archive/` holds mods that are kept but not
built: Bigger Lobbies, which the game's Photon servers cap at four players, and Replay.

See the [design](docs/design.md), [the suite plan](docs/superpowers/plans/2026-10-20-mod-suite.md),
[loading evidence](docs/loading-investigation.md) and [outstanding findings](FINDINGS.md)
for implementation and verification limits.
