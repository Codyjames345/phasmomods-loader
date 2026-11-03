# Evidence: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [Evidence](../systems/evidence.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## What the host adds

`host/content_evidence.h` does the following, through the entries above only:

- **Giving.** Nothing of its own. `host/content_types.h` writes a custom ghost's evidence numbers into its
  GhostData; the game's pick gives or withholds a custom evidence there exactly as it does its own, and
  the game's sync carries it. The host never writes `givenEvidence`.
- **What the contract holds.** The custom evidence its custom ghosts list, by number, decided with the
  ghosts when the contract loads and held until it ends.
- **The row.** On each journal of this client, other players' books included, once the contract holds a
  custom evidence: a copy of the Spirit Box's row (`Scene.JournalEvidenceButton`) after the game's rows,
  its number set, named by the evidence's English name. Its name and line keep the game's localisers,
  with keys in the game's form: the part every game row's name key begins with, then the evidence's id;
  the line's key adds the suffix every game row's line key adds (`Journal2_phasmomods.spectral-flame.flame` and
  `Journal2_phasmomods.spectral-flame.flame_Tooltip` in this build). The host answers both through
  `TryGetLocalisedValue` with `PmEvidenceDef::name` and `journalLine` in the player's language, else
  English. When the rows show no single form, no row is made and the log says so; a journal met before
  any journal showed the form gets its row once one does. Two held evidence whose keys meet (one's line
  key is the other's name key) get no row and no page line, logged.
- **Marks, sync and filter.** The row joins `evidenceButtons`, and the evidence sync runs `Initialise`
  again (through the runtime, as for the guess sync), so the game's own code marks it, syncs it to the
  other players and filters the ghost buttons by its number. Each game row keeps the one subscription
  it had. The second `Initialise` adds a second player-left listener, as `systems/journal.md` describes
  for the guess sync.
- **Layout.** The column's spacing shrinks so every row fits in the height the game's rows filled: with
  row height h (the Spirit Box row's) and spacing s, r rows and n added, (r - 1) * s + r * h equals
  (r + n - 1) * s' + (r + n) * h, never below 0.
- **Pages.** A custom ghost's page shows a custom evidence by its row's name key, as the game's pages
  show theirs (`host/content_journal.h`).
- **Failure.** Nothing changes until every copy is made and every operation the change needs was found;
  when the evidence sync cannot read the rows, the array, the subscriptions and the copies are put back
  and the sync reads the game's rows again, logged.
- **Look.** `content_evidence_look` answers the contract ghost's own `PmGhostDef::evidenceLook`, by its
  identity: a Mimic that copies a custom ghost keeps the Mimic's (no) look.
