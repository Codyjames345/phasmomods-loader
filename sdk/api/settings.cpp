#include "settings.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <sstream>
#include <system_error>
#include <vector>

#include "Input.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "held.h"
#include "log.h"
#include "menu_rows.h"
#include "runtime.h"

namespace pm::api::settings {

namespace {

// How often the audio page is searched for, in ticks: a search goes through every loaded object.
constexpr int kSearchTicks = 60;

struct Row {
  Slider desc;
  float applied = 0.0f;  // the value that counts
  Held row;              // its row's GameObject on the page; its slider and number go with it
  Held slider;           // its XRSlider on the page
  Held number;           // its number text
  float shownValue = -1e9f;  // the value its number last showed
};

struct State {
  std::string owner, title;
  std::filesystem::path file;
  std::vector<Row> rows;
  std::vector<Held> extras;  // the heading
  rows::Lines lines;         // the list's lines: what space_out copied in and switched off
  std::string pageState;     // the page's own children and their switches, as it was when the rows were laid out
  std::vector<rows::Grown> grown;
  bool followsPage = false;
  bool percentSign = false;  // the game's master volume number ends in a percent sign
  int ticks = 0;
};
State g;

std::vector<std::pair<std::string, float>> read_saved() {
  std::vector<std::pair<std::string, float>> out;
  std::ifstream in(g.file, std::ios::binary);
  std::string line;
  while (std::getline(in, line)) {
    const size_t tab = line.find('\t');
    if (tab == std::string::npos || tab == 0) continue;
    char* end = nullptr;
    const std::string number = line.substr(tab + 1);
    const float v = std::strtof(number.c_str(), &end);
    if (end != number.c_str() && std::isfinite(v)) out.emplace_back(line.substr(0, tab), v);
  }
  return out;
}

void save() {
  std::ostringstream text;
  for (const Row& r : g.rows) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.4f", r.applied);
    text << r.desc.id << '\t' << buf << '\n';
  }
  std::error_code ec;
  std::filesystem::create_directories(g.file.parent_path(), ec);
  std::ofstream out(g.file, std::ios::binary | std::ios::trunc);
  if (!out) {
    PM_LOG("settings: cannot write %s", g.file.string().c_str());
    return;
  }
  out << text.str();
}

float clamped(const Slider& s, float v) { return std::clamp(v, s.min, std::max(s.min, s.max)); }

// The local player's audio page, and its master volume slider.
void* audio_page(void*& masterSlider) {
  masterSlider = nullptr;
  void* type = phasmo::GameMyAudioManager::type.type_object();
  const RefArray pages{type ? phasmo::Object::FindAllOfType(type, true) : nullptr};
  for (size_t i = 0; i < pages.size(); ++i) {
    void* slider = pages[i] ? phasmo::GameMyAudioManager::masterVolumeSlider.get(pages[i]) : nullptr;
    void* t = slider && alive(slider) ? phasmo::Component::get_transform(slider) : nullptr;
    void* row = t ? phasmo::Transform::get_parent(t) : nullptr;
    if (row && rows::journal_list(row)) {
      masterSlider = slider;
      return pages[i];
    }
  }
  return nullptr;
}

std::string shown(const Row& r, float v) {
  const float span = r.desc.max - r.desc.min;
  const int percent = static_cast<int>(std::lround(span > 0.0f ? (v - r.desc.min) / span * 100.0f : 0.0f));
  return std::to_string(percent) + (g.percentSign ? "%" : "");
}

// Whether `go` is one of ours on the page: the heading, a row, or a line space_out copied in.
bool ours(void* go) {
  const auto is = [&](const Held& h) { return h.get() == go; };
  return std::any_of(g.lines.made.begin(), g.lines.made.end(), is) || std::any_of(g.extras.begin(), g.extras.end(), is) ||
         std::any_of(g.rows.begin(), g.rows.end(), [&](const Row& r) { return r.row.get() == go; });
}

// The page's own shape: one character per child of the list that is the game's, whether the game has it switched on.
// Our own heading, rows and lines are left out, and the dividers we switched off count as the game left them, so this
// changes only when the game changes the page (a row it hides or shows). Counting ours made it differ every time, and the
// page was laid out again every search (the user's screenshot, 2026-11-10: three lines below Voice Input Mode).
std::string page_shape(void* list) {
  std::string out;
  const int32_t count = phasmo::Transform::get_childCount(list);
  for (int32_t i = 0; i < count; ++i) {
    void* child = phasmo::Transform::GetChild(list, i);
    void* go = child ? phasmo::Component::get_gameObject(child) : nullptr;
    if (!go || ours(go)) continue;
    const bool oursOff =
      std::any_of(g.lines.hidden.begin(), g.lines.hidden.end(), [&](const Held& h) { return h.get() == go; });
    out += (oursOff || phasmo::GameObject::get_activeSelf(go)) ? '1' : '0';
  }
  return out;
}

// Our rows and heading go. Object.Destroy takes them out only when the frame ends, so each is switched off first: the
// page is laid out again in this same frame, and a row still on there would get a line of its own that outlives it.
void remove_rows() {
  rows::lines_back(g.lines);
  for (Row& r : g.rows) {
    if (r.row && alive(r.row)) {
      phasmo::GameObject::SetActive(r.row, false);
      phasmo::Object::Destroy(r.row, 0.0f);
    }
    r.row = nullptr;
    r.slider = nullptr;
    r.number = nullptr;
  }
  for (Held& h : g.extras)
    if (h && alive(h)) {
      phasmo::GameObject::SetActive(h, false);
      phasmo::Object::Destroy(h, 0.0f);
    }
  g.extras.clear();
  for (const rows::Grown& grown : g.grown) rows::shrink(grown);
  g.grown.clear();
}

void ensure_rows() {
  if (++g.ticks < kSearchTicks) return;
  g.ticks = 0;
  void* masterSlider = nullptr;
  void* page = audio_page(masterSlider);
  if (!page) return;
  void* sliderT = phasmo::Component::get_transform(masterSlider);
  void* templ = phasmo::Transform::get_parent(sliderT);  // the master volume's row
  void* list = rows::journal_list(templ);
  void* number = phasmo::GameMyAudioManager::masterVolumeValueText.get(page);
  void* numberT = number && alive(number) ? phasmo::Component::get_transform(number) : nullptr;
  if (!list || !numberT || phasmo::Transform::get_parent(numberT).raw() != templ) {
    PM_LOG("settings: %s finds the audio page but not its master volume row as it expects", g.owner.c_str());
    return;
  }
  // The rows are already there and the page is the same shape as when they were laid out: nothing to do. The shape is
  // checked because the game switches its own rows on and off (the speaker mode row when there is one output device),
  // and the lines between rows have to follow.
  const std::string shape = page_shape(list);
  if (!shape.empty() && shape == g.pageState &&
      std::all_of(g.rows.begin(), g.rows.end(), [](const Row& r) { return r.slider && alive(r.slider); }))
    return;
  const std::string gameNumber = to_utf8(phasmo::TMP_Text::get_text(number));
  g.percentSign = !gameNumber.empty() && gameNumber.back() == '%';
  const int32_t sliderAt = phasmo::Transform::GetSiblingIndex(sliderT), numberAt = phasmo::Transform::GetSiblingIndex(numberT);
  void* templObject = phasmo::Component::get_gameObject(templ);
  remove_rows();
  // One line between the rows, as on the keybinds page (the user, 2026-11-10): the audio page's own dividers are double
  // rules between its groups, too heavy between the mod's rows.
  bool fromOther = false;
  void* divider = rows::line_divider(templ, &fromOther);
  PM_LOG("settings: %s divides its rows with %s", g.owner.c_str(),
         !divider ? "nothing: no line was found to copy"
                  : fromOther ? "a line taken from another journal menu"
                              : "the audio page's own divider");
  float added = 0.0f;
  const auto add = [&](void* original) -> void* {
    void* copy = original ? phasmo::Object::Instantiate(original, list, false) : nullptr;
    if (copy) added += rows::taken_height(copy, list);
    return copy;
  };
  if (!g.title.empty()) {
    if (void* h = add(templObject)) {
      g.extras.push_back(h);
      void* root = phasmo::GameObject::get_transform(h);
      void* name = rows::name_text(root);
      rows::heading(name, g.title);
      rows::show_only(root, name ? phasmo::Component::get_transform(name) : nullptr);
    }
  }
  for (Row& r : g.rows) {
    void* copy = add(templObject);
    void* root = copy ? phasmo::GameObject::get_transform(copy) : nullptr;
    void* sliderChild = root ? phasmo::Transform::GetChild(root, sliderAt) : nullptr;
    void* numberChild = root ? phasmo::Transform::GetChild(root, numberAt) : nullptr;
    void* slider = sliderChild ? phasmo::Component::GetComponent(sliderChild, phasmo::XRSlider::type.type_object()) : nullptr;
    void* text = numberChild ? phasmo::Component::GetComponent(numberChild, phasmo::TextMeshProUGUI::type.type_object()) : nullptr;
    // A change event of its own: the copy's would run the game's master volume listeners.
    void* fresh = phasmo::SliderEvent::type.make();
    if (fresh) phasmo::SliderEvent::ctor(fresh);
    if (!slider || !text || !fresh) {
      PM_LOG("settings: %s.%s gets no row: its copy of the master volume row is not as expected", g.owner.c_str(),
             r.desc.id.c_str());
      if (copy) phasmo::Object::Destroy(copy, 0.0f);
      continue;
    }
    phasmo::XRSlider::m_OnValueChanged.set(slider, fresh);
    rows::label(rows::name_text(root), r.desc.label);
    phasmo::XRSlider::set_minValue(slider, r.desc.min);
    phasmo::XRSlider::set_maxValue(slider, r.desc.max);
    phasmo::XRSlider::SetValueWithoutNotify(slider, r.applied);
    phasmo::TMP_Text::set_text(text, make_string(shown(r, r.applied)));
    r.shownValue = r.applied;
    r.row = copy;
    r.slider = slider;
    r.number = text;
    PM_LOG("settings: %s.%s has its row on the audio page", g.owner.c_str(), r.desc.id.c_str());
  }
  // One line between every pair of rows the page shows (the user, 2026-11-10): the page's own dividers double up where it
  // hides a row between two of them, and it groups two rows with no divider between them at all. This lays out the whole
  // list, the game's rows as well as ours, so the page reads the same top to bottom.
  g.lines = rows::space_out(list, divider);
  added += g.lines.added;
  g.pageState = shape;
  PM_LOG("settings: %s lines the audio page with %zu copied and %zu put away", g.owner.c_str(), g.lines.made.size(),
         g.lines.hidden.size());
  if (rows::Grown grown = rows::grow(list, added); grown.rect) g.grown.push_back(std::move(grown));
}

// The audio page decides when a setting counts, as it does for the game's master volume (sdk/map/Input.h,
// GameMyAudioManager): Apply keeps the sliders' values, and leaving the page puts back what was not applied.
void apply_now() {
  bool changed = false;
  for (Row& r : g.rows)
    if (r.slider && alive(r.slider)) {
      const float v = clamped(r.desc, phasmo::XRSlider::get_value(r.slider));
      changed |= v != r.applied;
      r.applied = v;
    }
  if (changed) save();
}
void detour_apply(Original<phasmo::GameMyAudioManager::ApplyButton> original, void* page) {
  original(page);
  apply_now();
}
void detour_closed(Original<phasmo::GameMyAudioManager::OnDisable> original, void* page) {
  original(page);
  for (Row& r : g.rows)
    if (r.slider && alive(r.slider)) phasmo::XRSlider::SetValueWithoutNotify(r.slider, r.applied);
}

}  // namespace

void begin_settings(std::string_view owner, std::string_view title, std::filesystem::path file) {
  clear_settings();
  g.owner = owner;
  g.title = title;
  g.file = std::move(file);
  g.ticks = kSearchTicks - 1;  // look for the page at once
  g.followsPage = hook<phasmo::GameMyAudioManager::ApplyButton>(&detour_apply, "settings: the audio page's Apply") &&
                  hook<phasmo::GameMyAudioManager::OnDisable>(&detour_closed, "settings: the audio page closing");
  if (!g.followsPage)
    PM_LOG("settings: %s cannot follow the audio page's Apply in this game build: a change counts at once",
           g.owner.c_str());
}

bool add_slider(const Slider& slider) {
  if (slider.id.empty() || !(slider.max > slider.min)) return false;
  if (std::any_of(g.rows.begin(), g.rows.end(), [&](const Row& r) { return r.desc.id == slider.id; })) return false;
  Row r;
  r.desc = slider;
  r.applied = clamped(slider, slider.initial);
  for (const auto& [id, v] : read_saved())
    if (id == slider.id) r.applied = clamped(slider, v);
  g.rows.push_back(std::move(r));
  return true;
}

void tick_settings() {
  if (g.rows.empty()) return;
  ensure_rows();
  for (Row& r : g.rows)
    if (r.slider && alive(r.slider) && r.number && alive(r.number)) {
      const float v = phasmo::XRSlider::get_value(r.slider);
      if (v == r.shownValue) continue;
      r.shownValue = v;
      phasmo::TMP_Text::set_text(r.number, make_string(shown(r, v)));
    }
  if (!g.followsPage) apply_now();
}

float value(std::string_view id) {
  for (const Row& r : g.rows)
    if (r.desc.id == id) return r.applied;
  return 0.0f;
}

void clear_settings() {
  remove_rows();
  g = State{};
}

}  // namespace pm::api::settings
