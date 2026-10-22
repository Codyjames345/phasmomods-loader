#include "hide.h"

#include <windows.h>

#include "game.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::hide {
namespace {

constexpr ULONGLONG kSweepMs = 250;  // new salt and crucifixes are hidden within this
constexpr int kMaxDepth = 8;         // how deep under each object renderers are looked for

struct Api {
  il2cpp::Object* saltType = nullptr;
  il2cpp::Object* crucifixType = nullptr;
  il2cpp::Object* rendererType = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  bool ok = false;
} api;

ULONGLONG g_next = 0;

void hide_under(void* transform, int depth) {
  il2cpp::Array* renderers = game::components(game::game_object_of(transform), api.rendererType);
  for (uintptr_t i = 0; renderers && i < renderers->max_length; ++i) {
    void* r = renderers->data<void*>()[i];
    if (r && game::alive(r) && game::renderer_enabled(r)) game::set_renderer_enabled(r, false);
  }
  if (depth >= kMaxDepth) return;
  for (int i = 0; i < game::child_count(transform); ++i) hide_under(game::child(transform, i), depth + 1);
}

void hide_all(il2cpp::Object* type) {
  il2cpp::Array* found = api.findObjects(type, nullptr);
  for (uintptr_t i = 0; found && i < found->max_length; ++i)
    if (void* o = found->data<void*>()[i]; o && game::alive(o)) hide_under(game::transform_of(o), 0);
}

}  // namespace

bool resolve() {
  constexpr const char* kGame = "Assembly-CSharp.dll";
  api.saltType = il2cpp::type_object(il2cpp::cls(kGame, "", "SaltSpot"));
  api.crucifixType = il2cpp::type_object(il2cpp::cls(kGame, "", "Crucifix"));
  api.rendererType = il2cpp::type_object(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Renderer"));
  api.findObjects = reinterpret_cast<decltype(api.findObjects)>(
      il2cpp::method_ptr(il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object"), "FindObjectsOfType", 1));
  api.ok = api.saltType && api.crucifixType && api.rendererType && api.findObjects;
  PG_LOG("hide: resolve %s", api.ok ? "ok" : "FAILED");
  return api.ok;
}

void tick(bool ghostPlayerInContract) {
  if (!ghostPlayerInContract || !api.ok) return;
  const ULONGLONG now = GetTickCount64();
  if (now < g_next) return;
  g_next = now + kSweepMs;
  hide_all(api.saltType);
  hide_all(api.crucifixType);
}

}  // namespace pg::hide
