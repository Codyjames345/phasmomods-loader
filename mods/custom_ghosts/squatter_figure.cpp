#include "squatter_figure.h"

#include <cmath>

#include "Animator.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "bundles.h"
#include "held.h"
#include "log.h"
#include "models.h"
#include "runtime.h"
#include "sheet_mesh.h"

namespace pm::custom::squatter::figure {
namespace {

constexpr const char* kBundleFile = "squatter.bundle";
constexpr const char* kController = "assets/squatter.controller";

// How the sheet sits and moves: starting values, judged with the user in game.
constexpr float kHeadCentreAboveBone = 0.08f;  // the head sphere's centre above the head bone, metres
const api::ClothSettings kCloth{true, 0.1f, 1.0f, 0.2f};
// The spheres the sheet rests on besides the head, from the engine test: chest, spine, hips and both upper arms.
constexpr float kChest = 0.19f, kSpine = 0.17f, kHips = 0.18f, kArm = 0.08f;
// The floor the sheet rests on: Cloth collides only with spheres and capsules, so a sphere this large lies under the
// feet, its top level with them. Across the sheet's spread (about 0.6 m) its surface falls only a few centimetres, so the
// hem settles on the floor rather than through it (user, 2026-11-09: it showed through the ceiling below).
constexpr float kFloorRadius = 3.0f;

api::Held g_bundle;
bool g_failed = false;  // a create that failed, logged once, until destroy()

struct Figure {
  api::Held root, animator, sheet, material;
  api::ClothParts cloth;
} g;

void fail(const char* step, const std::string& why) {
  if (!g_failed) PM_LOG("CustomGhosts: the Squatter's figure could not be built (%s): %s", step, why.c_str());
  g_failed = true;
}

// Destroys the figure's own assets, and its objects when the scene has not already.
void clear() {
  api::destroy_cloth(g.cloth);
  if (api::alive(g.material)) phasmo::Object::Destroy(g.material, 0.0f);
  if (api::alive(g.root)) phasmo::Object::Destroy(g.root, 0.0f);
  g = Figure{};
}

// A scene load destroys the copy: forget it, freeing what the scene does not.
bool check() {
  if (g.root && !api::alive(g.root)) clear();
  return api::alive(g.root) && api::alive(g.animator);
}

void* bone(int32_t which) { return phasmo::Animator::GetBoneTransform(g.animator, which); }

bool build_sheet() {
  void* head = bone(phasmo::HumanBodyBones::Head());
  if (!head) return fail("sheet", "the body has no head bone"), false;
  std::string why;
  void* material = api::two_sided_material(api::lit_material_of(g.root), {1, 1, 1, 1}, &why);
  if (!material) return fail("sheet material", why), false;
  g.material = material;

  // The sheet's crown sits on top of the head sphere, facing the way the body faces; it follows the head.
  const sheet::SheetShape shape;
  void* anchor = phasmo::GameObject::type.make();
  if (!anchor) return fail("sheet", "no GameObject could be made"), false;
  phasmo::GameObject::ctor(anchor, api::make_string("Squatter sheet"));
  g.sheet = anchor;
  phasmo::GameObject::set_layer(anchor, phasmo::GameObject::get_layer(g.root));
  void* t = phasmo::GameObject::get_transform(anchor);
  phasmo::Transform::SetParent(t, head, false);
  const phasmo::Vector3 h = phasmo::Transform::get_position(head);
  const phasmo::Vector3 crown{h.x, h.y + kHeadCentreAboveBone + shape.headRadius, h.z};
  const phasmo::Quaternion facing = phasmo::Transform::get_rotation(phasmo::GameObject::get_transform(g.root));
  phasmo::Transform::set_position(t, &crown);
  phasmo::Transform::set_rotation(t, &facing);

  const sheet::SheetMesh m = sheet::make_sheet(shape);
  std::vector<phasmo::Vector3> vertices, normals;
  std::vector<phasmo::Vector2> uv, coefficients;
  for (size_t i = 0; i < m.vertices.size(); ++i) {
    vertices.push_back({m.vertices[i].x, m.vertices[i].y, m.vertices[i].z});
    normals.push_back({m.normals[i].x, m.normals[i].y, m.normals[i].z});
    uv.push_back({m.uv[i].x, m.uv[i].y});
    coefficients.push_back({m.coefficients[i].maxDistance, m.coefficients[i].collisionSphereDistance});
  }
  const api::ClothMesh mesh{vertices, normals, uv, m.triangles, coefficients};
  const phasmo::Vector3 none{};
  const api::ClothSphere spheres[] = {
      {head, shape.headRadius, {0, kHeadCentreAboveBone, 0}},
      {bone(phasmo::HumanBodyBones::Chest()), kChest, none},
      {bone(phasmo::HumanBodyBones::Spine()), kSpine, none},
      {bone(phasmo::HumanBodyBones::Hips()), kHips, none},
      {bone(phasmo::HumanBodyBones::LeftUpperArm()), kArm, none},
      {bone(phasmo::HumanBodyBones::RightUpperArm()), kArm, none},
      // The figure's root is at its feet, and follows the ghost's position each frame (place).
      {phasmo::GameObject::get_transform(g.root), kFloorRadius, {0, -kFloorRadius, 0}}};
  if (!api::add_cloth(anchor, mesh, material, kCloth, spheres, g.cloth, &why)) return fail("cloth", why), false;
  return true;
}

}  // namespace

bool load(const std::filesystem::path& assets, std::string& why) {
  unload();
  if (assets.empty()) return why = "the mod has no assets folder", false;
  // Every optional entry the sheet needs, so a build lacking one fails the start rather than every contract's figure.
  if (!api::cloth_available(&why) || !api::material_available(&why)) return false;
  void* bundle = api::load_bundle(assets / kBundleFile, &why);
  if (!bundle) return why = std::string(kBundleFile) + ": " + why, false;
  g_bundle = bundle;
  void* controller = api::load_controller(bundle, kController);
  std::string missing;
  const std::vector<const char*> states = all_states();
  if (!controller) {
    why = std::string(kBundleFile) + " has no " + kController;
  } else if (!api::controller_has_states(controller, states, &missing)) {
    why = std::string(kController) + " lacks the state " + missing;
  } else {
    return true;
  }
  unload();
  return false;
}

void unload() {
  destroy();
  if (api::alive(g_bundle)) api::unload_bundle(g_bundle, false);
  g_bundle = nullptr;
}

void create(phasmo::Vector3 at) {
  if (check() || g_failed) return;
  if (!api::alive(g_bundle)) return fail("bundle", "the bundle is not loaded");
  std::string why;
  void* copy = api::copy_investigator_body("Squatter", &why);
  if (!copy) return fail("body", why);
  g.root = copy;
  g.animator = phasmo::Component::GetComponent(phasmo::GameObject::get_transform(copy),
                                               phasmo::Animator::type.type_object());
  void* controller = api::load_controller(g_bundle, kController);
  if (!api::use_controller(g.animator, controller)) {
    clear();
    return fail("animator", "the body has no Animator or the bundle no controller");
  }
  place(at, 0.0f);
  if (!build_sheet()) clear();
}

void destroy() {
  clear();
  g_failed = false;
}

void place(phasmo::Vector3 at, float yaw) {
  if (!check()) return;
  void* t = phasmo::GameObject::get_transform(g.root);
  const phasmo::Quaternion turn{0, std::sin(yaw / 2), 0, std::cos(yaw / 2)};
  phasmo::Transform::set_position(t, &at);
  phasmo::Transform::set_rotation(t, &turn);
}

void play(Move m, uint32_t variant) {
  if (check()) api::play_state(g.animator, state_for(m, variant));
}

void play_act(Act a) {
  if (check()) api::play_state(g.animator, state_of(a));
}

bool ready() { return check(); }

}  // namespace pm::custom::squatter::figure
