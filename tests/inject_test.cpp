#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::printf("FAIL line %d: %s\n", __LINE__, #value); ++failures; } } while (0)
HANDLE fakeProcess = reinterpret_cast<HANDLE>(1), fakeThread = reinterpret_cast<HANDLE>(2);
HANDLE fakeSnapshot = reinterpret_cast<HANDLE>(3);
bool allocationFails = false, writeFails = false, shortWrite = false, threadFails = false;
bool exitTarget = false, mapDll = true, wrongTarget = false, wrongArchitecture = false, wrongMappedPath = false;
bool denyOpen = false, truncateLog = false, snapshotFails = false, exitAfterThread = false;
DWORD threadWait = WAIT_OBJECT_0, processCount = 1;
int frees = 0, closes = 0, allocations = 0, threadStarts = 0, moduleIndex = 0;
ULONGLONG elapsed = 0;
std::wstring fakeLogPath, copiedPath;
std::string emitted, delayed;
std::vector<MODULEENTRY32W> fakeModules;

/** Write controlled complete or partial native events at the thread boundary. */
void write_log(const std::string& content, const wchar_t* mode = L"ab") {
  FILE* file = _wfsopen(fakeLogPath.c_str(), mode, _SH_DENYWR);
  CHECK(file != nullptr);
  if (file) { CHECK(fwrite(content.data(), 1, content.size(), file) == content.size()); fclose(file); }
}
/** Supply one fixed target without acquiring real process access. */
HANDLE WINAPI open_process(DWORD rights, BOOL inherit, DWORD pid) {
  CHECK(rights != PROCESS_ALL_ACCESS && (rights & SYNCHRONIZE) != 0);
  CHECK(!inherit && pid == 42);
  if (denyOpen) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
  return fakeProcess;
}
/** Reject names other than the game through the production identity check. */
BOOL WINAPI process_name(HANDLE, DWORD, LPWSTR value, PDWORD size) {
  const wchar_t* name = wrongTarget ? L"C:\\Games\\Other.exe" : L"C:\\Games\\Phasmophobia.exe";
  wcscpy_s(value, *size, name); *size = static_cast<DWORD>(wcslen(name)); return TRUE;
}
/** Model native x64 and WOW64 targets without depending on host architecture. */
BOOL WINAPI architecture(HANDLE, USHORT* machine, USHORT* native) {
  *machine = wrongArchitecture ? IMAGE_FILE_MACHINE_I386 : IMAGE_FILE_MACHINE_UNKNOWN;
  *native = IMAGE_FILE_MACHINE_AMD64; return TRUE;
}
/** Expose only synthetic process and module inventories. */
HANDLE WINAPI create_snapshot(DWORD, DWORD) {
  if (snapshotFails) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
  moduleIndex = 0; return fakeSnapshot;
}
/** Return a configured number of candidate games. */
BOOL WINAPI process_first(HANDLE, PROCESSENTRY32W* entry) {
  moduleIndex = 0;
  if (!processCount) { SetLastError(ERROR_NO_MORE_FILES); return FALSE; }
  wcscpy_s(entry->szExeFile, L"Phasmophobia.exe"); entry->th32ProcessID = 42; return TRUE;
}
/** End candidate iteration with the documented no-more-files result. */
BOOL WINAPI process_next(HANDLE, PROCESSENTRY32W* entry) {
  if (++moduleIndex < static_cast<int>(processCount)) { entry->th32ProcessID++; return TRUE; }
  SetLastError(ERROR_NO_MORE_FILES); return FALSE;
}
/** Return the first module from the controlled snapshot. */
BOOL WINAPI module_first(HANDLE, MODULEENTRY32W* entry) {
  moduleIndex = 0; *entry = fakeModules.front(); return TRUE;
}
/** Enumerate a complete module inventory, including the loaded unique copy. */
BOOL WINAPI module_next(HANDLE, MODULEENTRY32W* entry) {
  if (++moduleIndex < static_cast<int>(fakeModules.size())) { *entry = fakeModules[moduleIndex]; return TRUE; }
  SetLastError(ERROR_NO_MORE_FILES); return FALSE;
}
/** Track allocation ownership without writing another process. */
LPVOID WINAPI allocate(HANDLE, LPVOID, SIZE_T, DWORD, DWORD) {
  ++allocations;
  if (allocationFails) { SetLastError(ERROR_NOT_ENOUGH_MEMORY); return nullptr; }
  return reinterpret_cast<void*>(0x1000);
}
/** Capture the actual destination path and simulate complete or partial writes. */
BOOL WINAPI write_memory(HANDLE, LPVOID, LPCVOID value, SIZE_T bytes, SIZE_T* count) {
  copiedPath = static_cast<const wchar_t*>(value); *count = shortWrite ? bytes - 1 : bytes;
  if (writeFails) { SetLastError(ERROR_PARTIAL_COPY); return FALSE; }
  return TRUE;
}
/** Count only safe releases of the remote argument. */
BOOL WINAPI free_memory(HANDLE, LPVOID, SIZE_T bytes, DWORD kind) {
  CHECK(bytes == 0 && kind == MEM_RELEASE); ++frees;
  if (exitTarget) { SetLastError(ERROR_INVALID_HANDLE); return FALSE; }
  return TRUE;
}
/** Simulate mapping and logging when the remote loader thread is created. */
HANDLE WINAPI remote_thread(HANDLE, LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE start,
                             LPVOID argument, DWORD, LPDWORD) {
  ++threadStarts;
  CHECK(argument == reinterpret_cast<void*>(0x1000));
  CHECK(reinterpret_cast<void*>(start) != reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW")));
  if (threadFails) { SetLastError(ERROR_ACCESS_DENIED); return nullptr; }
  if (mapDll) {
    MODULEENTRY32W entry{sizeof entry};
    wcscpy_s(entry.szModule, wcsrchr(copiedPath.c_str(), L'\\') + 1);
    wcscpy_s(entry.szExePath, wrongMappedPath ? fakeModules[0].szExePath : copiedPath.c_str());
    fakeModules.push_back(entry);
  }
  write_log(emitted, truncateLog ? L"wb" : L"ab");
  if (exitAfterThread) exitTarget = true;
  return fakeThread;
}
/** Make waits deterministic and expose thread uncertainty separately from exit. */
DWORD WINAPI wait_handle(HANDLE handle, DWORD) {
  if (handle == fakeThread) { if (threadWait == WAIT_FAILED) SetLastError(ERROR_INVALID_HANDLE); return threadWait; }
  CHECK(handle == fakeProcess); return exitTarget ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}
/** Close fake handles and delegate genuine file handles to Windows. */
BOOL WINAPI close_handle(HANDLE handle) {
  if (handle == fakeProcess || handle == fakeThread || handle == fakeSnapshot) { ++closes; return TRUE; }
  return CloseHandle(handle);
}
/** Advance bounded activation polling without wall-clock delay. */
void WINAPI sleep_for(DWORD delay) {
  elapsed += delay;
  if (!delayed.empty()) { write_log(delayed); delayed.clear(); }
}
/** Supply deterministic monotonic time. */
ULONGLONG WINAPI ticks() { return elapsed; }
}

#define PG_LOADER_NO_MAIN
#define OpenProcess open_process
#define QueryFullProcessImageNameW process_name
#define IsWow64Process2 architecture
#define CreateToolhelp32Snapshot create_snapshot
#define Process32FirstW process_first
#define Process32NextW process_next
#define Module32FirstW module_first
#define Module32NextW module_next
#define VirtualAllocEx allocate
#define WriteProcessMemory write_memory
#define VirtualFreeEx free_memory
#define CreateRemoteThread remote_thread
#define WaitForSingleObject wait_handle
#define CloseHandle close_handle
#define Sleep sleep_for
#define GetTickCount64 ticks
#include "../tools/inject.cpp"
#undef CloseHandle
#undef GetTickCount64

namespace {
/** Build a process-specific native line using the production log format. */
std::string event(const char* message, DWORD pid = 42) {
  return "12:00:00.000 [pid=" + std::to_string(pid) + " tid=7] " + message + "\n";
}
/** Reset every external failure mode and supply a relocated system module. */
void reset() {
  allocationFails = writeFails = shortWrite = threadFails = exitTarget = wrongMappedPath = false;
  wrongTarget = wrongArchitecture = denyOpen = truncateLog = snapshotFails = exitAfterThread = false;
  mapDll = true; threadWait = WAIT_OBJECT_0; processCount = 1;
  frees = closes = allocations = threadStarts = 0; elapsed += 100000;
  emitted = event("ModHost loading") + event("ModHost active"); delayed.clear();
  fakeModules.clear();
  HMODULE owner = nullptr;
  auto address = GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  CHECK(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(address), &owner));
  MODULEENTRY32W module{sizeof module};
  CHECK(GetModuleFileNameW(owner, module.szExePath, MAX_PATH) > 0);
  wcscpy_s(module.szModule, wcsrchr(module.szExePath, L'\\') + 1);
  module.modBaseAddr = reinterpret_cast<BYTE*>(owner) + 0x100000; module.modBaseSize = 0x10000000;
  fakeModules.push_back(module); write_log("", L"wb");
}
}

/** Exercise actual loader control flow through simulated operating-system boundaries. */
int main() {
  wchar_t temporary[MAX_PATH], root[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary)); CHECK(GetTempFileNameW(temporary, L"pgi", 0, root));
  CHECK(DeleteFileW(root)); CHECK(CreateDirectoryW(root, nullptr));
  CHECK(SetEnvironmentVariableW(L"LOCALAPPDATA", root));
  std::wstring directory = std::wstring(root) + L"\\PhasmoMods";
  CHECK(CreateDirectoryW(directory.c_str(), nullptr)); fakeLogPath = directory + L"\\mods.log";
  std::wstring source = std::wstring(root) + L"\\ModHost.dll";
  HANDLE file = CreateFileW(source.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
  CHECK(file != INVALID_HANDLE_VALUE); CHECK(CloseHandle(file));
  std::wstring mod = std::wstring(root) + L"\\Example.dll";
  file = CreateFileW(mod.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr);
  CHECK(file != INVALID_HANDLE_VALUE); CHECK(CloseHandle(file));
  using pg::loader::Outcome;
  auto run = [&] { return pg::loader::load(42, source.c_str()); };
  reset(); CHECK(run().outcome == Outcome::active); CHECK(frees == 1 && threadStarts == 1 && closes == 4);
  CHECK(wcsstr(copiedPath.c_str(), L"\\bin\\host-42-") != nullptr);
  // The selected mods are copied and listed, in order, in the session file the host reads.
  reset(); CHECK(pg::loader::load(42, source.c_str(), {mod}).outcome == Outcome::active);
  {
    FILE* session = nullptr;
    CHECK(_wfopen_s(&session, (directory + L"\\bin\\session-42.txt").c_str(), L"rb") == 0);
    CHECK(session != nullptr);
    char text[2048] = {};
    if (session) { fread(text, 1, sizeof text - 1, session); fclose(session); }
    CHECK(std::strstr(text, "\\bin\\Example-42-") != nullptr && std::strstr(text, ".dll\n") != nullptr);
  }
  reset(); CHECK(pg::loader::load(42, source.c_str(), {std::wstring(root) + L"\\Missing.dll"}).outcome == Outcome::file_error);
  CHECK(threadStarts == 0);
  reset(); allocationFails = true; CHECK(run().outcome == Outcome::allocation_failed); CHECK(frees == 0);
  reset(); writeFails = true; CHECK(run().outcome == Outcome::write_failed); CHECK(frees == 1 && threadStarts == 0);
  reset(); shortWrite = true; CHECK(run().outcome == Outcome::write_failed); CHECK(frees == 1 && threadStarts == 0);
  reset(); threadFails = true; CHECK(run().outcome == Outcome::thread_failed); CHECK(frees == 1);
  reset(); threadWait = WAIT_TIMEOUT; CHECK(run().outcome == Outcome::indeterminate); CHECK(frees == 0 && closes == 3);
  reset(); threadWait = WAIT_FAILED; CHECK(run().outcome == Outcome::indeterminate); CHECK(frees == 0 && closes == 3);
  reset(); exitTarget = true; CHECK(run().outcome == Outcome::game_exited); CHECK(threadStarts == 0);
  reset(); exitAfterThread = true; CHECK(run().outcome == Outcome::game_exited); CHECK(frees == 0);
  reset(); exitAfterThread = true; threadWait = WAIT_TIMEOUT; CHECK(run().outcome == Outcome::game_exited); CHECK(frees == 0);
  reset(); mapDll = false; CHECK(run().outcome == Outcome::not_mapped);
  reset(); wrongMappedPath = true; CHECK(run().outcome == Outcome::not_mapped);
  reset(); wrongTarget = true; CHECK(run().outcome == Outcome::invalid_target); CHECK(allocations == 0);
  reset(); wrongArchitecture = true; CHECK(run().outcome == Outcome::wrong_architecture); CHECK(allocations == 0);
  reset(); denyOpen = true; CHECK(run().error == ERROR_ACCESS_DENIED); CHECK(allocations == 0);
  reset(); wcscpy_s(fakeModules[0].szModule, L"host-existing.dll"); CHECK(run().outcome == Outcome::already_loaded); CHECK(allocations == 0);
  reset(); emitted = event("ModHost loading") + event("ModHost inactive: test"); CHECK(run().outcome == Outcome::inactive);
  reset(); emitted = event("ModHost loading") + event("ModHost stopped; native module retained until process exit"); CHECK(run().outcome == Outcome::inactive);
  reset(); emitted = event("ModHost loading") + event("ModHost inactive: unsupported or unreadable game binaries; nothing hooked"); CHECK(run().outcome == Outcome::unsupported_build);
  reset(); emitted = event("ModHost active"); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted = event("ModHost loading", 99) + event("ModHost active", 99); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); write_log(event("ModHost loading") + event("ModHost active")); emitted.clear(); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted.pop_back(); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted.pop_back(); delayed = "\n"; CHECK(run().outcome == Outcome::active);
  reset(); write_log(std::string(4096, 'x')); truncateLog = true; CHECK(run().outcome == Outcome::log_error);
  reset(); write_log("stale partial line"); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); write_log(std::string(32, 'x')); truncateLog = true; CHECK(run().outcome == Outcome::log_error);
  reset(); DWORD pid = 0; CHECK(pg::loader::find_game(pid).outcome == Outcome::ready && pid == 42);
  processCount = 0; CHECK(pg::loader::find_game(pid).outcome == Outcome::no_process && pid == 0);
  processCount = 2; CHECK(pg::loader::find_game(pid).outcome == Outcome::ambiguous_process && pid == 0);
  snapshotFails = true; CHECK(pg::loader::find_game(pid).outcome == Outcome::process_error);
  return failures ? 1 : 0;
}
