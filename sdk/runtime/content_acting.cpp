#include "content_acting.h"

#include <algorithm>

namespace pm::content {
namespace {

void put32(std::vector<uint8_t>& out, int32_t v) {
  for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(static_cast<uint32_t>(v) >> (8 * i)));
}

int32_t get32(const uint8_t* at) {
  uint32_t v = 0;
  for (int i = 0; i < 4; ++i) v |= static_cast<uint32_t>(at[i]) << (8 * i);
  return static_cast<int32_t>(v);
}

}  // namespace

void Acting::observe(int32_t identity, int32_t behaviour, bool master) {
  // Another ghost, or another client runs it: an earlier copy is not this one's. A copy made for this
  // very identity before this first look at it (the Mimic copied before the suite's tick) stays.
  if ((identity != identity_ || master != master_) && copyIdentity_ != identity) copy_ = -1;
  if (identity < 0) {
    heard_ = false;
    copy_ = copyIdentity_ = -1;
    // The next contract's ghost is news, even when its pair is the one last sent.
    sentIdentity_ = sentActing_ = -1;
  }
  identity_ = identity;
  behaviour_ = behaviour;
  master_ = master;
  update();
}

void Acting::copied(int32_t identity, int32_t type) {
  copy_ = type;
  copyIdentity_ = identity;
  if (identity_ < 0 && identity >= 0) identity_ = identity;  // the copy is the first word of this ghost
  update();
}

void Acting::pin(std::map<int32_t, std::string> ids) { pinned_ = std::move(ids); }

void Acting::receive(int sender, int master, const uint8_t* data, size_t size) {
  if (!data || size != kActingBytes || data[0] != 'H' || data[1] != 'A' || data[2] != kActingVersion) return;
  if (sender == 0 || sender != master) return;
  heard_ = true;
  heardIdentity_ = get32(data + 3);
  heardActing_ = get32(data + 7);
  update();
}

void Acting::observe_room(int local, const std::vector<int>& actors) {
  for (int a : actors)
    if (a != local && std::find(actors_.begin(), actors_.end(), a) == actors_.end()) dueActors_.insert(a);
  std::erase_if(dueActors_, [&](int a) { return std::find(actors.begin(), actors.end(), a) == actors.end(); });
  actors_ = actors;
}

std::vector<ActingMessage> Acting::take_messages(bool agreed) {
  // A client that declined the mods, or has nothing that acts on the type, says nothing.
  if (!agreed || !(registry_.has_rules() || registry_.active())) return {};
  if (!master_ || identity_ < 0) return {};
  if (identity_ != sentIdentity_ || acting_ != sentActing_) dueAll_ = true;
  if (!dueAll_ && dueActors_.empty()) return {};
  std::vector<uint8_t> data{'H', 'A', kActingVersion};
  put32(data, identity_);
  put32(data, acting_);
  std::vector<ActingMessage> out;
  if (dueAll_) out.push_back({0, data});
  else
    for (int a : dueActors_) out.push_back({a, data});
  sentIdentity_ = identity_;
  sentActing_ = acting_;
  dueAll_ = false;
  dueActors_.clear();
  return out;
}

void Acting::send_failed(int actor) {
  if (actor == 0) dueAll_ = true;
  else dueActors_.insert(actor);
}

void Acting::update() {
  int32_t acting = -1;
  if (identity_ >= 0) {
    const int32_t none = registry_.game_enums().noGhostType;
    acting = behaviour_ >= 0 && behaviour_ != none ? behaviour_ : identity_;
    if (master_ && copy_ >= 0) acting = copy_;
    if (!master_ && heard_ && heardIdentity_ == identity_) acting = heardActing_;
  }
  acting_ = acting;
  const char* id = acting >= 0 ? registry_.id_of(acting) : nullptr;
  const auto pinned = id ? pinned_.end() : pinned_.find(acting);
  const std::string key = id ? id : pinned != pinned_.end() ? pinned->second : "";
  // This client's match with the host gates the rules (Registry::rules_for): a change of it takes them afresh.
  const unsigned decisions = registry_.agreement_decisions();
  if (key == runningId_ && decisions == decisions_) return;
  stop();
  runningId_ = key;
  decisions_ = decisions;
  if (key.empty()) return;
  running_ = registry_.rules_for(key);
  for (const PmGhostRules& r : running_)
    if (r.start) r.start(r.user);
}

void Acting::tick() {
  // A content change (a mod's rules gone, content off) shows here before any rule runs again.
  update();
  for (const PmGhostRules& r : running_)
    if (r.tick) r.tick(r.user);
}

void Acting::stop() {
  for (const PmGhostRules& r : running_)
    if (r.stop) r.stop(r.user);
  running_.clear();
  runningId_.clear();
}

bool Acting::hunt_speed(float& base, float& losScale) const {
  bool any = false;
  for (const PmGhostRules& r : running_)
    if (r.huntSpeed && r.huntSpeed(&base, &losScale, r.user)) any = true;
  return any;
}

bool Acting::held_back(int32_t kind) const {
  if (const Ghost* g = acting_ghost(); g && g->heldBack && g->heldBack(kind, g->user)) return true;
  for (const PmGhostRules& r : running_)
    if (r.heldBack && r.heldBack(kind, r.user)) return true;
  return false;
}

const Ghost* Acting::acting_ghost() const {
  if (acting_ < 0) return nullptr;
  // The contract's own custom type first, as for its rules: content may have gone off since it loaded.
  const auto pinned = pinned_.find(acting_);
  const char* id = pinned != pinned_.end() ? pinned->second.c_str() : registry_.id_of(acting_);
  return id ? registry_.registered(id) : nullptr;
}

bool Acting::use_ability() const {
  const Ghost* g = acting_ghost();
  return g && g->useAbility && g->useAbility(g->user);
}

int32_t Acting::hud_state() const {
  const Ghost* g = acting_ghost();
  return g && g->hudState ? g->hudState(g->user) : -1;
}

const char* Acting::hud_line(int32_t state) const {
  const Ghost* g = acting_ghost();
  return g && g->hudLine ? g->hudLine(state, g->user) : nullptr;
}

uint32_t Acting::player_view(float* xyz, uint32_t max) const {
  const Ghost* g = acting_ghost();
  if (!g || !g->playerView || !xyz || !max) return 0;
  return std::min(g->playerView(xyz, max, g->user), max);
}

void* Acting::target(const void* const* excluded, uint32_t excludedCount) const {
  const Ghost* g = acting_ghost();
  return g && g->target ? g->target(excluded, excluded ? excludedCount : 0, g->user) : nullptr;
}

void Acting::player_move(const float* at, float* dx, float* dz) const {
  const Ghost* g = acting_ghost();
  if (g && g->playerMove && at && dx && dz) g->playerMove(at, dx, dz, g->user);
}

bool Acting::interaction_lost() const {
  const Ghost* g = acting_ghost();
  return g && g->interactionLost && g->interactionLost(g->user);
}

const char* Acting::evidence_look(const char* evidenceId) const {
  if (!evidenceId || identity_ < 0) return nullptr;
  // The contract's own custom type first, as for its rules: content may have gone off since it loaded.
  const auto pinned = pinned_.find(identity_);
  const char* id = pinned != pinned_.end() ? pinned->second.c_str() : registry_.id_of(identity_);
  const Ghost* g = id ? registry_.registered(id) : nullptr;
  return g && g->evidenceLook ? g->evidenceLook(evidenceId, g->user) : nullptr;
}

}  // namespace pm::content
