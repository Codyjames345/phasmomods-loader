#include "difficulty.h"

#include <windows.h>

#include <fstream>
#include <string>
#include <vector>

#include "difficulty_rule.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "registry.h"
#include "text_files.h"

namespace pg::custom::difficulty {
namespace {

// Build 25811160 (verified by the host at load); obfuscated names, so by offset (REA).
constexpr size_t kDifficultyPreset = 0x18;       // Difficulty.difficulty: 6..8 are Custom 1..3
constexpr size_t kDifficultyGhostType = 0x7C;    // Difficulty.ghostTypeOverride: None is Random
constexpr size_t kMenuGhostOption = 0xC8;        // DifficultyUIManager: the Ghost Type DifficultyOption
constexpr size_t kMenuPresets = 0x2A8;           // DifficultyUIManager: Difficulty[] Custom 1..3, loaded from the save
constexpr size_t kMenuCurrent = 0x2B8;           // DifficultyUIManager: the lobby's difficulty
constexpr size_t kMenuApplied = 0x2C0;           // DifficultyUIManager: its copy with the weather settled
constexpr size_t kMenuGhosts = 0x2C8;            // DifficultyUIManager.allGhostsData: GhostCollection
constexpr size_t kCollectionGhosts = 0x18;       // GhostCollection.ghosts: List<GhostData>
constexpr size_t kDataType = 0x18;               // GhostData.type
constexpr size_t kOptionIndex = 0x6C;            // DifficultyOption: the choice shown
constexpr size_t kOptionCount = 0x70;            // DifficultyOption: how many choices (31 as the scene ships it)
constexpr size_t kLevelValuesDifficulty = 0x38;  // LevelValues: the difficulty the contract plays
// The Ghost Type option's change handler: index 0 is None, index i the collection's ghost i - 1, and
// the option's label names it. It runs only for the player's own arrows and dice; the game shows a
// loaded or synced difficulty by writing the index alone.
constexpr uintptr_t kGhostChoiceRva = 0x96DD20;
// EasySave3's generic code, shared by every class it saves (Difficulty, Loadout, ...).
constexpr uintptr_t kSaveRva = 0x2ECD840;       // ES3.Save<T>(string key, T value)
constexpr uintptr_t kSerializeRva = 0x2ECCA40;  // ES3.Serialize<T>(T value, ES3Settings): the lobby sync's bytes
// The custom difficulty's summary (string(Difficulty)) names its ghost by GhostType.ToString(), which
// prints a type with no enum name as its number. 0x96C920 is an obfuscation decoy of it.
constexpr uintptr_t kSummaryRva = 0x95E010;
constexpr uintptr_t kEnumNameRva = 0x41FFBC0;        // System.Enum.ToString(), every enum's
constexpr uintptr_t kGhostTypeClassRva = 0x644D320;  // GhostType's class (an obfuscated name) once the summary has run
constexpr size_t kBoxedValue = 0x10;                 // a boxed enum's value, after the object header

struct Api {
  il2cpp::Object* menuType = nullptr;
  void* (*findObjectOfType)(il2cpp::Object*, bool, const void*) = nullptr;  // (Type, includeInactive)
  void (*forceSync)(void*, void*, const void*) = nullptr;  // DifficultyUIManager.ForceSyncDifficulty: RPC to everyone
  void (*buttonCheck)(void*, const void*) = nullptr;       // DifficultyOption.ButtonCheck: arrows for the count
  bool ok = false;
} api;

using Choice_t = void (*)(void*, const void*);
using Save_t = void (*)(il2cpp::String*, void*, const void*);
using Serialize_t = il2cpp::Array* (*)(void*, void*, const void*);
using Summary_t = il2cpp::String* (*)(void*, void*, const void*);
using EnumName_t = il2cpp::String* (*)(void*, const void*);
Choice_t g_choice = nullptr;
Save_t g_save = nullptr;
Serialize_t g_serialize = nullptr;
Summary_t g_summary = nullptr;
EnumName_t g_enumName = nullptr;
bool g_summarising = false;  // a summary of a custom ghost's difficulty is being written (main thread)

Picks g_picks;
std::wstring g_picksFile;
bool (*g_everyone)() = nullptr;
il2cpp::Held g_menu;       // the lobby's DifficultyUIManager
il2cpp::Held g_offeredTo;  // the menu whose option offers the custom ghosts
ULONGLONG g_nextSearch = 0;

bool everyone() { return g_everyone && g_everyone(); }
bool is_difficulty(void* object) { return il2cpp::is_class(object, "Difficulty"); }
int ghost_type(void* d) { return il2cpp::field<int>(d, kDifficultyGhostType); }
void set_ghost_type(void* d, int type) { il2cpp::set_field<int>(d, kDifficultyGhostType, type); }
int preset_of(void* d) { return il2cpp::field<int>(d, kDifficultyPreset); }

/** `send` runs with `d` carrying `type` for its ghost; its own is back afterwards, thrown or not. */
template <class Send>
auto carrying(void* d, int type, Send send) {
  struct Restore {
    void* d;
    int own;
    ~Restore() { set_ghost_type(d, own); }
  } restore{d, ghost_type(d)};
  set_ghost_type(d, type);
  return send();
}

bool save_picks() {
  std::ofstream out(g_picksFile, std::ios::binary | std::ios::trunc);
  out << g_picks.text();
  out.close();
  return !out.fail();
}

void detour_choice(void* menu, const void* method) {
  hooks::Guard guard;
  g_choice(menu, method);
  void* d = menu ? il2cpp::field<void*>(menu, kMenuCurrent) : nullptr;
  if (!d || !g_picks.choose(preset_of(d), ghost_type(d))) return;
  PG_LOG("difficulty: Custom %d now forces %s", preset_of(d) - 5, is_custom(ghost_type(d)) ? find(ghost_type(d))->name : "no custom ghost");
  if (!save_picks()) PG_LOG("difficulty: could not keep the picks in %s; they last until the game closes", text::narrow(g_picksFile).c_str());
}

void detour_save(il2cpp::String* key, void* value, const void* method) {
  hooks::Guard guard;
  if (!is_difficulty(value) || !is_custom(ghost_type(value))) return g_save(key, value, method);
  carrying(value, outbound_type(ghost_type(value), true, everyone()), [&] { g_save(key, value, method); });
}

il2cpp::Array* detour_serialize(void* value, void* settings, const void* method) {
  hooks::Guard guard;
  if (!is_difficulty(value) || !is_custom(ghost_type(value))) return g_serialize(value, settings, method);
  return carrying(value, outbound_type(ghost_type(value), false, everyone()), [&] { return g_serialize(value, settings, method); });
}

il2cpp::String* detour_summary(void* menu, void* d, const void* method) {
  hooks::Guard guard;
  if (!d || !is_custom(ghost_type(d))) return g_summary(menu, d, method);
  struct Summarising {
    Summarising() { g_summarising = true; }
    ~Summarising() { g_summarising = false; }
  } summarising;
  return g_summary(menu, d, method);
}

// Only inside the summary: the journal builds a ghost's name key from the same ToString, and finds a
// custom ghost's name by its number ("Journal_38Title").
il2cpp::String* detour_enum_name(void* value, const void* method) {
  hooks::Guard guard;
  if (g_summarising && value &&
      il2cpp::class_of(value) == *static_cast<void**>(game::game_assembly_address(kGhostTypeClassRva)))
    if (const Ghost* g = find(il2cpp::field<int>(value, kBoxedValue))) return il2cpp::string_new(g->name);
  return g_enumName(value, method);
}

void* find_menu() {
  if (game::alive(g_menu)) return g_menu;
  const ULONGLONG now = GetTickCount64();  // FindObjectOfType searches the whole scene: once a second
  if (now < g_nextSearch) return nullptr;
  g_nextSearch = now + 1000;
  // Inactive ones too: the game hides the Difficulty Selection board, menu and all, unless the
  // player is on it, and a contract started without opening it must still get the pick.
  g_menu = api.findObjectOfType(api.menuType, true, nullptr);
  return game::alive(g_menu) ? static_cast<void*>(g_menu) : nullptr;
}

il2cpp::List* menu_ghosts(void* menu) {
  void* collection = il2cpp::field<void*>(menu, kMenuGhosts);
  return collection ? il2cpp::field<il2cpp::List*>(collection, kCollectionGhosts) : nullptr;
}

/** The option's choices: Random and every ghost in the menu's collection. */
void fit_option(void* menu) {
  void* option = il2cpp::field<void*>(menu, kMenuGhostOption);
  il2cpp::List* ghosts = menu_ghosts(menu);
  if (!option || !ghosts) return;
  const int count = ghost_choices(ghosts->size);
  if (il2cpp::field<int>(option, kOptionCount) == count) return;
  il2cpp::set_field<int>(option, kOptionCount, count);
  il2cpp::set_field<int>(option, kOptionIndex, ghost_index(il2cpp::field<int>(option, kOptionIndex), count));
  api.buttonCheck(option, nullptr);
}

/** Every difficulty the lobby holds: the presets, its own, its settled copy and the contract's. */
std::vector<void*> lobby_difficulties(void* menu) {
  std::vector<void*> out;
  auto* presets = il2cpp::field<il2cpp::Array*>(menu, kMenuPresets);
  for (uintptr_t i = 0; presets && i < presets->max_length; ++i) out.push_back(presets->data<void*>()[i]);
  out.push_back(il2cpp::field<void*>(menu, kMenuCurrent));
  out.push_back(il2cpp::field<void*>(menu, kMenuApplied));
  void* values = game::level_values();
  out.push_back(values ? il2cpp::field<void*>(values, kLevelValuesDifficulty) : nullptr);
  return out;
}

/** Set every custom pick in the lobby back to Random; whether the lobby's own difficulty changed. */
bool set_aside(void* menu) {
  void* current = il2cpp::field<void*>(menu, kMenuCurrent);
  bool changed = false;
  for (void* d : lobby_difficulties(menu)) {
    if (!d || !is_custom(ghost_type(d))) continue;
    set_ghost_type(d, kNoGhostType);
    changed |= d == current;
  }
  return changed;
}

/** A Random difficulty takes its preset's pick; whether it did. */
bool take_pick(void* d) {
  if (!d || ghost_type(d) != kNoGhostType) return false;
  const int pick = g_picks.of(preset_of(d));
  if (!is_custom(pick)) return false;
  set_ghost_type(d, pick);
  return true;
}

/** Each custom preset the save (which only ever holds Random) or set_aside left Random takes its
 * pick: the game writes Random nowhere else but the option's own Random, which forgets the pick.
 * The host's lobby difficulty too, which every sync makes anew; a client's is the host's, which the
 * host sends. Whether the lobby's own difficulty changed. */
bool give_back(void* menu) {
  auto* presets = il2cpp::field<il2cpp::Array*>(menu, kMenuPresets);
  for (uintptr_t i = 0; presets && i < presets->max_length; ++i) take_pick(presets->data<void*>()[i]);
  void* current = il2cpp::field<void*>(menu, kMenuCurrent);
  if (!game::is_master() || !take_pick(current)) return false;
  PG_LOG("difficulty: the lobby's Custom %d gets its pick back: %s", preset_of(current) - 5, find(ghost_type(current))->name);
  return true;
}

void send(void* menu, bool changed) {
  if (changed && game::is_master()) api.forceSync(menu, il2cpp::field<void*>(menu, kMenuCurrent), nullptr);
}

/** The option's index for a ghost type: Random is 0, the collection's ghost i is i + 1; -1 when the
 * collection lacks it. */
int choice_of(il2cpp::List* ghosts, int type) {
  if (type == kNoGhostType) return 0;
  for (int i = 0; ghosts->items && i < ghosts->size; ++i) {
    void* data = ghosts->items->data<void*>()[i];
    if (data && il2cpp::field<int>(data, kDataType) == type) return i + 1;
  }
  return -1;
}

/** The option shows the lobby's ghost. The game writes its index only when a difficulty is loaded or
 * synced, and Random when the collection lacks the ghost: a sync that came before this client offered
 * the custom ghosts, or the lobby after a contract took them away. Offering or withdrawing them moves
 * every ghost after their places. The game's own handler then names it, writing back the same ghost. */
void show_pick(void* menu) {
  void* d = il2cpp::field<void*>(menu, kMenuCurrent);
  void* option = il2cpp::field<void*>(menu, kMenuGhostOption);
  il2cpp::List* ghosts = menu_ghosts(menu);
  if (!d || !option || !ghosts) return;
  const int index = choice_of(ghosts, ghost_type(d));
  if (index < 0 || il2cpp::field<int>(option, kOptionIndex) == index) return;
  il2cpp::set_field<int>(option, kOptionIndex, index);
  g_choice(menu, nullptr);
  api.buttonCheck(option, nullptr);
}

/** The menu no longer offers the custom ghosts, and nothing in the lobby forces one. */
void withdraw(void* menu) {
  const bool changed = set_aside(menu);
  registry::withdraw(il2cpp::field<void*>(menu, kMenuGhosts));
  fit_option(menu);
  send(menu, changed);
  show_pick(menu);
  g_offeredTo = nullptr;
  PG_LOG("difficulty: the Ghost Type option offers the game's ghosts only");
}

// A contract the custom ghosts stay out of must not force one: its spawn would make a ghost the game
// has no data for.
void keep_contract_vanilla() {
  if (!registry::decided() || registry::active()) return;
  void* values = game::level_values();
  void* d = values ? il2cpp::field<void*>(values, kLevelValuesDifficulty) : nullptr;
  if (!d || !is_custom(ghost_type(d))) return;
  set_ghost_type(d, kNoGhostType);
  PG_LOG("difficulty: not every player runs the custom ghosts; the contract's ghost is rolled instead");
}

}  // namespace

bool resolve() {
  il2cpp::Class* menu = il2cpp::cls("Assembly-CSharp.dll", "", "DifficultyUIManager");
  il2cpp::Class* option = il2cpp::cls("Assembly-CSharp.dll", "", "DifficultyOption");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  api.menuType = menu ? il2cpp::type_object(menu) : nullptr;
  bool ok = api.menuType != nullptr;
  ok &= il2cpp::bind(api.findObjectOfType, object, "FindObjectOfType", 2);
  ok &= il2cpp::bind(api.forceSync, menu, "ForceSyncDifficulty", 1);
  ok &= il2cpp::bind(api.buttonCheck, option, "ButtonCheck", 0);
  api.ok = ok;
  PG_LOG("difficulty: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks(const wchar_t* picksFile, bool (*everyoneRuns)()) {
  g_picksFile = picksFile;
  g_everyone = everyoneRuns;
  g_picks = Picks::parse(text::read_file(g_picksFile));
  return hooks::create(game::game_assembly_address(kGhostChoiceRva), reinterpret_cast<void*>(&detour_choice),
                       reinterpret_cast<void**>(&g_choice), "DifficultyUIManager ghost choice") &&
         hooks::create(game::game_assembly_address(kSaveRva), reinterpret_cast<void*>(&detour_save),
                       reinterpret_cast<void**>(&g_save), "ES3.Save<T>") &&
         hooks::create(game::game_assembly_address(kSerializeRva), reinterpret_cast<void*>(&detour_serialize),
                       reinterpret_cast<void**>(&g_serialize), "ES3.Serialize<T>") &&
         hooks::create(game::game_assembly_address(kSummaryRva), reinterpret_cast<void*>(&detour_summary),
                       reinterpret_cast<void**>(&g_summary), "DifficultyUIManager summary") &&
         hooks::create(game::game_assembly_address(kEnumNameRva), reinterpret_cast<void*>(&detour_enum_name),
                       reinterpret_cast<void**>(&g_enumName), "Enum.ToString");
}

void tick() {
  if (!api.ok) return;
  void* level = game::level_controller();
  if (level && game::alive(level)) {
    keep_contract_vanilla();
    return;
  }
  void* menu = find_menu();
  if (!menu) return;
  if (!everyone()) {
    // Only a menu that was offered them has anything to take back (a public lobby is left alone).
    if (g_offeredTo == menu) withdraw(menu);
    return;
  }
  // Offered every frame: a contract that shared the menu's collection took its entries away.
  registry::offer(il2cpp::field<void*>(menu, kMenuGhosts));
  fit_option(menu);
  if (g_offeredTo != menu) PG_LOG("difficulty: the Ghost Type option offers the custom ghosts");
  g_offeredTo = menu;
  send(menu, give_back(menu));
  show_pick(menu);
}

void teardown() {
  if (api.ok && game::alive(g_menu) && g_offeredTo == g_menu) withdraw(g_menu);
  g_menu = nullptr;
  g_offeredTo = nullptr;
  g_nextSearch = 0;
}

}  // namespace pg::custom::difficulty
