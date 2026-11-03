#include <windows.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "JournalController.h"
#include "Player.h"
#include "ServerManager.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "fake_game.h"
#include "game.h"
#include "text_fixture.h"
#include "ui.h"

// Offline test of the lobby's "Play as ghost" tape (mods/playable_ghost/lobby.cpp). The game is the fake
// backend: its fields sit in each fake object's own slots (below), its methods are the fakes here, and core's
// helpers the file still uses are modelled here too.
namespace {
int failures = 0;
#define CHECK(value) do { if (!(value)) { std::printf("FAIL line %d: %s\n", __LINE__, #value); ++failures; } } while (0)
// The test's own offsets for the fields the game API reads; the fake backend reads them in the object itself.
constexpr int32_t kEvent = 0x10;         // Toggle.onValueChanged
constexpr int32_t kGroup = 0x18;         // Toggle.m_Group
constexpr int32_t kGraphic = 0x20;       // Toggle.graphic
constexpr int32_t kPhotonPlayer = 0x28;  // PlayerSpot.photonPlayer
constexpr int32_t kSpot = 0x30;          // ProfileCard.spot
constexpr int32_t kReady = 0x38;         // ServerManager.readyToggle
constexpr int32_t kReadyText = 0x40;     // ServerManager.readyText
constexpr int32_t kCards = 0x48;         // ServerManager.cards
constexpr int32_t kColour = 0x50;        // ServerManager.enabledColour, a Color
// One fake Unity or managed object: its class first, as every managed object's, then its field slots.
struct Node;
// The fake classes: the type checks compare an object's first word with these.
struct FakeClass {
  void* klass = nullptr;
} toggleEventClass, profileClass, spotClass, serverManagerClass, rectClass, localiserClass, otherClass;
struct Node {
  void* klass = &otherClass;
  alignas(8) char fields[0x60]{};
  /** The reference held in the field slot at `offset`. */
  Node*& ref(int32_t offset) { return *reinterpret_cast<Node**>(reinterpret_cast<char*>(this) + offset); }
  const char* className = "";
  Node* parent = nullptr;
  Node* targetGraphic = nullptr;
  Node* gameObject = nullptr;
  Node* component = nullptr;
  Node* localiser = nullptr;
  int actor = 0;
  bool isGameObject = false;
  bool label = false, active = true, enabled = true, interactable = false, on = true;
  bool alive = true, owned = false;
  bool autoSize = false, wrap = true;
  float fontSize = 36, fontMin = 0, fontMax = 0;
  pm::ui::Vec2 position{0, 0}, size{0.0666f, 0.022f};
  phasmo::Vector3 scale{1, 1, 1};
  pm::ui::Color color{0.4f, 0.4f, 0.4f, 1};  // ServerManager's grey while Ready cannot be clicked
  std::string text = "Ready";
};
std::unordered_map<void*, std::unique_ptr<Node>> nodes;
Node *fixtureManager, *fixtureReady, *fixtureReadyLabel, *board, *nativeEvent, *nativeGroup;
Node *localCard, *otherCard;
ULONGLONG fixtureClock = 0;
const pm::ui::Color kEnabledColour{0.1f, 0.2f, 0.3f, 1};  // ServerManager.enabledColour
int localActor = 1;
bool privateRoom = true, instantiateOk = true, constructorThrows = false;
bool labelCloneFails = false;
int instantiated = 0, nativeCallbacks = 0, nativeGroupEffects = 0;
int labelReads = 0;      // clone attempts: each first asks whether ServerManager.readyText is alive
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
  if (hierarchy(value) && value->enabled && value->ref(kGroup) && value->on) {
    ++nativeGroupEffects;
    if (value->ref(kEvent) == nativeEvent) ++nativeCallbacks;
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
void user_click(Node* value) { value->on = !value->on; if (value->ref(kEvent) == nativeEvent) ++nativeCallbacks; }
/** Return deterministic monotonic time without driving the game. */
ULONGLONG WINAPI clock_ms() { return fixtureClock; }
/** Return the scene's native ServerManager. */
void* find_manager(void*, bool, const void*) { return fixtureManager; }
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
  if (result->ref(kGraphic) && result->ref(kGraphic)->parent == original) {
    auto* graphic = node(); *graphic = *result->ref(kGraphic);
    graphic->parent = result; graphic->owned = true; result->ref(kGraphic) = graphic;
  }
  if (result->localiser) {
    auto* localiser = node(); *localiser = *result->localiser; localiser->owned = true; result->localiser = localiser;
  }
  enable_effects(result);
  return result;
}
void* parent_of(void* value, const void*) { return static_cast<Node*>(value)->parent; }
void set_scale(void* value, phasmo::Vector3 scale, const void*) { static_cast<Node*>(value)->scale = scale; }
bool is_on(void* value, const void*) { return static_cast<Node*>(value)->on; }
/** Model Toggle's silent setter, which still affects its native group. */
void set_on(void* value, bool state, const void*) {
  auto* item = static_cast<Node*>(value);
  if (item->ref(kGroup) && state) ++nativeGroupEffects;
  item->on = state;
}
void set_interactable(void* value, bool state, const void*) { static_cast<Node*>(value)->interactable = state; }
/** A native Selectable can reference the externally owned Ready text. */
void* target_graphic(void* value, const void*) { return static_cast<Node*>(value)->targetGraphic; }
void set_target_graphic(void* value, void* graphic, const void*) { static_cast<Node*>(value)->targetGraphic = static_cast<Node*>(graphic); }
phasmo::Vector2 position_of(void* value, const void*) {
  const pm::ui::Vec2 p = static_cast<Node*>(value)->position;
  return {p.x, p.y};
}
void set_position(void* value, phasmo::Vector2 position, const void*) {
  static_cast<Node*>(value)->position = {position.x, position.y};
}
phasmo::Rect rect_of(void* value, const void*) {
  auto* item = static_cast<Node*>(value);
  return {-item->size.x / 2, -item->size.y / 2, item->size.x, item->size.y};
}
bool active(void* value, const void*) { return static_cast<Node*>(value)->active; }
bool active_hierarchy(void* value, const void*) { return hierarchy(static_cast<Node*>(value)); }
bool enabled(void* value, const void*) { return static_cast<Node*>(value)->enabled; }
bool interactable(void* value, const void*) { return static_cast<Node*>(value)->interactable; }
void* name_of(void*, const void*) { return nullptr; }
/** Construct an independent UnityEvent with no copied listeners. */
void event_ctor(void*, const void*) {
  if (constructorThrows) throw 1;
}
/** The only component production looks up by type is the label's localiser. */
void* get_component(void* value, void* type, const void*) {
  return type == phasmo::TextLocaliserUI::type.type_object() ? static_cast<Node*>(value)->localiser : nullptr;
}
/** The staging object's RectTransform. */
void* add_component(void* value, void* type, const void*) {
  auto* go = static_cast<Node*>(value);
  CHECK(go->isGameObject && type == phasmo::RectTransform::type.type_object());
  go->component = node(); go->component->gameObject = go; go->component->active = go->active;
  go->component->owned = true; return go->component;
}
}
#define GetTickCount64 clock_ms
#include "../mods/playable_ghost/lobby.cpp"
#undef GetTickCount64

namespace pm::game {
void* instantiate(void* original, void* parent, bool world) { return ::instantiate(original, parent, world, nullptr); }
void set_local_scale(void* value, Vector3 scale) { set_scale(value, scale, nullptr); }
bool private_room() { return privateRoom; }
int local_actor() { return localActor; }
int actor_number(void* player) { return static_cast<Node*>(player)->actor; }
/** Read synthetic destroyed-object state as Unity's implicit bool does. */
bool alive(void* value) {
  // A clone attempt asks first whether Ready's label is alive; nothing else asks of it.
  if (value && fixtureManager && value == fixtureManager->ref(kReadyText)) ++labelReads;
  auto found = nodes.find(value);
  return found != nodes.end() && found->second->alive;
}
/** Components share an identity with their fake transform. */
void* transform_of(void* value) {
  if (!value) return nullptr;
  CHECK(!static_cast<Node*>(value)->isGameObject);
  return static_cast<Node*>(value)->isGameObject ? nullptr : value;
}
void* game_object_of(void* value) { auto* item = static_cast<Node*>(value); return item->gameObject ? item->gameObject : item; }
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
namespace pm::ui {
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
namespace pm::log {
void write(const char* format, ...) {
  char text[1024]; va_list args; va_start(args, format); vsnprintf(text, sizeof text, format, args); va_end(args); logs.emplace_back(text);
}
}
namespace {
/** An ID card showing the given actor, as ServerManager's card list holds it. */
Node* card_for(int actor) {
  auto* card = node("MultiplayerProfile"); card->parent = board; card->klass = &profileClass;
  auto* spot = node("PlayerSpot"); spot->klass = &spotClass;
  spot->ref(kPhotonPlayer) = node("Player"); spot->ref(kPhotonPlayer)->actor = actor;
  card->ref(kSpot) = spot;
  return card;
}
/** Point ServerManager's List<MultiplayerProfile> at these cards. */
void set_cards(std::vector<Node*> cards) {
  fixtureManager->ref(kCards) = static_cast<Node*>(pm::fake::list({cards.begin(), cards.end()}));
}
/** Gives every entry lobby.cpp uses, at the test's own offsets and fakes, and binds the game API to them. */
void give_entries() {
  namespace fake = pm::fake;
  namespace map = pm::map;
  fake::reset();
  fake::type(map::ServerManager, &serverManagerClass);
  fake::type(map::MultiplayerProfile, &profileClass);
  fake::type(map::PlayerSpot, &spotClass);
  fake::type(map::ToggleEvent, &toggleEventClass);
  fake::type(map::RectTransform, &rectClass);
  fake::type(map::TextLocaliserUI, &localiserClass);
  fake::field(map::ServerManager_readyToggle, kReady);
  fake::field(map::ServerManager_readyText, kReadyText);
  fake::field(map::ServerManager_cards, kCards);
  fake::field(map::ServerManager_enabledColour, kColour);
  fake::field(map::ProfileCard_spot, kSpot);
  fake::field(map::PlayerSpot_photonPlayer, kPhotonPlayer);
  fake::field(map::Toggle_onValueChanged, kEvent);
  fake::field(map::Toggle_m_Group, kGroup);
  fake::field(map::Toggle_graphic, kGraphic);
  fake::method(map::UnityObject_FindObjectOfType, reinterpret_cast<void*>(&find_manager));
  fake::method(map::UnityObject_get_name, reinterpret_cast<void*>(&name_of));
  fake::method(map::Transform_get_parent, reinterpret_cast<void*>(&parent_of));
  fake::method(map::Toggle_get_isOn, reinterpret_cast<void*>(&is_on));
  fake::method(map::Toggle_SetIsOnWithoutNotify, reinterpret_cast<void*>(&set_on));
  fake::method(map::Selectable_set_interactable, reinterpret_cast<void*>(&set_interactable));
  fake::method(map::Selectable_get_interactable, reinterpret_cast<void*>(&interactable));
  fake::method(map::Selectable_get_targetGraphic, reinterpret_cast<void*>(&target_graphic));
  fake::method(map::Selectable_set_targetGraphic, reinterpret_cast<void*>(&set_target_graphic));
  fake::method(map::RectTransform_get_anchoredPosition, reinterpret_cast<void*>(&position_of));
  fake::method(map::RectTransform_set_anchoredPosition, reinterpret_cast<void*>(&set_position));
  fake::method(map::RectTransform_get_rect, reinterpret_cast<void*>(&rect_of));
  fake::method(map::GameObject_get_activeSelf, reinterpret_cast<void*>(&active));
  fake::method(map::GameObject_get_activeInHierarchy, reinterpret_cast<void*>(&active_hierarchy));
  fake::method(map::Behaviour_get_enabled, reinterpret_cast<void*>(&enabled));
  fake::method(map::ToggleEvent_ctor, reinterpret_cast<void*>(&event_ctor));
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&get_component));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&add_component));
  std::string missing;
  CHECK(pm::api::bind(fake::host(), missing));
  if (!missing.empty()) std::printf("missing: %s\n", missing.c_str());
}
/** A host lobby: native Ready and its separate localised label, and two ID cards (theirs first). */
pm::session::Session reset() {
  pm::lobby::teardown();
  for (auto& [_, value] : nodes) value->alive = false;
  fixtureClock += 10000; privateRoom = instantiateOk = true; localActor = 1;
  instantiated = nativeCallbacks = nativeGroupEffects = labelReads = liveLocalisers = 0; constructorThrows = labelCloneFails = false; logs.clear();
  board = node(); board->size = {100, 100};
  give_entries();
  fixtureManager = node("ServerManager"); fixtureManager->klass = &serverManagerClass;
  fixtureReady = node("Toggle"); fixtureReady->parent = board; fixtureReady->position = {0.0486f, -0.0883f};
  fixtureReadyLabel = node("TextMeshProUGUI"); fixtureReadyLabel->label = true; fixtureReadyLabel->parent = board;
  fixtureReadyLabel->position = {0.04852f, -0.0887f}; fixtureReadyLabel->size = {60.66f, 21.14f};
  fixtureReadyLabel->localiser = node("TextLocaliserUI");
  fixtureManager->ref(kReady) = fixtureReady; fixtureManager->ref(kReadyText) = fixtureReadyLabel;
  std::memcpy(fixtureManager->fields + (kColour - 8), &kEnabledColour, sizeof kEnabledColour);
  nativeEvent = node(); nativeEvent->klass = &toggleEventClass; nativeGroup = node(); fixtureReady->ref(kEvent) = nativeEvent; fixtureReady->ref(kGroup) = nativeGroup;
  fixtureReady->targetGraphic = fixtureReadyLabel;
  fixtureReady->ref(kGraphic) = node("Image"); fixtureReady->ref(kGraphic)->parent = fixtureReady;
  otherCard = card_for(2); localCard = card_for(1);
  set_cards({otherCard, localCard});
  CHECK(pm::lobby::resolve());
  pm::session::Session session; session.observe(1, 1, {1, 2}, fixtureClock); session.take_outgoing(); return session;
}
/** Completed cloned checkboxes, found through fake Unity state rather than production internals. */
std::vector<Node*> controls() {
  std::vector<Node*> result;
  for (auto& [_, value] : nodes) if (value->alive && value->owned && value->ref(kEvent)) result.push_back(value.get());
  return result;
}
Node* label_of(Node* toggle) {
  for (auto& [_, value] : nodes) if (value->alive && value->label && value->parent == toggle) return value.get();
  return nullptr;
}
bool close_to(float a, float b) { return std::abs(a - b) < 0.000001f; }

/** One isolated tape on the local player's own card, even for the host. */
void tape_on_own_card() {
  auto session = reset(); pm::lobby::tick(session);
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
  CHECK(tape->ref(kEvent) != nativeEvent && tape->ref(kGroup) == nullptr && tape->ref(kGraphic) != fixtureReady->ref(kGraphic));
  CHECK(!tape->on && !session.volunteer());
  CHECK(nativeGroupEffects == 0 && nativeCallbacks == 0);
  CHECK(fixtureReady->on && !fixtureReady->interactable && fixtureReady->ref(kEvent) == nativeEvent && fixtureReady->ref(kGroup) == nativeGroup);
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
  auto session = reset(); pm::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1);
  if (clones.empty()) return;
  Node* label = label_of(clones[0]);
  CHECK(pm::fake::held(clones[0]) && pm::fake::held(label) && pm::fake::held(localCard) && pm::fake::held(fixtureManager));
  pm::lobby::teardown();
  CHECK(!pm::fake::held(clones[0]) && !pm::fake::held(label) && !pm::fake::held(localCard) && !pm::fake::held(fixtureManager));
}
/** Clicking volunteers; once drawn the tape says so; leaving the private room removes it. */
void click_and_draw() {
  auto session = reset(); pm::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1);
  if (clones.empty()) return;
  user_click(clones[0]); pm::lobby::tick(session);
  CHECK(session.volunteer() && clones[0]->on && nativeCallbacks == 0);
  fixtureClock += 100; session.observe(1, 1, {1, 2}, fixtureClock);
  CHECK(session.local_is_ghost());  // the mod checks nobody: the loader keeps the room to the host's list
  fixtureClock += 600; pm::lobby::tick(session);
  Node* label = label_of(clones[0]); CHECK(label && label->text == "You are the ghost");
  user_click(clones[0]); pm::lobby::tick(session); CHECK(!session.volunteer());
  privateRoom = false; pm::lobby::tick(session); CHECK(controls().empty());
  privateRoom = true; fixtureClock += 1000; pm::lobby::tick(session); CHECK(controls().size() == 1);
}
/** The tape follows this player when the lobby moves them to another card, and waits for one. */
void follows_card() {
  auto session = reset(); pm::lobby::tick(session);
  Node* moved = card_for(1); set_cards({moved, otherCard});
  fixtureClock += 1000; pm::lobby::tick(session);
  auto clones = controls(); CHECK(clones.size() == 1 && clones[0]->parent == moved);
  set_cards({otherCard}); fixtureClock += 1000; pm::lobby::tick(session);
  CHECK(controls().empty());
  const size_t messages = logs.size();
  for (int frame = 0; frame < 5; ++frame) { fixtureClock += 1000; pm::lobby::tick(session); }
  CHECK(logs.size() == messages);  // "no ID card yet" is logged once, not every frame
  set_cards({otherCard, localCard}); fixtureClock += 1000; pm::lobby::tick(session);
  clones = controls(); CHECK(clones.size() == 1 && clones[0]->parent == localCard);
}
/** Incomplete clones are rolled back; retries are once a second and log only new failures. */
void failure_regression() {
  for (int mode = 0; mode < 9; ++mode) {
    auto session = reset();
    if (mode == 7) {  // the runtime cannot allocate the new event
      static PmHostApi noObjects;
      noObjects = *pm::fake::host();
      noObjects.object_new = [](const char*) -> void* { return nullptr; };
      std::string missing;
      CHECK(pm::api::bind(&noObjects, missing));
    }
    if (mode == 8) {  // a build without the event's constructor: the mod starts, only the tape is lost
      pm::fake::forget(pm::map::ToggleEvent_ctor);
      std::string missing;
      CHECK(pm::api::bind(pm::fake::host(), missing) && missing.empty());
    }
    if (mode == 0) fixtureReadyLabel->alive = false;  // Ready's label was destroyed
    if (mode == 1) nativeEvent->klass = &otherClass;  // Ready's event is no ToggleEvent
    if (mode == 2) fixtureReady->ref(kEvent) = nullptr;  // Ready has no event
    if (mode == 3) instantiateOk = false;
    if (mode == 4) labelCloneFails = true;
    if (mode == 5) constructorThrows = true;
    if (mode == 6) fixtureReady->ref(kGraphic) = fixtureReadyLabel;
    const size_t before = logs.size();
    pm::lobby::tick(session); CHECK(controls().empty() && !staging_left());
    CHECK(labelReads == 1 && logs.size() > before);  // one attempt, and its failure is logged
    const size_t messages = logs.size();
    for (int frame = 0; frame < 10; ++frame) { fixtureClock += 10; pm::lobby::tick(session); }
    CHECK(labelReads == 1 && logs.size() == messages);  // no retry within the second
    fixtureClock += 1000; pm::lobby::tick(session);
    CHECK(labelReads == 2);  // retried after a second
    CHECK(controls().empty() && !staging_left() && logs.size() == messages);  // same failure, not logged again
  }
}
/** A card list or card that stops holding what it should is named in the log, once. (The list's own type is the
 *  resolver's to check: ServerManager.cards is a List<MultiplayerProfile> or it does not resolve.) */
void wrong_contents_are_named() {
  for (int mode = 0; mode < 2; ++mode) {
    auto session = reset();
    if (mode == 0) localCard->klass = &otherClass;
    if (mode == 1) localCard->ref(kSpot)->klass = &otherClass;
    for (int frame = 0; frame < 3; ++frame) { fixtureClock += 1000; pm::lobby::tick(session); }
    CHECK(controls().empty() && labelReads == 0);
    int named = 0;
    for (auto& line : logs)
      named += line.find(mode ? "ProfileCard.spot is not a PlayerSpot"
                              : "the ID card list holds something other than MultiplayerProfile") != std::string::npos;
    CHECK(named == 1);
  }
}
}
/** Run deterministic regressions against the production lobby implementation. */
int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});  // the tape's labels are Playable Ghost's table's
  tape_on_own_card(); kept_objects_are_held(); click_and_draw(); follows_card(); failure_regression(); wrong_contents_are_named();
  pm::lobby::teardown(); std::printf("lobby: %d failures\n", failures); return failures ? 1 : 0;
}
