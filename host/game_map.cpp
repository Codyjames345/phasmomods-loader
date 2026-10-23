#include "game_map.h"

#include <windows.h>

#include <exception>
#include <functional>
#include <string>
#include <vector>

#include "all.h"
#include "game_api.h"
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
uintptr_t g_base = 0;                // the loaded GameAssembly the entries were published for
// The entries were published. Mods keep pointers to them (and to their twins) for the process
// lifetime, so they are never published again: that would reallocate them under the mods.
bool g_publishedOnce = false;
// The runtime's il2cpp_resolve_icall, kept for resolve_icalls; null when no model was built.
void* (*g_resolveIcall)(const char* name) = nullptr;

uint32_t kind_of(sdk::Kind kind) {
  switch (kind) {
    case sdk::Kind::Type: return PG_ENTRY_TYPE;
    case sdk::Kind::Method: return PG_ENTRY_METHOD;
    case sdk::Kind::Field: return PG_ENTRY_FIELD;
    case sdk::Kind::EnumValue: return PG_ENTRY_ENUM_VALUE;
    case sdk::Kind::Code: return PG_ENTRY_CODE;
    case sdk::Kind::Icall: return PG_ENTRY_ICALL;
    case sdk::Kind::Export: return PG_ENTRY_EXPORT;
  }
  return 0;
}

// Builds what mods receive for the resolved entry at index i: the runtime's class, method and field
// from the kept model, code and exports as addresses in the loaded module at g_base, an icall's code
// as the runtime gave it. An unresolved entry gets nothing.
void publish_one(size_t i) {
  const uintptr_t base = g_base;
  const sdk::Result& r = g_resolution.results[i];
  Published& p = g_published[i];
  p = {};
  if (r.status != sdk::Status::Resolved) return;
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
    // rva 0 is no code: base + 0 would be the PE header. The resolver never resolves one; this
    // keeps a call through the entry from ever reaching it.
    case sdk::Kind::Code:
    case sdk::Kind::Export: e.code = r.rva ? reinterpret_cast<void*>(base + r.rva) : nullptr; break;
    case sdk::Kind::Icall: e.code = reinterpret_cast<void*>(r.address); break;
    case sdk::Kind::Field:
      if (type && r.member >= 0 && size_t(r.member) < type->fields.size()) e.field = type->fields[r.member].runtime;
      e.offset = r.offset;
      break;
    case sdk::Kind::EnumValue: e.value = r.value; break;
    case sdk::Kind::Type: break;
  }
}

// False, publishing nothing and logging why, once the entries were published.
bool publish(uintptr_t base) {
  if (g_publishedOnce) {
    PG_LOG("game map: already published; a second publish is refused and the entries stay as they are");
    return false;
  }
  g_publishedOnce = true;
  g_base = base;
  g_published.assign(g_resolution.results.size(), {});
  for (size_t i = 0; i < g_resolution.results.size(); ++i) publish_one(i);
  return true;
}

#ifdef PG_GAME_MAP_TEST
// tests/game_map_test.cpp only, which includes this file: forgets the published entries, so the
// next test may publish afresh. The host never forgets them.
void forget_published() {
  g_publishedOnce = false;
  g_published.clear();
}
#endif

// An icall left for the main thread: the worker never asks the runtime (see build_and_resolve).
bool icall_pending(const sdk::Result& r) {
  return r.desc->kind == sdk::Kind::Icall && r.status == sdk::Status::Missing && r.detail == sdk::kCheckedAtStartup;
}

// Why resolution may not go on: startup was cancelled, or less than kHooksBudget of the deadline is
// left. Null while it may.
const char* stop_reason(void* stopEvent, unsigned long long deadline) {
  if (WaitForSingleObject(static_cast<HANDLE>(stopEvent), 0) != WAIT_TIMEOUT) return "startup was cancelled";
  if (GetTickCount64() + kHooksBudgetMs >= deadline) return "the startup budget ran out";
  return nullptr;
}

// Every entry that did not resolve, less the icalls resolve_icalls decides later.
void log_failures() {
  for (const sdk::Result& r : g_resolution.results) {
    if (r.status == sdk::Status::Resolved || icall_pending(r)) continue;
    PG_LOG("game map: %s %s (%s)", sdk::status_word(r.status), r.desc->key, r.detail.c_str());
    for (const std::string& c : r.candidates) PG_LOG("game map:     %s", c.c_str());
  }
}

// No model: every entry stays unresolved, for one reason logged once.
void abandon(const std::string& reason) {
  g_model = {};
  g_resolution = {};
  g_resolveIcall = nullptr;
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
  // No icall lookup here: the runtime's icall table is not locked, and nothing proves the engine has
  // registered every icall by now. resolve_icalls asks on the main thread at activation. Static
  // fields are never read here either: that can run a class's static constructor.
  g_resolveIcall = api.resolve_icall;
  g_resolution = sdk::resolve(map::kAllEntries, g_model, image.valid() ? &image : nullptr);
  return {};
}

}  // namespace

bool resolve(void* gameAssembly, void* stopEvent, unsigned long long deadline) {
  if (g_publishedOnce) {
    PG_LOG("game map: already resolved; a second resolve is refused and the entries stay as published");
    return false;
  }
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

void resolve_icalls() {
  try {
    std::vector<size_t> pending;
    std::vector<const sdk::EntryDesc*> entries;
    for (size_t i = 0; i < g_resolution.results.size(); ++i)
      if (icall_pending(g_resolution.results[i])) {
        pending.push_back(i);
        entries.push_back(g_resolution.results[i].desc);
      }
    if (pending.empty() || g_published.size() != g_resolution.results.size()) return;
    const sdk::IcallLookup lookup = [](const char* name) -> const void* {
      return g_resolveIcall ? g_resolveIcall(name) : nullptr;
    };
    sdk::Resolution icalls = sdk::resolve(entries, sdk::GameModel{}, nullptr, lookup);
    size_t resolved = 0;
    for (size_t k = 0; k < pending.size(); ++k) {
      sdk::Result& r = g_resolution.results[pending[k]];
      r = std::move(icalls.results[k]);
      publish_one(pending[k]);
      if (r.status == sdk::Status::Resolved) ++resolved;
      else PG_LOG("game map: %s %s (%s)", sdk::status_word(r.status), r.desc->key, r.detail.c_str());
    }
    PG_LOG("game map: %zu/%zu icalls resolved on the main thread", resolved, pending.size());
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("game map: managed exception while resolving icalls", e.ex);
  } catch (const std::exception& e) {
    PG_LOG("game map: the icalls could not be resolved: %s", e.what());
  } catch (...) {
    PG_LOG("game map: the icalls could not be resolved");
  }
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

namespace pg::gamemap {

const PgGameEntry* mod_entry(const char* key) {
  const PgGameEntry* e = entry(key);
  return e && e->kind != PG_ENTRY_EXPORT ? e : nullptr;
}

}  // namespace pg::gamemap

// pg::api::Export's lookup (sdk/api/game_api.h), for the host's own code.
const PgGameEntry* pg::api::host_only::game_entry(const char* key) { return gamemap::entry(key); }
