#include "mod_deps.h"

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <set>
#include <utility>

#include "mod_list.h"
#include "text_table.h"

namespace pm::moddeps {
namespace {

using modlist::Dependency;
using modlist::Manifest;

constexpr size_t kNamesInRow = 4;    // a row names at most this many mods, then "and N more"
constexpr size_t kLinksInLog = 8;    // a log's chain names at most this many links before the one it ends in
constexpr size_t kNoSlot = static_cast<size_t>(-1);

// The first installed manifest of each id.
std::map<std::string, size_t, std::less<>> index_of(const std::vector<Manifest>& installed) {
  std::map<std::string, size_t, std::less<>> at;
  for (size_t i = 0; i < installed.size(); ++i) at.try_emplace(installed[i].id, i);
  return at;
}

std::string listed(const std::vector<std::string>& names, size_t shown) {
  std::string text;
  for (size_t i = 0; i < names.size() && i < shown; ++i) text += (i ? ", " : "") + names[i];
  if (names.size() > shown) text += " and " + std::to_string(names.size() - shown) + " more";
  return text;
}

// As listed, for a row the player reads: the separators and the count from the loader's table.
std::string listed_shown(const std::vector<std::string>& names, size_t shown) {
  std::string text;
  for (size_t i = 0; i < names.size() && i < shown; ++i) text += (i ? i18n::text("list.separator") : std::string()) + names[i];
  if (names.size() > shown) text = i18n::text("deps.and-more", {{"first", text}, {"count", std::to_string(names.size() - shown)}});
  return text;
}

// Whether `have` (empty when unknown) meets `minimum` (empty for any). What cannot be read does not meet it.
bool meets(const std::string& have, const std::string& minimum) {
  if (minimum.empty()) return true;
  const std::optional<modid::Version> h = modid::parse_version(have), m = modid::parse_version(minimum);
  return h && m && *h >= *m;
}

// The chosen mods and the edges between them, by their place in the choice (a slot).
struct Graph {
  const std::vector<Manifest>& installed;
  std::map<std::string, size_t, std::less<>> at;  // id -> installed index
  std::vector<size_t> mods;                       // slot -> installed index
  std::vector<size_t> slotOf;                     // installed index -> slot, or kNoSlot

  Graph(const std::vector<Manifest>& all, const std::vector<std::string>& chosen)
      : installed(all), at(index_of(all)), slotOf(all.size(), kNoSlot) {
    for (const std::string& id : chosen) {
      const auto it = at.find(id);
      if (it == at.end() || slotOf[it->second] != kNoSlot) continue;
      slotOf[it->second] = mods.size();
      mods.push_back(it->second);
    }
  }
  const Manifest& of(size_t slot) const { return installed[mods[slot]]; }
  // The slot of the chosen mod `id`, or kNoSlot.
  size_t slot(const std::string& id) const {
    const auto it = at.find(id);
    return it == at.end() ? kNoSlot : slotOf[it->second];
  }
  const Manifest* manifest(const std::string& id) const {
    const auto it = at.find(id);
    return it == at.end() ? nullptr : &installed[it->second];
  }
  std::string name(const std::string& id) const {
    const Manifest* m = manifest(id);
    return m && !m->name.empty() ? m->name : id;
  }
};

// "it needs B, which needs C, which stays off: <C's row>", following each `because` from `first`, bounded.
std::string chain_log(const Graph& g, const std::vector<LeftOut>& off, const std::map<std::string, size_t>& offAt,
                      const std::string& first) {
  std::string text = "it needs " + g.name(first);
  std::string at = first;
  size_t links = 1, skipped = 0;
  for (size_t steps = 0; steps <= off.size(); ++steps) {
    const auto it = offAt.find(at);
    if (it == offAt.end()) break;
    const LeftOut& o = off[it->second];
    if (o.because.empty()) {
      if (skipped) text += ", and " + std::to_string(skipped) + " more, ending in " + g.name(at);
      return text + ", which stays off: " + o.row;
    }
    if (links < kLinksInLog) {
      text += ", which needs " + g.name(o.because);
      ++links;
    } else {
      ++skipped;
    }
    at = o.because;
  }
  return text + ", which did not start";
}

// The required dependencies among the chosen mods, as edges from a mod's slot to its dependencies' slots.
// A mod past kMaxMods is off whatever it names, so it has no edges: every walk below stays within kMaxMods mods and
// their at most kMaxListEntries dependencies each, however many mods are chosen.
std::vector<std::vector<size_t>> required_edges(const Graph& g) {
  std::vector<std::vector<size_t>> edges(g.mods.size());
  for (size_t k = 0; k < g.mods.size() && k < kMaxMods; ++k)
    for (const Dependency& d : g.of(k).dependencies) {
      const size_t to = g.slot(d.id);
      if (d.required && to != kNoSlot && to != k && std::find(edges[k].begin(), edges[k].end(), to) == edges[k].end())
        edges[k].push_back(to);
    }
  return edges;
}

// The shortest cycle through `k` by `edges`, as slots from k back to just before k; empty when k is on none.
std::vector<size_t> cycle_through(size_t k, const std::vector<std::vector<size_t>>& edges) {
  std::vector<size_t> parent(edges.size(), kNoSlot);
  std::deque<size_t> queue;
  for (size_t to : edges[k])
    if (parent[to] == kNoSlot) {
      parent[to] = k;
      queue.push_back(to);
    }
  while (!queue.empty()) {
    const size_t u = queue.front();
    queue.pop_front();
    if (u == k) {
      std::vector<size_t> path;
      for (size_t at = parent[k]; at != k; at = parent[at]) path.push_back(at);
      path.push_back(k);
      std::reverse(path.begin(), path.end());
      return path;
    }
    for (size_t to : edges[u])
      if (parent[to] == kNoSlot) {
        parent[to] = u;
        queue.push_back(to);
      }
  }
  return {};
}

// Whether `from` reaches `to` by `edges`.
bool reaches(size_t from, size_t to, const std::vector<std::vector<size_t>>& edges) {
  std::vector<bool> seen(edges.size(), false);
  std::vector<size_t> stack{from};
  seen[from] = true;
  while (!stack.empty()) {
    const size_t u = stack.back();
    stack.pop_back();
    if (u == to) return true;
    for (size_t v : edges[u])
      if (!seen[v]) {
        seen[v] = true;
        stack.push_back(v);
      }
  }
  return false;
}

}  // namespace

Plan plan(const std::vector<Manifest>& installed, const std::vector<std::string>& chosen) {
  const Graph g(installed, chosen);
  const size_t n = g.mods.size();
  std::vector<std::string> row(n), log(n), because(n);
  const auto leave = [&](size_t k, std::string r, std::string l) {
    if (!row[k].empty()) return;
    row[k] = std::move(r);
    log[k] = std::move(l);
  };
  // What is wrong with each mod by itself, its own declaration first.
  for (size_t k = 0; k < n; ++k) {
    const Manifest& m = g.of(k);
    if (k >= kMaxMods) {
      leave(k, i18n::text("deps.too-many", {{"count", std::to_string(kMaxMods)}}), "more than " + std::to_string(kMaxMods) + " mods are on");
      continue;
    }
    std::set<std::string, std::less<>> named;
    for (const Dependency& d : m.dependencies) {
      if (d.id == m.id) leave(k, i18n::text("deps.names-itself"), "it names itself as a dependency");
      else if (!named.insert(d.id).second) leave(k, i18n::text("deps.names-twice"), "it names " + d.id + " as a dependency twice");
    }
    for (const std::string& other : m.incompatibilities)
      if (other != m.id && g.slot(other) != kNoSlot)
        leave(k, i18n::text("deps.incompatible", {{"mod", g.name(other)}}), "it can't run beside " + other + ", which is on");
    std::vector<std::string> lacking, lackingLog;
    for (const Dependency& d : m.dependencies) {
      if (!d.required) continue;
      const Manifest* dep = g.manifest(d.id);
      if (!dep || g.slot(d.id) == kNoSlot) {
        lacking.push_back(g.name(d.id));
        lackingLog.push_back(g.name(d.id) + (dep ? ", which is off" : ", which is not installed"));
      } else if (!meets(dep->version, d.minimumVersion)) {
        const std::string has =
            dep->version.empty() ? i18n::text("deps.version-unknown") : i18n::text("deps.you-have", {{"version", dep->version}});
        lacking.push_back(i18n::text("deps.version-needed", {{"mod", g.name(d.id)}, {"minimum", d.minimumVersion}, {"has", has}}));
        lackingLog.push_back(lacking.back());
      }
    }
    if (!lacking.empty())
      leave(k, i18n::text("deps.needs", {{"mods", lacking.size() == 1 ? lacking[0]
                                                                   : i18n::text("deps.and-more", {{"first", lacking[0]},
                                                                                                  {"count", std::to_string(lacking.size() - 1)}})}}),
            "it needs " + listed(lackingLog, lackingLog.size()));
  }
  // Mods joined by a cycle of required dependencies, whatever else is wrong with them.
  const std::vector<std::vector<size_t>> required = required_edges(g);
  for (size_t k = 0; k < n && k < kMaxMods; ++k) {
    if (!row[k].empty()) continue;
    const std::vector<size_t> cycle = cycle_through(k, required);
    if (cycle.empty()) continue;
    std::vector<std::string> names;
    for (size_t s : cycle) names.push_back(g.name(g.of(s).id));
    std::string chain = names[0];
    for (size_t i = 1; i < names.size() && i <= kLinksInLog; ++i) chain += (i == 1 ? " needs " : ", which needs ") + names[i];
    if (names.size() > kLinksInLog + 1) chain += ", and " + std::to_string(names.size() - kLinksInLog - 1) + " more";
    leave(k, i18n::text("deps.cycle", {{"mods", listed_shown(names, kNamesInRow)}}), chain + ", which needs " + names[0]);
  }
  // Each mod whose required dependency is off is off too, down every chain: at most n passes.
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t k = 0; k < n; ++k) {
      if (!row[k].empty()) continue;
      for (size_t to : required[k])
        if (!row[to].empty()) {
          row[k] = i18n::text("deps.needs-not-started", {{"mod", g.name(g.of(to).id)}});
          because[k] = g.of(to).id;
          grew = true;
          break;
        }
    }
  }
  Plan p;
  std::map<std::string, size_t> offAt;
  for (size_t k = 0; k < n; ++k)
    if (!row[k].empty()) {
      offAt[g.of(k).id] = p.off.size();
      p.off.push_back({g.of(k).id, row[k], log[k], because[k]});
    }
  for (LeftOut& o : p.off)
    if (!o.because.empty()) o.log = chain_log(g, p.off, offAt, o.because);
  // The order: every required edge between mods that start, then each optional one that keeps it a cycle-free graph.
  std::vector<std::vector<size_t>> edges(n);
  for (size_t k = 0; k < n; ++k)
    if (row[k].empty()) edges[k] = required[k];
  size_t unlisted = 0;  // notes past kMaxNotes: a manifest can ask for thousands, on every start
  const auto note = [&](std::string text) {
    if (p.notes.size() < kMaxNotes) p.notes.push_back(std::move(text));
    else ++unlisted;
  };
  for (size_t k = 0; k < n; ++k) {
    if (!row[k].empty()) continue;
    const Manifest& m = g.of(k);
    for (const Dependency& d : m.dependencies) {
      const size_t to = g.slot(d.id);
      if (d.required || to == kNoSlot || to == k || !row[to].empty()) continue;
      if (!meets(g.of(to).version, d.minimumVersion)) {
        const std::string& v = g.of(to).version;
        note(m.id + " uses " + d.id + " only from " + d.minimumVersion + ", and it has " +
             (v.empty() ? "an unknown version" : v) + ", so it starts as if " + d.id + " were missing");
        continue;
      }
      if (std::find(edges[k].begin(), edges[k].end(), to) != edges[k].end()) continue;
      if (reaches(to, k, edges)) {
        note("the optional dependency of " + m.id + " on " + d.id + " would close a cycle, so it is ignored for the start order");
        continue;
      }
      edges[k].push_back(to);
    }
  }
  if (unlisted) p.notes.push_back("and " + std::to_string(unlisted) + " more optional dependencies were left out of the start order");
  // Each mod after what it depends on, otherwise in the order of the choice: a depth-first walk with its own stack.
  for (std::vector<size_t>& e : edges) std::sort(e.begin(), e.end());
  std::vector<bool> placed(n, false), open(n, false);
  for (size_t root = 0; root < n; ++root) {
    if (!row[root].empty() || placed[root]) continue;
    std::vector<std::pair<size_t, size_t>> stack{{root, 0}};  // a mod and the next of its edges to follow
    open[root] = true;
    while (!stack.empty()) {
      auto& [k, next] = stack.back();
      if (next < edges[k].size()) {
        const size_t to = edges[k][next++];
        if (!placed[to] && !open[to]) {
          open[to] = true;
          stack.push_back({to, 0});
        }
        continue;
      }
      placed[k] = true;
      open[k] = false;
      p.order.push_back(g.of(k).id);
      stack.pop_back();
    }
  }
  return p;
}

Run run(const Plan& plan, const std::vector<Manifest>& installed, const Attempt& attempt) {
  const Graph g(installed, plan.order);
  Run r;
  r.off = plan.off;
  std::map<std::string, size_t> offAt;
  for (size_t i = 0; i < r.off.size(); ++i) offAt[r.off[i].id] = i;
  for (const std::string& id : plan.order) {
    const Manifest* m = g.manifest(id);
    std::string failed;
    if (m)
      for (const Dependency& d : m->dependencies)
        if (d.required && offAt.count(d.id)) {
          failed = d.id;
          break;
        }
    LeftOut o{id, {}, {}, failed};
    if (!failed.empty()) {
      o.row = i18n::text("deps.needs-not-started", {{"mod", g.name(failed)}});
      o.log = chain_log(g, r.off, offAt, failed);
    } else if (attempt(id, o.row, o.log)) {
      r.started.push_back(id);
      continue;
    }
    offAt[id] = r.off.size();
    r.off.push_back(std::move(o));
  }
  return r;
}

std::vector<std::string> stop_order(const std::vector<std::string>& started) { return {started.rbegin(), started.rend()}; }

std::vector<std::string> dependents(const std::vector<std::string>& ids, const std::vector<Manifest>& installed) {
  std::set<std::string, std::less<>> off(ids.begin(), ids.end());
  const size_t looked = std::min(installed.size(), kMaxMods);
  std::vector<bool> taken(looked, false);
  // Every pass takes at least one more mod or ends: at most one pass per mod looked at, of at most kMaxListEntries each.
  for (bool grew = true; grew;) {
    grew = false;
    for (size_t i = 0; i < looked; ++i) {
      if (taken[i] || off.count(installed[i].id)) continue;
      for (const Dependency& d : installed[i].dependencies)
        if (d.required && off.count(d.id)) {
          taken[i] = grew = true;
          off.insert(installed[i].id);
          break;
        }
    }
  }
  std::vector<std::string> out;
  for (size_t i = 0; i < looked; ++i)
    if (taken[i]) out.push_back(installed[i].id);
  return out;
}

std::vector<std::string> switched(std::vector<std::string> ids, const std::vector<Manifest>& installed, const std::string& id,
                                  bool on) {
  if (!on) {
    std::vector<std::string> off = dependents({id}, installed);
    off.push_back(id);
    std::erase_if(ids, [&](const std::string& e) { return modlist::enabled(off, e); });
    return ids;
  }
  const auto at = index_of(installed);
  std::set<std::string, std::less<>> seen{id};
  std::deque<std::string> queue{id};
  while (!queue.empty()) {
    const std::string next = queue.front();
    queue.pop_front();
    if (!modlist::enabled(ids, next)) ids.push_back(next);
    const auto it = at.find(next);
    if (it == at.end()) continue;
    for (const Dependency& d : installed[it->second].dependencies)
      if (d.required && at.count(d.id) && seen.insert(d.id).second) queue.push_back(d.id);
  }
  return ids;
}

std::string switch_note(const std::vector<Manifest>& installed, const std::vector<std::string>& before,
                        const std::vector<std::string>& after, const std::string& id, bool on) {
  std::string name = id, others;
  int count = 0;
  for (const Manifest& m : installed) {
    if (m.id == id) name = m.name;
    else if (modlist::enabled(before, m.id) != modlist::enabled(after, m.id)) {
      others += (count ? i18n::text("list.separator") : std::string()) + m.name;
      ++count;
    }
  }
  if (!count) return {};
  if (on) return i18n::text("deps.cascade-on", {{"mod", name}, {"others", others}});
  return count == 1 ? i18n::text("deps.cascade-off-one", {{"mod", name}, {"others", others}})
                    : i18n::text("deps.cascade-off-many", {{"mod", name}, {"others", others}});
}

std::vector<std::string> with_requirements(std::vector<std::string> ids, const std::vector<Manifest>& installed) {
  const std::vector<std::string> chosenIds = ids;
  for (const std::string& id : chosenIds) ids = switched(std::move(ids), installed, id, true);
  return ids;
}

}  // namespace pm::moddeps
