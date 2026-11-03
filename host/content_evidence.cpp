#include "content_evidence.h"

#include <algorithm>
#include <set>

#include "JournalController.h"
#include "LocalisationSystem.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "content_text.h"
#include "content_widgets.h"
#include "game_api.h"
#include "log.h"
#include "runtime.h"

namespace pm::host {
namespace {

using widgets::active_children;
using widgets::appended;
using widgets::game_object_of;
using widgets::key_of;
using widgets::localiser_of;
using widgets::set_key;
using widgets::transform_of;

const pm::sdk::EntryDesc* const kNeeded[] = {
    &pm::map::JournalController_evidenceButtons, &pm::map::JournalController_evidenceSync,
    &pm::map::JournalController_SetGhostTypes, &pm::map::JournalController_RemoveEvidence, &pm::map::EvidenceButton,
    &pm::map::EvidenceButton_evidence,
    &pm::map::EvidenceButton_descriptionLabel, &pm::map::ThreeStateButton_textLabel,
    &pm::map::NetworkThreeStateButton_userStateChanged, &pm::map::HorizontalOrVerticalLayoutGroup,
    &pm::map::HorizontalOrVerticalLayoutGroup_get_spacing, &pm::map::HorizontalOrVerticalLayoutGroup_set_spacing,
    &pm::map::RectTransform_get_rect, &pm::map::TextLocaliserUI, &pm::map::TextLocaliserUI_key,
    &pm::map::TextLocaliserUI_set_Key, &pm::map::LocalisationSystem_TryGetLocalisedValue,
    &pm::map::LocalisationSystem_get_language, &pm::map::LocalisationSystem_get_voiceLanguage,
    &pm::map::LocalisationSystem_GetLanguageAbbreviationText, &pm::map::UnityObject_set_name,
    &pm::map::UnityObject_Instantiate, &pm::map::UnityObject_Destroy, &pm::map::GameObject_SetActive,
    &pm::map::GameObject_get_activeSelf, &pm::map::Component_get_transform, &pm::map::Component_get_gameObject,
    &pm::map::Component_GetComponent, &pm::map::Transform_get_childCount, &pm::map::Transform_GetChild,
    &pm::map::Scene_JournalEvidenceRows, &pm::map::Scene_JournalEvidenceButton,
};

}  // namespace

std::string ContentEvidence::missing() const {
  std::string out;
  const PmHostApi* host = pm::api::detail::host();
  for (const pm::sdk::EntryDesc* d : kNeeded)
    if (!host || !host->game_entry(d->key)) out += (out.empty() ? "" : ", ") + std::string(d->key);
  return out;
}

void ContentEvidence::tick(const std::vector<void*>& journals) {
  std::erase_if(journals_, [](const Journal& j) { return !pm::api::alive(j.journal.get()); });
  for (void* journal : journals) {
    auto it = std::find_if(journals_.begin(), journals_.end(),
                           [&](const Journal& j) { return j.journal.get() == journal; });
    if (it == journals_.end()) it = journals_.insert(journals_.end(), Journal{pm::api::Held(journal)});
    // Each journal is read once: the first whose rows show the form gives it to every journal.
    if (!it->learned) {
      it->learned = true;
      if (!form_) learn(journal);
    }
  }
  // The custom evidence the custom ghosts shown now list, by the numbers their entries were given.
  held_.clear();
  for (const auto& [type, h] : types_.shown()) {
    const content::Ghost* g = registry_.registered(h.id);
    for (size_t i = 0; g && i < 3; ++i)
      if (const content::Evidence* e = registry_.registered_evidence(g->evidence[i]))
        held_[h.evidence[i]] = {g->evidence[i], e};
  }
  drop_colliding();
  // What the journals show changed: a journal that got rows under the last decision is put back first.
  const unsigned shown = types_.shown_decisions();
  for (Journal& j : journals_)
    if (j.extension && j.extension->under != shown) restore(j);
  // A journal met before any showed the form waits for it: it is tried only once it can get its rows.
  if (held_.empty() || !form_) return;
  for (Journal& j : journals_) {
    if (j.tried == shown || std::find(journals.begin(), journals.end(), j.journal.get()) == journals.end()) continue;
    j.tried = shown;
    if (extend(j, shown)) PM_LOG("content: custom evidence added to a journal (%zu rows)", held_.size());
    else PM_LOG("content: custom evidence is missing from a journal, which stays as the game made it");
  }
}
void ContentEvidence::drop_colliding() {
  // Two held evidence whose keys meet (one's line key is another's name key: "a.x" and "a.x_Tooltip")
  // could not tell their texts apart: neither gets a row or a page line, logged once each.
  if (!form_) return;
  std::map<std::string, int> uses;
  for (const auto& [value, e] : held_) {
    ++uses[namePrefix_ + e.id];
    ++uses[line_key(e.id)];
  }
  std::erase_if(held_, [&](const auto& entry) {
    const std::string& id = entry.second.id;
    if (uses[namePrefix_ + id] == 1 && uses[line_key(id)] == 1) return false;
    if (collidedLogged_.insert(id).second)
      PM_LOG("content: the custom evidence \"%s\" has a journal key another custom evidence has; it gets no row",
             id.c_str());
    return true;
  });
}

void ContentEvidence::withdraw() {
  for (Journal& j : journals_)
    if (j.extension && pm::api::alive(j.journal.get())) restore(j);
  journals_.clear();
  held_.clear();
}

std::string ContentEvidence::line_key(const std::string& id) const { return namePrefix_ + id + lineSuffix_; }

std::optional<std::string> ContentEvidence::name_key(int32_t value) const {
  const auto it = held_.find(value);
  if (!form_ || it == held_.end()) return std::nullopt;
  return namePrefix_ + it->second.id;
}

std::optional<std::string> ContentEvidence::text_for_key(std::string_view key, std::string_view lang) const {
  if (!form_) return std::nullopt;
  for (const auto& [value, e] : held_) {
    const content::Text* text = key == namePrefix_ + e.id ? &e.def->name
                                : key == line_key(e.id)   ? &e.def->journalLine
                                                          : nullptr;
    if (!text) continue;
    const std::string& words = pick(*text, lang);
    if (words.empty()) return std::nullopt;
    return words;
  }
  return std::nullopt;
}

bool ContentEvidence::learn(void* journal) {
  // Each game row's name key and its line's key: the line's is the name's with a suffix, the same for
  // every row, and the custom keys begin as every name key does (the PCPlayer prefab's: "Journal2_EMF5"
  // and "Journal2_EMF5_Tooltip", sdk/systems/evidence.md).
  const pm::api::RefArray buttons{phasmo::JournalController::evidenceButtons.get(journal)};
  std::vector<std::string> names;
  std::set<std::string> suffixes;
  for (size_t i = 0; i < buttons.size(); ++i) {
    void* b = buttons[i];
    if (!b) continue;
    const std::string name = key_of(localiser_of(phasmo::ThreeStateButton::textLabel.get(b)));
    const std::string line = key_of(localiser_of(phasmo::EvidenceButton::descriptionLabel.get(b)));
    const bool added = !name.empty() && line.size() > name.size() && line.starts_with(name);
    suffixes.insert(added ? line.substr(name.size()) : std::string());
    names.push_back(name);
  }
  std::string prefix = names.empty() ? std::string() : names.front();
  for (const std::string& n : names)
    prefix.resize(static_cast<size_t>(std::mismatch(prefix.begin(), prefix.end(), n.begin(), n.end()).first -
                                      prefix.begin()));
  if (names.size() < 2 || prefix.empty() || suffixes.size() != 1 || suffixes.begin()->empty()) {
    if (!formLogged_) PM_LOG("content: the game's evidence rows show no one key form; custom evidence gets no row");
    formLogged_ = true;
    return false;
  }
  namePrefix_ = prefix;
  lineSuffix_ = *suffixes.begin();
  form_ = true;
  return true;
}

bool ContentEvidence::extend(Journal& j, unsigned under) {
  void* journal = j.journal.get();
  void* root = transform_of(journal);
  void* rows = phasmo::Journal::EvidenceRows.find(root);
  void* templateRow = phasmo::Journal::EvidenceButton.find(root);
  void* templateButton =
      templateRow ? phasmo::Component::GetComponent(templateRow, phasmo::EvidenceButton::type.type_object()) : nullptr;
  void* layout = rows ? phasmo::Component::GetComponent(rows, phasmo::HorizontalOrVerticalLayoutGroup::type.type_object())
                      : nullptr;
  void* buttons = phasmo::JournalController::evidenceButtons.get(journal);
  void* sync = phasmo::JournalController::evidenceSync.get(journal);
  if (!rows || !templateButton || !layout || !buttons || !sync) return false;
  // Every operation the change needs is found before anything changes.
  if (!ops_.initialise || !ops_.has_initialise || !ops_.has_initialise(sync)) {
    PM_LOG("content: the journal's evidence sync lacks the method the custom evidence needs");
    return false;
  }
  std::set<int32_t> there;  // the rows' numbers: none is added twice
  const pm::api::RefArray old{buttons};
  for (size_t i = 0; i < old.size(); ++i)
    if (old[i]) there.insert(phasmo::EvidenceButton::evidence.get(old[i]));
  const int32_t before = static_cast<int32_t>(active_children(rows).size());
  std::vector<void*> added;
  std::vector<pm::api::Held> made;  // every copy, held until the array holds it or it is taken out
  // Takes the copies out again: off at once, destroyed at the frame's end.
  const auto take_out = [&] {
    for (const pm::api::Held& object : made) {
      void* go = game_object_of(object.get());
      phasmo::GameObject::SetActive(go, false);
      phasmo::Object::Destroy(go, 0.0f);
    }
  };
  for (const auto& [value, e] : held_) {
    if (there.count(value)) continue;
    // The Spirit Box's row, copied with its children (the other players' marks included), after the
    // game's rows; its name and line are its own copies, each with the game's localiser.
    void* row = phasmo::Object::Instantiate(templateButton, rows, false);
    if (!row) return take_out(), false;
    made.emplace_back(row);
    phasmo::Object::set_name(row, pm::api::make_string(pick(e.def->name, "en")));
    phasmo::EvidenceButton::evidence.set(row, value);
    set_key(localiser_of(phasmo::ThreeStateButton::textLabel.get(row)), namePrefix_ + e.id);
    set_key(localiser_of(phasmo::EvidenceButton::descriptionLabel.get(row)), line_key(e.id));
    added.push_back(row);
  }
  if (added.empty()) return true;
  void* all = appended(buttons, phasmo::EvidenceButton::type, added);
  if (!all) return take_out(), false;
  const pm::api::Held keep(all);

  // From here the journal changes; a failure puts it back as it was.
  std::vector<pm::api::Held> delegates;  // each game row's subscriptions, as Initialise made them
  for (size_t i = 0; i < old.size(); ++i)
    delegates.emplace_back(old[i] ? phasmo::NetworkThreeStateButton::userStateChanged.get(old[i]) : nullptr);
  const auto restore_delegates = [&] {
    for (size_t i = 0; i < old.size(); ++i)
      if (old[i]) phasmo::NetworkThreeStateButton::userStateChanged.set(old[i], delegates[i].get());
  };
  phasmo::JournalController::evidenceButtons.set(journal, all);
  // The evidence sync reads the rows again and subscribes to each; the game's rows keep the one
  // subscription they had (sdk/systems/journal.md, as for the guess sync).
  if (!ops_.initialise(sync)) {
    restore_delegates();
    phasmo::JournalController::evidenceButtons.set(journal, buttons);
    take_out();
    const bool reread = ops_.initialise(sync);  // reads the game's own rows again
    restore_delegates();
    PM_LOG("content: the journal's evidence sync could not read the custom evidence's rows; the journal is put "
           "back, and its evidence sync %s",
           reread ? "read its own rows again" : "could not read its own rows again");
    return false;
  }
  restore_delegates();
  // The column's spacing shrinks so every row fits in the height the game's filled: with row height h
  // and spacing s, (r - 1) * s + r * h = (r + n - 1) * s' + (r + n) * h.
  const int32_t n = static_cast<int32_t>(added.size());
  const float s = phasmo::HorizontalOrVerticalLayoutGroup::get_spacing(layout);
  if (before > 0) {
    const float h = phasmo::RectTransform::get_rect(transform_of(templateButton)).height;
    phasmo::HorizontalOrVerticalLayoutGroup::set_spacing(layout,
                                                         std::max(0.0f, ((before - 1) * s - n * h) / (before + n - 1)));
  }
  phasmo::JournalController::SetGhostTypes(journal);  // cross out what the chosen evidence already rules out
  std::vector<int32_t> values;
  for (void* row : added) values.push_back(phasmo::EvidenceButton::evidence.get(row));
  j.extension = Extension{under, pm::api::Held(buttons), pm::api::Held(layout), s, std::move(made), std::move(values)};
  return true;
}

void ContentEvidence::restore(Journal& j) {
  void* journal = j.journal.get();
  Extension& e = *j.extension;
  // The game's rows as they were, each keeping the one subscription it has; the evidence sync reads
  // them again.
  const pm::api::RefArray old{e.buttons.get()};
  std::vector<pm::api::Held> delegates;
  for (size_t i = 0; i < old.size(); ++i)
    delegates.emplace_back(old[i] ? phasmo::NetworkThreeStateButton::userStateChanged.get(old[i]) : nullptr);
  phasmo::JournalController::evidenceButtons.set(journal, e.buttons.get());
  void* sync = phasmo::JournalController::evidenceSync.get(journal);
  const bool reread = sync && ops_.initialise && ops_.initialise(sync);
  for (size_t i = 0; i < old.size(); ++i)
    if (old[i]) phasmo::NetworkThreeStateButton::userStateChanged.set(old[i], delegates[i].get());
  // The copies: off at once, destroyed at the frame's end; the column's spacing as it was.
  for (const pm::api::Held& object : e.made) {
    void* go = game_object_of(object.get());
    phasmo::GameObject::SetActive(go, false);
    phasmo::Object::Destroy(go, 0.0f);
  }
  if (pm::api::alive(e.layout.get())) phasmo::HorizontalOrVerticalLayoutGroup::set_spacing(e.layout.get(), e.spacing);
  // A mark on a custom row (found: AddEvidence, ruled out: FilterOutEvidence) stays in the journal's
  // lists by number, crossing out the game's ghosts with no row left to clear it: the game's own
  // RemoveEvidence takes each off both lists (a number on neither changes nothing) and runs the filter.
  for (const int32_t value : e.values) phasmo::JournalController::RemoveEvidence(journal, value);
  phasmo::JournalController::SetGhostTypes(journal);
  if (reread) PM_LOG("content: a journal's custom evidence rows were taken out; it is as the game made it");
  else
    PM_LOG("content: a journal's custom evidence rows were taken out, but its evidence sync could not read its own "
           "rows again");
  j.extension.reset();
}

}  // namespace pm::host
