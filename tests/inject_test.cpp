#include <windows.h>
#include <tlhelp32.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "../core/pack_files.h"

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
int badLengthSnapshots = 0;  // module snapshots that fail with ERROR_BAD_LENGTH, as Windows does while the target loads modules
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
HANDLE WINAPI create_snapshot(DWORD flags, DWORD) {
  if (snapshotFails) { SetLastError(ERROR_ACCESS_DENIED); return INVALID_HANDLE_VALUE; }
  if ((flags & TH32CS_SNAPMODULE) && badLengthSnapshots > 0) {
    --badLengthSnapshots;
    SetLastError(ERROR_BAD_LENGTH);
    return INVALID_HANDLE_VALUE;
  }
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

#define PM_LOADER_NO_MAIN
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
#include "../tools/session_sweep.cpp"  // its process views come from the same fakes
#undef CloseHandle
#undef GetTickCount64

namespace {
/** Build a process-specific native line using the production log format, which names the line's level (core/log.h). */
std::string event(const char* message, DWORD pid = 42, const char* level = "INFO ") {
  return "12:00:00.000 [pid=" + std::to_string(pid) + " tid=7] " + level + " " + message + "\n";
}
/** The same line as a host before log levels wrote it, with no level. */
std::string unlevelled_event(const char* message, DWORD pid = 42) {
  return "12:00:00.000 [pid=" + std::to_string(pid) + " tid=7] " + message + "\n";
}
/** The per-process asset folders (*.assets) the launcher has made in bin. */
std::vector<std::wstring> assets_copies(const std::wstring& directory) {
  std::vector<std::wstring> found;
  WIN32_FIND_DATAW entry{};
  HANDLE search = FindFirstFileW((directory + L"\\bin\\*.assets").c_str(), &entry);
  if (search == INVALID_HANDLE_VALUE) return found;
  do { found.push_back(directory + L"\\bin\\" + entry.cFileName); } while (FindNextFileW(search, &entry));
  FindClose(search);
  return found;
}
/** Write a small file. */
void write_text(const std::wstring& path, const char* text) {
  FILE* f = nullptr;
  CHECK(_wfopen_s(&f, path.c_str(), L"wb") == 0);
  if (f) { fputs(text, f); fclose(f); }
}
/** A file's whole content, empty when it cannot be read. */
std::string read_text(const std::wstring& path) {
  FILE* f = nullptr;
  std::string content;
  if (_wfopen_s(&f, path.c_str(), L"rb") != 0 || !f) return content;
  char buffer[1024]; size_t n;
  while ((n = fread(buffer, 1, sizeof buffer, f)) > 0) content.append(buffer, n);
  fclose(f);
  return content;
}
/** The entries under bin matching `pattern`, as full paths. */
std::vector<std::wstring> bin_copies(const std::wstring& directory, const std::wstring& pattern) {
  std::vector<std::wstring> found;
  WIN32_FIND_DATAW entry{};
  HANDLE search = FindFirstFileW((directory + L"\\bin\\" + pattern).c_str(), &entry);
  if (search == INVALID_HANDLE_VALUE) return found;
  do { found.push_back(directory + L"\\bin\\" + entry.cFileName); } while (FindNextFileW(search, &entry));
  FindClose(search);
  return found;
}
/** A file's size in bytes, or -1 when there is no such file. */
long long file_size(const std::wstring& path) {
  WIN32_FILE_ATTRIBUTE_DATA data{};
  if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) return -1;
  return (static_cast<long long>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
}
/** Reset every external failure mode and supply a relocated system module. */
void reset() {
  allocationFails = writeFails = shortWrite = threadFails = exitTarget = wrongMappedPath = false;
  wrongTarget = wrongArchitecture = denyOpen = truncateLog = snapshotFails = exitAfterThread = false;
  mapDll = true; threadWait = WAIT_OBJECT_0; processCount = 1;
  frees = closes = allocations = threadStarts = 0; elapsed += 100000; badLengthSnapshots = 0;
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
  {  // the loader's shipped text (core/text_table.h), so the messages read as players see them
    std::string why;
    if (!pm::i18n::table().load(std::filesystem::path(PM_TEXT_DIR) / "i18n" / "default.json", why)) std::printf("FAIL the loader's text: %s\n", why.c_str());
  }
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
  using pm::loader::Outcome;
  auto run = [&] { return pm::loader::load(42, source.c_str()); };
  // One more snapshot handle than before the sweep: the sweep takes the process list on every load (tools/session_sweep.h).
  reset(); CHECK(run().outcome == Outcome::active); CHECK(frees == 1 && threadStarts == 1 && closes == 5);
  CHECK(wcsstr(copiedPath.c_str(), L"\\bin\\host-42-") != nullptr);
  // The selected mods are copied and listed, in order, in the session file the host reads.
  {
    FILE* json = nullptr;
    CHECK(_wfopen_s(&json, (std::wstring(root) + L"\\manifest.json").c_str(), L"wb") == 0);
    if (json) { fputs("{}", json); fclose(json); }
  }
  reset(); CHECK(pm::loader::load(42, source.c_str(), {mod}, {"?broken\tIts id is bad.\n"}).outcome == Outcome::active);
  {
    // The manifest goes beside the copy as <copy>.json, the name the host looks for.
    WIN32_FIND_DATAW found{};
    HANDLE search = FindFirstFileW((directory + L"\\bin\\Example-42-*.json").c_str(), &found);
    CHECK(search != INVALID_HANDLE_VALUE);
    if (search != INVALID_HANDLE_VALUE) FindClose(search);
  }
  {
    FILE* session = nullptr;
    CHECK(_wfopen_s(&session, (directory + L"\\bin\\session-42.txt").c_str(), L"rb") == 0);
    CHECK(session != nullptr);
    char text[2048] = {};
    if (session) { fread(text, 1, sizeof text - 1, session); fclose(session); }
    CHECK(std::strstr(text, "\\bin\\Example-42-") != nullptr && std::strstr(text, ".dll\n") != nullptr);
    CHECK(std::strstr(text, "?broken\tIts id is bad.\n") != nullptr);  // a refused mod reaches the host's Mods panel
  }
  // A mod whose DLL cannot be read stays off, with a line for the Mods panel; the host and the other mods still load.
  reset(); CHECK(pm::loader::load(42, source.c_str(), {std::wstring(root) + L"\\custom_ghosts\\Missing.dll", mod}).outcome == Outcome::active);
  {
    FILE* session = nullptr;
    CHECK(_wfopen_s(&session, (directory + L"\\bin\\session-42.txt").c_str(), L"rb") == 0);
    char text[2048] = {};
    if (session) { fread(text, 1, sizeof text - 1, session); fclose(session); }
    // Its row names the mod's folder, as the launcher's other refusals do, and not the DLL.
    CHECK(std::strstr(text, "?custom_ghosts\tMod DLL could not be read or copied.") != nullptr);
    CHECK(std::strstr(text, "?Missing") == nullptr);
    CHECK(std::strstr(text, "\\bin\\Example-42-") != nullptr);
    CHECK(std::strstr(text, "\\bin\\Missing-42-") == nullptr);
  }
  // A mod with no assets folder copies as before: no assets folder appears beside its copy.
  CHECK(assets_copies(directory).empty());
  // A mod's assets folder is copied beside its DLL copy as <copy>.assets, every file, nested folders kept.
  {
    const std::wstring assets = std::wstring(root) + L"\\assets";
    CHECK(CreateDirectoryW(assets.c_str(), nullptr));
    CHECK(CreateDirectoryW((assets + L"\\squatter").c_str(), nullptr));
    CHECK(CreateDirectoryW((assets + L"\\squatter\\voice").c_str(), nullptr));
    write_text(assets + L"\\top.txt", "top");
    write_text(assets + L"\\squatter\\voice\\line.wav", "wave");
    write_text(assets + L"\\squatter\\.gitkeep", "");
    reset(); CHECK(pm::loader::load(42, source.c_str(), {mod}).outcome == Outcome::active);
    const auto copies = assets_copies(directory);
    CHECK(copies.size() == 1);
    if (!copies.empty()) {
      CHECK(read_text(copies[0] + L"\\top.txt") == "top");
      CHECK(read_text(copies[0] + L"\\squatter\\voice\\line.wav") == "wave");
      CHECK(GetFileAttributesW((copies[0] + L"\\squatter\\.gitkeep").c_str()) != INVALID_FILE_ATTRIBUTES);
      // It is named after the DLL copy, as the manifest and the string table are, and the session lists that copy.
      const std::wstring name = copies[0].substr(copies[0].rfind(L'\\') + 1);
      const std::string stem = pm::text::narrow(name.substr(0, name.size() - 7));
      CHECK(read_text(directory + L"\\bin\\session-42.txt").find(stem + ".dll\n") != std::string::npos);
    }
    // A file that cannot be copied leaves that mod off with a reason, as a failed DLL copy does; the others still start.
    const std::wstring held = assets + L"\\squatter\\voice\\line.wav";
    const size_t before = assets_copies(directory).size();
    HANDLE lock = CreateFileW(held.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(lock != INVALID_HANDLE_VALUE);
    reset(); CHECK(pm::loader::load(42, source.c_str(), {mod}).outcome == Outcome::active);
    CloseHandle(lock);
    const std::string refused = read_text(directory + L"\\bin\\session-42.txt");
    CHECK(refused.find("\\bin\\Example-42-") == std::string::npos);  // not listed to run
    CHECK(refused.find(pm::i18n::text("launcher.mod-assets-unreadable")) != std::string::npos);
    CHECK(assets_copies(directory).size() == before);  // the failed copy leaves nothing behind
    // A junction inside the folder is never followed out of it: the mod is off.
    const std::wstring outside = std::wstring(root) + L"\\outside";
    CHECK(CreateDirectoryW(outside.c_str(), nullptr));
    write_text(outside + L"\\secret.txt", "secret");
    const std::wstring shell = L"cmd /c mklink /J \"" + assets + L"\\link\" \"" + outside + L"\" >nul";
    if (_wsystem(shell.c_str()) == 0) {
      reset(); CHECK(pm::loader::load(42, source.c_str(), {mod}).outcome == Outcome::active);
      const std::string linked = read_text(directory + L"\\bin\\session-42.txt");
      CHECK(linked.find("\\bin\\Example-42-") == std::string::npos);
      CHECK(linked.find(pm::i18n::text("launcher.mod-assets-unreadable")) != std::string::npos);
      for (const auto& copy : assets_copies(directory)) CHECK(GetFileAttributesW((copy + L"\\link\\secret.txt").c_str()) == INVALID_FILE_ATTRIBUTES);
      RemoveDirectoryW((assets + L"\\link").c_str());  // removes the junction, not its target
    } else {
      std::printf("FAIL: the junction could not be made, so the escape check did not run\n");
      ++failures;
    }
    CHECK(GetFileAttributesW((outside + L"\\secret.txt").c_str()) != INVALID_FILE_ATTRIBUTES);
  }
  // A content pack's stand-in path (core/pack_files.h) names no file to copy: it copies as an empty ".pack"
  // named like a DLL copy, its manifest beside it as <copy minus extension>.json, its string table as
  // <...>.i18n.json, its assets folder as <...>.assets, and the session lists that ".pack" copy.
  {
    const std::wstring packFolder = std::wstring(root) + L"\\EmotePack";
    CHECK(CreateDirectoryW(packFolder.c_str(), nullptr));
    CHECK(CreateDirectoryW((packFolder + L"\\i18n").c_str(), nullptr));
    CHECK(CreateDirectoryW((packFolder + L"\\assets").c_str(), nullptr));
    write_text(packFolder + L"\\manifest.json",
      R"({"id":"someone.emote-pack","name":"Emote Pack","author":"Someone","version":"1.0.0","description":"D","contentPackFor":{"id":"me.example"}})");
    write_text(packFolder + L"\\i18n\\default.json", R"({"emote.one":"One"})");
    write_text(packFolder + L"\\assets\\one.txt", "one");
    const std::wstring standIn = pm::packfiles::stand_in(std::filesystem::path(packFolder)).wstring();
    CHECK(pm::packfiles::is_stand_in(standIn));
    reset();
    CHECK(pm::loader::load(42, source.c_str(), {standIn}).outcome == Outcome::active);
    const std::vector<std::wstring> packs = bin_copies(directory, L"EmotePack-42-*.pack");
    CHECK(packs.size() == 1);
    if (!packs.empty()) {
      const std::wstring& packCopy = packs[0];
      CHECK(file_size(packCopy) == 0);  // the stand-in copies as an empty file, not as a DLL
      const std::wstring stem = packCopy.substr(0, packCopy.size() - 5);  // without ".pack"
      CHECK(read_text(stem + L".json").find("contentPackFor") != std::string::npos);
      CHECK(read_text(stem + L".i18n.json") == R"({"emote.one":"One"})");
      CHECK(read_text(stem + L".assets\\one.txt") == "one");
      // The session lists the ".pack" copy itself, and no DLL copy was made for the pack.
      CHECK(read_text(directory + L"\\bin\\session-42.txt").find(pm::text::narrow(packCopy)) != std::string::npos);
    }
    CHECK(bin_copies(directory, L"EmotePack-42-*.dll").empty());
    CHECK(GetFileAttributesW(standIn.c_str()) == INVALID_FILE_ATTRIBUTES);  // the launcher creates nothing in the pack's folder
  }
  // What an earlier game left in bin goes at the next load: every copy name carries the game's process id (the DLL, the
  // manifest and string-table sidecars, the .assets folder, the session file), and a name whose game has closed belongs
  // to nobody. A game from an earlier launch that is still running keeps everything it has loaded.
  {
    const std::wstring bin = directory + L"\\bin";
    write_text(bin + L"\\host-111-2-0.dll", "stale");
    write_text(bin + L"\\Example-111-2-0.json", "{}");
    write_text(bin + L"\\Example-111-2-0.i18n.json", "{}");
    write_text(bin + L"\\session-111.txt", "stale");
    CHECK(CreateDirectoryW((bin + L"\\Example-111-2-0.assets").c_str(), nullptr));
    write_text(bin + L"\\Example-111-2-0.assets\\gone.txt", "gone");
    write_text(bin + L"\\keepme.txt", "keep");       // a name the launcher never made is nobody's business
    write_text(bin + L"\\host-43-4-0.dll", "game");  // the second running game the fake snapshot reports
    reset(); processCount = 2;
    CHECK(pm::loader::load(42, source.c_str()).outcome == Outcome::active);
    processCount = 1;
    for (const wchar_t* gone : {L"host-111-2-0.dll", L"Example-111-2-0.json", L"Example-111-2-0.i18n.json",
                                L"session-111.txt", L"Example-111-2-0.assets"})
      CHECK(GetFileAttributesW((bin + L"\\" + gone).c_str()) == INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesW((bin + L"\\keepme.txt").c_str()) != INVALID_FILE_ATTRIBUTES);
    CHECK(GetFileAttributesW((bin + L"\\host-43-4-0.dll").c_str()) != INVALID_FILE_ATTRIBUTES);
  }
  // A copy another process holds stays until a later load can remove it: the sweep never fails a load.
  {
    const std::wstring bin = directory + L"\\bin";
    write_text(bin + L"\\Example-555-1-0.dll", "held");
    HANDLE held = CreateFileW((bin + L"\\Example-555-1-0.dll").c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    CHECK(held != INVALID_HANDLE_VALUE);
    reset(); CHECK(run().outcome == Outcome::active);
    CHECK(GetFileAttributesW((bin + L"\\Example-555-1-0.dll").c_str()) != INVALID_FILE_ATTRIBUTES);
    CloseHandle(held);
    reset(); CHECK(run().outcome == Outcome::active);
    CHECK(GetFileAttributesW((bin + L"\\Example-555-1-0.dll").c_str()) == INVALID_FILE_ATTRIBUTES);
  }
  // The shared native log cannot grow without end: past the cap the load moves it aside as mods.log.old, and the
  // activation handshake reads the fresh file. A log under the cap is left alone.
  reset();
  write_log(std::string(static_cast<size_t>(pm::loader::kNativeLogCapBytes) + 1, 'x'));
  CHECK(run().outcome == Outcome::active);
  CHECK(read_text(directory + L"\\mods.log.old").size() == static_cast<size_t>(pm::loader::kNativeLogCapBytes) + 1);
  const std::string freshLog = read_text(fakeLogPath);
  CHECK(freshLog.find('x') == std::string::npos);
  CHECK(freshLog.find("ModHost active") != std::string::npos);
  reset(); CHECK(run().outcome == Outcome::active);
  CHECK(read_text(directory + L"\\mods.log.old").size() == static_cast<size_t>(pm::loader::kNativeLogCapBytes) + 1);
  reset(); CHECK(pm::loader::load(42, nullptr).outcome == Outcome::file_error);
  CHECK(threadStarts == 0);
  reset(); allocationFails = true; CHECK(run().outcome == Outcome::allocation_failed); CHECK(frees == 0);
  reset(); writeFails = true; CHECK(run().outcome == Outcome::write_failed); CHECK(frees == 1 && threadStarts == 0);
  reset(); shortWrite = true; CHECK(run().outcome == Outcome::write_failed); CHECK(frees == 1 && threadStarts == 0);
  reset(); threadFails = true; CHECK(run().outcome == Outcome::thread_failed); CHECK(frees == 1);
  reset(); threadWait = WAIT_TIMEOUT; CHECK(run().outcome == Outcome::indeterminate); CHECK(frees == 0 && closes == 4);
  reset(); threadWait = WAIT_FAILED; CHECK(run().outcome == Outcome::indeterminate); CHECK(frees == 0 && closes == 4);
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
  reset(); emitted = event("ModHost active"); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted = unlevelled_event("ModHost loading") + unlevelled_event("ModHost active"); CHECK(run().outcome == Outcome::active);
  reset(); emitted = event("ModHost loading") + event("ModHost inactive: test", 42, "ERROR"); CHECK(run().outcome == Outcome::inactive);
  reset(); emitted = event("ModHost loading", 99) + event("ModHost active", 99); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); write_log(event("ModHost loading") + event("ModHost active")); emitted.clear(); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted.pop_back(); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); emitted.pop_back(); delayed = "\n"; CHECK(run().outcome == Outcome::active);
  reset(); write_log(std::string(4096, 'x')); truncateLog = true; CHECK(run().outcome == Outcome::log_error);
  reset(); write_log("stale partial line"); CHECK(run().outcome == Outcome::activation_timeout);
  reset(); write_log(std::string(32, 'x')); truncateLog = true; CHECK(run().outcome == Outcome::log_error);
  // A module snapshot taken while the game is still loading modules fails with ERROR_BAD_LENGTH, and Windows' documentation
  // for CreateToolhelp32Snapshot says to take it again. A game just started is such a target: the load goes ahead.
  reset(); badLengthSnapshots = 3; CHECK(run().outcome == Outcome::active); CHECK(threadStarts == 1);
  // It is taken again for a while, not for ever: a snapshot that never comes is the error it always was.
  reset(); badLengthSnapshots = 1 << 30; {
    const auto result = run();
    CHECK(result.outcome == Outcome::process_error && result.error == ERROR_BAD_LENGTH);
    CHECK(threadStarts == 0);
  }
  reset(); DWORD pid = 0; CHECK(pm::loader::find_game(pid).outcome == Outcome::ready && pid == 42);
  processCount = 0; CHECK(pm::loader::find_game(pid).outcome == Outcome::no_process && pid == 0);
  processCount = 2; CHECK(pm::loader::find_game(pid).outcome == Outcome::ambiguous_process && pid == 0);
  snapshotFails = true; CHECK(pm::loader::find_game(pid).outcome == Outcome::process_error);
  return failures ? 1 : 0;
}
