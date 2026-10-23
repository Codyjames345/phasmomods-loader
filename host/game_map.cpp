#include "game_map.h"

#include <windows.h>

#include <exception>
#include <string>
#include <vector>

#include "all.h"
#include "il2cpp.h"
#include "image.h"
#include "log.h"
#include "runtime_model.h"

namespace pg::gamemap {
namespace {

sdk::GameModel g_model;
sdk::Resolution g_resolution;

void log_failures() {
  for (const sdk::Result& r : g_resolution.results) {
    if (r.status == sdk::Status::Resolved) continue;
    PG_LOG("game map: %s %s (%s)", sdk::status_word(r.status), r.desc->key, r.detail.c_str());
    for (const std::string& c : r.candidates) PG_LOG("game map:     %s", c.c_str());
  }
}

bool build_and_resolve(void* gameAssembly) {
  sdk::RuntimeApi api;
  if (!il2cpp::runtime_api(gameAssembly, api)) {
    PG_LOG("game map: the runtime lacks an export the model reads; no entry resolved");
    return false;
  }
  const auto base = reinterpret_cast<uintptr_t>(gameAssembly);
  if (!sdk::load_runtime(api, base, sdk::assemblies_of(map::kAllEntries), g_model)) {
    PG_LOG("game map: the runtime's metadata could not be read; no entry resolved");
    return false;
  }
  // The loaded module's own memory, only read.
  const sdk::CodeImage image(sdk::loaded_module(base));
  if (!image.valid()) PG_LOG("game map: the loaded GameAssembly has no readable function table");
  g_resolution = sdk::resolve(map::kAllEntries, g_model, image.valid() ? &image : nullptr);
  return true;
}

}  // namespace

bool resolve(void* gameAssembly) {
  const ULONGLONG start = GetTickCount64();
  bool built = false;
  try {
    built = build_and_resolve(gameAssembly);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("game map: managed exception while reading the runtime", e.ex);
  } catch (const std::exception& e) {
    PG_LOG("game map: %s", e.what());
  } catch (...) {
    PG_LOG("game map: unknown exception while resolving");
  }
  if (!built) {
    g_model = {};
    g_resolution = {};
  }
  log_failures();
  size_t resolved = 0;
  for (const sdk::Result& r : g_resolution.results) resolved += r.status == sdk::Status::Resolved;
  PG_LOG("game map: %zu/%zu resolved in %llu ms", resolved, std::size(map::kAllEntries), GetTickCount64() - start);
  return built;
}

const sdk::Result* result(std::string_view key) { return g_resolution.find(key); }

// The host contract does not carry game entries yet.
const PgGameEntry* entry(const char*) { return nullptr; }

}  // namespace pg::gamemap
