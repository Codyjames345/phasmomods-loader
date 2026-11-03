#pragma once

#include <chrono>
#include <cmath>

#include "content.h"
#include "text_table.h"

// The SDK's worked example (sdk/guides/adding-a-ghost.md): one custom ghost, the Knell from Custom Ghosts, as a mod of
// its own. Its journal page is in its string table (i18n/default.json), its evidence is two of the game's and Spectral
// Flame's (a required dependency in its manifest), and its one tell is a hunt-speed rule: a surge for one second, a crawl
// for the next, over and over, with a tick to tell one hunt from the next. Nothing here calls the game, so it runs in
// tests over the fake host. Main thread.
namespace example {

inline constexpr const char* kGhostId = "example.ghost.knell";  // "<mod id>.<name>", never changed once released

// The Knell's numbers (mods/custom_ghosts/knell_rule.h).
inline constexpr float kCycle = 2.0f;  // seconds: one surge and one crawl
inline constexpr float kFast = 1.75f;  // times the hunt's base speed during a surge
inline constexpr float kSlow = 0.35f;  // times the hunt's base speed during a crawl

/** The beat of the hunt going on. The host runs a rule's `tick` every frame and its `huntSpeed` every frame of a
 *  hunt (sdk/README.md, "Hunt speed"), so a frame with no hunt-speed call means the hunt is over and the next one
 *  starts its own beat. */
struct Beat {
  double start = -1;   // seconds on the steady clock when this hunt's beat began; -1 between hunts
  bool asked = false;  // whether the host asked the hunt speed since the last tick

  /** The hunt's base speed for the game's own `base` (metres a second) at `now` (seconds on any steady clock). */
  float speed(float base, double now) {
    if (start < 0) start = now;
    asked = true;
    return base * (std::fmod(now - start, kCycle) < kCycle / 2 ? kFast : kSlow);
  }

  /** Once a frame, from the rule's tick: ends the beat when the frame had no hunt. */
  void frame() {
    if (!asked) start = -1;
    asked = false;
  }
};

inline double seconds_now() {
  return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/** The hunt-speed rule (GhostRules::huntSpeed): the beat's speed, with the game's line-of-sight speed-up held off. */
inline bool hunt_speed(float* base, float* losScale, void* user) {
  if (!base || !losScale || !user) return false;
  *base = static_cast<Beat*>(user)->speed(*base, seconds_now());
  *losScale = 1.0f;
  return true;
}

/** The ghost as it is registered: its id, its journal page from the mod's table, in English, and its evidence. */
inline pm::api::content::GhostDef definition() {
  pm::api::content::GhostDef def;
  def.id = kGhostId;
  def.name = {"en", pm::i18n::text("ghost.knell.name")};
  def.description = {"en", pm::i18n::text("ghost.knell.description")};
  def.strength = {"en", pm::i18n::text("ghost.knell.strength")};
  def.weakness = {"en", pm::i18n::text("ghost.knell.weakness")};
  def.evidence = {"spirit_box", "orbs", "phasmomods.spectral-flame.flame"};
  return def;
}

/** The rule's tick (GhostRules::tick), every frame while the ghost acts as a Knell. */
inline void tick(void* user) {
  if (user) static_cast<Beat*>(user)->frame();
}

/** The rule's stop (GhostRules::stop), when the ghost stops acting as a Knell (a Mimic's switch, or no ghost any more): the
 *  next start begins with no beat. */
inline void stop(void* user) {
  if (user) *static_cast<Beat*>(user) = {};
}

/** Registers the ghost and attaches its rules, from pmmod_start. False when the host refused either (the log says
 *  why). */
inline bool register_knell(Beat& beat) {
  if (pm::api::content::register_ghost(definition()) <= 0) return false;
  pm::api::content::GhostRules rules;
  rules.user = &beat;
  rules.tick = &tick;
  rules.stop = &stop;
  rules.huntSpeed = &hunt_speed;
  return pm::api::content::attach_rules(kGhostId, rules) > 0;
}

}  // namespace example
