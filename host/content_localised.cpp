#include "content_localised.h"

#include "LocalisationSystem.h"
#include "content_text.h"
#include "game_api.h"
#include "guarded.h"
#include "runtime.h"

namespace pm::host {
namespace {

ContentLocalised* g_self = nullptr;  // the instance whose hook is made

// TryGetLocalisedValue looks in the player's language when `current`, else in voice recognition's
// (decompiled, build 25811160); an answer is in the same one.
bool detour_localised(pm::api::Original<phasmo::LocalisationSystem::TryGetLocalisedValue> original, void* key,
                      void** text, bool current) {
  static bool logged = false;
  void* answer = nullptr;
  if (g_self && key && text && g_self->answers())
    guarded(logged, "answering a content text", [&] {
      // The game's own text: in the player's language through the rest of the chain, as the call asked;
      // in English through the game's own look-up with English asked, the one TryGetLocalisedValue
      // tail-jumps to (LocalisationSystem.LookUp).
      const auto game = [&](std::string_view k, bool english) -> std::optional<std::string> {
        void* out = nullptr;
        const bool found =
            english ? phasmo::LocalisationSystem::LookUp(phasmo::Language::English(), pm::api::make_string(k), &out)
                    : original(pm::api::make_string(k), &out, current);
        if (!found || !out) return std::nullopt;
        return pm::api::to_utf8(out);
      };
      if (const auto t = g_self->answer(pm::api::to_utf8(key), game_language(current), game))
        answer = pm::api::make_string(*t);
    });
  if (!answer) return original(key, text, current);
  // The out slot may be a managed field: written through the GC write barrier, as the game's
  // TryGetLocalisedValue writes it.
  pm::api::detail::store_reference(text, 0, answer);
  return true;
}

}  // namespace

bool ContentLocalised::create_hook() {
  // The hook is created, not enabled, so its detour cannot run before g_self names this instance; an
  // instance whose hook failed is never named.
  if (!pm::api::hook<phasmo::LocalisationSystem::TryGetLocalisedValue>(&detour_localised, "content: texts"))
    return false;
  g_self = this;
  return true;
}

bool ContentLocalised::answers() const { return types_.answers_names() || journal_.answers() || evidence_.answers(); }

std::optional<std::string> ContentLocalised::answer(std::string_view key, std::string_view lang,
                                                    const ContentJournal::GameText& game) const {
  if (types_.answers_names())
    if (auto name = types_.name_for_key(key, lang)) return name;
  if (journal_.answers())
    if (auto text = journal_.text_for_key(key, lang, game)) return text;
  if (evidence_.answers()) return evidence_.text_for_key(key, lang);
  return std::nullopt;
}

}  // namespace pm::host
