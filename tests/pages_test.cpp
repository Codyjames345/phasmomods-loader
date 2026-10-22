#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "game.h"
#include "ui.h"
#include "check.h"

namespace {
struct Node {
  alignas(8) char fields[0x400]{};
  std::string name;
  Node* event = nullptr;
  Node* transform = nullptr;
  Node* button = nullptr;
  bool active = true;
  bool constructed = false;
  bool clone = false;
  bool enabled = true;                  // as a Behaviour
  pg::ui::Vec2 position{0, 0};          // as a RectTransform: anchoredPosition
  pg::ui::Vec2 size{1, 1};              // as a RectTransform: rect size
  pg::ui::Vec2 cell{1, 1};              // as a GridLayoutGroup
  int constraint = 0, constraintCount = 0;
};
std::vector<std::unique_ptr<Node>> nodes;
std::vector<std::string> logs;
Node *fixtureJournal, *fixtureBook, *fixtureGrid, *fixtureList, *fixtureButtonTemplate, *fixtureEntryTemplate, *fixturePageTemplate, *nativeEvent;
Node* fixtureListGrid;              // the Ghosts tab list's GridLayoutGroup
std::vector<Node*> listEntries;     // the Ghosts tab list's shipped entries
void* hitEntry = nullptr;
int selectedPage = -1, constructorCalls = 0, forbiddenLookups = 0;
int allocationCalls = 0, allocationFailsAt = 0, constructorThrowsAt = 0, collectionAdds = 0;
bool eventAllocation = true, eventConstructor = true, constructorThrows = false;
bool entryHasButton = true, eventHasClass = true;
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
bool holding() {
  for (void* object : holds)
    if (object) return true;
  return false;
}

/** Allocate stable fake objects large enough for the production offset fields. */
Node* node(const char* name = "") {
  auto value = std::make_unique<Node>();
  value->name = name;
  auto* result = value.get();
  nodes.push_back(std::move(value));
  return result;
}
/** An object's name, as a managed string (to_utf8 reads it back). */
il2cpp::String* get_name(void* object, const void*) {
  return reinterpret_cast<il2cpp::String*>(node(static_cast<Node*>(object)->name.c_str()));
}
/** Preserve the names assigned to cloned custom entries. */
void set_name(void* object, il2cpp::String* name, const void*) {
  static_cast<Node*>(object)->name = reinterpret_cast<Node*>(name)->name;
}
/** Transform.SetSiblingIndex: the entry moves to `index` among the list's children. */
void set_sibling(void* child, int index, const void*) {
  auto it = std::find(listEntries.begin(), listEntries.end(), child);
  if (it == listEntries.end()) return;
  listEntries.erase(it);
  listEntries.insert(listEntries.begin() + std::min<size_t>(index, listEntries.size()), static_cast<Node*>(child));
}
/** A grid's cell size, as the fixture holds it. */
pg::ui::Vec2 get_cell(void* grid, const void*) { return static_cast<Node*>(grid)->cell; }
void set_cell(void* grid, pg::ui::Vec2 cell, const void*) { static_cast<Node*>(grid)->cell = cell; }
/** The shipped list grid's spacing. */
pg::ui::Vec2 get_spacing(void*, const void*) { return {0.5f, 2.5f}; }
int get_constraint(void* grid, const void*) { return static_cast<Node*>(grid)->constraint; }
void set_constraint(void* grid, int value, const void*) { static_cast<Node*>(grid)->constraint = value; }
int get_constraint_count(void* grid, const void*) { return static_cast<Node*>(grid)->constraintCount; }
void set_constraint_count(void* grid, int value, const void*) { static_cast<Node*>(grid)->constraintCount = value; }
/** A RectTransform's size and position. */
pg::ui::Rect get_rect(void* rect, const void*) { return {0, 0, static_cast<Node*>(rect)->size.x, static_cast<Node*>(rect)->size.y}; }
pg::ui::Vec2 get_position(void* rect, const void*) { return static_cast<Node*>(rect)->position; }
/** Read the event reference so a shallow clone exposes template sharing. */
void* get_event(void* object, const void*) { return static_cast<Node*>(object)->event; }
/** Replace only the clone's event, preserving its template. */
void set_event(void* object, void* event, const void*) { static_cast<Node*>(object)->event = static_cast<Node*>(event); }
/** Construct an empty event, with an injectable managed failure. */
void event_ctor(void* object, const void*) {
  ++constructorCalls;
  if (constructorThrows || constructorCalls == constructorThrowsAt) throw Il2CppExceptionWrapper{nativeEvent};
  static_cast<Node*>(object)->constructed = true;
}
/** Use the camera fallback for deterministic hit tests. */
void* get_camera(void*, const void*) { return nullptr; }
/** Accept evidence refresh without invoking native game logic. */
void refresh_ghosts(void*, const void*) {}
/** Record the page chosen by production manual click handling. */
void go_to_page(void*, int page, const void*) { selectedPage = page; }
/** Accept list additions; this regression concerns entry events and page mapping. */
void add_one(void*, void*, const void*) { ++collectionAdds; }
/** Accept dictionary additions without changing the production control flow. */
void add_two(void*, void*, void*, const void*) { ++collectionAdds; }
}

#include "../mods/custom_ghosts/pages.cpp"

namespace il2cpp {
/** Record kept objects so the tests can see each is held while kept and released after. */
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
/** Use distinct metadata identities for component selection. */
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
/** Preserve the metadata name as the fake System.Type identity. */
Object* type_object(Class* type) { return reinterpret_cast<Object*>(type); }
/** Bind only APIs present in the captured game build. */
void* method_ptr(Class*, const char* name, int) {
#define BIND(text, function) if (std::strcmp(name, text) == 0) return reinterpret_cast<void*>(&function)
  BIND("get_name", get_name); BIND("set_name", set_name); BIND("SetSiblingIndex", set_sibling);
  BIND("get_cellSize", get_cell); BIND("set_cellSize", set_cell); BIND("get_spacing", get_spacing);
  BIND("get_constraint", get_constraint); BIND("set_constraint", set_constraint);
  BIND("get_constraintCount", get_constraint_count); BIND("set_constraintCount", set_constraint_count);
  BIND("get_anchoredPosition", get_position);
  BIND("get_rect", get_rect); BIND("get_onClick", get_event); BIND("set_onClick", set_event);
  BIND("get_worldCamera", get_camera);
  if (std::strcmp(name, ".ctor") == 0) return eventConstructor ? reinterpret_cast<void*>(&event_ctor) : nullptr;
  if (std::strcmp(name, "GetPersistentEventCount") == 0 || std::strcmp(name, "SetPersistentListenerState") == 0)
    ++forbiddenLookups;
#undef BIND
  return nullptr;
}
/** Supply the Add method pointer layout consumed by production collections. */
const Method* method(Class*, const char* name, int argc) {
  static void* one = reinterpret_cast<void*>(&add_one);
  static void* two = reinterpret_cast<void*>(&add_two);
  return std::strcmp(name, "Add") == 0 ? (argc == 1 ? &one : &two) : nullptr;
}
/** Expose the event class, with an injectable missing metadata case. */
Class* class_of(const void* object) { return eventHasClass ? const_cast<void*>(object) : nullptr; }
/** Permit journal discovery checks without requiring managed metadata. */
bool is_class(const void* object, const char*) { return object != nullptr; }
/** Allocate an unconstructed event or reproduce allocation failure. */
Object* object_new(Class*) {
  ++allocationCalls;
  return eventAllocation && allocationCalls != allocationFailsAt ? reinterpret_cast<Object*>(node()) : nullptr;
}
/** Store small managed arrays in the fixture's aligned field storage. */
Array* array_new(Class*, uintptr_t length) {
  auto* result = reinterpret_cast<Array*>(node());
  result->max_length = length;
  return result;
}
/** Keep managed strings readable for assertions on entry names. */
String* string_new(const char* text) { return reinterpret_cast<String*>(node(text)); }
/** Names read back as the fixture holds them. */
void to_utf8(const String* s, char* out, size_t size) {
  if (size) std::snprintf(out, size, "%s", reinterpret_cast<const Node*>(s)->name.c_str());
}
/** Preserve offset references written by production. */
void set_ref_field(void* object, size_t offset, void* value) { set_field(object, offset, value); }
}

namespace pg::game {
/** Resolve the fixed journal layout used by extension. */
void* find_child(void* parent, const char* path) {
  if (parent == fixtureJournal && std::strcmp(path, "Content/GhostHuntingBookContent") == 0) return fixtureBook;
  if (parent == fixtureBook && std::strcmp(path, "Evidence.2/All Ghosts") == 0) return fixtureGrid;
  if (parent == fixtureBook && std::strcmp(path, "Ghost Select Page/Ghost Types List") == 0) return fixtureList;
  if (std::strcmp(path, "Spirit") == 0) {
    if (parent == fixtureGrid) return fixtureButtonTemplate;
    if (parent == fixtureList) return fixtureEntryTemplate;
    if (parent == fixtureBook) return fixturePageTemplate;
  }
  return nullptr;
}
/** Resolve only the components needed to reach the event replacement. */
void* get_component(void* object, il2cpp::Object* type) {
  const auto* name = reinterpret_cast<const char*>(type);
  if (std::strcmp(name, "GhostButton") == 0) return object;
  if (std::strcmp(name, "Button") == 0) return entryHasButton ? static_cast<Node*>(object)->button : nullptr;
  if (std::strcmp(name, "GridLayoutGroup") == 0) return object == fixtureList ? fixtureListGrid : nullptr;
  return nullptr;
}
/** Shallow-copy the serialized event to detect mutation of the template event. A copy made into the
 * list is its last child, where the original was placed (its anchored position comes with it). */
void* instantiate(void* original, void* parent, bool) {
  auto* result = node();
  *result = *static_cast<Node*>(original);
  result->clone = true;
  if (result->transform) {
    auto* rect = node();
    *rect = *result->transform;
    rect->clone = true;
    rect->button = result;
    result->transform = rect;
  }
  if (parent == fixtureList) listEntries.push_back(result->transform ? result->transform : result);
  return result;
}
/** Keep Button components distinct from their RectTransforms to check navigation uses transforms. */
void* transform_of(void* object) {
  auto* value = static_cast<Node*>(object);
  return value->transform ? value->transform : value;
}
/** Represent the owning game object by its transform for activation and names. */
void* game_object_of(void* object) { return transform_of(object); }
/** Track activation for the manual click filter. */
void set_active(void* object, bool active) { static_cast<Node*>(object)->active = active; }
/** Behaviours (the list's grid; there are no localisers in this fixture). */
void set_enabled(void* behaviour, bool on) { static_cast<Node*>(behaviour)->enabled = on; }
bool enabled(void* behaviour) { return static_cast<Node*>(behaviour)->enabled; }
/** Only the Ghosts tab list has children: its shipped entries. */
int child_count(void* parent) { return parent == fixtureList ? static_cast<int>(listEntries.size()) : 0; }
void* child(void* parent, int i) { return parent == fixtureList ? listEntries[i] : nullptr; }
/** Preserve active state for production filters. */
bool active_self(void* object) { return static_cast<Node*>(object)->active; }
/** There are no inactive ancestors in the fixture. */
bool active_in_hierarchy(void* object) { return active_self(object); }
/** Keep the fixture journal alive throughout each case. */
bool alive(void* object) { return object != nullptr; }
/** Direct extension tests do not discover network journals. */
void* network() { return nullptr; }
/** Allow the production manual click path. */
bool cursor_locked() { return false; }
/** Model one click per explicit handler call. */
bool mouse_left_pressed() { return true; }
/** Supply a deterministic pointer location. */
bool mouse_position(float& x, float& y) { x = y = 1; return true; }
/** Hit tests do not require a camera in this fixture. */
void* local_camera() { return nullptr; }
/** Resolve only the two verified game RVAs used by pages. */
void* game_assembly_address(uintptr_t rva) {
  return rva == 0x22C5090 ? reinterpret_cast<void*>(&refresh_ghosts) : reinterpret_cast<void*>(&go_to_page);
}
}
namespace pg::ui {
/** Page text is outside the event regression. */
void set_text(void*, const char*) {}
/** Select exactly one requested entry without geometric ambiguity. */
bool contains(void* object, Vec2, void*) { return object == hitEntry; }
}
namespace pg::log {
/** Capture diagnostics so failure cases must explain the missing event. */
void write(const char* format, ...) {
  char line[512];
  va_list args;
  va_start(args, format);
  std::vsnprintf(line, sizeof line, format, args);
  va_end(args);
  logs.emplace_back(line);
}
}
namespace {
/** Create a minimal journal with three existing pages and a shared template event. */
void reset() {
  pg::custom::pages::teardown();
  nodes.clear(); logs.clear();
  eventAllocation = eventConstructor = entryHasButton = eventHasClass = true;
  constructorThrows = false;
  constructorCalls = forbiddenLookups = 0;
  allocationCalls = allocationFailsAt = constructorThrowsAt = collectionAdds = 0;
  selectedPage = -1; hitEntry = nullptr;
  fixtureJournal = node(); fixtureBook = node(); fixtureGrid = node(); fixtureList = node();
  fixtureButtonTemplate = node(); fixturePageTemplate = node();
  // The Ghosts tab list as build 25811160 ships it: 30 entries placed by hand in alphabetical order,
  // its grid switched off and holding a cell size its entries do not use.
  fixtureListGrid = node();
  fixtureListGrid->enabled = false;
  fixtureListGrid->cell = {280, 100};
  fixtureListGrid->constraintCount = 2;
  listEntries.clear();
  const char* names[30] = {"Aswang", "Banshee", "Dayan", "Deildegast", "Demon", "Deogen", "Gallu", "Goryo",
                           "Hantu", "Jinn", "Kormos", "Mare", "Moroi", "Myling", "Obake", "Obambo",
                           "Oni", "Onryo", "Phantom", "Poltergeist", "Raiju", "Revenant", "Shade", "Spirit",
                           "Thaye", "The Mimic", "The Twins", "Wraith", "Yokai", "Yurei"};
  const float columnX[3] = {125.0f, 375.5f, 626.0f};
  for (int i = 0; i < 30; ++i) {
    listEntries.push_back(node(names[i]));
    listEntries.back()->position = {columnX[i % 3], -50.0f - 102.5f * (i / 3)};
    listEntries.back()->size = {250, 100};
  }
  fixtureEntryTemplate = listEntries[23];  // Spirit, which the custom entries are copied from
  nativeEvent = node("template callback");
  fixtureEntryTemplate->button = node();
  fixtureEntryTemplate->button->transform = fixtureEntryTemplate;
  fixtureEntryTemplate->button->event = nativeEvent;
  il2cpp::set_ref_field(fixtureJournal, 0xC0, il2cpp::array_new(nullptr, 3));
  il2cpp::set_ref_field(fixtureJournal, 0x1B8, node());
  il2cpp::set_ref_field(fixtureJournal, 0x1D8, node());
  il2cpp::set_ref_field(fixtureJournal, 0x208, node());
  il2cpp::set_field(fixtureJournal, 0xB8, true);
}
/** Missing listener-state APIs must not prevent load; each clone opens its own custom page. */
void independent_entries() {
  reset();
  CHECK(pg::custom::pages::resolve());
  CHECK(forbiddenLookups == 0);
  if (!pg::custom::pages::api.ok) return;
  pg::custom::pages::Extended extended{};
  CHECK(pg::custom::pages::extend(fixtureJournal, extended));
  CHECK(extended.entries.size() == pg::custom::kGhostCount);
  CHECK(constructorCalls == pg::custom::kGhostCount);
  CHECK(fixtureEntryTemplate->button->event == nativeEvent && !nativeEvent->constructed);
  CHECK(held(fixtureJournal));
  for (const auto& entry : extended.entries) CHECK(held(entry.rect));
  pg::custom::pages::g_journals.push_back(std::move(extended));
  const auto& recorded = pg::custom::pages::g_journals.back();
  for (size_t i = 0; i < recorded.entries.size(); ++i) {
    auto* entry = static_cast<Node*>(recorded.entries[i].rect);
    CHECK(entry->name == pg::custom::kGhosts[i].name);
    CHECK(entry->button != nullptr);
    if (!entry->button) continue;
    auto* event = entry->button->event;
    CHECK(event && event != nativeEvent && event->constructed);
    for (size_t j = 0; j < i; ++j)
      CHECK(event != static_cast<Node*>(recorded.entries[j].rect)->button->event);
    hitEntry = entry;
    pg::custom::pages::handle_clicks();
    CHECK(selectedPage == 3 + static_cast<int>(i));
  }
  CHECK(il2cpp::field<il2cpp::Array*>(fixtureJournal, 0xC0)->max_length == 3 + pg::custom::kGhostCount);
  pg::custom::pages::teardown();
  CHECK(!holding());  // every journal and entry let go with the record
}
/** Event setup failures must stop extension, explain why, and leave the template untouched. */
void event_failures() {
  for (int mode = 0; mode < 8; ++mode) {
    reset();
    if (!pg::custom::pages::resolve()) return;
    if (mode == 0) eventAllocation = false;
    if (mode == 1) eventConstructor = false;
    if (mode == 2) constructorThrows = true;
    if (mode == 3) fixtureEntryTemplate->button->event = nullptr;
    if (mode == 4) entryHasButton = false;
    if (mode == 5) eventHasClass = false;
    if (mode == 6) allocationFailsAt = 2;
    if (mode == 7) constructorThrowsAt = 3;
    const auto before = logs.size();
    pg::custom::pages::Extended extended{fixtureJournal, nullptr, {}};
    bool result = true;
    try { result = pg::custom::pages::extend(fixtureJournal, extended); }
    catch (...) { CHECK(false); }
    CHECK(!result);
    CHECK(extended.entries.empty());
    CHECK(logs.size() > before);
    CHECK(!nativeEvent->constructed);
    CHECK(fixtureEntryTemplate->button->event == (mode == 3 ? nullptr : nativeEvent));
    CHECK(il2cpp::field<il2cpp::Array*>(fixtureJournal, 0xC0)->max_length == 3);
    CHECK(collectionAdds == 0);
    // tick retains a failed journal and still handles clicks on its recorded entries.
    pg::custom::pages::g_journals.push_back(std::move(extended));
    int activeClones = 0, templateCallbacks = 0;
    for (const auto& object : nodes) {
      if (!object->clone || !object->active) continue;
      ++activeClones;
      templateCallbacks += object->event == nativeEvent;
      hitEntry = object.get();
      pg::custom::pages::handle_clicks();
    }
    CHECK(activeClones == 0);
    CHECK(templateCallbacks == 0);
    CHECK(selectedPage == -1);
    CHECK(held(fixtureJournal));
    pg::custom::pages::teardown();
    CHECK(!holding());
  }
}
/** The Ghosts tab list's grid ships switched off, so copies of an entry would all sit where it does:
 * extending switches the grid on, set to the shipped layout, with rows fitted to every entry. */
void the_ghosts_tab_list_is_laid_out_again() {
  reset();
  CHECK(pg::custom::pages::resolve());
  if (!pg::custom::pages::api.ok) return;
  pg::custom::pages::Extended extended{};
  CHECK(pg::custom::pages::extend(fixtureJournal, extended));
  CHECK(listEntries.size() == 45 && listEntries.front()->name == "Asterion");  // a copy of Spirit, on Spirit's row
  CHECK(fixtureListGrid->enabled);
  CHECK(fixtureListGrid->constraint == 1 && fixtureListGrid->constraintCount == 3);  // three fixed columns
  CHECK(fixtureListGrid->cell.x == 250.0f);
  CHECK(std::fabs(15 * (fixtureListGrid->cell.y + 2.5f) - 10 * 102.5f) < 0.01f);  // 45 in the 30's height
  pg::custom::pages::g_journals.push_back(std::move(extended));
  pg::custom::pages::teardown();
}
}
/** Run the production pages regression without a game runtime. */
int main() {
  independent_entries();
  event_failures();
  the_ghosts_tab_list_is_laid_out_again();
  std::printf("pages: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
