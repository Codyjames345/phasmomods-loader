#pragma once

// The host's one hook on the game's localisation (LocalisationSystem.TryGetLocalisedValue, which
// GetLocalisedValue and every TextLocaliserUI call) for content. The host's hooks on one method are made
// from one module, so a second hook of the same method would be the same detour, which the host's hook
// table refuses: the content sides that answer keys therefore share this hook. For each key it asks, in
// this order, the ghost types' name keys (host/content_types.h), the journal's page texts
// (host/content_journal.h, which may ask the game's own text through the rest of the chain) and the
// custom evidence's texts (host/content_evidence.h); the first answer is written, otherwise the game
// answers. Main thread.

#include <optional>
#include <string>
#include <string_view>

#include "content_evidence.h"
#include "content_journal.h"
#include "content_types.h"

namespace pm::host {

class ContentLocalised {
 public:
  ContentLocalised(ContentTypes& types, ContentJournal& journal, ContentEvidence& evidence)
      : types_(types), journal_(journal), evidence_(evidence) {}

  /// Creates the hook (not enabled), for this instance; one instance hooks at a time. False when it
  /// could not be made (the game API logs why).
  bool create_hook();
  /// Whether any side answers a key now, a cheap test before reading the key.
  bool answers() const;
  /// The text of `key` in the language `lang` from the first side that answers it, `game` giving the
  /// game's own texts to the journal's side; none when no side does.
  std::optional<std::string> answer(std::string_view key, std::string_view lang,
                                    const ContentJournal::GameText& game) const;

 private:
  ContentTypes& types_;
  ContentJournal& journal_;
  ContentEvidence& evidence_;
};

}  // namespace pm::host
