#pragma once

// The lobby board's ID cards for eight players. The game has four cards (ServerManager's list) and
// gives player N the Nth; four more are copied from its own, each with its own colour and a free
// scene PhotonView ID chosen the same way on every client, before Unity first wakes them. The cards
// keep their size: four are shown at a time and a tab beside them turns to players 5-8 and back.
// Main thread.
namespace pg::lobbies::cards {

bool resolve();
void tick();
void teardown();

}  // namespace pg::lobbies::cards
