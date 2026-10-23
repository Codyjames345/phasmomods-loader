#include "mods.h"

#include <windows.h>

#include <algorithm>
#include <filesystem>

#include "build_check.h"
#include "hooks.h"
#include "log.h"
#include "mod_list.h"
#include "text_files.h"

namespace pg::mods {
namespace {

namespace fs = std::filesystem;
using text::read_file;
using text::widen;

std::vector<Mod> g_mods;
fs::path g_dataDirectory;
std::vector<std::string> g_startOrder;  // the running mods' ids in the order they started

/** The folder this host DLL was loaded from. */
fs::path host_folder() {
  HMODULE self = nullptr;
  wchar_t path[MAX_PATH];
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&host_folder), &self) ||
      !GetModuleFileNameW(self, path, MAX_PATH))
    return {};
  return fs::path(path).parent_path();
}

bool duplicate(const PgModInfo* info) {
  for (const Mod& m : g_mods)
    if (std::string(m.info->id) == info->id || m.info->channel == info->channel) return true;
  return false;
}

/**
 * A mod's manifest, or an empty one (logged) when it has none or it cannot be read: the mod then
 * loads without a description or a requirement, and a mod that needs another checks for it itself.
 */
modlist::Manifest manifest_in(const fs::path& ini) {
  std::error_code error;
  modlist::Manifest m;
  std::string why;
  if (!fs::exists(ini, error)) {
    PG_LOG("mods: %ls is missing; that mod has no description or requirement", ini.c_str());
    return {};
  }
  if (!modlist::parse_manifest(read_file(ini), m, why)) {
    PG_LOG("mods: %ls cannot be used: %s", ini.c_str(), why.c_str());
    return {};
  }
  return m;
}

Mod* loaded_mod(const std::string& id) {
  for (Mod& m : g_mods)
    if (id == m.info->id) return &m;
  return nullptr;
}

std::string build_of(const fs::path& dll) {
  char hex[65];
  return build::sha256_file(dll.c_str(), hex) ? std::string(hex, 16) : std::string(16, '0');
}

}  // namespace

std::vector<Found> find(const fs::path& dataDirectory, const fs::path& hostFolder, unsigned long pid) {
  std::vector<Found> found;
  std::error_code error;
  // Only the launcher's own copy of the host, in bin, runs what the launcher listed for this process.
  // Read once, so a later process reusing the id never picks up a stale list.
  const fs::path bin = dataDirectory / L"bin";
  const fs::path session = bin / (L"session-" + std::to_wstring(pid) + L".txt");
  if (fs::equivalent(hostFolder, bin, error) && fs::exists(session, error)) {
    for (const std::string& p : modlist::parse_session(read_file(session))) {
      fs::path dll = widen(p);
      const modlist::Manifest m = manifest_in(fs::path(dll).replace_extension(L".ini"));
      found.push_back({dll, m.description, m.required});
    }
    fs::remove(session, error);
    PG_LOG("mods: %zu listed by the launcher", found.size());
    return found;
  }
  std::vector<fs::path> folders;
  for (const auto& entry : fs::directory_iterator(hostFolder / L"mods", error))
    if (entry.is_directory(error)) folders.push_back(entry.path());
  std::sort(folders.begin(), folders.end());
  for (const fs::path& folder : folders) {
    const fs::path ini = folder / L"mod.ini";
    if (!fs::exists(ini, error)) continue;
    modlist::Manifest m;
    std::string why;
    if (!modlist::parse_manifest(read_file(ini), m, why)) {
      PG_LOG("mods: skipping %ls: %s", ini.c_str(), why.c_str());
      continue;
    }
    found.push_back({folder / widen(m.dll), m.description, m.required});
  }
  PG_LOG("mods: %zu found beside the host", found.size());
  return found;
}

void load(const std::wstring& dataDirectory) {
  g_mods.clear();
  g_startOrder.clear();
  g_dataDirectory = dataDirectory;
  for (const Found& f : find(dataDirectory, host_folder(), GetCurrentProcessId())) {
    HMODULE module = LoadLibraryW(f.dll.c_str());
    if (!module) {
      PG_LOG("mods: could not load %ls (Windows error %lu)", f.dll.c_str(), GetLastError());
      continue;
    }
    HMODULE pinned = nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                       reinterpret_cast<LPCWSTR>(module), &pinned);  // detours in it must never be unmapped
    Mod m;
    auto info = reinterpret_cast<PgModInfoFn>(GetProcAddress(module, "pgmod_info"));
    m.start = reinterpret_cast<PgModStartFn>(GetProcAddress(module, "pgmod_start"));
    m.tick = reinterpret_cast<PgModTickFn>(GetProcAddress(module, "pgmod_tick"));
    m.stop = reinterpret_cast<PgModStopFn>(GetProcAddress(module, "pgmod_stop"));
    m.info = info ? info() : nullptr;
    if (!m.info || !m.start || !m.tick || !m.stop || !m.info->id || !m.info->name) {
      PG_LOG("mods: %ls is not a mod for this host", f.dll.c_str());
      continue;
    }
    if (m.info->apiVersion != PGMOD_API_VERSION || m.info->channel == 0) {
      PG_LOG("mods: %s was built for another host version", m.info->id);
      continue;
    }
    if (duplicate(m.info)) {
      PG_LOG("mods: %s repeats the id or channel of a mod already loaded", m.info->id);
      continue;
    }
    m.build = build_of(f.dll);
    m.description = f.description;
    m.required = f.required;
    PG_LOG("mods: loaded %s (%s, build %s)", m.info->id, m.info->name, m.build.c_str());
    g_mods.push_back(std::move(m));
  }
}

const std::vector<Mod>& loaded() { return g_mods; }

std::vector<std::string> chosen() {
  std::vector<std::string> installed;
  for (const Mod& m : g_mods) installed.emplace_back(m.info->id);
  // A choice saved before a mod gained a requirement still starts what that mod needs.
  return modlist::with_requirements(modlist::load_choice(g_dataDirectory / L"launcher.ini", installed), manifests());
}

bool save_chosen(const std::vector<std::string>& ids) { return modlist::save_choice(g_dataDirectory / L"launcher.ini", ids); }

std::vector<modlist::Manifest> manifests() {
  std::vector<modlist::Manifest> all;
  for (const Mod& m : g_mods) {
    modlist::Manifest e;
    e.id = m.info->id;
    e.name = m.info->name;
    e.required = m.required;
    all.push_back(std::move(e));
  }
  return all;
}

void start(const PgHostApi* api, const std::vector<std::string>& ids) {
  std::vector<std::string> loadOrder;  // the chosen mods that are loaded, in load order
  for (const Mod& m : g_mods)
    if (modlist::enabled(ids, m.info->id)) loadOrder.emplace_back(m.info->id);
  const std::vector<std::string> order = modlist::start_order(loadOrder, manifests());
  for (const std::string& id : loadOrder)
    if (!modlist::enabled(order, id))
      PG_LOG("mods: %s needs %s, which is not chosen, not installed or needs something missing; it stays off", id.c_str(),
             loaded_mod(id)->required.c_str());
  for (const std::string& id : order) {
    Mod& m = *loaded_mod(id);
    if (m.started) continue;
    if (!modlist::requirement_met(m.required, started_ids())) {
      PG_LOG("mods: %s needs %s, which did not start; it stays off", m.info->id, m.required.c_str());
      continue;
    }
    hooks::begin_group();
    m.started = m.start(api);
    if (m.started) g_startOrder.push_back(id);
    else hooks::rollback_group();
    PG_LOG("mods: %s %s", m.info->id, m.started ? "started" : "failed to start and stays off");
  }
  hooks::begin_group();  // nothing the host creates later belongs to a mod
}

void tick_all() {
  for (Mod& m : g_mods)
    if (m.started) m.tick();
}

void stop_all() {
  // Newest first, so a mod stops before the mod it requires.
  for (auto it = g_startOrder.rbegin(); it != g_startOrder.rend(); ++it) {
    Mod* m = loaded_mod(*it);
    if (m && m->started) {
      m->stop();
      PG_LOG("mods: %s stopped", m->info->id);
    }
  }
  g_startOrder.clear();
  for (Mod& m : g_mods) m.started = false;
}

std::vector<std::string> started_ids() {
  std::vector<std::string> ids;
  for (const Mod& m : g_mods)
    if (m.started) ids.emplace_back(m.info->id);
  return ids;
}

std::vector<presence::Mod> started_builds() {
  std::vector<presence::Mod> builds;
  for (const Mod& m : g_mods)
    if (m.started) builds.push_back({m.info->id, m.build});
  return builds;
}

bool started_allow_vanilla() {
  for (const Mod& m : g_mods)
    if (m.started && !(m.info->flags & PG_MOD_VANILLA_PEERS)) return false;
  return true;
}

}  // namespace pg::mods
