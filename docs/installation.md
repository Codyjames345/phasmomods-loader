# Phasmophobia mods

A set of mods for private Phasmophobia lobbies, started by one program, PhasmoMods
Loader. Tick the mods you want in PhasmoMods Loader; every player in the lobby needs
the same mods ticked, in the same versions.

This is a development build. Native activation has been observed, but a complete
two-client contract has not been verified. See the source project's FINDINGS.md
for outstanding gameplay checks.

Modified clients can get your account banned. Use these mods at your own risk, and
only in private lobbies with friends who have agreed to them.

## The mods

| Mod | What it does |
| --- | --- |
| Playable Ghost | One player in a private lobby plays the ghost. The game's own rules decide its evidence, abilities and hunts. |
| Spectral Flame | An eighth evidence. A candle, lantern or candelabra left burning where the ghost spends its time slowly turns blue. A new row on the journal's evidence page. |
| Custom Ghosts | Fifteen new ghost types, each with its own journal page, evidence and tells: the Echo, Dullahan, Kikimora, Draugr and Black Shuck, and ten that carry Spectral Flame (Lithobolia, Gremlin, Brownie, Bogeyman, Lethe, Mesmer, Knell, Asterion, Tempest, Hydra). Needs Spectral Flame, which switches on with it. |
| Ghost Tells | Three new tells each for the Spirit, Mare, Shade, Yurei, Goryo and Onryo, so they can be told apart by how they behave. Their journal pages name the new tells. |

Playing the ghost: Interact, Primary Use (tap: an event; hold: a hunt), Secondary Use (the type's
ability), the torch key (the fuse box) and Crouch held while tapping Primary Use (an event in an
investigator's form). As the Mimic, Crouch on its own picks the next type to copy.

## Requirements

- Windows x64 and a Steam installation of Phasmophobia.
- The audited game binaries from Steam build 25811160. The mods check both game
  library hashes and stay inactive on an unsupported update.
- .NET and administrator access are not required. The installer puts PhasmoMods Loader
  in `%LOCALAPPDATA%\PhasmoModsLoader`, outside the game's installation folder.

## The notice

When the mods load, a notice pinned to the main menu's board asks you first. It
explains the ban risk, voice recording (when a mod that records voices is on), and
that your progress is at your own risk, and names every installed mod. **I agree**
starts the ticked mods and is remembered until the notice changes. **Play without
mods** keeps the game unmodified until you restart it.

## Switching mods in the game

The **Mods** tape on the main menu lists every installed mod and whether it is on.
Click a mod to switch it, then **Apply**: the mods restart with your choice and the
game keeps running. PhasmoMods Loader remembers the same choice. Mods cannot be changed
while you are in a lobby.

## Lobbies with mods

A lobby whose host runs mods admits only players running the same mods, in the
same versions. If yours differ, you leave the lobby and a note on the board says
which mods differ. Players without the mod may stay when every mod the host runs
works without them, as every current mod does. A mod that needs everyone to have
it makes the host remove players without it, with the game's own kick.

## Install

1. Download `PhasmoModsLoader-win-Setup.exe` from the latest release and run it.
2. Windows SmartScreen shows "Windows protected your PC" the first time, because the
   release is not code signed. Choose **More info**, then **Run anyway**. This
   happens once; updates install without the warning.
3. Setup installs PhasmoMods Loader and opens it. Tick the mods to start. Your
   choice is saved, and you can change it later from the **Mods** button in the game.
4. Start Phasmophobia through Steam, wait for the main menu, then choose
   **Attach to running game** in PhasmoMods Loader.
5. Answer the notice in the game.
6. Create a private lobby with friends who use the same mods.

To load automatically instead, copy PhasmoMods Loader's Steam launch option into
Phasmophobia's **Properties > General > Launch Options**, keeping the quotes and
`%command%` exactly as shown, and start the game with Steam's **Play** button.
PhasmoMods Loader waits for the menu and loads the ticked mods. The program shows the
exact text to copy. It has this form:

```text
"<install folder>\current\PhasmoMods Loader.exe" --steam %command%
```

Restart the game before loading another version or trying again after a failed
initialization. To change which mods run, use the **Mods** button in the game.

## Updates

PhasmoMods Loader checks GitHub for the latest release each time it starts, waits at
most ten seconds, and then carries on with the version you have. When the latest
release differs from yours, newer or older, it installs that release and restarts
itself, with the same Steam launch option. A release that is taken down therefore
moves everyone back to the one before it. With no network, it starts the installed
version unchanged.

## Moving from the portable folder

Earlier builds came as a ZIP that you extracted to a folder.

1. Run `PhasmoModsLoader-win-Setup.exe`.
2. Open the installed PhasmoMods Loader and copy the Steam launch option it shows.
   In Steam, open Phasmophobia's **Properties > General > Launch Options** and paste it
   over the old one.
3. Delete the old extracted folder.

Your settings carry over: the ticked mods, your agreement to the notice and the logs
live in `%LOCALAPPDATA%\PhasmoMods`, which neither Setup nor an update touches.

## Removal

Close Phasmophobia and PhasmoMods Loader, then uninstall **PhasmoMods Loader** from
Windows **Settings > Apps**. Remove the Steam launch option to launch without mods.
Your settings stay in `%LOCALAPPDATA%\PhasmoMods` until you delete that folder. No
game file is replaced.

Game updates require an audit of native bindings and tests before the supported
hashes can change. The build check prevents using known offsets with unknown
binaries; it does not guarantee compatibility with future updates.

## Troubleshooting

- **Game not found:** launch through Steam and wait for the menu before attaching.
- **More than one game found:** close extra instances and try again.
- **Already loaded:** use the **Mods** button in the game to change mods; restart
  the game to load another version.
- **No mods were found:** run Setup again to repair the installation.
- **Unsupported build:** use a release audited for your installed game build.
- **Access denied:** run Steam and PhasmoMods Loader as the same Windows user with
  matching privilege levels. Normal operation does not require elevation.
- **Menu or activation timeout:** inspect the logs and restart before retrying.
  A loaded DLL is not proof that gameplay initialization succeeded.

Native diagnostics are in `%LOCALAPPDATA%\PhasmoMods\mods.log`. PhasmoMods Loader
checks fresh entries for the selected process, including `ModHost active`; each
mod then logs its own start. Steam wrapper game logs are written under
`%LOCALAPPDATA%\PhasmoMods\logs` unless you supplied an explicit Unity
`-logFile` argument. Preserve that file when reporting a startup failure. Review
logs before sharing them; game logs can contain account identifiers.
