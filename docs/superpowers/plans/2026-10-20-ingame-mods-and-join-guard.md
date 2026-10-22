# In-game mod list, join guard, themed consent notice

Requested 2026-10-20, after the first multiplayer test. Decisions are in the user's own words in the
session; summarised here as built.

## What the player gets

- **Mods button** on the main menu (a copy of the game's own "News" tape button, which the game keeps
  switched off), opening a **Mods panel**: every installed mod with its description and whether it
  is on. Clicking a row switches it; **Apply** restarts the mods with the new choice, without
  restarting the game. Changes are refused while in a room ("Leave the lobby to change mods"). The
  choice is saved in launcher.ini, the file the launcher already reads.
- **Consent notice** drawn as the game's own error popup (taped title, pinned lined paper, the
  game's button) over the main menu, instead of the mod's own dark overlay. It names every installed
  mod, since any of them can be switched on later. If the game's popup cannot be found, the old
  overlay is used so the player is still asked.
- **Join guard**, in private rooms only:
  - A modded player joining a room whose host runs a different set of mods (or different builds of
    them) leaves the lobby the game's own way (ServerManager.LeaveServer) and, once out of the room,
    sees a note in the game's popup style naming what differs.
  - The host removes, with the game's own kick (ServerManager.KickPlayer), a modded player whose
    mods still differ a few seconds after joining (their own leave did not start), and a player
    without the mod (or on an older mod suite, whose presence format is older) when any of the
    host's running mods cannot fall back to vanilla. Those players see the game's own kick message;
    nothing custom can be shown to a client without the mod. A leave or kick the game could not
    carry out is retried.
  - A mod declares that vanilla players can share its lobby with the new flag
    PG_MOD_VANILLA_PEERS (Afterlife, Replay, Playable Ghost, Custom Ghosts, which keeps contracts
    vanilla unless everyone runs it): every current mod. Bigger Lobbies, which did not, was archived.
  - Mods can be changed only outside a room: a change inside a lobby would at once make this client
    mismatched with its host (or, as the host, its players with it).

## How

1. **Presence v2** (host/presence.h): each announced mod carries an 8-byte build id (the first bytes
   of the SHA-256 of its DLL).
   **JoinGuard** (host/join_guard.h): pure logic deciding, from the room's actors, the master, the
   peers' announcements and the clock, whether this client must leave (and why) and which actors the
   master must kick. Actors are judged only in the lobby, after a grace period from when they were
   first seen, and an actor once heard stays heard until it leaves (a contract's load is silent).
2. **Runtime mod control** (host/mods.cpp, host/suite.cpp, host/hooks.cpp): every installed mod is
   loaded and pinned at start; launcher.ini says which start. Applying a new choice stops every
   running mod (newest first), removes every hook the mods created (hooks::rollback_to a mark taken
   before the first mod started, newest first, so chained detours unwind in order), clears their
   network receivers and services, then starts the chosen mods in order. Runs on the main thread from
   the host's own tick, where no mod detour is on the stack.
3. **Launcher** (tools/launcher.cpp): the session file lists every installed mod; the ticks only seed
   launcher.ini. Its text points to the in-game Mods button.
4. **Menu panel** (host/menu_panel.cpp): a copy of ErrorManager's own popup (ErrorManager+0x20,
   ErrorParent) beside the original, with its localisers and UIButtons switched off (their listeners
   would drive the game's error queue) and pointer hit tests done by the mod. Title, body, rows and
   buttons; shared by the consent notice and the Mods panel.
5. **Message for the joiner**: a menu panel ("OK") once out of the room, where it survives the
   lobby; the mod's overlay when the menu's popup cannot be copied.

## Checks

Offline: presence v2 and JoinGuard tests (mismatch, unannounced after grace, vanilla allowed or not,
silent load, master migration), hooks rollback_to, mods selection and apply order, launcher session
listing, consent notice naming installed mods. In game (FINDINGS): the Mods button and panel, Apply
in the menu, the consent popup, a mismatched join, a vanilla join.
