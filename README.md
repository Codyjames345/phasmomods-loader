# PhasmoMods Loader

Native mods for private Phasmophobia lobbies, started together by one launcher.
`ModHost.dll` verifies the game build, asks every player to agree to a notice
(ban risk, voice recording, progress at their own risk), then starts the mods
ticked in `ModLauncher.exe`. The **Mods** button on the game's main menu switches
mods on and off without restarting, and a modded lobby admits only players with
the same mods. Each mod is its own DLL under `mods/`:

| Mod | Source | What it does |
| --- | --- | --- |
| Playable Ghost | `mods/playable_ghost` | One player plays the ghost; the game supplies speeds, evidence, abilities and hunt rules. |
| Spectral Flame | `mods/spectral_flame` | An eighth evidence: a candle, lantern or candelabra burns blue where the ghost lingers ([details](docs/spectral-flame.md)). |
| Custom Ghosts | `mods/custom_ghosts` | Fifteen new ghost types with their own evidence, journal pages and rules; needs Spectral Flame ([details](docs/custom-ghosts.md)). |

`archive/` holds mods that are kept but not built. Bigger Lobbies is there because the game's
Photon servers cap a room at four players. Replay is there with an approved plan to film each
round's top moments for the lobby truck's monitor, for another time. Afterlife, which let
dead investigators watch through the ghost's eyes, is kept there too.

`core/` is shared by the host and every mod (IL2CPP access, game bindings, UI
built from game classes, and the host contract in `core/mod_api.h`); `host/` is
the host; `tools/` holds the launcher and loader. This is a development build;
full multiplayer round flow remains unverified.

Requires Windows x64, Steam Phasmophobia with the audited game libraries, and the
same package and mods on every client. Source builds use Visual Studio C++ x64
tools, CMake, Ninja and PowerShell. MinHook 1.3.4 is vendored in `third_party/minhook`.

See [installation instructions](docs/installation.md) to use a packaged release.
From this source directory, build and package with:

```powershell
.\package.ps1
```

The release is `dist/PhasmoMods.zip`. Extract it outside the game folder.

Run the complete local validation with:

```powershell
.\validate.ps1
```

The command builds native targets and runs native and packaging tests.
See [design](docs/design.md), [the suite plan](docs/superpowers/plans/2026-10-20-mod-suite.md),
[loading evidence](docs/loading-investigation.md) and [outstanding findings](FINDINGS.md)
for implementation and verification limits.
