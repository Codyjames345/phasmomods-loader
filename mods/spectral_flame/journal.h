#pragma once

// Spectral Flame on the journal's evidence page, as the game's own seven: an eighth row cloned from
// a vanilla one, registered with the journal's evidence buttons and their synchronisation, so it is
// marked, crossed out and filters the ghost list like the others. Every journal on this client is
// extended, other players' included, so the rows line up everywhere. Main thread.
namespace pg::flame::journal {

bool resolve();
void tick(bool enabled);  // enabled: the flames are in play in this contract
void teardown();

}  // namespace pg::flame::journal
