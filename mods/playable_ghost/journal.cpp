#include "journal.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <cstdio>
#include <utility>

#include "game.h"
#include "ghostbook.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "ui.h"

namespace pg::journal {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;

// Build 25811160 (verified at load); obfuscated names, so by offset, each object checked by class.
constexpr size_t kPlayerJournal = 0xC8;        // Player: its JournalController
constexpr size_t kJournalPlayers = 0x28;       // JournalController: PlayersList (the pause menu's)
constexpr size_t kJournalRig = 0x30;           // JournalRigController
constexpr size_t kJournalRaycaster = 0x80;     // the pages' GraphicRaycaster
constexpr size_t kJournalAudio = 0x90;         // AudioSource
constexpr size_t kJournalOpenClip = 0x98;      // AudioClip
constexpr size_t kJournalCloseClip = 0xA0;     // AudioClip
constexpr size_t kJournalIsOpen = 0xB8;
constexpr size_t kJournalTabs = 0xD0;          // tab entries; [0]+0x10 is the pause menu's tab
constexpr size_t kTabComponent = 0x10;
constexpr size_t kPlayersListLocal = 0x28;     // PlayersList: it is this client's (fills the list)
constexpr size_t kRigAnimator = 0x40;          // JournalRigController: Journal_anim's Animator
constexpr size_t kRigTabObjects = 0x60;        // GameObject[]: the book's 3D tabs
constexpr size_t kRigOpened = 0x78;
constexpr size_t kRigClosedLayer = 0x88;       // the tabs' layer while closed
constexpr size_t kViewSynchronization = 0x48;  // PhotonView.Synchronization (ViewSynchronization.Off = 0)
constexpr size_t kViewIsMine = 0x68;           // PhotonView.amController (IsMine)
constexpr size_t kGhostButtonType = 0x78;      // GhostButton: GhostType
constexpr int kOpenLayer = 11;                 // "PlayerView": the open journal's canvas and tabs (as the game sets them)
constexpr int kUiLayer = 5;                    // "UI": the closed journal's canvas
// JournalController's private canvas step (StateChangeSync calls it): canvas, contents and raycaster
// on or off, and the journal or the pause menu shown.
constexpr uintptr_t kShowRva = 0x22CCF30;
constexpr uintptr_t kSetBoolByNameRva = 0x51A4750;  // Animator.SetBool(string, bool); (int, bool) shares the arity
constexpr uintptr_t kViewRpcRva = 0x43638A0;        // PhotonView.RPC(string, RpcTarget, object[]); no journal code uses the Player overload

constexpr ui::Vec2 kPageSize = {916, 1460};  // a journal page (GhostHuntingBookContent's pages)
constexpr float kEvidenceContentX = 959;      // Evidence.1's Layout Group, if it cannot be found
constexpr ULONGLONG kDescribeDelayMs = 1000;
// Pages the ghost player may read but not use: the evidence tab, where the investigators tick evidence
// and ghosts. (The Ghosts tab's own pages stay usable.)
constexpr const char* kViewOnlyPages[] = {"Evidence.1", "Evidence.2"};

struct Api {
  il2cpp::Object* photonViewType = nullptr;
  il2cpp::Object* ghostButtonType = nullptr;
  il2cpp::Object* buttonType = nullptr;
  void (*pointerEnter)(void*, void*, const void*) = nullptr;  // Selectable.OnPointerEnter(PointerEventData), data unused
  void (*pointerExit)(void*, void*, const void*) = nullptr;
  il2cpp::Object* textType = nullptr;
  il2cpp::Object* localiserType = nullptr;  // optional
  il2cpp::Object* rectTransformType = nullptr;
  il2cpp::Object* canvasGroupType = nullptr;
  il2cpp::Field* tickedImage = nullptr;
  il2cpp::Field* crossedOutImage = nullptr;
  void* openCloseJournal = nullptr;  // JournalController.OpenCloseJournal(bool), hooked
  void (*show)(void*, bool, bool, const void*) = nullptr;
  void (*openEvidencePage)(void*, const void*) = nullptr;
  void (*updateTabs)(void*, bool, const void*) = nullptr;
  void (*selectTab)(void*, void*, const void*) = nullptr;
  void (*hideBook)(void*, bool, const void*) = nullptr;
  void (*refreshPlayers)(void*, bool, const void*) = nullptr;
  void (*setBool)(void*, il2cpp::String*, bool, const void*) = nullptr;
  void* (*currentEventSystem)(const void*) = nullptr;
  Vector3 (*worldToScreen)(void*, Vector3, const void*) = nullptr;  // optional, for the layout log
  void (*stopAudio)(void*, const void*) = nullptr;
  void (*playOneShot)(void*, void*, const void*) = nullptr;
  void (*setBlocksRaycasts)(void*, bool, const void*) = nullptr;
  bool ok = false;
} api;

using OpenClose_t = void (*)(void*, bool, const void*);
using Rpc_t = void (*)(void*, il2cpp::String*, int, void*, const void*);
OpenClose_t g_openClose = nullptr;
Rpc_t g_rpc = nullptr;

struct Hidden {
  il2cpp::Held gameObject;
  bool active;
};
/** A second GhostButton for the same button, with its own holds (a Held is moved, never copied). */
GhostButton copy_of(const GhostButton& b) {
  GhostButton copy;
  copy.type = b.type;
  copy.rect = b.rect.get();
  copy.ticked = b.ticked.get();
  copy.crossed = b.crossed.get();
  copy.button = b.button.get();
  return copy;
}
struct Marked {
  GhostButton button;
  bool ticked, crossed;
};
struct View {
  il2cpp::Held view;
  uint8_t synchronization;  // as it was, put back on release
};
struct Blocker {  // a CanvasGroup that makes a page view only
  il2cpp::Held group;
  bool added;  // the mod added it (destroyed on release) rather than switching an existing one
};

struct Kept {
  il2cpp::Held root;                    // NetworkedJournal (transform), still on the investigator's body
  il2cpp::Held controller;              // JournalController
  il2cpp::Held rig;                     // JournalRigController (switched off: it follows the body's hands)
  il2cpp::Held canvas;                  // Journal2.0Canvas (transform)
  il2cpp::Held book;                    // Content/GhostHuntingBookContent (transform)
  Vector3 rootScale{1, 1, 1};           // the root's local scale on the body, put back on release
  ULONGLONG describeAt = 0;             // log the layout once the journal has opened
  std::vector<View> views;              // its PhotonViews
  std::vector<Blocker> blockers;        // view-only pages
  std::vector<Hidden> pageHidden;       // the game's page contents under the ghost player's pages
  std::vector<il2cpp::Held> pageAdded;  // the ghost player's pages (GameObjects), destroyed on release
  bool open = false;
  bool pause = false;
  bool spreadOn = false;
  Spread spread;
  std::vector<Hidden> hidden;       // the evidence page's contents, hidden under the spread page
  std::vector<Marked> marked;       // ghost buttons as they were before the spread marked them
  std::vector<il2cpp::Held> added;  // the mod's objects on the game's pages (destroyed with the spread)
  il2cpp::Held hovered;             // the ghost Button highlighted
  bool markedChanged = false;       // set_state has drawn on them
  bool rpcDropLogged = false;
  // The ghost player's versions of the investigators' pages (src/ghostbook.h).
  struct Written {
    il2cpp::Held title;  // TextMeshProUGUI
    il2cpp::Held body;
    std::string titleShown, bodyShown;
  } pages[ghostbook::kPageCount];
  il2cpp::Held evidenceTitle;  // the evidence page's heading (a copy over the original)
  il2cpp::Held ghostsTitle;    // the ghost page's heading
  il2cpp::Held ghostsLine;     // and the line under it
  std::string evidenceTitleShown, ghostsTitleShown, ghostsLineShown;
} g;

// Where the game holds an investigator's open journal (NetworkedJournal) in their camera's space. It
// follows JournalParent, which an animation rig on the investigator's body keeps in front of the
// camera; the ghost player's body is parked in the truck, so the place is the one measured in game
// from an investigator's open journal (build 25811160): 13.9 cm ahead, square on, unscaled.
constexpr Vector3 kHeldPosition = {0.0014f, -0.0118f, 0.1387f};
constexpr Quaternion kHeldRotation = {-0.0024f, 0.0046f, 0.0004f, 1.0f};

Quaternion multiply(Quaternion a, Quaternion b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

Vector3 rotate(Quaternion q, Vector3 v) {  // q v q*
  const Vector3 u{q.x, q.y, q.z};
  const Vector3 t{2 * (u.y * v.z - u.z * v.y), 2 * (u.z * v.x - u.x * v.z), 2 * (u.x * v.y - u.y * v.x)};
  return {v.x + q.w * t.x + (u.y * t.z - u.z * t.y), v.y + q.w * t.y + (u.z * t.x - u.x * t.z),
          v.z + q.w * t.z + (u.x * t.y - u.y * t.x)};
}

template <typename F>
void each_object(void* transform, const F& fn) {
  if (!transform) return;
  fn(game::game_object_of(transform));
  for (int i = 0; i < game::child_count(transform); ++i) each_object(game::child(transform, i), fn);
}

bool is_kept_view(const void* view) {
  return std::any_of(g.views.begin(), g.views.end(), [&](const View& v) { return v.view == view; });
}

// Not this client's, and not synchronised: the journal's own code then turns pages and tabs locally,
// and nothing reaches the copies of this journal on the other clients' parked body (whose page turns
// would sound in the truck). PUN can recompute IsMine (ownership and master changes), so this is
// reapplied every frame.
void make_views_local() {
  for (const View& v : g.views) {
    if (!game::alive(v.view)) continue;
    il2cpp::set_field<uint8_t>(v.view, kViewSynchronization, 0);
    il2cpp::set_field<bool>(v.view, kViewIsMine, false);
  }
}

// This client's again, synchronising as before (release: the body is an investigator's once more).
void make_views_networked() {
  for (const View& v : g.views) {
    if (!game::alive(v.view)) continue;
    il2cpp::set_field<uint8_t>(v.view, kViewSynchronization, v.synchronization);
    il2cpp::set_field<bool>(v.view, kViewIsMine, true);
  }
}

void make_view_only(void* page) {
  if (!page) return;
  void* go = game::game_object_of(page);
  void* group = game::get_component(page, api.canvasGroupType);
  const bool added = !group;
  if (added) group = game::add_component(go, api.canvasGroupType);
  if (!group) return;
  api.setBlocksRaycasts(group, false, nullptr);
  g.blockers.push_back({group, added});
}

void set_raycaster(bool on) {
  void* raycaster = il2cpp::field<void*>(g.controller, kJournalRaycaster);
  if (game::alive(raycaster)) game::set_enabled(raycaster, on);
}

// The UI input module the game switches on while a journal is open (EventSystem+0x28).
void set_ui_input(bool on) {
  void* system = api.currentEventSystem(nullptr);
  void* module = game::alive(system) ? il2cpp::field<void*>(system, 0x28) : nullptr;
  if (game::alive(module)) game::set_enabled(module, on);
}

void set_tab_layers(int layer) {
  game::set_layer(game::game_object_of(g.rig), layer);
  auto* tabs = il2cpp::field<il2cpp::Array*>(g.rig, kRigTabObjects);
  for (uintptr_t i = 0; tabs && i < tabs->max_length; ++i)
    if (void* tab = tabs->data<void*>()[i]; game::alive(tab)) game::set_layer(tab, layer);
}

void select_pause_tab() {
  auto* tabs = il2cpp::field<il2cpp::Array*>(g.controller, kJournalTabs);
  void* entry = tabs && tabs->max_length > 0 ? tabs->data<void*>()[0] : nullptr;
  void* component = entry ? il2cpp::field<void*>(entry, kTabComponent) : nullptr;
  if (game::alive(component)) api.selectTab(g.rig, game::game_object_of(component), nullptr);
}

void play(size_t clipField) {
  void* audio = il2cpp::field<void*>(g.controller, kJournalAudio);
  void* clip = il2cpp::field<void*>(g.controller, clipField);
  if (!game::alive(audio) || !game::alive(clip)) return;
  api.stopAudio(audio, nullptr);
  api.playOneShot(audio, clip, nullptr);
}

// The pause menu's player list, filled as JournalController.OpenCloseJournal fills it.
void refresh_players() {
  void* players = il2cpp::field<void*>(g.controller, kJournalPlayers);
  if (!il2cpp::is_class(players, "PlayersList") || !game::alive(players)) return;
  il2cpp::set_field<bool>(players, kPlayersListLocal, true);
  try {
    api.refreshPlayers(players, true, nullptr);
  } catch (...) {
    PG_LOG("journal: the pause menu's player list could not be filled");
  }
}

// The journal's opening and closing as StateChangeSync and JournalRigController.OpenCloseJournalNetworked
// run them for an investigator, without the parts that need one (hand animation, hand points).
void apply(bool open, bool pause) {
  void* jc = g.controller;
  const bool wasOpen = g.open;
  il2cpp::set_field<bool>(jc, kJournalIsOpen, open);
  set_ui_input(open);
  il2cpp::set_field<bool>(g.rig, kRigOpened, open);
  set_tab_layers(open ? kOpenLayer : il2cpp::field<int>(g.rig, kRigClosedLayer));
  void* animator = il2cpp::field<void*>(g.rig, kRigAnimator);
  if (game::alive(animator)) api.setBool(animator, il2cpp::string_new("isOpened"), open, nullptr);
  api.updateTabs(g.rig, pause, nullptr);
  if (open && pause) select_pause_tab();
  api.show(jc, open, pause, nullptr);
  set_raycaster(open && !g.spreadOn);  // the game's step leaves it off for a journal that is not ours
  if (open != wasOpen) play(open ? kJournalOpenClip : kJournalCloseClip);
  if (open && pause) refresh_players();
  api.hideBook(g.rig, !open, nullptr);
  // As LocalJournalController.OpenCloseJournal does after opening or closing (it is on the investigator
  // rig, which is switched off for the ghost player): the pages canvas goes to the PlayerView layer
  // while open, so the game's HUD passes draw it after the book (also PlayerView), and back to UI.
  game::set_layer(game::game_object_of(g.canvas), open ? kOpenLayer : kUiLayer);
  g.open = open;
  g.pause = open && pause;
  if (open && !wasOpen) g.describeAt = GetTickCount64() + kDescribeDelayMs;
}

// Where the open journal's parts land on the screen, logged once a second after it opens.
void describe(void* camera) {
  if (!api.worldToScreen || !game::alive(camera)) return;
  auto at = [&](void* t, const char* name) {
    if (!game::alive(t)) {
      PG_LOG("journal:   %s missing", name);
      return;
    }
    const Vector3 p = api.worldToScreen(camera, game::position(t), nullptr);
    PG_LOG("journal:   %s active %d layer %d at screen (%.0f, %.0f) depth %.3f", name,
           game::active_in_hierarchy(game::game_object_of(t)), game::layer(game::game_object_of(t)), p.x, p.y, p.z);
  };
  PG_LOG("journal: open journal layout (canvas %s)", g.spreadOn ? "with the mod's spread" : "on the game's pages");
  at(g.canvas, "canvas");
  at(game::find_child(g.book, "Evidence.2/All Ghosts"), "ghost list");
  at(game::find_child(g.book, "Home Tab"), "home tab");
  at(game::find_child(game::parent(g.canvas), "Journal_geo"), "3D book");
  if (g.spreadOn) at(g.spread.page, "mod page");
}

// The book where the game holds an investigator's open journal, in front of the ghost player's camera.
// It stays on the parked body (it goes with the body, every view included, as an investigator's
// does), so it is placed in world space, unscaled whatever the body's own scale.
void hold_in_front(void* camera) {
  void* cameraTransform = game::transform_of(camera);
  const Quaternion view = game::rotation(cameraTransform);
  const Vector3 eye = game::position(cameraTransform);
  const Vector3 ahead = rotate(view, kHeldPosition);
  game::set_position(g.root, {eye.x + ahead.x, eye.y + ahead.y, eye.z + ahead.z});
  game::set_rotation(g.root, multiply(view, kHeldRotation));
  const Vector3 body = game::lossy_scale(game::parent(g.root));
  auto inverse = [](float f) { return std::fabs(f) > 1e-6f ? 1.0f / f : 1.0f; };
  game::set_local_scale(g.root, {inverse(body.x), inverse(body.y), inverse(body.z)});
}

void detour_open_close(void* controller, bool pause, const void* method) {
  hooks::Guard guard;
  if (!controller || controller != g.controller || !game::alive(g.root)) {
    g_openClose(controller, pause, method);
    return;
  }
  PG_LOG("journal: the game %s the kept journal", g.open ? "closed" : "opened");
  if (g.open) close();
  else open(pause);
}

void detour_rpc(void* view, il2cpp::String* name, int target, void* parameters, const void* method) {
  hooks::Guard guard;
  if (view && is_kept_view(view)) {
    if (!g.rpcDropLogged) {
      char buf[96];
      il2cpp::to_utf8(name, buf, sizeof buf);
      PG_LOG("journal: dropped RPC %s from the kept journal (logged once)", buf);
      g.rpcDropLogged = true;
    }
    return;
  }
  g_rpc(view, name, target, parameters, method);
}

// A copy of a text from the journal's own pages, over the original (hidden), in its place and style.
void* over(void* original) {
  if (!game::alive(original)) return nullptr;
  void* copy = copy_text(original, game::parent(original));
  if (!copy) return nullptr;
  void* go = game::game_object_of(original);
  g.pageHidden.push_back({go, game::active_self(go)});
  game::set_active(go, false);
  g.pageAdded.push_back(game::game_object_of(copy));
  return copy;
}

// The investigators' pages that become the ghost player's (src/ghostbook.h): left pages' contents sit
// near x 0 in the book and right pages' near 948 (Contents A and B); each page's own contents are
// hidden and a page in the journal's own style (Payments' heading, rule and paragraph) is written over.
constexpr struct {
  ghostbook::PageId id;
  const char* page;
  float contentX;
} kGhostPages[] = {{ghostbook::kHomeLeft, "Contents A", 0},     {ghostbook::kHomeRight, "Contents B", 948},
                   {ghostbook::kItemsLeft, "Payments", 0},      {ghostbook::kItemsRight, "Cursed Possessions A", 948},
                   {ghostbook::kOverviewLeft, "Overview", 0},   {ghostbook::kOverviewRight, "Capturing Media", 948}};

void build_ghost_pages() {
  void* title = game::find_child(g.book, "Payments/Title");
  void* rule = game::find_child(g.book, "Payments/------------ (2)");
  void* body = game::find_child(g.book, "Payments/Text (1)");
  if (!title || !rule || !body) {
    PG_LOG("journal: the journal's page styles were not found; the ghost player's pages are not written");
    return;
  }
  for (const auto& p : kGhostPages) {
    void* page = game::find_child(g.book, p.page);
    if (!page) continue;
    for (int i = 0; i < game::child_count(page); ++i) {
      void* child = game::game_object_of(game::child(page, i));
      g.pageHidden.push_back({child, game::active_self(child)});
      game::set_active(child, false);
    }
    void* go = game::new_game_object("PlayableGhost page");
    if (go) g.pageAdded.push_back(go);
    void* rect = go ? game::add_component(go, api.rectTransformType) : nullptr;
    if (!rect) continue;
    game::set_parent(rect, page, false);
    ui::place(rect, {0.5f, 0.5f}, {0.5f, 0.5f}, {p.contentX - game::local_position(page).x, 0}, kPageSize);
    g.pages[p.id].title = copy_text(title, rect);
    g.pages[p.id].body = copy_text(body, rect);
    if (void* line = game::instantiate(rule, rect, false)) game::set_active(game::game_object_of(line), true);
  }
  g.evidenceTitle = over(game::find_child(g.book, "Evidence.1/Title"));
  g.ghostsTitle = over(game::find_child(g.book, "Evidence.2/Title"));
  g.ghostsLine = over(game::find_child(g.book, "Evidence.2/Weve decided its a :"));
}

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (!text || value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

void* text(void* transform) { return game::alive(transform) ? game::get_component(transform, api.textType) : nullptr; }

void disable_localiser(void* transform) {
  if (!api.localiserType || !transform) return;
  if (void* localiser = game::get_component(transform, api.localiserType)) game::set_enabled(localiser, false);
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  il2cpp::Class* journal = il2cpp::cls(kGame, "", "JournalController");
  il2cpp::Class* rig = il2cpp::cls(kGame, "", "JournalRigController");
  il2cpp::Class* threeState = il2cpp::cls(kGame, "", "ThreeStateButton");
  il2cpp::Class* audio = il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource");
  il2cpp::Class* group = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "CanvasGroup");
  il2cpp::Class* view = il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonView");
  api.photonViewType = view ? il2cpp::type_object(view) : nullptr;
  api.ghostButtonType = il2cpp::type_object(il2cpp::cls(kGame, "", "GhostButton"));
  il2cpp::Class* selectable = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Selectable");
  api.buttonType = il2cpp::type_object(il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "Button"));
  api.pointerEnter = reinterpret_cast<decltype(api.pointerEnter)>(il2cpp::method_ptr(selectable, "OnPointerEnter", 1));
  api.pointerExit = reinterpret_cast<decltype(api.pointerExit)>(il2cpp::method_ptr(selectable, "OnPointerExit", 1));
  api.textType = il2cpp::type_object(il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI"));
  il2cpp::Class* localiser = il2cpp::cls(kGame, "", "TextLocaliserUI");
  api.localiserType = localiser ? il2cpp::type_object(localiser) : nullptr;
  api.rectTransformType = il2cpp::type_object(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "RectTransform"));
  api.canvasGroupType = group ? il2cpp::type_object(group) : nullptr;
  api.tickedImage = il2cpp::field_info(threeState, "tickedImage");
  api.crossedOutImage = il2cpp::field_info(threeState, "crossedOutImage");
  api.openCloseJournal = il2cpp::method_ptr(journal, "OpenCloseJournal", 1);
  api.show = reinterpret_cast<decltype(api.show)>(game::game_assembly_address(kShowRva));
  api.openEvidencePage = reinterpret_cast<decltype(api.openEvidencePage)>(il2cpp::method_ptr(journal, "OpenEvidencePage", 0));
  api.updateTabs = reinterpret_cast<decltype(api.updateTabs)>(il2cpp::method_ptr(rig, "UpdateJournalTabs", 1));
  api.selectTab = reinterpret_cast<decltype(api.selectTab)>(il2cpp::method_ptr(rig, "SelectTab", 1));
  api.hideBook = reinterpret_cast<decltype(api.hideBook)>(il2cpp::method_ptr(rig, "HideJournal", 1));
  api.refreshPlayers = reinterpret_cast<decltype(api.refreshPlayers)>(
      il2cpp::method_ptr(il2cpp::cls(kGame, "", "PlayersList"), "RefreshUI", 1));
  api.setBool = reinterpret_cast<decltype(api.setBool)>(game::game_assembly_address(kSetBoolByNameRva));
  api.currentEventSystem = reinterpret_cast<decltype(api.currentEventSystem)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.EventSystems", "EventSystem"), "get_current", 0));
  api.worldToScreen = reinterpret_cast<decltype(api.worldToScreen)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Camera"), "WorldToScreenPoint", 1));
  api.stopAudio = reinterpret_cast<decltype(api.stopAudio)>(il2cpp::method_ptr(audio, "Stop", 0));
  api.playOneShot = reinterpret_cast<decltype(api.playOneShot)>(il2cpp::method_ptr(audio, "PlayOneShot", 1));
  api.setBlocksRaycasts = reinterpret_cast<decltype(api.setBlocksRaycasts)>(il2cpp::method_ptr(group, "set_blocksRaycasts", 1));
  api.ok = api.photonViewType && api.ghostButtonType && api.textType && api.rectTransformType && api.canvasGroupType &&
           api.tickedImage && api.crossedOutImage && api.openCloseJournal && api.show && api.openEvidencePage &&
           api.updateTabs && api.selectTab && api.hideBook && api.refreshPlayers && api.setBool &&
           api.currentEventSystem && api.stopAudio && api.playOneShot && api.setBlocksRaycasts;
  // Optional: without it the ghost player has no journal and the picker uses its plain lists.
  PG_LOG("journal: resolve %s", api.ok ? "ok" : "unavailable (no journal for the ghost player)");
  return true;
}

bool create_hooks() {
  if (!api.ok) return true;  // nothing to keep, so nothing to intercept
  return hooks::create(api.openCloseJournal, reinterpret_cast<void*>(&detour_open_close),
                       reinterpret_cast<void**>(&g_openClose), "JournalController.OpenCloseJournal") &&
         hooks::create(game::game_assembly_address(kViewRpcRva), reinterpret_cast<void*>(&detour_rpc),
                       reinterpret_cast<void**>(&g_rpc), "PhotonView.RPC");
}

void keep(void* player) {
  release();
  if (!api.ok) return;
  void* controller = game::alive(player) ? il2cpp::field<void*>(player, kPlayerJournal) : nullptr;
  void* rig = il2cpp::is_class(controller, "JournalController") ? il2cpp::field<void*>(controller, kJournalRig) : nullptr;
  if (!il2cpp::is_class(rig, "JournalRigController") || !game::alive(rig) || !game::alive(controller)) {
    PG_LOG("journal: no journal on the investigator; the ghost player has none");
    return;
  }
  Kept k;
  k.root = game::transform_of(rig);
  k.controller = controller;
  k.rig = rig;
  k.canvas = game::transform_of(controller);
  k.book = game::find_child(k.canvas, "Content/GhostHuntingBookContent");
  k.rootScale = game::local_scale(k.root);
  // It stays on the body and goes with it, every view included, as an investigator's does. Only the
  // rig that would carry it to the body's hands, in the truck, is switched off.
  game::set_enabled(rig, false);
  each_object(k.root, [&](void* go) {
    il2cpp::Array* views = game::components(go, api.photonViewType);
    for (uintptr_t i = 0; views && i < views->max_length; ++i)
      if (void* v = views->data<void*>()[i]) k.views.push_back({v, il2cpp::field<uint8_t>(v, kViewSynchronization)});
  });
  g = std::move(k);
  make_views_local();
  for (const char* name : kViewOnlyPages) make_view_only(game::find_child(g.book, name));
  build_ghost_pages();
  if (il2cpp::field<bool>(controller, kJournalIsOpen)) PG_LOG("journal: it was open as the investigator left");
  apply(false, false);  // closed the game's way (book and tabs hidden), whatever state it was left in
  void* system = api.currentEventSystem(nullptr);
  void* module = game::alive(system) ? il2cpp::field<void*>(system, 0x28) : nullptr;
  PG_LOG("journal: kept the investigator's journal (%zu views made local; UI input module %s)", g.views.size(),
         module ? il2cpp::class_name(il2cpp::class_of(module)) : "missing");
}

// The journal goes back to the investigator's body as the game left it: closed, its own pages, its
// views networked, and carried by its rig again.
void release() {
  if (kept()) {
    close();
    for (void* go : g.pageAdded)
      if (game::alive(go)) game::destroy(go);
    for (const Hidden& h : g.pageHidden)
      if (game::alive(h.gameObject)) game::set_active(h.gameObject, h.active);
    for (const Blocker& b : g.blockers) {
      if (!game::alive(b.group)) continue;
      if (b.added) game::destroy(b.group);
      else api.setBlocksRaycasts(b.group, true, nullptr);
    }
    make_views_networked();
    game::set_local_scale(g.root, g.rootScale);
    if (game::alive(g.rig)) game::set_enabled(g.rig, true);
    PG_LOG("journal: the journal is the investigator's again");
  }
  g = Kept{};
}

void* root() { return kept() ? g.root.get() : nullptr; }

bool kept() { return api.ok && g.root && game::alive(g.root) && game::alive(g.controller); }

void open(bool pause) {
  if (!kept() || (g.open && g.pause == pause && !g.spreadOn)) return;
  end_spread();  // the game's own pages again
  apply(true, pause);
  PG_LOG("journal: opened%s", pause ? " on the pause menu" : "");
}

void close() {
  if (!kept() || !g.open) return;
  end_spread();
  apply(false, false);
  PG_LOG("journal: closed");
}

bool is_open() { return kept() && g.open; }
bool spread_active() { return kept() && g.spreadOn; }
bool on_pause() { return is_open() && g.pause; }

void tick(void* camera) {
  if (!kept()) return;
  make_views_local();
  if (g.open && game::alive(camera)) hold_in_front(camera);
  if (g.describeAt && GetTickCount64() >= g.describeAt) {
    g.describeAt = 0;
    describe(camera);
  }
}

const Spread* begin_spread() {
  if (!kept()) return nullptr;
  if (g.spreadOn) return &g.spread;
  void* evidence = game::find_child(g.book, "Evidence.1");
  void* ghosts = game::find_child(g.book, "Evidence.2");
  void* grid = ghosts ? game::find_child(ghosts, "All Ghosts") : nullptr;
  if (!evidence || !grid) {
    PG_LOG("journal: no evidence spread in the kept journal");
    return nullptr;
  }
  if (!g.open || g.pause) apply(true, false);
  api.openEvidencePage(g.controller, nullptr);  // a local page turn: the journal's view is not ours
  set_raycaster(false);                          // the mod's page takes the clicks

  Spread s;
  void* go = game::new_game_object("PlayableGhost page");
  s.page = go ? game::add_component(go, api.rectTransformType) : nullptr;
  if (!s.page) return nullptr;
  for (int i = 0; i < game::child_count(evidence); ++i) {
    void* child = game::game_object_of(game::child(evidence, i));
    g.hidden.push_back({child, game::active_self(child)});
    game::set_active(child, false);
  }
  game::set_parent(s.page, evidence, false);  // the evidence page's turn in the book
  // The page objects' origins are not where they draw: their contents sit about a page's width to the
  // right (the evidence page's Layout Group at x 959). The mod's page goes where they are.
  void* layout = game::find_child(evidence, "Layout Group");
  const float contentX = layout ? game::local_position(layout).x : kEvidenceContentX;
  ui::place(s.page, {0.5f, 0.5f}, {0.5f, 0.5f}, {contentX, 0}, kPageSize);
  s.title = game::find_child(ghosts, "Title");
  s.description = game::find_child(ghosts, "Weve decided its a :");  // "We've decided it's a:"
  if (!s.description) s.description = s.title.get();
  // The ghost page's heading and line are the mod's own copies already (keep): the picker writes them.
  s.ghostTitle = g.ghostsTitle.get();
  s.ghostLine = g.ghostsLine.get();
  for (int i = 0; i < game::child_count(grid); ++i) {
    void* t = game::child(grid, i);
    void* button = game::get_component(t, api.ghostButtonType);
    if (!button || !game::active_self(game::game_object_of(t))) continue;  // unused "[New Ghost]" slots
    GhostButton b;
    b.type = il2cpp::field<int>(button, kGhostButtonType);
    b.rect = t;
    b.ticked = il2cpp::reference_field(button, api.tickedImage);
    b.crossed = il2cpp::reference_field(button, api.crossedOutImage);
    b.button = api.buttonType ? game::get_component(t, api.buttonType) : nullptr;
    const bool ticked = game::alive(b.ticked) && game::enabled(b.ticked);
    const bool crossed = game::alive(b.crossed) && game::enabled(b.crossed);
    g.marked.push_back({copy_of(b), ticked, crossed});
    s.ghosts.push_back(std::move(b));
  }
  g.spread = std::move(s);
  g.spreadOn = true;
  PG_LOG("journal: evidence spread taken over (%zu ghost buttons)", g.spread.ghosts.size());
  return &g.spread;
}

void end_spread() {
  if (!g.spreadOn) return;
  hover(nullptr);
  g.ghostsTitleShown.clear();  // the picker wrote them: the ghost player's own text goes back next write()
  g.ghostsLineShown.clear();
  for (void* added : g.added)
    if (game::alive(added)) game::destroy(added);
  g.added.clear();
  for (const Marked& m : g.marked) {
    if (!g.markedChanged) break;  // untouched: investigators' choices that came in meanwhile stay
    if (game::alive(m.button.ticked)) game::set_enabled(m.button.ticked, m.ticked);
    if (game::alive(m.button.crossed)) game::set_enabled(m.button.crossed, m.crossed);
  }
  g.markedChanged = false;
  if (game::alive(g.spread.page)) game::destroy(game::game_object_of(g.spread.page));
  for (const Hidden& h : g.hidden)
    if (game::alive(h.gameObject)) game::set_active(h.gameObject, h.active);
  g.hidden.clear();
  g.marked.clear();
  g.spread = Spread{};
  g.spreadOn = false;
  if (kept()) set_raycaster(g.open);
}

void hover(const GhostButton* b) {
  void* button = b && game::alive(b->button) ? b->button.get() : nullptr;
  if (button == g.hovered || !api.pointerEnter || !api.pointerExit) return;
  if (game::alive(g.hovered)) api.pointerExit(g.hovered, nullptr, nullptr);
  if (button) api.pointerEnter(button, nullptr, nullptr);
  g.hovered = button;
}

void write(const ghostbook::Book& book) {
  if (!kept()) return;
  for (int i = 0; i < ghostbook::kPageCount; ++i) {
    set_if_changed(g.pages[i].title, g.pages[i].titleShown, book.pages[i].title);
    set_if_changed(g.pages[i].body, g.pages[i].bodyShown, book.pages[i].body);
  }
  set_if_changed(g.evidenceTitle, g.evidenceTitleShown, book.evidenceTitle);
  if (g.spreadOn) return;  // the picker writes the ghost page's heading and line meanwhile
  set_if_changed(g.ghostsTitle, g.ghostsTitleShown, book.ghostsTitle);
  set_if_changed(g.ghostsLine, g.ghostsLineShown, book.ghostsLine);
}

void set_state(const GhostButton& b, int state) {
  g.markedChanged = true;
  if (game::alive(b.ticked)) game::set_enabled(b.ticked, state == 1);
  if (game::alive(b.crossed)) game::set_enabled(b.crossed, state == 2);
}

void* copy_text(void* sourceTransform, void* parent) {
  if (!game::alive(sourceTransform) || !parent) return nullptr;
  void* t = game::instantiate(sourceTransform, parent, false);
  if (!t) return nullptr;
  disable_localiser(t);
  game::set_active(game::game_object_of(t), true);  // the source may be hidden under the mod's own copy
  return text(t);
}

}  // namespace pg::journal
