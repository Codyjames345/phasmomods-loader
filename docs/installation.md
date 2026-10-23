# Phasmophobia mods

A set of mods for private Phasmophobia lobbies, started by one launcher. Tick the
mods you want in the launcher; every player in the lobby needs the same package
and the same mods ticked.

This is a development build. Native activation has been observed, but a complete
two-client contract has not been verified. See the source project's FINDINGS.md
for outstanding gameplay checks.

## The mods

| Mod | What it does |
| --- | --- |
| Playable Ghost | One player in a private lobby plays the ghost. The game's own rules decide its evidence, abilities and hunts. |
| Spectral Flame | An eighth evidence. A candle, lantern or candelabra left burning where the ghost spends its time slowly turns blue. A new row on the journal's evidence page. |
| Custom Ghosts | Fifteen new ghost types, each with its own journal page, evidence and tells: the Echo, Dullahan, Kikimora, Draugr and Black Shuck, and ten that carry Spectral Flame (Lithobolia, Gremlin, Brownie, Bogeyman, Lethe, Mesmer, Knell, Asterion, Tempest, Hydra). Needs Spectral Flame, which switches on with it. |
| Ghost Tells | Three new tells each for the Spirit, Mare, Shade, Yurei, Goryo and Onryo, so they can be told apart by how they behave. Their journal pages name the new tells. |
| Afterlife | A dead investigator holds Crouch to watch through the ghost's eyes, and cannot touch anything while watching or for a minute after. |

Playing the ghost: Interact, Primary Use (tap: an event; hold: a hunt), Secondary Use (the type's
ability), the torch key (the fuse box) and Crouch held while tapping Primary Use (an event in an
investigator's form). As the Mimic, Crouch on its own picks the next type to copy.

## Requirements

- Windows x64 and a Steam installation of Phasmophobia.
- The audited game binaries from Steam build 25811160. The host checks both game
  library hashes and stays inactive on an unsupported update.
- Extract the whole package, with its `mods` folder, outside the game's
  installation folder. .NET and administrator access are not required.

## The notice

When the mods load, a notice pinned to the main menu's board asks you first. It
explains the ban risk, voice recording (when a mod that records voices is on), and
that your progress is at your own risk, and names every installed mod. **I agree**
starts the ticked mods and is remembered until the notice changes. **Play without
mods** keeps the game unmodified until you restart it.

## Switching mods in the game

The **Mods** tape on the main menu lists every installed mod and whether it is on.
Click a mod to switch it, then **Apply**: the mods restart with your choice and the
game keeps running. The launcher remembers the same choice. Mods cannot be changed
while you are in a lobby.

## Lobbies with mods

A lobby whose host runs mods admits only players running the same mods, in the
same versions. If yours differ, you leave the lobby and a note on the board says
which mods differ. Players without the mod may stay when every mod the host runs
works without them, as every current mod does. A mod that needs everyone to have
it makes the host remove players without it, with the game's own kick.

## Install and launch

1. Extract `PhasmoMods.zip` to a permanent folder outside the game directory.
2. Open `PhasmoMods Loader.exe` and tick the mods to start. Your choice is saved, and
   you can change it later from the **Mods** button in the game.
3. Start Phasmophobia through Steam, wait for the main menu, then choose
   **Attach to running game** in the launcher.
4. Answer the notice in the game.
5. Create a private lobby with friends who use the same package and mods.

To load automatically instead, copy the launcher's Steam launch option into
Phasmophobia's **Properties > General > Launch Options**, keeping the quotes and
`%command%` exactly as supplied, and start the game with Steam's **Play** button.
The launcher waits for the menu and loads the ticked mods. The option has this
form, with the path to your extracted launcher:

```text
"<folder>\PhasmoMods Loader.exe" --steam %command%
```

Restart the game before loading another version or trying again after a failed
initialization. To change which mods run, use the **Mods** button in the game.

## Updates and removal

Close Phasmophobia and the launcher before replacing the extracted files. If you
move the folder, copy the launch option again so Steam uses the correct path.
Remove the launch option to launch without mods. No game file is replaced.

Game updates require an audit of native bindings and tests before the supported
hashes can change. The build check prevents using known offsets with unknown
binaries; it does not guarantee compatibility with future updates.

## Troubleshooting

- **Game not found:** launch through Steam and wait for the menu before attaching.
- **More than one game found:** close extra instances and try again.
- **Already loaded:** use the **Mods** button in the game to change mods; restart
  the game to load another version.
- **No mods were found:** extract the complete package, with its `mods` folder,
  beside the launcher.
- **Unsupported build:** use a release audited for your installed game build.
- **Access denied:** run Steam and the launcher as the same Windows user with
  matching privilege levels. Normal operation does not require elevation.
- **Menu or activation timeout:** inspect the logs and restart before retrying.
  A loaded DLL is not proof that gameplay initialization succeeded.

Native diagnostics are in `%LOCALAPPDATA%\PhasmoMods\mods.log`. The launcher
checks fresh entries for the selected process, including `ModHost active`; each
mod then logs its own start. Steam wrapper game logs are written under
`%LOCALAPPDATA%\PhasmoMods\logs` unless you supplied an explicit Unity
`-logFile` argument. Preserve that file when reporting a startup failure. Review
logs before sharing them; game logs can contain account identifiers.

## Build and test from source

Install Visual Studio's C++ x64 tools, CMake, Ninja and PowerShell. From the
source folder, run:

```powershell
.\validate.ps1
.\package.ps1
```

Validation builds native targets and runs all native and packaging tests.
Packaging validates first and publishes `dist/PhasmoMods.zip`, containing the
launcher, the host, each mod in its own folder under `mods`, this README, an
artifact hash manifest and the MinHook license. The manifest does not claim
complete gameplay verification.

Every build also replaces `dist/PhasmoMods` with that payload, unpacked, so the
launcher there runs the build just made. Close it before building.
