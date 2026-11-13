// Offline test of the string tables (core/text_table.h) and of every table the project ships. Run: build\text_table_test.exe
#include "text_table.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"

namespace {
namespace fs = std::filesystem;
using pm::i18n::Table;

Table parsed(const char* json) {
  Table t;
  std::string why;
  CHECK(t.parse(json, why));
  return t;
}

// A flat object of text, with comments, as SMAPI's i18n files are.
void a_table_reads_text_and_comments() {
  const Table t = parsed(R"({
    // The lobby's popups
    "lobby.waiting": "Waiting for {{players}}.", // PM_TEXT_AI
    /* a block comment */ "lobby.left": "You left."
  })");
  CHECK(t.size() == 2 && t.has("lobby.left"));
  CHECK(t.get("lobby.left") == "You left.");
}

// Each {{token}} takes its value; one with none stays as written, so the gap is seen.
void tokens_take_their_values() {
  const Table t = parsed(R"({"k": "{{a}} and {{b}}, then {{a}} {{c}}", "brace": "a {{ b", "none": "plain"})");
  CHECK(t.get("k", {{"a", "Alice"}, {"b", "Bob"}}) == "Alice and Bob, then Alice {{c}}");
  CHECK(t.get("brace", {{"b", "x"}}) == "a {{ b");
  CHECK(t.get("none", {{"a", "x"}}) == "plain");
  CHECK((t.tokens_of("k") == std::vector<std::string>{"a", "b", "a", "c"}));
  CHECK(t.tokens_of("missing").empty());
}

// A missing key shows as [key], and its owner hears of it once.
void a_missing_key_is_seen_and_told_once() {
  Table t = parsed(R"({"a": "A"})");
  std::vector<std::string> told;
  t.on_missing([&](const std::string& key) { told.push_back(key); });
  CHECK(t.get("lobby.gone") == "[lobby.gone]");
  CHECK(t.get("lobby.gone") == "[lobby.gone]");
  CHECK((told == std::vector<std::string>{"lobby.gone"}));
}

// What is not a flat object of text is refused, and leaves the table empty.
void a_bad_table_is_refused() {
  Table t = parsed(R"({"a": "A"})");
  std::string why;
  CHECK(!t.parse("not json", why) && !why.empty() && t.size() == 0);
  CHECK(!t.parse(R"(["a"])", why));
  CHECK(!t.parse(R"({"a": 1})", why) && why.find("a") != std::string::npos);
  CHECK(!t.parse(R"({"a": {"b": "c"}})", why));
  CHECK(!t.load(fs::path("no such file.json"), why));
}

// A running copy finds the launcher's copy of its table beside it first, then its own i18n folder.
void a_component_finds_its_table(const fs::path& scratch) {
  const fs::path folder = scratch / "text_table_test";
  fs::remove_all(folder);
  fs::create_directories(folder / "i18n");
  const fs::path dll = folder / "host-1-2-0.dll";
  CHECK(pm::i18n::table_beside(dll) == folder / "i18n" / "default.json");
  std::ofstream(folder / "host-1-2-0.i18n.json") << "{}";
  CHECK(pm::i18n::table_beside(dll) == folder / "host-1-2-0.i18n.json");
  fs::remove_all(folder);
}

std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

// Every table the project ships reads, and every key its code asks for (text("key") or a text_key comment) is in it, with
// the same tokens the code gives; a key no code asks for fails too.
void every_shipped_table_matches_its_code(const fs::path& root) {
  struct Component {
    fs::path table;
    std::vector<fs::path> code;
  };
  std::vector<Component> components{{root / "i18n" / "default.json", {root / "core", root / "host", root / "tools"}}};
  for (const fs::directory_entry& mod : fs::directory_iterator(root / "mods"))
    if (fs::exists(mod.path() / "i18n" / "default.json")) components.push_back({mod.path() / "i18n" / "default.json", {mod.path()}});
  for (const fs::directory_entry& example : fs::directory_iterator(root / "sdk" / "examples"))
    if (fs::exists(example.path() / "i18n" / "default.json"))
      components.push_back({example.path() / "i18n" / "default.json", {example.path()}});
  // A lookup: text("key") or text("key", {{"token", ...}}), in whichever namespace it is called.
  static const std::regex lookup(R"re(\btext\(\s*"([a-z0-9_.-]+)"\s*(?:,\s*\{(.*?)\}\s*)?\))re");
  static const std::regex token(R"re(\{\s*"([a-z0-9_]+)"\s*,)re");
  static const std::regex key_only(R"re(\b(?:text|key)\(\s*"([a-z0-9_.-]+)")re");
  for (const Component& c : components) {
    if (!fs::exists(c.table)) continue;
    Table t;
    std::string why;
    if (!t.load(c.table, why)) {
      std::printf("FAIL %s: %s\n", c.table.string().c_str(), why.c_str());
      ++g_failures;
      continue;
    }
    std::set<std::string> used;
    for (const fs::path& folder : c.code)
      for (const fs::directory_entry& entry : fs::recursive_directory_iterator(folder)) {
        const auto ext = entry.path().extension();
        if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
        std::string source = read_file(entry.path());
        for (char& c : source) if (c == '\n' || c == '\r') c = ' ';  // a lookup may span lines
        // Every key asked for, a lookup nested in another's tokens included, and every key named with i18n::key.
        for (std::sregex_iterator it(source.begin(), source.end(), key_only), end; it != end; ++it) {
          const std::string key = (*it)[1];
          used.insert(key);
          if (!t.has(key)) {
            std::printf("FAIL %s asks for %s, which %s lacks\n", entry.path().string().c_str(), key.c_str(),
                        c.table.string().c_str());
            ++g_failures;
          }
        }
        for (std::sregex_iterator it(source.begin(), source.end(), lookup), end; it != end; ++it) {
          const std::string key = (*it)[1];
          if (!t.has(key)) continue;
          std::set<std::string> given;
          const std::string args = (*it)[2];
          for (std::sregex_iterator tk(args.begin(), args.end(), token), tend; tk != tend; ++tk) given.insert((*tk)[1]);
          for (const std::string& needed : t.tokens_of(key))
            if (!given.count(needed)) {
              std::printf("FAIL %s: %s needs {{%s}}, which the code does not give\n", entry.path().string().c_str(),
                          key.c_str(), needed.c_str());
              ++g_failures;
            }
        }
      }
    // An emote catalogue (mods/emotes/catalogue.h) names its emotes' labels: each counts as asked for, and must be there.
    static const std::regex label(R"re("label"\s*:\s*"([a-z0-9_.-]+)")re");
    for (const fs::path& folder : c.code) {
      const fs::path catalogue = folder / "assets" / "emotes.json";
      if (!fs::exists(catalogue)) continue;
      const std::string source = read_file(catalogue);
      for (std::sregex_iterator it(source.begin(), source.end(), label), end; it != end; ++it) {
        const std::string key = (*it)[1];
        used.insert(key);
        if (!t.has(key)) {
          std::printf("FAIL %s names the label %s, which %s lacks\n", catalogue.string().c_str(), key.c_str(),
                      c.table.string().c_str());
          ++g_failures;
        }
      }
    }
    for (const std::string& key : t.keys())
      if (!used.count(key)) {
        std::printf("FAIL %s: %s is asked for by no code\n", c.table.string().c_str(), key.c_str());
        ++g_failures;
      }
  }
}

}  // namespace

// Reading a table can fail on anybody's machine, so the failure paths must not throw: a path a code page can't hold makes
// path::string() throw, on a non-ASCII Windows user name, say. And the launcher loads its table before anything can need it.
void reading_a_table_cannot_throw_and_comes_first() {
  const auto read = [](const char* rel) {
    std::ifstream in(std::string(PM_TEXT_DIR) + "/" + rel, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
  };
  CHECK(read("host/suite.cpp").find("file.string()") == std::string::npos);
  CHECK(read("core/mod_glue.cpp").find("file.string()") == std::string::npos);
  const std::string launcher = read("tools/launcher.cpp");
  const size_t main = launcher.find("wWinMain");
  const size_t loaded = launcher.find("pm::i18n::table().load", main), host = launcher.find("adjacent_host(", main);
  CHECK(main != std::string::npos && loaded != std::string::npos && host != std::string::npos && loaded < host);
}

int main() {
  reading_a_table_cannot_throw_and_comes_first();
  a_table_reads_text_and_comments();
  tokens_take_their_values();
  a_missing_key_is_seen_and_told_once();
  a_bad_table_is_refused();
  a_component_finds_its_table(fs::temp_directory_path());
  every_shipped_table_matches_its_code(PM_SOURCE_DIR);
  return g_failures ? 1 : 0;
}
