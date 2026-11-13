#pragma once

#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "../third_party/nlohmann/json.hpp"

// Player-facing text, kept out of the code in string tables, as SMAPI mods keep theirs (the user's choice, 2026-11-03). Each
// component has one: the loader (the host and PhasmoMods Loader) its i18n/default.json, and each mod its own
// mods/<mod>/i18n/default.json. A table is a flat JSON object of key to text; // and /* */ comments are allowed, so an entry the
// author has yet to rewrite carries a "// PM_TEXT_AI" comment (AI_POLICY.md). A text names a value it is given as {{token}},
// as SMAPI's do. A translation would be another file beside it (i18n/<language>.json); none exists yet.
//
// A key the table lacks shows as "[key]", so a missing text is seen, never silently blank, and the table's owner is told once
// per key. Code reads its text with pm::i18n::text, giving the key and each token's value. Looking up is safe from several
// threads at once once the table has been loaded (the launcher looks up on its worker thread and its window thread both);
// parsing, loading, merging and on_missing belong to one thread before any lookup.
namespace pm::i18n {

class Table {
 public:
  /// Replaces the table with the JSON object `json`. False, with the table left empty and `why` set, when it is not a flat
  /// object of strings.
  bool parse(std::string_view json, std::string& why) {
    entries_.clear();
    nlohmann::json root;
    try {
      root = nlohmann::json::parse(json.begin(), json.end(), nullptr, true, true);
    } catch (const std::exception& e) {
      why = std::string("it is not JSON: ") + e.what();
      return false;
    }
    if (!root.is_object()) {
      why = "it is not a JSON object of key to text";
      return false;
    }
    std::map<std::string, std::string, std::less<>> entries;
    for (const auto& [key, value] : root.items()) {
      if (!value.is_string()) {
        why = "the entry " + key + " is not text";
        return false;
      }
      entries.emplace(key, value.get<std::string>());
    }
    entries_ = std::move(entries);
    return true;
  }

  /// Replaces the table with the file `file`. False, with `why` set, when it cannot be read or parsed.
  bool load(const std::filesystem::path& file, std::string& why) {
    std::ifstream in(file, std::ios::binary);
    if (!in) {
      entries_.clear();
      why = "it cannot be read";
      return false;
    }
    std::ostringstream text;
    text << in.rdbuf();
    return parse(text.str(), why);
  }

  /// Adds every entry of `other`. False, with `why` naming the key and nothing added, when the two share a key: a test that
  /// reads the loader's text and a mod's together merges their tables, and their keys must not clash.
  bool merge(const Table& other, std::string& why) {
    for (const auto& [key, text] : other.entries_)
      if (entries_.count(key)) {
        why = "both tables have " + key;
        return false;
      }
    entries_.insert(other.entries_.begin(), other.entries_.end());
    return true;
  }

  /// Who hears of a key the table lacks, once per key (for the log).
  void on_missing(std::function<void(const std::string& key)> missing) { missing_ = std::move(missing); }

  using Tokens = std::initializer_list<std::pair<std::string_view, std::string_view>>;

  /// The text of `key`, each {{token}} replaced by its value in `tokens`; a token with no value stays as written. "[key]"
  /// when the table lacks the key.
  std::string get(std::string_view key, Tokens tokens = {}) const {
    const auto it = entries_.find(key);
    if (it == entries_.end()) {
      // Two threads can miss the same key at once (the launcher looks up text on its worker thread and its window thread),
      // so the keys told and the tell they guard share a lock. A key the table has touches neither and stays lock-free.
      // The callback must not look up text in this table: the lock is held across the call.
      std::lock_guard<std::mutex> lock(told_mutex_);
      if (told_.insert(std::string(key)).second && missing_) missing_(std::string(key));
      return "[" + std::string(key) + "]";
    }
    std::string out;
    const std::string& text = it->second;
    for (size_t i = 0; i < text.size();) {
      const size_t open = text.find("{{", i);
      if (open == std::string::npos) {
        out.append(text, i, std::string::npos);
        break;
      }
      const size_t close = text.find("}}", open + 2);
      if (close == std::string::npos) {
        out.append(text, i, std::string::npos);
        break;
      }
      out.append(text, i, open - i);
      const std::string_view name(text.data() + open + 2, close - open - 2);
      bool given = false;
      for (const auto& [token, value] : tokens)
        if (token == name) {
          out += value;
          given = true;
          break;
        }
      if (!given) out.append(text, open, close + 2 - open);
      i = close + 2;
    }
    return out;
  }

  bool has(std::string_view key) const { return entries_.find(key) != entries_.end(); }
  size_t size() const { return entries_.size(); }
  std::vector<std::string> keys() const {
    std::vector<std::string> out;
    for (const auto& [key, text] : entries_) out.push_back(key);
    return out;
  }

  /// The tokens the text of `key` names, in order; none for a missing key.
  std::vector<std::string> tokens_of(std::string_view key) const {
    std::vector<std::string> out;
    const auto it = entries_.find(key);
    if (it == entries_.end()) return out;
    const std::string& text = it->second;
    for (size_t open = text.find("{{"); open != std::string::npos; open = text.find("{{", open + 2)) {
      const size_t close = text.find("}}", open + 2);
      if (close == std::string::npos) break;
      out.emplace_back(text.substr(open + 2, close - open - 2));
    }
    return out;
  }

 private:
  std::map<std::string, std::string, std::less<>> entries_;
  std::function<void(const std::string&)> missing_;
  mutable std::mutex told_mutex_;  // guards missing_ and told_: get() takes it only when the key is missing
  mutable std::set<std::string, std::less<>> told_;
};

/// Where a component's table lies, beside its running file `running` (a DLL or the launcher): first the launcher's copy of it,
/// named as the running copy with ".i18n.json" in place of its extension, then the component's own i18n/default.json.
inline std::filesystem::path table_beside(const std::filesystem::path& running) {
  std::filesystem::path copy = running;
  copy.replace_extension(L".i18n.json");
  std::error_code error;
  if (std::filesystem::exists(copy, error)) return copy;
  return running.parent_path() / L"i18n" / L"default.json";
}

/// This module's own table: each DLL (the host, each mod) and the launcher has its own, since every module holds its own
/// copy of this function's static. The module loads it once, as it starts (host/suite.cpp, core/mod_glue.cpp,
/// tools/launcher.cpp). Main thread.
inline Table& table() {
  static Table t;
  return t;
}

/// Names a key of this module's table where the code keeps the key to read later (a table of keys, say), so the checks
/// (tests/text_table_test.cpp) count it as used. It returns `k` unchanged.
constexpr const char* key(const char* k) { return k; }

/// The text of `key` in this module's table, each {{token}} given its value: what every player-facing string is read through.
inline std::string text(std::string_view key, Table::Tokens tokens = {}) { return table().get(key, tokens); }

}  // namespace pm::i18n
