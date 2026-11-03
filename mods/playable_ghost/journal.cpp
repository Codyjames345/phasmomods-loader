#include "journal.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <cstdio>
#include <utility>

#include "Animator.h"
#include "Camera.h"
#include "JournalController.h"
#include "Photon.h"
#include "Player.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "game.h"
#include "ghostbook.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "ui.h"

namespace pm::journal {
namespace {

using phasmo::Quaternion;
using phasmo::Vector3;

using phasmo::JournalController;
using phasmo::JournalRigController;
using phasmo::PhotonView;

constexpr ui::Vec2 kPageSize = {916, 1460};  // a journal page (GhostHuntingBookContent's pages)
constexpr float kEvidenceContentX = 959;      // Evidence.1's Layout Group, if it cannot be found
constexpr ULONGLONG kDescribeDelayMs = 1000;

struct Hidden {
  api::Held gameObject;
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
  api::Held view;
  int32_t synchronization;  // its ViewSynchronization as it was, put back on release
};
struct Blocker {  // a CanvasGroup that makes a page view only
  api::Held group;
  bool added;  // the mod added it (destroyed on release) rather than switching an existing one
};

struct Kept {
  api::Held root;                    // NetworkedJournal (transform), still on the investigator's body
  api::Held controller;              // JournalController
  api::Held rig;                     // JournalRigController (switched off: it follows the body's hands)
  api::Held canvas;                  // Journal2.0Canvas (transform)
  api::Held book;                    // Content/GhostHuntingBookContent (transform)
  Vector3 rootScale{1, 1, 1};           // the root's local scale on the body, put back on release
  ULONGLONG describeAt = 0;             // log the layout once the journal has opened
  std::vector<View> views;              // its PhotonViews
  std::vector<Blocker> blockers;        // view-only pages
  std::vector<Hidden> pageHidden;       // the game's page contents under the ghost player's pages
  std::vector<api::Held> pageAdded;  // the ghost player's pages (GameObjects), destroyed on release
  bool open = false;
  bool pause = false;
  bool spreadOn = false;
  Spread spread;
  std::vector<Hidden> hidden;       // the evidence page's contents, hidden under the spread page
  std::vector<Marked> marked;       // ghost buttons as they were before the spread marked them
  std::vector<api::Held> added;  // the mod's objects on the game's pages (destroyed with the spread)
  api::Held hovered;             // the ghost Button highlighted
  bool markedChanged = false;       // set_state has drawn on them
  // The ghost player's versions of the investigators' pages (src/ghostbook.h).
  struct Written {
    api::Held title;  // TextMeshProUGUI
    api::Held body;
    std::string titleShown, bodyShown;
  } pages[ghostbook::kPageCount];
  api::Held evidenceTitle;  // the evidence page's heading (a copy over the original)
  api::Held ghostsTitle;    // the ghost page's heading
  api::Held ghostsLine;     // and the line under it
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

// Not this client's, and not synchronised: the journal's own code then turns pages and tabs locally,
// and nothing reaches the copies of this journal on the other clients' parked body (whose page turns
// would sound in the truck): each of its sends checks IsMine first. PUN can recompute IsMine (ownership
// and master changes), so this is reapplied every frame.
void make_views_local() {
  for (const View& v : g.views) {
    if (!game::alive(v.view)) continue;
    PhotonView::Synchronization.set(v.view, phasmo::ViewSynchronization::Off());
    PhotonView::isMine.set(v.view, false);
  }
}

// This client's again, synchronising as before (release: the body is an investigator's once more).
void make_views_networked() {
  for (const View& v : g.views) {
    if (!game::alive(v.view)) continue;
    PhotonView::Synchronization.set(v.view, v.synchronization);
    PhotonView::isMine.set(v.view, true);
  }
}

void make_view_only(void* page) {
  if (!page) return;
  void* go = game::game_object_of(page);
  void* groupType = phasmo::CanvasGroup::type.type_object();
  void* group = phasmo::Component::GetComponent(page, groupType);
  const bool added = !group;
  if (added && go) group = phasmo::GameObject::AddComponent(go, groupType);
  if (!group) return;
  phasmo::CanvasGroup::set_blocksRaycasts(group, false);
  g.blockers.push_back({group, added});
}

void set_raycaster(bool on) {
  void* raycaster = JournalController::canvasRaycaster.get(g.controller);
  if (game::alive(raycaster)) game::set_enabled(raycaster, on);
}

// The UI input module the game switches on while a journal is open.
void* ui_input_module() {
  void* system = phasmo::EventSystem::get_current();
  return game::alive(system) ? phasmo::EventSystem::currentInputModule.get(system) : nullptr;
}

void set_ui_input(bool on) {
  if (void* module = ui_input_module(); game::alive(module)) game::set_enabled(module, on);
}

void set_tab_layers(int layer) {
  game::set_layer(game::game_object_of(g.rig), layer);
  const api::RefArray tabs{JournalRigController::allTabsObjects.get(g.rig)};
  for (size_t i = 0, n = tabs.size(); i < n; ++i)
    if (void* tab = tabs[i]; game::alive(tab)) game::set_layer(tab, layer);
}

void select_pause_tab() {
  const api::RefArray tabs{JournalController::pauseMenuTabs.get(g.controller)};
  void* entry = tabs.size() > 0 ? tabs[0] : nullptr;
  void* component = entry ? phasmo::JournalTab::button.get(entry) : nullptr;
  if (game::alive(component)) JournalRigController::SelectTab(g.rig, game::game_object_of(component));
}

void play(bool opening) {
  void* audio = JournalController::openSource.get(g.controller);
  void* clip = opening ? JournalController::openClip.get(g.controller) : JournalController::closeClip.get(g.controller);
  if (!game::alive(audio) || !game::alive(clip)) return;
  phasmo::AudioSource::Stop(audio, true);  // Unity's Stop()
  phasmo::AudioSource::PlayOneShot(audio, clip);
}

// The pause menu's player list, filled as JournalController.OpenCloseJournal fills it.
void refresh_players() {
  void* players = JournalController::playersList.get(g.controller);
  if (!phasmo::PlayersList::type.is(players) || !game::alive(players)) return;
  phasmo::PlayersList::isLocal.set(players, true);
  try {
    phasmo::PlayersList::RefreshUI(players, true);
  } catch (...) {
    PM_LOG("journal: the pause menu's player list could not be filled");
  }
}

// The journal's opening and closing as StateChangeSync and JournalRigController.OpenCloseJournalNetworked
// run them for an investigator, without the parts that need one (hand animation, hand points).
void apply(bool open, bool pause) {
  void* jc = g.controller;
  const bool wasOpen = g.open;
  const int openLayer = phasmo::Layers::PlayerView.index();  // the open journal's canvas and tabs, as the game sets them
  JournalController::open.set(jc, open);
  set_ui_input(open);
  JournalRigController::opened.set(g.rig, open);
  set_tab_layers(open ? openLayer : JournalRigController::closedLayer.get(g.rig));
  void* animator = JournalRigController::animator.get(g.rig);
  if (const int32_t id = game::alive(animator) ? phasmo::Journal::Opened.hash(animator) : 0)
    phasmo::Animator::SetBoolID(animator, id, open);
  JournalRigController::UpdateJournalTabs(g.rig, pause);
  if (open && pause) select_pause_tab();
  JournalController::ShowCanvas(jc, open, pause);
  set_raycaster(open && !g.spreadOn);  // the game's step leaves it off for a journal that is not ours
  if (open != wasOpen) play(open);
  if (open && pause) refresh_players();
  JournalRigController::HideJournal(g.rig, !open);
  // As LocalJournalController.OpenCloseJournal does after opening or closing (it is on the investigator
  // rig, which is switched off for the ghost player): the pages canvas goes to the PlayerView layer
  // while open, so the game's HUD passes draw it after the book (also PlayerView), and back to UI.
  game::set_layer(game::game_object_of(g.canvas), open ? openLayer : phasmo::Layers::UI.index());
  g.open = open;
  g.pause = open && pause;
  if (open && !wasOpen) g.describeAt = GetTickCount64() + kDescribeDelayMs;
}

// Where the open journal's parts land on the screen, logged once a second after it opens.
void describe(void* camera) {
  if (!game::alive(camera)) return;
  auto at = [&](void* t, const char* name) {
    if (!game::alive(t)) {
      PM_LOG("journal:   %s missing", name);
      return;
    }
    const Vector3 world = game::position(t);
    const phasmo::Vector3 p = phasmo::Camera::WorldToScreenPoint(camera, phasmo::Vector3{world.x, world.y, world.z});
    PM_LOG("journal:   %s active %d layer %d at screen (%.0f, %.0f) depth %.3f", name,
           game::active_in_hierarchy(game::game_object_of(t)), game::layer(game::game_object_of(t)), p.x, p.y, p.z);
  };
  PM_LOG("journal: open journal layout (canvas %s)", g.spreadOn ? "with the mod's spread" : "on the game's pages");
  at(g.canvas, "canvas");
  at(phasmo::Journal::GhostGrid.find(g.canvas), "ghost list");
  at(phasmo::Journal::HomeTab.find(g.canvas), "home tab");
  at(phasmo::Journal::Model.find(g.root), "3D book");
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

void detour_open_close(api::Original<JournalController::OpenCloseJournal> original, void* controller, bool pause) {
  if (!controller || controller != g.controller || !game::alive(g.root)) {
    original(controller, pause);
    return;
  }
  PM_LOG("journal: the game %s the kept journal", g.open ? "closed" : "opened");
  if (g.open) close();
  else open(pause);
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
  void* (*page)(void* canvas);
  float contentX;
} kGhostPages[] = {{ghostbook::kHomeLeft, [](void* c) { return phasmo::Journal::ContentsA.find(c); }, 0},
                   {ghostbook::kHomeRight, [](void* c) { return phasmo::Journal::ContentsB.find(c); }, 948},
                   {ghostbook::kItemsLeft, [](void* c) { return phasmo::Journal::Payments.find(c); }, 0},
                   {ghostbook::kItemsRight, [](void* c) { return phasmo::Journal::CursedPossessions.find(c); }, 948},
                   {ghostbook::kOverviewLeft, [](void* c) { return phasmo::Journal::Overview.find(c); }, 0},
                   {ghostbook::kOverviewRight, [](void* c) { return phasmo::Journal::CapturingMedia.find(c); }, 948}};

void build_ghost_pages() {
  void* title = phasmo::Journal::PaymentsTitle.find(g.canvas);
  void* rule = phasmo::Journal::PaymentsRule.find(g.canvas);
  void* body = phasmo::Journal::PaymentsBody.find(g.canvas);
  if (!title || !rule || !body) {
    PM_LOG("journal: the journal's page styles were not found; the ghost player's pages are not written");
    return;
  }
  for (const auto& p : kGhostPages) {
    void* page = p.page(g.canvas);
    if (!page) continue;
    for (int i = 0; i < game::child_count(page); ++i) {
      void* child = game::game_object_of(game::child(page, i));
      g.pageHidden.push_back({child, game::active_self(child)});
      game::set_active(child, false);
    }
    void* go = game::new_game_object("PlayableGhost page");
    if (go) g.pageAdded.push_back(go);
    void* rect = go ? phasmo::GameObject::AddComponent(go, phasmo::RectTransform::type.type_object()) : nullptr;
    if (!rect) continue;
    game::set_parent(rect, page, false);
    ui::place(rect, {0.5f, 0.5f}, {0.5f, 0.5f}, {p.contentX - game::local_position(page).x, 0}, kPageSize);
    g.pages[p.id].title = copy_text(title, rect);
    g.pages[p.id].body = copy_text(body, rect);
    if (void* line = game::instantiate(rule, rect, false)) game::set_active(game::game_object_of(line), true);
  }
  g.evidenceTitle = over(phasmo::Journal::EvidenceTitle.find(g.canvas));
  g.ghostsTitle = over(phasmo::Journal::GhostsTitle.find(g.canvas));
  g.ghostsLine = over(phasmo::Journal::GhostsLine.find(g.canvas));
}

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (!text || value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

void* text(void* transform) {
  return game::alive(transform) ? phasmo::Component::GetComponent(transform, phasmo::TextMeshProUGUI::type.type_object())
                                : nullptr;
}

void disable_localiser(void* transform) {
  if (!transform) return;
  void* localiser = phasmo::Component::GetComponent(transform, phasmo::TextLocaliserUI::type.type_object());
  if (localiser) game::set_enabled(localiser, false);
}

}  // namespace

bool create_hooks() {
  // The canonical bodies, as the raw hooks by address were.
  return api::hook<JournalController::OpenCloseJournal>(&detour_open_close, "JournalController.OpenCloseJournal",
                                                        api::Twins::CanonicalOnly);
}

void keep(void* player) {
  release();
  void* controller = game::alive(player) ? phasmo::Player::journal.get(player) : nullptr;
  void* rig = JournalController::type.is(controller) ? JournalController::rig.get(controller) : nullptr;
  if (!JournalRigController::type.is(rig) || !game::alive(rig) || !game::alive(controller)) {
    PM_LOG("journal: no journal on the investigator; the ghost player has none");
    return;
  }
  Kept k;
  k.root = game::transform_of(rig);
  k.controller = controller;
  k.rig = rig;
  k.canvas = game::transform_of(controller);
  k.book = phasmo::Journal::BookContent.find(k.canvas);
  k.rootScale = game::local_scale(k.root);
  // It stays on the body and goes with it, every view included, as an investigator's does. Only the
  // rig that would carry it to the body's hands, in the truck, is switched off.
  game::set_enabled(rig, false);
  void* viewType = PhotonView::type.type_object();
  each_object(k.root, [&](void* go) {
    const api::RefArray views{go && viewType ? phasmo::GameObject::GetComponents(go, viewType) : nullptr};
    for (size_t i = 0, n = views.size(); i < n; ++i)
      if (void* v = views[i]) k.views.push_back({v, PhotonView::Synchronization.get(v)});
  });
  g = std::move(k);
  make_views_local();
  make_view_only(phasmo::Journal::EvidencePage.find(g.canvas));
  make_view_only(phasmo::Journal::GhostsPage.find(g.canvas));
  build_ghost_pages();
  if (JournalController::open.get(controller)) PM_LOG("journal: it was open as the investigator left");
  apply(false, false);  // closed the game's way (book and tabs hidden), whatever state it was left in
  PM_LOG("journal: kept the investigator's journal (%zu views made local; UI input module %s)", g.views.size(),
         ui_input_module() ? "found" : "missing");
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
      else phasmo::CanvasGroup::set_blocksRaycasts(b.group, true);
    }
    make_views_networked();
    game::set_local_scale(g.root, g.rootScale);
    if (game::alive(g.rig)) game::set_enabled(g.rig, true);
    PM_LOG("journal: the journal is the investigator's again");
  }
  g = Kept{};
}

void* root() { return kept() ? g.root.get() : nullptr; }

bool kept() { return g.root && game::alive(g.root) && game::alive(g.controller); }

void open(bool pause) {
  if (!kept() || (g.open && g.pause == pause && !g.spreadOn)) return;
  end_spread();  // the game's own pages again
  apply(true, pause);
  PM_LOG("journal: opened%s", pause ? " on the pause menu" : "");
}

void close() {
  if (!kept() || !g.open) return;
  end_spread();
  apply(false, false);
  PM_LOG("journal: closed");
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
  void* evidence = phasmo::Journal::EvidencePage.find(g.canvas);
  void* ghosts = phasmo::Journal::GhostsPage.find(g.canvas);
  void* grid = ghosts ? phasmo::Journal::GhostGrid.find(g.canvas) : nullptr;
  if (!evidence || !grid) {
    PM_LOG("journal: no evidence spread in the kept journal");
    return nullptr;
  }
  if (!g.open || g.pause) apply(true, false);
  JournalController::OpenEvidencePage(g.controller);  // a local page turn: the journal's view is not ours
  set_raycaster(false);                               // the mod's page takes the clicks

  Spread s;
  void* go = game::new_game_object("PlayableGhost page");
  s.page = go ? phasmo::GameObject::AddComponent(go, phasmo::RectTransform::type.type_object()) : nullptr;
  if (!s.page) return nullptr;
  for (int i = 0; i < game::child_count(evidence); ++i) {
    void* child = game::game_object_of(game::child(evidence, i));
    g.hidden.push_back({child, game::active_self(child)});
    game::set_active(child, false);
  }
  game::set_parent(s.page, evidence, false);  // the evidence page's turn in the book
  // The page objects' origins are not where they draw: their contents sit about a page's width to the
  // right (the evidence page's Layout Group at x 959). The mod's page goes where they are.
  void* layout = phasmo::Journal::EvidenceRows.find(g.canvas);
  const float contentX = layout ? game::local_position(layout).x : kEvidenceContentX;
  ui::place(s.page, {0.5f, 0.5f}, {0.5f, 0.5f}, {contentX, 0}, kPageSize);
  s.title = phasmo::Journal::GhostsTitle.find(g.canvas);
  s.description = phasmo::Journal::GhostsLine.find(g.canvas);  // "We've decided it's a:"
  if (!s.description) s.description = s.title.get();
  // The ghost page's heading and line are the mod's own copies already (keep): the picker writes them.
  s.ghostTitle = g.ghostsTitle.get();
  s.ghostLine = g.ghostsLine.get();
  void* ghostButtonType = phasmo::GhostButton::type.type_object();
  void* buttonType = phasmo::Button::type.type_object();
  for (int i = 0; i < game::child_count(grid); ++i) {
    void* t = game::child(grid, i);
    void* button = t ? phasmo::Component::GetComponent(t, ghostButtonType) : nullptr;
    if (!button || !game::active_self(game::game_object_of(t))) continue;  // unused "[New Ghost]" slots
    GhostButton b;
    b.type = phasmo::GhostButton::ghostType.get(button);
    b.rect = t;
    b.ticked = phasmo::ThreeStateButton::tickedImage.get(button);
    b.crossed = phasmo::ThreeStateButton::crossedOutImage.get(button);
    b.button = phasmo::Component::GetComponent(t, buttonType);
    const bool ticked = game::alive(b.ticked) && game::enabled(b.ticked);
    const bool crossed = game::alive(b.crossed) && game::enabled(b.crossed);
    g.marked.push_back({copy_of(b), ticked, crossed});
    s.ghosts.push_back(std::move(b));
  }
  g.spread = std::move(s);
  g.spreadOn = true;
  PM_LOG("journal: evidence spread taken over (%zu ghost buttons)", g.spread.ghosts.size());
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
  if (button == g.hovered) return;
  if (game::alive(g.hovered)) phasmo::Selectable::OnPointerExit(g.hovered, nullptr);
  if (button) phasmo::Selectable::OnPointerEnter(button, nullptr);
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

}  // namespace pm::journal
