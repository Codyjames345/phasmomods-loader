// The map's own rules, read from the headers in sdk/map: every descriptor is listed in kAllEntries,
// keys are unique, each descriptor has its behaviour note on the line above, and nothing names the
// anti-cheat or the moderation bindings.
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

#include "all.h"
#include "check.h"

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
  static const std::regex decl(R"(^inline constexpr pg::sdk::EntryDesc (\w+)\{)");
  static const std::regex key(R"(\.key = "([^"]*)\")");
  std::vector<Declared> out;
  const fs::path dir = fs::path(PG_SOURCE_DIR) / "sdk" / "map";
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
  CHECK(all.size() == std::size(pg::map::kAllEntries));
  for (const Declared& d : all) {
    const bool listed = std::any_of(std::begin(pg::map::kAllEntries), std::end(pg::map::kAllEntries),
                                    [&](const pg::sdk::EntryDesc* e) { return e->key == d.key; });
    if (!listed) std::printf("not in kAllEntries: %s (%s, key \"%s\")\n", d.name.c_str(), d.file.c_str(), d.key.c_str());
    CHECK(listed);
  }
}

void keys_unique() {
  std::set<std::string> seen;
  for (const pg::sdk::EntryDesc* e : pg::map::kAllEntries) {
    CHECK(e->key != nullptr);
    if (!e->key) continue;
    const bool fresh = seen.insert(e->key).second;
    if (!fresh) std::printf("duplicate key: %s\n", e->key);
    CHECK(fresh);
  }
}

void every_entry_has_a_note() {
  for (const Declared& d : declared()) {
    const bool noted = d.above.starts_with("///");
    if (!noted) std::printf("no /// note above %s (%s)\n", d.name.c_str(), d.file.c_str());
    CHECK(noted);
  }
}

void no_anticheat_entries() {
  for (const pg::sdk::EntryDesc* e : pg::map::kAllEntries)
    for (const char* text : {e->key, e->name, e->hint, e->owner})
      for (const std::string_view word : {"SelfReport", "Moderation"}) CHECK(!contains_ci(text, word));
  for (const Declared& d : declared())
    for (const std::string_view word : {"SelfReport", "Moderation"}) CHECK(!contains_ci(d.name.c_str(), word));
}

}  // namespace

int main() {
  every_entry_listed();
  keys_unique();
  every_entry_has_a_note();
  no_anticheat_entries();
  return g_failures ? 1 : 0;
}
