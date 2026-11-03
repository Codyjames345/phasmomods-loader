// Offline test of the host's watch on Photon's room cleanup (host/room_watch.cpp) over the fake game: the
// cleanup runs as before, its end or its exception is logged with the registered views that lost their
// objects, and the exception goes on. Run: build\room_watch_test.exe
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "Photon.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "log.h"
#include "room_watch.h"
#include "runtime.h"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

struct View {
  void* klass = nullptr;
  int32_t id = 0;
};
constexpr int32_t kViewId = 0x8;

View live{nullptr, 1001}, gone{nullptr, 2005};
void* views = nullptr;
std::vector<std::string> g_log;
void capture(const char* line) { g_log.push_back(line); }
bool logged(const char* text) {
  for (const std::string& line : g_log)
    if (line.find(text) != std::string::npos) return true;
  return false;
}
int cleanups = 0;
bool throwInCleanup = false;
bool throwInListing = false;
bool hookWorks = true;
void* hookedTarget = nullptr;
void* hookedDetour = nullptr;
void** hookedOriginal = nullptr;
std::string hookedKey;

void cleanup(const void*) {
  ++cleanups;
  if (throwInCleanup) throw std::runtime_error("cleanup");
}
void* get_views(const void*) {
  if (throwInListing) throw 7;
  return views;
}
int32_t view_id(void* view, const void*) {
  int32_t id = 0;
  std::memcpy(&id, static_cast<char*>(view) + kViewId, sizeof id);
  return id;
}

void reset() {
  fake::reset();
  fake::method(map::PhotonNetwork_get_PhotonViews, reinterpret_cast<void*>(&get_views));
  fake::method(map::PhotonView_get_ViewID, reinterpret_cast<void*>(&view_id));
  std::string missing;
  CHECK(pm::api::bind(fake::host(), missing));
  views = fake::array(sizeof(void*), 3);
  pm::api::RefArray{views}.set(0, &live);
  pm::api::RefArray{views}.set(1, &gone);
  pm::api::RefArray{views}.set(2, nullptr);
  fake::destroy(&gone);
  cleanups = 0;
  throwInCleanup = throwInListing = false;
  g_log.clear();
}

// A cleanup that finishes runs once and says so; nothing is listed.
void a_finished_cleanup_is_logged() {
  reset();
  pm::room_watch::run_cleanup(&cleanup, nullptr);
  CHECK(cleanups == 1);
  CHECK(logged("net: Photon's room cleanup finished"));
  CHECK(!logged("threw"));
}

// A cleanup that throws: the views whose objects are gone are listed, and the same exception goes on.
void a_throwing_cleanup_is_logged_and_rethrown() {
  reset();
  throwInCleanup = true;
  bool rethrown = false;
  try {
    pm::room_watch::run_cleanup(&cleanup, nullptr);
  } catch (const std::runtime_error& e) {
    rethrown = std::string(e.what()) == "cleanup";
  }
  CHECK(rethrown && cleanups == 1);
  CHECK(logged("net: Photon's room cleanup threw; if this client is disconnecting, Photon's peer will not"));
  CHECK(logged("PhotonView 2005 is registered but its object is gone"));
  CHECK(!logged("PhotonView 1001"));
  CHECK(logged("a null PhotonView is registered"));
  CHECK(logged("2 of 3 registered PhotonViews have lost their objects"));
  CHECK(!logged("finished"));
}

// A listing that fails never replaces the cleanup's own exception.
void a_failed_listing_keeps_the_cleanups_exception() {
  reset();
  throwInCleanup = throwInListing = true;
  bool rethrown = false;
  try {
    pm::room_watch::run_cleanup(&cleanup, nullptr);
  } catch (const std::runtime_error&) {
    rethrown = true;
  } catch (...) {
  }
  CHECK(rethrown);
  CHECK(logged("the registered PhotonViews could not be listed"));
}

// A resolved entry is hooked by its code, and the hook runs the cleanup through the watch.
void a_resolved_entry_is_hooked() {
  reset();
  fake::method(map::PhotonNetwork_LeftRoomCleanup, reinterpret_cast<void*>(&cleanup));
  hookWorks = true;
  CHECK(pm::room_watch::create_hook());
  CHECK(hookedTarget == reinterpret_cast<void*>(&cleanup));
  CHECK(hookedKey == map::PhotonNetwork_LeftRoomCleanup.key);
  *hookedOriginal = reinterpret_cast<void*>(&cleanup);
  reinterpret_cast<void (*)(const void*)>(hookedDetour)(nullptr);
  CHECK(cleanups == 1);
  CHECK(logged("net: Photon's room cleanup finished"));
}

// A hook that cannot be made leaves the cleanup unwatched, and says so.
void a_failed_hook_is_logged() {
  reset();
  fake::method(map::PhotonNetwork_LeftRoomCleanup, reinterpret_cast<void*>(&cleanup));
  hookWorks = false;
  CHECK(!pm::room_watch::create_hook());
  CHECK(logged("Photon's room cleanup could not be hooked; it is not watched"));
}

// Without its entry nothing is hooked, and that is logged.
void no_entry_no_hook() {
  reset();
  CHECK(!pm::room_watch::create_hook());
  CHECK(logged("PhotonNetwork.LeftRoomCleanup did not resolve; Photon's room cleanup is not watched"));
}

}  // namespace

// The host's own lookup, over the fake game (host/game_map.cpp in the host).
const PmGameEntry* pm::api::host_only::game_entry(const char* key) { return pm::fake::host_entry(key); }
// host/il2cpp's exception log, which reads a managed exception the fake never raises.
namespace il2cpp {
void log_exception(const char* where, const void*) { PM_LOG("%s", where); }
}  // namespace il2cpp
namespace pm::hooks {
std::atomic<int> g_inflight{0};
bool create(void* target, void* detour, void** original, const char* key) {
  hookedTarget = target;
  hookedDetour = detour;
  hookedOriginal = original;
  hookedKey = key;
  return hookWorks;
}
}  // namespace pm::hooks

int main() {
  pm::log::set_sink(&capture, "");
  a_finished_cleanup_is_logged();
  a_throwing_cleanup_is_logged_and_rethrown();
  a_failed_listing_keeps_the_cleanups_exception();
  a_resolved_entry_is_hooked();
  a_failed_hook_is_logged();
  no_entry_no_hook();
  std::printf("room_watch: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
