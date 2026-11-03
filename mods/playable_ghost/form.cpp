#include "form.h"

#include <windows.h>

#include <vector>

#include "Scene.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "game.h"
#include "held.h"
#include "Transform.h"
#include "log.h"

namespace pm::form {
namespace {

using phasmo::Vector3;

constexpr int kMaxDepth = 12;

using phasmo::Renderer;
using phasmo::SkinnedMeshRenderer;

struct Figure {
  api::Held root;                 // GameObject
  std::vector<api::Held> meshes;  // baked, destroyed with the figure
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
  void* mesh = phasmo::Mesh::type.make();
  if (!mesh) return;
  phasmo::Mesh::ctor(mesh);
  SkinnedMeshRenderer::BakeMesh(skinned, mesh, false);
  g.meshes.push_back(mesh);
  void* part = game::new_game_object("Form part");
  void* t = game::object_transform(part);
  void* from = game::transform_of(skinned);
  game::set_position(t, game::position(from));
  game::set_rotation(t, game::rotation(from));
  game::set_parent(t, game::object_transform(root), true);
  if (void* filter = phasmo::GameObject::AddComponent(part, phasmo::MeshFilter::type.type_object()))
    phasmo::MeshFilter::set_sharedMesh(filter, mesh);
  if (void* drawn = phasmo::GameObject::AddComponent(part, phasmo::MeshRenderer::type.type_object()))
    Renderer::set_sharedMaterials(drawn, Renderer::get_sharedMaterials(skinned));
}

/** Every drawn skinned body part of the investigator, wherever it sits under their Player. */
void collect(void* transform, int depth, std::vector<void*>& out) {
  if (!transform || depth > kMaxDepth) return;
  void* go = game::game_object_of(transform);
  if (!go || !game::active_in_hierarchy(go) || game::layer(go) == phasmo::Layers::PlayerView.index()) return;
  void* skinned = game::alive(transform) ? phasmo::Component::GetComponent(transform, SkinnedMeshRenderer::type.type_object())
                                         : nullptr;
  if (skinned && Renderer::get_enabled(skinned)) out.push_back(skinned);
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
    PM_LOG("form: nothing of that investigator is drawn here");
    return;
  }
  g.root = game::new_game_object("Ghost form");
  void* rootT = game::object_transform(g.root);
  game::set_position(rootT, game::position(body));  // the parts keep their place on the body ...
  game::set_rotation(rootT, game::rotation(body));
  for (void* p : parts) bake_part(p, g.root);
  g.until = GetTickCount64() + f.ms;
  PM_LOG("form: the ghost takes an investigator's form (%zu parts)", parts.size());
}

void follow(void* ghost) {
  void* gt = game::transform_of(ghost);
  void* rootT = game::object_transform(g.root);
  game::set_position(rootT, game::position(gt));  // ... and the body stands where the ghost is
  game::set_rotation(rootT, game::rotation(gt));
}

}  // namespace

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

}  // namespace pm::form
