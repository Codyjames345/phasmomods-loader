#pragma once

#include <windows.h>

// Loading a mod's DLL, which may be anybody's file. Left to itself, Windows answers a corrupt or wrong-architecture image with
// a blocking "Bad Image" dialog, in the game or in a test run, until somebody dismisses it. While a QuietLoad lives, this
// thread's loader failures only come back as errors, so a bad mod is refused with a reason. It changes this thread's error
// mode for no longer than that, restores it however the scope ends, and leaves the thread's last error as it found it.
// The host's and launcher's own DLLs, which ship together, have no need of it.
namespace pm::dll {

class QuietLoad {
 public:
  QuietLoad() { armed_ = SetThreadErrorMode(GetThreadErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX, &old_) != 0; }
  ~QuietLoad() {
    if (!armed_) return;
    const DWORD error = GetLastError();
    SetThreadErrorMode(old_, nullptr);
    SetLastError(error);
  }
  QuietLoad(const QuietLoad&) = delete;
  QuietLoad& operator=(const QuietLoad&) = delete;

 private:
  DWORD old_ = 0;
  bool armed_ = false;
};

/** LoadLibraryExW(path, nullptr, flags) for a mod's DLL, with no dialog; null with GetLastError on failure. */
inline HMODULE load_mod(const wchar_t* path, DWORD flags = 0) {
  const QuietLoad quiet;
  return LoadLibraryExW(path, nullptr, flags);
}

}  // namespace pm::dll
