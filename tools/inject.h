#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace pg::loader {
enum class Outcome {
  ready, active, already_loaded, no_process, ambiguous_process, invalid_target,
  wrong_architecture, process_error, file_error, allocation_failed, write_failed,
  thread_failed, indeterminate, not_mapped, inactive, unsupported_build,
  activation_timeout, game_exited, log_error
};

/** Describe loading and activation separately from the underlying Windows error. */
struct Result {
  Outcome outcome = Outcome::ready;
  DWORD error = ERROR_SUCCESS;
  std::wstring detail;
  std::wstring logPath;
};

/** Select exactly one running game; leave pid zero on absence, ambiguity or failure. */
Result find_game(DWORD& pid);

/**
 * Load a fresh copy of the host (ModHost.dll) into an x64 game, with fresh copies of the mod DLLs it
 * is to run listed in its session file, and require process-specific activation.
 */
Result load(DWORD pid, const wchar_t* hostDllPath, const std::vector<std::wstring>& modDllPaths = {});
}  // namespace pg::loader
