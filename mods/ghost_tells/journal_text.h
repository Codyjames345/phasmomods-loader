#pragma once

#include <string>

#include "tells.h"
#include "text_table.h"

// What each told ghost's journal page says about its new tells: one line after the page's own text,
// added through the content layer's journal_lines (mod.cpp), which shows it in a private room in the
// player's language when the line has it, else English. English only.
namespace pm::tells {

/** The table key of the line naming a told ghost's new tells (mods/ghost_tells/i18n/default.json); null for any other. */
inline const char* tell_line_key(int told) {
  switch (told) {
    case kSpirit: return i18n::key("tells.spirit");
    case kMare: return i18n::key("tells.mare");
    case kShade: return i18n::key("tells.shade");
    case kYurei: return i18n::key("tells.yurei");
    case kGoryo: return i18n::key("tells.goryo");
    case kOnryo: return i18n::key("tells.onryo");
    default: return nullptr;
  }
}

/** The line naming a told ghost's new tells, from the mod's table; empty for any other. */
inline std::string tell_line(int told) {
  const char* key = tell_line_key(told);
  return key ? i18n::text(key) : std::string();
}

}  // namespace pm::tells
