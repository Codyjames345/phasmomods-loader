#pragma once

// Content's text in the game's languages (core/mod_api.h, version 2.2): which language the game asks
// for and which of a registered text answers it. The game's localisation (LocalisationSystem,
// sdk/systems/journal.md) looks a key up in the player's language, then in English, the language
// enum's 0 (TryGetLocalisedValue); content's text does the same with the game's own abbreviation of
// the language (GetLanguageAbbreviationText, lower case: "en", "de", "pt"), so a text names its
// languages as the game abbreviates them. Main thread.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content_registry.h"

namespace pm::host {

/// The game's abbreviation of the language value `language`, in lower case; empty when it cannot be
/// read.
std::string language_code(int32_t language);
/// The language TryGetLocalisedValue looks in: the player's (`current`) or voice recognition's, as its
/// abbreviation; empty when it cannot be read.
std::string game_language(bool current);
/// Every language the game has, as abbreviations, each once, read by stepping through the game's
/// languages with GetNextOrPreviousLanguage from the player's; empty when they cannot be read.
std::vector<std::string> game_languages();

/// The text of `text` in the language `lang` (its abbreviation, any case), else its English one.
const std::string& pick(const content::Text& text, std::string_view lang);

}  // namespace pm::host
