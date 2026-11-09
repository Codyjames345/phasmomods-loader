#include "content_types.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostHuntState.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "LevelController.h"
#include "LocalisationSystem.h"
#include "content_text.h"
#include "game_api.h"
#include "guarded.h"
#include "log.h"
#include "runtime.h"

namespace pm::host {
namespace {

using pm::api::Use;

ContentTypes* g_self = nullptr;  // the instance whose hooks are made

// Lower case, for the game's own case-insensitive comparisons of ghost names.
std::string lower(std::string_view text) {
  std::string out(text);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

// The contract's ghost, its GhostAI, or null outside a contract and before it spawns.
void* current_ghost() {
  void* level = phasmo::LevelController::instance.get();
  if (!pm::api::alive(level)) return nullptr;
  void* ghost = phasmo::LevelController::currentGhost.get(level);
  return pm::api::alive(ghost) ? ghost : nullptr;
}

// The contract's GhostController, or null.
void* current_controller() {
  void* controller = phasmo::GhostController::instance.get();
  return pm::api::alive(controller) ? controller : nullptr;
}

int32_t read_int(const void* at) {
  int32_t v;
  std::memcpy(&v, at, sizeof v);
  return v;
}

void write_int(void* at, int32_t v) { std::memcpy(at, &v, sizeof v); }

void detour_init(pm::api::Original<phasmo::GhostAI::Init> original, void* ghost, void* traits) {
  static bool logged = false;
  if (g_self && traits) guarded(logged, "setting a custom ghost up", [&] { g_self->before_init(traits); });
  original(ghost, traits);
}

void detour_sync_send(pm::api::Original<phasmo::GhostInfo::SyncValues> original, void* info, const void* traits) {
  static bool logged = false;
  // The x64 convention passes the traits as the caller's own copy, so changing them in place is the
  // callee's right; what changes is put back after the call all the same.
  void* const t = const_cast<void*>(traits);
  int32_t put = -1;
  if (g_self && t) guarded(logged, "sending a custom ghost's type", [&] { put = g_self->before_sync_send(t); });
  original(info, traits);
  if (g_self && t && put >= 0) guarded(logged, "sending a custom ghost's type", [&] { g_self->after_sync_send(t, put); });
}

void detour_sync_values(pm::api::Original<phasmo::GhostInfo::SyncValuesNetworked> original, void* info, int32_t type,
                        int32_t a, bool b, int32_t c, int32_t d, bool e, int32_t f, int32_t g, bool h, void* message) {
  static bool logged = false;
  original(info, type, a, b, c, d, e, f, g, h, message);
  if (g_self && info) guarded(logged, "syncing a custom ghost", [&] { g_self->after_sync_values(info); });
}

void detour_build_mimic_copies(pm::api::Original<phasmo::GhostController::BuildMimicCopies> original, void* controller) {
  static bool logged = false;
  original(controller);
  if (g_self && controller)
    guarded(logged, "adding custom types to the Mimic's list", [&] { g_self->after_build_mimic_copies(controller); });
}

void detour_mimic_copy(pm::api::Original<phasmo::GhostController::MimicCopy> original, void* controller) {
  static bool logged = false;
  original(controller);
  if (g_self && controller) guarded(logged, "following the Mimic's copy", [&] { g_self->after_mimic_copy(controller); });
}

void detour_pick_evidence(pm::api::Original<phasmo::GhostController::PickEvidence> original, void* controller) {
  static bool logged = false;
  if (g_self && controller) guarded(logged, "following an evidence pick", [&] { g_self->before_pick_evidence(controller); });
  struct After {  // also when the game's pick throws
    ~After() {
      if (g_self) guarded(logged, "following an evidence pick", [] { g_self->after_pick_evidence(); });
    }
  } after;
  original(controller);
}

void* detour_evidence_list(pm::api::Original<phasmo::GhostData::GetEvidenceList> original, void* data) {
  static bool logged = false;
  void* list = original(data);
  if (g_self) guarded(logged, "following an evidence pick", [] { g_self->after_evidence_list(); });
  return list;
}

void detour_update_speed(pm::api::Original<phasmo::GhostHuntState::UpdateSpeed> original, void* hunt) {
  static bool logged = false;
  if (g_self && hunt) guarded(logged, "setting a hunt's speed", [&] { g_self->before_update_speed(hunt); });
  original(hunt);
}

}  // namespace

bool ContentTypes::contract_loaded() { return current_controller() != nullptr; }

std::string ContentTypes::missing() const {
  struct Need {
    const char* key;
    bool resolved;
  };
  const Need needs[] = {
      {"LevelController.instance", Use<pm::map::LevelController_instance>::resolved()},
      {"LevelController.currentGhost", Use<pm::map::LevelController_currentGhost>::resolved()},
      {"GhostAI.info", Use<pm::map::GhostAI_info>::resolved()},
      {"GhostAI.Init", Use<pm::map::GhostAI_Init>::resolved()},
      {"GhostInfo.traits", Use<pm::map::GhostInfo_traits>::resolved()},
      {"GhostInfo.identityType", Use<pm::map::GhostInfo_identityType>::resolved()},
      {"GhostInfo.behaviourType", Use<pm::map::GhostInfo_behaviourType>::resolved()},
      {"GhostInfo.SyncValues", Use<pm::map::GhostInfo_SyncValues>::resolved()},
      {"GhostInfo.SyncValuesNetworked", Use<pm::map::GhostInfo_SyncValuesNetworked>::resolved()},
      {"GhostController.instance", Use<pm::map::GhostController_instance>::resolved()},
      {"GhostController.allGhostsCollection", Use<pm::map::GhostController_allGhostsCollection>::resolved()},
      {"GhostController.mimicCopies", Use<pm::map::GhostController_mimicCopies>::resolved()},
      {"GhostController.MimicCopy", Use<pm::map::GhostController_MimicCopy>::resolved()},
      {"GhostController.BuildMimicCopies", Use<pm::map::GhostController_BuildMimicCopies>::resolved()},
      {"GhostCollection.ghosts", Use<pm::map::GhostCollection_ghosts>::resolved()},
      {"GhostData", Use<pm::map::GhostData>::resolved()},
      {"GhostData.type", Use<pm::map::GhostData_type>::resolved()},
      {"GhostData.ghostNameLocalisationKey", Use<pm::map::GhostData_ghostNameLocalisationKey>::resolved()},
      {"GhostData.evidence1", Use<pm::map::GhostData_evidence1>::resolved()},
      {"GhostData.evidence2", Use<pm::map::GhostData_evidence2>::resolved()},
      {"GhostData.evidence3", Use<pm::map::GhostData_evidence3>::resolved()},
      {"GhostHuntState.ghost", Use<pm::map::GhostHuntState_ghost>::resolved()},
      {"GhostHuntState.baseSpeed", Use<pm::map::GhostHuntState_baseSpeed>::resolved()},
      {"GhostHuntState.sightSpeedUp", Use<pm::map::GhostHuntState_sightSpeedUp>::resolved()},
      {"GhostHuntState.UpdateSpeed", Use<pm::map::GhostHuntState_UpdateSpeed>::resolved()},
      {"GhostController.PickEvidence", Use<pm::map::GhostController_PickEvidence>::resolved()},
      {"GhostData.GetEvidenceList", Use<pm::map::GhostData_GetEvidenceList>::resolved()},
      {"LocalisationSystem.TryGetLocalisedValue", Use<pm::map::LocalisationSystem_TryGetLocalisedValue>::resolved()},
  };
  std::string out;
  for (const Need& n : needs)
    if (!n.resolved) out += (out.empty() ? "" : ", ") + std::string(n.key);
  return out;
}

bool ContentTypes::create_hooks() {
  g_self = this;
  using pm::api::hook;
  return hook<phasmo::GhostAI::Init>(&detour_init, "content: a custom ghost's AI") &&
         hook<phasmo::GhostInfo::SyncValues>(&detour_sync_send, "content: a custom ghost's type sent") &&
         hook<phasmo::GhostInfo::SyncValuesNetworked>(&detour_sync_values, "content: a custom ghost's type synced") &&
         hook<phasmo::GhostController::BuildMimicCopies>(&detour_build_mimic_copies, "content: the Mimic's list") &&
         hook<phasmo::GhostController::MimicCopy>(&detour_mimic_copy, "content: the Mimic's copy") &&
         hook<phasmo::GhostHuntState::UpdateSpeed>(&detour_update_speed, "content: hunt speed") &&
         hook<phasmo::GhostController::PickEvidence>(&detour_pick_evidence, "content: a custom ghost's evidence pick") &&
         hook<phasmo::GhostData::GetEvidenceList>(&detour_evidence_list, "content: a custom ghost's evidence list");
}

void ContentTypes::tick(content::State state, bool privateRoom, bool master, bool roomVerified) {
  void* controller = current_controller();
  if (decided_ && controller != contract_.get()) end_contract();  // it ended, or another loaded
  // The lobby offers the custom ghosts while content is active in a private room; a contract loading,
  // or content that stops being active, ends the offer.
  const bool lobby = !controller && privateRoom && state == content::State::Active;
  if (offering_ && !lobby) end_offer();
  if (controller && !decided_) begin_contract(controller, state == content::State::Active && roomVerified);
  if (lobby && !offering_) begin_offer();
  if (decided_ && !held_.empty()) {
    if (void* list = collection_list()) place(list, held_);
    add_to_mimic_list(controller);
  }
  // A withdrawal a List operation refused is tried again whenever no contract holds content: in the
  // lobby, and in a contract without custom ghosts.
  if (held_.empty()) retry_pending();
  // Outside a private room the suite behaves as if it were not installed: no rules run there.
  void* ghost = privateRoom ? current_ghost() : nullptr;
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  const int32_t identity = info ? phasmo::GhostInfo::identityType.get(info) : -1;
  const int32_t behaviour = info ? phasmo::GhostInfo::behaviourType.get(info) : -1;
  acting_.observe(identity, behaviour, master);
  acting_.tick();
}

void ContentTypes::begin_contract(void* controller, bool active) {
  decided_ = true;
  contract_ = controller;
  held_.clear();
  ++shown_;
  // What the lobby offered is not the contract's: from here only the contract's own entries have their
  // keys answered.
  if (offering_) end_offer();
  keys_.clear();
  void* list = collection_list();
  // Any of this client's numbers a lobby left in the collection (a withdrawal a List operation refused,
  // or a menu that shares the collection) come out first, so an entry with another contract's data is
  // never taken for this one's.
  if (list) {
    const auto [first, end] = own_numbers();
    take_out(list, first, end, false);
  }
  if (!active) {
    PM_LOG("content: this contract has no custom ghosts");
    return;
  }
  held_ = active_ghosts();
  registry_.hold_contract(true);  // its numbers answer until it ends, whatever the room does
  std::map<int32_t, std::string> ids;
  for (const auto& [type, h] : held_) ids[type] = h.id;
  acting_.pin(std::move(ids));
  placed_ = list;
  PM_LOG("content: this contract has %zu custom ghosts", held_.size());
}

std::map<int32_t, ContentTypes::HeldGhost> ContentTypes::active_ghosts() const {
  std::map<int32_t, HeldGhost> out;
  for (const int32_t type : registry_.custom_types()) {
    const char* id = registry_.id_of(type);
    const content::Ghost* g = id ? registry_.ghost(id) : nullptr;
    if (!g) continue;
    HeldGhost h{g->id, g->name, {}, (g->flags & PM_GHOST_KEEPS_LAST_EVIDENCE) != 0,
                (g->flags & PM_GHOST_NO_MIMIC_COPY) != 0};
    bool known = true;
    for (size_t i = 0; i < 3; ++i) {
      h.evidence[i] = g->evidence[i].empty() ? registry_.game_enums().noEvidence : registry_.evidence_of(g->evidence[i]);
      known = known && h.evidence[i] >= 0;
    }
    if (!known) continue;  // a custom evidence not in play: this ghost cannot give it
    out[type] = std::move(h);
  }
  return out;
}

bool ContentTypes::offer(void* list) {
  if (!list || !offering_) return false;
  if (std::none_of(offeredTo_.begin(), offeredTo_.end(), [&](const pm::api::Held& l) { return l.get() == list; }))
    offeredTo_.emplace_back(list);
  place(list, offered_);
  const pm::api::List entries{list};
  for (const auto& [type, h] : offered_) {
    bool there = false;
    for (size_t i = 0; i < entries.size() && !there; ++i)
      there = entries[i] && phasmo::GhostData::ghostType.get(entries[i]) == type;
    if (!there) return false;
  }
  return !offered_.empty();
}

void ContentTypes::withdraw_offer(void* list) {
  const auto [first, end] = own_numbers();
  take_out(list, first, end, false);
  std::erase_if(offeredTo_, [&](const pm::api::Held& l) { return l.get() == list; });
}

void ContentTypes::begin_offer() {
  offered_ = active_ghosts();
  offering_ = true;
  ++shown_;
  PM_LOG("content: the lobby offers %zu custom ghosts", offered_.size());
}

void ContentTypes::end_offer() {
  const auto [first, end] = own_numbers();
  for (const pm::api::Held& list : offeredTo_)
    if (list.get()) take_out(list.get(), first, end, false);
  offeredTo_.clear();
  offered_.clear();
  offering_ = false;
  if (held_.empty()) keys_.clear();
  ++shown_;
  PM_LOG("content: the lobby no longer offers custom ghosts");
}

void ContentTypes::end_contract() {
  if (placed_.get() && !held_.empty()) {
    const auto [first, end] = own_numbers();
    take_out(placed_.get(), first, end, false);
  }
  placed_ = nullptr;
  held_.clear();
  keys_.clear();
  acting_.pin({});
  registry_.hold_contract(false);
  contract_ = nullptr;
  decided_ = false;
  ++shown_;
}

void ContentTypes::withdraw() {
  if (offering_) end_offer();
  const auto [first, end] = own_numbers();
  if (void* list = collection_list()) take_out(list, first, end, false);
  if (placed_.get() && placed_.get() != collection_list()) take_out(placed_.get(), first, end, false);
  if (void* controller = current_controller())
    if (void* mimic = phasmo::GhostController::mimicCopies.get(controller)) take_out(mimic, first, end, true);
  placed_ = nullptr;
  held_.clear();
  keys_.clear();
  ++shown_;
  acting_.pin({});
  registry_.hold_contract(false);
  acting_.stop();
}

std::optional<std::string> ContentTypes::name_for_key(std::string_view key, std::string_view lang) const {
  const auto it = keys_.find(key);
  if (it == keys_.end()) return std::nullopt;
  const HeldGhost* h = nullptr;
  if (const auto c = held_.find(it->second); c != held_.end()) h = &c->second;
  else if (const auto o = offered_.find(it->second); o != offered_.end()) h = &o->second;
  if (!h) return std::nullopt;
  const std::string& name = pick(h->name, lang);
  if (name.empty()) return std::nullopt;
  return name;
}

std::optional<std::string> ContentTypes::name_key(int32_t type) const {
  for (const auto& [key, t] : keys_)
    if (t == type) return key;
  return std::nullopt;
}

void* ContentTypes::collection() const {
  void* controller = current_controller();
  void* collection = controller ? phasmo::GhostController::allGhostsCollection.get(controller) : nullptr;
  return collection && pm::api::alive(collection) ? collection : nullptr;
}

bool ContentTypes::custom(int32_t type) const {
  // Before the first look at a new contract (the game may set its ghost up in that very frame) the
  // registry answers; after it, the contract's decision.
  return decided_ ? held_.count(type) > 0 : registry_.is_custom(type);
}

std::pair<int32_t, int32_t> ContentTypes::own_numbers() const {
  const content::GameEnums& enums = registry_.game_enums();
  if (!enums.read()) return {0, 0};
  int32_t end = enums.firstGhostType;
  while (registry_.own_range(end)) ++end;
  for (const auto& [type, h] : held_) end = std::max(end, type + 1);
  return {enums.firstGhostType, end};
}

int32_t ContentTypes::traits_offset(bool identity) const {
  const PmGameEntry* traits = Use<pm::map::GhostInfo_traits>::entry();
  const PmGameEntry* field = identity ? Use<pm::map::GhostInfo_identityType>::entry()
                                      : Use<pm::map::GhostInfo_behaviourType>::entry();
  return traits && field && field->offset >= traits->offset ? field->offset - traits->offset : -1;
}

void ContentTypes::before_init(void* traits) const {
  const int32_t id = traits_offset(true), beh = traits_offset(false);
  if (id < 0 || beh < 0) return;
  auto* t = static_cast<char*>(traits);
  if (custom(read_int(t + id))) write_int(t + beh, registry_.game_enums().noGhostType);
}

int32_t ContentTypes::before_sync_send(void* traits) const {
  const int32_t id = traits_offset(true), beh = traits_offset(false);
  if (id < 0 || beh < 0) return -1;
  auto* t = static_cast<char*>(traits);
  const int32_t identity = read_int(t + id), behaviour = read_int(t + beh);
  if (!custom(identity) || behaviour == identity) return -1;
  write_int(t + beh, identity);
  return behaviour;
}

void ContentTypes::after_sync_send(void* traits, int32_t behaviour) const {
  const int32_t beh = traits_offset(false);
  if (beh >= 0) write_int(static_cast<char*>(traits) + beh, behaviour);
}

void ContentTypes::after_sync_values(void* info) const {
  if (custom(phasmo::GhostInfo::identityType.get(info)))
    phasmo::GhostInfo::behaviourType.set(info, registry_.game_enums().noGhostType);
}

void ContentTypes::after_build_mimic_copies(void* controller) {
  if (!held_.empty()) add_to_mimic_list(controller);
}

void ContentTypes::after_mimic_copy(void*) {
  void* ghost = current_ghost();
  void* info = ghost ? phasmo::GhostAI::info.get(ghost) : nullptr;
  if (!info) return;
  const int32_t copied = phasmo::GhostInfo::behaviourType.get(info);
  // The generic AI runs a custom ghost: its behaviour type is the game's "no type", as for one of its own.
  if (custom(copied)) phasmo::GhostInfo::behaviourType.set(info, registry_.game_enums().noGhostType);
  acting_.copied(phasmo::GhostInfo::identityType.get(info), copied);
}

bool ContentTypes::keeps_last_evidence(int32_t type) const {
  if (!custom(type)) return false;
  if (decided_) {
    const auto it = held_.find(type);
    return it != held_.end() && it->second.keepsLastEvidence;
  }
  // Before a decision: the active content's own definition, as active_ghosts reads it.
  const char* id = registry_.id_of(type);
  const content::Ghost* g = id ? registry_.ghost(id) : nullptr;
  return g && (g->flags & PM_GHOST_KEEPS_LAST_EVIDENCE) != 0;
}

void ContentTypes::before_pick_evidence(void* controller) {
  after_pick_evidence();  // a pick never nests; whatever was left is put back
  const int32_t type = phasmo::GhostController::behaviourType.get(controller);
  if (!keeps_last_evidence(type)) return;
  if (!Use<pm::map::GhostType_Goryo>::resolved() || !Use<pm::map::GhostController_behaviourType>::resolved()) {
    if (!priorityLogged_) PM_LOG("content: the Goryo's type is not available, so a custom ghost's last evidence is withheld as any");
    priorityLogged_ = true;
    return;
  }
  picking_ = controller;
  pickType_ = type;
}

void ContentTypes::after_evidence_list() {
  if (!picking_ || pickSwitched_ || !pm::api::alive(picking_)) return;
  if (phasmo::GhostController::behaviourType.get(picking_) != pickType_) return;
  phasmo::GhostController::behaviourType.set(picking_, phasmo::GhostType::Goryo());
  pickSwitched_ = true;
}

void ContentTypes::after_pick_evidence() {
  if (pickSwitched_ && picking_ && pm::api::alive(picking_))
    phasmo::GhostController::behaviourType.set(picking_, pickType_);
  picking_ = nullptr;
  pickType_ = -1;
  pickSwitched_ = false;
}

void ContentTypes::before_update_speed(void* hunt) {
  void* ghost = current_ghost();
  if (!ghost || phasmo::GhostHuntState::ghost.get(hunt) != ghost) return;
  const float current = phasmo::GhostHuntState::baseSpeed.get(hunt);
  // The game writes its own base from time to time (RefreshBase, every two seconds): whatever differs
  // from what was last written here is the game's.
  if (!wrote_ || current != written_) gameBase_ = current;
  float base = gameBase_, sight = phasmo::GhostHuntState::sightSpeedUp.get(hunt);
  if (acting_.hunt_speed(base, sight)) {
    phasmo::GhostHuntState::baseSpeed.set(hunt, base);
    phasmo::GhostHuntState::sightSpeedUp.set(hunt, sight);
    written_ = base;
    wrote_ = true;
  } else if (wrote_) {
    phasmo::GhostHuntState::baseSpeed.set(hunt, gameBase_);
    wrote_ = false;
  }
}

void* ContentTypes::collection_list() const {
  void* controller = current_controller();
  void* collection = controller ? phasmo::GhostController::allGhostsCollection.get(controller) : nullptr;
  return collection && pm::api::alive(collection) ? phasmo::GhostCollection::ghosts.get(collection) : nullptr;
}

bool ContentTypes::learn_key_form(void* list) {
  if (keyForm_) return true;
  // A game ghost's name key holds its GhostType value's name; what stands around it is the form every
  // type's key takes, a custom type's number included. Each game entry allows the splits around each
  // place its name stands in its key; the form is the one split every entry allows, and there must be
  // exactly one.
  const content::GameEnums& enums = registry_.game_enums();
  const pm::api::List ghosts{list};
  std::vector<std::pair<std::string, std::string>> forms;
  bool first = true;
  for (size_t i = 0; i < ghosts.size(); ++i) {
    void* data = ghosts[i];
    const char* id = data ? enums.ghost_id(phasmo::GhostData::ghostType.get(data)) : nullptr;
    if (!id) continue;
    const std::string key = pm::api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(data));
    const std::string name(std::string_view(id).substr(content::kVanillaPrefix.size()));
    const std::string folded = lower(key);
    std::vector<std::pair<std::string, std::string>> splits;
    for (size_t at = folded.find(name); at != std::string::npos; at = folded.find(name, at + 1))
      splits.emplace_back(key.substr(0, at), key.substr(at + name.size()));
    if (first) forms = splits;
    else std::erase_if(forms, [&](const auto& f) { return std::find(splits.begin(), splits.end(), f) == splits.end(); });
    first = false;
  }
  if (forms.size() != 1) {
    if (!first && !keyFormLogged_)
      PM_LOG("content: the game's ghost name keys show no one form; custom ghosts get no name key");
    keyFormLogged_ = !first;
    return false;
  }
  keyPrefix_ = forms[0].first;
  keySuffix_ = forms[0].second;
  keyForm_ = true;
  return true;
}

std::string ContentTypes::sort_name(void* data) const {
  const int32_t type = phasmo::GhostData::ghostType.get(data);
  if (const auto h = held_.find(type); h != held_.end()) return lower(pick(h->second.name, "en"));
  if (const auto h = offered_.find(type); h != offered_.end()) return lower(pick(h->second.name, "en"));
  const std::string key = pm::api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(data));
  if (key.size() >= keyPrefix_.size() + keySuffix_.size() && key.starts_with(keyPrefix_) && key.ends_with(keySuffix_))
    return lower(key.substr(keyPrefix_.size(), key.size() - keyPrefix_.size() - keySuffix_.size()));
  return lower(key);
}

void ContentTypes::place(void* list, const std::map<int32_t, HeldGhost>& wanted) {
  if (!learn_key_form(list)) return;
  for (const auto& [type, h] : wanted) {
    const pm::api::List ghosts{list};
    bool there = false;
    for (size_t i = 0; i < ghosts.size() && !there; ++i)
      there = ghosts[i] && phasmo::GhostData::ghostType.get(ghosts[i]) == type;
    const std::string key = keyPrefix_ + std::to_string(type) + keySuffix_;
    if (!there) {
      void* data = phasmo::GhostData::type.make();
      if (!data) continue;
      phasmo::GhostData::ghostType.set(data, type);
      phasmo::GhostData::ghostNameLocalisationKey.set(data, pm::api::make_string(key));
      phasmo::GhostData::evidence1.set(data, h.evidence[0]);
      phasmo::GhostData::evidence2.set(data, h.evidence[1]);
      phasmo::GhostData::evidence3.set(data, h.evidence[2]);
      // Its alphabetical place among the entries, as the game sorts its own.
      const std::string name = sort_name(data);
      size_t at = 0;
      while (at < ghosts.size() && ghosts[at] && sort_name(ghosts[at]) <= name) ++at;
      if (!lists_.insert(list, static_cast<int32_t>(at), data)) {
        if (!insertLogged_) PM_LOG("content: the ghost \"%s\" could not join the ghost collection; tried again", h.id.c_str());
        insertLogged_ = true;
        continue;
      }
      insertLogged_ = false;  // a later refusal is news again
    }
    keys_[key] = type;  // answered only once the entry is there
  }
}

void ContentTypes::take_out(void* list, int32_t first, int32_t end, bool values) {
  if (!list || first >= end) return;
  if (remove_types(list, first, end, values)) {
    if (pending_.empty()) removeLogged_ = false;  // a later refusal is news again
    return;
  }
  if (!removeLogged_)
    PM_LOG("content: custom ghosts could not all leave %s; it is tried again", values ? "the Mimic's list" : "the ghost collection");
  removeLogged_ = true;
  for (const Pending& p : pending_)
    if (p.list.get() == list) return;
  pending_.push_back({pm::api::Held(list), first, end, values});
}

bool ContentTypes::remove_types(void* list, int32_t first, int32_t end, bool values) {
  const pm::api::List l{list};
  bool clean = true;
  int taken = 0;
  for (size_t i = l.size(); i-- > 0;) {
    int32_t type = -1;
    if (values) type = pm::api::Array<int32_t>{l.items()}[i];
    else if (void* data = l[i]) type = phasmo::GhostData::ghostType.get(data);
    if (type < first || type >= end) continue;
    if (lists_.removeAt(list, static_cast<int32_t>(i))) ++taken;
    else clean = false;
  }
  if (taken) PM_LOG("content: %d custom ghosts left %s", taken, values ? "the Mimic's list" : "the ghost collection");
  return clean;
}

void ContentTypes::retry_pending() {
  std::erase_if(pending_, [&](Pending& p) {
    if (!p.list.get() || !remove_types(p.list.get(), p.first, p.end, p.values)) return false;
    PM_LOG("content: a withdrawal that failed earlier is done");
    return true;
  });
  if (pending_.empty()) removeLogged_ = false;
}

void ContentTypes::add_to_mimic_list(void* controller) {
  void* list = controller ? phasmo::GhostController::mimicCopies.get(controller) : nullptr;
  if (!list) return;  // the game builds it at the Mimic's first copy
  const pm::api::List l{list};
  const pm::api::Array<int32_t> items{l.items()};
  std::vector<int32_t> there;
  for (size_t i = 0; i < l.size(); ++i) there.push_back(items[i]);
  for (const auto& [type, h] : held_) {
    if (std::find(there.begin(), there.end(), type) != there.end()) continue;
    if (h.noMimicCopy) continue;  // never copied (PM_GHOST_NO_MIMIC_COPY)
    if (lists_.addValue(list, type)) {
      addLogged_ = false;  // a later refusal is news again
      continue;
    }
    if (!addLogged_) PM_LOG("content: the Mimic's list could not take the custom ghosts; tried again");
    addLogged_ = true;
  }
}

}  // namespace pm::host
