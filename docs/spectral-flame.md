# Spectral Flame

Status: in development. The code is the authority on what is implemented; `FINDINGS.md` lists what
has not run in the game yet. Design: `docs/superpowers/specs/2026-10-20-spectral-flame-design.md`.

An eighth evidence type, a separate mod of the suite (`mods/spectral_flame`). A candle or lantern
left burning where the ghost spends its time slowly turns blue. It tells investigators where the
ghost lives, never where it is right now: the motion sensor keeps that job.

It acts only in a private room in which every client runs it, and only in a contract where some
ghost type carries it. No vanilla ghost does; the custom ghosts that do come from Custom Ghosts,
which requires this mod. Otherwise nothing changes: no journal row, no blue, no extra blow-outs.

## How it reads

| | |
| --- | --- |
| Which flames | A lit Firelight (candle or lantern) that has been set down, and every lit candle of a candelabra in the house. A flame in a player's hand never changes; picking one up starts it over. |
| What turns it blue | Time the ghost spends in the flame's room, hunts included. Every second counts, so short visits add up; a ghost passing through changes little (a true flame turns about 1.7% a second on a Tier 1, 2% on a Tier 2 and 2.5% on the lantern; a false flame half that). |
| How fast | A fully blue flame takes 60 s of ghost time in its room (Tier 1 and candelabras), 50 s (Tier 2) or 40 s (Tier 3, the lantern). Whenever the ghost is away it drops back towards yellow at half that speed: fully blue to none in 120 s for a Tier 1. A ghost that spends p of its time in the room turns a Tier 1 flame fully blue in about 120 / (3p − 1) s of real time (under two minutes at 70%), and never below a third. |
| The false positive | Any ghost turns a flame halfway, a pale washed-out blue, in the same time a true flame takes to turn fully blue, and no further. A false flame is half a true one in everything, fading included, so whatever the ghost does it is half as blue as a true flame would be: half a minute in, a true flame already looks the way a false one ends up, and players who know the speed can tell them apart before the minute is up. |
| The evidence | Only a ghost that gives Spectral Flame takes a flame past the pale blue to a deep blue. Difficulty that hides evidence can hide it, as any other. |
| The Hydra | Its flame turns green where any other ghost's turns blue, in the same time: pale green at the false positive's limit, and a deep green only when Spectral Flame is one of its given evidence. Where the difficulty withholds it (zero evidence included) the flame still stops at the pale green, so a pale green flame names the Hydra whatever the evidence. |
| What it costs | A bluer flame is hotter and burns down faster, twice as fast at full blue, so candles spread through the house to find the ghost room cost the candles a test needs. A flame more than half blue no longer eases sanity drain as Firelight does. |
| Blow-outs | A ghost that gives Spectral Flame cannot blow out a flame set down until it has been fully blue, so nothing interrupts the test; afterwards it may, though the flame has faded a little since, and while fully blue it also blows it out now and then (about once every 75 s). Picking the flame up or putting it out starts the test over. Other ghosts blow candles out as the game's do, so a flame blown out before it has been fully blue rules Spectral Flame out, as a thrown writing book rules out Ghost Writing. |

The flame turns hotter as it goes: from orange to white-hot, then a pale sky blue at the false
positive's limit, then a deep blue (the same for candles and the lantern). Both blues are far dimmer
than the orange flame, about as bright as the game's own blue flame, since a blue as bright as the
orange renders white. Every light on the candle and its flames follows its bluest flame at the
flame's own hue (a Tier 2 Firelight's own "Point light.1" included), so the walls take the same cold
cast as the flame, visible from the doorway or on a video camera. The blue light is darker than the
warm one, as a blue flame is: one bright enough to match would show as a pale cyan.

For the Hydra the same path ends in green: white-hot, then a pale green at the limit, then a deep
green. The game ships no green flame, so the deep green takes the hue of its D.O.T.S. green (the
"DOTS Reveal" material, 0.098, 1, 0) at the deep blue's brightness. The pale green is a mint green
as bright as the pale blue, with no more red than blue, so players see it as plainly neither the
pale blue nor a lit candle's warm white: on zero evidence that stop is the Hydra's whole tell. The
lights on the walls turn green with it.

## The journal

Every journal on every client gets an eighth row on the evidence page, "Spectral Flame", cloned
from the Spirit Box row, registered with the journal's evidence buttons and their synchronisation.
It marks, crosses out and filters the ghost list like the others.

## How it works

* The host works out each flame's blueness every frame (`mods/spectral_flame/flame_rules.h`, tested
  offline) and sends every client the blue ones four times a second on the mod's channel; every client
  shows the colour (each flame's own material; each candle's lights once per candle, with its
  bluest flame), burns a blue candle down faster and keeps it out of the sanity effect. The host
  also rolls the extra blow-out through the game's own `FireSource.BlowOutFire`, so the
  interaction EMF and every client's candle follow as for any blow-out, and holds back the ghost's
  own `BlowOutFire` calls on a flame still turning (they also complete the candle objective, which
  waits with them).
* The ghost's evidence is the list the game kept for it after the difficulty removed evidence
  (`GhostInfo`, sent to every client by its `SyncEvidence`). A custom difficulty giving no evidence
  gives no Spectral Flame either.
* Every client asks Custom Ghosts which ghost types turn a flame green (the service
  `custom-ghosts.v1`, version 5, `greenFlame`), with the ghost's own type (`GhostInfo +0x28`, written
  on every client by the ghost's sync), once a second; a flame already showing a colour is repainted
  when the answer changes. Without Custom Ghosts every flame turns blue.
* Other mods ask whether it can be given through the service `spectral-flame.v1` (`core/services.h`).
