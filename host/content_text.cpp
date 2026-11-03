#include "content_text.h"

#include <algorithm>

#include "LocalisationSystem.h"
#include "runtime.h"

namespace pm::host {
namespace {

std::string lower(std::string text) {
  for (char& c : text)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return text;
}

// The most languages read: a bound on the walk, far past the game's 28.
constexpr int kMaxLanguages = 256;

}  // namespace

std::string language_code(int32_t language) {
  return lower(pm::api::to_utf8(phasmo::LocalisationSystem::GetLanguageAbbreviationText(language)));
}

std::string game_language(bool current) {
  if (!phasmo::LocalisationSystem::GetLanguageAbbreviationText.available()) return "";
  return language_code(current ? phasmo::LocalisationSystem::get_language()
                               : phasmo::LocalisationSystem::get_voiceLanguage());
}

std::vector<std::string> game_languages() {
  if (!phasmo::LocalisationSystem::GetNextOrPreviousLanguage.available() ||
      !phasmo::LocalisationSystem::GetLanguageAbbreviationText.available())
    return {};
  // GetNextOrPreviousLanguage answers the value itself past either end, so step down to the first,
  // then up to the last.
  int32_t first = phasmo::LocalisationSystem::get_language();
  for (int i = 0; i < kMaxLanguages; ++i) {
    const int32_t before = phasmo::LocalisationSystem::GetNextOrPreviousLanguage(first, -1, false);
    if (before == first) break;
    first = before;
  }
  std::vector<std::string> out;
  for (int32_t at = first, i = 0; i < kMaxLanguages; ++i) {
    const std::string code = language_code(at);
    if (!code.empty() && std::find(out.begin(), out.end(), code) == out.end()) out.push_back(code);
    const int32_t next = phasmo::LocalisationSystem::GetNextOrPreviousLanguage(at, 1, false);
    if (next == at) break;
    at = next;
  }
  return out;
}

const std::string& pick(const content::Text& text, std::string_view lang) {
  const std::string want = lower(std::string(lang));
  const std::pair<std::string, std::string>* english = nullptr;
  for (const auto& entry : text) {
    const std::string have = lower(entry.first);
    if (!want.empty() && have == want) return entry.second;
    if (have == "en") english = &entry;
  }
  static const std::string kNone;
  return english ? english->second : kNone;
}

}  // namespace pm::host
