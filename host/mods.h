#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "consent_text.h"
#include "mod_api.h"
#include "mod_list.h"
#include "presence.h"

// The mods the host runs. Every installed mod is found, in the launcher's session file for this process
// (bin\session-<pid>.txt) or, when the host was loaded some other way, in the "mods" folder beside the
// host. Until the player agrees the host reads their manifests and nothing else: the notice and the Mods
// panel build from those, and no mod's DLL is mapped into the game, so none of its code runs. A DLL is
// loaded the first time its mod starts, so a mod that is off is not loaded until it is switched on, and
// "Play without mods" loads none. Once loaded it is pinned for the process lifetime, even if it is then
// refused: its DllMain has run, and what it set going cannot be known, so unmapping it is not safe. Mods may
// be stopped and started again (the host removes their hooks in between). Main thread only.
namespace pm::mods {

struct Mod {
  std::filesystem::path dll;
  modlist::Manifest manifest;  // who it is, as its manifest.json says: all the host knows until its DLL loads
  // From its DLL. Null and empty until it has loaded and passed the checks in start.
  const PmModInfo* info = nullptr;  // its id equals the manifest's
  PmModStartFn start = nullptr;
  PmModTickFn tick = nullptr;
  PmModStopFn stop = nullptr;
  std::string build;                 // the first 16 hex digits of its DLL's SHA-256: which build it is
  std::vector<std::string> entries;  // from its pmmod_requires: the game entries it cannot run without
  // Its network channel once the DLL has loaded and passed the checks: the one it declared, or, when
  // it declared 0, the one derived from its id (core/mod_ids.h hashed_channel). 0 until then.
  uint8_t channel = 0;
  bool started = false;
  // Why its DLL was refused once loaded, for its row and the log; empty when it was not. It lasts until the game restarts.
  std::string refusedRow, refusedLog;
};

struct Found {
  std::filesystem::path dll;
  modlist::Manifest manifest;
};

// What the host found: the mods it may run, those that stay off whatever the player chooses, each with
// why, and the folder of checked-build records in the package they came from (host/build_check.h).
struct Installed {
  std::vector<Found> found;
  std::vector<modlist::Refused> refused;
  std::filesystem::path buildRecords;
};

/**
 * Every installed mod: those the launcher listed for process `pid` when the host is the launcher's
 * copy in <dataDirectory>/bin (the list is then deleted; each copy's manifest is beside it as
 * <copy>.json, or <copy>.ini for an old-format mod), else every mod in the mods folder beside the
 * host, in folder order. A bad or missing manifest refuses that mod, logged; a folder with neither a
 * manifest nor a DLL is no mod. The folder beside the host is the install folder, unless the host is a
 * copy in <dataDirectory>/bin: a mod there that claims the project's author is refused
 * (modlist::reserved_author_refusal). The launcher judged the mods it listed in their own folders. An empty
 * `hostFolder` (the host's own path could not be read) finds nothing beside it.
 */
Installed find(const std::filesystem::path& dataDirectory, const std::filesystem::path& hostFolder, unsigned long pid);
// Where a mods folder beside a host in `hostFolder` is: a host in `bin`, the data folder's copy, has no install folder beside
// it. When the two cannot be compared and a `bin` is there, it counts as that, since nothing shows it is not.
modlist::Place place_beside(const std::filesystem::path& hostFolder, const std::filesystem::path& bin);
void read_installed(const std::wstring& dataDirectory);  // find, for this process: maps no DLL
// Takes `installed.found` as the installed mods (what read_installed does with every one it finds), logging what their
// manifests say that this loader passes over. It maps no DLL. A mod that repeats an id found before it is refused, logged
// and listed in refused(). Nothing refused stops the others.
void set_installed(const std::filesystem::path& dataDirectory, const Installed& installed);
const std::vector<Mod>& installed();  // in the order found; each with its manifest, whether or not its DLL has loaded
const std::vector<modlist::Refused>& refused();  // the mods that stay off, for the Mods panel
const std::filesystem::path& build_records();    // the folder of checked-build records in the package they came from

std::vector<modlist::Manifest> manifests();  // the installed mods' manifests
std::vector<consent::Selected> notice_list();  // what the consent notice names, from the manifests
std::vector<std::string> chosen();  // the saved choice (launcher.ini), every mod when none is saved
bool save_chosen(const std::vector<std::string>& ids);

// Starts the installed mods named in `ids` that are not running, as host/mod_deps.h plans it from their
// manifests: each after every dependency of it that starts, otherwise in the order of `ids`, and each
// as its own hook group (a mod that fails to start has its hooks removed). A mod the plan leaves off is never
// loaded. Each other has its DLL loaded here if it has not been, with no Windows error dialog, and stays off for good,
// for this run of the game, when the DLL does not load or is no mod, when it was built against another major version of
// the host contract or a newer minor one, when its PmModInfo.id is not its manifest's id, when the channel it declares is
// not one that mod may name (1 to 63 are the loader's own mods', and 64 to 255 are what the loader hands out from ids),
// or when its channel is one another loaded mod uses, in which case every mod of this start not yet running on that
// channel stays off naming the others by id: channels are settled across all the planned mods, after loading their DLLs
// and before any of them starts, so a colliding mod never runs (a mod already running from an earlier start keeps its
// channel, and only the newcomers to it are refused). A planned mod's DLL is loaded here even when it then stays off
// for another reason, because its channel counts against the others. It also stays off when it says it records voice and
// its manifest does not, since the player agreed on the manifest's word. It stays off for now when one of its game entries api->game_entry does not give, or its pmmod_start
// fails. Each of those takes the mods that require it with it. Each is logged
// with the chain of why, and off_reason says it for the Mods panel. `group` is what the host records
// for a mod besides its hooks (its content): `begin` runs before each pmmod_start and `end` after it,
// told whether the mod started, so what a failed mod recorded goes with its hooks. `untracked` says a
// game method's calls could not be tracked (context::tracking_failed): a mod that requires one stays
// off too, logged, since pm::api::inside would answer it false for ever. `begin` is given the mod's id. `refusal`, asked
// after each pmmod_start and before `end`, says why the host refused something the mod registered during that start (a
// ghost while custom content is off, say), or nothing. A mod whose start met any failure of the host's stays off whatever
// its pmmod_start answered: a refusal, a hook that could not be made or switched on (hooks::group_failure), or a host call
// that failed (start_error). The host stops it and removes its hooks and content, so no mod has to check what its own
// calls returned.
struct Group {
  void (*begin)(const std::string& id) = nullptr;
  void (*end)(bool started) = nullptr;
  bool (*untracked)(const char* key) = nullptr;
  std::string (*refusal)() = nullptr;
};
void start(const PmHostApi* api, const std::vector<std::string>& ids, Group group = {});
// The content packs of the framework `frameworkId` that the current start starts or has started, less those it
// reported failed, in id order (PmHostApi::content_packs). Main thread.
std::vector<const Mod*> packs_for(std::string_view frameworkId);
// During the pmmod_start of the framework of the pack `packId`, before the pack starts: the pack stays off for this
// start, with `why` as its Mods panel reason (PmHostApi::content_pack_failed). False, logged, otherwise. Main thread.
bool pack_failed(const std::string& packId, const std::string& why);
// During a mod's pmmod_start: notes `why` a host call the mod made failed (the first one counts), which keeps that mod
// off (start, above). Outside a start it does nothing. Main thread.
void start_error(const std::string& why);
// After a start: the mod `id`, which started, is to stay off for that start, for `why` (the host dropped its content once
// every mod had started, say). Logged and kept as its off_reason until a start tries it again. The caller then restarts
// the mods without it, so the mods that require it stay off with it. Main thread.
void keep_off(const std::string& id, const std::string& why);
// Content dropped once every mod had started: its owner's id (empty for none) and why.
struct Dropped {
  std::string owner, why;
};
// After a start of `ids`: `commit` settles the content and says what it dropped. Each started owner of dropped content is
// kept off (keep_off, "the host dropped its content (...)") and `restart` starts the mods again without it, then `commit`
// runs again, until nothing more is dropped. Each round leaves one mod or more off, so it ends. Main thread.
void settle(std::vector<std::string> ids, const std::function<std::vector<Dropped>()>& commit,
            const std::function<void(const std::vector<std::string>&)>& restart);
// Why the last start that tried the mod `id` left it off, for its Mods panel row ("Needs Spectral Flame"), or why its DLL
// was refused once loaded, chosen or not; empty when it runs or nothing has tried it. A start that does not ask for it, as a
// room's restart of a few mods does not, leaves its reason.
std::string off_reason(const std::string& id);
// Whether the mod `id` runs on this client now: the host contract's mod_started. False for null and for an id no
// installed mod has.
bool started(const char* id);
// The name the manifest of the installed mod `id` gives it, the Mods panel's: the host contract's mod_name. Null for null
// and for an id no installed mod has. It lasts until the mods are listed afresh.
const char* name_of(const char* id);
// The installed mod at `index` for the host contract's mod_entry (core/mod_api.h): fills what fits in `out->size`. Its
// strings are the host's, kept until the mods are listed afresh or the next ask for the same index. False past the end,
// for null and for a size too small for `id`.
bool entry(int32_t index, PmModEntry* out);
void tick_all();
void stop_all();  // in reverse start order, so a mod stops before those it depends on; their hooks stay until the host removes them

// What surrounds a restart of the mods, whichever asked for it (the Mods panel's Apply out of a room, the lobby rules in a
// lobby, never during a contract): `withdraw` takes what they gave out of the game while they still run, and `stopped`
// removes what they left behind, once every one has stopped (their hooks, their network messages, their content).
struct Restart {
  void (*withdraw)() = nullptr;
  void (*stopped)() = nullptr;
};
// From the host's own tick, with no mod detour on the stack: withdraw, stop every mod, stopped, and start `ids` as start
// does.
void restart(const PmHostApi* api, const std::vector<std::string>& ids, Group group, const Restart& steps);

std::vector<std::string> started_ids();  // in the order they started, so starting them again keeps that order
std::vector<presence::Mod> started_builds();  // with each one's name and version, for the other players

}  // namespace pm::mods
