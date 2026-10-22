#include "form.h"

#include <windows.h>

#include <vector>

#include "game.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::form {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr int kPlayerViewLayer = 11;         // held items and first-person parts: never part of the figure
constexpr int kMaxDepth = 12;

struct Api {
  il2cpp::Class* meshClass = nullptr;
  il2cpp::Object* skinnedType = nullptr;
  il2cpp::Object* filterType = nullptr;
  il2cpp::Object* rendererType = nullptr;  // MeshRenderer
  void (*meshCtor)(void*, const void*) = nullptr;
  void (*bake)(void*, void*, bool) = nullptr;
  il2cpp::Array* (*sharedMaterials)(void*, const void*) = nullptr;
  void (*setSharedMaterials)(void*, il2cpp::Array*, const void*) = nullptr;
  void (*setSharedMesh)(void*, void*, const void*) = nullptr;
  bool (*rendererEnabled)(void*, const void*) = nullptr;
} api;

struct Figure {
  il2cpp::Held root;                 // GameObject
  std::vector<il2cpp::Held> meshes;  // baked, destroyed with the figure
  ULONGLONG until = 0;
} g;

void clear() {
  if (g.root && game::alive(g.root)) game::destroy(g.root);
  for (void* m : g.meshes)
    if (game::alive(m)) game::destroy(m);
  g = Figure{};
}

/** Bake one skinned body part into a still mesh under the figure, where it is now. */
void bake_part(void* skinned, void* root) {
  void* mesh = il2cpp::object_new(api.meshClass);
  if (!mesh) return;
  api.meshCtor(mesh, nullptr);
  api.bake(skinned, mesh, false);
  g.meshes.push_back(mesh);
  void* part = game::new_game_object("Form part");
  void* t = game::object_transform(part);
  void* from = game::transform_of(skinned);
  game::set_position(t, game::position(from));
  game::set_rotation(t, game::rotation(from));
  game::set_parent(t, game::object_transform(root), true);
  api.setSharedMesh(game::add_component(part, api.filterType), mesh, nullptr);
  api.setSharedMaterials(game::add_component(part, api.rendererType), api.sharedMaterials(skinned, nullptr), nullptr);
}

/** Every drawn skinned body part of the investigator, wherever it sits under their Player. */
void collect(void* transform, int depth, std::vector<void*>& out) {
  if (!transform || depth > kMaxDepth) return;
  void* go = game::game_object_of(transform);
  if (!go || !game::active_in_hierarchy(go) || game::layer(go) == kPlayerViewLayer) return;
  if (void* skinned = game::get_component(transform, api.skinnedType); skinned && api.rendererEnabled(skinned, nullptr))
    out.push_back(skinned);
  const int n = game::child_count(transform);
  for (int i = 0; i < n; ++i) collect(game::child(transform, i), depth + 1, out);
}

void take_form(const session::Form& f, void* ghost) {
  clear();
  void* player = game::player_of_actor(f.actor);
  if (!player || !game::alive(player) || !ghost) return;
  std::vector<void*> parts;
  void* body = game::transform_of(player);
  collect(body, 0, parts);
  if (parts.empty()) {
    PG_LOG("form: nothing of that investigator is drawn here");
    return;
  }
  g.root = game::new_game_object("Ghost form");
  void* rootT = game::object_transform(g.root);
  game::set_position(rootT, game::position(body));  // the parts keep their place on the body ...
  game::set_rotation(rootT, game::rotation(body));
  for (void* p : parts) bake_part(p, g.root);
  g.until = GetTickCount64() + f.ms;
  PG_LOG("form: the ghost takes an investigator's form (%zu parts)", parts.size());
}

void follow(void* ghost) {
  void* gt = game::transform_of(ghost);
  void* rootT = game::object_transform(g.root);
  game::set_position(rootT, game::position(gt));  // ... and the body stands where the ghost is
  game::set_rotation(rootT, game::rotation(gt));
}

}  // namespace

bool resolve() {
  constexpr const char* kCore = "UnityEngine.CoreModule.dll";
  api.meshClass = il2cpp::cls(kCore, "UnityEngine", "Mesh");
  il2cpp::Class* skinned = il2cpp::cls(kCore, "UnityEngine", "SkinnedMeshRenderer");
  il2cpp::Class* renderer = il2cpp::cls(kCore, "UnityEngine", "Renderer");
  il2cpp::Class* filter = il2cpp::cls(kCore, "UnityEngine", "MeshFilter");
  il2cpp::Class* meshRenderer = il2cpp::cls(kCore, "UnityEngine", "MeshRenderer");
  bool ok = api.meshClass && skinned && renderer && filter && meshRenderer;
  ok &= il2cpp::bind(api.meshCtor, api.meshClass, ".ctor", 0);
  // Unity 2022.3 registers this native call even when the managed wrapper is stripped.
  api.bake = reinterpret_cast<decltype(api.bake)>(
      il2cpp::resolve_icall("UnityEngine.SkinnedMeshRenderer::BakeMesh"));
  ok &= api.bake != nullptr;
  ok &= il2cpp::bind(api.sharedMaterials, renderer, "get_sharedMaterials", 0);
  ok &= il2cpp::bind(api.setSharedMaterials, renderer, "set_sharedMaterials", 1);
  ok &= il2cpp::bind(api.setSharedMesh, filter, "set_sharedMesh", 1);
  ok &= il2cpp::bind(api.rendererEnabled, renderer, "get_enabled", 0);
  if (ok) {
    api.skinnedType = il2cpp::type_object(skinned);
    api.filterType = il2cpp::type_object(filter);
    api.rendererType = il2cpp::type_object(meshRenderer);
  }
  ok &= api.skinnedType && api.filterType && api.rendererType;
  PG_LOG("form: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

void tick(session::Session& s, void* ghost) {
  session::Form f;
  while (s.take_form(f)) take_form(f, ghost);
  if (!g.root) return;
  if (!ghost || !game::alive(ghost) || !game::alive(g.root) || GetTickCount64() >= g.until || !game::level_controller()) {
    clear();
    return;
  }
  follow(ghost);
}

void teardown() { clear(); }

}  // namespace pg::form
