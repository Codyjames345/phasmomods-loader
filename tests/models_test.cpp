// Offline tests for the SDK's figure helpers (sdk/api/bundles.h, sdk/api/models.h) against the fake game: a mod's
// AssetBundle and its controller, a script-free copy of the investigator's body, states played by name, a two-sided
// material, and Cloth on a mesh the mod builds.
// Run: build\models_test.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "Animator.h"
#include "Camera.h"
#include "Input.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "Player.h"
#include "Scene.h"
#include "Transform.h"
#include "UnityAssets.h"
#include "UnityAudio.h"
#include "UnityCloth.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "UnityWorld.h"
#include "bundles.h"
#include "fake_game.h"
#include "game_api.h"
#include "models.h"
#include "runtime.h"

#include "check.h"

namespace {

namespace map = pm::map;
namespace fake = pm::fake;
namespace api = pm::api;
using phasmo::Vector2;
using phasmo::Vector3;

// The classes of the fake game's objects.
int kCamera, kLight, kListener;
int kGameObject, kTransform, kAnimator, kScript, kAudio, kCollider, kRigidbody, kRenderer, kSkinned, kCloth, kCapsule,
    kMaterial, kMesh, kController, kPlayerCosmetics, kVector3, kVector2, kInt32, kBoneWeight, kMatrix, kShader;

// A fake engine object: its class first, as a real one's.
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x60]{};
};

// The fake scene: each GameObject's transform, children, components and state; each component's GameObject.
struct Node {
  Obj transform{&kTransform};
  void* parent = nullptr;  // a GameObject
  std::vector<void*> children;  // GameObjects
  std::vector<void*> components;
  bool active = true;
  int layer = 0;
  std::string name;
  Vector3 position{};
  phasmo::Quaternion rotation{0, 0, 0, 1};
  Vector3 scale{1, 1, 1};
};
std::map<void*, Node> g_nodes;         // by GameObject
std::map<void*, void*> g_ownerOf;      // a component (transform included) -> its GameObject
std::vector<Obj*> g_owned;             // objects the fake world made, freed at the end of each test
std::set<void*> g_stuck;               // components DestroyImmediate will not take off (a script another requires)
std::vector<void*> g_destroyed;

void* klass_of(void* o) { return o ? *static_cast<void**>(o) : nullptr; }
Obj* make_obj(void* klass) {
  g_owned.push_back(new Obj{klass});
  return g_owned.back();
}
Node& node(void* go) {
  auto [it, made] = g_nodes.try_emplace(go);
  if (made) g_ownerOf[&it->second.transform] = go;
  return it->second;
}
void* go_new(const char* name) {
  void* go = make_obj(&kGameObject);
  node(go).name = name;
  return go;
}
void* add(void* go, void* klass) {
  void* c = make_obj(klass);
  node(go).components.push_back(c);
  g_ownerOf[c] = go;
  return c;
}
void attach(void* child, void* parent) {
  node(child).parent = parent;
  node(parent).children.push_back(child);
}
std::string text(void* managedString) { return api::to_utf8(managedString); }

// Every class a type entry resolves to, and the class of the System.Type the fake gives for it.
std::map<void*, void*> g_typeObjectClass;  // System.Type object -> class
void* class_of_type(void* typeObject) {
  for (auto& [t, k] : g_typeObjectClass)
    if (t == typeObject) return k;
  return nullptr;
}
bool is_a(void* component, void* klass) {
  if (klass_of(component) == klass) return true;
  // A SkinnedMeshRenderer is a Renderer, a script a MonoBehaviour, a capsule a Collider.
  return (klass == &kRenderer && klass_of(component) == &kSkinned) ||
         (klass == &kCollider && klass_of(component) == &kCapsule);
}

// The fake game's members.
void* f_get_transform(void* go, const void*) { return &node(go).transform; }
void* f_get_gameObject(void* component, const void*) { return g_ownerOf.count(component) ? g_ownerOf[component] : nullptr; }
int32_t f_childCount(void* t, const void*) { return static_cast<int32_t>(node(g_ownerOf[t]).children.size()); }
void* f_GetChild(void* t, int32_t i, const void*) {
  Node& n = node(g_ownerOf[t]);
  return i >= 0 && static_cast<size_t>(i) < n.children.size() ? &node(n.children[static_cast<size_t>(i)]).transform
                                                               : nullptr;
}
bool f_IsChildOf(void* t, void* parent, const void*) {
  for (void* go = g_ownerOf[t]; go; go = node(go).parent)
    if (&node(go).transform == parent) return true;
  return false;
}
// What happened to the copy, in order: its instantiation (under an active or an inactive parent), each component or
// object destroyed, each controller set (and whether its Animator was awake), and the moment it woke.
std::vector<std::string> g_events;
bool awake(void* go) {
  for (void* g = go; g; g = node(g).parent)
    if (!node(g).active) return false;
  return true;
}
void note_wake(void* go, bool was) {
  if (!was && awake(go)) g_events.push_back("wake " + node(go).name);
}
void f_SetParent(void* t, void* parent, bool, const void*) {
  void* go = g_ownerOf[t];
  const bool was = awake(go);
  if (void* old = node(go).parent) std::erase(node(old).children, go);
  node(go).parent = nullptr;
  if (parent) attach(go, g_ownerOf[parent]);
  note_wake(go, was);
}
void f_set_position(void* t, const Vector3* at, const void*) { node(g_ownerOf[t]).position = *at; }
Vector3 f_get_position(void* t, const void*) { return node(g_ownerOf[t]).position; }
phasmo::Quaternion f_get_rotation(void* t, const void*) { return node(g_ownerOf[t]).rotation; }
Vector3 f_get_lossyScale(void* t, const void*) { return node(g_ownerOf[t]).scale; }
void f_ctor(void* go, void* name, const void*) { node(go).name = text(name); }
void f_set_name(void* o, void* name, const void*) {
  if (g_nodes.count(o)) node(o).name = text(name);
}
void f_SetActive(void* go, bool on, const void*) {
  const bool was = awake(go);
  node(go).active = on;
  note_wake(go, was);
}
void f_set_layer(void* go, int32_t layer, const void*) { node(go).layer = layer; }
std::set<void*> g_hiddenRenderers;  // renderers set_enabled(false) switched off
void f_renderer_enabled(void* r, bool on, const void*) {
  if (on) g_hiddenRenderers.erase(r);
  else g_hiddenRenderers.insert(r);
}
void* f_get_name(void* o, const void*) {
  void* go = g_nodes.count(o) ? o : (g_ownerOf.count(o) ? g_ownerOf[o] : nullptr);
  return api::make_string(go ? node(go).name.c_str() : "");
}
void* f_GetComponents(void* go, void* type, const void*) {
  std::vector<void*> found;
  for (void* c : node(go).components)
    if (is_a(c, class_of_type(type))) found.push_back(c);
  void* arr = fake::array(sizeof(void*), found.size());
  for (size_t i = 0; i < found.size(); ++i) api::RefArray{arr}.set(i, found[i]);
  return arr;
}
void* f_GetComponent(void* component, void* type, const void*) {
  for (void* c : node(g_ownerOf[component]).components)
    if (is_a(c, class_of_type(type))) return c;
  return nullptr;
}
void* find_in(void* go, void* klass) {
  for (void* c : node(go).components)
    if (is_a(c, klass)) return c;
  for (void* child : node(go).children)
    if (void* c = find_in(child, klass)) return c;
  return nullptr;
}
void* f_GetComponentInChildren(void* component, void* type, bool, const void*) {
  return find_in(g_ownerOf[component], class_of_type(type));
}
void* f_AddComponent(void* go, void* type, const void*) { return add(go, class_of_type(type)); }
void remove_component(void* c) {
  void* go = g_ownerOf[c];
  std::erase(node(go).components, c);
}
void f_DestroyImmediate(void* o, bool) {
  if (g_stuck.count(o)) return;
  g_destroyed.push_back(o);
  g_events.push_back("destroy");
  if (g_ownerOf.count(o) && klass_of(o) != &kTransform) remove_component(o);
  if (g_nodes.count(o))
    if (void* parent = node(o).parent) std::erase(node(parent).children, o);
  fake::destroy(o);
}
void f_Destroy(void* o, float) {
  g_destroyed.push_back(o);
  fake::destroy(o);
}
// Instantiate copies the GameObject and its children with every component, under `parent`.
void* copy_of(void* go) {
  void* copy = go_new(node(go).name.c_str());
  for (void* c : node(go).components) add(copy, klass_of(c));
  node(copy).active = node(go).active;
  for (void* child : node(go).children) attach(copy_of(child), copy);
  return copy;
}
void* f_Instantiate(void* original, void* parent, bool, const void*) {
  g_events.push_back(std::string("instantiate under ") + (parent && awake(g_ownerOf[parent]) ? "awake" : "asleep"));
  void* copy = copy_of(original);
  if (parent) attach(copy, g_ownerOf[parent]);
  return copy;
}

// The body inside the fake PCPlayer prefab: a root with an Animator and a script, a skeleton under its hips, a
// renderer, and a rig of targets that draws nothing.
struct Prefab {
  void* prefab = nullptr;
  void* cosmetics = nullptr;
  void* body = nullptr;
};
Prefab g_prefab;
std::string g_loadedPath;
constexpr int32_t kPlayerAnimator = 0x10;

void* f_Load(void* path, void* type, const void*) {
  g_loadedPath = text(path);
  return g_loadedPath == "PCPlayer" && class_of_type(type) == &kGameObject ? g_prefab.prefab : nullptr;
}
void* f_GetBoneTransform(void* animator, int32_t bone, const void*) {
  if (bone != 0) return nullptr;  // the hips are 0 in this fake
  void* go = g_ownerOf[animator];
  for (void* child : node(go).children)
    if (node(child).name == "Hips") return &node(child).transform;
  return nullptr;
}

void build_prefab() {
  g_prefab.prefab = go_new("PCPlayer");
  g_prefab.cosmetics = add(g_prefab.prefab, &kPlayerCosmetics);
  g_prefab.body = go_new("Rigged PC Model Base");
  attach(g_prefab.body, g_prefab.prefab);
  void* animator = add(g_prefab.body, &kAnimator);
  std::memcpy(reinterpret_cast<unsigned char*>(g_prefab.cosmetics) + kPlayerAnimator, &animator, sizeof animator);
  add(g_prefab.body, &kScript);
  add(g_prefab.body, &kAudio);
  add(g_prefab.body, &kListener);
  add(g_prefab.body, &kCamera);
  add(g_prefab.body, &kLight);
  void* hips = go_new("Hips");
  attach(hips, g_prefab.body);
  add(hips, &kScript);
  add(hips, &kRigidbody);
  void* head = go_new("Head");
  attach(head, hips);
  add(head, &kCapsule);
  void* skin = go_new("Body");
  attach(skin, g_prefab.body);
  add(skin, &kSkinned);
  void* rig = go_new("Equipment Swapping Rig");
  attach(rig, g_prefab.body);
  add(rig, &kScript);
}

// What the fake engine was asked to do.
std::string g_bundlePath;
bool g_refuseBundle = false;
void* g_bundle = nullptr;
void* g_controller = nullptr;
std::set<std::string> g_states;  // the controller's states, by name
std::map<int32_t, std::string> g_hashes;
bool g_initialises = true, g_rebound = false;
std::map<void*, void*> g_controllerOf;
std::map<void*, std::pair<bool, int32_t>> g_animatorSetup;  // root motion, culling mode
std::vector<std::string> g_played;
std::map<void*, std::map<int32_t, float>> g_floats;
std::map<void*, std::map<int32_t, phasmo::Color>> g_colours;
std::set<int32_t> g_shaderProperties;  // what the fake shader has
void* g_shader = nullptr;
std::map<void*, std::map<std::string, void*>> g_set;  // object -> setter -> array or object
std::map<void*, bool> g_flags;
std::vector<std::pair<std::string, float>> g_clothSettings;

void* f_LoadFromFile(void* path, uint32_t crc, uint64_t offset) {
  g_bundlePath = text(path);
  CHECK(crc == 0 && offset == 0);
  return g_refuseBundle ? nullptr : g_bundle;
}
void* f_LoadAsset(void* bundle, void* name, void* type) {
  return bundle == g_bundle && text(name) == "assets/test.controller" && class_of_type(type) == &kController
             ? g_controller
             : nullptr;
}
std::map<void*, int32_t> g_hideFlags;  // Object.set_hideFlags
void f_set_hideFlags(void* o, int32_t flags, void*) { g_hideFlags[o] = flags; }

void* f_GetAllAssetNames(void*) {
  void* arr = fake::array(sizeof(void*), 1);
  api::RefArray{arr}.set(0, api::make_string("assets/test.controller"));
  return arr;
}
bool g_unloadedAll = false;
void* g_unloaded = nullptr;
void f_Unload(void* bundle, bool all) {
  g_unloaded = bundle;
  g_unloadedAll = all;
}
int32_t f_StringToHash(void* name, const void*) {
  const std::string s = text(name);
  const auto h = static_cast<int32_t>(std::hash<std::string>{}(s) & 0x7fffffff);  // pm:not-game-data: a test hash
  g_hashes[h] = s;
  return h;
}
void f_set_controller(void* animator, void* controller, const void*) {
  g_controllerOf[animator] = controller;
  g_events.push_back(std::string(controller ? "controller" : "no controller") +
                     (awake(g_ownerOf[animator]) ? " while awake" : " while asleep"));
}
bool f_isInitialized(void* animator) {
  return g_controllerOf.count(animator) && node(g_ownerOf[animator]).active && (g_initialises || g_rebound);
}
void f_Rebind(void*, bool) { g_rebound = true; }
bool f_HasState(void* animator, int32_t layer, int32_t hash) {
  return f_isInitialized(animator) && layer == 0 && g_controllerOf[animator] == g_controller &&
         g_states.count(g_hashes[hash]);
}
void f_Play(void*, int32_t hash, int32_t layer, float time, const void*) {
  CHECK(layer == 0 && time == 0.0f);
  g_played.push_back(g_hashes[hash]);
}
void f_applyRootMotion(void* animator, bool on, const void*) { g_animatorSetup[animator].first = on; }
void f_cullingMode(void* animator, int32_t mode, const void*) { g_animatorSetup[animator].second = mode; }
void* f_get_shader(void*, const void*) { return g_shader; }
void* g_shaderOf = nullptr;
void f_material_ctor(void* material, void* shader, const void*) { g_shaderOf = shader ? material : nullptr; }
bool f_HasProperty(void*, int32_t id) { return g_shaderProperties.count(id); }
void f_SetColor(void* material, int32_t id, phasmo::Color* c) { g_colours[material][id] = *c; }
void f_SetFloat(void* material, int32_t id, float v) { g_floats[material][id] = v; }
void* f_get_sharedMaterial(void* renderer, const void*) { return g_set[renderer]["material"]; }
void* g_meshCtor = nullptr;
void f_mesh_ctor(void* mesh, const void*) { g_meshCtor = mesh; }
#define SETTER(fn, key) \
  void fn(void* o, void* v, const void*) { g_set[o][key] = v; }
SETTER(f_set_vertices, "vertices")
SETTER(f_set_normals, "normals")
SETTER(f_set_uv, "uv")
SETTER(f_set_triangles, "triangles")
SETTER(f_set_boneWeights, "boneWeights")
SETTER(f_set_bindposes, "bindposes")
SETTER(f_set_sharedMesh, "mesh")
SETTER(f_set_bones, "bones")
SETTER(f_set_rootBone, "rootBone")
SETTER(f_set_sharedMaterials, "materials")
SETTER(f_set_capsules, "capsules")
#undef SETTER
void f_updateOffscreen(void* o, bool on, const void*) { g_flags[o] = on; }
void f_coefficients(void* cloth, void* v) { g_set[cloth]["coefficients"] = v; }
void f_gravity(void*, bool on) { g_clothSettings.emplace_back("gravity", on ? 1.0f : 0.0f); }
void f_bending(void*, float v) { g_clothSettings.emplace_back("bending", v); }
void f_stretching(void*, float v) { g_clothSettings.emplace_back("stretching", v); }
void f_damping(void*, float v) { g_clothSettings.emplace_back("damping", v); }
void f_world_velocity(void*, float v) { g_clothSettings.emplace_back("worldVelocity", v); }
void f_world_acceleration(void*, float v) { g_clothSettings.emplace_back("worldAcceleration", v); }
void f_virtual(void*, float v) { g_clothSettings.emplace_back("virtual", v); }
std::map<void*, float> g_radius, g_height;
std::map<void*, int32_t> g_axis;
std::map<void*, Vector3> g_center;
void f_radius(void* c, float v, const void*) { g_radius[c] = v; }
void f_height(void* c, float v, const void*) { g_height[c] = v; }
void f_axis(void* c, int32_t v, const void*) { g_axis[c] = v; }
void f_center(void* c, const Vector3* v, const void*) { g_center[c] = *v; }

template <class F>
void m(const pm::sdk::EntryDesc& d, F* fn) {
  fake::method(d, reinterpret_cast<void*>(fn));
}
template <class F>
void ic(const pm::sdk::EntryDesc& d, F* fn) {
  fake::icall(d, reinterpret_cast<void*>(fn));
}
std::vector<std::pair<const pm::sdk::EntryDesc*, int*>> g_types;
void type(const pm::sdk::EntryDesc& d, int& klass) {
  fake::type(d, &klass);
  g_types.emplace_back(&d, &klass);
}

// A fresh fake game with every entry the helpers use, bound.
void start(bool withCloth = true) {
  for (Obj* o : g_owned) delete o;
  g_owned.clear();
  g_nodes.clear();
  g_ownerOf.clear();
  g_stuck.clear();
  g_destroyed.clear();
  g_typeObjectClass.clear();
  g_types.clear();
  g_prefab = {};
  g_hiddenRenderers.clear();
  g_loadedPath.clear();
  g_bundlePath.clear();
  g_refuseBundle = false;
  g_states = {"Run", "Kick"};
  g_initialises = true;
  g_rebound = false;
  g_controllerOf.clear();
  g_animatorSetup.clear();
  g_played.clear();
  g_floats.clear();
  g_colours.clear();
  g_set.clear();
  g_flags.clear();
  g_clothSettings.clear();
  g_radius.clear();
  g_height.clear();
  g_axis.clear();
  g_center.clear();
  g_unloaded = nullptr;
  g_events.clear();
  fake::reset();
  type(map::GameObject, kGameObject);
  type(map::Transform, kTransform);
  type(map::Animator, kAnimator);
  type(map::MonoBehaviour, kScript);
  type(map::AudioSource, kAudio);
  type(map::Collider, kCollider);
  type(map::Rigidbody, kRigidbody);
  type(map::Renderer, kRenderer);
  type(map::SkinnedMeshRenderer, kSkinned);
  type(map::CapsuleCollider, kCapsule);
  type(map::Material, kMaterial);
  type(map::Mesh, kMesh);
  type(map::RuntimeAnimatorController, kController);
  type(map::PlayerCosmetics, kPlayerCosmetics);
  type(map::Vector3, kVector3);
  type(map::Vector2, kVector2);
  type(map::Int32, kInt32);
  type(map::BoneWeight, kBoneWeight);
  type(map::Matrix4x4, kMatrix);
  type(map::Camera, kCamera);
  type(map::Light, kLight);
  type(map::AudioListener, kListener);
  fake::element_size(map::Vector3, sizeof(Vector3));
  fake::element_size(map::Vector2, sizeof(Vector2));
  fake::element_size(map::Int32, sizeof(int32_t));
  fake::element_size(map::BoneWeight, sizeof(phasmo::BoneWeight));
  fake::element_size(map::Matrix4x4, sizeof(phasmo::Matrix4x4));
  fake::element_size(map::Transform, sizeof(void*));
  fake::element_size(map::Material, sizeof(void*));
  fake::element_size(map::CapsuleCollider, sizeof(void*));
  if (withCloth) {
    type(map::Cloth, kCloth);
    m(map::Cloth_set_capsuleColliders, &f_set_capsules);
    ic(map::Cloth_set_coefficients, &f_coefficients);
    ic(map::Cloth_set_useGravity, &f_gravity);
    ic(map::Cloth_set_bendingStiffness, &f_bending);
    ic(map::Cloth_set_stretchingStiffness, &f_stretching);
    ic(map::Cloth_set_damping, &f_damping);
    ic(map::Cloth_set_worldVelocityScale, &f_world_velocity);
    ic(map::Cloth_set_worldAccelerationScale, &f_world_acceleration);
    ic(map::Cloth_set_useVirtualParticles, &f_virtual);
  }
  m(map::GameObject_get_transform, &f_get_transform);
  m(map::Component_get_gameObject, &f_get_gameObject);
  m(map::Transform_get_childCount, &f_childCount);
  m(map::Transform_GetChild, &f_GetChild);
  m(map::Transform_IsChildOf, &f_IsChildOf);
  m(map::Transform_SetParent, &f_SetParent);
  m(map::Transform_set_position, &f_set_position);
  m(map::Transform_get_position, &f_get_position);
  m(map::Transform_get_rotation, &f_get_rotation);
  m(map::Transform_get_lossyScale, &f_get_lossyScale);
  m(map::GameObject_ctor, &f_ctor);
  m(map::UnityObject_set_name, &f_set_name);
  m(map::GameObject_SetActive, &f_SetActive);
  m(map::GameObject_set_layer, &f_set_layer);
  m(map::GameObject_GetComponents, &f_GetComponents);
  m(map::GameObject_AddComponent, &f_AddComponent);
  m(map::Component_GetComponent, &f_GetComponent);
  m(map::Component_GetComponentInChildren, &f_GetComponentInChildren);
  m(map::UnityObject_Instantiate, &f_Instantiate);
  ic(map::UnityObject_DestroyImmediate, &f_DestroyImmediate);
  ic(map::UnityObject_Destroy, &f_Destroy);
  m(map::Resources_Load, &f_Load);
  fake::field(map::PlayerCosmetics_playerAnimator, kPlayerAnimator);
  m(map::Animator_GetBoneTransform, &f_GetBoneTransform);
  fake::value(map::HumanBodyBones_Hips, 0);
  ic(map::AssetBundle_LoadFromFile, &f_LoadFromFile);
  ic(map::AssetBundle_LoadAsset, &f_LoadAsset);
  ic(map::AssetBundle_GetAllAssetNames, &f_GetAllAssetNames);
  m(map::UnityObject_set_hideFlags, &f_set_hideFlags);
  fake::value(map::HideFlags_DontUnloadUnusedAsset, 32);
  ic(map::AssetBundle_Unload, &f_Unload);
  m(map::Animator_StringToHash, &f_StringToHash);
  m(map::Animator_set_runtimeAnimatorController, &f_set_controller);
  ic(map::Animator_get_isInitialized, &f_isInitialized);
  ic(map::Animator_Rebind, &f_Rebind);
  ic(map::Animator_HasState, &f_HasState);
  m(map::Animator_Play, &f_Play);
  m(map::Animator_set_applyRootMotion, &f_applyRootMotion);
  m(map::Animator_set_cullingMode, &f_cullingMode);
  fake::value(map::AnimatorCullingMode_AlwaysAnimate, 7);  // a number the test can tell from 0
  m(map::Material_get_shader, &f_get_shader);
  m(map::Material_ctor_Shader, &f_material_ctor);
  ic(map::Material_HasProperty, &f_HasProperty);
  ic(map::Material_SetColorImpl, &f_SetColor);
  ic(map::Material_SetFloatImpl, &f_SetFloat);
  fake::value(map::CullMode_Off, 0);
  fake::shader_property(map::ShaderProperty_Lit_BaseColor);
  fake::shader_property(map::ShaderProperty_Lit_Cull);
  fake::shader_property(map::ShaderProperty_Lit_Smoothness);
  m(map::Renderer_get_sharedMaterial, &f_get_sharedMaterial);
  m(map::Renderer_set_enabled, &f_renderer_enabled);
  m(map::UnityObject_get_name, &f_get_name);
  m(map::Renderer_set_sharedMaterials, &f_set_sharedMaterials);
  m(map::Mesh_ctor, &f_mesh_ctor);
  m(map::Mesh_set_vertices, &f_set_vertices);
  m(map::Mesh_set_normals, &f_set_normals);
  m(map::Mesh_set_uv, &f_set_uv);
  m(map::Mesh_set_triangles, &f_set_triangles);
  m(map::Mesh_set_boneWeights, &f_set_boneWeights);
  m(map::Mesh_set_bindposes, &f_set_bindposes);
  m(map::SkinnedMeshRenderer_set_sharedMesh, &f_set_sharedMesh);
  m(map::SkinnedMeshRenderer_set_bones, &f_set_bones);
  m(map::SkinnedMeshRenderer_set_rootBone, &f_set_rootBone);
  m(map::SkinnedMeshRenderer_set_updateWhenOffscreen, &f_updateOffscreen);
  m(map::CapsuleCollider_set_radius, &f_radius);
  m(map::CapsuleCollider_set_height, &f_height);
  m(map::CapsuleCollider_set_direction, &f_axis);
  m(map::CapsuleCollider_set_center, &f_center);
  fake::layer_names({"Default", "TransparentFX", "Ignore Raycast"});
  fake::layer(map::Layer_IgnoreRaycast);
  std::string missing;
  api::bind(fake::host(), missing);
  for (const auto& [d, klass] : g_types) g_typeObjectClass[api::detail::type_object(d->key)] = klass;
  g_bundle = make_obj(&kGameObject);  // any object stands for the engine's bundle
  g_controller = make_obj(&kController);
  g_shader = make_obj(&kShader);
  g_shaderProperties = {phasmo::LitShader::BaseColor.id(), phasmo::LitShader::Cull.id(),
                        phasmo::LitShader::Smoothness.id()};
  build_prefab();
}

std::filesystem::path scratch() { return std::filesystem::temp_directory_path() / "pm_models_test"; }

void a_bundle_loads_from_its_file_and_not_without_one() {
  start();
  std::string why;
  const std::filesystem::path file = scratch() / "test.bundle";
  CHECK(!api::load_bundle(file, &why));
  CHECK(why == "the file is missing" && g_bundlePath.empty());  // the engine is never asked for a missing file
  std::ofstream(file) << "bundle";
  CHECK(api::load_bundle(file, &why) == g_bundle);
  CHECK(g_bundlePath == file.string());
  g_refuseBundle = true;
  CHECK(!api::load_bundle(file, &why));
  CHECK(why.find("engine refused") != std::string::npos);
}

void a_controller_loads_by_its_name() {
  start();
  CHECK(api::load_controller(g_bundle, "assets/test.controller") == g_controller);
  // Kept from a scene load's unload of unused assets: an asset loaded at the menu is in no scene, and Unity would
  // destroy it before it is used (the Emotes mod's clips, songs and icons, 2026-11-10).
  CHECK(g_hideFlags[g_controller] == 32);
  CHECK(!api::load_controller(g_bundle, "assets/other.controller"));
  CHECK(!api::load_controller(nullptr, "assets/test.controller"));
  CHECK(api::bundle_asset_names(g_bundle) == std::vector<std::string>{"assets/test.controller"});
  api::unload_bundle(g_bundle, false);
  CHECK(g_unloaded == g_bundle && !g_unloadedAll);
}

void the_start_check_names_a_missing_state() {
  start();
  std::string missing;
  const char* const both[] = {"Run", "Kick"};
  CHECK(api::controller_has_states(g_controller, both, &missing));
  const char* const more[] = {"Run", "Duck_A", "Kick"};
  CHECK(!api::controller_has_states(g_controller, more, &missing));
  CHECK(missing == "Duck_A");
  // Its own Animator is gone again either way.
  size_t probes = 0;
  for (void* o : g_destroyed) probes += g_nodes.count(o) && node(o).name == "Controller check";
  CHECK(probes == 2);
  // An Animator that binds only when rebound is rebound; one that never binds fails the check, naming why.
  g_initialises = false;
  CHECK(api::controller_has_states(g_controller, both, &missing) && g_rebound);
  g_rebound = false;
  ic(map::Animator_Rebind, +[](void*, bool) {});
  std::string ignored;
  api::bind(fake::host(), ignored);
  CHECK(!api::controller_has_states(g_controller, both, &missing));
  CHECK(missing == "the Animator holding it did not initialise");
  CHECK(!api::controller_has_states(nullptr, both, &missing));
}

void the_body_copy_has_no_script_and_wakes_only_once_stripped() {
  start();
  std::string why;
  void* copy = api::copy_investigator_body("Squatter", &why);
  CHECK(copy != nullptr);
  if (!copy) return;
  CHECK(g_loadedPath == "PCPlayer");
  CHECK(copy != g_prefab.body && node(copy).name == "Squatter");
  CHECK(node(copy).active && node(copy).parent == nullptr);
  for (int* kind : {&kScript, &kAudio, &kListener, &kCamera, &kLight, &kRigidbody, &kCapsule})
    CHECK(!find_in(copy, kind));
  // It was made under a sleeping holder, lost every component and the game's controller while asleep, and woke once,
  // last.
  CHECK(!g_events.empty() && g_events.front() == "instantiate under asleep");
  const auto at = [&](const std::string& e) { return std::find(g_events.begin(), g_events.end(), e) - g_events.begin(); };
  const auto wake = at("wake Squatter");
  CHECK(wake < static_cast<std::ptrdiff_t>(g_events.size()) && std::count(g_events.begin(), g_events.end(), "wake Squatter") == 1);
  CHECK(at("no controller while asleep") < wake);
  CHECK(std::find(g_events.begin(), g_events.end(), "no controller while awake") == g_events.end());
  for (std::ptrdiff_t i = wake + 1; i < static_cast<std::ptrdiff_t>(g_events.size()); ++i)
    CHECK(g_events[static_cast<size_t>(i)] != "destroy");  // nothing taken off after it woke
  for (auto& [animator, controller] : g_controllerOf)
    if (g_ownerOf[animator] == copy) CHECK(controller == nullptr);
  CHECK(find_in(copy, &kSkinned) && find_in(copy, &kAnimator));  // what draws it and moves it stays
  std::set<std::string> children;
  for (void* child : node(copy).children) children.insert(node(child).name);
  CHECK((children == std::set<std::string>{"Hips", "Body"}));  // the rig that draws nothing is gone
  // The holder it was copied under, inactive, is destroyed.
  bool holderGone = false;
  for (void* o : g_destroyed) holderGone = holderGone || (g_nodes.count(o) && node(o).name == "Squatter" && o != copy);
  CHECK(holderGone);
  CHECK(find_in(g_prefab.body, &kScript));  // the prefab itself is untouched

  // A script that will not come off keeps the copy from ever waking: nothing is kept.
  start();
  // Make every copied script stuck as Instantiate makes it.
  m(map::UnityObject_Instantiate, +[](void* original, void* parent, bool, const void*) -> void* {
    void* copy = copy_of(original);
    if (parent) attach(copy, g_ownerOf[parent]);
    for (auto& [go, n] : g_nodes)
      for (void* c : n.components)
        if (klass_of(c) == &kScript) g_stuck.insert(c);
    return copy;
  });
  std::string ignored;
  api::bind(fake::host(), ignored);
  CHECK(!api::copy_investigator_body("Squatter", &why));
  CHECK(why == "a component would not come off the copy");
  CHECK(std::count(g_events.begin(), g_events.end(), "wake Squatter") == 0);

  // So does a light, a camera or a listener that will not come off.
  for (int* kind : {&kLight, &kCamera, &kListener}) {
    start();
    static int* stuckKind;
    stuckKind = kind;
    m(map::UnityObject_Instantiate, +[](void* original, void* parent, bool, const void*) -> void* {
      void* copy = copy_of(original);
      if (parent) attach(copy, g_ownerOf[parent]);
      for (void* c : node(copy).components)
        if (klass_of(c) == stuckKind) g_stuck.insert(c);
      return copy;
    });
    api::bind(fake::host(), ignored);
    CHECK(!api::copy_investigator_body("Squatter", &why));
    CHECK(std::count(g_events.begin(), g_events.end(), "wake Squatter") == 0);
  }
}

// What add_cloth and two_sided_material need of the optional entries is checked at a mod's start, naming the entry.
void availability_names_each_missing_entry() {
  start();
  std::string why;
  CHECK(api::cloth_available(&why) && api::material_available(&why));
  const pm::sdk::EntryDesc* const cloth[] = {&map::Cloth,
                                             &map::Cloth_set_capsuleColliders,
                                             &map::Cloth_set_coefficients,
                                             &map::Cloth_set_useGravity,
                                             &map::Cloth_set_bendingStiffness,
                                             &map::Cloth_set_stretchingStiffness,
                                             &map::Cloth_set_damping,
                                             &map::Layer_IgnoreRaycast};
  for (const pm::sdk::EntryDesc* d : cloth) {
    start();
    fake::forget(*d);
    api::bind(fake::host(), why);
    CHECK(!api::cloth_available(&why));
    CHECK(why == std::string(d->key) + " is not available in this game build");
    CHECK(api::material_available(&why));
    // add_cloth refuses the same build, making nothing.
    void* anchor = go_new("Sheet");
    api::ClothParts parts;
    CHECK(!api::add_cloth(anchor, {}, make_obj(&kMaterial), {}, {}, parts, &why));
    CHECK(why == std::string(d->key) + " is not available in this game build" && !find_in(anchor, &kSkinned));
  }
  const pm::sdk::EntryDesc* const material[] = {&map::ShaderProperty_Lit_BaseColor, &map::ShaderProperty_Lit_Cull};
  for (const pm::sdk::EntryDesc* d : material) {
    start();
    fake::forget(*d);
    api::bind(fake::host(), why);
    CHECK(!api::material_available(&why));
    CHECK(why == std::string(d->key) + " is not available in this game build");
    CHECK(api::cloth_available(&why));
    CHECK(!api::two_sided_material(make_obj(&kMaterial), {1, 1, 1, 1}, &why));
  }
}

void a_state_plays_only_when_the_controller_has_it() {
  start();
  void* go = go_new("Figure");
  void* animator = add(go, &kAnimator);
  CHECK(!api::play_state(animator, "Run"));  // no controller yet
  CHECK(api::use_controller(animator, g_controller));
  CHECK(g_controllerOf[animator] == g_controller);
  CHECK(g_animatorSetup[animator] == std::make_pair(false, 7));  // no root motion, animates unseen
  CHECK(api::play_state(animator, "Kick"));
  CHECK(!api::play_state(animator, "Duck_A"));
  CHECK(g_played == std::vector<std::string>{"Kick"});
  node(go).active = false;
  CHECK(!api::play_state(animator, "Run"));
  CHECK(!api::use_controller(nullptr, g_controller));
}

void the_material_is_plain_and_two_sided() {
  start();
  void* like = make_obj(&kMaterial);
  std::string why;
  void* material = api::two_sided_material(like, {1, 1, 1, 1}, &why);
  CHECK(material && g_shaderOf == material);
  const phasmo::Color c = g_colours[material][phasmo::LitShader::BaseColor.id()];
  CHECK(c.r == 1 && c.g == 1 && c.b == 1 && c.a == 1);
  CHECK(g_floats[material].count(phasmo::LitShader::Cull.id()) && g_floats[material][phasmo::LitShader::Cull.id()] == 0);
  g_shaderProperties = {phasmo::LitShader::BaseColor.id()};  // a shader with no cull property
  CHECK(!api::two_sided_material(like, {1, 1, 1, 1}, &why));
  CHECK(why == "the shader is not the Lit shader");
  CHECK(!api::two_sided_material(nullptr, {1, 1, 1, 1}, &why));
}

// A material is made matte by its smoothness alone, as the game's own untextured fabric is (smoothness 0, metallic
// 0); a smoothness past 0 to 1 is brought within it, and a shader with no smoothness is refused.
void a_material_is_made_matte() {
  start();
  void* material = api::two_sided_material(make_obj(&kMaterial), {1, 1, 1, 1});
  CHECK(material);
  CHECK(api::set_smoothness(material, 0.0f));
  CHECK(g_floats[material].count(phasmo::LitShader::Smoothness.id()) &&
        g_floats[material][phasmo::LitShader::Smoothness.id()] == 0.0f);
  CHECK(api::set_smoothness(material, 1.5f) && g_floats[material][phasmo::LitShader::Smoothness.id()] == 1.0f);
  CHECK(api::set_smoothness(material, -1.0f) && g_floats[material][phasmo::LitShader::Smoothness.id()] == 0.0f);
  CHECK(!api::set_smoothness(nullptr, 0.0f));
  g_shaderProperties = {phasmo::LitShader::BaseColor.id(), phasmo::LitShader::Cull.id()};  // no smoothness
  g_floats.clear();
  CHECK(!api::set_smoothness(material, 0.0f) && g_floats[material].empty());
}

// A one-triangle sheet with two spheres.
struct Sheet {
  std::vector<Vector3> vertices{{0, 0, 0}, {1, 0, 0}, {0, 0, 1}};
  std::vector<Vector3> normals{{0, 1, 0}, {0, 1, 0}, {0, 1, 0}};
  std::vector<Vector2> uv{{0, 0}, {1, 0}, {0, 1}};
  std::vector<int32_t> triangles{0, 2, 1};
  std::vector<Vector2> coefficients{{0, 9}, {5, 9}, {5, 9}};
  api::ClothMesh mesh() const { return {vertices, normals, uv, triangles, coefficients}; }
};

// A costume covers only part of a body: hide_renderers keeps drawing the renderers whose GameObject a name of `keep`
// names, at any depth, and switches every other off (user, 2026-11-09: the Squatter's head and legs show).
void a_costume_hides_only_what_it_covers() {
  start();
  void* root = go_new("Body");
  void* head = go_new("CC_Base_Body");
  void* jacket = go_new("JacketSkinnedMesh");
  void* boots = go_new("Boots");
  attach(head, root);
  attach(jacket, root);
  attach(boots, jacket);  // deeper in the tree
  void* headRenderer = add(head, &kSkinned);
  void* jacketRenderer = add(jacket, &kSkinned);
  void* bootsRenderer = add(boots, &kSkinned);
  const char* keep[] = {"CC_Base_Body", "Boots"};
  CHECK(api::hide_renderers(root, keep) == 1);
  CHECK(g_hiddenRenderers == std::set<void*>{jacketRenderer});
  CHECK(!g_hiddenRenderers.contains(headRenderer) && !g_hiddenRenderers.contains(bootsRenderer));
  CHECK(api::hide_renderers(root) == 3);  // keeping nothing hides all
  CHECK(api::hide_renderers(nullptr, keep) == 0);
}

void cloth_builds_the_mesh_and_its_colliders() {
  start();
  const Sheet sheet;
  void* anchor = go_new("Sheet");
  void* bone = go_new("Head");
  node(bone).position = {0, 2, 0};
  void* material = make_obj(&kMaterial);
  const api::ClothSphere spheres[] = {{&node(bone).transform, 0.1f, {0, 0.05f, 0}}};
  api::ClothParts parts;
  std::string why;
  CHECK(api::add_cloth(anchor, sheet.mesh(), material, {true, 0.3f, 0.9f, 0.1f}, spheres, parts, &why));
  void* mesh = parts.mesh;
  CHECK(mesh && g_meshCtor == mesh);
  const api::Array<Vector3> vertices{g_set[mesh]["vertices"]};
  CHECK(vertices.size() == 3 && vertices[1].x == 1 && vertices[2].z == 1);
  const api::Array<int32_t> triangles{g_set[mesh]["triangles"]};
  CHECK(triangles.size() == 3 && triangles[1] == 2);
  const api::Array<phasmo::BoneWeight> weights{g_set[mesh]["boneWeights"]};
  CHECK(weights.size() == 3 && weights[2].weight0 == 1 && weights[2].bone0 == 0 && weights[2].weight1 == 0);
  const api::Array<phasmo::Matrix4x4> poses{g_set[mesh]["bindposes"]};
  CHECK(poses.size() == 1 && poses[0].m[0] == 1 && poses[0].m[15] == 1 && poses[0].m[1] == 0);
  void* renderer = find_in(anchor, &kSkinned);
  CHECK(renderer && g_set[renderer]["mesh"] == mesh && g_set[renderer]["rootBone"] == &node(anchor).transform);
  CHECK(api::RefArray{g_set[renderer]["bones"]}[0] == &node(anchor).transform);
  CHECK(api::RefArray{g_set[renderer]["materials"]}[0] == material && g_flags[renderer]);
  void* cloth = parts.cloth;
  CHECK(cloth && klass_of(cloth) == &kCloth);
  const api::Array<Vector2> coefficients{g_set[cloth]["coefficients"]};
  CHECK(coefficients.size() == 3 && coefficients[0].x == 0 && coefficients[1].x == 5);
  CHECK(g_clothSettings.size() == 4 && g_clothSettings[1].second == 0.3f && g_clothSettings[2].second == 0.9f);
  // The sphere is on the Ignore Raycast layer at the bone plus its offset, a sphere's height.
  CHECK(parts.colliders.size() == 1);
  void* sphereGo = parts.colliders.empty() ? nullptr : static_cast<void*>(parts.colliders[0]);
  if (sphereGo) {
    CHECK(node(sphereGo).layer == 2 && node(sphereGo).parent == bone);
    CHECK(std::abs(node(sphereGo).position.y - 2.05f) < 1e-5f);
    void* capsule = find_in(sphereGo, &kCapsule);
    CHECK(g_radius[capsule] == 0.1f && g_height[capsule] == 0.0f);
    CHECK(api::RefArray{g_set[cloth]["capsules"]}.size() == 1 && api::RefArray{g_set[cloth]["capsules"]}[0] == capsule);
  }
  api::destroy_cloth(parts);
  CHECK(!parts.mesh && parts.colliders.empty());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), mesh) != g_destroyed.end());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), sphereGo) != g_destroyed.end());
}

// Self-collision, so the cloth does not fold through itself (user, 2026-11-13: the Squatter's sheet clipped into
// itself): add_cloth hands Unity the points the mesh names as a List<UInt32> it builds with the list's own constructor
// and Add, then the distance and stiffness. A build without those entries still gets its cloth, without it; a point past
// the vertices fails the cloth, as any bad index does.
int kList = 0;
std::map<void*, std::vector<uint32_t>> g_listItems;
std::vector<void*> g_selfLists;
float g_selfDistance = 0.0f, g_selfStiffness = 0.0f;
void f_list_ctor(void* list, int32_t, const void*) { g_listItems[list].clear(); }
void f_list_add(void* list, uint32_t v, const void*) { g_listItems[list].push_back(v); }
void f_self_indices(void*, void* list) { g_selfLists.push_back(list); }
void f_self_distance(void*, float v) { g_selfDistance = v; }
void f_self_stiffness(void*, float v) { g_selfStiffness = v; }

void cloth_collides_with_itself_where_asked() {
  for (const bool withSelf : {true, false}) {
    start();
    g_listItems.clear();
    g_selfLists.clear();
    g_selfDistance = g_selfStiffness = 0.0f;
    if (withSelf) {
      fake::method(map::List_UInt32_ctor, reinterpret_cast<void*>(&f_list_ctor), &kList);
      fake::method(map::List_UInt32_Add, reinterpret_cast<void*>(&f_list_add), &kList);
      ic(map::Cloth_SetSelfAndInterCollisionIndices, &f_self_indices);
      ic(map::Cloth_set_selfCollisionDistance, &f_self_distance);
      ic(map::Cloth_set_selfCollisionStiffness, &f_self_stiffness);
      std::string ignored;
      api::bind(fake::host(), ignored);  // start() bound before these were given
    }
    const Sheet sheet;
    const uint32_t points[] = {1, 2};
    api::ClothMesh mesh = sheet.mesh();
    mesh.selfCollision = points;
    api::ClothSettings settings;
    settings.selfCollisionDistance = 0.02f;
    settings.selfCollisionStiffness = 0.3f;
    void* anchor = go_new("Sheet");
    api::ClothParts parts;
    std::string why;
    CHECK(api::add_cloth(anchor, mesh, make_obj(&kMaterial), settings, {}, parts, &why));
    if (withSelf) {
      CHECK(g_selfLists.size() == 1 && klass_of(g_selfLists[0]) == &kList);
      CHECK(!g_selfLists.empty() && g_listItems[g_selfLists[0]] == std::vector<uint32_t>({1, 2}));
      CHECK(g_selfDistance == 0.02f && g_selfStiffness == 0.3f);
      // Off unless asked: no distance, or no points, sets nothing.
      g_selfLists.clear();
      api::ClothParts plain;
      CHECK(api::add_cloth(go_new("Plain"), sheet.mesh(), make_obj(&kMaterial), settings, {}, plain, &why));
      CHECK(g_selfLists.empty());
      // A point past the vertices fails the cloth.
      const uint32_t past[] = {3};
      mesh.selfCollision = past;
      api::ClothParts bad;
      CHECK(!api::add_cloth(go_new("Bad"), mesh, make_obj(&kMaterial), settings, {}, bad, &why));
    } else {
      CHECK(g_selfLists.empty() && g_selfDistance == 0.0f);
    }
  }
}

// A collider the cloth rests on may run along a limb as a capsule rather than being a line of spheres: one collider of
// continuous coverage per limb is how games set cloth up on a body, and it is a fraction of the colliders the cloth has
// to answer to each frame (Unity's cloth capsule colliders).
void a_cloth_collider_can_run_along_a_limb() {
  start();
  const Sheet sheet;
  void* anchor = go_new("Sheet");
  void* bone = go_new("UpperArm");
  node(bone).position = {0, 1.5f, 0};
  void* material = make_obj(&kMaterial);
  const api::ClothSphere limb[] = {{&node(bone).transform, 0.055f, {0.0f, -0.14f, 0.0f}, {0.0f, -1.0f, 0.0f}, 0.28f}};
  api::ClothParts parts;
  std::string why;
  CHECK(api::add_cloth(anchor, sheet.mesh(), material, {}, limb, parts, &why));
  void* go = parts.colliders.empty() ? nullptr : static_cast<void*>(parts.colliders[0]);
  CHECK(go && node(go).parent == bone);
  if (go) {
    CHECK(std::abs(node(go).position.y - 1.36f) < 1e-5f);  // its centre: the bone plus the offset
    void* capsule = find_in(go, &kCapsule);
    CHECK(g_radius[capsule] == 0.055f && std::abs(g_height[capsule] - 0.28f) < 1e-5f);
    CHECK(g_axis[capsule] == 1);  // the bone's axis nearest the direction it was given
  }
  // Unity's capsule is its own diameter at the shortest, which is a sphere: a length below it is raised to it.
  const api::ClothSphere stub[] = {{&node(bone).transform, 0.2f, {}, {0, 1, 0}, 0.1f}};
  api::ClothParts stump;
  CHECK(api::add_cloth(go_new("Plain"), sheet.mesh(), material, {}, stub, stump, &why));
  void* stumpGo = stump.colliders.empty() ? nullptr : static_cast<void*>(stump.colliders[0]);
  CHECK(stumpGo && std::abs(g_height[find_in(stumpGo, &kCapsule)] - 0.4f) < 1e-5f);
  api::destroy_cloth(parts);
  api::destroy_cloth(stump);
}

bool near(const phasmo::Matrix4x4& a, const phasmo::Matrix4x4& b) {
  for (int i = 0; i < 16; ++i)
    if (std::abs(a.m[i] - b.m[i]) > 1e-4f) return false;
  return true;
}

// A bone's bind pose is the inverse of its matrix times the mesh's: carried by the bone where it stood, the mesh stays
// where it was made, whatever the bone's turn, place and size.
void a_bind_pose_undoes_the_bone() {
  const float h = std::sqrt(0.5f);
  const phasmo::Matrix4x4 bone = api::trs_matrix({1, 2, 3}, {0, h, 0, h}, {2, 2, 2});
  const phasmo::Matrix4x4 mesh = api::trs_matrix({0, 1, 0}, {h, 0, 0, h}, {1, 1, 1});
  CHECK(near(api::multiply(bone, api::bind_pose(bone, mesh)), mesh));
  const phasmo::Matrix4x4 identity = api::trs_matrix({0, 0, 0}, {0, 0, 0, 1}, {1, 1, 1});
  CHECK(identity.m[0] == 1 && identity.m[5] == 1 && identity.m[10] == 1 && identity.m[15] == 1 && identity.m[12] == 0);
  const phasmo::Matrix4x4 moved = api::trs_matrix({4, 5, 6}, {0, 0, 0, 1}, {1, 1, 1});
  CHECK(moved.m[12] == 4 && moved.m[13] == 5 && moved.m[14] == 6);  // a column-major translation
}

// Given bones, add_cloth carries each vertex by its weights and gives each bone its bind pose, so the cloth bends with
// a body as worn cloth does in games.
// A cloth can be made to hang heavier: how much it answers its object's motion and acceleration, and smooth contact
// with its colliders, set only when asked, so a cloth that asks nothing keeps Unity's own values.
void a_cloth_can_hang_heavier() {
  start();
  void* anchor = go_new("Sheet");
  void* material = make_obj(&kMaterial);
  api::ClothParts parts;
  std::string why;
  api::ClothSettings heavy{true, 1.0f, 1.0f, 0.4f};
  heavy.worldVelocityScale = 0.25f;
  heavy.worldAccelerationScale = 0.5f;
  heavy.virtualParticles = true;
  CHECK(api::add_cloth(anchor, Sheet{}.mesh(), material, heavy, {}, parts, &why));
  const auto set = [](const char* what) {
    for (const auto& [name, v] : g_clothSettings)
      if (name == what) return v;
    return -1.0f;
  };
  CHECK(set("worldVelocity") == 0.25f && set("worldAcceleration") == 0.5f && set("virtual") == 1.0f);
  g_clothSettings.clear();
  api::ClothParts plain;
  CHECK(api::add_cloth(go_new("Plain"), Sheet{}.mesh(), material, {}, {}, plain, &why));
  CHECK(set("worldVelocity") == -1.0f && set("worldAcceleration") == -1.0f && set("virtual") == -1.0f);
}

void cloth_follows_its_bones() {
  start();
  const Sheet sheet;
  void* anchor = go_new("Sheet");
  node(anchor).position = {0, 1, 0};
  void* head = go_new("Head");
  node(head).position = {0, 2, 0};
  void* hips = go_new("Hips");
  node(hips).position = {0, 1, 0};
  void* const bones[] = {&node(head).transform, &node(hips).transform};
  const api::ClothWeight weights[] = {{{0, 0, 0, 0}, {1, 0, 0, 0}},
                                      {{0, 1, 0, 0}, {0.5f, 0.25f, 0.25f, 0}},
                                      {{1, 1, 0, 0}, {1, 0, 0, 0}}};
  api::ClothMesh mesh = sheet.mesh();
  mesh.bones = bones;
  mesh.weights = weights;
  api::ClothParts parts;
  std::string why;
  CHECK(api::add_cloth(anchor, mesh, make_obj(&kMaterial), {}, {}, parts, &why));
  const api::Array<phasmo::BoneWeight> w{g_set[parts.mesh]["boneWeights"]};
  CHECK(w.size() == 3 && w[1].bone0 == 0 && w[1].bone1 == 1 && w[1].weight0 == 0.5f && w[1].weight1 == 0.25f &&
        w[1].weight2 == 0.25f && w[1].bone2 == 0);
  CHECK(w[2].bone0 == 1 && w[2].weight0 == 1);
  const api::Array<phasmo::Matrix4x4> poses{g_set[parts.mesh]["bindposes"]};
  CHECK(poses.size() == 2 && poses[0].m[13] == -1.0f && poses[1].m[13] == 0.0f);
  void* renderer = find_in(anchor, &kSkinned);
  CHECK(renderer && api::RefArray{g_set[renderer]["bones"]}.size() == 2);
  CHECK(api::RefArray{g_set[renderer]["bones"]}[1] == &node(hips).transform);
  api::destroy_cloth(parts);
  // A weight naming no bone, or weights not one per vertex: nothing made.
  const api::ClothWeight bad[] = {{{0, 0, 0, 0}, {1, 0, 0, 0}}, {{0, 0, 2, 0}, {0.5f, 0.25f, 0.25f, 0}},
                                  {{1, 1, 0, 0}, {1, 0, 0, 0}}};
  mesh.weights = bad;
  CHECK(!api::add_cloth(anchor, mesh, make_obj(&kMaterial), {}, {}, parts, &why) && !parts.mesh);
  mesh.weights = std::span<const api::ClothWeight>(weights, 2);
  CHECK(!api::add_cloth(anchor, mesh, make_obj(&kMaterial), {}, {}, parts, &why) && !parts.mesh);
}

void cloth_refuses_a_bad_mesh_and_a_build_without_cloth() {
  start();
  Sheet sheet;
  void* anchor = go_new("Sheet");
  void* material = make_obj(&kMaterial);
  api::ClothParts parts;
  std::string why;
  sheet.coefficients.pop_back();
  CHECK(!api::add_cloth(anchor, sheet.mesh(), material, {}, {}, parts, &why));
  CHECK(why == "the mesh's parts disagree in length" && !find_in(anchor, &kSkinned));
  sheet = Sheet{};
  sheet.triangles[1] = 3;
  CHECK(!api::add_cloth(anchor, sheet.mesh(), material, {}, {}, parts, &why));
  CHECK(why == "a triangle indexes past the vertices");
  start(false);  // a build whose Cloth module is gone
  anchor = go_new("Sheet");
  material = make_obj(&kMaterial);
  CHECK(!api::add_cloth(anchor, Sheet{}.mesh(), material, {}, {}, parts, &why));
  CHECK(why == "Cloth is not available in this game build" && !find_in(anchor, &kSkinned) && !parts.mesh);
}

// A sphere is never dropped: one with no bone, or one the engine will not make, fails the whole cloth.
void cloth_refuses_a_sphere_it_cannot_make() {
  start();
  void* anchor = go_new("Sheet");
  void* bone = go_new("Head");
  void* material = make_obj(&kMaterial);
  api::ClothParts parts;
  std::string why;
  const api::ClothSphere noBone[] = {{&node(bone).transform, 0.1f, {}}, {nullptr, 0.2f, {}}};
  CHECK(!api::add_cloth(anchor, Sheet{}.mesh(), material, {}, noBone, parts, &why));
  CHECK(why == "a sphere has no bone" && !find_in(anchor, &kSkinned) && !parts.mesh && parts.colliders.empty());
  // The engine refuses the second capsule: everything made so far goes.
  static int capsules;
  capsules = 0;
  m(map::GameObject_AddComponent, +[](void* go, void* type, const void*) -> void* {
    if (class_of_type(type) == &kCapsule && ++capsules == 2) return nullptr;
    return add(go, class_of_type(type));
  });
  api::bind(fake::host(), why);
  const api::ClothSphere two[] = {{&node(bone).transform, 0.1f, {}}, {&node(bone).transform, 0.2f, {}}};
  CHECK(!api::add_cloth(anchor, Sheet{}.mesh(), material, {}, two, parts, &why));
  CHECK(why == "a sphere collider could not be made");
  CHECK(!parts.mesh && !parts.cloth && parts.colliders.empty());
  void* renderer = find_in(anchor, &kSkinned);
  void* cloth = find_in(anchor, &kCloth);
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), renderer) != g_destroyed.end());
  CHECK(std::find(g_destroyed.begin(), g_destroyed.end(), cloth) != g_destroyed.end());
  size_t sphereObjects = 0;
  for (void* o : g_destroyed) sphereObjects += g_nodes.count(o) && node(o).name == "Cloth collider";
  CHECK(sphereObjects == 2);
}

}  // namespace

int main() {
  std::error_code ignored;
  std::filesystem::remove_all(scratch(), ignored);
  std::filesystem::create_directories(scratch());
  a_bundle_loads_from_its_file_and_not_without_one();
  a_controller_loads_by_its_name();
  the_start_check_names_a_missing_state();
  the_body_copy_has_no_script_and_wakes_only_once_stripped();
  a_state_plays_only_when_the_controller_has_it();
  the_material_is_plain_and_two_sided();
  a_material_is_made_matte();
  a_costume_hides_only_what_it_covers();
  cloth_builds_the_mesh_and_its_colliders();
  cloth_collides_with_itself_where_asked();
  a_cloth_collider_can_run_along_a_limb();
  a_bind_pose_undoes_the_bone();
  a_cloth_can_hang_heavier();
  cloth_follows_its_bones();
  cloth_refuses_a_bad_mesh_and_a_build_without_cloth();
  cloth_refuses_a_sphere_it_cannot_make();
  availability_names_each_missing_entry();
  std::filesystem::remove_all(scratch(), ignored);
  for (Obj* o : g_owned) delete o;
  return g_failures ? 1 : 0;
}
