#include "mod_glue.h"

#include <windows.h>

#include <filesystem>
#include <string>
#include <system_error>

#include "game_api.h"
#include "hooks.h"
#include "managed_exception.h"
#include "log.h"
#include "net.h"
#include "text_files.h"
#include "text_table.h"

namespace pm {
namespace {

const PmHostApi* g_host = nullptr;
const PmModInfo* g_info = nullptr;
std::string g_prefix;
net::Handler g_handler = nullptr;

/** Hands a payload to the mod's handler; nothing thrown may unwind into the host. */
void receive(int32_t sender, const uint8_t* data, size_t size) {
  if (!g_handler) return;
  try {
    g_handler(sender, data, size);
  } catch (...) {
    PM_LOG("net: exception while handling a message");
  }
}

// The mod's player-facing text (core/text_table.h), from beside this DLL's copy. Without it every text shows its key, and the
// log says why.
void load_text() {
  HMODULE self = nullptr;
  wchar_t path[MAX_PATH];
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&load_text), &self) ||
      !GetModuleFileNameW(self, path, MAX_PATH)) {
    PM_LOG("this DLL's path cannot be read, so its text cannot be found; its messages show their keys");
    return;
  }
  i18n::table().on_missing([](const std::string& key) { PM_LOG("the mod's text has no %s", key.c_str()); });
  std::string why;
  const std::filesystem::path file = i18n::table_beside(path);
  std::error_code error;
  if (!i18n::table().load(file, why) && std::filesystem::exists(file.parent_path(), error))
    PM_LOG("its text (%s) could not be read: %s; its messages show their keys", text::narrow(file.wstring()).c_str(), why.c_str());
}

}  // namespace

namespace mod {

bool bind(const PmHostApi* api, const PmModInfo* info) {
  if (!api || !pm_api_compatible(api->version, PMMOD_API_VERSION) || !info) return false;
  g_host = api;
  g_info = info;
  g_prefix = std::string("[") + info->id + "] ";
  log::set_sink(api->log, g_prefix.c_str());
  load_text();
  std::string missing;
  if (!api::bind(api, missing)) {
    PM_LOG("game entries this game build lacks: %s", missing.c_str());
    return false;
  }
  return true;
}

const PmHostApi& host() { return *g_host; }
const PmModInfo& info() { return *g_info; }

bool guarded_start(const char* name, bool& running, bool (*start)(const PmHostApi*), const PmHostApi* api) {
  try {
    running = start(api);
  } catch (const Il2CppExceptionWrapper& e) {
    if (g_host) g_host->log_exception((std::string(name) + " inactive: managed exception while starting").c_str(), e.ex);
    else PM_LOG("%s inactive: managed exception while starting", name);
    running = false;
  } catch (...) {
    PM_LOG("%s inactive: exception while starting", name);
    running = false;
  }
  if (running) PM_LOG("%s active", name);
  return running;
}

void guarded_tick(const bool& running, void (*tick)()) {
  if (!running) return;
  try {
    tick();
  } catch (const Il2CppExceptionWrapper& e) {
    // The host writes this line, not the mod's own log sink, so it carries no mod prefix: name the mod.
    const std::string where = std::string(g_info && g_info->id ? g_info->id : "a mod") +
                              ": tick: managed exception caught at the frame boundary";
    if (g_host) g_host->log_exception(where.c_str(), e.ex);
    else PM_LOG("tick: managed exception caught at the frame boundary");
  } catch (...) {
    PM_LOG("tick: unknown exception caught at the frame boundary");
  }
}

void guarded_stop(bool& running, void (*stop)()) {
  if (!running) return;
  running = false;
  try {
    stop();
  } catch (...) {
    PM_LOG("stop: exception while tearing down; the game keeps running without the mod");
  }
}

bool peer_has(int actor, const char* id) { return g_host && g_host->peer_has_mod(actor, id); }

bool mod_started(const char* id) { return g_host && g_host->mod_started(id); }

std::string shown_name() {
  const char* name = g_host && g_info ? g_host->mod_name(g_info->id) : nullptr;
  return name && *name ? std::string(name) : std::string(g_info ? g_info->name : "");
}

void show_message(const std::string& title, const std::string& body) {
  if (g_host) g_host->show_message(title.c_str(), body.c_str());
}

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
  const int32_t target = to == To::Master ? PM_TO_MASTER : to == To::Actor ? PM_TO_ACTOR : PM_TO_OTHERS;
  return g_host->send(g_info->channel, data, size, target, reliable, actor);
}

}  // namespace net
}  // namespace pm

// The mod's required game entries, for the host before it starts the mod (core/mod_api.h). Nothing
// thrown may leave it; should listing fail, the mod's own bind still refuses a missing entry.
extern "C" __declspec(dllexport) size_t pmmod_requires(const char* const** keys) {
  try {
    return pm::api::used_required(keys);
  } catch (...) {
    *keys = nullptr;
    return 0;
  }
}
