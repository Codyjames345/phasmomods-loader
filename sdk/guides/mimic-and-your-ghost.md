# The Mimic and your ghost

The game's Mimic copies other ghost types during a contract. This guide says what that means for a custom ghost and
for rules on a vanilla type. The game's side, with each method cited, is in
[ghost types and the Mimic](../systems/ghost-types.md).

## Two types: identity and behaviour

Every ghost has an **identity** (`GhostInfo.identityType`), which the journal and the results judge, and a
**behaviour type** (`GhostInfo.behaviourType`), which the game's type-specific code reads. A Mimic's identity stays
the Mimic all contract. When it copies, the game changes only the behaviour type, on the host, and sends nothing to
the other clients.

A custom ghost is registered with its own identity and the game's "no type" as its behaviour, so the game's generic
AI runs it and no vanilla type's code applies. The host keeps a third value, the **acting-as** type: the behaviour
type, or the identity when the behaviour is "no type". Rules run by acting-as.

## The Mimic can copy your ghost

While a contract holds custom ghosts, the host adds them to the Mimic's copy list (`GhostController.mimicCopies`),
and the game then picks among them like any other. When the Mimic copies a custom ghost the host records that ghost as
acting-as and sets the behaviour back to "no type", so the generic AI runs as it does for your ghost.

What follows for your mod:

- **Your rules run for the copy.** `start` runs when the Mimic begins acting as your ghost and `stop` when it switches
  away, on every client, since the host sends the acting-as type to all of them. Keep per-hunt or per-copy state in
  your rules and reset it in `stop`, as the [example ghost](adding-a-ghost.md) clears its beat.
- **The identity stays the Mimic's.** The journal judges a Mimic, and your ghost's evidence is not the copy's: the
  Mimic keeps its own evidence and its orbs, which the game spawned before the first copy.
- **Your evidence's look stays the Mimic's.** `evidence_look` is asked by identity, so a Mimic copying the Hydra does
  not turn Spectral Flame green.
- **Callbacks a player uses** (`content_use_ability`, `content_hud_state`, `content_held_back` and the rest) answer by
  the acting-as type, so a player controlling a Mimic that copies your ghost uses your ghost's ability and limits.

Rules on a vanilla type work the same way: a Mimic copying the Mare runs every mod's rules on `"vanilla.mare"`
([tells for vanilla ghosts](tells-for-vanilla-ghosts.md)).

## Questions to ask

- `pm::api::content::acting_as()`: the type the contract's ghost acts as on this client, -1 with no ghost.
- `pm::api::content::identity()`: its identity, -1 with no ghost.
- `pm::api::content::id_of(type)`: the id of a type number, `"vanilla.<name>"` or a custom ghost's.

Never compare against a fixed number. The game's types and yours are numbered at runtime (`type_of`, `id_of`).

## A player choosing the copy

In Playable Ghost the player picks what the Mimic copies at each of the game's own switches (an allowed difference
from the AI). The choice goes through the game's own `mimicNext`, which takes any type number, a custom one included.
Your ghost needs nothing extra for this.
