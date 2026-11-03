#include "hide.h"

#include <windows.h>

#include "Equipment.h"
#include "Props.h"
#include "Transform.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "runtime.h"

namespace pm::hide {
namespace {

constexpr ULONGLONG kSweepMs = 250;  // new salt and crucifixes are hidden within this
constexpr int kMaxDepth = 8;         // how deep under each object renderers are looked for

ULONGLONG g_next = 0;

void hide_under(void* transform, int depth) {
  if (!transform) return;
  void* object = phasmo::Component::get_gameObject(transform);
  const api::RefArray renderers{object ? phasmo::GameObject::GetComponents(object, phasmo::Renderer::type.type_object())
                                       : nullptr};
  for (size_t i = 0, n = renderers.size(); i < n; ++i) {
    void* r = renderers[i];
    if (r && api::alive(r) && phasmo::Renderer::get_enabled(r)) phasmo::Renderer::set_enabled(r, false);
  }
  if (depth >= kMaxDepth) return;
  for (int32_t i = 0; i < phasmo::Transform::get_childCount(transform); ++i)
    hide_under(phasmo::Transform::GetChild(transform, i), depth + 1);
}

void hide_all(void* type) {
  const api::RefArray found{phasmo::Object::FindObjectsOfType(type)};
  for (size_t i = 0, n = found.size(); i < n; ++i)
    if (void* o = found[i]; o && api::alive(o)) hide_under(phasmo::Component::get_transform(o), 0);
}

}  // namespace

void tick(bool ghostPlayerInContract) {
  if (!ghostPlayerInContract) return;
  const ULONGLONG now = GetTickCount64();
  if (now < g_next) return;
  g_next = now + kSweepMs;
  hide_all(phasmo::SaltSpot::type.type_object());
  hide_all(phasmo::Crucifix::type.type_object());
}

}  // namespace pm::hide
