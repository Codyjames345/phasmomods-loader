# The custom difficulty: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [The custom difficulty](../systems/difficulty.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## EasySave3

- The game's EasySave3 settings (the `ES3/ES3Defaults` asset in `resources.assets`) give the location 4, the
  cache. `ES3.Save<T>(key, value)` with the cache goes to `ES3File.Save<T>`, which serialises the value at once
  with `ES3.Serialize<T>`, the same code the lobby sync calls. Both are shared generic code, so they have no map
  entry.
- Every write of a `Difficulty` goes through its EasySave3 type, `ES3UserType_Difficulty`:
  `WriteScriptableObject` writes each field, `ghostTypeOverride` as a boxed `GhostType`.
  The host's hook on it is the one place a save or a sync could take a custom type.
- A load or deserialisation of a ScriptableObject (`ES3ScriptableObjectType.ReadObject`) reads the saved
  `_ES3Ref` id; when the reference manager knows a live object of that id it reads the fields into that object
  and returns it, else it makes one with `CreateInstance` and registers it. So a load can write a saved Random
  into a live difficulty, and the lobby sync on the client that sent it reads into its own object.

## What the host does

- While content is active the menu's collection gets the custom ghosts (`ContentTypes::offer`, the
  contract collection's placement) and the option's count follows it.
- Picks are learnt from the handler (after it) for a custom preset (a value the presets array holds),
  by ghost id, in `custom_difficulty.txt` ("<preset>=<ghost id>"). A file of the older form, keyed by
  type number, has those lines dropped (logged) when a pick is next kept.
- Every frame in the lobby while content is active, a difficulty at None for a custom preset with a
  pick gets the pick: the lobby's difficulty, its settled copy and `LevelValues.difficulty` (the
  master's only), then the presets. The host sends the lobby's difficulty (`ForceSyncDifficulty`, or
  `LevelValues.difficulty` when the menu has none) when one of them changed, and once each time content
  turns active, since a player who joined was sent Random.
- The hook on `WriteScriptableObject` writes None for a custom type, except in the host's own sends
  while content is active and everyone in Photon's player list now is verified (a player who joined since
  waits for the next send). So saves never hold one, and neither does the game's own sync
  (`ApplyDifficulty`'s) or the join sender's; the host sends the pick once everyone is verified.
  Sending stays the master's alone. While a player is being verified the host's own view does not change.
- Content not active (off, or pending: the host not heard from yet, or this client's mods not matching
  it): every custom type in the presets, the
  lobby's difficulty, its copy and `LevelValues.difficulty` of the menu that was found becomes None
  (offered to that menu or not), the custom ghosts leave the menu's collection (the lobby's offer ends,
  `ContentTypes`), and the master sends Random. A contract that loaded then would play none of them,
  so the menu offers none. Each pick comes back once content is active again, unless the player
  changes the preset's ghost meanwhile (the menu then offers the game's ghosts only): that choice
  replaces the pick, as under Off.
- In a contract the content side has decided to keep vanilla, `LevelValues.difficulty` loses a custom
  type before the spawn reads it.
- Inside the summary's builder, whichever method called it, `Enum.ToString` of a `GhostType` box
  holding a custom type answers the ghost's name in the game's language.

## Why a pick was lost after a contract (2026-10-25)

Custom Ghosts' `difficulty.cpp` gave each custom preset at Random its pick, then the lobby's difficulty, and
sent only when the lobby's difficulty took its pick. Once the player pressed the custom preset's save button,
the lobby's difficulty was the preset object itself, and `ApplyDifficulty` saved "LastDifficulty" from it, with
the preset's reference. After a contract the menu scene loads again: `Awake` loads the presets (new objects,
Random from the save, each registered under its saved reference), and the board's one-shot visible handler loads
"LastDifficulty"; `ReadObject` returns the very preset object, so the lobby's difficulty is `presets[i]`.
`ApplyDifficulty` then syncs it, and `LevelValues.difficulty` becomes a Random copy. On the mod's next tick the
preset loop gave `presets[i]` its pick, which was the lobby's difficulty too, so the lobby's own check found no
Random and nothing was sent: `LevelValues.difficulty` stayed Random and the spawn rolled. The host checks the
lobby's difficulty and `LevelValues.difficulty` before the presets, and sends whenever either changes.
