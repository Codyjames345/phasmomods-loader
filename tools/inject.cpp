#include "inject.h"
#include "text_table.h"
#include "mod_assets.h"
#include "pack_files.h"
#include "mod_list.h"
#include "session_sweep.h"
#include "text_files.h"
#include "win_handle.h"
#include <tlhelp32.h>

#include <algorithm>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <vector>

namespace pm::loader {
namespace {
using tools::Handle;

/** Create a typed failure without losing the Windows error to cleanup operations. */
Result fail(Outcome outcome, const std::wstring& detail, DWORD error = GetLastError()) {
  return {outcome, error, detail, {}};
}

/** Return the filename portion for Windows process and module comparisons. */
const wchar_t* filename(const wchar_t* path) {
  const wchar_t* separator = wcsrchr(path, L'\\');
  return separator ? separator + 1 : path;
}

/** Resolve a real file, or with `folder` a real folder, to its normalized DOS path without loading its contents. */
bool canonical_path(const wchar_t* path, std::wstring& output, bool folder = false) {
  Handle file(CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                          nullptr, OPEN_EXISTING, folder ? FILE_FLAG_BACKUP_SEMANTICS : FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE) return false;
  std::vector<wchar_t> buffer(32768);
  DWORD size = GetFinalPathNameByHandleW(file.value, buffer.data(), static_cast<DWORD>(buffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
  if (!size || size >= buffer.size()) { if (size) SetLastError(ERROR_FILENAME_EXCED_RANGE); return false; }
  output.assign(buffer.data(), size);
  return true;
}

/** Obtain a complete module snapshot or reject an incomplete inventory. While the game is still loading modules (a game
 *  started moments ago) Windows refuses the snapshot with ERROR_BAD_LENGTH, and its documentation for
 *  CreateToolhelp32Snapshot says to take it again; this does, for up to five seconds. */
Result list_modules(DWORD pid, std::vector<MODULEENTRY32W>& modules) {
  const ULONGLONG started = GetTickCount64();
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid));
  while (snapshot.value == INVALID_HANDLE_VALUE && GetLastError() == ERROR_BAD_LENGTH && GetTickCount64() - started < 5000) {
    Sleep(50);
    snapshot.value = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);  // the old one was no handle
  }
  if (snapshot.value == INVALID_HANDLE_VALUE) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-inspect-s-modules")));
  MODULEENTRY32W entry{sizeof entry};
  if (!Module32FirstW(snapshot.value, &entry)) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-read-s-modules")));
  do { modules.push_back(entry); } while (Module32NextW(snapshot.value, &entry));
  if (GetLastError() != ERROR_NO_MORE_FILES) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-finish-inspecting-s")));
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
  if (!kernel) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-locate-library-launcher")));
  FARPROC address = GetProcAddress(kernel, "LoadLibraryW");
  if (!address || !GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                     reinterpret_cast<LPCWSTR>(address), &owner))
    return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-resolve-library-launcher")));
  wchar_t path[32768];
  DWORD length = GetModuleFileNameW(owner, path, 32768);
  if (!length || length >= 32768) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-identify-library-launcher")));
  std::wstring localPath;
  if (!canonical_path(path, localPath)) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.windows-read-library-path")));
  auto offset = reinterpret_cast<const BYTE*>(address) - reinterpret_cast<const BYTE*>(owner);
  for (const auto& module : modules) {
    if (_wcsicmp(filename(path), module.szModule)) continue;
    std::wstring remotePath;
    if (!canonical_path(module.szExePath, remotePath)) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.windows-verify-s-system")));
    if (!_wcsicmp(localPath.c_str(), remotePath.c_str()) && offset >= 0 && static_cast<size_t>(offset) < module.modBaseSize) {
      entry = reinterpret_cast<LPTHREAD_START_ROUTINE>(module.modBaseAddr + offset);
      return {};
    }
  }
  return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.s-windows-library-does")), ERROR_MOD_NOT_FOUND);
}

/** Verify process identity and architecture before requesting any remote execution. */
Result check_target(HANDLE process) {
  DWORD wait = WaitForSingleObject(process, 0);
  if (wait == WAIT_OBJECT_0) return fail(Outcome::game_exited, pm::text::widen(pm::i18n::text("launcher.phasmophobia-has-closed-start")), 0);
  if (wait != WAIT_TIMEOUT) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-inspect-process")));
  wchar_t path[32768]; DWORD length = 32768;
  if (!QueryFullProcessImageNameW(process, 0, path, &length)) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-identify-process")));
  if (_wcsicmp(filename(path), L"Phasmophobia.exe")) return fail(Outcome::invalid_target, pm::text::widen(pm::i18n::text("launcher.selected-process-phasmophobia-start")), 0);
  USHORT machine = 0, native = 0;
  if (!IsWow64Process2(process, &machine, &native)) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-check-architecture-launcher")));
  if (machine != IMAGE_FILE_MACHINE_UNKNOWN || native != IMAGE_FILE_MACHINE_AMD64 || sizeof(void*) != 8)
    return fail(Outcome::wrong_architecture, pm::text::widen(pm::i18n::text("launcher.launcher-requires-windows-x64")), 0);
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

/** Make an empty file at a name no earlier copy has used, the copy of a content pack's stand-in (core/pack_files.h): its
 *  manifest, string table and assets are copied beside it as a mod's are beside its DLL's copy. */
bool fresh_stand_in(const std::wstring& directory, const std::wstring& stem, DWORD pid, std::wstring& copy) {
  for (unsigned attempt = 0; attempt < 10; ++attempt) {
    copy = directory + L"\\" + stem + L"-" + std::to_wstring(pid) + L"-" + std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(attempt) + L".pack";
    Handle file(CreateFileW(copy.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if (file.value != INVALID_HANDLE_VALUE) return true;
    if (GetLastError() != ERROR_FILE_EXISTS && GetLastError() != ERROR_ALREADY_EXISTS) break;
  }
  return false;
}

/** A copy's path without its extension (".dll" or ".pack"), which its manifest and string table are named after. */
std::wstring without_extension(const std::wstring& copy) {
  const size_t dot = copy.rfind(L'.');
  const size_t slash = copy.find_last_of(L"\\/");
  return dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash) ? copy : copy.substr(0, dot);
}

/** The file stem of a path, for naming its copy. */
std::wstring stem_of(const std::wstring& path) {
  std::wstring name = filename(path.c_str());
  const size_t dot = name.rfind(L'.');
  return dot == std::wstring::npos ? name : name.substr(0, dot);
}

/** The folder of `file`, with its trailing separator; empty for a bare name. */
std::wstring folder_of(const std::wstring& file) {
  const size_t slash = file.find_last_of(L"\\/");
  return slash == std::wstring::npos ? L"" : file.substr(0, slash + 1);
}

/** Copy a component's string table (its i18n\default.json, core/text_table.h) to <copy>.i18n.json, where its copy reads it. A
 *  component without one shows its keys, and logs which. */
void copy_text(const std::wstring& original, const std::wstring& copy) {
  const std::wstring stem = without_extension(copy);
  CopyFileW((folder_of(original) + L"i18n\\default.json").c_str(), (stem + L".i18n.json").c_str(), FALSE);
}

/** Copy the manifest beside a mod's DLL to <copy>.json, or an old-format mod's mod.ini to <copy>.ini, where the host reads it. */
void copy_manifest(const std::wstring& dll, const std::wstring& copy) {
  const std::wstring folder = folder_of(dll);
  const std::wstring stem = without_extension(copy);
  // A mod with neither, or one whose copy fails, is refused by the host, with why.
  if (!CopyFileW((folder + L"manifest.json").c_str(), (stem + L".json").c_str(), FALSE))
    CopyFileW((folder + L"mod.ini").c_str(), (stem + L".ini").c_str(), FALSE);
}

/** Whether `path` is a link of any kind (a symbolic link or a junction), or cannot be inspected: never followed out of a mod's folder. */
bool is_link_or_unknown(const std::filesystem::path& path) {
  const DWORD attributes = GetFileAttributesW(path.c_str());
  return attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT);
}

/** Copy every file under `from` to `to`, keeping nested folders. False for a link, an entry that is neither file nor folder, a
 *  name that would leave `from`, or any file that cannot be copied; what was copied before stays for the caller to remove. */
bool copy_tree(const std::filesystem::path& from, const std::filesystem::path& to) {
  namespace fs = std::filesystem;
  std::error_code error;
  fs::create_directories(to, error);
  if (error) return false;
  for (fs::directory_iterator it(from, error), end; !error && it != end; it.increment(error)) {
    const fs::path name = it->path().filename();
    if (name.empty() || name == L"." || name == L".." || name.has_parent_path()) return false;
    if (is_link_or_unknown(it->path())) return false;
    const fs::file_status status = it->status(error);
    if (error) return false;
    if (fs::is_directory(status)) {
      if (!copy_tree(it->path(), to / name)) return false;
    } else if (fs::is_regular_file(status)) {
      if (!CopyFileW(it->path().c_str(), (to / name).c_str(), TRUE)) return false;
    } else {
      return false;
    }
  }
  return !error;
}

/** Copy the assets folder beside a mod's DLL to the folder named after its copy (core/mod_assets.h), where the mod finds it
 *  (sdk/api/assets.h). A mod with no assets folder copies nothing and succeeds. False when it has one that cannot be copied whole. */
bool copy_assets(const std::wstring& dll, const std::wstring& copy) {
  const std::filesystem::path from = std::filesystem::path(folder_of(dll)) / L"assets";
  const DWORD attributes = GetFileAttributesW(from.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES) {
    const DWORD error = GetLastError();
    return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;  // none; any other reason it cannot be read is a failure
  }
  if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) || !(attributes & FILE_ATTRIBUTE_DIRECTORY)) return false;
  return copy_tree(from, pm::modassets::beside_copy(copy));
}

/** Remove a mod's copies made so far (its DLL and any assets folder), when it is left off. */
void remove_copy(const std::wstring& copy) {
  std::error_code ignored;
  std::filesystem::remove_all(pm::modassets::beside_copy(copy), ignored);
  DeleteFileW(copy.c_str());
}

/** Write the session file the host reads for this process: one UTF-8 mod path per line, then `extraLines` as they are. */
bool write_session(const std::wstring& directory, DWORD pid, const std::vector<std::wstring>& copies, const std::vector<std::string>& extraLines) {
  std::string text;
  for (const auto& copy : copies) text += pm::text::narrow(copy) + "\n";
  for (const auto& line : extraLines) text += line;
  const std::wstring path = directory + L"\\session-" + std::to_wstring(pid) + L".txt";
  Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (file.value == INVALID_HANDLE_VALUE) return false;
  DWORD written = 0;
  return text.empty() || (WriteFile(file.value, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) && written == text.size());
}

/** The name of the folder holding `path`, as the launcher's other refusals call a mod; the file's stem when it names no folder. */
std::wstring mod_folder_of(const std::wstring& path) {
  const size_t slash = path.find_last_of(L"\\/");
  if (slash == std::wstring::npos || slash == 0) return stem_of(path);
  const size_t before = path.find_last_of(L"\\/", slash - 1);
  const std::wstring folder = path.substr(before == std::wstring::npos ? 0 : before + 1, slash - (before == std::wstring::npos ? 0 : before + 1));
  return folder.empty() || folder.back() == L':' ? stem_of(path) : folder;
}

/** The session file's line for a mod whose DLL the launcher cannot read or copy, which the host's Mods panel shows. */
std::string refused_line(const std::wstring& mod) {
  return modlist::session_refused_line({text::narrow(mod_folder_of(mod)), pm::i18n::text("launcher.mod-dll-unreadable")});
}

/** The session file's line for a mod whose assets folder the launcher cannot copy whole. */
std::string assets_refused_line(const std::wstring& mod) {
  return modlist::session_refused_line({text::narrow(mod_folder_of(mod)), pm::i18n::text("launcher.mod-assets-unreadable")});
}

/** Prepare local storage and make exclusive copies of the host and its mods for this process lifetime. */
Result copy_native(DWORD pid, const wchar_t* host, const std::vector<std::wstring>& mods, const std::vector<std::string>& sessionLines, std::wstring& copy, std::wstring& log) {
  wchar_t base[32768];
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", base, 32768);
  if (!length || length >= 32768) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.local-application-data-folder-2")));
  std::wstring directory = std::wstring(base) + L"\\PhasmoMods";
  if (!ensure_directory(directory)) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.launcher-cannot-create-data")));
  log = directory + L"\\mods.log";
  directory += L"\\bin";
  if (!ensure_directory(directory)) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.launcher-cannot-create-native")));
  sweep_previous_sessions();  // one session's copies outlive no more than its game (tools/session_sweep.h)
  std::vector<std::wstring> modCopies;
  std::vector<std::string> lines = sessionLines;
  for (const auto& mod : mods) {
    std::wstring modCopy;
    // A content pack has no DLL: its copy is an empty stand-in that its files sit beside (core/pack_files.h).
    const bool pack = pm::packfiles::is_stand_in(mod);
    if (pack ? !fresh_stand_in(directory, stem_of(mod), pid, modCopy) : !fresh_copy(directory, mod.c_str(), stem_of(mod), pid, modCopy)) {
      lines.push_back(refused_line(mod));  // that mod stays off, and the others still start
      continue;
    }
    if (!copy_assets(mod, modCopy)) {
      remove_copy(modCopy);
      lines.push_back(assets_refused_line(mod));  // as a failed DLL copy: that mod stays off
      continue;
    }
    copy_manifest(mod, modCopy);
    copy_text(mod, modCopy);
    modCopies.push_back(modCopy);
  }
  if (!write_session(directory, pid, modCopies, lines))
    return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.launcher-cannot-write-list")));
  if (!fresh_copy(directory, host, L"host", pid, copy))
    return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.launcher-cannot-copy-modhost")));
  copy_text(host, copy);
  return {};
}

/** Start and wait for LoadLibraryW without ever freeing a live thread's argument. */
Result map_native(HANDLE process, LPTHREAD_START_ROUTINE entry, const std::wstring& path) {
  SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t), written = 0;
  void* argument = VirtualAllocEx(process, nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
  if (!argument) return fail(Outcome::allocation_failed, pm::text::widen(pm::i18n::text("launcher.windows-reserve-memory-loading")));
  Result result;
  if (!WriteProcessMemory(process, argument, path.c_str(), bytes, &written))
    result = fail(Outcome::write_failed, pm::text::widen(pm::i18n::text("launcher.windows-send-mod-path")));
  else if (written != bytes)
    result = fail(Outcome::write_failed, pm::text::widen(pm::i18n::text("launcher.windows-sent-only-part")), ERROR_PARTIAL_COPY);
  if (result.outcome == Outcome::ready) {
    Handle thread(CreateRemoteThread(process, nullptr, 0, entry, argument, 0, nullptr));
    if (!thread.value) result = fail(Outcome::thread_failed, pm::text::widen(pm::i18n::text("launcher.windows-start-loading-mod")));
    else {
      DWORD wait = WaitForSingleObject(thread.value, 10000);
      if (wait != WAIT_OBJECT_0) {
        DWORD error = wait == WAIT_TIMEOUT ? ERROR_TIMEOUT : GetLastError();
        if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
          return fail(Outcome::game_exited, pm::text::widen(pm::i18n::text("launcher.phasmophobia-closed-while-loading")), 0);
        return fail(Outcome::indeterminate, pm::text::widen(pm::i18n::text("launcher.loading-has-finished-before")), error);
      }
      if (WaitForSingleObject(process, 0) == WAIT_OBJECT_0)
        return fail(Outcome::game_exited, pm::text::widen(pm::i18n::text("launcher.phasmophobia-closed-while-loading")), 0);
    }
  }
  if (!VirtualFreeEx(process, argument, 0, MEM_RELEASE) && result.outcome == Outcome::ready)
    return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-finish-cleaning-up")));
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
  // The line's level, five letters wide and a space (core/log.h's level_word); a host before log levels wrote none.
  for (const char* level : {"TRACE ", "DEBUG ", "INFO  ", "WARN  ", "ERROR ", "ALERT "})
    if (message.starts_with(level)) {
      message.erase(0, 6);
      break;
    }
  if (message == "ModHost loading") loading = true;
  if (!loading) return Outcome::ready;
  if (message == "ModHost active") return Outcome::active;
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
  if (!GetFileSizeEx(log, &size)) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-cannot-read")));
  if (size.QuadPart < cursor.offset || size.QuadPart - cursor.offset > 1024 * 1024)
    return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-changed-unexpectedly")), ERROR_INVALID_DATA);
  std::string anchor;
  if (!read_anchor(log, cursor.offset, anchor)) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-cannot-read-2")));
  if (anchor != cursor.anchor) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-was-rewritten")), ERROR_INVALID_DATA);
  Outcome state = Outcome::ready;
  while (cursor.offset < size.QuadPart) {
    char bytes[4096]; DWORD count = 0;
    DWORD wanted = static_cast<DWORD>(std::min<LONGLONG>(sizeof bytes, size.QuadPart - cursor.offset));
    if (!ReadFile(log, bytes, wanted, &count, nullptr)) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-cannot-read-2")));
    if (!count) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-was-truncated")), ERROR_HANDLE_EOF);
    cursor.offset += count; cursor.pending.append(bytes, count);
    size_t end;
    while ((end = cursor.pending.find('\n')) != std::string::npos) {
      Outcome next = cursor.skipLine ? Outcome::ready : parse_line(cursor.pending.substr(0, end), pid, cursor.loading);
      cursor.skipLine = false;
      cursor.pending.erase(0, end + 1);
      if (next == Outcome::inactive) state = next;
      else if (next == Outcome::active && state == Outcome::ready) state = next;
    }
    if (cursor.pending.size() > 4096) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-contains-invalid")), ERROR_INVALID_DATA);
  }
  if (!read_anchor(log, cursor.offset, cursor.anchor)) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-cannot-read-2")));
  if (state == Outcome::active) return {state, 0, pm::text::widen(pm::i18n::text("launcher.running-answer-notice-start")), {}};
  if (state == Outcome::inactive) return fail(state, pm::text::widen(pm::i18n::text("launcher.mods-activate-check-native")), 0);
  return {};
}

/** Wait at most 65 seconds for activation, keeping game exit distinct from log failure. */
Result wait_activation(HANDLE process, HANDLE log, LogCursor cursor, DWORD pid) {
  ULONGLONG started = GetTickCount64();
  do {
    DWORD wait = WaitForSingleObject(process, 0);
    if (wait == WAIT_OBJECT_0) return fail(Outcome::game_exited, pm::text::widen(pm::i18n::text("launcher.phasmophobia-closed-before-mod")), 0);
    if (wait != WAIT_TIMEOUT) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-check-process")));
    Result result = read_activation(log, cursor, pid);
    if (result.outcome != Outcome::ready) return result;
    Sleep(100);
  } while (GetTickCount64() - started < 65000);
  return fail(Outcome::activation_timeout, pm::text::widen(pm::i18n::text("launcher.mods-did-confirm-activation")), ERROR_TIMEOUT);
}
}  // namespace

Result find_game(DWORD& pid) {
  pid = 0;
  Handle snapshot(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
  if (snapshot.value == INVALID_HANDLE_VALUE) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-list-running-games")));
  PROCESSENTRY32W entry{sizeof entry}; DWORD candidate = 0; unsigned count = 0;
  BOOL more = Process32FirstW(snapshot.value, &entry);
  while (more) {
    if (!_wcsicmp(entry.szExeFile, L"Phasmophobia.exe")) { candidate = entry.th32ProcessID; ++count; }
    more = Process32NextW(snapshot.value, &entry);
  }
  if (GetLastError() != ERROR_NO_MORE_FILES) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-finish-listing-games")));
  if (!count) return fail(Outcome::no_process, pm::text::widen(pm::i18n::text("launcher.phasmophobia-running-start-through")), 0);
  if (count != 1) return fail(Outcome::ambiguous_process, pm::text::widen(pm::i18n::text("launcher.more-than-one-phasmophobia")), 0);
  pid = candidate;
  return {};
}

Result load(DWORD pid, const wchar_t* hostDllPath, const std::vector<std::wstring>& modDllPaths, const std::vector<std::string>& sessionLines) {
  if (!hostDllPath || !*hostDllPath) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.modhost-dll-missing-extract-2")), ERROR_INVALID_PARAMETER);
  std::wstring source;
  if (!canonical_path(hostDllPath, source)) return fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.modhost-dll-cannot-read")));
  std::vector<std::wstring> mods;
  std::vector<std::string> lines = sessionLines;
  for (const auto& mod : modDllPaths) {
    std::wstring canonical;
    // A content pack's stand-in names no file (core/pack_files.h): its folder is what must be real.
    const std::filesystem::path path(mod);
    if (pm::packfiles::is_stand_in(path)) {
      if (canonical_path(path.parent_path().c_str(), canonical, true)) mods.push_back(canonical + L"\\" + path.filename().wstring());
      else lines.push_back(refused_line(mod));
    } else if (canonical_path(mod.c_str(), canonical)) mods.push_back(canonical);
    else lines.push_back(refused_line(mod));  // that mod stays off, and the others still start
  }
  constexpr DWORD rights = PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION | PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ | SYNCHRONIZE;
  Handle process(OpenProcess(rights, FALSE, pid));
  if (!process.value) return fail(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.windows-denied-access-phasmophobia")));
  Result result = check_target(process.value);
  if (result.outcome != Outcome::ready) return result;
  std::vector<MODULEENTRY32W> modules;
  result = list_modules(pid, modules);
  if (result.outcome != Outcome::ready) return result;
  if (already_loaded(modules)) return fail(Outcome::already_loaded, pm::text::widen(pm::i18n::text("launcher.already-running-change-which")), 0);
  LPTHREAD_START_ROUTINE entry = nullptr;
  result = resolve_loader(modules, entry);
  if (result.outcome != Outcome::ready) return result;
  std::wstring copy, logPath;
  result = copy_native(pid, source.c_str(), mods, lines, copy, logPath);
  if (result.outcome != Outcome::ready) return result;
  Handle log(CreateFileW(logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr));
  if (log.value == INVALID_HANDLE_VALUE) return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-cannot-opened")));
  LARGE_INTEGER offset;
  LogCursor cursor;
  if (!GetFileSizeEx(log.value, &offset) || !read_anchor(log.value, offset.QuadPart, cursor.anchor))
    return fail(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.native-log-position-cannot")));
  cursor.offset = offset.QuadPart;
  cursor.skipLine = !cursor.anchor.empty() && cursor.anchor.back() != '\n';
  result = map_native(process.value, entry, copy);
  if (result.outcome == Outcome::ready) {
    modules.clear(); result = list_modules(pid, modules);
    if (result.outcome == Outcome::ready) {
      std::wstring expected; bool mapped = false;
      if (!canonical_path(copy.c_str(), expected)) result = fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.copied-native-library-cannot")));
      else {
        for (const auto& module : modules) {
          if (_wcsicmp(filename(copy.c_str()), module.szModule)) continue;
          std::wstring actual;
          if (!canonical_path(module.szExePath, actual)) { result = fail(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.loaded-native-library-cannot"))); break; }
          if (!_wcsicmp(expected.c_str(), actual.c_str())) mapped = true;
        }
        if (result.outcome == Outcome::ready)
          result = mapped ? wait_activation(process.value, log.value, cursor, pid)
                          : fail(Outcome::not_mapped, pm::text::widen(pm::i18n::text("launcher.windows-did-load-modhost")), ERROR_MOD_NOT_FOUND);
      }
    }
  }
  result.logPath = logPath;
  return result;
}
}  // namespace pm::loader

#ifndef PM_LOADER_NO_MAIN
/** Expose the shared loader for development; changing versions requires a game restart. */
int wmain(int argc, wchar_t** argv) {
  {  // the loader's player-facing text (core/text_table.h), beside this program; without it each message shows its key
    std::string why;
    wchar_t self[32768];
    const DWORD n = GetModuleFileNameW(nullptr, self, 32768);
    if (n && n < 32768) pm::i18n::table().load(pm::i18n::table_beside(self), why);
  }
  if (argc < 3 || _wcsicmp(argv[1], L"inject")) {
    fwprintf(stderr, L"Usage: pminject inject <ModHost.dll> [<mod.dll> ...]\nRestart the game to stop or change versions.\n"); return 2;
  }
  DWORD pid = 0;
  auto result = pm::loader::find_game(pid);
  if (result.outcome == pm::loader::Outcome::ready) result = pm::loader::load(pid, argv[2], {argv + 3, argv + argc});
  fwprintf(result.outcome == pm::loader::Outcome::active ? stdout : stderr, L"%ls (Windows error %lu)\n", result.detail.c_str(), result.error);
  if (!result.logPath.empty()) fwprintf(stderr, L"Native log: %ls\n", result.logPath.c_str());
  return result.outcome == pm::loader::Outcome::active ? 0 : 1;
}
#endif
