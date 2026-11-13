// Offline test of a mod's side of the host contract (core/mod_glue.h) over the fake host. Run: build\mod_glue_test.exe
#include "mod_glue.h"

#include <windows.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "data_folder.h"
#include "fake_game.h"
#include "log.h"
#include "text_files.h"

namespace {
namespace fake = pm::fake;
namespace fs = std::filesystem;

const PmModInfo kInfo{PMMOD_API_VERSION, "someone.glue", "Glue", 210, 0};

// What a host's log and log_level were given, in order.
std::vector<std::string> g_lines;
std::vector<std::pair<uint32_t, std::string>> g_levelled;
void host_log(const char* line) { g_lines.emplace_back(line); }
void host_log_level(uint32_t level, const char* line) { g_levelled.emplace_back(level, line); }
bool g_oldMemberCalled = false;
void must_not_be_called(uint32_t, const char*) { g_oldMemberCalled = true; }

bool logged(const std::string& part) {
  for (const std::string& l : g_lines)
    if (l.find(part) != std::string::npos) return true;
  return false;
}

// A mod's message goes to the host's one popup, as it was given; before bind it goes nowhere.
void a_message_reaches_the_hosts_popup() {
  fake::reset();
  pm::mod::show_message("Before", "bind");
  CHECK(fake::messages().empty());
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  pm::mod::show_message("Your deception score: 2 of 3", "$120 and 300 XP: 1 killed, 1 misled");
  CHECK(fake::messages().size() == 1);
  if (fake::messages().empty()) return;
  CHECK(fake::messages()[0].first == "Your deception score: 2 of 3");
  CHECK(fake::messages()[0].second == "$120 and 300 XP: 1 killed, 1 misled");
}

// On a host of contract 2.7, a mod's line at a level reaches log_level with that level and the mod's name; PM_LOG's
// lines stay at info.
void a_level_reaches_the_hosts_log_level() {
  fake::reset();
  PmHostApi host = *fake::host();
  host.log = &host_log;
  host.log_level = &host_log_level;
  CHECK(pm::mod::bind(&host, &kInfo));
  g_lines.clear();
  g_levelled.clear();
  PM_LOG_AT(pm::log::Level::Warn, "the file %s is gone", "a.txt");
  PM_LOG("ready");
  CHECK(g_lines.empty());
  CHECK(g_levelled.size() == 2);
  if (g_levelled.size() == 2) {
    CHECK(g_levelled[0].first == PM_LOG_WARN && g_levelled[0].second == "[someone.glue] the file a.txt is gone");
    CHECK(g_levelled[1].first == PM_LOG_INFO && g_levelled[1].second == "[someone.glue] ready");
  }
  pm::log::set_level_sink(nullptr);
  pm::log::set_sink(nullptr, nullptr);
}

// A host without log_level (a table that leaves it null) gets the line in its log with the level's word. A host of an
// older minor than the mod's never gets that far: bind refuses it (pm_api_compatible), and its table past
// content_pack_failed is never read.
void a_host_without_log_level_gets_the_level_in_the_text() {
  fake::reset();
  PmHostApi host = *fake::host();
  host.log = &host_log;
  host.log_level = nullptr;
  CHECK(pm::mod::bind(&host, &kInfo));
  g_lines.clear();
  PM_LOG_AT(pm::log::Level::Error, "cannot load %d", 3);
  PM_LOG("ready");
  CHECK(g_lines.size() == 2);
  if (g_lines.size() == 2) {
    CHECK(g_lines[0] == "[someone.glue] ERROR cannot load 3");
    CHECK(g_lines[1] == "[someone.glue] ready");
  }
  host.version = (PMMOD_API_MAJOR << 16) | 6u;
  host.log_level = &must_not_be_called;  // stands for whatever lies past a 2.6 host's table
  g_oldMemberCalled = false;
  CHECK(!pm::mod::bind(&host, &kInfo));
  PM_LOG_AT(pm::log::Level::Error, "after a refused bind");
  CHECK(!g_oldMemberCalled);
  pm::log::set_level_sink(nullptr);
  pm::log::set_sink(nullptr, nullptr);
}

// A fresh folder for one test, under the system's temporary folder: never the player's %LOCALAPPDATA%.
fs::path fresh_root(const char* name) {
  const fs::path root = fs::temp_directory_path() / ("pm_glue_test_" + std::to_string(GetCurrentProcessId())) / name;
  std::error_code ec;
  fs::remove_all(root, ec);
  fs::create_directories(root, ec);
  return root;
}

void write(const fs::path& file, const std::string& text) {
  std::error_code ec;
  fs::create_directories(file.parent_path(), ec);
  FILE* f = nullptr;
  if (_wfopen_s(&f, file.c_str(), L"wb") == 0 && f) {
    fputs(text.c_str(), f);
    fclose(f);
  }
}

// Binds the mod to a fake host whose data folder is `root`, its log kept in g_lines.
bool bind_in(const fs::path& root, PmHostApi& host, const PmModInfo& info = kInfo) {
  fake::reset();
  fake::data_directory(root.wstring());
  host = *fake::host();
  host.log = &host_log;
  host.log_level = nullptr;  // every line reaches host_log, a level other than info named in its text
  g_lines.clear();
  return pm::mod::bind(&host, &info);
}

// The mod's own folder is <data>\data\<mod id>, made on the first ask.
void the_data_folder_is_made_on_first_ask() {
  const fs::path root = fresh_root("made");
  PmHostApi host{};
  CHECK(bind_in(root, host));
  const fs::path folder = root / "data" / "someone.glue";
  CHECK(!fs::exists(folder));
  CHECK(pm::mod::data_folder() == folder);
  CHECK(fs::is_directory(folder));
  CHECK(pm::mod::data_folder() == folder);  // the same answer again
  CHECK(!logged("data folder:"));           // nothing to move
  pm::log::set_sink(nullptr, nullptr);
}

// A player's files from the folder the mod used before, <data>\<mod id>, move into the new one once: each the new
// folder lacks, sub-folders too, and the old folder goes once it is empty.
void the_old_folders_files_move_in_once() {
  const fs::path root = fresh_root("move");
  const fs::path old = root / "someone.glue", folder = root / "data" / "someone.glue";
  write(old / "keybinds.txt", "toggle\t0\t<Keyboard>/f1\n");
  write(old / "sub" / "settings.txt", "volume\t0.5\n");
  PmHostApi host{};
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CHECK(pm::text::read_file(folder / "keybinds.txt") == "toggle\t0\t<Keyboard>/f1\n");
  CHECK(pm::text::read_file(folder / "sub" / "settings.txt") == "volume\t0.5\n");
  CHECK(!fs::exists(old));
  CHECK(logged("[someone.glue] data folder: moved keybinds.txt from the old folder"));
  CHECK(logged("data folder: moved sub/settings.txt from the old folder"));
  // The next start finds nothing left to move.
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CHECK(!logged("data folder:"));
  pm::log::set_sink(nullptr, nullptr);
}

// A file the new folder already has stays in the old one, and the data folder's own copy is kept. The old folder is
// then put aside as <id>.moved, with one line for what it keeps, so the move runs once: no later start logs it again,
// and a copy the player deletes from the data folder does not come back from the old one.
void a_file_the_new_folder_has_stays_in_the_old_one() {
  const fs::path root = fresh_root("both");
  const fs::path old = root / "someone.glue", folder = root / "data" / "someone.glue";
  const fs::path aside = root / "someone.glue.moved";
  write(old / "keybinds.txt", "old");
  write(old / "settings.txt", "moves");
  write(folder / "keybinds.txt", "new");
  PmHostApi host{};
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CHECK(pm::text::read_file(folder / "keybinds.txt") == "new");
  CHECK(pm::text::read_file(folder / "settings.txt") == "moves");
  CHECK(!fs::exists(old));
  CHECK(pm::text::read_file(aside / "keybinds.txt") == "old");
  CHECK(!fs::exists(aside / "settings.txt"));
  CHECK(logged("DEBUG data folder: kept keybinds.txt in the old folder: the data folder has its own"));
  CHECK(logged("[someone.glue] data folder: kept 1 file(s) the data folder already has in someone.glue.moved"));
  // The player resets their keys: the old copy stays aside, and nothing is logged.
  fs::remove(folder / "keybinds.txt");
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CHECK(!fs::exists(folder / "keybinds.txt"));
  CHECK(!logged("data folder:"));
  pm::log::set_sink(nullptr, nullptr);
}

// A file that can be copied but not removed from the old folder (another program holds it open without letting it go)
// ends up in both: the data folder has it, and the old folder keeps its own. Nothing is lost.
void a_file_that_cannot_be_removed_is_copied() {
  const fs::path root = fresh_root("shared");
  const fs::path old = root / "someone.glue", folder = root / "data" / "someone.glue";
  write(old / "keybinds.txt", "shared");
  const HANDLE held =
      CreateFileW((old / "keybinds.txt").c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
  CHECK(held != INVALID_HANDLE_VALUE);
  PmHostApi host{};
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CloseHandle(held);
  CHECK(pm::text::read_file(folder / "keybinds.txt") == "shared");
  CHECK(pm::text::read_file(old / "keybinds.txt") == "shared" ||
        pm::text::read_file(root / "someone.glue.moved" / "keybinds.txt") == "shared");
  CHECK(logged("data folder: copied keybinds.txt from the old folder, which keeps it too"));
  pm::log::set_sink(nullptr, nullptr);
}

// An old-format id ("logs", "bin", "mods", "data") can name one of the loader's own folders under PhasmoMods: such a
// mod gets its data folder, and nothing is moved out of the folder its id names.
void an_old_format_id_moves_nothing() {
  const fs::path root = fresh_root("flat");
  const PmModInfo flat{PMMOD_API_VERSION, "logs", "Flat", 211, 0};
  write(root / "logs" / "launcher.log", "the launcher's");
  PmHostApi host{};
  CHECK(bind_in(root, host, flat));
  CHECK(pm::mod::data_folder() == root / "data" / "logs");
  CHECK(pm::text::read_file(root / "logs" / "launcher.log") == "the launcher's");
  CHECK(!fs::exists(root / "data" / "logs" / "launcher.log"));
  CHECK(!logged("data folder:"));
  pm::log::set_sink(nullptr, nullptr);
}

// Two folders that hold each other never move: the data folder inside the old one (an id "data" would make that), or
// the old one inside the data folder.
void folders_that_hold_each_other_never_move() {
  const fs::path root = fresh_root("nested");
  pm::log::set_sink(&host_log, "");
  pm::log::set_level_sink(nullptr);
  g_lines.clear();
  write(root / "data" / "someone.else" / "keys.txt", "another mod's");
  pm::datafolder::move_old_files(root / "data", root / "data" / "data");
  CHECK(pm::text::read_file(root / "data" / "someone.else" / "keys.txt") == "another mod's");
  CHECK(!fs::exists(root / "data" / "data" / "someone.else"));
  write(root / "outer" / "inner" / "keys.txt", "inner");
  pm::datafolder::move_old_files(root / "outer" / "inner", root / "outer");
  CHECK(pm::text::read_file(root / "outer" / "inner" / "keys.txt") == "inner");
  CHECK(!fs::exists(root / "outer" / "keys.txt"));
  CHECK(logged("data folder: the old folder and the data folder hold each other; nothing moves"));
  pm::log::set_sink(nullptr, nullptr);
}

// A file that cannot be moved or copied (another program holds it open) is never deleted: it stays in the old folder,
// logged, and the new folder has no copy of it.
void a_file_that_cannot_move_is_never_deleted() {
  const fs::path root = fresh_root("held");
  const fs::path old = root / "someone.glue", folder = root / "data" / "someone.glue";
  write(old / "keybinds.txt", "held");
  const HANDLE held = CreateFileW((old / "keybinds.txt").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
  CHECK(held != INVALID_HANDLE_VALUE);
  PmHostApi host{};
  CHECK(bind_in(root, host));
  CHECK(pm::mod::data_folder() == folder);
  CloseHandle(held);
  CHECK(pm::text::read_file(old / "keybinds.txt") == "held");
  CHECK(!fs::exists(folder / "keybinds.txt"));
  CHECK(logged("data folder: could not move keybinds.txt from the old folder"));
  pm::log::set_sink(nullptr, nullptr);
}

// Without a data folder from the host there is no folder to give: empty, so nothing is made in the working folder.
void no_host_folder_gives_no_data_folder() {
  fake::reset();
  CHECK(pm::mod::bind(fake::host(), &kInfo));  // the fake host's data folder is empty
  CHECK(pm::mod::data_folder().empty());
  pm::log::set_sink(nullptr, nullptr);
}

}  // namespace

int main() {
  a_message_reaches_the_hosts_popup();
  a_level_reaches_the_hosts_log_level();
  a_host_without_log_level_gets_the_level_in_the_text();
  the_data_folder_is_made_on_first_ask();
  the_old_folders_files_move_in_once();
  a_file_the_new_folder_has_stays_in_the_old_one();
  a_file_that_cannot_move_is_never_deleted();
  a_file_that_cannot_be_removed_is_copied();
  an_old_format_id_moves_nothing();
  folders_that_hold_each_other_never_move();
  no_host_folder_gives_no_data_folder();
  std::error_code ec;
  fs::remove_all(fs::temp_directory_path() / ("pm_glue_test_" + std::to_string(GetCurrentProcessId())), ec);
  return g_failures ? 1 : 0;
}
