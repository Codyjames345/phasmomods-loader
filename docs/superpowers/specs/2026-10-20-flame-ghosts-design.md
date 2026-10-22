# The flame ghosts: design

Status: design, not yet approved. Game build 25811160. Depends on
`2026-10-20-spectral-flame-design.md` (the Spectral Flame evidence, evidence value 8).

## Intent

Twelve custom ghost types carry Spectral Flame, so the new evidence turns up about as often as a
vanilla one (each vanilla evidence appears in about 13 of the 30 vanilla types). Two are the
existing Draugr and Kikimora, which swap Ghost Orbs for Spectral Flame (see the Spectral Flame
design). Ten are new, added to Custom Ghosts (`mods/custom_ghosts`). Every one has one strength,
one weakness and a tell that no vanilla ghost shares, so investigators can identify it by play, not
only by evidence.

Settled with the user on 2026-10-20. Names and inspirations are original wording; no character or
trademark from a game or film is used.

## Goals

- Ten new ghost types (GhostType 36 to 45), each a full custom type as described in
  `docs/custom-ghosts.md`: in the type roll, the journal (a guess button and a page), the
  investigators' guess and the results, with GhostType.None as its behaviour type.
- Every rule below works for the AI ghost. Where Playable Ghost lets a player control one, the
  rules bind the player as they do for the existing custom ghosts.
- Every type works in single player and is more interesting in multiplayer, never the reverse.

## Non-goals

- Any change to a vanilla ghost type.
- New evidence beyond Spectral Flame.
- Changes to the Echo, Dullahan or Black Shuck.

## Anti-goals

- A tell or strength that makes a vanilla type's evidence or behaviour lie. Readings stay honest;
  only these ghosts' own rules differ.
- A counter that switches a ghost off completely (shutting every door, standing in one spot).
  Every weakness buys time or information, never immunity.
- A hunt rule that cannot be survived by a player who has read the journal page.

## The evidence spread

Each evidence appears 3 or 4 times beside Spectral Flame across the twelve.

| Ghost | Evidence |
| --- | --- |
| Draugr (existing) | EMF 5, Freezing, Spectral Flame |
| Kikimora (existing) | Ultraviolet, Ghost Writing, Spectral Flame |
| Lithobolia | Spirit Box, Freezing, Spectral Flame |
| Gremlin | EMF 5, D.O.T.S., Spectral Flame |
| Brownie | EMF 5, Ghost Writing, Spectral Flame |
| Bogeyman | Ultraviolet, Ghost Orbs, Spectral Flame |
| Lethe | Ghost Orbs, Freezing, Spectral Flame |
| Mesmer | Spirit Box, D.O.T.S., Spectral Flame |
| Knell | Spirit Box, Ghost Orbs, Spectral Flame |
| Asterion | Ghost Writing, D.O.T.S., Spectral Flame |
| Tempest | Spirit Box, Ultraviolet, Spectral Flame |
| Hydra | Ghost Orbs, D.O.T.S., Spectral Flame |

Totals beside the flame: EMF 3, Spirit Box 4, Ultraviolet 3, Ghost Orbs 4, Ghost Writing 3,
Freezing 3, D.O.T.S. 4. Every combination is unique among all 45 types.

## The ten new ghosts

Numbers are starting points for playtesting, kept in one place (`mods/custom_ghosts/ghosts.h`) so
they can be tuned.

### Lithobolia
After the 1682 "stone-throwing devil" of New Hampshire.
- **Strength:** throws objects *at* players; a hit costs extra sanity.
- **Weakness:** cannot throw at a player it has no line of sight to.
- **Tell:** thrown objects fly toward people. A vanilla throw goes in any direction.

### Gremlin
After the saboteurs of machines in Second World War airmen's folklore.
- **Strength:** switches off placed electronic equipment (video cameras, motion and sound sensors,
  the D.O.T.S. projector, and similar).
- **Weakness:** cannot switch off equipment that a player holds or stands next to.
- **Tell:** placed electronics found switched off, which no vanilla ghost does.

### Brownie
After the Scottish household spirit that only works unseen.
- **Speed:** very fast in a hunt while no player is looking at it; quiet footsteps in a hunt, heard
  only from close by, as the Myling's.
- **Weakness:** slows to less than a walk while any player is looking at it (changed from freezing on
  2026-10-20, so staring at it buys time but never immunity), and never gains the hunt's
  line-of-sight speed-up. Changed 2026-10-21: unseen it is the fastest ghost (2.25 times the game's
  base, 3.83 m/s, above the Moroi's 3.71), and watched it walks at 0.6 times the base (1.02 m/s)
  rather than crawling, so it does not play like the Deogen (3.0 m/s far, 0.4 m/s close).
- **Tell:** none of its interactions happen in view of a player or a video camera.

### Bogeyman
After the thing in the closet.
- **Strength:** in a hunt it checks closets and lockers first; hiding places are not safe.
- **Weakness:** in open rooms it is slower and loses sight of players sooner.
- **Tell:** closet and locker doors creak open by themselves outside hunts.

### Lethe
After the river of forgetting in the Greek underworld.
- **Strength:** photos, videos and recordings of its activity fade from the journal about 3 minutes
  after they are taken, unless someone looks at them in the journal first.
- **Weakness:** in a hunt it gives up on a player it has lost sight of much sooner than other ghosts.
- **Tell:** deja vu. Each of its ghost events plays again exactly (same place, form and sound) a
  few minutes later.

### Mesmer
After Franz Mesmer and his spellbound audiences.
- **Behaviour:** every ghost event is staged separately for each player: a different form (another
  of the game's ghost models), a different sound, and the apparition a few metres from where the
  others see it. In a hunt each player sees a different form, always in its true position.
- **Strength:** the truck's sanity monitor shows the team up to about 20% saner than they are.
- **Weakness:** it cannot fool a camera. Video cameras, photos and the D.O.T.S. projector show its
  true form and place.
- **Tells:** lights in its room dim slowly instead of flickering (solo and multiplayer); teammates'
  accounts of the same event never agree (multiplayer); what you saw live does not match the video
  camera footage (solo).

### Knell
After the slow toll of a funeral bell.
- **Speed:** in a hunt its speed swings on a strict cycle of about 2 s between very fast and a crawl
  at a third of its speed. Corrected 2026-10-21: the crawl (0.6 to 0.98 m/s) is slower than every
  vanilla ghost's ordinary pace, but a Deogen up close (0.4) and a Deildegast that has moved many
  things are slower still, so the journal no longer says "slower than any other ghost".
- **Counterplay:** sprint on its slow beat.
- **Tell:** its footsteps keep a perfectly even rhythm, like a metronome.

### Asterion
After the Minotaur of the labyrinth, by its own name.
- **Strength:** every door it passes in a hunt slams shut behind it, boxing players in.
- **Weakness:** it cannot open a closed door. It batters it for about 3 s (loud bangs that give away
  where it is) until it bursts open, and that door then stays jammed open for the rest of the
  contract. Shutting doors buys time once per door; it never stops it.
- **Tell:** doors found shut along the path of its hunts, and doors jammed open after them.

### Tempest
After the storm-raisers of weather-witch folklore. Reworked 2026-10-22 (approved by the user): the
first version changed the weather mid-contract, which no other ghost does, so it gave itself away at
once, and its hunt gate held back hunts far too long at 0% starting sanity.
- **Behaviour:** it never changes the weather; it hunts with the contract's own. Every hunt opens with
  a lightning strike, and more follow as often as the weather says: every 3 to 6 s in heavy rain (the
  game's own storm lightning counts too), 6 to 10 s in rain, 9 to 14 s in any other weather.
- **Strength:** in a hunt it is invisible except when lightning strikes; for 1.5 s after a strike it
  shows in full and runs very fast (2.25 times the game's base speed).
- **Weakness:** in the dark between strikes it is slow (0.6 times the base) and never speeds up on
  sight of its prey; on a clear night its strikes are rare.
- **Tell:** lightning flashes the moment its ghost events begin.

### Hydra
After the many-headed serpent: cut one head and two grow.
- **Behaviour:** smudge it during a hunt and it splits into two apparitions, only one of them real.
  Each further smudge in the same hunt adds another decoy, up to three.
- **Weakness:** decoys are harmless and make no footstep sounds; the real one does.
- **Tells:** two flickering forms in one hunt; some of its ghost events briefly show two
  apparitions, one fading.

## The journal

45 ghost types need a new layout for the evidence spread's ghost page (now 3 x 12 for 35). The
builder chooses the grid (3 x 15 is the obvious one), following `mods/custom_ghosts/pages.cpp`, and
checks that every name fits its button.

## Constraints

- All of `docs/custom-ghosts.md` applies: a custom type only in a private room where every client
  runs the mod, entries in GhostCollection only while a contract is loaded, names through the
  game's localisation keys.
- Custom Ghosts requires Spectral Flame (see the Spectral Flame design).
- Every game fact a rule relies on (doors, closets, equipment power, the weather, ghost models,
  line of sight, the photo and media lists, the sanity monitor) is checked by reverse engineering
  with REA before it is built on.

## Left to the builder

- How each rule is hooked into the game.
- The order the ten are built in. The designer's suggestion, cheapest first: Knell, Hydra,
  Asterion, Lithobolia, Gremlin, Bogeyman, Brownie, Lethe, Tempest, Mesmer.
- Exact speeds, timings and ranges within the descriptions above.

## Known risks

- Mesmer needs a ghost event to render differently on each client; Tempest needs the game's
  lightning played in a contract; Lethe needs journal media removed. All three need new reverse
  engineering and may change shape if the game does not allow them.
- Ten new types roughly doubles Custom Ghosts. It ships in batches, each tested in the game before
  the next.
