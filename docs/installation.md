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
- .NET and administrator access are not required. The installer puts PhasmoMods Loader
  in `%LOCALAPPDATA%\PhasmoModsLoader`, outside the game's installation folder.

## The notice

When the game reaches its main menu, a notice pinned to the board asks you first. It
explains the ban risk, that a mod is a program that runs on your computer, voice
recording (when a mod that records voices is on), and to back up your saves, and names every
installed mod. No mod has
been loaded yet: nothing of theirs runs until you answer. **I agree** loads and starts
the ticked mods, and is remembered until the notice or the game's version changes.
**Play without mods** loads none and takes the loader's hook back out of the game, and the notice asks again the next
time you start.

## When the game updates

You don't need a new release of the mods for the game to update.

The first time you start a version of the game these mods haven't been checked
against, the notice asks again, with a paragraph saying so. The mods can't tell whether
the update changed the anti-cheat, so playing then carries more risk than usual. It
asks once for each new version.

If you agree, every mod that still finds everything it needs in the game runs. Each
one that doesn't stays off, and its row in the **Mods** panel says what it lacks. A mod
that needs one of those stays off too. Your choice of mods is kept, so they come back
on their own when a later release or game update fixes them.

Some updates leave the mods unable to run at all. If the game moves to another Unity or
metadata version, the loader's own layouts no longer fit it: the loader stays out of the
game and hooks nothing, there is no notice and no **Mods** button, the game plays without
mods, and `mods.log` names both versions. If the game changes what the
lobby's rules need, no mod starts, and `mods.log` names what is missing. If the notice
itself can't be drawn, the game runs without mods. In both cases the **Mods** panel
says why when the game still lets it open. If there is no **Mods** button, `mods.log`
says why. The game itself plays on without mods.

## Switching mods in the game

The **Mods** tape on the main menu lists every installed mod and whether it is on.
Click a mod to switch it, then **Apply**: the mods restart with your choice and the
game keeps running. PhasmoMods Loader remembers the same choice. Mods cannot be changed
while you are in a lobby.

## Lobbies with mods

Mods only run in private lobbies and single player. In a public lobby every mod switches
off, the host's too, with a note saying so, and comes back on when you leave.

**The host's mods are the lobby's mods.** Every player in the lobby needs the same mods
as the host, in the same versions. Nothing a player who joins does changes what anyone
else runs.

- **You join with exactly the host's mods:** you play with them as usual.
- **You're missing one of the host's mods, or have another version of one:** you leave
  the lobby, and a note lists every mod the lobby needs, with yours that don't match
  first, highlighted, and what's wrong with each.
- **You have extra mods the host doesn't run:** you stay, your extra mods switch off for
  that lobby with a note naming them, and they come back on when you leave.
- **The host doesn't run PhasmoMods:** all your mods switch off for that lobby, with a
  note, and come back on when you leave.
- **A player without PhasmoMods joins a modded lobby:** the host removes them with the
  game's own kick, within about ten seconds.

**Starting a contract.** When the host presses Start, the contract waits until every
player's mods are confirmed. If someone is still being checked, a note names them and
the host presses Start again once they are. As the contract starts, the lobby closes
before anyone else can join, so everyone in a contract has the same mods. Nobody is
kicked because the host pressed Start.

## Mods that need other mods

Some mods need another mod to work: Custom Ghosts needs Spectral Flame. Switching a mod
on in the **Mods** panel switches on what it needs, and switching one off switches off
the mods that need it. If a mod can't run, every mod that needs it stays off too, and its
row in the panel says why.

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
PhasmoMods Loader waits for the menu, and the notice appears there. The program shows
the exact text to copy. It has this form:

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

## Development builds

PhasmoMods Loader has a checkbox for development builds, which are newer and less tested than a release, with the same ban risk.
Ticking it installs the latest development build and restarts. Each time the program starts after that, it moves to the newest
one. Unticking it moves you back to the latest release. The box can't be changed once mods are loaded into the game.

Everyone in a lobby needs the same build, so a player on a development build can only play with others on that same build. After
a new one comes out, restart PhasmoMods Loader before joining.

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

## Troubleshooting

- **Game not found:** launch through Steam and wait for the menu before attaching.
- **More than one game found:** close extra instances and try again.
- **Already loaded:** use the **Mods** button in the game to change mods; restart
  the game to load another version.
- **No mods were found:** run Setup again to repair the installation.
- **A mod's row says "This game build lacks...":** a game update changed something
  that mod needs. It stays off until a release or game update fixes it, and your other
  mods carry on.
- **The notice never appears and no mod runs:** read `mods.log`. Lines starting
  `consent:` say why the notice could not be shown, and `ModHost inactive:` says when the
  game moved to an engine the loader can't run on. If neither is there, the notice is still
  waiting for the main menu to come into view, and the game plays without mods until it does.
- **Access denied:** run Steam and PhasmoMods Loader as the same Windows user with
  matching privilege levels. Normal operation does not require elevation.
- **Menu or activation timeout:** inspect the logs and restart before retrying.
  A loaded DLL is not proof that gameplay initialization succeeded.

Native diagnostics are in `%LOCALAPPDATA%\PhasmoMods\mods.log`. PhasmoMods Loader
checks fresh entries for the selected process, including `ModHost active`; once you
agree, each mod logs its own load and start. Steam wrapper game logs are written under
`%LOCALAPPDATA%\PhasmoMods\logs` unless you supplied an explicit Unity
`-logFile` argument. Preserve that file when reporting a startup failure. Review
logs before sharing them; game logs can contain account identifiers.
