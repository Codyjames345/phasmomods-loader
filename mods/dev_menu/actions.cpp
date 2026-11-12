// The dev menu's game side. Every action is the game's own method on the client the game would call it
// from (the mod never sends an RPC by name, never runs an RPC body); the rules are in menu_rule.h and the
// click already passed refuse() before an action here runs. Everything reads the running game now, per the
// dynamic-over-hard-coded rule: state numbers, room lists, type lists and every value come from the map.
#include "actions.h"
#include "dummy.h"

#include <chrono>
#include <cstdio>
#include <cmath>
#include <random>
#include <type_traits>
#include <string>
#include <string_view>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "log.h"
#include "runtime.h"
#include "text_table.h"

#include "Difficulty.h"
#include "Equipment.h"
#include "EvidenceController.h"
#include "GhostInfo.h"
#include "GameController.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostHuntState.h"
#include "GhostTypes.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "MainManager.h"
#include "Photon.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "Rewards.h"
#include "Sensor.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityWorld.h"
#include "Weather.h"

#include "content.h"

namespace pm::devmenu::actions {
namespace {

using namespace phasmo;

bool g_session = false;      // the dev session: on once the menu has opened in this run
bool g_sessionTold = false;  // its log line was written
Switches g_switches;

// Whether a wrapper's map entry resolved in this game build, for a decision that must not guess at a value
// the game could not give (the same check the other mods use).
template <auto& Member>
bool entry_resolved() {
  return std::remove_cvref_t<decltype(Member)>::use::resolved();
}
bool g_frozen = false;       // our last ToggleFreezePlayerMovement for the local investigator
// What the ghost's vision feeds on, and the layer mask it had before the menu emptied it: so the menu can
// give the sight back, and so a mod stop never leaves a contract with a blind ghost.
api::Held g_visionInput;
int32_t g_visionLayers = 0;
bool g_visionEmptied = false;
// The physical step noclip switched off, so the walk hook knows its rig and a stop can switch it back on.
api::Held g_noclipController;
// The revive's loan: the Difficulty whose cursed possessions are lent the Monkey Paw, its own array to give back,
// and when the loan ends at the latest.
api::Held g_lentFrom;
api::Held g_ownCursed;
uint64_t g_loanEnds = 0;
uint64_t g_nextSanityLock = 0;  // the sanity lock asks the game's setter at most this often (ms clock)

uint64_t now_ms() {
  return static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
}

// ---- The hooks. Their creation is actions::start(): if one cannot be made the mod stays off, so a dev
// session always has progression suppression (fail closed). ----

// God mode: a hunt's kill of the local investigator is refused before the game's Kill runs. Offline the
// kill decision is this client's and the kill's network body is never touched.
bool det_kill(api::Original<GhostHuntState::Kill> original, void* hunt, void* player, bool force, float reach,
              float slowReach) {
  if (dummy::on_kill(player)) return true;  // the dummy dies here: its Photon owner, which it lacks, is never asked
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

// A dev session spends no consumable: the saved stock's decrement is skipped while the session is on.
void det_remove_amount(api::Original<EquipmentInfo::RemoveAmountOwned> original, void* info, int32_t amount) {
  if (suppress_progress(g_session)) return;
  original(info, amount);
}

// The reward screen's results save the contract's money, progress the money and experience challenges and save
// the seasonal event's score (RewardManager.Results, sdk/systems/contract.md). None of it runs in a dev session;
// the screen's Next does not wait on it (its decompile: it refreshes the inventory and the level read-out).
void det_start_results(api::Original<RewardManager::StartResults> original, void* screen) {
  if (suppress_progress(g_session)) {
    PM_LOG("dev-menu: the reward screen's results are skipped: this run is a dev session");
    return;
  }
  original(screen);
}

// The experience pay: every call is the reward screen's (its Skip among them).
void det_add_xp(api::Original<SingleplayerProfile::AddXPAndFunds> original, void* profile, int32_t xp,
                int32_t money) {
  if (suppress_progress(g_session)) return;
  original(profile, xp, money);
}

// A death with the difficulty's lose-items set costs the brought equipment at the reward screen. In a dev
// session the screen's other branch runs instead, the game's own return of it to the player's stock.
void det_reset_equipment(api::Original<InventoryManager::ResetAllEquipment> original, void* inventory) {
  if (return_instead_of_losing(g_session)) {
    InventoryManager::ReturnItemstoInventory(inventory);
    return;
  }
  original(inventory);
}

// The save freeze: once a dev session has started, EasySave3's commit of a written save is skipped, so the save on
// disk never changes for the rest of the run (menu_rule.h, freeze_save). The temporary copy the writer made is all
// that changes, and the next write rewrites it from the real save.
void det_commit(api::Original<ES3IO::CommitBackup> original, void* settings) {
  if (freeze_save(g_session)) {
    static bool told = false;
    if (!told) {
      told = true;
      PM_LOG("dev-menu: the save is frozen for this run: the game's saves stop at EasySave3's temporary copy");
    }
    return;
  }
  original(settings);
}

// The store switches. Each answers only inside the single player sandbox (store_switch_applies), so a lobby joined
// later in the same run gets the game's own answers.
bool det_unlocked(api::Original<StoreItem::IsUnlocked> original, void* item) {
  if (store_switch_applies(g_switches.unlockAll, can_open())) return true;
  return original(item);
}
bool det_meets_level(api::Original<StoreItem::MeetsLevel> original, void* item) {
  if (store_switch_applies(g_switches.unlockAll, can_open())) return true;
  return original(item);
}
bool det_can_afford(api::Original<StoreItem::CanAfford> original, void* item, int32_t amount) {
  if (store_switch_applies(g_switches.freeShop, can_open())) return true;
  return original(item, amount);
}
bool det_can_buy(api::Original<StoreEquipment::CanBuy> original, void* entry, int32_t amount) {
  if (store_switch_applies(g_switches.freeShop, can_open())) return true;
  return original(entry, amount);
}

// Noclip: while the rig's physical step is switched off, the walk the game asks for this frame moves the rig
// along the camera instead, so looking up and walking forward climbs. The horizontal walk is split into the
// part along the camera's facing, which follows the camera's pitch, and the part across it, which stays level;
// its length is the game's own walk or sprint at the rig's own speed multiplier.
void det_move(api::Original<PhysicsCharacterController::Move> original, void* controller, Vector3 walk) {
  original(controller, walk);
  if (!g_switches.noclip || !controller || controller != static_cast<void*>(g_noclipController)) return;
  void* camera = game::local_camera();
  void* body = game::transform_of(controller);
  if (!camera || !body) return;
  const Vector3 f = game::forward(game::transform_of(camera));
  const float flat = std::hypot(f.x, f.z);
  Vector3 step{walk.x, 0.0f, walk.z};
  if (flat > 0.0001f) {
    const float fx = f.x / flat, fz = f.z / flat;
    const float along = walk.x * fx + walk.z * fz;  // metres along the camera's level facing
    // The part along the facing is turned onto the camera's own forward, a unit vector, so its length stays.
    step = {walk.x - along * fx + along * f.x, along * f.y, walk.z - along * fz + along * f.z};
  }
  const Vector3 at = game::position(body);
  game::set_position(body, {at.x + step.x, at.y + step.y, at.z + step.z});
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
         api::hook<RewardManager::StartResults>(&det_start_results, "Dev Menu: no reward results") &&
         api::hook<SingleplayerProfile::AddXPAndFunds>(&det_add_xp, "Dev Menu: no experience pay") &&
         api::hook<InventoryManager::ResetAllEquipment>(&det_reset_equipment, "Dev Menu: equipment comes back") &&
         api::hook<PhysicsCharacterController::Move>(&det_move, "Dev Menu: noclip moves along the camera") &&
         api::hook<ES3IO::CommitBackup>(&det_commit, "Dev Menu: the save stays as it was in a dev session") &&
         api::hook<StoreItem::IsUnlocked>(&det_unlocked, "Dev Menu: everything unlocked") &&
         api::hook<StoreItem::MeetsLevel>(&det_meets_level, "Dev Menu: every level reached") &&
         api::hook<StoreItem::CanAfford>(&det_can_afford, "Dev Menu: the store is free") &&
         api::hook<StoreEquipment::CanBuy>(&det_can_buy, "Dev Menu: the buy button is free") &&
         dummy::hook() &&
         hooks::enable_all();
}

void end_loan();

void stop() {
  // The host takes the mod's hooks down when the mod stops; nothing to do here beyond our own state. The
  // one thing the menu leaves in the game is an emptied vision mask, and that goes back: a mod must never
  // stop with the contract's ghost left sightless.
  g_frozen = false;
  if (g_visionEmptied && g_visionInput && api::alive(g_visionInput) && entry_resolved<RangeSensor::layers>())
    RangeSensor::layers.set(g_visionInput, g_visionLayers);
  g_visionEmptied = false;
  g_visionInput = nullptr;
  // Nor with the investigator's physical step switched off, or the contract's possessions on loan.
  set_noclip(false);
  end_loan();
  dummy::despawn();
  g_switches = Switches{};
}

// Puts the contract's own cursed possessions back once the revive the loan was for has landed.
void end_loan() {
  if (g_lentFrom && api::alive(g_lentFrom)) Difficulty::actualCursedItems.set(g_lentFrom, g_ownCursed);
  g_lentFrom = nullptr;
  g_ownCursed = nullptr;
  g_loanEnds = 0;
}

void tick() {
  dummy::tick();
  if (g_lentFrom) {
    void* player = game::local_player();
    const bool revived = player && !Player::isDead.get(player) && !Player::dying.get(player);
    if (revived || now_ms() >= g_loanEnds) end_loan();
  }
  // A scene change takes the rig, and its switched-off step, with it: the switch goes off with them.
  if (g_switches.noclip && (!g_noclipController || !api::alive(g_noclipController))) {
    g_switches.noclip = false;
    g_noclipController = nullptr;
  }
  if (g_switches.lockSanity) {
    void* player = game::local_player();
    void* s = player ? Player::sanity.get(player) : nullptr;
    const uint64_t now = now_ms();
    if (s && now >= g_nextSanityLock && sanity_needs_reset(PlayerSanity::insanity.get(s), g_switches.lockedSanity)) {
      g_nextSanityLock = now + 250;
      PlayerSanity::SetInsanity(s, g_switches.lockedSanity);
    }
  }
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

bool can_open() {
  // Fail closed: with either probe unreadable this client cannot tell the sandbox from a room. The game's
  // own single player mode is offline mode plus a joined room, and the main menu is a connected Photon
  // client in no room, so the gate is offline mode or no room (re/out/offline.c).
  if (!phasmo::PhotonNetwork::get_OfflineMode.available() ||
      !phasmo::PhotonNetwork::get_InRoom.available())
    return false;
  return may_open(offline(), game::in_room());
}

Can snapshot() {
  Can c;
  c.sandbox = can_open();
  void* level = game::level_controller();
  c.contract = level && game::alive(level);
  c.ghost = c.contract && game::ghost_ai() != nullptr;
  c.difficulty = difficulty() != nullptr;
  void* player = game::local_player();
  c.player = player != nullptr;
  c.hunting = c.ghost && GhostAI::isHunting.get(game::ghost_ai());
  c.dead = c.player && Player::isDead.get(player);
  c.dying = c.player && Player::dying.get(player);
  c.friendly = friendly();
  c.dummy = dummy::player() != nullptr;
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
  // GetInvestigationBonusReward throws a NullReferenceException unless the contract's objectives are set
  // (its map note): at the main menu it threw on every frame the menu was open (the live log of
  // 2026-11-11). Ask only while a contract runs and its main objective is there.
  void* v = values();
  void* level = game::level_controller();
  if (!v || !level || !game::alive(level) || !LevelValues::mainObjective.get(v)) return 0;
  return LevelValues::GetInvestigationBonusReward(v);
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

// The rig the game takes a speed change from: LocalPCPlayer's own methods, so a VR rig (whose stamina applies
// its speed instead, VRStamina.Update) is not asked.
void* pc_rig() {
  void* rig = game::local_player_component();
  return rig && LocalPCPlayer::type.is(rig) ? rig : nullptr;
}

float speed_multiplier() {
  void* rig = game::local_player_component();
  return rig ? LocalPlayer::speedMultiplier.get(rig) : 0.0f;
}

bool speed_settable() { return pc_rig() != nullptr && LocalPCPlayer::SetPlayerSpeedMultiplier.available(); }

// The sensor the ghost sees through, at the end its line-of-sight sensor tests: GhostAI.vision is the LOS
// sensor, and what it tests is its input sensor's signals, so that is the sensor whose layers decide what
// ever reaches the ghost (Sensor.h). Null, fail closed, when either half is missing.
void* ghost_vision_input() {
  void* ai = game::ghost_ai();
  if (!ai) return nullptr;
  void* vision = GhostAI::vision.get(ai);
  if (!vision) return nullptr;
  return LOSSensor::get_InputSensor(vision);
}

bool ghost_blind() {
  if (!entry_resolved<RangeSensor::layers>()) return false;
  void* input = ghost_vision_input();
  return input && RangeSensor::layers.get(input) == 0;
}

std::string hunt_target_room() {
  void* ai = game::ghost_ai();
  if (!ai) return {};
  void* machine = GhostAI::stateMachine.get(ai);
  if (!machine) return {};
  void* state = GhostStateMachine::current.get(machine);
  if (!state || !GhostHuntState::type.is(state)) return {};  // not hunting: no hunt target
  void* target = GhostHuntState::target.get(state);
  if (!target) target = GhostHuntState::lastPlayer.get(state);
  void* room = target ? Player::currentRoom.get(target) : nullptr;
  if (!room) return {};
  const api::RefArray rooms = game::level_rooms();
  for (size_t i = 0; i < rooms.size(); ++i)
    if (rooms[i] == room) return room_name(static_cast<int>(i));
  return {};
}

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

void bump_speed_multiplier(int steps) {
  void* rig = pc_rig();
  if (!rig) return;
  // The game's own setter, which stores the multiplier and re-applies it to the controller in one call (the
  // path a Monkey Paw wish's speed takes).
  LocalPCPlayer::SetPlayerSpeedMultiplier(rig, next_speed_multiplier(speed_multiplier(), steps));
}

void reset_speed_multiplier() {
  void* rig = pc_rig();
  if (rig) LocalPCPlayer::ResetPlayerSpeedMultiplier(rig);
}

void set_ghost_blind(bool blind) {
  void* input = ghost_vision_input();
  if (!input || !entry_resolved<RangeSensor::layers>()) return;
  if (blind) {
    // Remember this contract's mask the first time, and never overwrite it with an emptied one.
    if (!g_visionInput || g_visionInput != input || !g_visionEmptied) {
      g_visionInput = input;
      g_visionLayers = RangeSensor::layers.get(input);
    }
    RangeSensor::layers.set(input, 0);
    g_visionEmptied = true;
    return;
  }
  if (g_visionEmptied && g_visionInput == input && api::alive(g_visionInput))
    RangeSensor::layers.set(input, g_visionLayers);
  g_visionEmptied = false;
}

bool pause_player(bool held) {
  // The game's own hold for a full-screen overlay, what opening the journal does (PCCanvas's own entry
  // note): no walking, no looking, crosshair away, cursor free. Ask only for a rig that can carry it:
  // the release reads the rig's journal controller and throws when the rig has none (the decompile,
  // 2026-11-11), so a rig without a journal or HUD never hears the call at all, and the answer says
  // whether the game was asked (the mod only ever releases a hold it asked for).
  // Network.localRig is typed LocalPlayer, which is abstract: the VR rig is a different class with its own
  // fields, so its object is only read as a LocalPCPlayer when it really is one.
  void* rig = game::local_player_component();
  if (!rig || !LocalPCPlayer::type.is(rig)) return false;
  void* hud = LocalPCPlayer::canvas.get(rig);
  void* journal = LocalPlayer::journalController.get(rig);
  if (!hud || !journal) return false;
  PCCanvas::PausePlayerInPlace(hud, held);
  return true;
}

void kill_local() {
  // The documented mod path for a kill outside a hunt (sdk/systems/players.md): the master runs it with
  // the hunt's own death type, offline this client is the master.
  void* player = game::local_player();
  if (player) Player::BeginDeathSequence(player, phasmo::DeathType::SnappedNeck());
}

bool revive_local() {
  // The game's own revive, Player.RevivePlayer with false as both of its callers pass. Its Revive RPC goes on only
  // when the contract carries the tarot cards or the Monkey Paw (Difficulty.actualCursedItems; players.md), the
  // two things that revive in vanilla. A contract without either borrows the paw: the array is swapped for a copy
  // with the paw added until the revive lands (tick), then the contract's own array goes back.
  void* player = game::local_player();
  void* d = difficulty();
  if (!player || !d) return false;
  if (!g_lentFrom) {
    void* own = Difficulty::actualCursedItems.get(d);
    const int32_t tarot = CursedItemType::tarotCards(), paw = CursedItemType::monkeyPaw();
    bool carried = false;
    {
      const api::Array<int32_t> items{own};
      for (size_t i = 0; i < items.size(); ++i)
        if (items[i] == tarot || items[i] == paw) carried = true;
    }
    if (!carried) {
      const size_t n = api::Array<int32_t>{own}.size();
      void* lent = CursedItemType::type.make_array(n + 1);
      if (!lent) return false;
      const api::Array<int32_t> copy{lent};
      const api::Array<int32_t> items{own};  // read afresh: make_array was a managed call
      for (size_t i = 0; i < n; ++i) copy[i] = items[i];
      copy[n] = paw;
      g_lentFrom = d;
      g_ownCursed = own;
      g_loanEnds = now_ms() + 5000;
      Difficulty::actualCursedItems.set(d, lent);
    }
  }
  Player::RevivePlayer(player, false);
  return true;
}

void unlock_main_door() {
  void* setup = SetupPhaseController::instance.get();
  if (setup && game::alive(setup)) SetupPhaseController::UnlockMainDoor(setup);
}

// The truck's ExitLevel object, for the end-contract row. Found the same careful way as the pause menu
// (at most one scene search a second; the object may sit switched off).
void* exit_level() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  const uint64_t now = static_cast<uint64_t>(
      std::chrono::steady_clock::now().time_since_epoch() / std::chrono::milliseconds(1));
  if (found && game::alive(found)) return found;
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = ExitLevel::type.type_object();
  if (!type) return nullptr;
  void* level = Object::FindObjectOfType(type, false);
  if (!level) level = Object::FindObjectOfType(type, true);
  found = level;
  return level;
}

bool end_contract() {
  // The game's own master-side starter, the call LiftButton.NetworkedUse makes on the master: its
  // coroutine sends the game's own Exit RPC with the game's own sender. Nothing here is an RPC.
  void* level = exit_level();
  if (!level) return false;
  ExitLevel::StartAttemptExitLevel(level);
  return true;
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

// ---- The ghost's type, changed mid-contract the way the game set it up at spawn ----

namespace {

// The scene's EvidenceController, found as the pause menu is (at most one scene search a second).
void* evidence_controller() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  if (found && game::alive(found)) return found;
  const uint64_t now = now_ms();
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = EvidenceController::type.type_object();
  found = type ? Object::FindObjectOfType(type, true) : nullptr;
  return found;
}

bool list_holds(void* list, int32_t value) {
  const api::List l{list};
  const size_t n = l.size();
  const api::Array<int32_t> items{l.items()};
  for (size_t i = 0; i < n; ++i)
    if (items[i] == value) return true;
  return false;
}

}  // namespace

int type_options() { return static_cast<int>(offered_types().size()); }

int32_t type_option(int index) {
  const std::vector<int32_t> types = offered_types();
  return index >= 0 && static_cast<size_t>(index) < types.size() ? types[index] : -1;
}

std::string type_name(int32_t type) {
  if (type < 0) return {};
  const std::string id = api::content::id_of(type);
  return id.empty() ? std::to_string(type) : id + " (" + std::to_string(type) + ")";
}

bool retype(int32_t type) {
  // The spawn path, run again on this client, the master: the controller's traits take the type as the roll
  // writes it (identity and behaviour), the game's own evidence pick runs for it (GhostController.PickEvidence,
  // local), and GhostAI.Init runs on those traits as GhostController.CreateGhost runs it. Init copies the traits to
  // GhostInfo and runs StartGhost: the game's own sync (SyncValuesNetworked, then SyncEvidence, whose once-flag is
  // cleared first so it takes the new evidence; both pass their sender check, this client being the master) and
  // every per-type start step (the Banshee's target, the Mimic's and the Dayan's lists). The host's content layer
  // hooks the same Init and sync, so a custom type arms as it does at spawn.
  void* c = controller();
  void* ai = game::ghost_ai();
  void* info = ai ? GhostAI::info.get(ai) : nullptr;
  const PmGameEntry* traits = api::Use<pm::map::GhostController_traits>::entry();
  if (!c || !info || !traits || type < 0) return false;
  GhostController::ghostType.set(c, type);
  GhostController::behaviourType.set(c, type);
  GhostController::PickEvidence(c);
  GhostInfo::evidenceSynced.set(info, false);
  GhostAI::Init(ai, static_cast<char*>(c) + traits->offset);
  // What the game never undoes, since a contract's type never changes: the orb the last type's evidence switched
  // on, and the freezing flag it set (EvidenceController's notes).
  if (void* ec = evidence_controller()) {
    void* given = GhostController::givenEvidence.get(c);
    const bool orbs = list_holds(given, EvidenceType::GhostOrb()) || type == GhostType::Mimic();
    void* orb = EvidenceController::ghostOrb.get(ec);
    if (!orbs && orb && api::alive(orb)) GameObject::SetActive(Component::get_gameObject(orb), false);
    EvidenceController::freezing.set(ec, list_holds(given, EvidenceType::Temperature()));
  }
  PM_LOG("dev-menu: the ghost is now type %d (%s)", type, type_name(type).c_str());
  return true;
}

// ---- The custom difficulty's options, live (the user's first ask: every option, changeable mid-contract) ----

namespace {

enum class OptionKind { Bool, Int, Float, Level, Fuse };

struct Option {
  const char* label;  // the option's text key
  OptionKind kind;
  float step;         // the menu's step, not the game's: the game's own menu sets these by its sliders
  float (*get)(void* d);
  void (*put)(void* d, float v);
};

const std::vector<Option>& options() {
  static const std::vector<Option> table = {
      {pm::i18n::key("dev-menu.option.starting-sanity"), OptionKind::Int, 5.0f, [](void* d) { return static_cast<float>(Difficulty::startingSanity.get(d)); },
       [](void* d, float v) { Difficulty::startingSanity.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.pill-restore"), OptionKind::Int, 5.0f, [](void* d) { return static_cast<float>(Difficulty::sanityPillRestorationAmount.get(d)); },
       [](void* d, float v) { Difficulty::sanityPillRestorationAmount.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.sanity-drain"), OptionKind::Float, 0.25f, [](void* d) { return Difficulty::sanityDrain.get(d); },
       [](void* d, float v) { Difficulty::sanityDrain.set(d, v); }},
      {pm::i18n::key("dev-menu.option.sprinting"), OptionKind::Int, 1.0f, [](void* d) { return static_cast<float>(Difficulty::sprinting.get(d)); },
       [](void* d, float v) { Difficulty::sprinting.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.player-speed-option"), OptionKind::Float, 0.25f, [](void* d) { return Difficulty::playerSpeed.get(d); },
       [](void* d, float v) { Difficulty::playerSpeed.set(d, v); }},
      {pm::i18n::key("dev-menu.option.flashlights"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::flashlights.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::flashlights.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.lose-equipment"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::loseItemsOnDeath.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::loseItemsOnDeath.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.ghost-speed"), OptionKind::Float, 0.25f, [](void* d) { return Difficulty::ghostSpeed.get(d); },
       [](void* d, float v) { Difficulty::ghostSpeed.set(d, v); }},
      {pm::i18n::key("dev-menu.option.roaming"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::roamingFrequency.get(d)); },
       [](void* d, float v) { Difficulty::roamingFrequency.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.favourite-room-changes"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::changingFavouriteRoom.get(d)); },
       [](void* d, float v) { Difficulty::changingFavouriteRoom.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.interactions"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::interactionAmount.get(d)); },
       [](void* d, float v) { Difficulty::interactionAmount.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.events"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::eventFrequency.get(d)); },
       [](void* d, float v) { Difficulty::eventFrequency.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.friendly"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::friendlyGhost.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::friendlyGhost.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.grace-period"), OptionKind::Int, 1.0f, [](void* d) { return static_cast<float>(Difficulty::gracePeriod.get(d)); },
       [](void* d, float v) { Difficulty::gracePeriod.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.hunt-duration"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::huntDuration.get(d)); },
       [](void* d, float v) { Difficulty::huntDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.kills-extend"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::killsExtendHuntDuration.get(d)); },
       [](void* d, float v) { Difficulty::killsExtendHuntDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.evidence"), OptionKind::Int, 1.0f, [](void* d) { return static_cast<float>(Difficulty::evidenceGiven.get(d)); },
       [](void* d, float v) { Difficulty::evidenceGiven.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.fingerprint-chance"), OptionKind::Float, 0.1f, [](void* d) { return Difficulty::fingerprintChance.get(d); },
       [](void* d, float v) { Difficulty::fingerprintChance.set(d, v); }},
      {pm::i18n::key("dev-menu.option.fingerprint-duration"), OptionKind::Int, 5.0f, [](void* d) { return static_cast<float>(Difficulty::fingerprintDuration.get(d)); },
       [](void* d, float v) { Difficulty::fingerprintDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.setup-time"), OptionKind::Int, 30.0f, [](void* d) { return static_cast<float>(Difficulty::setupTime.get(d)); },
       [](void* d, float v) { Difficulty::setupTime.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.doors-open"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::amountOfDoorsOpen.get(d)); },
       [](void* d, float v) { Difficulty::amountOfDoorsOpen.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.hiding-places"), OptionKind::Level, 1.0f, [](void* d) { return static_cast<float>(Difficulty::amountOfHidingPlaces.get(d)); },
       [](void* d, float v) { Difficulty::amountOfHidingPlaces.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.sanity-monitor"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::sanityMonitor.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::sanityMonitor.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.activity-monitor"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::activityMonitor.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::activityMonitor.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.fusebox-power"), OptionKind::Fuse, 1.0f, [](void* d) { return static_cast<float>(Difficulty::fuseBoxStartPower.get(d)); },
       [](void* d, float v) { Difficulty::fuseBoxStartPower.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.fusebox-on-map"), OptionKind::Bool, 0.0f, [](void* d) { return Difficulty::fuseBoxVisibleOnMap.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::fuseBoxVisibleOnMap.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.cursed-quantity"), OptionKind::Int, 1.0f, [](void* d) { return static_cast<float>(Difficulty::cursedPossessionsQuantity.get(d)); },
       [](void* d, float v) { Difficulty::cursedPossessionsQuantity.set(d, static_cast<int32_t>(std::lround(v))); }},
  };
  return table;
}

// The highest number of the two option enums, from the game's own value names (the map), so an update that adds a
// level still reaches it.
float top_of(OptionKind kind) {
  if (kind == OptionKind::Level) return static_cast<float>(DifficultyValueType::VeryHigh());
  if (kind == OptionKind::Fuse) return static_cast<float>(FuseBoxValueType::On());
  return 0.0f;
}

}  // namespace

int option_count() { return static_cast<int>(options().size()); }

const char* option_label(int index) {
  return index >= 0 && index < option_count() ? options()[index].label : "";
}

std::string option_value(int index) {
  void* d = difficulty();
  if (!d || index < 0 || index >= option_count()) return {};
  const Option& o = options()[index];
  const float v = o.get(d);
  if (o.kind == OptionKind::Bool) return v > 0.5f ? "on" : "off";
  if (o.kind == OptionKind::Float) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
    return buf;
  }
  return std::to_string(static_cast<int>(std::lround(v)));
}

void option_step(int index, int direction) {
  void* d = difficulty();
  if (!d || index < 0 || index >= option_count() || direction == 0) return;
  const Option& o = options()[index];
  float v = o.get(d);
  switch (o.kind) {
    case OptionKind::Bool: v = v > 0.5f ? 0.0f : 1.0f; break;
    case OptionKind::Level:
    case OptionKind::Fuse: {
      const float top = top_of(o.kind);
      v += static_cast<float>(direction);
      if (v < 0.0f) v = top;  // the enum's levels wrap, as a picker's do
      if (v > top) v = 0.0f;
      break;
    }
    default:
      v += o.step * static_cast<float>(direction);
      if (v < 0.0f) v = 0.0f;  // the menu's floor: none of these options means anything below nothing
      break;
  }
  o.put(d, v);
}

// ---- The type's own dials ----

std::string type_dials() {
  void* ai = game::ghost_ai();
  void* c = controller();
  if (!ai || !c) return {};
  std::string out = "flames " + std::to_string(GhostAI::flamesBlownOut.get(ai));
  out += ", thaye " + std::to_string(GhostController::thayeStep.get(c)) + " (" +
         std::to_string(GhostController::thayeThreshold.get(c)) + ")";
  out += ", gallu " + std::to_string(GhostController::galluState.get(c));
  out += ", dayan " + std::to_string(GhostController::dayanState.get(c));
  if (GhostAI::interactionBoost.get(ai)) out += ", boost";
  void* banshee = GhostAI::bansheeTarget.get(ai);
  if (banshee) out += banshee == game::local_player() ? ", banshee: you" : ", banshee: other";
  return out;
}

void bump_flames(int delta) {
  void* ai = game::ghost_ai();
  if (!ai) return;
  // The Onryo's hunt reads the count at its idle check (above 2: it hunts); 0 clears it as the game does.
  const int32_t n = delta == 0 ? 0 : GhostAI::flamesBlownOut.get(ai) + delta;
  GhostAI::flamesBlownOut.set(ai, n < 0 ? 0 : n);
}

void type_change_now() {
  // The game's own moment: AttemptGhostTypeStateChange runs at the ghost's next state change and, with this set,
  // runs the identity's change (the Mimic's copy, the Thaye's ageing, the Obambo's switch); any other type ignores it.
  void* c = controller();
  if (c) GhostController::typeChangeDue.set(c, true);
}

bool chase_me() {
  void* ai = game::ghost_ai();
  void* player = game::local_player();
  void* machine = ai ? GhostAI::stateMachine.get(ai) : nullptr;
  void* state = machine ? GhostStateMachine::current.get(machine) : nullptr;
  if (!state || !player || !GhostHuntState::type.is(state)) return false;
  GhostHuntState::SetTarget(state, player);
  return true;
}

// ---- Weather: the Monkey Paw's transition, and the difficulty's weather with it ----

namespace {

// The weathers a contract can have, paired by name across the game's two numberings: the scene's profiles
// (LevelValues' WeatherType, what RandomWeather.TransitionWeatherProfile takes) and the difficulty's
// (WeatherValueType, what Difficulty.actualWeather holds and the hunt's Blood Moon branch reads). Both numbers are
// read from the game now; the dead players' profile is never a contract's and is left out.
struct WeatherPair {
  const char* name;
  int32_t scene;
  int32_t played;
};

std::vector<WeatherPair> weathers() {
  return {{"clear", WeatherType::clear(), WeatherValueType::Clear()},
          {"foggy", WeatherType::foggy(), WeatherValueType::Fog()},
          {"rain", WeatherType::rain(), WeatherValueType::LightRain()},
          {"heavyRain", WeatherType::heavyRain(), WeatherValueType::HeavyRain()},
          {"snow", WeatherType::snow(), WeatherValueType::Snow()},
          {"wind", WeatherType::wind(), WeatherValueType::Wind()},
          {"sunrise", WeatherType::sunrise(), WeatherValueType::Sunrise()},
          {"bloodMoon", WeatherType::bloodMoon(), WeatherValueType::BloodMoon()}};
}

}  // namespace

int weather_count() { return static_cast<int>(weathers().size()); }

std::string weather_name(int index) {
  const std::vector<WeatherPair> all = weathers();
  return index >= 0 && static_cast<size_t>(index) < all.size() ? all[index].name : std::string{};
}

std::string weather_now() {
  void* d = difficulty();
  if (!d) return {};
  const int32_t played = Difficulty::actualWeather.get(d);
  for (const WeatherPair& w : weathers())
    if (w.played == played) return w.name;
  return std::to_string(played);
}

bool apply_weather(int index) {
  const std::vector<WeatherPair> all = weathers();
  void* scene = RandomWeather::instance.get();
  void* d = difficulty();
  if (index < 0 || static_cast<size_t>(index) >= all.size() || !d || !scene || !api::alive(scene)) return false;
  // The difficulty first, so the hunt's Blood Moon branch (GhostHuntState.UpdateSpeed, GhostActivity.GhostAbility)
  // agrees with the sky from the next frame; then the paw's own transition: LevelValues.weather, the sky, the sound.
  // The storm's lightning reads LevelValues.weather at each strike, so it starts and stops with the change.
  Difficulty::actualWeather.set(d, all[index].played);
  RandomWeather::TransitionWeatherProfile(scene, all[index].scene, true);
  return true;
}

// ---- Movement: noclip and the teleports ----

// The PC rig's physical step (the PhysicsCharacterController the FirstPersonController drives), or null for a
// VR rig, off contract, or where the class did not resolve.
void* rig_controller() {
  void* rig = pc_rig();
  void* type = PhysicsCharacterController::type.type_object();
  if (!rig || !type) return nullptr;
  void* controller = Component::GetComponentInChildren(rig, type, true);
  if (!controller) controller = Component::GetComponentInParent(rig, type, true);
  return controller && api::alive(controller) ? controller : nullptr;
}

bool noclip() { return g_switches.noclip; }

bool set_noclip(bool on) {
  if (!on) {
    if (g_noclipController && api::alive(g_noclipController)) Behaviour::set_enabled(g_noclipController, true);
    g_noclipController = nullptr;
    g_switches.noclip = false;
    return true;
  }
  void* controller = rig_controller();
  if (!controller) return false;
  g_noclipController = controller;
  Behaviour::set_enabled(controller, false);
  g_switches.noclip = true;
  return true;
}

namespace {

// The nearest point of the navigation mesh within `reach` metres, or false.
bool on_mesh(Vector3 p, float reach, Vector3& out) {
  NavMeshHit hit{};
  if (!NavMesh::SamplePosition(&p, &hit, reach, -1)) return false;
  out = hit.position;
  return true;
}

// Moves the rig so it stands over `floor` (a point of the navigation mesh) as high as it stands over the mesh
// now, so the capsule lands on its feet wherever the rig's own origin sits. The next physical step starts from
// the transform (PhysicsCharacterController's note).
bool stand_at(Vector3 floor) {
  void* controller = rig_controller();
  void* body = controller ? game::transform_of(controller) : nullptr;
  if (!body) return false;
  const Vector3 at = game::position(body);
  Vector3 under{};
  const float lift = on_mesh(at, 4.0f, under) ? at.y - under.y : 0.0f;
  game::set_position(body, {floor.x, floor.y + lift, floor.z});
  return true;
}

Vector3 ghost_position() {
  void* ai = game::ghost_ai();
  void* t = ai ? game::transform_of(ai) : nullptr;
  return t ? game::position(t) : Vector3{};
}

}  // namespace

bool me_to_ghost() {
  if (!game::ghost_ai()) return false;
  Vector3 floor{};
  if (!on_mesh(ghost_position(), 4.0f, floor)) return false;
  return stand_at(floor);
}

bool me_to_ghost_room() {
  void* room = nullptr;
  {
    const api::RefArray rooms = game::level_rooms();
    const int index = favourite_room_index();
    if (index >= 0 && static_cast<size_t>(index) < rooms.size()) room = rooms[index];
  }
  void* list = room ? LevelRoom::roomColliders.get(room) : nullptr;
  if (!list) return false;
  api::RefArray colliders{api::List{list}.items()};
  for (size_t i = 0; i < colliders.size(); ++i) {
    if (!colliders[i]) continue;
    const Bounds b = Collider::get_bounds(colliders[i]);
    Vector3 floor{};
    if (on_mesh(b.center, b.extents.y + 2.0f, floor)) return stand_at(floor);
  }
  return false;
}

bool me_to_truck() {
  // The truck is where the scene's ExitLevel (the lift) stands.
  void* lift = exit_level();
  void* t = lift ? game::transform_of(lift) : nullptr;
  Vector3 floor{};
  if (!t || !on_mesh(game::position(t), 6.0f, floor)) return false;
  return stand_at(floor);
}

bool ghost_to_me() {
  void* ai = game::ghost_ai();
  void* agent = ai ? GhostAI::agent.get(ai) : nullptr;
  void* player = game::local_player();
  void* t = player ? game::transform_of(player) : nullptr;
  if (!agent || !t) return false;
  Vector3 floor{};
  if (!on_mesh(game::position(t), 4.0f, floor)) return false;
  return NavMeshAgent::Warp(agent, &floor);
}

// ---- Read-outs added with the HUD ----

bool ghost_at(Vector3& out) {
  void* ai = game::ghost_ai();
  void* t = ai ? game::transform_of(ai) : nullptr;
  if (!t) return false;
  out = game::position(t);
  return true;
}

float ghost_agent_speed() {
  void* ai = game::ghost_ai();
  void* agent = ai ? GhostAI::agent.get(ai) : nullptr;
  return agent ? NavMeshAgent::get_speed(agent) : 0.0f;
}

std::string ghost_room_now() {
  Vector3 p{};
  if (!ghost_at(p)) return {};
  void* room = game::room_at(p);
  return room ? api::to_utf8(LevelRoom::name.get(room)) : std::string{};
}

float ghost_distance() {
  Vector3 g{};
  void* camera = game::local_camera();
  void* t = camera ? game::transform_of(camera) : nullptr;
  if (!t || !ghost_at(g)) return -1.0f;
  return game::distance(g, game::position(t));
}

bool room_centre(int index, Vector3& out) {
  const api::RefArray rooms = game::level_rooms();
  if (index < 0 || static_cast<size_t>(index) >= rooms.size() || !rooms[index]) return false;
  void* list = LevelRoom::roomColliders.get(rooms[index]);
  if (!list) return false;
  api::RefArray colliders{api::List{list}.items()};
  const size_t n = api::List{list}.size();
  for (size_t i = 0; i < n; ++i) {
    if (!colliders[i]) continue;
    out = Collider::get_bounds(colliders[i]).center;
    return true;
  }
  return false;
}

std::string given_evidence() {
  void* c = controller();
  void* list = c ? GhostController::givenEvidence.get(c) : nullptr;
  if (!list) return {};
  // The vanilla evidence by the enum's own value names, read now; anything else (a custom evidence) by number.
  const std::pair<int32_t, const char*> names[] = {
      {EvidenceType::EMF(), "EMF"},
      {EvidenceType::SpiritBox(), "SpiritBox"},
      {EvidenceType::ultraViolet(), "ultraViolet"},
      {EvidenceType::GhostOrb(), "GhostOrb"},
      {EvidenceType::GhostWritingBook(), "GhostWritingBook"},
      {EvidenceType::Temperature(), "Temperature"},
      {EvidenceType::DotsProjector(), "DotsProjector"},
  };
  std::string out;
  const api::List items{list};
  const size_t n = items.size();
  const api::Array<int32_t> values{items.items()};
  for (size_t i = 0; i < n; ++i) {
    const int32_t e = values[i];
    std::string name = std::to_string(e);
    for (const auto& [value, label] : names)
      if (value == e) name = label;
    if (!out.empty()) out += ", ";
    out += name;
  }
  return out;
}

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
