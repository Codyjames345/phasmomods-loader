#pragma once

#include <cstddef>
#include <cstdint>

// The Tempest (tempest_rule.h) in the game. It never changes the weather. The host strikes lightning
// as each of its hunts opens, again and again through the hunt (as often as the contract's weather
// says), and as each of its ghost events begins, and tells every client each strike; every client
// plays the game's own lightning (LightningController.PlayLocalLightning) and, in a hunt, shows the
// ghost only while a strike lights it up. The game's own storm lightning (heavy rain) lights it up
// too. behaviour.cpp sets its hunt speed by striking(). Main thread.
namespace pg::custom::tempest {

bool resolve();
bool create_hooks();  // the game's storm lightning; before hooks::enable_all
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
// The host, from restraint.cpp's state-change hook: one of its ghost events begins.
void event_begins();
bool available();  // its bindings resolved; without them it hunts as any ghost
bool striking();   // a strike lights it up now
void receive(int sender, const uint8_t* data, size_t size);
void reset();

}  // namespace pg::custom::tempest
