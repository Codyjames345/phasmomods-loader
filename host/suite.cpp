#include "suite.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "consent.h"
#include "entry_hooks.h"
#include "game.h"
#include "game_map.h"
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
#include "service_registry.h"
#include "ui.h"

namespace pg::suite {
namespace {

std::wstring g_dataDirectory;
PgHostApi g_api{};
presence::Presence g_presence;
services::Registry g_services;  // every table a running mod's
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
    if (name && !g_services.publish(name, table))
      PG_LOG("suite: service %s is already registered; the first table stays", name);
  } catch (...) {
    PG_LOG("suite: could not register service %s", name ? name : "?");
  }
}
const void* api_service(const char* name) {
  try {
    return g_services.find(name);
  } catch (...) {
    return nullptr;
  }
}

const PgGameEntry* api_game_entry(const char* key) {
  try {
    return key ? gamemap::mod_entry(key) : nullptr;
  } catch (...) {
    return nullptr;  // an allocation failure while looking it up: treat the entry as unresolved
  }
}
bool api_hook_entry(const char* key, void* detour, void** original, const char* name, bool allTwins) {
  try {
    const PgGameEntry* e = key ? gamemap::entry(key) : nullptr;
    if (!e || e->kind != PG_ENTRY_METHOD || !e->code) {
      PG_LOG("suite: game entry %s is unknown, unresolved or not a method; nothing hooked", key ? key : "?");
      return false;
    }
    return entry_hooks::hook(e->code, {e->twins, e->twinCount}, detour, original, name ? name : key, allTwins);
  } catch (...) {
    PG_LOG("suite: exception while hooking game entry %s", key ? key : "?");
    return false;
  }
}
void api_set_reference(void* obj, int32_t offset, void* value) {
  if (!obj) return;
  try {
    il2cpp::set_ref_field(obj, static_cast<size_t>(offset), value);
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception("suite: managed exception while storing a reference", e.ex);
  } catch (...) {
    PG_LOG("suite: exception while storing a reference");
  }
}
int32_t api_inside_entry(const char* key) { return context::depth(key); }

// The runtime services (version 2.1). Each runs `body` and contains what it throws: a managed
// exception is logged with its stack, anything else with `what`, and the caller gets `fallback`.
template <class R, class F>
R contained(const char* what, R fallback, F body) {
  try {
    return body();
  } catch (const Il2CppExceptionWrapper& e) {
    il2cpp::log_exception((std::string("suite: managed exception while ") + what).c_str(), e.ex);
  } catch (...) {
    PG_LOG("suite: exception while %s", what);
  }
  return fallback;
}

// The class of the Type entry `key`, or null when it is unknown, unresolved or not a type.
il2cpp::Class* type_class(const char* key) {
  const PgGameEntry* e = key ? gamemap::entry(key) : nullptr;
  return e && e->kind == PG_ENTRY_TYPE ? e->klass : nullptr;
}

// A service handed an object of the wrong kind reads no layout from it. It logs that once per
// service (`refused` is the service's own flag, atomic so two threads still log once) and answers 0
// or null.
bool refuse_once(std::atomic<bool>& refused, const char* service, const char* kind) {
  if (!refused.exchange(true))
    PG_LOG("suite: a mod passed %s an object that is not %s; it answers 0 or null", service, kind);
  return false;
}
bool string_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_string(obj) || refuse_once(refused, service, "a System.String");
}
bool array_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_array(obj) || refuse_once(refused, service, "a managed array");
}
bool list_given(const void* obj, const char* service) {
  static std::atomic<bool> refused{false};
  return il2cpp::is_list(obj) || refuse_once(refused, service, "a System.Collections.Generic.List");
}

void* api_string_new(const char* utf8) {
  return contained("making a string", static_cast<void*>(nullptr),
                   [&]() -> void* { return utf8 ? il2cpp::string_new(utf8) : nullptr; });
}
size_t api_string_utf8(const void* str, char* out, size_t size) {
  return contained("reading a string", size_t{0}, [&]() -> size_t {
    if (out && size) out[0] = '\0';
    if (!str || !string_given(str, "string_utf8")) return 0;
    return il2cpp::to_utf8(static_cast<const il2cpp::String*>(str), out, size);
  });
}
size_t api_array_length(const void* arr) {
  return contained("reading an array", size_t{0}, [&]() -> size_t {
    return arr && array_given(arr, "array_length") ? static_cast<const il2cpp::Array*>(arr)->max_length : 0;
  });
}
void* api_array_data(void* arr) {
  return contained("reading an array", static_cast<void*>(nullptr), [&]() -> void* {
    return arr && array_given(arr, "array_data") ? static_cast<il2cpp::Array*>(arr)->data<char>() : nullptr;
  });
}
void* api_array_new(const char* elementTypeKey, size_t length) {
  return contained("making an array", static_cast<void*>(nullptr), [&]() -> void* {
    il2cpp::Class* const klass = type_class(elementTypeKey);
    return klass ? il2cpp::array_new(klass, length) : nullptr;
  });
}
int32_t api_list_count(const void* list) {
  return contained("reading a list", int32_t{0}, [&]() -> int32_t {
    return list && list_given(list, "list_count") ? static_cast<const il2cpp::List*>(list)->size : 0;
  });
}
void* api_list_items(const void* list) {
  return contained("reading a list", static_cast<void*>(nullptr), [&]() -> void* {
    return list && list_given(list, "list_items") ? static_cast<const il2cpp::List*>(list)->items : nullptr;
  });
}
void* api_type_object(const char* typeKey) {
  return contained("making a type object", static_cast<void*>(nullptr),
                   [&]() -> void* { return il2cpp::type_object(type_class(typeKey)); });
}
void* api_object_new(const char* typeKey) {
  return contained("making an object", static_cast<void*>(nullptr), [&]() -> void* {
    il2cpp::Class* const klass = type_class(typeKey);
    return klass ? il2cpp::object_new(klass) : nullptr;
  });
}
bool api_is_instance(const void* obj, const char* typeKey) {
  return contained("checking an object's type", false, [&] { return il2cpp::is_instance(obj, type_class(typeKey)); });
}
bool api_alive(const void* unityObject) {
  return contained("checking an object is alive", false, [&] { return game::alive(const_cast<void*>(unityObject)); });
}
const void* api_virtual_method(const void* obj, const char* methodKey) {
  return contained("finding a virtual method", static_cast<const void*>(nullptr), [&]() -> const void* {
    const PgGameEntry* e = methodKey ? gamemap::entry(methodKey) : nullptr;
    if (!obj || !e || e->kind != PG_ENTRY_METHOD || !e->method || !il2cpp::is_instance(obj, e->klass)) return nullptr;
    return il2cpp::virtual_method(const_cast<void*>(obj), e->method);
  });
}
uint32_t api_gc_hold(void* obj) {
  return contained("holding an object", uint32_t{0}, [&] { return il2cpp::gc_hold(obj); });
}
void api_gc_release(uint32_t handle) {
  try {
    il2cpp::gc_release(handle);
  } catch (...) {
    PG_LOG("suite: exception while releasing an object");
  }
}

// The static field entry `key`: its class and FieldInfo, or null when it is unknown, unresolved, not
// a field or not static.
const PgGameEntry* static_field(const char* key) {
  const PgGameEntry* e = key ? gamemap::entry(key) : nullptr;
  const sdk::Result* r = e ? gamemap::result(key) : nullptr;
  if (!e || !r || e->kind != PG_ENTRY_FIELD || !(r->desc->flags & sdk::kStatic) || !e->klass || !e->field)
    return nullptr;
  return e;
}

// Whether a managed exception's stack from a static field access was logged: once in all, since a
// failed static constructor throws again on every access.
std::atomic<bool> g_staticStackLogged{false};

// A static field access that failed: logged once per key, with `why`. Called from catch handlers too,
// so it throws nothing.
void static_refused(const char* key, const char* why) {
  try {
    static std::mutex mutex;
    static std::set<std::string> logged;
    const std::lock_guard lock(mutex);
    if (logged.insert(key).second) PG_LOG("suite: static field %s: %s; it answers false", key, why);
  } catch (...) {
    // Logging is best effort; the caller answers false either way.
  }
}

// Runs a static field access, `access(entry)`, for the entry `key`. False, logged once per key, when
// the entry is not a resolved static field, the access refuses (a size other than the field's), or
// it throws (the class's static constructor, say).
template <class F>
bool static_access(const char* key, const char* what, F access) {
  if (!key) return false;
  try {
    const PgGameEntry* e = static_field(key);
    if (!e) {
      static_refused(key, "it is unknown, unresolved or not a static field");
      return false;
    }
    if (access(e)) return true;
    static_refused(key, "the size given is not the size of its value");
  } catch (const Il2CppExceptionWrapper& ex) {
    if (!g_staticStackLogged.exchange(true)) il2cpp::log_exception((std::string("suite: ") + what).c_str(), ex.ex);
    static_refused(key, "a managed exception was thrown");
  } catch (...) {
    static_refused(key, "an exception was thrown");
  }
  return false;
}
bool api_static_get(const char* key, void* out, size_t size) {
  return out && static_access(key, "reading a static field",
                              [&](const PgGameEntry* e) { return il2cpp::static_get(e->klass, e->field, out, size); });
}
bool api_static_set(const char* key, const void* in, size_t size) {
  return in && static_access(key, "writing a static field",
                             [&](const PgGameEntry* e) { return il2cpp::static_set(e->klass, e->field, in, size); });
}

void api_log_exception(const char* where, const void* exception) {
  try {
    il2cpp::log_exception(where ? where : "mod", exception);
  } catch (...) {
    PG_LOG("suite: exception while logging a managed exception");
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

bool create_hooks() { return hostnet::create_hooks() && context::install(&gamemap::entry); }

bool start(void* gameAssembly, void* unityPlayer) {
  gamemap::resolve_icalls();  // on the main thread, before any mod starts
  g_dataDirectory = data_directory();
  if (g_dataDirectory.empty()) {
    PG_LOG("suite: the local application-data folder is unavailable; set LOCALAPPDATA and restart the game");
    return false;
  }
  g_api = {PGMOD_API_VERSION, gameAssembly, unityPlayer, g_dataDirectory.c_str(),
           &api_log, &api_create_hook, &api_enable_hooks, &api_send, &api_set_receiver,
           &api_peer_has_mod, &api_register_service, &api_service,
           &api_game_entry, &api_hook_entry, &api_set_reference, &api_inside_entry,
           &api_string_new, &api_string_utf8, &api_array_length, &api_array_data, &api_array_new,
           &api_list_count, &api_list_items, &api_type_object, &api_object_new, &api_is_instance,
           &api_alive, &api_virtual_method, &api_gc_hold, &api_gc_release, &api_log_exception,
           &api_static_get, &api_static_set};
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
