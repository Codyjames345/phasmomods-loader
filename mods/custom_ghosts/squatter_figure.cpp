#include "squatter_figure.h"

#include <algorithm>
#include <cmath>

#include "Animator.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "bundles.h"
#include "held.h"
#include "log.h"
#include "models.h"
#include "quat.h"
#include "runtime.h"
#include "sheet_mesh.h"

namespace pm::custom::squatter::figure {
namespace {

constexpr const char* kBundleFile = "squatter.bundle";
constexpr const char* kController = "assets/squatter.controller";

// How the sheet sits and moves: starting values, judged with the user in game.
constexpr float kHeadCentreAboveBone = 0.09f;  // the head sphere's centre above the head bone, metres
const api::ClothSettings kCloth{true, 0.1f, 1.0f, 0.2f};
// The spheres the sheet rests on besides the head, so the body never shows through it (user, 2026-11-09: the sheet was
// too thin and the legs and arms came through): a little larger than the body there, which gives the cloth its
// thickness, on the torso, neck, shoulders, both arms and both legs. Starting values, judged with the user in game.
constexpr float kChest = 0.24f, kSpine = 0.22f, kHips = 0.22f, kNeck = 0.09f, kShoulder = 0.10f;
constexpr float kArm = 0.10f, kForearm = 0.08f, kThigh = 0.12f, kShin = 0.09f;
// The floor the sheet rests on: Cloth collides only with spheres and capsules, so a sphere this large lies under the
// feet, its top level with them. Across the sheet's spread (about 0.6 m) its surface falls only a few centimetres, so the
// hem settles on the floor rather than through it (user, 2026-11-09: it showed through the ceiling below).
constexpr float kFloorRadius = 3.0f;

api::Held g_bundle;
bool g_failed = false;  // a create that failed, logged once, until destroy()

struct Figure {
  api::Held root, animator, sheet, material;
  api::ClothParts cloth;
  const char* playing = nullptr;  // the state it plays, which a cross-fade blends from
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

// How much wider than the shoulders' spheres the sheet hangs at the shoulders, and how far past the floor its hem
// reaches so it rests on it: starting values, judged with the user in game (2026-11-09: wider ballooned it).
constexpr float kSheetClearance = 0.02f;
constexpr float kHemOnFloor = 0.05f;

// The bones the sheet follows, each as a line to the next joint along it (sheet::Segment), and the Transform that
// carries that line: the head first, which alone carries the cap over it.
struct Carrier {
  int32_t bone;
  int32_t to;  // the joint the line runs to; -1 for a point at the bone
};

bool build_sheet() {
  namespace B = phasmo::HumanBodyBones;
  void* head = bone(B::Head());
  if (!head) return fail("sheet", "the body has no head bone"), false;
  std::string why;
  void* material = api::two_sided_material(api::lit_material_of(g.root), {1, 1, 1, 1}, &why);
  if (!material) return fail("sheet material", why), false;
  g.material = material;

  // The body stands in its standing clip (create), so the sheet is laid over it as it stands: its cap on the head,
  // its shoulders just outside the upper arms' spheres, its hem on the floor at the feet. The arms below hang inside
  // the flare, and carry the sheet with them (the skinning below).
  sheet::SheetShape shape;
  const phasmo::Vector3 h = phasmo::Transform::get_position(head);
  const phasmo::Vector3 crown{h.x, h.y + kHeadCentreAboveBone + shape.headRadius, h.z};
  const phasmo::Vector3 feet = phasmo::Transform::get_position(phasmo::GameObject::get_transform(g.root));
  float reach = 0.0f;
  for (const int32_t arm : {B::LeftUpperArm(), B::RightUpperArm()}) {
    void* t = bone(arm);
    if (!t) return fail("sheet", "the body lacks an arm bone the sheet covers"), false;
    const phasmo::Vector3 at = phasmo::Transform::get_position(t);
    reach = std::max(reach, std::hypot(at.x - crown.x, at.z - crown.z));
  }
  shape.shoulderRadius = std::max(shape.headRadius + kSheetClearance, reach + kArm + kSheetClearance);
  void* upperArm = bone(B::LeftUpperArm());
  shape.shoulderDrop = std::max(0.05f, crown.y - shape.headRadius - phasmo::Transform::get_position(upperArm).y);
  shape.side = 2.0f * (sheet::reach_distance(shape, crown.y - feet.y) + kHemOnFloor);

  // The sheet's crown sits on top of the head sphere, facing the way the body faces; it follows the head.
  void* anchor = phasmo::GameObject::type.make();
  if (!anchor) return fail("sheet", "no GameObject could be made"), false;
  phasmo::GameObject::ctor(anchor, api::make_string("Squatter sheet"));
  g.sheet = anchor;
  phasmo::GameObject::set_layer(anchor, phasmo::GameObject::get_layer(g.root));
  void* t = phasmo::GameObject::get_transform(anchor);
  phasmo::Transform::SetParent(t, head, false);
  const phasmo::Quaternion facing = phasmo::Transform::get_rotation(phasmo::GameObject::get_transform(g.root));
  phasmo::Transform::set_position(t, &crown);
  phasmo::Transform::set_rotation(t, &facing);
  // A world point in the sheet's own space: from its crown, turned back from the way it faces.
  const auto on_sheet = [&](phasmo::Vector3 w) {
    const phasmo::Vector3 v = qrot(qinv(facing), {w.x - crown.x, w.y - crown.y, w.z - crown.z});
    return sheet::Vec3{v.x, v.y, v.z};
  };

  const sheet::SheetMesh m = sheet::make_sheet(shape);
  std::vector<phasmo::Vector3> vertices, normals;
  std::vector<phasmo::Vector2> uv, coefficients;
  for (size_t i = 0; i < m.vertices.size(); ++i) {
    vertices.push_back({m.vertices[i].x, m.vertices[i].y, m.vertices[i].z});
    normals.push_back({m.normals[i].x, m.normals[i].y, m.normals[i].z});
    uv.push_back({m.uv[i].x, m.uv[i].y});
    coefficients.push_back({m.coefficients[i].maxDistance, m.coefficients[i].collisionSphereDistance});
  }
  // Skinned to the whole body as worn clothing is in games (sheet::weight_by_nearness): each point follows the bones
  // nearest it, so the sheet goes with the arms and legs, and the cloth only sways about that (kHemSway). The cap over
  // the head follows the head alone, so the eye holes stay over the eyes.
  const Carrier carriers[] = {
      {B::Head(), -1},
      {B::Neck(), B::Head()},
      {B::Chest(), B::Neck()},
      {B::Spine(), B::Chest()},
      {B::Hips(), B::Spine()},
      {B::LeftShoulder(), B::LeftUpperArm()},
      {B::RightShoulder(), B::RightUpperArm()},
      {B::LeftUpperArm(), B::LeftLowerArm()},
      {B::RightUpperArm(), B::RightLowerArm()},
      {B::LeftLowerArm(), B::LeftHand()},
      {B::RightLowerArm(), B::RightHand()},
      {B::LeftHand(), -1},
      {B::RightHand(), -1},
      {B::LeftUpperLeg(), B::LeftLowerLeg()},
      {B::RightUpperLeg(), B::RightLowerLeg()},
      {B::LeftLowerLeg(), B::LeftFoot()},
      {B::RightLowerLeg(), B::RightFoot()},
      {B::LeftFoot(), -1},
      {B::RightFoot(), -1}};
  std::vector<void*> bones;
  std::vector<sheet::Segment> segments;
  for (const Carrier& c : carriers) {
    void* from = bone(c.bone);
    void* to = c.to >= 0 ? bone(c.to) : from;
    if (!from || !to) return fail("sheet", "the body lacks a bone the sheet follows"), false;
    bones.push_back(from);
    segments.push_back({on_sheet(phasmo::Transform::get_position(from)), on_sheet(phasmo::Transform::get_position(to))});
  }
  // The head's line runs up to the middle of the head, which its sphere covers.
  segments[0].to = on_sheet({h.x, h.y + kHeadCentreAboveBone, h.z});
  const float cap = sheet::cap_distance(shape);
  std::vector<api::ClothWeight> weights;
  for (size_t v = 0; v < m.vertices.size(); ++v) {
    api::ClothWeight w;  // the head alone
    if (std::hypot(m.flat[v].x, m.flat[v].y) > cap) {
      const sheet::Weight near = sheet::weight_by_nearness(m.vertices[v], segments);
      for (int k = 0; k < 4; ++k) {
        w.bone[k] = near.bone[k];
        w.weight[k] = near.weight[k];
      }
    }
    weights.push_back(w);
  }
  api::ClothMesh mesh{vertices, normals, uv, m.triangles, coefficients};
  mesh.bones = bones;
  mesh.weights = weights;
  const phasmo::Vector3 none{};
  const api::ClothSphere spheres[] = {
      {head, shape.headRadius, {0, kHeadCentreAboveBone, 0}},
      {bone(B::Chest()), kChest, none},
      {bone(B::Spine()), kSpine, none},
      {bone(B::Hips()), kHips, none},
      {bone(B::LeftUpperArm()), kArm, none},
      {bone(B::RightUpperArm()), kArm, none},
      {bone(B::Neck()), kNeck, none},
      {bone(B::LeftShoulder()), kShoulder, none},
      {bone(B::RightShoulder()), kShoulder, none},
      {bone(B::LeftLowerArm()), kForearm, none},
      {bone(B::RightLowerArm()), kForearm, none},
      {bone(B::LeftUpperLeg()), kThigh, none},
      {bone(B::RightUpperLeg()), kThigh, none},
      {bone(B::LeftLowerLeg()), kShin, none},
      {bone(B::RightLowerLeg()), kShin, none},
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
  // The sheet is laid over the body as it stands: the standing clip's first pose, posed now, not whatever pose the copy
  // was taken in (user, 2026-11-09: the sheet sat off the head and the arms came out of it).
  if (api::play_state(g.animator, kStandStates[0])) {
    g.playing = kStandStates[0];
    api::pose_now(g.animator);
  }
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

// Blends into `state` from the one playing over kBlendSeconds (user, 2026-11-09: the clips cut from one to the next),
// or plays it outright when nothing plays yet.
void blend_to(const char* state) {
  if (!check()) return;
  const float from = seconds_of(g.playing);
  const bool played = from > 0.0f ? api::fade_to_state(g.animator, state, kBlendSeconds / from)
                                  : api::play_state(g.animator, state);
  if (played) g.playing = state;
}

void play(Move m, uint32_t variant) { blend_to(state_for(m, variant)); }

void play_act(Act a) { blend_to(state_of(a)); }

bool ready() { return check(); }

}  // namespace pm::custom::squatter::figure
