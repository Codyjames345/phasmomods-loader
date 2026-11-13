#pragma once

#include <cstdint>

// Append-only log at %LOCALAPPDATA%\PhasmoMods\mods.log, safe from any thread. The host writes the
// file; a mod sends its lines to the host instead (set_sink), so the suite keeps one log. Each line
// carries its level (SMAPI's LogLevel): "12:00:00.000 [pid=1 tid=2] WARN  text".
namespace pm::log {

// How much a line matters, in the order and numbering of the host contract's PmLogLevel.
enum class Level : uint32_t { Trace, Debug, Info, Warn, Error, Alert };

// The level a contract number names; a number past Alert is Info.
Level level_of(uint32_t number);
// The level's word in mods.log, five letters wide as SMAPI's are: "TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "ALERT".
const char* level_word(Level level);

void open();
void close();
// One line at the info level.
void write(const char* fmt, ...);
// One line at `level`.
void write_at(Level level, const char* fmt, ...);
// From then on, write() formats the line, puts `prefix` in front of it and hands it to `sink`
// instead of the file (a mod's lines go to the host this way). A line at a level other than info
// goes to the level sink when one is set (set_level_sink), else to `sink` with its level's word
// after the prefix.
void set_sink(void (*sink)(const char* line), const char* prefix);
// The sink for a mod's lines with their level (the host contract's log_level), or null for none.
void set_level_sink(void (*sink)(uint32_t level, const char* line));
}  // namespace pm::log

#define PM_LOG(...) ::pm::log::write(__VA_ARGS__)
// One line at a level: PM_LOG_AT(::pm::log::Level::Warn, "...", ...).
#define PM_LOG_AT(level, ...) ::pm::log::write_at(level, __VA_ARGS__)
