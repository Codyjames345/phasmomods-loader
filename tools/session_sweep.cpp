#include "session_sweep.h"
#include "win_handle.h"
#include <windows.h>
#include <tlhelp32.h>
#include <filesystem>
#include <set>

namespace pm::loader {
namespace {
namespace fs = std::filesystem;
using tools::Handle;

/** %LOCALAPPDATA%\PhasmoMods, or empty when it cannot be read. */
std::wstring data_directory() {
  wchar_t base[32768];
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, 32768);
  if (!length || length >= 32768) return {};
  return std::wstring(base) + L"\\PhasmoMods";
}

/** Whether [from, to) is digits and fits a DWORD, read into `value`. */
bool digits(const std::wstring& text, size_t from, size_t to, DWORD& value) {
  if (from >= to) return false;
  unsigned long long number = 0;
  for (size_t i = from; i < to; ++i) {
    if (text[i] < L'0' || text[i] > L'9') return false;
    number = number * 10 + static_cast<unsigned>(text[i] - L'0');
    if (number > 0xFFFFFFFFull) return false;
  }
  value = static_cast<DWORD>(number);
  return true;
}

/** The game's process id out of a copy name as fresh_copy makes them (tools/inject.cpp): "<stem>-<pid>-<tick>-<attempt>"
 *  with any extension, where the stem may itself hold dots or digits. 0 when the name is not of that shape, which leaves
 *  it alone. */
DWORD staged_pid(const std::wstring& name) {
  const size_t dash = name.find_last_of(L'-');
  // The extension, if any, starts at the first dot after that dash; a dot in the stem, before it, is part of the stem.
  const size_t dot = dash == std::wstring::npos ? std::wstring::npos : name.find(L'.', dash + 1);
  const std::wstring body = dot == std::wstring::npos ? name : name.substr(0, dot);
  size_t end = body.size();
  DWORD pid = 0;
  for (int segment = 0; segment < 3; ++segment) {  // attempt, then tick, then pid
    if (end == 0) return 0;
    const size_t separator = body.rfind(L'-', end - 1);
    if (separator == std::wstring::npos) return 0;
    DWORD value = 0;
    if (!digits(body, separator + 1, end, value)) return 0;
    end = separator;
    pid = value;
  }
  return end == 0 ? 0 : pid;  // no stem in front of the three numbers: not a copy's name
}

/** The game's process id out of the session file's name "session-<pid>.txt" (write_session, tools/inject.cpp); 0 otherwise. */
DWORD session_file_pid(const std::wstring& name) {
  const size_t dot = name.rfind(L'.');
  if (dot == std::wstring::npos || name.compare(dot, std::wstring::npos, L".txt") != 0) return 0;
  if (name.rfind(L"session-", 0) != 0) return 0;
  DWORD pid = 0;
  return digits(name, 8, dot, pid) ? pid : 0;
}

/** The writer's process id out of a game log's name "game-<pid>-<tick>.log" (reserve_log, tools/launcher.cpp); 0 otherwise. */
DWORD game_log_pid(const std::wstring& name) {
  if (name.rfind(L"game-", 0) != 0) return 0;
  const size_t dash = name.find(L'-', 5);
  const size_t dot = name.rfind(L'.');
  if (dash == std::wstring::npos || dot == std::wstring::npos || dot < dash) return 0;
  DWORD pid = 0, tick = 0;
  if (!digits(name, 5, dash, pid) || !digits(name, dash + 1, dot, tick)) return 0;
  return pid;
}

/** Every running Phasmophobia.exe pid into `games` and every running pid into `all`. False for an inventory Windows
 *  would not complete (the rule find_game keeps, tools/inject.cpp): nothing may go on a partial view. */
bool running_processes(std::set<DWORD>& games, std::set<DWORD>& all) {
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.value == INVALID_HANDLE_VALUE) return false;
  PROCESSENTRY32W entry{sizeof entry};
  BOOL more = Process32FirstW(snapshot.value, &entry);
  while (more) {
    all.insert(entry.th32ProcessID);
    if (!_wcsicmp(entry.szExeFile, L"Phasmophobia.exe")) games.insert(entry.th32ProcessID);
    more = Process32NextW(snapshot.value, &entry);
  }
  return GetLastError() == ERROR_NO_MORE_FILES;
}

/** Move a mods.log past the cap aside as mods.log.old, replacing the copy that is there. While a host still writes,
 *  Windows refuses the move and the next load rotates it then. */
void rotate_native_log(const std::wstring& root) {
  const std::wstring path = root + L"\\mods.log";
  WIN32_FILE_ATTRIBUTE_DATA found{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &found)) return;
  const long long size = (static_cast<long long>(found.nFileSizeHigh) << 32) | found.nFileSizeLow;
  if (size <= kNativeLogCapBytes) return;
  MoveFileExW(path.c_str(), (path + L".old").c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}
}  // namespace

void sweep_previous_sessions() {
  const std::wstring root = data_directory();
  if (root.empty()) return;
  std::set<DWORD> games, all;
  if (!running_processes(games, all)) return;
  std::error_code error;
  for (fs::directory_iterator it(fs::path(root) / L"bin", error), end; !error && it != end; it.increment(error)) {
    const std::wstring name = it->path().filename().wstring();
    DWORD pid = staged_pid(name);
    if (!pid) pid = session_file_pid(name);
    if (!pid || games.contains(pid)) continue;
    std::error_code ignored;
    fs::remove_all(it->path(), ignored);  // held by another process, it stays for the next load
  }
  for (fs::directory_iterator it(fs::path(root) / L"logs", error), end; !error && it != end; it.increment(error)) {
    const std::wstring name = it->path().filename().wstring();
    const DWORD pid = game_log_pid(name);
    if (!pid || all.contains(pid)) continue;
    std::error_code ignored;
    fs::remove(it->path(), ignored);
  }
  rotate_native_log(root);
}

}  // namespace pm::loader
