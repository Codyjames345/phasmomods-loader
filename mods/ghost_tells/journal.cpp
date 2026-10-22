#include "journal.h"

#include <string>
#include <string_view>

#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "journal_text.h"
#include "tells.h"

namespace pg::tells::journal {
namespace {

constexpr uintptr_t kLocalisedValueRva = 0x8D1EF0;  // LocalisationSystem.GetLocalisedValue(string key, bool)
constexpr size_t kMaxText = 4096;                   // bytes of UTF-8: room for any journal description

using Localised_t = il2cpp::String* (*)(il2cpp::String*, bool, const void*);
Localised_t g_localised = nullptr;

il2cpp::String* detour_localised(il2cpp::String* key, bool fallback, const void* method) {
  hooks::Guard guard;
  il2cpp::String* text = g_localised(key, fallback, method);
  if (!key || key->length <= 0 || !text) return text;
  const int type = journal_type(std::u16string_view(key->chars, static_cast<size_t>(key->length)));
  const char* line = type >= 0 ? tell_line(type) : nullptr;
  if (!line || !tells_on()) return text;
  char utf8[kMaxText];
  il2cpp::to_utf8(text, utf8, sizeof utf8);
  const std::string extended = std::string(utf8) + "\n Tells: " + line;
  return il2cpp::string_new(extended.c_str());
}

}  // namespace

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kLocalisedValueRva), reinterpret_cast<void*>(&detour_localised),
                       reinterpret_cast<void**>(&g_localised), "LocalisationSystem.GetLocalisedValue");
}

}  // namespace pg::tells::journal
