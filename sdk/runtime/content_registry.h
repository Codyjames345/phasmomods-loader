#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content_ids.h"
#include "mod_agreement.h"
#include "mod_api.h"

// The content registry behind the host contract's content functions (core/mod_api.h, version 2.2):
// the ghosts and evidence the running mods registered, their numbers, and the lobby rule. It only
// records and agrees on content; it touches no game object. Pure logic, so it is tested offline; the
// host and the fake backend (sdk/fake) run this one registry. Main thread.
//
// Registration. A mod registers content only while it starts, between begin_group and end_group; a
// mod that fails to start takes its content with it. Once every mod has started, commit settles the
// content: a ghost whose evidence no started mod registered is dropped (logged), so mods may start in
// any order. Nothing is in play before commit.
//
// The lobby rule. Each client's content manifest is its committed ghost and evidence ids, sorted, each
// with a hash of its definition, and the numbers its game's enums give the first custom ghost type and
// evidence (two clients whose games read different enums would number content differently). A client in a private room sends it at once when the room's players or
// its content change, again every kAnnounceMs, and kRetryMs after a send that failed; only while it has
// content, or has sent a manifest in this room before (an empty one then says its content is gone).
// Content follows the host: it is active on the master, and on another client once the master has sent a
// manifest equal to its own. No other player's manifest changes anything. A host not heard from yet keeps it
// pending, for kGraceMs at most; a host with other content (or none), or silent past kGraceMs, turns it off.
// Every player in a contract matches the host, so active content is the same on every client, and numbering it
// in byte order of its ids gives every client the same numbers without negotiation. What keeps a custom
// value from a player who is not verified yet is the sender's care, not this state (host/lobby_gate.h).
//
// Wire format, after the host channel's byte: 'H' 'C' version serial part parts, then entries
// (kind:u8 'g', 'e' or 'n', length:u8, id bytes, hash:8 bytes, little-endian) up to PM_MAX_MESSAGE
// bytes; the one 'n' entry has an empty id and the enums' first custom numbers for its hash. A
// manifest too long for one message goes in `parts` messages; the receiver judges it once it holds
// every part of one serial, which the sender changes whenever its content does. An empty manifest is
// one message with no entries.
namespace pm::content {

constexpr uint8_t kVersion = 2;       // the wire format's version: a manifest of another is ignored
constexpr size_t kHeaderBytes = 6;    // 'H' 'C' version serial part parts
constexpr size_t kMaxIdBytes = 255;   // an id's length is one byte on the wire
constexpr uint32_t kMaxTells = 64;    // tells per ghost
constexpr uint32_t kMaxLanguages = 64;      // languages per piece of text
constexpr size_t kMaxTextBytes = 64 * 1024;  // bytes per text, in UTF-8
constexpr uint64_t kAnnounceMs = 2000;  // how often a client in a room sends its manifest again
constexpr uint64_t kRetryMs = 500;      // how soon a manifest whose send failed is sent again
constexpr uint64_t kGraceMs = 10000;    // how long a player may stay unheard before content turns off

// Where content stands in this client's room.
enum class State {
  Off,      // not in play: outside a private room, no content, the host disagrees, the game's enums unread,
            // or the host cannot put content into the game (withhold)
  Pending,  // the host is not heard from yet (within kGraceMs), or this client's mods do not match its list
  Active,   // this client is the host, or matches it: content is in play
};

// Text as registered: (language, text) pairs, sorted by language, English among them.
using Text = std::vector<std::pair<std::string, std::string>>;

// A registered ghost: its definition copied, with the mod's `user` and callbacks as given
// (PmGhostDef in core/mod_api.h says what each means).
struct Ghost {
  int32_t handle = 0;  // what ghost_register returned; handles grow in registration order
  std::string id;      // "<mod id>.<name>"
  Text name, description, strength, weakness;
  std::vector<Text> tells;               // one line each
  std::array<std::string, 3> evidence;  // vanilla or custom evidence ids; "" for an empty slot
  Text abilityName, abilityDescription;  // both empty: no ability
  void* user = nullptr;                  // the mod's context, handed to every callback
  bool (*useAbility)(void*) = nullptr;
  int32_t (*hudState)(void*) = nullptr;
  bool (*heldBack)(int32_t, void*) = nullptr;
  uint32_t (*playerView)(float*, uint32_t, void*) = nullptr;
  const char* (*evidenceLook)(const char*, void*) = nullptr;
  uint32_t flags = 0;  // PM_GHOST_* bits
  void* (*target)(const void* const*, uint32_t, void*) = nullptr;
  const char* (*hudLine)(int32_t, void*) = nullptr;
  void (*playerMove)(const float*, float*, float*, void*) = nullptr;
  bool (*interactionLost)(void*) = nullptr;
  uint64_t hash = 0;  // of its id, text and evidence: what its manifest entry carries
};

// Rules attached to a ghost type (PmGhostRules in core/mod_api.h), copied, with the mod's `user` and
// callbacks as given.
struct Rules {
  int32_t handle = 0;  // what ghost_rules returned
  std::string id;      // the ghost's id: custom, or "vanilla.<name>"
  PmGhostRules rules{};
};

// A registered evidence.
struct Evidence {
  int32_t handle = 0;  // what evidence_register returned
  std::string id;      // "<mod id>.<name>"
  Text name, journalLine;
  uint64_t hash = 0;  // of its id and text
};

class Registry {
 public:
  static constexpr int kLowestActor = -1;
  /// Before a mod's pmmod_start: registration is open, and what is registered from now on is its.
  void begin_group();
  /// The answer (mod_agreement.h) the registry gates the mods' rules, journal lines and content on:
  /// rules_for and lines_for answer nothing, and content stays Pending, while this client's mods do not match
  /// the host's list. Null (the default) gates nothing.
  void use_agreement(const Agreement* agreement) { agreement_ = agreement; }
  /// Agreement::decisions of the agreement in use (0 without one): what was shown under the last answer
  /// is made again when it grows.
  unsigned agreement_decisions() const { return agreement_ ? agreement_->decisions() : 0; }
  /// After it: registration closes, and when the mod did not start its content is forgotten.
  void end_group(bool started);
  /// Once every mod has started: drops each ghost that gives an evidence no started mod registered
  /// (logged), and puts the content in the manifest.
  void commit();
  /// Forgets all content, as when the mods that registered it stop.
  void clear();
  /// The game's GhostType and EvidenceType enums as read from the map (content_ids.h): where custom
  /// numbers begin and the vanilla ids. Until they are given, content stays off and vanilla ids do not
  /// answer. Value names given no id are logged. clear keeps them.
  void set_game_enums(GameEnums enums);
  /// The game's enums cannot be read (`why`): content never activates, vanilla ids do not answer, and
  /// this client tells the others it has nothing they can agree on. Logged once; clear keeps it.
  void refuse(std::string why);

  /// Records the ghost `def` and returns its handle (greater than 0), or 0 when it is refused, which
  /// is logged with the ghost's id and why (core/mod_api.h, ghost_register, lists the refusals).
  int32_t add_ghost(const PmGhostDef* def);
  /// Records the evidence `def` likewise.
  int32_t add_evidence(const PmEvidenceDef* def);
  /// Records `rules` for the ghost `id` and returns their handle (greater than 0), or 0 when refused,
  /// logged (core/mod_api.h, ghost_rules, lists the refusals). commit drops rules on a custom id that
  /// no started mod registered.
  int32_t add_rules(const char* id, const PmGhostRules* rules);
  /// Records `count` lines for the journal page of the vanilla ghost `id` and returns their handle
  /// (greater than 0), or 0 when refused, logged (core/mod_api.h, journal_lines, lists the refusals).
  /// They are not in the manifest: they change no number and no page.
  int32_t add_lines(const char* id, const PmTextSet* lines, uint32_t count);
  /// The host cannot put custom content into the game (`why`, logged once): it never activates, and
  /// this client tells the others it has nothing they can agree on. Vanilla ids still answer. clear
  /// keeps it.
  void withhold(std::string why);

  /// Every frame with this client's actor (0 outside a private room), the room's actors, the time in
  /// milliseconds and the master's actor (0 when unread: no host is known). `master` left out takes the
  /// lowest actor, as Photon picks one, for what has no master to read.
  void observe(int localActor, const std::vector<int>& actors, uint64_t now, int master = kLowestActor);
  /// A message on the host's channel from the client of `sender`. Anything that is not a content
  /// manifest (or is malformed) is ignored.
  void receive(int sender, const uint8_t* data, size_t size);
  /// The messages carrying this client's manifest when they are due (see above), else none. Each is
  /// at most PM_MAX_MESSAGE bytes, for every other client in the room.
  std::vector<std::vector<uint8_t>> take_messages();
  /// Whether sending the messages take_messages gave failed: they are due again kRetryMs later.
  void send_failed();

  /// Where content stands in this room (State).
  State state() const;
  /// state() is Active.
  bool active() const { return state() == State::Active; }
  /// A contract that loaded with content active holds it (true) until it ends (false): type_of, id_of
  /// and evidence_of answer the custom numbers and ids meanwhile, whatever state() does, as the
  /// contract's entries keep them. ContentTypes (host/content_types.h) says so.
  void hold_contract(bool holds) { contractHolds_ = holds; }
  /// The game type of the ghost `id`: a vanilla id's whenever the enums were read, a custom ghost's
  /// (from GameEnums::firstGhostType, in byte order of the ids) while active or a contract holds
  /// content, else -1.
  int32_t type_of(std::string_view id) const;
  /// The id of the ghost type `type` ("vanilla.<name>" for a vanilla type other than None, a custom
  /// ghost's while active or a contract holds content), or null. Valid until the content changes
  /// (commit, clear).
  const char* id_of(int32_t type) const;
  /// The game value of the evidence `id`: a vanilla id's whenever the enums were read, a custom
  /// evidence's (from GameEnums::firstEvidence, in byte order) while active or a contract holds content,
  /// else -1.
  int32_t evidence_of(std::string_view id) const;
  /// The game's enums as given to set_game_enums (unread before).
  const GameEnums& game_enums() const { return enums_; }
  /// The custom ghost `id` as registered, while content is active; else null. Valid until the content
  /// changes.
  const Ghost* ghost(std::string_view id) const;
  /// Every custom ghost's type while content is active, ascending; else none.
  std::vector<int32_t> custom_types() const;
  /// Whether `type` lies where this client's custom ghosts are numbered while active (from
  /// GameEnums::firstGhostType, one for each ghost registered), active or not: what it may have put
  /// into the game and must take out again. A type past them is none of its own.
  bool own_range(int32_t type) const {
    return enums_.read() && type >= enums_.firstGhostType &&
           type < enums_.firstGhostType + static_cast<int32_t>(ghosts_.size());
  }
  /// Whether `type` is a custom ghost's type while content is active.
  bool is_custom(int32_t type) const { return type >= enums_.firstGhostType && enums_.read() && id_of(type); }
  /// Whether any rules are attached.
  bool has_rules() const { return !rules_.empty(); }
  /// The custom ghost `id` as registered, active or not; else null. Valid until the content changes.
  const Ghost* registered(std::string_view id) const {
    const auto it = ghosts_.find(id);
    return it == ghosts_.end() ? nullptr : &it->second;
  }
  /// The custom evidence `id` as registered, active or not; else null. Valid until the content changes.
  const Evidence* registered_evidence(std::string_view id) const {
    const auto it = evidence_.find(id);
    return it == evidence_.end() ? nullptr : &it->second;
  }
  /// The rules attached to the ghost `id`, in the order attached; none while this client's mods do not
  /// match the host's (use_agreement).
  std::vector<PmGhostRules> rules_for(std::string_view id) const;
  /// Every line added to the vanilla ghost `id`'s journal page, in the order added (mod by mod); none
  /// while this client's mods do not match the host's (use_agreement).
  std::vector<Text> lines_for(std::string_view id) const;
  /// Whether any journal lines were added.
  bool has_lines() const { return !lines_.empty(); }
  /// Whether journal_lines added lines to the vanilla ghost `id`, shown now or not.
  bool has_lines_for(std::string_view id) const {
    return std::any_of(lines_.begin(), lines_.end(), [&](const Lines& l) { return l.id == id; });
  }

 private:
  // Lines a mod added to a vanilla ghost's journal page.
  struct Lines {
    int32_t handle = 0;
    std::string id;  // "vanilla.<name>"
    std::vector<Text> lines;
  };
  // One line of a manifest.
  struct Entry {
    uint8_t kind;  // 'g' a ghost, 'e' an evidence, 'n' the enums' first custom numbers
    std::string id;
    uint64_t hash;
    auto operator<=>(const Entry&) const = default;
  };
  // Another client in the room, as heard.
  struct Peer {
    uint8_t serial = 0;
    std::vector<std::vector<Entry>> parts;  // what arrived of the manifest `serial`, by part
    std::vector<bool> got;
    bool complete = false;        // a whole manifest arrived
    std::vector<Entry> manifest;  // sorted, once complete
  };

  bool open(const char* what, const char* id) const;
  bool answering() const { return active() || contractHolds_; }  // the custom numbers answer
  void changed();

  std::map<std::string, Ghost, std::less<>> ghosts_;  // by id, in byte order
  std::map<std::string, Evidence, std::less<>> evidence_;
  std::vector<Rules> rules_;  // in the order attached
  std::vector<Lines> lines_;  // in the order added
  std::vector<Entry> manifest_;  // this client's, sorted; empty before commit
  std::map<int, Peer> peers_;
  std::map<int, uint64_t> firstSeen_;  // when each other player was first seen in the room
  std::vector<int> actors_;
  GameEnums enums_;      // the game's enums; unread until set_game_enums
  std::string refused_;   // why content never activates, or empty
  std::string withheld_;  // why the host cannot put content into the game, or empty
  int local_ = 0;
  int master_ = 0;
  int32_t nextHandle_ = 1;
  int32_t groupStart_ = 1;  // the first handle of the mod starting now
  bool open_ = false;       // a mod is starting: registration is open
  bool committed_ = false;
  uint8_t serial_ = 0;
  uint64_t now_ = 0, next_ = 0;  // the time, and when the manifest is next due
  bool sentInRoom_ = false;      // a manifest was sent in this room
  bool tooLongLogged_ = false;
  bool contractHolds_ = false;  // hold_contract
  const Agreement* agreement_ = nullptr;  // use_agreement
  bool agreed() const { return !agreement_ || agreement_->agreed(); }
};

}  // namespace pm::content
