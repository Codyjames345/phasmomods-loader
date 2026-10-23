# Ghost AI

The ghost's brain is the `GhostAI` component, one per contract, a MonoBehaviourPun in Assembly-CSharp
(global namespace). It holds the ghost's state machine, its NavMeshAgent and the hunt flags. The
entries below are in `sdk/map/GhostAI.h`; their `///` notes are the authority, and this page ties them
together. Facts are from Steam build 25811160 (GameAssembly.dll sha256 `e300eda9...4b790c8`), read
from decompiled code unless a note says otherwise. Addresses are RVAs in that build and are what
`tools/gamecheck.ps1` prints; a mod never uses them.

## What is mapped

| Key | Kind | Found by | In build 25811160 |
| --- | --- | --- | --- |
| `GhostAI` | type | name | |
| `GhostState` | enum, nested in GhostAI | shape: owner and `values` idle, wander, hunting | `ദശസറദബഹജഭ`, an enum of idle, wander, hunting, favouriteRoom and 20 more |
| `GhostState.idle`, `.wander`, `.hunting` | enum values | name | 0, 1, 2 |
| `GhostAI.state` | field | shape: type `@GhostState` | +0x30 |
| `GhostAI.agent` | field | shape: type `NavMeshAgent` | +0x40 |
| `GhostAI.isHunting` | field | shape `bool` plus facts | +0xF3 |
| `GhostAI.postHuntCooldown` | field | shape `bool` plus a fact | +0xD8 |
| `GhostAI.Awake` | method | name | 0x20211C0 |
| `GhostAI.Hunting` | method, a Photon RPC | name, params `bool, int, PhotonMessageInfo` | 0x2022400 |
| `GhostAI.ChangeState` | method | name, params `@GhostState, PhotonObjectInteract, bool` | 0x2021290 |
| `GhostAI.UnAppear` | method, RPC to every client | name | 0x202E140, no twins |
| `GhostAI.LookAtPlayer` | method | name, params `Player` | 0x2023410, twins 0x2025B70 and 0x2027FF0 |
| `GhostAI.TemporarilyStopWander` | method | name, params `float` | 0x2024900, alias `ളദലഴഫഫശരഷ` at the same address |

The game API is `phasmo::GhostAI` (`type`, `state`, `agent`, `isHunting`, `postHuntCooldown`, `Awake`,
`Hunting`, `ChangeState`, `UnAppear`, `LookAtPlayer`, `TemporarilyStopWander`) and `phasmo::GhostState`
(`type`, `idle`, `wander`, `hunting`). The `GhostState` enum has more values than are mapped; map one
when a mod needs it. Public methods, Unity messages and Photon RPCs keep their real names. Nearly every
field and the state enum are obfuscated.

## How it fits together

- **State.** `ChangeState(state, interact, force)` stores the state in `state` and starts that state's
  object, but only while a flag of the ghost's PhotonView (+0x68) is set; otherwise it logs an error.
  For `hunting` it stores idle instead while a flag of LevelValues is set, and starts the state object
  only when `isHunting` is clear or its third argument forces it. `wander` gives the state the ghost's
  NavMeshAgent.
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

The obfuscator adds same-signature copies of real methods. Measured offline on build 25811160 on
2026-10-23: `GhostAI` has 15 true twin pairs, bodies that read the same once placed. It also has
decoys that look the same but reference other metadata. Examples:

- `GhostAI.Init(x)` has a twin `ഞയറജഭബഠഴമ(x)`, and the linker folded both onto one address (0x2022A70):
  an alias. Hooking the address once covers both.
- `GhostAI.SetGhostModel` has a twin `ഞതപശഝമരജഡ` at a different address (0x2023DB0 against
  0x2024EA0). The game may call either, so a hook on one body misses calls to the other. These two are
  not mapped yet.
- `GhostAI.LookAtPlayer` has two twins (0x2025B70 and 0x2027FF0), each 0x179 bytes with the same
  normalised instructions as the canonical body at 0x2023410.
- `GhostAI.UnAppear` has a **decoy**: 0x2024B20 has the same signature and a nearly identical body, but
  the 11th token of its normalised instructions loads a different runtime-metadata address, and its length is 0x187 against
  0x184. It is another method, not a twin, and `UnAppear` correctly has no twins.

So a signature alone does not identify a method or its behaviour. The resolver calls bodies twins only
when both were read in full, have the same length, and have the same normalised instructions
(rip-relative operands compared by absolute address, in-function branches by offset, and the method's
own metadata-init flag masked). A hook with `Twins::All` covers every twin; `Twins::CanonicalOnly`
keeps the exact behaviour of a hook placed by raw address on the canonical body.

## How the fingerprints were chosen

`GhostAI` has 13 unnamed instance bool fields, at +0xA8, +0xD8, +0xD9, +0xF0, +0xF1, +0xF2, +0xF3,
+0xF4, +0xF5, +0x108, +0x109, +0x10A and +0x130. Shape alone cannot tell them apart, so a field is
found by what `GhostAI.Hunting` and `GhostAI.Awake` do to it:

- `Hunting` stores five bools, four of them from registers. Of those four, one is also stored by
  `Awake`: that is `isHunting` (+0xF3). Its facts are `Stores Hunting`, `Stores Awake`, and `StoresConst
  Hunting 1` negated (the value comes from a register, not a constant).
- `postHuntCooldown` (+0xD8) is the bool `Hunting` stores the constant 1 into: `StoresConst Hunting 1`.
- `Awake` also clears the bool at +0x109, but inside a 16-bit store at +0x108, and the resolver keys a
  store by its start offset. Should `Awake` come to store at +0x109 on its own, `isHunting` fails closed
  (`Ambiguous`), never to the wrong field.
- Proof that the facts decide: with the cooldown's fact removed, 13 bool fields match
  (`AMBIGUOUS`); with the constant 2 in place of 1 it is `MISSING`.

Update drill (2026-10-23): renaming the +0xD8 field and moving the agent to +0x48 in a copy of the dump
left every entry resolved, reporting `GhostAI.agent +0x48` and `GhostAI.postHuntCooldown +0xD8,
renamed from ടഴദബടഠഹടന`. A stale obfuscated name never decides.

## Startup time

The host resolves the whole map in the running game at startup, inside a 60-second deadline. Measured
`game map: <n>/<n> resolved in <ms> ms` in `%LOCALAPPDATA%\PhasmoMods\mods.log` with 23 entries:
**not measured yet.** The live check in the game fills this in (a value goes here, with the date).

## Not mapped yet

The other state objects, `Init`, `SetGhostModel`, and the hunt speed (the base speed is set in `Awake`;
its branches by ghost type are not in an entry). When one is mapped, its facts go in its entry's note
and any finding about the whole system on this page. Reverse engineering done before the map existed
sits in project notes outside this repository; move what a mod needs into an entry as you map it.
