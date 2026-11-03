#include "actions.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <utility>

#include "Difficulty.h"
#include "GhostAI.h"
#include "GhostIdleState.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "Player.h"
#include "PlayerSanity.h"
#include "Sensor.h"
#include "ability.h"
#include "aim.h"
#include "chances.h"
#include "content.h"
#include "content_link.h"
#include "game.h"
#include "held.h"
#include "Transform.h"
#include "UnityWorld.h"
#include "log.h"
#include "roll.h"
#include "round.h"
#include "runtime.h"
#include "target.h"
#include "tether.h"

namespace pm::actions {
namespace {

using phasmo::Vector3;

constexpr const char* kKindName[kKindCount] = {"interaction", "event", "ability", "hunt", "fuse box", "disguise"};

using phasmo::GhostAI;

constexpr float kDisguiseReach = 30.0f;  // metres: investigators whose form the ghost may take

constexpr float kMaxCharges = 2.0f;       // up to two of each kind of action are kept
constexpr float kInteractReach = 4.0f;    // ray length; the native sensor range still decides
constexpr int kTwinRoll = 10;             // TryInteract: Random(0, 10) (lea edx,[r8+0Ah])
constexpr int kTwinFrom = 5;              // 5 and up, the twin interacts (5-7 alone, 8-9 with the ghost)

api::Held g_ghost;     // the ghost under player control now, null while nobody steers it
api::Held g_owner;     // the ghost the saved actions below belong to: kept across a short input gap
float g_interact = 0.0f;  // may go below zero: an expensive interaction is paid off by later rolls
int g_events = 0, g_abilities = 0, g_fuses = 0;
ULONGLONG g_lastAttempt = 0;  // the ghost player's last try at an action, or when they took control
bool g_huntAvailable = false;
int g_huntModifier = 0;  // the hunt check's insanity over the players' own, from the last idle roll
bool g_haveModifier = false;

// Idle roll scope (GhostIdleState.Execute): which of its draws are which (roll.h).
bool g_inIdleRoll = false;
int g_inChangeState = 0;  // how many GhostAI.ChangeState calls are running, under this mod's hook
roll::Idle g_idle;

// Aimed interaction scope.
bool g_inAim = false;
api::Held g_aimTransform;
int g_aimState = -1;
bool g_aimLost = false;  // the ghost's own rules lost what the aim chose

bool g_replaying = false;  // inside a native entry we called ourselves

float insanity_of(void* player) {
  void* sanity = player && api::alive(player) ? phasmo::Player::sanity.get(player) : nullptr;
  return sanity ? phasmo::PlayerSanity::insanity.get(sanity) : -1.0f;
}

// The living investigators' average insanity, as GameController.GetAveragePlayerInsanity counts it;
// negative when nobody is alive.
float average_insanity() {
  void* net = game::network();
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  float sum = 0;
  int count = 0;
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    void* player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (!player || !api::alive(player) || phasmo::Player::isDead.get(player)) continue;
    const float v = insanity_of(player);
    if (v < 0) continue;
    sum += v;
    ++count;
  }
  return count ? sum / count : -1.0f;
}

bool g_insanityWorked = false;  // the idle roll running now has worked out the insanity its hunt check uses

void detour_idle_insanity(api::Original<phasmo::GhostIdleState::HuntInsanity> original, void* idle) {
  original(idle);
  g_insanityWorked = true;
}

// How far the type and difficulty move the insanity the roll just worked out from the players' own, so the hunt
// threshold can be shown in sanity. Read once the whole of Execute has run, the only caller of HuntInsanity, so
// another mod's hook on it has added its part (the Draugr's angered bonus) whichever mod's hook runs first.
void note_hunt_modifier(void* idle) {
  if (!std::exchange(g_insanityWorked, false)) return;
  void* ghost = phasmo::GhostIdleState::ghost.get(idle);
  if (!g_ghost || ghost != g_ghost) return;
  void* focus = GhostAI::bansheeTarget.get(ghost);
  const float own = focus && api::alive(focus) ? insanity_of(focus) : average_insanity();
  if (own < 0) return;
  const int modifier = phasmo::GhostIdleState::insanity.get(idle) - static_cast<int>(own);
  if (!g_haveModifier || modifier != g_huntModifier) PM_LOG("actions: hunts possible at %d%% sanity or below", tether::kHuntInsanity + modifier);
  g_huntModifier = modifier;
  g_haveModifier = true;
}

int ghost_type() {
  void* info = GhostAI::info.get(g_ghost);
  return info ? phasmo::GhostInfo::behaviourType.get(info) : -1;  // the type whose rules its AI follows
}

int identity_type() {
  void* info = GhostAI::info.get(g_ghost);
  return info ? phasmo::GhostInfo::identityType.get(info) : -1;  // what the ghost is
}

// The id of the type whose rules the ghost's AI follows ("vanilla.shade"), empty for none.
std::string ghost_type_id() { return api::content::id_of(ghost_type()); }

// The game's numbers for the states the vanilla rules ask about (tether.h).
tether::States states() {
  namespace state = phasmo::GhostState;
  return {state::idle(),          state::wander(),   state::dots(),        state::hunting(),      state::favouriteRoom(),
          state::throwing(),      state::fusebox(),  state::randomEvent(), state::GhostAbility()};
}

// What the aimed interaction that set `state` costs of the charge, by the type's own chances (roll.h).
float interaction_cost(int state) {
  const roll::Aimed kind = state == phasmo::GhostState::door() ? roll::Aimed::Door
                           : state == phasmo::GhostState::light() || state == phasmo::GhostState::flicker()
                               ? roll::Aimed::Light
                               : roll::Aimed::Prop;
  return roll::interaction_cost(ghost_type_id(), kind);
}

bool restless() { return tether::restless(GetTickCount64(), g_lastAttempt); }

// The roll's actions are the ghost player's unless they have been idle long enough, or the Tower
// card's boost has them happen on their own.
bool intercepting() {
  return g_ghost && g_inIdleRoll && !g_replaying && !restless() && !GhostAI::interactionBoost.get(g_ghost);
}

void restart_idle() { api::below_hook<GhostAI::ChangeState>()(g_ghost, phasmo::GhostState::idle(), nullptr, false); }

void grant(int kind) {
  switch (kind) {
    case kInteract: g_interact = std::min(kMaxCharges, g_interact + 1.0f); break;
    case kEvent: g_events = std::min(static_cast<int>(kMaxCharges), g_events + 1); break;
    case kAbility: g_abilities = std::min(static_cast<int>(kMaxCharges), g_abilities + 1); break;
    case kFuse: g_fuses = std::min(static_cast<int>(kMaxCharges), g_fuses + 1); break;
    default: return;
  }
  PM_LOG("actions: native roll granted a %s", kKindName[kind]);
}

void withdraw_hunt(const char* why) {
  if (!g_huntAvailable) return;
  g_huntAvailable = false;
  PM_LOG("actions: hunt no longer available (%s)", why);
}

void detour_idle_execute(api::Original<phasmo::GhostIdleState::Execute> original, void* self) {
  if (!g_ghost || g_replaying) {
    original(self);
    return;
  }
  g_inIdleRoll = true;
  g_idle.begin();
  g_insanityWorked = false;
  original(self);
  note_hunt_modifier(self);
  // The roll reached its hunt check again without hunting: the earlier offer has passed.
  if (g_idle.passed_without_hunting()) withdraw_hunt("the next hunt check did not pass");
  g_inIdleRoll = false;
}

int detour_random_range_int(api::Original<phasmo::Random::RandomRangeInt> original, int min, int max) {
  const int r = original(min, max);
  if (g_inIdleRoll) g_idle.draw(min, max, r, g_inChangeState > 0);
  return chances::draw(min, max, r, g_inAim);
}

void detour_change_state(api::Original<GhostAI::ChangeState> original, void* ghost, int state, void* interact, bool flag) {
  roll::Running running{g_inChangeState};  // what the state change draws is not the idle roll's own
  if (ghost == g_ghost && intercepting()) {
    if (state == phasmo::GhostState::hunting() && g_idle.huntRolled) {
      if (!g_huntAvailable) PM_LOG("actions: hunt available");
      g_huntAvailable = true;
      restart_idle();
      return;
    }
    // GhostActivity.GhostAbility's choice. A type without an ability of its own does nothing in
    // the ability state, as in vanilla.
    const int kind = state == phasmo::GhostState::fusebox() ? kFuse
                     : state == phasmo::GhostState::randomEvent() ? kEvent
                     : state == phasmo::GhostState::GhostAbility() && content_link::own_ability(ghost_type()) ? kAbility
                     : -1;
    if (kind >= 0) {
      grant(kind);
      restart_idle();
      return;
    }
  }
  if (g_inAim && ghost == g_ghost) {
    if (state == phasmo::GhostState::favouriteRoom()) return;  // nothing qualified: the ghost simply stays
    if (state == phasmo::GhostState::throwing() && !interact) return;  // the aimed object cannot be thrown: nothing happens
    if (tether::aimed_state_counts(state, interact != nullptr, states())) {
      g_aimState = state;
      // The ghost's own rules may lose what the aim chose, as they lose a share of its AI's rolls: it does not
      // begin, and it is paid for as if it had (aimed_interaction).
      g_aimLost = api::content::interaction_lost();
      if (g_aimLost) return;
    }
  }
  original(ghost, state, interact, flag);
}

// The roll's interaction. TryInteract sends the ghost home until the front door has been unlocked;
// that part stays native.
void detour_try_interact(api::Original<phasmo::GhostActivity::TryInteract> original, void* activity) {
  if (intercepting() && activity == game::ghost_activity(g_ghost)) {
    void* setup = game::setup_phase();
    if (setup && phasmo::SetupPhaseController::mainDoorOpen.get(setup)) {
      grant(kInteract);
      restart_idle();
      return;
    }
  }
  original(activity);
}

// While an aimed interaction runs, the ghost's sensor reports only the aimed object, and nothing for an aim whose
// ray hit nothing.
void* detour_detected_components(api::Original<phasmo::Sensor::GetDetectedComponents> original, void* sensor, void* list) {
  void* result = original(sensor, list);
  if (!g_inAim || !result) return result;
  aim::keep_only(api::List{result}, [](void* c) {
    return g_aimTransform && c && game::alive(c) && game::is_child_of(g_aimTransform, game::transform_of(c));
  });
  return result;
}

// The vanilla gates the roll applied when it chose an action, read now.
tether::ActionGate action_gate() {
  tether::ActionGate g;
  void* setup = game::setup_phase();
  g.mainDoorOpen = setup && phasmo::SetupPhaseController::mainDoorOpen.get(setup);
  void* values = game::level_values();
  g.noEvents = values && phasmo::LevelValues::eventsBlocked.get(values);
  g.stayHome = GhostAI::stayHome.get(g_ghost);
  void* lc = game::level_controller();
  g.shadeWatched =
      ghost_type_id() == "vanilla.shade" && lc && game::any_players_in_room(phasmo::LevelController::currentGhostRoom.get(lc));
  g.interactionBoost = GhostAI::interactionBoost.get(g_ghost);
  return g;
}

void* fuse_box() {
  void* lc = game::level_controller();
  void* box = lc ? phasmo::LevelController::fuseBox.get(lc) : nullptr;
  return game::alive(box) ? box : nullptr;
}

bool power_on() {
  void* box = fuse_box();
  return box && phasmo::FuseBox::powerOn.get(box);
}

tether::FuseAction fuse_action(const tether::ActionGate& g) {
  void* box = fuse_box();
  return tether::fuse_action(box != nullptr, box && phasmo::FuseBox::outOfOrder.get(box), g.noEvents, power_on(), ghost_type_id());
}

// The ghost's own ability, allowed by the vanilla gates and by its own rules: a held-back event holds its
// ability back too (content_link::held_back kind 1, "an event or its ability"), and a rule may silence the
// ability alone.
bool ability_ready(const tether::ActionGate& g) {
  const int type = ghost_type();
  return content_link::own_ability(type) && tether::may_use_ability(g) && !content_link::held_back(1) &&
         !content_link::held_back(content_link::kAbilityAlone) &&
         (!ability::needs_power(api::content::id_of(type)) || power_on());
}

// TryInteract's Twins split, of Random(0, 10): 0-4 the ghost alone, 5-7 its twin alone, 8-9 both. The
// twin pulses the sensor around its own box a second or two later (GhostActivity.TwinInteraction).
// The aimed interaction is the ghost's own choice, so the twin follows it at the twin's vanilla rate
// of one roll in two.
void twin_follow_up(void* activity) {
  if (ghost_type_id() != "vanilla.thetwins" || api::below_hook<phasmo::Random::RandomRangeInt>()(0, kTwinRoll) < kTwinFrom) return;
  void* routine = phasmo::GhostActivity::TwinInteraction(activity, false);
  game::start_coroutine(activity, routine);
  PM_LOG("actions: the twin interacts as well");
}

int g_disguise = 0;  // the investigator whose form was just taken

/** The living investigator nearest the ghost's line of sight (then nearest the ghost), as an actor. */
int disguise_target(const float aim[3]) {
  const Vector3 g = game::position(game::transform_of(g_ghost));
  void* net = game::network();
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  int best = 0;
  float bestScore = -1e9f;
  for (size_t i = 0; i < spots.size(); ++i) {
    void* spot = spots[i];
    void* player = spot ? phasmo::PlayerSpot::player.get(spot) : nullptr;
    if (!player || !game::alive(player) || phasmo::Player::isDead.get(player)) continue;
    const Vector3 p = game::position(game::transform_of(player));
    const float dx = p.x - g.x, dz = p.z - g.z, d = std::sqrt(dx * dx + dz * dz);
    if (d > kDisguiseReach) continue;
    const float facing = d > 0.01f ? (dx * aim[0] + dz * aim[2]) / d : 1.0f;  // -1..1
    const float score = facing * 100.0f - d;
    const int actor = game::actor_number(phasmo::PlayerSpot::photonPlayer.get(spot));
    if (actor && score > bestScore) best = actor, bestScore = score;
  }
  return best;
}

bool aimed_interaction(const float aim[3]) {
  void* activity = game::ghost_activity(g_ghost);
  if (!activity) return false;
  Vector3 p = game::position(game::transform_of(g_ghost));
  Vector3 eye{p.x, p.y + game::kGhostEyeHeight, p.z};
  // A ray that hits nothing still pulses, with nothing detected, as a hit on bare wall does: a rule on the pulse
  // that needs only the ghost's sensor (the Echo's repeat) runs as for its AI.
  void* hit = game::raycast(eye, {aim[0], aim[1], aim[2]}, kInteractReach);
  g_inAim = true;
  g_aimTransform = hit;
  g_aimState = -1;
  g_aimLost = false;
  g_replaying = true;
  phasmo::GhostActivity::NormalInteraction(activity, false);  // native sensor pulse and chain
  g_replaying = false;
  g_inAim = false;
  g_aimTransform = nullptr;
  if (g_aimState < 0) return false;
  const float cost = interaction_cost(g_aimState);
  g_interact -= cost;
  PM_LOG("actions: interaction state %d, cost %.2f, charge %.2f%s", g_aimState, cost, g_interact,
         g_aimLost ? ", lost to the ghost's own rules" : "");
  if (!g_aimLost) twin_follow_up(activity);
  return true;
}

// Whether the ghost's AI follows the Banshee's rules, the one type the game itself gives a Banshee target.
bool banshee_type() { return ghost_type_id() == "vanilla.banshee"; }

// Whether the ghost hunts now.
bool hunting_now() { return game::ghost_state(g_ghost) == phasmo::GhostState::hunting(); }

// The insanity the hunt check measures: the ghost's focused player's if it has one, else the team's. A
// Banshee target the host holds a hunt to (target.h) is no focus of the hunt check's.
float measured_insanity(bool* focused = nullptr) {
  void* focus = target::hunt_focus(banshee_type(), hunting_now(), GhostAI::bansheeTarget.get(g_ghost));
  const bool f = insanity_of(focus) >= 0;
  if (focused) *focused = f;
  return f ? insanity_of(focus) : average_insanity();
}

tether::HuntGate hunt_gate() {
  tether::HuntGate g;
  g.hunting = GhostAI::isHunting.get(g_ghost);
  g.cooldown = GhostAI::postHuntCooldown.get(g_ghost);
  g.smudged = GhostAI::huntsHeldOff.get(g_ghost);
  g.insanity = measured_insanity();
  g.haveModifier = g_haveModifier;
  g.modifier = g_huntModifier;
  g.thaye = ghost_type_id() == "vanilla.thaye";
  return g;
}

// The ghost player's chosen state, through the game's own GhostAI.ChangeState entry rather than this mod's
// hook's original, so every mod's hook on it runs as for the AI's own state changes, whichever mod hooked it
// first (a Bogeyman's hunt steps out of its closet there); this mod's own hook passes it straight on.
void run_native(int state) {
  g_replaying = true;
  GhostAI::ChangeState(g_ghost, state, nullptr, false);
  g_replaying = false;
}

}  // namespace

bool create_hooks() {
  constexpr api::Twins kOne = api::Twins::CanonicalOnly;
  return api::hook<phasmo::GhostIdleState::Execute>(&detour_idle_execute, "GhostIdle.Execute", kOne) &&
         api::hook<GhostAI::ChangeState>(&detour_change_state, "GhostAI.ChangeState", kOne) &&
         api::hook<phasmo::GhostActivity::TryInteract>(&detour_try_interact, "GhostActivity.TryInteract", kOne) &&
         api::hook<phasmo::Random::RandomRangeInt>(&detour_random_range_int, "Random.RandomRangeInt", kOne) &&
         api::hook<phasmo::Sensor::GetDetectedComponents>(&detour_detected_components, "Sensor.GetDetectedComponents", kOne) &&
         api::hook<phasmo::GhostIdleState::HuntInsanity>(&detour_idle_insanity, "GhostIdleState.HuntInsanity") &&
         chances::create_hooks();
}

void tick(void* ghost) {
  if (ghost && ghost != g_owner) {  // a new ghost: nothing saved up yet
    clear();
    g_owner = ghost;
    g_lastAttempt = GetTickCount64();
  }
  if (ghost != g_ghost) PM_LOG(ghost ? "actions: ghost under player control" : "actions: ghost released to the native AI");
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
  t.smudged = game::alive(GhostAI::smudge.get(g_ghost));
  return tether::reason(t, states());
}

bool perform(int kind, const float aim[3]) {
  if (!g_ghost || kind < 0 || kind >= kKindCount) return false;
  g_lastAttempt = GetTickCount64();  // trying is enough to take the roll back from restlessness
  // Never in the middle of another action, a hunt or a native walk (src/tether.h). Read live, so a
  // second trigger in the same frame does not cancel what the first one started.
  if (!tether::may_act_from(game::ghost_state(g_ghost), states())) return false;
  const tether::ActionGate gate = action_gate();
  bool done = false;
  switch (kind) {
    case kHunt:
      if (!g_huntAvailable || !tether::hunt_allowed(hunt_gate()) || content_link::held_back(2)) break;
      g_huntAvailable = false;
      run_native(phasmo::GhostState::hunting());
      done = true;
      break;
    case kInteract:
      if (g_interact >= 1.0f && tether::may_interact(gate) && !content_link::held_back(0))
        done = aimed_interaction(aim);
      break;
    case kEvent:
      if (g_events < 1 || !tether::may_cause_event(gate) || content_link::held_back(1)) break;
      --g_events;
      run_native(phasmo::GhostState::randomEvent());  // its own type rules (Shade, Thaye, Banshee) decide which
      done = true;
      break;
    case kAbility:
      if (g_abilities < 1 || !ability_ready(gate)) break;
      if (content_link::definition_ability(ghost_type())) {
        done = content_link::use_definition_ability();  // a registered ghost's own, by its definition
      } else {
        run_native(phasmo::GhostState::GhostAbility());
        done = true;
      }
      if (done) --g_abilities;
      break;
    case kDisguise: {
      if (g_events < 1 || !tether::may_cause_event(gate) || content_link::held_back(1)) break;
      const int target = disguise_target(aim);
      if (!target) break;
      --g_events;
      g_disguise = target;
      done = true;
      break;
    }
    case kFuse:
      // Its AI's fuse box is no interaction: a rule holds it back only as the fuse box (kind 4).
      if (g_fuses < 1 || fuse_action(gate) == tether::kFuseNone || content_link::held_back(content_link::kFuseBox)) break;
      --g_fuses;
      run_native(phasmo::GhostState::fusebox());  // turns it off, or tries to turn it on (1 in 5), as the type may
      done = true;
      break;
    default: break;
  }
  PM_LOG("actions: %s %s", kKindName[kind], done ? "performed" : "not available");
  return done;
}

int take_disguise() { return std::exchange(g_disguise, 0); }

bool roll_taken() { return intercepting(); }

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
  if (game::ghost_state(g_ghost) == phasmo::GhostState::hunting()) s.flags |= session::kHunting;
  const tether::Reason r = movement();
  if (tether::native_moves(r)) s.flags |= session::kNativeMove;
  if (r == tether::kDrawnHome) s.flags |= session::kDrawnHome;
  if (r == tether::kBlinded) s.flags |= session::kBlinded;
  if (r == tether::kResting) s.more |= session::kResting;
  if (!GhostAI::canWander.get(g_ghost)) s.flags |= session::kBound;
  const tether::ActionGate gate = action_gate();
  if (ability_ready(gate)) s.flags |= session::kAbilityAvailable;
  if (tether::may_interact(gate) && !content_link::held_back(0)) s.more |= session::kInteractAllowed;
  if (tether::may_cause_event(gate) && !content_link::held_back(1)) s.more |= session::kEventAllowed;
  const tether::FuseAction fuse = content_link::held_back(content_link::kFuseBox) ? tether::kFuseNone : fuse_action(gate);
  if (fuse == tether::kFuseTurnOff) s.more |= session::kFuseTurnOff;
  if (fuse == tether::kFuseTurnOn) s.more |= session::kFuseTurnOn;
  // The same players the hunt check measures (the ghost's focused player if it has one), and the one
  // player a ghost with a target hunts, shown as a Banshee's target is (in a hunt, the one the host fixed
  // at its start).
  bool focused = false;
  const float measured = measured_insanity(&focused);
  void* shown = target::shown(banshee_type(), hunting_now(), GhostAI::bansheeTarget.get(g_ghost), api::content::target());
  session::set_sanity(s, measured, focused, insanity_of(shown));
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

}  // namespace pm::actions
