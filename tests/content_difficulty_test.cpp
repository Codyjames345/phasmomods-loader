// The custom difficulty side of content (host/content_difficulty.h) on the fake backend, over a faked
// lobby menu that does with each field what the game's code does (sdk/systems/difficulty.md, build
// 25811160): the Ghost Type option and its change handler, the menu's GhostCollection, the custom
// presets, the lobby's difficulty with its settled copy, LevelValues.difficulty, the lobby sync (a write
// through EasySave3's Difficulty type, then SyncDifficultyValues on this client reading into the live
// object), and a contract's spawn reading LevelValues.difficulty. The cases of tests/difficulty_test.cpp
// on the content layer, and: picks kept by ghost id, the file of the older form, a pick that lasts past
// a contract while the lobby's difficulty is a preset object, and the summary through the host's hooks
// as the game calls them.
// Run: build\content_difficulty_test.exe
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "Difficulty.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostTypes.h"
#include "LevelController.h"
#include "UnityObject.h"
#include "check.h"
#include "content.h"
#include "content_difficulty.h"
#include "content_types.h"
#include "fake_game.h"
#include "game_api.h"
#include "log.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace content = pm::api::content;
namespace fake = pm::fake;
namespace map = pm::map;

constexpr int32_t kRandom = 24;  // GhostType.None in the audited enums

std::vector<std::string> g_lines;
void capture(const char* line) { g_lines.emplace_back(line); }
bool logged(const std::string& text) {
  return std::any_of(g_lines.begin(), g_lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
}

std::wstring picks_file() {
  wchar_t dir[MAX_PATH];
  GetTempPathW(MAX_PATH, dir);
  return std::wstring(dir) + L"pm_content_difficulty_test.txt";
}
std::string file_text(const std::wstring& path) {
  std::ifstream in(path, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// A boxed enum as the summary boxes GhostType on the stack: its class, the monitor, the value.
struct Boxed {
  void* klass;
  void* monitor;
  int32_t value;
};

struct Lobby;
Lobby* g_lobby = nullptr;

// What the game calls for a method a hook is on: the hook's entry, which runs the host's detour and then
// the method (tests/game_api_test.cpp does the same).
using WSummary = std::remove_cvref_t<decltype(phasmo::DifficultyUIManager::SummaryText)>;
using WName = std::remove_cvref_t<decltype(phasmo::Enum::ToString)>;
template <class W>
void* hooked() {
  return api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0);
}

int32_t type_of(void* d) { return phasmo::Difficulty::ghostTypeOverride.get(d); }
void set_type(void* d, int32_t t) { phasmo::Difficulty::ghostTypeOverride.set(d, t); }

// The faked game: the menu scene and the contract, one lobby.
struct Lobby {
  int difficultyClass = 0, menuClass = 0, optionClass = 0, collectionClass = 0, dataClass = 0, ghostTypeClass = 0,
      otherEnumClass = 0, controllerClass = 0, levelClass = 0;
  void* levelInstance = nullptr;       // LevelController.instance: set while in a contract
  void* controllerInstance = nullptr;  // GhostController.instance
  void *menu = nullptr, *option = nullptr, *collection = nullptr, *values = nullptr, *controller = nullptr;
  bool menuInScene = true, boardShown = true;
  std::vector<int32_t> sent;   // the ghost type each lobby sync carried
  int32_t last_sent() const { return sent.empty() ? -1 : sent.back(); }
  int buttonChecks = 0, handlerRuns = 0, searches = 0;
  int32_t spawned = -1;        // the type the last contract's spawn forced, or -1: rolled
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(),
                               {&fake::list_insert, &fake::list_remove_at, &fake::list_add_value}};
  pm::host::ContentDifficulty side{fake::content_registry(), types, &room_verified};
  uint64_t now = 0;
  // The players Photon lists at a send, by actor, when not the room content last judged.
  bool verified = true;  // whether everyone in the room now is, as the host asks as a send is written
  // What the save holds, as EasySave3 references it: the object "LastDifficulty" was written from, and
  // each "CustomDifficulty{i}"'s; and the reference each loaded preset now carries.
  void* lastRef = nullptr;
  int32_t lastPreset = 7;
  void* presetSavedFrom[3] = {};
  void* presetRef[3] = {};

  Lobby() {
    g_lobby = this;
    fake::type(map::Difficulty, &difficultyClass);
    fake::type(map::DifficultyUIManager, &menuClass);
    fake::type(map::DifficultyOption, &optionClass);
    fake::type(map::GhostCollection, &collectionClass);
    fake::type(map::GhostData, &dataClass);
    fake::type(map::GhostType, &ghostTypeClass);
    fake::type(map::GhostController, &controllerClass);
    fake::type(map::LevelController, &levelClass);
    fake::field(map::Difficulty_difficulty, 0x18);
    fake::field(map::Difficulty_ghostTypeOverride, 0x7C);
    fake::field(map::DifficultyOption_index, 0x6C);
    fake::field(map::DifficultyOption_count, 0x70);
    // The menu's members at offsets of the fixture's choosing (a fake object has 0x200 bytes).
    fake::field(map::DifficultyUIManager_ghostType, 0x20);
    fake::field(map::DifficultyUIManager_presets, 0x28);
    fake::field(map::DifficultyUIManager_current, 0x30);
    fake::field(map::DifficultyUIManager_applied, 0x38);
    fake::field(map::DifficultyUIManager_allGhostsData, 0x40);
    fake::field(map::LevelValues_difficulty, 0x38);
    fake::field(map::GhostCollection_ghosts, 0x18);
    fake::field(map::GhostData_type, 0x18);
    fake::field(map::GhostData_ghostNameLocalisationKey, 0x20);
    fake::field(map::GhostData_evidence1, 0x28);
    fake::field(map::GhostData_evidence2, 0x2C);
    fake::field(map::GhostData_evidence3, 0x30);
    fake::field(map::GhostController_allGhostsCollection, 0x90);
    fake::field(map::GhostController_mimicCopies, 0xC8);
    fake::field(map::LevelController_currentGhost, 0x40);
    fake::static_field(map::LevelController_instance, &levelInstance, sizeof levelInstance, true);
    fake::static_field(map::GhostController_instance, &controllerInstance, sizeof controllerInstance, true);
    fake::value(map::GhostType_None, kRandom);
    const auto m = [](const pm::sdk::EntryDesc& d, auto fn) { fake::method(d, reinterpret_cast<void*>(fn)); };
    m(map::DifficultyOption_ButtonCheck, &button_check);
    m(map::DifficultyUIManager_ForceSyncDifficulty, &force_sync);
    m(map::DifficultyUIManager_GhostTypeChosen, &handler);
    m(map::DifficultyUIManager_SummaryText, &summary_text);
    m(map::ES3UserType_Difficulty_WriteScriptableObject, &never_called);
    m(map::SystemEnum_ToString, &to_string);
    m(map::SystemEnum_GetHashCode, &hash_code);
    m(map::LevelValues_get_Instance, &get_instance);
    m(map::UnityObject_FindObjectOfType, &find_object);
    std::string missing;
    api::bind(fake::host(), missing);

    fake::type(map::LevelValues, &levelClass);
    values = phasmo::DifficultyOption::type.make();  // LevelValues: any fake object holds the field
    option = phasmo::DifficultyOption::type.make();
    collection = phasmo::GhostCollection::type.make();
    controller = phasmo::GhostController::type.make();
    phasmo::GhostController::allGhostsCollection.set(controller, collection);
    // The game's ghosts as the menu scene's collection ships them, alphabetical by the names in their keys.
    std::vector<void*> ghosts;
    for (const auto& [type, name] : std::vector<std::pair<int32_t, std::string>>{
             {4, "Banshee"}, {6, "Mare"}, {0, "Spirit"}, {1, "Wraith"}}) {
      void* data = phasmo::GhostData::type.make();
      phasmo::GhostData::ghostType.set(data, type);
      phasmo::GhostData::ghostNameLocalisationKey.set(data, api::make_string("Journal_" + name + "Title"));
      ghosts.push_back(data);
    }
    phasmo::GhostCollection::ghosts.set(collection, fake::list(ghosts));
    phasmo::DifficultyOption::count.set(option, 5);  // as the scene ships it: Random and the game's ghosts
    load_menu();
    phasmo::LevelValues::difficulty.set(values, pm::api::Ref<pm::map::Difficulty>::unchecked(difficulty(7, kRandom)));
  }
  ~Lobby() { g_lobby = nullptr; }

  static void* difficulty(int32_t preset, int32_t type) {
    void* d = phasmo::Difficulty::type.make();
    phasmo::Difficulty::difficulty.set(d, preset);
    set_type(d, type);
    return d;
  }
  // The menu scene loads: a new DifficultyUIManager whose Awake loads the custom presets from the save
  // (Random: no save holds a custom ghost), numbered 6 up. The lobby's difficulty stays null until the
  // board is shown or a sync comes.
  void load_menu() {
    menu = phasmo::DifficultyUIManager::type.make();
    phasmo::DifficultyUIManager::ghostType.set(menu, option);
    phasmo::DifficultyUIManager::allGhostsData.set(menu, collection);
    void* presets = phasmo::Difficulty::type.make_array(3);
    for (size_t i = 0; i < 3; ++i) {
      api::RefArray{presets}.set(i, difficulty(6 + static_cast<int32_t>(i), kRandom));
      presetRef[i] = presetSavedFrom[i];  // registered under the reference it was saved with
    }
    phasmo::DifficultyUIManager::presets.set(menu, presets);
  }
  void* preset(size_t i) const { return api::RefArray{phasmo::DifficultyUIManager::presets.get(menu)}[i]; }
  void* current() const { return phasmo::DifficultyUIManager::current.get(menu); }
  void* contract_difficulty() const { return phasmo::LevelValues::difficulty.get(values); }
  int32_t count() const { return phasmo::DifficultyOption::count.get(option); }
  int32_t index() const { return phasmo::DifficultyOption::index.get(option); }
  std::vector<int32_t> listed() const {
    std::vector<int32_t> out;
    for (void* d : fake::list_items_of(phasmo::GhostCollection::ghosts.get(collection)))
      out.push_back(phasmo::GhostData::ghostType.get(d));
    return out;
  }
  int32_t choice_of(int32_t type) const {
    if (type == kRandom) return 0;
    const std::vector<int32_t> l = listed();
    const auto it = std::find(l.begin(), l.end(), type);
    return it == l.end() ? -1 : static_cast<int32_t>(it - l.begin()) + 1;
  }
  // The preset choice (0x96DE70): the lobby's difficulty is a copy of the preset (Clone(false)); then
  // the display is refreshed, nothing applied.
  void choose_preset(size_t i) {
    phasmo::DifficultyUIManager::current.set(
        menu, pm::api::Ref<pm::map::Difficulty>::unchecked(difficulty(phasmo::Difficulty::difficulty.get(preset(i)),
                                                                      type_of(preset(i)))));
  }
  // The custom preset's save button (0x9718D0): ES3.Save("CustomDifficulty{i}", current), then that very
  // object is the preset.
  void save_preset(size_t i) {
    write(current());
    presetSavedFrom[i] = presetRef[i] = current();
    api::RefArray{phasmo::DifficultyUIManager::presets.get(menu)}.set(i, current());
  }
  // ApplyDifficulty: saved, then synced to every player through the game's own write.
  void apply() {
    write(current());  // ES3.Save("LastDifficulty")
    lastRef = current();
    lastPreset = phasmo::Difficulty::difficulty.get(current());
    sync(current());
  }
  // A write of `d` through EasySave3's Difficulty type, as the host's hook wraps it.
  int32_t write(void* d) {
    const int32_t put = side.before_write(d);
    const int32_t written = type_of(d);
    if (put >= 0) side.after_write(d, put);
    return written;
  }
  // The lobby sync: written, then SyncDifficultyValues on this client: the bytes are read into the live
  // object the saved reference names (this client's own `d`), its settled copy (Clone(true)) becomes the
  // applied one and LevelValues.difficulty.
  void sync(void* d) {
    sent.push_back(write(d));
    set_type(d, sent.back());
    phasmo::DifficultyUIManager::current.set(menu, pm::api::Ref<pm::map::Difficulty>::unchecked(d));
    void* copy = difficulty(phasmo::Difficulty::difficulty.get(d), type_of(d));
    phasmo::DifficultyUIManager::applied.set(menu, pm::api::Ref<pm::map::Difficulty>::unchecked(copy));
    phasmo::LevelValues::difficulty.set(values, pm::api::Ref<pm::map::Difficulty>::unchecked(copy));
  }
  // The board's one-shot visible handler, the master in a room (0x969670): the lobby's difficulty is
  // ES3.Load("LastDifficulty"), read into the live object its reference names (a preset Awake loaded
  // under the same reference) or else a new one, with what the save holds: Random. Then applied.
  void show_board() {
    void* d = nullptr;
    for (size_t i = 0; i < 3 && !d; ++i)
      if (lastRef && presetRef[i] == lastRef) d = preset(i);
    if (!d) d = difficulty(lastPreset, kRandom);
    set_type(d, kRandom);
    phasmo::DifficultyUIManager::current.set(menu, pm::api::Ref<pm::map::Difficulty>::unchecked(d));
    apply();
  }
  // The master's join sender (0x968ED0): LevelValues.difficulty written for the player who joined.
  int32_t send_to_joiner() { return write(contract_difficulty()); }
  // The summary's builder (0x95E010) as its callers reach it, through whatever is hooked on it: the
  // summary panel's toggle (0x975900) when it shows the panel, and the display's refresh (0x973580
  // through 0x96D880), each on the lobby's difficulty.
  std::string summary() {
    using Builder = void* (*)(void*, void*, const void*);
    return api::to_utf8(reinterpret_cast<Builder>(hooked<WSummary>())(menu, current(), nullptr));
  }
  std::string toggle_summary() { return summary(); }
  std::string refresh_display() { return summary(); }
  // The player's arrows land the option on `type`: the handler runs, then the host's hook after it.
  void choose(int32_t type) {
    phasmo::DifficultyOption::index.set(option, choice_of(type));
    handler(menu, nullptr);
    side.after_choice(menu);
  }
  // A frame as the host runs it (host/suite.cpp's tick_content): the ghost types, which start and end
  // the lobby's offer, then the menu.
  void tick(pm::content::State state, bool master = true) {
    types.tick(state, true, master);
    side.tick(state, true, master, now);
    now += 16;
  }
  void tick() { tick(fake::content_state()); }
  // A contract loads (a new scene: the menu goes), spawns its ghost, and ends.
  void contract() {
    now += 2000;                 // a contract lasts a while
    levelInstance = controller;  // any alive object: a level is loaded
    controllerInstance = controller;
    for (int i = 0; i < 3; ++i) {
      types.tick(fake::content_state(), true, true);
      side.tick(fake::content_state(), true, true, now);
    }
    // The spawn (0x22B4050): the rolled type replaced by LevelValues.difficulty's when not None.
    const int32_t forced = type_of(contract_difficulty());
    spawned = forced == kRandom ? -1 : forced;
    levelInstance = nullptr;
    controllerInstance = nullptr;
    fake::destroy(menu);
    types.tick(fake::content_state(), true, true);
    load_menu();
  }

  // The game's code behind each faked method.
  static void button_check(void*, const void*) { ++g_lobby->buttonChecks; }
  static void force_sync(void*, void* d, const void*) { g_lobby->sync(d); }
  // The option's change handler (0x96DD20): index 0 is None, index i the collection's ghost i - 1.
  static void handler(void* menu, const void*) {
    Lobby& l = *g_lobby;
    ++l.handlerRuns;
    const int32_t i = phasmo::DifficultyOption::index.get(l.option);
    void* d = phasmo::DifficultyUIManager::current.get(menu);
    const std::vector<int32_t> listed = l.listed();
    set_type(d, i == 0 ? kRandom : listed.at(static_cast<size_t>(i - 1)));
  }
  static void* get_instance(const void*) { return g_lobby->values; }
  // The builder: its Ghost line is Enum.ToString of the difficulty's type, boxed on the stack, called
  // as the game calls it, through whatever is hooked on it.
  static void* summary_text(void*, void* d, const void*) {
    Boxed box{&g_lobby->ghostTypeClass, nullptr, type_of(d)};
    using Name = void* (*)(void*, const void*);
    return api::make_string("Ghost: " + api::to_utf8(reinterpret_cast<Name>(hooked<WName>())(&box, nullptr)));
  }
  // The game's Enum.ToString for a value with no name of the enum's: its number.
  static void* to_string(void* boxed, const void*) {
    return api::make_string(std::to_string(static_cast<Boxed*>(boxed)->value));
  }
  static bool room_verified() { return g_lobby->verified; }
  // Object.FindObjectOfType(Type, includeInactive): the board is inactive unless the player is on it.
  static void* find_object(void*, bool includeInactive, const void*) {
    ++g_lobby->searches;
    return g_lobby->menuInScene && (g_lobby->boardShown || includeInactive) ? g_lobby->menu : nullptr;
  }
  static int32_t hash_code(void* boxed, const void*) { return static_cast<Boxed*>(boxed)->value; }
  static void never_called() {}
};

content::GhostDef ghost(const std::string& id, const std::string& name) {
  content::GhostDef g;
  g.id = id;
  g.name = content::Text("en", name);
  g.description = content::Text("en", name + " haunts.");
  g.strength = content::Text("en", "Strong.");
  g.weakness = content::Text("en", "Weak.");
  g.evidence = {"emf", "orbs", "freezing"};
  return g;
}

// A fresh fake game with `ghosts` registered and committed, a picks file holding `picks` (none when
// null), and this client alone in a private room: content active.
void start(const std::vector<content::GhostDef>& ghosts, const char* picks) {
  fake::reset();
  g_lines.clear();
  pm::log::set_sink(&capture, "");
  std::string missing;
  api::bind(fake::host(), missing);  // the content functions go through the host contract
  for (const content::GhostDef& g : ghosts) CHECK(content::register_ghost(g) > 0);
  fake::commit_content();
  fake::room(1, {1});
  std::filesystem::remove(picks_file());
  if (picks) std::ofstream(picks_file(), std::ios::binary) << picks;
}
std::vector<content::GhostDef> some() {
  return {ghost("a.asterion", "Asterion"), ghost("a.echo", "Echo"), ghost("a.lethe", "Lethe")};
}
// Numbers in byte order of the ids: a.asterion 31, a.echo 32, a.lethe 33.
constexpr int32_t kAsterion = 31, kEcho = 32, kLethe = 33;
// Another player joins without the content: it turns off.
void stranger_joins() {
  fake::room(1, {1, 2}, 100000, 2);
}
void stranger_leaves() { fake::room(1, {1}, 200000); }

void save_never_holds_custom_type() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  l.tick();
  CHECK(fake::content_state() == pm::content::State::Active);
  void* d = Lobby::difficulty(7, kEcho);
  CHECK(l.write(d) == kRandom);  // the save got Random, even with content active
  CHECK(type_of(d) == kEcho);    // and the game keeps its pick
  set_type(d, 5);
  CHECK(l.write(d) == 5);        // the game's own ghosts are written as they are
  // The game's own sync (ApplyDifficulty) carries Random too: only the host's send carries one.
  l.choose_preset(1);
  set_type(l.current(), kEcho);
  l.apply();
  CHECK(l.last_sent() == kRandom);
}

void lobby_sync_carries_it_only_to_everyone() {
  start(some(), "7=a.lethe\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  CHECK(l.last_sent() == kLethe);  // the host's send, everyone running the content
  stranger_joins();
  l.tick();  // pending: the game's ghosts only
  CHECK(fake::content_state() == pm::content::State::Pending && type_of(l.current()) == kRandom);
  CHECK(l.last_sent() == kRandom);
  fake::room(1, {1, 2}, 120000, 2);  // silent past the grace: off
  CHECK(fake::content_state() == pm::content::State::Off);
  l.tick();
  CHECK(type_of(l.current()) == kRandom && l.last_sent() == kRandom);
  void* d = Lobby::difficulty(7, kLethe);
  CHECK(l.write(d) == kRandom);  // and anything written now carries Random
}

void a_pick_is_remembered_for_its_preset() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();  // the option offers the custom ghosts
  l.choose(kEcho);
  CHECK(type_of(l.current()) == kEcho);
  CHECK(file_text(picks_file()) == "7=a.echo\n");
  CHECK(logged("custom difficulty preset 7 now forces a.echo"));
  l.choose(6);  // a game ghost replaces it
  CHECK(file_text(picks_file()).empty());
}

void a_pick_that_cannot_be_kept_is_reported() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file() + L".missing\\picks.txt");  // in a folder that does not exist
  l.choose_preset(1);
  l.tick();
  l.choose(kEcho);
  CHECK(type_of(l.current()) == kEcho);  // it holds until the game closes
  CHECK(logged("could not be kept"));
}

// The presets were loaded from the save (Random) by the menu's Awake.
void a_preset_gets_its_pick_back_once_everyone_runs_them() {
  start(some(), "6=a.lethe\n");
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  Lobby l;
  l.side.load(picks_file());
  l.tick();
  CHECK(type_of(l.preset(0)) == kRandom);  // not while anyone lacks the content
  CHECK(l.count() == 5);                   // and the option offers the game's ghosts only
  stranger_leaves();
  CHECK(fake::content_state() == pm::content::State::Active);
  l.tick();
  CHECK(type_of(l.preset(0)) == kLethe);
  // The count read from the collection: Random and every ghost in it.
  CHECK(l.count() == static_cast<int32_t>(l.listed().size()) + 1 && l.count() == 8);
  CHECK(l.sent.empty());  // the lobby's own difficulty did not change
}

// A player who starts a contract without opening the board (inactive, so only the search that
// includes inactive objects finds the menu) still plays the preset's pick.
void a_pick_comes_back_with_the_board_hidden() {
  start(some(), "7=a.asterion\n");
  Lobby l;
  l.boardShown = false;
  l.side.load(picks_file());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, kRandom)));  // as a sync left it
  l.tick();
  CHECK(type_of(l.current()) == kAsterion);
  CHECK(l.sent.size() == 1 && l.last_sent() == kAsterion);
  CHECK(type_of(l.contract_difficulty()) == kAsterion);
  CHECK(logged("gets its custom ghost back"));
  l.contract();
  CHECK(l.spawned == kAsterion);
}

// A player who starts a contract with the board never shown and no sync yet, so the lobby's own
// difficulty is still null: LevelValues.difficulty, at Random, gets the pick, and the host's send falls
// back to it.
void a_pick_comes_back_with_the_board_never_shown() {
  start(some(), "7=a.asterion\n");
  Lobby l;
  l.boardShown = false;
  l.side.load(picks_file());
  CHECK(l.current() == nullptr && type_of(l.contract_difficulty()) == kRandom);
  l.tick();
  CHECK(type_of(l.contract_difficulty()) == kAsterion);
  CHECK(l.sent.size() == 1 && l.last_sent() == kAsterion);
  l.contract();
  CHECK(l.spawned == kAsterion);
}
void the_lobby_difficulty_follows_who_is_in_the_room() {
  start(some(), "7=a.asterion\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  CHECK(type_of(l.current()) == kAsterion && l.last_sent() == kAsterion);
  CHECK(l.index() == l.choice_of(kAsterion));
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  l.tick();
  CHECK(type_of(l.current()) == kRandom && l.last_sent() == kRandom);
  CHECK(l.listed().size() == 4 && l.count() == 5 && l.index() == 0 && l.buttonChecks > 0);
  const size_t sends = l.sent.size();
  stranger_leaves();
  l.tick();  // they leave: the pick comes back and is sent
  CHECK(type_of(l.current()) == kAsterion && l.sent.size() == sends + 1 && l.last_sent() == kAsterion);
  l.tick();
  CHECK(l.sent.size() == sends + 1);  // once
}

void a_client_leaves_the_lobby_difficulty_to_the_host() {
  start(some(), "7=a.asterion\n");
  Lobby l;
  l.side.load(picks_file());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, kRandom)));
  l.tick(fake::content_state(), false);
  CHECK(type_of(l.current()) == kRandom && l.sent.empty());
  set_type(l.current(), kAsterion);  // the host sent its pick
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  l.tick(fake::content_state(), false);
  CHECK(type_of(l.current()) == kRandom && l.sent.empty());  // set aside, nothing sent
}

// The game shows a synced difficulty by its ghost's place in the menu's collection, Random when it is
// not there yet.
void the_option_shows_the_lobby_ghost() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, kEcho)));  // the host's sync
  l.tick(fake::content_state(), false);
  CHECK(l.index() == l.choice_of(kEcho) && l.index() > 0);
  CHECK(l.handlerRuns == 1 && type_of(l.current()) == kEcho);  // the game's handler named it
  CHECK(file_text(picks_file()).empty());                      // which is no pick of this player's
  l.tick(fake::content_state(), false);
  CHECK(l.handlerRuns == 1);
}

// Offering or withdrawing the custom ghosts moves the game's ghosts after their places.
void the_option_keeps_showing_a_game_ghost() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, 1)));  // Wraith
  phasmo::DifficultyOption::index.set(l.option, l.choice_of(1));
  l.tick();
  CHECK(l.index() == l.choice_of(1) && type_of(l.current()) == 1);
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  l.tick();
  CHECK(l.index() == l.choice_of(1) && type_of(l.current()) == 1 && l.sent.empty());
}

void the_summary_names_the_ghost() {
  start(some(), nullptr);
  Lobby l;
  Boxed custom{&l.ghostTypeClass, nullptr, kEcho}, vanilla{&l.ghostTypeClass, nullptr, 3},
      other{&l.otherEnumClass, nullptr, kEcho};
  CHECK(!l.side.enum_name(&custom));  // outside the summary a custom type keeps its number
  l.side.enter_summary();
  CHECK(l.side.enum_name(&custom) == std::optional<std::string>("Echo"));
  CHECK(!l.side.enum_name(&vanilla));  // the game's own ghosts keep their names
  CHECK(!l.side.enum_name(&other));    // only GhostType's names change
  l.side.leave_summary();
  CHECK(!l.side.enum_name(&custom));
  // Through the host's hooks as the game calls them: the builder names it whichever method asks, the
  // summary panel's toggle and the display's refresh, and Enum.ToString anywhere else keeps the number.
  CHECK(l.side.create_hooks());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, kEcho)));
  CHECK(l.toggle_summary() == "Ghost: Echo");
  CHECK(l.refresh_display() == "Ghost: Echo");
  using Name = void* (*)(void*, const void*);
  CHECK(api::to_utf8(reinterpret_cast<Name>(hooked<WName>())(&custom, nullptr)) == "32");
  set_type(l.current(), 3);
  CHECK(l.toggle_summary() == "Ghost: 3");  // the fake's ToString for a game ghost: its number
}

void a_vanilla_contract_never_forces_one() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  set_type(l.contract_difficulty(), kEcho);
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  l.contract();  // decided without custom ghosts
  CHECK(l.spawned == -1 && logged("its ghost is rolled instead"));
}

void stopping_the_mod_leaves_no_custom_pick() {
  start(some(), "6=a.lethe\n7=a.asterion\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  CHECK(type_of(l.current()) == kAsterion && type_of(l.preset(0)) == kLethe);
  l.side.withdraw(true);
  CHECK(type_of(l.current()) == kRandom && type_of(l.preset(0)) == kRandom);
  CHECK(l.listed().size() == 4 && l.count() == 5 && l.last_sent() == kRandom);
  CHECK(file_text(picks_file()) == "6=a.lethe\n7=a.asterion\n");  // kept for next time
  l.tick();  // switched on again, in the same lobby
  CHECK(type_of(l.current()) == kAsterion && type_of(l.preset(0)) == kLethe);
}

// A pending room (a player not heard from yet, or the room's mod lists not matching, which can last)
// offers the game's ghosts only, as off does: a contract loading then would play none of the custom
// ones. The lobby plays Random meanwhile, and each pick comes back once content is active again.
void pending_offers_the_game_ghosts_only() {
  start(some(), "7=a.lethe\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  CHECK(type_of(l.current()) == kLethe && l.listed().size() == 7 && l.count() == 8);
  stranger_joins();
  CHECK(fake::content_state() == pm::content::State::Pending);
  for (int i = 0; i < 5; ++i) l.tick();
  CHECK(type_of(l.current()) == kRandom && l.listed().size() == 4 && l.count() == 5);
  CHECK(l.last_sent() == kRandom && l.index() == 0);
  CHECK(file_text(picks_file()) == "7=a.lethe\n");  // kept for when content is active
  stranger_leaves();
  CHECK(fake::content_state() == pm::content::State::Active);
  l.tick();
  CHECK(type_of(l.current()) == kLethe && l.listed().size() == 7 && l.last_sent() == kLethe);
}

// A new mod's ghost that sorts first shifts every number; the pick stays on its ghost.
void picks_keyed_by_id_survive_renumbering() {
  start(some(), nullptr);
  {
    Lobby l;
    l.side.load(picks_file());
    l.choose_preset(1);
    l.tick();
    l.choose(kLethe);
    CHECK(file_text(picks_file()) == "7=a.lethe\n");
  }
  std::vector<content::GhostDef> more = some();
  more.push_back(ghost("0.abaddon", "Abaddon"));  // sorts before every a.* id
  start(more, file_text(picks_file()).c_str());
  Lobby l;
  l.side.load(picks_file());
  CHECK(fake::content_registry().type_of("a.lethe") == kLethe + 1);
  l.choose_preset(1);
  l.tick();
  CHECK(type_of(l.current()) == kLethe + 1);
  CHECK(fake::content_registry().id_of(type_of(l.current())) == std::string("a.lethe"));
}

// The older file kept each pick by type number, which no longer names one ghost: it is read once,
// its numbers left out (logged), and the next pick writes the file by ghost id.
void picks_file_of_the_older_form_is_rewritten() {
  start(some(), "7=38\n6=a.echo\n");
  Lobby l;
  l.side.load(picks_file());
  CHECK(logged("holds 1 picks by type number from an older version, which no longer name a ghost; those "
               "presets play Random until a ghost is picked again"));
  CHECK(l.side.picks().of(7).empty() && l.side.picks().of(6) == "a.echo");
  CHECK(file_text(picks_file()) == "7=38\n6=a.echo\n");  // untouched until a pick is kept
  l.choose_preset(1);
  l.tick();
  l.choose(kAsterion);
  CHECK(file_text(picks_file()) == "6=a.echo\n7=a.asterion\n");
  CHECK(logged("no longer holds the older picks by type number"));
  int legacy = -1;
  CHECK(pm::host::DifficultyPicks::parse("6=a.echo\nnonsense\n7=\n=a.x\n", legacy).text() == "6=a.echo\n" &&
        legacy == 0);
  // Not a content id at all: a dot alone no longer passes for one.
  CHECK(pm::host::DifficultyPicks::parse("6=a.b.c.d\n7=Bad.x\n8=x.\n", legacy).text().empty());
}

// A pick saved by a bundled mod's old id moves to the new one, once, in the file too; a pick nobody has reverts to
// Random, by the same path as any unknown pick.
void old_bundled_pick_moves_once() {
  int legacy = -1, migrated = 0;
  CHECK(pm::host::DifficultyPicks::parse("6=custom_ghosts.black_shuck\n7=gone.mod.ghost\n", legacy, &migrated).text() ==
            "6=phasmomods.custom-ghosts.black_shuck\n7=gone.mod.ghost\n" &&
        migrated == 1 && legacy == 0);
  migrated = 0;
  CHECK(pm::host::DifficultyPicks::parse("6=phasmomods.custom-ghosts.black_shuck\n", legacy, &migrated).of(6) ==
            "phasmomods.custom-ghosts.black_shuck" &&
        migrated == 0);
  const std::wstring file = picks_file();
  start(some(), "6=custom_ghosts.echo\n");
  Lobby l;
  l.side.load(file);
  CHECK(file_text(file) == "6=phasmomods.custom-ghosts.echo\n");
  CHECK(logged("they move to its new id"));
  CHECK(l.side.picks().of(6) == "phasmomods.custom-ghosts.echo");
}

// The live bug of 2026-10-25: a custom ghost picked on Custom 2 applied to the first contract only.
// The preset choice (0x96DE70) makes the lobby's difficulty a copy; the save button (0x9718D0) makes it
// the preset object itself, and ApplyDifficulty saves "LastDifficulty" from that object. After the
// contract the menu scene loads again: Awake loads the presets (Random) under their saved references,
// and the board's one-shot handler (0x969670) loads "LastDifficulty" into the very preset object, then
// applies it: the lobby's difficulty is the preset, LevelValues.difficulty a Random copy. The preset must
// not take its pick first and hide the change: the lobby's difficulty is sent, so the next contract,
// started without opening the page, plays it.
void pick_lasts_past_a_contract() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  l.choose(kLethe);
  l.save_preset(1);
  CHECK(l.current() == l.preset(1));
  l.apply();  // the board closes: the game's own sync carries Random
  CHECK(l.last_sent() == kRandom);
  l.tick();
  CHECK(l.last_sent() == kLethe);  // the host's send after it carries the pick
  l.contract();
  CHECK(l.spawned == kLethe);
  l.show_board();  // back in the lobby, the board's one-shot handler
  CHECK(l.current() == l.preset(1) && type_of(l.contract_difficulty()) == kRandom);
  l.tick();
  CHECK(l.last_sent() == kLethe && type_of(l.contract_difficulty()) == kLethe);
  l.contract();  // the page never opened
  CHECK(l.spawned == kLethe);
}

// The same without the save button: the lobby's difficulty is a copy, which the handler's load makes a
// new object holding Random; the pick still reaches the contract.
void pick_lasts_past_a_contract_without_a_save() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  l.choose(kLethe);
  CHECK(l.current() != l.preset(1));
  l.apply();
  l.tick();
  CHECK(l.last_sent() == kLethe);
  l.contract();
  CHECK(l.spawned == kLethe);
  l.show_board();
  CHECK(l.current() != l.preset(1) && type_of(l.current()) == kRandom && type_of(l.contract_difficulty()) == kRandom);
  l.tick();
  CHECK(l.last_sent() == kLethe && type_of(l.contract_difficulty()) == kLethe);
  l.contract();
  CHECK(l.spawned == kLethe);
}

// A player Photon lists who is not verified yet gets no custom pick, even in the very frame they appear.
void a_send_waits_for_a_player_who_just_joined() {
  start(some(), "7=a.lethe\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.verified = false;
  l.tick();
  CHECK(type_of(l.current()) == kLethe && l.sent.empty());  // given back here, not sent
  l.verified = true;
  l.tick();
  CHECK(l.last_sent() == kLethe);
}

// Another client with the same content, in the room as `actor`, its manifest delivered: content agrees.
void agreeing_peer(pm::content::Registry& peer, int actor, const std::vector<int>& room, uint64_t now) {
  pm::content::GameEnums enums;
  CHECK(pm::content::read_game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                                     fake::kAuditedNoEvidence, enums)
            .empty());
  peer.set_game_enums(std::move(enums));
  peer.begin_group();
  for (const content::GhostDef& g : some()) CHECK(peer.add_ghost(&g.c_def()) > 0);
  peer.end_group(true);
  peer.commit();
  peer.observe(actor, room, now);
  for (const std::vector<uint8_t>& m : peer.take_messages()) fake::host_message(actor, m.data(), m.size());
}

// The host's mods stay on while a player is being verified, so nothing custom goes out, and the host sees nothing change: the
// menu still offers the ghosts, and its own difficulty keeps its pick. The game's own sync while it lasts writes Random, and the
// next frame gives the host's copies their pick back.
void custom_values_held_back_while_a_player_is_unverified() {
  start(some(), "7=a.lethe\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.verified = false;  // a player has joined, not verified yet
  l.tick();
  CHECK(fake::content_state() == pm::content::State::Active);
  CHECK(type_of(l.current()) == kLethe && l.sent.empty());
  const std::vector<int32_t> listedNow = l.listed();
  for (const int32_t type : {kAsterion, kEcho, kLethe})
    CHECK(std::count(listedNow.begin(), listedNow.end(), type) == 1);  // the menu still offers them
  CHECK(l.send_to_joiner() == kRandom);
  l.apply();  // the host applies the difficulty meanwhile: the game sends it to everyone
  CHECK(!l.sent.empty() && l.last_sent() == kRandom);
  const size_t sends = l.sent.size();
  l.tick();
  CHECK(type_of(l.current()) == kLethe && type_of(l.contract_difficulty()) == kLethe);
  CHECK(l.sent.size() == sends);  // nothing more until they are verified
}

// Once everyone is verified the pick goes out, once.
void custom_values_flow_once_everyone_is_verified() {
  start(some(), "7=a.lethe\n");
  Lobby l;
  l.side.load(picks_file());
  l.choose_preset(1);
  l.tick();
  CHECK(l.last_sent() == kLethe);
  // A joiner: what it is sent holds no custom type, and nor does anything go to the others.
  l.verified = false;
  const size_t before = l.sent.size();
  CHECK(l.send_to_joiner() == kRandom);
  l.tick();
  CHECK(l.sent.size() == before);
  l.verified = true;
  l.tick();
  CHECK(l.sent.size() == before + 1 && l.last_sent() == kLethe);
  l.tick();
  CHECK(l.sent.size() == before + 1);
}
// Off takes every custom type out of the lobby the menu was found in, offered to it or not.
void off_sets_aside_a_menu_never_offered() {
  start(some(), nullptr);
  stranger_joins();
  fake::room(1, {1, 2}, 120000, 2);
  Lobby l;
  l.side.load(picks_file());
  phasmo::DifficultyUIManager::current.set(l.menu,
                                          pm::api::Ref<pm::map::Difficulty>::unchecked(Lobby::difficulty(7, kEcho)));  // a sync brought one
  set_type(l.contract_difficulty(), kEcho);
  l.tick();
  CHECK(type_of(l.current()) == kRandom && type_of(l.contract_difficulty()) == kRandom);
  CHECK(l.last_sent() == kRandom);
}

// The menu is searched for once a second at most until found.
void the_menu_search_is_rate_limited() {
  start(some(), nullptr);
  Lobby l;
  l.side.load(picks_file());
  l.menuInScene = false;
  for (int i = 0; i < 60; ++i) l.tick();  // about a second of frames
  CHECK(l.searches == 1);
  l.menuInScene = true;
  for (int i = 0; i < 70; ++i) l.tick();
  CHECK(l.searches == 2);
}

}  // namespace

int main() {
  save_never_holds_custom_type();
  lobby_sync_carries_it_only_to_everyone();
  a_pick_is_remembered_for_its_preset();
  a_pick_that_cannot_be_kept_is_reported();
  a_preset_gets_its_pick_back_once_everyone_runs_them();
  a_pick_comes_back_with_the_board_hidden();
  a_pick_comes_back_with_the_board_never_shown();
  the_lobby_difficulty_follows_who_is_in_the_room();
  a_client_leaves_the_lobby_difficulty_to_the_host();
  the_option_shows_the_lobby_ghost();
  the_option_keeps_showing_a_game_ghost();
  the_summary_names_the_ghost();
  a_vanilla_contract_never_forces_one();
  stopping_the_mod_leaves_no_custom_pick();
  pending_offers_the_game_ghosts_only();
  picks_keyed_by_id_survive_renumbering();
  picks_file_of_the_older_form_is_rewritten();
  pick_lasts_past_a_contract();
  old_bundled_pick_moves_once();
  pick_lasts_past_a_contract_without_a_save();
  a_send_waits_for_a_player_who_just_joined();
  custom_values_held_back_while_a_player_is_unverified();
  custom_values_flow_once_everyone_is_verified();
  off_sets_aside_a_menu_never_offered();
  the_menu_search_is_rate_limited();
  std::filesystem::remove(picks_file());
  std::printf("content_difficulty: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
