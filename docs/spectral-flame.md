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
| What turns it blue | Time the ghost spends in the flame's room, hunts included. Every second counts, so short visits add up; a ghost passing through changes little (a true flame's ghost time builds about 1.7% a second on a Tier 1, 2% on a Tier 2 and 2.5% on the lantern; a false flame's half that). |
| How fast | A fully blue flame takes 60 s of ghost time in its room (Tier 1 and candelabras), 50 s (Tier 2) or 40 s (Tier 3, the lantern); only the time differs between tiers. The colour comes late, so it is not obvious straight away: a Tier 1 is a quarter of the way to white-hot after 30 s, white-hot at about 42 s, the pale blue at about 50 s and the deep blue at 60 s (its colour follows its ghost time to the fourth power; every tier in its own time). Whenever the ghost is away it fades evenly towards yellow at half the build speed: fully blue to none in 120 s for a Tier 1. When the ghost comes back the colour holds until the ghost time has caught up with it, so it never follows the ghost in and out of the room. A ghost that spends p of its time in the room turns a Tier 1 flame fully blue in about 120 / (3p − 1) s of real time (under two minutes at 70%), and never below a third. |
| The false positive | Any ghost turns a flame halfway, a pale washed-out blue, in the same time a true flame takes to turn fully blue, and no further. A false flame is half a true one in everything, fading included, so whatever the ghost does it shows half the colour a true flame would: a minute in, a false flame has only reached the pale blue a true one passed ten seconds before, and players who know the speed can tell them apart. |
| The evidence | Only a ghost that gives Spectral Flame takes a flame past the pale blue to a deep blue. Difficulty that hides evidence can hide it, as any other. |
| The Hydra | Its flame turns green where any other ghost's turns blue, in the same time: pale green at the false positive's limit, and a deep green only when Spectral Flame is one of its given evidence. Where the difficulty withholds it (zero evidence included) the flame still stops at the pale green, so a pale green flame names the Hydra whatever the evidence. |
| What it costs | A bluer flame is hotter and burns down faster, twice as fast at full blue, so candles spread through the house to find the ghost room cost the candles a test needs. A flame more than half blue no longer eases sanity drain as Firelight does. |
| Blow-outs | A ghost that gives Spectral Flame cannot blow out a flame set down until it has been fully blue, so nothing interrupts the test; afterwards it may, though the flame has faded a little since, and while fully blue it also blows it out now and then (about once every 75 s). Picking the flame up or putting it out starts the test over. Other ghosts blow candles out as the game's do, so a flame blown out before it has been fully blue rules Spectral Flame out, as a thrown writing book rules out Ghost Writing. |

The flame turns hotter as it goes: from its own yellow-white to white-hot, then a pale sky blue at
the false positive's limit, then a deep blue, the same for candles and the lantern. The whole flame
takes the colour, tip and base too. The game's candle flame picture is itself orange and yellow
around a white core, so no tint could turn it blue; in a contract where Spectral Flame can be given,
every candle flame shows a grey copy of that picture from the start instead, tinted so it looks as
the game's own does (on screen within a few levels of 255 from core to base), and the colour then
tints it evenly. The lantern's flame blends two colours of its own, and both turn. The glow at the
top of a Tier 1 or Tier 2 candle's wax turns with its flame.

The colours are chosen through the game's own picture pipeline (its colour space, Unity's colour
conversion and the ACES tone mapping its post-processing uses, `tests/urp_display.h`). The deep blue
is a fully saturated blue from the flame's core to its rim, a stop darker or brighter too, and
dimmer than the yellow flame: a brighter blue turns purple at the core under ACES. Every light on
the candle and its flames follows its bluest flame, set to the colour the flame shows on screen (a
Tier 2 Firelight's own "Point light.1" included), so a wall it lights shows the flame's own hue,
visible from the doorway or on a video camera. The blue light is darker than the warm one, as a
blue flame is.

For the Hydra the same path ends in green: white-hot, then a pale green at the limit, then a deep
green. The game ships no green flame. The deep green is a saturated green, dimmer than the deep
blue, since ACES washes a brighter green out to lime. The pale green is a mint as washed out as the
pale blue, with no more red than blue, so players see it as plainly neither the pale blue nor a lit
candle's warm white: on zero evidence that stop is the Hydra's whole tell. The lights on the walls
turn green with it, at the flame's own hue.

## The journal

Every journal on every client gets an eighth row on the evidence page, "Spectral Flame", cloned
from the Spirit Box row, registered with the journal's evidence buttons and their synchronisation.
It marks, crosses out and filters the ghost list like the others.

## How it works

* The host works out each flame's blueness every frame (`mods/spectral_flame/flame_rules.h`, tested
  offline) and sends every client the blue ones four times a second on the mod's channel; every client
  shows the colour (each flame's own material and its candle's wax glow; each candle's lights once
  per candle, with its bluest flame), burns a blue candle down faster and keeps it out of the sanity
  effect. Every client makes the grey flame picture when it first finds a candle flame: the game's
  picture is drawn into a render texture, read back, greyed (`flame_rules.h` `to_grey`), and the
  tint that shows it as the game's own did is fitted to it (`grey_match`, both tested offline); it is
  kept until the contract ends, when every flame gets the game's own picture and tint back. The host
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
