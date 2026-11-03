# Lobbies, saves and players

The loader owns every question of who runs what. This guide lists what it guarantees, so your mod can rely on it and
check nothing itself. The rules in full are in the [SDK's README](../README.md), "The lobby's rules".

## Who runs your mod

- **Mods run only in single player and private lobbies.** In a public lobby every mod is off for that room, the
  host's too, and nothing is sent.
- **The host's list is the lobby's list.** Every player in a lobby needs every mod the host runs, in the same build.
  A joiner without one, or with another build, is shown the lobby's full list and removed; a player who runs no mods
  is removed too. A joiner's extra mods are off for that room, with a notice. Nobody changes the list from inside a
  room; the Mods panel's changes wait until the player is out of it.
- **A contract only begins once every player's mods are confirmed.** On Start the host closes the room, waits for the
  server to confirm it, reads the room again, and then starts, or reopens with the game's popup naming who it waits
  for. Nobody joins late. So everyone in a contract runs your mod, in your build, and agreed to the consent notice.
- **Under a vanilla host** every mod is off for that lobby, with a notice, and back on after leaving.

So a mod never asks "does every player run me?". `room_agrees` and `peer_has_mod` stay only so older mods load, and
`tests/raw_scan_test.cpp` fails on either in the bundled mods.

## When your mod is started and stopped

The loader starts a mod after the player agrees to the consent notice, dependencies first, and stops it in reverse.
The lobby rules above stop and start it again (`pmmod_stop`, hooks and receivers removed, content withdrawn, then
`pmmod_start`), but only out of a room or on the lobby's screen, never during a contract. Rules attached through the
content layer never start partway through a contract and run until it ends.

## Talking to other players

Send on your mod's own channel (`send`, `set_receiver`, at most 511 bytes a message). A message reaches only verified
players and comes only from one, so a send before then is refused, not queued: a mod that announces something repeats
it. The sender is the Photon actor number from the event, never from your payload; check it for anything only the
master may send. What you put in your messages is your own business.

Never send a game RPC by name or call an RPC's body. The game checks who sent an RPC, and a refusal can harm a
player's account ([Photon](../systems/photon.md), "The game's checks on an RPC").

## Saves

- **Custom content never reaches a save file or a player without it.** The host keeps each custom difficulty's ghost
  pick by id in its own file (`%LOCALAPPDATA%\PhasmoMods\custom_difficulty.txt`), and the game's own save and sync
  hold Random in its place whenever content is not active ([difficulty](../systems/difficulty.md)).
- **Keep your own data in your own files.** Never write a custom value into the game's save. Kinetic Games allows mods
  in private lobbies as long as they don't add excessive amounts of in-game currency or collectables, which would break
  progression.

## Player-facing text and windows

- Everything a player reads comes from your string table (`i18n/default.json`), never from code.
- Show your mod on surfaces the game already has: journal pages, lobby boards, the truck monitor, copies of the game's
  own widgets, and the game's popup through `pm::mod::show_message`. Never draw a window of your own.
