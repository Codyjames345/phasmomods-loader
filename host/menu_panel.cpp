#include "menu_panel.h"

#include <windows.h>

#include <algorithm>
#include <utility>
#include <vector>

#include "Doozy.h"
#include "JournalController.h"
#include "MainMenu.h"
#include "Transform.h"
#include "UnityEvents.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "contained.h"
#include "find_child.h"
#include "host_game.h"
#include "il2cpp.h"
#include "log.h"
#include "runtime.h"
#include "runtime_services.h"

namespace pm::menu_panel {
namespace {

constexpr float kMargin = 0.006f;      // inside the paper, in the menu canvas's units
constexpr float kRowsShare = 0.76f;    // leave a separate footer for status and dependency messages
constexpr float kPaperTextScale = 2.0f;  // the paper's largest text against the game's own: the game fits a short error on a small box
constexpr float kBodyTextShare = 0.75f;  // paragraphs against that largest text
constexpr float kRowTextMostShare = 0.4f;  // however many rows the panel holds, a row's text stays within its height
constexpr float kCheckboxShare = 0.09f;
constexpr float kNameShare = 0.26f;
constexpr float kSmallestText = 0.35f;  // auto-sizing goes down to this share of the game's size
constexpr ULONGLONG kMessageTryMs = 1000;  // finding the popup searches the scene: at most this often
constexpr ULONGLONG kMessageTryMostMs = 16000;  // the wait doubles up to this while the scene has no popup (a contract)

using phasmo::Canvas;
using phasmo::CameraFocusUI;
using phasmo::ConnectToServer;
using phasmo::ErrorManager;
using phasmo::Image;
using phasmo::RectTransform;
using phasmo::TextMeshProUGUI;
using phasmo::Toggle;

// The copy's scripts that would act on the game's behalf: its buttons (whose listeners drive the error queue) until
// clickable() takes their listeners off, the localisers that put the game's words back, and the error manager itself.
// The buttons' colour animators stay on: they only colour the copy, as its button's state says.
bool acts_for_the_game(const void* script) {
  return phasmo::UIButton::type.is(script) || phasmo::TextLocaliserUI::type.is(script) ||
         phasmo::Button::type.is(script) || ErrorManager::type.is(script);
}

ui::Vec2 to_ui(phasmo::Vector2 v) { return {v.x, v.y}; }

Panel g_message;
struct WaitingMessage {
  std::string title, body;
  bool due = false;  // the player has yet to click it
  bool fromMod = false;  // a mod's news, which waits behind the host's
} g_waiting, g_behind;
ULONGLONG g_nextMessageTry = 0;
ULONGLONG g_messageTryWait = kMessageTryMs;
struct Decoration {
  il2cpp::Held object;
  bool active;
};
std::vector<Decoration> g_decor;
std::vector<il2cpp::Held> g_decorPanels;
il2cpp::Held g_decorMenu;

// The clicks the loader's listener counted, one entry per button it listens to, until clicked() takes them. Each entry
// holds its button, so no other object takes its address while it is counted. Main thread: the game raises UI events
// there.
struct Listening {
  il2cpp::Held target;
  int clicks = 0;
};
std::vector<Listening> g_listening;

// The loader's listener (runtime_services::NativeListener), bound to a button: counts the click. It runs inside the
// game's UnityEvent.Invoke, so it only counts.
void on_click(void* target, const void*) noexcept {
  for (Listening& l : g_listening)
    if (l.target.get() == target) {
      ++l.clicks;
      return;
    }
}

/** Read the same aggregate that drives the native menu's explicit control list. */
bool menu_enabled(void* menu) {
  const auto objects = hostgame::alive(menu) ? ConnectToServer::menuObjects.get(menu) : nullptr;
  return objects && phasmo::MenuObjects::enabled.get(objects);
}

/** Restore the board only after every owning popup has closed or been destroyed by the scene. */
void release_decor(void* closing = nullptr) {
  g_decorPanels.erase(std::remove_if(g_decorPanels.begin(), g_decorPanels.end(), [closing](const il2cpp::Held& root) {
    return root.get() == closing || !hostgame::alive(root.get());
  }), g_decorPanels.end());
  if (!g_decorPanels.empty()) return;
  if (menu_enabled(g_decorMenu.get()))
    for (const Decoration& decor : g_decor)
      if (hostgame::alive(decor.object.get())) hostgame::set_active(decor.object.get(), decor.active);
  g_decor.clear();
  g_decorMenu = nullptr;
}

void* error_popup() {
  const auto manager = phasmo::Object::FindObjectOfType(ErrorManager::type.type_object(), false);
  const auto popup = hostgame::alive(manager.raw()) ? ErrorManager::errorParent.get(manager) : nullptr;
  if (!hostgame::alive(popup.raw())) return nullptr;
  return phasmo::GameObject::type.is(popup) ? hostgame::object_transform(popup.raw()) : hostgame::transform_of(popup.raw());
}

template <typename F>
void each_object(void* transform, const F& fn) {
  if (!transform) return;
  fn(hostgame::game_object_of(transform));
  for (int i = 0; i < hostgame::child_count(transform); ++i) each_object(hostgame::child(transform, i), fn);
}

// A new UnityEvent with no listeners, or null.
void* new_event() {
  const auto event = phasmo::UnityEvent::type.make();
  if (event) phasmo::UnityEvent::ctor(event);
  return event.raw();
}

// Puts the loader's listener on `event`, bound to `target`, and counts its clicks for `c`. Refused for an event with a
// listener of the game's (persistent, so it cannot be taken off): it would run as well.
bool listen(Control& c, void* target, void* event) {
  if (!target || !event || phasmo::UnityEventBase::GetPersistentEventCount(event) != 0) return false;
  const auto action = api::Object::unchecked(
      runtime_services::delegate_new(map::UnityAction_ctor.key, map::UnityAction_Invoke.key, target, &on_click));
  if (!action) return false;
  std::erase_if(g_listening, [target](const Listening& l) {
    return l.target.get() == target || !hostgame::alive(l.target.get());  // a destroyed copy counts no more clicks
  });
  phasmo::UnityEvent::AddListener(event, action.raw());
  g_listening.push_back({il2cpp::Held(target), 0});
  c.button = il2cpp::Held(target);
  return true;
}

// Takes Doozy's own actions off a copied event (ModyEventBase.Runners), as its listeners are. False when the list does
// not empty.
bool clear_runners(void* event) {
  const api::List runners{phasmo::ModyEventBase::Runners.get(event).raw()};  // the list's array goes to the raw List
  for (size_t n = runners.size(); n > 0; --n)
    if (!runners.remove_at(0)) return false;
  return runners.size() == 0;
}

}  // namespace

void make_inert(void* root) {
  each_object(root, [](void* go) {
    const api::RefArray scripts{hostgame::components(go, phasmo::Behaviour::type.type_object())};
    for (size_t i = 0, n = scripts.size(); i < n; ++i) {
      const auto s = api::Object::unchecked(scripts[i]);  // a RefArray element read
      if (s && hostgame::alive(s.raw()) && acts_for_the_game(s.raw())) hostgame::set_enabled(s.raw(), false);
    }
  });
}

bool clickable(Control& c, void* root) {
  const auto button = api::Object::unchecked(
      root && can_copy() ? hostgame::get_component(root, phasmo::UIButton::type.type_object()) : nullptr);
  const auto behaviours = button ? phasmo::UISelectable::Behaviours.get(button) : nullptr;
  const auto list = behaviours ? phasmo::DoozyBehaviours::Behaviours.get(behaviours) : nullptr;
  if (!list) return false;
  // Every behaviour, not the click's alone: a copy runs none of its original's listeners, on an enter or a selection
  // either.
  const api::List each{list.raw()};
  for (size_t i = 0, n = each.size(); i < n; ++i) {
    const auto behaviour = api::Object::unchecked(each[i]);  // a List element read
    if (!behaviour) continue;
    const auto fresh = phasmo::ModyEvent::type.is(behaviour) ? api::Object::unchecked(new_event()) : nullptr;
    if (!fresh || !clear_runners(behaviour.raw())) return false;
    phasmo::ModyEvent::Event.set(behaviour, fresh);
    if (phasmo::ModyEvent::Event.get(behaviour) != fresh) return false;
  }
  const auto click = phasmo::UISelectable::GetBehaviour(button, phasmo::DoozyBehaviourName::PointerClick());
  if (!click || !listen(c, button.raw(), phasmo::ModyEvent::Event.get(click).raw())) return false;
  hostgame::set_enabled(button.raw(), true);
  return true;
}

namespace {

// The reaction behind the disabled colour of the tape on root's GameObject, or null.
void* disabled_reaction(void* root) {
  const auto animator =
      api::Object::unchecked(root ? hostgame::get_component(root, phasmo::UISelectableColorAnimator::type.type_object()) : nullptr);
  const auto animation = animator ? phasmo::UISelectableColorAnimator::DisabledAnimation.get(animator) : nullptr;
  return animation ? phasmo::ColorAnimation::Animation.get(animation).raw() : nullptr;
}

}  // namespace

bool match_disabled_look(void* root, void* like) {
  if (!api::all_resolved<phasmo::UISelectableColorAnimator::type, phasmo::UISelectableColorAnimator::DisabledAnimation,
                         phasmo::ColorAnimation::Animation, phasmo::ColorTargetReaction::ToCustomValue>())
    return false;
  const auto copied = api::Object::unchecked(disabled_reaction(root));  // still a void* return until 3c
  const auto game = hostgame::alive(like) ? api::Object::unchecked(disabled_reaction(like)) : nullptr;
  if (!copied || !game) return false;
  phasmo::ColorTargetReaction::ToCustomValue.set(copied, phasmo::ColorTargetReaction::ToCustomValue.get(game));
  return true;
}

bool clicked(Control& c) {
  for (Listening& l : g_listening)
    if (c.button && l.target.get() == c.button.get()) return std::exchange(l.clicks, 0) > 0;
  return false;
}

void release(Control& c) {
  const auto target = api::Object::unchecked(c.button.get());  // a Held's raw side
  if (target) std::erase_if(g_listening, [target](const Listening& l) { return l.target.get() == target.raw(); });
  c.button = nullptr;
}

namespace {

void* text_at(void* root, const char* path) {
  const auto t = api::Object::unchecked(host::find_child(root, path));  // still a void* return until 3c
  return t ? hostgame::get_component(t.raw(), TextMeshProUGUI::type.type_object()) : nullptr;
}

void fit(void* text) {
  const float size = ui::font_size(text);
  ui::set_word_wrap(text, true);
  ui::set_auto_size(text, size * kSmallestText, size);
}

/** Resolve the active main-menu component without searching the scene each frame. */
void* main_menu() {
  const auto menu = menu_available() ? ConnectToServer::get_Instance() : nullptr;
  return hostgame::alive(menu.raw()) && hostgame::active_in_hierarchy(hostgame::game_object_of(menu.raw()))
             ? menu.raw()
             : nullptr;
}

/** Hide the game's curated decoration list without losing its pre-popup visibility states. */
void hide_decor(void* root) {
  release_decor();
  if (g_decorPanels.empty()) {
    const auto menu = api::Object::unchecked(main_menu());  // still a void* return until 3c
    g_decorMenu = il2cpp::Held(menu.raw());
    const api::List list{menu ? ConnectToServer::menuDecor.get(menu).raw() : nullptr};
    for (size_t i = 0, n = list.size(); i < n; ++i) {
      const auto object = api::Object::unchecked(list[i]);  // a List element read
      if (hostgame::alive(object.raw())) g_decor.push_back({il2cpp::Held(object.raw()), hostgame::active_self(object.raw())});
    }
    for (const Decoration& decor : g_decor) hostgame::set_active(decor.object.get(), false);
  }
  g_decorPanels.emplace_back(root);
}

/** Copy only the paper's text style; each column has its own fixed size and rectangle. */
void* text_cell(Panel& p, void* parent, float x, float width, float height, float size) {
  const auto copy = api::Object::unchecked(hostgame::instantiate(hostgame::transform_of(p.body), parent, false));
  const auto text = copy ? api::Object::unchecked(hostgame::get_component(copy.raw(), TextMeshProUGUI::type.type_object()))
                         : nullptr;
  if (!text) return nullptr;
  ui::place(text.raw(), {0, 0.5f}, {0, 0.5f}, {x, 0}, {width, height});
  ui::set_alignment(text.raw(), ui::Align::Left);
  ui::set_word_wrap(text.raw(), true);
  ui::set_fixed_font_size(text.raw(), size);
  return text.raw();
}

/** A copy of the lobby paper's checkbox (Create Lobby's Toggle.1, a Toggle) under `parent`, with a new change event
 *  and no group, so none of the game's listeners runs for it. Null, with nothing left behind, when it cannot be made. */
void* toggle_row(void* parent) {
  const auto menu = api::Object::unchecked(main_menu());  // still a void* return until 3c
  const auto board = menu ? api::Object::unchecked(hostgame::parent(hostgame::transform_of(menu.raw()))) : nullptr;
  const auto source = api::Object::unchecked(host::find_child(board.raw(), "Create Lobby/Hardware/Toggle.1"));
  const auto toggle = source && can_copy_rows()
                          ? api::Object::unchecked(hostgame::get_component(source.raw(), Toggle::type.type_object()))
                          : nullptr;
  // A toggle in a group registers its copy with it as the copy switches on, and the group would then switch the game's
  // own toggles off.
  if (!toggle || Toggle::m_Group.get(toggle) || !host::find_child(source.raw(), "Background/Checkmark")) return nullptr;
  const auto fresh = phasmo::ToggleEvent::type.make();
  if (!fresh) return nullptr;
  phasmo::ToggleEvent::ctor(fresh);
  const auto copy = api::Object::unchecked(hostgame::instantiate(source.raw(), parent, false));
  const auto copied = copy ? api::Object::unchecked(hostgame::get_component(copy.raw(), Toggle::type.type_object())) : nullptr;
  if (copied) Toggle::onValueChanged.set(copied, fresh);
  if (!copied || Toggle::onValueChanged.get(copied) != fresh) {
    if (copy) hostgame::destroy(hostgame::game_object_of(copy.raw()));
    return nullptr;
  }
  return copied.raw();
}

}  // namespace

bool menu_available() {
  return api::all_resolved<Canvas::type, CameraFocusUI::type, ConnectToServer::get_Instance, Canvas::get_worldCamera,
                           CameraFocusUI::get_isFocused, ConnectToServer::menuObjects, phasmo::MenuObjects::enabled>();
}

bool listeners_available() {
  return api::all_resolved<phasmo::UnityEvent::AddListener, phasmo::UnityEventBase::GetPersistentEventCount,
                           phasmo::UnityAction::ctor, phasmo::UnityAction::Invoke>();
}

void* menu_camera() {
  const auto menu = api::Object::unchecked(main_menu());  // still a void* return until 3c
  const auto canvas = menu ? api::Object::unchecked(hostgame::get_component(menu.raw(), Canvas::type.type_object())) : nullptr;
  const auto camera = hostgame::alive(canvas.raw()) ? Canvas::get_worldCamera(canvas) : nullptr;
  return hostgame::alive(camera.raw()) && hostgame::enabled(camera.raw()) &&
                 hostgame::active_in_hierarchy(hostgame::game_object_of(camera.raw()))
             ? camera.raw()
             : nullptr;
}

bool menu_buttons_shown() {
  const auto menu = api::Object::unchecked(main_menu());  // still a void* return until 3c
  return menu && menu_enabled(menu.raw());
}

const char* why_menu_hidden() {
  const auto menu = api::Object::unchecked(main_menu());  // still a void* return until 3c
  if (!menu) return "no main menu is active";
  if (!menu_enabled(menu.raw())) return "the menu's buttons are off";
  if (!menu_camera()) return "the menu's camera is off";
  for (auto transform = api::Object::unchecked(hostgame::transform_of(menu.raw())); transform;
       transform = api::Object::unchecked(hostgame::parent(transform.raw()))) {
    const auto focus = api::Object::unchecked(hostgame::get_component(transform.raw(), CameraFocusUI::type.type_object()));
    if (hostgame::alive(focus.raw()))
      return CameraFocusUI::get_isFocused(focus) ? nullptr : "the camera is on another board";
  }
  return "the menu's board has no camera focus";
}

bool main_menu_visible() { return !why_menu_hidden(); }

void report() {
  if (!menu_available()) PM_LOG("menu: the main menu cannot be read in this game build; the consent notice answers Declined");
  else if (!listeners_available())
    PM_LOG("menu: this game build gives no listener on a button; the consent notice answers Declined");
  PM_LOG("menu: %s", can_copy() ? "available" : "unavailable (the consent notice answers Declined)");
}

bool can_copy() {
  return menu_available() && listeners_available() &&
         api::all_resolved<ErrorManager::type, ErrorManager::errorParent, phasmo::Object::FindObjectOfType,
                           phasmo::Object::Instantiate, phasmo::GameObject::type, phasmo::GameObject::get_transform,
                           phasmo::Component::get_transform, phasmo::Component::get_gameObject,
                           phasmo::Transform::get_childCount, phasmo::Transform::GetChild,
                           phasmo::Behaviour::type, phasmo::Behaviour::set_enabled, phasmo::GameObject::GetComponents,
                           TextMeshProUGUI::type, Image::type, RectTransform::get_sizeDelta,
                           RectTransform::get_anchoredPosition, phasmo::UIButton::type, phasmo::TextLocaliserUI::type,
                           phasmo::Button::type, phasmo::UISelectable::Behaviours, phasmo::UISelectable::GetBehaviour,
                           phasmo::DoozyBehaviours::Behaviours, phasmo::DoozyBehaviourName::PointerClick,
                           phasmo::ModyEvent::type, phasmo::ModyEvent::Event, phasmo::ModyEventBase::Runners,
                           phasmo::UnityEvent::type, phasmo::UnityEvent::ctor>();
}

bool can_copy_rows() {
  return can_copy() &&
         api::all_resolved<Toggle::type, Toggle::m_Group, Toggle::onValueChanged, Toggle::get_isOn,
                           Toggle::SetIsOnWithoutNotify, phasmo::ToggleEvent::type, phasmo::ToggleEvent::ctor,
                           phasmo::Selectable::set_interactable, phasmo::Transform::set_localScale>();
}

namespace {

// Makes the popup's copy once the caller has checked that it can be seen. See open().
bool build_popup(Panel& p, void* source, const std::string& title, const std::string& body,
                 const std::vector<std::string>& buttons, OpenFailure* failure);

}  // namespace

bool open(Panel& p, const std::string& title, const std::string& body, const std::vector<std::string>& buttons,
          OpenFailure* failure) {
  close(p);
  const auto fail = [&](bool lasting, const char* why) {
    if (failure) *failure = {lasting, why};
    close(p);
    return false;
  };
  if (!can_copy()) return fail(true, "this game build lacks an entry a copy of the menu's popup needs");
  if (buttons.empty()) return fail(true, "a popup needs a button");
  if (!main_menu_visible()) return fail(false, "the main menu is out of view");
  const auto source = api::Object::unchecked(error_popup());  // still a void* return until 3c
  if (!source) return fail(false, "the game's popup is not in the scene");
  return build_popup(p, source.raw(), title, body, buttons, failure);
}

namespace {

// Whether the popup can be seen from where it hangs: why not, or null. The popup is on the corkboard whatever paper is up,
// so the nearest board above it decides. Answers "not focused" when no board can be proved to be focused.
const char* why_board_hidden(void* transform) {
  if (!transform) return "the game's popup is not in the scene";
  if (!api::all_resolved<CameraFocusUI::type, CameraFocusUI::get_isFocused>()) return "the board's focus cannot be read";
  for (auto t = api::Object::unchecked(transform); t; t = api::Object::unchecked(hostgame::parent(t.raw()))) {
    const auto focus = api::Object::unchecked(hostgame::get_component(t.raw(), CameraFocusUI::type.type_object()));
    if (hostgame::alive(focus.raw())) return CameraFocusUI::get_isFocused(focus) ? nullptr : "the board is not focused";
  }
  return "the popup's board has no camera focus";
}

// As open(), over whichever paper of the corkboard is up. Out of view is never a lasting failure.
bool open_on_board(Panel& p, const std::string& title, const std::string& body, OpenFailure& failure) {
  close(p);
  if (!can_copy()) {
    failure = {true, "this game build lacks an entry a copy of the menu's popup needs"};
    return false;
  }
  const auto source = api::Object::unchecked(error_popup());  // still a void* return until 3c
  const char* hidden = why_board_hidden(source.raw());
  if (hidden) {
    failure = {false, hidden};
    failure.noPopup = !source;
    return false;
  }
  if (!build_popup(p, source.raw(), title, body, {"OK"}, &failure)) return false;
  p.anyPaper = true;
  return true;
}

bool build_popup(Panel& p, void* source, const std::string& title, const std::string& body,
                 const std::vector<std::string>& buttons, OpenFailure* failure) {
  const auto fail = [&](bool lasting, const char* why) {
    if (failure) *failure = {lasting, why};
    close(p);
    return false;
  };
  const auto copy = api::Object::unchecked(hostgame::instantiate(source, hostgame::parent(source), false));
  const auto root = copy && phasmo::GameObject::type.is(copy) ? api::Object::unchecked(hostgame::object_transform(copy.raw()))
                                                              : copy;
  if (!root) return fail(false, "the game's popup could not be copied");
  p.root = root.raw();  // from here on close can take it away, whatever throws below
  make_inert(root.raw());
  hostgame::set_active(hostgame::game_object_of(root.raw()), true);
  p.title = text_at(root.raw(), "Error Title tape/Error Title Text (TMP)");
  p.body = text_at(root.raw(), "Error_Text box/Background/Error Message");
  p.paper = host::find_child(root.raw(), "Error_Text box/Background");
  const auto button = api::Object::unchecked(host::find_child(root.raw(), "Error Accept Button"));
  if (!p.title || !p.body || !p.paper || !button) return fail(true, "the game's popup no longer has its title, paper or button");
  set_text(p.title, title);
  p.rowFontSize = ui::font_size_max(p.body) * kPaperTextScale;
  const ui::Vec2 paper = to_ui(RectTransform::get_sizeDelta(p.paper.get()));
  ui::place(p.body, {0, 1}, {0, 1}, {kMargin, -kMargin}, {paper.x - 2 * kMargin, paper.y - 2 * kMargin});
  ui::set_alignment(p.body, ui::Align::TopLeft);
  ui::set_word_wrap(p.body, true);
  ui::set_fixed_font_size(p.body, p.rowFontSize * kBodyTextShare);
  set_text(p.body, body);
  const ui::Vec2 size = to_ui(RectTransform::get_sizeDelta(button));
  const ui::Vec2 at = to_ui(RectTransform::get_anchoredPosition(button));
  const float width = size.x * (buttons.size() > 1 ? 1.6f : 1.0f);
  for (size_t i = 0; i < buttons.size(); ++i) {
    const auto b = i == 0 ? button : api::Object::unchecked(hostgame::instantiate(button.raw(), root.raw(), false));
    const float x = (static_cast<float>(i) - (static_cast<float>(buttons.size()) - 1) / 2) * width * 1.15f;
    ui::place(b.raw(), {0.5f, 0.5f}, {0.5f, 0.5f}, {x, at.y}, {width, size.y});
    const auto image = api::Object::unchecked(hostgame::get_component(b.raw(), Image::type.type_object()));
    const auto label = api::Object::unchecked(text_at(b.raw(), "Accept Text (TMP)"));
    if (!image || !label) break;
    fit(label.raw());
    set_text(label.raw(), buttons[i]);
    Control c;
    if (!clickable(c, b.raw())) return fail(true, "the game's button could not take the mod's listener");
    p.buttons.push_back(std::move(c));
    p.buttonTexts.emplace_back(label.raw());
  }
  if (p.buttons.size() != buttons.size()) return fail(true, "the game's button has no image or label");
  hide_decor(root.raw());
  return true;
}

}  // namespace

void fit_body(const Panel& p) {
  if (p.body) fit(p.body);
}

void close(Panel& p) {
  // The copy goes first: should a step after it throw, no popup without a working button stays on screen. Unity destroys
  // it at the end of the frame, so release_decor still sees it as the popup closing.
  if (p.root && hostgame::alive(p.root)) hostgame::destroy(hostgame::game_object_of(p.root));
  for (Control& c : p.buttons) release(c);
  release_decor(p.root);
  p = Panel{};
}

bool is_open(const Panel& p) { return p.root && hostgame::alive(p.root); }

bool drawn(const Panel& p) {
  const auto root = api::Object::unchecked(p.root.get());  // a Held's raw side
  if (!hostgame::alive(root.raw()) || !hostgame::active_in_hierarchy(hostgame::game_object_of(root.raw()))) return false;
  const auto canvas = api::Object::unchecked(
      hostgame::get_component(hostgame::parent(root.raw()), Canvas::type.type_object()));  // the error popup's own canvas
  return !hostgame::alive(canvas.raw()) || hostgame::enabled(canvas.raw());
}

int tick(Panel& p) {
  if (!is_open(p)) return -1;
  const bool inView = p.anyPaper ? !why_board_hidden(p.root.get()) : main_menu_visible();
  if (!inView || !hostgame::active_in_hierarchy(hostgame::game_object_of(p.root))) {
    close(p);
    return -1;
  }
  if (hostgame::cursor_locked()) hostgame::free_cursor();
  int clicked = -1;
  for (size_t i = 0; i < p.buttons.size(); ++i)
    if (menu_panel::clicked(p.buttons[i])) clicked = static_cast<int>(i);
  return clicked;
}

bool add_row(Panel& p, int index, int count, const std::string& name, const std::string& description, bool locked) {
  if (!is_open(p) || count <= 0 || index < 0 || index >= count) return false;
  const ui::Vec2 paper = to_ui(RectTransform::get_sizeDelta(p.paper.get()));
  const float inner = paper.x - 2 * kMargin;
  const float height = (paper.y * kRowsShare - kMargin) / static_cast<float>(count);
  if (p.rows.empty())
    ui::place(p.body, {0, 0}, {0, 0}, {kMargin, kMargin}, {inner, paper.y * (1 - kRowsShare) - kMargin});
  const auto toggle = api::Object::unchecked(toggle_row(p.paper));  // still a void* return until 3c
  const auto root = toggle ? api::Object::unchecked(hostgame::transform_of(toggle.raw())) : nullptr;
  const auto box = root ? api::Object::unchecked(host::find_child(root.raw(), "Background")) : nullptr;
  const auto mark = box ? api::Object::unchecked(host::find_child(box.raw(), "Checkmark")) : nullptr;
  if (!mark) {
    if (root) hostgame::destroy(hostgame::game_object_of(root.raw()));
    return false;
  }
  // The row is the toggle, its texts its children, so the game's toggle is what the row's clicks reach.
  const float boxSize = std::min(p.rowFontSize * 0.9f, height * 0.6f);
  const float rowText = std::min(p.rowFontSize, height * kRowTextMostShare);
  for (const auto t : {root, box, mark}) {
    hostgame::set_local_scale(t.raw(), {1, 1, 1});
    hostgame::set_active(hostgame::game_object_of(t.raw()), true);  // whatever the lobby's own are at this moment
  }
  // The toggle fades the tick by its alpha, so both images stay on whichever way it is set.
  for (const auto t : {box, mark}) {
    const auto image = api::Object::unchecked(hostgame::get_component(t.raw(), Image::type.type_object()));
    if (!image) {
      hostgame::destroy(hostgame::game_object_of(root.raw()));
      return false;
    }
    hostgame::set_enabled(image.raw(), true);
  }
  ui::place(root.raw(), {0, 1}, {0, 1}, {kMargin, -kMargin - height * static_cast<float>(index)}, {inner, height});
  ui::place(box.raw(), {0, 0.5f}, {0, 0.5f}, {0, 0}, {boxSize, boxSize});
  ui::place(mark.raw(), {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {boxSize, boxSize});
  const float nameX = inner * kCheckboxShare;
  const float descriptionX = nameX + inner * kNameShare;
  Row row{toggle.raw(),
          text_cell(p, root.raw(), nameX, inner * kNameShare - kMargin, height, rowText),
          text_cell(p, root.raw(), descriptionX, inner - descriptionX, height, rowText * kBodyTextShare)};
  if (!row.name || !row.description) {
    hostgame::destroy(hostgame::game_object_of(root.raw()));
    return false;
  }
  set_text(row.name, name);
  set_text(row.description, description);
  Toggle::SetIsOnWithoutNotify(toggle, false);
  phasmo::Selectable::set_interactable(toggle, !locked);
  row.locked = locked;
  p.rows.push_back(std::move(row));
  return true;
}

void set_row_checked(Panel& p, size_t index, bool checked) {
  if (index >= p.rows.size() || !hostgame::alive(p.rows[index].toggle)) return;
  Toggle::SetIsOnWithoutNotify(p.rows[index].toggle.get(), checked);
  p.rows[index].shownOn = checked;
}

void set_rows_interactable(Panel& p, bool on) {
  for (Row& row : p.rows)
    if (hostgame::alive(row.toggle)) phasmo::Selectable::set_interactable(row.toggle.get(), on && !row.locked);
}

int row_clicked(const Panel& p) {
  if (!is_open(p) || !main_menu_visible() || !hostgame::active_in_hierarchy(hostgame::game_object_of(p.root))) return -1;
  for (size_t i = 0; i < p.rows.size(); ++i)
    if (!p.rows[i].locked && hostgame::alive(p.rows[i].toggle) &&
        Toggle::get_isOn(p.rows[i].toggle.get()) != p.rows[i].shownOn)
      return static_cast<int>(i);
  return -1;
}

void set_text(void* text, const std::string& value) {
  if (text && hostgame::alive(text)) ui::set_text(text, value.c_str());
}

namespace {

// Closes whatever may be half built; what it throws is logged, and the panel forgets the copy.
void close_contained(Panel& p) {
  const bool closed = host::contained("menu", "closing a message", false, [&] {
    close(p);
    return true;
  });
  if (!closed) p = Panel{};
}

// Makes the waiting message once the popup's board is in view; drops it when it could never be made.
void show_waiting_message() {
  const ULONGLONG now = GetTickCount64();
  if (now < g_nextMessageTry) return;
  OpenFailure failure;
  const bool opened = host::contained("menu", "building a message", false, [&] {
    failure = {true, "building it threw"};
    if (!open_on_board(g_message, g_waiting.title, g_waiting.body, failure)) return false;
    failure = {};
    fit(g_message.body);  // the list of a lobby's mods can be long
    return true;
  });
  // A scene with no popup stays so for its whole length, such as a contract's: there is no use in searching it every second.
  g_messageTryWait = failure.noPopup ? std::min(g_messageTryWait * 2, kMessageTryMostMs) : kMessageTryMs;
  g_nextMessageTry = now + g_messageTryWait;
  if (opened || !failure.lasting) return;
  close_contained(g_message);
  PM_LOG("menu: the message \"%s\" cannot be shown (%s): %s", g_waiting.title.c_str(), failure.why, g_waiting.body.c_str());
  g_waiting = {};
}

}  // namespace

namespace {
void put_up(const std::string& title, const std::string& body, bool fromMod) {
  close_contained(g_message);
  g_waiting = {title, body, true, fromMod};
  g_nextMessageTry = 0;
  g_messageTryWait = kMessageTryMs;
}
}  // namespace

void message(const std::string& title, const std::string& body) {
  if (g_waiting.due && g_waiting.fromMod) g_behind = g_waiting;
  put_up(title, body, false);
}

void mod_message(const std::string& title, const std::string& body) {
  if (g_waiting.due && !g_waiting.fromMod) {
    g_behind = {title, body, true, true};
    return;
  }
  put_up(title, body, true);
}

void drop_message() {
  close_contained(g_message);
  g_waiting = {};
}

const Panel& shown_message() { return g_message; }

void tick_message() {
  release_decor();
  if (!g_waiting.due) {
    if (!g_behind.due) return;
    const WaitingMessage next = std::exchange(g_behind, {});
    put_up(next.title, next.body, true);
  }
  const bool fine = host::contained("menu", "showing a message", false, [] {
    if (!is_open(g_message)) {
      show_waiting_message();
      return true;
    }
    if (tick(g_message) != 0) return true;  // closed if its board went out of view: made again when it is back
    close_contained(g_message);
    g_waiting = {};
    return true;
  });
  if (!fine) drop_message();
}

}  // namespace pm::menu_panel
