#include "launcher.h"
#include "inject.h"
#include "update.h"
#include "text_files.h"
#include "text_table.h"
#include "win_handle.h"
#include "../host/mod_deps.h"
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <algorithm>
#include <filesystem>
#include <atomic>
#include <chrono>
#include <fstream>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

namespace pm::launcher {
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
      return pm::text::widen(pm::i18n::text("launcher.remove-nolog-use-logfile"));
    if (_wcsicmp(argument.c_str(), L"-logFile") != 0) continue;
    if (!logPath.empty() || ++i == arguments.size() || arguments[i].empty() || arguments[i][0] == L'-')
      return pm::text::widen(pm::i18n::text("launcher.use-one-logfile-option"));
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
Result failure(Outcome outcome, const std::wstring& detail, DWORD error = 0) {
  return {outcome, error, detail, {}};
}

using text::read_file;
using text::widen;

fs::path selection_file();  // defined below, beside the launcher's other data files

std::vector<ModEntry> discover_mods(const std::vector<ModsFolder>& roots, std::wstring& problems,
                                    std::vector<modlist::Refused>& refused) {
  std::vector<ModEntry> mods;
  problems.clear();
  refused.clear();
  const auto refuse = [&](const fs::path& dir, const std::string& why) {
    problems += pm::text::widen(pm::i18n::text("launcher.mod-unusable", {{"folder", pm::text::narrow(dir.filename().wstring())}, {"why", why}})) + L"\r\n";
    refused.push_back({pm::text::narrow(dir.filename().wstring()), why});
  };
  std::error_code error;
  std::vector<std::pair<fs::path, modlist::Place>> folders;
  for (const ModsFolder& root : roots) {
    std::vector<fs::path> here;
    for (const auto& entry : fs::directory_iterator(root.path, error))
      if (entry.is_directory(error)) here.push_back(entry.path());
    std::sort(here.begin(), here.end());
    for (fs::path& dir : here) folders.emplace_back(std::move(dir), root.place);
  }
  // A developer's launcher.ini line is what shows developerOnly mods; nobody else sees them at all.
  const bool developer = modlist::parse_developer(pm::text::read_file(selection_file()));
  for (const auto& [dir, place] : folders) {
    ModEntry m;
    std::string why;
    const modlist::Reading reading = modlist::read_folder(dir, m.manifest, why, place, developer);
    if (reading == modlist::Reading::Refused) refuse(dir, why);
    if (reading != modlist::Reading::Readable) continue;
    m.dll = (dir / widen(m.manifest.dll)).wstring();
    if (!fs::is_regular_file(m.dll, error)) {  // a folder of that name is no DLL either
      refuse(dir, pm::i18n::text("launcher.mod-missing-dll"));
      continue;
    }
    bool repeated = false;
    for (const ModEntry& other : mods) repeated |= other.manifest.id == m.manifest.id;
    if (repeated) {
      refuse(dir, pm::i18n::text("launcher.mod-repeats-id"));
      continue;
    }
    mods.push_back(std::move(m));
  }
  return mods;
}

std::vector<std::string> session_lines(const std::vector<modlist::Refused>& refused, const fs::path& builds) {
  std::vector<std::string> lines;
  for (const modlist::Refused& r : refused) lines.push_back(modlist::session_refused_line(r));
  if (!builds.empty()) lines.push_back(modlist::session_builds_line(text::narrow(builds.wstring())));
  return lines;
}

std::vector<std::string> saved_selection(const fs::path& launcherIni, const std::vector<ModEntry>& mods) {
  std::vector<std::string> installed;
  std::vector<modlist::Manifest> manifests;
  for (const ModEntry& m : mods) {
    installed.push_back(m.manifest.id);
    manifests.push_back(m.manifest);
  }
  // A choice saved before a mod gained a requirement still ticks what that mod needs.
  return moddeps::with_requirements(modlist::load_choice(launcherIni, installed), manifests);
}

bool save_selection(const fs::path& launcherIni, const std::vector<std::string>& ids) {
  return modlist::save_choice(launcherIni, ids);
}

std::vector<std::wstring> all_dlls(const std::vector<ModEntry>& mods) {
  std::vector<std::wstring> dlls;
  for (const ModEntry& m : mods) dlls.push_back(m.dll);
  return dlls;
}

/** %LOCALAPPDATA%\PhasmoMods\`name`, or empty when the folder is unavailable. */
fs::path data_file(const wchar_t* name) {
  std::wstring local(32768, L'\0');
  const DWORD length = GetEnvironmentVariableW(L"LOCALAPPDATA", local.data(), static_cast<DWORD>(local.size()));
  if (!length || length >= local.size()) return {};
  local.resize(length);
  return fs::path(local) / L"PhasmoMods" / name;
}

/** %LOCALAPPDATA%\PhasmoMods\launcher.ini, or empty when the folder is unavailable. */
fs::path selection_file() { return data_file(L"launcher.ini"); }

fs::path launcher_log() { return data_file(L"launcher.log"); }

/** This program's own file, or empty when it cannot be read. */
std::wstring running_file() {
  std::wstring file(32768, L'\0');
  const DWORD length = GetModuleFileNameW(nullptr, file.data(), static_cast<DWORD>(file.size()));
  if (!length || length >= file.size()) return {};
  file.resize(length);
  return file;
}

/** Resolve the packaged host beside the launcher, independent of Steam's directory. */
Result adjacent_host(std::wstring& path, std::wstring& executable) {
  executable = running_file();
  if (executable.empty())
    return failure(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.location-read-extract-package")), GetLastError());
  path = (fs::path(executable).parent_path() / L"ModHost.dll").wstring();
  const DWORD attributes = GetFileAttributesW(path.c_str());
  if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY))
    return failure(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.modhost-dll-missing-extract")), GetLastError());
  return {Outcome::ready, 0, {}, {}};
}

/** Serialize loading for this process without holding a machine-wide launch lock. */
Result load_once(DWORD pid, const Package& package) {
  const auto name = L"Local\\PhasmoModLauncher-" + std::to_wstring(pid);
  Handle lock(CreateMutexW(nullptr, FALSE, name.c_str()));
  DWORD error = GetLastError();
  if (!lock.value)
    return failure(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.mod-loading-lock-created")), error);
  if (error == ERROR_ALREADY_EXISTS)
    return failure(Outcome::already_loaded, pm::text::widen(pm::i18n::text("launcher.another-copy-loading-wait")));
  return loader::load(pid, package.host.c_str(), package.mods, package.refused);
}

/** Attach only to the single game instance selected by the shared identity check. */
Result attach(const Package& package) {
  if (package.mods.empty())
    return failure(Outcome::invalid_target, pm::text::widen(pm::i18n::text("launcher.no-mods-were-found")));
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
    return failure(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.log-read-check-path")), error);
  }
  BY_HANDLE_FILE_INFORMATION information{};
  if (!GetFileInformationByHandle(file.value, &information))
    return failure(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.log-inspected-check-access")), GetLastError());
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
    return failure(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.log-read")), GetLastError());
  char buffer[16384];
  DWORD read = 0;
  for (size_t total = 0; total < 1024 * 1024; total += read) {
    if (!ReadFile(file.value, buffer, baseline ? 1 : sizeof(buffer), &read, nullptr))
      return failure(Outcome::log_error, pm::text::widen(pm::i18n::text("launcher.log-read-failed")), GetLastError());
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
    return failure(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.local-application-data-folder")), GetLastError());
  local.resize(length);
  const auto directory = fs::path(local) / L"PhasmoMods" / L"logs";
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) return failure(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.log-folder-created-check")), error.value());
  path = (directory / (L"game-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
    std::to_wstring(GetTickCount64()) + L".log")).wstring();
  Handle file(CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, 0, nullptr));
  if (file.value == INVALID_HANDLE_VALUE)
    return failure(Outcome::file_error, pm::text::widen(pm::i18n::text("launcher.fresh-log-created-close")), GetLastError());
  return {Outcome::ready, 0, {}, {}};
}

/** Wait up to five minutes for this child to reach its completed main-menu log marker. */
Result wait_menu(HANDLE process, LogCursor& cursor) {
  const auto deadline = GetTickCount64() + 300000;
  while (GetTickCount64() < deadline) {
    const DWORD state = WaitForSingleObject(process, 0);
    if (state == WAIT_OBJECT_0)
      return failure(Outcome::game_exited, pm::text::widen(pm::i18n::text("launcher.phasmophobia-exited-before-reaching")));
    if (state != WAIT_TIMEOUT)
      return failure(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.process-monitored")), GetLastError());
    auto result = read_menu(cursor);
    if (result.outcome != Outcome::ready) return result;
    Sleep(200);
  }
  return failure(Outcome::activation_timeout, pm::text::widen(pm::i18n::text("launcher.main-menu-was-confirmed")));
}

/** Launch exactly Steam's supplied game command and keep its returned process identity. */
Result launch_steam(std::vector<std::wstring> arguments, const Package& package) {
  if (arguments.empty() || !fs::path(arguments[0]).is_absolute() ||
      _wcsicmp(fs::path(arguments[0]).filename().c_str(), L"Phasmophobia.exe") != 0)
    return failure(Outcome::invalid_target, pm::text::widen(pm::i18n::text("launcher.steam-command-missing-invalid")));
  // The named object's lifetime reserves startup before a game PID exists.
  SetLastError(ERROR_SUCCESS);
  Handle startupLock(CreateMutexW(nullptr, FALSE, L"Local\\PhasmoModSteamLauncher"));
  const DWORD lockError = GetLastError();
  if (!startupLock.value)
    return failure(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.startup-lock-created-close")), lockError);
  if (lockError == ERROR_ALREADY_EXISTS)
    return failure(Outcome::already_loaded, pm::text::widen(pm::i18n::text("launcher.another-copy-starting-phasmophobia")));
  DWORD existingPid = 0;
  auto result = loader::find_game(existingPid);
  if (result.outcome == Outcome::ready)
    return failure(Outcome::already_loaded, pm::text::widen(pm::i18n::text("launcher.phasmophobia-already-running-close")));
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
    return failure(Outcome::invalid_target, pm::text::widen(pm::i18n::text("launcher.command-too-long-shorten")));
  STARTUPINFOW startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION child{};
  // The game runs in its own folder, as from a normal Steam launch, not in the launcher's (after an
  // update restart that is Velopack's current folder).
  const std::wstring gameFolder = fs::path(arguments[0]).parent_path().wstring();
  if (!CreateProcessW(arguments[0].c_str(), command.data(), nullptr, nullptr, FALSE,
                      0, nullptr, gameFolder.c_str(), &startup, &child))
    return failure(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.phasmophobia-start-check-steam")), GetLastError());
  Handle process(child.hProcess), thread(child.hThread);
  if (package.mods.empty())
    return {Outcome::active, 0, pm::text::widen(pm::i18n::text("launcher.no-mods-were-found-2")), logPath};
  result = wait_menu(process.value, cursor);
  if (result.outcome == Outcome::active) result = load_once(child.dwProcessId, package);
  if (result.logPath.empty()) result.logPath = logPath;
  return result;
}

/** Present failure details without confusing a mapped DLL with an activated mod. */
std::wstring describe(const Result& result) {
  std::wstring text = result.outcome == Outcome::active ? (result.detail.empty() ? pm::text::widen(pm::i18n::text("launcher.running-return-answer-notice")) : result.detail) : result.detail;
  if (result.error) text += L"\r\n" + pm::text::widen(pm::i18n::text("launcher.windows-error", {{"code", std::to_string(result.error)}}));
  if (!result.logPath.empty()) text += L"\r\nLog: " + result.logPath;
  return text;
}

/** The outcome's name as the code spells it, for the log. */
const wchar_t* outcome_name(Outcome outcome) {
  switch (outcome) {
    case Outcome::ready: return L"ready";
    case Outcome::active: return L"active";
    case Outcome::already_loaded: return L"already_loaded";
    case Outcome::no_process: return L"no_process";
    case Outcome::ambiguous_process: return L"ambiguous_process";
    case Outcome::invalid_target: return L"invalid_target";
    case Outcome::wrong_architecture: return L"wrong_architecture";
    case Outcome::process_error: return L"process_error";
    case Outcome::file_error: return L"file_error";
    case Outcome::allocation_failed: return L"allocation_failed";
    case Outcome::write_failed: return L"write_failed";
    case Outcome::thread_failed: return L"thread_failed";
    case Outcome::indeterminate: return L"indeterminate";
    case Outcome::not_mapped: return L"not_mapped";
    case Outcome::inactive: return L"inactive";
    case Outcome::activation_timeout: return L"activation_timeout";
    case Outcome::game_exited: return L"game_exited";
    case Outcome::log_error: return L"log_error";
  }
  return L"unknown";
}

std::wstring result_line(const Result& result, std::wstring_view how) {
  // Each line break, \r\n or alone, becomes one space.
  const auto one_line = [](const std::wstring& text) {
    std::wstring out;
    for (size_t i = 0; i < text.size(); ++i) {
      if (text[i] == L'\r' && i + 1 < text.size() && text[i + 1] == L'\n') ++i;
      out += (text[i] == L'\r' || text[i] == L'\n') ? L' ' : text[i];
    }
    while (!out.empty() && out.back() == L' ') out.pop_back();
    return out;
  };
  std::wstring line = std::wstring(how) + L" " + outcome_name(result.outcome);
  if (!result.detail.empty()) line += L": " + one_line(result.detail);
  if (result.error) {
    wchar_t words[512] = {};
    const DWORD n = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, result.error, 0, words,
                                   static_cast<DWORD>(std::size(words)), nullptr);
    line += L" (Windows error " + std::to_wstring(result.error) + (n ? L": " + one_line({words, n}) : L"") + L")";
  }
  if (!result.logPath.empty()) line += L" log " + result.logPath;
  return line;
}

#ifndef PM_LAUNCHER_TEST
constexpr wchar_t kName[] = L"PhasmoMods Loader";      // the message boxes' title
constexpr wchar_t kTitle[] = L"PhasmoMods Loader " PM_WIDEN(PM_SUITE_VERSION);  // the window's title
constexpr int kIcon = 1;                               // tools/launcher.rc
constexpr std::chrono::seconds kCheckLimit{10};        // the longest an update check delays play
constexpr std::chrono::seconds kDownloadStall{30};     // a download with no progress for this long gives up
constexpr int kDevelopmentBox = 105;                   // the checkbox that takes development builds
constexpr int kUpdateNow = 1001, kRemindLater = 1002, kIgnore = 1003;  // the update popup's buttons

/** Add a timestamped line to launcher_log(): what `--steam` mode cannot show in a window, and how every load ended. */
void append_log(const std::wstring& message) {
  const fs::path file = launcher_log();
  if (file.empty() || message.empty()) return;
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  SYSTEMTIME now{};
  GetLocalTime(&now);
  wchar_t stamp[32];
  swprintf_s(stamp, L"%04u-%02u-%02u %02u:%02u:%02u ", now.wYear, now.wMonth, now.wDay,
             now.wHour, now.wMinute, now.wSecond);
  std::ofstream(file, std::ios::binary | std::ios::app) << text::narrow(stamp + message) << "\r\n";
}

/** %LOCALAPPDATA%\PhasmoMods\update_channel.txt, the development box's choice, or empty when the folder is unavailable. */
fs::path development_file() { return data_file(L"update_channel.txt"); }

/** Whether this install takes development builds. */
bool takes_development() {
  return update::wants_development(update::read_development_choice(development_file()), PM_SUITE_VERSION);
}

/** %LOCALAPPDATA%\PhasmoMods\update_ignored.txt, the version the player chose to ignore. */
fs::path ignored_file() { return data_file(L"update_ignored.txt"); }

/** How the player is asked: the update popup at start-up, or the development box's confirmation. */
enum class Ask { Offer, Confirm };
/** What the player chose in the update dialog. */
enum class Choice { UpdateNow, RemindLater, Ignore, Declined };

/** What the update dialog and its callback share; the strings live as long as the dialog. */
struct UpdateDialog {
  std::shared_ptr<update::Backend> backend;
  std::string version;
  std::vector<std::wstring> arguments;
  std::wstring question, yes, later, ignore, downloading;
  TASKDIALOG_BUTTON buttons[3]{};
  TASKDIALOGCONFIG progressPage{};
  std::optional<update::Install> install;
  bool finished = false;
  update::Report report{update::Outcome::CarryOn, L"", ""};
};

/** Settle the dialog's result and close it. */
void close_with(HWND dialog, UpdateDialog* state, update::Report report) {
  state->finished = true;
  state->report = std::move(report);
  SendMessageW(dialog, TDM_CLICK_BUTTON, IDCANCEL, 0);
}

HRESULT CALLBACK update_dialog_proc(HWND dialog, UINT note, WPARAM wParam, LPARAM, LONG_PTR data) {
  auto* state = reinterpret_cast<UpdateDialog*>(data);
  switch (note) {
    case TDN_BUTTON_CLICKED:
      if (!state->install && !state->finished) {
        // The question's page: Update now (or Yes) moves to the progress page; any other button closes it.
        if (wParam == kUpdateNow || wParam == IDYES) {
          SendMessageW(dialog, TDM_NAVIGATE_PAGE, 0, reinterpret_cast<LPARAM>(&state->progressPage));
          return S_FALSE;
        }
        return S_OK;
      }
      // The progress page's Cancel. Once the apply has started it runs to its end, so the click waits for it.
      if (state->finished) return S_OK;
      if (!state->install->cancel()) return S_FALSE;
      state->finished = true;
      state->report = {update::Outcome::CarryOn, L"", ""};
      return S_OK;
    case TDN_NAVIGATED:
      try {
        state->install = update::Install::start(state->backend, state->version, state->arguments);
      } catch (...) {
        close_with(dialog, state, {update::Outcome::CarryOn,
                   pm::text::widen(pm::i18n::text("updater.failed", {{"reason", pm::i18n::text("updater.unknown-error")}})), ""});
      }
      return S_OK;
    case TDN_TIMER: {
      if (!state->install || state->finished) return S_OK;
      const update::Install::Status status = state->install->poll(std::chrono::steady_clock::now(), kDownloadStall);
      SendMessageW(dialog, TDM_SET_PROGRESS_BAR_POS, status.percent, 0);
      if (status.finished) close_with(dialog, state, status.report);
      return S_OK;
    }
  }
  return S_OK;
}

/**
 * Ask the player about `version` with the Windows task dialog, over `owner` (null: on its own, as under Steam).
 * Updating moves to a progress bar until the install restarts this program, fails, stalls or is cancelled.
 */
update::Report ask_to_update(HWND owner, std::shared_ptr<update::Backend> backend, const std::string& version,
                             std::vector<std::wstring> arguments, Ask ask, Choice& choice) {
  UpdateDialog state;
  state.backend = std::move(backend);
  state.version = version;
  state.arguments = std::move(arguments);
  state.question = pm::text::widen(ask == Ask::Offer ? pm::i18n::text("updater.available", {{"version", version}})
                                                    : pm::i18n::text("launcher.development-confirm", {{"version", version}}));
  state.yes = pm::text::widen(pm::i18n::text("updater.update-now"));
  state.later = pm::text::widen(pm::i18n::text("updater.remind-later"));
  state.ignore = pm::text::widen(pm::i18n::text("updater.ignore"));
  state.downloading = pm::text::widen(pm::i18n::text("launcher.updating", {{"version", version}}));
  state.buttons[0] = {kUpdateNow, state.yes.c_str()};
  state.buttons[1] = {kRemindLater, state.later.c_str()};
  state.buttons[2] = {kIgnore, state.ignore.c_str()};

  TASKDIALOGCONFIG question{};
  question.cbSize = sizeof(question);
  question.hwndParent = owner;
  question.hInstance = GetModuleHandleW(nullptr);
  question.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
  question.pszWindowTitle = kName;
  question.pszMainInstruction = state.question.c_str();
  if (ask == Ask::Offer) {
    question.pButtons = state.buttons;
    question.cButtons = 3;
    question.nDefaultButton = kUpdateNow;
  } else {
    question.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    question.nDefaultButton = IDYES;
  }
  question.pfCallback = update_dialog_proc;
  question.lpCallbackData = reinterpret_cast<LONG_PTR>(&state);

  state.progressPage = question;
  state.progressPage.pButtons = nullptr;
  state.progressPage.cButtons = 0;
  state.progressPage.dwCommonButtons = TDCBF_CANCEL_BUTTON;
  state.progressPage.nDefaultButton = IDCANCEL;
  state.progressPage.dwFlags = TDF_SHOW_PROGRESS_BAR | TDF_CALLBACK_TIMER | TDF_POSITION_RELATIVE_TO_WINDOW;
  state.progressPage.pszMainInstruction = state.downloading.c_str();

  int button = IDCANCEL;
  if (FAILED(TaskDialogIndirect(&question, &button, nullptr, nullptr))) {
    choice = Choice::RemindLater;
    return {update::Outcome::CarryOn, pm::text::widen(pm::i18n::text("launcher.check-updates-starting-installed")), ""};
  }
  if (state.install || state.finished) choice = Choice::UpdateNow;
  else if (button == kIgnore) choice = Choice::Ignore;
  else if (ask == Ask::Confirm) choice = Choice::Declined;
  else choice = Choice::RemindLater;  // Remind me later, or the dialog closed
  return state.report;
}

/**
 * The start-up update: check, waiting at most kCheckLimit, then offer any version the player has not ignored.
 * Remind me later and Ignore carry on with the installed version. A check that cannot start reports CarryOn.
 */
update::Report update_at_start(HWND owner, const std::vector<std::wstring>& launcherArguments) {
  update::Report report{update::Outcome::CarryOn, pm::text::widen(pm::i18n::text("launcher.check-updates-starting-installed")), ""};
  try {
    auto backend = update::velopack_backend(update::feed_for(takes_development()));
    report = update::check(backend, kCheckLimit);
    if (report.outcome != update::Outcome::Available) return report;
    if (!update::should_offer(report.version, update::read_ignored_version(ignored_file())))
      return {update::Outcome::CarryOn, L"", ""};
    Choice choice = Choice::RemindLater;
    const std::string version = report.version;
    report = ask_to_update(owner, backend, version, update::restart_arguments(launcherArguments), Ask::Offer, choice);
    if (choice == Choice::Ignore) update::save_ignored_version(ignored_file(), version);
  } catch (...) { /* An update that cannot start never stops the game: keep the prepared report. */ }
  return report;
}

/** Keep the small instruction window alive while one background attach operation finishes. */
struct WindowState {
  Package package;
  std::vector<ModEntry> mods;
  std::vector<std::string> ticked;
  fs::path selectionPath;
  std::vector<HWND> boxes;
  HWND developmentBox = nullptr;
  std::wstring executable;
  std::wstring launchOption;
  std::wstring usualStatus;  // the status box's text when nothing is running: what is wrong with the mods, or nothing
  HWND status = nullptr;
  HWND attachButton = nullptr;
  bool busy = false;
  bool updating = false;  // the worker is checking for updates, not attaching
  bool switching = false;  // that check follows a click on the development box, which the player confirms
  std::shared_ptr<update::Backend> updateBackend;  // what the check asked, ready to download what it found
  bool attached = false;  // mods were loaded into the game: what they run from can't be replaced until it closes
  std::atomic<bool> closing{false};  // the window was closed while busy: it goes when the worker ends
  Result result;
  update::Report updateReport;
  std::atomic<bool> completed{false};
  std::thread worker;
};

/** Attach and the mod checkboxes are usable only while no load is running, so the shown choice is the loaded one. */
void set_controls(WindowState* state, bool on) {
  EnableWindow(state->attachButton, on ? TRUE : FALSE);
  for (HWND box : state->boxes) EnableWindow(box, on ? TRUE : FALSE);
  EnableWindow(state->developmentBox, on && !state->attached ? TRUE : FALSE);
}

/** Put the development box and its saved choice back as they were, after the player declined the switch or it could not be checked. */
void undo_switch(WindowState* state) {
  const bool ticked = SendMessageW(state->developmentBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
  update::save_development_choice(development_file(), !ticked);
  SendMessageW(state->developmentBox, BM_SETCHECK, ticked ? BST_UNCHECKED : BST_CHECKED, 0);
}

/**
 * Check for updates on a worker, with the controls off until it ends; WM_TIMER asks the player and shows the result.
 * `switching`: the check follows a click on the development box. False when it could not start.
 */
bool start_update(HWND window, WindowState* state, bool switching) {
  if (!SetTimer(window, 1, 100, nullptr)) return false;
  state->busy = state->updating = true;
  state->switching = switching;
  state->completed.store(false, std::memory_order_relaxed);
  set_controls(state, false);
  SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.checking-updates")).c_str());
  try {
    state->updateReport = {update::Outcome::CarryOn, pm::text::widen(pm::i18n::text("launcher.check-updates-starting-installed")), ""};
    state->worker = std::thread([state] {
      try {
        auto backend = update::velopack_backend(update::feed_for(takes_development()));
        state->updateBackend = backend;
        state->updateReport = update::check(backend, kCheckLimit);
      } catch (...) { /* Keep the prepared report. */ }
      state->completed.store(true, std::memory_order_release);
    });
  } catch (...) {
    KillTimer(window, 1);
    state->busy = state->updating = false;
    set_controls(state, true);
    SetWindowTextW(state->status, state->usualStatus.c_str());
    return false;
  }
  return true;
}

/**
 * After the check: ask the player about what it found, and install it if they agree. The start-up check offers
 * Update now, Remind me later and Ignore; the development box asks for confirmation, and declining undoes the switch.
 */
update::Report settle_update(HWND window, WindowState* state) {
  update::Report report = state->updateReport;
  if (report.outcome == update::Outcome::Available && !state->closing) {
    const std::string version = report.version;
    Choice choice = Choice::RemindLater;
    if (state->switching) {
      report = ask_to_update(window, state->updateBackend, version, {}, Ask::Confirm, choice);
      if (choice == Choice::Declined) undo_switch(state);
    } else if (update::should_offer(version, update::read_ignored_version(ignored_file()))) {
      report = ask_to_update(window, state->updateBackend, version, {}, Ask::Offer, choice);
      if (choice == Choice::Ignore) update::save_ignored_version(ignored_file(), version);
    } else {
      report = {update::Outcome::CarryOn, L"", ""};
    }
  } else if (state->switching && report.outcome == update::Outcome::CarryOn) {
    undo_switch(state);  // the other builds could not be checked, so the box keeps the ones installed
  }
  state->updateBackend.reset();
  return report;
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
          SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.monitor-loading-close")).c_str());
          return 0;
        }
        state->busy = true;
        state->completed.store(false, std::memory_order_relaxed);
        set_controls(state, false);
        SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.loading-mods-waiting-confirm")).c_str());
        try {
          state->result = failure(Outcome::process_error, pm::text::widen(pm::i18n::text("launcher.finish-loading")));
          state->worker = std::thread([state] {
            try { state->result = attach(state->package); }
            catch (...) { /* Keep the prepared failure without allocating in this path. */ }
            state->completed.store(true, std::memory_order_release);
          });
        } catch (...) {
          KillTimer(window, 1);
          state->busy = false;
          set_controls(state, true);
          SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.loading-task-start-close")).c_str());
        }
      } else if (LOWORD(wParam) >= 200 && LOWORD(wParam) < 200 + state->boxes.size() && !state->busy) {
        // A mod is ticked with what it requires and unticked with what requires it.
        const size_t clicked = LOWORD(wParam) - 200;
        const bool on = SendMessageW(state->boxes[clicked], BM_GETCHECK, 0, 0) == BST_CHECKED;
        std::vector<modlist::Manifest> manifests;
        for (const ModEntry& m : state->mods) manifests.push_back(m.manifest);
        const std::string& id = state->mods[clicked].manifest.id;
        std::vector<std::string> next = moddeps::switched(state->ticked, manifests, id, on);
        const std::string note = moddeps::switch_note(manifests, state->ticked, next, id, on);
        state->ticked = std::move(next);
        if (!note.empty()) SetWindowTextW(state->status, widen(note).c_str());
        for (size_t i = 0; i < state->boxes.size(); ++i)
          SendMessageW(state->boxes[i], BM_SETCHECK,
                       modlist::enabled(state->ticked, state->mods[i].manifest.id) ? BST_CHECKED : BST_UNCHECKED, 0);
        if (!save_selection(state->selectionPath, state->ticked))
          SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.save-which-mods-are")).c_str());
      } else if (LOWORD(wParam) == kDevelopmentBox && !state->busy) {
        // The choice is kept before the update, so whatever comes of it, the next start reads the same builds.
        const bool on = SendMessageW(state->developmentBox, BM_GETCHECK, 0, 0) == BST_CHECKED;
        if (!update::save_development_choice(development_file(), on)) {
          SendMessageW(state->developmentBox, BM_SETCHECK, on ? BST_UNCHECKED : BST_CHECKED, 0);
          SetWindowTextW(state->status, pm::text::widen(pm::i18n::text("launcher.development-not-saved")).c_str());
        } else if (!start_update(window, state, true)) {
          undo_switch(state);
        }
      } else if (LOWORD(wParam) == 102) {
        const bool copied = copy_option(window, state->launchOption);
        SetWindowTextW(state->status, (copied ? pm::text::widen(pm::i18n::text("launcher.copied-steam-open-phasmophobia"))
                                              : pm::text::widen(pm::i18n::text("launcher.clipboard-unavailable-select-copy"))).c_str());
      }
      return 0;
    case WM_TIMER: {
      if (wParam != 1 || !state->busy || !state->completed.load(std::memory_order_acquire)) return 0;
      KillTimer(window, 1);
      if (state->worker.joinable()) state->worker.join();
      state->busy = false;
      if (state->updating) {
        state->updating = false;
        state->updateReport = settle_update(window, state);
        // Velopack's updater applies the new version once this program exits, then starts it again.
        if (state->updateReport.outcome == update::Outcome::Restarting) { DestroyWindow(window); return 0; }
        set_controls(state, true);
        SetFocus(state->attachButton);
        // What the update found comes first, then the usual text, which names any broken mods.
        const std::wstring& why = state->updateReport.message;
        const std::wstring both = why.empty() || state->usualStatus.empty() ? why + state->usualStatus
                                                                           : why + L"\r\n" + state->usualStatus;
        SetWindowTextW(state->status, both.c_str());
        if (state->closing) DestroyWindow(window);
        return 0;
      }
      state->attached = state->attached || state->result.outcome == Outcome::active;
      append_log(result_line(state->result, L"attach"));  // the window's text goes when it closes; the log keeps the cause
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
  std::vector<modlist::Refused> refused;
  state.mods = discover_mods({{fs::path(executable).parent_path() / L"mods", modlist::Place::InstallFolder}}, problems, refused);
  state.selectionPath = selection_file();
  state.ticked = saved_selection(state.selectionPath, state.mods);
  state.package = {hostPath, all_dlls(state.mods), session_lines(refused, fs::path(executable).parent_path() / L"builds")};
  state.launchOption = steam_launch_option(executable);
  state.executable = executable;
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
    650, 425 + listHeight, nullptr, nullptr, instance, &state);
  if (!window) return 1;
  const auto control = [&](const wchar_t* kind, const wchar_t* text, DWORD style,
                           int x, int y, int width, int height, int identifier) {
    HWND child = CreateWindowExW(0, kind, text, WS_CHILD | WS_VISIBLE | style,
      x, y, width, height, window, reinterpret_cast<HMENU>(static_cast<INT_PTR>(identifier)), instance, nullptr);
    SendMessageW(child, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
    return child;
  };
  control(L"STATIC", pm::text::widen(pm::i18n::text("launcher.tick-mods-start-you")).c_str(), 0, 18, 14, 600, 22, 0);
  int y = 40;
  for (size_t i = 0; i < state.mods.size(); ++i, y += 30) {
    const auto& m = state.mods[i].manifest;
    const std::wstring label = widen(m.name) + L" " + (m.version.empty() ? pm::text::widen(pm::i18n::text("panel.version-unknown")) : widen(m.version)) +
                               (m.description.empty() ? L"" : L": " + widen(m.description));
    HWND box = control(L"BUTTON", label.c_str(), WS_TABSTOP | BS_AUTOCHECKBOX, 18, y, 600, 26, 200 + static_cast<int>(i));
    SendMessageW(box, BM_SETCHECK, modlist::enabled(state.ticked, m.id) ? BST_CHECKED : BST_UNCHECKED, 0);
    state.boxes.push_back(box);
  }
  if (state.mods.empty()) {
    control(L"STATIC", pm::text::widen(pm::i18n::text("launcher.no-mods-were-found")).c_str(), 0, 18, y, 600, 26, 0);
    y += 30;
  }
  state.developmentBox = control(L"BUTTON", pm::text::widen(pm::i18n::text("launcher.development")).c_str(),
                                 WS_TABSTOP | BS_AUTOCHECKBOX, 18, y, 600, 26, kDevelopmentBox);
  SendMessageW(state.developmentBox, BM_SETCHECK, takes_development() ? BST_CHECKED : BST_UNCHECKED, 0);
  y += 40;
  control(L"STATIC", pm::text::widen(pm::i18n::text("launcher.start-phasmophobia-through-steam")).c_str(), 0, 18, y, 600, 22, 0);
  state.attachButton = control(L"BUTTON", pm::text::widen(pm::i18n::text("launcher.attach")).c_str(), WS_TABSTOP | BS_DEFPUSHBUTTON, 18, y + 28, 205, 30, 101);
  control(L"STATIC", pm::text::widen(pm::i18n::text("launcher.copy-intro")).c_str(), 0, 18, y + 74, 600, 24, 0);
  control(L"EDIT", state.launchOption.c_str(), WS_TABSTOP | WS_BORDER | ES_READONLY | ES_AUTOHSCROLL, 18, y + 101, 600, 26, 103);
  control(L"BUTTON", pm::text::widen(pm::i18n::text("launcher.copy")).c_str(), WS_TABSTOP, 18, y + 135, 205, 30, 102);
  state.usualStatus = problems;
  state.status = control(L"EDIT", state.usualStatus.c_str(),
    WS_TABSTOP | ES_READONLY | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL, 18, y + 186, 600, 75, 104);
  ShowWindow(window, SW_SHOW);
  SetFocus(state.attachButton);
  // Check for updates on the worker before Attach is usable.
  start_update(window, &state, false);
  MSG message{};
  while (GetMessageW(&message, nullptr, 0, 0) > 0) {
    if (!IsDialogMessageW(window, &message)) { TranslateMessage(&message); DispatchMessageW(&message); }
  }
  if (state.worker.joinable()) state.worker.join();
  return 0;
}
#endif
}

#ifndef PM_LAUNCHER_TEST
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
    // The loader's player-facing text (core/text_table.h), beside the program; without it each text shows its key. First of all, so
    // the box a damaged install needs most, that the host is missing, can be read.
    if (const std::wstring running = pm::launcher::running_file(); !running.empty()) {
      std::string why;
      pm::i18n::table().load(pm::i18n::table_beside(running), why);
    }
    std::wstring hostPath, executable;
    auto result = pm::launcher::adjacent_host(hostPath, executable);
    if (result.outcome == pm::loader::Outcome::ready && values.size() == 1)
      return pm::launcher::show_window(instance, executable, hostPath);
    const bool steam = values.size() > 2 && values[1] == L"--steam";
    if (result.outcome != pm::loader::Outcome::ready) {
      // A damaged install still updates, which is how it repairs itself.
      const auto update = pm::launcher::update_at_start(nullptr, values);
      if (update.outcome == pm::update::Outcome::Restarting) return 0;
      if (steam) pm::launcher::append_log(update.message);
      if (!update.message.empty()) result.detail += L"\r\n" + update.message;
    } else if (steam) {
      // Update before the game starts. Steam shows no window, so the popup stands on its own, and anything
      // but a restart goes to the log.
      const auto update = pm::launcher::update_at_start(nullptr, values);
      if (update.outcome == pm::update::Outcome::Restarting) return 0;
      pm::launcher::append_log(update.message);
      std::wstring problems;
      std::vector<pm::modlist::Refused> refused;
      const auto mods = pm::launcher::discover_mods(
          {{std::filesystem::path(executable).parent_path() / L"mods", pm::modlist::Place::InstallFolder}}, problems, refused);
      pm::launcher::append_log(problems);  // Steam shows no window; the host's Mods panel says the same
      const pm::launcher::Package package{hostPath, pm::launcher::all_dlls(mods),
                                          pm::launcher::session_lines(refused, std::filesystem::path(executable).parent_path() / L"builds")};
      result = pm::launcher::launch_steam({values.begin() + 2, values.end()}, package);
    } else result = pm::launcher::failure(pm::loader::Outcome::invalid_target,
      pm::text::widen(pm::i18n::text("launcher.open-without-arguments-copy")));
    pm::launcher::append_log(pm::launcher::result_line(result, steam ? L"steam" : L"start"));
    if (result.outcome == pm::loader::Outcome::active) return 0;
    MessageBoxW(nullptr, pm::launcher::describe(result).c_str(), pm::launcher::kName, MB_OK | MB_ICONWARNING);
  } catch (...) {
    MessageBoxW(nullptr, pm::text::widen(pm::i18n::text("launcher.finish-close")).c_str(), pm::launcher::kName, MB_OK | MB_ICONERROR);
  }
  return 1;
}
#endif
