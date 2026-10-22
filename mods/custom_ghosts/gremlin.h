#pragma once

// The Gremlin (gremlin_rule.h) in the game, on the host. Some of its interaction rolls switch off a
// piece of running electronic equipment in its room instead of the usual interaction (Equipment.Use,
// which the game sends to every client), leaving the EMF of a ghost's touch where it stands. Main thread.
namespace pg::custom::gremlin {

bool resolve();
// The host's interaction roll for the Gremlin: true when it sabotaged something (the game's own
// interaction is then skipped).
bool interact(void* ghost);
// The player ability: switch off the nearest running electronics within reach; false if none.
bool short_out(void* ghost);

}  // namespace pg::custom::gremlin
