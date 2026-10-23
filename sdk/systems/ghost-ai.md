# Ghost AI

The ghost's brain is the `GhostAI` component, one per contract, a MonoBehaviourPun in Assembly-CSharp
(global namespace). It holds the ghost's state machine, its NavMeshAgent and the hunt flags. The
entries below are in `sdk/map/GhostAI.h`; their `///` notes are the authority, and this page ties them
together. Facts were read from decompiled code of Steam build 25811160 unless a note says otherwise.
This page lists no addresses or offsets: they belong to one build. `tools/gamecheck.ps1` prints the
value of every entry for the build in hand.

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
| `GhostAI.Awake` | method | name |
| `GhostAI.Hunting` | method, a Photon RPC | name, params `bool, int, PhotonMessageInfo` |
| `GhostAI.ChangeState` | method | name, params `@GhostState, PhotonObjectInteract, bool` |
| `GhostAI.UnAppear` | method, RPC to every client | name |
| `GhostAI.LookAtPlayer` | method | name, params `Player`; has twins |
| `GhostAI.TemporarilyStopWander` | method | name, params `float`; has an alias |

The game API is `phasmo::GhostAI` (`type`, `state`, `agent`, `isHunting`, `postHuntCooldown`, `Awake`,
`Hunting`, `ChangeState`, `UnAppear`, `LookAtPlayer`, `TemporarilyStopWander`) and `phasmo::GhostState`
(`type`, `idle`, `wander`, `hunting`). The `GhostState` enum has more values than are mapped; map one
when a mod needs it. Public methods, Unity messages and Photon RPCs keep their real names. Nearly every
field and the state enum are obfuscated.

## How it fits together

- **State.** `ChangeState(state, interact, force)` stores the state in `state` and starts that state's
  object, but only while a flag of the ghost's PhotonView is set; otherwise it logs an error. For
  `hunting` it stores idle instead while a flag of LevelValues is set, and starts the state object only
  when `isHunting` is clear or its third argument forces it. `wander` gives the state the ghost's
  NavMeshAgent. Writing `state` directly skips all of that, so a mod changes state through
  `ChangeState`.
- **Hunt flags.** `Hunting(start, number, info)` is the RPC that starts or ends a hunt on each client.
  It stores its first argument to `isHunting`, and when that is true sets `postHuntCooldown` to 1.
  `Awake` clears both. It also sets the agent's speed to 1.7, which the ghost keeps as its base hunt
  speed.
- **Appearing.** `UnAppear()` does something only when the ghost's shown flag is set (the flag `Hunting`
  clears as it hides the model). It then sends an RPC with two arguments to every client, so calling it
  on the host ends the appearance for everyone. Ghost Tells' Shade calls
  `phasmo::GhostAI::UnAppear(ghost)` when a player stares at it.
- **Facing and wandering.** `LookAtPlayer(player)` turns the ghost's transform to the player's
  position on this client only. `TemporarilyStopWander(seconds)` restarts the coroutine that times
  wandering; Tarot's Hermit card and a smudged Yurei use it.

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
by what `GhostAI.Hunting` and `GhostAI.Awake` do to it:

- `Hunting` stores five bools, four of them from registers. Of those four, one is also stored by
  `Awake`: that is `isHunting`. Its facts are `Stores Hunting`, `Stores Awake`, and `StoresConst
  Hunting 1` negated (the value comes from a register, not a constant).
- `postHuntCooldown` is the bool `Hunting` stores the constant 1 into: `StoresConst Hunting 1`.
- `Awake` also clears a second bool, but inside a 16-bit store that starts at the neighbouring offset,
  and the resolver keys a store by its start offset. Should `Awake` come to store at that bool's own
  offset, `isHunting` fails closed (`Ambiguous`), never to the wrong field.
- Proof that the facts decide: with the cooldown's fact removed, all 13 bool fields match
  (`AMBIGUOUS`); with the constant 2 in place of 1 it is `MISSING`.

Update drill (2026-10-23): renaming the cooldown field and moving the agent in a copy of the dump left
every entry resolved, reporting the agent's new offset and the cooldown as renamed from its old name.
A stale obfuscated name never decides.

## Startup time

The host resolves the whole map in the running game at startup, inside a 60-second deadline. Measured
`game map: <n>/<n> resolved in <ms> ms` in `%LOCALAPPDATA%\PhasmoMods\mods.log` with 23 entries:
**not measured yet.** The live check in the game fills this in (a value goes here, with the date).

## Not mapped yet

The other state objects, `Init`, `SetGhostModel`, and the hunt speed (the base speed is set in `Awake`;
its branches by ghost type are not in an entry). When one is mapped, its facts go in its entry's note
and any finding about the whole system on this page.
