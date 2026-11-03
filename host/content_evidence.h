#pragma once

// The custom evidence side of content (core/mod_api.h, version 2.2), following sdk/systems/evidence.md:
//
// - a custom ghost gives a custom evidence through the game's own evidence pick, with nothing done
//   here: host/content_types.h puts the evidence's number into the ghost's GhostData, the pick
//   (GhostController.PickEvidence) withholds it as it withholds the game's own, and the game's sync
//   (GhostInfo.SyncValues, SyncEvidence) carries it to every client as a number;
// - the custom evidence held is what the custom ghosts this client's journals show list
//   (ContentTypes::shown: a contract's, decided when it loads and held until it ends, else the lobby's
//   while content is active there);
// - on every journal of this client, other players' books included, in a contract and in the lobby
//   alike, each held custom evidence gets a
//   copy of the game's Spirit Box row after the game's rows on the evidence page: its evidence number,
//   its name and the line under it, each with the game's TextLocaliserUI and a key the host answers
//   through the game's localisation (host/content_localised.h) in the player's language, else English. The
//   keys take the game's form, learnt from its rows: the part every row's name key begins with, then
//   the evidence's id, and for the line the suffix every row's line key adds to its name key. The row
//   joins the journal's evidence buttons and its evidence sync controller reads them again, so it is
//   marked, synced and filters the ghost buttons as the game's rows do; the column's spacing shrinks so
//   every row fits where the game's did. Nothing of the journal changes until every copy is made and
//   every operation the change needs was found; what fails part-way is taken out again. Two held
//   evidence whose keys meet get no row (logged). When what the journals show changes, a journal still
//   alive that got rows under the last decision is put back as the game made it first: the copies
//   taken out, its evidence buttons and the column's spacing as they were, its evidence sync reading
//   the game's rows again, and each custom evidence number taken off its found and ruled-out lists by
//   the game's own RemoveEvidence, so no game ghost stays crossed out by a mark whose row is gone;
// - a custom ghost's journal page (host/content_journal.h) names such an evidence by its name key, as
//   the game's pages name theirs by their rows' keys.
//
// It reaches the game only through map entries (the game API bound to the host's own contract) and the
// operations it is given. Main thread.

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "content_journal.h"
#include "content_registry.h"
#include "content_types.h"
#include "held.h"

namespace pm::host {

class ContentEvidence {
 public:
  /// A custom evidence the contract holds.
  struct HeldEvidence {
    std::string id;
    const content::Evidence* def;  // as registered, valid while the content does not change
  };

  /// `ops`: the journal operations the game API has no wrapper for (host/content_journal.h); this uses
  /// the sync controller's Initialise and the check that its class has one.
  ContentEvidence(content::Registry& registry, ContentTypes& types, JournalOps ops)
      : registry_(registry), types_(types), ops_(ops) {}

  /// The map entries this needs that did not resolve, ", "-separated; empty when every one did. Until
  /// they all resolve the host withholds custom content. Main thread, after the game API is bound.
  std::string missing() const;

  /// Every frame, after the ghost types' tick and before the journal's: the custom evidence held now,
  /// the key form read from each new journal of `journals` (the journals the host's content journal
  /// knows) until one shows it, each journal that got rows under a decision that no longer stands put
  /// back, and each journal given its rows once per decision, once some is held and the form is known
  /// (a journal met earlier waits for it).
  void tick(const std::vector<void*>& journals);
  /// Puts every journal still alive back as the game made it, forgets them all and stops answering, as
  /// before the mods stop.
  void withdraw();

  /// The custom evidence held now, by number; empty when none is (as of the last tick).
  const std::map<int32_t, HeldEvidence>& held() const { return held_; }
  /// The name key of the held custom evidence `value` once the key form was read, else none: what its
  /// row's name and a ghost page's evidence line show.
  std::optional<std::string> name_key(int32_t value) const;

  // What the host's localisation hook asks (host/content_localised.h); it may be called on its own.

  /// LocalisationSystem.TryGetLocalisedValue: the name or the line of a held custom evidence whose key
  /// is `key`, in the language `lang` (else English); nothing for any other key.
  std::optional<std::string> text_for_key(std::string_view key, std::string_view lang) const;
  /// Whether any key is answered now, a cheap test before reading the key.
  bool answers() const { return form_ && !held_.empty(); }

 private:
  // What extend changed in a journal, to put it back as the game made it.
  struct Extension {
    unsigned under;                   // ContentTypes::shown_decisions when it was made
    pm::api::Held buttons;            // the journal's evidence buttons before
    pm::api::Held layout;             // the column's layout group
    float spacing;                    // its spacing before
    std::vector<pm::api::Held> made;  // every copy
    std::vector<int32_t> values;      // the custom evidence numbers it got rows for
  };
  struct Journal {
    pm::api::Held journal;
    bool learned = false;               // its rows were read for the key form (when it was not known yet)
    std::optional<unsigned> tried;      // the decision it was last given rows under, or failed to be
    std::optional<Extension> extension;  // what giving them changed
  };

  bool learn(void* journal);  // the key form, from this journal's rows; false when they show none
  bool extend(Journal& j, unsigned under);
  void restore(Journal& j);  // puts back what extend changed
  void drop_colliding();  // takes held evidence whose keys meet another's out of held_ (logged)
  std::string line_key(const std::string& id) const;

  content::Registry& registry_;
  ContentTypes& types_;
  JournalOps ops_;
  std::vector<Journal> journals_;
  std::map<int32_t, HeldEvidence> held_;
  bool form_ = false;          // the key form was read
  bool formLogged_ = false;    // that the rows show none was logged
  std::set<std::string> collidedLogged_;  // the evidence whose colliding keys were logged
  std::string namePrefix_;     // what every game row's name key begins with ("Journal2_")
  std::string lineSuffix_;     // what every game row's line key adds to its name key ("_Tooltip")
};

}  // namespace pm::host
