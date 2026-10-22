# Custom ghosts

Status: in development. The code is the authority on what is implemented; `FINDINGS.md` lists what
has not run in the game yet.

Fifteen new ghost types, a separate mod of the suite (`mods/custom_ghosts`). They take part only in a
private room in which every client runs the mod: a client without it would have no journal page,
evidence or name for them.

Custom Ghosts requires the Spectral Flame mod (`docs/spectral-flame.md`, `requires=spectral-flame` in
its `mod.ini`): some of its ghosts carry that evidence, so they join a contract only when Spectral
Flame is running for every player too (its service, `core/services.h`). Switching Custom Ghosts on
in the launcher or the in-game Mods panel switches Spectral Flame on; switching Spectral Flame off
switches Custom Ghosts off.

Difficulty is how readily the ghost gives itself away when its evidence is withheld. Easy ghosts show their tell in almost every hunt or event; Medium ones need a test or a setup (salt, a camera, a flame, a hiding place); Hard ones need patience or several players comparing notes.

| Ghost | Difficulty | Evidence | Strength | Weakness |
| --- | --- | --- | --- | --- |
| Echo | Hard | Spirit Box, Freezing Temperatures, D.O.T.S. | Calls out in the voice of a player it has heard, from wherever it is, and repeats what players do in its room: within a minute of a player using a light switch or a door in its favourite room, its next interaction once its own interaction sensor detects that switch or door uses it (only the latest such use counts), dormant or not | With nobody heard near it for a minute it falls quiet: most of its interactions and events do not happen |
| Dullahan | Easy | EMF 5, Ghost Orbs, Ghost Writing | Very fast in a hunt on long, straight runs (3.4 m/s, twice the game's base speed) | Turns slowly (45 degrees a second) and loses most of its speed at every corner (0.43 m/s facing away from its path) |
| Kikimora | Medium | Ultraviolet, Ghost Writing, Spectral Flame | Carries off equipment lying loose near it and hides it in another dark, empty room, leaving EMF where it was | Never touches anything in a room with the lights on |
| Draugr | Medium | EMF 5, Freezing Temperatures, Spectral Flame | While anything taken from its room is outside it, it may hunt from 65% sanity | Never changes favourite room; in a hunt it runs at 2.8 m/s in its room and slows 0.15 m/s for every metre away from it, to a crawl of 0.3 m/s from about 17 m |
| Black Shuck | Medium | Ultraviolet, Freezing Temperatures, D.O.T.S. | Blind and deaf in a hunt: it never sees players, hears their voices or senses their equipment, but picks up a player's scent trail within 8 m (where they walked in the last 30 s) and follows it step for step, round every corner, to them, and the game's own hunt kill takes them. Closets do not hide you: a trail that ends at a closet or locker leads it there, and the game's own closet search finds you | Salt or running water breaks a trail, and it loses anyone on the other side: a running tap (a sink whose water is flowing; baths and showers only where the game makes them sinks), or rain outdoors, where no trail is laid. Smudges stun it as they stun any ghost |
| Lithobolia | Easy | Spirit Box, Freezing Temperatures, Spectral Flame | Throws objects only at a player the ghost's own vision sensor detects within 8 m, and in a hunt what it throws flies at its target if it sees them within 8 m; a hit costs 5% sanity | With nobody in sight it throws nothing at all |
| Gremlin | Easy | EMF 5, D.O.T.S., Spectral Flame | Half of its interactions with running electronics in its room switch one off (EMF where it stands) | Never touches equipment within 2 m of a living player |
| Brownie | Medium | EMF 5, Ghost Writing, Spectral Flame | In a hunt it is very fast (3.83 m/s, 2.25 times the game's base speed) while nobody looks at it; its footsteps carry 8 m less far, as a hunting Myling's do (12 m instead of 20 m) | In a hunt it slows to 1.02 m/s while any player looks at it, and never speeds up on sight of its prey. Outside hunts it never interacts or causes an event while a player or a running video camera can see it |
| Bogeyman | Medium | Ultraviolet, Ghost Orbs, Spectral Flame | Each hunt begins from the closet or locker nearest the youngest living player (the lowest prestige, then the lowest level, as the lobby shows them; ties go to the nearest), other than one they hide in, when one lies within 10 m of them on their floor (at most 2 m above or below them): the game's own ghost teleport moves it there and the door opens as it steps out. In a hunt it goes for the youngest of the players it sees, and with nobody in sight it searches the closets and hiding spots, nearest first (8 s at most on one it cannot reach). Outside hunts, 3 in 4 of its door interactions are on a closet door when the ghost's own door sensor detects one | Slower in open rooms, where it loses prey beyond 7 m |
| Lethe | Hard | Ghost Orbs, Freezing Temperatures, Spectral Flame | Photos, videos and recordings of its activity fade from the journal after three minutes unless someone looks at them first | In a hunt it forgets prey out of its sight for 1 s |
| Mesmer | Hard | Spirit Box, D.O.T.S., Spectral Flame | Each player sees its appearances as another form, elsewhere, and its hunts as another form where it truly is; the truck monitor shows the team a fifth saner; the lights in its room dim slowly instead of flickering | Video cameras, photos and the D.O.T.S. projector show it as it is |
| Knell | Easy | Spirit Box, Ghost Orbs, Spectral Flame | In a hunt it surges to 1.75 times the base speed (2.98 m/s) for one second in every two | In between it crawls at about a third of the base speed (0.6 m/s, 0.35 times the base); the game's own footsteps quicken in every surge and slow in every crawl |
| Asterion | Easy | Ghost Writing, D.O.T.S., Spectral Flame | A closed door never stops it for good: it batters it down, and the door stays broken open; it never closes a door | Battering a door down takes it 3 s, and it can be heard far away |
| Tempest | Easy | Spirit Box, Ultraviolet, Spectral Flame | In a hunt it cannot be seen except when lightning strikes: for 1.5 s after each strike it shows in full and runs at 3.83 m/s (2.25 times the game's base speed). Every hunt opens with a strike; more follow every 3 to 6 s in heavy rain (the game's own storm lightning counts too), every 6 to 10 s in rain and every 9 to 14 s in any other weather | In the dark between strikes it crawls at 1.02 m/s and never speeds up on sight of its prey. It never changes the weather; outside hunts, lightning flashes as each of its events begins |
| Hydra | Medium | Ghost Orbs, D.O.T.S., Spectral Flame | Each smudge in a hunt grows a false head (at most three) that walks and flickers as it does. Its Spectral Flame burns green where another ghost's turns blue: pale green at the false-positive limit even when the evidence is withheld, deep green when it is given | The false heads are harmless and silent |

Each has a combination of three evidence types that no other ghost, vanilla or custom, shares. The
Echo, Dullahan and Black Shuck use three of the five combinations of the game's seven evidence types
that the 30 vanilla types leave free; the other twelve carry Spectral Flame, about as often as a
vanilla evidence turns up, with each other evidence beside it three or four times (design:
`docs/superpowers/specs/2026-10-20-flame-ghosts-design.md`).

Speeds work as the game's own do. Its hunt update gives a ghost its own speed (the 1.7 m/s base every ghost
gets, times the line-of-sight speed-up, which grows 0.05 a second to 1.65 while it sees its target) and then
the multipliers every type shares: the difficulty's ghost speed and Blood Moon's 1.15. A custom rule writes
the inputs of that sum, never the speed it produces: the hunt state's base, and the line-of-sight speed-up
held at 1 where the rule has none. It writes them every frame and again right after the game's two-second
base refresh. The multipliers then apply once, with no factor on the final speed and no caps, and the
footsteps, run sound and walk animation read the real speed. Speeds above are before the multipliers
(`speed_rule.h` `OwnSpeed`).

Where each rule lives: `knell_rule.h` and the speed rules in `behaviour.cpp`; `hydra.cpp` and the
shared `apparition.cpp` (copies of the ghost's model that only show); `asterion.cpp`; `lithobolia.cpp`
and `gremlin.cpp`, reached through `restraint.cpp`'s hooks on the interaction roll and the state change;
`bogeyman.cpp`; `brownie.cpp`; `lethe.cpp`; `tempest.cpp`; `mesmer.cpp`. Each has its arithmetic in a
`*_rule.h` header tested offline. The flame ghosts' messages share the mod's channel, starting
`'C' 'G' 'F'` (`flame_net.h`).

## How they become ghost types

* **Identity and behaviour.** The game keeps two types for a ghost (the Mimic is the vanilla case):
  what it is (`GhostInfo +0x28`) and whose rules its AI follows (`+0x2C`). A custom ghost is its own
  new type (31 to 45) for everything that names or judges it, and `GhostType.None` for its AI, set in
  `GhostAI.Init` and `GhostInfo.SyncValuesNetworked` on every client before the AI runs. The host's
  `GhostInfo.SyncValues` sends the AI's type as the ghost's type and every client copies it to both
  fields, so for a custom ghost it sends the custom type instead. No vanilla type's rules apply to
  it; only its own rules set it apart.
* **The type roll and evidence.** While a contract is loaded, a `GhostData` for each is in the game's
  `GhostCollection`; the weighted type roll, the evidence a ghost gives and the journal's lists all
  read it. Whether a contract has them is decided once, when it loads (every player in the private
  room runs the mod), and holds until it unloads; a contract without them loses any the lobby left
  in a collection it shares.
* **The custom difficulty.** In a private lobby where every player runs the mod, the Ghost Type
  option of Custom 1, 2 and 3 offers the custom ghosts among the game's own: they join the menu's
  `GhostCollection`, each in its alphabetical place (the game's ghosts are sorted by their asset names,
  the names in their `Journal_<name>Title` keys, so Asterion comes first), and the option's count (31
  as the menu scene ships it) grows to 46. The game's
  own code does the rest: the option's change handler writes the pick to
  `Difficulty.ghostTypeOverride`, which sets the reward multiplier to 0 and makes the contract's
  spawn force that type. A custom pick never leaves the game, because the unmodded game has no data
  for it. The save file always gets Random (`ES3.Save`), and so does the lobby sync while anyone in
  the room is without the mod (`ES3.Serialize`, asked as each sync is sent, so a player who has only
  just joined counts). The mod keeps each preset's pick in `custom_difficulty.txt` beside its log.
  Picks are learnt from the option's arrows and dice only, so a preset loaded or shown never changes
  them. While everyone runs the mod, any custom preset the game holds at Random gets its pick back
  (the game writes Random nowhere else but the option's own Random, which forgets the pick), and so
  does the host's lobby difficulty, which the host then sends. When someone without the mod joins,
  every custom pick in the lobby goes back to Random until they leave, the option offers the game's
  ghosts only, and the host sends Random. Switching the mod off does the same before it stops. The
  game shows a synced difficulty by its ghost's place in the menu's collection, and Random when the
  ghost is not there yet, and offering or withdrawing the custom ghosts moves every ghost after their
  places, so whenever the option's place misses the lobby's ghost the mod has the game's own handler
  name it again. The difficulty's summary names its ghost by `GhostType.ToString()`, a number for a
  type with no enum name; while the summary is written, the mod answers that call with the custom
  ghost's name.
* **Names.** The game names a type through the localisation key `Journal_<type>Title`; the mod
  answers those keys for its types.
* **The journal.** Every journal on every client gets, for each custom ghost, a guess button on the
  evidence spread's ghost page (a clone of a vanilla one, registered with the journal's ghost lists
  and its synchronisation, crossed out by its own evidence filter), with the grid's rows shortened
  so all 45 fit in 3 columns (15 rows), alphabetically; and an entry in the Ghosts tab's list with a
  page of its own. That list's entries are placed by hand and its GridLayoutGroup ships switched
  off, so the mod sets the grid to the layout the entries were given (250 x 100 cells, three
  columns), switches it on, and shortens its rows the same way. The journal's choice is the
  players' guess, judged as for any ghost.

## With the playable ghost

A player can be any of them. Their rules apply as to the AI ghost, except where the player steers
(the Black Shuck does not follow trails for a player, who sees the scent trail in a hunt instead, in
the darkened view Playable Ghost gives the Kormos, and hunts by their own judgement; the game's sight
and hearing stay as they are for them; a Bogeyman player's hunts begin where they stand, its sight
does not favour the youngest for them, and the doors it opens are the ones they choose; an Echo player's
interactions are the ones they aim, never a repeat of what the others did). The Echo's ability ("Speak") and the Kikimora's ("Tidy away") are the
ghost player's ability for those types, granted at the vanilla rate of the ability roll. Their
weaknesses bind a player too: a dormant Echo cannot interact or cause events until a voice wakes it
(the AI loses three in four), and a Kikimora touches nothing in a room with the lights on. The
ghost player's HUD says so (the Custom Ghosts service, `core/services.h`).

## Voices

The Echo's voice comes from what each client already receives: the game hands every client each
other player's voice through Vivox participant audio taps. While an Echo haunts the contract, every
client keeps the last minute of what each other player said, at telephone quality, in memory
only, and only for players whose own client runs the mod (who agreed to the notice). When it
speaks, a client that heard the chosen player sends their last phrase (0.5 to 3 s) to everyone
as G.711 mu-law audio on the mod's channel, and every client plays it from where the ghost was, a
shade lower than the original. Every client tells the host whose last phrase it holds every two
seconds; the Echo speaks only as someone whose phrase a client holds, and that client sends it.
A voice whose audio has stopped arriving (a player who died or left) no longer wakes the Echo.
Recordings are dropped when the contract ends.
