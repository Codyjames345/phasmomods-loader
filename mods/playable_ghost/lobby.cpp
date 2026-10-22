#include "lobby.h"

#include <windows.h>

#include <string>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "ui.h"

namespace pg::lobby {
namespace {

// Obfuscated field names, so by offset (build 25811160, verified at load); each read is checked
// against the class it should hold.
constexpr size_t kManagerCards = 0xC8;  // ServerManager: List<MultiplayerProfile>, the lobby's ID cards
constexpr size_t kCardSpot = 0x60;      // ProfileCard: Network.PlayerSpot shown on the card

// The game's Ready tape, small, stuck across the bottom edge of the player's own ID card just below
// its XP bar. Card units (scene level2): the face is 0.0767 x 0.0410 about the origin and the XP bar
// spans y -0.0184..-0.0143; the card grid leaves 0.005 board units (card scale 0.95) between rows.
// At 0.36 the 0.022-tall tape spans y -0.0188..-0.0268, clearing the XP bar above and the next row
// of cards (and, for the bottom row, the Leave/Debrief buttons) below.
constexpr float kTapeScale = 0.36f;
constexpr ui::Vec2 kTapePosition = {0.0f, -0.0228f};
constexpr float kMinFontFraction = 0.3f;  // "Play as ghost" auto-sizes down to this share of Ready's font
constexpr ULONGLONG kRelabelMs = 500;     // in case a localisation component rewrites the label

struct Api {
  il2cpp::Object* serverManagerType = nullptr;
  il2cpp::Object* rectTransformType = nullptr;
  il2cpp::Object* localiserType = nullptr;  // TextLocaliserUI, optional
  il2cpp::Field* readyToggle = nullptr;
  il2cpp::Field* readyText = nullptr;
  il2cpp::Field* enabledColour = nullptr;  // ServerManager: Ready's text colour when it can be clicked
  il2cpp::Field* onValueChanged = nullptr;
  il2cpp::Field* group = nullptr;
  il2cpp::Field* graphic = nullptr;
  il2cpp::Field* photonPlayer = nullptr;  // PlayerSpot.photonPlayer, bound from the first spot seen
  void* (*findObjectOfType)(void*, const void*) = nullptr;
  void* (*getParent)(void*, const void*) = nullptr;
  bool (*isOn)(void*, const void*) = nullptr;
  void (*setIsOnSilently)(void*, bool, const void*) = nullptr;
  void (*setInteractable)(void*, bool, const void*) = nullptr;
  void* (*targetGraphic)(void*, const void*) = nullptr;
  void (*setTargetGraphic)(void*, void*, const void*) = nullptr;
  ui::Vec2 (*anchoredPosition)(void*, const void*) = nullptr;
  void (*setAnchoredPosition)(void*, ui::Vec2, const void*) = nullptr;
} api;

using ui::Rect;
struct Diagnostics {
  bool (*activeSelf)(void*, const void*) = nullptr;
  bool (*activeHierarchy)(void*, const void*) = nullptr;
  bool (*enabled)(void*, const void*) = nullptr;
  bool (*interactable)(void*, const void*) = nullptr;
  il2cpp::String* (*name)(void*, const void*) = nullptr;
  Rect (*rect)(void*, const void*) = nullptr;
  bool available = false;
} diagnostics;

struct Clone {
  il2cpp::Held toggle;
  il2cpp::Held label;
  il2cpp::Held card;  // the local player's MultiplayerProfile the tape is stuck to
  bool shownOn = false;
  std::string text;
};

Clone g_volunteer;
il2cpp::Held g_manager;
ULONGLONG g_nextSearch = 0;
ULONGLONG g_nextClone = 0;
ULONGLONG g_nextRelabel = 0;
int g_visibility = -1;  // the tape's last logged activeSelf + 2 * activeInHierarchy
bool g_photonPlayerLookedUp = false;  // the lookup logs its own failure, so it runs once
std::string g_failure;

/** Bind the existing Unity API, rejecting unavailable required methods. */
using il2cpp::bind;

/** Log a failure once until it changes, so retries stay quiet; true when it was logged. */
bool fail(const char* reason) {
  if (g_failure == reason) return false;
  g_failure = reason;
  PG_LOG("lobby: Play as ghost unavailable: %s", reason);
  return true;
}

/** Log a control's state and ancestry, for checking placement from pg.log alone. */
void describe_control(const char* label, void* toggle) {
  if (!diagnostics.available || !game::alive(toggle)) {
    PG_LOG("lobby: %s inspection unavailable (object=%p)", label, toggle);
    return;
  }
  PG_LOG("lobby: %s enabled=%d interactable=%d isOn=%d", label,
    diagnostics.enabled(toggle, nullptr), diagnostics.interactable(toggle, nullptr), api.isOn(toggle, nullptr));
  void* transform = game::transform_of(toggle);
  for (int depth = 0; transform && depth < 5; ++depth) {
    void* go = game::game_object_of(transform);
    char name[128];
    il2cpp::to_utf8(diagnostics.name(go, nullptr), name, sizeof name);
    PG_LOG("lobby: %s ancestor=%d name=%s activeSelf=%d activeHierarchy=%d", label, depth,
      name, diagnostics.activeSelf(go, nullptr), diagnostics.activeHierarchy(go, nullptr));
    if (depth == 0) {
      const Rect r = diagnostics.rect(transform, nullptr);
      const auto p = api.anchoredPosition(transform, nullptr);
      PG_LOG("lobby: %s rect=(%.6g,%.6g,%.6g,%.6g) pos=(%.6g,%.6g)", label, r.x, r.y, r.width, r.height, p.x, p.y);
    }
    transform = api.getParent(transform, nullptr);
  }
}

/**
 * The ID card showing this client's player, or null with `reason` saying why: the lobby has not
 * assigned one yet, or a checked offset no longer holds what it should.
 */
void* local_card(const char*& reason) {
  auto* cards = il2cpp::field<il2cpp::List*>(g_manager, kManagerCards);
  if (!cards) { reason = "ServerManager has no ID card list yet"; return nullptr; }
  if (!il2cpp::is_class(cards, "List`1") || !cards->items) { reason = "ServerManager+0xC8 is not the ID card list"; return nullptr; }
  const int local = game::local_actor();
  reason = "this player has no ID card in the lobby yet";
  for (int i = 0; i < cards->size && static_cast<uintptr_t>(i) < cards->items->max_length; ++i) {
    void* card = cards->items->data<void*>()[i];
    if (!game::alive(card)) continue;
    if (!il2cpp::is_class(card, "MultiplayerProfile")) { reason = "the ID card list holds something other than MultiplayerProfile"; return nullptr; }
    void* spot = il2cpp::field<void*>(card, kCardSpot);
    if (!spot) continue;  // a card with no player
    if (!il2cpp::is_class(spot, "PlayerSpot")) { reason = "ProfileCard+0x60 is not a PlayerSpot"; return nullptr; }
    if (!g_photonPlayerLookedUp) {
      g_photonPlayerLookedUp = true;
      api.photonPlayer = il2cpp::field_info(il2cpp::class_of(spot), "photonPlayer");
    }
    if (!api.photonPlayer) { reason = "PlayerSpot.photonPlayer is missing"; return nullptr; }
    void* player = il2cpp::reference_field(spot, api.photonPlayer);
    if (player && game::actor_number(player) == local) return card;
  }
  return nullptr;
}

/**
 * Clone Ready and its separate label inside an inactive staging object, isolate the copy from
 * Ready's listeners and group, then stick it to the card. Returns an empty Clone on failure.
 */
Clone make_clone(void* source, void* sourceLabel, const ui::Color* textColour, void* card, bool initialOn, const char*& failure) {
  Clone c;
  if (!game::alive(sourceLabel)) { failure = "native Ready label is unavailable"; return {}; }
  void* oldEvent = il2cpp::reference_field(source, api.onValueChanged);
  auto* eventClass = oldEvent ? il2cpp::class_of(oldEvent) : nullptr;
  auto ctor = reinterpret_cast<void (*)(void*, const void*)>(eventClass ? il2cpp::method_ptr(eventClass, ".ctor", 0) : nullptr);
  auto* fresh = ctor ? il2cpp::object_new(eventClass) : nullptr;
  if (!fresh) { failure = "independent ToggleEvent is unavailable"; return {}; }
  void* cardTransform = game::transform_of(card);
  void* staging = game::new_game_object("PlayableGhost lobby staging");
  if (!staging) { failure = "inactive staging object allocation failed"; return {}; }
  game::set_active(staging, false);
  float readySize = 0;
  try {
    void* stageRect = game::add_component(staging, api.rectTransformType);
    if (!stageRect) { failure = "inactive staging RectTransform is unavailable"; game::destroy(staging); return {}; }
    game::set_parent(stageRect, cardTransform, false);
    ui::place(stageRect, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {0, 0});
    ctor(fresh, nullptr);
    c.toggle = game::instantiate(source, stageRect, false);
    if (c.toggle) {
      // The inactive clone has never registered; the property setter would touch native group members.
      il2cpp::set_reference_field(c.toggle, api.group, nullptr);
      il2cpp::set_reference_field(c.toggle, api.onValueChanged, fresh);
      c.label = game::instantiate(sourceLabel, game::transform_of(c.toggle), false);
    }
    if (!c.toggle || !c.label) {
      failure = "native control or label cloning failed";
      game::destroy(staging);
      return {};
    }
    void* target = api.targetGraphic(c.toggle, nullptr);
    if (target == sourceLabel) api.setTargetGraphic(c.toggle, c.label, nullptr);
    else if (target && !game::is_child_of(game::transform_of(target), game::transform_of(c.toggle))) {
      failure = "cloned control references an external target graphic";
      game::destroy(staging);
      return {};
    }
    void* graphic = il2cpp::reference_field(c.toggle, api.graphic);
    if (graphic && !game::is_child_of(game::transform_of(graphic), game::transform_of(c.toggle))) {
      failure = "cloned toggle references an external graphic";
      game::destroy(staging);
      return {};
    }
    if (il2cpp::reference_field(c.toggle, api.onValueChanged) != fresh) {
      failure = "cloned ToggleEvent was not replaced"; game::destroy(staging); return {};
    }
    // Ready and its label are siblings centred on the board, so the label keeps its offset from Ready.
    const auto readyAt = api.anchoredPosition(game::transform_of(source), nullptr);
    const auto labelAt = api.anchoredPosition(game::transform_of(sourceLabel), nullptr);
    api.setAnchoredPosition(game::transform_of(c.label), {labelAt.x - readyAt.x, labelAt.y - readyAt.y}, nullptr);
    // The copied localiser would put "Ready" back as soon as the label is enabled.
    if (void* localiser = api.localiserType ? game::get_component(c.label, api.localiserType) : nullptr)
      game::set_enabled(localiser, false);
    // ServerManager greys Ready's label while Ready cannot be clicked; the tape always can.
    if (textColour) ui::set_color(c.label, *textColour);
    readySize = ui::font_size(c.label);
    ui::set_word_wrap(c.label, false);
    ui::set_auto_size(c.label, readySize * kMinFontFraction, readySize);
    api.setIsOnSilently(c.toggle, initialOn, nullptr);
    c.shownOn = initialOn;
    api.setInteractable(c.toggle, true, nullptr);
    game::set_enabled(c.toggle, true);
    game::set_enabled(c.label, true);
    game::set_active(game::game_object_of(c.label), true);
    game::set_active(game::game_object_of(c.toggle), true);
    void* tape = game::transform_of(c.toggle);
    game::set_local_scale(tape, {kTapeScale, kTapeScale, 1});
    api.setAnchoredPosition(tape, kTapePosition, nullptr);
    game::set_parent(tape, cardTransform, false);
  } catch (...) {
    failure = "native control initialization threw";
    ui::destroy(c.toggle);
    game::destroy(staging);
    return {};
  }
  game::destroy(staging);
  c.card = card;
  PG_LOG("lobby: Play as ghost on card %p (toggle=%p label=%p on=%d fontSize=%.3g)", card, c.toggle.get(), c.label.get(), initialOn, readySize);
  return c;
}

/** Remove the tape; its label is its child. */
void destroy_clone() {
  ui::destroy(g_volunteer.toggle);
  g_volunteer = Clone{};
  g_visibility = -1;
}

/** Refresh the tape's state and text without invoking native Ready listeners. */
void show(Clone& c, bool on, const std::string& text, bool relabel) {
  if (!game::alive(c.toggle)) return;
  api.setIsOnSilently(c.toggle, on, nullptr);
  c.shownOn = on;
  if (c.label && (relabel || text != c.text)) {
    c.text = text;
    ui::set_text(c.label, text.c_str());
  }
}

}  // namespace

/** Resolve required native controls by named metadata and existing Unity UI methods. */
bool resolve() {
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  il2cpp::Class* transform = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
  il2cpp::Class* rect = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "RectTransform");
  il2cpp::Class* toggle = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Toggle");
  auto* manager = il2cpp::cls("Assembly-CSharp.dll", "", "ServerManager");
  api.serverManagerType = il2cpp::type_object(manager);
  api.rectTransformType = il2cpp::type_object(rect);
  auto* localiser = il2cpp::cls("Assembly-CSharp.dll", "", "TextLocaliserUI");
  api.localiserType = localiser ? il2cpp::type_object(localiser) : nullptr;
  api.readyToggle = il2cpp::field_info(manager, "readyToggle");
  api.readyText = il2cpp::field_info(manager, "readyText");
  api.enabledColour = il2cpp::field_info(manager, "enabledColour");
  api.onValueChanged = il2cpp::field_info(toggle, "onValueChanged");
  api.group = il2cpp::field_info(toggle, "m_Group");
  api.graphic = il2cpp::field_info(toggle, "graphic");
  bool ok = api.serverManagerType && api.rectTransformType && api.readyToggle && api.readyText && api.onValueChanged && api.group && api.graphic;
  ok &= bind(api.findObjectOfType, object, "FindObjectOfType", 1);
  ok &= bind(api.getParent, transform, "get_parent", 0);
  ok &= bind(api.isOn, toggle, "get_isOn", 0);
  ok &= bind(api.setIsOnSilently, toggle, "SetIsOnWithoutNotify", 1);
  ok &= bind(api.setInteractable, toggle, "set_interactable", 1);
  ok &= bind(api.targetGraphic, toggle, "get_targetGraphic", 0);
  ok &= bind(api.setTargetGraphic, toggle, "set_targetGraphic", 1);
  ok &= bind(api.anchoredPosition, rect, "get_anchoredPosition", 0);
  ok &= bind(api.setAnchoredPosition, rect, "set_anchoredPosition", 1);
  auto* gameObject = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "GameObject");
  auto* behaviour = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Behaviour");
  diagnostics.available = bind(diagnostics.activeSelf, gameObject, "get_activeSelf", 0);
  diagnostics.available &= bind(diagnostics.activeHierarchy, gameObject, "get_activeInHierarchy", 0);
  diagnostics.available &= bind(diagnostics.enabled, behaviour, "get_enabled", 0);
  diagnostics.available &= bind(diagnostics.interactable, toggle, "get_interactable", 0);
  diagnostics.available &= bind(diagnostics.name, object, "get_name", 0);
  diagnostics.available &= bind(diagnostics.rect, rect, "get_rect", 0);
  PG_LOG("lobby: resolve %s%s%s", ok ? "ok" : "FAILED", api.localiserType ? "" : " (TextLocaliserUI not found)",
         api.enabledColour ? "" : " (enabledColour not found: label keeps Ready's colour)");
  return ok;
}

/** Keep the tape on the local player's ID card and translate clicks into volunteering. */
void tick(session::Session& s) {
  // FindObjectOfType searches the whole scene: look for the lobby at most once a second.
  const ULONGLONG now = GetTickCount64();
  if (!game::private_room()) {
    teardown();
    return;
  } else if (!game::alive(g_manager) && now >= g_nextSearch) {
    g_manager = api.findObjectOfType(api.serverManagerType, nullptr);
    g_nextSearch = now + 1000;
  }
  void* source = game::alive(g_manager) ? il2cpp::reference_field(g_manager, api.readyToggle) : nullptr;
  if (!game::alive(source)) {
    destroy_clone();
    return;
  }
  const char* missing = nullptr;
  void* card = local_card(missing);
  if (g_volunteer.toggle && (card != g_volunteer.card || !game::alive(g_volunteer.toggle))) destroy_clone();
  if (!card) {
    fail(missing);
    return;
  }
  if (!g_volunteer.toggle && now >= g_nextClone) {
    g_nextClone = now + 1000;
    const char* failure = nullptr;
    ui::Color textColour{};
    if (api.enabledColour) il2cpp::read_field(g_manager, api.enabledColour, &textColour);
    g_volunteer = make_clone(source, il2cpp::reference_field(g_manager, api.readyText),
                             api.enabledColour ? &textColour : nullptr, card, s.volunteer(), failure);
    if (failure) {
      if (fail(failure)) describe_control("Ready", source);
      return;
    }
    g_failure.clear();
  }
  if (!g_volunteer.toggle) return;
  if (diagnostics.available) {
    void* go = game::game_object_of(g_volunteer.toggle);
    const int visibility = diagnostics.activeSelf(go, nullptr) + 2 * diagnostics.activeHierarchy(go, nullptr);
    if (visibility != g_visibility) {
      g_visibility = visibility;
      describe_control("Play as ghost", g_volunteer.toggle);
    }
  }

  if (api.isOn(g_volunteer.toggle, nullptr) != g_volunteer.shownOn) s.set_volunteer(!s.volunteer());
  const bool relabel = now >= g_nextRelabel;
  if (relabel) g_nextRelabel = now + kRelabelMs;
  // A volunteer in a room where someone lacks the mod is told why nobody becomes the ghost.
  const char* text = s.local_is_ghost()                             ? "You are the ghost"
                     : s.volunteer() && !s.everyone_has_mod() ? "Everyone needs the mod"
                                                                    : "Play as ghost";
  show(g_volunteer, s.volunteer(), text, relabel);
}

/** Release the tape and scene discovery state when leaving the lobby. */
void teardown() {
  destroy_clone();
  g_manager = nullptr;
  g_nextSearch = g_nextClone = g_nextRelabel = 0;
  g_failure.clear();
}

}  // namespace pg::lobby
