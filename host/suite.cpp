#include "suite.h"

#include <windows.h>

#include <algorithm>
#include <cstring>
#include <atomic>
#include <cstddef>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "build_check.h"
#include "consent.h"
#include "contained.h"
#include "content_acting.h"
#include "content_difficulty.h"
#include "content_evidence.h"
#include "content_journal.h"
#include "content_localised.h"
#include "content_registry.h"
#include "content_target.h"
#include "content_text.h"
#include "content_types.h"
#include "entry_hooks.h"
#include "GhostController.h"
#include "game_api.h"
#include "game_map.h"
#include "host_game.h"
#include "hooks.h"
#include "join_guard.h"
#include "lobby_gate.h"
#include "log.h"
#include "menu_panel.h"
#include "mod_deps.h"
#include "mods.h"
#include "mod_assets.h"
#include "mod_ids.h"
#include "mod_session.h"
#include "mods_panel.h"
#include "net.h"
#include "notice.h"
#include "presence.h"
static_assert(pm::presence::kMaxMessage == PM_MAX_MESSAGE, "an announcement's parts fit the host channel");
static_assert(pm::moddeps::kMaxMods == pm::presence::kMaxMods, "the mods that may start are what another player hears");
#include "Photon.h"
#include "room_actions.h"
#include "room_watch.h"
#include "runtime_services.h"
#include "scene_engine.h"
#include "service_registry.h"
#include "services.h"
#include "start_close.h"
#include "start_flow.h"
#include "steering.h"
#include "text_files.h"
#include "text_table.h"
#include "UnityEvents.h"
#include "ui.h"

namespace pm::suite {
namespace {

using namespace runtime_services;

std::wstring g_dataDirectory;
PmHostApi g_api{};
presence::Presence g_presence;
services::Registry g_services;  // every table a running mod's
content::Registry g_content;    // every ghost and evidence a running mod registered
content::Agreement g_agreement;  // whether this client's mods equal the host's: gates what the mods gave content
content::Acting g_acting{g_content};  // what the contract's ghost acts as, and the rules running for it
bool g_manifestUnsent = false;  // the last content manifest due could not be sent
bool g_actingUnsent = false;    // the last acting-as message due could not be sent
bool g_agreed = false;     // the player agreed to the notice: the mods are the host's to run
bool g_contentCanHook = false;  // every game entry content needs resolved, so its hooks go in on "I agree"
std::vector<std::string> g_wanted;  // what ran before this room's rules switched mods off; empty while none are
bool g_resumeDue = false;  // a room that switched mods off was left, and they have yet to start again
size_t g_modHooks = 0;     // hooks::mark() before the first mod started: everything after is the mods'
consent::Answer g_answer = consent::Answer::Pending;
std::string g_offCause;  // why no mod runs when the player did not choose that, for the Mods panel; empty otherwise
std::vector<int> g_actors;

std::string mod_name(const std::string& id) {
  for (const mods::Mod& m : mods::installed())
    if (id == m.manifest.id) return m.manifest.name;
  return id;
}

// Which of this client's mods cannot run without one the lobby's rules switch off (host/mod_deps.h).
joinguard::Guard g_guard(&mod_name, [](const std::vector<std::string>& ids) { return moddeps::dependents(ids, mods::manifests()); });
// A contract's start closes the room first (host/start_close.h, the user's ruling of 2026-11-03).
startclose::Closer g_closer;

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
    PM_LOG("%s", line);
  } catch (...) {
    // Logging is best effort; a failed line is not worth failing the caller for.
  }
}
bool api_create_hook(void* target, void* detour, void** original, const char* name) {
  try {
    return hooks::create(target, detour, original, name);
  } catch (...) {
    PM_LOG("suite: exception while creating hook %s", name ? name : "?");
    mods::start_error("creating the hook " + std::string(name ? name : "?") + " threw");
    return false;
  }
}
bool api_enable_hooks() {
  try {
    return hooks::enable_all();
  } catch (...) {
    PM_LOG("suite: exception while enabling hooks");
    mods::start_error("switching its hooks on threw");
    return false;
  }
}
// A mod's messages go only to verified players, from a client that matches the host. Outside private rooms the suite behaves as
// if it were not installed.
bool send_mod_message(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor) {
  if (channel == hostnet::kHostChannel || !hostgame::private_room() || !g_agreement.agreed()) return false;
  if (to == PM_TO_MASTER) return hostnet::send(channel, data, size, to, reliable, actor);
  if (to == PM_TO_ACTOR) return g_agreement.verified(actor) && hostnet::send(channel, data, size, to, reliable, actor);
  std::vector<int> actors;
  if (!hostgame::room_actors(actors)) return false;
  const int local = hostgame::local_actor();
  std::vector<int> verified;
  for (int a : actors)
    if (a != local && g_agreement.verified(a)) verified.push_back(a);
  return hostnet::send_to_verified(channel, data, size, reliable, verified, g_agreement.unverified(actors).empty());
}
bool api_send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor) {
  try {
    return send_mod_message(channel, data, size, to, reliable, actor);
  } catch (const Il2CppExceptionWrapper& e) {
    host::log_managed("suite: managed exception while sending a mod message", e.ex);
  } catch (...) {
    PM_LOG("suite: exception while sending a mod message");
  }
  return false;
}
void api_set_receiver(uint8_t channel, PmReceiver receiver) {
  if (channel != hostnet::kHostChannel) hostnet::set_receiver(channel, receiver);
}
bool api_peer_has_mod(int32_t actor, const char* id) {
  try {
    return id && g_agreement.peer_runs(actor, modid::migrated_id(id));  // a mod built before the ids moved asks by the old one
  } catch (...) {
    return false;  // an allocation failure while comparing: treat the peer as unmodded
  }
}
void api_show_message(const char* title, const char* body) {
  host::contained("suite", "showing a mod's message", 0, [&] {
    if (title && body) menu_panel::mod_message(title, body);
    return 0;
  });
}
bool api_mod_started(const char* id) {
  return host::contained("suite", "asking whether a mod started", false, [&] {
    return id && mods::started(modid::migrated_id(id).c_str());  // a bundled mod's old id means that mod
  });
}
void api_register_service(const char* name, const void* table) {
  try {
    if (name && !g_services.publish(name, table)) {
      PM_LOG("suite: service %s is already registered; the first table stays", name);
      mods::start_error("the service " + std::string(name) + " is already registered");
    }
  } catch (...) {
    PM_LOG("suite: could not register service %s", name ? name : "?");
    mods::start_error("the service " + std::string(name ? name : "?") + " could not be registered");
  }
}
const void* api_service(const char* name) {
  try {
    return g_services.find(name);
  } catch (...) {
    return nullptr;
  }
}

const PmGameEntry* api_game_entry(const char* key) {
  try {
    return key ? gamemap::mod_entry(key) : nullptr;
  } catch (...) {
    return nullptr;  // an allocation failure while looking it up: treat the entry as unresolved
  }
}
// The resolved method entry `key` names, or null when it is unknown, unresolved or not a method.
const PmGameEntry* method_entry(const char* key) {
  const PmGameEntry* e = key ? gamemap::entry(key) : nullptr;
  return e && e->kind == PM_ENTRY_METHOD && e->code ? e : nullptr;
}
bool api_hook_entry(const char* key, void* detour, void** original, const char* name, bool allTwins) {
  try {
    const PmGameEntry* e = method_entry(key);
    if (!e) {
      PM_LOG("suite: game entry %s is unknown, unresolved or not a method; nothing hooked", key ? key : "?");
      mods::start_error("the game entry " + std::string(key ? key : "?") + " is unknown, unresolved or not a method");
      return false;
    }
    const bool hooked = entry_hooks::hook(e->code, {e->twins, e->twinCount}, detour, original, name ? name : key, allTwins);
    if (!hooked) mods::start_error("the game entry " + std::string(key) + " could not be hooked");
    return hooked;
  } catch (...) {
    PM_LOG("suite: exception while hooking game entry %s", key ? key : "?");
    mods::start_error("hooking the game entry " + std::string(key ? key : "?") + " threw");
    return false;
  }
}
bool api_hook_body(const char* key, uint32_t bodyIndex, void* detour, void** original, const char* name) {
  try {
    const PmGameEntry* e = method_entry(key);
    if (!e) {
      PM_LOG("suite: game entry %s is unknown, unresolved or not a method; nothing hooked", key ? key : "?");
      mods::start_error("the game entry " + std::string(key ? key : "?") + " is unknown, unresolved or not a method");
      return false;
    }
    const bool hooked = entry_hooks::hook_body(e->code, {e->twins, e->twinCount}, bodyIndex, detour, original, name ? name : key);
    if (!hooked) mods::start_error("body " + std::to_string(bodyIndex) + " of the game entry " + std::string(key) + " could not be hooked");
    return hooked;
  } catch (...) {
    PM_LOG("suite: exception while hooking game entry %s", key ? key : "?");
    mods::start_error("hooking the game entry " + std::string(key ? key : "?") + " threw");
    return false;
  }
}
uint32_t api_body_count(const char* key) {
  try {
    const PmGameEntry* e = method_entry(key);
    return e ? static_cast<uint32_t>(entry_hooks::bodies(e->code, {e->twins, e->twinCount}).size()) : 0;
  } catch (...) {
    return 0;  // an allocation failure while listing the bodies: treat the entry as unresolved
  }
}
int32_t api_inside_entry(const char* key) { return context::depth(key); }

// The scene facts (version 2.1): the game map keeps and checks them.
void* api_scene_find(const char* key, void* root) {
  return host::contained("suite", "finding a scene path", static_cast<void*>(nullptr),
                         [&]() -> void* { return gamemap::scene_find(key, root); });
}
bool api_animator_param(const char* key, const void* animator, int32_t* hash) {
  return host::contained("suite", "checking an Animator parameter", false,
                         [&] { return gamemap::animator_param(key, animator, hash); });
}

// Content (version 2.2): the registry records and agrees on it (sdk/runtime/content_registry.h).
int32_t api_ghost_register(const PmGhostDef* def) {
  return host::contained("content", "registering a ghost", int32_t{0}, [&] { return g_content.add_ghost(def); });
}
int32_t api_evidence_register(const PmEvidenceDef* def) {
  return host::contained("content", "registering an evidence", int32_t{0}, [&] { return g_content.add_evidence(def); });
}
int32_t api_content_type(const char* ghostId) {
  return host::contained("content", "numbering a ghost", int32_t{-1},
                         [&] { return ghostId ? g_content.type_of(ghostId) : -1; });
}
const char* api_content_ghost_id(int32_t type) {
  return host::contained("content", "naming a ghost type", static_cast<const char*>(nullptr),
                         [&] { return g_content.id_of(type); });
}
int32_t api_content_evidence(const char* evidenceId) {
  return host::contained("content", "numbering an evidence", int32_t{-1},
                         [&] { return evidenceId ? g_content.evidence_of(evidenceId) : -1; });
}
bool api_content_active() {
  return host::contained("content", "judging the room's content", false, [&] { return g_content.active(); });
}
bool api_content_player_controlled() {
  return host::contained("content", "asking Playable Ghost", false, [&] {
    const auto* pm = static_cast<const PmPlayableGhostService*>(g_services.find(PM_SERVICE_PLAYABLE_GHOST));
    return pm && pm->version >= 1 && pm->ghostControlled && pm->ghostControlled();
  });
}

int32_t api_ghost_rules(const char* ghostId, const PmGhostRules* rules) {
  return host::contained("content", "attaching rules", int32_t{0}, [&] { return g_content.add_rules(ghostId, rules); });
}
int32_t api_content_acting_as() {
  return host::contained("content", "asking what the ghost acts as", int32_t{-1}, [&] { return g_acting.acting_as(); });
}
int32_t api_journal_lines(const char* vanillaGhostId, const PmTextSet* lines, uint32_t count) {
  return host::contained("content", "adding journal lines", int32_t{0},
                         [&] { return g_content.add_lines(vanillaGhostId, lines, count); });
}
int32_t api_content_identity() {
  return host::contained("content", "asking the ghost's identity", int32_t{-1}, [&] { return g_acting.identity(); });
}
const char* api_content_evidence_look(const char* evidenceId) {
  return host::contained("content", "asking how the ghost changes an evidence's look",
                         static_cast<const char*>(nullptr), [&] { return g_acting.evidence_look(evidenceId); });
}

// The contract's ghost as a player controls it (core/mod_api.h, content_use_ability and after): the
// definition of the type it acts as answers.
bool api_content_use_ability() {
  return host::contained("content", "using the ghost's ability", false, [&] { return g_acting.use_ability(); });
}
const char* api_content_ability_text(bool description) {
  return host::contained("content", "reading the ghost's ability", static_cast<const char*>(nullptr), [&] {
    const content::Ghost* g = g_acting.acting_ghost();
    const content::Text* text = g ? (description ? &g->abilityDescription : &g->abilityName) : nullptr;
    return text && !text->empty() ? host::pick(*text, host::game_language(true)).c_str() : nullptr;
  });
}
int32_t api_content_hud_state() {
  return host::contained("content", "asking the ghost's state", int32_t{-1}, [&] { return g_acting.hud_state(); });
}
const char* api_content_hud_line(int32_t state) {
  return host::contained("content", "asking the ghost's HUD line", static_cast<const char*>(nullptr),
                         [&] { return g_acting.hud_line(state); });
}
bool api_content_held_back(int32_t kind) {
  return host::contained("content", "asking whether the ghost is held back", false,
                         [&] { return g_acting.held_back(kind); });
}
uint32_t api_content_player_view(float* xyz, uint32_t max) {
  return host::contained("content", "asking the ghost player's view", uint32_t{0},
                         [&] { return g_acting.player_view(xyz, max); });
}
uint32_t api_content_flags() {
  return host::contained("content", "asking the ghost's flags", uint32_t{0}, [&] {
    const content::Ghost* g = g_acting.acting_ghost();
    return g ? g->flags : 0u;
  });
}
uint32_t api_content_flags_of(int32_t type) {
  return host::contained("content", "asking a ghost type's flags", uint32_t{0}, [&] {
    const char* id = g_content.id_of(type);
    const content::Ghost* g = id ? g_content.ghost(id) : nullptr;
    return g ? g->flags : 0u;
  });
}
const char* api_mod_name(const char* id) {
  return host::contained("suite", "asking a mod's name", static_cast<const char*>(nullptr), [&] {
    return id ? mods::name_of(modid::migrated_id(id).c_str()) : nullptr;  // a bundled mod's old id means that mod
  });
}
// The texts content_packs hands out, one entry per pack id, never replaced or removed: the host contract says they last
// until the mods stop, and a pack's id, manifest and copied paths stay the same for the whole game process. A map's
// nodes never move, so the pointers stay valid across later calls.
struct PackTexts {
  std::string id, name, version;
  std::wstring folder, text;
};
std::map<std::string, PackTexts> g_packTexts;
uint32_t api_content_packs(const char* modId, PmContentPack* out, uint32_t max, uint32_t itemSize) {
  return host::contained("suite", "listing a mod's content packs", uint32_t{0}, [&]() -> uint32_t {
    // A caller built for 2.6 passes 2.6's size; the host fills each entry up to the caller's size, at most its own.
    if (!modId || itemSize < PM_CONTENT_PACK_SIZE_2_6) return 0;
    const size_t filled = std::min<size_t>(itemSize, sizeof(PmContentPack));
    const std::vector<const mods::Mod*> packs = mods::packs_for(modid::migrated_id(modId));
    for (uint32_t i = 0; i < packs.size(); ++i) {
      const mods::Mod* m = packs[i];
      std::filesystem::path text = m->dll;
      text.replace_extension(L".i18n.json");
      const PackTexts& t = g_packTexts.try_emplace(m->manifest.id, PackTexts{m->manifest.id, m->manifest.name, m->manifest.version,
                                                 modassets::beside_copy(m->dll).wstring(), text.wstring()}).first->second;
      if (!out || i >= max) continue;
      const PmContentPack entry{static_cast<uint32_t>(filled), t.id.c_str(), t.name.c_str(), t.version.c_str(),
                                t.folder.c_str(), t.text.c_str()};
      std::memcpy(reinterpret_cast<unsigned char*>(out) + static_cast<size_t>(i) * itemSize, &entry, filled);
    }
    return static_cast<uint32_t>(packs.size());
  });
}
void api_content_pack_failed(const char* packId, const char* why) {
  host::contained("suite", "noting a content pack its framework could not use", 0, [&] {
    if (packId) mods::pack_failed(packId, why && *why ? why : "its framework could not use it");
    return 0;
  });
}
bool api_mod_entry(int32_t index, PmModEntry* out) {
  return host::contained("suite", "listing the installed mods", false, [&] { return mods::entry(index, out); });
}
void* api_unity_action_new(void* target, void (*listener)(void*)) {
  // The game's listener shape takes the target and the hidden MethodInfo of the instance-method calling
  // convention; a mod's listener takes only the target and leaves the rest unread.
  return runtime_services::delegate_new(map::UnityAction_ctor.key, map::UnityAction_Invoke.key, target,
                                        reinterpret_cast<runtime_services::NativeListener>(listener));
}
bool api_room_agrees() {
  return host::contained("content", "asking whether this client matches the host", false, [&] { return g_agreement.agreed(); });
}
void api_content_player_move(const float* at, float* dx, float* dz) {
  host::contained("content", "limiting where the ghost player steers", 0, [&] {
    g_acting.player_move(at, dx, dz);
    return 0;
  });
}
bool api_content_interaction_lost() {
  return host::contained("content", "asking whether an interaction is lost", false,
                         [&] { return g_acting.interaction_lost(); });
}
bool api_content_roll_taken() {
  return host::contained("content", "asking Playable Ghost", false, [&] {
    const auto* pm = static_cast<const PmPlayableGhostService*>(g_services.find(PM_SERVICE_PLAYABLE_GHOST));
    return pm && pm->version >= 3 && pm->rollTaken && pm->rollTaken();
  });
}

bool api_steer_agent(void* agent, float dx, float dz) {
  return host::contained("suite", "steering an agent", false, [&] { return steering::steer(agent, dx, dz); });
}
void* api_content_target(const void* const* excluded, uint32_t excludedCount) {
  return host::contained("content", "asking the ghost's target", static_cast<void*>(nullptr),
                         [&] { return g_acting.target(excluded, excludedCount); });
}

host::ContentTypes g_types{g_content, g_acting, {&list_insert, &list_remove_at, &list_add_value}};

host::ContentEvidence g_evidence{g_content, g_types,
                                 {&collection_clear, &sync_initialise, &collection_has_clear, &sync_has_initialise}};
host::ContentJournal g_journal{g_content, g_types,
                               {&collection_clear, &sync_initialise, &collection_has_clear, &sync_has_initialise},
                               &g_evidence};
// The host's one hook on the game's localisation, for the keys the three sides above answer.
host::ContentLocalised g_localised{g_types, g_journal, g_evidence};
// Whether everyone in the room now is verified, on Photon's player list at this moment, as a custom value is about to go out.
// False whenever it cannot be read, so it waits.
bool room_verified_now() {
  std::vector<int> actors;
  return hostgame::private_room() && hostgame::room_actors(actors) && g_agreement.agreed() && g_agreement.unverified(actors).empty();
}
host::ContentDifficulty g_difficulty{g_content, g_types, &room_verified_now};
// A ghost whose definition names a target hunts as the game's Banshee does (host/content_target.h).
host::ContentTarget g_target{g_acting};

// Whether this client's mods equal the host's, read from presence: every frame before the content is put into the game
// (tick_content), and as an announcement arrives. A contract holds the answer it loaded with, on the signal the ghost types
// decide it on. The players' lists are read only when presence heard something new or the room changed.
void observe_agreement(bool inPrivate) {
  std::vector<int> actors;
  const bool listRead = !inPrivate || hostgame::room_actors(actors);
  const int local = inPrivate ? hostgame::local_actor() : 0;
  const int master = inPrivate ? hostgame::master_actor() : 0;
  std::vector<int> others;
  for (int a : actors)
    if (a != local) others.push_back(a);
  const bool wasAgreed = g_agreement.agreed();
  static const content::Agreement::ModsOf modsOf = [](int actor) -> std::optional<std::vector<content::ModBuild>> {
    const std::vector<presence::Mod>* mods = g_presence.mods_of(actor);
    if (!mods) return std::nullopt;
    std::vector<content::ModBuild> out;
    for (const presence::Mod& m : *mods) out.push_back({m.id, m.build});
    return out;
  };
  g_agreement.observe(inPrivate, host::ContentTypes::contract_loaded(), local, master, others, g_presence.revision(), modsOf, listRead);
  if (inPrivate && g_agreement.agreed() != wasAgreed)
    PM_LOG("suite: this client's mods %s the host's: the mods' content, rules and journal lines %s",
           g_agreement.agreed() ? "match" : "do not match (yet)", g_agreement.agreed() ? "run" : "wait");
}

// The host's channel carries the presence announcements, the content manifests and the ghost's
// acting-as type; each ignores the others' messages.
void on_host_message(int32_t sender, const uint8_t* data, size_t size) {
  if (!hostgame::private_room()) return;
  const unsigned heard = g_presence.revision();
  g_presence.receive(sender, data, size, GetTickCount64());
  g_content.receive(sender, data, size);
  g_acting.receive(sender, hostgame::master_actor(), data, size);
  if (const uint32_t marker = startclose::read_marker(data, size)) g_closer.heard(sender, hostgame::local_actor(), marker);
  // A player's mods may speak as soon as they are verified, and their announcement arrives first, in the same batch of
  // events: the message right behind it finds them heard.
  if (g_presence.revision() != heard) observe_agreement(true);
}

// A mod's channel takes a message only from a verified player, on a client that matches the host (hostnet::set_gate). So an
// older client's mods, whose presence never reads here, reach none of this client's mods.
bool channel_open(uint8_t channel, int32_t sender) {
  return host::contained("suite", "asking whom a mod's channel takes", false, [&] {
    for (const mods::Mod& m : mods::installed())
      if (m.started && m.info && m.info->channel == channel)
        return g_agreement.delivers(sender, hostgame::local_actor(), {m.info->id, m.build});
    return false;
  });
}

// The room as the join guard reads it, fresh, for what cannot wait for a frame.
joinguard::Frame room_now() {
  const int local = hostgame::local_actor();
  joinguard::Frame frame{local, hostgame::master_actor(), {}, true, GetTickCount64()};
  frame.listRead = hostgame::room_actors(frame.actors);
  frame.publicRoom = local != 0 && !hostgame::private_room();
  return frame;
}

std::vector<std::string> shown_players(const std::vector<int>& actors) {
  std::vector<std::string> names;
  for (int actor : actors) names.push_back(joinguard::shown_player(room_actions::player_name(actor), actor));
  return names;
}

// Tells the host, through the game's popup, who a contract's start is waiting for.
void tell_start_waits(const joinguard::StartAnswer& answer) {
  PM_LOG("suite: a contract's start waits: %zu being checked, %zu the game did not remove, list unread %d", answer.waiting.size(),
         answer.stuck.size(), answer.listUnread ? 1 : 0);
  menu_panel::message(i18n::text("popup.contract-waiting"),
                      joinguard::start_wait_notice(shown_players(answer.waiting), shown_players(answer.stuck), answer.listUnread, answer.notFirstActor));
}

// Whether a contract may start now (host/lobby_gate.h). When not, the host is told through the game's popup who it is waiting for,
// and the game's state stands as it was, so the host can press again.
bool start_allowed() {
  return host::contained("suite", "checking a contract's start", false, [] {
    const joinguard::StartAnswer answer = g_guard.start_answer(room_now(), g_presence);
    if (!answer.go) tell_start_waits(answer);
    return answer.go;
  });
}

// The room this client is in now, as the closer tells rooms apart: the Photon room object, which the game makes afresh for each
// room it joins. 0 outside one, or when it cannot be read.
uint64_t current_room() {
  return phasmo::PhotonNetwork::get_CurrentRoom.available()
             ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(phasmo::PhotonNetwork::get_CurrentRoom()))
             : 0;
}

// Opens or closes the room this client is in, as ServerManager.LoadScene and the lobby's return do (PhotonRoom.set_IsOpen). False
// when there is no room or the setter did not resolve.
bool set_room_open(bool open) {
  void* room = phasmo::PhotonNetwork::get_CurrentRoom.available() ? phasmo::PhotonNetwork::get_CurrentRoom() : nullptr;
  if (!room || !phasmo::PhotonRoom::set_IsOpen.available()) return false;
  phasmo::PhotonRoom::set_IsOpen(room, open);
  return true;
}

// Opens the room a start closed, and says so when it cannot.
void reopen_room() {
  if (!host::contained("suite", "opening the room again", false, [] { return set_room_open(true); }))
    PM_LOG("suite: the room closed for a contract's start could not be opened again");
}

bool frame_in_lobby();

// Everything the closed-room start (host/start_flow.h) touches outside the closer, made from this client. The decisions,
// and every log line and popup, live in start_flow.h, where tests/start_flow_test.cpp checks them; this only hands over
// the game's own room, marker, held send, popup and log. The release's containment stands here, as it did in the flow.
startflow::Calls<joinguard::StartAnswer> flow_calls() {
  startflow::Calls<joinguard::StartAnswer> calls;
  calls.clock = &GetTickCount64;
  calls.room = &current_room;
  calls.local_actor = &hostgame::local_actor;
  calls.is_master = &hostgame::local_is_master;
  calls.in_lobby = &frame_in_lobby;
  calls.close_room = [] { return set_room_open(false); };
  calls.send_marker = [](const uint8_t* marker, size_t size) {
    return hostnet::send_through_server_to_all(hostnet::kHostChannel, marker, size);
  };
  calls.reopen_room = &reopen_room;
  calls.release_held = [] { return host::contained("suite", "sending a contract's start", false, [] { return lobby_gate::release_held(); }); };
  calls.drop_held = [] { lobby_gate::drop_held(); };
  calls.start_answer = [](joinguard::StartAnswer& answer) {
    answer = g_guard.start_answer(room_now(), g_presence);
    return answer.go;
  };
  calls.tell_start_waits = &tell_start_waits;
  calls.popup = [](const char* title_key, const std::string& text) { menu_panel::message(i18n::text(title_key), text); };
  calls.log = &pm::log::write;
  return calls;
}

// What becomes of an allowed start's send (lobby_gate::Config::startSend), decided by host/start_flow.h on this client.
int start_send() { return startflow::start_send(g_closer, flow_calls()); }

// Every frame: a start whose room is closed goes once the server confirms the close and everyone is still verified.
void tick_start() { startflow::tick_start(g_closer, flow_calls()); }

// The hooks the lobby's rules cannot do without (host/mod_session.h), made once the player agreed. All of them or none.
bool install_required_hooks() {
  const size_t before = hooks::mark();
  lobby_gate::Config gate;
  gate.isStartButton = &room_actions::is_start_button;
  gate.startAllowed = &start_allowed;
  gate.roomVerified = &room_verified_now;
  gate.customPage = [](void* view, int32_t page) { return g_journal.custom_page_on(view, page); };
  gate.boxedInt = &runtime_services::boxed_int;
  gate.registry = &g_content;
  gate.startPending = [] { return !g_closer.idle(); };
  gate.startSend = &start_send;
  gate.privateRoom = [] { return hostgame::private_room(); };
  if (hostnet::create_hooks() && lobby_gate::create_hooks(gate) && hooks::enable_all()) return true;
  hooks::rollback_to(before);
  return false;
}

// Custom content's own hooks on the game. Without them content stays off: a mod that registers a ghost or an evidence
// is then refused and stays off (host/mods.h, Group::refusal), and the mods that register none run.
void install_content_hooks() {
  if (!g_contentCanHook) return;
  if (!g_types.create_hooks() || !g_journal.create_hooks() || !g_localised.create_hook() || !g_difficulty.create_hooks() ||
      !g_target.create_hooks())
    g_content.withhold("its hooks on the game could not be made");
  g_journal.note_existing();  // the local player's book woke before these hooks could see it
}

// The host's own hooks that serve the mods (host/mod_session.h), made once the player agreed.
bool install_session_hooks() {
  bool ok = context::install(&gamemap::entry);
  room_watch::create_hook();  // for the logs only: without it Photon's room cleanup runs unwatched
  install_content_hooks();
  if (!hooks::enable_all()) {
    PM_LOG("suite: the host's hooks for the mods could not be switched on");
    ok = false;
  }
  return ok;
}

mods::Group content_group() {
  return {[](const std::string& id) { g_content.begin_group(id); }, [](bool started) { g_content.end_group(started); },
          &context::tracking_failed, [] { return g_content.refused_in_group(); }};
}

// What every start of the mods owes the rest of the host, once they have started.
void restart_mods_once(const std::vector<std::string>& ids);

// Every mod has started: its content may name evidence from any of them, so only now can the registry settle it. A mod
// whose content it drops stays off for this start, and the mods start again without it (and so without those that require
// it), until nothing more is dropped: each round leaves one mod or more off, so it ends.
void mods_started(const std::vector<std::string>& ids) {
  mods::settle(
      ids,
      [] {
        std::vector<mods::Dropped> dropped;
        for (content::Registry::Dropped& d : g_content.commit()) dropped.push_back({std::move(d.owner), std::move(d.why)});
        return dropped;
      },
      &restart_mods_once);
  g_presence.set_local(mods::started_builds());
  std::vector<content::ModBuild> builds;
  for (const presence::Mod& m : mods::started_builds()) builds.push_back({m.id, m.build});
  g_agreement.set_local(std::move(builds));
}

void start_mods(const std::vector<std::string>& ids) {
  mods::start(&g_api, ids, content_group());
  mods_started(ids);
}

// Takes custom content out of the game and stops the rules, before the mods that gave them stop: the
// custom entries leave every collection first, so the journals put back after it make their lists
// again with the game's ghosts only.
void withdraw_content() {
  host::contained("content", "taking content out of the game", 0, [&] {
    g_difficulty.withdraw(hostgame::private_room() && hostgame::local_is_master());
    g_target.withdraw();
    g_types.withdraw();
    g_journal.withdraw();
    g_evidence.withdraw();
    return 0;
  });
}

// A restart of the mods with `ids`, from the host's own tick (no mod detour is on the stack), never during a contract: every
// running mod stops, newest first; every hook the mods made goes, newest first, so detours chained on one function unwind
// in order; their receivers and services go with them; then those mods start afresh.
void run_mods(const std::vector<std::string>& ids) {
  restart_mods_once(ids);
  mods_started(ids);
}

// One restart of the mods with `ids`, as run_mods describes, with no settling of their content.
void restart_mods_once(const std::vector<std::string>& ids) {
  mods::Restart steps;
  steps.withdraw = &withdraw_content;  // the rules stop while their mods still run
  steps.stopped = [] {
    steering::expire();
    hooks::rollback_to(g_modHooks);
    steering::reset();  // the mods start afresh: a refused patch is tried once more
    for (const mods::Mod& m : mods::installed())
      if (m.info) hostnet::set_receiver(m.info->channel, nullptr);
    g_services.clear();
    g_content.clear();
  };
  mods::restart(&g_api, ids, content_group(), steps);
}

// The Mods panel's Apply: never in a room.
void restart_mods(const std::vector<std::string>& ids) {
  PM_LOG("suite: restarting the mods with the player's choice");
  g_resumeDue = false;  // the player's choice replaces what a room's rules had switched off
  g_wanted.clear();
  run_mods(ids);
  bool ok = true;
  const std::vector<std::string> running = mods::started_ids();
  for (const mods::Mod& m : mods::installed())
    if (std::find(ids.begin(), ids.end(), m.manifest.id) != ids.end() &&
        std::find(running.begin(), running.end(), m.manifest.id) == running.end())
      ok = false;
  const bool saved = mods::save_chosen(ids);
  if (!saved) PM_LOG("suite: the choice could not be saved; the launcher keeps the old one");
  mods_panel::applied(ok, saved);
}

// Whether the lobby's screen is up in a room: the only place the join guard judges anyone or switches a mod off. False, so
// nothing changes, whenever it cannot be read.
bool frame_in_lobby() {
  return hostgame::local_actor() != 0 && hostgame::out_of_contract() && room_actions::lobby_open();
}

// The join guard's word on which mods run in this room. The saved choice does not change: what was running when a room's
// rules first switched some off starts again on leaving the room, and until then g_wanted holds it.
void apply_to_room(const joinguard::Verdict& verdict) {
  if (!verdict.suspend.empty()) {
    if (g_wanted.empty()) g_wanted = mods::started_ids();
    std::vector<std::string> keep;
    for (const std::string& id : g_wanted)
      if (std::find(verdict.suspend.begin(), verdict.suspend.end(), id) == verdict.suspend.end()) keep.push_back(id);
    PM_LOG("suite: switching off %zu of the %zu mods for this room", verdict.suspend.size(), g_wanted.size());
    run_mods(keep);
    menu_panel::message(i18n::text("popup.mods-off"), verdict.notice);
  }
  if (verdict.resume) {
    g_resumeDue = true;
    if (verdict.show.empty()) menu_panel::drop_message();  // it spoke of the room this client has left
  }
  // In the lobby at once. Out of a room only once no contract is up: a client leaves one while its scene still runs.
  if (g_resumeDue && (frame_in_lobby() || (hostgame::out_of_room() && hostgame::out_of_contract()))) {
    g_resumeDue = false;
    PM_LOG("suite: the mods this room switched off start again");
    run_mods(std::exchange(g_wanted, {}));
  }
}

// The join guard (host/join_guard.h): leave a lobby whose host lacks a mod match, as the host remove those whose mods
// differ, and switch off whatever this room may not run, with its reason.
void tick_guard() {
  const bool isPrivate = hostgame::private_room();
  const int local = hostgame::local_actor();
  joinguard::Frame frame{local, hostgame::master_actor(), g_actors, frame_in_lobby(), GetTickCount64()};
  frame.publicRoom = local != 0 && !isPrivate;
  frame.roomUnknown = local == 0 && !hostgame::out_of_room();
  const joinguard::Verdict verdict = g_guard.judge(frame, g_presence);
  if (verdict.leave) g_guard.left(room_actions::leave_lobby());
  for (int actor : verdict.kick) g_guard.kicked(actor, room_actions::kick(actor));
  for (int actor : verdict.unremoved) PM_LOG("room: actor %d is still in the room after %d kicks through the game", actor, joinguard::kMaxKicks);
  if (!verdict.unremoved.empty()) menu_panel::message(i18n::text("popup.player-still-here"), joinguard::unremoved_notice(shown_players(verdict.unremoved)));
  if (!verdict.show.empty()) menu_panel::message(i18n::text("popup.mods-dont-match"), verdict.show);
  apply_to_room(verdict);
}

void tick_presence() {
  const bool inPrivate = hostgame::private_room();
  if (inPrivate) hostgame::room_actors(g_actors);
  else g_actors.clear();
  g_presence.observe(inPrivate ? hostgame::local_actor() : 0, g_actors, GetTickCount64());
  bool told = true;
  for (const std::vector<uint8_t>& part : g_presence.take_announcement())
    told = hostnet::send(hostnet::kHostChannel, part.data(), part.size(), PM_TO_OTHERS, true, 0) && told;
  if (!told) PM_LOG("suite: the presence announcement could not be sent whole; it is sent again in 2 s");
  g_content.observe(inPrivate ? hostgame::local_actor() : 0, g_actors, GetTickCount64(), inPrivate ? hostgame::master_actor() : 0);
  observe_agreement(inPrivate);
  bool sent = true;
  for (const std::vector<uint8_t>& manifest : g_content.take_messages())
    sent = sent && hostnet::send(hostnet::kHostChannel, manifest.data(), manifest.size(), PM_TO_OTHERS, true, 0);
  if (!sent) {
    g_content.send_failed();  // tried again shortly; said once until a send works
    if (!g_manifestUnsent) PM_LOG("suite: the content manifest could not be sent; it is tried again");
  }
  g_manifestUnsent = !sent;
}

// Content in the game: the ghost types follow the room's content state, the acting-as type reaches
// the other clients, and the rules run.
void tick_content() {
  const bool inPrivate = hostgame::private_room();
  const bool master = inPrivate && hostgame::local_is_master();
  host::contained("content", "putting content into the game", 0, [&] {
    // The master's word alone: it spawns the ghost, and a client that had not heard a player the master had must not leave a
    // custom ghost's contract without its entries.
    g_types.tick(g_content.state(), inPrivate, master, !master || g_agreement.unverified(g_actors).empty());
    g_evidence.tick(g_journal.journals());  // before the journal's: a custom ghost's page names its evidence
    g_journal.tick(inPrivate);
    g_difficulty.tick(g_content.state(), inPrivate, master, GetTickCount64());
    g_target.tick();  // after the types: the acting-as type is this frame's
    return 0;
  });
  g_acting.observe_room(inPrivate ? hostgame::local_actor() : 0, g_actors);
  if (!inPrivate) return;
  bool sent = true;
  for (const content::ActingMessage& m : g_acting.take_messages(g_agreed)) {
    const int32_t to = m.actor == 0 ? PM_TO_OTHERS : PM_TO_ACTOR;
    if (hostnet::send(hostnet::kHostChannel, m.data.data(), m.data.size(), to, true, m.actor)) continue;
    g_acting.send_failed(m.actor);  // tried again next frame; said once until a send works
    sent = false;
  }
  if (!sent && !g_actingUnsent) PM_LOG("suite: the ghost's acting-as type could not be sent; it is tried again");
  g_actingUnsent = !sent;
}

}  // namespace

// The loader's player-facing text (core/text_table.h), from beside this DLL's copy, before anything shows. Without it every
// text shows its key, and the log says why.
void load_text() {
  HMODULE self = nullptr;
  wchar_t path[MAX_PATH];
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&load_text), &self) ||
      !GetModuleFileNameW(self, path, MAX_PATH)) {
    PM_LOG("suite: this DLL's path cannot be read, so the loader's text cannot be found; its messages show their keys");
    return;
  }
  i18n::table().on_missing([](const std::string& key) { PM_LOG("suite: the loader's text has no %s", key.c_str()); });
  std::string why;
  const std::filesystem::path file = i18n::table_beside(path);
  if (!i18n::table().load(file, why))
    PM_LOG("suite: the loader's text (%s) could not be read: %s; its messages show their keys", text::narrow(file.wstring()).c_str(), why.c_str());
}

bool start(void* gameAssembly, void* unityPlayer, const build::Hashes& game) {
  load_text();
  gamemap::resolve_icalls();  // on the main thread, before any mod starts
  gamemap::resolve_inflated();
  gamemap::resolve_scene_facts(scene_engine::engine());  // the layers, from the running game
  // Custom numbers and vanilla ids come from the game's own enums; unreadable, content stays off.
  content::GameEnums enums;
  g_content.use_agreement(&g_agreement);  // the mods' content, rules and journal lines follow this client's match with the host
  if (const std::string why = gamemap::read_content_enums(enums); why.empty()) g_content.set_game_enums(std::move(enums));
  else g_content.refuse(why);
  g_dataDirectory = data_directory();
  if (g_dataDirectory.empty()) {
    PM_LOG("suite: the local application-data folder is unavailable; set LOCALAPPDATA and restart the game");
    return false;
  }
  // Positional: a member added to PmHostApi after log_level fails this until it is given here too.
  static_assert(offsetof(PmHostApi, log_level) + sizeof(PmHostApi::log_level) == sizeof(PmHostApi),
                "g_api lists every PmHostApi member");
  g_api = {PMMOD_API_VERSION, gameAssembly, unityPlayer, g_dataDirectory.c_str(),
           &api_log, &api_create_hook, &api_enable_hooks, &api_send, &api_set_receiver,
           &api_peer_has_mod, &api_register_service, &api_service,
           &api_game_entry, &api_hook_entry, &api_set_reference, &api_inside_entry,
           &api_string_new, &api_string_utf8, &api_array_length, &api_array_data, &api_array_new,
           &api_list_count, &api_list_items, &api_type_object, &api_object_new, &api_is_instance,
           &api_alive, &api_virtual_method, &api_gc_hold, &api_gc_release, &api_log_exception,
           &api_static_get, &api_static_set, &api_hook_body, &api_body_count,
           &api_scene_find, &api_animator_param,
           &api_ghost_register, &api_evidence_register, &api_content_type, &api_content_ghost_id,
           &api_content_evidence, &api_content_active, &api_content_player_controlled, &api_ghost_rules,
           &api_content_acting_as, &api_content_identity, &api_journal_lines, &api_content_evidence_look,
           &api_list_add, &api_copy_settings, &api_fields_of_type, &api_content_use_ability,
           &api_content_ability_text, &api_content_hud_state, &api_content_hud_line, &api_content_held_back,
           &api_content_player_view, &api_content_target, &api_content_flags,
           &api_room_agrees, &api_collection_count, &api_array_element_size, &api_list_remove_at,
           &api_content_player_move, &api_content_interaction_lost, &api_content_roll_taken, &api_steer_agent,
           &api_abandon_ghost_spawn, &api_dictionary_int_entries, &api_mod_started, &api_show_message,
           &api_content_flags_of, &api_mod_name, &api_unity_action_new, &api_mod_entry, &api_content_packs,
           &api_content_pack_failed, &api_log_level};
  // The host's own game side of content reaches the game through the map, like a mod: the game API
  // bound to the host's contract. Without every entry it needs, custom content stays off.
  std::string unbound;
  api::bind(&g_api, unbound);
  menu_panel::report();
  mods_panel::report();
  // Steering's entries are the host's own, which no mod requires: say here when this build lacks one.
  if (const std::string lacking = steering::missing(); !lacking.empty())
    PM_LOG("suite: steering is unavailable; these game entries did not resolve: %s", lacking.c_str());
  std::string missing = g_types.missing();
  if (const std::string more = g_journal.missing(); !more.empty()) missing += (missing.empty() ? "" : ", ") + more;
  if (const std::string more = g_evidence.missing(); !more.empty()) missing += (missing.empty() ? "" : ", ") + more;
  if (const std::string more = g_difficulty.missing(); !more.empty()) missing += (missing.empty() ? "" : ", ") + more;
  if (const std::string more = g_target.missing(); !more.empty()) missing += (missing.empty() ? "" : ", ") + more;
  g_difficulty.load(g_dataDirectory + L"\\custom_difficulty.txt");
  if (!missing.empty()) g_content.withhold("these game entries did not resolve: " + missing);
  g_contentCanHook = missing.empty();
  hostnet::set_receiver(hostnet::kHostChannel, &on_host_message);
  hostnet::set_gate(&channel_open);
  mods::read_installed(g_dataDirectory);  // their manifests: no mod's code runs before the player agrees
  std::string rules = room_actions::missing();  // and what holds a contract's start until everyone is verified
  if (const std::string gate = lobby_gate::missing(); !gate.empty()) rules += (rules.empty() ? "" : ", ") + gate;
  g_answer = mod_session::opening(!mods::installed().empty(), hostnet::missing(), rules, g_offCause);
  if (g_answer == consent::Answer::Pending) {
    if (const std::string lacking = consent::missing_notice_text(i18n::table()); !lacking.empty()) {
      PM_LOG("suite: the loader's text has no %s, so the notice cannot be read; no mod starts", lacking.c_str());
      g_answer = consent::Answer::Declined;
      g_offCause = i18n::text("off.notice-cannot-show");
    }
  }
  if (g_answer == consent::Answer::Pending) {
    const build::Standing standing = build::judge(mods::build_records(), game, gamemap::map_digest());
    consent::begin(consent::notice(mods::notice_list(), build::checked(standing)), game, g_dataDirectory + L"\\consent.txt");
  }
  return true;
}

bool tick() {
  if (g_answer == consent::Answer::Pending) {
    g_answer = consent::tick();
    g_offCause = consent::off_cause();
    if (!g_agreed) {
      const mod_session::Steps steps{&install_session_hooks, &hooks::mark, [] { start_mods(mods::chosen()); },
                                     &install_required_hooks};
      g_agreed = mod_session::begin(g_answer, steps, g_modHooks);
      if (g_answer == consent::Answer::Agreed && !g_agreed) g_offCause = mod_session::lacks_the_rules();
    }
  }
  // "Play without mods", or nothing to agree to: nothing is left to do, or to tell the player. When the player did not
  // choose this, the frame stays, for the Mods panel that says why.
  if (g_answer == consent::Answer::Declined && g_offCause.empty()) return false;
  if (g_agreed) {
    tick_presence();
    tick_content();
    tick_guard();
    tick_start();
    mods_panel::tick(hostgame::out_of_room());
    std::vector<std::string> choice;
    if (mods_panel::take_choice(choice) && hostgame::out_of_room()) restart_mods(choice);
    steering::expire();  // an agent stays steered only while a mod keeps asking, in its tick
    mods::tick_all();
  } else if (!g_offCause.empty()) {
    mods_panel::tick_off(g_offCause);
  }
  menu_panel::tick_message();
  notice::tick();
  return true;
}

content::State content_state() { return g_content.state(); }

void stop() {
  // A room closed for a start that has not gone opens again: the mods that asked for it are going.
  lobby_gate::drop_held();
  if (g_closer.abandon()) reopen_room();
  consent::hide();
  mods_panel::teardown();
  withdraw_content();
  if (g_agreed) mods::stop_all();
  steering::expire();  // shutdown writes the site back from another thread, frames from now
  g_agreed = false;
  g_content.clear();
  gamemap::forget_scene_facts();  // after the mods, which may still ask in their stop
  g_presence.set_local({});
  // The players still in the room hear that this client runs no mods now (host/presence.h).
  host::contained("suite", "saying the mods stopped", 0, [&] {
    for (const std::vector<uint8_t>& bye : g_presence.take_announcement())  // one part: it lists none
      if (!hostnet::send(hostnet::kHostChannel, bye.data(), bye.size(), PM_TO_OTHERS, true, 0))
        PM_LOG("suite: that the mods stopped could not be sent");
    return 0;
  });
}

}  // namespace pm::suite
