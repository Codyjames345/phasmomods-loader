// Offline tests for the custom difficulty's custom ghosts (mods/custom_ghosts/difficulty.cpp): the
// game's save, lobby sync and Ghost Type option run through the module's detours, against fakes that
// do with each field what the game's code does (REA, build 25811160).
// Run: build\difficulty_test.exe
#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "check.h"
#include "game.h"
#include "ghosts.h"
#include "hooks.h"
#include "il2cpp.h"
#include "registry.h"
#include "text_files.h"

namespace {

// A managed object: fields at their game offsets; its class is kept beside it.
struct Obj {
  alignas(8) unsigned char fields[0x300]{};
};
std::vector<std::unique_ptr<Obj>> objects;
std::map<const void*, std::string> classes;
void* make(const char* klass) {
  objects.push_back(std::make_unique<Obj>());
  classes[objects.back().get()] = klass;
  return objects.back().get();
}
int type_of(void* difficulty) { return il2cpp::field<int>(difficulty, 0x7C); }
void set_type(void* difficulty, int type) { il2cpp::set_field<int>(difficulty, 0x7C, type); }
void* difficulty(int preset, int type) {
  void* d = make("Difficulty");
  il2cpp::set_field<int>(d, 0x18, preset);
  set_type(d, type);
  return d;
}

// The lobby's custom difficulty menu (DifficultyUIManager), its Ghost Type option and its
// GhostCollection, whose List<GhostData> holds the game's 30 ghosts, then the custom ghosts once
// offered: they take their alphabetical places, which moves the game's own (here all of them).
void *fixtureMenu, *fixtureOption, *fixtureCollection, *ghostList, *fixturePresets, *levelValues;
il2cpp::Array *gameItems, *offeredItems;
std::map<std::string, void*> detours;
bool roomRunsMod = true, master = true, inContract = false, contractDecided = false, registryActive = false;
bool menuInScene = true;
bool boardShown = true;  // the game hides the Difficulty Selection board unless the player is on it
int savedType = -1;      // the ghost type the save file got
int sentType = -1;       // the ghost type the lobby sync carried
std::vector<int> forceSyncs;  // the ghost type each re-send carried
int buttonChecks = 0, handlerRuns = 0;
std::vector<std::string> logs;

void* current() { return il2cpp::field<void*>(fixtureMenu, 0x2B8); }
il2cpp::List* list() { return static_cast<il2cpp::List*>(ghostList); }
int option_count() { return il2cpp::field<int>(fixtureOption, 0x70); }
int option_index() { return il2cpp::field<int>(fixtureOption, 0x6C); }
int ghost_at(int i) { return il2cpp::field<int>(list()->items->data<void*>()[i], 0x18); }
/** The option's index for a ghost type: Random is 0, the collection's ghost i is i + 1. */
int choice_of(int type) {
  for (int i = 0; type != 24 && i < list()->size; ++i)
    if (ghost_at(i) == type) return i + 1;
  return 0;
}

// The game's code behind each hooked function.
void native_choice(void*, const void*) {  // the option's change handler: its index names the ghost
  ++handlerRuns;
  set_type(current(), option_index() == 0 ? 24 : ghost_at(option_index() - 1));
}
void native_save(il2cpp::String*, void* value, const void*) { savedType = type_of(value); }
il2cpp::Array* native_serialize(void* value, void*, const void*) {
  sentType = type_of(value);
  return nullptr;
}
void force_sync(void*, void* d, const void*);
void button_check(void*, const void*) { ++buttonChecks; }
// Object.FindObjectOfType(Type) finds only active objects; (Type, includeInactive) hidden ones too.
void* find_object(il2cpp::Object*, const void*) { return menuInScene && boardShown ? fixtureMenu : nullptr; }
void* find_any_object(il2cpp::Object*, bool includeInactive, const void*) {
  return menuInScene && (boardShown || includeInactive) ? fixtureMenu : nullptr;
}

// The managed strings the fakes made, by address.
std::map<const void*, std::string> strings;
il2cpp::String* text(const std::string& s) {
  void* o = make("String");
  strings[o] = s;
  return static_cast<il2cpp::String*>(o);
}
// GhostType's class, as the summary's metadata slot holds it once the summary has run, and another enum's.
char ghostTypeClass, otherEnumClass;
void* ghostTypeSlot = &ghostTypeClass;
struct Boxed {  // an enum value as the summary boxes it for ToString
  void* klass;
  void* monitor;
  int value;
};
/** System.Enum.ToString: a value's name, its number when it has none (no custom ghost has one). */
il2cpp::String* native_enum_name(void* value, const void*) {
  const auto* box = static_cast<const Boxed*>(value);
  return text(box->klass == &ghostTypeClass && box->value == 3 ? "Poltergeist" : std::to_string(box->value));
}
std::string enum_name(void* klass, int value) {
  using EnumName_t = il2cpp::String* (*)(void*, const void*);
  Boxed box{klass, nullptr, value};
  return strings[reinterpret_cast<EnumName_t>(detours.at("Enum.ToString"))(&box, nullptr)];
}
std::string otherEnumInSummary;  // another enum's name the summary asked for on the way
/** DifficultyUIManager's summary: its Ghost line names the difficulty's ghost by GhostType.ToString(). */
il2cpp::String* native_summary(void*, void* d, const void*) {
  otherEnumInSummary = enum_name(&otherEnumClass, type_of(d));
  return text("Ghost: " + enum_name(&ghostTypeClass, type_of(d)));
}

std::wstring picks_file() {
  wchar_t dir[MAX_PATH];
  GetTempPathW(MAX_PATH, dir);
  return std::wstring(dir) + L"pg_difficulty_test_picks.txt";
}

}  // namespace

#include "../mods/custom_ghosts/difficulty.cpp"

namespace pg::hooks {
std::atomic<int> g_inflight{0};
bool create(void*, void* detour, void** original, const char* name) {
  detours[name] = detour;
  const std::string n = name;
  if (n == "DifficultyUIManager ghost choice") *original = reinterpret_cast<void*>(&native_choice);
  else if (n == "ES3.Save<T>") *original = reinterpret_cast<void*>(&native_save);
  else if (n == "ES3.Serialize<T>") *original = reinterpret_cast<void*>(&native_serialize);
  else if (n == "DifficultyUIManager summary") *original = reinterpret_cast<void*>(&native_summary);
  else if (n == "Enum.ToString") *original = reinterpret_cast<void*>(&native_enum_name);
  else return false;
  return true;
}
}  // namespace pg::hooks
namespace pg::custom::registry {
void offer(void* c) {
  if (c != fixtureCollection) return;
  list()->items = offeredItems;
  list()->size = 30 + kGhostCount;
}
void withdraw(void* c) {
  if (c != fixtureCollection) return;
  list()->items = gameItems;
  list()->size = 30;
}
bool decided() { return contractDecided; }
bool active() { return registryActive; }
}  // namespace pg::custom::registry
namespace pg::game {
void* game_assembly_address(uintptr_t rva) { return rva == 0x644D320 ? &ghostTypeSlot : reinterpret_cast<void*>(rva); }
void* level_controller() { return inContract ? levelValues : nullptr; }
bool alive(void* object) { return object != nullptr; }
bool is_master() { return master; }
void* level_values() { return levelValues; }
}  // namespace pg::game
namespace il2cpp {
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
Object* type_object(Class* klass) { return reinterpret_cast<Object*>(klass); }
void* method_ptr(Class*, const char* name, int argc) {
  if (std::strcmp(name, "FindObjectOfType") == 0)
    return argc == 2 ? reinterpret_cast<void*>(&find_any_object) : reinterpret_cast<void*>(&find_object);
  if (std::strcmp(name, "ForceSyncDifficulty") == 0) return reinterpret_cast<void*>(&force_sync);
  if (std::strcmp(name, "ButtonCheck") == 0) return reinterpret_cast<void*>(&button_check);
  return nullptr;
}
bool is_class(const void* obj, const char* name) {
  auto it = classes.find(obj);
  return it != classes.end() && it->second == name;
}
Class* class_of(const void* obj) { return static_cast<const Boxed*>(obj)->klass; }
String* string_new(const char* utf8) { return text(utf8); }
uint32_t gc_hold(void*) { return 1; }
void gc_release(uint32_t) {}
}  // namespace il2cpp
namespace pg::log {
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  logs.emplace_back(line);
}
}  // namespace pg::log

namespace {

using Save_t = void (*)(il2cpp::String*, void*, const void*);
using Serialize_t = il2cpp::Array* (*)(void*, void*, const void*);
using Choice_t = void (*)(void*, const void*);
void save(void* d) { reinterpret_cast<Save_t>(detours.at("ES3.Save<T>"))(nullptr, d, nullptr); }
void serialize(void* d) { reinterpret_cast<Serialize_t>(detours.at("ES3.Serialize<T>"))(d, nullptr, nullptr); }
/** The player's arrows or dice land the option on `type`. */
void choose(int type) {
  il2cpp::set_field<int>(fixtureOption, 0x6C, choice_of(type));
  reinterpret_cast<Choice_t>(detours.at("DifficultyUIManager ghost choice"))(fixtureMenu, nullptr);
}
/** The lobby's difficulty as SyncDifficultyValues leaves it: deserialised anew, its settled copy and
 * the contract's a clone of that. */
void synced(int preset, int type) {
  il2cpp::set_field<void*>(fixtureMenu, 0x2B8, difficulty(preset, type));
  il2cpp::set_field<void*>(fixtureMenu, 0x2C0, difficulty(preset, type));
  il2cpp::set_field<void*>(levelValues, 0x38, il2cpp::field<void*>(fixtureMenu, 0x2C0));
}
// ForceSyncDifficulty: ES3.Serialize, then an RPC to everyone that this client runs at once.
void force_sync(void*, void* d, const void*) {
  serialize(d);
  forceSyncs.push_back(sentType);
  synced(il2cpp::field<int>(d, 0x18), sentType);
}

/** A lobby whose menu shows Custom 2 (preset 7) and holds the three custom presets. */
void reset_lobby() {
  pg::custom::difficulty::teardown();  // the previous case's objects are still there to tidy
  objects.clear();
  classes.clear();
  logs.clear();
  roomRunsMod = master = menuInScene = boardShown = true;
  inContract = contractDecided = registryActive = false;
  forceSyncs.clear();
  buttonChecks = handlerRuns = 0;
  savedType = sentType = -1;
  fixtureMenu = make("DifficultyUIManager");
  fixtureOption = make("DifficultyOption");
  fixtureCollection = make("GhostCollection");
  ghostList = make("List");
  fixturePresets = make("Difficulty[]");
  levelValues = make("LevelValues");
  // The game's ghosts (every GhostType but None); offered, the custom ghosts come before them all.
  gameItems = static_cast<il2cpp::Array*>(make("GhostData[]"));
  offeredItems = static_cast<il2cpp::Array*>(make("GhostData[]"));
  gameItems->max_length = 30;
  offeredItems->max_length = 30 + pg::custom::kGhostCount;
  for (int i = 0; i < 30 + pg::custom::kGhostCount; ++i) {
    void* data = make("GhostData");
    const int game = i - pg::custom::kGhostCount;
    il2cpp::set_field<int>(data, 0x18, game < 0 ? pg::custom::kGhosts[i].type : game < 24 ? game : game + 1);
    offeredItems->data<void*>()[i] = data;
    if (game >= 0) gameItems->data<void*>()[game] = data;
  }
  list()->items = gameItems;
  list()->size = 30;
  il2cpp::set_field<int>(fixtureOption, 0x70, 31);
  il2cpp::set_field<void*>(fixtureCollection, 0x18, ghostList);
  il2cpp::set_field<void*>(fixtureMenu, 0xC8, fixtureOption);
  il2cpp::set_field<void*>(fixtureMenu, 0x2C8, fixtureCollection);
  auto* array = static_cast<il2cpp::Array*>(fixturePresets);
  array->max_length = 3;
  for (int i = 0; i < 3; ++i) array->data<void*>()[i] = difficulty(6 + i, 24);
  il2cpp::set_field<void*>(fixtureMenu, 0x2A8, fixturePresets);
  synced(7, 24);  // as the save file holds it: no custom ghost
}
void* preset(int i) { return static_cast<il2cpp::Array*>(fixturePresets)->data<void*>()[i]; }
bool logged(const char* text) {
  for (const std::string& line : logs)
    if (line.find(text) != std::string::npos) return true;
  return false;
}

void start(const char* picks) {
  std::filesystem::remove(picks_file());
  if (picks) std::ofstream(picks_file(), std::ios::binary) << picks;
  CHECK(pg::custom::difficulty::resolve());
  CHECK(pg::custom::difficulty::create_hooks(picks_file().c_str(), [] { return roomRunsMod; }));
}

void the_save_file_never_holds_a_custom_ghost() {
  reset_lobby();
  start(nullptr);
  void* d = difficulty(7, pg::custom::kBlackShuck);
  save(d);
  CHECK(savedType == 24);                          // the save file got Random
  CHECK(type_of(d) == pg::custom::kBlackShuck);    // and the game keeps its pick
  set_type(d, 5);
  save(d);
  CHECK(savedType == 5);                           // the game's own ghosts are saved as they are
  void* other = make("Loadout");                   // ES3's code is shared by every saved class
  set_type(other, pg::custom::kBlackShuck);
  save(other);
  CHECK(savedType == pg::custom::kBlackShuck);
}

void the_lobby_sync_carries_it_only_to_everyone() {
  reset_lobby();
  start(nullptr);
  void* d = difficulty(7, pg::custom::kHydra);
  serialize(d);
  CHECK(sentType == pg::custom::kHydra);
  roomRunsMod = false;  // a player without the custom ghosts has just joined
  serialize(d);
  CHECK(sentType == 24);
  CHECK(type_of(d) == pg::custom::kHydra);
}

void a_pick_is_remembered_for_its_preset() {
  reset_lobby();
  start(nullptr);
  pg::custom::difficulty::tick();  // the option offers the custom ghosts
  choose(pg::custom::kEcho);
  CHECK(type_of(current()) == pg::custom::kEcho);
  CHECK(pg::text::read_file(picks_file()) == "7=31\n");
  choose(3);  // a vanilla ghost replaces it
  CHECK(pg::text::read_file(picks_file()).empty());
}

void a_pick_that_cannot_be_kept_is_reported() {
  reset_lobby();
  const std::wstring unwritable = picks_file() + L".missing\\picks.txt";  // in a folder that does not exist
  CHECK(pg::custom::difficulty::resolve());
  CHECK(pg::custom::difficulty::create_hooks(unwritable.c_str(), [] { return roomRunsMod; }));
  pg::custom::difficulty::tick();
  choose(pg::custom::kEcho);
  CHECK(type_of(current()) == pg::custom::kEcho);  // it holds until the game closes
  CHECK(logged("could not keep"));
}

// The presets were loaded from the save (Random) by the menu's Awake, perhaps before the mod started.
void a_preset_gets_its_pick_back_once_everyone_runs_them() {
  reset_lobby();
  start("6=40\n");
  roomRunsMod = false;
  pg::custom::difficulty::tick();
  CHECK(type_of(preset(0)) == 24);   // not while anyone lacks the custom ghosts
  CHECK(option_count() == 31);       // and the option offers only the game's own
  roomRunsMod = true;
  pg::custom::difficulty::tick();
  CHECK(type_of(preset(0)) == pg::custom::kLethe);
  CHECK(option_count() == 46);
  CHECK(forceSyncs.empty());         // the lobby's own difficulty did not change
}

// A player who starts a contract from the lobby without ever opening the Difficulty Selection board
// (hidden, so the plain FindObjectOfType misses the menu) still plays their preset's pick.
void a_pick_comes_back_with_the_board_hidden() {
  reset_lobby();
  boardShown = false;
  start("7=38\n");
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == pg::custom::kBrownie);
  CHECK(forceSyncs.size() == 1 && forceSyncs.back() == pg::custom::kBrownie);
  CHECK(type_of(il2cpp::field<void*>(levelValues, 0x38)) == pg::custom::kBrownie);  // the contract's
  CHECK(logged("Custom 2 gets its pick back: Brownie"));
}

void the_lobby_difficulty_follows_who_is_in_the_room() {
  reset_lobby();
  start("7=43\n");
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == pg::custom::kAsterion);
  CHECK(forceSyncs.size() == 1 && forceSyncs.back() == pg::custom::kAsterion);  // the host sends it
  CHECK(option_index() == choice_of(pg::custom::kAsterion));
  roomRunsMod = false;                  // someone without the custom ghosts joins
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == 24);
  CHECK(forceSyncs.size() == 2 && forceSyncs.back() == 24);
  CHECK(list()->size == 30 && option_count() == 31 && option_index() == 0 && buttonChecks > 0);
  roomRunsMod = true;                   // they leave again; the sync made the lobby's difficulty anew
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == pg::custom::kAsterion);
  CHECK(forceSyncs.size() == 3 && forceSyncs.back() == pg::custom::kAsterion);
  pg::custom::difficulty::tick();
  CHECK(forceSyncs.size() == 3);        // and it is sent once
}

void a_client_leaves_the_lobby_difficulty_to_the_host() {
  reset_lobby();
  start("7=43\n");
  master = false;
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == 24);
  CHECK(forceSyncs.empty());
  synced(7, pg::custom::kAsterion);  // the host sent its pick
  roomRunsMod = false;
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == 24);  // set aside before the menu's collection loses the custom ghosts
  CHECK(forceSyncs.empty());
}

// The game shows a synced difficulty by the index of its ghost in the menu's collection, Random when
// it is not there: a sync that came before this client offered the custom ghosts, or the lobby after
// a contract took them away.
void the_option_shows_the_lobby_ghost() {
  reset_lobby();
  start(nullptr);
  master = false;
  synced(7, pg::custom::kKnell);
  CHECK(option_index() == 0);
  pg::custom::difficulty::tick();
  CHECK(option_index() == choice_of(pg::custom::kKnell) && option_index() > 0);
  CHECK(handlerRuns == 1);                       // the game's handler shows its name
  CHECK(type_of(current()) == pg::custom::kKnell);
  CHECK(pg::text::read_file(picks_file()).empty());  // which is no pick of this player's
  pg::custom::difficulty::tick();
  CHECK(handlerRuns == 1);
}

// The game's ghosts move in the collection when the custom ghosts are offered or withdrawn, and the
// game writes the option's index only when a difficulty is loaded or synced.
void the_option_keeps_showing_a_game_ghost() {
  reset_lobby();
  start(nullptr);
  synced(7, 3);
  il2cpp::set_field<int>(fixtureOption, 0x6C, choice_of(3));  // as the game showed it
  pg::custom::difficulty::tick();                              // offered
  CHECK(option_index() == choice_of(3) && type_of(current()) == 3);
  roomRunsMod = false;                                         // withdrawn
  pg::custom::difficulty::tick();
  CHECK(option_index() == choice_of(3) && type_of(current()) == 3);
  CHECK(forceSyncs.empty());
  synced(7, 24);                                               // Random, the game's index 0
  il2cpp::set_field<int>(fixtureOption, 0x6C, 0);
  roomRunsMod = true;
  pg::custom::difficulty::tick();
  CHECK(option_index() == 0 && type_of(current()) == 24);
}

// The custom difficulty's summary names its ghost by GhostType.ToString(), which has no name for a
// custom ghost.
void the_summary_names_the_ghost() {
  reset_lobby();
  start(nullptr);
  using Summary_t = il2cpp::String* (*)(void*, void*, const void*);
  auto summary = [](int type) {
    return strings[reinterpret_cast<Summary_t>(detours.at("DifficultyUIManager summary"))(fixtureMenu, difficulty(7, type), nullptr)];
  };
  CHECK(summary(pg::custom::kBrownie) == "Ghost: Brownie");
  CHECK(otherEnumInSummary == "38");  // only GhostType's names change
  CHECK(summary(pg::custom::kBlackShuck) == "Ghost: Black Shuck");
  CHECK(summary(3) == "Ghost: Poltergeist");
  // Outside the summary a custom type keeps its number: the journal's name keys are built from it.
  CHECK(enum_name(&ghostTypeClass, pg::custom::kBrownie) == "38");
}

void a_vanilla_contract_never_forces_one() {
  reset_lobby();
  start(nullptr);
  void* contract = il2cpp::field<void*>(levelValues, 0x38);
  set_type(contract, pg::custom::kGremlin);
  inContract = true;
  pg::custom::difficulty::tick();
  CHECK(type_of(contract) == pg::custom::kGremlin);  // not yet decided
  contractDecided = true;
  registryActive = true;
  pg::custom::difficulty::tick();
  CHECK(type_of(contract) == pg::custom::kGremlin);  // the custom ghosts are in this contract
  registryActive = false;
  pg::custom::difficulty::tick();
  CHECK(type_of(contract) == 24);                    // they are not: the game's roll decides
}

void stopping_the_mod_leaves_no_custom_pick() {
  reset_lobby();
  start("6=40\n7=43\n");
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == pg::custom::kAsterion && type_of(preset(0)) == pg::custom::kLethe);
  pg::custom::difficulty::teardown();
  CHECK(type_of(current()) == 24 && type_of(preset(0)) == 24);
  CHECK(list()->size == 30 && option_count() == 31);
  CHECK(forceSyncs.back() == 24);
  CHECK(pg::text::read_file(picks_file()) == "6=40\n7=43\n");  // the picks are kept for next time
  start("6=40\n7=43\n");             // switched on again, in the same lobby
  pg::custom::difficulty::tick();
  CHECK(type_of(current()) == pg::custom::kAsterion && type_of(preset(0)) == pg::custom::kLethe);
}

}  // namespace

int main() {
  the_save_file_never_holds_a_custom_ghost();
  the_lobby_sync_carries_it_only_to_everyone();
  a_pick_is_remembered_for_its_preset();
  a_pick_that_cannot_be_kept_is_reported();
  a_preset_gets_its_pick_back_once_everyone_runs_them();
  a_pick_comes_back_with_the_board_hidden();
  the_lobby_difficulty_follows_who_is_in_the_room();
  a_client_leaves_the_lobby_difficulty_to_the_host();
  the_option_shows_the_lobby_ghost();
  the_option_keeps_showing_a_game_ghost();
  the_summary_names_the_ghost();
  a_vanilla_contract_never_forces_one();
  stopping_the_mod_leaves_no_custom_pick();
  std::filesystem::remove(picks_file());
  std::printf("difficulty: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
