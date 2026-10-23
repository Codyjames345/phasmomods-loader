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
#include "mod_api.h"
#include "runtime_model.h"

namespace pg::gamemap {
namespace {

sdk::GameModel g_model;
sdk::Resolution g_resolution;

// An entry as mods receive it, with the storage its twins point at.
struct Published {
  PgGameEntry entry{};
  std::vector<void*> twins;  // a method's twins' code addresses
};
std::vector<Published> g_published;  // one per result, in the same order; built once, never resized

uint32_t kind_of(sdk::Kind kind) {
  switch (kind) {
    case sdk::Kind::Type: return PG_ENTRY_TYPE;
    case sdk::Kind::Method: return PG_ENTRY_METHOD;
    case sdk::Kind::Field: return PG_ENTRY_FIELD;
    case sdk::Kind::EnumValue: return PG_ENTRY_ENUM_VALUE;
    case sdk::Kind::Code: return PG_ENTRY_CODE;
  }
  return 0;
}

// Builds what mods receive for every resolved entry: the runtime's class, method and field from the
// kept model, code as addresses in the loaded module at `base`.
void publish(uintptr_t base) {
  g_published.assign(g_resolution.results.size(), {});
  for (size_t i = 0; i < g_resolution.results.size(); ++i) {
    const sdk::Result& r = g_resolution.results[i];
    if (r.status != sdk::Status::Resolved) continue;
    Published& p = g_published[i];
    PgGameEntry& e = p.entry;
    e.kind = kind_of(r.desc->kind);
    const sdk::ModelType* type = r.type >= 0 && size_t(r.type) < g_model.types.size() ? &g_model.types[r.type] : nullptr;
    if (type) e.klass = type->runtime;
    switch (r.desc->kind) {
      case sdk::Kind::Method:
        if (type && r.member >= 0 && size_t(r.member) < type->methods.size()) e.method = type->methods[r.member].runtime;
        for (const uint64_t twin : r.twins) p.twins.push_back(reinterpret_cast<void*>(base + twin));
        e.twins = p.twins.data();
        e.twinCount = static_cast<uint32_t>(p.twins.size());
        [[fallthrough]];
      case sdk::Kind::Code: e.code = reinterpret_cast<void*>(base + r.rva); break;
      case sdk::Kind::Field:
        if (type && r.member >= 0 && size_t(r.member) < type->fields.size()) e.field = type->fields[r.member].runtime;
        e.offset = r.offset;
        break;
      case sdk::Kind::EnumValue: e.value = r.value; break;
      case sdk::Kind::Type: break;
    }
  }
}

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
  publish(reinterpret_cast<uintptr_t>(gameAssembly));
  size_t resolved = 0;
  for (const sdk::Result& r : g_resolution.results) resolved += r.status == sdk::Status::Resolved;
  PG_LOG("game map: %zu/%zu resolved in %llu ms", resolved, std::size(map::kAllEntries), GetTickCount64() - start);
  return failure.empty();
}

const sdk::Result* result(std::string_view key) { return g_resolution.find(key); }

const PgGameEntry* entry(const char* key) {
  if (!key || g_published.size() != g_resolution.results.size()) return nullptr;
  for (size_t i = 0; i < g_resolution.results.size(); ++i)
    if (std::string_view(g_resolution.results[i].desc->key) == key)
      return g_resolution.results[i].status == sdk::Status::Resolved ? &g_published[i].entry : nullptr;
  return nullptr;
}

}  // namespace pg::gamemap
