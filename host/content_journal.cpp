#include "content_journal.h"

#include <algorithm>
#include <cmath>
#include <set>

#include "GhostCollection.h"
#include "JournalController.h"
#include "LocalisationSystem.h"
#include "Photon.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "content_evidence.h"
#include "content_text.h"
#include "content_widgets.h"
#include "game_api.h"
#include "guarded.h"
#include "log.h"
#include "runtime.h"

namespace pm::host {
namespace {

using pm::api::Use;

ContentJournal* g_self = nullptr;  // the instance whose hooks are made

std::string lower(std::string_view text) {
  std::string out(text);
  for (char& c : out)
    if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
  return out;
}

void detour_awake(pm::api::Original<phasmo::JournalController::Awake> original, void* journal) {
  static bool logged = false;
  original(journal);
  if (g_self && journal) guarded(logged, "noting a journal", [&] { g_self->after_awake(journal); });
}

// Button.OnPointerClick jumps to Press for the left button only, and OnSubmit calls it before showing
// the pressed state (build 25811160): Press is where a click invokes onClick.
void detour_press(pm::api::Original<phasmo::Button::Press> original, void* button) {
  static bool logged = false;
  bool done = false;
  if (g_self && button && g_self->has_entries())
    guarded(logged, "opening a custom ghost's page", [&] { done = g_self->press(button); });
  if (!done) original(button);
}

// The page sync (JournalController.PageSync): the RPC on every copy of a book, and GoToPage's own call on this client.
void detour_page_sync(pm::api::Original<phasmo::JournalController::PageSync> original, void* journal, int32_t page,
                      const void* info) {
  static bool logged = false;
  bool syncs = true;  // any failure below passes the call to the game untouched
  if (g_self && journal)
    guarded(logged, "checking a page sync", [&] { syncs = g_self->page_syncs(journal, page, info); });
  if (syncs) original(journal, page, info);
}

using widgets::active_children;
using widgets::appended;
using widgets::game_object_of;
using widgets::key_of;
using widgets::localiser_of;
using widgets::set_key;
using widgets::transform_of;
using widgets::transform_of_object;

std::string name_of(void* object) { return pm::api::to_utf8(phasmo::Object::get_name(object)); }

void* localiser_in(void* component) {
  return component ? phasmo::Component::GetComponentInChildren(component, phasmo::TextLocaliserUI::type.type_object(), true)
                   : nullptr;
}

// The sibling index that keeps a parent's active children in alphabetical order (by name, ignoring
// case) with one named `name` added, as the game ships its grids.
int32_t alphabetical_index(void* parent, const std::string& name) {
  const std::string key = lower(name);
  const int32_t n = phasmo::Transform::get_childCount(parent);
  int32_t last = -1;
  for (int32_t i = 0; i < n; ++i) {
    void* child = phasmo::Transform::GetChild(parent, i);
    if (!child || !phasmo::GameObject::get_activeSelf(game_object_of(child))) continue;
    if (lower(name_of(child)) > key) return i;
    last = i;
  }
  return last + 1;
}

// A Home tab contents row's page number: its one child, a TextMeshProUGUI. Null when it has none.
void* row_number(void* row) {
  return phasmo::Transform::get_childCount(row) > 0 ? phasmo::Transform::GetChild(row, 0) : nullptr;
}

// A contents page's ghost rows are those showing a ghost's name; the rows above them name the book's sections.
bool is_ghost_row(void* row, const std::set<std::string>& ghostKeys) {
  return row && phasmo::GameObject::get_activeSelf(game_object_of(row)) && ghostKeys.count(key_of(localiser_in(row)));
}

// A contents page's ghost rows in alphabetical order, and the places among its children they fill. The
// game's own aren't quite in order: its Obambo sits above its Obake.
struct GhostRows {
  std::vector<pm::api::Held> rows;
  std::vector<int32_t> places;
};
GhostRows ghost_rows(void* parent, const std::set<std::string>& ghostKeys) {
  GhostRows out;
  const int32_t n = phasmo::Transform::get_childCount(parent);
  for (int32_t i = 0; i < n; ++i)
    if (void* row = phasmo::Transform::GetChild(parent, i); is_ghost_row(row, ghostKeys)) {
      out.rows.emplace_back(row);
      out.places.push_back(i);
    }
  std::stable_sort(out.rows.begin(), out.rows.end(), [](const pm::api::Held& a, const pm::api::Held& b) {
    return lower(name_of(a.get())) < lower(name_of(b.get()));
  });
  return out;
}

// A contents row's page number as a text: null when it has none.
void* number_text(void* row) {
  void* number = row_number(row);
  return number ? phasmo::Component::GetComponent(number, phasmo::TextMeshProUGUI::type.type_object()) : nullptr;
}

// The local player's own book on a page whose number has changed: the page turns would step from a number that means another page,
// or none. It turns to the page its Ghosts tab opens (the tab's first, the page the tab's own button names), as the player's press
// of the tab does; the page sync tells every copy.
void turn_reader_back(void* journal, int32_t firstMoved, void* ghostsTab) {
  void* view = phasmo::JournalController::photonView.get(journal);
  if (view && ghostsTab && phasmo::PhotonView::get_IsMine(view) &&
      phasmo::JournalController::currentPage.get(journal) >= firstMoved)
    phasmo::JournalController::GoToPage(journal, static_cast<int32_t>(phasmo::JournalTab::min.get(ghostsTab)));
}

// Shortens the rows of both contents pages by one factor, the fuller page's, so each page's rows fit the
// height they filled and the pages match. The layout group leaves a row's size alone and the texts size
// themselves to it (Scene.JournalContentsRows), so the number's height goes with its row's.
void fit_contents(void* first, int32_t firstBefore, void* second, int32_t secondBefore) {
  const std::vector<void*> firstRows = active_children(first), secondRows = active_children(second);
  float factor = 1.0f;
  if (firstRows.size() > 0)
    factor = std::min(factor, static_cast<float>(firstBefore) / static_cast<float>(firstRows.size()));
  if (secondRows.size() > 0)
    factor = std::min(factor, static_cast<float>(secondBefore) / static_cast<float>(secondRows.size()));
  if (factor >= 1.0f) return;
  const auto shorter = [&](void* rect) {
    phasmo::Vector2 size = phasmo::RectTransform::get_sizeDelta(rect);
    size.y *= factor;
    phasmo::RectTransform::set_sizeDelta(rect, size);
  };
  for (const std::vector<void*>* rows : {&firstRows, &secondRows})
    for (void* row : *rows) {
      shorter(row);
      if (void* number = row_number(row)) shorter(number);
    }
}

// The grid that lays children out as they were placed by hand, in rows from the first child: its cell
// is the first child's size, and a row holds as many children as share the first one's height.
struct GridShape {
  phasmo::Vector2 cell{0, 0};
  int32_t columns = 0;
};
GridShape placed_grid(const std::vector<void*>& children) {
  GridShape s;
  if (children.empty()) return s;
  const phasmo::Rect first = phasmo::RectTransform::get_rect(children.front());
  const float y = phasmo::RectTransform::get_anchoredPosition(children.front()).y;
  s.cell = {first.width, first.height};
  for (void* c : children)
    if (std::fabs(phasmo::RectTransform::get_anchoredPosition(c).y - y) < 0.5f) ++s.columns;
  return s;
}

void* grid_of(void* transform) {
  return transform ? phasmo::Component::GetComponent(transform, phasmo::GridLayoutGroup::type.type_object()) : nullptr;
}

// A list placed by hand whose GridLayoutGroup is switched off: the grid of that placement, measured
// before copies join it (each lies on the entry it was copied from); none for a grid already on.
std::optional<GridShape> placed_shape(void* transform) {
  void* grid = grid_of(transform);
  if (!grid || phasmo::Behaviour::get_enabled(grid)) return std::nullopt;
  const GridShape shape = placed_grid(active_children(transform));
  if (shape.columns <= 0) return std::nullopt;
  return shape;
}

// Sets the list's grid to that placement and switches it on, so the copies take their own cells.
void use_grid(void* transform, const GridShape& shape) {
  void* grid = grid_of(transform);
  if (!grid) return;
  phasmo::GridLayoutGroup::set_cellSize(grid, shape.cell);
  phasmo::GridLayoutGroup::set_constraint(grid, phasmo::GridConstraint::FixedColumnCount());
  phasmo::GridLayoutGroup::set_constraintCount(grid, shape.columns);
  phasmo::Behaviour::set_enabled(grid, true);
}

// Shortens a grid's rows so `after` children fit in the height `before` filled.
void fit_grid(void* transform, int32_t before, int32_t after) {
  void* grid = grid_of(transform);
  if (!grid || before <= 0 || after <= before) return;
  phasmo::Vector2 cell = phasmo::GridLayoutGroup::get_cellSize(grid);
  const phasmo::Vector2 spacing = phasmo::GridLayoutGroup::get_spacing(grid);
  int32_t columns = 0;
  if (phasmo::GridLayoutGroup::get_constraint(grid) == phasmo::GridConstraint::FixedColumnCount())
    columns = phasmo::GridLayoutGroup::get_constraintCount(grid);
  else if (cell.x + spacing.x > 0)
    columns = static_cast<int32_t>((phasmo::RectTransform::get_rect(transform).width + spacing.x) / (cell.x + spacing.x));
  if (columns <= 0) return;
  const int32_t rowsBefore = (before + columns - 1) / columns, rowsAfter = (after + columns - 1) / columns;
  if (rowsAfter <= rowsBefore) return;
  cell.y = (cell.y + spacing.y) * rowsBefore / rowsAfter - spacing.y;
  phasmo::GridLayoutGroup::set_cellSize(grid, cell);
}

// A key's form split around a name ("Journal2_" and "" around "Spirit"): every split the key allows.
std::vector<std::pair<std::string, std::string>> splits(const std::string& key, const std::string& name) {
  std::vector<std::pair<std::string, std::string>> out;
  const std::string folded = lower(key), want = lower(name);
  for (size_t at = folded.find(want); !want.empty() && at != std::string::npos; at = folded.find(want, at + 1))
    out.emplace_back(key.substr(0, at), key.substr(at + want.size()));
  return out;
}

const char* kNeeded[] = {
    "JournalController.Awake", "JournalController.pages", "JournalController.journalTabs",
    "JournalController.allGhostsData", "JournalController.allGhostTypeButtons", "JournalController.ghostSync",
    "JournalController.allGhosts", "JournalController.ghostButtons", "JournalController.RebuildGhosts",
    "JournalController.SetGhostTypes", "JournalController.GoToPage", "JournalTab.min", "JournalTab.max", "GhostButton",
    "GhostButton.type", "NetworkThreeStateButton.userStateChanged", "TextLocaliserUI", "TextLocaliserUI.key",
    "TextLocaliserUI.set_Key", "LocalisationSystem.TryGetLocalisedValue", "LocalisationSystem.LookUp",
    "Language.English", "LocalisationSystem.get_language",
    "LocalisationSystem.get_voiceLanguage", "LocalisationSystem.GetLanguageAbbreviationText",
    "LocalisationSystem.GetNextOrPreviousLanguage", "GhostCollection.ghosts", "GhostData.type",
    "GhostData.ghostNameLocalisationKey", "GhostData.evidence1", "GhostData.evidence2", "GhostData.evidence3",
    "Object.get_name", "Object.set_name", "Object.Internal_CloneSingleWithParent", "Object.Destroy", "GameObject",
    "GameObject.SetActive", "GameObject.get_activeSelf", "GameObject.get_transform", "Component.get_transform", "Component.get_gameObject",
    "Component.GetComponent", "Component.GetComponentInChildren", "Behaviour.get_enabled", "Behaviour.set_enabled",
    "Transform.get_childCount", "Transform.GetChild", "Transform.GetSiblingIndex", "Transform.SetSiblingIndex",
    "RectTransform.get_rect", "RectTransform.get_anchoredPosition", "GridLayoutGroup",
    "GridLayoutGroup.get_cellSize", "GridLayoutGroup.set_cellSize", "GridLayoutGroup.get_spacing",
    "GridLayoutGroup.get_constraint", "GridLayoutGroup.set_constraint", "GridLayoutGroup.get_constraintCount",
    "GridLayoutGroup.set_constraintCount", "GridConstraint.FixedColumnCount", "Button", "Button.Press",
    "UIBehaviour.IsActive", "Selectable.IsInteractable", "Scene.JournalBookContent",
    "Scene.JournalGhostGrid", "Scene.JournalGhostButton", "Scene.JournalGhostList", "Scene.JournalGhostEntry",
    "Scene.JournalGhostPage", "Scene.JournalGhostPageTitle", "Scene.JournalGhostPageDescription",
    "Scene.JournalGhostPageEvidence1", "Scene.JournalGhostPageEvidence2", "Scene.JournalGhostPageEvidence3",
    "JournalController.PageSync", "JournalController.currentPage", "JournalController.photonView",
    "PhotonView.get_IsMine", "PhotonView.owner", "PhotonMessageInfo.Sender", "JournalController.GetGhostType",
    "JournalController.UnselectGhost", "Object.FindObjectsOfType(Type,bool)", "PhotonView.RPC(Player)", "RpcTarget.All",
    "RectTransform.set_anchoredPosition", "RectTransform.get_anchorMin", "RectTransform.set_anchorMin",
    "RectTransform.get_anchorMax", "RectTransform.set_anchorMax", "RectTransform.get_sizeDelta",
    "RectTransform.set_sizeDelta", "Scene.JournalContentsRows", "Scene.JournalContentsGhostRows",
    "Scene.JournalContentsGhostRow", "TextMeshProUGUI", "TMP_Text.set_text", "TMP_Text.get_text",
};

// What the game's page sync writes in the corner a ghost's page lies in, past the page's number in the
// book (JournalController.PageSync). The contents' numbers are that number, typed into the prefab.
constexpr int32_t kShownPageNumber = 2;

}  // namespace

std::optional<PageLayout> read_layout(std::string_view text) {
  // Paragraphs are what stands between line breaks, less the spaces around them; an empty one is part
  // of a separator.
  struct Paragraph {
    size_t start, end;  // the text, trimmed
  };
  std::vector<Paragraph> paragraphs;
  for (size_t at = 0; at <= text.size();) {
    size_t end = text.find('\n', at);
    if (end == std::string_view::npos) end = text.size();
    size_t a = at, b = end;
    while (a < b && (text[a] == ' ' || text[a] == '\r')) ++a;
    while (b > a && (text[b - 1] == ' ' || text[b - 1] == '\r')) --b;
    if (a < b) paragraphs.push_back({a, b});
    at = end + 1;
  }
  if (paragraphs.size() < 3) return std::nullopt;
  const auto label = [&](const Paragraph& p) -> std::optional<std::string> {
    const std::string_view body = text.substr(p.start, p.end - p.start);
    size_t colon = body.find(':');
    size_t width = 1;
    const size_t wide = body.find("\xEF\xBC\x9A");  // a full-width colon
    if (wide != std::string_view::npos && (colon == std::string_view::npos || wide < colon)) colon = wide, width = 3;
    if (colon == std::string_view::npos) return std::nullopt;
    return std::string(body.substr(0, colon + width));
  };
  const Paragraph& strength = paragraphs[paragraphs.size() - 2];
  const Paragraph& weakness = paragraphs.back();
  const auto s = label(strength), w = label(weakness);
  if (!s || !w) return std::nullopt;
  PageLayout out;
  out.separator = std::string(text.substr(strength.end, weakness.start - strength.end));
  out.strength = *s;
  out.weakness = *w;
  return out;
}

std::string page_text(const content::Ghost& g, std::string_view lang, const PageLayout& layout) {
  std::string out = pick(g.description, lang);
  out += layout.separator + layout.strength + " " + pick(g.strength, lang);
  out += layout.separator + layout.weakness + " " + pick(g.weakness, lang);
  if (!g.abilityName.empty()) {
    // The ability's name as a label, with the punctuation the game's labels end in.
    const std::string& label = layout.strength;
    const std::string colon = label.size() >= 3 && label.ends_with("\xEF\xBC\x9A") ? "\xEF\xBC\x9A"
                              : label.ends_with(":")                              ? ":"
                                                                                   : "";
    out += layout.separator + pick(g.abilityName, lang) + colon + " " + pick(g.abilityDescription, lang);
  }
  for (const content::Text& tell : g.tells) out += layout.separator + pick(tell, lang);
  return out;
}

std::optional<PageLayout> agreed_layout(const std::vector<std::string>& texts) {
  std::vector<std::pair<PageLayout, int>> counts;
  for (const std::string& t : texts) {
    const auto l = read_layout(t);
    if (!l) continue;
    auto it = std::find_if(counts.begin(), counts.end(), [&](const auto& c) { return c.first == *l; });
    if (it == counts.end()) counts.emplace_back(*l, 1);
    else ++it->second;
  }
  std::sort(counts.begin(), counts.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
  if (counts.empty() || counts[0].second < 2 || (counts.size() > 1 && counts[1].second == counts[0].second))
    return std::nullopt;
  return counts[0].first;
}

std::string ContentJournal::missing() const {
  std::string out;
  const PmHostApi* host = pm::api::detail::host();
  for (const char* key : kNeeded)
    if (!host || !host->game_entry(key)) out += (out.empty() ? "" : ", ") + std::string(key);
  return out;
}

bool ContentJournal::create_hooks() {
  g_self = this;
  using pm::api::hook;
  return hook<phasmo::JournalController::Awake>(&detour_awake, "content: the journals") &&
         hook<phasmo::Button::Press>(&detour_press, "content: journal entries pressed") &&
         hook<phasmo::JournalController::PageSync>(&detour_page_sync, "content: journal page syncs");
}

bool ContentJournal::custom_page(void* journal, int32_t page) const {
  for (const Journal& j : journals_)
    if (j.journal.get() == journal && j.extension) return page >= j.extension->firstMoved;
  return page >= static_cast<int32_t>(pm::api::RefArray{phasmo::JournalController::pages.get(journal)}.size());
}

bool ContentJournal::custom_page_on(void* view, int32_t page) const {
  if (!view) return false;
  for (const Journal& j : journals_) {
    void* journal = j.journal.get();
    if (pm::api::alive(journal) && phasmo::JournalController::photonView.get(journal) == view) return custom_page(journal, page);
  }
  return false;
}

bool ContentJournal::page_syncs(void* journal, int32_t page, const void* info) {
  const size_t count = pm::api::RefArray{phasmo::JournalController::pages.get(journal)}.size();
  if (page >= 0 && static_cast<size_t>(page) < count) return true;
  // The game checks first that the sender owns the book, and handles a sender who does not itself:
  // such a call, and any whose sender or owner is not known here, goes to the game untouched. Only the
  // owner's own sync, which the game would take as far as its throw, is skipped.
  if (!privateRoom_ || !info) return true;
  void* view = phasmo::JournalController::photonView.get(journal);
  void* owner = view ? phasmo::PhotonView::owner.get(view) : nullptr;
  void* sender = phasmo::PhotonMessageInfo::Sender.get(info);
  if (!owner || !sender || sender != owner) return true;
  if (!pageSkipLogged_)
    PM_LOG("content: a page sync beyond this client's journal (page %d of %zu) was skipped; the copy keeps its page",
           page, count);
  pageSkipLogged_ = true;
  return false;
}

void ContentJournal::note_existing() {
  const pm::api::RefArray found{phasmo::Object::FindAllOfType(phasmo::JournalController::type.type_object(), true)};
  for (size_t i = 0; i < found.size(); ++i)
    if (found[i] && pm::api::alive(found[i])) after_awake(found[i]);
}

void ContentJournal::after_awake(void* journal) {
  for (const Journal& j : journals_)
    if (j.journal.get() == journal) return;
  journals_.push_back({pm::api::Held(journal)});
}

void ContentJournal::tick(bool privateRoom) {
  privateRoom_ = privateRoom;
  std::erase_if(journals_, [](const Journal& j) { return !pm::api::alive(j.journal.get()); });
  std::erase_if(entries_, [](const Entry& e) { return !pm::api::alive(e.journal.get()); });
  // What the journals show changed (ContentTypes::shown): each journal given ghosts under the last
  // decision is put back first, every one before any is given the new ones, since putting one back
  // makes its lists again from the collection. The old ghosts' keys are no longer answered.
  const unsigned shown = types_.shown_decisions();
  for (Journal& j : journals_)
    if (j.extension && j.extension->under != shown) restore(j);
  if (shown != shown_) {
    shown_ = shown;
    customKeys_.clear();
  }
  // Whether this client matches the host shows or hides the lines (Registry::lines_for), and a contract holds that as it
  // loads: each time it changes, the pages with lines are written again.
  const unsigned decisions = registry_.agreement_decisions();
  if (decisions != decisions_) {
    decisions_ = decisions;
    for (Journal& j : journals_) j.refreshed = false;
  }
  const std::map<int32_t, ContentTypes::HeldGhost>& ghosts = types_.shown();
  for (Journal& j : journals_) {
    if (!j.learned) {
      j.learned = true;
      j.fit = learn(j.journal.get());  // logged when unfit
    }
    // Pages with lines write their text again once each time this client enters a private room, an
    // extended journal's too.
    if (!privateRoom_) j.refreshed = false;
    else if (!j.refreshed && registry_.has_lines() && j.fit) {
      refresh_lines(j.journal.get());
      j.refreshed = true;
    }
    // Once per decision, and only with custom ghosts shown: lines only otherwise.
    if (!j.fit || ghosts.empty() || j.tried == shown) continue;
    void* collection = phasmo::JournalController::allGhostsData.get(j.journal.get());
    if (types_.decided()) {
      bool placed = true;
      for (const auto& [type, h] : ghosts) placed = placed && types_.name_key(type).has_value();
      if (!placed) continue;  // the contract's collection does not hold them all yet
      if (collection != types_.collection()) {
        j.tried = shown;
        PM_LOG("content: a journal lists another collection than the contract's; it gets no custom ghosts");
        continue;
      }
    } else if (!types_.offer(collection ? phasmo::GhostCollection::ghosts.get(collection) : nullptr)) {
      continue;  // the lobby: the collection the journal lists does not hold them all yet
    }
    j.tried = shown;
    if (extend(j, ghosts, shown)) PM_LOG("content: custom ghosts added to a journal (%zu pages)", ghosts.size());
    else PM_LOG("content: the custom ghosts are missing from a journal, which stays as the game made it");
  }
}

void ContentJournal::withdraw() {
  for (Journal& j : journals_)
    if (j.extension && pm::api::alive(j.journal.get())) restore(j);
  journals_.clear();
  entries_.clear();
  customKeys_.clear();
  vanillaDescriptions_.clear();
}

bool ContentJournal::answers() const {
  return privateRoom_ && (!customKeys_.empty() || (registry_.has_lines() && !vanillaDescriptions_.empty()));
}

std::string ContentJournal::description_key(int32_t type) const { return descPrefix_ + std::to_string(type) + descSuffix_; }

std::optional<PageLayout> ContentJournal::layout(const GameText& game) const {
  // The layout the game's pages agree on in the language asked, else in English.
  for (const bool english : {false, true}) {
    std::vector<std::string> texts;
    for (const auto& [type, k] : vanillaDescriptions_)
      if (auto text = game(k, english)) texts.push_back(std::move(*text));
    if (const auto l = agreed_layout(texts)) return l;
  }
  if (!layoutLogged_) PM_LOG("content: the game's ghost pages agree on no layout; custom texts on them stay empty");
  layoutLogged_ = true;
  return std::nullopt;
}

std::optional<std::string> ContentJournal::text_for_key(std::string_view key, std::string_view lang,
                                                        const GameText& game) const {
  if (const auto it = customKeys_.find(key); it != customKeys_.end()) {
    const auto held = types_.shown().find(it->second);
    const content::Ghost* g = held != types_.shown().end() ? registry_.registered(held->second.id) : nullptr;
    const auto l = g ? layout(game) : std::nullopt;
    if (!l) return std::nullopt;
    return page_text(*g, lang, *l);
  }
  if (!privateRoom_) return std::nullopt;
  for (const auto& [type, k] : vanillaDescriptions_) {
    if (k != key) continue;
    const char* id = registry_.game_enums().ghost_id(type);
    const std::vector<content::Text> lines = id ? registry_.lines_for(id) : std::vector<content::Text>{};
    if (lines.empty()) return std::nullopt;
    const auto own = game(key, false);
    const auto l = own ? layout(game) : std::nullopt;
    if (!l) return std::nullopt;
    std::string out = *own;
    for (const content::Text& line : lines) out += l->separator + pick(line, lang);
    return out;
  }
  return std::nullopt;
}

bool ContentJournal::press(void* button) {
  for (const Entry& e : entries_) {
    if (e.button.get() != button) continue;
    // What Press checks before it invokes onClick (where OnPointerClick jumps): the button active, then
    // interactable, both virtual.
    if (!phasmo::UIBehaviour::IsActive.virtual_call(button) || !phasmo::Selectable::IsInteractable.virtual_call(button))
      return true;
    if (pm::api::alive(e.journal.get())) phasmo::JournalController::GoToPage(e.journal.get(), e.page);
    return true;
  }
  return false;
}

int32_t ContentJournal::page_of(void* button) const {
  for (const Entry& e : entries_)
    if (e.button.get() == button) return e.page;
  return -1;
}

std::vector<void*> ContentJournal::journals() const {
  std::vector<void*> out;
  for (const Journal& j : journals_)
    if (pm::api::alive(j.journal.get())) out.push_back(j.journal.get());
  return out;
}

bool ContentJournal::learn(void* journal) {
  void* root = transform_of(journal);
  // Where a ghost page's texts are among its children: every page of the game's has them at the
  // Spirit's places (read from the PCPlayer prefab, sdk/systems/journal.md).
  void* title = phasmo::Journal::GhostPageTitle.find(root);
  void* description = phasmo::Journal::GhostPageDescription.find(root);
  void* evidence[3] = {phasmo::Journal::GhostPageEvidence1.find(root), phasmo::Journal::GhostPageEvidence2.find(root),
                       phasmo::Journal::GhostPageEvidence3.find(root)};
  if (!title || !description || !evidence[0] || !evidence[1] || !evidence[2]) {
    PM_LOG("content: a journal has no ghost page where the game's are; it gets no custom ghosts");
    return false;
  }
  titleAt_ = phasmo::Transform::GetSiblingIndex(title);
  descriptionAt_ = phasmo::Transform::GetSiblingIndex(description);
  for (int i = 0; i < 3; ++i) evidenceAt_[i] = phasmo::Transform::GetSiblingIndex(evidence[i]);
  const int32_t lastAt = std::max({titleAt_, descriptionAt_, evidenceAt_[0], evidenceAt_[1], evidenceAt_[2]});

  // The game's ghosts as the journal's collection names them: a vanilla type by its name key.
  const content::GameEnums& enums = registry_.game_enums();
  struct Vanilla {
    int32_t type;
    int32_t evidence[3];
  };
  std::map<std::string, Vanilla> byNameKey;
  ghostNameKeys_.clear();
  void* collection = phasmo::JournalController::allGhostsData.get(journal);
  const pm::api::List ghosts{collection ? phasmo::GhostCollection::ghosts.get(collection) : nullptr};
  for (size_t i = 0; i < ghosts.size(); ++i) {
    void* data = ghosts[i];
    const int32_t type = data ? phasmo::GhostData::ghostType.get(data) : -1;
    if (!data || !enums.ghost_id(type)) continue;
    byNameKey[pm::api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(data))] = {
        type,
        {phasmo::GhostData::evidence1.get(data), phasmo::GhostData::evidence2.get(data),
         phasmo::GhostData::evidence3.get(data)}};
    ghostNameKeys_.insert(pm::api::to_utf8(phasmo::GhostData::ghostNameLocalisationKey.get(data)));
  }

  // Each game page of a vanilla ghost: its description key, and the keys its evidence texts show.
  vanillaDescriptions_.clear();
  std::map<int32_t, std::set<std::string>> candidates;  // evidence value -> keys every page of it shows
  std::map<std::pair<std::string, std::string>, int> forms;  // each form a description key's name allows -> pages that allow it
  const pm::api::RefArray pages{phasmo::JournalController::pages.get(journal)};
  for (size_t p = 0; p < pages.size(); ++p) {
    void* page = transform_of_object(pages[p]);
    if (!page || phasmo::Transform::get_childCount(page) <= lastAt) continue;
    const auto it = byNameKey.find(key_of(localiser_of(phasmo::Transform::GetChild(page, titleAt_))));
    if (it == byNameKey.end()) continue;
    const Vanilla& v = it->second;
    const std::string desc = key_of(localiser_of(phasmo::Transform::GetChild(page, descriptionAt_)));
    if (desc.empty()) continue;
    vanillaDescriptions_[v.type] = desc;
    const std::string name(std::string_view(enums.ghost_id(v.type)).substr(content::kVanillaPrefix.size()));
    // The game's keys aren't all regular, so no page decides alone: the Poltergeist is Journal2_Polter, the Mimic Journal2_TheMimic.
    for (const auto& form : splits(desc, name)) ++forms[form];
    std::set<std::string> shown;
    for (int i = 0; i < 3; ++i) {
      const std::string k = key_of(localiser_of(phasmo::Transform::GetChild(page, evidenceAt_[i])));
      if (!k.empty()) shown.insert(k);
    }
    for (int i = 0; i < 3; ++i) {
      const int32_t e = v.evidence[i];
      if (e == enums.noEvidence) continue;
      auto [c, fresh] = candidates.try_emplace(e, shown);
      if (!fresh)
        std::erase_if(c->second, [&](const std::string& k) { return !shown.count(k); });
    }
  }
  // An evidence's key is the one key every page of a ghost that gives it shows.
  evidenceKeys_.clear();
  for (const auto& [e, keys] : candidates)
    if (keys.size() == 1) evidenceKeys_[e] = *keys.begin();
  // The form most pages agree on, two at least, with no tie, as the labels of a page's text are chosen.
  int most = 0, atMost = 0;
  const std::pair<std::string, std::string>* best = nullptr;
  for (const auto& [form, agreeing] : forms) {
    if (agreeing > most) {
      most = agreeing;
      atMost = 1;
      best = &form;
    } else if (agreeing == most) {
      ++atMost;
    }
  }
  descForm_ = best && most >= 2 && atMost == 1;
  if (descForm_) {
    descPrefix_ = best->first;
    descSuffix_ = best->second;
  }
  return true;
}

void ContentJournal::refresh_lines(void* journal) {
  // A page whose ghost has lines shows them, or stops showing them, from its localiser's next writing
  // (Registry::lines_for answers as this client's match with the host stands); set_Key writes now
  // (TextLocaliserUI.set_Key). Its key is the one learnt for its ghost.
  const content::GameEnums& enums = registry_.game_enums();
  const int32_t lastAt = std::max({titleAt_, descriptionAt_, evidenceAt_[0], evidenceAt_[1], evidenceAt_[2]});
  const pm::api::RefArray pages{phasmo::JournalController::pages.get(journal)};
  for (size_t p = 0; p < pages.size(); ++p) {
    void* page = transform_of_object(pages[p]);
    if (!page || phasmo::Transform::get_childCount(page) <= lastAt) continue;
    void* localiser = localiser_of(phasmo::Transform::GetChild(page, descriptionAt_));
    const std::string key = key_of(localiser);
    for (const auto& [type, k] : vanillaDescriptions_) {
      const char* id = k == key ? enums.ghost_id(type) : nullptr;
      if (id && registry_.has_lines_for(id)) set_key(localiser, key);  // shown or not: written again
    }
  }
}

bool ContentJournal::extend(Journal& j, const std::map<int32_t, ContentTypes::HeldGhost>& ghosts, unsigned under) {
  if (!descForm_) {
    PM_LOG("content: the game's ghost pages show no one description key form; no custom ghost pages");
    return false;
  }
  void* journal = j.journal.get();
  void* root = transform_of(journal);
  void* book = phasmo::Journal::BookContent.find(root);
  void* grid = phasmo::Journal::GhostGrid.find(root);
  void* list = phasmo::Journal::GhostList.find(root);
  void* buttonAt = phasmo::Journal::GhostButton.find(root);
  void* entryAt = phasmo::Journal::GhostEntry.find(root);
  void* pageAt = phasmo::Journal::GhostPage.find(root);
  void* contentsFirst = phasmo::Journal::ContentsRows.find(root);
  void* contentsSecond = phasmo::Journal::ContentsGhostRows.find(root);
  void* rowAt = phasmo::Journal::ContentsGhostRow.find(root);
  void* templateButton = buttonAt ? phasmo::Component::GetComponent(buttonAt, phasmo::GhostButton::type.type_object()) : nullptr;
  void* pages = phasmo::JournalController::pages.get(journal);
  void* buttons = phasmo::JournalController::allGhostTypeButtons.get(journal);
  void* sync = phasmo::JournalController::ghostSync.get(journal);
  void* allGhosts = phasmo::JournalController::allGhosts.get(journal);
  void* ghostButtons = phasmo::JournalController::ghostButtons.get(journal);
  if (!book || !grid || !list || !templateButton || !entryAt || !pageAt || !contentsFirst || !contentsSecond || !rowAt ||
      !pages || !buttons || !sync || !allGhosts || !ghostButtons)
    return false;
  // Every operation the change needs is found before anything changes.
  if (!ops_.has_clear(allGhosts) || !ops_.has_clear(ghostButtons) || !ops_.has_initialise(sync)) {
    PM_LOG("content: the journal's lists or guess sync lack the methods the custom ghosts need");
    return false;
  }
  // The Ghosts tab: the tab the game selects for the last page (min <= page <= max, the first such in
  // order). The PCPlayer prefab's covers 24 to 55 for 55 pages (0 to 54).
  const int32_t firstPage = static_cast<int32_t>(pm::api::RefArray{pages}.size());
  const pm::api::RefArray tabs{phasmo::JournalController::journalTabs.get(journal)};
  void* ghostsTab = nullptr;
  for (size_t i = 0; i < tabs.size() && !ghostsTab; ++i)
    if (void* tab = tabs[i]; tab && phasmo::JournalTab::min.get(tab) <= static_cast<float>(firstPage - 1) &&
                             static_cast<float>(firstPage - 1) <= phasmo::JournalTab::max.get(tab))
      ghostsTab = tab;
  if (!ghostsTab) {
    PM_LOG("content: no journal tab covers the book's last page; no custom ghost pages");
    return false;
  }

  if (!languagesLogged_) {
    // A text in a language the game does not have never shows.
    const std::vector<std::string> languages = game_languages();
    for (const auto& [type, h] : ghosts)
      for (const auto& [lang, text] : h.name)
        if (!languages.empty() && std::find(languages.begin(), languages.end(), lower(lang)) == languages.end())
          PM_LOG("content: the ghost \"%s\" has text in \"%s\", a language the game does not have; it never shows",
                 h.id.c_str(), lang.c_str());
    languagesLogged_ = true;
  }

  const int32_t gridBefore = static_cast<int32_t>(active_children(grid).size());
  const std::vector<void*> listEntries = active_children(list);
  const int32_t listBefore = static_cast<int32_t>(listEntries.size());
  const std::optional<GridShape> listShape = placed_shape(list);  // before the copies join it
  // What changes, to put back when what the journals show changes (restore).
  void* gridLayout = grid_of(grid);
  void* listLayout = grid_of(list);
  Extension ext{under,
                pm::api::Held(phasmo::JournalController::allGhostTypeButtons.get(journal)),
                pm::api::Held(phasmo::JournalController::pages.get(journal)),
                pm::api::Held(nullptr),
                0.0f,
                pm::api::Held(grid),
                gridLayout ? phasmo::GridLayoutGroup::get_cellSize(gridLayout) : phasmo::Vector2{0, 0},
                pm::api::Held(list),
                listLayout && phasmo::Behaviour::get_enabled(listLayout),
                listLayout ? phasmo::GridLayoutGroup::get_cellSize(listLayout) : phasmo::Vector2{0, 0},
                listLayout ? phasmo::GridLayoutGroup::get_constraint(listLayout) : 0,
                listLayout ? phasmo::GridLayoutGroup::get_constraintCount(listLayout) : 0,
                {},
                {}};
  const auto keep_placement = [&](void* rect) {
    ext.placements.push_back({pm::api::Held(rect), phasmo::RectTransform::get_anchorMin(rect),
                              phasmo::RectTransform::get_anchorMax(rect), phasmo::RectTransform::get_sizeDelta(rect),
                              phasmo::RectTransform::get_anchoredPosition(rect), phasmo::Transform::GetSiblingIndex(rect)});
  };
  // The grid switched on below lays out the game's own entries too: their hand placement is kept.
  if (listShape)
    for (void* entry : listEntries) keep_placement(entry);
  // The Home tab's contents: the game's rows get shorter below, and their numbers.
  const std::vector<void*> firstRows = active_children(contentsFirst), secondRows = active_children(contentsSecond);
  for (const std::vector<void*>* rows : {&firstRows, &secondRows})
    for (void* row : *rows) {
      keep_placement(row);
      if (void* number = row_number(row)) keep_placement(number);
    }
  std::set<std::string> ghostKeys = ghostNameKeys_;
  // A name goes on the first page when it comes before the second's first ghost, as the game divides them.
  const GhostRows gameSecond = ghost_rows(contentsSecond, ghostKeys);
  const std::string secondStarts = gameSecond.rows.empty() ? std::string() : lower(name_of(gameSecond.rows.front().get()));
  // The book's ghost pages by the name key their titles show, and where the first of them lies.
  const pm::api::RefArray gamePages{pages};
  std::map<std::string, void*> pageOfKey;
  size_t firstGhostPage = gamePages.size();
  for (size_t p = 0; p < gamePages.size(); ++p) {
    void* t = transform_of_object(gamePages[p]);
    if (!t || phasmo::Transform::get_childCount(t) <= titleAt_) continue;
    const std::string key = key_of(localiser_of(phasmo::Transform::GetChild(t, titleAt_)));
    if (!ghostNameKeys_.count(key)) continue;
    pageOfKey[key] = gamePages[p];
    firstGhostPage = std::min(firstGhostPage, p);
  }
  std::vector<void*> newButtons, newPages;
  std::vector<pm::api::Held> made;  // every copy, held until the arrays hold it or it is taken out
  const size_t entriesBefore = entries_.size();
  std::vector<std::string> keysAdded;
  // Takes the copies out again: off at once, destroyed at the frame's end, forgotten here.
  const auto take_out = [&] {
    for (const pm::api::Held& object : made) {
      void* go = game_object_of(object.get());
      phasmo::GameObject::SetActive(go, false);
      phasmo::Object::Destroy(go, 0.0f);
    }
    entries_.resize(entriesBefore);
    for (const std::string& k : keysAdded) customKeys_.erase(k);
  };
  std::vector<std::pair<pm::api::Held, pm::api::Held>> newEntries;  // each entry's Button and its page
  for (const auto& [type, h] : ghosts) {
    const std::string name = pick(h.name, "en");
    const std::optional<std::string> nameKey = types_.name_key(type);
    void* nameString = pm::api::make_string(name);
    // The guess button, copied with its children (the other players' marks included).
    void* button = phasmo::Object::Instantiate(templateButton, grid, false);
    if (!button) return take_out(), false;
    made.emplace_back(button);
    phasmo::Object::set_name(button, nameString);
    phasmo::GhostButton::ghostType.set(button, type);
    set_key(localiser_in(button), *nameKey);
    phasmo::Transform::SetSiblingIndex(transform_of(button), alphabetical_index(grid, name));
    newButtons.push_back(button);
    // Its page: the name, the text and the evidence, each a key the game writes.
    void* pageCopy = phasmo::Object::Instantiate(pageAt, book, false);
    if (!pageCopy) return take_out(), false;
    made.emplace_back(pageCopy);
    phasmo::Object::set_name(pageCopy, nameString);
    set_key(localiser_of(phasmo::Transform::GetChild(pageCopy, titleAt_)), *nameKey);
    const std::string descKey = description_key(type);
    customKeys_[descKey] = type;
    keysAdded.push_back(descKey);
    set_key(localiser_of(phasmo::Transform::GetChild(pageCopy, descriptionAt_)), descKey);
    for (int i = 0; i < 3; ++i) {
      void* child = phasmo::Transform::GetChild(pageCopy, evidenceAt_[i]);
      // The game's own evidence by the key its pages show, a custom one by its row's name key.
      const auto key = evidenceKeys_.find(h.evidence[i]);
      const std::optional<std::string> custom = evidence_ ? evidence_->name_key(h.evidence[i]) : std::nullopt;
      if (key != evidenceKeys_.end()) set_key(localiser_of(child), key->second);
      else if (custom) set_key(localiser_of(child), *custom);
      else phasmo::GameObject::SetActive(game_object_of(child), false);  // no evidence there, or none named
    }
    void* pageObject = game_object_of(pageCopy);
    phasmo::GameObject::SetActive(pageObject, false);
    newPages.push_back(pageObject);
    pageOfKey[*nameKey] = pageObject;
    // Its entry in the Ghosts tab's list.
    void* entry = phasmo::Object::Instantiate(entryAt, list, false);
    if (!entry) return take_out(), false;
    made.emplace_back(entry);
    phasmo::Object::set_name(entry, nameString);
    set_key(localiser_in(entry), *nameKey);
    phasmo::Transform::SetSiblingIndex(entry, alphabetical_index(list, name));
    void* entryButton = phasmo::Component::GetComponent(entry, phasmo::Button::type.type_object());
    if (!entryButton) return take_out(), false;
    // Its row on the Home tab's contents, put in order and given its number below.
    void* contents = lower(name) < secondStarts ? contentsFirst : contentsSecond;
    void* row = phasmo::Object::Instantiate(rowAt, contents, false);
    if (!row) return take_out(), false;
    made.emplace_back(row);
    phasmo::Object::set_name(row, nameString);
    set_key(localiser_in(row), *nameKey);
    ghostKeys.insert(*nameKey);
    if (!number_text(row)) return take_out(), false;
    newEntries.emplace_back(pm::api::Held(entryButton), pm::api::Held(pageObject));
  }
  // The book's ghost pages, from where the first lay, in the order the contents list them, so the numbers count up. The pages before
  // them keep their numbers, which the game's tabs name; its list entries name their pages by object (GoToPage(GameObject)).
  const GhostRows firstGhosts = ghost_rows(contentsFirst, ghostKeys), secondGhosts = ghost_rows(contentsSecond, ghostKeys);
  std::vector<void*> newBook;
  for (size_t p = 0; p < firstGhostPage; ++p) newBook.push_back(gamePages[p]);
  std::set<void*> placed;
  std::vector<std::pair<pm::api::Held, int32_t>> rowPages;  // each ghost row and its page's number in the book
  for (const GhostRows* ghostRows : {&firstGhosts, &secondGhosts})
    for (const pm::api::Held& row : ghostRows->rows) {
      const auto it = pageOfKey.find(key_of(localiser_in(row.get())));
      if (it == pageOfKey.end()) continue;  // a row no page's title names keeps what it says
      rowPages.emplace_back(pm::api::Held(row.get()), static_cast<int32_t>(newBook.size()));
      newBook.push_back(it->second);
      placed.insert(it->second);
      pageOfKey.erase(it);
    }
  // Any page no row names keeps its order after them.
  for (size_t p = firstGhostPage; p < gamePages.size(); ++p)
    if (!placed.count(gamePages[p])) newBook.push_back(gamePages[p]);
  int32_t firstMoved = 0;
  while (static_cast<size_t>(firstMoved) < gamePages.size() && gamePages[static_cast<size_t>(firstMoved)] == newBook[static_cast<size_t>(firstMoved)])
    ++firstMoved;
  void* allButtons = appended(buttons, phasmo::GhostButton::type, newButtons);
  void* allPages = phasmo::GameObject::type.make_array(newBook.size());
  if (!allButtons || !allPages) return take_out(), false;
  const pm::api::RefArray bookArray{allPages};
  for (size_t p = 0; p < newBook.size(); ++p) bookArray.set(p, newBook[p]);
  for (const auto& [entryButton, pageObject] : newEntries)
    entries_.push_back({pm::api::Held(entryButton.get()), pm::api::Held(journal),
                        static_cast<int32_t>(std::find(newBook.begin(), newBook.end(), pageObject.get()) - newBook.begin())});
  const pm::api::Held keepButtons(allButtons), keepPages(allPages);

  // From here the journal changes; a failure puts it back as it was.
  const float tabMax = phasmo::JournalTab::max.get(ghostsTab);
  const pm::api::RefArray old{buttons};
  std::vector<pm::api::Held> delegates;  // each game button's subscriptions, as Initialise made them
  for (size_t i = 0; i < old.size(); ++i)
    delegates.emplace_back(old[i] ? phasmo::NetworkThreeStateButton::userStateChanged.get(old[i]) : nullptr);
  const auto restore_delegates = [&] {
    for (size_t i = 0; i < old.size(); ++i)
      if (old[i]) phasmo::NetworkThreeStateButton::userStateChanged.set(old[i], delegates[i].get());
  };
  const auto put_back = [&] {
    phasmo::JournalController::allGhostTypeButtons.set(journal, buttons);
    phasmo::JournalController::pages.set(journal, pages);
    phasmo::JournalTab::max.set(ghostsTab, tabMax);
    take_out();
  };
  phasmo::JournalController::allGhostTypeButtons.set(journal, allButtons);
  phasmo::JournalController::pages.set(journal, allPages);
  // The Ghosts tab covers the new pages, its max moved as far as the pages were added.
  phasmo::JournalTab::max.set(ghostsTab, tabMax + static_cast<float>(newPages.size()));
  // The journal's lists, made again by its own builder from its collection (which holds the custom
  // ghosts) and its buttons. The dictionary goes first: its builder adds with Add, which throws on a
  // ghost already there, while the list takes a ghost again without complaint.
  if (!ops_.clear(ghostButtons)) {
    put_back();
    PM_LOG("content: the journal's ghost buttons could not be emptied");
    return false;
  }
  if (!ops_.clear(allGhosts)) {
    put_back();
    phasmo::JournalController::RebuildGhosts(journal);  // the buttons paired again, with the game's own
    PM_LOG("content: the journal's ghost list could not be emptied; it holds its old ghosts and then the whole "
           "collection again, custom ghosts included, with no button for them");
    return false;
  }
  phasmo::JournalController::RebuildGhosts(journal);
  // The sync controller reads the buttons again and subscribes to each; the game's buttons keep the one
  // subscription they had.
  if (!ops_.initialise(sync)) {
    restore_delegates();
    put_back();
    const bool clearedButtons = ops_.clear(ghostButtons);
    const bool clearedList = ops_.clear(allGhosts);
    phasmo::JournalController::RebuildGhosts(journal);
    const bool reread = ops_.initialise(sync);  // reads the game's own buttons again
    restore_delegates();
    PM_LOG("content: the journal's guess sync could not read the custom ghosts' buttons; putting the journal back, "
           "its buttons were %s, its list %s, and its guess sync %s",
           clearedButtons ? "emptied" : "not emptied", clearedList ? "emptied" : "not emptied",
           reread ? "read its own buttons again" : "could not read its own buttons again");
    return false;
  }
  restore_delegates();
  if (listShape) use_grid(list, *listShape);
  fit_grid(grid, gridBefore, gridBefore + static_cast<int32_t>(newButtons.size()));
  fit_grid(list, listBefore, listBefore + static_cast<int32_t>(newButtons.size()));
  fit_contents(contentsFirst, static_cast<int32_t>(firstRows.size()), contentsSecond, static_cast<int32_t>(secondRows.size()));
  // The contents in order, each ghost's number the one its page shows.
  for (const GhostRows* ghostRows : {&firstGhosts, &secondGhosts})
    for (size_t i = 0; i < ghostRows->rows.size(); ++i)
      phasmo::Transform::SetSiblingIndex(ghostRows->rows[i].get(), ghostRows->places[i]);
  for (const auto& [row, number] : rowPages) {
    void* text = number_text(row.get());
    if (!text) continue;
    ext.numbers.push_back({pm::api::Held(text), pm::api::Held(phasmo::TMP_Text::get_text(text))});
    phasmo::TMP_Text::set_text(text, pm::api::make_string(std::to_string(number + kShownPageNumber)));
  }
  phasmo::JournalController::SetGhostTypes(journal);  // cross out what the chosen evidence already rules out
  ext.ghostsTab = ghostsTab;
  ext.tabMax = tabMax;
  ext.made = std::move(made);
  for (const auto& [type, h] : ghosts) ext.types.push_back(type);
  ext.firstMoved = firstMoved;
  j.extension = std::move(ext);
  turn_reader_back(journal, firstMoved, ghostsTab);
  return true;
}

void ContentJournal::restore(Journal& j) {
  void* journal = j.journal.get();
  Extension& e = *j.extension;
  // The game's arrays and tab as they were; each game button keeps the one subscription it has.
  const pm::api::RefArray old{e.buttons.get()};
  std::vector<pm::api::Held> delegates;
  for (size_t i = 0; i < old.size(); ++i)
    delegates.emplace_back(old[i] ? phasmo::NetworkThreeStateButton::userStateChanged.get(old[i]) : nullptr);
  phasmo::JournalController::allGhostTypeButtons.set(journal, e.buttons.get());
  phasmo::JournalController::pages.set(journal, e.pages.get());
  if (e.ghostsTab.get()) phasmo::JournalTab::max.set(e.ghostsTab.get(), e.tabMax);
  // The copies: off at once, destroyed at the frame's end.
  for (const pm::api::Held& object : e.made) {
    void* go = game_object_of(object.get());
    phasmo::GameObject::SetActive(go, false);
    phasmo::Object::Destroy(go, 0.0f);
  }
  std::erase_if(entries_, [&](const Entry& entry) { return entry.journal.get() == journal; });
  // The journal's lists, made again by its own builder from its collection, which no longer holds the
  // custom ghosts, and its guess sync reading the game's buttons again.
  void* ghostButtons = phasmo::JournalController::ghostButtons.get(journal);
  void* allGhosts = phasmo::JournalController::allGhosts.get(journal);
  const bool cleared = ghostButtons && allGhosts && ops_.clear(ghostButtons) && ops_.clear(allGhosts);
  if (cleared) phasmo::JournalController::RebuildGhosts(journal);
  void* sync = phasmo::JournalController::ghostSync.get(journal);
  const bool reread = sync && ops_.initialise && ops_.initialise(sync);
  for (size_t i = 0; i < old.size(); ++i)
    if (old[i]) phasmo::NetworkThreeStateButton::userStateChanged.set(old[i], delegates[i].get());
  // A guess on a custom ghost (SelectGhost keeps the ghost's type in the journal) has no button now.
  const int32_t guess = phasmo::JournalController::GetGhostType(journal);
  if (std::find(e.types.begin(), e.types.end(), guess) != e.types.end())
    phasmo::JournalController::UnselectGhost(journal);
  // The grids as they were. A list whose grid was off gets it off again, then each entry the hand
  // placement the grid replaced.
  if (void* grid = grid_of(e.grid.get())) phasmo::GridLayoutGroup::set_cellSize(grid, e.gridCell);
  if (void* list = grid_of(e.list.get())) {
    phasmo::Behaviour::set_enabled(list, e.listOn);
    phasmo::GridLayoutGroup::set_constraint(list, e.listConstraint);
    phasmo::GridLayoutGroup::set_constraintCount(list, e.listCount);
    phasmo::GridLayoutGroup::set_cellSize(list, e.listCell);
  }
  for (const Placement& p : e.placements) {
    void* entry = p.entry.get();
    if (!pm::api::alive(entry)) continue;
    phasmo::RectTransform::set_anchorMin(entry, p.anchorMin);
    phasmo::RectTransform::set_anchorMax(entry, p.anchorMax);
    phasmo::RectTransform::set_sizeDelta(entry, p.sizeDelta);
    phasmo::RectTransform::set_anchoredPosition(entry, p.position);
    phasmo::Transform::SetSiblingIndex(entry, p.sibling);
  }
  for (const ContentsNumber& n : e.numbers)
    if (pm::api::alive(n.text.get())) phasmo::TMP_Text::set_text(n.text.get(), n.said.get());
  phasmo::JournalController::SetGhostTypes(journal);
  turn_reader_back(journal, e.firstMoved, e.ghostsTab.get());
  if (cleared && reread) PM_LOG("content: a journal's custom ghosts were taken out; it is as the game made it");
  else
    PM_LOG("content: a journal's custom ghosts were taken out, but its lists %s and its guess sync %s",
           cleared ? "were made again" : "could not be emptied and keep them, with no button",
           reread ? "read its own buttons again" : "could not read its own buttons again");
  j.extension.reset();
}

}  // namespace pm::host
