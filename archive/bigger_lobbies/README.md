# Bigger Lobbies (archived 2026-10-20)

Private lobbies for eight players. The lobby's ID cards and the truck's sanity monitor kept their
size and paged between players 1-4 and 5-8. The journal marked all eight players' choices, and
each player got a colour.

It was archived because it cannot work. Kinetic's Photon servers cap a room at four players.
Creating a private room for eight fails with "Max Players Peer Room value is too big. value:8.
limit:4". That limit is a setting of the game's Photon app, enforced on the server, so no
client-side change can raise it legitimately. The mod only becomes possible if Kinetic raises
that limit.

The code is kept as it was and is not built. To bring it back: move this folder to
`mods/bigger_lobbies` and `tests/lobbies_test.cpp` back to `tests/`. Then restore the
`BiggerLobbies` target, `pg_test(lobbies)` and the include directory in `CMakeLists.txt`, and the
two entries in `tools/payload.psd1`. Its channel (5) and id (`bigger-lobbies`) are not reused.
None of it has run in a game with five or more players.
