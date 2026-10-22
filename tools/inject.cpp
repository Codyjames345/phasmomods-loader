#include "inject.h"
#include "text_files.h"
#include "win_handle.h"
#include <tlhelp32.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <vector>

namespace pg::loader {
namespace {
using tools::Handle;

/** Create a typed failure without losing the Windows error to cleanup operations. */
Result fail(Outcome outcome, const wchar_t* detail, DWORD error = GetLastError()) {
  return {outcome, error, detail, {}};
}

/** Return the filename portion for Windows process and module comparisons. */
const wchar_t* filename(const wchar_t* path) {
  const wchar_t* separator = wcsrchr(path, L'\\');
  return separator ? separator + 1 : path;
}

/** Resolve a real file to its normalized DOS path without loading its contents. */
bool canonical_path(const wchar_t* path, std::wstring& output) {
  Handle file(CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE) return false;
  std::vector<wchar_t> buffer(32768);
  DWORD size = GetFinalPathNameByHandleW(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!size || size >= buffer.size()) { if (size) SetLastError(ERROR_FILENAME_EXCED_RANGE); return false; }
  output.assign(buffer.data(), size);
  return true;
}

/** Obtain a complete module snapshot or reject an incomplete inventory. */
Result list_modules(DWORD pid, std::vector<MODULEENTRY32W>& modules) {
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
  if (snapshot.value == INVALID_HANDLE_VALUE) return fail(Outcome::process_error, L"Windows could not inspect the game's modules. Restart the game and try again.");
  MODULEENTRY32W entry{sizeof entry};
  if (!Module32FirstW(snapshot.value, &entry)) return fail(Outcome::process_error, L"Windows could not read the game's modules. Restart the game and try again.");
  do { modules.push_back(entry); } while (Module32NextW(snapshot.value, &entry));
  if (GetLastError() != ERROR_NO_MORE_FILES) return fail(Outcome::process_error, L"Windows could not finish inspecting the game's modules. Try again.");
  return {};
}

/** Recognize any retained host copy before attempting another load. */
bool already_loaded(const std::vector<MODULEENTRY32W>& modules) {
  for (const auto& entry : modules)
    if (!_wcsicmp(entry.szModule, L"ModHost.dll") || !_wcsnicmp(entry.szModule, L"host-", 5)) return true;
  return false;
}

/** Resolve the system export relative to its actual containing module in the target. */
Result resolve_loader(const std::vector<MODULEENTRY32W>& modules, LPTHREAD_START_ROUTINE& entry) {
  HMODULE kernel = GetModuleHandleW(L"kernel32.dll"), owner = nullptr;
  if (!kernel) return fail(Outcome::process_error, L"Windows could not locate its library loader. Restart the launcher.");
  FARPROC address = GetProcAddress(kernel, "LoadLibraryW");
  if (!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     reinterpret_cast<LPCWSTR>(address), &owner))
    return fail(Outcome::process_error, L"Windows could not resolve its library loader. Restart the launcher.");
  wchar_t path[32768];
  DWORD length = GetModuleFileNameW(owner, path, 32768);
  if (!length || length >= 32768) return fail(Outcome::process_error, L"Windows could not identify its library loader. Restart the launcher.");
  std::wstring localPath;
  if (!canonical_path(path, localPath)) return fail(Outcome::file_error, L"Windows could not read its library loader path. Restart the launcher.");
  auto offset = reinterpret_cast<const BYTE*>(address) - reinterpret_cast<const BYTE*>(owner);
  for (const auto& module : modules) {
    if (_wcsicmp(filename(path), module.szModule)) continue;
    std::wstring remotePath;
    if (!canonical_path(module.szExePath, remotePath)) return fail(Outcome::file_error, L"Windows could not verify the game's system library. Restart the game.");
    if (!_wcsicmp(localPath.c_str(), remotePath.c_str()) && offset >= 0 && static_cast<size_t>(offset) < module.modBaseSize) {
      entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(module.modBaseAddr + offset);
      return {};
    }
  }
  return fail(Outcome::process_error, L"The game's Windows library does not match this launcher. Restart the game and launcher.", ERROR_MOD_NOT_FOUND);
}

/** Verify process identity and architecture before requesting any remote execution. */
Result check_target(HANDLE process) {
  DWORD wait = WaitForSingleObject(process, 0);
  if (wait == WAIT_OBJECT_0) return fail(Outcome::game_exited, L"Phasmophobia has closed. Start it again before loading the mod.", 0);
  if (wait != WAIT_TIMEOUT) return fail(Outcome::process_error, L"Windows could not inspect the game process. Try again.");
  wchar_t path[32768]; DWORD length = 32768;
  if (!QueryFullProcessImageNameW(process, 0, path, &length)) return fail(Outcome::process_error, L"Windows could not identify the game process. Try again.");
  if (_wcsicmp(filename(path), L"Phasmophobia.exe")) return fail(Outcome::invalid_target, L"The selected process is not Phasmophobia. Start the game through Steam.", 0);
  USHORT machine = 0, native = 0;
  if (!IsWow64Process2(process, &machine, &native)) return fail(Outcome::process_error, L"Windows could not check the game architecture. Restart the launcher.");
  if (machine != IMAGE_FILE_MACHINE_UNKNOWN || native != IMAGE_FILE_MACHINE_AMD64 || sizeof(void*) != 8)
    return fail(Outcome::wrong_architecture, L"This launcher requires the Windows x64 version of Phasmophobia.", 0);
  return {};
}

/** Create a directory or verify that an existing path is a directory. */
bool ensure_directory(const std::wstring& path) {
  if (CreateDirectoryW(path.c_str(), nullptr)) return true;
  if (GetLastError() != ERROR_ALREADY_EXISTS) return false;
  DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) return false;
  if (!(attributes & FILE_ATTRIBUTE_DIRECTORY)) { SetLastError(ERROR_DIRECTORY); return false; }
  return true;
}

/** Copy one DLL to a name no earlier copy has used, so a running game never holds the original. */
bool fresh_copy(const std::wstring& directory, const wchar_t* source, const std::wstring& stem, DWORD pid, std::wstring& copy) {
  for (unsigned attempt = 0; attempt < 10; ++attempt) {
    copy = directory + L"\\" + stem + L"-" + std::to_wstring(pid) + L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt) + L".dll";
    if (CopyFileW(source, copy.c_str(), TRUE)) return true;
    if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
  }
  return false;
}

/** The file stem of a path, for naming its copy. */
std::wstring stem_of(const std::wstring& path) {
  std::wstring name = filename(path.c_str());
  const size_t dot = name.rfind(L'.');
  return dot == std::wstring::npos ? name : name.substr(0, dot);
}

/** Copy the mod.ini beside a mod's DLL to <copy>.ini, where the host reads its description and requirement. */
void copy_manifest(const std::wstring& dll, const std::wstring& copy) {
  const size_t slash = dll.find_last_of(L"\\/");
  const std::wstring manifest = (slash == std::wstring::npos ? L"" : dll.substr(0, slash + 1)) + L"mod.ini";
  const std::wstring target = copy.substr(0, copy.size() - 4) + L".ini";
  // A mod without it still loads; the host logs the missing manifest, and a mod that needs another
  // (Custom Ghosts needs Spectral Flame) checks for it itself through the other's service.
  CopyFileW(manifest.c_str(), target.c_str(), FALSE);
}

/** Write the session file the host reads for this process: one UTF-8 mod path per line. */
bool write_session(const std::wstring& directory, DWORD pid, const std::vector<std::wstring>& copies) {
  std::string text;
  for (const auto& copy : copies) text += pg::text::narrow(copy) + "\n";
  const std::wstring path = directory + L"\\session-" + std::to_wstring(pid) + L".txt";
  Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  return text.empty() || (WriteFile(file.value, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size());
}

/** Prepare local storage and make exclusive copies of the host and its mods for this process lifetime. */
Result copy_native(DWORD pid, const wchar_t* host, const std::vector<std::wstring>& mods, std::wstring& copy, std::wstring& log) {
  wchar_t base[32768];
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, 32768);
  if (!length || length >= 32768) return fail(Outcome::file_error, L"The local application-data folder is unavailable. Check LOCALAPPDATA and try again.");
  std::wstring directory = std::wstring(base) + L"\\PhasmoMods";
  if (!ensure_directory(directory)) return fail(Outcome::file_error, L"The launcher cannot create its data folder. Check folder access and try again.");
  log = directory + L"\\mods.log";
  directory += L"\\bin";
  if (!ensure_directory(directory)) return fail(Outcome::file_error, L"The launcher cannot create its native-copy folder. Check folder access and try again.");
  std::vector<std::wstring> modCopies;
  for (const auto& mod : mods) {
    std::wstring modCopy;
    if (!fresh_copy(directory, mod.c_str(), stem_of(mod), pid, modCopy))
      return fail(Outcome::file_error, L"The launcher cannot copy a mod's DLL. Check the package and folder access, then try again.");
    copy_manifest(mod, modCopy);
    modCopies.push_back(modCopy);
  }
  if (!write_session(directory, pid, modCopies))
    return fail(Outcome::file_error, L"The launcher cannot write the list of mods to start. Check folder access and try again.");
  if (!fresh_copy(directory, host, L"host", pid, copy))
    return fail(Outcome::file_error, L"The launcher cannot copy ModHost.dll. Check the package and folder access, then try again.");
  return {};
}

/** Start and wait for LoadLibraryW without ever freeing a live thread's argument. */
Result map_native(HANDLE process, LPTHREAD_START_ROUTINE entry, const std::wstring& path) {
  SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t), written = 0;
  void* argument = VirtualAllocEx(process, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!argument) return fail(Outcome::allocation_failed, L"Windows could not reserve memory for loading the mod. Restart the game and try again.");
  Result result;
  if (!WriteProcessMemory(process, argument, path.c_str(), bytes, &written))
    result = fail(Outcome::write_failed, L"Windows could not send the mod path to the game. Restart the game and try again.");
  else if (written != bytes)
    result = fail(Outcome::write_failed, L"Windows sent only part of the mod path. Restart the game and try again.", ERROR_PARTIAL_COPY);
  if (result.outcome == Outcome::ready) {
    Handle thread(CreateRemoteThread(process, nullptr, 0, entry, argument, 0, nullptr));
    if (!thread.value) result = fail(Outcome::thread_failed, L"Windows could not start loading the mod. Restart the game and try again.");
    else {
      DWORD wait = WaitForSingleObject(thread.value, 10000);
      if (wait != WAIT_OBJECT_0) {
        DWORD error = wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
          return fail(Outcome::game_exited, L"Phasmophobia closed while loading the mod. Start the game again.", 0);
        return fail(Outcome::indeterminate, L"Loading has not finished. Restart the game before trying again.", error);
      }
      if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
        return fail(Outcome::game_exited, L"Phasmophobia closed while loading the mod. Start the game again.", 0);
    }
  }
  if (!VirtualFreeEx(process, argument, 0, MEM_RELEASE) && result.outcome == Outcome::ready)
    return fail(Outcome::process_error, L"Windows could not finish cleaning up the loading operation. Restart the game before trying again.");
  return result;
}

/** Interpret complete PID-tagged lines, requiring loading before activation. */
Outcome parse_line(const std::string& line, DWORD pid, bool& loading) {
  std::string prefix = "[pid=" + std::to_string(pid) + " tid=";
  if (line.size() <= 13 + prefix.size() || line.compare(13, prefix.size(), prefix)) return Outcome::ready;
  unsigned long thread = 0;
  const char* end = line.data() + line.size();
  auto parsed = std::from_chars(line.data() + 13 + prefix.size(), end, thread);
  if (parsed.ec != std::errc{} || end - parsed.ptr < 2 || parsed.ptr[0] != ']' || parsed.ptr[1] != ' ') return Outcome::ready;
  std::string message(parsed.ptr + 2, end);
  if (!message.empty() && message.back() == '\r') message.pop_back();
  if (message == "ModHost loading") loading = true;
  if (!loading) return Outcome::ready;
  if (message == "ModHost active") return Outcome::active;
  if (message.starts_with("ModHost inactive: unsupported or unreadable game binaries")) return Outcome::unsupported_build;
  if (message.starts_with("ModHost inactive:") || message.starts_with("ModHost stopped;")) return Outcome::inactive;
  return Outcome::ready;
}

/** Retain a bounded tail so truncation followed by regrowth cannot reuse prior events. */
struct LogCursor {
  LONGLONG offset = 0;
  std::string anchor, pending;
  bool loading = false, skipLine = false;
};

/** Read the bytes before an offset and leave the reader positioned at that offset. */
bool read_anchor(HANDLE log, LONGLONG offset, std::string& anchor) {
  DWORD wanted = static_cast<DWORD>(std::min<LONGLONG>(offset, 64)), count = 0;
  LARGE_INTEGER start; start.QuadPart = offset - wanted;
  char bytes[64];
  if (!SetFilePointerEx(log, start, nullptr, FILE_BEGIN) || !ReadFile(log, bytes, wanted, &count, nullptr)) return false;
  if (count != wanted) { SetLastError(ERROR_HANDLE_EOF); return false; }
  anchor.assign(bytes, count);
  return true;
}

/** Consume only appended complete lines and reject truncation or unbounded log output. */
Result read_activation(HANDLE log, LogCursor& cursor, DWORD pid) {
  LARGE_INTEGER size;
  if (!GetFileSizeEx(log, &size)) return fail(Outcome::log_error, L"The native log cannot be read. Check the log file and restart the game.");
  if (size.QuadPart < cursor.offset || size.QuadPart - cursor.offset > 1024 * 1024)
    return fail(Outcome::log_error, L"The native log changed unexpectedly. Restart the game and try again.", ERROR_INVALID_DATA);
  std::string anchor;
  if (!read_anchor(log, cursor.offset, anchor)) return fail(Outcome::log_error, L"The native log cannot be read. Restart the game and try again.");
  if (anchor != cursor.anchor) return fail(Outcome::log_error, L"The native log was rewritten. Restart the game and try again.", ERROR_INVALID_DATA);
  Outcome state = Outcome::ready;
  while (cursor.offset < size.QuadPart) {
    char bytes[4096]; DWORD count = 0;
    DWORD wanted = static_cast<DWORD>(std::min<LONGLONG>(sizeof bytes, size.QuadPart - cursor.offset));
    if (!ReadFile(log, bytes, wanted, &count, nullptr)) return fail(Outcome::log_error, L"The native log cannot be read. Restart the game and try again.");
    if (!count) return fail(Outcome::log_error, L"The native log was truncated. Restart the game and try again.", ERROR_HANDLE_EOF);
    cursor.offset += count; cursor.pending.append(bytes, count);
    size_t end;
    while ((end = cursor.pending.find('\n')) != std::string::npos) {
      Outcome next = cursor.skipLine ? Outcome::ready : parse_line(cursor.pending.substr(0, end), pid, cursor.loading);
      cursor.skipLine = false;
      cursor.pending.erase(0, end + 1);
      if (next == Outcome::inactive || next == Outcome::unsupported_build) state = next;
      else if (next == Outcome::active && state == Outcome::ready) state = next;
    }
    if (cursor.pending.size() > 4096) return fail(Outcome::log_error, L"The native log contains an invalid line. Restart the game and try again.", ERROR_INVALID_DATA);
  }
  if (!read_anchor(log, cursor.offset, cursor.anchor)) return fail(Outcome::log_error, L"The native log cannot be read. Restart the game and try again.");
  if (state == Outcome::active) return {state, 0, L"The mods are loaded. Answer the notice in the game to start them.", {}};
  if (state == Outcome::unsupported_build) return fail(state, L"This game build is unsupported or unreadable. Use a mod release verified for this game update.", 0);
  if (state == Outcome::inactive) return fail(state, L"The mods could not activate. Check the native log and restart the game before trying again.", 0);
  return {};
}

/** Wait at most 65 seconds for activation, keeping game exit distinct from log failure. */
Result wait_activation(HANDLE process, HANDLE log, LogCursor cursor, DWORD pid) {
  ULONGLONG started = GetTickCount64();
  do {
    DWORD wait = WaitForSingleObject(process, 0);
    if (wait == WAIT_OBJECT_0) return fail(Outcome::game_exited, L"Phasmophobia closed before the mod activated. Check the native log.", 0);
    if (wait != WAIT_TIMEOUT) return fail(Outcome::process_error, L"Windows could not check the game process. Restart the game.");
    Result result = read_activation(log, cursor, pid);
    if (result.outcome != Outcome::ready) return result;
    Sleep(100);
  } while (GetTickCount64() - started < 65000);
  return fail(Outcome::activation_timeout, L"The mods did not confirm activation. Check the native log and restart the game before trying again.", ERROR_TIMEOUT);
}
}  // namespace

Result find_game(DWORD& pid) {
  pid = 0;
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.value == INVALID_HANDLE_VALUE) return fail(Outcome::process_error, L"Windows could not list running games. Try again.");
  PROCESSENTRY32W entry{sizeof entry}; DWORD candidate = 0; unsigned count = 0;
  BOOL more = Process32FirstW(snapshot.value, &entry);
  while (more) {
    if (!_wcsicmp(entry.szExeFile, L"Phasmophobia.exe")) { candidate = entry.th32ProcessID; ++count; }
    more = Process32NextW(snapshot.value, &entry);
  }
  if (GetLastError() != ERROR_NO_MORE_FILES) return fail(Outcome::process_error, L"Windows could not finish listing games. Try again.");
  if (!count) return fail(Outcome::no_process, L"Phasmophobia is not running. Start it through Steam and wait for the main menu.", 0);
  if (count != 1) return fail(Outcome::ambiguous_process, L"More than one Phasmophobia process is running. Close the extra games and try again.", 0);
  pid = candidate;
  return {};
}

Result load(DWORD pid, const wchar_t* hostDllPath, const std::vector<std::wstring>& modDllPaths) {
  if (!hostDllPath || !*hostDllPath) return fail(Outcome::file_error, L"ModHost.dll is missing. Extract the complete package again.", ERROR_INVALID_PARAMETER);
  std::wstring source;
  if (!canonical_path(hostDllPath, source)) return fail(Outcome::file_error, L"ModHost.dll cannot be read. Extract the complete package again.");
  std::vector<std::wstring> mods;
  for (const auto& mod : modDllPaths) {
    std::wstring canonical;
    if (!canonical_path(mod.c_str(), canonical)) return fail(Outcome::file_error, L"A selected mod's DLL cannot be read. Extract the complete package again.");
    mods.push_back(canonical);
  }
  constexpr DWORD rights = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE;
  Handle process(OpenProcess(rights, FALSE, pid));
  if (!process.value) return fail(Outcome::process_error, L"Windows denied access to Phasmophobia. Run the game and launcher under the same account and elevation level.");
  Result result = check_target(process.value);
  if (result.outcome != Outcome::ready) return result;
  std::vector<MODULEENTRY32W> modules;
  result = list_modules(pid, modules);
  if (result.outcome != Outcome::ready) return result;
  if (already_loaded(modules)) return fail(Outcome::already_loaded, L"The mods are already loaded. Change which mods run from the Mods button on the game's main menu; restart the game to change their versions.", 0);
  LPTHREAD_START_ROUTINE entry = nullptr;
  result = resolve_loader(modules, entry);
  if (result.outcome != Outcome::ready) return result;
  std::wstring copy, logPath;
  result = copy_native(pid, source.c_str(), mods, copy, logPath);
  if (result.outcome != Outcome::ready) return result;
  Handle log(CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (log.value == INVALID_HANDLE_VALUE) return fail(Outcome::log_error, L"The native log cannot be opened. Check folder access and try again.");
  LARGE_INTEGER offset;
  LogCursor cursor;
  if (!GetFileSizeEx(log.value, &offset) || !read_anchor(log.value, offset.QuadPart, cursor.anchor))
    return fail(Outcome::log_error, L"The native log position cannot be read. Check the log file and try again.");
  cursor.offset = offset.QuadPart;
  cursor.skipLine = !cursor.anchor.empty() && cursor.anchor.back() != '\n';
  result = map_native(process.value, entry, copy);
  if (result.outcome == Outcome::ready) {
    modules.clear(); result = list_modules(pid, modules);
    if (result.outcome == Outcome::ready) {
      std::wstring expected; bool mapped = false;
      if (!canonical_path(copy.c_str(), expected)) result = fail(Outcome::file_error, L"The copied native library cannot be verified. Restart the game.");
      else {
        for (const auto& module : modules) {
          if (_wcsicmp(filename(copy.c_str()), module.szModule)) continue;
          std::wstring actual;
          if (!canonical_path(module.szExePath, actual)) { result = fail(Outcome::file_error, L"The loaded native library cannot be verified. Restart the game."); break; }
          if (!_wcsicmp(expected.c_str(), actual.c_str())) mapped = true;
        }
        if (result.outcome == Outcome::ready)
          result = mapped ? wait_activation(process.value, log.value, cursor, pid)
                          : fail(Outcome::not_mapped, L"Windows did not load ModHost.dll. Check the package and restart the game before trying again.", ERROR_MOD_NOT_FOUND);
      }
    }
  }
  result.logPath = logPath;
  return result;
}
}  // namespace pg::loader

#ifndef PG_LOADER_NO_MAIN
/** Expose the shared loader for development; changing versions requires a game restart. */
int wmain(int argc, wchar_t** argv) {
  if (argc < 3 || _wcsicmp(argv[1], L"inject")) {
    fwprintf(stderr, L"Usage: pginject inject <ModHost.dll> [<mod.dll> ...]\nRestart the game to stop or change versions.\n"); return 2;
  }
  DWORD pid = 0;
  auto result = pg::loader::find_game(pid);
  if (result.outcome == pg::loader::Outcome::ready) result = pg::loader::load(pid, argv[2], {argv + 3, argv + argc});
  fwprintf(result.outcome == pg::loader::Outcome::active ? stdout : stderr, L"%ls (Windows error %lu)\n", result.detail.c_str(), result.error);
  if (!result.logPath.empty()) fwprintf(stderr, L"Native log: %ls\n", result.logPath.c_str());
  return result.outcome == pg::loader::Outcome::active ? 0 : 1;
}
#endif
