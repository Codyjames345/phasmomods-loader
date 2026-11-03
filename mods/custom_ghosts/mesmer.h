#pragma once

// The Mesmer (mesmer_rule.h) in the game, on every client.
//  - Its appearances: each client shows its own player another of the ghost's forms, a little way off
//    and walking as the ghost walks (an apparition on the one free rendering layer, drawn by the player's own camera only),
//    and hides the true one from that camera alone; video cameras and photos keep seeing it as it is.
//    In a hunt each player sees that form too, where the ghost truly is, flickering as it does. A player
//    sees one form through the contract and other players another, so they find it out by comparing.
//  - The truck's monitor (HeartRateData.UpdateUI) is shown every player's sanity a fifth higher.
//  - Outside hunts the lights in its favourite room dim and brighten slowly instead of flickering
//    (LightSwitch.FlickerNetworked is not shown there). Main thread.
namespace pm::custom::mesmer {

bool resolve();
bool create_hooks();  // before hooks::enable_all
// Every frame on every client: ghost is the custom ghost's GhostAI (null for none), type its type.
void tick(void* ghost, int type);
void reset();

}  // namespace pm::custom::mesmer
