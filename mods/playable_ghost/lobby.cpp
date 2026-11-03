#include "lobby.h"

#include <windows.h>

#include <string>

#include "JournalController.h"
#include "Player.h"
#include "ServerManager.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "text_table.h"
#include "ui.h"

namespace pm::lobby {
namespace {

using phasmo::RectTransform;
using phasmo::ServerManager;
using phasmo::Toggle;

// The game's Ready tape, small, stuck across the bottom edge of the player's own ID card just below
// its XP bar. Card units (scene level2): the face is 0.0767 x 0.0410 about the origin and the XP bar
// spans y -0.0184..-0.0143; the card grid leaves 0.005 board units (card scale 0.95) between rows.
// At 0.36 the 0.022-tall tape spans y -0.0188..-0.0268, clearing the XP bar above and the next row
// of cards (and, for the bottom row, the Leave/Debrief buttons) below.
constexpr float kTapeScale = 0.36f;
constexpr ui::Vec2 kTapePosition = {0.0f, -0.0228f};
constexpr float kMinFontFraction = 0.3f;  // "Play as ghost" auto-sizes down to this share of Ready's font
constexpr ULONGLONG kRelabelMs = 500;     // in case a localisation component rewrites the label

bool g_diagnostics = false;  // the optional entries describe_control reads resolved

struct Clone {
  api::Held toggle;
  api::Held label;
  api::Held card;  // the local player's MultiplayerProfile the tape is stuck to
  bool shownOn = false;
  std::string text;
};

Clone g_volunteer;
api::Held g_manager;
ULONGLONG g_nextSearch = 0;
ULONGLONG g_nextClone = 0;
ULONGLONG g_nextRelabel = 0;
int g_visibility = -1;  // the tape's last logged activeSelf + 2 * activeInHierarchy
std::string g_failure;

phasmo::Vector2 anchored(void* transform) { return RectTransform::get_anchoredPosition(transform); }
void place_at(void* transform, phasmo::Vector2 at) { RectTransform::set_anchoredPosition(transform, at); }

/** Log a failure once until it changes, so retries stay quiet; true when it was logged. */
bool fail(const char* reason) {
  if (g_failure == reason) return false;
  g_failure = reason;
  PM_LOG("lobby: Play as ghost unavailable: %s", reason);
  return true;
}

/** Log a control's state and ancestry, for checking placement from mods.log alone. */
void describe_control(const char* label, void* toggle) {
  if (!g_diagnostics || !game::alive(toggle)) {
    PM_LOG("lobby: %s inspection unavailable (object=%p)", label, toggle);
    return;
  }
  PM_LOG("lobby: %s enabled=%d interactable=%d isOn=%d", label, phasmo::Behaviour::get_enabled(toggle),
         phasmo::Selectable::get_interactable(toggle), Toggle::get_isOn(toggle));
  void* transform = game::transform_of(toggle);
  for (int depth = 0; transform && depth < 5; ++depth) {
    void* go = game::game_object_of(transform);
    // As the old fixed buffer did, a long name is cut short.
    const std::string name = api::to_utf8(phasmo::Object::get_name(go)).substr(0, 127);
    PM_LOG("lobby: %s ancestor=%d name=%s activeSelf=%d activeHierarchy=%d", label, depth, name.c_str(),
           phasmo::GameObject::get_activeSelf(go), phasmo::GameObject::get_activeInHierarchy(go));
    if (depth == 0) {
      const phasmo::Rect r = RectTransform::get_rect(transform);
      const phasmo::Vector2 p = anchored(transform);
      PM_LOG("lobby: %s rect=(%.6g,%.6g,%.6g,%.6g) pos=(%.6g,%.6g)", label, r.x, r.y, r.width, r.height, p.x, p.y);
    }
    transform = phasmo::Transform::get_parent(transform);
  }
}

/**
 * The ID card showing this client's player, or null with `reason` saying why: the lobby has not
 * assigned one yet, or the list holds something other than what it should.
 */
void* local_card(const char*& reason) {
  void* list = ServerManager::cards.get(g_manager);
  if (!list) { reason = "ServerManager has no ID card list yet"; return nullptr; }
  const api::List cards{list};
  const int local = game::local_actor();
  reason = "this player has no ID card in the lobby yet";
  for (size_t i = 0, n = cards.size(); i < n; ++i) {
    void* card = cards[i];
    if (!game::alive(card)) continue;
    if (!phasmo::MultiplayerProfile::type.is(card)) { reason = "the ID card list holds something other than MultiplayerProfile"; return nullptr; }
    void* spot = phasmo::ProfileCard::spot.get(card);
    if (!spot) continue;  // a card with no player
    if (!phasmo::PlayerSpot::type.is(spot)) { reason = "ProfileCard.spot is not a PlayerSpot"; return nullptr; }
    void* player = phasmo::PlayerSpot::photonPlayer.get(spot);
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
  // Ready's own event is a ToggleEvent; the copy gets a new one, so none of Ready's listeners run for it.
  // The constructor is an optional entry: a build without it loses only the tape.
  void* oldEvent = Toggle::onValueChanged.get(source);
  const bool canMake = api::all_resolved<phasmo::ToggleEvent::ctor>() && oldEvent && phasmo::ToggleEvent::type.is(oldEvent);
  void* fresh = canMake ? phasmo::ToggleEvent::type.make() : nullptr;
  if (!fresh) { failure = "independent ToggleEvent is unavailable"; return {}; }
  void* cardTransform = game::transform_of(card);
  void* staging = game::new_game_object("PlayableGhost lobby staging");
  if (!staging) { failure = "inactive staging object allocation failed"; return {}; }
  game::set_active(staging, false);
  float readySize = 0;
  try {
    void* stageRect = phasmo::GameObject::AddComponent(staging, RectTransform::type.type_object());
    if (!stageRect) { failure = "inactive staging RectTransform is unavailable"; game::destroy(staging); return {}; }
    game::set_parent(stageRect, cardTransform, false);
    ui::place(stageRect, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {0, 0});
    phasmo::ToggleEvent::ctor(fresh);
    c.toggle = game::instantiate(source, stageRect, false);
    if (c.toggle) {
      // The inactive clone has never registered; the property setter would touch native group members.
      Toggle::m_Group.set(c.toggle, nullptr);
      Toggle::onValueChanged.set(c.toggle, fresh);
      c.label = game::instantiate(sourceLabel, game::transform_of(c.toggle), false);
    }
    if (!c.toggle || !c.label) {
      failure = "native control or label cloning failed";
      game::destroy(staging);
      return {};
    }
    void* target = phasmo::Selectable::get_targetGraphic(c.toggle);
    if (target == sourceLabel) phasmo::Selectable::set_targetGraphic(c.toggle, c.label);
    else if (target && !game::is_child_of(game::transform_of(target), game::transform_of(c.toggle))) {
      failure = "cloned control references an external target graphic";
      game::destroy(staging);
      return {};
    }
    void* graphic = Toggle::graphic.get(c.toggle);
    if (graphic && !game::is_child_of(game::transform_of(graphic), game::transform_of(c.toggle))) {
      failure = "cloned toggle references an external graphic";
      game::destroy(staging);
      return {};
    }
    if (Toggle::onValueChanged.get(c.toggle) != fresh) {
      failure = "cloned ToggleEvent was not replaced"; game::destroy(staging); return {};
    }
    // Ready and its label are siblings centred on the board, so the label keeps its offset from Ready.
    const phasmo::Vector2 readyAt = anchored(game::transform_of(source));
    const phasmo::Vector2 labelAt = anchored(game::transform_of(sourceLabel));
    place_at(game::transform_of(c.label), {labelAt.x - readyAt.x, labelAt.y - readyAt.y});
    // The copied localiser would put "Ready" back as soon as the label is enabled.
    if (void* localiser = phasmo::Component::GetComponent(c.label, phasmo::TextLocaliserUI::type.type_object()))
      game::set_enabled(localiser, false);
    // ServerManager greys Ready's label while Ready cannot be clicked; the tape always can.
    if (textColour) ui::set_color(c.label, *textColour);
    readySize = ui::font_size(c.label);
    ui::set_word_wrap(c.label, false);
    ui::set_auto_size(c.label, readySize * kMinFontFraction, readySize);
    Toggle::SetIsOnWithoutNotify(c.toggle, initialOn);
    c.shownOn = initialOn;
    phasmo::Selectable::set_interactable(c.toggle, true);
    game::set_enabled(c.toggle, true);
    game::set_enabled(c.label, true);
    game::set_active(game::game_object_of(c.label), true);
    game::set_active(game::game_object_of(c.toggle), true);
    void* tape = game::transform_of(c.toggle);
    game::set_local_scale(tape, {kTapeScale, kTapeScale, 1});
    place_at(tape, {kTapePosition.x, kTapePosition.y});
    game::set_parent(tape, cardTransform, false);
  } catch (...) {
    failure = "native control initialization threw";
    ui::destroy(c.toggle);
    game::destroy(staging);
    return {};
  }
  game::destroy(staging);
  c.card = card;
  PM_LOG("lobby: Play as ghost on card %p (toggle=%p label=%p on=%d fontSize=%.3g)", card, c.toggle.get(), c.label.get(), initialOn, readySize);
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
  Toggle::SetIsOnWithoutNotify(c.toggle, on);
  c.shownOn = on;
  if (c.label && (relabel || text != c.text)) {
    c.text = text;
    ui::set_text(c.label, text.c_str());
  }
}

}  // namespace

/** Notes which optional entries resolved; bind has already checked the required ones. Always true. */
bool resolve() {
  g_diagnostics =
      api::all_resolved<phasmo::GameObject::get_activeInHierarchy, phasmo::Selectable::get_interactable>();
  const bool coloured = api::all_resolved<ServerManager::enabledColour>();
  PM_LOG("lobby: resolve ok%s%s", coloured ? "" : " (enabledColour not found: label keeps Ready's colour)",
         g_diagnostics ? "" : " (no control diagnostics)");
  return true;
}

/** Keep the tape on the local player's ID card and translate clicks into volunteering. */
void tick(session::Session& s) {
  // FindObjectOfType searches the whole scene: look for the lobby at most once a second.
  const ULONGLONG now = GetTickCount64();
  if (!game::private_room()) {
    teardown();
    return;
  } else if (!game::alive(g_manager) && now >= g_nextSearch) {
    g_manager = phasmo::Object::FindObjectOfType(ServerManager::type.type_object(), false);
    g_nextSearch = now + 1000;
  }
  void* source = game::alive(g_manager) ? ServerManager::readyToggle.get(g_manager) : nullptr;
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
    const bool coloured = api::all_resolved<ServerManager::enabledColour>();
    const phasmo::Color colour = coloured ? ServerManager::enabledColour.get(g_manager) : phasmo::Color{};
    const ui::Color textColour{colour.r, colour.g, colour.b, colour.a};
    g_volunteer = make_clone(source, ServerManager::readyText.get(g_manager), coloured ? &textColour : nullptr, card,
                             s.volunteer(), failure);
    if (failure) {
      if (fail(failure)) describe_control("Ready", source);
      return;
    }
    g_failure.clear();
  }
  if (!g_volunteer.toggle) return;
  if (g_diagnostics) {
    void* go = game::game_object_of(g_volunteer.toggle);
    const int visibility =
        phasmo::GameObject::get_activeSelf(go) + 2 * phasmo::GameObject::get_activeInHierarchy(go);
    if (visibility != g_visibility) {
      g_visibility = visibility;
      describe_control("Play as ghost", g_volunteer.toggle);
    }
  }

  if (Toggle::get_isOn(g_volunteer.toggle) != g_volunteer.shownOn) s.set_volunteer(!s.volunteer());
  const bool relabel = now >= g_nextRelabel;
  if (relabel) g_nextRelabel = now + kRelabelMs;
  const std::string text = s.local_is_ghost() ? i18n::text("lobby.you-are-the-ghost") : i18n::text("lobby.play-as-ghost");
  show(g_volunteer, s.volunteer(), text, relabel);
}

/** Release the tape and scene discovery state when leaving the lobby. */
void teardown() {
  destroy_clone();
  g_manager = nullptr;
  g_nextSearch = g_nextClone = g_nextRelabel = 0;
  g_failure.clear();
}

}  // namespace pm::lobby
