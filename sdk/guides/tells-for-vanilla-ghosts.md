# Tells for vanilla ghosts

A mod can change how one of the game's own ghosts behaves without registering a new type: it attaches rules to the
vanilla type's id and adds lines to its journal page. Ghost Tells (`mods/ghost_tells/`) does this for six ghosts, and
this guide follows it. Read [adding a ghost](adding-a-ghost.md) for the rules themselves.

## Rules on a vanilla id

A vanilla ghost's id is `"vanilla."` and its `GhostType` value's name in lower case (`"vanilla.mare"`), read from the
game's enum at runtime, so a type a game update adds gets an id too. `pm::api::content::type_of("vanilla.mare")`
answers its number whenever the game API is bound.

```cpp
pm::api::content::GhostRules rules;
rules.user = &my_state;
rules.start = &on_start;          // the ghost begins acting as the Mare
rules.tick = &on_tick;            // every frame while it does, on every client
rules.stop = &on_stop;            // it stops: another type, the contract's end, the mods stopping
rules.heldBack = &held_back;      // whether the type's own rules forbid acting now
const bool ok = pm::api::content::attach_rules("vanilla.mare", rules) > 0;   // from pmmod_start
```

Ghost Tells registers one set of functions for all six ghosts, with `user` set to which ghost it is
(`mods/ghost_tells/register.h`). Several mods may attach rules to one type; each runs.

The rules run while the contract's ghost **acts as** that type, which is its behaviour type, so a Mimic copying the
Mare runs the Mare's rules for as long as the copy lasts, and the switch stops one set and starts the next in the same
call. The host sends the acting-as type to every client, so `tick` runs everywhere
([README, "The host contract, version 2.2"](../README.md#the-host-contract-version-22)). Rules never start partway
through a contract.

## Lines on its journal page

```cpp
pm::api::content::add_journal_lines("vanilla.mare", {pm::api::content::Text("en", pm::i18n::text("tells.mare"))});
```

Each line shows after the page's own text, in every journal, the lobby's included, in the player's language when the
line has it, else English. Ghost Tells adds one line per ghost naming its tells (`mods/ghost_tells/i18n/default.json`).

## Changing what the ghost does

Change the game through its own inputs and its own methods, so that everything else the game does follows. Ghost
Tells' ways of doing that:

- **Holding the ghost back.** Its gate hooks the game's own `GhostActivity.TryInteract` and `GhostAI.ChangeState` and
  refuses an interaction, event or hunt the tell forbids. It never refuses a forced change (the tarot cards) or a
  cursed possession's pull. The same rules answer `heldBack`, by kind (0 an interaction, 1 an event or its ability,
  2 a hunt, 3 its ability alone, 4 the fuse box, `core/mod_api.h`), so a player controlling the ghost in Playable
  Ghost is held back by the same tell.
- **Holding a hunt still.** Through the rules' `huntSpeed`, which writes the hunt's base speed and line-of-sight
  speed-up as the game's inputs ([ghost AI, "Hunt speed"](../systems/ghost-ai.md#hunt-speed)). Ghost Tells keeps
  `huntSpeed` only on the two ghosts whose tells change hunt speed.
- **Answering one of the game's rolls.** A tell that changes a roll's odds (the Mare's light switch-off) marks the
  game's own call with a flag around it and answers `Random.RandomRangeInt` only while the flag is set. Never tell a
  caller apart by its return address: other mods' detours chain there. For the methods the host tracks,
  `pm::api::inside<M>()` answers whether a call is running ([README, "Hooking and caller context"](../README.md#hooking-and-caller-context)).

Every rule of a tell is proven from the game's code and cited by map key in its notes; `docs/ghost-tells.md` lists the
methods each meets.

## Rules the bundled tells follow

- A tell runs on every contract, with or without evidence, never on zero-evidence contracts alone.
- A tell keeps the game's mechanics and overlaps no other ghost's tell, vanilla or custom.
- The ghost's AI and a player controlling it follow the same rules (`sdk/systems/ghost-ai.md`, "A player and its AI").
- The host runs the rules and shows the lines only while this client's mods match the host's list. The mod keeps no
  check of its own on the other players.
