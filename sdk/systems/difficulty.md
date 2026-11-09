# The custom difficulty

How the game's difficulty menu, its presets, the lobby sync and the contract's difficulty work, and how the host
offers custom ghosts in the Ghost Type option (`host/content_difficulty.h`). The entries are in
`sdk/map/Difficulty.h` (and `Object.FindObjectOfType` in `sdk/map/UnityObject.h`). The notes were read from
build 25811160; the map's entries find each member again on any build.

## The objects

- A `Difficulty` is a ScriptableObject. The game makes them with `ScriptableObject.CreateInstance` (the
  constructor writes `ghostTypeOverride` = None) or `Clone` (which copies every field, `ghostTypeOverride` and
  `difficulty` included). Nothing destroys them with a scene, so a difficulty outlives the menu that made it.
- `Difficulty.difficulty` says which preset it is; `ghostTypeOverride` is the forced ghost type, None
  (24 in this build) for Random. A scan of the whole code for an immediate store of None into
  `ghostTypeOverride` finds two: the constructor and the Ghost Type option's change handler.
- The menu, `DifficultyUIManager`, is on the "Difficulty Selection" board of the menu scene (one in `Main_Menu`,
  by the scene files). The game keeps the board inactive unless the player is on it, and makes the menu again with
  every load of the menu scene. Its members:
  - the Ghost Type option (`DifficultyUIManager.ghostType`);
  - the `GhostCollection` the option offers (`DifficultyUIManager.allGhostsData`);
  - the custom presets (`DifficultyUIManager.presets`, a `Difficulty[]`);
  - the lobby's difficulty (`DifficultyUIManager.current`) and its settled copy (`DifficultyUIManager.applied`).
- `LevelValues` (its `Awake`) keeps itself across scenes with `DontDestroyOnLoad`; a later copy
  (the menu scene has one) destroys itself. Its `difficulty` (`LevelValues.difficulty`) is the difficulty the
  next contract plays, so it survives the return to the lobby.

## Loading, choosing, applying and syncing

- The menu's `Awake` makes the presets array and loads each preset from the save,
  `ES3.Load<Difficulty>("CustomDifficulty{i}", amateur.Clone())`, numbering them 6, 7 and 8. It does not set the
  lobby's difficulty. When `LevelValues.difficulty` is null it becomes the amateur preset.
- `Start` subscribes a one-shot handler to the board's `UIView` visible event (an event of the board's
  UIContainer). The handler runs once and removes itself. As the master in a room, it loads the lobby's
  difficulty, `ES3.Load("LastDifficulty", amateur.Clone())`, then runs `ApplyDifficulty`.
- For Custom 1 to 3 (cases 6 to 8), the preset choice makes the lobby's difficulty a copy of the preset
  (`presets[i].Clone(false)`, a new object), then refreshes the display. It applies nothing. Only the custom
  preset's save button makes the lobby's difficulty a preset object: it saves it under "CustomDifficulty{i}" and
  puts that very object into the presets array.
- `ApplyDifficulty` saves the lobby's difficulty, `ES3.Save("LastDifficulty", d)`. Then it sends it to every
  player, this one included, with the `SyncDifficultyValues` RPC (`ES3.Serialize`, then
  `PhotonView.RPC(name, RpcTarget.All)`). `ForceSyncDifficulty(d)` sends any difficulty the same way, and
  nothing in the game calls it. When a player joins, the master sends `LevelValues.difficulty` to that player
  only.
- `SyncDifficultyValues` sets three things, then refreshes the display: the lobby's difficulty becomes
  `ES3.Deserialize(bytes)`, its settled copy becomes `Clone(true)` of it, and `LevelValues.difficulty` becomes
  that copy. Each client also saves the settled copy's `GetRewardMultiplier` (ES3.Save of a float). With the
  host's resend ([the maintainers' page](../internals/difficulty.md)), a client therefore saves it twice per
  apply: first for Random, then 0 for the custom pick.
- The contract's spawn (`GhostController`'s type roll, master only) replaces its rolled type with
  `LevelValues.difficulty.ghostTypeOverride` when that is not None. `GetRewardMultiplier` gives 0 for a forced
  type.

## The Ghost Type option

- Its change handler is `DifficultyUIManager.GhostTypeChosen`. Index 0 writes None to the lobby difficulty's
  `ghostTypeOverride` and names it "Random" (`GetLocalisedValue`). Index i writes the type of
  `allGhostsData.ghosts[i - 1]` and names it through `GhostData.GetGhostData`. Two never-called look-alikes
  write other constants and call obfuscated localisation methods.
- The handler runs from the option's arrows and dice. The display's refresh writes the index alone.
- The option's count is set in the scene, never by code. `ButtonCheck` sets the arrows from the index and the
  count, and `Next` (`DifficultyOption.Next`) moves the index, clamped to the count.
- So the option's choices are Random and then every ghost of the menu's collection, in its order: the
  host sets the count to the collection's size plus one, and finds a ghost's index by its place there.

## The summary

The custom difficulty's summary is built by `DifficultyUIManager.SummaryText`. Its Ghost line is "Random"
(`GetLocalisedValue`) for None, else `Enum.ToString()` of `ghostTypeOverride` boxed on the stack as a
`GhostType`, which prints a type with no enum name as its number. Four methods call it. Two of them are:

- the summary panel's toggle (`DifficultyUIManager.ToggleSummary`). It flips the panel's shown flag, shows or
  hides the panel and refreshes the buttons (`DifficultyUIManager.RefreshButtons`). When the panel is now shown,
  it writes the summary of the lobby's difficulty into the panel's text. `ApplyDifficulty`, `SyncDifficultyValues`
  and the preset choice run the toggle while the panel is shown, which closes it. So what players read is the
  toggle's text when they open the panel;
- the display's refresh, through a helper, into the same text while the panel may be hidden.

The other two are obfuscated methods: one has no direct caller, and the other is called by two more obfuscated
methods and by the obfuscated copy of `SyncDifficultyValues` (itself never called). Since the host scopes its
answer to the builder, every caller gets the name either way.

An obfuscated copy of the builder, calling `Enum.ToString` too, is called only from methods nothing calls; the
entry tells them apart by the toggle calling the real one. `Enum.GetHashCode` gives an int enum's value.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/difficulty.md).
