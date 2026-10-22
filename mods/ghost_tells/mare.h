#pragma once

// The Mare's tells (mare_rule.h) in the game. Every client answers the roll of its vanilla switch-off
// coroutine (it runs where the light was switched on); the host darkens a lit room before the Mare
// shows itself and darkens the rooms it hunted through when the hunt ends. Main thread.
namespace pg::tells::mare {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame while the tells are on: the ghost's GhostAI and behaviour type (`host`: this client runs it).
void tick(void* ghost, int type, bool host, float dt);
void reset();

}  // namespace pg::tells::mare
