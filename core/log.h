#pragma once

// Append-only log at %LOCALAPPDATA%\PhasmoMods\mods.log, safe from any thread. The host writes the
// file; a mod sends its lines to the host instead (set_sink), so the suite keeps one log.
namespace pg::log {
void open();
void close();
void write(const char* fmt, ...);
// From then on, write() formats the line, puts `prefix` in front of it and hands it to `sink`
// instead of the file (a mod's lines go to the host this way).
void set_sink(void (*sink)(const char* line), const char* prefix);
}  // namespace pg::log

#define PG_LOG(...) ::pg::log::write(__VA_ARGS__)
