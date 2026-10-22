#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

#include "perceive.h"
#include "score.h"

// Who plays the ghost, and the messages between the ghost player, the host and the investigators.
// Pure logic (no game or Photon calls) so it can be tested offline; the caller supplies the
// Photon identities each frame and sends the queued outgoing messages.
//
// Wire format: 'P' 'G' version type, then a fixed little-endian body per type.
//   HELLO      flags:u8 (bit0 volunteer, bit1 stepped out of     reliable, to others, every 2 s
//              their investigator to play the ghost)              and at once when either changes
//   ASSIGN     ghostActor:i32 (0 = nobody)                         reliable, master to others, every 2 s
//   INPUT      seq:u16 dx:f32 dz:f32                              unreliable, ghost to master, 20 Hz
//   TRIGGER    kind:u8 aim:f32x3 (unit view direction)            reliable, ghost to master
//   STATUS     interact:u8 events:u8 abilities:u8 fuses:u8 restless:u8 flags:u8 more:u8 sanity:u8
//              huntBelow:u8 type:u8 room:i16 identity:u8 trait:u8x2 mimicNext:u8
//                                                                  unreliable, master to ghost, 5 Hz
//   PICK_OFFER remainingMs:u32 fixedType:u8 rooms:u16 n:u8 types:u8[n]   reliable, master to ghost
//   PICK       type:i8 room:i16 (-1 = not chosen yet)              reliable, ghost to master
//   LOCK       remainingMs:u32 (0 = investigators may leave)       reliable, master to others, every 2 s
//   ROOM_OFFER remainingMs:u32 nativeRoom:i16 (remaining 0 closes) reliable, master to ghost
//   ROOM_CLAIM room:i16                                            reliable, ghost to master
//   WAIT       (no body)                                           reliable, master to one investigator
//   GUESS      type:u8 (0xFF none) dead:u8                         reliable, investigator to ghost player, on change
//   SCORE      killed:u8 misled:u8 investigators:u8                reliable, ghost player to others, once a contract
//   MIMIC_OFFER n:u8 types:u8[n]                                   reliable, master to ghost, when the Mimic's choices change
//   MIMIC_PICK  type:u8 (0xFF: let the game choose)                reliable, ghost to master
//   SENSE      n:u8 (kind:u8 x:f32 y:f32 z:f32)[n]                unreliable, master to ghost, 5 Hz
//   ALERT      kind:u8 arg:u8 located:u8 x:f32 y:f32 z:f32 n:u8 text:u8[n]   reliable, master to ghost
//   FORM       actor:i32 ms:u16                                   reliable, master to others: the ghost takes an investigator's form
namespace pg::session {

constexpr uint8_t kVersion = 8;
enum Type : uint8_t {
  kHello = 1,
  kAssign = 2,
  kInput = 3,
  kTrigger = 4,
  kStatus = 5,
  kPickOffer = 6,
  kPick = 7,
  kLock = 8,
  kRoomOffer = 9,
  kRoomClaim = 10,
  kWait = 11,
  kGuess = 12,
  kScore = 13,
  kMimicOffer = 14,
  kMimicPick = 15,
  kSense = 16,
  kAlert = 17,
  kForm = 18,
};
constexpr uint64_t kAnnounceMs = 2000;
constexpr uint64_t kPeerTimeoutMs = 6000;
constexpr uint64_t kInputTimeoutMs = 500;
constexpr uint64_t kStatusMs = 200;
constexpr int kTriggerKinds = 6;      // see actions::Kind
constexpr int kNobody = 0;            // no ghost player: the native AI plays the ghost
constexpr uint8_t kNoType = 0xFF;     // PickOffer::fixedType when the ghost player picks the type
constexpr size_t kMaxOfferedTypes = 64;

enum class To { Others, Master, Actor };
struct Outgoing {
  std::vector<uint8_t> bytes;
  To to;
  bool reliable;
  int actor;
};

struct Peer {
  bool volunteer = false;
  uint64_t lastSeen = 0;
};

constexpr uint8_t kUnknown = 0xFF;  // Status::sanity and huntBelow when the host cannot tell yet

// What the host tells the ghost player's HUD. The native idle roll grants one action of the kind it
// chose: interact is the interaction charge as a fraction of the most that can be held (in 1/255; an
// interaction with a rarely touched object costs more than one), events, abilities and fuses are
// whole counts. restless is a fraction in 1/255 of the time without an action before the native AI
// acts on its own again. sanity is the living investigators' average sanity in percent; the ghost
// may hunt once it is at or below huntBelow (its type and the difficulty move that from 50). type
// and room are the spawned ghost's GhostType and favourite-room index (kNoType and -1 before it
// spawns). type is the type whose rules its AI follows (a Mimic's current copy; GhostType.None
// for a custom ghost); identity is what the ghost is (the Mimic itself; a custom ghost's own type).
struct Status {
  uint8_t interact = 0;
  uint8_t events = 0;
  uint8_t abilities = 0;
  uint8_t fuses = 0;
  uint8_t restless = 0;
  uint8_t flags = 0;  // StatusFlag bits
  uint8_t more = 0;   // StatusMore bits
  uint8_t sanity = kUnknown;
  uint8_t huntBelow = kUnknown;
  uint8_t type = 0xFF;  // kNoType
  int16_t room = -1;
  uint8_t identity = 0xFF;  // kNoType
  uint8_t trait[2] = {0, 0};  // the type's own state (typestate.h), meaning by type
  uint8_t mimicNext = 0xFF;   // a Mimic: the type the ghost player chose to copy next (kNoType: the game's)
  bool operator==(const Status&) const = default;
};
// The type the ghost is named by: what it is (a custom ghost, the Mimic), else the type its AI follows.
inline uint8_t named_type(const Status& st) { return st.identity != kNoType ? st.identity : st.type; }
// Status::interact: the interaction charge (0..maxCharges, possibly owed below zero) in 1/255 of
// maxCharges, rounded so whole charges survive the trip, and the whole interactions it allows.
inline uint8_t encode_charge(float charge, float maxCharges) {
  const float f = charge <= 0 ? 0.0f : charge >= maxCharges ? 1.0f : charge / maxCharges;
  return static_cast<uint8_t>(f * 255.0f + 0.5f);
}
inline int whole_charges(uint8_t encoded, int maxCharges) { return encoded * maxCharges / 255; }

// kFocusSanity: sanity is the ghost's focused player's (the hunt check uses theirs), not the team's.
// kNativeMove: the native AI moves the ghost instead of its player (src/tether.h); kDrawnHome and
// kBlinded say why. kBound: the ghost may not leave its favourite room for now.
enum StatusFlag : uint8_t {
  kHuntAvailable = 1,
  kHunting = 2,
  kDrawnHome = 4,
  kAbilityAvailable = 8,
  kFocusSanity = 16,
  kBound = 32,
  kBlinded = 64,
  kNativeMove = 128,
};
// What the vanilla gates allow right now (src/tether.h), and how the ghost may move. kFuseTurnOff and
// kFuseTurnOn: what the fuse box action would do. kResting: the ghost holds still until it next
// wanders. kRoamEdge: it has walked as far as this wander goes. kRestlessActing: the ghost player
// has done nothing for long enough that the native AI acts on its own until they do.
enum StatusMore : uint8_t {
  kInteractAllowed = 1,
  kEventAllowed = 2,
  kFuseTurnOff = 4,
  kFuseTurnOn = 8,
  kResting = 16,
  kRoamEdge = 32,
  kRestlessActing = 64,
};

// The ghost appears in an investigator's form (form.h) for `ms`.
struct Form {
  int actor = 0;
  uint16_t ms = 0;
  bool operator==(const Form&) const = default;
};

struct Trigger {
  int kind = 0;
  float aim[3] = {0, 0, 1};
};

struct Input {
  float dx = 0, dz = 0;
  uint64_t receivedAt = 0;
  uint16_t seq = 0;
  bool valid = false;
};

// The ghost types and favourite rooms the ghost player may choose from. Types are GhostType
// values the native pick would allow; rooms are indices into the level's favourite-room list.
// fixedType is set when the difficulty forces the type natively (nothing to choose).
struct PickOffer {
  std::vector<uint8_t> types;
  uint8_t fixedType = kNoType;
  uint16_t rooms = 0;
  uint64_t deadline = 0;  // local clock
  bool valid = false;
};

// The ghost player's confirmed choices; -1 means not chosen (the native random roll decides).
struct Choice {
  int type = -1;
  int room = -1;
  bool operator==(const Choice&) const = default;
};

// A native favourite-room change waiting for the ghost player to claim a room instead.
struct RoomOffer {
  int nativeRoom = -1;
  uint64_t deadline = 0;  // local clock
  bool valid = false;
};

using Random = std::function<int(int count)>;  // uniform in [0, count)

class Session {
 public:
  // Called every frame with the current Photon view; `actors` is the room's player list.
  void observe(int localActor, int masterActor, const std::vector<int>& actors, uint64_t now);
  void receive(int sender, const uint8_t* data, size_t size, uint64_t now);

  void set_volunteer(bool on);
  bool volunteer() const { return volunteer_; }
  // The ghost player's client has stepped out of its investigator (src/body.h); everyone else parks
  // that body only once the ghost player says so.
  void set_stepped_out(bool on);
  bool ghost_stepped_out() const;
  // The ghost player is drawn in the lobby, so everyone (and the ghost player's own client, which
  // must not bring an investigator into the contract) knows before the contract loads. The host
  // draws among the volunteers who have played the ghost least often in
  // this room, using `random` (uniform in [0, count)).
  void set_random(Random random) { random_ = std::move(random); }
  void start_round();  // master only, when a contract starts: counts the round for the rotation
  void end_round();    // master only, back in the lobby: the ghost player is drawn again
  bool in_round() const { return inRound_; }

  // kNobody or the ghost player's actor.
  int ghost_actor() const { return ghost_; }
  bool local_is_ghost() const { return ghost_ > 0 && ghost_ == local_; }
  bool local_is_master() const { return local_ != 0 && local_ == master_; }
  std::vector<int> volunteers() const;  // modded actors (including self) who volunteered
  bool has_mod(int actor) const { return actor == local_ || peers_.count(actor) != 0; }
  // Every player in the room has been heard from by this mod since they joined. A ghost player is
  // only drawn while this holds: a client without the mod would neither hide the ghost player's
  // body nor keep it out of the game's rules.
  bool everyone_has_mod() const;

  // Ghost player side.
  void send_input(float dx, float dz);
  void send_trigger(int kind, const float aim[3]);
  const Status& status() const { return status_; }
  const PickOffer& pick_offer() const { return offer_; }
  void send_pick(const Choice& choice);
  const RoomOffer& room_offer() const { return roomOffer_; }
  void send_room_claim(int room);
  bool take_wait_notice();  // investigators: the host kept the truck shut for us
  // Investigators: this client's current guess and whether its investigator is dead, sent to the
  // ghost player whenever either (or the ghost player) changes.
  void send_guess(const score::Guess& guess);
  bool take_score(score::Tally& out);  // the ghost player's score, once it arrives

  // Ghost player: every investigator's last guess, by actor, and the score sent at the end.
  const std::map<int, score::Guess>& guesses() const { return guesses_; }
  void clear_guesses() { guesses_.clear(); }
  void send_score(const score::Tally& tally);

  // The Mimic's next copy. Host: the types it may copy, offered to the ghost player, and their pick.
  void offer_mimic(const std::vector<uint8_t>& types);
  bool take_mimic_pick(uint8_t& type);
  // Ghost player: the types offered, and their pick.
  const std::vector<uint8_t>& mimic_offer() const { return mimicOffer_; }
  void send_mimic_pick(uint8_t type);

  // What the ghost perceives (perceive.h). Host: the marks for its type, and alerts the ghost player's
  // own client may not have seen. Ghost player: the marks (with when they came) and alerts to show.
  void send_marks(const std::vector<perceive::Mark>& marks);
  void send_alert(const perceive::Alert& alert);
  const std::vector<perceive::Mark>& marks() const { return marks_; }
  uint64_t marks_at() const { return marksAt_; }
  bool take_alert(perceive::Alert& out);
  // Host: the ghost takes an investigator's form, shown by everyone but the ghost player.
  void send_form(const Form& form);
  bool take_form(Form& out);  // everyone but the ghost player

  // Host side.
  bool fresh_input(uint64_t now, Input& out) const;
  void set_status(const Status& s) { status_ = s; }
  std::vector<Trigger> take_triggers();
  void offer_pick(const PickOffer& offer);  // to the ghost player; clears any previous choice
  void close_pick();
  const Choice& choice() const { return choice_received_; }  // validated against the offer
  void offer_room(int nativeRoom, uint64_t deadline);
  void close_room_offer();
  bool take_room_claim(int& room);
  void notify_wait(int actor);  // tell an investigator the truck stays shut until the ghost is ready

  // Everyone. The host sets the truck lock; investigators enforce it on themselves.
  void set_lock(uint64_t deadline);  // master only; 0 unlocks
  bool locked(uint64_t now) const { return lockDeadline_ != 0 && now < lockDeadline_; }
  uint64_t lock_deadline() const { return lockDeadline_; }

  std::vector<Outgoing> take_outgoing();

 private:
  void queue(std::vector<uint8_t> bytes, To to, bool reliable, int actor = 0);
  void announce();
  void reset_round_state();
  void send_offer();
  void send_lock();
  void send_room_offer();
  bool is_volunteer(int actor) const;
  bool stopped_volunteering(int actor) const;  // known to have unticked; unknown is not "stopped"
  void apply_pick(int type, int room);
  void draw_ghost();

  int local_ = 0, master_ = 0;
  int ghost_ = kNobody;
  Random random_ = [](int) { return 0; };
  bool inRound_ = false;
  bool volunteer_ = false;
  bool steppedOut_ = false;
  std::map<int, Peer> peers_;
  std::set<int> heard_;  // actors heard from while in this room; kept through a contract's silent load
  // Actors whose last HELLO said they stepped out of their investigator; kept through silence, so a
  // quiet ghost player's body stays parked until their own HELLO or their leaving says otherwise.
  std::set<int> steppedOutPeers_;
  std::map<int, int> roundsAsGhost_;  // host: rounds each actor has played the ghost in this room
  std::vector<int> drawnAmong_;       // host: the volunteers when the ghost player was last drawn
  std::vector<int> actors_;
  uint64_t now_ = 0, nextAnnounce_ = 0;
  uint16_t inputSeq_ = 0;
  Input input_;
  Status status_;
  uint64_t nextStatus_ = 0;
  std::vector<Trigger> triggers_;
  PickOffer offer_;
  Choice choice_received_;
  RoomOffer roomOffer_;
  std::vector<int> roomClaims_;
  uint64_t lockDeadline_ = 0;
  bool waitNotice_ = false;
  std::map<int, score::Guess> guesses_;  // ghost player: by investigator actor
  score::Guess guessSent_;
  int guessSentTo_ = kNobody;
  score::Tally score_;
  bool haveScore_ = false;
  std::vector<uint8_t> mimicOffer_;  // host: as last sent; ghost player: as received
  uint8_t mimicPick_ = kNoType;
  bool haveMimicPick_ = false;
  std::vector<perceive::Mark> marks_;   // ghost player: as received (host as ghost: as set)
  std::vector<perceive::Mark> marksSent_;
  uint64_t marksAt_ = 0;
  std::vector<perceive::Alert> alerts_;  // ghost player: waiting to be shown
  std::vector<Form> forms_;              // waiting to be shown here
  std::vector<Outgoing> out_;
};

}  // namespace pg::session
