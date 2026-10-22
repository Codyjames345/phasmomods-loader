#pragma once

// The Mesmer (mesmer_rule.h) in the game, on every client.
//  - Its appearances: each client shows its own player another of the ghost's forms, standing a little
//    way off (an apparition on the one free rendering layer, drawn by the player's own camera only),
//    and hides the true one from that camera alone; video cameras and photos keep seeing it as it is.
//    In a hunt each player sees another form too, where the ghost truly is, flickering as it does.
//  - The truck's monitor (HeartRateData.UpdateUI) is shown every player's sanity a fifth higher.
//  - Outside hunts the lights in its favourite room dim and brighten slowly instead of flickering
//    (LightSwitch.FlickerNetworked is not shown there). Main thread.
namespace pg::custom::mesmer {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
void reset();

}  // namespace pg::custom::mesmer
