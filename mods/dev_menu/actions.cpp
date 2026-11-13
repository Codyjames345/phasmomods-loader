// The dev menu's game side. Every action is the game's own method on the client the game would call it
// from (the mod never sends an RPC by name, never runs an RPC body); the rules are in menu_rule.h and the
// click already passed refuse() before an action here runs. Everything reads the running game now, per the
// dynamic-over-hard-coded rule: state numbers, room lists, type lists and every value come from the map.
#include "actions.h"
#include "dummy.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cctype>
#include <cmath>
#include <random>
#include <type_traits>
#include <string>
#include <string_view>
#include <vector>

#include "game.h"
#include "hooks.h"
#include "log.h"
#include "managed_exception.h"
#include "runtime.h"
#include "text_table.h"

#include "Difficulty.h"
#include "Equipment.h"
#include "JournalController.h"
#include "Props.h"
#include "UnityEvents.h"
#include "Candle.h"
#include "EvidenceController.h"
#include "GhostInfo.h"
#include "GameController.h"
#include "GhostAI.h"
#include "GhostCollection.h"
#include "GhostController.h"
#include "GhostHuntState.h"
#include "GhostIdleState.h"
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

// The reward screen's results fill the screen (its rows, the ghost's type) and also save the contract's money,
// progress challenges and send the seasonal event's score (RewardManager.Results, sdk/internals/contract.md). Round 14
// skipped the results whole, which left the screen unfilled ("Failed to load ghost type" and $0 on every row, the
// user's test of 2026-11-13). Now they run, and in a dev session each progress part is taken out the way Playable
// Ghost takes them out for the ghost player: the totals are set to nothing straight after the rows are built, which is
// what the results then save; challenge progress is dropped; and the screen gets the game's own answer for a contract
// with no seasonal event, so no score is sent. The rows still show what the contract would have paid.
void det_build_rows(api::Original<RewardManager::BuildRows> original, void* screen) {
  original(screen);
  if (!suppress_progress(g_session)) return;
  RewardManager::SetTotals(screen, 0, 0);
  PM_LOG("dev-menu: the reward screen pays nothing: this run is a dev session");
}
RewardEventRow det_event_row(api::Original<RewardManager::EventRow> original, void* screen) {
  if (!suppress_progress(g_session)) return original(screen);
  return RewardEventRow{};
}
void det_challenge_progress(api::Original<ChallengesController::ChangeChallengeProgression> original, void* controller,
                            int32_t type, int32_t amount) {
  if (suppress_progress(g_session)) return;
  original(controller, type, amount);
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

// The ghost sight setting, on the hunt's own sight test. Every target the hunt picks, keeps or kills passes this
// test (GhostHuntState.Sees' note), so it decides who a hunt can see. The game's test still runs first, so its record
// of whom it has seen stays right whenever its sensor does see the player.
bool det_sees(api::Original<GhostHuntState::Sees> original, void* hunt, void* player, bool skipDeogen) {
  const bool sees = original(hunt, player, skipDeogen);
  return sight_answer(g_switches.ghostSight, sees, player && Player::isDead.get(player));
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
         api::hook<RewardManager::BuildRows>(&det_build_rows, "Dev Menu: the reward screen pays nothing",
                                             api::Twins::CanonicalOnly) &&
         api::hook<RewardManager::EventRow>(&det_event_row, "Dev Menu: no seasonal event score",
                                            api::Twins::CanonicalOnly) &&
         api::hook<ChallengesController::ChangeChallengeProgression>(
             &det_challenge_progress, "Dev Menu: no challenge progress", api::Twins::CanonicalOnly) &&
         api::hook<SingleplayerProfile::AddXPAndFunds>(&det_add_xp, "Dev Menu: no experience pay") &&
         api::hook<InventoryManager::ResetAllEquipment>(&det_reset_equipment, "Dev Menu: equipment comes back") &&
         api::hook<PhysicsCharacterController::Move>(&det_move, "Dev Menu: noclip moves along the camera") &&
         api::hook<GhostHuntState::Sees>(&det_sees, "Dev Menu: the ghost sight setting", api::Twins::CanonicalOnly) &&
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

namespace {
// The type's offset to the idle hunt check, as the game's own check last gave it for this ghost.
int g_huntOffset = 0;
bool g_haveOffset = false;
void* g_offsetGhost = nullptr;
uint64_t g_nextOffsetAsk = 0;
api::Held g_scratchIdle;  // the menu's own idle state for this ghost, never run by the state machine

// The menu's own idle state for `ai`, made as GhostAI.ChangeState makes one (GhostIdleState.ctor) and given the three
// scene controllers its enter copies (the entry's note); remade for another ghost. Null when it cannot be made.
void* scratch_idle(void* ai) {
  // A plain managed object, not an engine one: api::alive does not apply, the Held keeps it.
  if (!g_scratchIdle || GhostIdleState::ghost.get(g_scratchIdle) != ai) {
    g_scratchIdle = api::Held{};
    void* idle = GhostIdleState::type.make();
    if (!idle) return nullptr;
    GhostIdleState::ctor(idle, ai);
    g_scratchIdle = api::Held{idle};
  }
  void* level = LevelController::instance.get();
  void* game = GameController::instance.get();
  void* sound = SoundController::instance.get();
  if (!level || !game || !sound || !api::alive(level) || !api::alive(game) || !api::alive(sound)) return nullptr;
  GhostIdleState::levelController.set(g_scratchIdle, level);
  GhostIdleState::gameController.set(g_scratchIdle, game);
  GhostIdleState::soundController.set(g_scratchIdle, sound);
  return g_scratchIdle;
}

// Asks the game's own hunt insanity (GhostIdleState.HuntInsanity) twice a second, on the menu's own idle state, so
// the answer comes whatever the ghost is doing and the ghost's running state is never touched (the user's ask of
// 2026-11-13). The check starts from the insanity it measures and adds the type's offset, for every type and from
// the contract as it stands (the Mare's lights, the Yokai's talk, the Raiju's electronics...); so the answer less
// that starting insanity is the offset.
void ask_hunt_offset() {
  void* ai = game::ghost_ai();
  if (ai != g_offsetGhost) {  // another contract or another ghost: the last answer no longer holds
    g_offsetGhost = ai;
    g_haveOffset = false;
  }
  const uint64_t now = now_ms();
  if (!ai || now < g_nextOffsetAsk) return;
  g_nextOffsetAsk = now + 500;
  void* idle = scratch_idle(ai);
  if (!idle) return;
  // Where the check starts: the ghost's own player when it has one (the Banshee), the players' average otherwise.
  float base = 0.0f;
  void* focus = GhostAI::bansheeTarget.get(ai);
  if (focus && api::alive(focus)) {
    void* s = Player::sanity.get(focus);
    if (!s) return;
    base = PlayerSanity::insanity.get(s);
  } else {
    base = GameController::GetAveragePlayerInsanity(GameController::instance.get());
  }
  GhostIdleState::rollRange.set(idle, 100);  // as made: the Shade's rules grow it on every run
  try {
    GhostIdleState::HuntInsanity(idle);
  } catch (const Il2CppExceptionWrapper&) {
    static bool told = false;
    if (!told) PM_LOG("dev-menu: the game's hunt check threw on the menu's idle state; the hunt line stays unknown");
    told = true;
    return;
  }
  g_huntOffset = GhostIdleState::insanity.get(idle) - static_cast<int>(base);
  g_haveOffset = true;
}
}  // namespace

int hunt_threshold() {
  void* c = controller();
  if (!c || !game::ghost_ai()) return -1;
  // The Thaye's check is by its age instead: it hunts while the insanity is above 100 less
  // GhostController.thayeThreshold, so at a sanity below the threshold.
  if (api::content::id_of(GhostController::behaviourType.get(c)) == "vanilla.thaye")
    return GhostController::thayeThreshold.get(c) - 1;
  return g_haveOffset ? kHuntInsanity + g_huntOffset : -1;
}

void tick() {
  dummy::tick();
  ask_hunt_offset();
  end_contract_tick();
  keep_supplies();
  hold_sight();
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
    if (s && now >= g_nextSanityLock &&
        sanity_needs_reset(100.0f - PlayerSanity::insanity.get(s), g_switches.lockedSanity)) {
      g_nextSanityLock = now + 250;
      PlayerSanity::SetInsanity(s, 100 - g_switches.lockedSanity);  // the lock holds sanity; the setter takes insanity
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
  // The identity card: the type the contract shows, and the rules its AI follows when they differ. A custom ghost's
  // behaviour type is the game's "no type" (the host runs its rules on the generic AI, docs/custom-ghosts.md), so it
  // shows only its type; a Mimic's is the type it copies (GhostController.ghostType and .behaviourType).
  const int32_t type = GhostController::ghostType.get(c);
  std::string out = pretty_type(type) + " (" + std::to_string(type) + ")";
  const int32_t behaviour = GhostController::behaviourType.get(c);
  if (behaviour != type && behaviour != GhostType::None()) out += ", acting as " + pretty_type(behaviour);
  return out;
}

std::string pretty_type(int32_t type) {
  // A developer's name for a type, from its content id: the part after the last dot, words split and capitalised
  // ("vanilla.yurei" reads Yurei, "phasmomods.custom-ghosts.black_shuck" reads Black Shuck).
  const std::string id = api::content::id_of(type);
  if (id.empty()) return std::to_string(type);
  std::string name = id.substr(id.rfind('.') + 1);
  bool start = true;
  for (char& ch : name) {
    if (ch == '_' || ch == '-') {
      ch = ' ';
      start = true;
    } else if (start) {
      ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
      start = false;
    }
  }
  return name;
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
  return pretty_type(type);
}

// Every ghost type the game and the running mods know, in number order: the host answers a content id for each
// (vanilla.<name> from the GhostType enum's own value names, a custom ghost's own id), the "no type" value aside. Read
// without a contract too, so the forced type can be chosen in the lobby, where the next contract's roll reads it.
std::vector<int32_t> known_types() {
  static uint64_t nextScan = 0;
  static std::vector<int32_t> types;
  const uint64_t now = now_ms();
  if (now < nextScan && !types.empty()) return types;
  nextScan = now + 2000;
  types.clear();
  const int32_t none = GhostType::None();
  for (int32_t t = 0; t < 256; ++t)
    if (t != none && !api::content::id_of(t).empty()) types.push_back(t);
  return types;
}

int forced_type_options() { return 1 + static_cast<int>(known_types().size()); }

int forced_type_value(int option) {
  if (option <= 0) return 0;  // the game's Random: no override
  const std::vector<int32_t> types = known_types();
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
  // PlayerSanity.insanity is how far gone the investigator is (0 fully sane): the sanity the game shows is 100 less it
  // (Playable Ghost's session.h reads it the same way).
  void* player = game::local_player();
  void* s = player ? Player::sanity.get(player) : nullptr;
  return s ? 100.0f - PlayerSanity::insanity.get(s) : 0.0f;
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
  // SetInsanity takes the insanity, so a sanity of `percent` is an insanity of 100 less it.
  void* player = game::local_player();
  void* s = player ? Player::sanity.get(player) : nullptr;
  if (s) PlayerSanity::SetInsanity(s, 100 - (percent < 0 ? 0 : percent > 100 ? 100 : percent));
}

void set_frozen(bool value) {
  void* player = game::local_player();
  if (!player) return;
  Player::ToggleFreezePlayerMovement(player, value);
  g_frozen = value;
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

namespace {

// The truck's door button, found as the pause menu is (at most one scene search a second).
void* lift_button() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  if (found && game::alive(found)) return found;
  const uint64_t now = now_ms();
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = LiftButton::type.type_object();
  found = type ? Object::FindObjectOfType(type, false) : nullptr;
  return found;
}

// The truck's button pressed as a player presses it: the button's Start adds its own press to its interaction's use
// event at run time (PhotonObjectInteract.AddUseEvent), and a player's use runs that event; the press sends
// LiftButton.AttemptUse to the master. Round 14 looked for a listener saved in the scene instead, and there is none
// (the user's test of 2026-11-13: "no event that presses it").
bool press(void* button) {
  void* interact = LiftButton::photonInteract.get(button);
  if (!interact) return false;
  void* events[16] = {};
  const size_t n = PhotonObjectInteract::type.fields_of<UnityEvent::type>(interact, false, events, 16);
  for (size_t i = 0; i < n && i < 16; ++i) {
    void* lists = events[i] ? UnityEventBase::m_Calls.get(events[i]) : nullptr;
    const api::List calls{lists ? InvokableCallList::m_RuntimeCalls.get(lists) : nullptr};
    for (size_t c = 0; c < calls.size(); ++c) {
      void* call = calls[c];
      void* action = call && InvokableCall::type.is(call) ? InvokableCall::Delegate.get(call) : nullptr;
      if (action && Delegate::m_target.get(action) == button) {
        UnityEvent::Invoke(events[i]);
        return true;
      }
    }
  }
  PM_LOG("dev-menu: none of the truck button's events holds its press; it was not pressed");
  return false;
}

// A press the menu is waiting to make once the game counts the investigator inside the truck.
uint64_t g_pressAt = 0;
uint64_t g_pressGiveUp = 0;

}  // namespace

EndContract end_contract_by_button() {
  // The game's own way to end a contract is the truck's button: the press goes to the master, which closes the doors
  // only with every living player inside, then the doors' use starts the truck's leaving (LiftButton.AttemptUse's
  // note). Calling ExitLevel.StartAttemptExitLevel directly did nothing: only the press sets the flag it needs.
  void* button = lift_button();
  void* exit = button ? LiftButton::exitLevel.get(button) : nullptr;
  if (!button || !exit) return EndContract::NoTruck;
  if (LiftButton::isClosed.get(button)) return EndContract::DoorsShut;
  if (!ExitLevel::ThereAreAlivePlayersOutsideTheTruck(exit))
    return press(button) ? EndContract::Pressed : EndContract::NoTruck;
  // Outside: into the truck first, then the press once the truck's inside has counted the investigator (its trigger
  // runs on the physics step, so not this frame). Noclip goes off first: with the investigator's collider off the
  // truck's trigger never counts them inside.
  set_noclip(false);
  if (!me_to_truck()) return EndContract::NoTruck;
  const uint64_t now = now_ms();
  g_pressAt = now + 300;
  g_pressGiveUp = now + 4000;
  return EndContract::Waiting;
}

void end_contract_tick() {
  if (!g_pressAt) return;
  const uint64_t now = now_ms();
  if (now < g_pressAt) return;
  void* button = lift_button();
  void* exit = button ? LiftButton::exitLevel.get(button) : nullptr;
  if (button && exit && !LiftButton::isClosed.get(button) && !ExitLevel::ThereAreAlivePlayersOutsideTheTruck(exit)) {
    press(button);
    g_pressAt = 0;
    return;
  }
  if (now >= g_pressGiveUp) {
    PM_LOG("dev-menu: the truck never counted the investigator inside; its button was not pressed");
    g_pressAt = 0;
  }
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

std::string content_line() {
  // The contract's offered collection, and how many of it are custom ghosts: a vanilla type's content id is the
  // host's "vanilla." one, a custom ghost's is its mod's own.
  const std::vector<int32_t> types = offered_types();
  int custom = 0;
  for (const int32_t t : types) {
    const std::string id = api::content::id_of(t);
    if (!id.empty() && id.rfind("vanilla.", 0) != 0) ++custom;
  }
  if (types.empty()) return {};
  return std::to_string(types.size()) + " types, " + std::to_string(custom) + " custom";
}

std::string type_name(int32_t type) {
  if (type < 0) return {};
  return pretty_type(type) + " (" + std::to_string(type) + ")";
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

enum class OptionKind { Bool, Int, Float, Level, Fuse, Choice };

// When the game reads the option (re/dump/notes/2026-11-12-difficulty-readers.md, from decompiles of every reader
// reached through LevelValues): Live on each use or frame, so a change applies at once; Start once as the contract
// begins, so a change waits for the next contract; Mixed both; Unknown where no reader was found.
enum class OptionWhen { Live, Start, Mixed, Unknown };

struct Option {
  const char* label;  // the option's text key
  OptionGroup group;  // where the game's own custom difficulty menu puts it
  OptionWhen when;
  OptionKind kind;
  float step;         // the menu's step and top, not the game's: the game's own menu sets these with its sliders
  float top;          // a number's or a choice's top; levels and the fuse box take theirs from the game's enum
  float (*get)(void* d);
  void (*put)(void* d, float v);
};

const std::vector<Option>& options() {
  static const std::vector<Option> table = {
      {pm::i18n::key("dev-menu.option.starting-sanity"), OptionGroup::Player, OptionWhen::Mixed, OptionKind::Int, 5.0f, 100.0f,
       [](void* d) { return static_cast<float>(Difficulty::startingSanity.get(d)); },
       [](void* d, float v) { Difficulty::startingSanity.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.pill-restore"), OptionGroup::Player, OptionWhen::Live, OptionKind::Int, 5.0f, 100.0f,
       [](void* d) { return static_cast<float>(Difficulty::sanityPillRestorationAmount.get(d)); },
       [](void* d, float v) { Difficulty::sanityPillRestorationAmount.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.sanity-drain"), OptionGroup::Player, OptionWhen::Start, OptionKind::Float, 0.25f, 5.0f,
       [](void* d) { return Difficulty::sanityDrain.get(d); },
       [](void* d, float v) { Difficulty::sanityDrain.set(d, v); }},
      {pm::i18n::key("dev-menu.option.sprinting"), OptionGroup::Player, OptionWhen::Mixed, OptionKind::Choice, 1.0f, 2.0f,
       [](void* d) { return static_cast<float>(Difficulty::sprinting.get(d)); },
       [](void* d, float v) { Difficulty::sprinting.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.player-speed-option"), OptionGroup::Player, OptionWhen::Start, OptionKind::Float, 0.25f, 5.0f,
       [](void* d) { return Difficulty::playerSpeed.get(d); },
       [](void* d, float v) { Difficulty::playerSpeed.set(d, v); }},
      {pm::i18n::key("dev-menu.option.flashlights"), OptionGroup::Player, OptionWhen::Live, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::flashlights.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::flashlights.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.lose-equipment"), OptionGroup::Player, OptionWhen::Live, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::loseItemsOnDeath.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::loseItemsOnDeath.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.ghost-speed"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Float, 0.25f, 5.0f,
       [](void* d) { return Difficulty::ghostSpeed.get(d); },
       [](void* d, float v) { Difficulty::ghostSpeed.set(d, v); }},
      {pm::i18n::key("dev-menu.option.roaming"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::roamingFrequency.get(d)); },
       [](void* d, float v) { Difficulty::roamingFrequency.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.favourite-room-changes"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::changingFavouriteRoom.get(d)); },
       [](void* d, float v) { Difficulty::changingFavouriteRoom.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.interactions"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::interactionAmount.get(d)); },
       [](void* d, float v) { Difficulty::interactionAmount.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.events"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::eventFrequency.get(d)); },
       [](void* d, float v) { Difficulty::eventFrequency.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.friendly"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::friendlyGhost.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::friendlyGhost.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.grace-period"), OptionGroup::Ghost, OptionWhen::Start, OptionKind::Int, 1.0f, 10.0f,
       [](void* d) { return static_cast<float>(Difficulty::gracePeriod.get(d)); },
       [](void* d, float v) { Difficulty::gracePeriod.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.hunt-duration"), OptionGroup::Ghost, OptionWhen::Start, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::huntDuration.get(d)); },
       [](void* d, float v) { Difficulty::huntDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.kills-extend"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::killsExtendHuntDuration.get(d)); },
       [](void* d, float v) { Difficulty::killsExtendHuntDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.evidence"), OptionGroup::Ghost, OptionWhen::Start, OptionKind::Int, 1.0f, 3.0f,
       [](void* d) { return static_cast<float>(Difficulty::evidenceGiven.get(d)); },
       [](void* d, float v) { Difficulty::evidenceGiven.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.fingerprint-chance"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Float, 0.05f, 1.0f,
       [](void* d) { return Difficulty::fingerprintChance.get(d); },
       [](void* d, float v) { Difficulty::fingerprintChance.set(d, v); }},
      {pm::i18n::key("dev-menu.option.fingerprint-duration"), OptionGroup::Ghost, OptionWhen::Live, OptionKind::Int, 5.0f, 300.0f,
       [](void* d) { return static_cast<float>(Difficulty::fingerprintDuration.get(d)); },
       [](void* d, float v) { Difficulty::fingerprintDuration.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.setup-time"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Int, 30.0f, 600.0f,
       [](void* d) { return static_cast<float>(Difficulty::setupTime.get(d)); },
       [](void* d, float v) { Difficulty::setupTime.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.doors-open"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::amountOfDoorsOpen.get(d)); },
       [](void* d, float v) { Difficulty::amountOfDoorsOpen.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.hiding-places"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Level, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::amountOfHidingPlaces.get(d)); },
       [](void* d, float v) { Difficulty::amountOfHidingPlaces.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.sanity-monitor"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::sanityMonitor.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::sanityMonitor.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.activity-monitor"), OptionGroup::Contract, OptionWhen::Unknown, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::activityMonitor.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::activityMonitor.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.fusebox-power"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Fuse, 1.0f, 0.0f,
       [](void* d) { return static_cast<float>(Difficulty::fuseBoxStartPower.get(d)); },
       [](void* d, float v) { Difficulty::fuseBoxStartPower.set(d, static_cast<int32_t>(std::lround(v))); }},
      {pm::i18n::key("dev-menu.option.fusebox-on-map"), OptionGroup::Contract, OptionWhen::Unknown, OptionKind::Bool, 0.0f, 1.0f,
       [](void* d) { return Difficulty::fuseBoxVisibleOnMap.get(d) ? 1.0f : 0.0f; },
       [](void* d, float v) { Difficulty::fuseBoxVisibleOnMap.set(d, v > 0.5f); }},
      {pm::i18n::key("dev-menu.option.cursed-quantity"), OptionGroup::Contract, OptionWhen::Start, OptionKind::Int, 1.0f, 7.0f,
       [](void* d) { return static_cast<float>(Difficulty::cursedPossessionsQuantity.get(d)); },
       [](void* d, float v) { Difficulty::cursedPossessionsQuantity.set(d, static_cast<int32_t>(std::lround(v))); }},
  };
  return table;
}

// A level's or the fuse box's top, from the game's own value names (the map), so an update that adds one reaches it.
float top_of(const Option& o) {
  if (o.kind == OptionKind::Level) return static_cast<float>(DifficultyValueType::VeryHigh());
  if (o.kind == OptionKind::Fuse) return static_cast<float>(FuseBoxValueType::On());
  if (o.kind == OptionKind::Bool) return 1.0f;
  return o.top;
}

bool valid(int index) { return index >= 0 && index < static_cast<int>(options().size()); }

}  // namespace

int option_count() { return static_cast<int>(options().size()); }

int option_when(int index) { return valid(index) ? static_cast<int>(options()[index].when) : 3; }

const char* option_label(int index) { return valid(index) ? options()[index].label : ""; }

OptionGroup option_group(int index) { return valid(index) ? options()[index].group : OptionGroup::Player; }

OptionControl option_control(int index) {
  if (!valid(index)) return OptionControl::Slider;
  switch (options()[index].kind) {
    case OptionKind::Bool: return OptionControl::Switch;
    case OptionKind::Level:
    case OptionKind::Fuse:
    case OptionKind::Choice: return OptionControl::Choice;
    default: return OptionControl::Slider;
  }
}

float option_top(int index) { return valid(index) ? top_of(options()[index]) : 0.0f; }

float option_step_size(int index) { return valid(index) ? options()[index].step : 1.0f; }

float option_number(int index) {
  void* d = difficulty();
  return d && valid(index) ? options()[index].get(d) : 0.0f;
}

void option_set(int index, float value) {
  void* d = difficulty();
  if (!d || !valid(index)) return;
  const Option& o = options()[index];
  const float top = top_of(o);
  o.put(d, value < 0.0f ? 0.0f : (value > top ? top : value));
}

std::string option_value(int index) {
  void* d = difficulty();
  if (!d || !valid(index)) return {};
  const Option& o = options()[index];
  const float v = o.get(d);
  if (o.kind == OptionKind::Bool) return v > 0.5f ? "on" : "off";
  // A level or the fuse box by the game's own value names (Difficulty.ValueType, Difficulty's fuse box enum), read
  // through the map, so a renumbering in an update still names them right.
  const int32_t n = static_cast<int32_t>(std::lround(v));
  if (o.kind == OptionKind::Level) {
    static constexpr const char* kLevelKeys[] = {i18n::key("dev-menu.level.none"), i18n::key("dev-menu.level.low"),
                                                 i18n::key("dev-menu.level.medium"), i18n::key("dev-menu.level.high"),
                                                 i18n::key("dev-menu.level.very-high")};
    const int32_t values[] = {DifficultyValueType::None(), DifficultyValueType::Low(), DifficultyValueType::Medium(),
                              DifficultyValueType::High(), DifficultyValueType::VeryHigh()};
    for (int i = 0; i < 5; ++i)
      if (values[i] == n) return i18n::text(kLevelKeys[i]);
  }
  if (o.kind == OptionKind::Fuse) {
    static constexpr const char* kFuseKeys[] = {i18n::key("dev-menu.fuse.broken"), i18n::key("dev-menu.fuse.off"),
                                                i18n::key("dev-menu.fuse.on")};
    const int32_t values[] = {FuseBoxValueType::Broken(), FuseBoxValueType::Off(), FuseBoxValueType::On()};
    for (int i = 0; i < 3; ++i)
      if (values[i] == n) return i18n::text(kFuseKeys[i]);
  }
  if (o.kind == OptionKind::Float) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
    return buf;
  }
  return std::to_string(static_cast<int>(std::lround(v)));
}

void option_step(int index, int direction) {
  void* d = difficulty();
  if (!d || !valid(index) || direction == 0) return;
  const Option& o = options()[index];
  float v = o.get(d);
  const float top = top_of(o);
  switch (o.kind) {
    case OptionKind::Bool: v = v > 0.5f ? 0.0f : 1.0f; break;
    case OptionKind::Level:
    case OptionKind::Fuse:
    case OptionKind::Choice:
      v += static_cast<float>(direction);
      if (v < 0.0f) v = top;  // a choice wraps, as a picker's does
      if (v > top) v = 0.0f;
      break;
    default:
      v += o.step * static_cast<float>(direction);
      if (v < 0.0f) v = 0.0f;
      if (v > top) v = top;
      break;
  }
  o.put(d, v);
}

// ---- Objectives: the game's own ObjectiveManager ----

namespace {

void* objective_manager() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  if (found && game::alive(found)) return found;
  const uint64_t now = now_ms();
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = ObjectiveManager::type.type_object();
  found = type ? Object::FindObjectOfType(type, true) : nullptr;
  return found;
}

// The contract's objectives: the main one, then the side ones, as LevelValues holds them.
std::vector<void*> objectives() {
  std::vector<void*> out;
  void* v = values();
  if (!v) return out;
  if (void* main = LevelValues::mainObjective.get(v)) out.push_back(main);
  const api::List sides{LevelValues::sideObjectives.get(v)};
  for (size_t i = 0; i < sides.size(); ++i)
    if (sides[i]) out.push_back(sides[i]);
  return out;
}

int32_t objective_type(void* objective) {
  void* values = objective ? Objective::values.get(objective) : nullptr;
  return values ? ObjectiveValues::type.get(values) : -1;
}

}  // namespace

int objective_count() { return static_cast<int>(objectives().size()); }

std::string objective_line(int index) {
  const std::vector<void*> all = objectives();
  if (index < 0 || static_cast<size_t>(index) >= all.size()) return {};
  void* o = all[static_cast<size_t>(index)];
  const int32_t state = Objective::state.get(o);
  // The kind's number (ObjectiveValues.type) and the game's own state: 0 open, 1 complete, 2 failed.
  return "type " + std::to_string(objective_type(o)) + ", " +
         (state == 1 ? "complete" : state == 2 ? "failed" : "open") + (index == 0 ? " (main)" : "");
}

bool settle_objective(int index, bool complete) {
  const std::vector<void*> all = objectives();
  void* manager = objective_manager();
  if (index < 0 || static_cast<size_t>(index) >= all.size() || !manager) return false;
  // The game's own local call, the one its RPC body runs; it refuses itself until the objectives are set up.
  if (!ObjectiveManager::HasSetupObjectives(manager)) return false;
  const int32_t type = objective_type(all[static_cast<size_t>(index)]);
  if (type < 0) return false;
  if (complete)
    ObjectiveManager::CompleteObjective(manager, type);
  else
    ObjectiveManager::FailObjective(manager, type);
  return true;
}

// ---- Equipment: the truck's items, one at a time, and supplies that never run out ----

namespace {

void* pooler() {
  static uint64_t nextSearch = 0;
  static api::Held found;
  if (found && game::alive(found)) return found;
  const uint64_t now = now_ms();
  if (now < nextSearch) return nullptr;
  nextSearch = now + 1000;
  void* type = ObjectPooler::type.type_object();
  found = type ? Object::FindObjectOfType(type, true) : nullptr;
  return found;
}

// The contract's loadout, as the truck's spawner reads it (LevelValues.equipmentLoadout).
std::vector<void*> loadout_items() {
  std::vector<void*> out;
  void* v = values();
  const api::List list{v ? LevelValues::equipmentLoadout.get(v) : nullptr};
  for (size_t i = 0; i < list.size(); ++i) {
    void* entry = list[i];
    void* item = entry ? GameEquipmentLoadout::equipment.get(entry) : nullptr;
    if (item) out.push_back(item);
  }
  return out;
}

// What each candle held when the switch first saw it, so it can be put back after the game burns it down.
std::vector<std::pair<api::Held, float>> g_candles;
uint64_t g_nextSupplyScan = 0;

}  // namespace

int item_count() { return static_cast<int>(loadout_items().size()); }

std::string item_name(int index) {
  const std::vector<void*> items = loadout_items();
  if (index < 0 || static_cast<size_t>(index) >= items.size()) return {};
  return api::to_utf8(StoreItem::itemName.get(items[static_cast<size_t>(index)]));
}

namespace {
// Whether `p` lies in the truck: inside the box of the trigger the game counts players in (ExitLevel.trigger).
bool in_truck(Vector3 p) {
  void* lift = exit_level();
  void* inside = lift ? ExitLevel::trigger.get(lift) : nullptr;
  void* box = inside ? Component::GetComponent(inside, Collider::type.type_object()) : nullptr;
  if (!box) return false;
  const Bounds b = Collider::get_bounds(box);
  return std::fabs(p.x - b.center.x) <= b.extents.x && std::fabs(p.y - b.center.y) <= b.extents.y &&
         std::fabs(p.z - b.center.z) <= b.extents.z;
}

// A copy of the item free to hand out. First one the pool has never handed out: the pool's Start switches every copy
// off and its hand-out switches the one it gives on (ObjectPooler.SpawnFromEquipmentPool's note), so a copy still off
// is unused. Then one lying in the truck that no hand or ghost is using (PhotonObjectInteract.inUse): the truck's own
// spawner hands out every copy the loadout brings, so those are the copies a player sees as left (the user's test of
// 2026-11-13, where every row said none were left). Null when neither exists; the counts go to the log either way.
void* free_copy(void* pool, void* item) {
  void* inTruck = nullptr;
  size_t on = 0, truck = 0;
  const api::List tiers{ObjectPooler::tierPools.get(pool)};
  for (size_t t = 0; t < tiers.size(); ++t) {
    const api::RefArray pools{tiers[t] ? TierEquipmentPool::equipmentPools.get(tiers[t]) : nullptr};
    for (size_t p = 0; p < pools.size(); ++p) {
      if (!pools[p] || EquipmentPool::item.get(pools[p]) != item) continue;
      const api::RefArray copies{EquipmentPool::objects.get(pools[p])};
      for (size_t c = 0; c < copies.size(); ++c) {
        void* copy = copies[c];
        if (!copy || !api::alive(copy)) continue;
        if (!GameObject::get_activeSelf(copy)) return copy;
        ++on;
        void* t = GameObject::get_transform(copy);
        void* interact = t ? Component::GetComponent(t, PhotonObjectInteract::type.type_object()) : nullptr;
        if (interact && !PhotonObjectInteract::inUse.get(interact) && in_truck(game::position(t))) {
          ++truck;
          if (!inTruck) inTruck = copy;
        }
      }
    }
  }
  PM_LOG("dev-menu: spawn: %zu copies of the item handed out, %zu of them free in the truck", on, truck);
  return inTruck;
}
}  // namespace

SpawnResult spawn_item(int index) {
  const std::vector<void*> items = loadout_items();
  void* pool = pooler();
  void* camera = game::local_camera();
  void* eyes = camera ? game::transform_of(camera) : nullptr;
  if (index < 0 || static_cast<size_t>(index) >= items.size() || !pool || !eyes) return SpawnResult::Failed;
  // The game's hand-out moves whatever copy is at the front of its queue, even one in a hand, once every copy is out;
  // so the menu picks a free copy itself and does to it what the hand-out does.
  void* copy = free_copy(pool, items[static_cast<size_t>(index)]);
  if (!copy) return SpawnResult::NoneLeft;
  // A metre in front of the eyes, where the player sees it fall.
  const Vector3 from = game::position(eyes);
  const Vector3 ahead = game::forward(eyes);
  const Vector3 at{from.x + ahead.x, from.y + ahead.y, from.z + ahead.z};
  const Quaternion upright{0.0f, 0.0f, 0.0f, 1.0f};
  void* t = GameObject::get_transform(copy);
  if (!t) return SpawnResult::Failed;
  Transform::set_position(t, &at);
  Transform::set_rotation(t, &upright);
  GameObject::SetActive(copy, true);
  if (void* interact = Component::GetComponent(t, PhotonObjectInteract::type.type_object())) {
    PhotonObjectInteract::spawnPosition.set(interact, at);
    // A copy resting in the truck may be held still (a kinematic body); it falls where it was put. Only the body's own
    // switch: PhotonObjectInteract.FreezeRigidbody would also send the game's RPC.
    if (void* body = PhotonObjectInteract::body.get(interact)) Rigidbody::set_isKinematic(body, false);
  }
  return SpawnResult::Spawned;
}

void keep_supplies() {
  if (!g_switches.neverRunOut) {
    g_candles.clear();
    return;
  }
  const uint64_t now = now_ms();
  if (now >= g_nextSupplyScan) {
    g_nextSupplyScan = now + 2000;
    // New candles join with what they hold now; the ones already known keep their first value.
    void* type = Candle::type.type_object();
    const api::RefArray found{type ? Object::FindAllOfType(type, false) : nullptr};
    for (size_t i = 0; i < found.size(); ++i) {
      void* c = found[i];
      if (!c) continue;
      bool known = false;
      for (const auto& [held, start] : g_candles) known = known || held == c;
      if (!known) g_candles.emplace_back(api::Held{c}, Candle::candleDuration.get(c));
    }
    void* cross = Crucifix::type.type_object();
    const api::RefArray crosses{cross ? Object::FindAllOfType(cross, false) : nullptr};
    for (size_t i = 0; i < crosses.size(); ++i)
      if (crosses[i]) Crucifix::useCount.set(crosses[i], 0);  // a crucifix as good as new
  }
  std::erase_if(g_candles, [](const auto& c) { return !c.first || !api::alive(c.first); });
  for (const auto& [held, start] : g_candles)
    if (Candle::candleDuration.get(held) < start) Candle::candleDuration.set(held, start);
}

// ---- The hunt's sight assist ----

namespace {
float g_heldSight = -1.0f;  // the value a frozen hold keeps, or negative before it is taken
}

float sight_now() {
  void* ai = game::ghost_ai();
  void* machine = ai ? GhostAI::stateMachine.get(ai) : nullptr;
  void* state = machine ? GhostStateMachine::current.get(machine) : nullptr;
  return state && GhostHuntState::type.is(state) ? GhostHuntState::sightSpeedUp.get(state) : -1.0f;
}

void hold_sight() {
  void* ai = game::ghost_ai();
  void* machine = ai ? GhostAI::stateMachine.get(ai) : nullptr;
  void* state = machine ? GhostStateMachine::current.get(machine) : nullptr;
  if (g_switches.sightHold == kSightGame || !state || !GhostHuntState::type.is(state)) {
    g_heldSight = -1.0f;
    return;
  }
  if (g_switches.sightHold == kSightNone) {
    GhostHuntState::sightSpeedUp.set(state, 1.0f);  // the multiplier that changes nothing: no assist
    return;
  }
  if (g_heldSight < 0.0f) g_heldSight = GhostHuntState::sightSpeedUp.get(state);
  GhostHuntState::sightSpeedUp.set(state, g_heldSight);
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

int flames_blown() {
  void* ai = game::ghost_ai();
  return ai ? GhostAI::flamesBlownOut.get(ai) : 0;
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

// ---- The dials: numbers a slider sets ----

namespace {
struct Dial {
  float low, high, step;
  Need need;
  float (*get)();
  void (*set)(float);
};
const Dial& dial(DialId id) {
  static const Dial kDials[] = {
      // Sanity, as the game shows it.
      {0.0f, 100.0f, 1.0f, Need::Player, [] { return sanity(); },
       [](float v) { set_sanity(static_cast<int>(std::lround(v))); }},
      // The ghost's base hunt speed (GhostAI.baseSpeed): the menu's range, the game puts none on it.
      {0.0f, 5.0f, 0.05f, Need::Ghost, [] { return base_speed(); },
       [](float v) {
         if (void* ai = game::ghost_ai()) GhostAI::baseSpeed.set(ai, v < 0.0f ? 0.0f : v);
       }},
      // The rig's speed multiplier, through the game's own setter (a Monkey Paw wish's path).
      {0.25f, 4.0f, 0.05f, Need::Player, [] { return speed_multiplier(); },
       [](float v) {
         if (void* rig = pc_rig()) LocalPCPlayer::SetPlayerSpeedMultiplier(rig, v);
       }},
      // The Onryo's blown-out flames.
      {0.0f, 5.0f, 1.0f, Need::Ghost, [] { return static_cast<float>(flames_blown()); },
       [](float v) {
         if (void* ai = game::ghost_ai()) GhostAI::flamesBlownOut.set(ai, static_cast<int32_t>(std::lround(v)));
       }},
  };
  return kDials[static_cast<int>(id)];
}
}  // namespace

float dial_low(DialId id) { return dial(id).low; }
float dial_high(DialId id) { return dial(id).high; }
float dial_step(DialId id) { return dial(id).step; }
Need dial_need(DialId id) { return dial(id).need; }
float dial_value(DialId id) { return dial(id).get(); }
void dial_set(DialId id, float value) {
  const Dial& d = dial(id);
  float v = value < d.low ? d.low : (value > d.high ? d.high : value);
  v = d.low + std::round((v - d.low) / d.step) * d.step;  // on the dial's own step
  d.set(v);
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

// ---- The player's clicks while the menu is open ----

namespace {
// The journal whose open flag the menu raised, and the flag as the menu found it.
void* g_flagJournal = nullptr;
bool g_flagWas = false;
}  // namespace

void hold_input(bool held) {
  // The journal's own gate: the item handlers behind the interact and primary-use actions (PCPropGrab's input
  // handlers, read from their decompile 2026-11-13) do nothing while the local player's JournalController.open is
  // set, which is how a click on the journal's pages never reaches the world. The menu raises the same flag while it
  // is open, without showing the book, and puts it back as it found it on close (the user's report and ruling of
  // 2026-11-13). Setting the field sends nothing: the journal's network sync is a separate call.
  if (!held) {
    if (g_flagJournal && api::alive(g_flagJournal)) JournalController::open.set(g_flagJournal, g_flagWas);
    g_flagJournal = nullptr;
    return;
  }
  void* rig = game::local_player_component();
  void* journal = rig ? LocalPlayer::journalController.get(rig) : nullptr;
  if (!journal) return;
  if (journal != g_flagJournal) {
    if (g_flagJournal && api::alive(g_flagJournal)) JournalController::open.set(g_flagJournal, g_flagWas);
    g_flagJournal = journal;
    g_flagWas = JournalController::open.get(journal);
  }
  JournalController::open.set(journal, true);
}

void overlay_closed() { g_flagWas = false; }

// ---- The menu's settings, kept between runs ----

namespace {
std::filesystem::path g_settingsFile;
}  // namespace

void set_settings_file(const std::filesystem::path& file) { g_settingsFile = file; }

// The file is menu_rule.h's settings_text, written whole each time; reading it back is read_settings.
void save_settings() {
  if (g_settingsFile.empty()) return;
  std::error_code ec;
  std::filesystem::create_directories(g_settingsFile.parent_path(), ec);
  std::ofstream file(g_settingsFile, std::ios::binary | std::ios::trunc);
  if (file) file << settings_text(g_switches);
}

void load_settings() {
  if (g_settingsFile.empty()) return;
  std::ifstream file(g_settingsFile, std::ios::binary);
  if (!file) return;
  std::ostringstream text;
  text << file.rdbuf();
  read_settings(text.str(), g_switches);
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

// The rig's body and collider while noclip is on, and whether the body was kinematic before: with the physical step
// off, the investigator's own capsule was still in the physics world, so walls still pushed it back out (the user's
// test of 2026-11-13: flying worked, passing through did not). The collider goes off and the body stops being
// simulated for as long as noclip is on, and both come back as they were.
api::Held g_noclipBody;
api::Held g_noclipCapsule;
bool g_noclipWasKinematic = false;

bool set_noclip(bool on) {
  if (!on) {
    if (g_noclipCapsule && api::alive(g_noclipCapsule)) Collider::set_enabled(g_noclipCapsule, true);
    if (g_noclipBody && api::alive(g_noclipBody)) Rigidbody::set_isKinematic(g_noclipBody, g_noclipWasKinematic);
    if (g_noclipController && api::alive(g_noclipController)) Behaviour::set_enabled(g_noclipController, true);
    g_noclipController = nullptr;
    g_noclipBody = nullptr;
    g_noclipCapsule = nullptr;
    g_switches.noclip = false;
    return true;
  }
  void* controller = rig_controller();
  if (!controller) return false;
  g_noclipController = controller;
  Behaviour::set_enabled(controller, false);
  if (void* body = PhysicsCharacterController::get_Rigidbody(controller)) {
    g_noclipBody = body;
    g_noclipWasKinematic = Rigidbody::get_isKinematic(body);
    Rigidbody::set_isKinematic(body, true);
  }
  if (void* capsule = PhysicsCharacterController::get_CapsuleCollider(controller)) {
    g_noclipCapsule = capsule;
    Collider::set_enabled(capsule, false);
  }
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
// The game's own move of the local investigator (LocalPlayer.TeleportPlayer, PCPlayer.Teleport's body): the rig's
// root goes to `to`, with the physics controller off for the move. Moving the controller's own transform instead put
// the investigator into the floor (the user's tests of 2026-11-13). The game's move switches the controller back on,
// so noclip switches it off again afterwards. False without a rig or the entry.
bool teleport_rig(Vector3 to) {
  void* rig = game::local_player_component();
  if (!rig || !entry_resolved<LocalPlayer::TeleportPlayer>()) return false;
  const bool flying = g_switches.noclip;
  if (flying) set_noclip(false);
  LocalPlayer::TeleportPlayer(rig, &to);
  if (flying) set_noclip(true);
  return true;
}

// Moves the rig so its root stands over `floor` (a point of the navigation mesh) as high as it stands over the mesh
// now, never below it, and a few centimetres up so it settles onto the floor under gravity rather than starting in it.
bool stand_at(Vector3 floor) {
  void* rig = game::local_player_component();
  void* root = rig ? game::transform_of(rig) : nullptr;
  if (!root) return false;
  const Vector3 at = game::position(root);
  Vector3 under{};
  const float lift = on_mesh(at, 4.0f, under) ? std::max(0.0f, at.y - under.y) : 0.0f;
  return teleport_rig({floor.x, floor.y + lift + 0.05f, floor.z});
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
  // Where the contract itself put the investigator: the first of MultiplayerController.spawnPoints, the truck's start
  // places, reached by the game's own move (LocalPlayer.TeleportPlayer, the rig's root, as the contract made it there).
  // Placing the physics controller by the truck's trigger box put the investigator into the floor twice (the user's
  // tests of 2026-11-13), so the box is only the fallback for a scene without start places.
  {
    void* mc = MultiplayerController::instance.get();
    const api::List spots{mc && api::alive(mc) ? MultiplayerController::spawnPoints.get(mc) : nullptr};
    void* spot = spots.size() > 0 ? spots[0] : nullptr;
    if (spot && api::alive(spot) && teleport_rig(game::position(spot))) return true;
  }
  // Fallback: into the trigger the game counts players in (ExitLevel.trigger), on the floor a ray finds under the
  // box's top, as high above it as the rig stands above the ground now.
  void* lift = exit_level();
  void* inside = lift ? ExitLevel::trigger.get(lift) : nullptr;
  if (!inside) {
    PM_LOG("dev-menu: the truck's inside could not be found (no ExitLevel or no trigger)");
    return false;
  }
  void* box = Component::GetComponent(inside, Collider::type.type_object());
  Vector3 target = game::position(game::transform_of(inside));
  float top = target.y + 2.0f;
  float reach = 4.0f;
  if (box) {
    const Bounds b = Collider::get_bounds(box);
    target = b.center;
    top = b.center.y + b.extents.y;
    reach = 2.0f * b.extents.y + 1.0f;
  }
  // The truck's floor under the box's centre, found by a ray down from the box's top: the trigger's box reaches
  // below the floor, so its own bottom put the rig inside the floor (the user's test of 2026-11-13).
  const float down = game::raycast_distance({target.x, top, target.z}, {0.0f, -1.0f, 0.0f}, reach, -5);
  if (down < 0.0f) {
    PM_LOG("dev-menu: no floor was found inside the truck");
    return false;
  }
  // The rig's root stands at its feet, as the contract's start places put it.
  return teleport_rig({target.x, top - down + 0.05f, target.z});
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
