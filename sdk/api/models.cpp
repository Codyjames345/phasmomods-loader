#include "models.h"

#include "Animator.h"
#include "Input.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Player.h"
#include "Scene.h"
#include "UnityAssets.h"
#include "UnityAudio.h"
#include "UnityCloth.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "runtime.h"

namespace pm::api {
namespace {

// The Resources path of the prefab the game spawns every investigator from (LocalPCPlayer.CreateNetworkedPlayer loads
// it with Resources.Load; sdk/systems/scene.md, "The investigator's body").
constexpr const char* kPlayerPrefab = "PCPlayer";
constexpr int kMaxDepth = 64;

bool fail(std::string* why, const char* reason) {
  if (why) *why = reason;
  return false;
}

void* game_object_of(void* component) { return component ? phasmo::Component::get_gameObject(component) : nullptr; }

void each_transform(void* transform, int depth, std::vector<void*>& out) {
  if (!transform || depth > kMaxDepth) return;
  out.push_back(transform);
  const int n = phasmo::Transform::get_childCount(transform);
  for (int i = 0; i < n; ++i) each_transform(phasmo::Transform::GetChild(transform, i), depth + 1, out);
}

size_t count_of(const std::vector<void*>& transforms, void* type) {
  size_t n = 0;
  for (void* t : transforms) n += RefArray{phasmo::GameObject::GetComponents(game_object_of(t), type)}.size();
  return n;
}

// Takes every script, sound, collider and rigidbody off, in that order (a script may need a collider, a collider a
// rigidbody), twice, since a component another requires only comes off once that one is gone. True when no script is
// left.
bool strip(void* root) {
  std::vector<void*> transforms;
  each_transform(phasmo::GameObject::get_transform(root), 0, transforms);
  void* const scripts = phasmo::MonoBehaviour::type.type_object();
  void* const kinds[] = {scripts, phasmo::AudioSource::type.type_object(), phasmo::Collider::type.type_object(),
                         phasmo::Rigidbody::type.type_object()};
  for (void* kind : kinds)
    if (!kind) return false;
  for (int pass = 0; pass < 2; ++pass)
    for (void* kind : kinds)
      for (void* t : transforms) {
        const RefArray parts{phasmo::GameObject::GetComponents(game_object_of(t), kind)};
        for (size_t i = 0; i < parts.size(); ++i)
          if (void* part = parts[i]) phasmo::Object::DestroyImmediate(part, false);
      }
  return count_of(transforms, scripts) == 0;
}

// Removes the root's children that neither hold the skeleton nor draw anything: the prefab's rigs, aiming targets
// and sockets, which mean nothing without their scripts.
void prune(void* root, void* animator) {
  void* hips = phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Hips());
  if (!hips) return;
  void* const drawn = phasmo::Renderer::type.type_object();
  void* const top = phasmo::GameObject::get_transform(root);
  std::vector<void*> unused;
  const int n = phasmo::Transform::get_childCount(top);
  for (int i = 0; i < n; ++i) {
    void* child = phasmo::Transform::GetChild(top, i);
    if (!phasmo::Transform::IsChildOf(hips, child) && !phasmo::Component::GetComponentInChildren(child, drawn, true))
      unused.push_back(child);
  }
  for (void* child : unused) phasmo::Object::DestroyImmediate(game_object_of(child), false);
}

void* new_game_object(const char* name) {
  void* go = phasmo::GameObject::type.make();
  if (!go) return nullptr;
  phasmo::GameObject::ctor(go, make_string(name));
  return go;
}

// A managed array of `count` elements of `type`, filled from `values`, or null when the type did not resolve or the
// runtime lays its elements out at another size than T.
template <class T, class TypeWrapper>
void* filled_array(const TypeWrapper& type, std::span<const T> values) {
  void* raw = type.make_array(values.size());
  if (!raw || ValueArray{raw}.stride() != sizeof(T)) return nullptr;
  const Array<T> out{raw};
  for (size_t i = 0; i < values.size(); ++i) out[i] = values[i];
  return raw;
}

}  // namespace

void* copy_investigator_body(const char* name, std::string* why) {
  void* prefab = phasmo::Resources::Load(make_string(kPlayerPrefab), phasmo::GameObject::type.type_object());
  if (!prefab || !phasmo::GameObject::type.is(prefab)) return fail(why, "the PCPlayer prefab did not load"), nullptr;
  void* cosmetics = phasmo::Component::GetComponentInChildren(phasmo::GameObject::get_transform(prefab),
                                                              phasmo::PlayerCosmetics::type.type_object(), true);
  void* body = game_object_of(cosmetics ? phasmo::PlayerCosmetics::playerAnimator.get(cosmetics) : nullptr);
  if (!body) return fail(why, "the prefab has no body (PlayerCosmetics.playerAnimator)"), nullptr;
  // Copied under an inactive holder, so nothing of it wakes before it is stripped.
  void* holder = new_game_object(name);
  if (!holder) return fail(why, "no GameObject could be made"), nullptr;
  phasmo::GameObject::SetActive(holder, false);
  void* copy = phasmo::Object::Instantiate(body, phasmo::GameObject::get_transform(holder), false);
  if (!copy || !strip(copy)) {
    phasmo::Object::Destroy(holder, 0.0f);
    return fail(why, copy ? "a script would not come off the copy" : "the body could not be copied"), nullptr;
  }
  phasmo::Object::set_name(copy, make_string(name));
  void* transform = phasmo::GameObject::get_transform(copy);
  if (void* animator = phasmo::Component::GetComponent(transform, phasmo::Animator::type.type_object()))
    prune(copy, animator);
  phasmo::Transform::SetParent(transform, nullptr, false);
  phasmo::Vector3 origin{};
  phasmo::Transform::set_position(transform, &origin);
  phasmo::GameObject::SetActive(copy, true);
  phasmo::Object::Destroy(holder, 0.0f);
  return copy;
}

bool use_controller(void* animator, void* controller) {
  if (!alive(animator) || !alive(controller)) return false;
  phasmo::Animator::set_runtimeAnimatorController(animator, controller);
  phasmo::Animator::set_applyRootMotion(animator, false);
  phasmo::Animator::set_cullingMode(animator, phasmo::AnimatorCullingMode::AlwaysAnimate());
  return true;
}

bool play_state(void* animator, const char* state) {
  if (!alive(animator) || !state || !phasmo::Animator::get_isInitialized(animator)) return false;
  const int32_t hash = phasmo::Animator::StringToHash(make_string(state));
  if (!phasmo::Animator::HasState(animator, 0, hash)) return false;
  phasmo::Animator::Play(animator, hash, 0, 0.0f);
  return true;
}

bool controller_has_states(void* controller, std::span<const char* const> states, std::string* missing) {
  if (!alive(controller)) return fail(missing, "no controller");
  void* probe = new_game_object("Controller check");
  void* animator = probe ? phasmo::GameObject::AddComponent(probe, phasmo::Animator::type.type_object()) : nullptr;
  bool ok = animator != nullptr;
  if (!ok) fail(missing, "no Animator could be made to check it");
  if (ok) {
    phasmo::Animator::set_runtimeAnimatorController(animator, controller);
    if (!phasmo::Animator::get_isInitialized(animator)) phasmo::Animator::Rebind(animator, true);
    ok = phasmo::Animator::get_isInitialized(animator);
    if (!ok) fail(missing, "the Animator holding it did not initialise");
  }
  for (size_t i = 0; ok && i < states.size(); ++i)
    if (!states[i] || !phasmo::Animator::HasState(animator, 0, phasmo::Animator::StringToHash(make_string(states[i])))) {
      ok = false;
      if (missing) *missing = states[i] ? states[i] : "(null)";
    }
  if (probe) phasmo::Object::DestroyImmediate(probe, false);
  return ok;
}

void* two_sided_material(void* like, phasmo::Color colour, std::string* why) {
  if (!alive(like)) return fail(why, "no material to take the shader from"), nullptr;
  if (!phasmo::LitShader::BaseColor.available() || !phasmo::LitShader::Cull.available())
    return fail(why, "the Lit shader's properties did not resolve"), nullptr;
  void* shader = phasmo::Material::get_shader(like);
  void* material = shader ? phasmo::Material::type.make() : nullptr;
  if (!material) return fail(why, "no material could be made"), nullptr;
  phasmo::Material::ctor_Shader(material, shader);
  if (!phasmo::Material::HasProperty(material, phasmo::LitShader::BaseColor.id()) ||
      !phasmo::Material::HasProperty(material, phasmo::LitShader::Cull.id())) {
    phasmo::Object::Destroy(material, 0.0f);
    return fail(why, "the shader is not the Lit shader"), nullptr;
  }
  phasmo::Color c = colour;
  phasmo::Material::SetColorImpl(material, phasmo::LitShader::BaseColor.id(), &c);
  phasmo::Material::SetFloatImpl(material, phasmo::LitShader::Cull.id(), static_cast<float>(phasmo::CullMode::Off()));
  return material;
}

void* lit_material_of(void* gameObject) {
  void* renderer = alive(gameObject) ? phasmo::Component::GetComponentInChildren(
                                           phasmo::GameObject::get_transform(gameObject),
                                           phasmo::Renderer::type.type_object(), true)
                                     : nullptr;
  return renderer ? phasmo::Renderer::get_sharedMaterial(renderer) : nullptr;
}

bool add_cloth(void* gameObject, const ClothMesh& mesh, void* material, const ClothSettings& settings,
               std::span<const ClothSphere> spheres, ClothParts& out, std::string* why) {
  out = ClothParts{};
  if (!alive(gameObject) || !alive(material)) return fail(why, "no GameObject or no material");
  if (!phasmo::Cloth::available() || !phasmo::Cloth::set_coefficients.available() ||
      !phasmo::Cloth::set_capsuleColliders.available())
    return fail(why, "Cloth is not available in this game build");
  if (!spheres.empty() && !phasmo::Layers::IgnoreRaycast.available())
    return fail(why, "the Ignore Raycast layer did not resolve");
  const size_t n = mesh.vertices.size();
  if (!n || mesh.normals.size() != n || mesh.uv.size() != n || mesh.coefficients.size() != n ||
      mesh.triangles.empty() || mesh.triangles.size() % 3)
    return fail(why, "the mesh's parts disagree in length");
  for (const int32_t index : mesh.triangles)
    if (index < 0 || static_cast<size_t>(index) >= n) return fail(why, "a triangle indexes past the vertices");

  // Every vertex follows the GameObject itself, its one bone, whose bind pose is the identity.
  std::vector<phasmo::BoneWeight> weights(n, phasmo::BoneWeight{1, 0, 0, 0, 0, 0, 0, 0});
  phasmo::Matrix4x4 identity{};
  identity.m[0] = identity.m[5] = identity.m[10] = identity.m[15] = 1;
  void* const vertices = filled_array(phasmo::Vector3::type, mesh.vertices);
  void* const normals = filled_array(phasmo::Vector3::type, mesh.normals);
  void* const uv = filled_array(pm::api::Type<pm::map::Vector2>{}, mesh.uv);
  void* const indices = filled_array(pm::api::Type<pm::map::Int32>{}, mesh.triangles);
  void* const boneWeights = filled_array(phasmo::BoneWeight::type, std::span<const phasmo::BoneWeight>(weights));
  void* const bindposes = filled_array(phasmo::Matrix4x4::type, std::span<const phasmo::Matrix4x4>(&identity, 1));
  void* const coefficients = filled_array(pm::api::Type<pm::map::Vector2>{}, mesh.coefficients);
  void* const bones = phasmo::Transform::type.make_array(1);
  void* const materials = phasmo::Material::type.make_array(1);
  if (!vertices || !normals || !uv || !indices || !boneWeights || !bindposes || !coefficients || !bones || !materials)
    return fail(why, "an array type did not resolve or has another layout");
  void* const anchor = phasmo::GameObject::get_transform(gameObject);
  RefArray{bones}.set(0, anchor);
  RefArray{materials}.set(0, material);

  void* built = phasmo::Mesh::type.make();
  if (!built) return fail(why, "no Mesh could be made");
  phasmo::Mesh::ctor(built);
  out.mesh = built;
  phasmo::Mesh::set_vertices(built, vertices);
  phasmo::Mesh::set_normals(built, normals);
  phasmo::Mesh::set_uv(built, uv);
  phasmo::Mesh::set_boneWeights(built, boneWeights);
  phasmo::Mesh::set_bindposes(built, bindposes);
  phasmo::Mesh::set_triangles(built, indices);

  void* renderer = phasmo::GameObject::AddComponent(gameObject, phasmo::SkinnedMeshRenderer::type.type_object());
  if (!renderer) {
    destroy_cloth(out);
    return fail(why, "no SkinnedMeshRenderer could be added");
  }
  phasmo::SkinnedMeshRenderer::set_bones(renderer, bones);
  phasmo::SkinnedMeshRenderer::set_rootBone(renderer, anchor);
  phasmo::SkinnedMeshRenderer::set_sharedMesh(renderer, built);
  phasmo::SkinnedMeshRenderer::set_updateWhenOffscreen(renderer, true);
  phasmo::Renderer::set_sharedMaterials(renderer, materials);

  void* cloth = phasmo::GameObject::AddComponent(gameObject, phasmo::Cloth::type.type_object());
  if (!cloth) {
    phasmo::Object::Destroy(renderer, 0.0f);
    destroy_cloth(out);
    return fail(why, "no Cloth could be added");
  }
  out.cloth = cloth;
  phasmo::Cloth::set_useGravity(cloth, settings.useGravity);
  phasmo::Cloth::set_bendingStiffness(cloth, settings.bendingStiffness);
  phasmo::Cloth::set_stretchingStiffness(cloth, settings.stretchingStiffness);
  phasmo::Cloth::set_damping(cloth, settings.damping);
  phasmo::Cloth::set_coefficients(cloth, coefficients);

  std::vector<void*> capsules;
  for (const ClothSphere& s : spheres) {
    if (!alive(s.bone)) continue;
    void* go = new_game_object("Cloth collider");
    void* capsule = go ? phasmo::GameObject::AddComponent(go, phasmo::CapsuleCollider::type.type_object()) : nullptr;
    if (!capsule) {
      if (go) phasmo::Object::Destroy(go, 0.0f);
      continue;
    }
    out.colliders.emplace_back(go);
    phasmo::GameObject::set_layer(go, phasmo::Layers::IgnoreRaycast.index());
    void* t = phasmo::GameObject::get_transform(go);
    phasmo::Transform::SetParent(t, s.bone, false);
    const phasmo::Vector3 at = phasmo::Transform::get_position(s.bone);
    phasmo::Vector3 there{at.x + s.offset.x, at.y + s.offset.y, at.z + s.offset.z};
    phasmo::Transform::set_position(t, &there);
    phasmo::CapsuleCollider::set_radius(capsule, s.radius);
    phasmo::CapsuleCollider::set_height(capsule, 0.0f);  // shorter than its diameter: a sphere
    capsules.push_back(capsule);
  }
  void* list = phasmo::CapsuleCollider::type.make_array(capsules.size());
  for (size_t i = 0; list && i < capsules.size(); ++i) RefArray{list}.set(i, capsules[i]);
  if (list) phasmo::Cloth::set_capsuleColliders(cloth, list);
  return true;
}

void destroy_cloth(ClothParts& parts) {
  for (Held& go : parts.colliders)
    if (alive(go)) phasmo::Object::Destroy(go, 0.0f);
  if (alive(parts.mesh)) phasmo::Object::Destroy(parts.mesh, 0.0f);
  parts = ClothParts{};
}

}  // namespace pm::api
