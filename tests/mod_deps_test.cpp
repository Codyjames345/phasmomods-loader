// Offline tests for which mods start, in what order, and what a click switches with it (host/mod_deps.h).
// Run: build\mod_deps_test.exe
#include "mod_deps.h"

#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "text_fixture.h"

namespace {

using pm::modlist::Dependency;
using pm::modlist::Manifest;
using namespace pm::moddeps;
using Ids = std::vector<std::string>;

Dependency needs(const std::string& id, const std::string& minimum = "") { return {id, minimum, true}; }
Dependency uses(const std::string& id, const std::string& minimum = "") { return {id, minimum, false}; }

Manifest mod(const std::string& id, std::vector<Dependency> deps = {}, const std::string& version = "1.0.0") {
  Manifest m;
  m.id = id;
  m.name = id;
  m.dll = "x.dll";
  m.version = version;
  m.dependencies = std::move(deps);
  return m;
}

const LeftOut* off(const Plan& p, const std::string& id) {
  for (const LeftOut& o : p.off)
    if (o.id == id) return &o;
  return nullptr;
}
const LeftOut* off(const Run& r, const std::string& id) {
  for (const LeftOut& o : r.off)
    if (o.id == id) return &o;
  return nullptr;
}
bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

// Every attempt starts, except those in `failing`, which say why.
Attempt starts_except(std::set<std::string> failing, Ids* tried = nullptr) {
  return [failing, tried](const std::string& id, std::string& row, std::string& log) {
    if (tried) tried->push_back(id);
    if (!failing.count(id)) return true;
    row = "It did not start";
    log = "its start failed";
    return false;
  };
}

void required_dependency_starts_first() {
  const std::vector<Manifest> all = {mod("me.custom", {needs("me.flame")}), mod("me.replay"), mod("me.flame")};
  const Plan p = plan(all, {"me.custom", "me.replay", "me.flame"});
  CHECK(p.order == (Ids{"me.flame", "me.custom", "me.replay"}));
  CHECK(p.off.empty() && p.notes.empty());
}

void optional_dependency_starts_first_when_present() {
  const std::vector<Manifest> all = {mod("me.ghost", {uses("me.custom")}), mod("me.custom")};
  CHECK(plan(all, {"me.ghost", "me.custom"}).order == (Ids{"me.custom", "me.ghost"}));
  // Ties keep the order of the choice.
  const std::vector<Manifest> three = {mod("me.a"), mod("me.b"), mod("me.c", {uses("me.a")})};
  CHECK(plan(three, {"me.b", "me.c", "me.a"}).order == (Ids{"me.b", "me.a", "me.c"}));
}

void missing_required_dependency_leaves_mod_off_with_reason() {
  std::vector<Manifest> all = {mod("me.custom", {needs("me.flame")}), mod("me.flame"), mod("me.other")};
  all[1].name = "Spectral Flame";
  // Installed and not chosen.
  Plan p = plan(all, {"me.custom", "me.other"});
  CHECK(p.order == Ids{"me.other"});
  CHECK(off(p, "me.custom") && off(p, "me.custom")->row == "Needs Spectral Flame");
  CHECK(off(p, "me.custom") && contains(off(p, "me.custom")->log, "Spectral Flame"));
  // Not installed at all: an id no installed mod has counts as missing, and is named by its id.
  p = plan({mod("me.custom", {needs("me.flame"), needs("me.ash")})}, {"me.custom"});
  CHECK(p.order.empty() && off(p, "me.custom") && off(p, "me.custom")->row == "Needs me.flame and 1 more");
  CHECK(off(p, "me.custom") && contains(off(p, "me.custom")->log, "me.flame") && contains(off(p, "me.custom")->log, "me.ash"));
  // A chosen id no installed mod has is not started and not listed.
  CHECK(plan(all, {"me.nobody"}).order.empty() && plan(all, {"me.nobody"}).off.empty());
}

void failed_required_dependency_cascades() {
  std::vector<Manifest> all = {mod("me.a", {needs("me.b")}), mod("me.b", {needs("me.c")}), mod("me.c"), mod("me.d")};
  all[1].name = "B";
  all[2].name = "C";
  const Plan p = plan(all, {"me.a", "me.b", "me.c", "me.d"});
  CHECK(p.order == (Ids{"me.c", "me.b", "me.a", "me.d"}));
  Ids tried;
  const Run r = run(p, all, starts_except({"me.c"}, &tried));
  CHECK(r.started == Ids{"me.d"});
  CHECK(tried == (Ids{"me.c", "me.d"}));  // neither B nor A is even tried
  CHECK(off(r, "me.c") && off(r, "me.c")->row == "It did not start");
  CHECK(off(r, "me.b") && off(r, "me.b")->row == "Needs C, which did not start" && off(r, "me.b")->because == "me.c");
  CHECK(off(r, "me.a") && off(r, "me.a")->row == "Needs B, which did not start" && off(r, "me.a")->because == "me.b");
  CHECK(off(r, "me.a") && contains(off(r, "me.a")->log, "B, which needs C"));  // the chain of why
  // Planned off, the same cascade, before anything is tried: C needs something missing.
  all[2].dependencies = {needs("me.gone")};
  const Plan q = plan(all, {"me.a", "me.b", "me.c", "me.d"});
  CHECK(q.order == Ids{"me.d"});
  CHECK(off(q, "me.c") && off(q, "me.c")->row == "Needs me.gone");
  CHECK(off(q, "me.b") && off(q, "me.b")->row == "Needs C, which did not start");
  CHECK(off(q, "me.a") && contains(off(q, "me.a")->log, "B, which needs C, which stays off: Needs me.gone"));
}

void missing_optional_dependency_still_starts() {
  const std::vector<Manifest> all = {mod("me.ghost", {uses("me.custom")}), mod("me.custom")};
  const Plan p = plan(all, {"me.ghost"});
  CHECK(p.order == Ids{"me.ghost"} && p.off.empty());
  // Nor does one that is chosen and then does not start stop it.
  const Run r = run(plan(all, {"me.custom", "me.ghost"}), all, starts_except({"me.custom"}));
  CHECK(r.started == Ids{"me.ghost"} && r.off.size() == 1);
}

void required_cycle_refused_and_named() {
  std::vector<Manifest> all = {mod("me.a", {needs("me.b")}), mod("me.b", {needs("me.c")}), mod("me.c", {needs("me.a")}),
                               mod("me.d", {needs("me.a")}), mod("me.e")};
  for (Manifest& m : all) m.name = m.id.substr(3);
  const Plan p = plan(all, {"me.a", "me.b", "me.c", "me.d", "me.e"});
  CHECK(p.order == Ids{"me.e"});
  for (const char* id : {"me.a", "me.b", "me.c"}) {
    CHECK(off(p, id) && contains(off(p, id)->row, "Its required mods form a cycle"));
  }
  CHECK(off(p, "me.a") && off(p, "me.a")->row == "Its required mods form a cycle: a, b, c");
  CHECK(off(p, "me.a") && contains(off(p, "me.a")->log, "a needs b, which needs c, which needs a"));
  CHECK(off(p, "me.d") && off(p, "me.d")->row == "Needs a, which did not start");  // outside the cycle: it cascades
}

void optional_edge_closing_cycle_ignored_for_order() {
  const std::vector<Manifest> all = {mod("me.a", {needs("me.b")}), mod("me.b", {uses("me.a")})};
  const Plan p = plan(all, {"me.b", "me.a"});
  CHECK(p.order == (Ids{"me.b", "me.a"}) && p.off.empty());
  CHECK(p.notes.size() == 1 && contains(p.notes[0], "me.b") && contains(p.notes[0], "me.a") &&
        contains(p.notes[0], "cycle"));
  // Two optional edges between two mods: the first in the choice's order counts, the other is ignored.
  const std::vector<Manifest> both = {mod("me.a", {uses("me.b")}), mod("me.b", {uses("me.a")})};
  const Plan q = plan(both, {"me.a", "me.b"});
  CHECK(q.order == (Ids{"me.b", "me.a"}) && q.notes.size() == 1);
}

void self_and_duplicate_dependency_refused() {
  const std::vector<Manifest> all = {mod("me.self", {needs("me.self")}), mod("me.twice", {uses("me.x"), needs("me.x")}),
                                     mod("me.x")};
  const Plan p = plan(all, {"me.self", "me.twice", "me.x"});
  CHECK(p.order == Ids{"me.x"});
  CHECK(off(p, "me.self") && off(p, "me.self")->row == "It names itself as a dependency");
  CHECK(off(p, "me.twice") && off(p, "me.twice")->row == "It names one dependency twice");
}

void stop_runs_in_reverse_start_order() {
  const std::vector<Manifest> all = {mod("me.custom", {needs("me.flame")}), mod("me.ghost", {uses("me.custom")}),
                                     mod("me.flame")};
  const Run r = run(plan(all, {"me.ghost", "me.custom", "me.flame"}), all, starts_except({}));
  CHECK(r.started == (Ids{"me.flame", "me.custom", "me.ghost"}));
  CHECK(stop_order(r.started) == (Ids{"me.ghost", "me.custom", "me.flame"}));
  CHECK(stop_order({}).empty());
}

void panel_enabling_turns_on_required_dependencies() {
  const std::vector<Manifest> all = {mod("me.ghost", {uses("me.custom")}), mod("me.custom", {needs("me.flame")}),
                                     mod("me.flame", {needs("me.base")}), mod("me.base")};
  CHECK(switched({"me.other"}, all, "me.custom", true) == (Ids{"me.other", "me.custom", "me.flame", "me.base"}));
  CHECK(switched({"me.base"}, all, "me.custom", true) == (Ids{"me.base", "me.custom", "me.flame"}));
  // Optional dependencies switch nothing.
  CHECK(switched({}, all, "me.ghost", true) == Ids{"me.ghost"});
  // A required mod that is not installed is not added; the mod itself is, and its row will say why it is off.
  CHECK(switched({}, {mod("me.a", {needs("me.gone")})}, "me.a", true) == Ids{"me.a"});
  CHECK(switched({"me.base"}, all, "me.base", true) == Ids{"me.base"});  // already on
  // A cycle of requirements ends.
  const std::vector<Manifest> loop = {mod("me.a", {needs("me.b")}), mod("me.b", {needs("me.a")})};
  CHECK(switched({}, loop, "me.a", true) == (Ids{"me.a", "me.b"}));
  // A choice saved before a mod gained a requirement still starts it.
  CHECK(with_requirements({"me.ghost", "me.custom"}, all) == (Ids{"me.ghost", "me.custom", "me.flame", "me.base"}));
  CHECK(with_requirements({}, all).empty());
}

void panel_disabling_turns_off_dependents() {
  std::vector<Manifest> all = {mod("me.ghost", {uses("me.custom")}), mod("me.custom", {needs("me.flame")}),
                               mod("me.flame"), mod("me.other", {needs("me.flame")})};
  all[1].name = "Custom Ghosts";
  all[2].name = "Spectral Flame";
  all[3].name = "Other";
  const Ids everything{"me.ghost", "me.custom", "me.flame", "me.other"};
  CHECK(switched(everything, all, "me.flame", false) == Ids{"me.ghost"});  // optional users stay
  CHECK(switched(everything, all, "me.custom", false) == (Ids{"me.ghost", "me.flame", "me.other"}));
  CHECK(switched({"me.flame"}, all, "me.unknown", false) == Ids{"me.flame"});
  CHECK(dependents({"me.flame"}, all) == (Ids{"me.custom", "me.other"}));
  CHECK(dependents({"me.custom"}, all).empty());
  // What the panel says when a click switched more than one mod.
  CHECK(switch_note(all, {}, {"me.custom", "me.flame"}, "me.custom", true) ==
        "Custom Ghosts needs Spectral Flame, so they are switched on together.");
  CHECK(switch_note(all, {"me.custom", "me.flame"}, {}, "me.flame", false) ==
        "Custom Ghosts needs Spectral Flame, so they are switched off together.");
  CHECK(switch_note(all, everything, {"me.ghost"}, "me.flame", false) ==
        "Custom Ghosts, Other need Spectral Flame, so they are switched off together.");
  CHECK(switch_note(all, {"me.flame"}, {}, "me.flame", false).empty());  // only itself
}

void mod_without_dependencies_depends_on_nothing() {
  Manifest old = mod("oldmod", {}, "");
  old.oldFormat = true;
  const std::vector<Manifest> all = {mod("me.a"), old, mod("me.b")};
  const Plan p = plan(all, {"me.b", "oldmod", "me.a"});
  CHECK(p.order == (Ids{"me.b", "oldmod", "me.a"}) && p.off.empty() && p.notes.empty());
  CHECK(switched({}, all, "me.a", true) == Ids{"me.a"});
  CHECK(switched(Ids{"me.a", "oldmod", "me.b"}, all, "me.a", false) == (Ids{"oldmod", "me.b"}));
  CHECK(dependents({"me.a", "oldmod"}, all).empty());
}

void dependency_below_minimum_version_counts_as_missing() {
  std::vector<Manifest> all = {mod("me.custom", {needs("me.flame", "1.2.0")}), mod("me.flame", {}, "1.0.10"),
                               mod("me.ghost", {uses("me.custom", "2.0.0")})};
  all[1].name = "Spectral Flame";
  Plan p = plan(all, {"me.custom", "me.flame", "me.ghost"});
  CHECK(off(p, "me.custom") && off(p, "me.custom")->row == "Needs Spectral Flame 1.2.0 or newer, and you have 1.0.10");
  CHECK(p.order == (Ids{"me.flame", "me.ghost"}));
  // Versions compare as numbers: 1.10.0 meets 1.2.0.
  all[1].version = "1.10.0";
  p = plan(all, {"me.custom", "me.flame", "me.ghost"});
  CHECK(p.off.empty() && p.order == (Ids{"me.flame", "me.custom", "me.ghost"}));
  // An optional one below its minimum counts as missing: it changes nothing, not even the order, and the log says so.
  CHECK(p.notes.size() == 1 && contains(p.notes[0], "2.0.0") && contains(p.notes[0], "1.0.0"));
  p = plan(all, {"me.ghost", "me.custom", "me.flame"});
  CHECK(p.order == (Ids{"me.ghost", "me.flame", "me.custom"}));
  // An old-format mod's version is unknown, so it never meets a minimumVersion.
  all[1].version.clear();
  all[1].oldFormat = true;
  p = plan(all, {"me.custom", "me.flame"});
  CHECK(off(p, "me.custom") && off(p, "me.custom")->row == "Needs Spectral Flame 1.2.0 or newer, and its version is unknown");
}

void incompatible_mod_stays_off_with_reason() {
  std::vector<Manifest> all = {mod("me.a"), mod("me.b"), mod("me.c"), mod("me.d")};
  all[0].incompatibilities = {"me.b", "me.gone"};
  all[1].name = "B";
  all[2].incompatibilities = {"me.d"};
  all[3].incompatibilities = {"me.c"};
  all[3].name = "D";
  Plan p = plan(all, {"me.a", "me.b", "me.c", "me.d"});
  CHECK(off(p, "me.a") && off(p, "me.a")->row == "Can't run beside B");
  CHECK(!off(p, "me.b"));  // the other mod is unaffected
  CHECK(off(p, "me.c") && off(p, "me.c")->row == "Can't run beside D" && off(p, "me.d"));  // each in a mutual refusal
  CHECK(p.order == Ids{"me.b"});
  // Naming a mod that is installed and not chosen, or not installed, refuses nothing.
  p = plan(all, {"me.a", "me.c"});
  CHECK(p.order == (Ids{"me.a", "me.c"}) && p.off.empty());
}

std::string read_source(const std::string& relative) {
  std::ifstream in(std::string(PM_SOURCE_DIR) + "/" + relative, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

// Playable Ghost optionally uses Custom Ghosts, so it starts after it whatever order the choice holds, and a player
// Lithobolia's aimed throw stays paid for (its hooks then run before Custom Ghosts' on the same method).
void playable_ghost_starts_after_custom_ghosts() {
  std::vector<Manifest> bundled;
  for (const char* folder : {"spectral_flame", "playable_ghost", "ghost_tells", "custom_ghosts"}) {
    Manifest m;
    std::string why;
    CHECK(pm::modlist::parse_manifest_json(read_source(std::string("mods/") + folder + "/manifest.json"), m, why));
    bundled.push_back(m);
  }
  for (const Ids& choice : {Ids{"phasmomods.playable-ghost", "phasmomods.custom-ghosts", "phasmomods.spectral-flame"},
                            Ids{"phasmomods.custom-ghosts", "phasmomods.playable-ghost", "phasmomods.spectral-flame"},
                            Ids{"phasmomods.spectral-flame", "phasmomods.custom-ghosts", "phasmomods.ghost-tells",
                                "phasmomods.playable-ghost"}}) {
    const Plan p = plan(bundled, choice);
    CHECK(p.off.empty() && p.notes.empty());
    size_t flame = p.order.size(), custom = p.order.size(), ghost = p.order.size();
    for (size_t i = 0; i < p.order.size(); ++i) {
      if (p.order[i] == "phasmomods.spectral-flame") flame = i;
      if (p.order[i] == "phasmomods.custom-ghosts") custom = i;
      if (p.order[i] == "phasmomods.playable-ghost") ghost = i;
    }
    CHECK(flame < custom && custom < ghost && ghost < p.order.size());
  }
  // The default order, the folders' (as the first run saves it), is the order the mods started in before.
  const Plan folders = plan(bundled, {"phasmomods.custom-ghosts", "phasmomods.ghost-tells", "phasmomods.playable-ghost",
                                      "phasmomods.spectral-flame"});
  CHECK(folders.order == (Ids{"phasmomods.spectral-flame", "phasmomods.custom-ghosts", "phasmomods.ghost-tells",
                              "phasmomods.playable-ghost"}));
  // Without Spectral Flame, Custom Ghosts stays off and says why; Playable Ghost starts without it.
  const Plan alone = plan(bundled, {"phasmomods.custom-ghosts", "phasmomods.playable-ghost"});
  CHECK(alone.order == Ids{"phasmomods.playable-ghost"});
  CHECK(off(alone, "phasmomods.custom-ghosts") && off(alone, "phasmomods.custom-ghosts")->row == "Needs Spectral Flame");
}

// Manifests come from anybody: a long chain, a cycle through everything and more mods than may start all end, with
// every mod accounted for. Nothing recurses, so a deep chain costs no native stack.
void untrusted_graphs_are_bounded() {
  std::vector<Manifest> chain;
  Ids forward;
  for (size_t i = 0; i < 300; ++i) {
    chain.push_back(mod("me.m" + std::to_string(i), i ? std::vector<Dependency>{needs("me.m" + std::to_string(i - 1))}
                                                       : std::vector<Dependency>{}));
    forward.push_back(chain.back().id);
  }
  // The first kMaxMods of the choice may start; the rest say why not.
  Plan p = plan(chain, forward);
  CHECK(p.order.size() == kMaxMods && p.order.front() == "me.m0" && p.order.back() == "me.m255");
  CHECK(p.off.size() == 300 - kMaxMods && off(p, "me.m256") && off(p, "me.m256")->row == "More than 256 mods are on");
  // The deepest first: the mods past the limit are the chain's roots, so everything else cascades. However long the
  // chain, its log is bounded.
  const Ids reversed(forward.rbegin(), forward.rend());
  p = plan(chain, reversed);
  CHECK(p.order.empty() && p.off.size() == 300);
  CHECK(off(p, "me.m0") && off(p, "me.m0")->row == "More than 256 mods are on");
  CHECK(off(p, "me.m299") && off(p, "me.m299")->row == "Needs me.m298, which did not start");
  CHECK(off(p, "me.m299") && off(p, "me.m299")->log.size() < 1024 &&
        contains(off(p, "me.m299")->log, "which stays off: More than 256 mods are on"));
  // What goes off with a mod looks through as many mods as may start, however many are installed. One past them that needed
  // it is left off by the plan all the same.
  const Ids goesOff = dependents({"me.m0"}, chain);
  CHECK(goesOff.size() == kMaxMods - 1 && goesOff.front() == "me.m1" && goesOff.back() == "me.m255");
  CHECK(plan(chain, forward).order.size() == kMaxMods);
  // Everything in one required cycle.
  chain[0].dependencies = {needs("me.m255")};
  p = plan(std::vector<Manifest>(chain.begin(), chain.begin() + 256), Ids(forward.begin(), forward.begin() + 256));
  CHECK(p.order.empty() && p.off.size() == 256);
  CHECK(off(p, "me.m0") && contains(off(p, "me.m0")->row, "and 252 more") && off(p, "me.m0")->log.size() < 1024);
  // Optional edges everywhere, both ways.
  std::vector<Manifest> mesh;
  Ids all;
  for (size_t i = 0; i < 64; ++i) all.push_back("me.n" + std::to_string(i));
  for (size_t i = 0; i < 64; ++i) {
    std::vector<Dependency> deps;
    for (size_t j = 0; j < 64; ++j)
      if (j != i) deps.push_back(uses(all[j]));
    mesh.push_back(mod(all[i], deps));
  }
  p = plan(mesh, all);
  // Each mod's edges to those after it in the choice count, and the ones back would close a cycle, so are noted.
  CHECK(p.order.size() == 64 && p.off.empty() && p.order.front() == "me.n63" && p.order.back() == "me.n0");
  // The log gets the first of them and a count of the rest, however many a manifest asks for.
  CHECK(p.notes.size() == kMaxNotes + 1 && contains(p.notes.back(), std::to_string(64 * 63 / 2 - kMaxNotes) + " more"));

  // The worst the parser lets through: as many mods as may start, each with the most optional dependencies a manifest holds.
  std::vector<Manifest> widest;
  Ids everyMod;
  for (size_t i = 0; i < kMaxMods; ++i) everyMod.push_back("me.w" + std::to_string(i));
  for (size_t i = 0; i < kMaxMods; ++i) {
    std::vector<Dependency> deps;
    for (size_t j = 1; j <= pm::modlist::kMaxListEntries; ++j) deps.push_back(uses(everyMod[(i + j) % kMaxMods]));
    widest.push_back(mod(everyMod[i], deps));
  }
  p = plan(widest, everyMod);
  CHECK(p.order.size() == kMaxMods && p.off.empty() && p.notes.size() == kMaxNotes + 1);
  CHECK(std::set<std::string>(p.order.begin(), p.order.end()).size() == kMaxMods);  // each once
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  required_dependency_starts_first();
  optional_dependency_starts_first_when_present();
  missing_required_dependency_leaves_mod_off_with_reason();
  failed_required_dependency_cascades();
  missing_optional_dependency_still_starts();
  required_cycle_refused_and_named();
  optional_edge_closing_cycle_ignored_for_order();
  self_and_duplicate_dependency_refused();
  stop_runs_in_reverse_start_order();
  panel_enabling_turns_on_required_dependencies();
  panel_disabling_turns_off_dependents();
  mod_without_dependencies_depends_on_nothing();
  dependency_below_minimum_version_counts_as_missing();
  incompatible_mod_stays_off_with_reason();
  playable_ghost_starts_after_custom_ghosts();
  untrusted_graphs_are_bounded();
  return g_failures ? 1 : 0;
}
