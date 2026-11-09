# Adding evidence

This guide shows how a mod adds an evidence type, with Spectral Flame (`mods/spectral_flame/`) as the worked case.
Read [adding a ghost](adding-a-ghost.md) first: a custom evidence matters only once a ghost gives it.

An evidence has two halves, and the SDK does one of them:

- **In the journal and the game's rules** (the host's): a number after the game's own, a row on every journal's
  evidence page, a place in a ghost's three evidence, the game's own pick and a difficulty's withholding, and the sync
  to every client. You register the evidence and the host does the rest.
- **In the world** (your mod's): what the players see or measure that reveals it. Spectral Flame turns candle flames
  blue. Nothing in the SDK does this part for you.

## Registering it

From `pmmod_start`, with its text from the mod's string table:

```cpp
pm::api::content::EvidenceDef def;
def.id = "phasmomods.spectral-flame.flame";                          // "<mod id>.<name>", never changed
def.name = {"en", pm::i18n::text("evidence.name")};                   // its journal row and ghost pages
def.journalLine = {"en", pm::i18n::text("evidence.journal-line")};    // the line under its name
const bool ok = pm::api::content::register_evidence(def) > 0;
```

(`mods/spectral_flame/contract.cpp`, `register_evidence`.) It answers 0, and logs why, outside `pmmod_start`, for a
repeated or malformed id, for text without English, or while custom content is off on this client. A refusal during
`pmmod_start` keeps the whole mod off, whatever the start returns, so checking the answer is optional.

A ghost gives it by naming its id in `GhostDef::evidence`, whichever mod registers that ghost. Custom Ghosts' twelve
flame ghosts name `phasmomods.spectral-flame.flame`, and Custom Ghosts requires Spectral Flame in its manifest, so the
loader starts Spectral Flame first. Once every mod has started, the host drops (and logs) any ghost whose evidence no
running mod registered ([README, "The host contract, version 2.3"](../README.md#the-host-contract-version-23)).

## What the host does with it

- **Its number.** The host numbers custom evidence after the largest value of the game's `EvidenceType` enum, read at
  startup, so a game update that adds evidence moves yours along. Never store the number: ask
  `pm::api::content::evidence_of(id)` when you need it. It answers -1 while content is not active
  ([evidence](../systems/evidence.md), "Numbers, not a count").
- **The pick.** The number stands in the ghost's `GhostData`, so the game's own pick (`GhostController.PickEvidence`)
  treats it as any evidence: a difficulty that gives fewer evidence may withhold it, and zero evidence gives none
  ([evidence](../systems/evidence.md), "How the game picks a ghost's evidence").
- **The sync.** `GhostInfo.SyncEvidence` gives every client the same `GhostInfo.givenEvidence` numbers, yours included.
- **The journal.** Whenever the ghosts shown give it, every journal gets a row for it after the game's rows: a copy of
  the Spirit Box row with your name and line. It is marked, synced and filters the ghost buttons as the game's rows do
  ([evidence](../internals/evidence.md), "What the host adds").

All of this happens only in a private room where every player runs the host's mods, decided when a contract loads and
held until it ends. Your mod checks none of it.

## Revealing it in the world

Your mod reads, on each client, whether the contract's ghost gives the evidence, and shows it only then. Spectral
Flame reads the list the game kept after the difficulty's pick:

```cpp
bool ghost_gives(void* ghost, int32_t evidence) {
  void* info = phasmo::GhostAI::info.get(ghost);
  if (evidence < 0 || !pm::api::alive(info)) return false;
  const pm::api::List given{phasmo::GhostInfo::givenEvidence.get(info)};
  const pm::api::Array<int32_t> values{given.items()};
  for (size_t i = 0; i < given.size(); ++i)
    if (values[i] == evidence) return true;
  return false;
}
```

(`mods/spectral_flame/contract.cpp`.) A ghost whose given list lacks the evidence is treated as one that never gives
it, so a withheld evidence never shows. Spectral Flame also acts only in a contract where some ghost type in the
game's collection carries the evidence, so a contract of vanilla ghosts alone is untouched.

Design the reveal around the game's own systems, as Spectral Flame does: it changes the look of the game's own
flames, blows them out through the game's own `FireSource.BlowOutFire`, and leaves the sanity effect to the game's
own candle test ([flames](../systems/flames.md)). Every claim about what the game does comes from its code, cited by
map key.

## Options a ghost has

- **A look of its own.** A ghost's `GhostDef::evidenceLook(evidenceId)` may change how your evidence looks for it.
  Your mod asks `pm::api::content::evidence_look(id)`: the Hydra answers "green" for Spectral Flame, and an empty
  answer is the evidence's own look. It is asked by the ghost's identity, so a Mimic copying the Hydra keeps the
  Mimic's look.
- **Keeping its last evidence.** `PM_GHOST_KEEPS_LAST_EVIDENCE` in `GhostDef::flags` makes the pick never withhold the
  ghost's third evidence, as the game does for the Goryo's D.O.T.S.; on zero evidence all three still go
  ([evidence](../systems/evidence.md), "A custom ghost that keeps its last evidence").
- **A false positive** is the evidence mod's own design. Spectral Flame's half-blue for every ghost is its rule, not
  the SDK's.

## Rules the bundled evidence follows

- A custom evidence value never reaches a save file or a player without the content.
- An extra sign that a ghost always shows, whatever its evidence, is a tell, not evidence (as the Mimic's orbs).
- Player-facing text lives in the mod's string table.
