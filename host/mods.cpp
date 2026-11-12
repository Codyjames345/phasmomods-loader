#include "mods.h"

#include <windows.h>

#include <algorithm>
#include <map>
#include <filesystem>
#include <iterator>

#include "contained.h"
#include "file_hash.h"
#include "pack_files.h"
#include "hooks.h"
#include "log.h"
#include "mod_deps.h"
#include "mod_exports.h"
#include "mod_list.h"
#include "quiet_load.h"
#include "text_files.h"
#include "text_table.h"

namespace pm::mods {
namespace {

namespace fs = std::filesystem;
using text::read_file;
using text::widen;

std::vector<Mod> g_mods;
std::vector<modlist::Refused> g_refused;
fs::path g_dataDirectory;
fs::path g_buildRecords;
std::vector<std::string> g_startOrder;  // the running mods' ids in the order they started
std::vector<moddeps::LeftOut> g_off;   // the chosen mods the last start left off, with why
bool g_starting = false;               // a mod's pmmod_start runs
std::string g_startError;              // the first host call that failed during it
std::string g_startingId;              // whose pmmod_start runs, while g_starting
std::vector<std::string> g_planned;    // the ids the current start starts, in order (moddeps::Plan::order)
std::map<std::string, std::string> g_failedPacks;  // packs their framework reported failed in this start, and why

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

/** What a launcher's copy of a mod is called for the player, until its manifest says: "GhostTells-1234-5-0.dll" is GhostTells. */
std::string copy_name(const fs::path& dll) {
  const std::wstring stem = dll.stem().wstring();
  return text::narrow(stem.substr(0, stem.find(L'-')));
}

/** A mod that stays off whatever the player chooses, with why: in the log and on its Mods panel row. */
void refuse(std::vector<modlist::Refused>& list, std::string name, std::string why) {
  PM_LOG("mods: %s stays off: %s", name.c_str(), why.c_str());
  list.push_back({std::move(name), std::move(why)});
}

Mod* find_mod(std::string_view id) {
  for (Mod& m : g_mods)
    if (id == m.manifest.id) return &m;
  return nullptr;
}

/** The game entries `m` requires that the host did not resolve. */
std::vector<std::string> unresolved_entries(const PmHostApi* api, const Mod& m) {
  std::vector<std::string> lacking;
  for (const std::string& key : m.entries)
    if (!api->game_entry(key.c_str())) lacking.push_back(key);
  return lacking;
}

std::string joined(const std::vector<std::string>& keys) {
  std::string text;
  for (const std::string& key : keys) text += (text.empty() ? "" : ", ") + key;
  return text;
}

/** A Mods panel row's reason for missing entries: the first by its key, since a row has room for little. The key is the
 *  mod's own text, shown with a generic instance's angle brackets as look-alikes (modid::shown_key). */
std::string lacking_row(const std::vector<std::string>& keys) {
  const std::string shown = modid::shown_key(keys[0]);
  const std::string first = modid::display_safe(shown, modid::kMaxNameBytes) ? shown : i18n::text("mods.a-game-entry");
  return keys.size() > 1 ? i18n::text("mods.lacks-entries", {{"entry", first}, {"count", std::to_string(keys.size() - 1)}})
                         : i18n::text("mods.lacks-entry", {{"entry", first}});
}

/** The game entries `m` requires whose calls the host could not track, separated by ", "; empty when none. */
std::string untracked_entries(bool (*untracked)(const char* key), const Mod& m) {
  std::string lost;
  if (untracked)
    for (const std::string& key : m.entries)
      if (untracked(key.c_str())) lost += (lost.empty() ? "" : ", ") + key;
  return lost;
}

std::string build_of(const fs::path& dll) {
  char hex[65];
  return build::sha256_file(dll.c_str(), hex) ? std::string(hex, 16) : std::string(16, '0');
}

/** Keeps `m` off for this run of the game: its DLL was refused once loaded. */
bool refuse_loaded(Mod& m, std::string row, std::string log) {
  m.refusedRow = std::move(row);
  m.refusedLog = std::move(log);
  return false;
}

/** Whether a loaded mod other than `m` already has this channel. */
bool channel_taken(const Mod& m, uint8_t channel) {
  for (const Mod& other : g_mods)
    if (&other != &m && other.info && other.info->channel == channel) return true;
  return false;
}

/**
 * Loads `m`'s DLL the first time it is needed, and checks what only the DLL can say. False with `m.refusedRow` and
 * `m.refusedLog` when it is refused. A DLL that loaded stays mapped, refused or not, so it is never loaded twice.
 */
bool load_dll(Mod& m) {
  if (!m.refusedRow.empty()) return false;
  if (m.info || !m.build.empty()) return true;
  // A content pack has no code: nothing loads, and its build is a hash of its copied files (core/pack_files.h).
  if (m.manifest.is_pack()) {
    m.build = packfiles::build_of(m.dll);
    PM_LOG("mods: found %s %s (%s, a content pack for %s, build %s)", m.manifest.id.c_str(), m.manifest.version.c_str(),
           m.manifest.name.c_str(), m.manifest.contentPackFor.c_str(), m.build.c_str());
    return true;
  }
  // Only a real DLL path ever reaches the loader: never a content pack's stand-in, whatever its manifest claims.
  if (m.dll.filename().empty() || packfiles::is_stand_in(m.dll))
    return refuse_loaded(m, i18n::text("mods.not-a-mod"), "it names no DLL");
  HMODULE module = dll::load_mod(m.dll.c_str());
  if (!module) {
    const std::string error = std::to_string(GetLastError());
    return refuse_loaded(m, i18n::text("mods.dll-unloadable"), "its DLL could not be loaded (Windows error " + error + ")");
  }
  HMODULE pinned = nullptr;
  GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                     reinterpret_cast<LPCWSTR>(module), &pinned);  // detours in it must never be unmapped
  // A mod built before the rename exports the old names (core/mod_exports.h): it loads, and the log says so once.
  bool oldNames = false;
  const auto find = [&](std::string_view name) {
    const modexports::Found found =
        modexports::find([&](const char* full) { return reinterpret_cast<void*>(GetProcAddress(module, full)); }, name);
    oldNames = oldNames || found.old;
    return found.address;
  };
  auto infoOf = reinterpret_cast<PmModInfoFn>(find("info"));
  const auto start = reinterpret_cast<PmModStartFn>(find("start"));
  const auto tick = reinterpret_cast<PmModTickFn>(find("tick"));
  const auto stop = reinterpret_cast<PmModStopFn>(find("stop"));
  // The contract forbids a throw here, and the SDK's glue guards it, but this DLL is anybody's.
  bool threw = false;
  const PmModInfo* info = !infoOf ? nullptr : host::contained("mods", "asking a mod what it is", static_cast<const PmModInfo*>(nullptr), [&] {
    threw = true;  // until its call returns
    const PmModInfo* answer = infoOf();
    threw = false;
    return answer;
  });
  if (threw) return refuse_loaded(m, i18n::text("mods.info-threw"), "its pmmod_info threw");
  if (!info || !start || !tick || !stop || !info->id || !info->name)
    return refuse_loaded(m, i18n::text("mods.not-a-mod"), "its DLL is not a mod for this loader");
  if (!pm_api_compatible(PMMOD_API_VERSION, info->apiVersion) || info->channel == 0)
    return refuse_loaded(m, i18n::text("mods.other-version"), "it was built for another version of the loader");
  if (m.manifest.id != info->id)
    return refuse_loaded(m, i18n::text("mods.id-mismatch"),
                         "the manifest says " + m.manifest.id + ", the DLL says " + info->id);
  if (channel_taken(m, info->channel))
    return refuse_loaded(m, i18n::text("mods.channel-taken"),
                         "it repeats channel " + std::to_string(info->channel) + " of a mod already loaded");
  // The player agreed on the manifest's word. A manifest that says so when the DLL does not only warns too much.
  if ((info->flags & PM_MOD_RECORDS_VOICE) && !m.manifest.recordsVoice)
    return refuse_loaded(m, i18n::text("mods.voice-undeclared"), "its DLL records voice, which its manifest does not say");
  // A mod built before the game API has no pmmod_requires and requires no game entry.
  if (auto listRequired = reinterpret_cast<PmModRequiresFn>(find("requires"))) {
    const char* const* keys = nullptr;
    threw = true;
    const size_t n = host::contained("mods", "asking a mod what it requires", size_t{0}, [&] {
      const size_t count = listRequired(&keys);
      threw = false;
      return count;
    });
    if (threw) return refuse_loaded(m, i18n::text("mods.requires-threw"), "its pmmod_requires threw");
    for (size_t i = 0; keys && i < n; ++i)
      if (keys[i]) m.entries.emplace_back(keys[i]);
  }
  if (oldNames)
    PM_LOG("mods: %s exports the names of contract 2.1 and before (%s*); they load until the next major contract version",
           info->id, std::string(modexports::kOldPrefix).c_str());
  m.info = info;
  m.start = start;
  m.tick = tick;
  m.stop = stop;
  m.build = build_of(m.dll);
  PM_LOG("mods: loaded %s %s (%s, build %s)", info->id, m.manifest.version.empty() ? "(version unknown)" : m.manifest.version.c_str(),
         m.manifest.name.c_str(), m.build.c_str());
  return true;
}

}  // namespace

modlist::Place place_beside(const fs::path& hostFolder, const fs::path& bin) {
  std::error_code compared, looked;
  if (fs::equivalent(hostFolder, bin, compared)) return modlist::Place::Elsewhere;
  if (!compared) return modlist::Place::InstallFolder;
  // No bin is the usual reason, on a first run. Any other leaves it unknown, and unknown is not the install folder.
  return fs::exists(bin, looked) || looked ? modlist::Place::Elsewhere : modlist::Place::InstallFolder;
}

Installed find(const fs::path& dataDirectory, const fs::path& hostFolder, unsigned long pid) {
  Installed installed;
  std::error_code error;
  // Only the launcher's own copy of the host, in bin, runs what the launcher listed for this process.
  // Read once, so a later process reusing the id never picks up a stale list.
  const fs::path bin = dataDirectory / L"bin";
  const fs::path session = bin / (L"session-" + std::to_wstring(pid) + L".txt");
  const bool inBin = fs::equivalent(hostFolder, bin, error);
  if (inBin && fs::exists(session, error)) {
    const std::string list = read_file(session);
    for (const std::string& p : modlist::parse_session(list)) {
      fs::path dll = widen(p);
      modlist::Manifest m;
      std::string why;
      if (modlist::read_copy(dll, m, why) == modlist::Reading::Readable) installed.found.push_back({dll, std::move(m)});
      else refuse(installed.refused, copy_name(dll), why);
    }
    for (modlist::Refused& r : modlist::parse_session_refused(list)) refuse(installed.refused, std::move(r.name), std::move(r.why));
    installed.buildRecords = widen(modlist::parse_session_builds(list));
    fs::remove(session, error);
    PM_LOG("mods: %zu listed by the launcher", installed.found.size());
    return installed;
  }
  if (hostFolder.empty()) {  // "mods" alone would be a folder in the game's working directory
    PM_LOG("mods: the host's own folder could not be read, so no mod is found beside it");
    return installed;
  }
  installed.buildRecords = hostFolder / L"builds";
  std::vector<fs::path> folders;
  for (const auto& entry : fs::directory_iterator(hostFolder / L"mods", error))
    if (entry.is_directory(error)) folders.push_back(entry.path());
  std::sort(folders.begin(), folders.end());
  const modlist::Place place = place_beside(hostFolder, bin);
  for (const fs::path& folder : folders) {
    modlist::Manifest m;
    std::string why;
    const modlist::Reading reading = modlist::read_folder(folder, m, why, place);
    if (reading == modlist::Reading::Refused) refuse(installed.refused, text::narrow(folder.filename().wstring()), why);
    if (reading != modlist::Reading::Readable) continue;
    fs::path dll = m.is_pack() ? packfiles::stand_in(folder) : folder / widen(m.dll);
    installed.found.push_back({std::move(dll), std::move(m)});
  }
  PM_LOG("mods: %zu found beside the host", installed.found.size());
  return installed;
}

void read_installed(const std::wstring& dataDirectory) { set_installed(dataDirectory, find(dataDirectory, host_folder(), GetCurrentProcessId())); }

void set_installed(const fs::path& dataDirectory, const Installed& installed) {
  g_mods.clear();
  g_startOrder.clear();
  g_off.clear();
  g_refused = installed.refused;
  g_dataDirectory = dataDirectory;
  g_buildRecords = installed.buildRecords;
  for (const Found& f : installed.found) {
    if (find_mod(f.manifest.id)) {
      refuse(g_refused, f.manifest.name, i18n::text("mods.id-repeated"));
      continue;
    }
    if (f.manifest.oldFormat) PM_LOG("mods: %s has a mod.ini, the old format; its version shows as unknown", f.manifest.id.c_str());
    if (!f.manifest.unknownFields.empty()) {
      std::string fields;
      for (const std::string& field : f.manifest.unknownFields) fields += (fields.empty() ? "" : ", ") + field;
      PM_LOG("mods: %s's manifest has fields this loader does not know, which it ignores: %s", f.manifest.id.c_str(), fields.c_str());
    }
    Mod m;
    m.dll = f.dll;
    m.manifest = f.manifest;
    g_mods.push_back(std::move(m));
  }
}

const std::vector<Mod>& installed() { return g_mods; }

const std::vector<modlist::Refused>& refused() { return g_refused; }

const fs::path& build_records() { return g_buildRecords; }

std::vector<std::string> chosen() {
  std::vector<std::string> ids;
  for (const Mod& m : g_mods) ids.push_back(m.manifest.id);
  // A choice saved before a mod gained a requirement still starts what that mod needs.
  return moddeps::with_requirements(modlist::load_choice(g_dataDirectory / L"launcher.ini", ids), manifests());
}

bool save_chosen(const std::vector<std::string>& ids) { return modlist::save_choice(g_dataDirectory / L"launcher.ini", ids); }

std::vector<modlist::Manifest> manifests() {
  std::vector<modlist::Manifest> all;
  for (const Mod& m : g_mods) all.push_back(m.manifest);
  return all;
}

std::vector<consent::Selected> notice_list() {
  std::vector<consent::Selected> list;
  for (const Mod& m : g_mods) list.push_back({m.manifest.name, m.manifest.recordsVoice});
  return list;
}

void start(const PmHostApi* api, const std::vector<std::string>& ids, Group group) {
  const std::vector<modlist::Manifest> all = manifests();
  const moddeps::Plan plan = moddeps::plan(all, ids);
  g_planned = plan.order;
  g_failedPacks.clear();
  for (const std::string& note : plan.notes) PM_LOG("mods: %s", note.c_str());
  // The dependencies are settled from the manifests, so a mod they leave off never loads. Its DLL, what only that can say
  // (its game entries) and its start are judged here, one mod at a time, and a mod that does not start takes those that
  // require it with it.
  moddeps::Run run = moddeps::run(plan, all, [&](const std::string& id, std::string& row, std::string& log) {
    Mod& m = *find_mod(id);
    if (m.started) return true;
    if (!load_dll(m)) {
      row = m.refusedRow;
      log = m.refusedLog;
      return false;
    }
    if (const auto failed = g_failedPacks.find(id); failed != g_failedPacks.end()) {
      row = failed->second;
      log = "its framework could not use it (" + failed->second + ")";
      return false;
    }
    if (m.manifest.is_pack()) {  // data only: started once its framework has, which the dependency order guarantees
      m.started = true;
      g_startOrder.push_back(id);
      PM_LOG("mods: %s started (a content pack)", id.c_str());
      return true;
    }
    if (const std::vector<std::string> lacking = unresolved_entries(api, m); !lacking.empty()) {
      row = lacking_row(lacking);
      log = "it needs game entries this game build lacks (" + joined(lacking) + ")";
      return false;
    }
    if (const std::string lost = untracked_entries(group.untracked, m); !lost.empty()) {
      row = i18n::text("mods.untracked");
      log = "it needs calls of game methods the host could not track (" + lost + ")";
      return false;
    }
    hooks::begin_group();
    if (group.begin) group.begin(id);
    g_startError.clear();
    g_starting = true;
    g_startingId = id;
    m.started = m.start(api);
    g_starting = false;
    g_startingId.clear();
    // Any failure of the host's during the start keeps the mod off, whatever its start answered: it never runs half there.
    std::string failure;
    if (const std::string hook = hooks::group_failure(); !hook.empty()) failure = "a hook could not be made (" + hook + ")";
    else if (!g_startError.empty()) failure = "a call to the host failed (" + g_startError + ")";
    else if (const std::string refused = group.refusal ? group.refusal() : std::string(); !refused.empty())
      failure = "the host refused its content (" + refused + ")";
    if (m.started && !failure.empty()) {
      m.stop();
      m.started = false;
    }
    if (group.end) group.end(m.started);
    if (!m.started) {
      hooks::rollback_group();
      row = "It did not start. mods.log says why";
      log = failure.empty() ? "its pmmod_start failed" : failure;
      return false;
    }
    g_startOrder.push_back(id);
    PM_LOG("mods: %s started", m.info->id);
    return true;
  });
  hooks::begin_group();  // nothing the host creates later belongs to a mod
  for (const moddeps::LeftOut& o : run.off) PM_LOG("mods: %s stays off: %s", o.id.c_str(), o.log.c_str());
  // This start speaks only for what it tried: a room's restart of a few mods leaves the reasons of those it never asked for.
  std::erase_if(g_off, [&](const moddeps::LeftOut& o) {
    return std::find(ids.begin(), ids.end(), o.id) != ids.end() || started(o.id.c_str()) ||
           std::any_of(run.off.begin(), run.off.end(), [&](const moddeps::LeftOut& now) { return now.id == o.id; });
  });
  g_off.insert(g_off.end(), std::make_move_iterator(run.off.begin()), std::make_move_iterator(run.off.end()));
}

std::string off_reason(const std::string& id) {
  for (const moddeps::LeftOut& o : g_off)
    if (o.id == id) return o.row;
  const Mod* m = find_mod(id);
  return m ? m->refusedRow : std::string();
}

bool started(const char* id) {
  if (!id) return false;
  const Mod* m = find_mod(id);
  return m && m->started;
}

const char* name_of(const char* id) {
  if (!id) return nullptr;
  const Mod* m = find_mod(id);
  return m ? m->manifest.name.c_str() : nullptr;
}

std::vector<const Mod*> packs_for(std::string_view frameworkId) {
  std::vector<const Mod*> packs;
  for (const Mod& m : g_mods) {
    if (!m.manifest.is_pack() || m.manifest.contentPackFor != frameworkId || g_failedPacks.count(m.manifest.id)) continue;
    if (m.started || std::find(g_planned.begin(), g_planned.end(), m.manifest.id) != g_planned.end()) packs.push_back(&m);
  }
  std::sort(packs.begin(), packs.end(), [](const Mod* a, const Mod* b) { return a->manifest.id < b->manifest.id; });
  return packs;
}

bool pack_failed(const std::string& packId, const std::string& why) {
  const Mod* m = find_mod(packId);
  if (!g_starting || !m || !m->manifest.is_pack() || m->manifest.contentPackFor != g_startingId || m->started) {
    PM_LOG("mods: a report that pack %s failed was ignored: only its framework's pmmod_start may make one, before it starts",
           packId.c_str());
    return false;
  }
  g_failedPacks[packId] = why;
  return true;
}

void start_error(const std::string& why) {
  if (g_starting && g_startError.empty()) g_startError = why;
}

void keep_off(const std::string& id, const std::string& why) {
  PM_LOG("mods: %s stays off: %s", id.c_str(), why.c_str());
  std::erase_if(g_off, [&](const moddeps::LeftOut& o) { return o.id == id; });
  g_off.push_back({id, "It did not start. mods.log says why", why, {}});
}

void settle(std::vector<std::string> ids, const std::function<std::vector<Dropped>()>& commit,
            const std::function<void(const std::vector<std::string>&)>& restart) {
  for (;;) {
    std::vector<std::string> off;
    for (const Dropped& d : commit()) {
      if (d.owner.empty() || !started(d.owner.c_str()) || std::find(off.begin(), off.end(), d.owner) != off.end()) continue;
      keep_off(d.owner, "the host dropped its content (" + d.why + ")");
      off.push_back(d.owner);
    }
    if (off.empty()) return;
    std::erase_if(ids, [&](const std::string& id) { return std::find(off.begin(), off.end(), id) != off.end(); });
    restart(ids);
  }
}

void tick_all() {
  for (Mod& m : g_mods)
    if (m.started && m.tick) m.tick();
}

void stop_all() {
  // Newest first, so a mod stops before the mods it depends on.
  for (const std::string& id : moddeps::stop_order(g_startOrder)) {
    Mod* m = find_mod(id);
    if (m && m->started) {
      if (m->stop) m->stop();  // a content pack has no code to stop
      m->started = false;  // at once: a mod stopping after it asks mod_started about it
      PM_LOG("mods: %s stopped", m->manifest.id.c_str());
    }
  }
  g_startOrder.clear();
}

std::vector<std::string> started_ids() { return g_startOrder; }

std::vector<presence::Mod> started_builds() {
  std::vector<presence::Mod> builds;
  for (const Mod& m : g_mods)
    if (m.started) builds.push_back({m.manifest.id, m.build, m.manifest.name, m.manifest.version});
  return builds;
}

void restart(const PmHostApi* api, const std::vector<std::string>& ids, Group group, const Restart& steps) {
  if (steps.withdraw) steps.withdraw();
  stop_all();
  if (steps.stopped) steps.stopped();
  start(api, ids, group);
}

}  // namespace pm::mods
