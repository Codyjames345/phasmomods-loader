// Offline tests for the shared log (core/log.h): every line of mods.log shows its level, and a mod's lines reach the
// host with theirs (the host contract's log_level), or, on a host without it, with the level's word in the text.
// The file goes to a temporary LOCALAPPDATA, never the player's. Run: build\log_test.exe
#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "log.h"
#include "text_files.h"

namespace {

namespace fs = std::filesystem;
using pm::log::Level;

std::vector<std::string> g_plain;
std::vector<std::pair<uint32_t, std::string>> g_levelled;
void plain(const char* line) { g_plain.emplace_back(line); }
void levelled(uint32_t level, const char* line) { g_levelled.emplace_back(level, line); }

// Contract numbers map onto the levels in SMAPI's order, and a number the contract does not know is info.
void the_levels_follow_the_contracts_numbers() {
  CHECK(pm::log::level_of(0) == Level::Trace && pm::log::level_of(1) == Level::Debug);
  CHECK(pm::log::level_of(2) == Level::Info && pm::log::level_of(3) == Level::Warn);
  CHECK(pm::log::level_of(4) == Level::Error && pm::log::level_of(5) == Level::Alert);
  CHECK(pm::log::level_of(6) == Level::Info && pm::log::level_of(0xFFFFFFFFu) == Level::Info);
  CHECK(std::string(pm::log::level_word(Level::Trace)) == "TRACE");
  CHECK(std::string(pm::log::level_word(Level::Info)) == "INFO ");
  CHECK(std::string(pm::log::level_word(Level::Warn)) == "WARN ");
  CHECK(std::string(pm::log::level_word(Level::Alert)) == "ALERT");
}

// mods.log: each line names its level after the process and thread, before the text.
void every_line_in_the_file_shows_its_level() {
  const fs::path root = fs::temp_directory_path() / ("pm_log_test_" + std::to_string(GetCurrentProcessId()));
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  CHECK(SetEnvironmentVariableW(L"LOCALAPPDATA", root.c_str()));
  pm::log::set_sink(nullptr, nullptr);
  pm::log::set_level_sink(nullptr);
  pm::log::open();
  PM_LOG("ModHost loading");
  PM_LOG_AT(Level::Warn, "a %s went missing", "file");
  PM_LOG_AT(Level::Trace, "step %d", 3);
  pm::log::close();
  const std::string text = pm::text::read_file(root / "PhasmoMods" / "mods.log");
  const std::string thread = "[pid=" + std::to_string(GetCurrentProcessId()) + " tid=" + std::to_string(GetCurrentThreadId()) + "] ";
  // The file is written in text mode, so each line ends in CR LF.
  CHECK(text.find(thread + "INFO  ModHost loading\r\n") != std::string::npos);
  CHECK(text.find(thread + "WARN  a file went missing\r\n") != std::string::npos);
  CHECK(text.find(thread + "TRACE step 3\r\n") != std::string::npos);
  fs::remove_all(root, ec);
}

// A mod's side: with the host's log_level, each line goes there with its level and the mod's prefix; info lines too.
void a_mods_lines_reach_the_host_with_their_level() {
  g_plain.clear();
  g_levelled.clear();
  pm::log::set_sink(&plain, "[someone.mod] ");
  pm::log::set_level_sink(&levelled);
  PM_LOG_AT(Level::Error, "cannot read %s", "x.txt");
  PM_LOG("plain");
  CHECK(g_levelled.size() == 2 && g_plain.empty());
  if (g_levelled.size() == 2) {
    CHECK(g_levelled[0].first == 4 && g_levelled[0].second == "[someone.mod] cannot read x.txt");
    CHECK(g_levelled[1].first == 2 && g_levelled[1].second == "[someone.mod] plain");
  }
  pm::log::set_level_sink(nullptr);
  pm::log::set_sink(nullptr, nullptr);
}

// On a host without log_level, a line goes to its log with the level's word, except at info, which is that log's own.
void without_log_level_the_word_goes_into_the_text() {
  g_plain.clear();
  g_levelled.clear();
  pm::log::set_sink(&plain, "[someone.mod] ");
  pm::log::set_level_sink(nullptr);
  PM_LOG_AT(Level::Warn, "slow");
  PM_LOG("plain");
  CHECK(g_plain.size() == 2 && g_levelled.empty());
  if (g_plain.size() == 2) {
    CHECK(g_plain[0] == "[someone.mod] WARN  slow");
    CHECK(g_plain[1] == "[someone.mod] plain");
  }
  pm::log::set_sink(nullptr, nullptr);
}

}  // namespace

int main() {
  the_levels_follow_the_contracts_numbers();
  every_line_in_the_file_shows_its_level();
  a_mods_lines_reach_the_host_with_their_level();
  without_log_level_the_word_goes_into_the_text();
  return g_failures ? 1 : 0;
}
