#include "journal.h"

#include <algorithm>
#include <vector>

#include "flame_rules.h"
#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "players.h"
#include "ui.h"

namespace pg::flame::journal {
namespace {

using il2cpp::bind;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset. The evidence page's
// layout is from the journal prefab (resources.assets, read with UnityPy, 2026-10-20).
constexpr size_t kPlayerJournal = 0xC8;        // Player: JournalController
constexpr size_t kJournalEvidenceButtons = 0x1A8;  // JournalController: EvidenceButton[]
constexpr size_t kJournalEvidenceSync = 0x1B0;     // JournalController: JournalEvidenceSyncController
constexpr size_t kSyncButtons = 0x30;          // JournalTypedButtonSyncController: its buttons (as the ghost one)
constexpr size_t kButtonEvidence = 0x78;       // EvidenceButton: its evidence type
constexpr size_t kButtonUserChanged = 0x58;    // NetworkThreeStateButton: Action raised on the user's choice
constexpr size_t kButtonDescription = 0x90;    // EvidenceButton: the line under the name (the game's "Journal2_*_Tooltip")
constexpr uintptr_t kSetGhostTypesRva = 0x22C5090;  // JournalController.SetGhostTypes(): the evidence filter
constexpr const char* kRows = "Content/GhostHuntingBookContent/Evidence.1/Layout Group";
constexpr const char* kTemplate = "Spirit Box";
constexpr const char* kLabel = "Text (TMP)";
constexpr const char* kName = "Spectral Flame";
constexpr const char* kDescription = "Watch a placed candle or lantern burn blue from base to tip";

struct Api {
  il2cpp::Class* evidenceButton = nullptr;
  il2cpp::Object* textType = nullptr;
  il2cpp::Object* localiserType = nullptr;
  il2cpp::Object* layoutType = nullptr;
  void (*setName)(void*, il2cpp::String*, const void*) = nullptr;
  float (*spacing)(void*, const void*) = nullptr;
  void (*setSpacing)(void*, float, const void*) = nullptr;
  ui::Rect (*rect)(void*, const void*) = nullptr;
  void (*setGhostTypes)(void*, const void*) = nullptr;
  bool ok = false;
} api;

std::vector<il2cpp::Held> g_journals;  // extended (or found unusable), never twice; held across frames

/** Set a text object's words for good: its localiser would put the game's own back. */
void set_text(void* textTransform, const char* text) {
  if (!textTransform) return;
  if (api.localiserType)
    if (void* localiser = game::get_component(textTransform, api.localiserType)) game::set_enabled(localiser, false);
  if (void* tmp = game::get_component(textTransform, api.textType)) ui::set_text(tmp, text);
}

/** A copy of `array` with `extra` appended; null when it cannot be made. */
il2cpp::Array* appended(il2cpp::Array* array, void* extra) {
  const uintptr_t n = array ? array->max_length : 0;
  il2cpp::Array* out = il2cpp::array_new(api.evidenceButton, n + 1);
  if (!out) return nullptr;
  for (uintptr_t i = 0; i < n; ++i) out->data<void*>()[i] = array->data<void*>()[i];
  out->data<void*>()[n] = extra;
  return out;
}

bool holds_evidence_buttons(il2cpp::Array* array) {
  return array && array->max_length > 0 && il2cpp::is_class(array->data<void*>()[0], "EvidenceButton");
}

/**
 * Close up a vertical layout's rows so `rows` + 1 span what `rows` did: with row height h and
 * spacing s, (rows - 1) * s + rows * h = rows * s' + (rows + 1) * h.
 */
void fit_rows(void* rowsTransform, void* rowTransform, int rows) {
  void* layout = game::get_component(rowsTransform, api.layoutType);
  if (!layout || rows < 1) return;
  const float h = api.rect(rowTransform, nullptr).height, s = api.spacing(layout, nullptr);
  api.setSpacing(layout, std::max(0.0f, ((rows - 1) * s - h) / rows), nullptr);
}

/** Add the Spectral Flame row to one journal. False when its layout is not the one this build expects. */
bool extend(void* journal) {
  void* rows = game::find_child(game::transform_of(journal), kRows);
  void* templateRow = rows ? game::find_child(rows, kTemplate) : nullptr;
  void* templateButton = templateRow ? game::get_component(templateRow, il2cpp::type_object(api.evidenceButton)) : nullptr;
  auto* buttons = il2cpp::field<il2cpp::Array*>(journal, kJournalEvidenceButtons);
  void* sync = il2cpp::field<void*>(journal, kJournalEvidenceSync);
  auto* syncButtons = sync ? il2cpp::field<il2cpp::Array*>(sync, kSyncButtons) : nullptr;
  if (!templateButton || !holds_evidence_buttons(buttons) || !holds_evidence_buttons(syncButtons)) return false;
  for (uintptr_t i = 0; i < buttons->max_length; ++i)
    if (void* b = buttons->data<void*>()[i]; b && il2cpp::field<int>(b, kButtonEvidence) == kEvidence) return true;

  const int before = game::child_count(rows);
  void* button = game::instantiate(templateButton, rows, false);  // the row with its children (marks included)
  if (!button) return false;
  api.setName(game::game_object_of(button), il2cpp::string_new(kName), nullptr);
  il2cpp::set_field<int>(button, kButtonEvidence, kEvidence);
  il2cpp::set_ref_field(button, kButtonUserChanged, il2cpp::field<void*>(templateButton, kButtonUserChanged));
  set_text(game::find_child(game::transform_of(button), kLabel), kName);
  if (void* description = il2cpp::field<void*>(button, kButtonDescription); description && game::alive(description))
    set_text(game::transform_of(description), kDescription);
  il2cpp::Array* allButtons = appended(buttons, button);
  il2cpp::Array* allSync = appended(syncButtons, button);
  if (!allButtons || !allSync) return false;
  il2cpp::set_ref_field(journal, kJournalEvidenceButtons, allButtons);
  il2cpp::set_ref_field(sync, kSyncButtons, allSync);
  fit_rows(rows, game::transform_of(templateButton), before);
  api.setGhostTypes(journal, nullptr);  // cross out what the chosen evidence already rules out
  return true;
}

bool known(void* journal) {
  for (const il2cpp::Held& j : g_journals)
    if (j.get() == journal) return true;
  return false;
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  il2cpp::Class* object = il2cpp::cls(kCore, "UnityEngine", "Object");
  il2cpp::Class* rect = il2cpp::cls(kCore, "UnityEngine", "RectTransform");
  il2cpp::Class* layout = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "HorizontalOrVerticalLayoutGroup");
  il2cpp::Class* localiser = il2cpp::cls("Assembly-CSharp.dll", "", "TextLocaliserUI");
  api.evidenceButton = il2cpp::cls("Assembly-CSharp.dll", "", "EvidenceButton");
  api.textType = il2cpp::type_object(il2cpp::cls("Unity.TextMeshPro.dll", "TMPro", "TextMeshProUGUI"));
  api.localiserType = localiser ? il2cpp::type_object(localiser) : nullptr;
  api.layoutType = layout ? il2cpp::type_object(layout) : nullptr;
  bool ok = api.evidenceButton && api.textType && api.layoutType;
  ok &= bind(api.setName, object, "set_name", 1);
  ok &= bind(api.spacing, layout, "get_spacing", 0);
  ok &= bind(api.setSpacing, layout, "set_spacing", 1);
  ok &= bind(api.rect, rect, "get_rect", 0);
  api.setGhostTypes = reinterpret_cast<decltype(api.setGhostTypes)>(game::game_assembly_address(kSetGhostTypesRva));
  api.ok = ok && api.setGhostTypes;
  PG_LOG("journal: resolve %s", api.ok ? "ok" : "FAILED");
  return api.ok;
}

void tick(bool enabled) {
  // Journals are scene objects: forget those that went with their scene.
  g_journals.erase(std::remove_if(g_journals.begin(), g_journals.end(), [](const il2cpp::Held& j) { return !game::alive(j.get()); }),
                   g_journals.end());
  if (!enabled || !api.ok) return;
  game::each_player([](void* player) {
    void* journal = il2cpp::field<void*>(player, kPlayerJournal);
    if (!journal || !game::alive(journal) || !il2cpp::is_class(journal, "JournalController") || known(journal)) return;
    // Recorded before extending: whatever happens (even a managed exception part-way), the same
    // journal is never extended twice.
    g_journals.emplace_back(journal);
    if (extend(journal)) PG_LOG("journal: Spectral Flame added to a journal's evidence");
    else PG_LOG("journal: a journal's evidence page is not the expected one; Spectral Flame is missing from it");
  });
}

void teardown() { g_journals.clear(); }

}  // namespace pg::flame::journal
