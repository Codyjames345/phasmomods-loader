#include "log.h"

#include <windows.h>

#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <string>

#include "mod_api.h"

namespace pm::log {
static_assert(static_cast<uint32_t>(Level::Trace) == PM_LOG_TRACE && static_cast<uint32_t>(Level::Info) == PM_LOG_INFO &&
                  static_cast<uint32_t>(Level::Alert) == PM_LOG_ALERT,
              "Level numbers the levels as the host contract's PmLogLevel does");
namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
void (*g_sink)(const char*) = nullptr;
void (*g_levelSink)(uint32_t, const char*) = nullptr;
const char* g_prefix = "";

void write_line(Level level, const char* line) {
  std::lock_guard lock(g_mutex);
  if (g_sink || g_levelSink) {
    char prefixed[1100];
    if (g_levelSink) {
      snprintf(prefixed, sizeof prefixed, "%s%s", g_prefix, line);
      g_levelSink(static_cast<uint32_t>(level), prefixed);
    } else if (level == Level::Info) {
      snprintf(prefixed, sizeof prefixed, "%s%s", g_prefix, line);
      g_sink(prefixed);
    } else {
      // A host older than log_level writes every line at info: the level goes into the text.
      snprintf(prefixed, sizeof prefixed, "%s%s %s", g_prefix, level_word(level), line);
      g_sink(prefixed);
    }
    return;
  }
  if (!g_file) return;
  SYSTEMTIME t;
  GetLocalTime(&t);
  fprintf(g_file, "%02d:%02d:%02d.%03d [pid=%lu tid=%lu] %s %s\n", t.wHour, t.wMinute, t.wSecond,
          t.wMilliseconds, GetCurrentProcessId(), GetCurrentThreadId(), level_word(level), line);
  fflush(g_file);
}
}  // namespace

Level level_of(uint32_t number) {
  return number <= static_cast<uint32_t>(Level::Alert) ? static_cast<Level>(number) : Level::Info;
}

const char* level_word(Level level) {
  switch (level) {
    case Level::Trace: return "TRACE";
    case Level::Debug: return "DEBUG";
    case Level::Warn: return "WARN ";
    case Level::Error: return "ERROR";
    case Level::Alert: return "ALERT";
    case Level::Info: break;
  }
  return "INFO ";
}

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

void set_level_sink(void (*sink)(uint32_t level, const char* line)) {
  std::lock_guard lock(g_mutex);
  g_levelSink = sink;
}

void write(const char* fmt, ...) {
  char line[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof line, fmt, args);
  va_end(args);
  write_line(Level::Info, line);
}

void write_at(Level level, const char* fmt, ...) {
  char line[1024];
  va_list args;
  va_start(args, fmt);
  vsnprintf(line, sizeof line, fmt, args);
  va_end(args);
  write_line(level, line);
}
}  // namespace pm::log
