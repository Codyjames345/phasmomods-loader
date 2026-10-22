#include <windows.h>
#include <shellapi.h>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include "../tools/launcher.h"
#include "../tools/inject.h"

namespace pg::launcher {
loader::Result launch_steam(std::vector<std::wstring> arguments, const Package& package);
}

namespace {
int failures = 0;
pg::loader::Outcome candidate = pg::loader::Outcome::no_process;
bool spawnSucceeds = false;
int spawnCalls = 0, loadCalls = 0;
DWORD loadedPid = 0;
std::wstring spawnedApplication, spawnedCommand, fixturePath;
bool inheritedDefaults = false;
ULONGLONG ticks = 0;
DWORD processState = WAIT_TIMEOUT;
unsigned sleeps = 0, readyAfterSleeps = 0;
bool concurrentLaunch = false;
pg::loader::Outcome overlappingOutcome = pg::loader::Outcome::ready;
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
  const auto executable = fixturePath + L"\\ModLauncher.exe";
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
  inheritedDefaults = !processSecurity && !threadSecurity && !inheritHandles && !flags &&
    !environment && !directory && startup->cb == sizeof(STARTUPINFOW);
  if (concurrentLaunch) {
    concurrentLaunch = false;
    overlappingOutcome = pg::launcher::launch_steam({app, L"-logFile", fixturePath}, {L"unused.dll", {L"mod.dll"}}).outcome;
  }
  if (!spawnSucceeds) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
  if (!append_menu_marker()) { SetLastError(ERROR_WRITE_FAULT); return FALSE; }
  process->hProcess = OpenProcess(SYNCHRONIZE, FALSE, GetCurrentProcessId());
  process->hThread = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  process->dwProcessId = GetCurrentProcessId();
  return TRUE;
}
}

namespace pg::loader {
/** Supply deterministic discovery results without inspecting any running game. */
Result find_game(DWORD& pid) {
  ++discoveryCalls;
  pid = candidate == Outcome::ready ? GetCurrentProcessId() : 0;
  return {candidate};
}
/** Record the selected child identity without touching another process. */
Result load(DWORD pid, const wchar_t*, const std::vector<std::wstring>&) { ++loadCalls; loadedPid = pid; return {Outcome::active}; }
}

#define PG_LAUNCHER_TEST
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
  auto parsed = CommandLineToArgvW(pg::launcher::command_line(arguments).c_str(), &count);
  CHECK(parsed != nullptr);
  CHECK(count == static_cast<int>(arguments.size()));
  if (parsed && count == static_cast<int>(arguments.size()))
    for (int i = 0; i < count; ++i) CHECK(arguments[i] == parsed[i]);
  if (parsed) LocalFree(parsed);
}

/** Reject conflicting or disabled logs while preserving user arguments. */
void log_options() {
  using pg::launcher::configure_log;
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
  pg::launcher::MenuReadiness parser;
  CHECK(!parser.feed("Set active scene to Main Menu Halloween\n"));
  CHECK(!parser.feed("Loaded Level: Main Menu Extra | Players: 0\n"));
  CHECK(!parser.feed("prefix Loaded Level: Main Menu | Players: 0\n"));
  CHECK(!parser.feed("Loaded Level: Main"));
  CHECK(!parser.feed(" Menu | Players: 0\r"));
  CHECK(parser.feed("\n"));
  pg::launcher::MenuReadiness stale(true);
  CHECK(!stale.feed("Loaded Level: Main Menu | Players: 0\n"));
  CHECK(stale.feed("Loaded Level: Main Menu | Players: 0\n"));
  pg::launcher::MenuReadiness oversized;
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
  using namespace pg::launcher;
  write_fixture("Loaded Level: Main Menu | Players: 0\nLoaded Level: Main");
  LogCursor cursor{fixturePath};
  CHECK(read_menu(cursor, true).outcome == pg::loader::Outcome::ready);
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::ready);
  write_fixture(" Menu | Players: 0\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::ready);
  write_fixture("Loaded Level: Main Menu | Players: 0\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::active);
  write_fixture("Loading\n");
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::ready);
  write_fixture("Loaded Level: Main Menu | Players: 0", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::ready);
  write_fixture("\n", OPEN_EXISTING);
  CHECK(read_menu(cursor).outcome == pg::loader::Outcome::active);
}

/** Failure, duplicate and success paths never rediscover or launch an unrelated process. */
void orchestration() {
  using namespace pg::launcher;
  using pg::loader::Outcome;
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
  spawnSucceeds = true;
  CHECK(launch_steam(arguments, package).outcome == Outcome::active);
  CHECK(loadCalls == 1 && loadedPid == GetCurrentProcessId());
}

/** Mods are found one folder each, broken ones are named, and the saved selection is honoured. */
void mod_folders() {
  using namespace pg::launcher;
  namespace fs = std::filesystem;
  const fs::path root = fs::path(fixturePath).parent_path() / (L"pgl-mods-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const auto write = [&](const fs::path& file, const std::string& text) {
    fs::create_directories(file.parent_path(), error);
    std::ofstream(file, std::ios::binary) << text;
  };
  write(root / "mods" / "b" / "mod.ini", "id=second\nname=Second\ndll=Second.dll\n");
  write(root / "mods" / "b" / "Second.dll", "x");
  write(root / "mods" / "a" / "mod.ini", "# first\r\nid=first\r\nname=First\r\ndll=First.dll\r\ndescription=One\r\n");
  write(root / "mods" / "a" / "First.dll", "x");
  write(root / "mods" / "c" / "mod.ini", "id=Bad Id\nname=Bad\ndll=Bad.dll\n");
  write(root / "mods" / "d" / "mod.ini", "id=nodll\nname=No DLL\ndll=Missing.dll\n");
  write(root / "mods" / "e" / "mod.ini", "id=first\nname=Copy\ndll=First.dll\n");
  write(root / "mods" / "e" / "First.dll", "x");
  std::wstring problems;
  const auto mods = discover_mods(root / "mods", problems);
  CHECK(mods.size() == 2 && mods[0].manifest.id == "first" && mods[1].manifest.id == "second");
  CHECK(mods.size() == 2 && mods[0].manifest.description == "One");
  CHECK(problems.find(L"in c cannot be used") != std::wstring::npos);
  CHECK(problems.find(L"in d is missing its DLL") != std::wstring::npos);
  CHECK(problems.find(L"in e repeats the id") != std::wstring::npos);
  const fs::path ini = root / "data" / "launcher.ini";
  CHECK(saved_selection(ini, mods) == (std::vector<std::string>{"first", "second"}));  // nothing saved: all
  CHECK(save_selection(ini, {"second"}));
  CHECK(saved_selection(ini, mods) == std::vector<std::string>{"second"});
  const auto dlls = all_dlls(mods);  // every installed mod loads; the saved choice decides which start
  CHECK(dlls.size() == 2 && dlls[0] == (root / "mods" / "a" / "First.dll").wstring() &&
        dlls[1] == (root / "mods" / "b" / "Second.dll").wstring());
  CHECK(save_selection(ini, {}));
  CHECK(saved_selection(ini, mods).empty());  // an empty saved selection stays empty
  fs::remove_all(root, error);
}

/** Slow startup waits for evidence; timeout and child exit cannot become activation. */
void startup_wait() {
  using namespace pg::launcher;
  using pg::loader::Outcome;
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
  using pg::loader::Outcome;
  candidate = Outcome::no_process;
  spawnSucceeds = false;
  concurrentLaunch = true;
  const int beforeSpawn = spawnCalls, beforeDiscovery = discoveryCalls;
  const std::vector<std::wstring> arguments{L"C:\\Games\\Phasmophobia.exe", L"-logFile", fixturePath};
  CHECK(pg::launcher::launch_steam(arguments, {L"unused.dll", {L"mod.dll"}}).outcome == Outcome::process_error);
  CHECK(overlappingOutcome == Outcome::already_loaded);
  CHECK(spawnCalls == beforeSpawn + 1 && discoveryCalls == beforeDiscovery + 1);
  CHECK(pg::launcher::launch_steam(arguments, {L"unused.dll", {L"mod.dll"}}).outcome == Outcome::process_error);
  CHECK(spawnCalls == beforeSpawn + 2 && discoveryCalls == beforeDiscovery + 2);
}

/** Exercise argument preservation and conservative startup detection without a game. */
int main() {
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
  startup_wait();
  concurrent_startup();
  CHECK(DeleteFileW(fixturePath.c_str()));
  return failures ? 1 : 0;
}
