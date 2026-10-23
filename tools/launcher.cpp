#include "launcher.h"
#include "inject.h"
#include "text_files.h"
#include "win_handle.h"
#include <windows.h>
#include <shellapi.h>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <fstream>
#include <thread>
#include <utility>

namespace pg::launcher {
namespace fs = std::filesystem;
using loader::Outcome;
using loader::Result;
using tools::Handle;

/** Quote an argument using the Windows backslash-before-quote convention. */
std::wstring quote(std::wstring_view value) {
  std::wstring output = L"\"";
  size_t slashes = 0;
  for (wchar_t character : value) {
    if (character == L'\\') { ++slashes; continue; }
    output.append(slashes * (character == L'\"' ? 2 : 1), L'\\');
    slashes = 0;
    if (character == L'\"') output += L'\\';
    output += character;
  }
  output.append(slashes * 2, L'\\');
  return output + L'\"';
}

/** Join arguments without expanding shell syntax or environment variables. */
std::wstring command_line(const std::vector<std::wstring>& arguments) {
  std::wstring output;
  for (const auto& argument : arguments) {
    if (!output.empty()) output += L' ';
    output += quote(argument);
  }
  return output;
}

/** The Steam launch option: the path is always quoted, because the program's name has a space. */
std::wstring steam_launch_option(const std::wstring& executable) {
  return quote(executable) + L" --steam %command%";
}

/** Require an observable file log while retaining existing argument values. */
std::wstring configure_log(std::vector<std::wstring>& arguments,
                           const std::wstring& freshPath, std::wstring& logPath) {
  logPath.clear();
  for (size_t i = 1; i < arguments.size(); ++i) {
    const auto& argument = arguments[i];
    if (_wcsicmp(argument.c_str(), L"-nolog") == 0 || _wcsnicmp(argument.c_str(), L"-logFile=", 9) == 0)
      return L"Remove -nolog or use -logFile followed by a file path in Steam launch options.";
    if (_wcsicmp(argument.c_str(), L"-logFile") != 0) continue;
    if (!logPath.empty() || ++i == arguments.size() || arguments[i].empty() || arguments[i][0] == L'-')
      return L"Use one -logFile option followed by a file path, or remove it from Steam launch options.";
    logPath = arguments[i];
  }
  if (logPath.empty()) {
    logPath = freshPath;
    arguments.insert(arguments.end(), {L"-logFile", freshPath});
  }
  return {};
}

/** Consume complete bounded lines and reject prefixes that merely mention the menu. */
bool MenuReadiness::feed(std::string_view bytes) {
  constexpr std::string_view marker = "Loaded Level: Main Menu |";
  for (char character : bytes) {
    if (character == '\n') {
      if (!discard_ && pending_.starts_with(marker)) ready_ = true;
      pending_.clear();
      discard_ = false;
    } else if (!discard_) {
      if (pending_.size() < 65536) pending_ += character;
      else { pending_.clear(); discard_ = true; }
    }
  }
  return ready_;
}

/** Create a launcher error using the shared loader's user-facing result shape. */
Result failure(Outcome outcome, const wchar_t* detail, DWORD error = 0) {
  return {outcome, error, detail, {}};
}

using text::read_file;
using text::widen;

std::vector<ModEntry> discover_mods(const fs::path& folder, std::wstring& problems) {
  std::vector<ModEntry> mods;
  problems.clear();
  std::error_code error;
  std::vector<fs::path> folders;
  for (const auto& entry : fs::directory_iterator(folder, error))
    if (entry.is_directory(error)) folders.push_back(entry.path());
  std::sort(folders.begin(), folders.end());
  for (const fs::path& dir : folders) {
    const fs::path ini = dir / L"mod.ini";
    if (!fs::exists(ini, error)) continue;
    ModEntry m;
    std::string why;
    if (!modlist::parse_manifest(read_file(ini), m.manifest, why)) {
      problems += L"The mod in " + dir.filename().wstring() + L" cannot be used. " + widen(why) + L"\r\n";
      continue;
    }
    m.dll = (dir / widen(m.manifest.dll)).wstring();
    if (!fs::exists(m.dll, error)) {
      problems += L"The mod in " + dir.filename().wstring() + L" is missing its DLL. Extract the package again.\r\n";
      continue;
    }
    bool repeated = false;
    for (const ModEntry& other : mods) repeated |= other.manifest.id == m.manifest.id;
    if (repeated) {
      problems += L"The mod in " + dir.filename().wstring() + L" repeats the id of another mod and is left out.\r\n";
      continue;
    }
    mods.push_back(std::move(m));
  }
  return mods;
}

std::vector<std::string> saved_selection(const fs::path& launcherIni, const std::vector<ModEntry>& mods) {
  std::vector<std::string> installed;
  std::vector<modlist::Manifest> manifests;
  for (const ModEntry& m : mods) {
    installed.push_back(m.manifest.id);
    manifests.push_back(m.manifest);
  }
  // A choice saved before a mod gained a requirement still ticks what that mod needs.
  return modlist::with_requirements(modlist::chosen(read_file(launcherIni), installed), manifests);
}

bool save_selection(const fs::path& launcherIni, const std::vector<std::string>& ids) {
  std::error_code error;
  fs::create_directories(launcherIni.parent_path(), error);
  std::ofstream out(launcherIni, std::ios::binary | std::ios::trunc);
  out << modlist::write_enabled(ids);
  return static_cast<bool>(out);
}

std::vector<std::wstring> all_dlls(const std::vector<ModEntry>& mods) {
  std::vector<std::wstring> dlls;
  for (const ModEntry& m : mods) dlls.push_back(m.dll);
  return dlls;
}

/** %LOCALAPPDATA%\PhasmoMods\launcher.ini, or empty when the folder is unavailable. */
fs::path selection_file() {
  std::wstring local(32768, L'\0');
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
  if (!length || length >= local.size()) return {};
  local.resize(length);
  return fs::path(local) / L"PhasmoMods" / L"launcher.ini";
}

/** Resolve the packaged host beside the launcher, independent of Steam's directory. */
Result adjacent_host(std::wstring& path, std::wstring& executable) {
  executable.assign(32768, L'\0');
  DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
  if (!length || length >= executable.size())
    return failure(Outcome::file_error, L"The location of PhasmoMods Loader could not be read. Extract the package again.", GetLastError());
  executable.resize(length);
  path = (fs::path(executable).parent_path() / L"ModHost.dll").wstring();
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    return failure(Outcome::file_error, L"ModHost.dll is missing. Extract the complete package beside PhasmoMods Loader.", GetLastError());
  return {Outcome::ready, 0, {}, {}};
}

/** Serialize loading for this process without holding a machine-wide launch lock. */
Result load_once(DWORD pid, const Package& package) {
  const auto name = L"Local\\PhasmoModLauncher-" + std::to_wstring(pid);
  Handle lock(CreateMutexW(nullptr, FALSE, name.c_str()));
  DWORD error = GetLastError();
  if (!lock.value)
    return failure(Outcome::process_error, L"The mod loading lock could not be created. Close other copies of PhasmoMods Loader and try again.", error);
  if (error == ERROR_ALREADY_EXISTS)
    return failure(Outcome::already_loaded, L"Another copy of PhasmoMods Loader is loading this game. Wait for it to finish.");
  return loader::load(pid, package.host.c_str(), package.mods);
}

/** Attach only to the single game instance selected by the shared identity check. */
Result attach(const Package& package) {
  if (package.mods.empty())
    return failure(Outcome::invalid_target, L"No mods were found. Extract the complete package, with its mods folder, beside PhasmoMods Loader.");
  DWORD pid = 0;
  auto result = loader::find_game(pid);
  return result.outcome == Outcome::ready ? load_once(pid, package) : result;
}

/** Track only bytes produced after this launch, including file replacement and truncation. */
struct LogCursor {
  std::wstring path;
  ULONGLONG offset = 0;
  DWORD volume = 0, high = 0, low = 0;
  bool existed = false;
  MenuReadiness parser;
};

/** Read a bounded slice of fresh game output, retaining incomplete lines between polls. */
Result read_menu(LogCursor& cursor, bool baseline = false) {
  Handle file(CreateFileW(cursor.path.c_str(), GENERIC_READ,
    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, 0, nullptr));
  if (file.value == INVALID_HANDLE_VALUE) {
    const DWORD error = GetLastError();
    if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
      return {Outcome::ready, 0, {}, {}};
    return failure(Outcome::log_error, L"The game log could not be read. Check its path and access permissions.", error);
  }
  BY_HANDLE_FILE_INFORMATION information{};
  if (!GetFileInformationByHandle(file.value, &information))
    return failure(Outcome::log_error, L"The game log could not be inspected. Check its access permissions.", GetLastError());
  const ULONGLONG size = (ULONGLONG(information.nFileSizeHigh) << 32) | information.nFileSizeLow;
  const bool changed = !cursor.existed || cursor.volume != information.dwVolumeSerialNumber ||
    cursor.high != information.nFileIndexHigh || cursor.low != information.nFileIndexLow || size < cursor.offset;
  if (changed) { cursor.offset = 0; cursor.parser = MenuReadiness(); }
  cursor.existed = true;
  cursor.volume = information.dwVolumeSerialNumber;
  cursor.high = information.nFileIndexHigh;
  cursor.low = information.nFileIndexLow;
  if (baseline) cursor.offset = size ? size - 1 : 0;
  LARGE_INTEGER position{};
  position.QuadPart = cursor.offset;
  if (!SetFilePointerEx(file.value, position, nullptr, FILE_BEGIN))
    return failure(Outcome::log_error, L"The game log could not be read. Restart the game and try again.", GetLastError());
  char buffer[16384];
  DWORD read = 0;
  for (size_t total = 0; total < 1024 * 1024; total += read) {
    if (!ReadFile(file.value, buffer, baseline ? 1 : sizeof(buffer), &read, nullptr))
      return failure(Outcome::log_error, L"The game log read failed. Restart the game and try again.", GetLastError());
    cursor.offset += read;
    if (baseline) { cursor.parser = MenuReadiness(read && buffer[0] != '\n'); break; }
    if (!read) break;
    if (cursor.parser.feed(std::string_view(buffer, read))) return {Outcome::active, 0, {}, {}};
  }
  return {Outcome::ready, 0, {}, {}};
}

/** Reserve an empty unique game log without changing any files in the game directory. */
Result reserve_log(std::wstring& path) {
  std::wstring local(32768, L'\0');
  DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
  if (!length || length >= local.size())
    return failure(Outcome::file_error, L"The local application data folder is unavailable. Check your Windows profile.", GetLastError());
  local.resize(length);
  const auto directory = fs::path(local) / L"PhasmoMods" / L"logs";
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) return failure(Outcome::file_error, L"The game log folder could not be created. Check its access permissions.", error.value());
  path = (directory / (L"game-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
    std::to_wstring(GetTickCount64()) + L".log")).wstring();
  Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr));
  if (file.value == INVALID_HANDLE_VALUE)
    return failure(Outcome::file_error, L"A fresh game log could not be created. Close PhasmoMods Loader and try again.", GetLastError());
  return {Outcome::ready, 0, {}, {}};
}

/** Wait up to five minutes for this child to reach its completed main-menu log marker. */
Result wait_menu(HANDLE process, LogCursor& cursor) {
  const auto deadline = GetTickCount64() + 300000;
  while (GetTickCount64() < deadline) {
    const DWORD state = WaitForSingleObject(process, 0);
    if (state == WAIT_OBJECT_0)
      return failure(Outcome::game_exited, L"Phasmophobia exited before reaching the menu. Check the game log and try a normal Steam launch.");
    if (state != WAIT_TIMEOUT)
      return failure(Outcome::process_error, L"The game process could not be monitored. Restart the game and try again.", GetLastError());
    auto result = read_menu(cursor);
    if (result.outcome != Outcome::ready) return result;
    Sleep(200);
  }
  return failure(Outcome::activation_timeout, L"The main menu was not confirmed within five minutes. Once it appears, open PhasmoMods Loader and choose Attach.");
}

/** Launch exactly Steam's supplied game command and keep its returned process identity. */
Result launch_steam(std::vector<std::wstring> arguments, const Package& package) {
  if (arguments.empty() || !fs::path(arguments[0]).is_absolute() ||
      _wcsicmp(fs::path(arguments[0]).filename().c_str(), L"Phasmophobia.exe") != 0)
    return failure(Outcome::invalid_target, L"The Steam game command is missing or invalid. Copy the launch option from PhasmoMods Loader again.");
  // The named object's lifetime reserves startup before a game PID exists.
  SetLastError(ERROR_SUCCESS);
  Handle startupLock(CreateMutexW(nullptr, FALSE, L"Local\\PhasmoModSteamLauncher"));
  const DWORD lockError = GetLastError();
  if (!startupLock.value)
    return failure(Outcome::process_error, L"The game startup lock could not be created. Close other copies of PhasmoMods Loader and try again.", lockError);
  if (lockError == ERROR_ALREADY_EXISTS)
    return failure(Outcome::already_loaded, L"Another copy of PhasmoMods Loader is starting Phasmophobia. Wait for it to finish.");
  DWORD existingPid = 0;
  auto result = loader::find_game(existingPid);
  if (result.outcome == Outcome::ready)
    return failure(Outcome::already_loaded, L"Phasmophobia is already running. Close it before launching from Steam, or use Attach.");
  if (result.outcome != Outcome::no_process) return result;
  std::wstring freshPath, logPath;
  auto probe = arguments;
  auto error = configure_log(probe, L"", logPath);
  if (!error.empty()) return {Outcome::log_error, 0, error, {}};
  if (logPath.empty()) {
    result = reserve_log(freshPath);
    if (result.outcome != Outcome::ready) return result;
  }
  error = configure_log(arguments, freshPath, logPath);
  if (!error.empty()) return {Outcome::log_error, 0, error, {}};
  LogCursor cursor{logPath};
  result = read_menu(cursor, true);
  if (result.outcome != Outcome::ready) return result;
  auto command = command_line(arguments);
  if (command.size() >= 32767)
    return failure(Outcome::invalid_target, L"The game command is too long. Shorten the Steam launch options.");
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child{};
  if (!CreateProcessW(arguments[0].c_str(), command.data(), nullptr, nullptr, FALSE,
                      0, nullptr, nullptr, &startup, &child))
    return failure(Outcome::process_error, L"Phasmophobia could not start. Check its Steam installation and launch options.", GetLastError());
  Handle process(child.hProcess), thread(child.hThread);
  if (package.mods.empty())
    return {Outcome::active, 0, L"No mods were found beside the launcher, so the game started without them.", logPath};
  result = wait_menu(process.value, cursor);
  if (result.outcome == Outcome::active) result = load_once(child.dwProcessId, package);
  if (result.logPath.empty()) result.logPath = logPath;
  return result;
}

/** Present failure details without confusing a mapped DLL with an activated mod. */
std::wstring describe(const Result& result) {
  std::wstring text = result.outcome == Outcome::active ? (result.detail.empty() ? L"The mods are loaded. Return to the game and answer the notice there." : result.detail) : result.detail;
  if (result.error) text += L"\r\nWindows error: " + std::to_wstring(result.error) + L".";
  if (!result.logPath.empty()) text += L"\r\nLog: " + result.logPath;
  return text;
}

#ifndef PG_LAUNCHER_TEST
constexpr wchar_t kName[] = L"PhasmoMods Loader";      // the message boxes' title
constexpr wchar_t kTitle[] = L"PhasmoMods Loader " PG_WIDEN(PG_SUITE_VERSION);  // the window's title
constexpr int kIcon = 1;                               // tools/launcher.rc

/** Keep the small instruction window alive while one background attach operation finishes. */
struct WindowState {
  Package package;
  std::vector<ModEntry> mods;
  std::vector<std::string> ticked;
  fs::path selectionPath;
  std::vector<HWND> boxes;
  std::wstring launchOption;
  HWND status = nullptr;
  HWND attachButton = nullptr;
  bool busy = false;
  bool closing = false;
  Result result;
  std::atomic<bool> completed{false};
  std::thread worker;
};

/** Attach and the mod checkboxes are usable only while no load is running, so the shown choice is the loaded one. */
void set_controls(WindowState* state, bool on) {
  EnableWindow(state->attachButton, on ? TRUE : FALSE);
  for (HWND box : state->boxes) EnableWindow(box, on ? TRUE : FALSE);
}

/** Copy only the displayed setup command, transferring clipboard ownership on success. */
bool copy_option(HWND window, const std::wstring& text) {
  HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, (text.size() + 1) * sizeof(wchar_t));
  if (!memory) return false;
  void* data = GlobalLock(memory);
  if (!data) { GlobalFree(memory); return false; }
  memcpy(data, text.c_str(), (text.size() + 1) * sizeof(wchar_t));
  GlobalUnlock(memory);
  if (!OpenClipboard(window)) { GlobalFree(memory); return false; }
  bool success = EmptyClipboard() && SetClipboardData(CF_UNICODETEXT, memory);
  CloseClipboard();
  if (!success) GlobalFree(memory);
  return success;
}

/** Dispatch accessible native controls and keep the game operation off the UI thread. */
LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
  if (message == WM_NCCREATE) {
    state = static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
  }
  if (!state) return DefWindowProcW(window, message, wParam, lParam);
  switch (message) {
    case WM_COMMAND:
      if (LOWORD(wParam) == 101 && !state->busy) {
        if (!SetTimer(window, 1, 100, nullptr)) {
          SetWindowTextW(state->status, L"PhasmoMods Loader could not monitor loading. Close it and try again.");
          return 0;
        }
        state->busy = true;
        state->completed.store(false, std::memory_order_relaxed);
        set_controls(state, false);
        SetWindowTextW(state->status, L"Loading the mods. Waiting for the game to confirm...");
        try {
          state->result = failure(Outcome::process_error, L"PhasmoMods Loader could not finish loading. Restart it and try again.");
          state->worker = std::thread([state] {
            try { state->result = attach(state->package); }
            catch (...) { /* Keep the prepared failure without allocating in this path. */ }
            state->completed.store(true, std::memory_order_release);
          });
        } catch (...) {
          KillTimer(window, 1);
          state->busy = false;
          set_controls(state, true);
          SetWindowTextW(state->status, L"The loading task could not start. Close PhasmoMods Loader and try again.");
        }
      } else if (LOWORD(wParam) >= 200 && LOWORD(wParam) < 200 + state->boxes.size() && !state->busy) {
        // A mod is ticked with what it requires and unticked with what requires it.
        const size_t clicked = LOWORD(wParam) - 200;
        const bool on = SendMessageW(state->boxes[clicked], BM_GETCHECK, 0, 0) == BST_CHECKED;
        std::vector<modlist::Manifest> manifests;
        for (const ModEntry& m : state->mods) manifests.push_back(m.manifest);
        const std::string& id = state->mods[clicked].manifest.id;
        std::vector<std::string> next = modlist::switched(state->ticked, manifests, id, on);
        const std::string note = modlist::switch_note(manifests, state->ticked, next, id, on);
        state->ticked = std::move(next);
        if (!note.empty()) SetWindowTextW(state->status, widen(note).c_str());
        for (size_t i = 0; i < state->boxes.size(); ++i)
          SendMessageW(state->boxes[i], BM_SETCHECK,
                       modlist::enabled(state->ticked, state->mods[i].manifest.id) ? BST_CHECKED : BST_UNCHECKED, 0);
        if (!save_selection(state->selectionPath, state->ticked))
          SetWindowTextW(state->status, L"PhasmoMods Loader could not save which mods are ticked. Check access to your local application-data folder.");
      } else if (LOWORD(wParam) == 102) {
        const bool copied = copy_option(window, state->launchOption);
        SetWindowTextW(state->status, copied ? L"Copied. In Steam, open Phasmophobia > Properties > General and paste into Launch Options. Keep %command% at the end." :
          L"The clipboard is unavailable. Select and copy the launch option above.");
      }
      return 0;
    case WM_TIMER: {
      if (wParam != 1 || !state->busy || !state->completed.load(std::memory_order_acquire)) return 0;
      KillTimer(window, 1);
      if (state->worker.joinable()) state->worker.join();
      state->busy = false;
      set_controls(state, true);
      SetWindowTextW(state->status, describe(state->result).c_str());
      if (state->closing) DestroyWindow(window);
      return 0;
    }
    case WM_CLOSE:
      if (state->busy) { state->closing = true; ShowWindow(window, SW_HIDE); }
      else DestroyWindow(window);
      return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
  }
  return DefWindowProcW(window, message, wParam, lParam);
}

/** Build an ordinary keyboard-accessible Windows setup and attach window. */
int show_window(HINSTANCE instance, const std::wstring& executable, const std::wstring& hostPath) {
  WindowState state;
  std::wstring problems;
  state.mods = discover_mods(fs::path(executable).parent_path() / L"mods", problems);
  state.selectionPath = selection_file();
  state.ticked = saved_selection(state.selectionPath, state.mods);
  state.package = {hostPath, all_dlls(state.mods)};
  state.launchOption = steam_launch_option(executable);
  WNDCLASSEXW type{};
  type.cbSize = sizeof(type);
  type.lpfnWndProc = window_proc;
  type.hInstance = instance;
  type.lpszClassName = L"PhasmoModLauncher";
  type.hCursor = LoadCursor(nullptr, IDC_ARROW);
  type.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_BTNFACE + 1);
  type.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(kIcon));
  type.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(kIcon), IMAGE_ICON,
    GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_SHARED));
  if (!RegisterClassExW(&type)) return 1;
  const int listHeight = 30 * static_cast<int>(state.mods.size() ? state.mods.size() : 1);
  HWND window = CreateWindowExW(WS_EX_CONTROLPARENT, type.lpszClassName, kTitle,
    WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT,
    650, 395 + listHeight, nullptr, nullptr, instance, &state);
  if (!window) return 1;
  const auto control = [&](const wchar_t* kind, const wchar_t* text, DWORD style,
                           int x, int y, int width, int height, int identifier) {
    HWND child = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
      x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance, nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return child;
  };
  control(L"STATIC", L"Tick the mods to start. You can change them later from the Mods button on the game's main menu.", 0, 18, 14, 600, 22, 0);
  int y = 40;
  for (size_t i = 0; i < state.mods.size(); ++i, y += 30) {
    const auto& m = state.mods[i].manifest;
    const std::wstring label = widen(m.name) + (m.description.empty() ? L"" : L": " + widen(m.description));
    HWND box = control(L"BUTTON", label.c_str(), WS_TABSTOP | BS_AUTOCHECKBOX, 18, y, 600, 26, 200 + static_cast<int>(i));
    SendMessageW(box, BM_SETCHECK, modlist::enabled(state.ticked, m.id) ? BST_CHECKED : BST_UNCHECKED, 0);
    state.boxes.push_back(box);
  }
  if (state.mods.empty()) {
    control(L"STATIC", L"No mods were found. Extract the complete package, with its mods folder, beside PhasmoMods Loader.", 0, 18, y, 600, 26, 0);
    y += 30;
  }
  y += 10;
  control(L"STATIC", L"Start Phasmophobia through Steam and wait for the main menu, then choose Attach.", 0, 18, y, 600, 22, 0);
  state.attachButton = control(L"BUTTON", L"&Attach to running game", WS_TABSTOP | BS_DEFPUSHBUTTON, 18, y + 28, 205, 30, 101);
  control(L"STATIC", L"To load automatically, copy this command into Steam's Launch Options:", 0, 18, y + 74, 600, 24, 0);
  control(L"EDIT", state.launchOption.c_str(), WS_TABSTOP | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL, 18, y + 101, 600, 26, 103);
  control(L"BUTTON", L"&Copy Steam launch option", WS_TABSTOP, 18, y + 135, 205, 30, 102);
  const std::wstring status = problems.empty() ? L"Keep this folder outside the game directory. Restart the game to change the mods' versions." : problems;
  state.status = control(L"EDIT", status.c_str(),
    WS_TABSTOP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 18, y + 186, 600, 75, 104);
  ShowWindow(window, SW_SHOW);
  SetFocus(state.attachButton);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
  }
  if (state.worker.joinable()) state.worker.join();
  return 0;
}
#endif
}

#ifndef PG_LAUNCHER_TEST
#include "Velopack.hpp"

/** Enter either Steam wrapping or the attach UI without spawning Steam recursively. */
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int) {
  Velopack::VelopackApp::Build().Run();
  try {
    int count = 0;
    auto arguments = CommandLineToArgvW(GetCommandLineW(), &count);
    if (!arguments) return 1;
    std::vector<std::wstring> values(arguments, arguments + count);
    LocalFree(arguments);
    std::wstring hostPath, executable;
    auto result = pg::launcher::adjacent_host(hostPath, executable);
    if (result.outcome == pg::loader::Outcome::ready && values.size() == 1)
      return pg::launcher::show_window(instance, executable, hostPath);
    if (result.outcome == pg::loader::Outcome::ready) {
      std::wstring problems;
      const auto mods = pg::launcher::discover_mods(std::filesystem::path(executable).parent_path() / L"mods", problems);
      const pg::launcher::Package package{hostPath, pg::launcher::all_dlls(mods)};
      if (values.size() > 2 && values[1] == L"--steam")
        result = pg::launcher::launch_steam({values.begin() + 2, values.end()}, package);
      else result = pg::launcher::failure(pg::loader::Outcome::invalid_target,
        L"Open PhasmoMods Loader without arguments, or copy its Steam launch option into Steam.");
    }
    if (result.outcome == pg::loader::Outcome::active) return 0;
    MessageBoxW(nullptr, pg::launcher::describe(result).c_str(), pg::launcher::kName, MB_OK | MB_ICONWARNING);
  } catch (...) {
    MessageBoxW(nullptr, L"PhasmoMods Loader could not finish. Close it and try again.", pg::launcher::kName, MB_OK | MB_ICONERROR);
  }
  return 1;
}
#endif
