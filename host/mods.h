#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "mod_api.h"
#include "mod_list.h"
#include "presence.h"

// The mod DLLs the host runs. Every installed mod is found, in the launcher's session file for this
// process (bin\session-<pid>.txt) or, when the host was loaded some other way, in the "mods" folder
// beside the host. Each is loaded (and pinned for the process lifetime) at once, so the notice can
// name it and the in-game Mods panel can switch it on later; launcher.ini says which start. Mods may
// be stopped and started again (the host removes their hooks in between). Main thread only.
namespace pg::mods {

struct Mod {
  const PgModInfo* info = nullptr;
  PgModStartFn start = nullptr;
  PgModTickFn tick = nullptr;
  PgModStopFn stop = nullptr;
  bool started = false;
  std::string build;        // the first 16 hex digits of its DLL's SHA-256: which build it is
  std::string description;  // from its mod.ini, for the Mods panel
  std::string required;     // from its mod.ini: the id of the mod it cannot run without, or empty
};

struct Found {
  std::filesystem::path dll;
  std::string description;
  std::string required;
};

/**
 * Every installed mod: those the launcher listed for process `pid` when the host is the launcher's
 * copy in <dataDirectory>/bin (the list is then deleted; each copy's mod.ini is beside it as
 * <copy>.ini), else every valid mod in the mods folder beside the host, in folder order.
 */
std::vector<Found> find(const std::filesystem::path& dataDirectory, const std::filesystem::path& hostFolder,
                        unsigned long pid);
void load(const std::wstring& dataDirectory);
const std::vector<Mod>& loaded();

std::vector<modlist::Manifest> manifests();  // the loaded mods' ids, names and requirements
std::vector<std::string> chosen();  // the saved choice (launcher.ini), every mod when none is saved
bool save_chosen(const std::vector<std::string>& ids);

// Starts the loaded mods named in `ids` that are not running, each after the mod it requires and
// otherwise in load order, each as its own hook group (a mod that fails to start has its hooks
// removed). A mod whose requirement is not chosen, or did not start, stays off.
void start(const PgHostApi* api, const std::vector<std::string>& ids);
void tick_all();
void stop_all();  // newest first; their hooks stay until the host removes them

std::vector<std::string> started_ids();
std::vector<presence::Mod> started_builds();
bool started_allow_vanilla();  // every running mod has PG_MOD_VANILLA_PEERS (true when none runs)

}  // namespace pg::mods
