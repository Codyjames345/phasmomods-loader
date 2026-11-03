#pragma once

// What the host's journal sides (host/content_journal.h, host/content_evidence.h) do alike with the
// game's widgets: a component's Transform and GameObject, a text's TextLocaliserUI and its key, a
// parent's active children, and a managed array grown by copies. Through map entries only. Main thread.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "JournalController.h"
#include "Transform.h"
#include "UnityObject.h"
#include "game_api.h"
#include "runtime.h"

namespace pm::host::widgets {

/// The Transform of `component` (Component.get_transform), or null for null.
inline void* transform_of(void* component) { return component ? phasmo::Component::get_transform(component) : nullptr; }
/// The Transform of `gameObject` (GameObject.get_transform), or null for null. Component.get_transform on a GameObject reads the
/// wrong native object and crashes the game: an array the map notes as GameObject[] goes through here.
inline void* transform_of_object(void* gameObject) { return gameObject ? phasmo::GameObject::get_transform(gameObject) : nullptr; }
/// The GameObject of `component` (Component.get_gameObject), or null for null.
inline void* game_object_of(void* component) {
  return component ? phasmo::Component::get_gameObject(component) : nullptr;
}

/// The TextLocaliserUI beside `component` (a text), or null.
inline void* localiser_of(void* component) {
  return component ? phasmo::Component::GetComponent(component, phasmo::TextLocaliserUI::type.type_object()) : nullptr;
}

/// The key `localiser` writes, or empty.
inline std::string key_of(void* localiser) {
  return localiser ? pm::api::to_utf8(phasmo::TextLocaliserUI::key.get(localiser)) : std::string();
}

/// Gives `localiser` the key `key`, which it writes at once (TextLocaliserUI.set_Key).
inline void set_key(void* localiser, const std::string& key) {
  if (localiser) phasmo::TextLocaliserUI::set_Key(localiser, pm::api::make_string(key));
}

/// The children of `parent` whose GameObjects are active, in sibling order.
inline std::vector<void*> active_children(void* parent) {
  std::vector<void*> out;
  const int32_t n = phasmo::Transform::get_childCount(parent);
  for (int32_t i = 0; i < n; ++i) {
    void* child = phasmo::Transform::GetChild(parent, i);
    if (child && phasmo::GameObject::get_activeSelf(game_object_of(child))) out.push_back(child);
  }
  return out;
}

/// A copy of the managed array `array` with `extra` after its elements, of the element type `type`;
/// null when it cannot be made.
template <class T>
void* appended(void* array, const T& type, const std::vector<void*>& extra) {
  const pm::api::RefArray from{array};
  void* out = type.make_array(from.size() + extra.size());
  if (!out) return nullptr;
  const pm::api::RefArray to{out};
  for (size_t i = 0; i < from.size(); ++i) to.set(i, from[i]);
  for (size_t i = 0; i < extra.size(); ++i) to.set(from.size() + i, extra[i]);
  return out;
}

}  // namespace pm::host::widgets
