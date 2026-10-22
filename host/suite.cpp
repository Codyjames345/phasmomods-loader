#include "suite.h"

#include <windows.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "consent.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "join_guard.h"
#include "log.h"
#include "menu_panel.h"
#include "mods.h"
#include "mods_panel.h"
#include "net.h"
#include "notice.h"
#include "presence.h"
#include "room_actions.h"
#include "ui.h"

namespace pg::suite {
namespace {

std::wstring g_dataDirectory;
PgHostApi g_api{};
presence::Presence g_presence;
std::map<std::string, const void*> g_services;  // every one a running mod's
bool g_agreed = false;     // the player agreed to the notice: the mods are the host's to run
size_t g_modHooks = 0;     // hooks::mark() before the first mod started: everything after is the mods'
consent::Answer g_answer = consent::Answer::Pending;
std::vector<int> g_actors;

std::string mod_name(const std::string& id) {
  for (const mods::Mod& m : mods::loaded())
    if (id == m.info->id) return m.info->name;
  return id;
}

joinguard::Guard g_guard(&mod_name);

std::wstring data_directory() {
  wchar_t base[MAX_PATH];
  const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", base, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return {};
  std::wstring dir = std::wstring(base) + L"\\PhasmoMods";
  CreateDirectoryW(dir.c_str(), nullptr);
  return dir;
}

// The host's side of the API. Nothing thrown may cross into a mod (core/mod_api.h), so each entry
// point contains its own failures.
void api_log(const char* line) {
  try {
    PG_LOG("%s", line);
  } catch (...) {
    // Logging is best effort; a failed line is not worth failing the caller for.
  }
}
bool api_create_hook(void* target, void* detour, void** original, const char* name) {
  try {
    return hooks::create(target, detour, original, name);
  } catch (...) {
    PG_LOG("suite: exception while creating hook %s", name ? name : "?");
    return false;
  }
}
bool api_enable_hooks() {
  try {
    return hooks::enable_all();
  } catch (...) {
    PG_LOG("suite: exception while enabling hooks");
    return false;
  }
}
bool api_send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor) {
  try {
    // Outside private rooms the suite behaves as if it were not installed.
    return channel != hostnet::kHostChannel && game::private_room() && hostnet::send(channel, data, size, to, reliable, actor);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("suite: managed exception while sending a mod message", e.ex);
  } catch (...) {
    PG_LOG("suite: exception while sending a mod message");
  }
  return false;
}
void api_set_receiver(uint8_t channel, PgReceiver receiver) {
  if (channel != hostnet::kHostChannel) hostnet::set_receiver(channel, receiver);
}
bool api_peer_has_mod(int32_t actor, const char* id) {
  try {
    return id && g_presence.has(actor, id);
  } catch (...) {
    return false;  // an allocation failure while comparing: treat the peer as unmodded
  }
}
void api_register_service(const char* name, const void* table) {
  try {
    if (name) g_services[name] = table;
  } catch (...) {
    PG_LOG("suite: could not register service %s", name);
  }
}
const void* api_service(const char* name) {
  try {
    auto it = name ? g_services.find(name) : g_services.end();
    return it == g_services.end() ? nullptr : it->second;
  } catch (...) {
    return nullptr;
  }
}

void on_presence(int32_t sender, const uint8_t* data, size_t size) {
  if (game::private_room()) g_presence.receive(sender, data, size, GetTickCount64());
}

// Every installed mod: any of them can be switched on from the Mods panel once the player agreed.
std::vector<consent::Selected> installed() {
  std::vector<consent::Selected> out;
  for (const mods::Mod& m : mods::loaded()) out.push_back({m.info->name, (m.info->flags & PG_MOD_RECORDS_VOICE) != 0});
  return out;
}

void start_mods(const std::vector<std::string>& ids) {
  mods::start(&g_api, ids);
  g_presence.set_local(mods::started_builds(), mods::started_allow_vanilla());
}

// The Mods panel's Apply, from the host's own tick (no mod detour is on the stack) and never in a
// room: every running mod stops, newest first; every hook the mods made goes, newest first, so
// detours chained on one function unwind in order; their receivers and services go with them; then
// the chosen mods start afresh.
void restart_mods(const std::vector<std::string>& ids) {
  PG_LOG("suite: restarting the mods with the player's choice");
  mods::stop_all();
  hooks::rollback_to(g_modHooks);
  for (const mods::Mod& m : mods::loaded()) hostnet::set_receiver(m.info->channel, nullptr);
  g_services.clear();
  start_mods(ids);
  bool ok = true;
  const std::vector<std::string> running = mods::started_ids();
  for (const mods::Mod& m : mods::loaded())
    if (std::find(ids.begin(), ids.end(), m.info->id) != ids.end() &&
        std::find(running.begin(), running.end(), m.info->id) == running.end())
      ok = false;
  if (!mods::save_chosen(ids)) PG_LOG("suite: the choice could not be saved; the launcher keeps the old one");
  mods_panel::applied(ok);
}

// The join guard (host/join_guard.h): leave a lobby whose host runs other mods, and as the host
// remove those whose mods differ, or who have none while a running mod needs everyone to.
void tick_guard() {
  const bool inRoom = game::private_room();
  const bool lobby = inRoom && !game::level_controller() && room_actions::lobby_open();
  const joinguard::Frame frame{inRoom ? game::local_actor() : 0, game::master_actor(), g_actors, lobby, GetTickCount64()};
  const joinguard::Verdict verdict = g_guard.judge(frame, g_presence);
  if (verdict.leave) g_guard.left(room_actions::leave_lobby());
  for (int actor : verdict.kick) g_guard.kicked(actor, room_actions::kick(actor));
  if (!verdict.show.empty()) menu_panel::message("These mods don't match", verdict.show);
}

void tick_presence() {
  const bool inPrivate = game::private_room();
  if (inPrivate) game::room_actors(g_actors);
  else g_actors.clear();
  g_presence.observe(inPrivate ? game::local_actor() : 0, g_actors, GetTickCount64());
  const std::vector<uint8_t> hello = g_presence.take_announcement();
  if (!hello.empty() && !hostnet::send(hostnet::kHostChannel, hello.data(), hello.size(), PG_TO_OTHERS, true, 0))
    PG_LOG("suite: the presence announcement could not be sent");
}

}  // namespace

bool resolve() {
  return ui::resolve() && hostnet::resolve() && room_actions::resolve() && menu_panel::resolve() && mods_panel::resolve();
}

bool create_hooks() { return hostnet::create_hooks(); }

bool start(void* gameAssembly, void* unityPlayer) {
  g_dataDirectory = data_directory();
  if (g_dataDirectory.empty()) {
    PG_LOG("suite: the local application-data folder is unavailable; set LOCALAPPDATA and restart the game");
    return false;
  }
  g_api = {PGMOD_API_VERSION, gameAssembly, unityPlayer, g_dataDirectory.c_str(),
           &api_log, &api_create_hook, &api_enable_hooks, &api_send, &api_set_receiver,
           &api_peer_has_mod, &api_register_service, &api_service};
  hostnet::set_receiver(hostnet::kHostChannel, &on_presence);
  mods::load(g_dataDirectory);
  if (mods::loaded().empty()) {
    g_answer = consent::Answer::Declined;  // nothing to agree to: never block the game with the notice
    PG_LOG("suite: no mods to start");
    return true;
  }
  consent::begin(consent::notice(installed()), g_dataDirectory + L"\\consent.txt");
  return true;
}

void tick() {
  if (g_answer == consent::Answer::Pending) {
    g_answer = consent::tick();
    if (g_answer == consent::Answer::Agreed && !g_agreed) {
      g_modHooks = hooks::mark();
      start_mods(mods::chosen());
      g_agreed = true;
    }
  }
  tick_presence();
  if (g_agreed) {
    tick_guard();
    mods_panel::tick(!game::in_room());
    std::vector<std::string> choice;
    if (mods_panel::take_choice(choice) && !game::in_room()) restart_mods(choice);
    mods::tick_all();
  }
  menu_panel::tick_message();
  notice::tick();
}

void stop() {
  consent::hide();
  mods_panel::teardown();
  if (g_agreed) mods::stop_all();
  g_agreed = false;
  g_presence.set_local({}, false);
}

}  // namespace pg::suite
