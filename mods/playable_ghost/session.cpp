#include "session.h"

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstring>
#include <utility>

namespace pg::session {
namespace {

std::vector<uint8_t> header(Type t) { return {'P', 'G', kVersion, t}; }

template <class T>
void put(std::vector<uint8_t>& b, T v) {
  const auto* p = reinterpret_cast<const uint8_t*>(&v);
  b.insert(b.end(), p, p + sizeof v);
}

template <class T>
bool get(const uint8_t*& p, const uint8_t* end, T& v) {
  if (static_cast<size_t>(end - p) < sizeof v) return false;
  std::memcpy(&v, p, sizeof v);
  p += sizeof v;
  return true;
}

bool contains(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

uint32_t remaining_ms(uint64_t deadline, uint64_t now) {
  if (deadline <= now) return 0;
  return static_cast<uint32_t>(std::min<uint64_t>(deadline - now, UINT32_MAX));
}

}  // namespace

void Session::observe(int localActor, int masterActor, const std::vector<int>& actors, uint64_t now) {
  const bool masterChanged = masterActor != master_;
  local_ = localActor;
  master_ = masterActor;
  actors_ = actors;
  now_ = now;
  for (auto it = peers_.begin(); it != peers_.end();) {
    const bool gone = !contains(actors, it->first) || now - it->second.lastSeen > kPeerTimeoutMs;
    it = gone ? peers_.erase(it) : std::next(it);
  }
  std::erase_if(heard_, [&](int actor) { return !contains(actors, actor); });
  std::erase_if(steppedOutPeers_, [&](int actor) { return !contains(actors, actor); });
  if (local_ == 0) {  // left the room: forget the round and the rotation
    heard_.clear();
    steppedOutPeers_.clear();
    ghost_ = kNobody;
    inRound_ = false;
    roundsAsGhost_.clear();
    drawnAmong_.clear();
    reset_round_state();
    return;
  }
  // The ghost player left: nobody controls the ghost; native AI keeps it for this contract.
  if (ghost_ > 0 && !contains(actors, ghost_)) {
    ghost_ = kNobody;
    reset_round_state();
    nextAnnounce_ = 0;
  }
  if (masterChanged) input_ = Input{};
  if (local_is_master() && !inRound_) draw_ghost();
  if (now >= nextAnnounce_) announce();
  if (local_is_master() && ghost_ > 0 && !local_is_ghost() && now >= nextStatus_) {
    std::vector<uint8_t> b = header(kStatus);
    put<uint8_t>(b, status_.interact);
    put<uint8_t>(b, status_.events);
    put<uint8_t>(b, status_.abilities);
    put<uint8_t>(b, status_.fuses);
    put<uint8_t>(b, status_.restless);
    put<uint8_t>(b, status_.flags);
    put<uint8_t>(b, status_.more);
    put<uint8_t>(b, status_.sanity);
    put<uint8_t>(b, status_.huntBelow);
    put<uint8_t>(b, status_.type);
    put<int16_t>(b, status_.room);
    put<uint8_t>(b, status_.identity);
    put<uint8_t>(b, status_.trait[0]);
    put<uint8_t>(b, status_.trait[1]);
    put<uint8_t>(b, status_.mimicNext);
    queue(std::move(b), To::Actor, false, ghost_);
    nextStatus_ = now + kStatusMs;
  }
}

void Session::announce() {
  nextAnnounce_ = now_ + kAnnounceMs;
  std::vector<uint8_t> hello = header(kHello);
  put<uint8_t>(hello, static_cast<uint8_t>((volunteer_ ? 1 : 0) | (steppedOut_ ? 2 : 0)));
  queue(std::move(hello), To::Others, true);
  if (local_is_master()) {
    std::vector<uint8_t> assign = header(kAssign);
    put<int32_t>(assign, ghost_);
    queue(std::move(assign), To::Others, true);
    if (locked(now_)) send_lock();  // repeated so a late or reconnecting client learns of it
  }
}

void Session::receive(int sender, const uint8_t* data, size_t size, uint64_t now) {
  if (size < 4 || data[0] != 'P' || data[1] != 'G' || data[2] != kVersion) return;
  if (sender == 0 || sender == local_ || !contains(actors_, sender)) return;
  const uint8_t* p = data + 4;
  const uint8_t* end = data + size;
  Peer& peer = peers_[sender];
  peer.lastSeen = now;
  heard_.insert(sender);
  const bool fromMaster = sender == master_;
  switch (data[3]) {
    case kHello: {
      uint8_t flags;
      if (get(p, end, flags)) {
        peer.volunteer = (flags & 1) != 0;
        if (flags & 2) steppedOutPeers_.insert(sender);
        else steppedOutPeers_.erase(sender);
      }
      break;
    }
    case kAssign: {
      int32_t actor;
      // Only the current master decides; a stale or spoofed assignment is ignored.
      if (fromMaster && get(p, end, actor) && (actor == kNobody || contains(actors_, actor))) {
        if (actor != ghost_) reset_round_state();
        ghost_ = actor;
      }
      break;
    }
    case kInput: {
      uint16_t seq;
      float dx, dz;
      if (!local_is_master() || sender != ghost_) break;
      if (!get(p, end, seq) || !get(p, end, dx) || !get(p, end, dz)) break;
      if (!std::isfinite(dx) || !std::isfinite(dz) || dx * dx + dz * dz > 1.0001f) break;
      // Unreliable packets can arrive out of order; accept only newer sequence numbers.
      if (input_.valid && static_cast<int16_t>(seq - input_.seq) <= 0) break;
      input_ = {dx, dz, now, seq, true};
      break;
    }
    case kTrigger: {
      uint8_t kind;
      Trigger t;
      if (!local_is_master() || sender != ghost_ || !get(p, end, kind) || kind >= kTriggerKinds) break;
      if (!get(p, end, t.aim[0]) || !get(p, end, t.aim[1]) || !get(p, end, t.aim[2])) break;
      const float m2 = t.aim[0] * t.aim[0] + t.aim[1] * t.aim[1] + t.aim[2] * t.aim[2];
      if (!std::isfinite(m2) || std::fabs(m2 - 1.0f) > 0.01f) break;  // must be a view direction
      t.kind = kind;
      triggers_.push_back(t);
      break;
    }
    case kStatus: {
      Status s;
      if (fromMaster && local_is_ghost() && get(p, end, s.interact) && get(p, end, s.events) &&
          get(p, end, s.abilities) && get(p, end, s.fuses) && get(p, end, s.restless) && get(p, end, s.flags) &&
          get(p, end, s.more) && get(p, end, s.sanity) && get(p, end, s.huntBelow) && get(p, end, s.type) &&
          get(p, end, s.room) && get(p, end, s.identity) && get(p, end, s.trait[0]) && get(p, end, s.trait[1]) &&
          get(p, end, s.mimicNext))
        status_ = s;
      break;
    }
    case kPickOffer: {
      uint32_t remaining;
      uint8_t fixed, count;
      uint16_t rooms;
      if (!fromMaster || !local_is_ghost() || !get(p, end, remaining) || !get(p, end, fixed) ||
          !get(p, end, rooms) || !get(p, end, count) || count > kMaxOfferedTypes ||
          static_cast<size_t>(end - p) < count)
        break;
      PickOffer o;
      o.types.assign(p, p + count);
      o.fixedType = fixed;
      o.rooms = rooms;
      o.deadline = now + remaining;
      o.valid = remaining != 0;
      offer_ = o.valid ? o : PickOffer{};
      break;
    }
    case kPick: {
      int8_t type;
      int16_t room;
      if (local_is_master() && sender == ghost_ && get(p, end, type) && get(p, end, room)) apply_pick(type, room);
      break;
    }
    case kLock: {
      uint32_t remaining;
      if (fromMaster && get(p, end, remaining)) lockDeadline_ = remaining ? now + remaining : 0;
      break;
    }
    case kRoomOffer: {
      uint32_t remaining;
      int16_t room;
      if (!fromMaster || !local_is_ghost() || !get(p, end, remaining) || !get(p, end, room)) break;
      roomOffer_ = remaining ? RoomOffer{room, now + remaining, true} : RoomOffer{};
      break;
    }
    case kWait:
      if (fromMaster) waitNotice_ = true;
      break;
    case kGuess: {
      uint8_t type, dead;
      if (local_is_ghost() && sender != local_ && get(p, end, type) && get(p, end, dead)) guesses_[sender] = {type, dead != 0};
      break;
    }
    case kMimicOffer: {
      uint8_t n;
      if (!fromMaster || !local_is_ghost() || !get(p, end, n) || static_cast<size_t>(end - p) < n) break;
      mimicOffer_.assign(p, p + n);
      break;
    }
    case kMimicPick: {
      uint8_t type;
      if (!local_is_master() || sender != ghost_ || !get(p, end, type)) break;
      bool offered = type == kNoType;
      for (uint8_t t : mimicOffer_) offered |= t == type;
      if (!offered) break;  // only what the game would let the Mimic copy
      mimicPick_ = type;
      haveMimicPick_ = true;
      break;
    }
    case kScore: {
      uint8_t killed, misled, investigators;
      if (sender == ghost_ && !local_is_ghost() && get(p, end, killed) && get(p, end, misled) && get(p, end, investigators)) {
        score_ = {killed, misled, investigators};
        haveScore_ = true;
      }
      break;
    }
    case kSense: {
      uint8_t n;
      if (!fromMaster || !local_is_ghost() || !get(p, end, n) || n > perceive::kMaxMarks) break;
      std::vector<perceive::Mark> marks(n);
      bool ok = true;
      for (perceive::Mark& m : marks) {
        ok = ok && get(p, end, m.kind) && get(p, end, m.x) && get(p, end, m.y) && get(p, end, m.z) &&
             std::isfinite(m.x) && std::isfinite(m.y) && std::isfinite(m.z);
      }
      if (!ok) break;
      marks_ = std::move(marks);
      marksAt_ = now;
      break;
    }
    case kAlert: {
      perceive::Alert a;
      uint8_t located, n;
      if (!fromMaster || !local_is_ghost() || !get(p, end, a.kind) || !get(p, end, a.arg) || !get(p, end, located) ||
          !get(p, end, a.x) || !get(p, end, a.y) || !get(p, end, a.z) || !get(p, end, n) ||
          n > perceive::kMaxAlertText || static_cast<size_t>(end - p) < n)
        break;
      if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z)) break;
      a.located = located != 0;
      a.text.assign(reinterpret_cast<const char*>(p), n);
      if (alerts_.size() < 16) alerts_.push_back(std::move(a));
      break;
    }
    case kForm: {
      Form f;
      if (fromMaster && !local_is_ghost() && get(p, end, f.actor) && get(p, end, f.ms) && f.actor > 0 && forms_.size() < 4)
        forms_.push_back(f);
      break;
    }
    case kRoomClaim: {
      int16_t room;
      if (local_is_master() && sender == ghost_ && roomOffer_.valid && get(p, end, room) && room >= 0)
        roomClaims_.push_back(room);
      break;
    }
    default:
      break;
  }
}

void Session::set_volunteer(bool on) {
  if (on == volunteer_) return;
  volunteer_ = on;
  nextAnnounce_ = 0;  // announce on the next frame
}

void Session::set_stepped_out(bool on) {
  if (on == steppedOut_) return;
  steppedOut_ = on;
  nextAnnounce_ = 0;  // announce on the next frame
}

bool Session::ghost_stepped_out() const {
  if (ghost_ <= 0) return false;
  if (ghost_ == local_) return steppedOut_;
  return steppedOutPeers_.count(ghost_) != 0;
}

bool Session::is_volunteer(int actor) const {
  return actor == local_ ? volunteer_ : (peers_.count(actor) && peers_.at(actor).volunteer);
}

bool Session::stopped_volunteering(int actor) const {
  return actor == local_ ? !volunteer_ : (peers_.count(actor) && !peers_.at(actor).volunteer);
}

// Host, in the lobby: draws the ghost player among the volunteers who have been the ghost least
// often in this room, and draws again whenever a new volunteer ticks in, so the first to tick has
// no advantage; otherwise it keeps its drawn volunteer while they still volunteer.
void Session::draw_ghost() {
  if (!everyone_has_mod()) {
    if (ghost_ == kNobody) return;
    reset_round_state();
    ghost_ = kNobody;
    drawnAmong_.clear();
    nextAnnounce_ = 0;
    return;
  }
  // Only knowing that a player stopped volunteering (or their leaving the room) replaces them:
  // a peer that has merely gone quiet keeps the role, because Photon holds messages back while a
  // contract loads and the ghost player's client has already prepared for the role by then.
  auto stays = [&](int actor) { return contains(actors_, actor) && !stopped_volunteering(actor); };
  int next = kNobody;
  const std::vector<int> current = volunteers();
  // A ghost player this client did not draw was inherited with the master role: not a fresh draw.
  if (ghost_ > 0 && drawnAmong_.empty()) drawnAmong_ = current;
  const bool newcomer = std::any_of(current.begin(), current.end(), [&](int a) { return !contains(drawnAmong_, a); });
  if (ghost_ > 0 && stays(ghost_) && !newcomer) {
    next = ghost_;
  } else {
    drawnAmong_ = current;
    std::vector<int> pool;
    int fewest = INT_MAX;
    for (int a : current) {
      const int n = roundsAsGhost_.count(a) ? roundsAsGhost_.at(a) : 0;
      if (n < fewest) pool.clear(), fewest = n;
      if (n == fewest) pool.push_back(a);
    }
    const int count = static_cast<int>(pool.size());
    if (count) next = pool[std::clamp(random_(count), 0, count - 1)];
  }
  if (next == ghost_) return;
  reset_round_state();
  ghost_ = next;
  nextAnnounce_ = 0;
}

void Session::start_round() {
  if (!local_is_master() || inRound_) return;
  inRound_ = true;
  reset_round_state();
  if (ghost_ > 0) ++roundsAsGhost_[ghost_];
  announce();  // the assignment goes out before any pick offer or lock
}

void Session::end_round() {
  if (!local_is_master() || !inRound_) return;
  inRound_ = false;
  const bool wasLocked = lockDeadline_ != 0;
  reset_round_state();
  ghost_ = kNobody;  // the rotation picks the next ghost player
  draw_ghost();
  if (wasLocked) send_lock();
  announce();
}

bool Session::everyone_has_mod() const {
  return std::all_of(actors_.begin(), actors_.end(), [&](int a) { return a == local_ || heard_.count(a) != 0; });
}

std::vector<int> Session::volunteers() const {
  std::vector<int> v;
  for (int a : actors_) {
    if (is_volunteer(a)) v.push_back(a);
  }
  return v;
}

void Session::send_input(float dx, float dz) {
  if (!local_is_ghost() || local_is_master()) return;
  std::vector<uint8_t> b = header(kInput);
  put<uint16_t>(b, ++inputSeq_);
  put<float>(b, dx);
  put<float>(b, dz);
  queue(std::move(b), To::Master, false);
}

void Session::send_trigger(int kind, const float aim[3]) {
  if (!local_is_ghost() || kind < 0 || kind >= kTriggerKinds) return;
  if (local_is_master()) {
    Trigger t;
    t.kind = kind;
    std::memcpy(t.aim, aim, sizeof t.aim);
    triggers_.push_back(t);
    return;
  }
  std::vector<uint8_t> b = header(kTrigger);
  put<uint8_t>(b, static_cast<uint8_t>(kind));
  for (int i = 0; i < 3; ++i) put<float>(b, aim[i]);
  queue(std::move(b), To::Master, true);
}

void Session::send_pick(const Choice& c) {
  if (!local_is_ghost() || !offer_.valid) return;
  if (local_is_master()) {
    apply_pick(c.type, c.room);
    return;
  }
  std::vector<uint8_t> b = header(kPick);
  put<int8_t>(b, static_cast<int8_t>(std::clamp(c.type, -1, 127)));
  put<int16_t>(b, static_cast<int16_t>(std::clamp(c.room, -1, 32767)));
  queue(std::move(b), To::Master, true);
}

void Session::apply_pick(int type, int room) {
  if (!offer_.valid) return;
  const bool typeOffered = offer_.fixedType == kNoType &&
                           std::find(offer_.types.begin(), offer_.types.end(), type) != offer_.types.end();
  if (type >= 0 && typeOffered) choice_received_.type = type;
  if (room >= 0 && room < offer_.rooms) choice_received_.room = room;
}

void Session::send_room_claim(int room) {
  if (!local_is_ghost() || !roomOffer_.valid || room < 0) return;
  if (local_is_master()) {
    roomClaims_.push_back(room);
    return;
  }
  std::vector<uint8_t> b = header(kRoomClaim);
  put<int16_t>(b, static_cast<int16_t>(std::min(room, 32767)));
  queue(std::move(b), To::Master, true);
}

bool Session::fresh_input(uint64_t now, Input& out) const {
  if (!input_.valid || now - input_.receivedAt > kInputTimeoutMs) return false;
  out = input_;
  return true;
}

std::vector<Trigger> Session::take_triggers() { return std::exchange(triggers_, {}); }

void Session::offer_pick(const PickOffer& offer) {
  if (!local_is_master() || ghost_ <= 0) return;
  offer_ = offer;
  offer_.valid = true;
  if (offer_.types.size() > kMaxOfferedTypes) offer_.types.resize(kMaxOfferedTypes);
  choice_received_ = Choice{};
  send_offer();
}

void Session::close_pick() {
  if (!local_is_master() || !offer_.valid) return;
  offer_ = PickOffer{};
  send_offer();
}

void Session::send_offer() {
  if (local_is_ghost() || ghost_ <= 0) return;  // the host as ghost reads its own offer
  std::vector<uint8_t> b = header(kPickOffer);
  put<uint32_t>(b, offer_.valid ? std::max<uint32_t>(1, remaining_ms(offer_.deadline, now_)) : 0);
  put<uint8_t>(b, offer_.fixedType);
  put<uint16_t>(b, offer_.rooms);
  put<uint8_t>(b, static_cast<uint8_t>(offer_.types.size()));
  b.insert(b.end(), offer_.types.begin(), offer_.types.end());
  queue(std::move(b), To::Actor, true, ghost_);
}

void Session::offer_room(int nativeRoom, uint64_t deadline) {
  if (!local_is_master() || ghost_ <= 0) return;
  roomOffer_ = {nativeRoom, deadline, true};
  roomClaims_.clear();
  send_room_offer();
}

void Session::close_room_offer() {
  if (!local_is_master() || !roomOffer_.valid) return;
  roomOffer_ = RoomOffer{};
  roomClaims_.clear();
  send_room_offer();
}

void Session::send_room_offer() {
  if (local_is_ghost() || ghost_ <= 0) return;
  std::vector<uint8_t> b = header(kRoomOffer);
  put<uint32_t>(b, roomOffer_.valid ? std::max<uint32_t>(1, remaining_ms(roomOffer_.deadline, now_)) : 0);
  put<int16_t>(b, static_cast<int16_t>(std::clamp(roomOffer_.nativeRoom, -1, 32767)));
  queue(std::move(b), To::Actor, true, ghost_);
}

void Session::notify_wait(int actor) {
  if (!local_is_master() || actor <= 0) return;
  if (actor == local_) {
    waitNotice_ = true;
    return;
  }
  queue(header(kWait), To::Actor, true, actor);
}

bool Session::take_wait_notice() { return std::exchange(waitNotice_, false); }

void Session::send_guess(const score::Guess& guess) {
  if (ghost_ <= 0 || local_is_ghost() || (guess == guessSent_ && guessSentTo_ == ghost_)) return;
  std::vector<uint8_t> b = header(kGuess);
  put<uint8_t>(b, guess.type);
  put<uint8_t>(b, guess.dead ? 1 : 0);
  queue(std::move(b), To::Actor, true, ghost_);
  guessSent_ = guess;
  guessSentTo_ = ghost_;
}

bool Session::take_score(score::Tally& out) {
  if (!haveScore_) return false;
  out = score_;
  haveScore_ = false;
  return true;
}

void Session::offer_mimic(const std::vector<uint8_t>& types) {
  if (!local_is_master() || ghost_ <= 0 || types == mimicOffer_) return;
  mimicOffer_ = types;
  if (local_is_ghost()) return;  // the host is the ghost player: nothing to send
  std::vector<uint8_t> b = header(kMimicOffer);
  put<uint8_t>(b, static_cast<uint8_t>(std::min<size_t>(types.size(), 255)));
  for (size_t i = 0; i < types.size() && i < 255; ++i) put<uint8_t>(b, types[i]);
  queue(std::move(b), To::Actor, true, ghost_);
}

bool Session::take_mimic_pick(uint8_t& type) {
  if (!haveMimicPick_) return false;
  type = mimicPick_;
  haveMimicPick_ = false;
  return true;
}

void Session::send_mimic_pick(uint8_t type) {
  if (!local_is_ghost()) return;
  if (local_is_master()) {  // the host is the ghost player
    bool offered = type == kNoType;
    for (uint8_t t : mimicOffer_) offered |= t == type;
    if (offered) {
      mimicPick_ = type;
      haveMimicPick_ = true;
    }
    return;
  }
  std::vector<uint8_t> b = header(kMimicPick);
  put<uint8_t>(b, type);
  queue(std::move(b), To::Master, true);
}

void Session::send_marks(const std::vector<perceive::Mark>& marks) {
  if (!local_is_master() || ghost_ <= 0) return;
  const size_t n = std::min<size_t>(marks.size(), perceive::kMaxMarks);
  if (local_is_ghost()) {  // the host is the ghost player: nothing to send
    marks_.assign(marks.begin(), marks.begin() + static_cast<long long>(n));
    marksAt_ = now_;
    return;
  }
  if (marks.empty() && marksSent_.empty()) return;  // nothing then, nothing now
  marksSent_.assign(marks.begin(), marks.begin() + static_cast<long long>(n));
  std::vector<uint8_t> b = header(kSense);
  put<uint8_t>(b, static_cast<uint8_t>(n));
  for (size_t i = 0; i < n; ++i) {
    put<uint8_t>(b, marks[i].kind);
    put<float>(b, marks[i].x);
    put<float>(b, marks[i].y);
    put<float>(b, marks[i].z);
  }
  queue(std::move(b), To::Actor, false, ghost_);
}

void Session::send_alert(const perceive::Alert& alert) {
  if (!local_is_master() || ghost_ <= 0) return;
  perceive::Alert a = alert;
  if (a.text.size() > perceive::kMaxAlertText) a.text.resize(perceive::kMaxAlertText);
  if (local_is_ghost()) {
    if (alerts_.size() < 16) alerts_.push_back(std::move(a));
    return;
  }
  std::vector<uint8_t> b = header(kAlert);
  put<uint8_t>(b, a.kind);
  put<uint8_t>(b, a.arg);
  put<uint8_t>(b, a.located ? 1 : 0);
  put<float>(b, a.x);
  put<float>(b, a.y);
  put<float>(b, a.z);
  put<uint8_t>(b, static_cast<uint8_t>(a.text.size()));
  b.insert(b.end(), a.text.begin(), a.text.end());
  queue(std::move(b), To::Actor, true, ghost_);
}

bool Session::take_alert(perceive::Alert& out) {
  if (alerts_.empty()) return false;
  out = std::move(alerts_.front());
  alerts_.erase(alerts_.begin());
  return true;
}

void Session::send_form(const Form& form) {
  if (!local_is_master() || ghost_ <= 0 || form.actor <= 0) return;
  if (!local_is_ghost() && forms_.size() < 4) forms_.push_back(form);  // the host sees it too
  std::vector<uint8_t> b = header(kForm);
  put<int32_t>(b, form.actor);
  put<uint16_t>(b, form.ms);
  queue(std::move(b), To::Others, true);
}

bool Session::take_form(Form& out) {
  if (forms_.empty()) return false;
  out = forms_.front();
  forms_.erase(forms_.begin());
  return true;
}

void Session::send_score(const score::Tally& tally) {
  if (!local_is_ghost()) return;
  std::vector<uint8_t> b = header(kScore);
  put<uint8_t>(b, static_cast<uint8_t>(tally.killed));
  put<uint8_t>(b, static_cast<uint8_t>(tally.misled));
  put<uint8_t>(b, static_cast<uint8_t>(tally.investigators));
  queue(std::move(b), To::Others, true);
}

bool Session::take_room_claim(int& room) {
  if (roomClaims_.empty()) return false;
  room = roomClaims_.front();
  roomClaims_.erase(roomClaims_.begin());
  return true;
}

void Session::set_lock(uint64_t deadline) {
  if (!local_is_master()) return;
  lockDeadline_ = deadline;
  send_lock();
}

void Session::send_lock() {
  std::vector<uint8_t> b = header(kLock);
  put<uint32_t>(b, lockDeadline_ ? remaining_ms(lockDeadline_, now_) : 0);
  queue(std::move(b), To::Others, true);
}

void Session::reset_round_state() {
  input_ = Input{};
  status_ = Status{};
  triggers_.clear();
  offer_ = PickOffer{};
  choice_received_ = Choice{};
  roomOffer_ = RoomOffer{};
  roomClaims_.clear();
  lockDeadline_ = 0;
  waitNotice_ = false;
  mimicOffer_.clear();
  haveMimicPick_ = false;
  marks_.clear();
  marksSent_.clear();
  marksAt_ = 0;
  alerts_.clear();
  forms_.clear();
}

std::vector<Outgoing> Session::take_outgoing() { return std::exchange(out_, {}); }

void Session::queue(std::vector<uint8_t> bytes, To to, bool reliable, int actor) {
  out_.push_back({std::move(bytes), to, reliable, actor});
}

}  // namespace pg::session
