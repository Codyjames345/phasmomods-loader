#include "apparition.h"

#include <windows.h>

#include <cmath>

#include "Animator.h"
#include "GhostAI.h"
#include "LevelRoom.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "apparition_rule.h"
#include "log.h"
#include "runtime.h"

namespace pm::custom::apparition {
namespace {

constexpr int kMaxDepth = 12;

ULONGLONG g_nextNote = 0;  // the copy diagnostics: one line at most every 5 s

void* transform_of(void* component) { return component ? phasmo::Component::get_transform(component) : nullptr; }
void* game_object_of(void* component) { return component ? phasmo::Component::get_gameObject(component) : nullptr; }
void* component_of(void* component, void* systemType) {
  return component ? phasmo::Component::GetComponent(component, systemType) : nullptr;
}

void each_transform(void* transform, int depth, std::vector<void*>& out) {
  if (!transform || depth > kMaxDepth) return;
  out.push_back(transform);
  const int n = phasmo::Transform::get_childCount(transform);
  for (int i = 0; i < n; ++i) each_transform(phasmo::Transform::GetChild(transform, i), depth + 1, out);
}

std::vector<api::Held> renderers_under(void* transform) {
  std::vector<void*> transforms;
  std::vector<api::Held> found;
  each_transform(transform, 0, transforms);
  void* rendererType = phasmo::Renderer::type.type_object();
  for (void* t : transforms)
    if (void* r = component_of(t, rendererType)) found.emplace_back(r);
  return found;
}

/**
 * Take every script, sound, collider and rigidbody off a copy, before it ever wakes, in this order (a
 * script may need a collider, a collider a rigidbody).
 */
void strip(void* root) {
  std::vector<void*> transforms;
  each_transform(root, 0, transforms);
  void* const stripped[] = {phasmo::MonoBehaviour::type.type_object(), phasmo::AudioSource::type.type_object(),
                            phasmo::Collider::type.type_object(), phasmo::Rigidbody::type.type_object()};
  for (void* type : stripped)
    for (void* t : transforms) {
      void* go = game_object_of(t);
      const api::RefArray parts{go && type ? phasmo::GameObject::GetComponents(go, type) : nullptr};
      for (size_t i = 0; i < parts.size(); ++i)
        if (void* part = parts[i]) phasmo::Object::DestroyImmediate(part, false);
    }
}

std::vector<Parameter> parameters_of(void* animator) {
  std::vector<Parameter> out;
  if (!api::alive(animator)) return out;
  const int kinds[] = {phasmo::AnimatorControllerParameterType::Float(), phasmo::AnimatorControllerParameterType::Int(),
                       phasmo::AnimatorControllerParameterType::Bool()};
  const api::RefArray parameters{phasmo::Animator::get_parameters(animator)};
  for (size_t i = 0; i < parameters.size(); ++i)
    if (void* p = parameters[i]) {
      const int type = phasmo::AnimatorControllerParameter::m_Type.get(p);
      // AnimatorControllerParameter.nameHash is Animator.StringToHash(name); this build strips its getter.
      if (type == kinds[0] || type == kinds[1] || type == kinds[2])
        out.push_back({phasmo::Animator::StringToHash(phasmo::AnimatorControllerParameter::m_Name.get(p)), type});
    }
  return out;
}

/**
 * The copy's Animator: on the transform at the place the model's Animator has in the model's hierarchy
 * (it may sit on a child, where the copy's root has none), or on the copy's root when that place is not
 * found.
 */
void* animator_of_copy(void* copyRoot, void* model, void* modelAnimator) {
  void* animatorType = phasmo::Animator::type.type_object();
  if (api::alive(modelAnimator)) {
    std::vector<void*> theirs, ours;
    each_transform(transform_of(model), 0, theirs);
    each_transform(copyRoot, 0, ours);
    const int at = index_in(theirs, transform_of(modelAnimator));
    if (at >= 0 && at < static_cast<int>(ours.size()))
      if (void* animator = component_of(ours[at], animatorType)) return animator;
  }
  return component_of(copyRoot, animatorType);
}

// The game plays the model's walk at the agent's pace through Animator.speed (the hunt sets it from
// NavMeshAgent.speed every frame), so a copy takes the speed as well as the parameters.
void copy_parameters(void* to, void* from, const std::vector<Parameter>& parameters) {
  if (!api::alive(to) || !api::alive(from)) return;
  phasmo::Animator::set_speed(to, phasmo::Animator::get_speed(from));
  const int floatKind = phasmo::AnimatorControllerParameterType::Float();
  const int intKind = phasmo::AnimatorControllerParameterType::Int();
  for (const Parameter& p : parameters) {
    if (p.type == floatKind) phasmo::Animator::SetFloatID(to, p.hash, phasmo::Animator::GetFloatID(from, p.hash));
    else if (p.type == intKind) phasmo::Animator::SetIntegerID(to, p.hash, phasmo::Animator::GetIntegerID(from, p.hash));
    else phasmo::Animator::SetBoolID(to, p.hash, phasmo::Animator::GetBoolID(from, p.hash));
  }
}

void set_renderer(void* renderer, bool on) { phasmo::Renderer::set_enabled(renderer, on); }

}  // namespace

bool resolve() { return true; }

void* model_of(void* ghostAi) {
  void* model = api::alive(ghostAi) ? phasmo::GhostAI::model.get(ghostAi) : nullptr;
  return api::alive(model) ? model : nullptr;
}

Copy make(void* model) {
  Copy c;
  if (!api::alive(model)) return c;
  // Copied under an inactive holder, so none of its scripts wakes before it is stripped.
  void* holder = phasmo::GameObject::type.make();
  if (!holder) return c;
  phasmo::GameObject::ctor(holder, api::make_string("Ghost apparition"));
  phasmo::GameObject::SetActive(holder, false);
  void* copy = phasmo::Object::Instantiate(game_object_of(model), phasmo::GameObject::get_transform(holder), false);
  if (!copy) {
    phasmo::Object::Destroy(holder, 0.0f);
    return c;
  }
  c.holder = holder;
  c.object = copy;
  c.root = phasmo::GameObject::get_transform(copy);
  strip(c.root);
  c.renderers = renderers_under(c.root);
  c.model = model;
  c.modelAnimator = phasmo::GhostModel::animator.get(model);
  c.animator = animator_of_copy(c.root, model, c.modelAnimator);
  c.modelRenderers = renderers_under(transform_of(model));
  c.parameters = parameters_of(c.modelAnimator);
  const ULONGLONG now = GetTickCount64();
  if (now >= g_nextNote) {  // what the copy animates with, for the in-game check
    g_nextNote = now + 5000;
    void* modelAnimator = c.modelAnimator;
    const bool found = api::alive(modelAnimator);
    const bool onRoot = found && transform_of(modelAnimator) == transform_of(model);
    PM_LOG("apparition: a copy: the model's Animator %s (%s, enabled %d, speed %.2f); the copy's %s (enabled %d); "
           "%zu parameters, %zu renderers",
           modelAnimator ? "found" : "missing", onRoot ? "on the model's root" : "on a child",
           found ? phasmo::Behaviour::get_enabled(modelAnimator) : -1,
           found ? phasmo::Animator::get_speed(modelAnimator) : -1.0f, c.animator ? "found" : "missing",
           c.animator ? phasmo::Behaviour::get_enabled(c.animator) : -1, c.parameters.size(), c.renderers.size());
  }
  phasmo::GameObject::SetActive(copy, true);  // a spare model is kept inactive; its copy shows
  phasmo::Transform::SetParent(c.root, nullptr, true);  // stands alone in the scene, so it may be placed anywhere
  phasmo::GameObject::SetActive(holder, true);
  return c;
}

void place(Copy& copy, phasmo::Vector3 at, float yaw) {
  if (!api::alive(copy.root)) return;
  const float half = yaw * 3.14159265f / 360.0f;
  const phasmo::Vector3 where{at.x, at.y, at.z};
  const phasmo::Quaternion turn{0, std::sin(half), 0, std::cos(half)};
  phasmo::Transform::set_position(copy.root, &where);
  phasmo::Transform::set_rotation(copy.root, &turn);
}

void mirror(Copy& copy, void* model) {
  if (!api::alive(copy.root) || model != copy.model || !api::alive(model)) return;
  const size_t n = std::min(copy.renderers.size(), copy.modelRenderers.size());
  for (size_t i = 0; i < n; ++i)
    if (api::alive(copy.renderers[i]) && api::alive(copy.modelRenderers[i]))
      set_renderer(copy.renderers[i], phasmo::Renderer::get_enabled(copy.modelRenderers[i]));
  copy_parameters(copy.animator, copy.modelAnimator, copy.parameters);
}

void follow(Copy& copy, void* model) {
  if (!api::alive(copy.root) || !api::alive(model)) return;
  if (model != copy.followed) {  // read once per model: its renderers and the parameters both Animators have
    copy.followed = model;
    copy.followedAnimator = phasmo::GhostModel::animator.get(model);
    copy.followedRenderers = renderers_under(transform_of(model));
    copy.shared.clear();
    for (const Parameter& p : parameters_of(copy.followedAnimator))
      for (const Parameter& own : copy.parameters)
        if (own.hash == p.hash && own.type == p.type) copy.shared.push_back(p);
  }
  bool visible = false;
  for (void* r : copy.followedRenderers)
    if (api::alive(r) && phasmo::Renderer::get_enabled(r)) visible = true;
  show(copy, visible);
  copy_parameters(copy.animator, copy.followedAnimator, copy.shared);
}

void set_layer(Copy& copy, int layer) {
  std::vector<void*> transforms;
  each_transform(copy.root, 0, transforms);
  for (void* t : transforms)
    if (void* go = game_object_of(t)) phasmo::GameObject::set_layer(go, layer);
}

void show(Copy& copy, bool visible) {
  for (void* r : copy.renderers)
    if (api::alive(r)) set_renderer(r, visible);
}

void destroy(Copy& copy) {
  if (api::alive(copy.root)) phasmo::Object::Destroy(game_object_of(copy.root), 0.0f);
  if (api::alive(copy.holder)) phasmo::Object::Destroy(copy.holder, 0.0f);
  copy = Copy{};
}

}  // namespace pm::custom::apparition
