# Custom ghosts

Status: in development. The code is the authority on what is implemented; `FINDINGS.md` lists what
has not run in the game yet.

Sixteen new ghost types, a separate mod of the suite (`mods/custom_ghosts`). The mod registers each
through the SDK's content layer (`sdk/api/content.h`, `mods/custom_ghosts/registration.h`) under the id
`phasmomods.custom-ghosts.<name in lower case, spaces as _>` (`phasmomods.custom-ghosts.black_shuck`), with its English
journal text, its evidence and its ability, and attaches its rules to that id. The host does the rest
(`sdk/README.md`): it numbers the ghosts after the game's own types, in the byte order of their ids,
and they take part only in a private room, on a client whose mods match the host's list. The host removes a
client without the mod, which would have no journal page, evidence or name for them, and no contract starts until it is gone. The mod keeps no check of
its own on the other players.

Custom Ghosts requires the Spectral Flame mod (`docs/spectral-flame.md`, a required dependency in
its `manifest.json`): twelve of its ghosts give that evidence (by its content id, `phasmomods.spectral-flame.flame`), and
once every mod has started the host drops any ghost whose evidence no running mod registered.
Switching Custom Ghosts on in the launcher or the in-game Mods panel switches Spectral Flame on;
switching Spectral Flame off switches Custom Ghosts off.

Difficulty is how readily the ghost gives itself away when its evidence is withheld. Easy ghosts show their tell in almost every hunt or event; Medium ones need a test or a setup (salt, a camera, a flame, a hiding place); Hard ones need patience or several players comparing notes.

| Ghost | Difficulty | Evidence | Strength | Weakness |
| --- | --- | --- | --- | --- |
| Echo | Hard | Spirit Box, Freezing Temperatures, D.O.T.S. | Calls out in the voice of a player it has heard, from wherever it is, and repeats what players do in its room: within a minute of a player using a light switch or a door in its favourite room, its next interaction once its own interaction sensor detects that switch or door uses it (only the latest such use counts), dormant or not | With nobody heard near it for a minute it falls quiet: it does not speak, and 3 in 4 of its interactions and events do not happen, though a repeat always does |
| Dullahan | Easy | EMF 5, Ghost Orbs, Ghost Writing | Very fast in a hunt on long, straight runs (3.4 m/s, twice the game's base speed) | Turns slowly (45 degrees a second) and loses most of its speed at every corner (0.43 m/s facing away from its path) |
| Kikimora | Medium | Ultraviolet, Ghost Writing, Spectral Flame | Carries off equipment lying loose near it and hides it in another dark, empty room, leaving EMF where it was | Never touches anything in a room with the lights on |
| Draugr | Medium | EMF 5, Spectral Flame, Freezing Temperatures (a difficulty that withholds evidence always leaves Freezing, as it leaves the Goryo's D.O.T.S.) | Its things are the house's own props that lay in its room when its rules started (never equipment). While any of them is outside the room it is angered: it may hunt from 65% sanity, and its hunts go only for the living player who most recently carried one out, as the game's Banshee keeps to its one target, until they are in the truck, dead or killed (`host/content_target.h`) | Keeps its room as the Goryo does: its wander never takes the long roam after which a ghost may change rooms (a Monkey Paw wish can still move it, and its things are then what lies in the new room, its anger and thieves forgotten). While angered, outside hunts it takes its things back one at a time to the very place each lay (every 25 to 45 s, as the Kikimora hides things, never one a player carries), and its room is 3 °C colder on every client than the game's own temperature, a tell, not evidence: it may read below freezing even when the contract withholds Freezing, as the Mimic's orbs show on any evidence. In a hunt it runs at 2.8 m/s in its room and slows 0.15 m/s for every metre away from it, to a crawl of 0.3 m/s from about 17 m |
| Black Shuck | Medium | Ultraviolet, Freezing Temperatures, D.O.T.S. | Blind and deaf in a hunt: it never sees players, hears their voices or senses their equipment, but picks up a player's scent trail within 8 m (where they walked in the last 30 s) and follows it step for step, round every corner, to them, and the game's own hunt kill takes them. Closets do not hide you: a trail that ends at a closet or locker leads it there, and the game's own closet search finds you | Salt or running water breaks a trail, and it loses anyone on the other side: a running tap (a sink whose water is flowing; baths and showers only where the game makes them sinks), or rain outdoors, where no trail is laid. Smudges stun it as they stun any ghost |
| Lithobolia | Easy | Spirit Box, Freezing Temperatures, Spectral Flame | Throws objects only at a player the ghost's own vision sensor detects within 8 m, and in a hunt what it throws flies at its target if it sees them within 8 m; a hit costs 5% sanity | With nobody in sight it throws nothing at all |
| Gremlin | Easy | EMF 5, D.O.T.S., Spectral Flame | Half of its interactions with running electronics in its room switch one off (EMF where it stands) | Never touches equipment within 2 m of a living player |
| Brownie | Medium | EMF 5, Ghost Writing, Spectral Flame | In a hunt it is very fast (3.83 m/s, 2.25 times the game's base speed) while nobody looks at it, and then its footsteps make no sound: players hear nothing, and the sound sensor and the parabolic microphone never detect them (no reading, no radar point, no playback). The steps still fall, with their timing and prints | In a hunt it slows to 1.02 m/s while any player looks at it, or views it on the truck's monitor, never speeds up on sight of its prey, and its footsteps carry 8 m less far, as a hunting Myling's do (12 m instead of 20 m). Outside hunts its footsteps are the game's own, and it never interacts or causes an event while a player or a running video camera can see it |
| Bogeyman | Medium | Ultraviolet, Ghost Orbs, Spectral Flame | Each hunt begins from the closet or locker nearest the youngest living player (the lowest prestige, then the lowest level, as the lobby shows them; ties go to the player who joined first), other than one they hide in, when one lies within 10 m of them on their floor (at most 2 m above or below them): the game's own ghost teleport moves it there and the door opens as it steps out. Its hunts go only for that youngest living player, as the game's Banshee keeps to its one target: while they are in the truck it goes for anyone it hears and its sight stays on them, and when they die or it kills them it takes the next youngest. With no target it searches the closets and hiding spots, nearest first (8 s at most on one it cannot reach). Outside hunts, 3 in 4 of its door interactions are on a closet door when the ghost's own door sensor detects one | Slower in open rooms, where it loses prey beyond 7 m |
| Lethe | Hard | Ghost Orbs, Freezing Temperatures, Spectral Flame | Photos, videos and recordings of its activity fade from the journal after three minutes unless someone looks at them first | In a hunt it forgets prey out of its sight for 1 s |
| Mesmer | Hard | Spirit Box, D.O.T.S., Spectral Flame | Each player sees it as one other form for the whole contract, a different form from other players: a little way off in its appearances, walking as it walks, and where it truly is in hunts; the truck monitor shows the team a fifth saner; the lights in its room dim slowly instead of flickering | Video cameras, photos and the D.O.T.S. projector show it as it is |
| Knell | Easy | Spirit Box, Ghost Orbs, Spectral Flame | In a hunt it surges to 1.75 times the base speed (2.98 m/s) for one second in every two | In between it crawls at about a third of the base speed (0.6 m/s, 0.35 times the base); the game's own footsteps quicken in every surge and slow in every crawl |
| Asterion | Easy | Ghost Writing, D.O.T.S., Spectral Flame | A closed door never stops it for good: it batters it down, and the door stays broken open; it never closes a door | Battering a door down takes it 3 s, and it can be heard far away |
| Tempest | Easy | Spirit Box, Ultraviolet, Spectral Flame | In a hunt it cannot be seen except when lightning strikes: for 1.5 s after each strike it shows in full and runs at 3.83 m/s (2.25 times the game's base speed). Every hunt opens with a strike; more follow every 3 to 6 s in heavy rain (the game's own storm lightning counts too), every 6 to 10 s in rain and every 9 to 14 s in any other weather | In the dark between strikes it crawls at 1.02 m/s and never speeds up on sight of its prey. It never changes the weather; outside hunts, lightning flashes as each of its events begins |
| Hydra | Medium | Ghost Orbs, D.O.T.S., Spectral Flame | Each smudge in a hunt grows a false head (at most three) that walks and flickers as it does, and throws objects as the hunting ghost does: on the hunt's own throw timer and roll, each head picks from the objects its own copy of the ghost's interaction sensor detects around it, then uses the game's test and Prop.TryThrow (with its EMF); no object is thrown twice in one tick. Its Spectral Flame burns green where another ghost's turns blue: pale green at the false-positive limit even when the evidence is withheld, deep green when it is given | The false heads are harmless and silent (they throw, but never kill and make no footsteps) |
| Squatter | (the author's call) | None: no evidence and no paranormal activity at all; it is told apart by sight and sound, being visible and talking ([design](superpowers/specs/2026-11-04-squatter-design.md)) | Not a hunt: once it sees a player it chases them at just under their sprint until every living player is out of the house, kicks whoever it reaches, then locks the exits. It knows a player is in a closet or locker it saw them enter: it kicks a shut one, and opens one that stands ajar and walks in | Clumsy: it trips over props, jumps drops and dive-rolls at its target, each stopping it a moment. It never opens a shut hiding place and gives up kicking at one after 10 s; it never passes through a door, opening each first |

Each has a combination of three evidence types that no other ghost, vanilla or custom, shares. The
Echo, Dullahan and Black Shuck use three of the five combinations of the game's seven evidence types
that the 30 vanilla types leave free; the other twelve carry Spectral Flame, about as often as a
vanilla evidence turns up, with each other evidence beside it three or four times (design:
`docs/superpowers/specs/2026-10-20-flame-ghosts-design.md`).

Speeds work as the game's own do. Its hunt update gives a ghost its own speed (the 1.7 m/s base every ghost
gets, times the line-of-sight speed-up, which grows 0.05 a second to 1.65 while it sees its target) and then
the multipliers every type shares: the difficulty's ghost speed and Blood Moon's 1.15. A custom rule gives
the inputs of that sum, never the speed it produces: the hunt's base, and the line-of-sight speed-up held at
1 where the rule has none. It gives them through its content rule's hunt speed (`PmGhostRules.huntSpeed`),
which the host writes into the hunt state before every speed update. The multipliers then apply once, with
no factor on the final speed and no caps, and the footsteps, run sound and walk animation read the real
speed. Speeds above are before the multipliers (`speed_rule.h` `OwnSpeed`).

Where each rule lives: `knell_rule.h` and the speed rules in `behaviour.cpp`; `hydra.cpp` and the
shared `apparition.cpp` (copies of the ghost's model that only show; the heads' throws are the game's own); `asterion.cpp`; `lithobolia.cpp`
and `gremlin.cpp`, reached through `restraint.cpp`'s hooks on the interaction roll and the state change;
`bogeyman.cpp`; `brownie.cpp`; `lethe.cpp`; `tempest.cpp`; `mesmer.cpp`. Each has its arithmetic in a
`*_rule.h` header tested offline. The flame ghosts' messages share the mod's channel, starting
`'C' 'G' 'F'` (`flame_net.h`).

## How they become ghost types

The SDK's content layer makes them ghost types of the game; the mod gives it only their registrations
and rules (`sdk/README.md` describes the content functions, and each system overview the game's side):

* **Identity and behaviour** (`sdk/systems/ghost-types.md`). A custom ghost is its own type for
  everything that names or judges it, numbered by the host, and the game's "no type" for its AI, so
  no vanilla type's rules apply to it; only its own rules, attached to its id, set it apart. They run
  on every client while the contract's ghost acts as it: its own type, or a Mimic's copy of it.
* **The type roll and evidence.** While a contract that loaded with content holds them, the game's
  `GhostCollection` lists each with its evidence, so the weighted type roll, the evidence a ghost gives
  and the journal's lists read them. Whether a contract has them is decided once, when it loads, and
  holds until it ends.
* **The custom difficulty** (`sdk/systems/difficulty.md`). In a private lobby where the mod lists
  match, the Ghost Type option of Custom 1, 2 and 3 offers them among the game's own, in alphabetical
  order; a custom pick never reaches a save file or a player without the mod. The host keeps each
  preset's pick, by ghost id, in `custom_difficulty.txt`.
* **The journal** (`sdk/systems/journal.md`, `sdk/systems/evidence.md`). Every journal on every
  client, in a contract and in the lobby, gets a guess button, a list entry and a page for each, and
  the Spectral Flame row while a ghost that gives it is shown. The journal's choice is the players'
  guess, judged as for any ghost.

## With the playable ghost

A player can be any of them. Each of their rules runs for a player as for the AI ghost, in the same code, and
the hooks that apply them take no input for who steers:

* The Draugr's and the Bogeyman's hunts are held to the same one target (`host/content_target.h`).
* A Black Shuck player's hunt is blind and deaf, as its AI's is: the game's own sight, voice and electronics
  tests answer for nobody but its prey, who is the player whose trail it walks. It takes them through the same
  `GhostHuntState.SetTarget`, and the same hold on its kill reach and the same handover to the game's closet
  search apply, so it kills under the same conditions. A trail it loses it ignores for 30 s.
* A Bogeyman player's hunts begin from the closet nearest the youngest as its AI's do, 3 in 4 of the doors they
  open are a closet door in reach, and with no target a hiding place they reach is searched, closets through
  `ClosetZone.CheckCloset`.
* An Echo player's interaction is a repeat of the switch or door a player just used in its room, once in reach,
  whatever the aim's ray hits, as its AI's is, and a restless Echo's own roll repeats too. Dormant, 3 in 4 of its
  interactions and events are lost, never a repeat, and it does not speak. A lost action stays paid for: its
  AI's roll is spent whether or not it is lost, and a free one could be pressed again until it took.
* A Lithobolia player's hunt throw flies at someone it sees, as its AI's does.
* A Kikimora touches nothing in a room with the lights on, and a Brownie nothing while it is watched.

The ghost player's HUD says what binds them. Each ghost's definition carries these (`PmGhostDef`: its ability,
`hudState` with its `hudLine`, `heldBack`, `interactionLost`, the Draugr's and the Bogeyman's `target` and, for the
Black Shuck, the scent as `playerView` and its trail corridor as `playerMove`), which Playable Ghost reads through the content layer for
whichever ghost the contract's ghost acts as, never by a type number.

The user allowed these differences between a player and the AI, and no others (2026-10-31):

* **Hearing.** The Yokai player hears any volume within 2.5 m.
* **The timing of abilities.** The Echo's "Speak", the Kikimora's "Tidy away" and the Gremlin's "Short out" are
  used on demand, granted at the vanilla rate of the ability roll, where the AI speaks and hides things on its own
  timers and sabotages inside its interaction roll.
* **Aiming and charges.** Interactions are aimed and paid for, not rolled and picked. A rule that turns an
  interaction still turns the aimed one: the Bogeyman's closet doors, the Echo's repeats.
* **Free steering in hunts.** Nothing walks a player's ghost, so the route is theirs: the Bogeyman's search of the
  nearest hiding place, the Goryo's head for its room with no prey and which trail the Black Shuck takes. The one
  limit is the Black Shuck's corridor: they steer within 1 m of the trails its AI may follow, so salt, running
  water and rain leave gaps they cannot cross. Outside the corridor they may only walk nearer, along the way its
  AI's agent would walk to the trail, round a wall by the door; salt or water on that way stops neither of them. Where
  its scent reaches
  no step (none within 8 m, all behind a break, or standing by running water or salt) its AI roams by the game's
  own hunt, and they steer freely too (`trail.h` `confine`, `sdk/systems/ghost-ai.md`).

## Voices

The Echo's voice comes from what each client already receives: the game hands every client each
other player's voice through Vivox participant audio taps. While an Echo haunts the contract, every
client keeps the last minute of what each other player said, at telephone quality, in memory
only. An Echo's rules run only while the room's mod lists match, so every player's client runs the
mod and every player agreed to the notice. When it
speaks, a client that heard the chosen player sends their last phrase (0.5 to 3 s) to everyone
as G.711 mu-law audio on the mod's channel, and every client plays it from where the ghost was, a
shade lower than the original. Every client tells the host whose last phrase it holds every two
seconds; the Echo speaks only as someone whose phrase a client holds, and that client sends it.
A voice whose audio has stopped arriving (a player who died or left) no longer wakes the Echo.
Recordings are dropped when the contract ends.
