#include "menu_rows.h"

#include "Input.h"
#include "JournalController.h"
#include "Player.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "runtime.h"

namespace pm::api::rows {

namespace {

void* transform_of(void* row) {
  if (!row) return nullptr;
  return phasmo::GameObject::type.is(row) ? phasmo::GameObject::get_transform(row) : phasmo::Component::get_transform(row);
}

bool has_name(void* transform) {
  void* type = phasmo::TextLocaliserUI::type.type_object();
  return type && phasmo::Component::GetComponentInChildren(transform, type, true) != nullptr;
}

}  // namespace

void* journal_list(void* row) {
  void* t = transform_of(row);
  void* list = t ? phasmo::Transform::get_parent(t) : nullptr;
  void* groupType = phasmo::HorizontalOrVerticalLayoutGroup::type.type_object();
  if (!list || !groupType || !phasmo::Component::GetComponent(list, groupType)) return nullptr;
  void* net = phasmo::Network::get_Instance();
  void* player = net ? phasmo::Network::localPlayer.get(net) : nullptr;
  void* playerTransform = player && alive(player) ? phasmo::Component::get_transform(player) : nullptr;
  if (playerTransform && !phasmo::Transform::IsChildOf(t, playerTransform)) return nullptr;
  return list;
}

void* divider_beside(void* row) {
  void* t = transform_of(row);
  void* list = t ? phasmo::Transform::get_parent(t) : nullptr;
  if (!list) return nullptr;
  const int32_t at = phasmo::Transform::GetSiblingIndex(t), count = phasmo::Transform::get_childCount(list);
  for (const int32_t i : {at + 1, at - 1}) {
    if (i < 0 || i >= count) continue;
    void* sibling = phasmo::Transform::GetChild(list, i);
    if (sibling && !has_name(sibling)) return phasmo::Component::get_gameObject(sibling);
  }
  return nullptr;
}

void* line_divider(void* row, bool* fromOtherPage) {
  void* t = transform_of(row);
  void* own = t ? phasmo::Transform::get_parent(t) : nullptr;
  // Another menu's line: the keybinds page's rows are `RebindActionUI`s, and the game puts a thin line between them.
  void* type = phasmo::RebindActionUI::type.type_object();
  const RefArray found{type ? phasmo::Object::FindAllOfType(type, true) : nullptr};
  for (size_t i = 0; i < found.size(); ++i) {
    void* list = found[i] ? journal_list(found[i]) : nullptr;
    if (!list || list == own) continue;
    if (void* line = divider_beside(found[i])) {
      if (fromOtherPage) *fromOtherPage = true;
      return line;
    }
  }
  if (fromOtherPage) *fromOtherPage = false;
  return divider_beside(row);
}

Lines space_out(void* list, void* line) {
  Lines out;
  if (!list) return out;
  // What the layout sees, in its own order. A switched-off child takes no room, so it divides nothing and needs no line.
  std::vector<std::pair<void*, bool>> seq;  // each child, and whether it is a row rather than a divider
  const int32_t count = phasmo::Transform::get_childCount(list);
  for (int32_t i = 0; i < count; ++i) {
    void* child = phasmo::Transform::GetChild(list, i);
    if (!child) continue;
    void* go = phasmo::Component::get_gameObject(child);
    if (!phasmo::GameObject::get_activeSelf(go)) continue;
    seq.emplace_back(go, has_name(child) || phasmo::Transform::get_childCount(child) > 0);
  }
  // The list's own dividers are put away and `line` goes between every pair of rows instead: the audio page's dividers
  // draw as double rules, one of them doubled again where a row it hides sits between two (the user, 2026-11-10).
  // With no line to copy, a run of dividers between two rows keeps its first, so the rows stay divided. One at either
  // end of the list has no pair of rows to divide.
  for (size_t i = 0; i < seq.size();) {
    if (seq[i].second) {
      ++i;
      continue;
    }
    size_t j = i;
    while (j < seq.size() && !seq[j].second) ++j;
    const bool between = i > 0 && seq[i - 1].second && j < seq.size() && seq[j].second;
    for (size_t k = i + (between && !line ? 1 : 0); k < j; ++k) {
      out.added -= taken_height(seq[k].first, list);
      phasmo::GameObject::SetActive(seq[k].first, false);
      out.hidden.emplace_back(seq[k].first);
    }
    i = j;
  }
  if (!line) return out;
  // A line before every row but the first. The copies go in last first, so the places still hold.
  std::vector<void*> before;
  bool first = true;
  for (const auto& [go, row] : seq) {
    if (!row) continue;
    if (!first) before.push_back(go);
    first = false;
  }
  for (auto it = before.rbegin(); it != before.rend(); ++it) {
    void* copy = phasmo::Object::Instantiate(line, list, false);
    if (!copy) continue;
    phasmo::GameObject::SetActive(copy, true);  // `line` may be one of the dividers just put away
    void* row = *it;  // a GameObject, as seq holds them
    phasmo::Transform::SetSiblingIndex(phasmo::GameObject::get_transform(copy),
                                       phasmo::Transform::GetSiblingIndex(phasmo::GameObject::get_transform(row)));
    out.added += taken_height(copy, list);
    out.made.emplace_back(copy);
  }
  return out;
}

void lines_back(Lines& lines) {
  for (Held& h : lines.made)
    if (h && alive(h)) phasmo::Object::Destroy(h, 0.0f);
  lines.made.clear();
  for (Held& h : lines.hidden)
    if (h && alive(h)) phasmo::GameObject::SetActive(h.get(), true);
  lines.hidden.clear();
}

void* name_text(void* row) {
  void* t = transform_of(row);
  void* type = phasmo::TextLocaliserUI::type.type_object();
  void* localiser = t && type ? phasmo::Component::GetComponentInChildren(t, type, true) : nullptr;
  return localiser ? phasmo::Component::GetComponent(localiser, phasmo::TextMeshProUGUI::type.type_object()) : nullptr;
}

void label(void* text, const std::string& utf8) {
  if (!text) return;
  if (void* localiser = phasmo::Component::GetComponent(text, phasmo::TextLocaliserUI::type.type_object()))
    phasmo::Object::DestroyImmediate(localiser, false);
  phasmo::TMP_Text::set_text(text, make_string(utf8));
}

float heading(void* text, const std::string& title) {
  if (!text) return 0.0f;
  label(text, "<b>" + title + "</b>");
  const float now = phasmo::TMP_Text::get_fontSize(text), most = phasmo::TMP_Text::get_fontSizeMax(text);
  const float base = most > 0.0f && (now <= 0.0f || most < now) ? most : now;
  if (base <= 0.0f) return 0.0f;
  phasmo::TMP_Text::set_enableAutoSizing(text, false);
  phasmo::TMP_Text::set_fontSize(text, base * kHeadingScale);
  return base * kHeadingScale;
}

void show_only(void* root, void* keep) {
  const int32_t children = root ? phasmo::Transform::get_childCount(root) : 0;
  for (int32_t i = 0; i < children; ++i) {
    void* child = phasmo::Transform::GetChild(root, i);
    if (child && !(keep && phasmo::Transform::IsChildOf(keep, child)))
      phasmo::GameObject::SetActive(phasmo::Component::get_gameObject(child), false);
  }
}

float taken_height(void* object, void* list) {
  void* t = object ? phasmo::GameObject::get_transform(object) : nullptr;
  if (!t) return 0.0f;
  void* group = list ? phasmo::Component::GetComponent(list, phasmo::HorizontalOrVerticalLayoutGroup::type.type_object())
                     : nullptr;
  const float spacing = group ? phasmo::HorizontalOrVerticalLayoutGroup::get_spacing(group) : 0.0f;
  return phasmo::RectTransform::get_rect(t).height + spacing;
}

Grown grow(void* list, float by) {
  if (!list || by <= 0.0f) return {};
  const phasmo::Vector2 lo = phasmo::RectTransform::get_anchorMin(list), hi = phasmo::RectTransform::get_anchorMax(list);
  void* target = lo.y != hi.y ? phasmo::Transform::get_parent(list) : list;
  if (!target) return {};
  phasmo::Vector2 size = phasmo::RectTransform::get_sizeDelta(target);
  size.y += by;
  phasmo::RectTransform::set_sizeDelta(target, size);
  return {target, by};
}

void shrink(const Grown& grown) {
  if (!grown.rect || !alive(grown.rect)) return;
  phasmo::Vector2 size = phasmo::RectTransform::get_sizeDelta(grown.rect);
  size.y -= grown.by;
  phasmo::RectTransform::set_sizeDelta(grown.rect, size);
}

}  // namespace pm::api::rows
