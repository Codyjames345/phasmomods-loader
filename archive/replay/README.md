# Replay (archived 2026-10-22)

After each contract, the contract's top moments: kills, close escapes, chases, events seen up
close and crucifix saves. The host records the contract twice a second, picks the moments
(`moments.h`), and sends them to every client running the mod. Each client shows them in the lobby
as coloured dots on a schematic map of the level, in a panel of the mod's own.

It was archived so it can be picked up another time. That panel breaks the suite's rule that mods
reuse the game's own screens and never add windows of their own. A replacement was designed and
planned but not started:

- `docs/2026-10-21-replay-3d-design.md` (approved): each modded client films hunts and ghost events
  from a hidden copy of the game's video-camera rig, with burned-in captions and all the sound,
  including voice chat. Back in the lobby, players put a cassette into the parked truck's CCTV unit
  to watch a moment together on its monitor. The doc also records the research into how other games
  store replays, and every game fact the design rests on (RVAs, offsets, scene paths, icalls), read
  on build 25811160.
- `docs/2026-10-21-replay-3d.md`: the eleven-task implementation plan for it.

The code here is the 2D version, kept as it was and not built. It has never run inside the game;
only its offline tests (`tests/replay_test.cpp`) have run. Its live checks were:

> In the phasmomods-loader repository, play a private contract with Replay on every
> client and make something happen: a hunt that catches someone, a near miss, a crucifix burn. Back
> in the lobby check mods.log for "replay: contract over, N frames, M moments" on the host and that
> every client shows the panel about ten seconds later, with the right floor's rooms, moving dots,
> captions naming the right players and who played the ghost. Fix what is wrong.

To bring it back:

1. Move this folder's code to `mods/replay` and `tests/replay_test.cpp` back to `tests/`.
2. In `CMakeLists.txt`, restore the `Replay` target, `pg_test(replay)` and `mods/replay` in
   `pg_test`'s include directories.
3. Restore the two `mods/replay` entries in `tools/payload.psd1`, and `mods/replay` in the scanned
   folders of `tests/held_scan_test.cpp`.
4. Restore the Replay rows in `README.md` and `docs/installation.md`, and its paragraph in
   `docs/design.md`.
5. To build the 3D version instead, move the two docs to `docs/superpowers/specs` and
   `docs/superpowers/plans` (the plan's paths assume those places) and follow the plan. Where it
   says FINDINGS 16, write a new entry with `/finding` (16 was removed when Replay was archived).

Its network channel (4) and id (`replay`) are not reused.
