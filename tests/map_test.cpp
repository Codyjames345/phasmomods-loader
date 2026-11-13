// The map's own rules, read from the headers in sdk/map: every descriptor is listed in kAllEntries,
// keys are unique, each descriptor has its behaviour note on the line above, and nothing names the
// anti-cheat or the moderation bindings.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "all.h"
#include "check.h"
#include "map_digest.h"

namespace {
namespace fs = std::filesystem;

// One descriptor as the source declares it.
struct Declared {
  std::string file, name, key, above;  // above: the line before the declaration, trimmed
};

std::string trim(const std::string& s) {
  const size_t begin = s.find_first_not_of(" \t\r");
  if (begin == std::string::npos) return {};
  return s.substr(begin, s.find_last_not_of(" \t\r") - begin + 1);
}

std::vector<Declared> declared() {
  static const std::regex decl(R"(^inline constexpr pm::sdk::EntryDesc (\w+)\{)");
  static const std::regex key(R"(\.key = "([^"]*)\")");
  std::vector<Declared> out;
  const fs::path dir = fs::path(PM_SOURCE_DIR) / "sdk" / "map";
  for (const fs::directory_entry& e : fs::directory_iterator(dir)) {
    if (e.path().extension() != ".h") continue;
    std::ifstream in(e.path());
    std::vector<std::string> lines;
    for (std::string line; std::getline(in, line);) lines.push_back(line);
    for (size_t i = 0; i < lines.size(); ++i) {
      std::smatch m;
      if (!std::regex_search(lines[i], m, decl)) continue;
      Declared d{e.path().filename().string(), m[1], "", i ? trim(lines[i - 1]) : ""};
      // The key is on the declaration's first line or the next.
      for (size_t j = i; j < std::min(i + 2, lines.size()) && d.key.empty(); ++j)
        if (std::smatch k; std::regex_search(lines[j], k, key)) d.key = k[1];
      out.push_back(std::move(d));
    }
  }
  return out;
}

std::string lower(std::string_view text) {
  std::string out(text);
  std::transform(out.begin(), out.end(), out.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return out;
}

bool contains_ci(const char* text, std::string_view word) {
  return text && lower(text).find(lower(word)) != std::string::npos;
}

void every_entry_listed() {
  const std::vector<Declared> all = declared();
  CHECK(!all.empty());
  // Together with keys_unique, the counts and the keys make kAllEntries exactly the declared set.
  CHECK(all.size() == std::size(pm::map::kAllEntries));
  for (const Declared& d : all) {
    const bool listed = std::any_of(std::begin(pm::map::kAllEntries), std::end(pm::map::kAllEntries),
                                    [&](const pm::sdk::EntryDesc* e) { return e->key == d.key; });
    if (!listed) std::printf("not in kAllEntries: %s (%s, key \"%s\")\n", d.name.c_str(), d.file.c_str(), d.key.c_str());
    CHECK(listed);
  }
}

void keys_unique() {
  std::set<std::string> seen;
  for (const pm::sdk::EntryDesc* e : pm::map::kAllEntries) {
    CHECK(e->key != nullptr);
    if (!e->key) continue;
    const bool fresh = seen.insert(e->key).second;
    if (!fresh) std::printf("duplicate key: %s\n", e->key);
    CHECK(fresh);
  }
}

// A type written from the dump can resolve offline and fail in the running game: the dump writes System.Object[] and
// UnityEngine.Object[] alike, as Object[], and the game tells them apart. PhotonView.RPC did this, and every mod stayed off on the
// first live run. So a parameter says which it means: object[] for System.Object[]. No entry needs UnityEngine.Object[] yet.
void no_entry_spells_an_object_array_the_dumps_way() {
  for (const pm::sdk::EntryDesc* e : pm::map::kAllEntries) {
    bool spelled = e->type && std::string_view(e->type) == "Object[]";
    for (const char* p : e->params) spelled |= std::string_view(p) == "Object[]";
    for (const char* f : e->fieldTypes) spelled |= std::string_view(f) == "Object[]";
    if (spelled) std::printf("write object[] for System.Object[]: %s\n", e->key);
    CHECK(!spelled);
  }
}

// A base is what typed handles convert to (sdk/api/ref.h): only a Type entry has one, and it is another Type entry of the
// map itself, which the offline check proves an ancestor (sdk/offline/gamecheck.cpp).
void every_base_is_a_mapped_type() {
  for (const pm::sdk::EntryDesc* e : pm::map::kAllEntries) {
    if (!e->base) continue;
    const bool listed = std::find(std::begin(pm::map::kAllEntries), std::end(pm::map::kAllEntries), e->base) !=
                        std::end(pm::map::kAllEntries);
    const bool types = e->kind == pm::sdk::Kind::Type && e->base->kind == pm::sdk::Kind::Type && e->base != e;
    if (!listed || !types) std::printf("a base that is not another mapped type: %s\n", e->key);
    CHECK(listed && types);
  }
}

void every_entry_has_a_note() {
  for (const Declared& d : declared()) {
    const bool noted = d.above.starts_with("///");
    if (!noted) std::printf("no /// note above %s (%s)\n", d.name.c_str(), d.file.c_str());
    CHECK(noted);
  }
}

// Every string an entry carries.
std::vector<const char*> strings_of(const pm::sdk::EntryDesc& e) {
  std::vector<const char*> out{e.key,  e.name,   e.hint,  e.owner, e.assembly, e.ns,
                               e.type, e.within, e.scene, e.path,  e.component};
  for (const std::span<const char* const> list : {e.params, e.fieldTypes, e.values})
    out.insert(out.end(), list.begin(), list.end());
  for (const pm::sdk::Fact& f : e.facts) out.push_back(f.method);
  return out;
}

void no_anticheat_entries() {
  for (const pm::sdk::EntryDesc* e : pm::map::kAllEntries)
    for (const char* text : strings_of(*e))
      for (const std::string_view word : {"SelfReport", "Moderation"}) {
        const bool clean = !contains_ci(text, word);
        if (!clean) std::printf("%s names %s: %s\n", e->key, std::string(word).c_str(), text);
        CHECK(clean);
      }
  for (const Declared& d : declared())
    for (const std::string_view word : {"SelfReport", "Moderation"}) CHECK(!contains_ci(d.name.c_str(), word));
}

}  // namespace

// A checked-build record is stale once an entry changes, so every field the resolver reads changes the digest. The same
// map gives the same digest, and how sure an entry's note may sound is not part of it.
void map_digest_follows_what_the_resolver_reads() {
  using pm::sdk::EntryDesc;
  using pm::sdk::Fact;
  static constexpr const char* kOne[] = {"a"};
  static constexpr const char* kTwo[] = {"b"};
  static constexpr Fact kFact[] = {{Fact::Is::Calls, "x"}};
  static constexpr Fact kOtherFact[] = {{Fact::Is::Calls, "x", 1}};
  const EntryDesc base{.key = "K", .owner = "O", .params = kOne, .facts = kFact};
  const auto digest = [](const EntryDesc& d) {
    const EntryDesc* one[] = {&d};
    return pm::sdk::map_digest(one);
  };
  const std::string same = digest(base);
  CHECK(same.size() == 16);
  CHECK(same == digest({.key = "K", .confidence = pm::sdk::Confidence::Live, .owner = "O", .params = kOne, .facts = kFact}));
  const auto changed = [&](const EntryDesc& d) { return digest(d) != same; };
  CHECK(changed({.key = "L", .owner = "O", .params = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .kind = pm::sdk::Kind::Method, .owner = "O", .params = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .flags = pm::sdk::kOptional, .owner = "O", .params = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "P", .params = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "O", .name = "O", .params = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "O", .params = kTwo, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .fieldTypes = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .values = kOne, .facts = kFact}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kOtherFact}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .pattern = "90"}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .patternOffset = 1}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .scene = "S"}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .access = pm::sdk::Visibility::Public}));
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .typeArgs = kOne}));
  static constexpr EntryDesc kBase{.key = "B"};
  CHECK(changed({.key = "K", .owner = "O", .params = kOne, .facts = kFact, .base = &kBase}));
  // Moving a character between two fields is a change too.
  CHECK(digest({.key = "K", .owner = "ab", .name = "c"}) != digest({.key = "K", .owner = "a", .name = "bc"}));
  CHECK(pm::sdk::map_digest(pm::map::kAllEntries) == pm::sdk::map_digest(pm::map::kAllEntries));
}

int main() {
  map_digest_follows_what_the_resolver_reads();
  every_entry_listed();
  keys_unique();
  every_base_is_a_mapped_type();
  every_entry_has_a_note();
  no_entry_spells_an_object_array_the_dumps_way();
  no_anticheat_entries();
  return g_failures ? 1 : 0;
}
