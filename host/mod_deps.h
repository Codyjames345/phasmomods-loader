#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "mod_manifest.h"

// Which chosen mods start, in what order, and what a click in the Mods panel switches with it, from the
// mods' manifests alone (`dependencies` and `incompatibilities`, core/mod_manifest.h). Shared by the host
// and the launcher. The manifests are untrusted, so everything here is iterative and bounded: no
// recursion, at most kMaxMods mods start, and a cycle or a long chain ends like any other graph. Pure
// logic, tested offline (tests/mod_deps_test.cpp).
//
// The rules (sdk/README.md, "Dependencies"):
//  - A mod starts after every dependency it names that starts too, required and optional. The order of
//    the choice (launcher.ini's "enabled=" line, which the Mods panel saves) breaks ties.
//  - A required dependency that is not installed, not chosen, older than its minimumVersion, or that does
//    not start leaves the mod off, and that cascades. An old-format mod's version is unknown, so it never
//    meets a minimumVersion.
//  - An optional dependency changes only the order. One older than its minimumVersion counts as missing.
//    One that would close a cycle is ignored for the order.
//  - Mods joined by a cycle of required dependencies stay off. So does a mod that names itself or one
//    id twice, and a mod that names a chosen, installed mod in its incompatibilities (the other runs).
namespace pm::moddeps {

constexpr size_t kMaxMods = 256;  // that start at once: what another player's presence takes (host/presence.h)
constexpr size_t kMaxNotes = 16;  // Plan::notes named one by one, before one line counts the rest

/** A chosen mod that does not start, with why. */
struct LeftOut {
  std::string id;
  std::string row;      // for its Mods panel row, one clause with no full stop: "Needs Spectral Flame"
  std::string log;      // for the log, the whole chain: "it needs B, which needs C, which stays off: Needs D"
  std::string because;  // the dependency it is off for, when that one is off too; empty otherwise
};

struct Plan {
  std::vector<std::string> order;  // the chosen mods that may start, in the order to start them
  std::vector<LeftOut> off;        // the chosen mods that may not, in the order of the choice
  std::vector<std::string> notes;  // for the log: an optional dependency ignored, and why; kMaxNotes, then a count
};

/**
 * The plan for starting `chosen` (ids, in the order of the choice) from the `installed` manifests. A
 * chosen id that no installed mod has is not started and not listed; the first manifest of an id counts.
 */
Plan plan(const std::vector<modlist::Manifest>& installed, const std::vector<std::string>& chosen);

/**
 * Starts one mod: true when it runs (also when it already did). False leaves it off, with `row` and `log`
 * saying why, as LeftOut's do. It never throws.
 */
using Attempt = std::function<bool(const std::string& id, std::string& row, std::string& log)>;

struct Run {
  std::vector<std::string> started;  // in the order they started, those already running included
  std::vector<LeftOut> off;          // the plan's, then each that did not start, in order
};

/**
 * Carries out `plan`: each mod in its order is attempted unless a required dependency of it did not
 * start, in which case it stays off too ("Needs B, which did not start"), and so on down the chain.
 */
Run run(const Plan& plan, const std::vector<modlist::Manifest>& installed, const Attempt& attempt);

/** The order to stop mods that started in `started`'s order: the newest first, so no mod outlives one it needs. */
std::vector<std::string> stop_order(const std::vector<std::string>& started);

/**
 * The installed mods that require one of `ids`, directly or through another, not counting `ids`
 * themselves, in installed order: what goes off with them. It looks through the first kMaxMods installed:
 * one past them that requires one of `ids` is not named, and plan leaves it off all the same.
 */
std::vector<std::string> dependents(const std::vector<std::string>& ids, const std::vector<modlist::Manifest>& installed);

/**
 * The choice `ids` after switching `id` on or off, as the Mods panel and the launcher's ticks do. On also
 * switches on every installed mod it requires, directly or through another; off also switches off every
 * mod that requires it (dependents). Optional dependencies switch nothing. Ids are added at the end, in
 * the order they are reached.
 */
std::vector<std::string> switched(std::vector<std::string> ids, const std::vector<modlist::Manifest>& installed,
                                  const std::string& id, bool on);

/**
 * Why switching `id` on or off (from `before` to `after`) changed other mods too, for the player; empty
 * when it changed only `id`. Names come from the manifests.
 */
std::string switch_note(const std::vector<modlist::Manifest>& installed, const std::vector<std::string>& before,
                        const std::vector<std::string>& after, const std::string& id, bool on);

/** `ids` with what each of them requires switched on too: a choice saved before a mod gained a requirement. */
std::vector<std::string> with_requirements(std::vector<std::string> ids, const std::vector<modlist::Manifest>& installed);

}  // namespace pm::moddeps
