# Spectral Flame: design

Status: design, not yet approved. Game build 25811160.

> **Superseded in part (2026-10-21).** After the first contracts the user rebalanced the evidence;
> `docs/spectral-flame.md` is the authority where the two differ. Changed: the false positive is
> half a flame (a pale blue), not a third, reached in the time a true flame takes to turn fully
> blue; a flame drops back to yellow at half the speed it turns (60/50/40 s to full blue by tier, as
> before, after a day at 120/100/80 s with a 20 s warm-up, removed on 2026-10-22); hunts build
> blueness; a ghost that gives Spectral Flame cannot blow out a set-down flame until it has been
> fully blue, instead of blow-outs favouring bluer flames.

## Intent

Phasmophobia has seven evidence types, and the 35 three-evidence combinations are all taken (30
vanilla ghosts plus the five in Custom Ghosts). A new evidence type, Spectral Flame, opens 21 new
combinations so more custom ghost types can be added later.

Spectral Flame lives on the game's existing Firelight (the candle and the lantern) and on lit
candelabras in the houses. A flame burning where the ghost spends its time slowly turns blue. It
answers "where does the ghost live", never "where is the ghost now": it must not become a free
ghost tracker. The motion sensor keeps that job.

## Goals

- An eighth evidence type that works like the other seven: it appears for ghost types that have
  it, never for those that do not, can be marked and filtered in every player's journal, and is
  hidden by the difficulty rules that hide evidence (Nightmare, Insanity, custom difficulty).
- A reading that is slow, room-based and costly to get, with an honest false positive.
- Reuse the game's own Firelight item, models, flame, light and blow-out behaviour. No new item,
  no new shop entry, no new model.

## Non-goals

- Any change to a vanilla ghost type's evidence. Only custom ghost types carry Spectral Flame.
- Any change to a contract where not every player runs the mod. Then the game is plain vanilla.
- A photo or media reward for a blue flame (possible later; not in this version).

## Anti-goals (a failure even if it technically works)

- A flame that tells you where the ghost is right now, which way it is, or when it walks past.
- A held flame used as a scanner while walking through the house.
- Anything that helps during a hunt.
- Any visible change to vanilla ghosts that do not have the evidence beyond the small false
  positive below.

## How it plays

**Which flames count.** A lit Firelight candle or lantern that has been put down (on the floor, a
surface or in a socket), and every lit candle of a candelabra in the house. A flame held in a
player's hand never changes colour; it flickers yellow as it does today. Picking a flame up resets
its blueness to zero (designer's call: a blue candle cannot be carried as a ready-made reading, and
setting it down elsewhere starts over).

**Room, not distance.** A flame belongs to the room it stands in (the game's own room of that
position). Only time the ghost spends in that same room counts. Walls never leak.

**Blueness builds and fades slowly.** Each counting flame has a blueness from 0 to 1.

- While the ghost is in the flame's room, not hunting and not in a ghost event, blueness rises.
- At other times it falls, slower than it rises.
- A ghost walking through the room changes almost nothing; a ghost that keeps coming back to its
  favourite room turns a flame there fully blue within a minute or two of its time there.

**What players see.**

- Every flame already has a small blue base. Any ghost, with or without the evidence, can push
  that base up to about a third of the flame (the false positive, like EMF 2 to 4).
- Only a ghost with Spectral Flame takes a flame past that, to fully blue from base to tip. A flame
  that is blue to the tip is the evidence.
- The flame's light turns blue with it, so the walls of the room take a cold blue cast that can be
  seen from the doorway or on a video camera.
- No lean, no direction, no sound cue.

**Costs.**

- A blue flame gives no comfort: while a placed flame is more than half blue it stops counting for
  the game's Firelight sanity effect (held flames are never blue, so they keep it).
- A blue flame burns down faster (the candle's own burn time).
- The bluer a flame, the likelier a ghost's ordinary flame blow-out picks it. Getting the evidence
  and losing the flame tend to happen together. Ghost type rules about flames (the Onryo) are
  unchanged.

**Hunts.** During a hunt blueness does not rise and flames behave as vanilla.

## Tiers

The game's Firelight tiers keep their vanilla roles. Spectral Flame adds a build speed per tier:

| Firelight tier | Ghost time in the room for a fully blue flame | Notes |
| --- | --- | --- |
| Tier 1 | about 60 s | |
| Tier 2 | about 50 s | |
| Tier 3 (lantern) | about 40 s | the glass softens the blue seen from a distance |

Candelabra candles build at the Tier 1 rate.

These numbers are a starting point for playtesting, kept in one place so they can be tuned.

Designer's call, pending reverse engineering: if a vanilla tier cannot be blown out by the ghost or
does not burn down, that stays vanilla, and that tier then builds at the Tier 1 rate instead of
faster. No tier may be both the fastest and free of the costs.

## With the playable ghost

Designer's call: the same rule as for the AI ghost, from the ghost's real position. A ghost player
can keep away from a room with a flame or linger in a decoy room; the favourite-room tether and
restlessness rules of Playable Ghost already limit how far they can take that. No HUD change.

## The journal

Every journal on every client gets an eighth evidence button, "Spectral Flame", cloned
from a vanilla one and registered with the journal's own evidence lists and synchronisation, so it
marks, crosses out and filters ghosts exactly like the vanilla buttons. The game's evidence enum
value is 8. Ghost types whose `GhostData` lists evidence 8 are filtered by it like any other.

The journal's evidence filtering was decompiled (JournalController.AddEvidence, FilterOutEvidence,
GhostData.GetEvidenceList): it works on generic lists and has no limit of seven.

## Constraints

- A mod of the suite (`mods/spectral_flame`, its own DLL, `mod.ini`, channel and id
  `spectral-flame`), started by ModHost like the others, loader-agnostic.
- Active only in a private room in which every client runs it; otherwise nothing changes.
- Designer's call on the gating between the two mods: Spectral Flame publishes a service
  (`spectral-flame.v1` in `core/services.h`) saying whether it is active for this contract. Custom
  Ghosts adds its types only when that service says yes, so no ghost ever has an evidence nobody
  can find. When no ghost type in the contract's collection carries evidence 8 (Spectral Flame
  running without Custom Ghosts), Spectral Flame adds no journal button and changes no flame (no
  false positive either), so marking the button can never cross out every ghost.
- The host decides blueness and the evidence (it runs the ghost); every client shows the same
  flame colours.
- Reverse engineering of the game is done with REA (slim PE slices, `re/slimpe.mjs`), and every
  game fact the code relies on is checked in a decompile, not assumed.
- Testing in the game follows the anti-cheat rules already recorded for this project: no evasion,
  no loader DLLs in the game folder.

## Left to the builder

- How blueness is computed and synchronised (rates, curve, update interval, message format).
- Where the flame colour, light colour, sanity effect, burn rate and blow-out choice are hooked,
  found by reverse engineering `Candle`, `FireSource`, `Candlebra`, `PlayerSanity` and the ghost's
  flame blow-out.
- How the evidence button is cloned and laid out on the evidence page, following
  `mods/custom_ghosts/pages.cpp`.
- How the difficulty rules that hide evidence are made to include evidence 8, after reverse
  engineering them.
- The exact blue colours, within: yellow flame with a small blue base when cold, fully blue when
  confirmed.

## Custom Ghosts depends on Spectral Flame

Decided by the user: Custom Ghosts requires Spectral Flame, and its ghosts use the new evidence
where it fits. Vanilla ghosts keep their evidence.

**A mod can require another.** The suite has no such mechanism yet. A mod's `mod.ini` gains
`requires=<mod id>`. The host never starts a mod whose requirement is not running. The launcher and
the in-game mod list keep the pair consistent: turning Custom Ghosts on turns Spectral Flame on, and
turning Spectral Flame off turns Custom Ghosts off, with the list saying why. The lobby join guard
already compares mod lists, so it needs nothing new.

**Existing custom ghosts.** Designer's call on "where it fits", each swapping Ghost Orbs for
Spectral Flame:

| Ghost | Before | After | Why it fits |
| --- | --- | --- | --- |
| Draugr | EMF 5, Ghost Orbs, Freezing | EMF 5, Freezing, Spectral Flame | Norse mound fire: blue flames burn over a barrow its dead guard |
| Kikimora | Ultraviolet, Ghost Orbs, Ghost Writing | Ultraviolet, Ghost Writing, Spectral Flame | Slavic folklore puts the kikimora behind the stove, by the household fire |

The Echo, Dullahan and Black Shuck keep their evidence. The two combinations freed
(EMF 5 + Ghost Orbs + Freezing, Ultraviolet + Ghost Orbs + Ghost Writing) become free again. A
candle in the Kikimora's room does not count as the lights being on for its weakness.

Their journal pages, `kGhosts` and the evidence names in `mods/custom_ghosts/ghosts.h` and
`docs/custom-ghosts.md` change to match. The custom ghosts are added to a contract only when
Spectral Flame is active in it.

## The ghosts that carry it

Settled: twelve custom types carry Spectral Flame, the Draugr and Kikimora above and ten new ones,
described in `2026-10-20-flame-ghosts-design.md`. This design is built first; the new ghosts follow
in batches.
