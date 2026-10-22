#include <windows.h>
#include <cstring>
#include <vector>

#include "check.h"
#include "game.h"
/** Keep form expiry deterministic while exercising the production tick. */
ULONGLONG WINAPI form_clock_ms() { return 1000; }
#define GetTickCount64 form_clock_ms
#include "../mods/playable_ghost/form.cpp"
#undef GetTickCount64

namespace {
int player, ghost, renderer, mesh, root, part, filter, material;
bool bakeAvailable = true, pending = false, meshAllocated = true;
int baked = 0, assigned = 0, destroyed = 0;
bool scaled = true;
void* bakedRenderer = nullptr;
void* bakedMesh = nullptr;
il2cpp::Object* skinnedType = reinterpret_cast<il2cpp::Object*>(1);
il2cpp::Object* filterType = reinterpret_cast<il2cpp::Object*>(2);
std::vector<void*> holds;  // gc_hold's handles (index + 1); a released one is null
bool held(void* value) {
  for (void* object : holds)
    if (object == value) return true;
  return false;
}

/** Model the engine's internal call, including its scale argument. */
void bake(void* source, void* destination, bool useScale) {
  ++baked;
  bakedRenderer = source;
  bakedMesh = destination;
  scaled = useScale;
}
/** Model the surviving Mesh constructor. */
void construct(void*, const void*) {}
/** Expose a drawn body renderer. */
bool enabled(void*, const void*) { return true; }
/** Supply the original renderer's materials. */
il2cpp::Array* materials(void*, const void*) { return reinterpret_cast<il2cpp::Array*>(&material); }
/** Observe assigning the newly baked mesh to its visible component. */
void set_mesh(void* target, void* value, const void*) {
  CHECK(target == &filter && value == &mesh);
  ++assigned;
}
/** Check that the figure preserves the original materials. */
void set_materials(void*, il2cpp::Array* value, const void*) {
  CHECK(value == reinterpret_cast<il2cpp::Array*>(&material));
}
}  // namespace

namespace pg::log {
void write(const char*, ...) {}
}

namespace il2cpp {
/** Model the retained class metadata, with no managed BakeMesh wrapper. */
Class* cls(const char*, const char*, const char* name) { return const_cast<char*>(name); }
/** Only methods present in the audited game can resolve. */
void* method_ptr(Class*, const char* name, int argc) {
  if (!std::strcmp(name, ".ctor") && argc == 0) return reinterpret_cast<void*>(construct);
  if (!std::strcmp(name, "get_sharedMaterials") && argc == 0) return reinterpret_cast<void*>(materials);
  if (!std::strcmp(name, "set_sharedMaterials") && argc == 1) return reinterpret_cast<void*>(set_materials);
  if (!std::strcmp(name, "set_sharedMesh") && argc == 1) return reinterpret_cast<void*>(set_mesh);
  if (!std::strcmp(name, "get_enabled") && argc == 0) return reinterpret_cast<void*>(enabled);
  return nullptr;
}
/** Resolve only the native entry registered by UnityPlayer. */
void* resolve_icall(const char* name) {
  return bakeAvailable && !std::strcmp(name, "UnityEngine.SkinnedMeshRenderer::BakeMesh")
      ? reinterpret_cast<void*>(bake) : nullptr;
}
/** Distinguish components requested by the production collector. */
Object* type_object(Class* klass) {
  if (!std::strcmp(static_cast<char*>(klass), "SkinnedMeshRenderer")) return skinnedType;
  if (!std::strcmp(static_cast<char*>(klass), "MeshFilter")) return filterType;
  return reinterpret_cast<Object*>(3);
}
uint32_t gc_hold(void* object) {
  if (!object) return 0;
  holds.push_back(object);
  return static_cast<uint32_t>(holds.size());
}
void gc_release(uint32_t handle) {
  if (handle) holds[handle - 1] = nullptr;
}
/** Model allocation without constructing a real Unity object. */
Object* object_new(Class*) { return meshAllocated ? reinterpret_cast<Object*>(&mesh) : nullptr; }
}

namespace pg::session {
/** Deliver a single received form request to the real rendering code. */
bool Session::take_form(Form& out) {
  if (!pending) return false;
  pending = false;
  out = {7, 5000};
  return true;
}
}

namespace pg::game {
bool alive(void* value) { return value != nullptr; }
void destroy(void*) { ++destroyed; }
void* player_of_actor(int actor) { return actor == 7 ? &player : nullptr; }
void* transform_of(void* value) { return value; }
void* object_transform(void* value) { return value; }
void* game_object_of(void* value) { return value; }
bool active_in_hierarchy(void*) { return true; }
int layer(void*) { return 0; }
int child_count(void*) { return 0; }
void* child(void*, int) { return nullptr; }
void* get_component(void*, il2cpp::Object* type) { return type == skinnedType ? &renderer : nullptr; }
void* new_game_object(const char* name) { return !std::strcmp(name, "Ghost form") ? &root : &part; }
void* add_component(void* value, il2cpp::Object* type) { return type == filterType ? &filter : value; }
Vector3 position(void*) { return {}; }
Quaternion rotation(void*) { return {}; }
void set_position(void*, Vector3) {}
void set_rotation(void*, Quaternion) {}
void set_parent(void*, void*, bool) {}
void* level_controller() { return &ghost; }
}

/** Exercise resolution, a visible baked figure, and cleanup through the public API. */
int main() {
  const bool resolved = pg::form::resolve();
  CHECK(resolved);
  if (resolved) {
    pg::session::Session session;
    pending = true;
    pg::form::tick(session, &ghost);
    CHECK(baked == 1 && bakedRenderer == &renderer && bakedMesh == &mesh);
    CHECK(!scaled && assigned == 1);
    CHECK(held(&root) && held(&mesh));  // kept until the figure goes: the GC does not scan this DLL
    pg::form::teardown();
    CHECK(destroyed == 2);
    CHECK(!held(&root) && !held(&mesh));
    meshAllocated = false;
    pending = true;
    pg::form::tick(session, &ghost);
    CHECK(baked == 1 && assigned == 1);
    pg::form::teardown();
  }
  bakeAvailable = false;
  CHECK(!pg::form::resolve());
  return g_failures ? 1 : 0;
}
