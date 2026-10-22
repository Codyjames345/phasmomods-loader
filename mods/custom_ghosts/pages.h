#pragma once

// The custom ghosts in every player's journal, as the game's own ghosts are:
//  - a ghost button on the evidence spread's ghost page (the investigators' guess), cloned from a
//    vanilla one, crossed out by the journal's own evidence filter and synchronised by its own
//    controller; the grid's rows get shorter so all of them fit (3 x 15);
//  - an entry in the Ghosts tab's list and a page of its own describing it. The list's entries are
//    placed by hand (its grid ships switched off), so its grid is set to that layout and switched
//    on, and its rows get shorter too.
// Every journal on this client is extended, other players' included, so the journal's page
// synchronisation (by page number) finds the same pages everywhere. Main thread.
namespace pg::custom::pages {

bool resolve();
void tick(bool enabled);  // enabled: the custom ghosts are in play (registry::active)
void teardown();

}  // namespace pg::custom::pages
