# Ghost types and the Mimic: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [Ghost types and the Mimic](../systems/ghost-types.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## What is mapped

| Key | Kind | Found by |
| --- | --- | --- |
| `EvidenceType` | top-level enum (None 0 to DotsProjector 7) | shape: `values` |
| `EvidenceType.None` ... `.DotsProjector` | enum values, all eight | name |
| `GhostTraits` | struct | shape: `fieldTypes` two `List<@EvidenceType>`, a string, three bools |
| `GhostType` | enum nested in GhostTraits (Spirit 0 to Deildegast 30, None 24) | shape: owner and `values` |
| `GhostType.None`, `.Mimic` | enum values (24, 20) | name |
| `GhostInfo` | type | name |
| `GhostInfo.traits` | field, `@GhostTraits` | shape |
| `GhostInfo.identityType` | field within `GhostInfo.traits`, `@GhostType` | `sameFieldAs` `GhostController.ghostType` |
| `GhostInfo.behaviourType` | field within `GhostInfo.traits`, `@GhostType` | shape plus two cross-owner facts |
| `GhostInfo.SyncValues`, `.SyncValuesNetworked`, `.SyncEvidence` | methods (the last two Photon RPCs) | name |
| `GhostAI.info` | field, `@GhostInfo` | shape |
| `GhostAI.Init` | method, has an alias | name, params `@GhostTraits` |
| `GhostController` | type | name |
| `GhostController.instance` | static field | shape |
| `GhostController.traits` | field, `@GhostTraits` | shape |
| `GhostController.ghostType` | field within `GhostController.traits`, `@GhostType` | shape plus a fact |
| `GhostController.allGhostsCollection` | field | name |
| `GhostController.mimicCopies` | field, `List<@GhostType>` | shape |
| `GhostController.mimicNext` | field, `Nullable<@GhostType>` | shape |
| `GhostController.typeChangeDue` | field, bool | shape plus a fact |
| `GhostController.AttemptGhostTypeStateChange` | method | name |
| `GhostController.MimicCopy` | method, `void()` | shape plus two facts |
| `GhostController.BuildMimicCopies` | method, `void()` | shape plus a fact |
| `GhostCollection`, `.ghosts`, `.GetGhostOfType` | type, field, method | name |
| `GhostData`, `.type`, `.ghostNameLocalisationKey`, `.evidence1` to `.evidence3`, `.GetEvidenceList` | type, fields, method | name |
| `LevelStats`, `LevelStats.get_Instance` | type, static method | name |
| `EvidenceController`, `.ForceRespawnGhostOrb` | type, method | name |

The game API is `phasmo::EvidenceType`, `phasmo::GhostType`, `phasmo::GhostTraits`, `phasmo::GhostInfo`,
`phasmo::GhostController`, `phasmo::GhostCollection`, `phasmo::GhostData`, `phasmo::LevelStats` and
`phasmo::EvidenceController`, plus
`phasmo::GhostAI::info` and `phasmo::GhostAI::Init`. An unresolved enum value gives 0, which is a real
value of both enums (Spirit, and no evidence), so check `use::resolved()` before relying on one.

**The identity and the behaviour type** are the first two fields of `GhostTraits`, both of type
`GhostType`, so each is a field `within` the class's traits field (see `sdk/README.md`): it resolves to
its place in GhostInfo or GhostController, and its facts read that class's code. Shape alone cannot tell
the two apart, so:

- `GhostController.ghostType` (the identity): `AttemptGhostTypeStateChange` loads it through `this` three
  times (it compares the identity with the Mimic, the Thaye and the Obambo) and never touches the behaviour
  type. Without the fact the entry is `AMBIGUOUS`.
- `GhostInfo.behaviourType`: `EvidenceController.ForceRespawnGhostOrb` reads it (the Mimic orb test) and
  nothing at the identity's place, which decides. `GhostController.MimicCopy` stores it too, but it also
  stores at the identity's place elsewhere in its body, so alone that fact leaves both (`AMBIGUOUS`); it
  stays as a second check. Both facts are cross-owner (they reach GhostInfo through another object).
- `GhostInfo.identityType` has no decisive fact of its own. The one reader found that touches the identity
  alone is the end of contract's check of the journal guess against it: `ExitLevel`'s async state machine
  (its `MoveNext`) compares `JournalController.GetGhostType()` with
  `currentGhost.GhostInfo.traits.identity` and completes the "identify the ghost" objective. That class is
  compiler-generated, so no entry can name it. Every GhostInfo method read (`SyncValuesNetworked` writes
  one 8-byte value over both, `GhostAI.Init` and `StartGhost` copy the traits whole, the media getters
  touch neither), the roll (which writes GhostController's copy, not GhostInfo's) and
  `SyncValuesNetworked`'s journal title key (built from its argument, not the field) touch both or neither.
  But GhostInfo and GhostController embed the same struct type, and `GhostController.ghostType` is decided
  by a real fact, so the identity is found with **`sameFieldAs`**: the very GhostTraits field
  `GhostController.ghostType` resolved to, placed in GhostInfo's traits. It follows that entry: unresolved,
  it is `Blocked`; were the two classes to hold different struct types, it would be `Missing`.

## The Mimic

These are the answers to the content design's "Left to the builder" questions on the Mimic.

### Which field the game switches

`GhostInfo.behaviourType` (the behaviour type of the ghost's GhostInfo), on the host only. The identity
(`GhostInfo.identityType`) stays `Mimic`, and `GhostController.traits` is never changed by a copy.

- `GhostAI.ChangeState` acts only while a flag of the ghost's PhotonView is set, as on the
  master that runs the AI (elsewhere it logs an error), and calls
  `GhostController.AttemptGhostTypeStateChange` at every state change. `Update` also calls it for the
  Obambo.
- `AttemptGhostTypeStateChange` returns unless `typeChangeDue` is set or `mimicNext` has a
  value. It then clears `typeChangeDue` and branches on the controller's identity: the Mimic (20) jumps
  to `MimicCopy`, the Thaye (23) to its ageing body, the Obambo (27) toggles a flag of the ghost.
- `typeChangeDue` is set by `GhostController.Update` when the Mimic's (or the Thaye's) timer runs out,
  while a flag of the SetupPhaseController is set, and the timer restarts at a random length.
- `MimicCopy` (an obfuscated `void()` that Attempt tail-jumps to) takes `mimicNext`'s value
  when it has one and clears it, otherwise a random element of `mimicCopies` (building the list first
  when it is null), and stores it at `levelController.currentGhost.GhostInfo.traits.behaviour`. It then
  adds one to a LevelStats counter of copies, and starts the Thaye's age state or sets the Obambo's flag
  when it copied one of those.
- Nothing sends the copy to the other clients: their GhostInfo keeps the behaviour type
  `SyncValuesNetworked` gave it (the Mimic). The AI runs on the host, which is where the copy matters.

The entry is found as the one `void()` method of GhostController that `AttemptGhostTypeStateChange`
calls (it jumps to the Mimic's and the Thaye's bodies) and that calls `LevelStats.get_Instance` (only the
Mimic's does). Without the LevelStats fact both bodies match and the entry is `AMBIGUOUS`.

### Where its copy list comes from, and whether it is rebuilt

`GhostController.mimicCopies`, a `List<GhostType>`, built by `BuildMimicCopies`:

- It loops over every number below `Enum.GetNames(typeof(GhostType)).Length` and adds each one but
  `None` (24). The Mimic (20) itself is in the list.
- When a bool of the traits is set (the roll sets it from a controller dictionary for the Banshee and
  the Dayan, and at random for other types), a type the dictionary marks false is left out. `Awake`
  fills that dictionary with the Banshee (4) and the Dayan (26).
- It is built **lazily and once**: `MimicCopy` calls the builder only when `mimicCopies` is null, which
  is on the first copy that has no `mimicNext`. The controller lives for one contract, so the list is
  never rebuilt during it. Before that first copy the field is null.
- **A custom type is never in it**: the count of the enum's names stops at 31. To let the Mimic copy a
  custom ghost the host adds the custom types to the list itself (after `BuildMimicCopies` returns, or by
  building it earlier), and the game then picks among them like any other. `mimicNext` takes any number
  as well, so a choice made ahead (Playable Ghost's) may name a custom type.

### How the Mimic's forced orbs are given

They are not part of its evidence: the evidence pick treats the Mimic like any type, and its `GhostData`
lists its three evidence without orbs. The orbs come from a hard-coded test of the **behaviour type**:

- `EvidenceController.SpawnAllGhostTypeEvidence(traits)`, called from `GhostInfo.SyncEvidence` on every
  client, spawns the ghost orb on the master when the given evidence holds `GhostOrb` **or** the traits'
  behaviour type is the Mimic. At that moment the behaviour type is still the Mimic, since
  `SyncValuesNetworked` has just set it and no copy has happened yet.
- `EvidenceController.ForceRespawnGhostOrb`, called when the favourite room changes, makes
  the same test against the ghost's `GhostInfo`: the given evidence holds `GhostOrb`, or its behaviour
  type is the Mimic. Once the Mimic has copied another type, the behaviour type is no longer the Mimic,
  so the game moves its orb only while it holds orbs as evidence or has not copied yet.

So a copy, of a vanilla or a custom type, keeps the Mimic's orbs: they were spawned before the first copy.

On the related builder question, whether a ghost may force a fourth evidence or have fewer than three:
`GhostData` has exactly three evidence fields, and the only fourth evidence in the game is this
hard-coded orb test for the Mimic. An empty slot (`EvidenceType.None`) is carried through the evidence
list like a real evidence, and the pick may withhold a real evidence and give the empty one instead.

### Which setup lets a custom ghost run the game's generic AI while the SDK dispatches its rules

**Identity = the custom type, behaviour = `None`, and the acting-as type kept by the host.** The
evidence:

- `None` is the one value no type-specific code tests for and that the game itself treats as "no type"
  (the roll's Random, the Mimic list's exclusion). Custom Ghosts runs every custom ghost this way today,
  and its ghosts hunt, roam and give evidence through the game's generic code.
- The identity carries the custom type wherever the ghost is named or judged, and the evidence pick and
  `GetFullEvidence` find the custom `GhostData` by type in the collection.
- **The Mimic can copy a custom type with this setup.** The host adds the custom types to
  `mimicCopies`. When `MimicCopy` stores a custom type C as the behaviour type, the host (hooking
  `MimicCopy` or `AttemptGhostTypeStateChange`, after the original returns) records acting-as = C and
  sets the behaviour type to `None`, so the generic AI runs as for a custom ghost. Nothing between the
  store and the hook's return reads the behaviour type except MimicCopy's own Thaye and Obambo tests,
  which a custom type fails, and `ChangeState` builds the new state only after
  `AttemptGhostTypeStateChange` returns. When the Mimic later copies a vanilla type V, the hook sets
  acting-as = V and leaves the behaviour type as the game wrote it. Either way the previous copy's rules
  stop and the new ones start in the same call, on the host.
- The identity stays the Mimic throughout, so the journal and the results judge a Mimic, and the orbs it
  already has stay.

## How content numbers custom ghosts and evidence

Nothing in the loader fixes the game's counts. At startup the host reads the `GhostType` and
`EvidenceType` enums whole from the entries' resolved types, every value with its name, and the none
value of each from `GhostType.None` and `EvidenceType.None` (`sdk/runtime/game_enums.h`). Custom ghost
types are numbered from one past the largest GhostType value, custom evidence from one past the largest
EvidenceType value, and a vanilla ghost's content id is `vanilla.` and its value's name in lower case
(`sdk/runtime/content_ids.h`). A value whose name is not plain ASCII gets no id and is logged. When the
enums cannot be read, custom content stays off and the log says why. `gamecheck` prints the ranges
this build holds as its `CONTENT` line (build 25811160: GhostType 0..30, EvidenceType 0..7).

The manifest each client sends carries those first numbers too (its `n` entry), so two clients whose
games read different enums never agree. An enum value whose name would give the id of another value
gets no id and is logged: a future `Orbs` beside `GhostOrb` (whose contract id is `orbs`) keeps
`orbs` for `GhostOrb`, and two ghost names alike but for case lose both. The host logs the ranges it
read at startup, `content: GhostType 0..30, EvidenceType 0..7` on build 25811160.

## How content puts custom ghosts into the game

`host/content_types.h` does what the Mimic section above settles, through these entries only, plus
`LevelController.currentGhost` (the contract's GhostAI) and `LocalisationSystem.GetLocalisedValue`
(the game's text by key). While content is active (`content_active`):

- **The collection.** Each custom ghost gets a `GhostData` (a plain managed object, as the collection's
  readers use only its fields) in the contract's `GhostCollection.ghosts`: its type, its three
  evidence numbers (an empty slot is `EvidenceType.None`) and a name key in the game's own form. The
  form is read from the game's own entries, whose keys hold their type's value name
  (`Journal_SpiritTitle`): what stands around the name is the form, and a custom type's key holds its
  number there (`Journal_31Title`), as `GhostType.ToString()` gives it. Each goes in its alphabetical
  place: the game's entries sort by the names in their keys (their asset names, so TheTwins sorts as
  one word), the custom ones by their English names, ignoring case.
- **Names.** `GetLocalisedValue` answers each of those keys with the ghost's English name (Task 5 adds
  the game's language).
- **Identity and behaviour.** `GhostAI.Init` gives a custom identity the behaviour type `None`; the
  host's `GhostInfo.SyncValues` sends the custom type in place of `None` (changed in the caller's copy
  of the traits and put back after); after `SyncValuesNetworked` every client sets the behaviour type
  back to `None`. The traits' members are placed from the entries (`GhostInfo.identityType` less
  `GhostInfo.traits`), not from a fixed layout.
- **The Mimic.** After `BuildMimicCopies` the custom types join the list as the game built it, after
  its own (and again each frame while the contract holds custom ghosts). After
  `MimicCopy` a copied custom type becomes the acting-as type and the behaviour type goes back to
  `None`; a vanilla copy is the acting-as type as the game wrote it. The host sends the acting-as type
  to every client on its own channel when it changes and once to each player who joins, since the
  game sends nothing of the copy (`sdk/runtime/content_acting.h`).

Whether a contract has custom ghosts is decided once, when it loads, as today's Custom Ghosts does:
content active then puts them in, and otherwise any of this client's numbers a lobby left in the
collection come out. The decision holds until the contract ends: content going pending or off in
between changes nothing, and the custom rules keep running for the contract's types. At the contract's
end, and when the mods stop, the entries in the numbers this client's content takes leave the
collection and the Mimic's list, and the name keys go unanswered; entries of other numbers (a mod that
adds types itself) are left alone. A removal the game's List refused is logged once and tried again
every frame, while no contract holds content, until it is done. Outside a contract the lobby follows the
room instead: while content is active in it the custom ghosts are offered, and each list that offers
ghost types there (the difficulty menu's collection, the collection each lobby journal lists; one
asset, `All Ghost Collection`, in build 25811160, `systems/journal.md`) gets their entries; they leave
every such list when content stops being active there or a contract loads. When an entry these need did not
resolve the host withholds custom content (`Registry::withhold`, logged): it never activates, and
vanilla rules still run.

## Not mapped yet

The roll itself (an obfuscated `void()` method of GhostController), the weighted list, `CreateGhost`,
`StartGhost`, `GetFullEvidence`, the EvidenceController methods named above but `ForceRespawnGhostOrb`,
the gender bool and dictionary, and GhostController's own behaviour type. Map one when a mod needs it,
with its facts in the entry's note. The evidence pick is mapped (`GhostController.PickEvidence`, with
the given evidence of both classes); [evidence](../systems/evidence.md) describes it.
