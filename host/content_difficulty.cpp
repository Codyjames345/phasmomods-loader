#include "content_difficulty.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iterator>

#include "Difficulty.h"
#include "GhostCollection.h"
#include "GhostTypes.h"
#include "LevelController.h"
#include "UnityObject.h"
#include "content_text.h"
#include "game_api.h"
#include "guarded.h"
#include "log.h"
#include "mod_ids.h"
#include "runtime.h"

namespace pm::host {
namespace {

using pm::api::Use;

ContentDifficulty* g_self = nullptr;  // the instance whose hooks are made
constexpr uint64_t kSearchMs = 1000;  // FindObjectOfType searches every loaded object: once a second

void detour_choice(pm::api::Original<phasmo::DifficultyUIManager::GhostTypeChosen> original, void* menu) {
  static bool logged = false;
  original(menu);
  if (g_self && menu) guarded(logged, "keeping a difficulty's ghost", [&] { g_self->after_choice(menu); });
}

void detour_write(pm::api::Original<phasmo::ES3UserType_Difficulty::WriteScriptableObject> original, void* es3type,
                  void* d, void* writer) {
  static bool logged = false;
  int32_t put = -1;
  if (g_self && d) guarded(logged, "keeping a custom ghost out of a save", [&] { put = g_self->before_write(d); });
  // What was changed is put back whatever the write does, a managed exception included.
  struct Restore {
    void* d;
    int32_t put;
    ~Restore() {
      static bool restoreLogged = false;
      if (g_self && put >= 0) guarded(restoreLogged, "putting a difficulty's ghost back", [&] { g_self->after_write(d, put); });
    }
  } restore{d, put};
  original(es3type, d, writer);
}

void* detour_summary(pm::api::Original<phasmo::DifficultyUIManager::SummaryText> original, void* menu, void* d) {
  struct Scope {
    Scope() {
      if (g_self) g_self->enter_summary();
    }
    ~Scope() {
      if (g_self) g_self->leave_summary();
    }
  } scope;
  return original(menu, d);
}

void* detour_enum_name(pm::api::Original<phasmo::Enum::ToString> original, void* boxed) {
  static bool logged = false;
  void* name = nullptr;
  if (g_self && boxed)
    guarded(logged, "naming a custom ghost in the summary", [&] {
      if (const auto n = g_self->enum_name(boxed)) name = pm::api::make_string(*n);
    });
  return name ? name : static_cast<void*>(original(boxed));
}

const pm::sdk::EntryDesc* const kNeeded[] = {
    &pm::map::Difficulty_difficulty, &pm::map::Difficulty_ghostTypeOverride, &pm::map::DifficultyOption_index,
    &pm::map::DifficultyOption_count, &pm::map::DifficultyOption_ButtonCheck, &pm::map::DifficultyUIManager,
    &pm::map::DifficultyUIManager_ghostType, &pm::map::DifficultyUIManager_allGhostsData,
    &pm::map::DifficultyUIManager_presets, &pm::map::DifficultyUIManager_current,
    &pm::map::DifficultyUIManager_applied, &pm::map::DifficultyUIManager_ForceSyncDifficulty,
    &pm::map::DifficultyUIManager_GhostTypeChosen, &pm::map::DifficultyUIManager_SummaryText,
    &pm::map::ES3UserType_Difficulty_WriteScriptableObject, &pm::map::LevelValues_get_Instance,
    &pm::map::LevelValues_difficulty, &pm::map::SystemEnum_ToString, &pm::map::SystemEnum_GetHashCode,
    &pm::map::UnityObject_FindObjectOfType, &pm::map::GhostType, &pm::map::GhostCollection_ghosts,
    &pm::map::GhostData_type, &pm::map::LevelController_instance,
};

int32_t ghost_of(void* d) { return phasmo::Difficulty::ghostTypeOverride.get(d); }
void set_ghost(void* d, int32_t type) { phasmo::Difficulty::ghostTypeOverride.set(d, type); }
int32_t preset_of(void* d) { return phasmo::Difficulty::difficulty.get(d); }

void* level_difficulty() {
  void* values = phasmo::LevelValues::get_Instance();
  return values ? phasmo::LevelValues::difficulty.get(values) : nullptr;
}

}  // namespace

const std::string& DifficultyPicks::of(int32_t preset) const {
  static const std::string none;
  const auto it = picks_.find(preset);
  return it == picks_.end() ? none : it->second;
}

bool DifficultyPicks::choose(int32_t preset, const std::string& id) {
  if (id.empty()) return picks_.erase(preset) > 0;
  const bool changed = of(preset) != id;
  picks_[preset] = id;
  return changed;
}

std::string DifficultyPicks::text() const {
  std::string out;
  for (const auto& [preset, id] : picks_) out += std::to_string(preset) + "=" + id + "\n";
  return out;
}

DifficultyPicks DifficultyPicks::parse(std::string_view text, int& legacy, int* migrated) {
  DifficultyPicks picks;
  legacy = 0;
  const auto number = [](std::string_view s, int32_t& out) {
    const auto [end, error] = std::from_chars(s.data(), s.data() + s.size(), out);
    return !s.empty() && error == std::errc() && end == s.data() + s.size();
  };
  while (!text.empty()) {
    const size_t end = text.find('\n');
    std::string_view line = text.substr(0, end);
    text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const size_t eq = line.find('=');
    int32_t preset = 0, type = 0;
    if (eq == std::string_view::npos || !number(line.substr(0, eq), preset)) continue;
    const std::string_view value = line.substr(eq + 1);
    // An id holds a mod id and a name ("<mod>.<name>"); the older form kept the type's number.
    if (number(value, type)) {
      ++legacy;
      continue;
    }
    std::string id = modid::migrated_content_id(value);
    if (!modid::split_content_id(id)) continue;
    if (migrated && id != value) ++*migrated;
    picks.choose(preset, std::move(id));
  }
  return picks;
}

std::string ContentDifficulty::missing() const {
  std::string out;
  const PmHostApi* host = pm::api::detail::host();
  for (const pm::sdk::EntryDesc* d : kNeeded)
    if (!host || !host->game_entry(d->key)) out += (out.empty() ? "" : ", ") + std::string(d->key);
  return out;
}

bool ContentDifficulty::create_hooks() {
  g_self = this;
  using pm::api::hook;
  return hook<phasmo::DifficultyUIManager::GhostTypeChosen>(&detour_choice, "content: the difficulty's ghost") &&
         hook<phasmo::ES3UserType_Difficulty::WriteScriptableObject>(&detour_write, "content: a difficulty written") &&
         hook<phasmo::DifficultyUIManager::SummaryText>(&detour_summary, "content: the difficulty's summary") &&
         hook<phasmo::Enum::ToString>(&detour_enum_name, "content: a custom ghost's name in the summary");
}

void ContentDifficulty::load(std::wstring file) {
  file_ = std::move(file);
  std::ifstream in(file_, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  int migrated = 0;
  picks_ = DifficultyPicks::parse(text, legacy_, &migrated);
  if (legacy_ > 0)
    PM_LOG("content: custom_difficulty.txt holds %d picks by type number from an older version, which no longer "
           "name a ghost; those presets play Random until a ghost is picked again",
           legacy_);
  if (migrated > 0) {
    PM_LOG("content: custom_difficulty.txt holds %d picks by a bundled mod's old id; they move to its new id", migrated);
    save();
  }
}

void ContentDifficulty::save() {
  if (file_.empty()) return;
  std::ofstream out(file_, std::ios::binary | std::ios::trunc);
  out << picks_.text();
  out.close();
  if (out.fail()) {
    if (!saveLogged_) PM_LOG("content: the difficulty picks could not be kept; they last until the game closes");
    saveLogged_ = true;
    return;
  }
  saveLogged_ = false;
  if (legacy_ > 0)
    PM_LOG("content: custom_difficulty.txt no longer holds the older picks by type number; it keeps picks by ghost "
           "id now");
  legacy_ = 0;
}

bool ContentDifficulty::ours(int32_t type) const { return registry_.own_range(type) || registry_.is_custom(type); }

bool ContentDifficulty::agreed_room() const {
  return state_ == content::State::Active && roomVerified_ && roomVerified_();
}

bool ContentDifficulty::custom_preset(void* menu, int32_t preset) const {
  const pm::api::RefArray presets{phasmo::DifficultyUIManager::presets.get(menu)};
  for (size_t i = 0; i < presets.size(); ++i)
    if (presets[i] && preset_of(presets[i]) == preset) return true;
  return false;
}

void* ContentDifficulty::find_menu(uint64_t nowMs) {
  if (pm::api::alive(menu_.get())) return menu_.get();
  menu_ = nullptr;
  if (nowMs < nextSearch_) return nullptr;
  nextSearch_ = nowMs + kSearchMs;
  // Inactive ones too: the game hides the board, menu and all, unless the player is on it, and a
  // contract started without opening it must still get the pick.
  void* menu = phasmo::Object::FindObjectOfType(phasmo::DifficultyUIManager::type.type_object(), true);
  if (!pm::api::alive(menu)) return nullptr;
  menu_ = menu;
  return menu;
}

void* ContentDifficulty::menu_list(void* menu) const {
  void* collection = phasmo::DifficultyUIManager::allGhostsData.get(menu);
  return collection ? phasmo::GhostCollection::ghosts.get(collection) : nullptr;
}

void ContentDifficulty::fit_option(void* menu) {
  void* option = phasmo::DifficultyUIManager::ghostType.get(menu);
  void* list = menu_list(menu);
  if (!option || !list) return;
  // The handler's choices: index 0 Random, index i the collection's ghost i - 1.
  const int32_t count = static_cast<int32_t>(pm::api::List{list}.size()) + 1;
  if (phasmo::DifficultyOption::count.get(option) == count) return;
  phasmo::DifficultyOption::count.set(option, count);
  const int32_t index = phasmo::DifficultyOption::index.get(option);
  if (index < 0 || index >= count) phasmo::DifficultyOption::index.set(option, 0);
  phasmo::DifficultyOption::ButtonCheck(option);
}

bool ContentDifficulty::give_back(void* menu, bool master) {
  const int32_t none = registry_.game_enums().noGhostType;
  const auto take = [&](void* d) {
    if (!d || ghost_of(d) != none || !custom_preset(menu, preset_of(d))) return false;
    const std::string& id = picks_.of(preset_of(d));
    const int32_t type = id.empty() ? -1 : registry_.type_of(id);
    if (!registry_.is_custom(type)) return false;
    set_ghost(d, type);
    return true;
  };
  // The lobby's own first: a preset may be that very object (the preset choice makes it so, and a load
  // reads into the live object its saved reference names), and giving the preset its pick first would
  // hide that the lobby's difficulty changed, so it would never be sent.
  bool changed = false;
  if (master)
    for (void* d : {static_cast<void*>(phasmo::DifficultyUIManager::current.get(menu)),
                    static_cast<void*>(phasmo::DifficultyUIManager::applied.get(menu)), level_difficulty()})
      changed = take(d) || changed;
  const pm::api::RefArray presets{phasmo::DifficultyUIManager::presets.get(menu)};
  for (size_t i = 0; i < presets.size(); ++i) take(presets[i]);
  if (changed) PM_LOG("content: the lobby's difficulty gets its custom ghost back");
  return changed;
}

void ContentDifficulty::send(void* menu) {
  void* d = phasmo::DifficultyUIManager::current.get(menu);
  if (!d) d = level_difficulty();
  if (!d) return;
  // The RPC runs on this client at once too: SyncDifficultyValues makes the lobby's difficulty, its
  // settled copy and LevelValues.difficulty from what is sent.
  sending_ = true;
  struct Done {
    bool& flag;
    ~Done() { flag = false; }
  } done{sending_};
  phasmo::DifficultyUIManager::ForceSyncDifficulty(menu, d);
}

void ContentDifficulty::show_pick(void* menu) {
  void* d = phasmo::DifficultyUIManager::current.get(menu);
  void* option = phasmo::DifficultyUIManager::ghostType.get(menu);
  void* list = menu_list(menu);
  if (!d || !option || !list) return;
  // The game shows a loaded or synced difficulty by writing the option's index alone, Random when the
  // collection lacks its ghost, and offering or withdrawing the custom ghosts moves the ghosts after
  // their places: the option shows the lobby's ghost again, named by the game's own handler.
  const int32_t type = ghost_of(d);
  int32_t index = -1;
  if (type == registry_.game_enums().noGhostType) index = 0;
  const pm::api::List ghosts{list};
  for (size_t i = 0; index < 0 && i < ghosts.size(); ++i)
    if (ghosts[i] && phasmo::GhostData::ghostType.get(ghosts[i]) == type) index = static_cast<int32_t>(i) + 1;
  if (index < 0 || phasmo::DifficultyOption::index.get(option) == index) return;
  phasmo::DifficultyOption::index.set(option, index);
  showing_ = true;
  struct Done {
    bool& flag;
    ~Done() { flag = false; }
  } done{showing_};
  phasmo::DifficultyUIManager::GhostTypeChosen(menu);
  phasmo::DifficultyOption::ButtonCheck(option);
}

void ContentDifficulty::set_aside(void* menu, bool master) {
  const int32_t none = registry_.game_enums().noGhostType;
  void* current = phasmo::DifficultyUIManager::current.get(menu);
  void* contract = level_difficulty();
  bool changed = false;
  const auto aside = [&](void* d) {
    if (!d || !ours(ghost_of(d))) return;
    set_ghost(d, none);
    changed = changed || d == current || d == contract;
  };
  const pm::api::RefArray presets{phasmo::DifficultyUIManager::presets.get(menu)};
  for (size_t i = 0; i < presets.size(); ++i) aside(presets[i]);
  for (void* d : {current, static_cast<void*>(phasmo::DifficultyUIManager::applied.get(menu)), contract}) aside(d);
  if (master && changed) send(menu);
}

void ContentDifficulty::withdraw_menu(void* menu, bool master) {
  set_aside(menu, master);
  if (void* list = menu_list(menu)) types_.withdraw_offer(list);
  fit_option(menu);
  show_pick(menu);
  offeredTo_ = nullptr;
  PM_LOG("content: the difficulty's Ghost Type option offers the game's ghosts only");
}

void ContentDifficulty::keep_contract_vanilla() {
  // A contract the custom ghosts stay out of must not force one: its spawn would make a ghost the game
  // has no data for.
  if (!types_.decided() || !types_.held().empty()) return;
  void* d = level_difficulty();
  if (!d || !ours(ghost_of(d))) return;
  set_ghost(d, registry_.game_enums().noGhostType);
  if (!vanillaLogged_) PM_LOG("content: this contract has no custom ghosts; its ghost is rolled instead");
  vanillaLogged_ = true;
}

void ContentDifficulty::tick(content::State state, bool privateRoom, bool master, uint64_t nowMs) {
  state_ = privateRoom ? state : content::State::Off;
  if (state_ != content::State::Active) sentActive_ = false;  // a send is due once it is active again
  if (pm::api::alive(phasmo::LevelController::instance.get())) {
    keep_contract_vanilla();
    return;
  }
  vanillaLogged_ = false;
  void* menu = find_menu(nowMs);
  if (!menu) return;
  if (state_ == content::State::Active) {
    if (void* list = menu_list(menu)) types_.offer(list);
    fit_option(menu);
    if (offeredTo_.get() != menu) PM_LOG("content: the difficulty's Ghost Type option offers the custom ghosts");
    offeredTo_ = menu;
    const bool changed = give_back(menu, master);
    // A player who joined was sent Random: a pick is due again once they are verified.
    const bool verified = agreed_room();
    if (!verified) sentActive_ = false;
    if (master && (changed || !sentActive_)) {
      void* d = phasmo::DifficultyUIManager::current.get(menu);
      if (!d) d = level_difficulty();
      // A custom pick goes only to verified players, and waits while anyone in the room now is not.
      if (!d || !registry_.is_custom(ghost_of(d))) sentActive_ = true;
      else if (verified) {
        send(menu);
        sentActive_ = true;
      }
    }
    show_pick(menu);
  } else {
    // Not active (off, or pending: the host not heard from yet, or this client's mods not matching its list):
    // the menu offers the game's ghosts only, as a contract loading now would play
    // none of the custom ones, and every custom type comes out of the lobby the menu was found in,
    // offered to it or not (a sync may have brought one). Each pick comes back once content is active.
    if (offeredTo_.get() == menu) withdraw_menu(menu, master);
    else set_aside(menu, master);
  }
}

void ContentDifficulty::withdraw(bool master) {
  if (pm::api::alive(menu_.get()) && offeredTo_.get() == menu_.get()) withdraw_menu(menu_.get(), master);
  menu_ = nullptr;
  offeredTo_ = nullptr;
  nextSearch_ = 0;
  sentActive_ = false;
  state_ = content::State::Off;
}

void ContentDifficulty::after_choice(void* menu) {
  if (showing_ || !registry_.own_range(registry_.game_enums().firstGhostType)) return;  // no content ghosts
  void* d = phasmo::DifficultyUIManager::current.get(menu);
  if (!d || !custom_preset(menu, preset_of(d))) return;
  const int32_t type = ghost_of(d);
  const char* id = registry_.is_custom(type) ? registry_.id_of(type) : nullptr;
  const int32_t preset = preset_of(d);
  if (!picks_.choose(preset, id ? id : "")) return;
  PM_LOG("content: custom difficulty preset %d now forces %s", preset, id ? id : "no custom ghost");
  save();
}

int32_t ContentDifficulty::before_write(void* d) const {
  const int32_t type = ghost_of(d);
  if (!ours(type)) return -1;
  if (sending_ && agreed_room() && registry_.is_custom(type)) return -1;
  set_ghost(d, registry_.game_enums().noGhostType);
  return type;
}

void ContentDifficulty::after_write(void* d, int32_t type) const { set_ghost(d, type); }

std::optional<std::string> ContentDifficulty::enum_name(void* boxed) const {
  if (summarising_ <= 0 || !phasmo::GhostType::type.is(boxed)) return std::nullopt;
  const int32_t type = phasmo::Enum::GetHashCode(boxed);
  const char* id = registry_.is_custom(type) ? registry_.id_of(type) : nullptr;
  const content::Ghost* g = id ? registry_.ghost(id) : nullptr;
  if (!g) return std::nullopt;
  const std::string& name = pick(g->name, game_language(true));
  if (name.empty()) return std::nullopt;
  return name;
}

}  // namespace pm::host
