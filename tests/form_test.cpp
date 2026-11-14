// Offline test of the ghost's investigator form (mods/playable_ghost/form.cpp): its Unity calls go through the
// game API over the fake backend, and core's helpers it still uses are modelled here.
// Run: build\form_test.exe
#include <windows.h>
#include <cstring>
#include <string>
#include <vector>

#include "check.h"
#include "fake_game.h"
#include "game.h"
/** Keep form expiry deterministic while exercising the production tick. */
ULONGLONG WINAPI form_clock_ms() { return 1000; }
#define GetTickCount64 form_clock_ms
#include "../mods/playable_ghost/form.cpp"
#undef GetTickCount64

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x20]{};
};
Obj player, ghost, renderer, root, part, filter, material, meshClass;
bool pending = false;
int baked = 0, assigned = 0, destroyed = 0, constructed = 0;
bool scaled = true;
void* bakedRenderer = nullptr;
void* bakedMesh = nullptr;
void* assignedMesh = nullptr;
// Whether the fake host holds the object with a GC handle the code under test took.
bool held(void* value) { return pm::fake::held(value); }

/** Model the engine's internal call, including its scale argument. */
void bake(void* source, void* destination, bool useScale) {
  ++baked;
  bakedRenderer = source;
  bakedMesh = destination;
  scaled = useScale;
}
/** Model the surviving Mesh constructor. */
void construct(void*, const void*) { ++constructed; }
/** Expose a drawn body renderer. */
bool enabled(void*, const void*) { return true; }
/** Supply the original renderer's materials. */
void* materials(void*, const void*) { return &material; }
/** Observe assigning the newly baked mesh to its visible component. */
void set_mesh(void* target, void* value, const void*) {
  CHECK(target == &filter);
  assignedMesh = value;
  ++assigned;
}
/** Check that the figure preserves the original materials. */
void set_materials(void*, void* value, const void*) { CHECK(value == &material); }
/** The investigator's body has a skinned renderer; nothing else has a component. */
void* get_component(void*, void* type, const void*) {
  return type == phasmo::SkinnedMeshRenderer::type.type_object() ? &renderer : nullptr;
}
/** A MeshFilter added to a part is the filter; any other component is the part itself. */
void* add_component(void* object, void* type, const void*) {
  return type == phasmo::MeshFilter::type.type_object() ? &filter : object;
}

/** Gives every entry form.cpp uses, with the Mesh type's class `mesh` (null: it cannot make one). */
void give_entries(void* mesh, bool withBake) {
  fake::reset();
  static Obj skinnedClass, filterClass, rendererClass;
  fake::type(map::Mesh, mesh);
  fake::method(map::Mesh_ctor, reinterpret_cast<void*>(&construct));
  fake::type(map::SkinnedMeshRenderer, &skinnedClass);
  if (withBake) fake::icall(map::SkinnedMeshRenderer_BakeMesh, reinterpret_cast<void*>(&bake));
  fake::type(map::MeshFilter, &filterClass);
  fake::type(map::MeshRenderer, &rendererClass);
  fake::method(map::MeshFilter_set_sharedMesh, reinterpret_cast<void*>(&set_mesh));
  fake::method(map::Renderer_get_enabled, reinterpret_cast<void*>(&enabled));
  fake::method(map::Renderer_get_sharedMaterials, reinterpret_cast<void*>(&materials));
  fake::method(map::Renderer_set_sharedMaterials, reinterpret_cast<void*>(&set_materials));
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&get_component));
  fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&add_component));
  fake::layer_names({"Default", "PlayerView"});
  fake::layer(map::Layer_PlayerView);
}
}  // namespace

namespace pm::session {
/** Deliver a single received form request to the real rendering code. */
bool Session::take_form(Form& out) {
  if (!pending) return false;
  pending = false;
  out = {7, 5000};
  return true;
}
}  // namespace pm::session

namespace pm::game {
bool alive(void* value) { return value != nullptr; }
void destroy(void*) { ++destroyed; }
api::Object player_of_actor(int actor) { return actor == 7 ? &player : nullptr; }
api::Ref<pm::map::Transform> transform_of(void* value) { return api::Ref<pm::map::Transform>::unchecked(value); }
api::Ref<pm::map::Transform> object_transform(void* value) { return api::Ref<pm::map::Transform>::unchecked(value); }
api::Object game_object_of(void* value) { return value; }
bool active_in_hierarchy(void*) { return true; }
int layer(void*) { return 0; }
int child_count(void*) { return 0; }
api::Ref<pm::map::Transform> child(void*, int) { return nullptr; }
api::Object new_game_object(const char* name) { return !std::strcmp(name, "Ghost form") ? &root : &part; }
Vector3 position(void*) { return {}; }
Quaternion rotation(void*) { return {}; }
void set_position(void*, Vector3) {}
void set_rotation(void*, Quaternion) {}
void set_parent(void*, void*, bool) {}
api::Ref<pm::map::LevelController> level_controller() { return api::Ref<pm::map::LevelController>::unchecked(&ghost); }
}  // namespace pm::game

/** Exercise binding, a visible baked figure, and cleanup through the public API. */
int main() {
  give_entries(&meshClass, true);
  std::string missing;
  const bool bound = pm::api::bind(fake::host(), missing);
  CHECK(bound);
  if (bound) {
    pm::session::Session session;
    pending = true;
    pm::form::tick(session, &ghost);
    CHECK(baked == 1 && bakedRenderer == &renderer && bakedMesh && constructed == 1);
    CHECK(!scaled && assigned == 1 && assignedMesh == bakedMesh);
    CHECK(held(&root) && held(bakedMesh));  // kept until the figure goes: the GC does not scan this DLL
    void* mesh = bakedMesh;
    pm::form::teardown();
    CHECK(destroyed == 2);
    CHECK(!held(&root) && !held(mesh));
  }
  give_entries(nullptr, true);  // the Mesh type makes nothing
  CHECK(pm::api::bind(fake::host(), missing));
  pm::session::Session session;
  pending = true;
  pm::form::tick(session, &ghost);
  CHECK(baked == 1 && assigned == 1);
  pm::form::teardown();
  give_entries(&meshClass, false);  // a runtime without the bake: the mod does not start
  CHECK(!pm::api::bind(fake::host(), missing) && missing == "SkinnedMeshRenderer.BakeMesh");
  return g_failures ? 1 : 0;
}
