#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <string>
#include "../tools/launcher.h"
#include "../tools/inject.h"

namespace pm::launcher {
loader::Result launch_steam(std::vector<std::wstring> arguments, const Package& package);
}

namespace {
int failures = 0;
pm::loader::Outcome candidate = pm::loader::Outcome::no_process;
bool spawnSucceeds = false;
int spawnCalls = 0, loadCalls = 0;
DWORD loadedPid = 0;
std::wstring spawnedApplication, spawnedCommand, spawnedDirectory, fixturePath;
bool inheritedDefaults = false;
ULONGLONG ticks = 0;
DWORD processState = WAIT_TIMEOUT;
unsigned sleeps = 0, readyAfterSleeps = 0;
bool concurrentLaunch = false;
pm::loader::Outcome overlappingOutcome = pm::loader::Outcome::ready;
int discoveryCalls = 0;

/** Emit the same completed marker for immediate and delayed fake child startup. */
bool append_menu_marker() {
  HANDLE file = CreateFileW(fixturePath.c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr, OPEN_EXISTING, 0, nullptr);
  if (file == INVALID_HANDLE_VALUE) return false;
  const char marker[] = "Loaded Level: Main Menu | Players: 0\n";
  DWORD written = 0;
  const bool success = WriteFile(file, marker, sizeof(marker) - 1, &written, nullptr) && written == sizeof(marker) - 1;
  CloseHandle(file);
  return success;
}

/** Advance a deterministic startup clock so timeout tests never sleep in real time. */
ULONGLONG WINAPI tick_count() { return ticks; }
/** Simulate child exit and process-monitor errors independently of file readiness. */
DWORD WINAPI wait_for_process(HANDLE, DWORD) { SetLastError(ERROR_INVALID_HANDLE); return processState; }
/** Emit a delayed marker only after the specified number of startup polls. */
void WINAPI sleep_for(DWORD) {
  ticks += 1000;
  if (++sleeps == readyAfterSleeps && !append_menu_marker()) ++failures;
}

/** Resolve a synthetic launcher location whose adjacent DLL cannot exist. */
DWORD WINAPI module_path(HMODULE, LPWSTR path, DWORD length) {
  const auto executable = fixturePath + L"\\PhasmoMods Loader.exe";
  if (executable.size() + 1 > length) return length;
  wcscpy_s(path, length, executable.c_str());
  return static_cast<DWORD>(executable.size());
}

/** Replace child creation while recording the actual production call contract. */
BOOL WINAPI create_process(LPCWSTR app, LPWSTR command, LPSECURITY_ATTRIBUTES processSecurity,
  LPSECURITY_ATTRIBUTES threadSecurity, BOOL inheritHandles, DWORD flags, LPVOID environment,
  LPCWSTR directory, LPSTARTUPINFOW startup, LPPROCESS_INFORMATION process) {
  ++spawnCalls;
  spawnedApplication = app;
  spawnedCommand = command;
  spawnedDirectory = directory ? directory : L"";
  inheritedDefaults = !processSecurity && !threadSecurity && !inheritHandles && !flags &&
    !environment && startup->cb == sizeof(STARTUPINFOW);
  if (concurrentLaunch) {
    concurrentLaunch = false;
    overlappingOutcome = pm::launcher::launch_steam({app, L"-logFile", fixturePath}, {L"unused.dll", {L"mod.dll"}}).outcome;
  }
  if (!spawnSucceeds) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
  if (!append_menu_marker()) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
  process->hProcess = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
  process->hThread = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  process->dwProcessId = GetCurrentProcessId();
  return TRUE;
}
}

namespace pm::loader {
/** Supply deterministic discovery results without inspecting any running game. */
Result find_game(DWORD& pid) {
  ++discoveryCalls;
  pid = candidate == Outcome::ready ? GetCurrentProcessId() : 0;
  return {candidate};
}
/** Record the selected child identity without touching another process. */
Result load(DWORD pid, const wchar_t*, const std::vector<std::wstring>&, const std::vector<std::string>&) { ++loadCalls; loadedPid = pid; return {Outcome::active}; }
}

#define PM_LAUNCHER_TEST
#define CreateProcessW create_process
#define GetModuleFileNameW module_path
#define GetTickCount64 tick_count
#define WaitForSingleObject wait_for_process
#define Sleep sleep_for
#include "../tools/launcher.cpp"
#undef CreateProcessW
#undef GetModuleFileNameW
#undef GetTickCount64
#undef WaitForSingleObject
#undef Sleep
#define CHECK(condition) do { if (!(condition)) { \
  std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

/** Check actual Windows parsing rather than duplicating the quoting algorithm. */
void quoting() {
  const std::vector<std::wstring> arguments = {
    L"C:\\Games with spaces\\Phasmophobia.exe", L"", L"plain", L"雪の幽霊",
    L"value with spaces", L"embedded\"quote", L"C:\\directory with spaces\\",
    L"\\\\server\\path\\", L"before\\\\\"after", L"\t", L"%PATH% & $(test)"
  };
  int count = 0;
  auto parsed = CommandLineToArgvW(pm::launcher::command_line(arguments).c_str(), &count);
  CHECK(parsed != nullptr);
  CHECK(count == static_cast<int>(arguments.size()));
  if (parsed && count == static_cast<int>(arguments.size()))
    for (int i = 0; i < count; ++i) CHECK(arguments[i] == parsed[i]);
  if (parsed) LocalFree(parsed);
}

/** Reject conflicting or disabled logs while preserving user arguments. */
void log_options() {
  using pm::launcher::configure_log;
  std::wstring path;
  std::vector<std::wstring> arguments{L"Phasmophobia.exe", L"-screen-width", L"1280"};
  CHECK(configure_log(arguments, L"fresh.log", path).empty());
  CHECK(path == L"fresh.log");
  CHECK(arguments.size() == 5 && arguments[3] == L"-logFile");
  arguments = {L"Phasmophobia.exe", L"-logFile", L"my log.log", L"-screen-width", L"1280"};
  const auto original = arguments;
  CHECK(configure_log(arguments, L"fresh.log", path).empty());
  CHECK(arguments == original && path == L"my log.log");
  for (const auto& bad : std::vector<std::vector<std::wstring>>{
         {L"Phasmophobia.exe", L"-nolog"},
         {L"Phasmophobia.exe", L"-logFile"},
         {L"Phasmophobia.exe", L"-logFile", L"-"},
         {L"Phasmophobia.exe", L"-logFile", L""},
         {L"Phasmophobia.exe", L"-logFile", L"a", L"-logFile", L"b"},
         {L"Phasmophobia.exe", L"-logFile=other.log"}}) {
    arguments = bad;
    CHECK(!configure_log(arguments, L"fresh.log", path).empty());
  }
}

/** A stale fragment, seasonal scene name or unterminated line cannot signal readiness. */
void readiness() {
  pm::launcher::MenuReadiness parser;
  CHECK(!parser.feed("Set active scene to Main Menu Halloween\n"));
  CHECK(!parser.feed("Loaded Level: Main Menu Extra | Players: 0\n"));
  CHECK(!parser.feed("prefix Loaded Level: Main Menu | Players: 0\n"));
  CHECK(!parser.feed("Loaded Level: Main"));
  CHECK(!parser.feed(" Menu | Players: 0\r"));
  CHECK(parser.feed("\n"));
  pm::launcher::MenuReadiness stale(true);
  CHECK(!stale.feed("Loaded Level: Main Menu | Players: 0\n"));
  CHECK(stale.feed("Loaded Level: Main Menu | Players: 0\n"));
  pm::launcher::MenuReadiness oversized;
  CHECK(!oversized.feed(std::string(100000, 'x')));
  CHECK(!oversized.feed("Loaded Level: Main Menu | Players: 0\n"));
  CHECK(oversized.feed("Loaded Level: Main Menu | Players: 0\n"));
}

/** Write a local fixture while permitting the production follower to read it. */
void write_fixture(const char* content, DWORD disposition = CREATE_ALWAYS) {
  HANDLE file = CreateFileW(fixturePath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
    nullptr, disposition, 0, nullptr);
  CHECK(file != INVALID_HANDLE_VALUE);
  if (disposition == OPEN_EXISTING) SetFilePointer(file, 0, nullptr, FILE_END);
  DWORD written = 0;
  CHECK(WriteFile(file, content, static_cast<DWORD>(strlen(content)), &written, nullptr));
  CHECK(written == strlen(content));
  CloseHandle(file);
}

/** Baselines skip old complete markers and discard old fragments through the next newline. */
void log_files() {
  using namespace pm::launcher;
  write_fixture("Loaded Level: Main Menu | Players: 0\nLoaded Level: Main");
  LogCursor cursor{fixturePath};
  CHECK(read_menu(cursor, true).outcome == pm::loader::Outcome::ready);
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::ready);
  write_fixture(" Menu | Players: 0\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::ready);
  write_fixture("Loaded Level: Main Menu | Players: 0\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::active);
  write_fixture("Loading\n");
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::ready);
  write_fixture("Loaded Level: Main Menu | Players: 0", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::ready);
  write_fixture("\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pm::loader::Outcome::active);
}

/** Failure, duplicate and success paths never rediscover or launch an unrelated process. */
void orchestration() {
  using namespace pm::launcher;
  using pm::loader::Outcome;
  std::wstring hostPath, executable;
  CHECK(adjacent_host(hostPath, executable).outcome == Outcome::file_error);
  CHECK(hostPath == fixturePath + L"\\ModHost.dll");
  const Package package{L"unused.dll", {L"mod.dll"}};
  CHECK(attach({L"unused.dll", {}}).outcome == Outcome::invalid_target);  // nothing ticked
  CHECK(discoveryCalls == 0);
  candidate = Outcome::no_process;
  CHECK(attach(package).outcome == Outcome::no_process);
  candidate = Outcome::ambiguous_process;
  CHECK(attach(package).outcome == Outcome::ambiguous_process);
  CHECK(loadCalls == 0);
  const auto lockName = L"Local\\PhasmoModLauncher-" + std::to_wstring(GetCurrentProcessId());
  HANDLE lock = CreateMutexW(nullptr, FALSE, lockName.c_str());
  candidate = Outcome::ready;
  CHECK(attach(package).outcome == Outcome::already_loaded);
  CHECK(loadCalls == 0);
  CloseHandle(lock);
  const std::vector<std::wstring> arguments = {
    L"C:\\Games with spaces\\Phasmophobia.exe", L"雪", L"trailing slash\\",
    L"-logFile", fixturePath
  };
  CHECK(launch_steam(arguments, package).outcome == Outcome::already_loaded);
  CHECK(spawnCalls == 0);
  candidate = Outcome::no_process;
  CHECK(launch_steam({L"C:\\other.exe"}, package).outcome == Outcome::invalid_target);
  write_fixture("old log\n");
  auto result = launch_steam(arguments, package);
  CHECK(result.outcome == Outcome::process_error && result.error == ERROR_ACCESS_DENIED);
  CHECK(loadCalls == 0 && spawnCalls == 1);
  CHECK(inheritedDefaults && spawnedApplication == arguments[0]);
  CHECK(spawnedCommand == command_line(arguments));
  CHECK(spawnedDirectory == L"C:\\Games with spaces");  // the game's own folder, not the launcher's
  spawnSucceeds = true;
  CHECK(launch_steam(arguments, package).outcome == Outcome::active);
  CHECK(loadCalls == 1 && loadedPid == GetCurrentProcessId());
}

/** Mods are found one folder each, broken ones are named, and the saved selection is honoured. */
void mod_folders() {
  using namespace pm::launcher;
  namespace fs = std::filesystem;
  const fs::path root = fs::path(fixturePath).parent_path() / (L"pgl-mods-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const auto write = [&](const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path(), error);
    std::ofstream(file, std::ios::binary) << text;
  };
  const auto json = [](const std::string& id, const std::string& dll) {
    return R"({"id":")" + id + R"(","name":"N","author":"Me","version":"1.2.3","description":"One","dll":")" + dll + R"("})";
  };
  write(root / "mods" / "b" / "mod.ini", "id=second\nname=Second\ndll=Second.dll\n");  // an old-format mod still loads
  write(root / "mods" / "b" / "Second.dll", "x");
  write(root / "mods" / "a" / "manifest.json", json("me.first", "First.dll"));
  write(root / "mods" / "a" / "First.dll", "x");
  write(root / "mods" / "c" / "manifest.json", "{ not json");
  write(root / "mods" / "c" / "mod.ini", "id=fallback\nname=Fallback\ndll=Bad.dll\n");  // a bad manifest.json is not passed over
  write(root / "mods" / "c" / "Bad.dll", "x");
  write(root / "mods" / "d" / "manifest.json", json("me.nodll", "Missing.dll"));
  write(root / "mods" / "e" / "manifest.json", json("me.first", "First.dll"));
  write(root / "mods" / "e" / "First.dll", "x");
  write(root / "mods" / "f" / "NoManifest.dll", "x");
  write(root / "mods" / "g" / "readme.txt", "not a mod");
  write(root / "mods" / "h" / "manifest.json", json("me.folder", "Folder.dll"));
  write(root / "mods" / "h" / "Folder.dll" / "inside.txt", "a folder, not a DLL");
  std::wstring problems;
  std::vector<pm::modlist::Refused> refused;
  const auto mods = discover_mods({{root / "mods", pm::modlist::Place::InstallFolder}}, problems, refused);
  CHECK(mods.size() == 2 && mods[0].manifest.id == "me.first" && mods[1].manifest.id == "second");
  CHECK(mods.size() == 2 && mods[0].manifest.description == "One" && mods[0].manifest.version == "1.2.3");
  CHECK(mods.size() == 2 && mods[1].manifest.oldFormat && mods[1].manifest.version.empty());
  CHECK(problems.find(L"in c cannot be used. manifest.json is not valid JSON") != std::wstring::npos);
  CHECK(problems.find(L"in d cannot be used. Mod is missing its DLL") != std::wstring::npos);
  CHECK(problems.find(L"in e cannot be used. Mod repeats the id") != std::wstring::npos);
  CHECK(problems.find(L"in f cannot be used. It has a DLL but no manifest.json") != std::wstring::npos);
  CHECK(problems.find(L"in g") == std::wstring::npos);  // a folder with no DLL is no mod
  CHECK(problems.find(L"in h cannot be used. Mod is missing its DLL") != std::wstring::npos);  // a folder is no DLL
  CHECK(refused.size() == 5 && refused[0].name == "c" && refused[3].name == "f" && refused[4].name == "h");
  // They reach the host's Mods panel through the session file.
  const auto lines = session_lines(refused);
  CHECK(lines.size() == 5 && pm::modlist::parse_session_refused(lines[1]).size() == 1 &&
        pm::modlist::parse_session_refused(lines[1])[0].name == "d");
  // And the package's checked-build records, which the session file's mod list does not take for a mod.
  std::string session;
  for (const std::string& line : session_lines(refused, root / "builds")) session += line;
  CHECK(pm::modlist::parse_session_builds(session) == (root / "builds").string());
  CHECK(pm::modlist::parse_session(session).empty());
  CHECK(pm::modlist::parse_session_refused(session).size() == 5);
  CHECK(pm::modlist::parse_session_builds("a.dll\n?b\tc\n").empty());
  const fs::path ini = root / "data" / "launcher.ini";
  CHECK(saved_selection(ini, mods) == (std::vector<std::string>{"me.first", "second"}));  // nothing saved: all
  CHECK(save_selection(ini, {"second"}));
  CHECK(saved_selection(ini, mods) == std::vector<std::string>{"second"});
  const auto dlls = all_dlls(mods);  // every installed mod is listed; the saved choice decides which load and start
  CHECK(dlls.size() == 2 && dlls[0] == (root / "mods" / "a" / "First.dll").wstring() &&
        dlls[1] == (root / "mods" / "b" / "Second.dll").wstring());
  CHECK(save_selection(ini, {}));
  CHECK(saved_selection(ini, mods).empty());  // an empty saved selection stays empty
  fs::remove_all(root, error);
}

/**
 * A mod outside the install folder that claims the project's author is refused as it is read, before any id is compared,
 * so the real mod in the install folder runs whichever folder is read first, and the impostor's row says why.
 */
void real_mod_runs_when_an_impostor_sorts_first() {
  using namespace pm::launcher;
  using pm::modlist::Place;
  namespace fs = std::filesystem;
  const fs::path root = fs::path(fixturePath).parent_path() / (L"pgl-impostor-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const auto write = [&](const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path(), error);
    std::ofstream(file, std::ios::binary) << text;
  };
  const auto json = [](const std::string& id, const std::string& author) {
    return R"({"id":")" + id + R"(","name":"Ghost Tells","author":")" + author +
           R"(","version":"1.0.0","description":"One","dll":"GhostTells.dll"})";
  };
  // "a" sorts before "ghost_tells", and the third-party folder is read first in one of the two orders.
  write(root / "install" / "ghost_tells" / "manifest.json", json("phasmomods.ghost-tells", "PhasmoMods"));
  write(root / "install" / "ghost_tells" / "GhostTells.dll", "x");
  write(root / "other" / "a" / "manifest.json", json("phasmomods.ghost-tells", "Someone"));
  write(root / "other" / "a" / "GhostTells.dll", "x");
  write(root / "other" / "b" / "manifest.json", json("someone.tells", " phasmomods "));
  write(root / "other" / "b" / "GhostTells.dll", "x");
  write(root / "other" / "c" / "manifest.json", json("someone.tells", "Someone"));
  write(root / "other" / "c" / "GhostTells.dll", "x");
  CHECK(ModsFolder{root / "other"}.place == Place::Elsewhere);  // a folder not said to be the install folder is not trusted
  const std::vector<ModsFolder> orders[] = {{{root / "other", Place::Elsewhere}, {root / "install", Place::InstallFolder}},
                                            {{root / "install", Place::InstallFolder}, {root / "other", Place::Elsewhere}}};
  for (const std::vector<ModsFolder>& order : orders) {
    std::wstring problems;
    std::vector<pm::modlist::Refused> refused;
    const auto mods = discover_mods(order, problems, refused);
    CHECK(mods.size() == 2);
    if (mods.size() != 2) continue;
    const ModEntry& real = mods[0].manifest.id == "phasmomods.ghost-tells" ? mods[0] : mods[1];
    const ModEntry& other = mods[0].manifest.id == "phasmomods.ghost-tells" ? mods[1] : mods[0];
    CHECK(real.manifest.id == "phasmomods.ghost-tells" && real.dll == (root / "install" / "ghost_tells" / "GhostTells.dll").wstring());
    CHECK(other.manifest.id == "someone.tells" && other.dll == (root / "other" / "c" / "GhostTells.dll").wstring());
    CHECK(refused.size() == 2 && refused[0].name == "a" && refused[1].name == "b");
    if (refused.size() != 2) continue;
    CHECK(refused[0].why == "id uses the author \"phasmomods\", which is reserved for bundled mods.");
    CHECK(refused[1].why == "Author is PhasmoMods, which is reserved for bundled mods.");
    CHECK(problems.find(L"in a cannot be used. id uses the author") != std::wstring::npos);
    CHECK(problems.find(L"repeats the id") == std::wstring::npos);  // the impostor never took the real one's id
  }
  fs::remove_all(root, error);
}

/** Slow startup waits for evidence; timeout and child exit cannot become activation. */
void startup_wait() {
  using namespace pm::launcher;
  using pm::loader::Outcome;
  write_fixture("");
  LogCursor cursor{fixturePath};
  CHECK(read_menu(cursor, true).outcome == Outcome::ready);
  sleeps = 0;
  readyAfterSleeps = 15;
  CHECK(wait_menu(GetCurrentProcess(), cursor).outcome == Outcome::active);
  CHECK(sleeps == 15);
  write_fixture("");
  cursor = LogCursor{fixturePath};
  readyAfterSleeps = 0;
  CHECK(wait_menu(GetCurrentProcess(), cursor).outcome == Outcome::activation_timeout);
  processState = WAIT_OBJECT_0;
  CHECK(wait_menu(GetCurrentProcess(), cursor).outcome == Outcome::game_exited);
  processState = WAIT_FAILED;
  CHECK(wait_menu(GetCurrentProcess(), cursor).outcome == Outcome::process_error);
  processState = WAIT_TIMEOUT;
}

/** An overlapping wrapper must be rejected before discovery or child creation. */
void concurrent_startup() {
  using pm::loader::Outcome;
  candidate = Outcome::no_process;
  spawnSucceeds = false;
  concurrentLaunch = true;
  const int beforeSpawn = spawnCalls, beforeDiscovery = discoveryCalls;
  const std::vector<std::wstring> arguments{L"C:\\Games\\Phasmophobia.exe", L"-logFile", fixturePath};
  CHECK(pm::launcher::launch_steam(arguments, {L"unused.dll", {L"mod.dll"}}).outcome == Outcome::process_error);
  CHECK(overlappingOutcome == Outcome::already_loaded);
  CHECK(spawnCalls == beforeSpawn + 1 && discoveryCalls == beforeDiscovery + 1);
  CHECK(pm::launcher::launch_steam(arguments, {L"unused.dll", {L"mod.dll"}}).outcome == Outcome::process_error);
  CHECK(spawnCalls == beforeSpawn + 2 && discoveryCalls == beforeDiscovery + 2);
}

/** The compiled-in suite version is the one in the VERSION file, and that file holds MAJOR.MINOR.PATCH. */
void version_matches_version_file() {
  std::ifstream file(PM_SOURCE_DIR "/VERSION");
  std::string line;
  CHECK(static_cast<bool>(std::getline(file, line)));
  const char* blanks = " \t\r\n";
  const auto first = line.find_first_not_of(blanks), last = line.find_last_not_of(blanks);
  const std::string version = first == std::string::npos ? "" : line.substr(first, last - first + 1);
  CHECK(version == PM_SUITE_VERSION);
  CHECK(std::regex_match(version, std::regex(R"(\d+\.\d+\.\d+(-dev\.\d+)?)")));  // a release, or a development build's
}

/** The executable's name has a space, so the Steam launch option must quote its path. */
void launch_option_quotes_spaced_name() {
  CHECK(pm::launcher::steam_launch_option(L"C:\\Games\\PhasmoMods Loader.exe") ==
        L"\"C:\\Games\\PhasmoMods Loader.exe\" --steam %command%");
}

/** The launcher's log is in the data directory, which nothing Velopack runs ever writes. */
void launcher_log_is_in_data_directory() {
  std::vector<wchar_t> saved(32768);
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", saved.data(), static_cast<DWORD>(saved.size()));
  const std::filesystem::path temp = std::filesystem::path(fixturePath).parent_path() / L"pgl appdata";
  CHECK(SetEnvironmentVariableW(L"LOCALAPPDATA", temp.c_str()));
  CHECK(pm::launcher::launcher_log() == temp / L"PhasmoMods" / L"launcher.log");
  SetEnvironmentVariableW(L"LOCALAPPDATA", length && length < saved.size() ? saved.data() : nullptr);
}

/** Exercise argument preservation and conservative startup detection without a game. */
int main() {
  {  // the loader's shipped text (core/text_table.h), so the messages read as players see them
    std::string why;
    if (!pm::i18n::table().load(std::filesystem::path(PM_TEXT_DIR) / "i18n" / "default.json", why)) std::printf("FAIL the loader's text: %s\n", why.c_str());
  }
  wchar_t directory[MAX_PATH], path[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, directory) > 0);
  CHECK(GetTempFileNameW(directory, L"pgl", 0, path) != 0);
  fixturePath = path;
  quoting();
  log_options();
  readiness();
  log_files();
  orchestration();
  mod_folders();
  real_mod_runs_when_an_impostor_sorts_first();
  startup_wait();
  concurrent_startup();
  version_matches_version_file();
  launch_option_quotes_spaced_name();
  launcher_log_is_in_data_directory();
  CHECK(DeleteFileW(fixturePath.c_str()));
  return failures ? 1 : 0;
}
