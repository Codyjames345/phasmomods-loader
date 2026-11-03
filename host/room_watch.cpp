#include "room_watch.h"

#include "Photon.h"
#include "contained.h"
#include "game_api.h"
#include "hooks.h"
#include "log.h"
#include "mod_api.h"
#include "runtime.h"

namespace pm::room_watch {
namespace {

// PhotonNetwork.set_OfflineMode runs the cleanup too, where no disconnect hangs on it.
constexpr const char* kThrew = "net: Photon's room cleanup threw; if this client is disconnecting, Photon's peer will not";

using Cleanup_t = void (*)(const void* method);
Cleanup_t g_cleanup = nullptr;

void detour_cleanup(const void* method) {
  hooks::Guard guard;
  run_cleanup(g_cleanup, method);
}

}  // namespace

size_t log_dead_views() {
  const api::RefArray views{phasmo::PhotonNetwork::get_PhotonViews()};
  size_t dead = 0;
  for (size_t i = 0, n = views.size(); i < n; ++i) {
    void* view = views[i];
    if (view && api::alive(view)) continue;
    ++dead;
    // ViewID reads the view's own field, which stays readable after its engine object is gone.
    if (view) PM_LOG("net:   PhotonView %d is registered but its object is gone", phasmo::PhotonView::get_ViewID(view));
    else PM_LOG("net:   a null PhotonView is registered");
  }
  PM_LOG("net:   %zu of %zu registered PhotonViews have lost their objects", dead, views.size());
  return dead;
}

namespace {

// The listing never replaces the cleanup's own exception with one of its own.
void log_dead_views_contained() {
  try {
    log_dead_views();
  } catch (...) {
    PM_LOG("net:   the registered PhotonViews could not be listed");
  }
}

}  // namespace

void run_cleanup(Cleanup_t cleanup, const void* method) {
  try {
    cleanup(method);
  } catch (const Il2CppExceptionWrapper& e) {
    host::log_managed(kThrew, e.ex);
    log_dead_views_contained();
    throw;
  } catch (...) {
    PM_LOG("%s", kThrew);
    log_dead_views_contained();
    throw;
  }
  PM_LOG("net: Photon's room cleanup finished");
}

bool create_hook() {
  const PmGameEntry* e = api::host_only::game_entry(map::PhotonNetwork_LeftRoomCleanup.key);
  if (!e || !e->code) {
    PM_LOG("net: %s did not resolve; Photon's room cleanup is not watched", map::PhotonNetwork_LeftRoomCleanup.key);
    return false;
  }
  if (hooks::create(e->code, reinterpret_cast<void*>(&detour_cleanup), reinterpret_cast<void**>(&g_cleanup),
                    map::PhotonNetwork_LeftRoomCleanup.key))
    return true;
  PM_LOG("net: Photon's room cleanup could not be hooked; it is not watched");
  return false;
}

}  // namespace pm::room_watch
