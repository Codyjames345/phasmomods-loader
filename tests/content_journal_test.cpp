// The journal side of content (host/content_journal.h) on the fake backend, over a faked journal laid
// out as the game ships it (the fixture of tests/pages_test.cpp, rebuilt on the game API): the Spirit's
// guess button, list entry and page, the Ghosts tab's list placed by hand with its grid switched off,
// and the game's pages for a few vanilla ghosts with the keys their texts show. Then: the guess
// buttons and the journal's own lists and evidence filter, the list entries and the pages they lead
// to, page numbers across clients, the texts in the game's languages, lines for vanilla ghosts, and a
// mod's ghosts leaving while another's stay. Then a custom evidence's row on the evidence page
// (host/content_evidence.h): its keys, its sync, the layout, the filter over its number, the page line
// of a ghost that gives it, and a failure part-way. Then the lobby's journal (a player book as a
// contract's): its custom ghosts, evidence rows and lines while content is active there, its pages after
// a contract ends, and the game's own journal again, list placement included, when content stops.
// Run: build\content_journal_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "Difficulty.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostInfo.h"
#include "GhostHuntState.h"
#include "JournalController.h"
#include "LevelController.h"
#include "LocalisationSystem.h"
#include "Photon.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "check.h"
#include "text_fixture.h"
#include "content.h"
#include "content_difficulty.h"
#include "content_evidence.h"
#include "content_journal.h"
#include "content_localised.h"
#include "content_text.h"
#include "content_types.h"
#include "contract.h"
#include "fake_game.h"
#include "game_api.h"
#include "log.h"
#include "registration.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace content = pm::api::content;
namespace fake = pm::fake;
namespace map = pm::map;

std::vector<std::string> g_lines;
void capture(const char* line) { g_lines.emplace_back(line); }
unsigned g_heard = 0;  // presence's revision as each agreement observe passes it: each hears something new
bool logged(const std::string& text) {
  return std::any_of(g_lines.begin(), g_lines.end(), [&](const std::string& l) { return l.find(text) != std::string::npos; });
}

// One scene object: its GameObject, its Transform and its components at once. Its fields start where
// a managed object's do, so the map's field entries read and write them.
enum Kind : unsigned { kGhostButton = 1, kButton = 2, kLocaliser = 4, kGrid = 8, kEvidenceButton = 16, kLayout = 32, kText = 64 };
struct Node {
  void* klass = nullptr;
  alignas(8) char fields[0x300]{};
  std::string name;
  Node* parent = nullptr;
  std::vector<Node*> children;
  unsigned kinds = 0;
  bool active = true;
  bool enabled = true;  // as a Behaviour (the grid)
  bool interactable = true;  // as a Selectable
  bool destroyed = false;
  phasmo::Vector2 position{0, 0}, size{1, 1}, cell{1, 1};
  phasmo::Vector2 anchorMin{0.5f, 0.5f}, anchorMax{0.5f, 0.5f}, sizeDelta{0, 0};  // as a RectTransform
  int32_t constraint = 0, constraintCount = 0;
  float spacing = 0;  // as a vertical layout
  std::string text;   // as a TextMeshProUGUI
};
std::vector<std::unique_ptr<Node>> g_nodes;
int g_nodeClass = 0, g_tabClass = 0;

Node* node(const std::string& name, Node* parent = nullptr, unsigned kinds = 0) {
  g_nodes.push_back(std::make_unique<Node>());
  Node* n = g_nodes.back().get();
  n->klass = &g_nodeClass;
  n->name = name;
  n->kinds = kinds;
  if (parent) {
    n->parent = parent;
    parent->children.push_back(n);
  }
  return n;
}
Node* as(void* p) { return static_cast<Node*>(p); }

// The fake engine.
struct Engine {
  std::vector<std::string> calls;  // the journal's calls, in order
  int32_t page = -1;               // GoToPage's last
  std::map<int32_t, void*> paired;  // RebuildGhosts' ghost type -> button
  std::vector<int32_t> listed;      // RebuildGhosts' ghosts, in collection order
  int initialised = 0;
  int32_t language = 0;
  std::vector<std::string> codes{"EN", "DE", "FR"};
  int children = 0;       // GetChild calls: the walks over the pages
  int components = 0;     // GetComponent calls: the reads of the evidence rows among them
  int instantiates = 0;   // Instantiate calls
  int failInstantiateAt = 0;  // the Instantiate call that answers null (1 the first), 0 none
  void* failClear = nullptr;  // the collection whose Clear fails
  void* missingClear = nullptr;  // the collection whose class has no Clear
  int failInitialise = 0;  // the Initialise call that fails (1 the first), 0 none
  bool missingInitialise = false;  // the sync controller's class has no Initialise
  bool threw = false;     // RebuildGhosts' Dictionary.Add met a ghost already there
  std::vector<int32_t> found;  // the journal's found evidence, as AddEvidence keeps it
  std::vector<int32_t> ruledOut;  // its ruled-out evidence, as FilterOutEvidence keeps it
  std::vector<int32_t> left;   // the ghost types SetGhostTypes leaves uncrossed, in collection order
  int32_t guess = 24;          // the player's guess, by type (None: 24, as Start and UnselectGhost leave it)
  bool mine = true;            // the journal's view is the local player's
  int pageThrows = 0;          // page syncs that reached the game's out-of-range throw
  int reports = 0;             // page syncs the game's sender check rejected (and reported)
} g_engine;

void* get_name(void* o, const void*) { return api::make_string(as(o)->name); }
void set_name(void* o, void* s, const void*) { as(o)->name = api::to_utf8(s); }
Node* clone_tree(Node* from, Node* parent, std::map<Node*, Node*>& made) {
  Node* n = node(from->name, parent, from->kinds);
  made[from] = n;
  std::memcpy(n->fields, from->fields, sizeof n->fields);
  n->active = from->active;
  n->enabled = from->enabled;
  n->position = from->position;
  n->size = from->size;
  n->anchorMin = from->anchorMin;
  n->anchorMax = from->anchorMax;
  n->sizeDelta = from->sizeDelta;
  n->cell = from->cell;
  n->constraint = from->constraint;
  n->constraintCount = from->constraintCount;
  n->spacing = from->spacing;
  n->text = from->text;
  for (Node* c : from->children) clone_tree(c, n, made);
  return n;
}
// Object.Instantiate: the copies' references to objects inside the copied tree point at the copies, as
// Unity's do.
Node* clone(Node* from, Node* parent) {
  std::map<Node*, Node*> made;
  Node* n = clone_tree(from, parent, made);
  for (const auto& [source, copy] : made)
    for (size_t at = 0; at + sizeof(void*) <= sizeof copy->fields; at += sizeof(void*)) {
      Node* to = nullptr;
      std::memcpy(&to, copy->fields + at, sizeof to);
      if (const auto it = made.find(to); it != made.end()) std::memcpy(copy->fields + at, &it->second, sizeof to);
    }
  return n;
}
void* instantiate(void* original, void* parent, bool, const void*) {
  if (++g_engine.instantiates == g_engine.failInstantiateAt) return nullptr;
  return clone(as(original), as(parent));
}
void destroy(void* o, float) {
  as(o)->destroyed = true;
  fake::destroy(o);
}
void set_active(void* o, bool on, const void*) { as(o)->active = on; }
bool active_self(void* o, const void*) { return as(o)->active; }
void* self(void* o, const void*) { return o; }
// The running game: Component.get_transform on a GameObject reads the wrong native object and crashes. So it fails a test here.
std::set<const void*> g_gameObjects;
void* component_transform(void* o, const void*) {
  CHECK(!g_gameObjects.count(o));
  return o;
}
unsigned kind_of(void* type) {
  if (type == phasmo::GhostButton::type.type_object()) return kGhostButton;
  if (type == phasmo::Button::type.type_object()) return kButton;
  if (type == phasmo::TextLocaliserUI::type.type_object()) return kLocaliser;
  if (type == phasmo::GridLayoutGroup::type.type_object()) return kGrid;
  if (type == phasmo::EvidenceButton::type.type_object()) return kEvidenceButton;
  if (type == phasmo::HorizontalOrVerticalLayoutGroup::type.type_object()) return kLayout;
  if (type == phasmo::TextMeshProUGUI::type.type_object()) return kText;
  return 0;
}
void* get_component(void* o, void* type, const void*) {
  ++g_engine.components;
  return as(o)->kinds & kind_of(type) ? o : nullptr;
}
Node* find_in(Node* n, unsigned kind) {
  if (n->kinds & kind) return n;
  for (Node* c : n->children)
    if (Node* f = find_in(c, kind)) return f;
  return nullptr;
}
void* get_component_in_children(void* o, void* type, bool, const void*) { return find_in(as(o), kind_of(type)); }
bool get_enabled(void* o, const void*) { return as(o)->enabled; }
void set_enabled(void* o, bool on, const void*) { as(o)->enabled = on; }
int32_t child_count(void* o, const void*) { return static_cast<int32_t>(as(o)->children.size()); }
void* get_child(void* o, int32_t i, const void*) {
  ++g_engine.children;
  return as(o)->children.at(static_cast<size_t>(i));
}
bool is_active(void* o, const void*) { return as(o)->active; }
bool is_interactable(void* o, const void*) { return as(o)->interactable; }
int32_t sibling_index(void* o, const void*) {
  const auto& c = as(o)->parent->children;
  return static_cast<int32_t>(std::find(c.begin(), c.end(), as(o)) - c.begin());
}
void set_sibling(void* o, int32_t i, const void*) {
  auto& c = as(o)->parent->children;
  c.erase(std::find(c.begin(), c.end(), as(o)));
  c.insert(c.begin() + std::min<size_t>(static_cast<size_t>(i), c.size()), as(o));
}
phasmo::Rect get_rect(void* o, const void*) { return {0, 0, as(o)->size.x, as(o)->size.y}; }
phasmo::Vector2 get_position(void* o, const void*) { return as(o)->position; }
void set_position(void* o, phasmo::Vector2 v, const void*) { as(o)->position = v; }
phasmo::Vector2 get_anchor_min(void* o, const void*) { return as(o)->anchorMin; }
void set_anchor_min(void* o, phasmo::Vector2 v, const void*) { as(o)->anchorMin = v; }
phasmo::Vector2 get_anchor_max(void* o, const void*) { return as(o)->anchorMax; }
void set_anchor_max(void* o, phasmo::Vector2 v, const void*) { as(o)->anchorMax = v; }
phasmo::Vector2 get_size_delta(void* o, const void*) { return as(o)->sizeDelta; }
void set_size_delta(void* o, phasmo::Vector2 v, const void*) { as(o)->sizeDelta = v; }
void set_text(void* o, void* s, const void*) { as(o)->text = api::to_utf8(s); }
void* get_text(void* o, const void*) { return api::make_string(as(o)->text); }
phasmo::Vector2 get_cell(void* o, const void*) { return as(o)->cell; }
void set_cell(void* o, phasmo::Vector2 v, const void*) { as(o)->cell = v; }
phasmo::Vector2 get_spacing(void*, const void*) { return {0.5f, 2.5f}; }
int32_t get_constraint(void* o, const void*) { return as(o)->constraint; }
void set_constraint(void* o, int32_t v, const void*) { as(o)->constraint = v; }
int32_t get_count(void* o, const void*) { return as(o)->constraintCount; }
void set_count(void* o, int32_t v, const void*) { as(o)->constraintCount = v; }
// The engine's layout pass at the frame's end: an enabled grid places its active children in its
// cells, row by row, writing each one's anchors (the top-left corner), size and position, as
// GridLayoutGroup.SetCellsAlongAxis does (0x5465620, 0x5473320).
void lay_out(Node* grid) {
  if (!(grid->kinds & kGrid) || !grid->enabled || grid->constraint != 1 || grid->constraintCount <= 0) return;
  const phasmo::Vector2 sp = get_spacing(grid, nullptr);
  int i = 0;
  for (Node* c : grid->children) {
    if (!c->active) continue;
    const int col = i % grid->constraintCount, row = i / grid->constraintCount;
    c->anchorMin = c->anchorMax = {0, 1};
    c->sizeDelta = c->size = grid->cell;
    c->position = {grid->cell.x * (col + 0.5f) + col * sp.x, -(grid->cell.y * (row + 0.5f) + row * sp.y)};
    ++i;
  }
}
float get_layout_spacing(void* o, const void*) { return as(o)->spacing; }
void set_layout_spacing(void* o, float v, const void*) { as(o)->spacing = v; }
void set_key(void* o, void* key, const void*) {
  phasmo::TextLocaliserUI::key.set(o, key);
  g_engine.calls.push_back("set_Key " + as(o)->name + " " + api::to_utf8(key));
}
// A PhotonMessageInfo as an RPC method receives it, by address: the sender at 8.
struct MessageInfo {
  int32_t time = 0;
  void* sender = nullptr;
  void* view = nullptr;
};
// The game's PageSync (decompiled, 0x22C41F0): a sender who does not own the book is reported (counted)
// and changes nothing; for the owner the number becomes the current page, every page goes off, then the
// page of that number comes on; a number outside the pages throws IndexOutOfRangeException there
// (counted), leaving the book blank.
void page_sync(void* journal, int32_t page, const void* info, const void*) {
  void* view = phasmo::JournalController::photonView.get(journal);
  void* owner = view ? phasmo::PhotonView::owner.get(view) : nullptr;
  void* sender = info ? static_cast<const MessageInfo*>(info)->sender : nullptr;
  if (sender && owner && sender != owner) {
    ++g_engine.reports;
    return;
  }
  phasmo::JournalController::currentPage.set(journal, page);
  const api::RefArray pages{phasmo::JournalController::pages.get(journal)};
  for (size_t i = 0; i < pages.size(); ++i) as(pages[i])->active = false;
  if (page < 0 || static_cast<size_t>(page) >= pages.size()) {
    ++g_engine.pageThrows;
    return;
  }
  as(pages[static_cast<size_t>(page)])->active = true;
}
// The page sync as the game reaches it, through whatever is hooked on it (the RPC on every client, or
// the local call from GoToPage, NextPage and PreviousPage).
using WPageSync = std::remove_cvref_t<decltype(phasmo::JournalController::PageSync)>;
// It is sent by `sender` (null: not known).
void sync_page(void* journal, int32_t page, void* sender) {
  MessageInfo info{0, sender, phasmo::JournalController::photonView.get(journal)};
  using B = api::detail::Bodies<WPageSync, api::detail::Hooked<WPageSync>>;
  if (!B::original[0]) return page_sync(journal, page, &info, nullptr);  // nothing hooked: the game's own
  B::entries[0](journal, page, &info, nullptr);
}
void* owner_of(void* journal) {
  void* view = phasmo::JournalController::photonView.get(journal);
  return view ? phasmo::PhotonView::owner.get(view) : nullptr;
}
// The game's GoToPage(int) (decompiled, 0x22C3140): the number held to 0 up to the page count, nothing
// for the current page, then the page sync with the view's owner as the sender: to every copy while the
// view is the local player's (this test's world holds only this copy), else on this client alone.
void go_to_page(void* journal, int32_t page, const void*) {
  const int32_t count = static_cast<int32_t>(api::RefArray{phasmo::JournalController::pages.get(journal)}.size());
  page = std::clamp(page, 0, count);
  if (page == phasmo::JournalController::currentPage.get(journal)) return;
  g_engine.page = page;
  sync_page(journal, page, owner_of(journal));
}
// The game's NextPage (0x22C34C0) and PreviousPage (0x22C4730): a step from the current page, held to the same bounds, then the
// page sync. Each holds its own copy of that code: neither calls GoToPage, and neither checks for the page it already holds.
void step_page(void* journal, int32_t step) {
  const int32_t count = static_cast<int32_t>(api::RefArray{phasmo::JournalController::pages.get(journal)}.size());
  sync_page(journal, std::clamp(phasmo::JournalController::currentPage.get(journal) + step, 0, count), owner_of(journal));
}
void next_page(void* journal) { step_page(journal, 1); }
void previous_page(void* journal) { step_page(journal, -1); }
bool is_mine(void*, const void*) { return g_engine.mine; }
int32_t get_guess(void*, const void*) { return g_engine.guess; }
void unselect_ghost(void*, const void*) { g_engine.guess = 24; }
// The game's RebuildGhosts (decompiled, 0x22C9950): each collection entry's ghost added to the list,
// and with the button of its type to the dictionary, by Add, which throws on a ghost already there.
void rebuild(void* journal, const void*) {
  g_engine.calls.push_back("RebuildGhosts");
  void* collection = phasmo::JournalController::allGhostsData.get(journal);
  for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(collection))) {
    const int32_t type = phasmo::GhostData::ghostType.get(data);
    g_engine.listed.push_back(type);
    const api::RefArray buttons{phasmo::JournalController::allGhostTypeButtons.get(journal)};
    for (size_t i = 0; i < buttons.size(); ++i) {
      if (phasmo::GhostButton::ghostType.get(buttons[i]) != type) continue;
      if (g_engine.paired.count(type)) {
        g_engine.threw = true;
        return;
      }
      g_engine.paired[type] = buttons[i];
      break;
    }
  }
}
// The game's SetGhostTypes (decompiled, 0x22C5090): a ghost paired with a button stays when its three
// evidence numbers hold every found evidence and none of the ruled-out ones.
void set_ghost_types(void* journal, const void*) {
  g_engine.calls.push_back("SetGhostTypes");
  g_engine.left.clear();
  void* collection = phasmo::JournalController::allGhostsData.get(journal);
  for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(collection))) {
    const int32_t type = phasmo::GhostData::ghostType.get(data);
    if (!g_engine.paired.count(type)) continue;
    const int32_t e[3] = {phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
                          phasmo::GhostData::evidence3.get(data)};
    const auto gives = [&](int32_t f) { return f == e[0] || f == e[1] || f == e[2]; };
    if (std::all_of(g_engine.found.begin(), g_engine.found.end(), gives) &&
        std::none_of(g_engine.ruledOut.begin(), g_engine.ruledOut.end(), gives))
      g_engine.left.push_back(type);
  }
}
// The game's RemoveEvidence (decompiled, 0x22C4AC0): the number off both lists, then the filter.
void remove_evidence(void* journal, int32_t evidence, const void*) {
  std::erase(g_engine.found, evidence);
  std::erase(g_engine.ruledOut, evidence);
  set_ghost_types(journal, nullptr);
}
// An evidence button turning to found: its override of the state change (decompiled, 0x1055030) runs
// AddEvidence's body (0x22C2940) with its evidence number on its journal.
void mark_found(void* journal, Node* button) {
  const int32_t evidence = phasmo::EvidenceButton::evidence.get(button);
  if (std::find(g_engine.found.begin(), g_engine.found.end(), evidence) == g_engine.found.end())
    g_engine.found.push_back(evidence);
  set_ghost_types(journal, nullptr);
}
int32_t get_language(const void*) { return g_engine.language; }
void* abbreviation(int32_t language, const void*) {
  return api::make_string(g_engine.codes.at(static_cast<size_t>(std::clamp<int32_t>(language, 0, 2))));
}
int32_t next_language(int32_t at, int32_t step, bool, const void*) {
  const int32_t to = at + step;
  return to >= 0 && to < static_cast<int32_t>(g_engine.codes.size()) ? to : at;
}
void never_called() {}
Node* g_journal = nullptr;  // the world's journal, whose sync controller Initialise reads
// List.Clear and Dictionary.Clear of the journal's two collections.
bool clear(void* c) {
  if (c == g_engine.failClear) return false;
  const bool list = c == phasmo::JournalController::allGhosts.get(g_journal).raw();
  g_engine.calls.push_back(list ? "Clear list" : "Clear dictionary");
  if (list) g_engine.listed.clear();
  else g_engine.paired.clear();
  return true;
}
bool has_clear(void* c) { return c != g_engine.missingClear; }
bool has_initialise(void*) { return !g_engine.missingInitialise; }
// The game's Initialise (decompiled, 0x3A9CC90) subscribes to every button it reads (the guess sync
// the journal's guess buttons, the evidence sync its evidence buttons): each one's delegate becomes a
// new one, the old one combined with the controller's.
bool initialise(void* controller) {
  if (++g_engine.initialised == g_engine.failInitialise) return false;
  const bool evidence = controller == phasmo::JournalController::evidenceSync.get(g_journal);
  g_engine.calls.push_back(evidence ? "Initialise evidence" : "Initialise");
  const api::RefArray buttons{evidence ? phasmo::JournalController::evidenceButtons.get(g_journal)
                                       : phasmo::JournalController::allGhostTypeButtons.get(g_journal)};
  for (size_t i = 0; i < buttons.size(); ++i)
    phasmo::NetworkThreeStateButton::userStateChanged.set(buttons[i], node("combined delegate"));
  return true;
}

// The game's own texts, as its localisation has them in English, German and French (whose pages show
// no layout), or with no layout at all (g_noLayout).
bool g_noLayout = false;
// The key forms of the game's ghost pages: a ghost's description key is g_pagePrefix and its name, its
// name key g_titlePrefix, its name and g_titleSuffix. The game's texts are keyed by the default page
// form, "Journal2_<name>", whatever form a test gives the pages.
std::string g_pagePrefix = "Journal2_", g_titlePrefix = "Journal_", g_titleSuffix = "Title";
std::string g_shortenedPage;  // the ghost whose description key holds only part of its name, as the game's Poltergeist's does
std::string g_prefixedPage;   // the ghost whose description key holds more than its name, as the game's Mimic's does
bool g_everyPageItsOwnForm = false;  // no two description keys share a form
void default_key_forms() {
  g_shortenedPage.clear();
  g_prefixedPage.clear();
  g_everyPageItsOwnForm = false;
  g_pagePrefix = "Journal2_";
  g_titlePrefix = "Journal_";
  g_titleSuffix = "Title";
}
std::optional<std::string> game_text(std::string_view key, std::string_view lang) {
  std::string k(key);
  if (k.starts_with(g_pagePrefix)) k = "Journal2_" + k.substr(g_pagePrefix.size());
  const bool de = lang == "de";
  if (g_noLayout && k.starts_with("Journal2_")) return "A " + k.substr(9) + ".";
  if (k == "Journal2_Mare")
    return de ? std::string("Ein Mare.\n \n Stärke: Dunkel.\n \n Schwäche: Licht.")
              : std::string("A Mare.\n \n Strength: Dark.\n \n Weakness: Light.");
  if (k == "Journal2_Banshee" && lang != "fr")
    return de ? std::string("Eine Banshee.\n \n Stärke: Schwächt.\n \n Schwäche: Mikrofon.")
              : std::string("A Banshee.\n \n Strength: Weakens.\n \n Weakness: Microphone.");
  if (k == "Journal2_Wraith" && lang == "en")
    return std::string("A Wraith.\n \n Strength: Flies.\n \n Weakness: Salt.");
  if (k == "Journal2_Spirit") return std::string("A Spirit.\n \n \n Weakness: Incense.");
  if (k.starts_with("Journal2_")) return "A " + k.substr(9) + ".";
  return std::nullopt;
}
// LocalisationSystem.LookUp in one language: the test asks it for English only.
bool look_up(int32_t language, void* key, void** out, const void*) {
  const auto t = game_text(api::to_utf8(key), language == 0 ? "en" : "?");
  if (!t) return false;
  *out = api::make_string(*t);
  return true;
}
// LocalisationSystem.TryGetLocalisedValue, the game's own: the text in the player's language (here
// English, the language the fake engine starts in), else false.
bool try_get(void* key, void** out, bool, const void*) { return look_up(0, key, out, nullptr); }

// The game's ghosts: (type, the name in its keys, its evidence). Evidence keys are "Ev_<value>".
struct Vanilla {
  int32_t type;
  const char* name;
  int32_t evidence[3];
};
const Vanilla kVanilla[] = {{0, "Spirit", {1, 2, 5}}, {1, "Wraith", {1, 2, 7}}, {4, "Banshee", {3, 4, 7}},
                            {5, "Jinn", {1, 3, 6}},   {6, "Mare", {2, 4, 5}},   {10, "Yurei", {4, 6, 7}}};

// The game's evidence rows: (value, row name). Each row's name key is "Ev_<value>", the line under it
// "Ev_<value>_Tip", as the game's are the name's key and a suffix.
struct VanillaEvidence {
  int32_t value;
  const char* name;
};
const VanillaEvidence kEvidenceRows[] = {{1, "EMF"}, {7, "DOTS"}, {2, "Spirit Box"}};

// A whole fake game: the contract (as content_test's), and one player's journal.
struct World {
  int controllerClass = 0, collectionClass = 0, dataClass = 0, levelClass = 0, gameObjectClass = 0, ghostButtonClass = 0,
      buttonClass = 0, localiserClass = 0, gridClass = 0, evidenceButtonClass = 0, layoutClass = 0, textClass = 0;
  void* controllerInstance = nullptr;
  void* levelInstance = nullptr;
  void *controller, *collection;
  Node *journal, *book, *grid, *list, *spiritButton, *spiritEntry, *spiritPage, *ghostsTab, *evidenceRows,
      *spiritBoxRow, *contentsA, *contentsB, *spiritRow;
  std::vector<Node*> vanillaButtons, vanillaRows;
  pm::host::ContentTypes types{fake::content_registry(), fake::acting(),
                               {&fake::list_insert, &fake::list_remove_at, &fake::list_add_value}};
  pm::host::ContentEvidence evidenceSide{fake::content_registry(), types,
                                         {&clear, &initialise, &has_clear, &has_initialise}};
  pm::host::ContentJournal journalSide{fake::content_registry(), types, {&clear, &initialise, &has_clear, &has_initialise},
                                       &evidenceSide};

  World() {
    g_engine = Engine{};
    fake::type(map::GhostController, &controllerClass);
    fake::type(map::GhostCollection, &collectionClass);
    fake::type(map::GhostData, &dataClass);
    fake::type(map::LevelController, &levelClass);
    fake::type(map::GameObject, &gameObjectClass);
    fake::type(map::GhostButton, &ghostButtonClass);
    fake::type(map::Button, &buttonClass);
    fake::type(map::TextLocaliserUI, &localiserClass);
    fake::type(map::GridLayoutGroup, &gridClass);
    fake::type(map::EvidenceButton, &evidenceButtonClass);
    fake::type(map::HorizontalOrVerticalLayoutGroup, &layoutClass);
    fake::type(map::TextMeshProUGUI, &textClass);
    fake::static_field(map::GhostController_instance, &controllerInstance, sizeof controllerInstance, true);
    fake::static_field(map::LevelController_instance, &levelInstance, sizeof levelInstance, true);
    fake::field(map::GhostController_allGhostsCollection, 0x90);
    fake::field(map::GhostController_mimicCopies, 0xC8);
    fake::field(map::GhostCollection_ghosts, 0x18);
    fake::field(map::GhostData_type, 0x18);
    fake::field(map::GhostData_ghostNameLocalisationKey, 0x20);
    fake::field(map::GhostData_evidence1, 0x28);
    fake::field(map::GhostData_evidence2, 0x2C);
    fake::field(map::GhostData_evidence3, 0x30);
    fake::field(map::LevelController_currentGhost, 0x40);
    // The journal's members at the build's own offsets, the fixture's being free to choose.
    fake::field(map::JournalController_pages, 0xC0);
    fake::field(map::JournalController_journalTabs, 0xC8);
    fake::field(map::JournalController_allGhostsData, 0x198);
    fake::field(map::JournalController_allGhostTypeButtons, 0x1A0);
    fake::field(map::JournalController_ghostSync, 0x1B8);
    fake::field(map::JournalController_allGhosts, 0x1D8);
    fake::field(map::JournalController_ghostButtons, 0x208);
    fake::field(map::JournalController_currentPage, 0xF8);
    fake::field(map::JournalController_photonView, 0x110);
    fake::field(map::PhotonView_owner, 0x80);
    fake::field(map::PhotonMessageInfo_Sender, offsetof(MessageInfo, sender));
    fake::field(map::JournalTab_min, 0x20);
    fake::field(map::JournalTab_max, 0x24);
    fake::field(map::GhostButton_type, 0x78);
    fake::field(map::NetworkThreeStateButton_userStateChanged, 0x58);
    fake::field(map::TextLocaliserUI_key, 0x30);
    fake::field(map::JournalController_evidenceButtons, 0x1A8);
    fake::field(map::JournalController_evidenceSync, 0x1B0);
    fake::field(map::EvidenceButton_evidence, 0x78);
    fake::field(map::EvidenceButton_descriptionLabel, 0x90);
    fake::field(map::ThreeStateButton_textLabel, 0x38);
    const auto m = [](const pm::sdk::EntryDesc& d, auto fn) { fake::method(d, reinterpret_cast<void*>(fn)); };
    m(map::UnityObject_get_name, &get_name);
    m(map::UnityObject_set_name, &set_name);
    m(map::UnityObject_Instantiate, &instantiate);
    m(map::GameObject_SetActive, &set_active);
    m(map::GameObject_get_activeSelf, &active_self);
    m(map::Component_get_transform, &component_transform);
    m(map::GameObject_get_transform, &self);
    m(map::Component_get_gameObject, &self);
    m(map::Component_GetComponent, &get_component);
    m(map::Component_GetComponentInChildren, &get_component_in_children);
    m(map::Behaviour_get_enabled, &get_enabled);
    m(map::Behaviour_set_enabled, &set_enabled);
    m(map::Transform_get_childCount, &child_count);
    m(map::Transform_GetChild, &get_child);
    m(map::Transform_GetSiblingIndex, &sibling_index);
    m(map::Transform_SetSiblingIndex, &set_sibling);
    m(map::RectTransform_get_rect, &get_rect);
    m(map::RectTransform_get_anchoredPosition, &get_position);
    m(map::RectTransform_set_anchoredPosition, &set_position);
    m(map::RectTransform_get_anchorMin, &get_anchor_min);
    m(map::RectTransform_set_anchorMin, &set_anchor_min);
    m(map::RectTransform_get_anchorMax, &get_anchor_max);
    m(map::RectTransform_set_anchorMax, &set_anchor_max);
    m(map::RectTransform_get_sizeDelta, &get_size_delta);
    m(map::RectTransform_set_sizeDelta, &set_size_delta);
    m(map::TMP_Text_set_text, &set_text);
    m(map::TMP_Text_get_text, &get_text);
    m(map::GridLayoutGroup_get_cellSize, &get_cell);
    m(map::GridLayoutGroup_set_cellSize, &set_cell);
    m(map::GridLayoutGroup_get_spacing, &get_spacing);
    m(map::GridLayoutGroup_get_constraint, &get_constraint);
    m(map::GridLayoutGroup_set_constraint, &set_constraint);
    m(map::GridLayoutGroup_get_constraintCount, &get_count);
    m(map::GridLayoutGroup_set_constraintCount, &set_count);
    m(map::TextLocaliserUI_set_Key, &set_key);
    m(map::JournalController_RebuildGhosts, &rebuild);
    m(map::JournalController_SetGhostTypes, &set_ghost_types);
    m(map::JournalController_GoToPage, &go_to_page);
    m(map::JournalController_PageSync, &page_sync);
    m(map::JournalController_GetGhostType, &get_guess);
    m(map::JournalController_UnselectGhost, &unselect_ghost);
    m(map::JournalController_RemoveEvidence, &remove_evidence);
    m(map::PhotonView_get_IsMine, &is_mine);
    m(map::LocalisationSystem_get_language, &get_language);
    m(map::LocalisationSystem_get_voiceLanguage, &get_language);
    m(map::LocalisationSystem_GetLanguageAbbreviationText, &abbreviation);
    m(map::LocalisationSystem_GetNextOrPreviousLanguage, &next_language);
    m(map::UIBehaviour_IsActive, &is_active);
    m(map::Selectable_IsInteractable, &is_interactable);
    m(map::LocalisationSystem_LookUp, &look_up);
    m(map::HorizontalOrVerticalLayoutGroup_get_spacing, &get_layout_spacing);
    m(map::HorizontalOrVerticalLayoutGroup_set_spacing, &set_layout_spacing);
    fake::icall(map::UnityObject_Destroy, reinterpret_cast<void*>(&destroy));
    fake::value(map::Language_English, 0);
    for (const pm::sdk::EntryDesc* d :
         {&map::JournalController_Awake, &map::Button_Press, &map::GhostAI_Init, &map::GhostInfo_SyncValues,
          &map::GhostInfo_SyncValuesNetworked, &map::GhostController_BuildMimicCopies, &map::GhostController_MimicCopy,
          &map::GhostHuntState_UpdateSpeed, &map::GhostController_PickEvidence, &map::GhostData_GetEvidenceList,
          &map::DifficultyUIManager_GhostTypeChosen,
          &map::ES3UserType_Difficulty_WriteScriptableObject, &map::DifficultyUIManager_SummaryText,
          &map::SystemEnum_ToString})
      fake::method(*d, reinterpret_cast<void*>(&never_called));
    m(map::LocalisationSystem_TryGetLocalisedValue, &try_get);
    fake::value(map::GridConstraint_FixedColumnCount, 1);
    for (const pm::sdk::EntryDesc* d :
         {&map::Scene_JournalBookContent, &map::Scene_JournalGhostGrid, &map::Scene_JournalGhostButton,
          &map::Scene_JournalGhostList, &map::Scene_JournalGhostEntry, &map::Scene_JournalGhostPage,
          &map::Scene_JournalContentsRows, &map::Scene_JournalContentsGhostRows, &map::Scene_JournalContentsGhostRow,
          &map::Scene_JournalGhostPageTitle, &map::Scene_JournalGhostPageDescription,
          &map::Scene_JournalGhostPageEvidence1, &map::Scene_JournalGhostPageEvidence2,
          &map::Scene_JournalGhostPageEvidence3, &map::Scene_JournalEvidenceRows, &map::Scene_JournalEvidenceButton})
      fake::scene_fact(*d);
    std::string missing;
    api::bind(fake::host(), missing);
    // The journal's hooks, as the host makes them (the page sync runs through its detour). A second world
    // without start() in between finds them made already; they reach the newest instance.
    journalSide.create_hooks();

    controller = phasmo::GhostController::type.make();
    collection = phasmo::GhostCollection::type.make();
    phasmo::GhostController::allGhostsCollection.set(controller, collection);
    std::vector<void*> ghosts;
    for (const Vanilla& v : kVanilla) {
      void* data = phasmo::GhostData::type.make();
      phasmo::GhostData::ghostType.set(data, v.type);
      phasmo::GhostData::ghostNameLocalisationKey.set(data, api::make_string(title_key(v.name)));
      phasmo::GhostData::evidence1.set(data, v.evidence[0]);
      phasmo::GhostData::evidence2.set(data, v.evidence[1]);
      phasmo::GhostData::evidence3.set(data, v.evidence[2]);
      ghosts.push_back(data);
    }
    std::sort(ghosts.begin(), ghosts.end(), [](void* a, void* b) {
      return api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(a)) <
             api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(b));
    });
    phasmo::GhostCollection::ghosts.set(collection, fake::list(ghosts));
    build_journal();
  }

  // The journal as the PCPlayer prefab ships it, listing the world's collection, made known to the
  // host's journal as its Awake does.
  void build_journal() {
    vanillaButtons.clear();
    vanillaRows.clear();
    journal = node("Journal2.0Canvas");
    g_journal = journal;
    Node* content = node("Content", journal);
    book = node("GhostHuntingBookContent", content);
    Node* evidence2 = node("Evidence.2", book);
    grid = node("All Ghosts", evidence2, kGrid);
    grid->constraint = 1;
    grid->constraintCount = 3;
    grid->cell = {250, 40};
    Node* select = node("Ghost Select Page", book);
    list = node("Ghost Types List", select, kGrid);
    list->enabled = false;  // shipped switched off
    list->cell = {280, 100};
    list->constraintCount = 2;
    Node* evidencePage = node("Evidence.1", book);
    Node* contentsPage = node("Contents A", book);
    std::vector<void*> pageObjects{contentsPage, evidencePage};
    // The Home tab's contents: the book's sections, then the ghosts over two pages, as the prefab has them.
    contentsA = node("Contents Group", contentsPage);
    contentsB = node("Ghost Types", node("Contents B", book));
    contents_row("Introduction", contentsA, "Journal2_IntroTitle", "3");
    contents_row("Ghost types", contentsA, "Journal2_GhostTypes", "26");
    // The evidence page: rows in a column, each an EvidenceButton with its name and the line under it.
    evidenceRows = node("Layout Group", evidencePage, kLayout);
    evidenceRows->spacing = 40;
    std::vector<void*> rows;
    for (const VanillaEvidence& e : kEvidenceRows) {
      Node* row = node(e.name, evidenceRows, kEvidenceButton | kButton);
      row->size = {400, 30};
      phasmo::EvidenceButton::evidence.set(row, e.value);
      phasmo::NetworkThreeStateButton::userStateChanged.set(row, row);  // its subscription, as Initialise made it
      node("OtherPlayerSelections", row);
      node("Box", row);
      Node* label = node("Text (TMP)", row, kLocaliser);
      phasmo::TextLocaliserUI::key.set(label, api::make_string("Ev_" + std::to_string(e.value)));
      Node* line = node("Text (TMP).1", row, kLocaliser);
      phasmo::TextLocaliserUI::key.set(line, api::make_string("Ev_" + std::to_string(e.value) + "_Tip"));
      phasmo::ThreeStateButton::textLabel.set(row, label);
      phasmo::EvidenceButton::descriptionLabel.set(row, line);
      rows.push_back(row);
      vanillaRows.push_back(row);
      if (std::string_view(e.name) == "Spirit Box") spiritBoxRow = row;
    }
    void* rowArray = phasmo::EvidenceButton::type.make_array(rows.size());
    for (size_t r = 0; r < rows.size(); ++r) api::RefArray{rowArray}.set(r, rows[r]);
    std::vector<void*> buttons;
    std::vector<std::string> names;
    for (const Vanilla& v : kVanilla) names.emplace_back(v.name);
    std::sort(names.begin(), names.end());
    int i = 0;
    for (const std::string& name : names) {
      const Vanilla& v = *std::find_if(std::begin(kVanilla), std::end(kVanilla), [&](const Vanilla& x) { return name == x.name; });
      Node* b = node(name, grid, kGhostButton | kButton);
      phasmo::GhostButton::ghostType.set(b, v.type);
      phasmo::NetworkThreeStateButton::userStateChanged.set(b, b);  // its subscription, as Initialise made it
      Node* text = node(name + "Text (TMP)", b, kLocaliser);
      phasmo::TextLocaliserUI::key.set(text, api::make_string(title_key(name)));
      node("OtherPlayerSelections Ghosts Variant", b);
      buttons.push_back(b);
      vanillaButtons.push_back(b);
      Node* entry = node(name, list, kButton);
      entry->position = {125.0f + 250.5f * (i % 3), -50.0f - 102.5f * (i / 3)};
      entry->size = entry->sizeDelta = {250, 100};
      entry->anchorMin = entry->anchorMax = {0.5f, 1.0f};  // placed by hand from the top's middle
      Node* entryText = node("Text (TMP)", entry, kLocaliser);
      phasmo::TextLocaliserUI::key.set(entryText, api::make_string(title_key(name)));
      Node* row = contents_row(name, name < "Hantu" ? contentsA : contentsB, title_key(name), std::to_string(30 + i));
      if (name == "Spirit") spiritRow = row;
      Node* page = node(name, book);
      page_text(page, "Title", title_key(name));
      node("------------", page);
      page_text(page, "Description", g_pagePrefix + (g_everyPageItsOwnForm ? std::to_string(i) + "x" : "") +
                                         (name == g_prefixedPage ? "The" : "") +
                                         (name == g_shortenedPage ? name.substr(0, 3) : name));
      page_text(page, "Evidence", "Journal_Evidence");
      for (int k = 0; k < 3; ++k)
        page_text(page, k == 1 ? "Evidence 2 " : "Evidence " + std::to_string(k + 1),
                  "Ev_" + std::to_string(v.evidence[k]));
      pageObjects.push_back(page);
      if (name == "Spirit") spiritButton = b, spiritEntry = entry, spiritPage = page;
      ++i;
    }
    void* pageArray = phasmo::GameObject::type.make_array(pageObjects.size());
    g_gameObjects.clear();
    g_gameObjects.insert(pageObjects.begin(), pageObjects.end());
    for (size_t p = 0; p < pageObjects.size(); ++p) api::RefArray{pageArray}.set(p, pageObjects[p]);
    phasmo::JournalController::pages.set(journal, pageArray);
    void* buttonArray = phasmo::GhostButton::type.make_array(buttons.size());
    for (size_t b = 0; b < buttons.size(); ++b) api::RefArray{buttonArray}.set(b, buttons[b]);
    phasmo::JournalController::allGhostTypeButtons.set(journal, buttonArray);
    phasmo::JournalController::allGhostsData.set(journal, collection);
    phasmo::JournalController::ghostSync.set(journal, static_cast<void*>(node("Evidence.2 sync")));
    phasmo::JournalController::evidenceButtons.set(journal, rowArray);
    phasmo::JournalController::evidenceSync.set(journal, static_cast<void*>(node("Evidence.1 sync")));
    Node* view = node("PhotonView");
    phasmo::PhotonView::owner.set(view, node("Player 1"));  // the book's owner
    phasmo::JournalController::photonView.set(journal, static_cast<void*>(view));
    phasmo::JournalController::allGhosts.set(journal, fake::list({}));
    phasmo::JournalController::ghostButtons.set(journal, fake::list({}));
    // Two tabs: the Ghosts tab covers the last page.
    void* tabs = fake::array(sizeof(void*), 2);
    Node* home = node("Home tab");
    ghostsTab = node("Ghosts tab");
    // As the PCPlayer prefab has them: the Ghosts tab's max is the page count, one past its last page.
    phasmo::JournalTab::min.set(home, 0.0f);
    phasmo::JournalTab::max.set(home, 1.0f);
    phasmo::JournalTab::min.set(ghostsTab, 2.0f);
    phasmo::JournalTab::max.set(ghostsTab, static_cast<float>(pageObjects.size()));
    api::RefArray{tabs}.set(0, home);
    api::RefArray{tabs}.set(1, ghostsTab);
    phasmo::JournalController::journalTabs.set(journal, tabs);
    const auto at = [&](const pm::sdk::EntryDesc& d, Node* n) { fake::child(journal, d.path, n); };
    at(map::Scene_JournalBookContent, book);
    at(map::Scene_JournalGhostGrid, grid);
    at(map::Scene_JournalGhostButton, spiritButton);
    at(map::Scene_JournalGhostList, list);
    at(map::Scene_JournalGhostEntry, spiritEntry);
    at(map::Scene_JournalGhostPage, spiritPage);
    at(map::Scene_JournalContentsRows, contentsA);
    at(map::Scene_JournalContentsGhostRows, contentsB);
    at(map::Scene_JournalContentsGhostRow, spiritRow);
    at(map::Scene_JournalGhostPageTitle, spiritPage->children[0]);
    at(map::Scene_JournalGhostPageDescription, spiritPage->children[2]);
    at(map::Scene_JournalGhostPageEvidence1, spiritPage->children[4]);
    at(map::Scene_JournalGhostPageEvidence2, spiritPage->children[5]);
    at(map::Scene_JournalGhostPageEvidence3, spiritPage->children[6]);
    at(map::Scene_JournalEvidenceRows, evidenceRows);
    at(map::Scene_JournalEvidenceButton, spiritBoxRow);
    journalSide.after_awake(journal);
  }
  static std::string title_key(const std::string& name) { return g_titlePrefix + name + g_titleSuffix; }
  static Node* contents_row(const std::string& name, Node* parent, const std::string& key, const std::string& number) {
    Node* row = node(name, parent, kLocaliser);
    phasmo::TextLocaliserUI::key.set(row, api::make_string(key));
    row->sizeDelta = {310, 60};
    Node* n = node("PageNumber", row, kText);
    n->sizeDelta = {50, 60};
    n->text = number;
    return row;
  }
  std::vector<std::string> page_names() const {
    std::vector<std::string> out;
    const api::RefArray pages{phasmo::JournalController::pages.get(journal)};
    for (size_t p = 0; p < pages.size(); ++p) out.push_back(as(pages[p])->name);
    return out;
  }
  // The page of that name's number in the book.
  int32_t page_number(const std::string& name) const {
    const api::RefArray pages{phasmo::JournalController::pages.get(journal)};
    for (size_t p = 0; p < pages.size(); ++p)
      if (as(pages[p])->name == name) return static_cast<int32_t>(p);
    return -1;
  }
  static void page_text(Node* page, const std::string& name, const std::string& key) {
    Node* n = node(name, page, kLocaliser);
    phasmo::TextLocaliserUI::key.set(n, api::make_string(key));
  }
  // A few frames as the host runs them (host/suite.cpp's tick_content), each ending with the engine's
  // layout pass over the journal's grids.
  void frames(int n = 3) {
    for (int i = 0; i < n; ++i) {
      types.tick(fake::content_state(), true, true);
      evidenceSide.tick(journalSide.journals());
      journalSide.tick(true);
      lay_out(grid);
      lay_out(list);
    }
  }
  // Content decided as the host decides it, and the journal given its ghosts.
  void contract() {
    controllerInstance = controller;
    frames();
  }
  // The lobby: no contract loaded, the journal given what the lobby shows.
  void lobby() {
    controllerInstance = nullptr;
    frames();
  }
  // A scene load: the journal goes with its scene, and the next scene's player book wakes, listing the
  // same collection, its Awake building its lists from it.
  void reload() {
    fake::destroy(journal);
    g_engine.paired.clear();
    g_engine.listed.clear();
    build_journal();
    rebuild(journal, nullptr);
  }
  int32_t pages() const { return static_cast<int32_t>(api::RefArray{phasmo::JournalController::pages.get(journal)}.size()); }
  std::vector<std::string> names(Node* parent) const {
    std::vector<std::string> out;
    for (Node* c : parent->children) out.push_back(c->name);
    return out;
  }
  // The child of that name not destroyed (a copy taken out stays among the children until the frame's
  // end).
  Node* child_named(Node* parent, const std::string& name) const {
    for (Node* c : parent->children)
      if (c->name == name && !c->destroyed) return c;
    return nullptr;
  }
  static std::string key(Node* localiser) { return api::to_utf8(phasmo::TextLocaliserUI::key.get(localiser)); }
};

// A fresh fake game. The scene's nodes are freed here, with the fake forgetting which objects were
// destroyed, never while it still remembers: a new node at a freed address must not read as destroyed.
void start() {
  g_nodes.clear();
  fake::reset();
  default_key_forms();
  g_lines.clear();
  pm::log::set_sink(&capture, "");
}

content::GhostDef ghost(const std::string& id, const std::string& name) {
  content::GhostDef g;
  g.id = id;
  g.name = content::Text("en", name);
  g.description = content::Text("en", name + " haunts.");
  g.strength = content::Text("en", "It is strong.");
  g.weakness = content::Text("en", "It is weak.");
  g.tells = {content::Text("en", "It tells.")};
  g.evidence = {"emf", "orbs", "freezing"};
  return g;
}

// Registers `ghosts`, commits, and opens a private room of one; the content is active.
void runs(const std::vector<content::GhostDef>& ghosts) {
  for (const content::GhostDef& g : ghosts) CHECK(content::register_ghost(g) > 0);
  fake::commit_content();
  fake::room(1, {1});
  CHECK(fake::content_state() == pm::content::State::Active);
}

// The content hooks come only once the player has agreed, and the local player's book may have woken while the notice
// waited: it is found, inactive or not, and noted once, as Awake's hook notes a later one.
void* g_foundJournals = nullptr;
void* find_journals(void*, bool includeInactive, const void*) {
  CHECK(includeInactive);
  return g_foundJournals;
}
void a_book_awake_before_the_hooks_is_noted() {
  World w;
  fake::method(map::UnityObject_FindObjectsOfType_inactive, reinterpret_cast<void*>(&find_journals));
  std::string missing;
  api::bind(fake::host(), missing);
  g_foundJournals = fake::array(sizeof(void*), 1);
  api::RefArray{g_foundJournals}.set(0, w.journal);
  CHECK(w.journalSide.journals_known() == 1);  // Awake's hook saw this one
  w.journalSide.note_existing();
  CHECK(w.journalSide.journals_known() == 1);  // the same book is not noted twice

  pm::host::ContentJournal late{fake::content_registry(), w.types, {&clear, &initialise, &has_clear, &has_initialise}, nullptr};
  CHECK(late.journals_known() == 0);
  late.note_existing();
  CHECK(late.journals_known() == 1);
  g_foundJournals = nullptr;  // nothing to find: nothing noted, nothing thrown
  pm::host::ContentJournal none{fake::content_registry(), w.types, {&clear, &initialise, &has_clear, &has_initialise}, nullptr};
  none.note_existing();
  CHECK(none.journals_known() == 0);
}

// The game's keys aren't all regular: the Poltergeist's page is Journal2_Polter. That page says nothing about the form the others
// agree on, and it must not cost every journal its custom ghosts, which is what a live run showed.
void a_page_key_without_its_name_says_nothing_of_the_form() {
  start();
  g_shortenedPage = "Banshee";
  World w;
  const int32_t gamePages = w.pages();
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  CHECK(w.pages() == gamePages + 2);
  const bool refused = std::any_of(g_lines.begin(), g_lines.end(), [](const std::string& line) {
    return line.find("no one description key form") != std::string::npos;
  });
  CHECK(!refused);
}

// The game's Mimic's page is Journal2_TheMimic: that holds its name, in a form no other page has. The form is what most pages agree
// on, so it costs nothing, which is what the second live run showed.
void a_page_key_in_another_form_does_not_decide_the_form() {
  start();
  g_prefixedPage = "Banshee";
  World w;
  const int32_t gamePages = w.pages();
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  CHECK(w.pages() == gamePages + 2);
  if (w.pages() != gamePages + 2) return;
  // A custom page's key is in the form the others agree on, not the odd one's.
  Node* page = w.child_named(w.book, "Lethe");
  CHECK(World::key(page->children[2]).starts_with("Journal2_") && !World::key(page->children[2]).starts_with("Journal2_The"));
}

// With no form most pages agree on, the host can't name a custom page the game's way, so it adds none and says so.
void pages_that_agree_on_no_form_get_no_custom_ghosts() {
  start();
  g_everyPageItsOwnForm = true;
  World w;
  const int32_t gamePages = w.pages();
  runs({ghost("a.lethe", "Lethe")});
  w.lobby();
  CHECK(w.pages() == gamePages);
  CHECK(std::any_of(g_lines.begin(), g_lines.end(), [](const std::string& line) {
    return line.find("no one description key form") != std::string::npos;
  }));
}

// The Home tab lists every ghost alphabetically with its page number, so a custom ghost joins that list: on the page its
// name falls on, after the rows that name the book's sections, showing the number its own page shows.
void a_custom_ghost_joins_the_home_tabs_contents() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion"), ghost("a.zennor", "Zennor")});
  w.lobby();
  CHECK(w.names(w.contentsA) == std::vector<std::string>({"Introduction", "Ghost types", "Asterion", "Banshee"}));
  CHECK(w.names(w.contentsB) == std::vector<std::string>({"Jinn", "Lethe", "Mare", "Spirit", "Wraith", "Yurei", "Zennor"}));
  for (const char* name : {"Lethe", "Asterion", "Zennor"}) {
    Node* row = w.child_named(name == std::string("Asterion") ? w.contentsA : w.contentsB, name);
    CHECK(row);
    if (!row) continue;
    // The page sync writes a page's number plus 2 in the corner a ghost's page lies in.
    CHECK(row->children[0]->text == std::to_string(w.page_number(name) + 2));
    CHECK(World::key(row) == World::key(w.child_named(w.book, name)->children[0]));
  }
  // A game's ghost has moved along the book, and its row says so.
  CHECK(w.spiritRow->children[0]->text == std::to_string(w.page_number("Spirit") + 2));
  // The rows that name the book's sections lie before the ghosts, and keep what the prefab says.
  CHECK(w.child_named(w.contentsA, "Introduction")->children[0]->text == "3");
  CHECK(w.child_named(w.contentsA, "Ghost types")->children[0]->text == "26");
}

// The contents count up: the book's ghost pages take the order the rows list them in, whatever order they came in. The game's own
// rows aren't quite in order (its Obambo sits above its Obake), and it prints four numbers that aren't their pages'.
void the_contents_numbers_count_up_over_both_pages() {
  start();
  World w;
  set_sibling(w.child_named(w.contentsB, "Mare"), 0, nullptr);  // above the Jinn, as the game's Obambo is above its Obake
  runs({ghost("a.zennor", "Zennor"), ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  CHECK(w.names(w.contentsA) == std::vector<std::string>({"Introduction", "Ghost types", "Asterion", "Banshee"}));
  CHECK(w.names(w.contentsB) == std::vector<std::string>({"Jinn", "Lethe", "Mare", "Spirit", "Wraith", "Yurei", "Zennor"}));
  const api::RefArray pages{phasmo::JournalController::pages.get(w.journal)};
  int32_t expected = w.page_number("Asterion") + 2;
  CHECK(expected == 4);  // the first ghost's place in the book, where the game's first ghost was
  if (expected != 4) return;
  for (Node* page : {w.contentsA, w.contentsB})
    for (Node* row : page->children) {
      if (row->name == "Introduction" || row->name == "Ghost types") continue;
      CHECK(row->children[0]->text == std::to_string(expected));
      // The number leads to that ghost's page.
      CHECK(World::key(as(pages[static_cast<size_t>(expected - 2)])->children[0]) == World::key(row));
      ++expected;
    }
  CHECK(expected - 2 == w.pages());
}

// The pages before the ghosts keep their numbers: the game's tabs and links name them by number.
void the_pages_before_the_ghosts_keep_their_numbers() {
  start();
  World w;
  const std::vector<std::string> before = w.page_names();
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  const std::vector<std::string> after = w.page_names();
  CHECK(after[0] == before[0] && after[1] == before[1]);
  CHECK(after[2] == "Asterion");
}

// A player reading a game's ghost when the custom pages come in: the page's number has changed under them, so their book turns to the
// page its Ghosts tab opens, as when content stops.
void a_reader_on_a_moved_page_is_turned_to_the_ghosts_tab() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  phasmo::JournalController::GoToPage(static_cast<void*>(w.journal), w.page_number("Spirit"));
  const int32_t tabFirst = static_cast<int32_t>(phasmo::JournalTab::min.get(w.ghostsTab));
  w.lobby();
  CHECK(phasmo::JournalController::currentPage.get(w.journal) == tabFirst);
  // A reader on a page before the ghosts stays there.
  start();
  World other;
  runs({ghost("a.lethe", "Lethe")});
  phasmo::JournalController::GoToPage(static_cast<void*>(other.journal), 1);
  other.lobby();
  CHECK(phasmo::JournalController::currentPage.get(other.journal) == 1);
}

// Every row on both pages gets shorter by one factor, the fuller page's, and their numbers with them: the texts size
// themselves to the row, so they stay level and every row fits in the height the game's filled.
void the_contents_rows_shorten_together_to_fit() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  // 3 rows became 4 on the first page, 5 became 6 on the second: 3/4 is the smaller.
  for (Node* page : {w.contentsA, w.contentsB})
    for (Node* row : page->children) {
      CHECK(std::fabs(row->sizeDelta.y - 45.0f) < 0.01f);
      CHECK(std::fabs(row->children[0]->sizeDelta.y - 45.0f) < 0.01f);
      CHECK(row->sizeDelta.x == 310.0f);
    }
}

// A custom page's number means nothing to a client without the mods, and a buffered one would reach every later joiner. So
// host/lobby_gate.cpp asks, from the view it is sent on, whether a page sync's number means another page here than in the book as the
// game made it. A view no journal holds has no custom pages.
void a_custom_page_is_told_from_the_view_it_is_sent_on() {
  start();
  World w;
  const int32_t gamePages = w.pages();
  CHECK(!w.journalSide.custom_page(w.journal, gamePages - 1) && w.journalSide.custom_page(w.journal, gamePages));
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.lobby();
  CHECK(w.pages() == gamePages + 2);
  // The Asterion takes the first ghost's place: from there on a number is another page than the game's.
  const int32_t firstMoved = w.page_number("Asterion");
  CHECK(firstMoved < gamePages - 1);
  CHECK(!w.journalSide.custom_page(w.journal, firstMoved - 1));
  CHECK(w.journalSide.custom_page(w.journal, firstMoved) && w.journalSide.custom_page(w.journal, gamePages - 1));
  CHECK(w.journalSide.custom_page(w.journal, gamePages + 2));
  void* view = phasmo::JournalController::photonView.get(w.journal);
  CHECK(view != nullptr);
  CHECK(!w.journalSide.custom_page_on(view, firstMoved - 1));
  CHECK(w.journalSide.custom_page_on(view, firstMoved) && w.journalSide.custom_page_on(view, gamePages + 2));
  int otherView = 0;
  CHECK(!w.journalSide.custom_page_on(&otherView, gamePages + 2));
  CHECK(!w.journalSide.custom_page_on(nullptr, gamePages + 2));
}

void guess_button_crossed_out_by_filter() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  w.contract();
  // A copy of the Spirit's button per custom ghost (types 31 Asterion, 32 Lethe), named, of its type, its
  // text the name key, in its alphabetical place.
  CHECK((w.names(w.grid) ==
         std::vector<std::string>{"Asterion", "Banshee", "Jinn", "Lethe", "Mare", "Spirit", "Wraith", "Yurei"}));
  Node* lethe = w.child_named(w.grid, "Lethe");
  CHECK(lethe && phasmo::GhostButton::ghostType.get(lethe) == 32);
  CHECK(lethe && World::key(lethe->children[0]) == "Journal_32Title");
  CHECK(lethe && lethe->children.size() == w.spiritButton->children.size());  // the other players' marks too
  // In the journal's buttons, and paired with its ghost by the journal's own builder after the lists
  // were emptied; then the evidence filter runs over them.
  const api::RefArray buttons{phasmo::JournalController::allGhostTypeButtons.get(w.journal)};
  CHECK(buttons.size() == 8);
  CHECK(g_engine.paired[32] == lethe && g_engine.paired[31] == w.child_named(w.grid, "Asterion"));
  CHECK(g_engine.listed.size() == 8);
  const auto& c = g_engine.calls;
  const auto clear = std::find(c.begin(), c.end(), "Clear dictionary"), rebuilt = std::find(c.begin(), c.end(), "RebuildGhosts"),
             filter = std::find(c.begin(), c.end(), "SetGhostTypes");
  CHECK(clear < rebuilt && rebuilt < filter && filter != c.end());
  // The sync controller read the buttons again; the game's own buttons keep their one subscription.
  CHECK(g_engine.initialised == 1);
  for (Node* b : w.vanillaButtons) CHECK(phasmo::NetworkThreeStateButton::userStateChanged.get(b).raw() == b);
  CHECK(lethe && as(phasmo::NetworkThreeStateButton::userStateChanged.get(lethe))->name == "combined delegate");
  // The grid's rows are shorter: 8 buttons in 3 columns fill the height 6 did (2 rows to 3).
  CHECK(std::fabs(3 * (w.grid->cell.y + 2.5f) - 2 * (40 + 2.5f)) < 0.01f);
  w.contract();  // extended once
  CHECK(api::RefArray{phasmo::JournalController::allGhostTypeButtons.get(w.journal)}.size() == 8);
}

void list_entry_alphabetical_and_leads_to_page() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const int32_t firstPage = w.pages();
  // The game's own list, as placed by hand: how many entries and how tall each is.
  const int32_t gameEntries = static_cast<int32_t>(w.list->children.size());
  const float entryHeight = w.spiritEntry->size.y;
  w.contract();
  CHECK((w.names(w.list) ==
         std::vector<std::string>{"Asterion", "Banshee", "Jinn", "Lethe", "Mare", "Spirit", "Wraith", "Yurei"}));
  // The list's grid is set to the placement its entries were given and switched on.
  CHECK(w.list->enabled && w.list->constraint == 1 && w.list->constraintCount == 3 && w.list->cell.x == 250.0f);
  // Its rows are shorter, so every entry fits in the height the game's own filled.
  const int32_t columns = w.list->constraintCount, entries = static_cast<int32_t>(w.list->children.size());
  const float gap = get_spacing(w.list, nullptr).y;
  const int32_t rowsBefore = (gameEntries + columns - 1) / columns, rowsAfter = (entries + columns - 1) / columns;
  CHECK(rowsAfter > rowsBefore);  // the custom entries start a row
  CHECK(std::fabs(rowsAfter * (w.list->cell.y + gap) - rowsBefore * (entryHeight + gap)) < 0.01f);
  // Pages among the book's ghosts, in the contents' order; the Ghosts tab covers them.
  CHECK(w.pages() == firstPage + 2);
  CHECK(std::lround(phasmo::JournalTab::max.get(w.ghostsTab)) == firstPage + 2);  // moved by the 2 pages added
  Node* asterion = w.child_named(w.list, "Asterion");
  Node* lethe = w.child_named(w.list, "Lethe");
  CHECK(asterion && lethe && World::key(lethe->children[0]) == "Journal_32Title");
  CHECK(w.journalSide.page_of(asterion) == w.page_number("Asterion") && w.journalSide.page_of(lethe) == w.page_number("Lethe"));
  CHECK(w.journalSide.press(lethe) && g_engine.page == w.page_number("Lethe"));
  g_engine.page = -1;
  CHECK(!w.journalSide.press(w.spiritEntry) && g_engine.page == -1);  // the game's own entries are the game's
  // As Press does nothing for a button not interactable or not active, a custom entry then turns no page.
  lethe->interactable = false;
  CHECK(w.journalSide.press(lethe) && g_engine.page == -1);
  lethe->interactable = true;
  lethe->active = false;
  CHECK(w.journalSide.press(lethe) && g_engine.page == -1);
  // The page: its name, its description key in the game's form, its evidence by the game's own keys.
  void* page = api::RefArray{phasmo::JournalController::pages.get(w.journal)}[static_cast<size_t>(w.page_number("Lethe"))];
  Node* p = as(page);
  CHECK(p->name == "Lethe" && p->active);  // made off; the press turned the book to it
  CHECK(World::key(p->children[0]) == "Journal_32Title");
  CHECK(World::key(p->children[2]) == "Journal2_32");
  CHECK(World::key(p->children[4]) == "Ev_1" && World::key(p->children[5]) == "Ev_4" &&
        World::key(p->children[6]) == "Ev_6");
}

// Writing (5) shows on the Spirit's and the Mare's pages only, beside the Spirit Box on both: no one key
// is the writing's, so a page that gives it hides that text rather than guess.
void unknown_evidence_text_is_hidden() {
  start();
  World w;
  content::GhostDef g = ghost("a.lethe", "Lethe");
  g.evidence = {"emf", "writing", "freezing"};
  runs({g});
  w.contract();
  Node* p = w.child_named(w.book, "Lethe");
  CHECK(p->children[4]->active && !p->children[5]->active && p->children[6]->active);
}

// A ghost that gives no evidence (the Squatter's spec, "Technical unknowns" 6): its GhostData holds the game's "no
// evidence" in all three slots, so its page names no evidence (its three evidence lines hidden), and the game's own
// filter (SetGhostTypes, sdk/systems/evidence.md) lists it while nothing is found, keeps it when evidence is only
// ruled out (none of its three is that evidence), and crosses it out once any evidence is found (no found evidence
// is one of its three).
void no_evidence_ghost_stays_until_evidence_is_marked() {
  start();
  World w;
  content::GhostDef quiet = ghost("a.quiet", "Quiet");
  quiet.evidence = {"", "", ""};
  runs({quiet, ghost("a.lethe", "Lethe")});
  w.contract();
  const int32_t q = content::type_of("a.quiet"), lethe = content::type_of("a.lethe");
  const auto listed = [](int32_t type) {
    return std::find(g_engine.left.begin(), g_engine.left.end(), type) != g_engine.left.end();
  };
  Node* page = w.child_named(w.book, "Quiet");
  CHECK(page && !page->children[4]->active && !page->children[5]->active && !page->children[6]->active);
  CHECK(listed(q) && listed(lethe));  // nothing marked: the contract's own filter keeps both
  g_engine.ruledOut = {1};             // EMF ruled out: the Lethe gives it, the quiet ghost does not
  set_ghost_types(w.journal, nullptr);
  CHECK(listed(q) && !listed(lethe));
  g_engine.ruledOut.clear();
  set_ghost_types(w.journal, nullptr);
  CHECK(listed(q) && listed(lethe));
  mark_found(w.journal, w.vanillaRows[0]);  // EMF found
  CHECK(!listed(q) && listed(lethe));
  mark_found(w.journal, w.vanillaRows[2]);  // and the Spirit Box, which the Lethe lacks
  CHECK(!listed(q) && !listed(lethe));
}

std::vector<std::pair<std::string, int32_t>> pages_by_id(const std::vector<content::GhostDef>& ghosts) {
  start();
  World w;
  runs(ghosts);
  w.contract();
  std::vector<std::pair<std::string, int32_t>> out;
  for (const auto& [type, h] : w.types.held())
    out.emplace_back(h.id, w.journalSide.page_of(w.child_named(w.list, pm::host::pick(h.name, "en"))));
  std::sort(out.begin(), out.end());
  return out;
}

void page_numbers_match_across_clients() {
  // Two clients with the same content, registered in another order.
  const auto one = pages_by_id({ghost("b.zephyr", "Zephyr"), ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const auto two = pages_by_id({ghost("a.asterion", "Asterion"), ghost("b.zephyr", "Zephyr"), ghost("a.lethe", "Lethe")});
  CHECK(one == two && one.size() == 3);
  for (const auto& [id, page] : one) CHECK(page > 0);
}

void missing_language_falls_back_to_english() {
  start();
  World w;
  content::GhostDef lethe = ghost("a.lethe", "Lethe");
  lethe.name.add("de", "Lethe (de)");
  lethe.description.add("de", "Sie spukt.");
  runs({lethe, ghost("a.asterion", "Asterion")});
  w.contract();
  const auto in = [](std::string_view lang) {
    return [lang](std::string_view k, bool english) { return game_text(k, english ? "en" : lang); };
  };
  // German: the ghost with German shows it, in its list entry's name and on its page; the other English.
  CHECK(w.types.name_for_key("Journal_32Title", "de") == std::optional<std::string>("Lethe (de)"));
  CHECK(w.types.name_for_key("Journal_31Title", "de") == std::optional<std::string>("Asterion"));
  const auto page = w.journalSide.text_for_key("Journal2_32", "de", in("de"));
  CHECK(page && page->starts_with("Sie spukt.\n \n Stärke: It is strong.\n \n Schwäche: It is weak."));
  const auto other = w.journalSide.text_for_key("Journal2_31", "de", in("de"));
  CHECK(other && other->starts_with("Asterion haunts.\n \n Stärke: It is strong."));
  // English, laid out as the game's English pages, the tells after.
  CHECK(w.journalSide.text_for_key("Journal2_32", "en", in("en")) ==
        std::optional<std::string>("Lethe haunts.\n \n Strength: It is strong.\n \n Weakness: It is weak.\n \n It tells."));
  // English and German are the game's: nothing is logged about them.
  CHECK(!logged("a language the game does not have"));
  // French pages agree on no layout: the English pages' layout, the French text where the ghost has it.
  const auto fr = w.journalSide.text_for_key("Journal2_32", "fr", in("fr"));
  CHECK(fr && fr->starts_with("Lethe haunts.\n \n Strength: It is strong."));
  // No page agrees on a layout in any language: no text is made up, and the log says so once.
  g_noLayout = true;
  CHECK(!w.journalSide.text_for_key("Journal2_32", "en", in("en")));
  CHECK(!w.journalSide.text_for_key("Journal2_32", "en", in("en")));
  g_noLayout = false;
  CHECK(std::count_if(g_lines.begin(), g_lines.end(),
                      [](const std::string& l) { return l.find("agree on no layout") != std::string::npos; }) == 1);
}

void language_switch_rewrites_pages() {
  start();
  World w;
  content::GhostDef lethe = ghost("a.lethe", "Lethe");
  lethe.name.add("fr", "Léthé");
  lethe.name.add("xx", "Nowhere");
  runs({lethe});
  w.contract();
  CHECK(logged("the ghost \"a.lethe\" has text in \"xx\", a language the game does not have"));
  // The copies keep the game's localisers with the host's keys, so the game writes them again when its
  // language changes; what the keys answer follows the language it asks in.
  Node* entry = w.child_named(w.list, "Lethe");
  CHECK(entry && World::key(entry->children[0]) == "Journal_31Title");
  g_engine.language = 0;
  CHECK(pm::host::game_language(true) == "en");
  CHECK(w.types.name_for_key("Journal_31Title", pm::host::game_language(true)) == std::optional<std::string>("Lethe"));
  g_engine.language = 2;
  CHECK(pm::host::game_language(true) == "fr");
  CHECK(w.types.name_for_key("Journal_31Title", pm::host::game_language(true)) == std::optional<std::string>("Léthé"));
  CHECK((pm::host::game_languages() == std::vector<std::string>{"en", "de", "fr"}));
}

void vanilla_lines_from_two_mods_both_show() {
  start();
  World w;
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "First mod's line.").add("de", "Erste.")}) > 0);
  fake::content_registry().end_group(true);
  fake::content_registry().begin_group();
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "Second mod's line.")}) > 0);
  CHECK(content::add_journal_lines("vanilla.wisp", {content::Text("en", "x")}) == 0);  // no such ghost
  CHECK(content::add_journal_lines("a.lethe", {content::Text("en", "x")}) == 0);       // not a vanilla ghost
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("de", "x")}) == 0);  // no English
  fake::commit_content();
  fake::room(1, {1});
  w.contract();  // no custom ghosts: the journal still learns its pages
  const auto in = [](std::string_view lang) {
    return [lang](std::string_view k, bool english) { return game_text(k, english ? "en" : lang); };
  };
  CHECK(w.journalSide.answers());
  CHECK(w.journalSide.text_for_key("Journal2_Mare", "en", in("en")) ==
        std::optional<std::string>("A Mare.\n \n Strength: Dark.\n \n Weakness: Light.\n \n First mod's line.\n \n "
                                   "Second mod's line."));
  const auto de = w.journalSide.text_for_key("Journal2_Mare", "de", in("de"));
  CHECK(de && de->ends_with("Licht.\n \n Erste.\n \n Second mod's line."));
  CHECK(!w.journalSide.text_for_key("Journal2_Spirit", "en", in("en")));  // no lines: the game's own
  // The Mare's page writes its text again so the lines show.
  CHECK(std::find(g_engine.calls.begin(), g_engine.calls.end(), "set_Key Description Journal2_Mare") != g_engine.calls.end());
  // Outside a private room nothing is added.
  w.journalSide.tick(false);
  CHECK(!w.journalSide.text_for_key("Journal2_Mare", "en", in("en")));
}

// The room's mod agreement (sdk/runtime/mod_agreement.h) shows or hides every mod's lines, and the
// pages with lines are written again each time it changes: the lobby's pages follow a player heard
// from late, and a contract, which decides it when it loads, shows its pages as decided at once.
void lines_follow_the_room_agreement_and_are_written_again() {
  start();
  World w;
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "Tells: dark.")}) > 0);
  fake::commit_content();
  const std::vector<pm::content::ModBuild> ours{{"ghost-tells", "0011223344556677"}};
  const auto heard = [&](bool fromPlayer2) {
    return [fromPlayer2, ours](int) -> std::optional<std::vector<pm::content::ModBuild>> {
      if (!fromPlayer2) return std::nullopt;
      return ours;
    };
  };
  pm::content::Agreement agreement;
  agreement.set_local(ours);
  fake::content_registry().use_agreement(&agreement);
  fake::room(1, {1, 2}, 0, 2);
  const auto en = [](std::string_view k, bool) { return game_text(k, "en"); };
  const auto writes = [] {
    return std::count(g_engine.calls.begin(), g_engine.calls.end(), "set_Key Description Journal2_Mare");
  };
  const std::optional<std::string> withLine("A Mare.\n \n Strength: Dark.\n \n Weakness: Light.\n \n Tells: dark.");
  // In the lobby, player 2 not heard from yet: the page is the game's own.
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, heard(false));
  for (int frame = 0; frame < 3; ++frame) w.journalSide.tick(true);
  CHECK(!w.journalSide.text_for_key("Journal2_Mare", "en", en));
  const auto before = writes();
  // Player 2 announces the same mods: the page is written again and shows the line.
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, heard(true));
  for (int frame = 0; frame < 3; ++frame) w.journalSide.tick(true);
  CHECK(writes() == before + 1);
  CHECK(w.journalSide.text_for_key("Journal2_Mare", "en", en) == withLine);
  // A contract holds the answer it loaded with, whatever the host then seems to say, so nothing is written again.
  agreement.observe(true, true, 1, 2, {2}, ++g_heard, [](int) -> std::optional<std::vector<pm::content::ModBuild>> {
    return std::vector<pm::content::ModBuild>{};
  });
  w.contract();
  CHECK(writes() == before + 1);
  CHECK(w.journalSide.text_for_key("Journal2_Mare", "en", en) == withLine);
  // Back in the lobby the answer follows the host again: the page is written again and hides the line.
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, [](int) -> std::optional<std::vector<pm::content::ModBuild>> {
    return std::vector<pm::content::ModBuild>{};
  });
  w.journalSide.tick(true);
  CHECK(writes() == before + 2);
  CHECK(!w.journalSide.text_for_key("Journal2_Mare", "en", en));
}

// One mod's ghosts leave between contracts (its mod switched off in the lobby): the next contract's
// journal holds the other mod's only, numbered from the first page on, the same on every client.
void withdrawing_one_mod_keeps_the_others_pages() {
  start();
  {
    World w;
    runs({ghost("a.lethe", "Lethe"), ghost("b.zephyr", "Zephyr")});
    w.contract();
    CHECK(w.child_named(w.list, "Zephyr") && w.child_named(w.list, "Lethe"));
    w.types.withdraw();  // in the host's order (host/suite.cpp): out of the collection, then the books
    w.journalSide.withdraw();
  }
  // The mods restart without b's: only a's content is registered now.
  fake::content_registry().clear();
  fake::content_registry().begin_group();
  CHECK(content::register_ghost(ghost("a.lethe", "Lethe")) > 0);
  fake::commit_content();
  fake::room(1, {1});
  World w;
  const int32_t firstPage = w.pages();
  w.contract();
  CHECK(w.child_named(w.list, "Lethe") && !w.child_named(w.list, "Zephyr") && !w.child_named(w.grid, "Zephyr"));
  const int32_t lethePage = w.page_number("Lethe");
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == lethePage && w.pages() == firstPage + 1);
  // Pages take the contents' order, which their names decide, so another client with the same content
  // has the same pages: the same count and the same number for the ghost.
  const int32_t pages = w.pages();
  const auto again = pages_by_id({ghost("a.lethe", "Lethe")});
  CHECK(again.size() == 1 && again[0].second == lethePage && pages == firstPage + static_cast<int32_t>(again.size()));
}

// A journal's pages are read once, and the pages with lines written again once per stay in a private
// room, however many frames pass.
void journal_is_read_once() {
  start();
  World w;
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "A line.")}) > 0);
  fake::commit_content();
  fake::room(1, {1});
  w.journalSide.tick(true);
  const int walks = g_engine.children;
  const auto writes = [] {
    return std::count(g_engine.calls.begin(), g_engine.calls.end(), "set_Key Description Journal2_Mare");
  };
  CHECK(walks > 0 && writes() == 1);
  for (int frame = 0; frame < 200; ++frame) w.journalSide.tick(true);
  CHECK(g_engine.children == walks && writes() == 1);
  // Out of the room and back in: written once more.
  w.journalSide.tick(false);
  for (int frame = 0; frame < 10; ++frame) w.journalSide.tick(true);
  CHECK(writes() == 2);
}

// What the game's journal looks like after a failed extension: as it was.
struct Snapshot {
  size_t buttons, pages;
  float tabMax;
  std::vector<std::string> grid, list, contents, numbers;
  std::vector<float> rowHeights;
  std::map<int32_t, void*> paired;
  std::vector<void*> delegates;
  explicit Snapshot(const World& w)
      : buttons(api::RefArray{phasmo::JournalController::allGhostTypeButtons.get(w.journal)}.size()),
        pages(api::RefArray{phasmo::JournalController::pages.get(w.journal)}.size()),
        tabMax(phasmo::JournalTab::max.get(w.ghostsTab)), paired(g_engine.paired) {
    for (Node* c : w.grid->children)
      if (c->active) grid.push_back(c->name);
    for (Node* c : w.list->children)
      if (c->active) list.push_back(c->name);
    for (Node* b : w.vanillaButtons) delegates.push_back(phasmo::NetworkThreeStateButton::userStateChanged.get(b));
    for (Node* page : {w.contentsA, w.contentsB})
      for (Node* row : page->children)
        if (row->active) {
          contents.push_back(row->name);
          rowHeights.push_back(row->sizeDelta.y);
          rowHeights.push_back(row->children[0]->sizeDelta.y);
          numbers.push_back(row->children[0]->text);
        }
  }
  bool operator==(const Snapshot&) const = default;
};

// Every failure part-way leaves the game's journal as it was: no copy stays, no key or entry is kept.
void failed_extension_leaves_the_journal_intact() {
  for (int point = 0; point < 6; ++point) {
    start();
    World w;
    runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
    // The game's own lists as its Awake built them.
    rebuild(w.journal, nullptr);
    const Snapshot before(w);
    void* list = phasmo::JournalController::allGhosts.get(w.journal);
    void* dictionary = phasmo::JournalController::ghostButtons.get(w.journal);
    if (point == 0) g_engine.missingClear = dictionary;  // found missing before anything changes
    if (point == 1) g_engine.failInstantiateAt = 2;      // the first ghost's page
    if (point == 2) g_engine.failInstantiateAt = 8;      // the second ghost's contents row
    if (point == 3) g_engine.failClear = dictionary;
    if (point == 4) g_engine.failClear = list;
    if (point == 5) g_engine.failInitialise = 1;
    w.contract();
    CHECK(logged("the custom ghosts are missing from a journal, which stays as the game made it"));
    const Snapshot after(w);
    CHECK(after.buttons == before.buttons && after.pages == before.pages && after.tabMax == before.tabMax);
    CHECK(after.grid == before.grid && after.list == before.list);
    CHECK(after.contents == before.contents && after.rowHeights == before.rowHeights && after.numbers == before.numbers);
    CHECK(after.paired == before.paired && !g_engine.threw);
    CHECK(after.delegates == before.delegates);
    for (const auto& n : g_nodes)
      if (n->name == "Lethe" || n->name == "Asterion") CHECK(n->destroyed && !n->active);
    CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == -1);
    CHECK(!w.journalSide.text_for_key("Journal2_32", "en", [](std::string_view k, bool) { return game_text(k, "en"); }));
    if (point == 5) {
      CHECK(g_engine.initialised == 2);  // read the game's own buttons again
      // What putting it back did is logged, each step's result.
      CHECK(logged("its buttons were emptied, its list emptied, and its guess sync read its own buttons again"));
    }
  }
}

// A guess sync whose class lacks Initialise is found before anything changes: the journal stays as the
// game made it, nothing is copied, and the log says why.
void missing_initialise_leaves_the_journal_intact() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe")});
  rebuild(w.journal, nullptr);
  const Snapshot before(w);
  g_engine.missingInitialise = true;
  w.contract();
  CHECK(logged("lack the methods the custom ghosts need"));
  CHECK(Snapshot(w) == before && g_engine.instantiates == 0 && g_engine.initialised == 0);
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == -1);
}

// An extended journal writes its pages with lines again each time this client enters a private room,
// as one without custom ghosts does.
void extended_journal_refreshes_lines_on_reentry() {
  start();
  World w;
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "A line.")}) > 0);
  runs({ghost("a.lethe", "Lethe")});
  w.contract();
  CHECK(w.child_named(w.list, "Lethe"));  // extended
  const auto writes = [] {
    return std::count(g_engine.calls.begin(), g_engine.calls.end(), "set_Key Description Journal2_Mare");
  };
  CHECK(writes() == 1);
  w.journalSide.tick(false);
  w.journalSide.tick(true);
  CHECK(writes() == 2);
}

content::EvidenceDef flame_def() {
  content::EvidenceDef e;
  e.id = "a.flame";
  e.name = content::Text("en", "Spectral Flame").add("de", "Spektralflamme");
  e.journalLine = content::Text("en", "Watch a placed candle burn blue");
  return e;
}
content::GhostDef hydra_def() {
  content::GhostDef g = ghost("a.hydra", "Hydra");
  g.evidence = {"emf", "a.flame", "orbs"};
  return g;
}

// A custom evidence that a contract's custom ghost gives gets a copy of the Spirit Box's row after the
// game's rows: its number, its texts by keys in the game's form, in the journal's evidence buttons and
// read by its evidence sync. Marking it found leaves only the ghosts that give it, as the game's filter
// runs over the numbers.
void evidence_button_filters_ghosts() {
  start();
  World w;
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({hydra_def(), ghost("a.lethe", "Lethe")});
  const int32_t flame = content::evidence_of("a.flame"), hydra = content::type_of("a.hydra");
  CHECK(flame == 8 && hydra == 31);
  w.contract();
  CHECK((w.names(w.evidenceRows) == std::vector<std::string>{"EMF", "DOTS", "Spirit Box", "Spectral Flame"}));
  Node* row = w.child_named(w.evidenceRows, "Spectral Flame");
  CHECK(row && phasmo::EvidenceButton::evidence.get(row) == flame);
  CHECK(row && row->children.size() == w.spiritBoxRow->children.size());  // the other players' marks too
  // Its name and the line under it: its own copies, each with a key in the game's form.
  Node* label = row ? as(phasmo::ThreeStateButton::textLabel.get(row)) : nullptr;
  Node* line = row ? as(phasmo::EvidenceButton::descriptionLabel.get(row)) : nullptr;
  CHECK(label && label->parent == row && World::key(label) == "Ev_a.flame");
  CHECK(line && line->parent == row && World::key(line) == "Ev_a.flame_Tip");
  CHECK(World::key(w.spiritBoxRow->children[2]) == "Ev_2");  // the template keeps its own
  // In the journal's evidence buttons, read again by its evidence sync; the game's rows keep their one
  // subscription.
  const api::RefArray buttons{phasmo::JournalController::evidenceButtons.get(w.journal)};
  CHECK(buttons.size() == 4 && buttons[3] == row);
  CHECK(std::count(g_engine.calls.begin(), g_engine.calls.end(), "Initialise evidence") == 1);
  for (Node* r : w.vanillaRows) CHECK(phasmo::NetworkThreeStateButton::userStateChanged.get(r).raw() == r);
  CHECK(row && as(phasmo::NetworkThreeStateButton::userStateChanged.get(row))->name == "combined delegate");
  // The column's spacing shrinks so four rows fill the height three did: 2 * 40 + 3 * 30 = 3 * s + 4 * 30.
  CHECK(std::fabs(w.evidenceRows->spacing - 50.0f / 3) < 0.01f);
  // Its texts, in the player's language when it has it, else English.
  CHECK(w.evidenceSide.answers());
  CHECK(w.evidenceSide.text_for_key("Ev_a.flame", "en") == std::optional<std::string>("Spectral Flame"));
  CHECK(w.evidenceSide.text_for_key("Ev_a.flame", "de") == std::optional<std::string>("Spektralflamme"));
  CHECK(w.evidenceSide.text_for_key("Ev_a.flame", "fr") == std::optional<std::string>("Spectral Flame"));
  CHECK(w.evidenceSide.text_for_key("Ev_a.flame_Tip", "de") ==
        std::optional<std::string>("Watch a placed candle burn blue"));
  CHECK(!w.evidenceSide.text_for_key("Ev_2", "en"));  // the game's own
  // The Hydra's page names it as the game's pages name theirs: by the name's key.
  Node* page = w.child_named(w.book, "Hydra");
  CHECK(page && World::key(page->children[5]) == "Ev_a.flame" && page->children[5]->active);
  // Marking it found leaves the ghosts that give it; EMF too leaves the Hydra.
  mark_found(w.journal, row);
  CHECK((g_engine.left == std::vector<int32_t>{hydra}));
  mark_found(w.journal, w.vanillaRows[0]);
  CHECK((g_engine.left == std::vector<int32_t>{hydra}));
  w.contract();  // extended once
  CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 4);
  // The contract's end takes the keys with it; the lobby answers them again only while content is
  // active there (lobby_journal_gets_custom_evidence_rows).
  w.controllerInstance = nullptr;
  w.types.tick(pm::content::State::Pending, true, true);
  w.evidenceSide.tick(w.journalSide.journals());
  CHECK(!w.evidenceSide.answers() && !w.evidenceSide.text_for_key("Ev_a.flame", "en"));
}

// A registered custom evidence that no ghost of the contract gives gets no row, and the game's
// evidence page stays as it was.
void evidence_without_a_ghost_gets_no_button() {
  start();
  World w;
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({ghost("a.lethe", "Lethe")});
  w.contract();
  CHECK(w.child_named(w.list, "Lethe"));  // the contract holds custom ghosts
  CHECK((w.names(w.evidenceRows) == std::vector<std::string>{"EMF", "DOTS", "Spirit Box"}));
  CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 3);
  CHECK(w.evidenceSide.held().empty() && !w.evidenceSide.answers() && w.evidenceRows->spacing == 40);
}

// The bundled mods as they register: Spectral Flame's evidence (mods/spectral_flame/contract.cpp) and
// Custom Ghosts' ghosts (mods/custom_ghosts/registration.h), twelve of which give it. The evidence page
// gets Spectral Flame's row, in a contract and in the lobby, and the Hydra's page names it. Without a
// ghost that gives it, as before Custom Ghosts registered through the content layer, it gets none
// (evidence_without_a_ghost_gets_no_button).
void spectral_flame_row_comes_with_custom_ghosts() {
  start();
  World w;
  CHECK(pm::flame::contract::register_evidence());
  CHECK(pm::custom::register_content(content::GhostDef{}, content::GhostRules{}));
  fake::commit_content();
  fake::room(1, {1});
  const int32_t flame = content::evidence_of(pm::flame::contract::kEvidenceId);
  const std::string nameKey = std::string("Ev_") + pm::flame::contract::kEvidenceId;
  CHECK(flame == 8);  // one past D.O.T.S. in the audited build
  for (const bool contract : {true, false}) {
    if (contract) {
      w.contract();
    } else {  // the contract ends and the lobby loads, with its own player book
      w.controllerInstance = nullptr;
      w.reload();
      w.lobby();
    }
    CHECK((w.names(w.evidenceRows) == std::vector<std::string>{"EMF", "DOTS", "Spirit Box", "Spectral Flame"}));
    Node* row = w.child_named(w.evidenceRows, "Spectral Flame");
    CHECK(row && phasmo::EvidenceButton::evidence.get(row) == flame);
    CHECK(row && World::key(as(phasmo::ThreeStateButton::textLabel.get(row))) == nameKey);
    CHECK(w.evidenceSide.text_for_key(nameKey, "en") == std::optional<std::string>("Spectral Flame"));
    Node* page = w.child_named(w.book, "Hydra");
    CHECK(page && World::key(page->children[6]) == nameKey && page->children[6]->active);  // its third evidence
  }
}

// The keys' form is read from the game's rows: when their lines' keys do not all add one suffix to the
// name's key, no row is made up, the log says so, and the custom ghost's page hides that evidence.
void evidence_key_form_comes_from_the_game() {
  start();
  World w;
  phasmo::TextLocaliserUI::key.set(w.vanillaRows[1]->children[3], api::make_string("Ev_7_Help"));
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({hydra_def()});
  w.contract();
  CHECK(w.evidenceRows->children.size() == 3 && g_engine.instantiates == 4);  // button, page, entry and contents row
  CHECK(logged("the game's evidence rows show no one key form; custom evidence gets no row"));
  Node* page = w.child_named(w.book, "Hydra");
  CHECK(page && !page->children[5]->active);
  // Each journal's rows are read once, however many frames pass.
  const int reads = g_engine.components;
  for (int frame = 0; frame < 200; ++frame) w.evidenceSide.tick(w.journalSide.journals());
  CHECK(g_engine.components == reads);
}

// A failure part-way through adding the rows leaves the game's evidence page as it was.
void failed_evidence_row_leaves_the_page_intact() {
  for (int point = 0; point < 2; ++point) {
    start();
    World w;
    CHECK(content::register_evidence(flame_def()) > 0);
    runs({hydra_def()});
    if (point == 0) g_engine.failInstantiateAt = 1;  // the row is the first copy made
    if (point == 1) g_engine.failInitialise = 1;     // the evidence sync cannot read the rows
    w.contract();
    CHECK(logged("custom evidence is missing from a journal, which stays as the game made it"));
    CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 3);
    CHECK(w.evidenceRows->spacing == 40);
    for (Node* r : w.vanillaRows) CHECK(phasmo::NetworkThreeStateButton::userStateChanged.get(r).raw() == r);
    for (const auto& n : g_nodes)
      if (n->name == "Spectral Flame") CHECK(n->destroyed && !n->active);
  }
}

// The host's hooks in one process, as the suite makes them: every side's hooks are made (one hook per
// method and detour, as the host's hook table allows), and the one hook on the game's localisation
// answers each side's keys, the game answering the rest through the chain.
bool nobody_verified() { return false; }
void one_localisation_hook_answers_every_side() {
  start();
  World w;
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({hydra_def(), ghost("a.lethe", "Lethe")});
  w.contract();
  pm::host::ContentDifficulty difficulty{fake::content_registry(), w.types, &nobody_verified};
  pm::host::ContentLocalised localised{w.types, w.journalSide, w.evidenceSide};
  CHECK(w.types.create_hooks());  // the journal's were made with the world
  CHECK(localised.create_hook());
  CHECK(difficulty.create_hooks());
  const std::vector<std::string> hooked = fake::hooked();
  CHECK(std::count(hooked.begin(), hooked.end(), "LocalisationSystem.TryGetLocalisedValue") == 1);
  CHECK(std::count(hooked.begin(), hooked.end(), "JournalController.PageSync") == 1);
  // The game's call, through the hook.
  using W = std::remove_cvref_t<decltype(phasmo::LocalisationSystem::TryGetLocalisedValue)>;
  using Call = bool (*)(void*, void**, bool, const void*);
  const auto text = [](std::string_view key) -> std::optional<std::string> {
    void* out = nullptr;
    const auto call = reinterpret_cast<Call>(api::detail::Bodies<W, api::detail::Hooked<W>>::entry(0));
    if (!call(api::make_string(key), &out, true, nullptr) || !out) return std::nullopt;
    return api::to_utf8(out);
  };
  CHECK(text("Journal_31Title") == std::optional<std::string>("Hydra"));  // the ghost types' name key
  const auto page = text("Journal2_31");                                   // the journal's page
  CHECK(page && page->starts_with("Hydra haunts.\n \n Strength: It is strong."));
  CHECK(text("Ev_a.flame") == std::optional<std::string>("Spectral Flame"));  // the evidence's row
  CHECK(text("Journal2_Mare") == std::optional<std::string>("A Mare.\n \n Strength: Dark.\n \n Weakness: Light."));
  CHECK(!text("No_such_key"));
  // A second hook with the same detour is refused, as the host's hook table refuses it.
  CHECK(!localised.create_hook());
}

// The localisation hook asks the sides in one order, and the first that answers a key is written: the
// ghost types' names, then the journal's pages, then the custom evidence's texts. The game's own key
// forms keep the sides' keys apart; pages whose keys take the evidence rows' form ("Ev_a.<name>") make a
// custom page's key the custom evidence "a.31"'s name key, and names of that form make it the custom
// ghost's name key too.
void localisation_asks_types_then_journal_then_evidence() {
  for (const bool namesMeetToo : {true, false}) {
    start();
    g_pagePrefix = "Ev_a.";
    if (namesMeetToo) g_titlePrefix = "Ev_a.", g_titleSuffix = "";
    World w;
    content::EvidenceDef e = flame_def();
    e.id = "a.31";
    CHECK(content::register_evidence(e) > 0);
    content::GhostDef hydra = ghost("a.hydra", "Hydra");
    hydra.evidence = {"emf", "a.31", "orbs"};
    runs({hydra});  // type 31
    w.contract();
    const auto en = [](std::string_view k, bool) { return game_text(k, "en"); };
    // Each side answers the one key.
    CHECK(w.evidenceSide.text_for_key("Ev_a.31", "en") == std::optional<std::string>("Spectral Flame"));
    const auto page = w.journalSide.text_for_key("Ev_a.31", "en", en);
    CHECK(page && page->starts_with("Hydra haunts.\n \n Strength: It is strong."));
    CHECK(w.types.name_for_key("Ev_a.31", "en") ==
          (namesMeetToo ? std::optional<std::string>("Hydra") : std::nullopt));
    // The hook writes the first side's.
    const pm::host::ContentLocalised localised{w.types, w.journalSide, w.evidenceSide};
    CHECK(localised.answer("Ev_a.31", "en", en) == (namesMeetToo ? std::optional<std::string>("Hydra") : page));
  }
}

// Two custom evidence whose keys meet (one's line key is the other's name key) could not tell their
// texts apart: neither gets a row or a page line, and each is logged once.
void colliding_evidence_keys_get_no_row() {
  start();
  World w;
  content::EvidenceDef x = flame_def(), tip = flame_def();
  x.id = "a.x";
  tip.id = "a.x_Tip";  // its name key, "Ev_a.x_Tip", is a.x's line key
  CHECK(content::register_evidence(x) > 0 && content::register_evidence(tip) > 0);
  content::GhostDef g = ghost("a.hydra", "Hydra");
  g.evidence = {"a.x", "a.x_Tip", "emf"};
  runs({g});
  w.contract();
  w.contract();
  CHECK(w.evidenceRows->children.size() == 3);
  CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 3);
  CHECK(w.evidenceSide.held().empty() && !w.evidenceSide.text_for_key("Ev_a.x", "en"));
  for (const char* id : {"\"a.x\"", "\"a.x_Tip\""})
    CHECK(std::count_if(g_lines.begin(), g_lines.end(), [&](const std::string& l) {
            return l.find(std::string("the custom evidence ") + id + " has a journal key another") != std::string::npos;
          }) == 1);
  Node* page = w.child_named(w.book, "Hydra");
  CHECK(page && !page->children[4]->active && !page->children[5]->active && page->children[6]->active);
}

// A journal met before any journal showed the key form waits for it: once another journal shows the
// form, the first gets its row.
void journal_waits_for_the_key_form() {
  start();
  World w;
  phasmo::TextLocaliserUI::key.set(w.vanillaRows[1]->children[3], api::make_string("Ev_7_Help"));  // no one form
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({hydra_def()});
  w.contract();
  CHECK(w.evidenceRows->children.size() == 3);
  // Another journal whose rows show the form: name keys "Ev_<value>", line keys with "_Tip".
  Node* other = node("Other Journal2.0Canvas");
  std::vector<void*> rows;
  for (const VanillaEvidence& e : kEvidenceRows) {
    Node* row = node(e.name, other, kEvidenceButton);
    Node* label = node("Text (TMP)", row, kLocaliser);
    phasmo::TextLocaliserUI::key.set(label, api::make_string("Ev_" + std::to_string(e.value)));
    Node* line = node("Text (TMP).1", row, kLocaliser);
    phasmo::TextLocaliserUI::key.set(line, api::make_string("Ev_" + std::to_string(e.value) + "_Tip"));
    phasmo::ThreeStateButton::textLabel.set(row, label);
    phasmo::EvidenceButton::descriptionLabel.set(row, line);
    rows.push_back(row);
  }
  void* array = phasmo::EvidenceButton::type.make_array(rows.size());
  for (size_t r = 0; r < rows.size(); ++r) api::RefArray{array}.set(r, rows[r]);
  phasmo::JournalController::evidenceButtons.set(other, array);
  for (int frame = 0; frame < 2; ++frame) w.evidenceSide.tick({w.journal, other});
  CHECK((w.names(w.evidenceRows) == std::vector<std::string>{"EMF", "DOTS", "Spirit Box", "Spectral Flame"}));
}
void layout_is_read_from_game_text() {
  // The layout most pages agree on, two at least, and none when two layouts tie.
  const std::string en1 = "A.\n \n Strength: S.\n \n Weakness: W.", en2 = "B.\n \n Strength: T.\n \n Weakness: V.";
  const std::string odd = "C.\n \n Power: P.\n \n Flaw: F.";
  CHECK(pm::host::agreed_layout({en1, odd, en2}) && pm::host::agreed_layout({en1, odd, en2})->strength == "Strength:");
  CHECK(!pm::host::agreed_layout({en1, odd}));
  CHECK(!pm::host::agreed_layout({en1}));
  const auto l = pm::host::read_layout("Desc.\n \n Strength: S.\n \n Weakness: W.");
  CHECK(l && l->separator == "\n \n " && l->strength == "Strength:" && l->weakness == "Weakness:");
  CHECK(!pm::host::read_layout("Desc.\n \n \n Weakness: W."));  // no strength: not every part
  const auto wide = pm::host::read_layout("説明。\n \n 強み：S\n \n 弱み：W");
  CHECK(wide && wide->strength == "強み：" && wide->weakness == "弱み：");
}

// The lobby's journals (sdk/systems/journal.md, "The lobby's journal") are the players' PCPlayer books, as
// a contract's are. While content is active in the lobby they get everything a contract's journal gets,
// through the same code: the custom ghosts join the collection the journal lists, and the journal gets
// their guess buttons, list entries and pages, numbered alike on every client with the same content.

std::vector<std::pair<std::string, int32_t>> lobby_pages(const std::vector<content::GhostDef>& ghosts) {
  start();
  World w;
  runs(ghosts);
  w.lobby();
  std::vector<std::pair<std::string, int32_t>> out;
  for (Node* entry : w.list->children)
    if (const int32_t page = w.journalSide.page_of(entry); page >= 0) out.emplace_back(entry->name, page);
  std::sort(out.begin(), out.end());
  return out;
}

void lobby_journal_gets_custom_ghosts_while_content_active() {
  // Two clients with the same content, registered in another order, number the pages alike (each its
  // own world, made before this test's).
  const auto one = lobby_pages({ghost("b.zephyr", "Zephyr"), ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const auto two = lobby_pages({ghost("a.asterion", "Asterion"), ghost("b.zephyr", "Zephyr"), ghost("a.lethe", "Lethe")});
  CHECK(one == two && one.size() == 3);
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const int32_t firstPage = w.pages();
  w.lobby();
  // The journal's collection holds them, each in its alphabetical place, its name key answered.
  std::vector<int32_t> listed;
  for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(w.collection)))
    listed.push_back(phasmo::GhostData::ghostType.get(data));
  CHECK((listed == std::vector<int32_t>{31, 4, 5, 32, 6, 0, 1, 10}));
  CHECK(w.types.name_for_key("Journal_32Title") == std::optional<std::string>("Lethe"));
  // The guess buttons, paired by the journal's own builder and read by its guess sync.
  CHECK((w.names(w.grid) ==
         std::vector<std::string>{"Asterion", "Banshee", "Jinn", "Lethe", "Mare", "Spirit", "Wraith", "Yurei"}));
  Node* button = w.child_named(w.grid, "Lethe");
  CHECK(button && phasmo::GhostButton::ghostType.get(button) == 32 && g_engine.paired[32] == button);
  CHECK(api::RefArray{phasmo::JournalController::allGhostTypeButtons.get(w.journal)}.size() == 8);
  CHECK(g_engine.initialised == 1);
  // The list entries and the pages they lead to, among the book's ghosts, the Ghosts tab covering them.
  Node* asterion = w.child_named(w.list, "Asterion");
  Node* lethe = w.child_named(w.list, "Lethe");
  CHECK(asterion && lethe && w.pages() == firstPage + 2);
  CHECK(w.journalSide.page_of(asterion) == w.page_number("Asterion") && w.journalSide.page_of(lethe) == w.page_number("Lethe"));
  CHECK(std::lround(phasmo::JournalTab::max.get(w.ghostsTab)) == firstPage + 2);
  CHECK(w.journalSide.press(lethe) && g_engine.page == w.page_number("Lethe"));
  Node* page = w.page_number("Lethe") >= 0 ? as(api::RefArray{phasmo::JournalController::pages.get(w.journal)}[
                                                 static_cast<size_t>(w.page_number("Lethe"))])
                                           : nullptr;
  CHECK(page && page->name == "Lethe" && World::key(page->children[0]) == "Journal_32Title" &&
        World::key(page->children[2]) == "Journal2_32");
  const auto en = [](std::string_view k, bool) { return game_text(k, "en"); };
  const auto text = w.journalSide.text_for_key("Journal2_32", "en", en);
  CHECK(text && text->starts_with("Lethe haunts.") && text->ends_with("It tells."));
  w.lobby();  // extended once
  CHECK(api::RefArray{phasmo::JournalController::allGhostTypeButtons.get(w.journal)}.size() == 8 &&
        w.pages() == firstPage + 2);
}

void lobby_journal_gets_custom_evidence_rows() {
  start();
  World w;
  CHECK(content::register_evidence(flame_def()) > 0);
  runs({hydra_def(), ghost("a.lethe", "Lethe")});
  const int32_t flame = content::evidence_of("a.flame"), hydra = content::type_of("a.hydra");
  w.lobby();
  CHECK((w.names(w.evidenceRows) == std::vector<std::string>{"EMF", "DOTS", "Spirit Box", "Spectral Flame"}));
  Node* row = w.child_named(w.evidenceRows, "Spectral Flame");
  CHECK(row && phasmo::EvidenceButton::evidence.get(row) == flame);
  CHECK(row && World::key(as(phasmo::ThreeStateButton::textLabel.get(row))) == "Ev_a.flame" &&
        World::key(as(phasmo::EvidenceButton::descriptionLabel.get(row))) == "Ev_a.flame_Tip");
  CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 4);
  CHECK(std::count(g_engine.calls.begin(), g_engine.calls.end(), "Initialise evidence") == 1);
  CHECK(std::fabs(w.evidenceRows->spacing - 50.0f / 3) < 0.01f);
  CHECK(w.evidenceSide.text_for_key("Ev_a.flame", "en") == std::optional<std::string>("Spectral Flame"));
  // The Hydra's page names it by its row's key, and the evidence filter runs over its number.
  Node* page = w.child_named(w.book, "Hydra");
  CHECK(page && World::key(page->children[5]) == "Ev_a.flame" && page->children[5]->active);
  if (row) mark_found(w.journal, row);
  CHECK((g_engine.left == std::vector<int32_t>{hydra}));
  w.lobby();  // extended once
  CHECK(api::RefArray{phasmo::JournalController::evidenceButtons.get(w.journal)}.size() == 4);
}

void lobby_journal_shows_journal_lines_on_vanilla_pages() {
  start();
  World w;
  CHECK(content::add_journal_lines("vanilla.mare", {content::Text("en", "Tells: dark.")}) > 0);
  runs({ghost("a.lethe", "Lethe")});
  w.lobby();
  const auto en = [](std::string_view k, bool) { return game_text(k, "en"); };
  // The lines follow the Mare's own text, and its page wrote its text again so they show.
  CHECK(w.journalSide.text_for_key("Journal2_Mare", "en", en) ==
        std::optional<std::string>("A Mare.\n \n Strength: Dark.\n \n Weakness: Light.\n \n Tells: dark."));
  CHECK(std::find(g_engine.calls.begin(), g_engine.calls.end(), "set_Key Description Journal2_Mare") !=
        g_engine.calls.end());
  // The same journal has the custom ghost's page, with its text.
  CHECK(w.child_named(w.list, "Lethe") && w.journalSide.text_for_key("Journal2_31", "en", en));
}

void lobby_journal_keeps_pages_after_contract_ends() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const int32_t firstPage = w.pages();
  w.contract();
  const int32_t lethePage = w.page_number("Lethe"), asterionPage = w.page_number("Asterion");
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == lethePage);
  // The contract ends and the lobby loads: the contract's book goes, and the lobby's player book wakes as
  // the game made it, then gets the same pages while content is active.
  w.controllerInstance = nullptr;
  w.reload();
  CHECK(!w.child_named(w.list, "Lethe") && w.pages() == firstPage);
  w.lobby();
  Node* lethe = w.child_named(w.list, "Lethe");
  CHECK(lethe && w.journalSide.page_of(lethe) == lethePage && w.pages() == firstPage + 2);
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Asterion")) == asterionPage);
  CHECK(w.types.name_for_key("Journal_32Title") == std::optional<std::string>("Lethe"));
  CHECK(w.journalSide.text_for_key("Journal2_32", "en", [](std::string_view k, bool) { return game_text(k, "en"); }));
  // The next contract's book gets them again, numbered as before.
  w.reload();
  w.contract();
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == lethePage && w.pages() == firstPage + 2);
}

// Everything of a journal the custom ghosts and evidence change, to compare with the game's own.
using Pair = std::pair<float, float>;
Pair pair_of(phasmo::Vector2 v) { return {v.x, v.y}; }
struct GameJournal {
  std::vector<void*> buttons, pages, rows;
  float tabMax;
  std::vector<std::string> grid, list, evidenceRows;
  std::map<int32_t, void*> paired;
  std::vector<int32_t> listed, collection;
  std::vector<int32_t> found = g_engine.found, ruledOut = g_engine.ruledOut;  // the evidence lists
  int32_t guess = g_engine.guess;
  std::vector<void*> delegates;
  Pair gridCell, listCell;
  bool listEnabled;
  int32_t listConstraint, listCount;
  float spacing;
  std::vector<std::tuple<Pair, Pair, Pair, Pair>> entries;  // each list entry's anchors, size and position
  std::vector<std::string> contents, contentsNumbers;       // the Home tab's rows, in order, and their numbers
  std::vector<Pair> contentsSizes;                          // each row's size and its number's
  explicit GameJournal(const World& w)
      : tabMax(phasmo::JournalTab::max.get(w.ghostsTab)), paired(g_engine.paired), listed(g_engine.listed),
        gridCell(pair_of(w.grid->cell)), listCell(pair_of(w.list->cell)), listEnabled(w.list->enabled),
        listConstraint(w.list->constraint), listCount(w.list->constraintCount), spacing(w.evidenceRows->spacing) {
    const auto all = [](void* array) {
      std::vector<void*> out;
      const api::RefArray a{array};
      for (size_t i = 0; i < a.size(); ++i) out.push_back(a[i]);
      return out;
    };
    buttons = all(phasmo::JournalController::allGhostTypeButtons.get(w.journal));
    pages = all(phasmo::JournalController::pages.get(w.journal));
    rows = all(phasmo::JournalController::evidenceButtons.get(w.journal));
    for (Node* c : w.grid->children)
      if (c->active) grid.push_back(c->name);
    for (Node* c : w.list->children)
      if (c->active) {
        list.push_back(c->name);
        entries.emplace_back(pair_of(c->anchorMin), pair_of(c->anchorMax), pair_of(c->sizeDelta), pair_of(c->position));
      }
    for (Node* c : w.evidenceRows->children)
      if (c->active) evidenceRows.push_back(c->name);
    for (Node* page : {w.contentsA, w.contentsB})
      for (Node* row : page->children)
        if (row->active) {
          contents.push_back(row->name);
          contentsSizes.push_back(pair_of(row->sizeDelta));
          contentsSizes.push_back(pair_of(row->children[0]->sizeDelta));
          contentsNumbers.push_back(row->children[0]->text);
        }
    for (void* data : fake::list_items_of(phasmo::GhostCollection::ghosts.get(w.collection)))
      collection.push_back(phasmo::GhostData::ghostType.get(data));
    for (Node* b : w.vanillaButtons) delegates.push_back(phasmo::NetworkThreeStateButton::userStateChanged.get(b));
    for (Node* r : w.vanillaRows) delegates.push_back(phasmo::NetworkThreeStateButton::userStateChanged.get(r));
  }
  bool operator==(const GameJournal&) const = default;
};

// Another client in the room as `actor` with the same content, its manifest delivered.
void agreeing_peer(pm::content::Registry& peer, int actor, const std::vector<int>& room,
                   const std::vector<content::GhostDef>& ghosts, const std::vector<content::EvidenceDef>& evidence) {
  pm::content::GameEnums enums;
  CHECK(pm::content::read_game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                                     fake::kAuditedNoEvidence, enums)
            .empty());
  peer.set_game_enums(std::move(enums));
  peer.begin_group();
  for (const content::EvidenceDef& e : evidence) CHECK(peer.add_evidence(&e.c_def()) > 0);
  for (const content::GhostDef& g : ghosts) CHECK(peer.add_ghost(&g.c_def()) > 0);
  peer.end_group(true);
  peer.commit();
  peer.observe(actor, room, 0);
  for (const std::vector<uint8_t>& m : peer.take_messages()) fake::host_message(actor, m.data(), m.size());
}

void lobby_journal_withdraws_when_content_turns_off() {
  start();
  World w;
  const std::vector<content::GhostDef> ghosts{hydra_def(), ghost("a.lethe", "Lethe")};
  CHECK(content::register_evidence(flame_def()) > 0);
  for (const content::GhostDef& g : ghosts) CHECK(content::register_ghost(g) > 0);
  fake::commit_content();
  // Player 2 has the same content; whether the room's mod lists match decides.
  const std::vector<pm::content::ModBuild> ours{{"a", "0011223344556677"}}, other{{"a", "8899aabbccddeeff"}};
  pm::content::Agreement agreement;
  agreement.set_local(ours);
  fake::content_registry().use_agreement(&agreement);
  pm::content::Registry peer;
  agreeing_peer(peer, 2, {1, 2}, ghosts, {flame_def()});
  fake::room(1, {1, 2}, 100);
  const auto announces = [](std::vector<pm::content::ModBuild> mods) {
    return [mods](int) -> std::optional<std::vector<pm::content::ModBuild>> { return mods; };
  };
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, announces(ours));
  CHECK(fake::content_state() == pm::content::State::Active);
  rebuild(w.journal, nullptr);  // the game's own lists as its Awake built them
  set_sibling(w.child_named(w.contentsB, "Mare"), 0, nullptr);  // out of order, as the game's Obambo is above its Obake
  const GameJournal before(w);
  w.lobby();
  const int32_t firstLethePage = w.page_number("Lethe");
  CHECK(w.child_named(w.list, "Lethe") && w.child_named(w.evidenceRows, "Spectral Flame"));
  CHECK(!(GameJournal(w) == before));
  const auto en = [](std::string_view k, bool) { return game_text(k, "en"); };
  // The player marks the custom evidence found (every game ghost is crossed out by it) and guesses the
  // Lethe.
  if (Node* row = w.child_named(w.evidenceRows, "Spectral Flame")) mark_found(w.journal, row);
  CHECK(g_engine.left == std::vector<int32_t>{content::type_of("a.hydra")});
  CHECK(g_engine.found == std::vector<int32_t>{content::evidence_of("a.flame")});
  g_engine.guess = content::type_of("a.lethe");
  // Player 2 turns out to run another build: the room's mods no longer agree, and the journal is the
  // game's own again, its lists rebuilt, its guess and evidence syncs reading the game's buttons, the
  // custom evidence off its lists and the guess on the Lethe cleared, so no game ghost stays crossed out.
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, announces(other));
  CHECK(fake::content_state() == pm::content::State::Pending);
  w.lobby();
  CHECK(GameJournal(w) == before);
  CHECK((g_engine.left == std::vector<int32_t>{4, 5, 6, 0, 1, 10}));
  for (const auto& n : g_nodes)
    if (n->name == "Lethe" || n->name == "Hydra" || n->name == "Spectral Flame") CHECK(n->destroyed && !n->active);
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == -1);
  CHECK(!w.journalSide.text_for_key("Journal2_32", "en", en) && !w.types.name_for_key("Journal_32Title"));
  CHECK(!w.evidenceSide.text_for_key("Ev_a.flame", "en"));
  // They agree again: the pages come back, numbered as before.
  agreement.observe(true, false, 1, 2, {2}, ++g_heard, announces(ours));
  w.lobby();
  CHECK(w.journalSide.page_of(w.child_named(w.list, "Lethe")) == firstLethePage);
  CHECK(w.child_named(w.evidenceRows, "Spectral Flame"));
  // The mods stop, the host taking their content out of the game first (host/suite.cpp): the game's own
  // journal again, a mark on the custom evidence ruling it out included.
  g_engine.ruledOut.push_back(content::evidence_of("a.flame"));
  w.types.withdraw();
  w.journalSide.withdraw();
  w.evidenceSide.withdraw();
  CHECK(GameJournal(w) == before);
  CHECK(w.journalSide.journals().empty());
  fake::content_registry().use_agreement(nullptr);
}

// The page sync is sent to every copy of a book (AllBuffered, so a player who joins replays every one).
// The game first checks that the sender owns the book (a sender who does not is reported and changes
// nothing); for the owner it throws on a number outside this client's pages after blanking the book.
// A copy without the custom pages (a joiner before its content is active, a client a moment behind)
// skips only the owner's such number, logged once, and keeps its page until the next turn. Every other
// call reaches the game untouched, its sender check included.
void a_page_sync_beyond_the_book_is_skipped() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe")});
  w.lobby();  // in a private room
  const int32_t pages = w.pages();
  void* owner = owner_of(w.journal);
  Node* stranger = node("Player 2");
  const auto current = [&] { return phasmo::JournalController::currentPage.get(w.journal); };
  // A page the book has always reaches the game: the owner's turns it, another's is reported.
  sync_page(w.journal, 1, owner);
  CHECK(current() == 1 && g_engine.pageThrows == 0 && g_engine.reports == 0);
  sync_page(w.journal, 2, stranger);
  CHECK(current() == 1 && g_engine.reports == 1);
  // The owner's number beyond the book is skipped: no throw, the page stays shown.
  for (const int32_t beyond : {pages, pages + 1, -1}) sync_page(w.journal, beyond, owner);
  CHECK(g_engine.pageThrows == 0 && current() == 1);
  CHECK(as(api::RefArray{phasmo::JournalController::pages.get(w.journal)}[1])->active);  // not blanked
  CHECK(std::count_if(g_lines.begin(), g_lines.end(), [](const std::string& l) {
          return l.find("a page sync beyond this client's journal") != std::string::npos;
        }) == 1);
  // Another sender's number beyond the book reaches the game, which reports it and changes nothing.
  sync_page(w.journal, pages + 3, stranger);
  CHECK(g_engine.reports == 2 && g_engine.pageThrows == 0 && current() == 1);
  // A sender not known reaches the game, as does a book whose owner is not known: the game's own result.
  sync_page(w.journal, pages + 3, nullptr);
  CHECK(g_engine.pageThrows == 1 && g_engine.reports == 2);
  void* view = phasmo::JournalController::photonView.get(w.journal);
  phasmo::PhotonView::owner.set(view, nullptr);
  sync_page(w.journal, pages + 3, owner);
  CHECK(g_engine.pageThrows == 2);
  phasmo::PhotonView::owner.set(view, owner);
  // Out of a private room every call reaches the game.
  w.journalSide.tick(false);
  sync_page(w.journal, pages + 3, owner);
  CHECK(g_engine.pageThrows == 3);
}

// A player reading a custom page when the lobby's content stops being active: their own book turns to
// the page its Ghosts tab opens (the tab's first), as their press of the tab would, so the next and
// previous page turns step from a page the book has. Another player's copy is left to its owner.
void a_reader_on_a_custom_page_is_turned_back_when_content_stops() {
  start();
  World w;
  runs({ghost("a.lethe", "Lethe"), ghost("a.asterion", "Asterion")});
  const int32_t firstPage = w.pages();
  w.lobby();
  CHECK(w.journalSide.press(w.child_named(w.list, "Lethe")));
  CHECK(phasmo::JournalController::currentPage.get(w.journal) == w.page_number("Lethe"));
  fake::room(1, {1, 2}, 100000, 2);  // the host, not heard from yet
  CHECK(fake::content_state() == pm::content::State::Pending);
  w.lobby();
  const int32_t tabFirst = static_cast<int32_t>(phasmo::JournalTab::min.get(w.ghostsTab));
  CHECK(w.pages() == firstPage && phasmo::JournalController::currentPage.get(w.journal) == tabFirst);
  CHECK(as(api::RefArray{phasmo::JournalController::pages.get(w.journal)}[static_cast<size_t>(tabFirst)])->active);
  next_page(w.journal);
  CHECK(phasmo::JournalController::currentPage.get(w.journal) == tabFirst + 1);
  previous_page(w.journal);
  previous_page(w.journal);
  CHECK(phasmo::JournalController::currentPage.get(w.journal) == tabFirst - 1 && g_engine.pageThrows == 0);
  // Another player's copy on a custom page is not turned here: its owner's turn reaches it.
  fake::room(1, {1}, 200000);
  w.lobby();
  g_engine.mine = false;
  CHECK(w.journalSide.press(w.child_named(w.list, "Lethe")));
  const int32_t lethePage = w.page_number("Lethe");
  fake::room(1, {1, 2}, 300000);
  const int32_t turns = g_engine.page;
  w.lobby();
  CHECK(g_engine.page == turns && phasmo::JournalController::currentPage.get(w.journal) == lethePage);
  // Its owner's next turn, sent from a client still showing the custom pages, is skipped on this copy
  // rather than thrown.
  sync_page(w.journal, firstPage + 2, owner_of(w.journal));
  CHECK(g_engine.pageThrows == 0 && phasmo::JournalController::currentPage.get(w.journal) == lethePage);
}

}  // namespace

int main() {
  // The bundled mods register their text from their own tables (spectral_flame_row_comes_with_custom_ghosts).
  use_text({"mods/spectral_flame/i18n/default.json", "mods/custom_ghosts/i18n/default.json"});
  a_book_awake_before_the_hooks_is_noted();
  a_custom_page_is_told_from_the_view_it_is_sent_on();
  guess_button_crossed_out_by_filter();
  list_entry_alphabetical_and_leads_to_page();
  unknown_evidence_text_is_hidden();
  page_numbers_match_across_clients();
  missing_language_falls_back_to_english();
  language_switch_rewrites_pages();
  vanilla_lines_from_two_mods_both_show();
  lines_follow_the_room_agreement_and_are_written_again();
  withdrawing_one_mod_keeps_the_others_pages();
  layout_is_read_from_game_text();
  journal_is_read_once();
  failed_extension_leaves_the_journal_intact();
  missing_initialise_leaves_the_journal_intact();
  extended_journal_refreshes_lines_on_reentry();
  evidence_button_filters_ghosts();
  evidence_without_a_ghost_gets_no_button();
  no_evidence_ghost_stays_until_evidence_is_marked();
  spectral_flame_row_comes_with_custom_ghosts();
  evidence_key_form_comes_from_the_game();
  a_page_key_without_its_name_says_nothing_of_the_form();
  a_page_key_in_another_form_does_not_decide_the_form();
  a_custom_ghost_joins_the_home_tabs_contents();
  the_contents_rows_shorten_together_to_fit();
  the_contents_numbers_count_up_over_both_pages();
  the_pages_before_the_ghosts_keep_their_numbers();
  a_reader_on_a_moved_page_is_turned_to_the_ghosts_tab();
  pages_that_agree_on_no_form_get_no_custom_ghosts();
  failed_evidence_row_leaves_the_page_intact();
  one_localisation_hook_answers_every_side();
  localisation_asks_types_then_journal_then_evidence();
  colliding_evidence_keys_get_no_row();
  journal_waits_for_the_key_form();
  lobby_journal_gets_custom_ghosts_while_content_active();
  lobby_journal_gets_custom_evidence_rows();
  lobby_journal_shows_journal_lines_on_vanilla_pages();
  lobby_journal_keeps_pages_after_contract_ends();
  lobby_journal_withdraws_when_content_turns_off();
  a_page_sync_beyond_the_book_is_skipped();
  a_reader_on_a_custom_page_is_turned_back_when_content_stops();
  std::printf("content_journal: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
