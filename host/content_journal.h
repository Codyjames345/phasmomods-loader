#pragma once

// The journal side of content (core/mod_api.h, version 2.2), on every journal of this client, other
// players' books included, in a contract and in the lobby alike (sdk/systems/journal.md says how the
// game's journal works, with the RVAs; the lobby's journals are the players' PCPlayer books, as a
// contract's are):
//
// - the custom ghosts this client's journals show (ContentTypes::shown: a contract's, decided when it
//   loads, else the lobby's while content is active there) give each journal, per custom ghost and
//   in the order of their type numbers, a copy of the Spirit's guess button on the evidence spread's
//   ghost page, a copy of the Spirit's entry in the Ghosts tab's list and a copy of the Spirit's page
//   after the book's last page, which the Ghosts tab then covers. So a ghost's page number is the same
//   on every client with the same content, as the game's page sync (by number) needs. Button and
//   entry go to their alphabetical place among the game's, by the objects' names; the grids' rows get
//   shorter so every ghost fits where the game's did, and the list's grid, which ships switched off
//   with its entries placed by hand, is set to that placement and switched on. The buttons join the
//   journal's arrays and lists the way its own builder makes them, and its sync controller reads them
//   again, so the evidence filter crosses them out and the players' guesses reach each other. Nothing
//   of the journal changes until every copy is made and every operation the change needs was found;
//   what fails part-way is taken out again, so the game's own journal stays as it was. In the lobby the
//   ghosts first join the collection the journal lists (ContentTypes::offer); in a contract the journal
//   must list the contract's;
// - the book's ghost pages take the order the Home tab's contents list them in, alphabetical, so a custom
//   ghost's page lies among the game's, the game's after it move along, and the contents' numbers count up;
//   their order follows from the ghosts' names alone, so every client with the same mods numbers them alike;
// - when what the journals show changes (the lobby's content stops being active, a contract loads or
//   ends, the mods stop), each journal still alive that was given ghosts under the last decision is put
//   back as the game made it before it is given the new ones: the copies taken out, its arrays, tab and
//   grids as they were, the hand placement of its list's entries (which the switched-on grid moved)
//   restored, its lists made again by its own builder and its guess sync reading the game's buttons, a
//   guess on a custom ghost cleared (UnselectGhost), and the local player's own book, when it was on a
//   page whose number changed, turned to the page its Ghosts tab opens (GoToPage, which tells every copy);
// - the book owner's page sync for a number outside this client's pages is skipped (page_syncs); any
//   other page sync reaches the game untouched, its sender check included;
// - every copied text keeps the game's TextLocaliserUI with a key the host answers through the game's
//   localisation (TryGetLocalisedValue, host/content_localised.h): the ghost's name key for its name, a
//   description key in the form of the game's own (learnt from its pages) for its page's text, and the
//   game's own key for each evidence it gives (a custom evidence's row's name key,
//   host/content_evidence.h). The game writes them in the player's language, again whenever it
//   changes; a ghost's text in that language answers, else its English;
// - a page's text is laid out as the game's: description, then strength and weakness, each with the
//   game's label in the player's language, separated as the game separates them (the layout most game
//   pages agree on in that language, else in English; with none, custom pages show no text, logged),
//   then the ability when there is one and the tells;
// - lines a mod adds to a vanilla ghost (journal_lines) follow that ghost's page text, in a private
//   room (the lobby's journals and a contract's alike) while this client's mods match the host's list;
// - Button.Press on a custom entry, while it is active and interactable, turns the journal to its page
//   instead of invoking the Spirit's entry's onClick (which would open the Spirit's page).
//
// It reaches the game only through map entries (the game API bound to the host's own contract) and the
// operations it is given. Main thread.

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "Transform.h"
#include "content_registry.h"
#include "content_types.h"
#include "held.h"

namespace pm::host {

/// What the game API has no wrapper for: the host runs these through the runtime (host/runtime_services.h), a
/// test through its fake. Each answers false when it could not.
struct JournalOps {
  bool (*clear)(void* collection);           // List<T>.Clear or Dictionary<K, V>.Clear
  bool (*initialise)(void* syncController);  // its generic base's Initialise (sdk/map/JournalController.h)
  bool (*has_clear)(void* collection);       // whether the collection's class has that Clear
  bool (*has_initialise)(void* syncController);  // whether the controller's class has that Initialise
};

/// Where the game's own page layout puts a ghost's texts, read from its pages.
struct PageLayout {
  std::string separator;           // between paragraphs
  std::string strength, weakness;  // the labels, their colon included
  bool operator==(const PageLayout&) const = default;
};

/// The layout of a game ghost page's text (its description, then "<label> <text>" paragraphs for
/// strength and weakness): the separator before its last paragraph and the labels of its last two;
/// none when it has fewer than three paragraphs or a paragraph has no label (no ':' or '：').
std::optional<PageLayout> read_layout(std::string_view pageText);
/// The layout most of `pageTexts` show, when at least two show it and no other is as common; else none.
std::optional<PageLayout> agreed_layout(const std::vector<std::string>& pageTexts);
/// A custom ghost's page text laid out as `layout` says, in the language `lang`.
std::string page_text(const content::Ghost& ghost, std::string_view lang, const PageLayout& layout);

class ContentEvidence;

class ContentJournal {
 public:
  /// The game's own text of a key: in the language the localisation was asked for, or in English
  /// (`english`); none when it has none.
  using GameText = std::function<std::optional<std::string>(std::string_view key, bool english)>;

  /// `evidence` gives a held custom evidence's name key for a custom ghost's page (host/
  /// content_evidence.h); without it such an evidence's line is hidden.
  ContentJournal(content::Registry& registry, ContentTypes& types, JournalOps ops,
                 const ContentEvidence* evidence = nullptr)
      : registry_(registry), types_(types), ops_(ops), evidence_(evidence) {}

  /// The map entries this needs that did not resolve, ", "-separated; empty when every one did. Until
  /// they all resolve the host withholds custom content. Main thread, after the game API is bound.
  std::string missing() const;
  /// Creates its hooks (not enabled), for this instance; one instance hooks at a time. False when one
  /// could not be made (the game API logs which).
  bool create_hooks();
  /// Whether `page` means another page here than in the book as the game made it: from the first page the custom ghosts moved on,
  /// and anything past the book's end. A client without the mods would show the wrong page, or none, for such a number.
  bool custom_page(void* journal, int32_t page) const;
  /// The same for a page sync sent on `view` (host/lobby_gate.h). A view no live journal here holds has no custom pages.
  bool custom_page_on(void* view, int32_t page) const;
  /// Notes the journals already awake, as their Awake's hook would have: the hooks come only once the player has agreed, and
  /// the local player's book may have woken while the notice waited.
  void note_existing();
  /// How many live journals it knows, for tests and logs.
  size_t journals_known() const { return journals_.size(); }

  /// Every frame, after the ghost types' tick and the evidence's: forgets journals that went with their
  /// scene, puts back each journal given ghosts under a decision that no longer stands, reads the
  /// game's page layout once from each new journal, has the pages with lines written again once each
  /// time this client enters a private room, and gives each journal the custom ghosts shown now once
  /// their entries are in its collection (in the lobby, offering them to it first), once per decision.
  /// `privateRoom`: vanilla lines show only there.
  void tick(bool privateRoom);
  /// Puts every journal still alive back as the game made it, forgets them all and stops answering, as
  /// before the mods stop; the ghost types' withdraw comes first, so their lists are made again without
  /// the custom ghosts.
  void withdraw();

  // What each hook does; each may be called on its own.

  /// JournalController.Awake, after: the journal is one to extend.
  void after_awake(void* journal);
  /// LocalisationSystem.TryGetLocalisedValue, asked by the host's hook on it (host/content_localised.h):
  /// the text of `key` in the language `lang` when this answers it (a custom ghost's page, or a vanilla
  /// page with lines after its own text); `game` gives the game's own texts. Else none.
  std::optional<std::string> text_for_key(std::string_view key, std::string_view lang, const GameText& game) const;
  /// Whether any key is answered now, a cheap test before reading the key.
  bool answers() const;
  /// Whether any custom entry exists, a cheap test before Button.Press looks.
  bool has_entries() const { return !entries_.empty(); }
  /// Button.Press, before: for a custom entry, what Press does with its onClick (nothing unless the
  /// button is active and interactable) is turning the journal to its page; true then, and the game's
  /// Press must not run. False for any other button.
  bool press(void* button);
  /// JournalController.PageSync, before: whether the game's page sync may run for `page`, sent with
  /// the message info at `info`. The game first checks that the sender owns the book; a sender who does
  /// not is reported by the game and changes nothing, so that check always runs as it would without
  /// the mods. False only when this client is in a private room, the sender is positively the book's
  /// owner (PhotonMessageInfo.Sender is the journal's view's owner), and the number is outside this
  /// client's pages: a copy of a book without the custom pages gets that from its owner (a joiner
  /// replaying the buffered syncs before its content is active, a client a moment behind), and the
  /// game would blank the book and throw there (sdk/systems/journal.md). The copy keeps its page until
  /// the next turn. Logged once. True in every other case: a page inside the book, a sender who is not
  /// the owner, either one not known, out of a private room.
  bool page_syncs(void* journal, int32_t page, const void* info);

  /// The page a custom entry's Button leads to, or -1 (for tests and logs).
  int32_t page_of(void* button) const;
  /// Every journal this client holds now (as JournalController.Awake made them known), alive.
  std::vector<void*> journals() const;

 private:
  // A list entry as placed by hand, before its list's grid was switched on: what the grid sets. Or a
  // contents row or its number, before the rows got shorter.
  struct Placement {
    pm::api::Held entry;  // its RectTransform
    phasmo::Vector2 anchorMin, anchorMax, sizeDelta, position;
    int32_t sibling;  // its place among its parent's children
  };
  // A contents row's page number: its text (a TextMeshProUGUI) and what it said.
  struct ContentsNumber {
    pm::api::Held text, said;
  };
  // What extend changed in a journal, to put it back as the game made it.
  struct Extension {
    unsigned under;                     // ContentTypes::shown_decisions when it was made
    pm::api::Held buttons, pages;       // the journal's arrays before
    pm::api::Held ghostsTab;
    float tabMax;
    pm::api::Held grid;                 // the guess buttons' grid (a Transform)
    phasmo::Vector2 gridCell;
    pm::api::Held list;                 // the Ghosts tab's list (a Transform)
    bool listOn;                        // its grid was switched on
    phasmo::Vector2 listCell;
    int32_t listConstraint, listCount;
    std::vector<Placement> placements;  // its entries as placed by hand, when its grid was off, and the contents' rows
    std::vector<pm::api::Held> made;    // every copy
    std::vector<int32_t> types;         // the custom ghost types it was given
    int32_t firstMoved = 0;             // the first page whose number means another page than the game's
    std::vector<ContentsNumber> numbers;  // the contents' numbers it wrote
  };
  std::set<std::string> ghostNameKeys_;  // the game's ghosts' name keys, which mark a contents page's ghost rows
  struct Journal {
    pm::api::Held journal;
    bool learned = false;    // its page layout was read
    bool fit = false;        // and successfully: it has ghost pages where the game's are
    bool refreshed = false;  // its pages with lines were written in this private room, under this agreement
    std::optional<unsigned> tried;         // the decision it was last given ghosts under, or failed to be
    std::optional<Extension> extension;    // what giving them changed
  };
  struct Entry {
    pm::api::Held button;  // the entry's Button
    pm::api::Held journal;
    int32_t page;
  };

  bool learn(void* journal);  // the page layout's places and keys, from this journal's pages
  void refresh_lines(void* journal);
  bool extend(Journal& j, const std::map<int32_t, ContentTypes::HeldGhost>& ghosts, unsigned under);
  void restore(Journal& j);  // puts back what extend changed
  std::string description_key(int32_t type) const;
  std::optional<PageLayout> layout(const GameText& game) const;

  content::Registry& registry_;
  ContentTypes& types_;
  JournalOps ops_;
  const ContentEvidence* evidence_;
  std::vector<Journal> journals_;
  std::vector<Entry> entries_;
  bool privateRoom_ = false;
  unsigned decisions_ = 0;  // Registry::agreement_decisions when the pages with lines were last written
  unsigned shown_ = 0;      // ContentTypes::shown_decisions the custom keys are answered for
  // Learnt from the game's pages: where a page's texts are among its children, the description key's
  // form, each vanilla type's description key and each evidence's key.
  int32_t titleAt_ = -1, descriptionAt_ = -1, evidenceAt_[3] = {-1, -1, -1};
  std::string descPrefix_, descSuffix_;
  bool descForm_ = false;
  std::map<int32_t, std::string> vanillaDescriptions_;  // by type
  std::map<int32_t, std::string> evidenceKeys_;         // by evidence value
  std::map<std::string, int32_t, std::less<>> customKeys_;  // custom description keys answered, by type
  bool languagesLogged_ = false;
  mutable bool layoutLogged_ = false;
  bool pageSkipLogged_ = false;  // that a page sync beyond the book was skipped was logged
};

}  // namespace pm::host
