# Ghost AI: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [Ghost AI](../systems/ghost-ai.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## What is mapped

| Key | Kind | Found by |
| --- | --- | --- |
| `GhostAI` | type | name |
| `GhostState` | enum nested in GhostAI (idle, wander, hunting, favouriteRoom and 20 more) | shape: owner and `values` idle, wander, hunting |
| `GhostState.idle`, `.wander`, `.hunting` | enum values | name |
| `GhostAI.state` | field | shape: type `@GhostState` |
| `GhostAI.agent` | field | shape: type `NavMeshAgent` |
| `GhostAI.isHunting` | field | shape `bool` plus facts |
| `GhostAI.postHuntCooldown` | field | shape `bool` plus a fact |
| `GhostAI.info` | field | shape: type `@GhostInfo` |
| `GhostAI.Awake` | method | name |
| `GhostAI.Hunting` | method, a Photon RPC | name, params `bool, int, PhotonMessageInfo` |
| `GhostAI.ChangeState` | method | name, params `@GhostState, PhotonObjectInteract, bool` |
| `GhostAI.UnAppear` | method, RPC to every client | name |
| `GhostAI.LookAtPlayer` | method | name, params `Player`; has twins |
| `GhostAI.TemporarilyStopWander` | method | name, params `float`; has an alias |
| `GhostAI.Init` | method | name, params `@GhostTraits`; has an alias |

The game API is `phasmo::GhostAI` (`type`, `state`, `agent`, `isHunting`, `postHuntCooldown`, `info`,
`Init`, `Awake`, `Hunting`, `ChangeState`, `UnAppear`, `LookAtPlayer`, `TemporarilyStopWander`) and
`phasmo::GhostState` (`type`, `idle`, `wander`, `hunting`). The `GhostState` enum has more values than
are mapped; map one when a mod needs it. Public methods, Unity messages and Photon RPCs keep their
real names. Nearly every field and the state enum are obfuscated.

## The twins finding

The obfuscator adds same-signature copies of real methods, so a signature alone does not identify a
method or its behaviour. Three cases appear in `GhostAI` on build 25811160:

- **Twins**: bodies that read the same once placed. Measured on that build with the strict rule below
  (equal length, equal normalised code, the method's own init flag masked), `GhostAI` has 15 twin pairs.
  `LookAtPlayer` is one method with two twins. A hook with `Twins::All` covers all of them; one on the
  canonical body alone misses calls that reach a twin.
- **Aliases**: two names folded by the linker onto one address. `Init` and its obfuscated look-alike are
  an alias, and `TemporarilyStopWander` has one. One hook on the address covers both names, and the
  host hooks it once.
- **Look-alikes that are not twins**: a method of the same signature whose body differs. The decoy of
  `UnAppear` is one: its body is nearly identical, but one of its instructions loads a different
  runtime-metadata address and its length differs. It is another method, so `UnAppear` correctly has no
  twins. `SetGhostModel` has a same-signature look-alike of a different length, which is not a twin
  either.

The resolver calls two bodies twins only when both were read in full, have the same length, and have the
same normalised instructions (rip-relative operands compared by absolute address, in-function branches
by offset, and the method's own metadata-init flag masked). Code it could not read in full is never
compared. `Twins::CanonicalOnly` keeps the exact behaviour of a hook placed by raw address on the
canonical body.

## How the fingerprints were chosen

`GhostAI` has 13 unnamed instance bool fields. Shape alone cannot tell them apart, so a field is found
by what `GhostAI.Hunting` and `GhostAI.ChangeState` do to it. Both reach the fields through a copy of
`this` (`rdi` in `Hunting`, `rsi` in `ChangeState`), which the resolver requires of a fact in a method
of the field's own type:

- `Hunting` stores five bools, four of them from registers. Of those four, `ChangeState` reads only
  one: that is `isHunting`. Its facts are `Stores Hunting`, `Loads ChangeState`, and `StoresConst
  Hunting 1` negated (the value comes from a register, not a constant). Should `ChangeState` come to
  read another of the four, `isHunting` fails closed (`Ambiguous`), never to the wrong field.
- `postHuntCooldown` is the bool `Hunting` stores the constant 1 into: `StoresConst Hunting 1`.
- `Awake` clears `isHunting` as well, through `rbx`, a copy of `this` it restores from the stack in
  its epilogue rather than with a `pop` (the reader counts such a restore as keeping `this` since
  2026-10-24; before that it could not prove `rbx` held `this`). It clears `postHuntCooldown` and other
  bools with it, so a fact on `Awake` would not single `isHunting` out, which is why `isHunting` does not
  use one.
- Proof that the facts decide: with the cooldown's fact removed, all 13 bool fields match
  (`AMBIGUOUS`); with the constant 2 in place of 1 it is `MISSING`.

Update drill (2026-10-23): renaming the cooldown field and moving the agent in a copy of the dump left
every entry resolved, reporting the agent's new offset and the cooldown as renamed from its old name.
A stale obfuscated name never decides.

## The wander and the favourite room (why the Goryo keeps its room)

Read from decompiles of build 25811160 (2026-10-30). The members a mod reads are mapped; the bullets below name the
others by what they do.

A player's walk reads the state it replaces (`mods/playable_ghost/walk_state.cpp`). `GhostState.wander` makes a
`GhostWanderState` and `GhostState.dots` a `GhostDotsState`, whose constructors set their radius to 3 and 2. Each has
`radius`, `hasDestination` and `destination`, and its reach is the radius or, while it has a point, the distance to that
point when that is further (`tether::reach_of`). In a hunt, `GhostHuntState.target` empty with `GhostHuntState.searchTarget`
alive means it searches.

- **The wander state.** `GhostAI.ChangeState` case 1 (`GhostState.wander`) makes a new state object of the
  obfuscated wander class, `GhostWanderState` (its constructor sets its wander radius to 3); the
  three other `GhostAI` void(state, interact, bool) look-alikes make it
  too. The state machine (`GhostAI.stateMachine`, whose `GhostStateMachine.current` holds the state) calls the state interface's slot 2 on the old state and slot 0 on
  the new one when it changes, and slot 1 every frame from GhostAI.Update. The
  class's three interface methods are the enter (it picks a destination through `GhostWanderState.GoToPoint` and
  the picker `GhostWanderState.PickDestination`, or sends a D.O.T.S. ghost to state 21), the per-frame step and
  `GhostWanderState.Exit`. The class's other methods include look-alikes of them; nine of those call
  ChangeFavouriteRoomTo as the exit does.
- **The room change.** `GhostWanderState.Exit` adds the radius to a LevelStats total, then returns at once
  when the radius is 3. Otherwise it returns when the difficulty's room-change setting (a field of
  `LevelValues.difficulty`) is 0 or when the ghost already stands in its favourite room (`LevelController.currentGhostRoom`
  against `GhostInfo.favouriteRoom`); then it passes a chance by that setting (`RandomRangeInt(0, 100)` at most 33,
  66 or 100, so 34, 67 or 100 in 100, and 33 for any other setting value) and a roll below the team's
  average insanity, and calls `GhostController.ChangeFavouriteRoomTo` with the room the ghost stands in.
- **The long roam.** Only the picker (`GhostWanderState.PickDestination`) changes the radius from the constructor's 3. For a ghost
  standing in its favourite room it rolls RandomRangeInt(0, 100) against a difficulty setting
  (90, 80 or 70) and, past it, sets a new random radius: a roam away from its room. That roll is skipped
  for a ghost whose behaviour type (`GhostInfo.behaviourType`) is 14, the Goryo, so the Goryo's wander
  radius stays 3 and its exit always returns before the room change. Types 6 and 4 have branches of
  their own there. There is no trait flag: the tests read the behaviour type.
- **The Goryo's other wander branches.** The same type test appears in the picker's destination check
  (a Goryo's sampled point is taken without the path test the others get), in the per-frame step (a
  Goryo that arrives while it stands in its favourite room goes back to idle), in the enter (a Goryo
  goes to the D.O.T.S. state only while nobody is in its room and it stands in its favourite room), and
  in the destination wrapper `GhostWanderState.GoToPoint` that the enter and the step call: when the picked point lies on
  another floor and a flag of the room is set or the radius is not 3, a ghost other than the Goryo
  goes to its destination when the floors differ by one and to state 0 otherwise, and a Goryo goes to
  state 0. None of them is on the room change.
- **Only the roll differs inside the picker.** The picker's destination check (the second type test)
  is reached only when the radius is not 3. A ghost whose type reads 14 throughout the picker never
  changes the radius there (it skips the type 6 and 4 branches and the roll), so for it that check is
  never reached: answering 14 inside the picker alone would change the long-roam roll and nothing else.
- **The entries.** The wander class (fields GhostAI, NavMeshAgent, a float, an int, three bools and a
  Vector3) is `GhostWanderState` (`sdk/map/GhostWanderState.h`). Its field list is exact
  (`kExactFields`): the roam state's obfuscated class, for `GhostState` 21 declares all of those and one
  more float, so without the flag the entry is AMBIGUOUS (2 types). Of the class's seventeen
  `bool(out Vector3)` methods only the picker compares the behaviour type with 14 (the decompiles show
  the others comparing it with other numbers). `GhostWanderState.PickDestination`'s
  `Compares GhostType.Goryo` keeps it alone: gamecheck resolves it to the one method, and without the fact it
  is AMBIGUOUS (17 methods).
- **Nothing the picker calls reads GhostInfo** (checked 2026-10-30 on decompiles of every callee):
  - Unity's `Component.get_transform`, `Transform.get_position`, `Random.RandomRangeInt` and
    `get_insideUnitSphere`, `NavMesh.SamplePosition`, `NavMeshHit.get_position`, `NavMeshPath`'s constructor and `get_status`, `NavMeshAgent.CalculatePath`, and
    `Object.op_Equality` and `op_Implicit`. These are engine thunks and field copies with no managed
    object of the game.
  - `LevelValues.get_Instance` returns the instance; with none it makes a GameObject and adds
    the component, and LevelValues has no Awake or OnEnable.
  - `LevelStats.get_Instance` returns the instance; with none it makes the stats object
    (two containers) and runs its async setup. That setup's
    `AsyncVoidMethodBuilder.Start` calls the state machine's MoveNext at once, and
    MoveNext only sets its state to -2 and calls SetResult. It reads no GhostInfo and sends nothing.
  - `LevelRoom.AreRoomLightsOn` (the type 6 branch only) reads the room's light switches
    (`LightSwitch.get_isOn`).
  - A path-length helper (radius not 3 only) reads the path's corners.
  - The runtime's own metadata init, class init and null-reference throw.
  - So the Draugr's substituted type (`mods/custom_ghosts/goryo_way.h`) is seen only by the picker's
    own four type reads, and it is put back on return, also when the picker throws.
- **Other changes of the room.** `GhostController.ChangeFavouriteRoomTo` has no type test: it stores only the
  room's index, in `GhostController.favouriteRoomIndex` and a field of the ghost's GhostInfo, counts it in LevelStats, sends
  the `SetGhostRoom` RPC to every client and respawns the ghost orb. That RPC's handler,
  `GhostController.SetGhostRoom(int, info)` (no direct callers), checks the sender and then
  stores `rooms[index]` in `GhostInfo.favouriteRoom`, and the index in the same GhostInfo field as above,
  on every client, the master included; it has no type test and no other branch. A look-alike
  has the same body under another RPC name that nothing sends. Besides the wander's exits,
  ChangeFavouriteRoomTo's only callers are two MonkeyPaw wish handlers (both on the
  master), which pick a random other room and have no type test, so a wish moves a Goryo's room too.
- So the Goryo keeps its room because it never takes a long roam: a limit on where it wanders, not a
  gate on the change itself.

## What the Draugr uses (temperature, carrying, its hunt's target)

Read from decompiles of build 25811160 (2026-10-30). The entries are in `sdk/map/LevelRoom.h`,
`sdk/map/Player.h` and `sdk/map/GhostHuntState.h`.

- **A room's temperature** is `LevelRoom.temperature`, one float per room on each client. Every client
  runs `LevelRoom.Update` for every room each frame. It returns at once while
  `LevelValues.eventsBlocked` is set, for a room outside the map, while a flag of another controller
  is clear and while a flag of the room's own is set. Otherwise, while the ghost is in the room
  (`LevelController.currentGhostRoom`, which the master sends to every client) it cools 0.1 a second
  above a level of the room's and 0.033 a second below it; while the ghost is elsewhere it moves at 0.1
  or 0.025 a second towards the room's own levels by the power (the fuse box) and the room's kind. Every
  one of those paths ends in a clamp between a floor and 20. The floor is -10 while EvidenceController's
  Freezing flag (set when the given evidence holds Temperature) is set and 1 otherwise. Its only
  other write is a time that counts up while LevelController's first LevelRoom field is
  the room. `LevelRoom.ResetTemperature` sets the room's starting level.
- **The readings.** The thermometer's reading (its look-alikes hold
  constants in the hundreds and thousands) is a nearby heat source's temperature, or else its room's,
  plus Random.Range(0, 3) (0 to 2 for one tier), and it enables its photo evidence only for a reading
  below 1. A player's breath (the method Breath.Start's InvokeRepeating names; its look-alikes test
  other numbers) shows while their room's temperature is at or below 5 and they are alive. So a ghost
  without Freezing never gives a reading below 1: its rooms never go below the floor of 1. The Draugr's
  cold is a tell, not evidence (user, 2026-10-30): its room shows the game's own temperature less 3, below
  that floor too, so a freezing reading may point to a Draugr whatever the evidence, as the Mimic's orbs
  show on any.
- **Who carries what.** Each Player has a `NetworkedPropGrab` (`Player.propGrab`), whose `inventory` lists
  every item the player carries, in a hand or put away. Its RPC `NetworkedPropGrab.GrabPropRPC`, which every client
  runs, adds the item (SyncInventoryItem), sets a flag on it and freezes its Rigidbody; `NetworkedPropGrab.DropNetworked`
  removes it. `NetworkedPropGrab.get_HeldObject` reads a field of the Player, the item in
  hand only.
- **Moving a carried thing.** The Kikimora's move, now shared (`mods/custom_ghosts/carry.h`): the host asks
  for the item's PhotonView (RequestOwnership, granted for a Takeover view), waits until it is the host's,
  then sets the transform's position and the Rigidbody's, its velocities zeroed.
- **Its hunt's target** is its thief, held by the Banshee's mechanism while it is angered ("The Banshee's one
  target", below); calm, or with no living thief, its definition names nobody and the hunt is the game's own.

## The Banshee's one target (how a hunt keeps to one player)

Read from decompiles of build 25811160 (2026-10-30, corrected 2026-10-31), for `PmGhostDef.target`,
which a custom ghost answers with the one player its hunts go for (the Draugr its thief, the Bogeyman the
youngest), as the Banshee has one target. Every place is named by its map key; the
numbers are in the gitignored `re/dump`.

- **The target.** `GhostAI.SetNewBansheeTarget(Player)` acts only on the master and only for a ghost
  whose behaviour type is `GhostType.Banshee`. It chooses a living player other than the one it is
  given and sets `GhostAI.bansheeTarget` on every client through an RPC. No other type gets a target
  from it: the tarot cards and `GameController` call it too, and it does nothing for them.
- **The hunt's kept player.** The hunt state keeps one player in a field of its own (no map entry; the
  mod never reads it). Its enter runs `GhostHuntState.StartPick` once, which sets the kept player: the
  ghost's `bansheeTarget` when `GhostHuntState.TargetAllows` passes for them and they are alive and not
  outside the map, and otherwise a player taken from the network's player list.
- **Every frame of the chase** (`GhostHuntState.Chase`) begins with `GhostHuntState.KeepNearest`, which
  moves the kept player to the nearest player the hunt may go for, unless the hunt's `ghostType` is the
  Banshee's or the Kormos's. So a Banshee keeps the player its start pick chose for the whole hunt, and
  any other ghost the nearest.
- **The target pick** (`GhostHuntState.TargetPick`) runs `GhostHuntState.Listen` first, then takes the
  kept player when `GhostHuntState.Sees` passes for them, and only then whoever
  `GhostHuntState.SightPick` finds. For the Banshee's `ghostType` that look finds nobody, so its sight
  never picks anyone but the kept player. The chase's own walk takes the kept player as its target when
  `Sees` passes for them too.
- **`TargetAllows`** lets the hunt go for any player only while `bansheeTarget` is null, or its player's
  room is outside the map (the truck), they are dead, or already in the hunt's killed list; otherwise
  only for that player. It reads the field, not the type, so it holds for whichever ghost has a target,
  which in vanilla is only the Banshee. `Listen` asks it for every player before its voice and
  electronics tests, `StartPick` for the target and `KeepNearest` for each player.
- **The kill** (`GhostHuntState.Kill`) has no type test of its own: it kills the player it is given, the
  hunt's target or, in the closet search, its search target, and a few hunt steps pass players they loop
  over. It returns false at once for a player in the killed list or while the ghost's smudge is alive,
  and unless forced needs the player within reach. A kill calls `SetNewBansheeTarget` with the victim
  before the victim's death begins (their `Player.isDead` is set only later, when the death completes) and
  before adding them to the killed list. While the field is set, `SetNewBansheeTarget` passes over the
  victim, the current target and the dead; while it is null, only the dead. It sends the first other
  player through its RPC, and leaves the field when nobody qualifies. So a Banshee moves to a new target
  at once; the kept player stays the victim. The Hanged Man card calls `SetNewBansheeTarget` with the
  player it kills, too.
- **A target who leaves the game.** An event handler of `GameController` (no map entry; it is told apart
  by its compare with the Banshee's type, which its look-alikes lack) runs for the Banshee's type alone:
  when the field has gone null (the target's object destroyed), it calls `SetNewBansheeTarget` with
  nobody, whose null-field path takes the first living player.
- **So the vanilla Banshee can kill other players.** While its target is in the house, alive and not yet
  killed, neither its sight nor `Listen` goes for anyone else. Once that target is in the truck, dead or
  killed, `Listen` sends it after any player it hears or whose electronics it senses, who becomes its
  search target and then its target, and the kill takes them. Its sight still goes only for the kept
  player.
- **Every way a hunt kills, and whom** (2026-10-31, from the binary: every caller of `GhostHuntState.Kill`,
  every caller of the player's death sequence, and every live writer of `GhostHuntState.target` and
  `GhostHuntState.searchTarget`, live meaning reachable by direct calls from the hunt's enter, Execute or
  `GhostHuntState.Exit`).
  - The player's death sequence (`Player.BeginDeathSequence`) is begun by the hunt's kill and
    otherwise only by the Monkey Paw and the tarot cards. No collision, trigger or contact handler kills:
    a player who walks into a hunting ghost dies only if the hunt's own kill is given that player.
  - `GhostHuntState.Kill` has three live callers: the chase's kill step (reached from `GhostHuntState.Chase`
    by a tail jump), which kills the hunt's `target`; the closet check, which force-kills the
    `searchTarget` hiding in the closet being searched; and a step that kills players it hears, which runs
    for the Kormos's `ghostType` alone. Two other callers are uncalled look-alikes.
  - Every live writer of `target` and `searchTarget` sets one of these: the player `SetTarget` is given by
    `GhostHuntState.TargetPick`; the player `Listen` or a door step picks, both gated by
    `GhostHuntState.TargetAllows`; the hunt's kept player (`StartPick`, then `KeepNearest`); or a copy of one
    of the two fields. A Kormos-only step sets the search target from what it hears. The rest clear them.
  - So for the Banshee's `ghostType`, while its target is alive, in the house and not killed, `TargetAllows`
    lets nobody else through, its kept player is the target, `SightPick` finds nobody, and every kill path
    takes the target alone. **The Banshee can kill only its target while the target is killable.** One case
    follows from the same code: a player the hunt went for while the target was out (in the truck) stays its
    `target` or `searchTarget` until the hunt drops them, so the kill can take that player after the target
    comes back in.
- **Who else reads `bansheeTarget`** (every load of the field's offset in code that holds a `GhostAI`,
  listed from the binary). Besides the steps above: the hunt's enter, which for the Banshee's type alone
  asks `SetNewBansheeTarget` for a target when the field is null and sends the ghost to its favourite
  room (`GhostState.favouriteRoom`) instead; the idle state's hunt insanity (`GhostIdleState.HuntInsanity`),
  which measures that player's insanity instead of the team's; methods of the wander and its roaming
  variant, reached only from look-alike overrides that the state interface does not declare; and one
  other state class, whose reader its own Execute calls. The ghost runs one state at a time, so none of
  these runs during a hunt. `GameController`'s handler for a target who leaves (above) reads the field
  only for the Banshee's type.
  The rest of the hits are look-alikes of the hunt's own steps. Each of these acts on the field only for
  the Banshee's type or outside a hunt, so it stays safe only while the hunt's `ghostType` is the ghost's
  own.
- **Playable Ghost** (`mods/playable_ghost/actions.cpp`) reads `bansheeTarget` to measure the hunt
  check's insanity, that player's own or else the team's, and its HUD then says "Your target's sanity"
  (`session::kFocusSanity`). Outside a hunt, where the check runs, it reads the field whatever the type, as
  the game does: a Mimic that has copied the Banshee got the field through the game's own RPC, and nothing
  clears it at its next copy. In a hunt the field of another type is the host's hold, not the check's focus. A
  player controlling a Banshee kills as the game's hunt does.
- **A ghost whose definition names a target** (`PmGhostDef.target`) reuses the Banshee's targeting
  mechanism: one target, enforced by the game's own Banshee code on the master, for its AI and a player
  controlling it alike (`host/content_target.h`), and shown on a ghost player's HUD. When a hunt of the
  contract's ghost begins, the definition of the type it acts as is asked once and its player becomes the
  ghost's `bansheeTarget`, before `StartPick` reads it. For that hunt `KeepNearest` does nothing and
  `SightPick` finds nobody, as for the Banshee's type. Where the game re-targets (`SetNewBansheeTarget`,
  from the kill or the Hanged Man card), the definition is asked again with the dying player excluded;
  when the target leaves the game, it is asked again with nobody excluded, at the next frame. It never
  names a dead player. Its answer becomes `bansheeTarget`, and an answer of nobody leaves the field, as
  the game's does. The hunt's end (`GhostHuntState.Exit`) clears the field, and so does the ghost ceasing
  to act as the type whose definition named the target (a Mimic's copy, of a game type or another custom
  one), after which the hunt is the game's own until the next. Who steers the ghost plays no part: a
  hand-over mid-hunt keeps the target. No type swap is made:
  the hunt's `ghostType` stays the ghost's, which keeps the field's other readers out (above). The HUD
  shows that hunt's target, with the team's sanity beside it, since the hunt check never measures a
  target the host holds a hunt to.
- **Design choices where it differs from the vanilla Banshee** (user, 2026-10-31):
  - At a re-target only the dying player is excluded, not the current target, so the ghost's own rule
    decides: the Bogeyman keeps the youngest and the Draugr its most recent thief while that player is
    not the one dying, and otherwise takes the rule's next player. The vanilla Banshee moves off its
    current target at every re-target.
  - A definition that answers nobody at a hunt's start leaves that hunt unrestricted.
  - A player it names later (a new thief mid-hunt) binds from the next hunt or re-target, as the field is
    fixed between them.
- **Why the field is written on the host only**, not through the game's `SetBansheeTargetNetworked` RPC as
  `SetNewBansheeTarget` sends it:
  - Every reader of the field runs in the ghost's AI states, which run on the master alone.
  - The RPC can only set a player: an actor that matches nobody leaves the field, so the clients' copies
    could never be cleared at the hunt's end.
  - `SetNewBansheeTarget` sends it buffered (`RpcTarget.AllBufferedViaServer`), so every late joiner
    would replay targets long gone stale.
  - After a host migration, the new master's idle `HuntInsanity` would read a stale field for any type.
  - A mod sends none of the game's RPCs (the loader owns networking).

## The Black Shuck's corridor (how a player steers it)

A player walks a hunting ghost themselves: they set the direction the agent's crowd steering uses, and the
game's destination, speed and acceleration stay its own ([Steering an agent](../systems/ghost-ai.md#steering-an-agent)). A custom ghost may limit
that direction through its definition (`PmGhostDef.playerMove`, asked every frame). The Black Shuck's limit
(`mods/custom_ghosts/trail.h` `confine`) is a corridor around the scent trails its AI follows (user, 2026-10-31),
built only from what the AI itself uses:

- **Its trails.** Those of the living players the hunt may go for, as `hunt::may_go_for` rebuilds the game's test,
  leaving out a trail lost in the last 30 s, which its AI ignores too.
- **The corridor.** Within `kCorridor` of a trail's spine, each unbroken run of a trail its own corridor with flat
  ends, so salt or running water across a trail leaves a gap no run spans. The width is the user's design, not
  game data. A step is tested `kCorridorLookAhead` ahead, so the agent's own slowing stops it before the edge.
- **Inside it** a step that would leave slides along the nearest stretch, or stops. A step never crosses a break,
  nor lands on the next run.
- **Outside it** a step must bring it nearer, measured as its AI's agent would walk (below). With nothing in the
  way, nearer a stretch or the step its scent reaches; straight at that step nothing can break, so a break never
  holds it still, and a step off that line does not cross a break either. With a wall in the way, nearer that way's
  next corner.
- **Salt or water on the way round a wall** stops nothing. Its AI sends its agent to the step with
  `NavMeshAgent.SetDestination` and tests nothing along the path, and neither blocks an agent: `SaltSpot.SyncSalt`
  flattens the pile and, for a hunting ghost and tier three salt, slows it. It walks on until its scent gives
  out (`trail::smells`, from where it then stands), so its AI loses its prey and a player is free. A gap in a trail
  is another matter, and holds both: no run of the corridor spans it, and its AI's `trail::advance` stops at it.
- **Free** wherever its scent reaches no step (`next_step` false: none within `kPickUp`, all behind a break, or
  it stands in reach of running water or salt), and outside hunts. Its AI then roams by the game's own hunt.

Its prey is whoever's trail it walks (`trail::walked`): the trail whose corridor it stands in, the prey it has
while it stands in none, and otherwise the one a hound would pick up. Everything after that is the AI's own code in
`behaviour.cpp` `tick_shuck`: `GhostHuntState.SetTarget`, the hold on `killReach` and `slowKillReach` while the game's own
vision does not see the prey, and the closet handover. `GhostHuntState.Sees`, `Hears` and `SensesGear` answer the
same whoever steers. Only walking differs: its AI's agent is sent along the trail through
`NavMeshAgent.SetDestination`, and a player's is sent nowhere.

Scent reaches through a wall, as the AI's own `nearest_step` measures it, while the AI's agent walks round. So
"nearer" follows the path the agent would walk to the step its scent reaches, asked of the game as
`GhostWanderState.PickDestination` asks it of the ghost's agent when it tests a long roam: a new `NavMeshPath`
(`NavMeshPath.ctor`), `NavMeshAgent.CalculatePath`, then `NavMeshPath.get_status` against
`NavMeshPathStatus.PathComplete`, and the path's corners (`NavMeshPath.get_corners`, start first). With a corner in
the way, a step must bring it nearer that corner, and a trail just beyond the wall is no corridor to hold it in.
These seven entries are optional: without any of them, with no path or with only part of one, it steers freely
outside the corridor, and inside it the corridor stands. `CalculatePath` runs on the main thread, so the answer is
kept in `trail::WayCache` while it stands, into one `NavMeshPath` kept for the ghost: it is asked again for another
step, after a walk of `kWayMoved`, with the next corner within `kWayMoved`, or after `kWayMaxAgeMs`, as a door may
shut meanwhile.

Its player sees what the hunt goes by (`behaviour.cpp` `scent`, shown through `PmGhostDef.playerView`): the steps it
may pick up (`trail::smells`) of the trails above. A trail it lost lately, a player it may not go for and steps behind
a break do not show. Its scent reaches through floors as it does through walls, so nor does the view stop at the floor.

## A ghost's footsteps (how the Brownie's fall silent)

A ghost's footstep is a pooled `Footstep` the footstep-spawn RPC enables on every client. `Footstep.OnEnable` plays it
once through `Footstep.Play`, giving it a volume of 1, 0.5 or 1.5 by whether the ghost's own sensor detects the local
player. Read from the decompiled code of build 25811160:

- **`Footstep.Play`.** It sets the Noise's pitch, then its reach with `Noise.SetMaxDistance` (`Footstep.reach`, which
  `Footstep.Awake` read from the AudioSource; 8 m less for a hunting Myling). It casts a line down to the floor; when the
  line misses, nothing plays. Otherwise it picks the clip for the floor's tag and plays it with `Noise.PlaySound` at
  the clip's own volume. Last it multiplies the AudioSource's volume by its own float argument, which it uses for
  nothing else.
- **`Noise.PlaySound` and `Noise.SetVolume`.** PlaySound stops the AudioSource, sets the clip and calls SetVolume.
  SetVolume stores `Noise.volume` and `Noise.loudness` (150 times the volume), and gives the AudioSource that volume, or
  0 while the Noise's mute flag is set. The flag comes from the spawn RPC's last argument; the Brownie never uses it.
- **`Noise.Update` and `StopSound`.** Once the clip's length has run out, Update calls StopSound, which stops the
  AudioSource and deactivates the Noise's own object. PlaySound activated it, so the Noise Cube is active only while
  its step sounds. `Footstep.OnDisable`, the pool's cleanup, switches the Noise component back on and clears its mute
  flag. Neither touches the cube's collider.
- **The prefab** (read from a scene's serialized data, not code). The footstep's root holds the print: its renderer,
  the Evidence and a solid 0.4 m BoxCollider on the UV layer. Its "Noise Cube" child, inactive in the prefab, holds a
  0.1 m trigger BoxCollider on the Noise layer, the AudioSource and the Noise. The AudioSource has volume 0.2, is fully
  3D and uses a custom rolloff over MinDistance 1 m to MaxDistance 20 m, with the curve near full volume close in. So a
  reach of 0 is not proven silent: what the engine does below MinDistance is not game code. The Noise's hunt volume
  multiplier is 1, so its hunt-start and hunt-end volume listeners are never registered.
- **Physics** (the game's physics settings). The layer matrix pairs the Noise layer only with layer 3, which has no
  name, and no object in any scene or assets file is on layer 3. So the cube's trigger meets no other collider. The
  settings let queries hit triggers, so it is found by any physics query whose layer mask takes in the Noise layer and
  that does not ask to ignore triggers, which is how the sensors detect it. `Footstep.Play`'s own line to the floor
  ignores triggers.

Who reads a footstep's Noise. Every method of every class whose members, fields or typed generics name `Noise` or
`PlayerNoise` was decompiled and searched for these two fields:

- **The sound sensor** detects a Noise by its collider and keeps it in a list. Its reading is the sum of `Noise.volume`
  over that list. On each detection it replays the clip on the truck's source at `Noise.volume`, and moves a challenge
  counter on.
- **The parabolic microphone** keeps the playing Noises it detects on its floor. Its readings are the sum, least and
  greatest `Noise.loudness`. On detection it replays the clip at the AudioSource's live volume, or at `Noise.volume`
  for a muted Noise. In radar mode it adds a point at the noise, labelled with `Noise.loudness`.
- **The ghost's hearing** takes only a `PlayerNoise` (every handler's parameter and `HeardNoise.noise`) and reads its
  kind, not its volume. A ghost's footstep is a plain `Noise`, so the hearing never takes one. Its sensor ("Player noise
  range sensor") is a Noise-layer sensor too, and its `GhostNoiseProcessor` refuses a plain `Noise`.
- **The sound recorder** has a third Noise-layer sensor. Its signal processor needs a `SoundRecorderDetectable`, which
  a footstep's cube does not have, so it refuses a footstep and the switch changes nothing for it.
- **`Footstep`** reads neither field.

| Key | Kind | Found by |
| --- | --- | --- |
| `Noise.PlaySound` | method | name, params `AudioClip, float, bool, bool` |
| `Noise.SetVolume` | method | name, params `float` |
| `Noise.volume` | field | shape: public `float`, loaded by `Noise.PlaySound` |
| `Noise.loudness` | field | shape: public `float`, stored by `Noise.SetVolume`, not `Noise.volume` |
| `Collider.set_enabled` | method | name, params `bool` |

The game API is `phasmo::Noise` (`SetMaxDistance`, `volume`, `loudness`) in `sdk/map/Footstep.h`, and
`phasmo::Collider::set_enabled` in `sdk/map/LevelRoom.h`.

The Brownie's rule (`mods/custom_ghosts/brownie.cpp`, a hook on `Footstep.Play`, on every client):

- **Outside hunts:** untouched.
- **In a hunt, while anyone looks at it:** after the game's play it cuts the reach by 8 m, the Myling's rule.
- **In a hunt, while nobody looks:**
  - before the game's play activates the cube, it switches off the collider on the Noise's own object, so no query
    finds the step: no sensor detects it, no list holds it, no radar point, replay or challenge count follows;
  - it gives the game's play a volume of 0, which silences only the AudioSource;
  - after the play it sets `Noise.volume` and `Noise.loudness` to 0.

The collider stays off while the cube sounds, through StopSound and through the pool's cleanup, which never touch it.
It comes back on before that footstep's next step that is not silent, whatever ghost makes it, and at once when no
Brownie haunts or the mod stops. The step's timing (the root's own timer), pitch, reach, clip and print (the root's
renderer, Evidence and collider) stay the game's. One small difference follows: `Stream`'s ray, which has no layer
mask, can no longer end on the cube of a silent step.

"Anyone looks" is the host's test, which also sets the Brownie's hunt speed. The host sends it on the mod's channel.
Outside a hunt it sends only the first word. On entering a hunt it sends at once, so a client has the current state
from the first hunting frame. In a hunt it sends at once when the state changes and every second otherwise, and a
client takes it only from the master client.

## Startup time

The host resolves the whole map in the running game at startup, inside a 60-second deadline. Measured
`game map: <n>/<n> resolved in <ms> ms` in `%LOCALAPPDATA%\PhasmoMods\mods.log` with 23 entries:
**not measured yet.** The live check in the game fills this in (a value goes here, with the date).

## Not mapped yet

The other state objects, `SetGhostModel`, the state machine and the hunt speed's branches by ghost
type. GhostAI's own base speed is `GhostAI.baseSpeed` (`Stores Awake`, and
`StoresConst Awake 1.0` negated: Awake stores three floats of 1 beside it). The ghost's type, `info` and `Init` are covered in
[ghost types](../systems/ghost-types.md). When one is mapped, its facts go in its entry's note and any finding
about the whole system on this page.
