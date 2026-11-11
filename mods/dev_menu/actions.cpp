// The dev menu's game side. Every action is the game's own method on the client the game would call it
// from (the mod never sends an RPC by name, never runs an RPC body); the rules are in menu_rule.h and the
// click already passed refuse() before an action here runs. Everything reads the running game now, per the
// dynamic-over-hard-coded rule: state numbers, room lists, type lists and every value come from the map.
#include "actions.h"

#include <chrono>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "log.h"
#include "runtime.h"

#include "Difficulty.h"
#include "GameController.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostHuntState.h"
#include "LevelController.h"
#include "MainManager.h"
#include "Photon.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "UnityAudio.h"
#include "UnityObject.h"

#include "content.h"

namespace pm::devmenu::actions {
namespace {

using namespace phasmo;

bool g_session = false;      // the dev session: on once the menu has opened in this run
bool g_sessionTold = false;  // its log line was written
Switches g_switches;
bool g_frozen = false;       // our last ToggleFreezePlayerMovement for the local investigator

// ---- The hooks. Their creation is actions::start(): if one cannot be made the mod stays off, so a dev
// session always has progression suppression (fail closed). ----

// God mode: a hunt's kill of the local investigator is refused before the game's Kill runs. Offline the
// kill decision is this client's and the kill's network body is never touched.
bool det_kill(api::Original<GhostHuntState::Kill> original, void* hunt, void* player, bool force, float reach,
              float slowReach) {
  if (god_refuses_kill(g_switches.godMode, player && player == game::local_player())) return false;
  return original(hunt, player, force, reach, slowReach);
}

// Progression suppression: while a dev session is on, the contract's mission and challenge checks are
// skipped, so the session earns nothing. These two are the checks the content plan maps; the remaining
// write sites (stats, daily and weekly quests, the apocalypse counter) wait for the RE pass the plan
// names, which is why the sandbox keeps the whole session offline and local.
void det_check_missions(api::Original<ExitLevel::CheckMissions> original) {
  if (suppress_progress(g_session)) return;
  original();
}
void det_check_challenges(api::Original<ExitLevel::CheckChallenges> original) {
  if (suppress_progress(g_session)) return;
  original();
}

// Never lose a consumable: the saved stock's decrement is skipped while the switch is on.
void det_remove_amount(api::Original<EquipmentInfo::RemoveAmountOwned> original, void* info, int32_t amount) {
  if (keep_refuses_consume(g_switches.keepConsumables)) return;
  original(info, amount);
}

// ---- Reading the contract ----

void* difficulty() {
  void* values = game::level_values();
  if (!values || !api::alive(values)) return nullptr;
  return LevelValues::difficulty.get(values);
}

void* values() {
  void* v = game::level_values();
  return (v && api::alive(v)) ? v : nullptr;
}

// The contract's GhostController: a destroyed one remains after a contract, so check it alive.
void* controller() {
  void* c = GhostController::instance.get();
  return (c && api::alive(c)) ? c : nullptr;
}

// The pause menu's controller, for the game's own Leave and Quit. FindObjectOfType searches the whole
// scene, so at most once a second (the pattern of the other finders).
void* pause_menu() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  const uint64_t now = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
  if (found && game::alive(found)) return found;
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = PauseMenuController::type.type_object();
  if (!type) return nullptr;
  // The pause menu's object is usually active behind its closed canvas; if it is switched off, search
  // again including inactive objects (the game reaches it the same way its own buttons do).
  void* menu = Object::FindObjectOfType(type, false);
  if (!menu) menu = Object::FindObjectOfType(type, true);
  found = menu;
  return menu;
}

// The game's GhostState numbers, named by the map: read now, so an update that renumbers the enum still
// reads right. Filled at first use; every entry the mod's code names must resolve or the host keeps the
// mod off, so an unresolved one cannot leave a wrong number in the table.
const std::vector<std::pair<int32_t, const char*>>& state_table() {
  static const std::vector<std::pair<int32_t, const char*>> table = [] {
    std::vector<std::pair<int32_t, const char*>> out;
    out.reserve(23);
    out.emplace_back(GhostState::idle(), "idle");
    out.emplace_back(GhostState::wander(), "wander");
    out.emplace_back(GhostState::hunting(), "hunting");
    out.emplace_back(GhostState::appear(), "appear");
    out.emplace_back(GhostState::randomEvent(), "randomEvent");
    out.emplace_back(GhostState::favouriteRoom(), "favouriteRoom");
    out.emplace_back(GhostState::light(), "light");
    out.emplace_back(GhostState::door(), "door");
    out.emplace_back(GhostState::throwing(), "throwing");
    out.emplace_back(GhostState::fusebox(), "fusebox");
    out.emplace_back(GhostState::doorKnock(), "doorKnock");
    out.emplace_back(GhostState::windowKnock(), "windowKnock");
    out.emplace_back(GhostState::carAlarm(), "carAlarm");
    out.emplace_back(GhostState::flicker(), "flicker");
    out.emplace_back(GhostState::cctv(), "cctv");
    out.emplace_back(GhostState::dots(), "dots");
    out.emplace_back(GhostState::GhostAbility(), "GhostAbility");
    out.emplace_back(GhostState::mannequin(), "mannequin");
    out.emplace_back(GhostState::teleportObject(), "teleportObject");
    out.emplace_back(GhostState::interact(), "interact");
    out.emplace_back(GhostState::summoningCircle(), "summoningCircle");
    out.emplace_back(GhostState::musicBox(), "musicBox");
    out.emplace_back(GhostState::ignite(), "ignite");
    return out;
  }();
  return table;
}

// The offered ghost collection (the same list the game's own type roll offers, custom ghosts included).
// Read at most twice a second: the level's collection is stable within a contract.
std::vector<int32_t> offered_types() {
  static uint64_t nextScan = 0;
  static std::vector<int32_t> types;
  const uint64_t now = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
  if (now < nextScan && !types.empty()) return types;
  nextScan = now + 500;
  types.clear();
  void* c = controller();
  void* collection = c ? GhostController::allGhostsCollection.get(c) : nullptr;
  if (!collection) return types;
  void* list = GhostCollection::ghosts.get(collection);
  if (!list) return types;
  api::RefArray ghosts{api::List{list}.items()};
  for (size_t i = 0; i < ghosts.size(); ++i) {
    void* data = ghosts[i];
    if (data) types.push_back(GhostData::ghostType.get(data));
  }
  return types;
}

}  // namespace

bool start() {
  return api::hook<GhostHuntState::Kill>(&det_kill, "Dev Menu: God mode keeps the local kill refused") &&
         api::hook<ExitLevel::CheckMissions>(&det_check_missions, "Dev Menu: no missions at the end") &&
         api::hook<ExitLevel::CheckChallenges>(&det_check_challenges, "Dev Menu: no challenges at the end") &&
         api::hook<EquipmentInfo::RemoveAmountOwned>(&det_remove_amount, "Dev Menu: consumables stay") &&
         hooks::enable_all();
}

void stop() {
  // The host takes the mod's hooks down when the mod stops; nothing to do here beyond our own state.
  g_frozen = false;
}

void note_session() {
  g_session = true;
  if (!g_sessionTold) {
    g_sessionTold = true;
    PM_LOG("dev-menu: dev session on: this client's contracts will not reach the mission or challenge checks");
  }
}

bool session_on() { return g_session; }

Switches& switches() { return g_switches; }

bool offline() { return phasmo::PhotonNetwork::get_OfflineMode(); }

bool can_open() { return may_open(offline(), game::in_room()); }

Can snapshot() {
  Can c;
  c.offline = offline();
  void* level = game::level_controller();
  c.contract = level && game::alive(level);
  c.ghost = c.contract && game::ghost_ai() != nullptr;
  c.difficulty = difficulty() != nullptr;
  void* player = game::local_player();
  c.player = player != nullptr;
  c.hunting = c.ghost && GhostAI::isHunting.get(game::ghost_ai());
  c.dead = c.player && Player::isDead.get(player);
  c.rooms = static_cast<int>(game::level_rooms().size());
  return c;
}

// ---- Read-outs ----

std::string state_name(int32_t state) {
  for (const auto& [value, name] : state_table())
    if (value == state) return name;
  return {};
}

std::string ghost_line() {
  void* ai = game::ghost_ai();
  if (!ai) return {};
  std::string line = state_name(GhostAI::state.get(ai));
  if (GhostAI::isHunting.get(ai)) line += " [hunting]";
  if (GhostAI::isAppeared.get(ai)) line += " [appeared]";
  if (GhostAI::postHuntCooldown.get(ai)) line += " [cooldown]";
  if (GhostAI::huntsHeldOff.get(ai)) line += " [held off]";
  if (GhostAI::canWander.get(ai)) line += " [can wander]";
  if (GhostAI::stayHome.get(ai)) line += " [stay home]";
  if (GhostAI::speedFlag.get(ai)) line += " [speed flag]";
  if (api::alive(GhostAI::smudge.get(ai))) line += " [smudged]";
  if (GhostAI::flamesBlownOut.get(ai)) line += " [flames blown]";
  return line;
}

std::string ghost_identity() {
  void* c = controller();
  if (!c) return {};
  // The plan's identity card: the type the contract shows and the behaviour the roll actually gave,
  // which differ only for a Mimic (GhostController.ghostType and .behaviourType).
  const int32_t type = GhostController::ghostType.get(c);
  std::string out = std::to_string(type);
  const std::string id = api::content::id_of(type);
  if (!id.empty()) out += " " + id;
  const int32_t behaviour = GhostController::behaviourType.get(c);
  if (behaviour != type) {
    out += " behaviour " + std::to_string(behaviour);
    const std::string bid = api::content::id_of(behaviour);
    if (!bid.empty()) out += " " + bid;
  }
  return out;
}

int favourite_room_index() {
  void* c = controller();
  return c ? GhostController::favouriteRoomIndex.get(c) : -1;
}

std::string room_name(int index) {
  api::RefArray rooms = game::level_rooms();
  if (index < 0 || static_cast<size_t>(index) >= rooms.size()) return {};
  void* room = rooms[index];
  if (!room) return {};
  return api::to_utf8(LevelRoom::name.get(room));
}

std::string favourite_room_name() { return room_name(favourite_room_index()); }

float base_speed() {
  void* ai = game::ghost_ai();
  return ai ? GhostAI::baseSpeed.get(ai) : 0.0f;
}

int room_count() { return static_cast<int>(game::level_rooms().size()); }

bool events_blocked() {
  void* v = values();
  return v && LevelValues::eventsBlocked.get(v);
}

bool friendly() {
  void* d = difficulty();
  return d && Difficulty::friendlyGhost.get(d);
}

bool lose_equipment() {
  void* d = difficulty();
  return d && Difficulty::loseItemsOnDeath.get(d);
}

int evidence_given() {
  void* d = difficulty();
  return d ? Difficulty::evidenceGiven.get(d) : 0;
}

int forced_type() {
  void* d = difficulty();
  return d ? Difficulty::ghostTypeOverride.get(d) : 0;
}

std::string forced_type_name() {
  const int32_t type = forced_type();
  if (type == 0) return "random";
  const std::string id = api::content::id_of(type);
  if (!id.empty()) return id;
  return std::to_string(type);
}

int forced_type_options() { return 1 + static_cast<int>(offered_types().size()); }

int forced_type_value(int option) {
  if (option <= 0) return 0;  // the game's Random: no override
  const std::vector<int32_t> types = offered_types();
  return static_cast<size_t>(option - 1) < types.size() ? types[option - 1] : 0;
}

float reward_multiplier() {
  void* d = difficulty();
  return d ? Difficulty::GetRewardMultiplier(d) : 0.0f;
}

int bonus_reward() {
  void* v = values();
  return v ? LevelValues::GetInvestigationBonusReward(v) : 0;
}

float sanity() {
  void* player = game::local_player();
  void* s = player ? Player::sanity.get(player) : nullptr;
  return s ? PlayerSanity::insanity.get(s) : 0.0f;
}

bool dead() {
  void* player = game::local_player();
  return player && Player::isDead.get(player);
}

bool frozen() { return g_frozen; }

float time_scale() { return Time::get_timeScale(); }

// ---- The rows' actions ----

void force_hunt() {
  void* ai = game::ghost_ai();
  if (ai) GhostAI::ChangeState(ai, phasmo::GhostState::hunting(), nullptr, true);
}

void end_hunt() {
  void* ai = game::ghost_ai();
  if (ai) GhostAI::ChangeState(ai, phasmo::GhostState::wander(), nullptr, true);
}

void clear_cooldown() {
  void* ai = game::ghost_ai();
  if (!ai) return;
  GhostAI::postHuntCooldown.set(ai, false);
  GhostAI::huntsHeldOff.set(ai, false);
}

void appear() {
  void* ai = game::ghost_ai();
  if (ai) GhostAI::Appear(ai, -1);  // -1: the game's own pick of form
}

void unappear() {
  void* ai = game::ghost_ai();
  if (ai) GhostAI::UnAppear(ai);
}

void apply_room(int index) {
  api::RefArray rooms = game::level_rooms();
  if (index < 0 || static_cast<size_t>(index) >= rooms.size()) return;
  void* room = rooms[index];
  void* c = controller();
  if (!room || !c) return;
  // The game's own favourite-room move, then the game's own warp onto it (the content plan's row).
  GhostController::ChangeFavouriteRoomTo(c, room);
  GhostController::WarpGhostToFavouriteRoom(c);
}

int random_room() {
  const int n = room_count();
  if (n <= 0) return -1;
  static std::mt19937_64 rng{std::random_device{}()};
  return static_cast<int>(rng() % static_cast<uint64_t>(n));
}

void bump_base_speed(float delta) {
  void* ai = game::ghost_ai();
  if (!ai) return;
  float speed = GhostAI::baseSpeed.get(ai) + delta;
  if (speed < 0.0f) speed = 0.0f;
  GhostAI::baseSpeed.set(ai, speed);  // RefreshBase rereads the field, so the change holds (ghost-ai.md)
}

void fire_event(int32_t state) {
  void* ai = game::ghost_ai();
  if (ai) GhostAI::ChangeState(ai, state, nullptr, false);  // not forced: the game's guards still apply
}

void set_events_blocked(bool value) {
  void* v = values();
  if (v) LevelValues::eventsBlocked.set(v, value);
}

void set_friendly(bool value) {
  void* d = difficulty();
  if (d) Difficulty::friendlyGhost.set(d, value);
}

void set_lose_equipment(bool value) {
  void* d = difficulty();
  if (d) Difficulty::loseItemsOnDeath.set(d, value);
}

void bump_evidence(int delta) {
  void* d = difficulty();
  if (!d) return;
  int n = Difficulty::evidenceGiven.get(d) + delta;
  if (n < 0) n = 0;
  Difficulty::evidenceGiven.set(d, n);
}

void set_forced_type(int value) {
  void* d = difficulty();
  if (d) Difficulty::ghostTypeOverride.set(d, value);
}

void set_sanity(int percent) {
  void* player = game::local_player();
  void* s = player ? Player::sanity.get(player) : nullptr;
  if (s) PlayerSanity::SetInsanity(s, percent);
}

void set_frozen(bool value) {
  void* player = game::local_player();
  if (!player) return;
  Player::ToggleFreezePlayerMovement(player, value);
  g_frozen = value;
}

void kill_local() {
  // The documented mod path for a kill outside a hunt (sdk/systems/players.md): the master runs it with
  // the hunt's own death type, offline this client is the master.
  void* player = game::local_player();
  if (player) Player::BeginDeathSequence(player, phasmo::DeathType::SnappedNeck());
}

void revive_local() {
  // The two game callers (the paw's wish and the High Priestess) both pass false (players.md).
  void* player = game::local_player();
  if (player) Player::RevivePlayer(player, false);
}

void unlock_main_door() {
  void* setup = SetupPhaseController::instance.get();
  if (setup && game::alive(setup)) SetupPhaseController::UnlockMainDoor(setup);
}

void leave() {
  void* menu = pause_menu();
  if (menu) PauseMenuController::Leave(menu);
}

void quit() {
  void* menu = pause_menu();
  if (menu) PauseMenuController::Quit(menu);
}

void set_time_scale(float value) { Time::set_timeScale(value); }

std::vector<EventKind> event_kinds() {
  static const EventKind kinds[] = {
      {"randomEvent", 0}, {"light", 0},      {"door", 0},           {"throwing", 0},  {"fusebox", 0},
      {"doorKnock", 0},  {"windowKnock", 0}, {"carAlarm", 0},       {"flicker", 0},   {"cctv", 0},
      {"GhostAbility", 0}, {"mannequin", 0}, {"teleportObject", 0}, {"interact", 0},  {"ignite", 0},
      {"summoningCircle", 0}, {"musicBox", 0},
  };
  std::vector<EventKind> out;
  out.reserve(sizeof(kinds) / sizeof(kinds[0]));
  for (EventKind k : kinds) {
    // Fill the number from the game now; the name is the map's.
    for (const auto& [value, name] : state_table())
      if (std::string_view(name) == k.name) k.value = value;
    out.push_back(k);
  }
  return out;
}

}  // namespace pm::devmenu::actions
