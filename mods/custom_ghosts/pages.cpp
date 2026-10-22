#include "pages.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <map>
#include <string>
#include <vector>

#include "game.h"
#include "ghosts.h"
#include "grid_rule.h"
#include "il2cpp.h"
#include "log.h"
#include "ui.h"

namespace pg::custom::pages {
namespace {

using il2cpp::bind;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kNetworkSpots = 0x38;            // Network: List<PlayerSpot>
constexpr size_t kSpotPlayer = 0x38;              // PlayerSpot: networked Player
constexpr size_t kPlayerJournal = 0xC8;           // Player: JournalController
constexpr size_t kJournalIsOpen = 0xB8;           // JournalController: open
constexpr size_t kJournalPages = 0xC0;            // GameObject[] pages (index = page number)
constexpr size_t kJournalTabs = 0xC8;             // JournalTab[]: each tab's page range
constexpr size_t kTabMin = 0x20, kTabMax = 0x24;  // JournalTab: first and last page (float)
constexpr size_t kJournalGhostButtons = 0x1A0;    // GhostButton[] (the evidence spread's ghosts)
constexpr size_t kJournalGhostSync = 0x1B8;       // JournalGhostSyncController
constexpr size_t kSyncButtons = 0x30;             // its GhostButton[] (whose choices it sends)
constexpr size_t kJournalGhosts = 0x1D8;          // List<Ghost>: every ghost type
constexpr size_t kJournalGhostToButton = 0x208;   // Dictionary<Ghost, GhostButton>
constexpr size_t kGhostButtonType = 0x78;         // GhostButton: its GhostType
constexpr size_t kButtonUserChanged = 0x58;       // NetworkThreeStateButton: Action raised on the user's choice
constexpr uintptr_t kSetGhostTypesRva = 0x22C5090;  // JournalController.SetGhostTypes(): evidence filter
constexpr uintptr_t kGoToPageRva = 0x22C3140;       // JournalController.GoToPage(int)
constexpr int kFixedColumnCount = 1;                // GridLayoutGroup.Constraint.FixedColumnCount

// The game's Ghost struct (GhostData.GetGhostData), the key of the journal's ghost lookups.
struct GhostValue {
  int type;
  int pad0;
  il2cpp::String* name;
  int evidence[3];
  int pad1;
};

struct Api {
  il2cpp::Class* ghostButton = nullptr;
  il2cpp::Class* gameObject = nullptr;
  il2cpp::Object* textType = nullptr;
  il2cpp::Object* localiserType = nullptr;
  il2cpp::Object* gridType = nullptr;
  il2cpp::Object* buttonType = nullptr;
  il2cpp::Object* canvasType = nullptr;
  il2cpp::String* (*getName)(void*, const void*) = nullptr;
  void (*setName)(void*, il2cpp::String*, const void*) = nullptr;
  void (*setSiblingIndex)(void*, int, const void*) = nullptr;
  ui::Vec2 (*getCellSize)(void*, const void*) = nullptr;
  void (*setCellSize)(void*, ui::Vec2, const void*) = nullptr;
  ui::Vec2 (*getSpacing)(void*, const void*) = nullptr;
  int (*getConstraint)(void*, const void*) = nullptr;
  void (*setConstraint)(void*, int, const void*) = nullptr;
  int (*getConstraintCount)(void*, const void*) = nullptr;
  void (*setConstraintCount)(void*, int, const void*) = nullptr;
  ui::Rect (*getRect)(void*, const void*) = nullptr;
  ui::Vec2 (*getAnchoredPosition)(void*, const void*) = nullptr;
  void* (*getOnClick)(void*, const void*) = nullptr;
  void (*setOnClick)(void*, void*, const void*) = nullptr;
  void* (*worldCamera)(void*, const void*) = nullptr;
  void (*setGhostTypes)(void*, const void*) = nullptr;
  void (*goToPage)(void*, int, const void*) = nullptr;
  bool ok = false;
} api;

struct Entry {
  il2cpp::Held rect;  // the Ghosts tab list entry (RectTransform)
  int page;           // its page number
};
struct Extended {
  il2cpp::Held journal;  // JournalController
  il2cpp::Held canvas;   // its world-space Canvas, for hit tests
  std::vector<Entry> entries;
};
std::vector<Extended> g_journals;

bool lists_type(il2cpp::List* ghosts, int type) {
  for (int i = 0; ghosts && ghosts->items && i < ghosts->size; ++i)
    if (ghosts->items->data<GhostValue>()[i].type == type) return true;
  return false;
}

std::string name_of(void* object) {
  char buf[128] = {};
  if (il2cpp::String* s = api.getName(object, nullptr)) il2cpp::to_utf8(s, buf, sizeof buf);
  return buf;
}

std::string lower(std::string s) {
  for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  return s;
}

/** The sibling index that keeps a grid's active children in alphabetical order with `name` added. */
int alphabetical_index(void* grid, const std::string& name) {
  const std::string key = lower(name);
  int last = -1;
  for (int i = 0; i < game::child_count(grid); ++i) {
    void* t = game::child(grid, i);
    if (!game::active_self(game::game_object_of(t))) continue;
    if (lower(name_of(t)) > key) return i;
    last = i;
  }
  return last + 1;
}

int active_children(void* grid) {
  int n = 0;
  for (int i = 0; i < game::child_count(grid); ++i)
    if (game::active_self(game::game_object_of(game::child(grid, i)))) ++n;
  return n;
}

/** Lay out by its grid a list whose entries were placed by hand. The Ghosts tab list ships with its
 * GridLayoutGroup switched off, so an entry copied into it would sit on the one it was copied from:
 * the grid is set to the layout the entries were given, then switched on. */
void use_grid(void* gridTransform) {
  void* grid = game::get_component(gridTransform, api.gridType);
  if (!grid || game::enabled(grid)) return;
  std::vector<PlacedCell> placed;
  for (int i = 0; i < game::child_count(gridTransform); ++i) {
    void* t = game::child(gridTransform, i);
    if (!game::active_self(game::game_object_of(t))) continue;
    const ui::Vec2 at = api.getAnchoredPosition(t, nullptr);
    const ui::Rect rect = api.getRect(t, nullptr);
    placed.push_back({at.x, at.y, rect.width, rect.height});
  }
  const GridShape shape = baked_grid(placed);
  if (shape.columns <= 0) return;
  api.setCellSize(grid, {shape.cellWidth, shape.cellHeight}, nullptr);
  api.setConstraint(grid, kFixedColumnCount, nullptr);
  api.setConstraintCount(grid, shape.columns, nullptr);
  game::set_enabled(grid, true);
}

/** Shorten a grid's rows so `after` children fit in the rows `before` children filled. */
void fit_grid(void* gridTransform, int before, int after) {
  void* grid = game::get_component(gridTransform, api.gridType);
  if (!grid || before <= 0 || after <= before) return;
  ui::Vec2 cell = api.getCellSize(grid, nullptr);
  const ui::Vec2 spacing = api.getSpacing(grid, nullptr);
  int columns = 0;
  if (api.getConstraint(grid, nullptr) == kFixedColumnCount) columns = api.getConstraintCount(grid, nullptr);
  else columns = static_cast<int>((api.getRect(gridTransform, nullptr).width + spacing.x) / (cell.x + spacing.x));
  const float height = fitted_cell_height(cell.y, spacing.y, columns, before, after);
  if (height == cell.y) return;
  cell.y = height;
  api.setCellSize(grid, cell, nullptr);
}

/** Set a text object's words for good: its localiser would put the game's own back. */
void set_text(void* textTransform, const std::string& text) {
  if (!textTransform) return;
  if (api.localiserType)
    if (void* localiser = game::get_component(textTransform, api.localiserType)) game::set_enabled(localiser, false);
  if (void* tmp = game::get_component(textTransform, api.textType)) ui::set_text(tmp, text.c_str());
}

void rename(void* component, const char* name) { api.setName(game::game_object_of(component), il2cpp::string_new(name), nullptr); }

/** A copy of `array` with `extra` appended, of the same element class; null when it cannot be made. */
il2cpp::Array* appended(il2cpp::Array* array, il2cpp::Class* element, const std::vector<void*>& extra) {
  const uintptr_t n = array ? array->max_length : 0;
  il2cpp::Array* out = il2cpp::array_new(element, n + extra.size());
  if (!out) return nullptr;
  for (uintptr_t i = 0; i < n; ++i) out->data<void*>()[i] = array->data<void*>()[i];
  for (size_t i = 0; i < extra.size(); ++i) out->data<void*>()[n + i] = extra[i];
  return out;
}

bool call_add(void* collection, int argc, void* a, void* b = nullptr) {
  const il2cpp::Method* m = collection ? il2cpp::method(il2cpp::class_of(collection), "Add", argc) : nullptr;
  if (!m) return false;
  void* code = *reinterpret_cast<void* const*>(m);
  if (argc == 1) reinterpret_cast<void (*)(void*, void*, const void*)>(code)(collection, a, m);
  else reinterpret_cast<void (*)(void*, void*, void*, const void*)>(code)(collection, a, b, m);
  return true;
}

/** The ghost's journal page: what it is, its strength and weakness, and its evidence. */
void write_page(void* page, const Ghost& g) {
  set_text(game::find_child(page, "Title"), g.name);
  set_text(game::find_child(page, "Description"), std::string(g.description) + "\n\n<b>Strength:</b> " + g.strength +
                                                       "\n\n<b>Weakness:</b> " + g.weakness);
  set_text(game::find_child(page, "Evidence"), "Evidence");
  set_text(game::find_child(page, "Evidence 1"), evidence_name(g.evidence[0]));
  set_text(game::find_child(page, "Evidence 2 "), evidence_name(g.evidence[1]));
  set_text(game::find_child(page, "Evidence 3"), evidence_name(g.evidence[2]));
}

/** Add the custom ghosts to one journal. False when its layout is not the one this build expects. */
bool extend(void* journal, Extended& out) {
  void* book = game::find_child(game::transform_of(journal), "Content/GhostHuntingBookContent");
  void* grid = book ? game::find_child(book, "Evidence.2/All Ghosts") : nullptr;
  void* list = book ? game::find_child(book, "Ghost Select Page/Ghost Types List") : nullptr;
  void* buttonTemplate = grid ? game::find_child(grid, "Spirit") : nullptr;
  void* entryTemplate = list ? game::find_child(list, "Spirit") : nullptr;
  void* pageTemplate = book ? game::find_child(book, "Spirit") : nullptr;
  void* templateButton = buttonTemplate ? game::get_component(buttonTemplate, il2cpp::type_object(api.ghostButton)) : nullptr;
  auto* pages = il2cpp::field<il2cpp::Array*>(journal, kJournalPages);
  void* sync = il2cpp::field<void*>(journal, kJournalGhostSync);
  auto* ghosts = il2cpp::field<il2cpp::List*>(journal, kJournalGhosts);
  void* ghostToButton = il2cpp::field<void*>(journal, kJournalGhostToButton);
  if (!templateButton || !entryTemplate || !pageTemplate || !pages || !sync || !ghosts || !ghostToButton) return false;

  void* templateEntryButton = game::get_component(entryTemplate, api.buttonType);
  if (!templateEntryButton) { PG_LOG("pages: custom entry has no Button"); return false; }
  void* onClick = api.getOnClick(templateEntryButton, nullptr);
  auto* eventClass = onClick ? il2cpp::class_of(onClick) : nullptr;
  auto ctor = reinterpret_cast<void (*)(void*, const void*)>(eventClass ? il2cpp::method_ptr(eventClass, ".ctor", 0) : nullptr);
  if (!ctor) { PG_LOG("pages: independent ButtonClickedEvent is unavailable"); return false; }
  // Keep every prepared event on the native stack until its clone owns it. Event setup must finish
  // before any scene or collection changes, so a later failure cannot leave partial custom entries.
  il2cpp::Object* events[kGhostCount]{};
  try {
    for (int k = 0; k < kGhostCount; ++k) {
      events[k] = il2cpp::object_new(eventClass);
      if (!events[k]) { PG_LOG("pages: independent ButtonClickedEvent is unavailable"); return false; }
      ctor(events[k], nullptr);
    }
  } catch (...) {
    PG_LOG("pages: custom entry event initialization threw");
    return false;
  }

  const int gridBefore = active_children(grid), listBefore = active_children(list);
  use_grid(list);  // measured before the custom entries join it, each on the entry it is copied from
  const int firstPage = static_cast<int>(pages->max_length);
  std::vector<void*> buttons, pageObjects;
  out = {journal, game::get_component(game::transform_of(journal), api.canvasType), {}};
  for (int k = 0; k < kGhostCount; ++k) {
    const Ghost& g = kGhosts[k];
    // The guess button, cloned with its children (the other players' marks included).
    void* button = game::instantiate(templateButton, grid, false);
    if (!button) return false;
    rename(button, g.name);
    il2cpp::set_field<int>(button, kGhostButtonType, g.type);
    il2cpp::set_ref_field(button, kButtonUserChanged, il2cpp::field<void*>(templateButton, kButtonUserChanged));
    set_text(game::find_child(game::transform_of(button), "SpiritText (TMP)"), g.name);
    api.setSiblingIndex(game::transform_of(button), alphabetical_index(grid, g.name), nullptr);
    buttons.push_back(button);
    // Its page, and the list entry that opens it.
    void* page = game::instantiate(pageTemplate, book, false);
    void* entryButton = game::instantiate(templateEntryButton, list, false);
    if (!page || !entryButton) return false;
    // The cloned entry opens its custom page through handle_clicks; keep the template event intact.
    api.setOnClick(entryButton, events[k], nullptr);
    void* entry = game::transform_of(entryButton);
    rename(page, g.name);
    write_page(page, g);
    game::set_active(game::game_object_of(page), false);
    pageObjects.push_back(game::game_object_of(page));
    rename(entry, g.name);
    set_text(game::find_child(entry, "Text (TMP)"), g.name);
    api.setSiblingIndex(entry, alphabetical_index(list, g.name), nullptr);
    out.entries.push_back({entry, firstPage + k});

    GhostValue value{g.type, 0, il2cpp::string_new(g.name), {g.evidence[0], g.evidence[1], g.evidence[2]}, 0};
    // A journal woken after the entries joined the collection already lists them (its Awake adds every
    // collection entry), but has no button for them.
    if ((!lists_type(ghosts, g.type) && !call_add(ghosts, 1, &value)) || !call_add(ghostToButton, 2, &value, button)) return false;
  }
  il2cpp::Array* allButtons = appended(il2cpp::field<il2cpp::Array*>(journal, kJournalGhostButtons), api.ghostButton, buttons);
  il2cpp::Array* syncButtons = appended(il2cpp::field<il2cpp::Array*>(sync, kSyncButtons), api.ghostButton, buttons);
  il2cpp::Array* allPages = appended(pages, api.gameObject, pageObjects);
  if (!allButtons || !syncButtons || !allPages) return false;
  il2cpp::set_ref_field(journal, kJournalGhostButtons, allButtons);
  il2cpp::set_ref_field(sync, kSyncButtons, syncButtons);
  il2cpp::set_ref_field(journal, kJournalPages, allPages);
  // The Ghosts tab covers the new pages, which follow its last one.
  auto* tabs = il2cpp::field<il2cpp::Array*>(journal, kJournalTabs);
  for (uintptr_t i = 0; tabs && i < tabs->max_length; ++i) {
    void* tab = tabs->data<void*>()[i];
    if (tab && std::lround(il2cpp::field<float>(tab, kTabMax)) == firstPage - 1)
      il2cpp::set_field<float>(tab, kTabMax, static_cast<float>(firstPage - 1 + kGhostCount));
  }
  fit_grid(grid, gridBefore, gridBefore + kGhostCount);
  fit_grid(list, listBefore, listBefore + kGhostCount);
  api.setGhostTypes(journal, nullptr);  // cross out what the chosen evidence already rules out
  return true;
}

bool known(void* journal) {
  for (const Extended& e : g_journals)
    if (e.journal == journal) return true;
  return false;
}

/** Every Player's journal on this client. */
void for_each_journal(void (*visit)(void*)) {
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    void* journal = player && game::alive(player) ? il2cpp::field<void*>(player, kPlayerJournal) : nullptr;
    if (journal && game::alive(journal) && il2cpp::is_class(journal, "JournalController")) visit(journal);
  }
}

/** A click on a custom ghost's entry in an open journal turns to its page, as the game's entries do. */
void handle_clicks() {
  float x = 0, y = 0;
  if (game::cursor_locked() || !game::mouse_left_pressed() || !game::mouse_position(x, y)) return;
  for (const Extended& e : g_journals) {
    if (!game::alive(e.journal) || !il2cpp::field<bool>(e.journal, kJournalIsOpen)) continue;
    void* camera = e.canvas ? api.worldCamera(e.canvas, nullptr) : nullptr;
    if (!camera) camera = game::local_camera();
    for (const Entry& entry : e.entries) {
      if (!game::active_in_hierarchy(game::game_object_of(entry.rect)) || !ui::contains(entry.rect, {x, y}, camera)) continue;
      api.goToPage(e.journal, entry.page, nullptr);
      return;
    }
  }
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  constexpr const char* kUi = "UnityEngine.UI.dll";
  api.ghostButton = il2cpp::cls("Assembly-CSharp.dll", "", "GhostButton");
  api.gameObject = il2cpp::cls(kCore, "UnityEngine", "GameObject");
  il2cpp::Class* object = il2cpp::cls(kCore, "UnityEngine", "Object");
  il2cpp::Class* transform = il2cpp::cls(kCore, "UnityEngine", "Transform");
  il2cpp::Class* rect = il2cpp::cls(kCore, "UnityEngine", "RectTransform");
  il2cpp::Class* grid = il2cpp::cls(kUi, "UnityEngine.UI", "GridLayoutGroup");
  il2cpp::Class* button = il2cpp::cls(kUi, "UnityEngine.UI", "Button");
  il2cpp::Class* canvas = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
  il2cpp::Class* localiser = il2cpp::cls("Assembly-CSharp.dll", "", "TextLocaliserUI");
  api.textType = il2cpp::type_object(il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI"));
  api.localiserType = localiser ? il2cpp::type_object(localiser) : nullptr;
  api.gridType = grid ? il2cpp::type_object(grid) : nullptr;
  api.buttonType = button ? il2cpp::type_object(button) : nullptr;
  api.canvasType = canvas ? il2cpp::type_object(canvas) : nullptr;
  bool ok = api.ghostButton && api.gameObject && api.textType && api.gridType && api.buttonType && api.canvasType;
  ok &= bind(api.getName, object, "get_name", 0);
  ok &= bind(api.setName, object, "set_name", 1);
  ok &= bind(api.setSiblingIndex, transform, "SetSiblingIndex", 1);
  ok &= bind(api.getCellSize, grid, "get_cellSize", 0);
  ok &= bind(api.setCellSize, grid, "set_cellSize", 1);
  ok &= bind(api.getSpacing, grid, "get_spacing", 0);
  ok &= bind(api.getConstraint, grid, "get_constraint", 0);
  ok &= bind(api.setConstraint, grid, "set_constraint", 1);
  ok &= bind(api.getConstraintCount, grid, "get_constraintCount", 0);
  ok &= bind(api.setConstraintCount, grid, "set_constraintCount", 1);
  ok &= bind(api.getRect, rect, "get_rect", 0);
  ok &= bind(api.getAnchoredPosition, rect, "get_anchoredPosition", 0);
  ok &= bind(api.getOnClick, button, "get_onClick", 0);
  ok &= bind(api.setOnClick, button, "set_onClick", 1);
  ok &= bind(api.worldCamera, canvas, "get_worldCamera", 0);
  api.setGhostTypes = reinterpret_cast<decltype(api.setGhostTypes)>(game::game_assembly_address(kSetGhostTypesRva));
  api.goToPage = reinterpret_cast<decltype(api.goToPage)>(game::game_assembly_address(kGoToPageRva));
  api.ok = ok && api.setGhostTypes && api.goToPage;
  PG_LOG("pages: resolve %s", api.ok ? "ok" : "FAILED");
  return api.ok;
}

void tick(bool enabled) {
  // Journals are scene objects: forget those that went with their scene.
  g_journals.erase(std::remove_if(g_journals.begin(), g_journals.end(), [](const Extended& e) { return !game::alive(e.journal); }),
                   g_journals.end());
  if (!enabled || !api.ok) return;
  for_each_journal([](void* journal) {
    if (known(journal)) return;
    // Recorded before extending: whatever happens (even a managed exception part-way), the same
    // journal is never extended twice.
    g_journals.emplace_back();
    g_journals.back().journal = journal;
    Extended& e = g_journals.back();
    if (extend(journal, e)) PG_LOG("pages: custom ghosts added to a journal (%zu pages)", e.entries.size());
    else PG_LOG("pages: a journal's layout is not the expected one; the custom ghosts are missing from it");
  });
  handle_clicks();
}

void teardown() { g_journals.clear(); }

}  // namespace pg::custom::pages
