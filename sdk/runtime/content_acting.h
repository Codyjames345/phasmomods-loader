#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "content_registry.h"
#include "mod_api.h"

// What the contract's ghost acts as on this client, and the content rules that run for it
// (core/mod_api.h, ghost_rules and content_acting_as). Pure logic over the registry, so it is tested
// offline; the host and the fake backend (sdk/fake) run it. Main thread.
//
// A ghost acts as its behaviour type, or as its identity when the behaviour type is the game's "no
// type" (a custom ghost). The Mimic's copy is made on the master only and writes only the master's
// GhostInfo (sdk/internals/ghost-types.md, "The Mimic"); when it copies a custom type the host records
// the copy here and sets the behaviour type back to "no type", so the master's acting-as type comes
// from the copy. The game sends nothing of the copy; this message is the suite's own, so that every
// client can run a copied type's client-side effects. The master sends its acting-as type to every
// client on the host's channel when it changes, and once to each player who joins the room; the
// channel is reliable, so nothing is repeated but a send that failed. Each client uses it while its
// own ghost has the identity it was sent for; until then a client answers from its own GhostInfo.
//
// The rules of the acting-as type's content id run: start when the ghost begins acting as it, tick
// every frame, stop when it stops. A custom type has an id while content is active, or while the
// contract that loaded with it pins it (pin), so a contract's custom rules last as long as the
// contract. A switch stops the old rules and starts the new ones in the same call.
//
// Wire format, after the host channel's byte: 'H' 'A' version, then the identity and the acting-as
// type, each a little-endian int32.
namespace pm::content {

constexpr uint8_t kActingVersion = 1;
constexpr size_t kActingBytes = 11;

// A message to send: to `actor` alone, or to every other client when `actor` is 0.
struct ActingMessage {
  int actor;
  std::vector<uint8_t> data;
};

class Acting {
 public:
  explicit Acting(const Registry& registry) : registry_(registry) {}

  /// Every frame: the contract's ghost as this client's GhostInfo has it (identity -1: no ghost), and
  /// whether this client is the room's master (which runs the ghost's AI). Starts and stops rules as
  /// the acting-as type changes.
  void observe(int32_t identity, int32_t behaviour, bool master);
  /// On the master, after the Mimic's copy of the ghost `identity`: the type it copied, which the
  /// ghost now acts as. The previous type's rules stop and the copy's start in this call. A copy made
  /// before observe first sees this ghost is kept for it.
  void copied(int32_t identity, int32_t type);
  /// The custom types a contract holds, with their ids, decided when it loaded: their rules keep
  /// running for that contract even when content is no longer active. Empty: none (the contract's
  /// end, or the mods stopping).
  void pin(std::map<int32_t, std::string> ids);
  /// A message on the host's channel from the client of `sender`, `master` being the room's master
  /// actor. Anything that is not an acting-as message from the master is ignored.
  void receive(int sender, int master, const uint8_t* data, size_t size);
  /// Every frame: this client's actor and the room's actors (none outside a room). A player who joins
  /// is due the acting-as type.
  void observe_room(int local, const std::vector<int>& actors);
  /// On the master, the messages carrying its acting-as type that are due: to everyone when it
  /// changed, to each player who joined since, and again after a send that failed; else none. Nothing
  /// unless `agreed` (the player agreed to the mods) and some rules are registered or content is
  /// active.
  std::vector<ActingMessage> take_messages(bool agreed);
  /// Sending `message` failed: it is due again.
  void send_failed(int actor);
  /// Runs the tick of the rules running now.
  void tick();
  /// Stops the rules running now, as before the mods stop; the next observe starts them afresh.
  void stop();

  /// The type the ghost acts as, or -1 with no ghost.
  int32_t acting_as() const { return acting_; }
  /// The ghost's identity, or -1 with no ghost.
  int32_t identity() const { return identity_; }
  /// The running rules' hunt speed: each rule with huntSpeed sees the values the one before left.
  /// True when any of them asked for its values to be written.
  bool hunt_speed(float& base, float& losScale) const;
  /// Whether the ghost may not act now as `kind`: the acting type's definition's heldBack, or any running
  /// rule's (PmGhostRules::heldBack). False with no ghost (core/mod_api.h, content_held_back).
  bool held_back(int32_t kind) const;
  /// The definition of the custom type the ghost acts as: the contract pinned it, or it is active
  /// content. Null with no ghost or for a game type.
  const Ghost* acting_ghost() const;
  /// What that definition's callbacks answer (core/mod_api.h, content_use_ability, content_hud_state,
  /// content_hud_line, content_player_view and content_target): false, -1, null or 0 without a
  /// definition or the callback.
  bool use_ability() const;
  int32_t hud_state() const;
  const char* hud_line(int32_t state) const;
  uint32_t player_view(float* xyz, uint32_t max) const;
  void* target(const void* const* excluded, uint32_t excludedCount) const;
  /// The definition's playerMove on (`dx`, `dz`) from `at` (core/mod_api.h, content_player_move); leaves them
  /// as they are without a definition, the callback or a null pointer.
  void player_move(const float* at, float* dx, float* dz) const;
  /// The definition's interactionLost (core/mod_api.h, content_interaction_lost); false without a definition
  /// or the callback.
  bool interaction_lost() const;
  /// What the ghost's own definition answers for the look of the evidence `evidenceId` (its
  /// evidenceLook, by the identity: a custom type the contract pinned, else one of the active content);
  /// null with no ghost, a game ghost, no evidenceLook, or a null `evidenceId` (core/mod_api.h,
  /// content_evidence_look).
  const char* evidence_look(const char* evidenceId) const;

 private:
  void update();  // works out the acting-as type and switches the rules to it

  const Registry& registry_;
  int32_t identity_ = -1, behaviour_ = -1, acting_ = -1;
  int32_t copy_ = -1;  // the master's latest Mimic copy for this ghost, or -1
  int32_t copyIdentity_ = -1;  // the identity of the ghost that copy was made for
  std::map<int32_t, std::string> pinned_;  // the contract's custom types and ids (pin)
  bool master_ = false;
  bool heard_ = false;  // an acting-as type arrived from the master for this ghost
  int32_t heardIdentity_ = -1, heardActing_ = -1;
  std::string runningId_;            // the content id whose rules run, or empty
  unsigned decisions_ = 0;           // Registry::agreement_decisions the running rules were taken under
  std::vector<PmGhostRules> running_;
  int32_t sentIdentity_ = -1, sentActing_ = -1;  // what was last sent to everyone
  bool dueAll_ = false;                          // a send to everyone failed: due again
  std::set<int> dueActors_;                      // players who joined, or whose send failed
  std::vector<int> actors_;                      // the room as last observed
};

}  // namespace pm::content
