#pragma once

// A watch on Photon's room cleanup, for the logs. PUN runs PhotonNetwork.LeftRoomCleanup in two places. One is
// as this client disconnects, on a leave and as the game quits, before the client's peer disconnects and closes
// Photon's socket: when it throws there, the peer never disconnects, Photon's receive thread stays in its
// blocking receive, and the runtime's shutdown waits on that thread for ever. The other is
// PhotonNetwork.set_OfflineMode, where a throw stops no disconnect. Vanilla's cleanup is not known to throw; a
// PhotonView left registered for an object Unity destroyed without its OnDestroy (one never activated) makes it
// throw, and that is how our own code once hung the game's exit. The watch changes nothing: it logs the
// cleanup's end, or what it threw and which registered views had lost their objects, and lets the exception go on.
namespace pm::room_watch {

// Hooks the cleanup when its entry resolved; otherwise, and when the hook cannot be made, it logs why and
// the game runs unwatched. Main thread, before hooks::enable_all of the hooks it makes.
bool create_hook();

// What the hook does around the cleanup, without the hook: runs `cleanup` and logs that it finished, or
// logs what it threw with the registered views whose engine object is gone, and rethrows. Main thread.
void run_cleanup(void (*cleanup)(const void* method), const void* method);

// Logs each PhotonView PUN holds registered whose engine object is gone (a null entry included), by its
// view id, and returns how many it logged. Main thread.
size_t log_dead_views();

}  // namespace pm::room_watch
