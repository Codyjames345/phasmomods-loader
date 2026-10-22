#include "actions.h"

#include <windows.h>

#include <intrin.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "ability.h"
#include "custom_link.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "round.h"
#include "tether.h"

#pragma intrinsic(_ReturnAddress)

namespace pg::actions {
namespace {

using il2cpp::Vector3;

constexpr const char* kKindName[kKindCount] = {"interaction", "event", "ability", "hunt", "fuse box", "disguise"};

// GhostAI.State values not in tether.h.
constexpr int kStateDoor = 5;
constexpr int kStateLight = 4;
constexpr int kStateLightFlicker = 12;

// GhostType values used by the native rules below.
constexpr int kPoltergeist = 3;
constexpr int kShade = 8;
constexpr int kTwins = 17;
constexpr int kDeogen = 22;
constexpr int kThaye = 23;
constexpr int kDeildegast = 30;

// Native functions whose own calls to Random.RandomRangeInt are recognised by return address
// (GameAssembly RVA, length) for build 25811160.
struct Range {
  uintptr_t rva, length;
};
constexpr Range kIdleExecute = {0x215B430, 0x525};      // idle roll
constexpr Range kDoorChance = {0x2007C60, 0xF6};        // door category chance + GetDoorToOpen
constexpr Range kLightChance = {0x2014DA0, 0x1FA};      // light category chance + switch choice
constexpr Range kPropChance = {0x2006DF0, 0x20A};       // InteractWithARandomProp

// The idle state's hunt check (build 25811160, obfuscated names). Each idle roll first writes the
// insanity it checks into +0x48: the focused player's (GhostAI+0x118) or the team's average, plus the
// ghost type's and difficulty's modifiers. The hunt branch then needs at least tether::kHuntInsanity.
constexpr uintptr_t kIdleInsanityRva = 0x215FFC0;
constexpr size_t kIdleGhost = 0x18;          // idle state: its GhostAI
constexpr size_t kIdleInsanity = 0x48;       // int
constexpr size_t kGhostFocus = 0x118;        // GhostAI: the player whose sanity counts, if any
constexpr size_t kPlayerSanity = 0x90;       // Player: PlayerSanity
constexpr size_t kSanityInsanity = 0x30;     // PlayerSanity: insanity, 0..100 (sanity is 100 minus it)
constexpr size_t kNetworkSpots = 0x38;       // Network: List<PlayerSpot>
constexpr size_t kSpotPlayer = 0x38;         // PlayerSpot: networked Player
constexpr size_t kPlayerDead = 0x28;
constexpr size_t kSpotPhotonPlayer = 0x18;   // PlayerSpot: Photon.Realtime.Player
constexpr float kDisguiseReach = 30.0f;      // metres: investigators whose form the ghost may take

// GhostAI flags the native rules set and the idle roll reads (build 25811160).
constexpr size_t kGhostInfo = 0x38;              // GhostInfo; +0x2C its effective GhostType
constexpr size_t kGhostCooldown = 0xD8;          // bool: after a hunt (Demon shorter)
constexpr size_t kGhostInteractionBoost = 0xD9;  // bool: RemoveInteractionBlock (the Tower card)
constexpr size_t kGhostHunting = 0xF3;           // bool
constexpr size_t kGhostSmudgeNoHunt = 0xF5;      // bool: smudged outside a hunt (Spirit longer, Demon shorter)
constexpr size_t kGhostIncense = 0xF8;           // WhiteSage: smudged in a hunt (StopHuntingForTime)
constexpr size_t kGhostCanWander = 0x108;        // bool: cleared by TemporarilyStopWander
constexpr size_t kGhostStayHome = 0x10A;         // bool: the roll's actions and wanders go home instead

// What the roll's gates read elsewhere (build 25811160).
constexpr size_t kLevelGhostRoom = 0x38;      // LevelController: the room the ghost is in
constexpr size_t kLevelFuseBox = 0x80;        // LevelController: FuseBox
constexpr size_t kFusePowerOn = 0x90;         // FuseBox: bool
constexpr size_t kFuseOutOfOrder = 0xE8;      // FuseBox: bool, the ghost's fuse state does nothing
constexpr size_t kValuesNoEvents = 0x23;      // LevelValues: bool, no events or hunts
constexpr size_t kSetupMainDoorOpen = 0x44;   // SetupPhaseController: bool, set by UnlockMainDoor

constexpr float kMaxCharges = 2.0f;       // up to two of each kind of action are kept
constexpr float kInteractReach = 4.0f;    // ray length; the native sensor range still decides
constexpr int kTwinRoll = 10;             // TryInteract: Random(0, 10) (lea edx,[r8+0Ah])
constexpr int kTwinFrom = 5;              // 5 and up, the twin interacts (5-7 alone, 8-9 with the ghost)

il2cpp::Held g_ghost;     // the ghost under player control now, null while nobody steers it
il2cpp::Held g_owner;     // the ghost the saved actions below belong to: kept across a short input gap
float g_interact = 0.0f;  // may go below zero: an expensive interaction is paid off by later rolls
int g_events = 0, g_abilities = 0, g_fuses = 0;
ULONGLONG g_lastAttempt = 0;  // the ghost player's last try at an action, or when they took control
bool g_huntAvailable = false;
int g_huntModifier = 0;  // the hunt check's insanity over the players' own, from the last idle roll
bool g_haveModifier = false;

// Idle roll scope.
bool g_inIdleRoll = false;
int g_idleCoin = -1;        // native 50/50 that decides whether the hunt branch runs
bool g_huntDecided = false;
bool g_huntRolled = false;  // the hunt branch passed every native check and its own chance

// Aimed interaction scope.
bool g_inAim = false;
il2cpp::Held g_aimTransform;
int g_aimState = -1;

bool g_replaying = false;  // inside a native entry we called ourselves

using IdleExecute_t = void (*)(void*, const void*);
using ChangeState_t = void (*)(void*, int, void*, bool, const void*);
using TryInteract_t = void (*)(void*, const void*);
using NormalInteraction_t = void (*)(void*, bool, const void*);
using TwinInteraction_t = void* (*)(void*, bool, const void*);
using RandomRangeInt_t = int (*)(int, int, const void*);
using DetectedComponents_t = void* (*)(void*, void*, const void*);
using IdleInsanity_t = void (*)(void*, const void*);
IdleExecute_t g_idleExecute = nullptr;
ChangeState_t g_changeState = nullptr;
TryInteract_t g_tryInteract = nullptr;
RandomRangeInt_t g_randomRangeInt = nullptr;
DetectedComponents_t g_detectedComponents = nullptr;
IdleInsanity_t g_idleInsanity = nullptr;

float insanity_of(void* player) {
  void* sanity = game::alive(player) ? il2cpp::field<void*>(player, kPlayerSanity) : nullptr;
  return sanity ? il2cpp::field<float>(sanity, kSanityInsanity) : -1.0f;
}

// The living investigators' average insanity, as GameController.GetAveragePlayerInsanity counts it;
// negative when nobody is alive.
float average_insanity() {
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  float sum = 0;
  int count = 0;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    if (!game::alive(player) || il2cpp::field<bool>(player, kPlayerDead)) continue;
    const float v = insanity_of(player);
    if (v < 0) continue;
    sum += v;
    ++count;
  }
  return count ? sum / count : -1.0f;
}

// After the idle roll has worked out the insanity its hunt check uses: how far the type and
// difficulty move it from the players' own, so the hunt threshold can be shown in sanity.
void detour_idle_insanity(void* idle, const void* method) {
  hooks::Guard guard;
  g_idleInsanity(idle, method);
  void* ghost = il2cpp::field<void*>(idle, kIdleGhost);
  if (!g_ghost || ghost != g_ghost) return;
  void* focus = il2cpp::field<void*>(ghost, kGhostFocus);
  const float own = game::alive(focus) ? insanity_of(focus) : average_insanity();
  if (own < 0) return;
  const int modifier = il2cpp::field<int>(idle, kIdleInsanity) - static_cast<int>(own);
  if (!g_haveModifier || modifier != g_huntModifier) PG_LOG("actions: hunts possible at %d%% sanity or below", tether::kHuntInsanity + modifier);
  g_huntModifier = modifier;
  g_haveModifier = true;
}

int ghost_type() {
  void* info = il2cpp::field<void*>(g_ghost, kGhostInfo);
  return info ? il2cpp::field<int>(info, 0x2C) : -1;  // GhostInfo effective GhostType
}

int identity_type() {
  void* info = il2cpp::field<void*>(g_ghost, kGhostInfo);
  return info ? il2cpp::field<int>(info, 0x28) : -1;  // GhostInfo GhostType: what the ghost is
}

// Native per-type odds of each interaction category, from the interaction chain.
float interaction_cost(int state) {
  const int type = ghost_type();
  if (state == kStateDoor) return 25.0f / (type == kDeogen || type == kThaye ? 30.0f : type == kDeildegast ? 10.0f : 25.0f);
  if (state == kStateLight || state == kStateLightFlicker) return 25.0f / (type == kDeildegast ? 10.0f : 25.0f);
  return 100.0f / (type == kPoltergeist ? 20.0f : type == kDeildegast ? 85.0f : 100.0f);
}

bool restless() { return tether::restless(GetTickCount64(), g_lastAttempt); }

// The roll's actions are the ghost player's unless they have been idle long enough, or the Tower
// card's boost has them happen on their own.
bool intercepting() {
  return g_ghost && g_inIdleRoll && !g_replaying && !restless() && !il2cpp::field<bool>(g_ghost, kGhostInteractionBoost);
}

void restart_idle() { g_changeState(g_ghost, tether::kIdle, nullptr, false, nullptr); }

void grant(int kind) {
  switch (kind) {
    case kInteract: g_interact = std::min(kMaxCharges, g_interact + 1.0f); break;
    case kEvent: g_events = std::min(static_cast<int>(kMaxCharges), g_events + 1); break;
    case kAbility: g_abilities = std::min(static_cast<int>(kMaxCharges), g_abilities + 1); break;
    case kFuse: g_fuses = std::min(static_cast<int>(kMaxCharges), g_fuses + 1); break;
    default: return;
  }
  PG_LOG("actions: native roll granted a %s", kKindName[kind]);
}

void withdraw_hunt(const char* why) {
  if (!g_huntAvailable) return;
  g_huntAvailable = false;
  PG_LOG("actions: hunt no longer available (%s)", why);
}

void detour_idle_execute(void* self, const void* method) {
  hooks::Guard guard;
  if (!g_ghost || g_replaying) {
    g_idleExecute(self, method);
    return;
  }
  g_inIdleRoll = true;
  g_idleCoin = -1;
  g_huntDecided = g_huntRolled = false;
  g_idleExecute(self, method);
  // The roll reached its hunt check again without hunting: the earlier offer has passed.
  if (g_idleCoin == 1 && !g_huntRolled) withdraw_hunt("the next hunt check did not pass");
  g_inIdleRoll = false;
}

int detour_random_range_int(int min, int max, const void* method) {
  hooks::Guard guard;
  void* caller = _ReturnAddress();
  const int r = g_randomRangeInt(min, max, method);
  if (g_inIdleRoll && game::in_function(caller, kIdleExecute.rva, kIdleExecute.length)) {
    if (g_idleCoin < 0 && min == 0 && max == 2) {
      g_idleCoin = r;
    } else if (g_idleCoin == 1 && !g_huntDecided) {
      // The hunt branch reaches its final 1-in-3/4/5 chance only after every native check
      // (type-modified sanity, setup, cooldowns, Thaye) has passed; otherwise the next draw is the
      // activity roll's.
      g_huntDecided = true;
      g_huntRolled = min == 0 && max >= 3 && max <= 5 && r == 1;
    }
  }
  if (g_inAim && min == 0 && max == 100 &&
      (game::in_function(caller, kDoorChance.rva, kDoorChance.length) ||
       game::in_function(caller, kLightChance.rva, kLightChance.length) ||
       game::in_function(caller, kPropChance.rva, kPropChance.length))) {
    return 0;  // the category chance becomes the interaction's cost instead
  }
  return r;
}

void detour_change_state(void* ghost, int state, void* interact, bool flag, const void* method) {
  hooks::Guard guard;
  if (ghost == g_ghost && intercepting()) {
    if (state == tether::kHunting && g_huntRolled) {
      if (!g_huntAvailable) PG_LOG("actions: hunt available");
      g_huntAvailable = true;
      restart_idle();
      return;
    }
    // GhostActivity.GhostAbility's choice. A type without an ability of its own does nothing in
    // the ability state, as in vanilla.
    const int kind = state == tether::kFuseBox ? kFuse
                     : state == tether::kRandomEvent ? kEvent
                     : state == tether::kAbility && custom_link::own_ability(ghost_type(), identity_type()) ? kAbility
                     : -1;
    if (kind >= 0) {
      grant(kind);
      restart_idle();
      return;
    }
  }
  if (g_inAim && ghost == g_ghost) {
    if (state == tether::kFavouriteRoom) return;  // nothing qualified: the ghost simply stays
    if (state == tether::kThrow && !interact) return;  // the aimed object cannot be thrown: nothing happens
    if (tether::aimed_state_counts(state, interact != nullptr)) g_aimState = state;
  }
  g_changeState(ghost, state, interact, flag, method);
}

// The roll's interaction. TryInteract sends the ghost home until the front door has been unlocked;
// that part stays native.
void detour_try_interact(void* activity, const void* method) {
  hooks::Guard guard;
  if (intercepting() && activity == game::ghost_activity(g_ghost)) {
    void* setup = game::setup_phase();
    if (setup && il2cpp::field<bool>(setup, kSetupMainDoorOpen)) {
      grant(kInteract);
      restart_idle();
      return;
    }
  }
  g_tryInteract(activity, method);
}

// While an aimed interaction runs, the ghost's sensor reports only the aimed object.
void* detour_detected_components(void* sensor, void* list, const void* method) {
  hooks::Guard guard;
  void* result = g_detectedComponents(sensor, list, method);
  if (!g_inAim || !result || !g_aimTransform) return result;
  auto* items = il2cpp::field<il2cpp::Array*>(result, 0x10);  // List<T>._items
  const int size = il2cpp::field<int>(result, 0x18);          // List<T>._size
  int kept = 0;
  for (int i = 0; items && i < size; ++i) {
    void* c = items->data<void*>()[i];
    if (c && game::alive(c) && game::is_child_of(g_aimTransform, game::transform_of(c))) {
      if (kept != i) il2cpp::set_ref_field(items, 0x20 + kept * sizeof(void*), c);
      ++kept;
    }
  }
  for (int i = kept; items && i < size; ++i) il2cpp::set_ref_field(items, 0x20 + i * sizeof(void*), nullptr);
  il2cpp::set_field<int>(result, 0x18, kept);
  il2cpp::set_field<int>(result, 0x1C, il2cpp::field<int>(result, 0x1C) + 1);  // _version
  return result;
}

// The vanilla gates the roll applied when it chose an action, read now.
tether::ActionGate action_gate() {
  tether::ActionGate g;
  void* setup = game::setup_phase();
  g.mainDoorOpen = setup && il2cpp::field<bool>(setup, kSetupMainDoorOpen);
  void* values = game::level_values();
  g.noEvents = values && il2cpp::field<bool>(values, kValuesNoEvents);
  g.stayHome = il2cpp::field<bool>(g_ghost, kGhostStayHome);
  void* lc = game::level_controller();
  g.shadeWatched = ghost_type() == kShade && lc && game::any_players_in_room(il2cpp::field<void*>(lc, kLevelGhostRoom));
  g.interactionBoost = il2cpp::field<bool>(g_ghost, kGhostInteractionBoost);
  return g;
}

void* fuse_box() {
  void* lc = game::level_controller();
  void* box = lc ? il2cpp::field<void*>(lc, kLevelFuseBox) : nullptr;
  return game::alive(box) ? box : nullptr;
}

bool power_on() {
  void* box = fuse_box();
  return box && il2cpp::field<bool>(box, kFusePowerOn);
}

tether::FuseAction fuse_action(const tether::ActionGate& g) {
  void* box = fuse_box();
  return tether::fuse_action(box != nullptr, box && il2cpp::field<bool>(box, kFuseOutOfOrder), g.noEvents, power_on(), ghost_type());
}

bool ability_ready(const tether::ActionGate& g) {
  const int type = ghost_type();
  return custom_link::own_ability(type, identity_type()) && tether::may_use_ability(g) &&
         (!ability::needs_power(type) || power_on());
}

// TryInteract's Twins split, of Random(0, 10): 0-4 the ghost alone, 5-7 its twin alone, 8-9 both. The
// twin pulses the sensor around its own box a second or two later (GhostActivity.TwinInteraction).
// The aimed interaction is the ghost's own choice, so the twin follows it at the twin's vanilla rate
// of one roll in two.
void twin_follow_up(void* activity) {
  if (ghost_type() != kTwins || g_randomRangeInt(0, kTwinRoll, nullptr) < kTwinFrom) return;
  void* routine = reinterpret_cast<TwinInteraction_t>(game::twin_interaction())(activity, false, nullptr);
  game::start_coroutine(activity, routine);
  PG_LOG("actions: the twin interacts as well");
}

int g_disguise = 0;  // the investigator whose form was just taken

/** The living investigator nearest the ghost's line of sight (then nearest the ghost), as an actor. */
int disguise_target(const float aim[3]) {
  const Vector3 g = game::position(game::transform_of(g_ghost));
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  int best = 0;
  float bestScore = -1e9f;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    void* player = spot ? il2cpp::field<void*>(spot, kSpotPlayer) : nullptr;
    if (!player || !game::alive(player) || il2cpp::field<bool>(player, kPlayerDead)) continue;
    const Vector3 p = game::position(game::transform_of(player));
    const float dx = p.x - g.x, dz = p.z - g.z, d = std::sqrt(dx * dx + dz * dz);
    if (d > kDisguiseReach) continue;
    const float facing = d > 0.01f ? (dx * aim[0] + dz * aim[2]) / d : 1.0f;  // -1..1
    const float score = facing * 100.0f - d;
    const int actor = game::actor_number(il2cpp::field<void*>(spot, kSpotPhotonPlayer));
    if (actor && score > bestScore) best = actor, bestScore = score;
  }
  return best;
}

bool aimed_interaction(const float aim[3]) {
  void* activity = game::ghost_activity(g_ghost);
  if (!activity) return false;
  Vector3 p = game::position(game::transform_of(g_ghost));
  Vector3 eye{p.x, p.y + game::kGhostEyeHeight, p.z};
  void* hit = game::raycast(eye, {aim[0], aim[1], aim[2]}, kInteractReach);
  if (!hit) return false;
  g_inAim = true;
  g_aimTransform = hit;
  g_aimState = -1;
  g_replaying = true;
  reinterpret_cast<NormalInteraction_t>(game::normal_interaction())(activity, false, nullptr);  // native sensor pulse and chain
  g_replaying = false;
  g_inAim = false;
  g_aimTransform = nullptr;
  if (g_aimState < 0) return false;
  const float cost = interaction_cost(g_aimState);
  g_interact -= cost;
  PG_LOG("actions: interaction state %d, cost %.2f, charge %.2f", g_aimState, cost, g_interact);
  twin_follow_up(activity);
  return true;
}

// The insanity the hunt check measures: the ghost's focused player's if it has one, else the team's.
float measured_insanity(bool* focused = nullptr) {
  void* focus = il2cpp::field<void*>(g_ghost, kGhostFocus);
  const bool f = game::alive(focus) && insanity_of(focus) >= 0;
  if (focused) *focused = f;
  return f ? insanity_of(focus) : average_insanity();
}

tether::HuntGate hunt_gate() {
  tether::HuntGate g;
  g.hunting = il2cpp::field<bool>(g_ghost, kGhostHunting);
  g.cooldown = il2cpp::field<bool>(g_ghost, kGhostCooldown);
  g.smudged = il2cpp::field<bool>(g_ghost, kGhostSmudgeNoHunt);
  g.insanity = measured_insanity();
  g.haveModifier = g_haveModifier;
  g.modifier = g_huntModifier;
  g.thaye = ghost_type() == kThaye;
  return g;
}

void run_native(int state) {
  g_replaying = true;
  g_changeState(g_ghost, state, nullptr, false, nullptr);
  g_replaying = false;
}

}  // namespace

bool create_hooks() {
  return hooks::create(game::idle_execute(), reinterpret_cast<void*>(&detour_idle_execute),
                       reinterpret_cast<void**>(&g_idleExecute), "GhostIdle.Execute") &&
         hooks::create(game::change_state(), reinterpret_cast<void*>(&detour_change_state),
                       reinterpret_cast<void**>(&g_changeState), "GhostAI.ChangeState") &&
         hooks::create(game::try_interact(), reinterpret_cast<void*>(&detour_try_interact),
                       reinterpret_cast<void**>(&g_tryInteract), "GhostActivity.TryInteract") &&
         hooks::create(game::random_range_int(), reinterpret_cast<void*>(&detour_random_range_int),
                       reinterpret_cast<void**>(&g_randomRangeInt), "Random.RandomRangeInt") &&
         hooks::create(game::detected_components(), reinterpret_cast<void*>(&detour_detected_components),
                       reinterpret_cast<void**>(&g_detectedComponents), "Sensor.GetDetectedComponents") &&
         hooks::create(game::game_assembly_address(kIdleInsanityRva), reinterpret_cast<void*>(&detour_idle_insanity),
                       reinterpret_cast<void**>(&g_idleInsanity), "GhostIdle.HuntInsanity");
}

void tick(void* ghost) {
  if (ghost && ghost != g_owner) {  // a new ghost: nothing saved up yet
    clear();
    g_owner = ghost;
    g_lastAttempt = GetTickCount64();
  }
  if (ghost != g_ghost) PG_LOG(ghost ? "actions: ghost under player control" : "actions: ghost released to the native AI");
  g_ghost = ghost;
  if (!g_ghost) return;
  // An offered hunt is taken back when a native check fails since its roll: a smudge, a hunt (with
  // its cooldown) from another source such as the Death card, or sanity restored.
  if (g_huntAvailable && !tether::hunt_allowed(hunt_gate())) withdraw_hunt("native checks");
}

tether::Reason movement() {
  if (!g_ghost) return tether::kSteer;
  tether::Ghost t;
  t.state = game::ghost_state(g_ghost);
  t.inFavouriteRoom = round::ghost_in_favourite_room();
  t.smudged = game::alive(il2cpp::field<void*>(g_ghost, kGhostIncense));
  return tether::reason(t);
}

bool perform(int kind, const float aim[3]) {
  if (!g_ghost || kind < 0 || kind >= kKindCount) return false;
  g_lastAttempt = GetTickCount64();  // trying is enough to take the roll back from restlessness
  // Never in the middle of another action, a hunt or a native walk (src/tether.h). Read live, so a
  // second trigger in the same frame does not cancel what the first one started.
  if (!tether::may_act_from(game::ghost_state(g_ghost))) return false;
  const tether::ActionGate gate = action_gate();
  bool done = false;
  switch (kind) {
    case kHunt:
      if (!g_huntAvailable || !tether::hunt_allowed(hunt_gate()) || custom_link::held_back(identity_type(), 2)) break;
      g_huntAvailable = false;
      run_native(tether::kHunting);
      done = true;
      break;
    case kInteract:
      if (g_interact >= 1.0f && tether::may_interact(gate) && !custom_link::held_back(identity_type(), 0))
        done = aimed_interaction(aim);
      break;
    case kEvent:
      if (g_events < 1 || !tether::may_cause_event(gate) || custom_link::held_back(identity_type(), 1)) break;
      --g_events;
      run_native(tether::kRandomEvent);  // its own type rules (Shade, Thaye, Banshee) decide which
      done = true;
      break;
    case kAbility:
      if (g_abilities < 1 || !ability_ready(gate)) break;
      if (custom_link::is_custom_ability(ghost_type(), identity_type())) {
        done = custom_link::use_custom_ability(identity_type());  // a custom ghost's own (Custom Ghosts)
      } else {
        run_native(tether::kAbility);
        done = true;
      }
      if (done) --g_abilities;
      break;
    case kDisguise: {
      if (g_events < 1 || !tether::may_cause_event(gate) || custom_link::held_back(identity_type(), 1)) break;
      const int target = disguise_target(aim);
      if (!target) break;
      --g_events;
      g_disguise = target;
      done = true;
      break;
    }
    case kFuse:
      if (g_fuses < 1 || fuse_action(gate) == tether::kFuseNone || custom_link::held_back(identity_type(), 0)) break;
      --g_fuses;
      run_native(tether::kFuseBox);  // turns it off, or tries to turn it on (1 in 5), as the type may
      done = true;
      break;
    default: break;
  }
  PG_LOG("actions: %s %s", kKindName[kind], done ? "performed" : "not available");
  return done;
}

int take_disguise() { return std::exchange(g_disguise, 0); }

session::Status status() {
  session::Status s;
  s.interact = session::encode_charge(g_interact, kMaxCharges);
  s.events = static_cast<uint8_t>(g_events);
  s.abilities = static_cast<uint8_t>(g_abilities);
  s.fuses = static_cast<uint8_t>(g_fuses);
  if (!g_ghost) return s;
  const ULONGLONG idle = GetTickCount64() - g_lastAttempt;
  s.restless = static_cast<uint8_t>(std::min<ULONGLONG>(255, idle * 255 / tether::kRestlessMs));
  if (restless()) s.more |= session::kRestlessActing;
  if (g_huntAvailable) s.flags |= session::kHuntAvailable;
  if (game::ghost_state(g_ghost) == tether::kHunting) s.flags |= session::kHunting;
  const tether::Reason r = movement();
  if (tether::native_moves(r)) s.flags |= session::kNativeMove;
  if (r == tether::kDrawnHome) s.flags |= session::kDrawnHome;
  if (r == tether::kBlinded) s.flags |= session::kBlinded;
  if (r == tether::kResting) s.more |= session::kResting;
  if (!il2cpp::field<bool>(g_ghost, kGhostCanWander)) s.flags |= session::kBound;
  const tether::ActionGate gate = action_gate();
  if (ability_ready(gate)) s.flags |= session::kAbilityAvailable;
  if (tether::may_interact(gate) && !custom_link::held_back(identity_type(), 0)) s.more |= session::kInteractAllowed;
  if (tether::may_cause_event(gate) && !custom_link::held_back(identity_type(), 1)) s.more |= session::kEventAllowed;
  const tether::FuseAction fuse = fuse_action(gate);
  if (fuse == tether::kFuseTurnOff) s.more |= session::kFuseTurnOff;
  if (fuse == tether::kFuseTurnOn) s.more |= session::kFuseTurnOn;
  // The same players the hunt check measures: the ghost's focused player if it has one.
  bool focused = false;
  if (const float insanity = measured_insanity(&focused); insanity >= 0)
    s.sanity = static_cast<uint8_t>(std::clamp(100.0f - insanity, 0.0f, 100.0f) + 0.5f);
  if (focused) s.flags |= session::kFocusSanity;
  if (g_haveModifier) s.huntBelow = static_cast<uint8_t>(std::clamp(tether::kHuntInsanity + g_huntModifier, 0, 100));
  if (const int type = ghost_type(); type >= 0 && type < 0xFF) s.type = static_cast<uint8_t>(type);
  if (const int type = identity_type(); type >= 0 && type < 0xFF) s.identity = static_cast<uint8_t>(type);
  return s;
}

void clear() {
  g_ghost = nullptr;
  g_owner = nullptr;
  g_interact = 0.0f;
  g_events = g_abilities = g_fuses = 0;
  g_lastAttempt = 0;
  g_huntAvailable = false;
  g_huntModifier = 0;
  g_haveModifier = false;
}

}  // namespace pg::actions
