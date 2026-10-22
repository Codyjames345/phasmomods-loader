#pragma once

#include "mod_api.h"

// A mod's side of the host contract (core/mod_api.h). pgmod_start calls bind first; from then on the
// mod's log lines go to the host's log, pg::hooks goes through the host's MinHook, pg::net uses the
// mod's own channel, and the IL2CPP API is resolved for this module. Main thread.
namespace pg::mod {

bool bind(const PgHostApi* api, const PgModInfo* info);
const PgHostApi& host();
const PgModInfo& info();

// Whether the client of `actor` runs the mod `id` (core/mod_api.h peer_has_mod).
bool peer_has(int actor, const char* id);

// The bodies of a mod's pgmod_start, pgmod_tick and pgmod_stop, so that nothing thrown leaves the
// mod (a managed exception is logged with its stack). `name` names the mod in the log ("<name>
// active"); `running` is the mod's own flag: set by guarded_start, cleared by guarded_stop, and
// guarded_tick and guarded_stop do nothing while it is false.
bool guarded_start(const char* name, bool& running, bool (*start)(const PgHostApi*), const PgHostApi* api);
void guarded_tick(const bool& running, void (*tick)());
void guarded_stop(bool& running, void (*stop)());

}  // namespace pg::mod
