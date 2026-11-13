// Typed handles (sdk/api/ref.h): a handle converts to its base types' handles as the map's base data says, to
// Object always, and back down only through pm::api::as, which asks the runtime (the fake game backend here). A
// HeldRef keeps its object across frames as Held does. Run: build\ref_test.exe
#include <atomic>
#include <string>
#include <type_traits>
#include <utility>

#include "Camera.h"
#include "GhostAI.h"
#include "UnityObject.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "held.h"
#include "ref.h"

// Hooks' detours hold a hooks::Guard; a mod's glue counts them, a test does here.
namespace pm::hooks {
std::atomic<int> g_inflight{0};
}  // namespace pm::hooks

namespace {

namespace api = pm::api;
namespace fake = pm::fake;
namespace map = pm::map;

using api::Object;
using api::Ref;

// GhostAI derives from MonoBehaviourPun, MonoBehaviour, Behaviour, Component and UnityEngine.Object; Camera from
// Behaviour; neither from the other.
static_assert(api::derives_from<map::GhostAI, map::GhostAI>());
static_assert(api::derives_from<map::GhostAI, map::MonoBehaviourPun>());
static_assert(api::derives_from<map::GhostAI, map::MonoBehaviour>());
static_assert(api::derives_from<map::GhostAI, map::UnityObject>());
static_assert(api::derives_from<map::Camera, map::Behaviour>());
static_assert(!api::derives_from<map::GhostAI, map::Camera>());
static_assert(!api::derives_from<map::Camera, map::GhostAI>());
static_assert(!api::derives_from<map::MonoBehaviour, map::GhostAI>());
static_assert(std::is_convertible_v<Ref<map::GhostAI>, Ref<map::MonoBehaviour>>);
static_assert(std::is_convertible_v<Ref<map::GhostAI>, Object>);
static_assert(!std::is_convertible_v<Ref<map::MonoBehaviour>, Ref<map::GhostAI>>);
static_assert(!std::is_convertible_v<Ref<map::GhostAI>, Ref<map::Camera>>);
static_assert(!std::is_convertible_v<void*, Ref<map::GhostAI>>);
// The transitional bridge (part 5): an Object still takes a raw void* or const void*, so every void*
// caller of a parameter that is now an Object keeps compiling while the wrappers move to handles.
static_assert(std::is_convertible_v<void*, Object>);
static_assert(std::is_convertible_v<const void*, Object>);
static_assert(std::is_convertible_v<Ref<map::GhostAI>, void*>);  // and gives one back
static_assert(std::is_convertible_v<Object, void*>);             // and gives one back
static_assert(!std::is_convertible_v<Object, Ref<map::GhostAI>>);
// A HeldRef reads as its Ref: up to a base type's handle and Object, never to an unrelated or derived one.
static_assert(std::is_convertible_v<const api::HeldRef<map::GhostAI>&, Ref<map::MonoBehaviour>>);
static_assert(std::is_convertible_v<const api::HeldRef<map::GhostAI>&, Object>);
static_assert(!std::is_convertible_v<const api::HeldRef<map::MonoBehaviour>&, Ref<map::GhostAI>>);
static_assert(!std::is_convertible_v<const api::HeldRef<map::GhostAI>&, Ref<map::Camera>>);
static_assert(std::is_constructible_v<api::HeldRef<map::MonoBehaviour>, Ref<map::GhostAI>>);
static_assert(!std::is_constructible_v<api::HeldRef<map::GhostAI>, Ref<map::MonoBehaviour>>);
static_assert(!std::is_constructible_v<api::HeldRef<map::GhostAI>, Ref<map::Camera>>);

// Class pointers for the fake: GhostAI, a class derived from it, and Camera.
int g_ghostClass = 0, g_derivedClass = 0, g_cameraClass = 0;
void* const kGhostClass = &g_ghostClass;
void* const kDerivedClass = &g_derivedClass;
void* const kCameraClass = &g_cameraClass;

void bind_fake() {
  fake::reset();
  fake::type(map::GhostAI, kGhostClass);
  fake::type(map::Camera, kCameraClass);
  fake::parent(kDerivedClass, kGhostClass);
  std::string missing;
  CHECK(api::bind(fake::host(), missing));
  CHECK(missing.empty());
}

void* behaviour_of(Ref<map::MonoBehaviour> behaviour) { return behaviour.raw(); }
void* object_of(Object object) { return object.raw(); }

void base_handle_accepts_derived() {
  void* ghost[1] = {kGhostClass};
  const Ref<map::GhostAI> handle = Ref<map::GhostAI>::unchecked(ghost);
  CHECK(handle.raw() == ghost);
  CHECK(behaviour_of(handle) == ghost);  // through MonoBehaviourPun
  const Ref<map::UnityObject> engineObject = handle;
  CHECK(engineObject.raw() == ghost);
  CHECK(object_of(handle) == ghost);
  CHECK(behaviour_of(nullptr) == nullptr);
}

void as_gives_null_for_wrong_type() {
  bind_fake();
  void* camera[1] = {kCameraClass};
  void* ghost[1] = {kGhostClass};
  CHECK(!api::as<map::GhostAI>(Object::unchecked(camera)));
  CHECK(!api::as<map::Camera>(Ref<map::GhostAI>::unchecked(ghost)));
  CHECK(!api::as<map::GhostAI>(Object{}));
  CHECK(!api::as<map::GhostAI>(Ref<map::MonoBehaviour>{}));
}

void as_gives_handle_for_right_type() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  void* derived[1] = {kDerivedClass};
  const Ref<map::GhostAI> fromObject = api::as<map::GhostAI>(Object::unchecked(ghost));
  CHECK(fromObject && fromObject.raw() == ghost);
  const Ref<map::MonoBehaviour> behaviour = Ref<map::GhostAI>::unchecked(derived);
  const Ref<map::GhostAI> fromBase = api::as<map::GhostAI>(behaviour);  // a class derived from GhostAI is one
  CHECK(fromBase && fromBase.raw() == derived);
}

void held_typed_keeps_object() {
  bind_fake();
  void* ghost[1] = {kGhostClass};
  void* other[1] = {kGhostClass};
  {
    api::HeldRef<map::GhostAI> kept = Ref<map::GhostAI>::unchecked(ghost);
    CHECK(kept && kept.get().raw() == ghost && fake::held() == 1 && fake::held(ghost));
    const Ref<map::GhostAI> read = kept;
    CHECK(read.raw() == ghost);
    CHECK(behaviour_of(kept) == ghost && object_of(kept) == ghost);  // where a base's handle or an Object is taken
    CHECK(api::as<map::GhostAI>(kept).raw() == ghost);
    api::HeldRef<map::MonoBehaviour> asBase = Ref<map::GhostAI>::unchecked(ghost);  // a base's HeldRef keeps a derived one
    CHECK(asBase.get().raw() == ghost && fake::held() == 2);
    asBase = nullptr;
    kept = Ref<map::GhostAI>::unchecked(ghost);  // the object it holds already keeps its handle
    CHECK(fake::held() == 1);
    kept = Ref<map::GhostAI>::unchecked(other);
    CHECK(kept.get().raw() == other && fake::held() == 1 && !fake::held(ghost) && fake::held(other));
    api::HeldRef<map::GhostAI> moved = std::move(kept);
    CHECK(!kept && moved.get().raw() == other && fake::held() == 1);
    moved = nullptr;
    CHECK(!moved && fake::held() == 0);
    moved = Ref<map::GhostAI>::unchecked(ghost);
    CHECK(fake::held() == 1);
  }
  CHECK(fake::held() == 0);
  api::HeldRef<map::GhostAI> none;
  CHECK(!none && none.get() == nullptr && fake::held() == 0);
}

void null_handles_compare() {
  void* ghost[1] = {kGhostClass};
  const Ref<map::GhostAI> none;
  const Ref<map::GhostAI> alsoNone = nullptr;
  CHECK(!none && none == nullptr && nullptr == none && none.raw() == nullptr);
  CHECK(none == alsoNone && none == Ref<map::Camera>{});
  CHECK(!Object{} && Object{} == nullptr && Object(nullptr) == none);
  const Ref<map::GhostAI> handle = Ref<map::GhostAI>::unchecked(ghost);
  CHECK(handle && handle != nullptr && nullptr != handle && handle != none);
  const Ref<map::MonoBehaviour> behaviour = handle;
  CHECK(handle == behaviour && behaviour == handle && handle != Ref<map::Camera>{});
  CHECK(Object::unchecked(ghost) == handle && handle == Object::unchecked(ghost));
  CHECK(Ref<map::GhostAI>::unchecked(nullptr) == nullptr);
}

}  // namespace

int main() {
  base_handle_accepts_derived();
  as_gives_null_for_wrong_type();
  as_gives_handle_for_right_type();
  held_typed_keeps_object();
  null_handles_compare();
  return g_failures ? 1 : 0;
}
