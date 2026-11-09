#include "squatter_figure.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>

#include "Animator.h"
#include "FinalIK.h"
#include "Scene.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "bundles.h"
#include "held.h"
#include "inertia.h"
#include "log.h"
#include "models.h"
#include "quat.h"
#include "runtime.h"
#include "sheet_mesh.h"
#include "squatter_ragdoll.h"

namespace pm::custom::squatter::figure {
namespace {

constexpr const char* kBundleFile = "squatter.bundle";
constexpr const char* kController = "assets/squatter.controller";

// How the sheet sits and moves: starting values, judged with the user in game.
constexpr float kHeadCentreAboveBone = 0.09f;  // the head sphere's centre above the head bone, metres
// A heavier, stiffer cloth than Unity's defaults, as a bedsheet is (user, 2026-11-09: thicker and stiffer): it bends
// less and its motion dies away sooner. Starting values, judged with the user in game.
const api::ClothSettings kCloth{true, 0.5f, 1.0f, 0.4f};
// The sheet material's smoothness: matte (user, 2026-11-10).
constexpr float kSheetSmoothness = 0.0f;
// The spheres the sheet rests on besides the head, so the body never shows through it (user, 2026-11-09: the sheet was
// too thin and the legs and arms came through): a little larger than the body there, which gives the cloth its
// thickness, on the torso, neck, shoulders, both arms and both legs. Starting values, judged with the user in game.
constexpr float kChest = 0.24f, kSpine = 0.22f, kHips = 0.22f, kNeck = 0.09f, kShoulder = 0.10f;
constexpr float kArm = 0.10f, kForearm = 0.08f, kThigh = 0.12f, kShin = 0.09f;
// The forearms and hands push the sheet out where they go, a hand reaching for a door's handle among them: spheres
// along each forearm, at the wrist and over the hand. Starting values.
constexpr float kWrist = 0.06f, kHand = 0.07f, kPalmAlong = 0.35f;
// The floor the sheet rests on: Cloth collides only with spheres and capsules, so a sphere this large lies under the
// feet, its top level with them. Across the sheet's spread (about 0.6 m) its surface falls only a few centimetres, so the
// hem settles on the floor rather than through it (user, 2026-11-09: it showed through the ceiling below).
constexpr float kFloorRadius = 3.0f;
// The investigator body's parts left drawing under the sheet, by their GameObject's name in the PCPlayer prefab
// (sdk/systems/scene.md): the head alone (CC_Base_Body is the head's mesh alone), its eyes, teeth, tongue and hair. The
// legs clipped through the sheet, so the trousers and boots are hidden with the jacket, shirt, hands and every cosmetic
// slot (user, 2026-11-09).
constexpr const char* kShownParts[] = {"CC_Base_Body",  "CC_Base_Eye",    "CC_Base_EyeOcclusion", "CC_Base_TearLine",
                                       "CC_Base_Teeth", "CC_Base_Tongue", "Hair", "Extra Hair Renderer 1",
                                       "Extra Hair Renderer 2"};

api::Held g_bundle;
bool g_failed = false;  // a create that failed, logged once, until destroy()

struct Figure {
  api::Held root, animator, sheet, material;
  api::ClothParts cloth;
  const char* playing = nullptr;  // the state it plays, which a cross-fade blends from
  api::Held fullBody, lookAt;      // Final IK: the body's FullBodyBipedIK, and the head's LookAtIK
  api::Held order;                 // Final IK's IKExecutionOrder, whose LateUpdate is the pass after the animation
  bool falling = false;            // its ragdoll has it, not its Animator or its IK
  phasmo::Quaternion fallChest{0, 0, 0, 1};  // the chest's rotation as the fall began
  phasmo::Vector3 fallFront{0, 0, 1};        // the body's front as the fall began
} g;

AfterAnimation g_after = nullptr;
std::chrono::steady_clock::time_point g_lastPass{};  // when the pass after the animation last ran; zero before the first
PoseInertia g_inertia;  // what is left of the old pose after a change of clip
// What is left of the hips' old place after a get-up starts, against their parent, dying away as the rotations do.
phasmo::Vector3 g_hipsOffset{};
float g_hipsSince = 0.0f, g_hipsSeconds = 0.0f;

// Seconds on the figure's own clock, from the first call: small enough that a float keeps a millisecond for hours.
float seconds() {
  static const auto start = std::chrono::steady_clock::now();
  return std::chrono::duration<float>(std::chrono::steady_clock::now() - start).count();
}

// The humanoid bones the figure's pose is kept for: every one the map has.
constexpr size_t kPoseBones = 19;
std::array<int32_t, kPoseBones> pose_bones() {
  namespace B = phasmo::HumanBodyBones;
  return {B::Hips(),          B::Spine(),        B::Chest(),        B::Neck(),          B::Head(),
          B::LeftShoulder(),  B::RightShoulder(), B::LeftUpperArm(), B::RightUpperArm(), B::LeftLowerArm(),
          B::RightLowerArm(), B::LeftHand(),     B::RightHand(),    B::LeftUpperLeg(),  B::RightUpperLeg(),
          B::LeftLowerLeg(),  B::RightLowerLeg(), B::LeftFoot(),     B::RightFoot()};
}

void* bone(int32_t which);

// Each pose bone's rotation against its parent now; identity for one the model lacks.
std::array<phasmo::Quaternion, kPoseBones> read_pose() {
  std::array<phasmo::Quaternion, kPoseBones> pose;
  const auto bones = pose_bones();
  for (size_t i = 0; i < kPoseBones; ++i) {
    void* b = bone(bones[i]);
    pose[i] = b ? phasmo::Transform::get_localRotation(b) : phasmo::Quaternion{0, 0, 0, 1};
  }
  return pose;
}

void forget_inertia() {
  g_inertia.clear();
  g_hipsSeconds = 0.0f;
}

// Puts what is left of the old pose before each bone's animated rotation, and of the hips' old place on theirs, while
// any is left.
void inertialize() {
  const float now = seconds();
  if (g_hipsSeconds > 0.0f && now - g_hipsSince < g_hipsSeconds) {
    if (void* hips = bone(phasmo::HumanBodyBones::Hips())) {
      const float left = inertia_left((now - g_hipsSince) / g_hipsSeconds);
      phasmo::Vector3 p = phasmo::Transform::get_localPosition(hips);
      p = {p.x + g_hipsOffset.x * left, p.y + g_hipsOffset.y * left, p.z + g_hipsOffset.z * left};
      phasmo::Transform::set_localPosition(hips, &p);
    }
  }
  if (!g_inertia.active(now)) return;
  const auto bones = pose_bones();
  for (size_t i = 0; i < kPoseBones; ++i) {
    void* b = bone(bones[i]);
    if (!b) continue;
    const phasmo::Quaternion shown = qmul(g_inertia.offset(i, now), phasmo::Transform::get_localRotation(b));
    phasmo::Transform::set_localRotation(b, &shown);
  }
}

// How much of the look the spine takes, against the head's whole turn, and how far past straight ahead it eases off:
// starting values, judged with the user in game.
constexpr float kLookBody = 0.3f, kLookClamp = 0.5f;

void fail(const char* step, const std::string& why) {
  if (!g_failed) PM_LOG("CustomGhosts: the Squatter's figure could not be built (%s): %s", step, why.c_str());
  g_failed = true;
}

// Destroys the figure's own assets, and its objects when the scene has not already.
void clear() {
  ragdoll::destroy();
  api::destroy_cloth(g.cloth);
  if (api::alive(g.material)) phasmo::Object::Destroy(g.material, 0.0f);
  if (api::alive(g.root)) phasmo::Object::Destroy(g.root, 0.0f);
  g = Figure{};
  forget_inertia();
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
  // Matte, as a cotton sheet is (user, 2026-11-10), and as the game's own untextured fabric sets it: no smoothness. A
  // build without the property keeps the shader's sheen.
  if (!api::set_smoothness(material, kSheetSmoothness)) PM_LOG("squatter: the sheet's material has no smoothness to set");

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
  // A sphere's offset from its bone, in the world as the body stands: `share` of the line from joint `from` to joint
  // `to`. On `from`'s bone it lies along the line; on `to`'s, past it (the palm past the wrist).
  const auto along = [&](int32_t from, int32_t to, float share) {
    const phasmo::Vector3 a = phasmo::Transform::get_position(bone(from)), b = phasmo::Transform::get_position(bone(to));
    return phasmo::Vector3{(b.x - a.x) * share, (b.y - a.y) * share, (b.z - a.z) * share};
  };
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
      {bone(B::LeftLowerArm()), kWrist, along(B::LeftLowerArm(), B::LeftHand(), 0.5f)},
      {bone(B::RightLowerArm()), kWrist, along(B::RightLowerArm(), B::RightHand(), 0.5f)},
      {bone(B::LeftHand()), kWrist, none},
      {bone(B::RightHand()), kWrist, none},
      {bone(B::LeftHand()), kHand, along(B::LeftLowerArm(), B::LeftHand(), kPalmAlong)},
      {bone(B::RightHand()), kHand, along(B::RightLowerArm(), B::RightHand(), kPalmAlong)},
      // The figure's root is at its feet, and follows the ghost's position each frame (place).
      {phasmo::GameObject::get_transform(g.root), kFloorRadius, {0, -kFloorRadius, 0}}};
  if (!api::add_cloth(anchor, mesh, material, kCloth, spheres, g.cloth, &why)) return fail("cloth", why), false;
  return true;
}

// The figure's inverse kinematics (Final IK, which the game ships), as games bend an animated body to the world: a
// full-body solver whose hands reach for what the figure touches and whose body follows them, a grounder on it that
// sets each foot on the ground under it and lowers the hips to the lower foot on stairs and slopes, and a look-at on
// the spine and head. Final IK's execution order runs the full body, then the look-at, after the Animator, as Final IK
// asks when two solvers bend the same bones. The hands and head are at no weight until reach or look_at asks.
bool build_ik() {
  namespace B = phasmo::HumanBodyBones;
  void* rootT = phasmo::GameObject::get_transform(g.root);
  void* refs = phasmo::BipedReferences::type.make();
  void* spine = phasmo::Transform::type.make_array(2);
  void* none = phasmo::Transform::type.make_array(0);
  if (!refs || !spine || !none) return fail("ik", "no BipedReferences could be made"), false;
  phasmo::BipedReferences::ctor(refs);
  api::RefArray{spine}.set(0, bone(B::Spine()));
  api::RefArray{spine}.set(1, bone(B::Chest()));
  using R = phasmo::BipedReferences;
  R::root.set(refs, rootT);
  R::pelvis.set(refs, bone(B::Hips()));
  R::leftThigh.set(refs, bone(B::LeftUpperLeg()));
  R::leftCalf.set(refs, bone(B::LeftLowerLeg()));
  R::leftFoot.set(refs, bone(B::LeftFoot()));
  R::rightThigh.set(refs, bone(B::RightUpperLeg()));
  R::rightCalf.set(refs, bone(B::RightLowerLeg()));
  R::rightFoot.set(refs, bone(B::RightFoot()));
  R::leftUpperArm.set(refs, bone(B::LeftUpperArm()));
  R::leftForearm.set(refs, bone(B::LeftLowerArm()));
  R::leftHand.set(refs, bone(B::LeftHand()));
  R::rightUpperArm.set(refs, bone(B::RightUpperArm()));
  R::rightForearm.set(refs, bone(B::RightLowerArm()));
  R::rightHand.set(refs, bone(B::RightHand()));
  R::head.set(refs, bone(B::Head()));
  R::spine.set(refs, spine);
  R::eyes.set(refs, none);

  void* body = phasmo::GameObject::AddComponent(g.root, phasmo::FullBodyBipedIK::type.type_object());
  if (!body) return fail("ik", "no FullBodyBipedIK could be added"), false;
  phasmo::FullBodyBipedIK::SetReferences(body, refs, nullptr);
  void* solver = phasmo::FullBodyBipedIK::solver.get(body);
  if (!solver) return fail("ik", "the full-body solver is missing"), false;
  phasmo::IKSolver::Initiate(solver, rootT);
  g.fullBody = body;

  void* grounder = phasmo::GameObject::AddComponent(g.root, phasmo::GrounderFBBIK::type.type_object());
  void* grounding = grounder ? phasmo::Grounder::solver.get(grounder) : nullptr;
  if (!grounding) return fail("ik", "no GrounderFBBIK could be added"), false;
  phasmo::GrounderFBBIK::ik.set(grounder, body);
  int32_t ground = 0;
  for (const int32_t layer : {phasmo::Layers::Default.index(), phasmo::Layers::Floor.index(),
                              phasmo::Layers::DefaultNoNavigation.index(), phasmo::Layers::StaticProps.index()})
    if (layer >= 0 && layer < 32) ground |= 1 << layer;
  phasmo::Grounding::layers.set(grounding, ground);

  void* look = phasmo::GameObject::AddComponent(g.root, phasmo::LookAtIK::type.type_object());
  void* lookSolver = look ? phasmo::LookAtIK::solver.get(look) : nullptr;
  if (!lookSolver) return fail("ik", "no LookAtIK could be added"), false;
  if (!phasmo::IKSolverLookAt::SetChain(lookSolver, spine, bone(B::Head()), none, rootT))
    return fail("ik", "the look-at chain is not valid"), false;
  phasmo::IKSolverLookAt::headWeight.set(lookSolver, 1.0f);
  phasmo::IKSolverLookAt::bodyWeight.set(lookSolver, kLookBody);
  phasmo::IKSolverLookAt::clampWeight.set(lookSolver, kLookClamp);
  phasmo::IKSolver::IKPositionWeight.set(lookSolver, 0.0f);
  g.lookAt = look;

  void* order = phasmo::GameObject::AddComponent(g.root, phasmo::IKExecutionOrder::type.type_object());
  void* list = phasmo::IK::type.make_array(2);
  if (!order || !list) return fail("ik", "no IKExecutionOrder could be added"), false;
  api::RefArray{list}.set(0, body);
  api::RefArray{list}.set(1, look);
  phasmo::IKExecutionOrder::IKComponents.set(order, list);
  phasmo::IKExecutionOrder::animator.set(order, g.animator);
  g.order = order;
  return true;
}

// Final IK's LateUpdate, for every model that has an IKExecutionOrder: on the figure's own, the pass after the
// animation runs first, so the IK bends the pose the pass leaves.
void detour_ik_late_update(api::Original<phasmo::IKExecutionOrder::LateUpdate> original, void* order) {
  if (is_own_ik(order, g.order)) {
    const auto now = std::chrono::steady_clock::now();
    const float dt = g_lastPass == decltype(g_lastPass){} ? 0.0f : std::chrono::duration<float>(now - g_lastPass).count();
    g_lastPass = now;
    if (!g.falling) inertialize();
    if (g_after) g_after(dt);
    if (!ik_runs(order, g.order, g.falling)) return;  // physics has the body: no IK bends it
  }
  original(order);
}

// Final IK's Update, for every model with an IKExecutionOrder: it puts the solvers' bones back in their stored pose each
// frame (IKExecutionOrder.Update's note in the map), which would stand a falling body back up, so it is skipped for the
// figure's own while it falls.
void detour_ik_update(api::Original<phasmo::IKExecutionOrder::Update> original, void* order) {
  if (ik_runs(order, g.order, g.falling)) original(order);
}

// The full-body solver's effector for `hand`, or null.
void* effector_of(int hand) {
  void* solver = api::alive(g.fullBody) ? phasmo::FullBodyBipedIK::solver.get(g.fullBody) : nullptr;
  if (!solver) return nullptr;
  return hand == 0 ? phasmo::IKSolverFullBodyBiped::get_leftHandEffector(solver)
                   : phasmo::IKSolverFullBodyBiped::get_rightHandEffector(solver);
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
  // Root motion on, so the Animator works out each clip's own step and turn (clip_step, clip_turn), which a roll, a trip
  // and a turn on the spot move the Squatter by (user, 2026-11-09: it rolled in place). Every client puts the figure back
  // where the ghost is, facing its own way, each frame (place), so the Animator's own move of it never shows.
  phasmo::Animator::set_applyRootMotion(g.animator, true);
  place(at, 0.0f);
  // The sheet is laid over the body as it stands: the standing clip's first pose, posed now, not whatever pose the copy
  // was taken in (user, 2026-11-09: the sheet sat off the head and the arms came out of it).
  if (api::play_state(g.animator, kStandStates[0])) {
    g.playing = kStandStates[0];
    api::pose_now(g.animator);
  }
  // What the sheet covers is never drawn, as games hide what a costume covers, so no arm or hand ever shows through it
  // (user, 2026-11-09); its bones still shape the sheet. The head still draws, since it is a person under a bedsheet
  // (user, 2026-11-09). The sheet, added next, draws.
  api::hide_renderers(g.root, kShownParts);
  if (!build_sheet() || !build_ik()) return clear();
  // The ragdoll a fall turns physical, built standing; without it (logged) a fall plays its clip as recorded.
  ragdoll::build(g.animator);
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

// Changes to `state` by inertialization (user, 2026-11-09: no pose snaps or floats between clips): the state plays at
// once, posed this frame, and the old pose's difference from its first pose dies away in the pass after the animation.
void blend_to(const char* state) {
  if (!check()) return;
  const auto from = read_pose();
  if (!api::play_state(g.animator, state)) return;
  api::pose_now(g.animator);
  g.playing = state;
  const auto to = read_pose();
  g_inertia.begin(from, to, seconds());
}

void play(Move m, uint32_t variant) { blend_to(state_for(m, variant)); }

void play_act(Act a) { blend_to(state_of(a)); }

void set_pace(Move m, float speed) {
  if (!check()) return;
  static const int32_t kSpeed = phasmo::Animator::StringToHash(api::make_string("Speed"));
  static const int32_t kPlayRate = phasmo::Animator::StringToHash(api::make_string("PlayRate"));
  phasmo::Animator::SetFloatID(g.animator, kSpeed, std::max(speed, 0.0f));
  phasmo::Animator::SetFloatID(g.animator, kPlayRate, play_rate(m, speed));
}

Hand nearer_hand(phasmo::Vector3 at) {
  if (!check()) return Hand::Right;
  const auto away = [&](int32_t which) {
    void* b = bone(which);
    if (!b) return 1e9f;
    const phasmo::Vector3 p = phasmo::Transform::get_position(b);
    return std::hypot(p.x - at.x, p.z - at.z);
  };
  return away(phasmo::HumanBodyBones::LeftUpperArm()) < away(phasmo::HumanBodyBones::RightUpperArm()) ? Hand::Left
                                                                                                      : Hand::Right;
}

void reach(Hand hand, phasmo::Vector3 at, float weight) {
  if (!check()) return;
  for (int i = 0; i < 2; ++i) {
    void* effector = effector_of(i);
    if (!effector) continue;
    const bool reaching = static_cast<int>(hand) == i && weight > 0.0f;
    if (reaching) phasmo::IKEffector::position.set(effector, at);
    phasmo::IKEffector::positionWeight.set(effector, reaching ? std::clamp(weight, 0.0f, 1.0f) : 0.0f);
  }
}

void look_at(phasmo::Vector3 at, float weight) {
  if (!check() || !api::alive(g.lookAt)) return;
  void* solver = phasmo::LookAtIK::solver.get(g.lookAt);
  if (!solver) return;
  if (weight > 0.0f) phasmo::IKSolver::IKPosition.set(solver, at);
  phasmo::IKSolver::IKPositionWeight.set(solver, std::clamp(weight, 0.0f, 1.0f));
}

void play_clip(const char* state) { blend_to(state); }

void snap_to(const char* state) {
  if (!check()) return;
  if (api::play_state(g.animator, state)) {
    g.playing = state;
    api::pose_now(g.animator);
    forget_inertia();
  }
}

float clip_turn() {
  if (!check()) return 0.0f;
  const phasmo::Quaternion q = phasmo::Animator::get_deltaRotation(g.animator);
  return 2.0f * std::atan2(q.y, q.w);
}

phasmo::Vector3 clip_step() { return check() ? phasmo::Animator::get_deltaPosition(g.animator) : phasmo::Vector3{}; }

void* head() { return check() ? bone(phasmo::HumanBodyBones::Head()) : nullptr; }

bool ready() { return check(); }

bool fall(phasmo::Vector3 velocity) {
  if (!check() || !ragdoll::built()) return false;
  forget_inertia();
  void* chest = bone(phasmo::HumanBodyBones::Chest());
  const phasmo::Quaternion facing = phasmo::Transform::get_rotation(phasmo::GameObject::get_transform(g.root));
  g.fallChest = chest ? phasmo::Transform::get_rotation(chest) : facing;
  g.fallFront = qrot(facing, {0, 0, 1});
  phasmo::Behaviour::set_enabled(g.animator, false);  // it would set the bones over physics every frame
  ragdoll::set_physical(true, velocity);
  g.falling = true;
  return true;
}

bool falling() { return check() && g.falling; }

float fall_speed() { return falling() ? ragdoll::fastest() : 0.0f; }

Landing landing() {
  if (!check()) return {};
  void* hips = bone(phasmo::HumanBodyBones::Hips());
  void* head = bone(phasmo::HumanBodyBones::Head());
  void* chest = bone(phasmo::HumanBodyBones::Chest());
  Landing l;
  if (hips) l.hips = phasmo::Transform::get_position(hips);
  l.head = head ? phasmo::Transform::get_position(head) : l.hips;
  // The front it had, turned as far as the chest has turned since the fall began.
  l.front = chest ? qrot(qmul(phasmo::Transform::get_rotation(chest), qinv(g.fallChest)), g.fallFront) : g.fallFront;
  return l;
}

void rise(const char* state, phasmo::Vector3 at, float yaw, float blendSeconds) {
  if (!check()) return;
  void* hips = bone(phasmo::HumanBodyBones::Hips());
  const phasmo::Vector3 hipsAt = hips ? phasmo::Transform::get_position(hips) : at;
  const phasmo::Quaternion hipsTurn = hips ? phasmo::Transform::get_rotation(hips) : phasmo::Quaternion{0, 0, 0, 1};
  ragdoll::set_physical(false, {});
  g.falling = false;
  // The root moves under the body, which stays where it lies.
  place(at, yaw);
  if (hips) {
    phasmo::Transform::set_position(hips, &hipsAt);
    phasmo::Transform::set_rotation(hips, &hipsTurn);
  }
  const auto from = read_pose();
  const phasmo::Vector3 hipsFrom = hips ? phasmo::Transform::get_localPosition(hips) : phasmo::Vector3{};
  phasmo::Behaviour::set_enabled(g.animator, true);
  if (!api::play_state(g.animator, state)) return forget_inertia();
  api::pose_now(g.animator);
  g.playing = state;
  const auto to = read_pose();
  const phasmo::Vector3 hipsTo = hips ? phasmo::Transform::get_localPosition(hips) : phasmo::Vector3{};
  const float now = seconds();
  g_inertia.begin(from, to, now, blendSeconds);
  g_hipsOffset = {hipsFrom.x - hipsTo.x, hipsFrom.y - hipsTo.y, hipsFrom.z - hipsTo.z};
  g_hipsSince = now;
  g_hipsSeconds = blendSeconds;
}

void set_after_animation(AfterAnimation fn) {
  g_after = fn;
  g_lastPass = {};
}

bool create_hooks() {
  return api::hook<phasmo::IKExecutionOrder::LateUpdate>(&detour_ik_late_update, "IKExecutionOrder.LateUpdate",
                                                         api::Twins::CanonicalOnly) &&
         api::hook<phasmo::IKExecutionOrder::Update>(&detour_ik_update, "IKExecutionOrder.Update",
                                                     api::Twins::CanonicalOnly);
}

}  // namespace pm::custom::squatter::figure
