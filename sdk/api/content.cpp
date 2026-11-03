#include "content.h"

#include "game_api.h"

namespace pm::api::content {

PmTextSet Text::c_set() const {
  view_.clear();
  for (const auto& [lang, text] : items_) view_.push_back({lang.c_str(), text.c_str()});
  return {view_.data(), static_cast<uint32_t>(view_.size())};
}

const PmGhostDef& GhostDef::c_def() const {
  tellSets_.clear();
  for (const Text& t : tells) tellSets_.push_back(t.c_set());
  def_ = {};
  def_.size = sizeof(PmGhostDef);
  def_.id = id.c_str();
  def_.name = name.c_set();
  def_.description = description.c_set();
  def_.strength = strength.c_set();
  def_.weakness = weakness.c_set();
  def_.tells = tellSets_.data();
  def_.tellCount = static_cast<uint32_t>(tellSets_.size());
  for (size_t i = 0; i < evidence.size(); ++i) def_.evidence[i] = evidence[i].c_str();
  def_.abilityName = abilityName.c_set();
  def_.abilityDescription = abilityDescription.c_set();
  def_.user = user;
  def_.useAbility = useAbility;
  def_.hudState = hudState;
  def_.heldBack = heldBack;
  def_.playerView = playerView;
  def_.evidenceLook = evidenceLook;
  def_.flags = flags;
  def_.target = target;
  def_.hudLine = hudLine;
  def_.playerMove = playerMove;
  def_.interactionLost = interactionLost;
  return def_;
}

const PmEvidenceDef& EvidenceDef::c_def() const {
  def_ = {};
  def_.size = sizeof(PmEvidenceDef);
  def_.id = id.c_str();
  def_.name = name.c_set();
  def_.journalLine = journalLine.c_set();
  return def_;
}

int32_t register_ghost(const GhostDef& def) {
  const PmHostApi* h = detail::host();
  return h ? h->ghost_register(&def.c_def()) : 0;
}

int32_t register_evidence(const EvidenceDef& def) {
  const PmHostApi* h = detail::host();
  return h ? h->evidence_register(&def.c_def()) : 0;
}

int32_t type_of(std::string_view ghostId) {
  const PmHostApi* h = detail::host();
  return h ? h->content_type(std::string(ghostId).c_str()) : -1;
}

std::string id_of(int32_t type) {
  const PmHostApi* h = detail::host();
  const char* id = h ? h->content_ghost_id(type) : nullptr;
  return id ? id : "";
}

int32_t evidence_of(std::string_view evidenceId) {
  const PmHostApi* h = detail::host();
  return h ? h->content_evidence(std::string(evidenceId).c_str()) : -1;
}

bool active() {
  const PmHostApi* h = detail::host();
  return h && h->content_active();
}

bool player_controlled() {
  const PmHostApi* h = detail::host();
  return h && h->content_player_controlled();
}

const PmGhostRules& GhostRules::c_rules() const {
  rules_ = {};
  rules_.size = sizeof(PmGhostRules);
  rules_.user = user;
  rules_.start = start;
  rules_.tick = tick;
  rules_.stop = stop;
  rules_.huntSpeed = huntSpeed;
  rules_.heldBack = heldBack;
  return rules_;
}

int32_t attach_rules(std::string_view ghostId, const GhostRules& rules) {
  const PmHostApi* h = detail::host();
  return h ? h->ghost_rules(std::string(ghostId).c_str(), &rules.c_rules()) : 0;
}

int32_t acting_as() {
  const PmHostApi* h = detail::host();
  return h ? h->content_acting_as() : -1;
}

int32_t identity() {
  const PmHostApi* h = detail::host();
  return h ? h->content_identity() : -1;
}

}  // namespace pm::api::content

namespace pm::api::content {

int32_t add_journal_lines(std::string_view vanillaGhostId, const std::vector<Text>& lines) {
  const PmHostApi* h = detail::host();
  if (!h) return 0;
  std::vector<PmTextSet> sets;
  for (const Text& t : lines) sets.push_back(t.c_set());
  return h->journal_lines(std::string(vanillaGhostId).c_str(), sets.data(), static_cast<uint32_t>(sets.size()));
}

std::string evidence_look(std::string_view evidenceId) {
  const PmHostApi* h = detail::host();
  const char* look = h ? h->content_evidence_look(std::string(evidenceId).c_str()) : nullptr;
  return look ? look : "";
}

bool use_ability() {
  const PmHostApi* h = detail::host();
  return h && h->content_use_ability();
}

std::string ability_name() {
  const PmHostApi* h = detail::host();
  const char* text = h ? h->content_ability_text(false) : nullptr;
  return text ? text : "";
}

std::string ability_description() {
  const PmHostApi* h = detail::host();
  const char* text = h ? h->content_ability_text(true) : nullptr;
  return text ? text : "";
}

int32_t hud_state() {
  const PmHostApi* h = detail::host();
  return h ? h->content_hud_state() : -1;
}

std::string hud_line(int32_t state) {
  const PmHostApi* h = detail::host();
  const char* line = h ? h->content_hud_line(state) : nullptr;
  return line ? line : "";
}

bool held_back(int32_t kind) {
  const PmHostApi* h = detail::host();
  return h && h->content_held_back(kind);
}

uint32_t player_view(float* xyz, uint32_t max) {
  const PmHostApi* h = detail::host();
  return h ? h->content_player_view(xyz, max) : 0;
}

void* target(const void* const* excluded, uint32_t excludedCount) {
  const PmHostApi* h = detail::host();
  return h ? h->content_target(excluded, excludedCount) : nullptr;
}

void player_move(const float at[3], float& dx, float& dz) {
  const PmHostApi* h = detail::host();
  if (h && at) h->content_player_move(at, &dx, &dz);
}

bool interaction_lost() {
  const PmHostApi* h = detail::host();
  return h && h->content_interaction_lost();
}

bool roll_taken() {
  const PmHostApi* h = detail::host();
  return h && h->content_roll_taken();
}

bool room_agrees() {
  const PmHostApi* h = detail::host();
  return h && h->room_agrees();
}

uint32_t flags() {
  const PmHostApi* h = detail::host();
  return h ? h->content_flags() : 0;
}

}  // namespace pm::api::content
