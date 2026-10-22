#include "expand.h"

#include <algorithm>
#include <cstring>

#include "game.h"
#include "log.h"

namespace pg::lobbies::expand {
namespace {

constexpr size_t kViewSceneId = 0x90;  // PhotonView.sceneViewId (serialized; Awake takes the ID from it)
constexpr size_t kViewId = 0x94;       // PhotonView.viewIdField

struct Api {
  il2cpp::Object* layoutType = nullptr;
  il2cpp::Object* viewType = nullptr;
  void (*setIgnore)(void*, bool, const void*) = nullptr;
  int (*sibling)(void*, const void*) = nullptr;
  il2cpp::Array* (*findAll)(il2cpp::Object*, bool, const void*) = nullptr;
} api;

bool is_game_object(void* o) { return il2cpp::is_class(o, "GameObject"); }
void* transform_of_any(void* o) { return is_game_object(o) ? game::object_transform(o) : game::transform_of(o); }

}  // namespace

bool resolve() {
  il2cpp::Class* layout = il2cpp::cls("UnityEngine.UI.dll", "UnityEngine.UI", "LayoutElement");
  il2cpp::Class* view = il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonView");
  il2cpp::Class* transform = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Transform");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  bool ok = layout && view && transform && object;
  ok &= il2cpp::bind(api.setIgnore, layout, "set_ignoreLayout", 1);
  ok &= il2cpp::bind(api.sibling, transform, "GetSiblingIndex", 0);
  ok &= il2cpp::bind(api.findAll, object, "FindObjectsOfType", 2);  // (Type, bool includeInactive)
  if (ok) {
    api.layoutType = il2cpp::type_object(layout);
    api.viewType = il2cpp::type_object(view);
  }
  return ok && api.layoutType && api.viewType;
}

void* clone(void* original) {
  void* t = transform_of_any(original);
  return original ? game::instantiate(original, game::parent(t), false) : nullptr;
}

void* counterpart(void* root, void* component, void* copyRoot) {
  void* rootT = transform_of_any(root);
  void* t = transform_of_any(component);
  std::vector<int> path;  // child indices from the root down to the component
  while (t && t != rootT) {
    path.push_back(api.sibling(t, nullptr));
    t = game::parent(t);
  }
  if (t != rootT) return nullptr;
  void* at = transform_of_any(copyRoot);
  for (auto it = path.rbegin(); it != path.rend() && at; ++it) at = *it < game::child_count(at) ? game::child(at, *it) : nullptr;
  if (!at) return nullptr;
  if (is_game_object(component)) return game::game_object_of(at);
  return game::get_component(at, il2cpp::type_object(il2cpp::class_of(component)));
}

il2cpp::Array* grown(il2cpp::Array* array, const std::vector<void*>& extra) {
  il2cpp::Class* element = array ? il2cpp::element_class(il2cpp::class_of(array)) : nullptr;
  il2cpp::Array* out = element ? il2cpp::array_new(element, array->max_length + extra.size()) : nullptr;
  if (!out) return nullptr;
  for (uintptr_t i = 0; i < array->max_length; ++i) out->data<void*>()[i] = array->data<void*>()[i];
  for (size_t i = 0; i < extra.size(); ++i) out->data<void*>()[array->max_length + i] = extra[i];
  return out;
}

void ignore_layout(void* component, bool ignore) {
  void* go = is_game_object(component) ? component : game::game_object_of(component);
  if (!go) return;
  void* element = game::get_component(game::object_transform(go), api.layoutType);
  if (!element && !ignore) return;  // nothing to undo
  if (!element) element = game::add_component(go, api.layoutType);
  if (element) api.setIgnore(element, ignore, nullptr);
}

bool list_add(il2cpp::List* list, void* item) {
  const il2cpp::Method* m = list ? il2cpp::method(il2cpp::class_of(list), "Add", 1) : nullptr;
  if (!m) return false;
  reinterpret_cast<void (*)(void*, void*, const void*)>(*reinterpret_cast<void* const*>(m))(list, item, m);
  return true;
}

int free_view_id(int offset) {
  int highest = 0;
  il2cpp::Array* views = api.findAll(api.viewType, true, nullptr);
  for (uintptr_t i = 0; views && i < views->max_length; ++i)
    if (void* v = views->data<void*>()[i]) highest = std::max({highest, il2cpp::field<int>(v, kViewSceneId), il2cpp::field<int>(v, kViewId) % 1000});
  return highest + 1 + offset;
}

void set_view_id(void* object, int id) {
  void* t = transform_of_any(object);
  void* view = t ? game::get_component(t, api.viewType) : nullptr;
  if (!view) return;
  il2cpp::set_field<int>(view, kViewSceneId, id);
  il2cpp::set_field<int>(view, kViewId, id);
}

}  // namespace pg::lobbies::expand
