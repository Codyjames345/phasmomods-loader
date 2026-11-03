# Ghost Tells

Status: in development. The code is the authority on what is implemented; `FINDINGS.md` (27) lists
what has not run in the game yet. Not run in the game since the move onto the SDK (2026-10-28).
Design: `docs/superpowers/specs/2026-10-21-ghost-tells-design.md`.

Three new tells each for six vanilla ghosts that are hard to tell apart by how they behave: the
Spirit, Mare, Shade, Yurei, Goryo and Onryo. A separate mod of the suite (`mods/ghost_tells`, channel
6).

It reaches the game only through the SDK's game API and content functions. Each ghost's tells are
content rules on its vanilla id (`vanilla.spirit` and so on, `sdk/api/content.h`): the content layer
runs them while the contract's ghost acts as that type, so a Mimic copying one of the six shows its
tells too, on every client, and only in a private room. Each of the six ghosts' journal pages ends
with a line naming its new tells (the content layer's `journal_lines`). The host runs the rules and
shows the lines only while the room's mod lists match, decided when the contract loads and held until
it ends (`sdk/README.md`, "The room's mod agreement"); otherwise the contract is plain vanilla. The
mod keeps no check of its own.

| Ghost | New tells |
| --- | --- |
| Spirit | Incense in a hunt stops it where it stands for 3 s. Incense outside a hunt silences it for a minute: no interactions, events or ability. It knocks on a door 1 to 3 s after a player within 10 m says its name (voice recognition on). |
| Mare | It never shows itself in a lit room: it switches the room's lights off first, or stays unseen when it cannot. The rooms it hunted through are dark when the hunt ends. It switches off a light just turned on near it 1 time in 2 (vanilla 1 in 7). |
| Shade | Stared at for 0.4 s while it shows itself, it vanishes. With two players within 5 m it does nothing at all. In a hunt it stops for 1.5 s when its prey has another player beside them. |
| Yurei | When incense traps it, it shuts the doors of its room and shuts any opened one again 3 s later, for the 90 s trap. The door its ability slams is locked for 3 s with the hunt's lock. A door left open in its room for 20 s draws its ability to that door. |
| Goryo | In its room with nobody within 10 m and a running video camera on it, it stands visible to cameras alone for 4 s now and then, and makes an extra interaction every 20 s. In a hunt, having lost everyone more than 12 m from its room, it walks home; it chases anyone it detects, anywhere. |
| Onryo | A flame lit within 4 m is blown out 1 time in 2 (counting toward its third-flame hunt). Every flame gutters as its count rises and burns low at two. A flame it blew out will not catch for 15 s. |

Speeds, cones, ranges and timings are in each ghost's `*_rule.h`, tested offline by
`tests/ghost_tells_test.cpp`, which also runs the game-side code over the SDK's fake backend. Where
each rule meets the game: `spirit.cpp`, `mare.cpp`, `shade.cpp`, `yurei.cpp`, `goryo.cpp`, `onryo.cpp`;
`world.cpp` reads the contract (the ghost, its rooms, the players, the hunt the speed update runs);
`gate.cpp` holds back the ghost's own interactions, events and hunts through GhostActivity.TryInteract
and GhostAI.ChangeState, never a forced change (the tarot cards) or a cursed possession's pull
(`gate_rule.h`, with the state numbers read from the game's GhostState enum); `rolls.cpp` answers rolls
of Random.RandomRangeInt that a tell marked with a flag around the game's own call (never by return
address: other mods chain there; the Mare's around LightSwitch.UseLightSwitch, which runs its
switch-off coroutine's first step); `hold.cpp` holds a hunting ghost still through the content rules'
`huntSpeed`. The Goryo's figure is the suite's model copy (`mods/custom_ghosts/apparition.cpp`) on the
game's camera-only layer, LaserGhost. Messages on the channel start `'G' 'T'` (`tells_net.h`).

A player controlling one of these ghosts in Playable Ghost is bound by the same rules: its events,
hunts and abilities go through ChangeState, and before each action Playable Ghost asks whether the rules
hold the ghost back: the rules answer the content layer's `heldBack` (`content::held_back`).
