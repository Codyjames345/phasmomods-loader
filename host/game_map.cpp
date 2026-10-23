#include "game_map.h"

#include <windows.h>

#include <exception>
#include <functional>
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

// Why resolution may not go on: startup was cancelled, or less than kHooksBudget of the deadline is
// left. Null while it may.
const char* stop_reason(void* stopEvent, unsigned long long deadline) {
  if (WaitForSingleObject(static_cast<HANDLE>(stopEvent), 0) != WAIT_TIMEOUT) return "startup was cancelled";
  if (GetTickCount64() + kHooksBudgetMs >= deadline) return "the startup budget ran out";
  return nullptr;
}

void log_failures() {
  for (const sdk::Result& r : g_resolution.results) {
    if (r.status == sdk::Status::Resolved) continue;
    PG_LOG("game map: %s %s (%s)", sdk::status_word(r.status), r.desc->key, r.detail.c_str());
    for (const std::string& c : r.candidates) PG_LOG("game map:     %s", c.c_str());
  }
}

// No model: every entry stays unresolved, for one reason logged once.
void abandon(const std::string& reason) {
  g_model = {};
  g_resolution = {};
  for (const sdk::EntryDesc* e : map::kAllEntries) {
    sdk::Result r;
    r.desc = e;
    r.detail = "the game map was abandoned: " + reason;
    g_resolution.results.push_back(std::move(r));
  }
  PG_LOG("game map: abandoned, no entry resolved: %s", reason.c_str());
}

// The reason it could not resolve, or empty once it has.
std::string build_and_resolve(void* gameAssembly, void* stopEvent, unsigned long long deadline) {
  if (const char* why = stop_reason(stopEvent, deadline)) return why;
  sdk::RuntimeApi api;
  if (!il2cpp::runtime_api(gameAssembly, api)) return "the runtime lacks an export the model reads";
  const char* stopped = nullptr;
  const std::function<bool()> proceed = [&] { return !(stopped = stop_reason(stopEvent, deadline)); };
  const auto base = reinterpret_cast<uintptr_t>(gameAssembly);
  if (!sdk::load_runtime(api, base, sdk::assemblies_of(map::kAllEntries), g_model, proceed))
    return stopped ? stopped : "the runtime's metadata could not be read";
  if (const char* why = stop_reason(stopEvent, deadline)) return why;
  // The loaded module's own memory, only read.
  const sdk::CodeImage image(sdk::loaded_module(base));
  if (!image.valid()) PG_LOG("game map: the loaded GameAssembly has no readable function table");
  g_resolution = sdk::resolve(map::kAllEntries, g_model, image.valid() ? &image : nullptr);
  return {};
}

}  // namespace

bool resolve(void* gameAssembly, void* stopEvent, unsigned long long deadline) {
  const ULONGLONG start = GetTickCount64();
  std::string failure = "an unknown exception was thrown";
  try {
    failure = build_and_resolve(gameAssembly, stopEvent, deadline);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("game map: managed exception while reading the runtime", e.ex);
    failure = "a managed exception was thrown";
  } catch (const std::exception& e) {
    failure = e.what();
  } catch (...) {
  }
  if (failure.empty()) log_failures();
  else abandon(failure);
  size_t resolved = 0;
  for (const sdk::Result& r : g_resolution.results) resolved += r.status == sdk::Status::Resolved;
  PG_LOG("game map: %zu/%zu resolved in %llu ms", resolved, std::size(map::kAllEntries), GetTickCount64() - start);
  return failure.empty();
}

const sdk::Result* result(std::string_view key) { return g_resolution.find(key); }

// The host contract does not carry game entries yet.
const PgGameEntry* entry(const char*) { return nullptr; }

}  // namespace pg::gamemap
