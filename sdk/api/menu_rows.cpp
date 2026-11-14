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
  void* type = phasmo::TextLocaliserUI::type.type_object();  // a System.Type: raw by design
  return type && phasmo::Component::GetComponentInChildren(transform, type, true) != nullptr;
}

}  // namespace

Ref<pm::map::Transform> journal_list(void* row) {
  const Object t = Object::unchecked(transform_of(row));
  const auto list = t ? phasmo::Transform::get_parent(t) : nullptr;
  void* groupType =
      phasmo::HorizontalOrVerticalLayoutGroup::type.type_object();  // a System.Type: raw by design
  if (!list || !groupType || !phasmo::Component::GetComponent(list, groupType)) return nullptr;
  const auto net = phasmo::Network::get_Instance();
  const auto player = net ? phasmo::Network::localPlayer.get(net) : nullptr;
  const auto playerTransform = player && alive(player.raw()) ? phasmo::Component::get_transform(player) : nullptr;
  // The raw bridge takes every handle argument of one call as raw, so both go raw here.
  if (playerTransform && !phasmo::Transform::IsChildOf(t.raw(), playerTransform.raw())) return nullptr;
  return list;
}

Ref<pm::map::GameObject> divider_beside(void* row) {
  const Object t = Object::unchecked(transform_of(row));
  const auto list = t ? phasmo::Transform::get_parent(t) : nullptr;
  if (!list) return nullptr;
  const int32_t at = phasmo::Transform::GetSiblingIndex(t), count = phasmo::Transform::get_childCount(list);
  for (const int32_t i : {at + 1, at - 1}) {
    if (i < 0 || i >= count) continue;
    const auto sibling = phasmo::Transform::GetChild(list, i);
    if (sibling && !has_name(sibling.raw())) return phasmo::Component::get_gameObject(sibling);
  }
  return nullptr;
}

Ref<pm::map::GameObject> line_divider(void* row, bool* fromOtherPage) {
  const Object t = Object::unchecked(transform_of(row));
  const auto own = t ? phasmo::Transform::get_parent(t) : nullptr;
  // Another menu's line: the keybinds page's rows are `RebindActionUI`s, and the game puts a thin line between them.
  void* type = phasmo::RebindActionUI::type.type_object();  // a System.Type: raw by design
  const RefArray found{type ? phasmo::Object::FindAllOfType(type, true).raw() : nullptr};
  for (size_t i = 0; i < found.size(); ++i) {
    const auto list = found[i] ? journal_list(found[i]) : nullptr;
    if (!list || list == own) continue;
    if (auto line = divider_beside(found[i])) {
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
  std::vector<std::pair<Ref<pm::map::GameObject>, bool>> seq;  // each child, and whether it is a row rather than a divider
  const int32_t count = phasmo::Transform::get_childCount(list);
  for (int32_t i = 0; i < count; ++i) {
    const auto child = phasmo::Transform::GetChild(list, i);
    if (!child) continue;
    const auto go = phasmo::Component::get_gameObject(child);
    if (!phasmo::GameObject::get_activeSelf(go)) continue;
    seq.emplace_back(go, has_name(child.raw()) || phasmo::Transform::get_childCount(child) > 0);
  }
  // The list's own dividers are put away and `line` goes between every pair of rows instead, one line to a line as on
  // the keybinds page (the user, 2026-11-10). With no line to copy, a run of dividers between two rows keeps its first, so the rows stay divided. One at either
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
      out.added -= taken_height(seq[k].first.raw(), list);
      phasmo::GameObject::SetActive(seq[k].first, false);
      out.hidden.emplace_back(seq[k].first.raw());
    }
    i = j;
  }
  if (!line) return out;
  // A line before every row but the first. The copies go in last first, so the places still hold.
  std::vector<Ref<pm::map::GameObject>> before;
  bool first = true;
  for (const auto& [go, row] : seq) {
    if (!row) continue;
    if (!first) before.push_back(go);
    first = false;
  }
  for (auto it = before.rbegin(); it != before.rend(); ++it) {
    const auto copy = phasmo::Object::Instantiate(line, list, false);
    if (!copy) continue;
    phasmo::GameObject::SetActive(copy, true);  // `line` may be one of the dividers just put away
    const auto row = *it;  // a GameObject, as seq holds them
    phasmo::Transform::SetSiblingIndex(phasmo::GameObject::get_transform(copy),
                                       phasmo::Transform::GetSiblingIndex(phasmo::GameObject::get_transform(row)));
    out.added += taken_height(copy.raw(), list);
    out.made.emplace_back(copy.raw());
  }
  return out;
}

void lines_back(Lines& lines) {
  // Switched off first: Object.Destroy takes them out only when the frame ends, and a list laid out again in this frame
  // must not count them.
  for (Held& h : lines.made)
    if (h && alive(h)) {
      phasmo::GameObject::SetActive(h.get(), false);
      phasmo::Object::Destroy(h, 0.0f);
    }
  lines.made.clear();
  for (Held& h : lines.hidden)
    if (h && alive(h)) phasmo::GameObject::SetActive(h.get(), true);
  lines.hidden.clear();
}

Object name_text(void* row) {
  const Object t = Object::unchecked(transform_of(row));
  void* type = phasmo::TextLocaliserUI::type.type_object();  // a System.Type: raw by design
  const auto localiser = t && type ? phasmo::Component::GetComponentInChildren(t, type, true) : nullptr;
  return localiser ? phasmo::Component::GetComponent(localiser, phasmo::TextMeshProUGUI::type.type_object()) : nullptr;
}

void label(void* text, const std::string& utf8) {
  if (!text) return;
  if (const auto localiser = phasmo::Component::GetComponent(text, phasmo::TextLocaliserUI::type.type_object()))
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
    const auto child = phasmo::Transform::GetChild(root, i);
    if (child && !(keep && phasmo::Transform::IsChildOf(keep, child.raw())))
      phasmo::GameObject::SetActive(phasmo::Component::get_gameObject(child), false);
  }
}

float taken_height(void* object, void* list) {
  const auto t = object ? phasmo::GameObject::get_transform(object) : nullptr;
  if (!t) return 0.0f;
  const auto group =
      list ? phasmo::Component::GetComponent(list, phasmo::HorizontalOrVerticalLayoutGroup::type.type_object())
           : nullptr;
  const float spacing = group ? phasmo::HorizontalOrVerticalLayoutGroup::get_spacing(group) : 0.0f;
  return phasmo::RectTransform::get_rect(t.raw()).height + spacing;
}

Grown grow(void* list, float by) {
  if (!list || by <= 0.0f) return {};
  const phasmo::Vector2 lo = phasmo::RectTransform::get_anchorMin(list), hi = phasmo::RectTransform::get_anchorMax(list);
  const Object target = lo.y != hi.y ? phasmo::Transform::get_parent(list) : Object::unchecked(list);
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
