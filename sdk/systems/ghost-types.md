# Ghost types and the Mimic

A ghost's type lives in a struct, `GhostTraits`, that two objects embed: `GhostController` (the host's
roll, one per contract) and `GhostInfo` (each client's copy, beside the `GhostAI`). The traits begin with
two `GhostType` fields: the **identity** (what the ghost is: what the journal judges and the results
name) and the **behaviour type** (whose type-specific rules the AI follows). The type numbers and their
evidence come from `GhostCollection` and `GhostData`. The entries are in `sdk/map/GhostTypes.h`,
`GhostInfo.h`, `GhostController.h`, `GhostCollection.h`, `LevelStats.h` and `GhostAI.h`; their `///`
notes are the authority, and this page ties them together. Facts were read from decompiled code of Steam
build 25811160. `tools/gamecheck.ps1`
prints every entry's value for the build in hand. Each type's own rules are gathered in
[the vanilla ghosts, type by type](vanilla-ghosts.md).

## How a ghost gets its type

1. **The roll (host).** `GhostController.Start` runs the roll on the master. It picks the behaviour type
   by weight from `GhostCollection.GetDefaultWeightedGhostList`, replaces it with the Custom difficulty's
   ghost type unless that is `None` (Random), copies it to the identity, and picks the given evidence
   (both in `GhostController.traits`).
2. **The evidence pick (host).** It looks the behaviour type up with `GhostCollection.GetGhostOfType` (with
   no null check: a type with no `GhostData` in the collection makes the roll throw), takes
   `GhostData.GetEvidenceList` (the three evidence fields in order, an empty slot included), then removes
   one at random for each evidence the difficulty withholds. For the Hantu, Goryo, Obake, Moroi and Deogen
   the random index never reaches the last one, so their third evidence is always given
   (`GhostController.PickEvidence`).
3. **Every client.** `GhostController.CreateGhost` (an RPC) calls `GhostAI.Init` with the controller's
   traits; Init copies them whole into `GhostInfo.traits` and runs `StartGhost`.
4. **The sync (host).** `StartGhost` calls `GhostInfo.SyncValues`, which sends `SyncValuesNetworked` with
   the traits' **behaviour type** as the ghost's type, then `SyncEvidence` with the given evidence.
   `SyncValuesNetworked` writes that one type to both the identity and the behaviour type on every client
   and asks `GhostController.GetFullEvidence` for its three evidence. `SyncEvidence` (once) stores the
   given evidence and passes the traits to `EvidenceController.SpawnAllGhostTypeEvidence`.

Custom Ghosts relies on steps 3 and 4 today: it gives a custom ghost the behaviour type `None` in Init,
sends the custom type in SyncValues instead of `None`, and sets the behaviour back to `None` after
`SyncValuesNetworked` (see `docs/custom-ghosts.md`).

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/ghost-types.md).
