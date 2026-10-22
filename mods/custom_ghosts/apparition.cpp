#include "apparition.h"

#include <cmath>

#include "game.h"
#include "log.h"

namespace pg::custom::apparition {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kGhostModel = 0x60;      // GhostAI: the GhostModel it wears now
constexpr size_t kModelAnimator = 0x48;   // GhostModel: its Animator
constexpr int kMaxDepth = 12;
constexpr int kFloat = 1, kInt = 3, kBool = 4;  // AnimatorControllerParameterType

struct Api {
  // What is taken off a copy, in this order (a script may need a collider, a collider a rigidbody).
  il2cpp::Object* strip[4] = {};
  il2cpp::Object* rendererType = nullptr;
  il2cpp::Object* animatorType = nullptr;
  il2cpp::Array* (*parameters)(void*, const void*) = nullptr;  // Animator.get_parameters
  // AnimatorControllerParameter.nameHash is Animator.StringToHash(name); this build strips its getter.
  il2cpp::String* (*parameterName)(void*, const void*) = nullptr;
  int (*stringToHash)(il2cpp::String*, const void*) = nullptr;
  int (*parameterType)(void*, const void*) = nullptr;          // AnimatorControllerParameter.get_type
  float (*getFloat)(void*, int) = nullptr;                     // icalls: Animator.GetFloatID and the rest
  void (*setFloat)(void*, int, float) = nullptr;
  int (*getInt)(void*, int) = nullptr;
  void (*setInt)(void*, int, int) = nullptr;
  bool (*getBool)(void*, int) = nullptr;
  void (*setBool)(void*, int, bool) = nullptr;
  bool ok = false;
} api;

void each_transform(void* transform, int depth, std::vector<void*>& out) {
  if (!transform || depth > kMaxDepth) return;
  out.push_back(transform);
  for (int i = 0; i < game::child_count(transform); ++i) each_transform(game::child(transform, i), depth + 1, out);
}

std::vector<il2cpp::Held> renderers_under(void* transform) {
  std::vector<void*> transforms;
  std::vector<il2cpp::Held> found;
  each_transform(transform, 0, transforms);
  for (void* t : transforms)
    if (void* r = game::get_component(t, api.rendererType)) found.emplace_back(r);
  return found;
}

/** Take every script, sound, collider and rigidbody off a copy, before it ever wakes. */
void strip(void* root) {
  std::vector<void*> transforms;
  each_transform(root, 0, transforms);
  for (il2cpp::Object* type : api.strip)
    for (void* t : transforms) {
      il2cpp::Array* parts = game::components(game::game_object_of(t), type);
      for (uintptr_t i = 0; parts && i < parts->max_length; ++i)
        if (void* part = parts->data<void*>()[i]) game::destroy_immediate(part);
    }
}

std::vector<Parameter> parameters_of(void* animator) {
  std::vector<Parameter> out;
  if (!animator || !game::alive(animator)) return out;
  if (il2cpp::Array* parameters = api.parameters(animator, nullptr))
    for (uintptr_t i = 0; i < parameters->max_length; ++i)
      if (void* p = parameters->data<void*>()[i]) {
        const int type = api.parameterType(p, nullptr);
        if (type == kFloat || type == kInt || type == kBool)
          out.push_back({api.stringToHash(api.parameterName(p, nullptr), nullptr), type});
      }
  return out;
}

void copy_parameters(void* to, void* from, const std::vector<Parameter>& parameters) {
  if (!to || !from || !game::alive(to) || !game::alive(from)) return;
  for (const Parameter& p : parameters) {
    if (p.type == kFloat) api.setFloat(to, p.hash, api.getFloat(from, p.hash));
    else if (p.type == kInt) api.setInt(to, p.hash, api.getInt(from, p.hash));
    else api.setBool(to, p.hash, api.getBool(from, p.hash));
  }
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  constexpr const char* kAnimation = "UnityEngine.AnimationModule.dll";
  il2cpp::Class* animator = il2cpp::cls(kAnimation, "UnityEngine", "Animator");
  il2cpp::Class* parameter = il2cpp::cls(kAnimation, "UnityEngine", "AnimatorControllerParameter");
  const il2cpp::Class* stripped[4] = {il2cpp::cls(kCore, "UnityEngine", "MonoBehaviour"),
                                      il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource"),
                                      il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Collider"),
                                      il2cpp::cls("UnityEngine.PhysicsModule.dll", "UnityEngine", "Rigidbody")};
  bool ok = animator && parameter;
  for (int i = 0; i < 4; ++i) {
    api.strip[i] = stripped[i] ? il2cpp::type_object(const_cast<il2cpp::Class*>(stripped[i])) : nullptr;
    ok &= api.strip[i] != nullptr;
  }
  il2cpp::Class* renderer = il2cpp::cls(kCore, "UnityEngine", "Renderer");
  api.rendererType = renderer ? il2cpp::type_object(renderer) : nullptr;
  api.animatorType = animator ? il2cpp::type_object(animator) : nullptr;
  ok &= api.rendererType && api.animatorType;
  ok &= il2cpp::bind(api.parameters, animator, "get_parameters", 0);
  ok &= il2cpp::bind(api.parameterName, parameter, "get_name", 0);
  ok &= il2cpp::bind(api.stringToHash, animator, "StringToHash", 1);
  ok &= il2cpp::bind(api.parameterType, parameter, "get_type", 0);
  api.getFloat = reinterpret_cast<decltype(api.getFloat)>(il2cpp::resolve_icall("UnityEngine.Animator::GetFloatID"));
  api.setFloat = reinterpret_cast<decltype(api.setFloat)>(il2cpp::resolve_icall("UnityEngine.Animator::SetFloatID"));
  api.getInt = reinterpret_cast<decltype(api.getInt)>(il2cpp::resolve_icall("UnityEngine.Animator::GetIntegerID"));
  api.setInt = reinterpret_cast<decltype(api.setInt)>(il2cpp::resolve_icall("UnityEngine.Animator::SetIntegerID"));
  api.getBool = reinterpret_cast<decltype(api.getBool)>(il2cpp::resolve_icall("UnityEngine.Animator::GetBoolID"));
  api.setBool = reinterpret_cast<decltype(api.setBool)>(il2cpp::resolve_icall("UnityEngine.Animator::SetBoolID"));
  ok &= api.getFloat && api.setFloat && api.getInt && api.setInt && api.getBool && api.setBool;
  api.ok = ok;
  PG_LOG("apparition: resolve %s", ok ? "ok" : "unavailable (no false heads or forms)");
  return true;  // optional: the ghosts still play without their apparitions
}

void* model_of(void* ghostAi) {
  void* model = ghostAi && game::alive(ghostAi) ? il2cpp::field<void*>(ghostAi, kGhostModel) : nullptr;
  return model && game::alive(model) ? model : nullptr;
}

Copy make(void* model) {
  Copy c;
  if (!api.ok || !model || !game::alive(model)) return c;
  // Copied under an inactive holder, so none of its scripts wakes before it is stripped.
  void* holder = game::new_game_object("Ghost apparition");
  if (!holder) return c;
  game::set_active(holder, false);
  void* copy = game::instantiate(game::game_object_of(model), game::object_transform(holder), false);
  if (!copy) {
    game::destroy(holder);
    return c;
  }
  c.holder = holder;
  c.object = copy;
  c.root = game::object_transform(copy);
  strip(c.root);
  c.renderers = renderers_under(c.root);
  c.animator = game::get_component(c.root, api.animatorType);
  c.model = model;
  c.modelAnimator = il2cpp::field<void*>(model, kModelAnimator);
  c.modelRenderers = renderers_under(game::transform_of(model));
  c.parameters = parameters_of(c.modelAnimator);
  game::set_active(copy, true);  // a spare model is kept inactive; its copy shows
  game::set_parent(c.root, nullptr, true);  // stands alone in the scene, so it may be placed anywhere
  game::set_active(holder, true);
  return c;
}

void place(Copy& copy, Vector3 at, float yaw) {
  if (!copy.root || !game::alive(copy.root)) return;
  const float half = yaw * 3.14159265f / 360.0f;
  game::set_position(copy.root, at);
  game::set_rotation(copy.root, {0, std::sin(half), 0, std::cos(half)});
}

void mirror(Copy& copy, void* model) {
  if (!copy.root || !game::alive(copy.root) || model != copy.model || !game::alive(model)) return;
  const size_t n = std::min(copy.renderers.size(), copy.modelRenderers.size());
  for (size_t i = 0; i < n; ++i)
    if (game::alive(copy.renderers[i]) && game::alive(copy.modelRenderers[i]))
      game::set_renderer_enabled(copy.renderers[i], game::renderer_enabled(copy.modelRenderers[i]));
  copy_parameters(copy.animator, copy.modelAnimator, copy.parameters);
}

void follow(Copy& copy, void* model) {
  if (!copy.root || !game::alive(copy.root) || !model || !game::alive(model)) return;
  if (model != copy.followed) {  // read once per model: its renderers and the parameters both Animators have
    copy.followed = model;
    copy.followedAnimator = il2cpp::field<void*>(model, kModelAnimator);
    copy.followedRenderers = renderers_under(game::transform_of(model));
    copy.shared.clear();
    for (const Parameter& p : parameters_of(copy.followedAnimator))
      for (const Parameter& own : copy.parameters)
        if (own.hash == p.hash && own.type == p.type) copy.shared.push_back(p);
  }
  bool visible = false;
  for (void* r : copy.followedRenderers)
    if (game::alive(r) && game::renderer_enabled(r)) visible = true;
  show(copy, visible);
  copy_parameters(copy.animator, copy.followedAnimator, copy.shared);
}

void set_layer(Copy& copy, int layer) {
  std::vector<void*> transforms;
  each_transform(copy.root, 0, transforms);
  for (void* t : transforms) game::set_layer(game::game_object_of(t), layer);
}

void show(Copy& copy, bool visible) {
  for (void* r : copy.renderers)
    if (game::alive(r)) game::set_renderer_enabled(r, visible);
}

void destroy(Copy& copy) {
  if (copy.root && game::alive(copy.root)) game::destroy(game::game_object_of(copy.root));
  if (copy.holder && game::alive(copy.holder)) game::destroy(copy.holder);
  copy = Copy{};
}

}  // namespace pg::custom::apparition
