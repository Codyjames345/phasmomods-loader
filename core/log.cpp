#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

namespace pg::log {
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
void (*g_sink)(const char*) = nullptr;
const char* g_prefix = "";
}  // namespace

void open() {
  std::lock_guard lock(g_mutex);
  if (g_file) return;
  wchar_t base[MAX_PATH];
  DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return;
  std::wstring dir = std::wstring(base) + L"\\PhasmoMods";
  CreateDirectoryW(dir.c_str(), nullptr);
  std::wstring path = dir + L"\\mods.log";
  g_file = _wfsopen(path.c_str(), L"a", _SH_DENYWR);
}

void close() {
  std::lock_guard lock(g_mutex);
  if (g_file) fclose(g_file);
  g_file = nullptr;
}

void set_sink(void (*sink)(const char* line), const char* prefix) {
  std::lock_guard lock(g_mutex);
  g_sink = sink;
  g_prefix = prefix ? prefix : "";
}

void write(const char* fmt, ...) {
  char line[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof line, fmt, args);
  va_end(args);
  std::lock_guard lock(g_mutex);
  if (g_sink) {
    char prefixed[1100];
    snprintf(prefixed, sizeof prefixed, "%s%s", g_prefix, line);
    g_sink(prefixed);
    return;
  }
  if (!g_file) return;
  SYSTEMTIME t;
  GetLocalTime(&t);
  fprintf(g_file, "%02d:%02d:%02d.%03d [pid=%lu tid=%lu] %s\n", t.wHour, t.wMinute, t.wSecond,
          t.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId(), line);
  fflush(g_file);
}
}  // namespace pg::log
