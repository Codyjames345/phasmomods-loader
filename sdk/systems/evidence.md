# Evidence

How the game picks the evidence a ghost gives, how a difficulty hides some of it, how what is given
reaches every client, and how the journal's evidence page marks evidence and filters the ghosts. Then how
the host's content evidence (`host/content_evidence.h`) adds a custom evidence. Facts were read from
decompiled code of Steam build 25811160 (`re/decomp.mjs`) and from the PCPlayer prefab in
`resources.assets` with UnityPy. The entries' `///` notes are the authority. They are in:

- `sdk/map/GhostController.h`: `givenEvidence`, `PickEvidence`;
- `GhostInfo.h`: `givenEvidence`, `SyncValues`, `SyncEvidence`;
- `Difficulty.h`: `evidenceGiven`;
- `GhostCollection.h`: the GhostData evidence;
- `JournalController.h`: `evidenceButtons`, `evidenceSync`, `AddEvidence`, `FilterOutEvidence`,
  `RemoveEvidence`, `SetGhostTypes`, `EvidenceButton`, `ThreeStateButton.textLabel`,
  `JournalEvidenceSyncController`;
- `UnityUI.h`: `HorizontalOrVerticalLayoutGroup`;
- `Scene.h`: `Scene.JournalEvidenceRows`, `Scene.JournalEvidenceButton`.

## Numbers, not a count

An evidence is an `EvidenceType` number: None 0, then EMF 1 to DotsProjector 7 in this build. Custom
evidence takes the numbers after the largest, read from the enum at startup
(`sdk/internals/ghost-types.md`, "How content numbers custom ghosts and evidence"). None of the code below
compares an evidence with the enum's size or a count of evidence types: the pick, the sync, the journal's
lists, its filter and its sync all carry evidence as numbers in lists and arrays. A number past the
enum's largest is carried like any other.

## How the game picks a ghost's evidence

`GhostController.PickEvidence`, on the master, from the type roll once the type is settled:

1. `givenEvidence` (the first of the traits' two evidence lists) becomes a new empty list.
2. When `LevelValues.difficulty`'s `evidenceGiven` is below 1, that is all: the ghost gives no evidence.
3. Otherwise `givenEvidence` becomes `GhostData.GetEvidenceList` of the GhostData that
   `GhostCollection.GetGhostOfType` finds in `allGhostsCollection` for the traits' behaviour type: its
   three evidence fields in order, an empty slot included.
4. Then, 3 minus `evidenceGiven` times, it removes the element at `Random.Range(0, count)`. For the
   Hantu (13), Goryo (14), Obake (19), Moroi (21) and Deogen (22) the range ends one short, so their
   third evidence is never removed.

Three obfuscated look-alikes do the same with other constants and call obfuscated getters in place of
`LevelValues.get_Instance`; the entry's facts tell them apart.

### A custom ghost that keeps its last evidence (PM_GHOST_KEEPS_LAST_EVIDENCE)

The pick reads `GhostController.behaviourType` first as GetGhostOfType's
argument, then once per removal, comparing it with the five numbers above to choose the removal's range.
Between GetEvidenceList's return and the pick's end it calls only `LevelValues.get_Instance`,
`Random.RandomRangeInt` and the List's RemoveAt, none of which reads the controller.

The host (`host/content_types.h`) follows the pick of a custom ghost with the flag:
- Once GetEvidenceList has returned inside that pick, the behaviour type reads as the Goryo's
  (`GhostType.Goryo`) for the rest of it, so the game's own removals skip the last evidence.
- The custom type is put back when the pick returns or throws.
- GetGhostOfType still gets the custom type, so the custom ghost's own three evidence are the ones
  picked from.
- With zero evidence given the pick returns before GetEvidenceList, and all three go, as the Goryo's
  do.

## How a difficulty hides evidence

Through `Difficulty.evidenceGiven` alone, in two places: the pick above removes 3 minus it, and
the sync below sends that many. Neither the pick nor the sync looks at which evidence a slot holds, so an
evidence is hidden by its slot and the roll, whatever its number.

## How what is given reaches every client

- `GhostInfo.SyncValues` (from `StartGhost` on the master) sends `SyncValuesNetworked` to
  every client, then makes an `int[]` of `evidenceGiven` elements, the first that many of the traits'
  `givenEvidence`, and sends it with the RPC `SyncEvidence` to every client.
- `GhostInfo.SyncEvidence` runs on every client, once (a flag of GhostInfo, set at its end), and only after its
  check of the sender. It makes `givenEvidence` a new list holding each number received, then passes the traits
  to `EvidenceController.SpawnAllGhostTypeEvidence`.

So every client's `GhostInfo.givenEvidence` holds the numbers the master's pick gave, a custom evidence's
number included, as plain ints. A mod that reveals an evidence in the world reads it there (Spectral Flame
does: a ghost whose given list lacks the flame is treated as one that never gives it).

## The journal's evidence page

- The page's rows (`Scene.JournalEvidenceRows`, a VerticalLayoutGroup) are seven `EvidenceButton`s, all active:
  EMF, DOTS, Fingerprints, Freezing, Ghost Orbs, Ghost Writing, Spirit Box. Each holds:
  - its number (`EvidenceButton.evidence`);
  - its name (`ThreeStateButton.textLabel`), whose key is the evidence's (`Journal2_EMF5`, `Journal2_DOTS`,
    `Journal2_Fingerprints`, ...);
  - the line under it (`EvidenceButton.descriptionLabel`), whose key is the name's key with `_Tooltip` after it,
    on every row.

  The name and the line are each a TextMeshPro text with a `TextLocaliserUI`. A ghost's page shows its evidence
  by the rows' name keys (the Banshee's: `Journal2_Fingerprints`, `Journal2_GhostOrbs`, `Journal2_DOTS`).
- The journal keeps the rows in `JournalController.evidenceButtons`, an `EvidenceButton[]`.
- A row has a state (a field of ThreeStateButton: 0 unmarked, 1 found, 2 ruled out). When it changes,
  `EvidenceButton`'s override of the state change runs the base's, then, on its journal, the body of
  `AddEvidence` at 1, of `FilterOutEvidence` at 2 and of `RemoveEvidence` at 0, inlined, with its own number.
- `AddEvidence` takes the number off the ruled-out list, adds it to the found list unless it is there, and runs
  `SetGhostTypes`. `FilterOutEvidence` is its mirror. `RemoveEvidence` takes it off both and tail-jumps to
  `SetGhostTypes`.
- `SetGhostTypes` empties the list of ghosts left and walks `ghostButtons`. A ghost stays when each found
  number is one of its three, and leaves when a ruled-out number is one of its three. A virtual method of each
  button sets its look by whether its ghost stayed.
- `JournalEvidenceSyncController` is the same generic base as the guess sync ([the journal](journal.md)). Its
  getter of the buttons returns `evidenceButtons`, and its getter of a button's value returns the button's
  number, so a mark is sent and matched by number. `Initialise` reads the buttons and subscribes to each one's
  `userStateChanged`.
- `HighlightGhostEvidence` and `ScratchWrongEvidence` walk the rows by their numbers as well.

## Read from the game, not fixed

The evidence numbers (the enum), how many a difficulty gives (the game's own pick and sync use it; the
host needs no copy), the rows and their order, the keys' form, the row height and spacing, the buttons'
array and sync, and the filter's lists (the game's own, which the host never copies).

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/evidence.md).
