#include "mod_glue.h"

#include <string>

#include "game_api.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "net.h"

namespace pg {
namespace {

const PgHostApi* g_host = nullptr;
const PgModInfo* g_info = nullptr;
std::string g_prefix;
net::Handler g_handler = nullptr;

/** Hands a payload to the mod's handler; nothing thrown may unwind into the host. */
void receive(int32_t sender, const uint8_t* data, size_t size) {
  if (!g_handler) return;
  try {
    g_handler(sender, data, size);
  } catch (...) {
    PG_LOG("net: exception while handling a message");
  }
}

}  // namespace

namespace mod {

bool bind(const PgHostApi* api, const PgModInfo* info) {
  if (!api || !pg_api_compatible(api->version, PGMOD_API_VERSION) || !info) return false;
  g_host = api;
  g_info = info;
  g_prefix = std::string("[") + info->id + "] ";
  log::set_sink(api->log, g_prefix.c_str());
  std::string missing;
  if (!api::bind(api, missing)) {
    PG_LOG("game entries this game build lacks: %s", missing.c_str());
    return false;
  }
  return il2cpp::resolve_exports(api->gameAssembly);
}

const PgHostApi& host() { return *g_host; }
const PgModInfo& info() { return *g_info; }

bool guarded_start(const char* name, bool& running, bool (*start)(const PgHostApi*), const PgHostApi* api) {
  try {
    running = start(api);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception((std::string(name) + " inactive: managed exception while starting").c_str(), e.ex);
    running = false;
  } catch (...) {
    PG_LOG("%s inactive: exception while starting", name);
    running = false;
  }
  if (running) PG_LOG("%s active", name);
  return running;
}

void guarded_tick(const bool& running, void (*tick)()) {
  if (!running) return;
  try {
    tick();
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("tick: managed exception caught at the frame boundary", e.ex);
  } catch (...) {
    PG_LOG("tick: unknown exception caught at the frame boundary");
  }
}

void guarded_stop(bool& running, void (*stop)()) {
  if (!running) return;
  running = false;
  try {
    stop();
  } catch (...) {
    PG_LOG("stop: exception while tearing down; the game keeps running without the mod");
  }
}

bool peer_has(int actor, const char* id) { return g_host && g_host->peer_has_mod(actor, id); }

}  // namespace mod

namespace hooks {

std::atomic<int> g_inflight{0};

bool create(void* target, void* detour, void** original, const char* name) {
  return g_host && g_host->create_hook(target, detour, original, name);
}

bool enable_all() { return g_host && g_host->enable_hooks(); }

}  // namespace hooks

namespace net {

void set_handler(Handler handler) {
  g_handler = handler;
  if (g_host) g_host->set_receiver(g_info->channel, handler ? &receive : nullptr);
}

bool send(const uint8_t* data, size_t size, To to, bool reliable, int actor) {
  if (!g_host) return false;
  const int32_t target = to == To::Master ? PG_TO_MASTER : to == To::Actor ? PG_TO_ACTOR : PG_TO_OTHERS;
  return g_host->send(g_info->channel, data, size, target, reliable, actor);
}

}  // namespace net
}  // namespace pg

// The mod's required game entries, for the host before it starts the mod (core/mod_api.h). Nothing
// thrown may leave it; should listing fail, the mod's own bind still refuses a missing entry.
extern "C" __declspec(dllexport) size_t pgmod_requires(const char* const** keys) {
  try {
    return pg::api::used_required(keys);
  } catch (...) {
    *keys = nullptr;
    return 0;
  }
}
