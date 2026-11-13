# Flames

How the game's flames burn, go out and are blown out, which of them are the Firelight a player buys,
how a flame's room is told, and how a lit candle eases sanity drain; then what Spectral Flame
(`mods/spectral_flame`) reads and changes. The entries are in `sdk/map/FireSource.h`, `Candle.h`
(`Candle`, `CandleObject`, `Candlebra`), `Equipment.h` (`storeItem`, `placed`, `StoreItem.tier`),
`PlayerSanity.h`, `LevelController.h` (`fireSources`, `rooms`, `currentGhostRoom`), `LevelRoom.h` and
`UnityRendering.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam
build 25811160 (`re/decomp.mjs`).

## The flames of a level

Every flame is a `FireSource`. Its `Start` adds it to `LevelController.instance`'s `fireSources`, so
that list holds every flame of the level once it has started: Firelight candles and lanterns,
candelabra candles, and the level's other candles (a summoning circle's, an altar's).

- `FireSource.Light` enables `flameLight`, switches `flame` on, plays its particles, sets
  `isOn` and warms its heat spot. `Extinguish` does the reverse.
- Each flame's flicker comes from a `LightPulse` component on the same object. It flickers only the
  light in practice: the renderer it is given is null on every Firelight, so its emission part of the
  flicker never runs.
- `FireSource.BlowOutFire(byGhost)` does nothing while `isOn` is clear. Otherwise it puts out the
  `equipment` it belongs to and sends `BlowOutFireNetworked` to every client. The ghost calls it on the
  master.
- Each blow-out by the ghost runs `FireSource.CountGhostBlowOut`, which counts the Onryo's blow-outs
  toward its hunt ([the vanilla ghosts](vanilla-ghosts.md), "Onryo").
- A `Candle` is an `Equipment`. Its `candleObjects` are its flames, each a `CandleObject` with its
  `fireSource` and its wax renderers (`candleRend`). Only the tier 3 lantern has separate truck, held
  and world models, and the game switches between them when it is put down and picked up; every other
  flame keeps the one model it starts with.
- `Candle.UseNetworked` lights or puts out every flame, on every client. It sets each wax material's
  `_EmissionColor` to `candleEmissionColor` when lit and to black when out. The tier 3 lantern's wax gets
  no glow.
- `Candle.Update` runs while the candle's Equipment on flag is set. It takes the frame's time off
  `candleDuration`, and melts the wax by what is left, leaving the lantern's wax as it is.
- `Equipment.storeItem` is the store's `StoreItem`, whose `tier` is One, Two or Three. It is null for an
  item the store does not sell, so a level's own candle has none.
- A `Candlebra` holds `allCandles`, and its `Start` sets its own `lightSource` by how many of them burn.
- `Equipment.placed` is set by `PlaceNetworked` and cleared by `GrabNetworked`, on every client.

## Rooms

`LevelController.rooms` is the level's `LevelRoom[]`. The master's `SyncCurrentGhostRoom` searches it
for the ghost's room and sends the index; `SyncCurrentGhostRoomNetworked` stores `rooms[index]` in
`currentGhostRoom` on every client. A room is made of the colliders in `LevelRoom.roomColliders` (its
`Awake` fills it with the room object's own colliders when the level left it empty). A point is in a
room when it lies inside one of their world-space bounds.

## Sanity and Firelight

When `PlayerSanity.Update` drains sanity, it looks through its list of candles near the player. It takes
the first one for which a lambda of its closure class answers true (`Enumerable.FirstOrDefault`, with the
delegate it caches in `<>c`'s static field). With such a candle, it scales the drain by that candle's
`StoreItem.tier`.

The lambda is mapped as `PlayerSanity.CandleIsOn`. It answers the candle's Equipment on flag, the one
`Candle.Update` tests. `Update` reads the list nowhere else, and the lambda has no other caller. So an
answer of false while `Update` runs leaves that candle out of the drain, and changes nothing else.

## What Spectral Flame does with them

- It keeps the flames of `fireSources` whose `equipment` is a `Candle` with a store item (Firelight) or
  in a candelabra; the tier gives each its kind. A flame counts as set down when `placed` (a
  candelabra's always), and the ghost is in its room when the flame's position lies in the room that
  is `currentGhostRoom`.
- It hooks `PlayerSanity.Update` and `CandleIsOn`: inside `Update`, a candle whose bluest flame is
  more than half blue answers false, so it eases no drain.
- It hooks `BlowOutFire` on the master, and calls it for its own extra blow-out.
- A blue candle burns down faster: it takes extra time off `candleDuration` while a flame burns.
- It recolours three things, through Unity's own material, light and texture functions:
  - each flame's material: the Candle Flame shader's `_TintColour`, over a grey copy of its `_CandleSprite`,
    and the Lantern Flame shader's `_Color_2` and `_Color_1`;
  - the wax glow: the Candle Melt shader's `_EmissionColor`;
  - the lights.

  The shader properties are ShaderProperty entries (`UnityRendering.h`). They were checked offline in the
  Tanglewood_Street_House scene's shared assets, where `Candle Flame` comes from the main menu's and the
  other two from the scene's own. A material counts as a flame's only when it has the property, and a
  property that did not resolve leaves what it colours as the game's. The texture functions that make the
  grey copy are optional entries: without them it tints the game's sprite. The grey copy matters
  because the game's own candle sprite is itself coloured (a white core, a yellow body, an orange tip
  and a blue base), so a tint over it cannot reach a pure hue.
- On the master it counts the ghost as busy, and never adds its own blow-out, while `GhostAI.state`
  is `hunting`, `appear` or `randomEvent` (`GhostAI.h` says what ChangeState does with each).
