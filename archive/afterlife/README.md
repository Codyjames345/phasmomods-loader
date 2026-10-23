# Afterlife (archived 2026-10-23)

A dead investigator holds Crouch for 0.8 seconds to watch through the ghost's eyes, and again to
return to their body. While watching they cannot move or touch anything, and for a minute after it
they still cannot touch anything, so they cannot spot the ghost and point it out by throwing things.
Nothing about the ghost is shown but what it sees. Otherwise dead players play as in vanilla. It sends
nothing over the network, so it needs nobody else to run it.

It was archived so it can be picked up another time. The code is kept as it was and is not built. It
has never run inside the game; only its offline tests (`tests/afterlife_test.cpp`, the hold and lock
rules in `haunt_rules.h`) have run. Unverified: switching off the dead investigator's camera and
rendering from a new Camera at the ghost's head (URP may need its additional camera data, and the
copied culling mask may not suit a dead player), freezing the dead player's movement, and switching
off Interact, Primary Use and Secondary Use for the lock; also whether Crouch reaches a dead player's
input and whether the game switches those actions back on by itself. Its live check was:

> In this repository, die in a private contract with Afterlife running.
> Check the hint, hold Crouch: the view is the ghost's, turning with it; you cannot move or touch
> anything. Hold Crouch again: you are back where you were, and for a minute the hint counts down and
> nothing can be picked up or thrown; afterwards you can. Check mods.log for "afterlife: haunting
> with the ghost" and fix what is wrong.

To bring it back:

1. Move this folder's code to `mods/afterlife` and `tests/afterlife_test.cpp` back to `tests/`.
2. In `CMakeLists.txt`, restore the `Afterlife` target (`mods/afterlife/mod.cpp` and
   `mods/afterlife/haunt.cpp`, linked to `pgmod`), `pg_test(afterlife)` and `mods/afterlife` in
   `pg_test`'s include directories.
3. Restore the two `mods/afterlife` entries in `tools/payload.psd1`, and `mods/afterlife` in the
   scanned folders of `tests/held_scan_test.cpp`.
4. Restore the Afterlife rows in `README.md` and `docs/installation.md`, and its paragraph in
   `docs/design.md`'s Companion mods.
5. Write a FINDINGS entry for the live check above with `/finding` (15 was removed when Afterlife was
   archived).

Its channel (3) and id (`afterlife`) are not reused.
