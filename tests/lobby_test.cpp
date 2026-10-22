#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "game.h"
#include "ui.h"

namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::printf("FAIL line %d: %s\n", __LINE__, #value); ++failures; } } while (0)
// One fake Unity or managed object. Raw IL2CPP reads (List, Array, offset fields) land in `fields`.
struct Node {
  char fields[0x200]{};
  const char* className = "";
  Node* parent = nullptr;
  Node* event = nullptr;
  Node* group = nullptr;
  Node* targetGraphic = nullptr;
  Node* graphic = nullptr;
  Node* gameObject = nullptr;
  Node* component = nullptr;
  Node* localiser = nullptr;
  Node* photonPlayer = nullptr;
  int actor = 0;
  bool isGameObject = false;
  bool label = false, active = true, enabled = true, interactable = false, on = true;
  bool alive = true, owned = false;
  bool autoSize = false, wrap = true;
  float fontSize = 36, fontMin = 0, fontMax = 0;
  pg::ui::Vec2 position{0, 0}, size{0.0666f, 0.022f};
  il2cpp::Vector3 scale{1, 1, 1};
  pg::ui::Color color{0.4f, 0.4f, 0.4f, 1};  // ServerManager's grey while Ready cannot be clicked
  std::string text = "Ready";
};
std::unordered_map<void*, std::unique_ptr<Node>> nodes;
Node *fixtureManager, *fixtureReady, *fixtureReadyLabel, *board, *nativeEvent, *nativeGroup;
Node *cardList, *cardArray, *localCard, *otherCard;
ULONGLONG fixtureClock = 0;
const pg::ui::Color kEnabledColour{0.1f, 0.2f, 0.3f, 1};  // ServerManager.enabledColour
int localActor = 1;
bool privateRoom = true, eventAllocation = true, eventConstructor = true, instantiateOk = true, constructorThrows = false;
bool labelCloneFails = false;
int instantiated = 0, nativeCallbacks = 0, nativeGroupEffects = 0;
int labelReads = 0;      // clone attempts: each reads ServerManager.readyText first
int liveLocalisers = 0;  // owned labels that became visible with their localiser still enabled
std::vector<std::string> logs;

/** Allocate persistent fake identities so destroyed objects remain safe to inspect. */
Node* node(const char* className = "") {
  auto value = std::make_unique<Node>();
  auto* result = value.get();
  result->className = className;
  nodes.emplace(result, std::move(value));
  return result;
}
/** Model hierarchy activation, including the inactive staging parent. */
bool hierarchy(Node* value) { return value && value->alive && value->active && (!value->parent || hierarchy(value->parent)); }
/** Observe native group side effects at each activation boundary. */
void enable_effects(Node* value) {
  if (hierarchy(value) && value->enabled && value->group && value->on) {
    ++nativeGroupEffects;
    if (value->event == nativeEvent) ++nativeCallbacks;
  }
}
/** A copied localiser that is enabled when its label first becomes visible would reset the text. */
void check_localisers() {
  for (auto& [_, value] : nodes)
    if (value->owned && value->label && hierarchy(value.get()) && value->localiser && value->localiser->enabled) ++liveLocalisers;
}
/** The inactive staging object must not outlive a clone attempt. */
bool staging_left() {
  for (auto& [_, value] : nodes) if (value->alive && value->owned && value->isGameObject) return true;
  return false;
}
/** A real user click invokes the checkbox event, including any copied native listeners. */
void user_click(Node* value) { value->on = !value->on; if (value->event == nativeEvent) ++nativeCallbacks; }
/** Return deterministic monotonic time without driving the game. */
ULONGLONG WINAPI clock_ms() { return fixtureClock; }
/** Return the scene's native ServerManager. */
void* find_manager(void*, const void*) { return fixtureManager; }
/** Clone the object, its owned graphic and its components, as Unity's Instantiate does. */
void* instantiate(void* original, void* parent, bool world, const void*) {
  ++instantiated;
  if (!instantiateOk || (labelCloneFails && original == fixtureReadyLabel)) return nullptr;
  auto* result = node();
  *result = *static_cast<Node*>(original);
  result->parent = static_cast<Node*>(parent);
  result->owned = true;
  if (world && result->parent) {
    result->position.x -= result->parent->position.x;
    result->position.y -= result->parent->position.y;
  }
  if (result->graphic && result->graphic->parent == original) {
    auto* graphic = node(); *graphic = *result->graphic;
    graphic->parent = result; graphic->owned = true; result->graphic = graphic;
  }
  if (result->localiser) {
    auto* localiser = node(); *localiser = *result->localiser; localiser->owned = true; result->localiser = localiser;
  }
  enable_effects(result);
  return result;
}
void* parent_of(void* value, const void*) { return static_cast<Node*>(value)->parent; }
void set_scale(void* value, il2cpp::Vector3 scale, const void*) { static_cast<Node*>(value)->scale = scale; }
bool is_on(void* value, const void*) { return static_cast<Node*>(value)->on; }
/** Model Toggle's silent setter, which still affects its native group. */
void set_on(void* value, bool state, const void*) {
  auto* item = static_cast<Node*>(value);
  if (item->group && state) ++nativeGroupEffects;
  item->on = state;
}
void set_interactable(void* value, bool state, const void*) { static_cast<Node*>(value)->interactable = state; }
/** A native Selectable can reference the externally owned Ready text. */
void* target_graphic(void* value, const void*) { return static_cast<Node*>(value)->targetGraphic; }
void set_target_graphic(void* value, void* graphic, const void*) { static_cast<Node*>(value)->targetGraphic = static_cast<Node*>(graphic); }
pg::ui::Vec2 position_of(void* value, const void*) { return static_cast<Node*>(value)->position; }
void set_position(void* value, pg::ui::Vec2 position, const void*) { static_cast<Node*>(value)->position = position; }
struct Rect { float x, y, width, height; };
Rect rect_of(void* value, const void*) {
  auto* item = static_cast<Node*>(value);
  return {-item->size.x / 2, -item->size.y / 2, item->size.x, item->size.y};
}
bool active(void* value, const void*) { return static_cast<Node*>(value)->active; }
bool active_hierarchy(void* value, const void*) { return hierarchy(static_cast<Node*>(value)); }
bool enabled(void* value, const void*) { return static_cast<Node*>(value)->enabled; }
bool interactable(void* value, const void*) { return static_cast<Node*>(value)->interactable; }
il2cpp::String* name_of(void*, const void*) { return nullptr; }
/** Construct an independent UnityEvent with no copied listeners. */
void event_ctor(void* value, const void*) { if (constructorThrows) throw 1; static_cast<Node*>(value)->text = "fresh event"; }
}

namespace il2cpp {
Field* field_info(Class*, const char*);
void* reference_field(void*, Field*);
void read_field(void*, Field*, void*);
void set_reference_field(void*, Field*, void*);
}
#define GetTickCount64 clock_ms
#include "../mods/playable_ghost/lobby.cpp"
#undef GetTickCount64

namespace il2cpp {
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
Class* cls(const char*, const char*, const char*) { return reinterpret_cast<Class*>(1); }
Object* type_object(Class*) { return reinterpret_cast<Object*>(1); }
/** Resolve the exact production method names to fake Unity boundaries. */
void* method_ptr(Class*, const char* name, int) {
#define BIND(nameText, function) if (std::strcmp(name, nameText) == 0) return reinterpret_cast<void*>(&function)
  BIND("FindObjectOfType", find_manager); BIND("get_parent", parent_of);
  BIND("get_isOn", is_on); BIND("SetIsOnWithoutNotify", set_on); BIND("set_interactable", set_interactable);
  BIND("get_targetGraphic", target_graphic); BIND("set_targetGraphic", set_target_graphic);
  BIND("get_anchoredPosition", position_of); BIND("set_anchoredPosition", set_position);
  BIND("get_activeSelf", active); BIND("get_activeInHierarchy", active_hierarchy);
  BIND("get_enabled", enabled); BIND("get_interactable", interactable); BIND("get_rect", rect_of); BIND("get_name", name_of);
  if (std::strcmp(name, ".ctor") == 0) return eventConstructor ? reinterpret_cast<void*>(&event_ctor) : nullptr;
#undef BIND
  return nullptr;
}
/** Every fake object is its own class; its name is what production checks. */
Class* class_of(const void* object) { return const_cast<void*>(object); }
const char* class_name(Class* klass) { return nodes.count(klass) ? static_cast<Node*>(klass)->className : nullptr; }
bool is_class(const void* object, const char* name) {
  const char* actual = object ? class_name(class_of(object)) : nullptr;
  return actual && std::strcmp(actual, name) == 0;
}
Object* object_new(Class*) { return eventAllocation ? reinterpret_cast<Object*>(node()) : nullptr; }
/** Stable named metadata handles for the touched fields. */
Field* field_info(Class*, const char* name) {
  const char* names[] = {"readyToggle", "readyText", "onValueChanged", "m_Group", "graphic", "photonPlayer", "enabledColour"};
  for (int i = 0; i < 7; ++i) if (std::strcmp(name, names[i]) == 0) return reinterpret_cast<Field*>(static_cast<uintptr_t>(i + 1));
  return nullptr;
}
void* reference_field(void* object, Field* field) {
  if (!object || !field) return nullptr;
  auto* item = static_cast<Node*>(object);
  switch (reinterpret_cast<uintptr_t>(field)) {
    case 1: return fixtureReady;
    case 2: ++labelReads; return fixtureReadyLabel;
    case 4: return item->group;
    case 5: return item->graphic;
    case 6: return item->photonPlayer;
    default: return item->event;
  }
}
void read_field(void* object, Field* field, void* out) {
  if (object == fixtureManager && field == reinterpret_cast<Field*>(7)) *static_cast<pg::ui::Color*>(out) = kEnabledColour;
}
void set_reference_field(void* object, Field* field, void* value) {
  auto* item = static_cast<Node*>(object);
  if (field == reinterpret_cast<Field*>(4)) item->group = static_cast<Node*>(value);
  else if (field == reinterpret_cast<Field*>(5)) item->graphic = static_cast<Node*>(value);
  else item->event = static_cast<Node*>(value);
}
void to_utf8(const String*, char* result, size_t length) { if (length) *result = 0; }
}
namespace pg::game {
void* instantiate(void* original, void* parent, bool world) { return ::instantiate(original, parent, world, nullptr); }
void set_local_scale(void* value, Vector3 scale) { set_scale(value, scale, nullptr); }
bool private_room() { return privateRoom; }
int local_actor() { return localActor; }
int actor_number(void* player) { return static_cast<Node*>(player)->actor; }
/** Read synthetic destroyed-object state as Unity's implicit bool does. */
bool alive(void* value) { auto found = nodes.find(value); return found != nodes.end() && found->second->alive; }
/** Components share an identity with their fake transform. */
void* transform_of(void* value) {
  if (!value) return nullptr;
  CHECK(!static_cast<Node*>(value)->isGameObject);
  return static_cast<Node*>(value)->isGameObject ? nullptr : value;
}
void* game_object_of(void* value) { auto* item = static_cast<Node*>(value); return item->gameObject ? item->gameObject : item; }
/** The only component production looks up by type is the label's localiser. */
void* get_component(void* value, il2cpp::Object*) { return static_cast<Node*>(value)->localiser; }
void* add_component(void* value, il2cpp::Object*) {
  auto* go = static_cast<Node*>(value); CHECK(go->isGameObject);
  go->component = node(); go->component->gameObject = go; go->component->active = go->active;
  go->component->owned = true; return go->component;
}
/** Unity IsChildOf includes the queried transform itself. */
bool is_child_of(void* value, void* ancestor) {
  for (auto* item = static_cast<Node*>(value); item; item = item->parent) if (item == ancestor) return true;
  return false;
}
void* new_game_object(const char*) { auto* result = node(); result->owned = true; result->isGameObject = true; return result; }
/** Reparenting can activate a checkbox and therefore must happen after isolation. */
void set_parent(void* value, void* parent, bool) {
  if (!value) return;
  auto* item = static_cast<Node*>(value); item->parent = static_cast<Node*>(parent); enable_effects(item); check_localisers();
}
void set_enabled(void* value, bool on) { auto* item = static_cast<Node*>(value); item->enabled = on; enable_effects(item); }
void set_active(void* value, bool on) {
  auto* item = static_cast<Node*>(value); item->active = on;
  if (item->component) item->component->active = on;
  enable_effects(item); check_localisers();
}
/** Destroy an owned hierarchy as Unity does. */
void destroy(void* value) {
  auto* item = static_cast<Node*>(value); item->alive = false;
  if (item->component && item->component->alive) destroy(item->component);
  for (auto& [_, child] : nodes) if (child->parent == item && child->alive) destroy(child.get());
}
}
namespace pg::ui {
void destroy(void* value) { if (game::alive(value)) game::destroy(value); }
void set_text(void* value, const char* text) { static_cast<Node*>(value)->text = text; }
void set_color(void* value, Color color) { static_cast<Node*>(value)->color = color; }
void place(void* value, Vec2, Vec2, Vec2 position, Vec2 size) {
  auto* item = static_cast<Node*>(value); item->position = position; item->size = size;
}
float font_size(void* value) { return static_cast<Node*>(value)->fontSize; }
void set_word_wrap(void* value, bool on) { static_cast<Node*>(value)->wrap = on; }
void set_auto_size(void* value, float min, float max) {
  auto* item = static_cast<Node*>(value); item->fontMin = min; item->fontMax = max; item->autoSize = true;
}
}
namespace pg::log {
void write(const char* format, ...) {
  char text[1024]; va_list args; va_start(args, format); vsnprintf(text, sizeof text, format, args); va_end(args); logs.emplace_back(text);
}
}
namespace {
/** An ID card showing the given actor, as ServerManager's card list holds it. */
Node* card_for(int actor) {
  auto* card = node("MultiplayerProfile"); card->parent = board;
  auto* spot = node("PlayerSpot"); spot->photonPlayer = node("Player"); spot->photonPlayer->actor = actor;
  il2cpp::set_field(card, 0x60, spot);
  return card;
}
/** Point ServerManager's List<MultiplayerProfile> at these cards. */
void set_cards(std::vector<Node*> cards) {
  il2cpp::set_field<uintptr_t>(cardArray, 0x18, cards.size());
  for (size_t i = 0; i < cards.size(); ++i) il2cpp::set_field(cardArray, 0x20 + 8 * i, cards[i]);
  il2cpp::set_field<int32_t>(cardList, 0x18, static_cast<int32_t>(cards.size()));
}
/** A host lobby: native Ready and its separate localised label, and two ID cards (theirs first). */
pg::session::Session reset() {
  pg::lobby::teardown();
  for (auto& [_, value] : nodes) value->alive = false;
  fixtureClock += 10000; privateRoom = eventAllocation = eventConstructor = instantiateOk = true; localActor = 1;
  instantiated = nativeCallbacks = nativeGroupEffects = labelReads = liveLocalisers = 0; constructorThrows = labelCloneFails = false; logs.clear();
  board = node(); board->size = {100, 100};
  fixtureManager = node("ServerManager");
  fixtureReady = node("Toggle"); fixtureReady->parent = board; fixtureReady->position = {0.0486f, -0.0883f};
  fixtureReadyLabel = node("TextMeshProUGUI"); fixtureReadyLabel->label = true; fixtureReadyLabel->parent = board;
  fixtureReadyLabel->position = {0.04852f, -0.0887f}; fixtureReadyLabel->size = {60.66f, 21.14f};
  fixtureReadyLabel->localiser = node("TextLocaliserUI");
  nativeEvent = node(); nativeGroup = node(); fixtureReady->event = nativeEvent; fixtureReady->group = nativeGroup;
  fixtureReady->targetGraphic = fixtureReadyLabel;
  fixtureReady->graphic = node("Image"); fixtureReady->graphic->parent = fixtureReady;
  cardList = node("List`1"); cardArray = node("MultiplayerProfile[]");
  il2cpp::set_field(cardList, 0x10, cardArray); il2cpp::set_field(fixtureManager, 0xC8, cardList);
  otherCard = card_for(2); localCard = card_for(1);
  set_cards({otherCard, localCard});
  CHECK(pg::lobby::resolve());
  pg::session::Session session; session.observe(1, 1, {1, 2}, fixtureClock); session.take_outgoing(); return session;
}
/** Completed cloned checkboxes, found through fake Unity state rather than production internals. */
std::vector<Node*> controls() {
  std::vector<Node*> result;
  for (auto& [_, value] : nodes) if (value->alive && value->owned && value->event) result.push_back(value.get());
  return result;
}
Node* label_of(Node* toggle) {
  for (auto& [_, value] : nodes) if (value->alive && value->label && value->parent == toggle) return value.get();
  return nullptr;
}
bool close_to(float a, float b) { return std::abs(a - b) < 0.000001f; }

/** One isolated tape on the local player's own card, even for the host. */
void tape_on_own_card() {
  auto session = reset(); pg::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1);
  if (clones.size() != 1) return;
  Node* tape = clones[0];
  CHECK(tape->parent == localCard);
  CHECK(close_to(tape->scale.x, 0.36f) && close_to(tape->scale.y, 0.36f) && close_to(tape->scale.z, 1));
  CHECK(close_to(tape->position.x, 0) && close_to(tape->position.y, -0.0228f));
  // Below the XP bar (-0.0184) and above the next row: the grid cell is 0.041 and the row gap 0.005
  // board units, and the card is scaled 0.95, so the next cell starts 0.0255 / 0.95 below the centre.
  CHECK(tape->position.y + 0.011f * tape->scale.y <= -0.0184f);
  CHECK(tape->position.y - 0.011f * tape->scale.y >= -0.0255f / 0.95f);
  CHECK(tape->interactable && tape->enabled && hierarchy(tape));
  CHECK(tape->event != nativeEvent && tape->group == nullptr && tape->graphic != fixtureReady->graphic);
  CHECK(!tape->on && !session.volunteer());
  CHECK(nativeGroupEffects == 0 && nativeCallbacks == 0);
  CHECK(fixtureReady->on && !fixtureReady->interactable && fixtureReady->event == nativeEvent && fixtureReady->group == nativeGroup);
  Node* label = label_of(tape); CHECK(label);
  if (!label) return;
  CHECK(tape->targetGraphic == label);
  CHECK(label->text == "Play as ghost" && fixtureReadyLabel->text == "Ready");
  CHECK(close_to(label->color.r, 0.1f) && close_to(label->color.g, 0.2f) && close_to(label->color.b, 0.3f));
  CHECK(close_to(fixtureReadyLabel->color.r, 0.4f));  // Ready's own label is left alone
  CHECK(close_to(label->position.x, 0.04852f - 0.0486f) && close_to(label->position.y, -0.0887f + 0.0883f));
  CHECK(label->localiser && label->localiser != fixtureReadyLabel->localiser && !label->localiser->enabled);
  CHECK(liveLocalisers == 0);  // disabled before the label was ever visible
  CHECK(!staging_left());
  CHECK(fixtureReadyLabel->localiser->enabled);
  CHECK(label->autoSize && !label->wrap && close_to(label->fontMax, 36) && close_to(label->fontMin, 36 * 0.3f));
  CHECK(label->enabled && hierarchy(label));
}
/** The tape, its label, its card and the lobby's manager stay reachable for the GC while kept. */
void kept_objects_are_held() {
  auto session = reset(); pg::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1);
  if (clones.empty()) return;
  Node* label = label_of(clones[0]);
  CHECK(il2cpp::held(clones[0]) && il2cpp::held(label) && il2cpp::held(localCard) && il2cpp::held(fixtureManager));
  pg::lobby::teardown();
  CHECK(!il2cpp::held(clones[0]) && !il2cpp::held(label) && !il2cpp::held(localCard) && !il2cpp::held(fixtureManager));
}
/** Clicking volunteers; once drawn the tape says so; leaving the private room removes it. */
void click_and_draw() {
  auto session = reset(); pg::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1);
  if (clones.empty()) return;
  user_click(clones[0]); pg::lobby::tick(session);
  CHECK(session.volunteer() && clones[0]->on && nativeCallbacks == 0);
  fixtureClock += 100; session.observe(1, 1, {1, 2}, fixtureClock);
  CHECK(!session.local_is_ghost());  // actor 2 has not been heard from: they may not have the mod
  fixtureClock += 600; pg::lobby::tick(session);
  Node* label = label_of(clones[0]); CHECK(label && label->text == "Everyone needs the mod");
  const uint8_t hello[] = {'P', 'G', pg::session::kVersion, pg::session::kHello, 0};
  session.receive(2, hello, sizeof hello, fixtureClock);
  fixtureClock += 100; session.observe(1, 1, {1, 2}, fixtureClock);
  CHECK(session.local_is_ghost());
  fixtureClock += 600; pg::lobby::tick(session);
  CHECK(label && label->text == "You are the ghost");
  user_click(clones[0]); pg::lobby::tick(session); CHECK(!session.volunteer());
  privateRoom = false; pg::lobby::tick(session); CHECK(controls().empty());
  privateRoom = true; fixtureClock += 1000; pg::lobby::tick(session); CHECK(controls().size() == 1);
}
/** The tape follows this player when the lobby moves them to another card, and waits for one. */
void follows_card() {
  auto session = reset(); pg::lobby::tick(session);
  Node* moved = card_for(1); set_cards({moved, otherCard});
  fixtureClock += 1000; pg::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1 && clones[0]->parent == moved);
  set_cards({otherCard}); fixtureClock += 1000; pg::lobby::tick(session);
  CHECK(controls().empty());
  const size_t messages = logs.size();
  for (int frame = 0; frame < 5; ++frame) { fixtureClock += 1000; pg::lobby::tick(session); }
  CHECK(logs.size() == messages);  // "no ID card yet" is logged once, not every frame
  set_cards({otherCard, localCard}); fixtureClock += 1000; pg::lobby::tick(session);
  clones = controls(); CHECK(clones.size() == 1 && clones[0]->parent == localCard);
}
/** Incomplete clones are rolled back; retries are once a second and log only new failures. */
void failure_regression() {
  for (int mode = 0; mode < 7; ++mode) {
    auto session = reset();
    if (mode == 0) fixtureReadyLabel = nullptr;
    if (mode == 1) eventAllocation = false;
    if (mode == 2) eventConstructor = false;
    if (mode == 3) instantiateOk = false;
    if (mode == 4) labelCloneFails = true;
    if (mode == 5) constructorThrows = true;
    if (mode == 6) fixtureReady->graphic = fixtureReadyLabel;
    const size_t before = logs.size();
    pg::lobby::tick(session); CHECK(controls().empty() && !staging_left());
    CHECK(labelReads == 1 && logs.size() > before);  // one attempt, and its failure is logged
    const size_t messages = logs.size();
    for (int frame = 0; frame < 10; ++frame) { fixtureClock += 10; pg::lobby::tick(session); }
    CHECK(labelReads == 1 && logs.size() == messages);  // no retry within the second
    fixtureClock += 1000; pg::lobby::tick(session);
    CHECK(labelReads == 2);  // retried after a second
    CHECK(controls().empty() && !staging_left() && logs.size() == messages);  // same failure, not logged again
  }
}
/** A checked offset that stops holding what it should is named in the log, once. */
void wrong_offsets_are_named() {
  for (int mode = 0; mode < 2; ++mode) {
    auto session = reset();
    if (mode == 0) cardList->className = "Dictionary`2";
    if (mode == 1) static_cast<Node*>(il2cpp::field<void*>(localCard, 0x60))->className = "Player";
    for (int frame = 0; frame < 3; ++frame) { fixtureClock += 1000; pg::lobby::tick(session); }
    CHECK(controls().empty() && labelReads == 0);
    int named = 0;
    for (auto& line : logs) named += line.find(mode ? "+0x60 is not a PlayerSpot" : "+0xC8 is not the ID card list") != std::string::npos;
    CHECK(named == 1);
  }
}
}
/** Run deterministic regressions against the production lobby implementation. */
int main() {
  tape_on_own_card(); kept_objects_are_held(); click_and_draw(); follows_card(); failure_regression(); wrong_offsets_are_named();
  pg::lobby::teardown(); std::printf("lobby: %d failures\n", failures); return failures ? 1 : 0;
}
