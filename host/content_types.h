#pragma once

// The game side of content's ghost types (core/mod_api.h, version 2.2): while content is active it
// puts each registered custom ghost into the game as a ghost type, and on every client it runs the
// content rules of the type the contract's ghost acts as. It follows sdk/systems/ghost-types.md:
//
// - the contract's GhostCollection gets a GhostData for each custom ghost (its type, a name key in the
//   game's own form and its three evidence), each in its alphabetical place, the game's ghosts sorted
//   by the names in their name keys and the custom ones by their English names;
// - the game's localisation (TryGetLocalisedValue, which GetLocalisedValue and every TextLocaliserUI
//   call) answers those name keys with the ghost's name in the player's language, through the host's
//   one hook on it (host/content_localised.h);
// - a custom ghost's identity is its custom type and its AI's behaviour type the game's "no type",
//   set in GhostAI.Init and after GhostInfo.SyncValuesNetworked on every client, and the host's
//   GhostInfo.SyncValues sends the custom type, as docs/custom-ghosts.md describes;
// - the game's type roll (GhostController.PickType) gives a weight of 0 to each custom ghost whose definition
//   says it may not be rolled now (PmGhostDef.rollable), before it draws;
// - the Mimic's copy list, as the game built it, gets the custom types after it (not one marked
//   PM_GHOST_NO_MIMIC_COPY, nor one that may not be rolled now); a copy of one is
//   recorded as the acting-as type and the behaviour type set back to "no type"
//   (sdk/runtime/content_acting.h);
// - a running rule's hunt speed is written into the hunt state, the base and the line-of-sight
//   speed-up, before the game's speed update reads them, never the speed itself;
// - outside a contract (the lobby), while content is active in a private room, the custom ghosts are
//   offered: each list that offers ghost types there and is given to offer (the difficulty menu's
//   collection, host/content_difficulty.h, and the collection each lobby journal lists,
//   host/content_journal.h) gets the same entries, and loses this client's numbers again when content
//   stops being active there, a contract loads, or on withdraw_offer.
//
// Whether a contract has custom content is decided once, when it loads (contract_loaded turns true), as
// the content state stands then (active: they go in; otherwise they stay out, and any of this client's
// numbers left in its collection come out). The decision holds until the contract ends: nothing is
// withdrawn in between, and the rules of the contract's custom types keep running even when content
// drops to pending or off, while the registry keeps answering the contract's custom numbers and ids
// (Registry::hold_contract). The entries leave at the contract's end and when the mods stop. The lobby
// follows the room instead: its offer starts each time content turns active there and ends each time it
// stops, so the lobby's journals show the custom ghosts before and after a contract while content is
// active (shown). A withdrawal a List operation refused is kept and retried every frame while no
// contract holds content (in the lobby, whatever the content state), logged. It reaches the game only
// through map entries (the game API, bound to the host's own contract) and the List operations it is
// given. Main thread.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content_acting.h"
#include "content_registry.h"
#include "held.h"

namespace pm::host {

/// The game's own List<T> methods the game API has no wrapper for, and one write to a Dictionary: the host
/// runs them through the runtime (host/runtime_services.h), a test through the fake. Each answers false when it
/// could not.
struct ListOps {
  bool (*insert)(void* list, int32_t index, void* item);  // List<T>.Insert of a reference
  bool (*removeAt)(void* list, int32_t index);            // List<T>.RemoveAt
  bool (*addValue)(void* list, int32_t value);            // List<T>.Add of a 32-bit value (an enum)
  // The value of the key `key` already in a Dictionary<K, int> whose K is a 4-byte value (an enum), written in
  // place; false, touching nothing, when the key is not there or the object is no such dictionary.
  bool (*setValue)(void* dict, int32_t key, int32_t value) = nullptr;
};

class ContentTypes {
 public:
  ContentTypes(content::Registry& registry, content::Acting& acting, ListOps lists)
      : registry_(registry), acting_(acting), lists_(lists) {}

  /// The map entries this needs that did not resolve, ", "-separated; empty when every one did. Until
  /// they all resolve the host withholds custom content (Registry::withhold). Main thread, after the
  /// game API is bound.
  std::string missing() const;
  /// Creates its hooks on the game's methods (not enabled), for this instance; one instance hooks at a
  /// time. False when one could not be made (the game API logs which).
  bool create_hooks();

  /// Every frame, after the room's content state is known: a contract that has just loaded is decided
  /// on `state`, one that has ended gives its entries back, the lobby's offer starts or ends as
  /// content is active there or not (`privateRoom` and `state`, no contract loaded), unfinished
  /// withdrawals are retried while no contract holds content, this client's view of the contract's
  /// ghost goes to the acting-as dispatch (no ghost outside a private room, so no rules run there),
  /// and the running rules tick. `master`: this client runs the ghost's AI. `roomVerified`, the master's to say: everyone in the
  /// room is verified. A contract's start waits for that, but a player can join in the round trip after it goes
  /// (sdk/systems/photon.md). A master's contract that loads with one has no custom ghosts, and so none spawns, since they may not
  /// have them. Another client's holds the entries all the same, which is harmless with none spawned.
  void tick(content::State state, bool privateRoom, bool master, bool roomVerified = true);
  /// Takes every custom entry out of the contract's collection, the Mimic's list and every list the
  /// lobby's offer went into, and stops the running rules, as before the mods stop. Nothing is held or
  /// offered after it.
  void withdraw();

  /// Puts the lobby's custom ghosts (offered) into `list`, a List<GhostData> that offers ghost types
  /// outside a contract (the difficulty menu's GhostCollection.ghosts, a lobby journal's), as into the
  /// contract's: a GhostData each, in its alphabetical place, its name key answered while the offer
  /// lasts. Only while the lobby's offer stands (tick starts it; else nothing is added). The list loses
  /// this client's numbers again when the offer ends. True when every offered ghost is in the list, and
  /// there is one at least. Main thread.
  bool offer(void* list);
  /// Takes this client's custom numbers out of `list` now (a removal a List operation refused is
  /// retried as for the contract's collection); the offer itself stands for the other lists. Main
  /// thread.
  void withdraw_offer(void* list);
  /// Whether the running contract was decided (its custom ghosts, or none, in held()); false outside a
  /// contract.
  bool decided() const { return decided_; }
  /// Whether a contract is loaded: its GhostController is alive. The one signal the host decides a
  /// contract on: tick decides the contract's content on the frame it turns true, and this client's
  /// match with the host (sdk/runtime/mod_agreement.h) holds from that frame (host/suite.cpp asks
  /// this before the content ticks). Main thread.
  static bool contract_loaded();

  // What each hook does, after or before the game's own code; each may be called on its own.

  /// LocalisationSystem.TryGetLocalisedValue, asked by the host's hook on it (host/content_localised.h):
  /// the name, in the language `lang` (else English; host/content_text.h), of the custom ghost whose
  /// name key is `key`, while the contract holds its entry; nothing for any other key.
  std::optional<std::string> name_for_key(std::string_view key, std::string_view lang = "en") const;

  // A custom ghost as the contract holds it, taken when the contract was decided.
  struct HeldGhost {
    std::string id;
    content::Text name;
    int32_t evidence[3];
    bool keepsLastEvidence = false;     // PM_GHOST_KEEPS_LAST_EVIDENCE
    bool noMimicCopy = false;           // PM_GHOST_NO_MIMIC_COPY
    bool (*rollable)(void*) = nullptr;  // PmGhostDef.rollable, asked with `user`
    void* user = nullptr;
  };
  /// Whether the held ghost `h` may haunt a contract now: its definition's rollable, true without one.
  static bool rollable(const HeldGhost& h) { return !h.rollable || h.rollable(h.user); }
  /// The contract's custom ghosts by type; empty when it has none or before it is decided.
  const std::map<int32_t, HeldGhost>& held() const { return held_; }
  /// The lobby's custom ghosts by type while its offer stands; empty otherwise.
  const std::map<int32_t, HeldGhost>& offered() const { return offered_; }
  /// The custom ghosts this client's journals show now, by type: the contract's once it is decided,
  /// else the lobby's offer.
  const std::map<int32_t, HeldGhost>& shown() const { return decided_ ? held_ : offered_; }
  /// Grows each time shown() starts answering for another decision (a contract decided or ended, the
  /// lobby's offer started or ended, the mods stopping), so what was shown under the last can be put
  /// back first.
  unsigned shown_decisions() const { return shown_; }
  /// The name key of the custom type `type` once its entry is in the contract's collection, else none.
  std::optional<std::string> name_key(int32_t type) const;
  /// The contract's GhostCollection, or null.
  void* collection() const;
  /// Whether any name key is answered now, a cheap test before reading the key: only while a contract
  /// holds custom entries.
  bool answers_names() const { return !keys_.empty(); }
  /// GhostAI.Init, before: a custom identity's behaviour type in `traits` becomes "no type".
  void before_init(void* traits) const;
  /// GhostInfo.SyncValues, before: for a custom identity the behaviour type in `traits` becomes the
  /// identity, which SyncValues sends as the ghost's type; returns the behaviour type to put back
  /// after it, or -1 when nothing changed.
  int32_t before_sync_send(void* traits) const;
  /// GhostInfo.SyncValues, after: puts back what before_sync_send changed.
  void after_sync_send(void* traits, int32_t behaviour) const;
  /// GhostInfo.SyncValuesNetworked, after: a custom identity's behaviour type becomes "no type".
  void after_sync_values(void* info) const;
  /// GhostController.BuildMimicCopies, after: the contract's custom types join the list the game built, but those
  /// marked PM_GHOST_NO_MIMIC_COPY and those whose rollable answers false now.
  void after_build_mimic_copies(void* controller);
  /// GhostController.MimicCopy, after (on the master): the copy is the acting-as type, and a custom
  /// one's behaviour type becomes "no type".
  void after_mimic_copy(void* controller);
  /// GhostController.PickEvidence, before (on the master): when the rolled behaviour type is a custom
  /// ghost that keeps its last evidence (PM_GHOST_KEEPS_LAST_EVIDENCE), the pick is followed until
  /// after_pick_evidence.
  void before_pick_evidence(void* controller);
  /// GhostData.GetEvidenceList, after: inside a followed pick, once its list is made, the controller's
  /// behaviour type reads as the Goryo's for the rest of the pick, so the game's own removals skip the
  /// last evidence as they do for the Goryo (sdk/systems/evidence.md). Nothing outside a followed pick.
  void after_evidence_list();
  /// GhostController.PickEvidence, after (also when it throws): the custom behaviour type is back.
  void after_pick_evidence();
  /// GhostController.PickType, before (on the master, inside GhostController.Spawn): in `weights`, the roll's
  /// Dictionary<GhostType, int>, each of the contract's custom ghosts whose definition says it may not be rolled
  /// now (PmGhostDef.rollable) gets a weight of 0, so the game's draw, which counts positive weights only, never
  /// picks it while another has weight. Every other weight stays the game's. Null, or a type the dictionary lacks,
  /// changes nothing.
  void before_pick_type(void* weights);
  /// GhostHuntState.UpdateSpeed, before (on the master): the running rules' hunt speed, written as the
  /// hunt's base and line-of-sight speed-up; the game's own base again once no rule asks.
  void before_update_speed(void* hunt);

 private:
  // A withdrawal from a list not finished yet: the types [first, end) still to take out.
  struct Pending {
    pm::api::Held list;
    int32_t first, end;
    bool values;  // a List<GhostType> (the Mimic's), else a List<GhostData>
  };

  void* collection_list() const;  // the contract's GhostCollection.ghosts, or null
  void begin_contract(void* controller, bool active);
  void end_contract();
  void begin_offer();
  void end_offer();  // takes this client's numbers out of every list offered to
  // The active content's custom ghosts by type, as a contract holds them; a ghost giving a custom
  // evidence not in play is left out.
  std::map<int32_t, HeldGhost> active_ghosts() const;
  // Adds the entries of `ghosts` the list lacks, and answers the name key of each one there.
  void place(void* list, const std::map<int32_t, HeldGhost>& ghosts);
  // Takes this client's numbers [first, end) out of `list` (values: the Mimic's list); what a List
  // operation refused is kept to retry, logged.
  void take_out(void* list, int32_t first, int32_t end, bool values);
  bool remove_types(void* list, int32_t first, int32_t end, bool values);  // true when none is left
  void retry_pending();
  void add_to_mimic_list(void* controller);
  bool custom(int32_t type) const;  // the contract's custom type, or before a decision a custom one
  bool keeps_last_evidence(int32_t type) const;  // a custom type with PM_GHOST_KEEPS_LAST_EVIDENCE
  std::pair<int32_t, int32_t> own_numbers() const;  // the types this client's content numbers
  bool learn_key_form(void* list);  // the game's name key around a type's name, from its own entries
  std::string sort_name(void* data) const;
  int32_t traits_offset(bool identity) const;  // a type's place inside GhostTraits, or -1

  content::Registry& registry_;
  content::Acting& acting_;
  ListOps lists_;
  std::string keyPrefix_, keySuffix_;  // "Journal_" and "Title" in the audited build, read from the game
  bool keyForm_ = false;
  bool keyFormLogged_ = false;  // that no one form shows was logged
  std::map<std::string, int32_t, std::less<>> keys_;  // the name keys of the custom entries placed, by type
  pm::api::Held contract_;            // the GhostController the decision is for
  bool decided_ = false;
  std::map<int32_t, HeldGhost> held_;      // the contract's custom types; empty: the contract has none
  std::map<int32_t, HeldGhost> offered_;   // the custom types offered outside a contract
  bool offering_ = false;                  // the lobby's offer stands
  std::vector<pm::api::Held> offeredTo_;   // the lists it went into
  unsigned shown_ = 0;                     // shown_decisions
  pm::api::Held placed_;              // the collection list the contract's entries went into
  std::vector<Pending> pending_;
  bool insertLogged_ = false, addLogged_ = false, removeLogged_ = false;
  float written_ = 0, gameBase_ = 0;  // the base last written to the hunt, and the game's own
  bool wrote_ = false;
  pm::api::Held picking_;     // the controller whose evidence pick is followed, or null
  int32_t pickType_ = -1;     // its custom behaviour type
  bool pickSwitched_ = false;  // its behaviour type reads as the Goryo's now
  bool priorityLogged_ = false;
};

}  // namespace pm::host
