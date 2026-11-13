#pragma once

#include <filesystem>
#include <initializer_list>
#include <string>

#include "check.h"
#include "text_table.h"

// The shipped string tables a test reads (core/text_table.h), so it checks the text players see. A test process has one
// table, as a DLL does: a test of the loader's code and a mod's loads both, and a key the two share fails the test. Paths
// are relative to the source folder: "i18n/default.json" for the loader, "mods/<folder>/i18n/default.json" for a mod.
inline void use_text(std::initializer_list<const char*> tables) {
  // A Table holds a mutex and is not copied, so the merged table is built straight into the process's
  // own one. It is emptied first, as the fresh table that used to be copied over was.
  pm::i18n::Table& all = pm::i18n::table();
  std::string why;
  all.parse("{}", why);
  for (const char* table : tables) {
    pm::i18n::Table one;
    const bool read = one.load(std::filesystem::path(PM_TEXT_DIR) / table, why);
    if (!read) std::printf("FAIL %s: %s\n", table, why.c_str());
    CHECK(read);
    const bool merged = all.merge(one, why);
    if (!merged) std::printf("FAIL %s: %s\n", table, why.c_str());
    CHECK(merged);
  }
}
