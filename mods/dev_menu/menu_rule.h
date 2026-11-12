#pragma once

// The rules behind the dev menu, kept free of game calls so they can be tested offline
// (docs/superpowers/specs/2026-11-10-dev-menu-content-plan.md): the sandbox gate, the pause while the menu
// is open, which actions a row may run right now and what the mod's three hooks refuse. Everything a row
// *shows* is read from the game every frame (actions.cpp); only these rules decide.
#include <cmath>

namespace pm::devmenu {

// ---- The sandbox gate (the user's final call, 2026-11-10, third round; probes fixed 2026-11-11) ----
// The menu opens only in the single player sandbox: either PUN runs offline, the game's own single player
// mode (its connect code calls set_OfflineMode(true) and then joins a room even in that mode, so an
// in-room state comes with offline mode in a solo contract - re/out/offline.c), or this client is in no
// room at all: the main menu and lobby browsing (the game is connected to Photon there, so offline mode
// is false and the old offline-only gate never opened anywhere). An online room, private or public, never
// opens it. An offline or room state the game cannot answer is not the sandbox: the menu stays shut,
// like every loader gate.
inline bool may_open(bool offlineMode, bool inRoom) { return offlineMode || !inRoom; }

// ---- Pause while the menu is open ----
struct Pause {
  bool frozen = false;  // the menu has the game frozen right now
  float saved = 1.0f;   // the timeScale to go back to when it closes
};

/** The timeScale when the menu opens: 0 when pausing is on, saving what ran. A second open (a re-entry
 *  while already frozen) keeps the first saved value. */
inline float pause_open(Pause& p, float current, bool pauseWhenOpen) {
  if (!pauseWhenOpen) return current;
  if (!p.frozen) p.saved = current;
  p.frozen = true;
  return 0.0f;
}

/** The timeScale when the menu closes: what ran before it opened; false, changing nothing, when the menu
 *  never froze the game. */
inline bool pause_close(Pause& p, float& restore) {
  if (!p.frozen) return false;
  p.frozen = false;
  restore = p.saved;
  return true;
}

/** The investigator's speed multiplier the speed row steps to: `steps` quarter-steps from `current`, snapped
 *  to the nearest quarter, kept between 0.25 and 4. The bounds are the menu's, not the game's: the game puts
 *  no range on the multiplier (Difficulty.playerSpeed sets it, and a Monkey Paw wish calls the same setter),
 *  and a multiplier of 0 would leave the row no way back up. */
inline float next_speed_multiplier(float current, int steps) {
  const float moved = current + 0.25f * static_cast<float>(steps);
  float snapped = std::lround(moved * 4.0f) / 4.0f;
  if (snapped < 0.25f) snapped = 0.25f;
  if (snapped > 4.0f) snapped = 4.0f;
  return snapped;
}

/** The game's timeScale the "tick speed" row cycles to: the next preset above `current`, wrapping to the
 *  first (0 paused, 0.25, 0.5, 1 real time, 2, 4). A value between presets moves to the next one above it. */
inline float next_time_scale(float current) {
  static constexpr float kScales[] = {0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f};
  for (const float s : kScales)
    if (s > current + 0.001f) return s;
  return kScales[0];
}

// ---- The switches the mod's hooks read ----
struct Switches {
  // Freeze the game at timeScale 0 while the menu is open. Off by default since 2026-11-12 (the user's call
  // after three live tries): the game's own code never writes the time scale at all, and its picture adapts
  // to the light using the frame's scaled time, so a frozen clock behind the menu is the one thing every
  // broken screen after a close shared. The row and the tick-speed row stay for a developer who wants them.
  bool pauseWhenOpen = false;
  bool godMode = false;  // a hunt's kill of the local investigator is refused
  // The debug read-out stays on screen while the menu is closed: the ghost's state, its type, its speed and
  // who its hunt is set on (the user's original brief). Off until a developer asks for it.
  bool debugHud = false;
  // Noclip: the investigator's physical step is switched off and the walk the game asks for moves them
  // along the camera instead, through walls and floors (actions.cpp, PhysicsCharacterController).
  bool noclip = false;
  // Hold the local investigator's sanity at lockedSanity: the menu sets it back through the game's own
  // setter whenever the game has moved it.
  bool lockSanity = false;
  int lockedSanity = 100;
  // ESP: labels drawn over the game's picture at the ghost and at each of the level's rooms.
  bool espGhost = false;
  bool espRooms = false;
  // The store: every item and tier unlocked, and every purchase allowed whatever the money. Both act only inside
  // the single player sandbox, and the dev session's save freeze keeps all of it off the save.
  bool unlockAll = false;
  bool freeShop = false;
  // More ESP: the level's doors (open, shut, locked), hiding places, light switches and fuse box.
  bool espDoors = false;
  bool espHiding = false;
  bool espSwitches = false;
};

/** The save freeze (the user's ask, 2026-11-12: unlock and free shop without touching the save): once a dev session
 *  has started, EasySave3's commit of a written save is skipped for the rest of the run, so nothing the run saves,
 *  bought, earned or changed, ever replaces the save on disk. It stays on until the game closes, because the run's
 *  in-memory state (a free purchase, say) would otherwise reach the save with the next ordinary write. */
inline bool freeze_save(bool devSessionOn) { return devSessionOn; }

/** Whether a store switch answers for the game now: only while it is on and the sandbox gate holds, so an online
 *  lobby later in the same run sees the game's own answers. */
inline bool store_switch_applies(bool switchOn, bool sandbox) { return switchOn && sandbox; }

/** God mode: only the local investigator's kill is refused; a hunt of anybody else (a dummy, another
 *  player) runs. The kill's network body is never touched, and offline the kill decision is this client's. */
inline bool god_refuses_kill(bool godMode, bool targetIsLocal) { return godMode && targetIsLocal; }

/** Progression suppression (the user's rule, 2026-11-10): while a dev session is on, every progress write the
 *  map knows is skipped: the contract's mission and challenge checks, the reward screen's results (its saved
 *  money, its challenge progress and the seasonal event's score), its experience pay, and the decrement of a
 *  saved consumable stock. The mod does not start when those hooks cannot be made, so a running mod with the
 *  session on always has them. Stats, quests and the apocalypse counter wait on the RE pass the plan names. */
inline bool suppress_progress(bool devSessionOn) { return devSessionOn; }

/** The reward screen's loss of the brought equipment (a death with the difficulty's lose-items set) becomes
 *  the game's own return of it while a dev session is on, so a dev session never costs a saved item. */
inline bool return_instead_of_losing(bool devSessionOn) { return devSessionOn; }

/** The sanity the lock holds next: the locked value cycles 100, 50, 0 and back to 100. */
inline int next_locked_sanity(int current) {
  if (current > 50) return 50;
  if (current > 0) return 0;
  return 100;
}

/** Whether the lock must put the value back now: the game moved PlayerSanity.insanity more than half a point
 *  from the locked value. A smaller drift waits, so the game's setter (which sends the value on) is not asked
 *  every frame. The lock holds the field the sanity read-out shows, and sets it through
 *  PlayerSanity.SetInsanity exactly as the set rows do, so the lock and those rows always agree. */
inline bool sanity_needs_reset(float valueNow, int locked) {
  return std::fabs(valueNow - static_cast<float>(locked)) > 0.5f;
}

// ---- What the rows may run right now ----
// A row's click asks this first; the game's own guards still decide, and a game refusal shows its reason.
enum class Action {
  UnlockDoor, EndContract, Leave, Quit,          // Contract
  PickWeather, ApplyWeather,                     // Contract: the weather
  ForceHunt, EndHunt, ClearCooldown, Appear, UnAppear, ApplyRoom, RandomRoom, PickRoom, BumpSpeed,  // Ghost
  ToggleEvents, FireEvent, SetGhostBlind,         // Ghost
  FireState,                                      // Ghost: one named attempt (interact, ability, throw)
  GhostToMe,                                      // Ghost: the NavMeshAgent warp to the investigator
  PickRetype, Retype,                             // Ghost: the type, changed now
  BumpFlames, TypeChangeNow, ChaseMe,             // Ghost: the type's own dials, and the hunt's target
  MeToTruck,                                      // Player: back to the truck
  SetFriendly, SetLoseEquipment, BumpEvidence, BumpType,  // Difficulty
  PickOption, StepOption,                         // Difficulty: every custom option, by a picker
  SetSanity, ToggleFreeze, KillLocal, ReviveLocal,  // Player
  BumpPlayerSpeed, ResetPlayerSpeed,              // Player: the game's own speed multiplier
  SetNoclip, MeToGhost, MeToGhostRoom,            // Player: movement
  SetLockSanity, CycleLockedSanity,               // Player: the sanity lock
  SetGodMode, SetPauseWhenOpen, CycleTimeScale, SetDebugHud, SetEspGhost, SetEspRooms,  // switches
  SetUnlockAll, SetFreeShop,                     // Equipment: the store
  SpawnDummy, DummyAction,                       // Player: the dummy investigator
  SetEspDoors, SetEspHiding, SetEspSwitches,     // Game: more ESP
  Info,                                          // a read-out: no click
};

// The order is the order of the reasons' text keys in menu.cpp (kWhyKeys): add at the end only.
enum class Why {
  None, NotOffline, NoContract, NoGhost, NoDifficulty, NoPlayer, Hunting, NotHunting, NoRooms, Dead, NoTruck,
  Friendly, NoRig, Alive, Dying, NoDummy,
};

// What the game looks like to the menu right now, read every tick (actions.cpp fills it).
struct Can {
  bool sandbox = false;      // the may_open gate holds: single player, or in no room
  bool contract = false;     // a LevelController, so a contract runs
  bool ghost = false;        // the contract has a ghost
  bool difficulty = false;   // LevelValues and its difficulty object
  bool player = false;       // this client has an investigator
  bool hunting = false;      // the ghost hunts now
  bool dead = false;         // the local investigator is dead
  bool dying = false;        // a kill of the local investigator has started and not finished (Player.dying)
  bool friendly = false;     // the contract's difficulty has a friendly ghost (Difficulty.friendlyGhost)
  bool dummy = false;        // the dummy investigator is in the contract (dummy.h)
  int rooms = 0;             // the level's rooms
};

/** Why running `action` now is refused, or Why::None. The sandbox gate is assumed (the menu shut otherwise)
 *  and kept as a defensive check. Why::NoTruck and Why::NoRig are not these rules' answers: refuse() never
 *  returns them; they are what an action reports when the game has no truck, or no PC rig, to ask.
 *
 *  Offline is not a shield. The game's RPC bodies let any sender through offline, but their checks of the
 *  game's state still run, and a refusal raises the game's own report on the sender: the live log of
 *  2026-11-12 shows the Revive RPC refused offline and reported, and the game faulting after it
 *  (sdk/systems/photon.md). So every row that reaches a checked RPC refuses here first, in every state the
 *  game's body would refuse: a kill on the menu's scene, of a dead investigator or on a friendly difficulty,
 *  a friendly switch while a hunt (and its kill) is under way, and a forced hunt on a friendly difficulty.
 *  The Revive body goes on only in a contract carrying the tarot cards or the Monkey Paw; the revive row's
 *  action lends the contract the paw for the length of the revive (the author's call, 2026-11-12; actions.cpp),
 *  so here it needs only a contract and an investigator who is dead or dying. */
inline Why refuse(Action action, const Can& c) {
  if (!c.sandbox) return Why::NotOffline;
  switch (action) {
    case Action::Info:
      return Why::None;
    case Action::PickWeather:  // only the menu's own cursor
      return Why::None;
    case Action::ApplyWeather:
      if (!c.contract) return Why::NoContract;
      return c.difficulty ? Why::None : Why::NoDifficulty;
    case Action::Leave:
    case Action::Quit:
    case Action::UnlockDoor:
    case Action::EndContract:
      return c.contract ? Why::None : Why::NoContract;
    case Action::ForceHunt:
      // The hunt's RPC body refuses on a friendly difficulty (photon.md): refuse before the game is asked.
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.friendly ? Why::Friendly : Why::None;
    case Action::Appear:
      // A hunt runs its own appearances; one asked mid-hunt is not a path the game takes.
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.hunting ? Why::Hunting : Why::None;
    case Action::ClearCooldown:
    case Action::UnAppear:
    case Action::FireState:  // ChangeState unforced: the game's own guards decide the attempt
    case Action::BumpSpeed:
    case Action::ToggleEvents:
    case Action::FireEvent:
    case Action::SetGhostBlind:  // asks the ghost's own vision sensor
      if (!c.contract) return Why::NoContract;
      return c.ghost ? Why::None : Why::NoGhost;
    case Action::EndHunt:
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.hunting ? Why::None : Why::NotHunting;
    case Action::ApplyRoom:
    case Action::RandomRoom:
    case Action::PickRoom:
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      if (c.hunting) return Why::Hunting;  // the game refuses a favourite room mid-hunt, and so does the row
      return c.rooms > 0 ? Why::None : Why::NoRooms;
    case Action::SetFriendly:
      // A hunt that started unfriendly would meet a friendly check in its own kill and door RPCs: never switch
      // it while one runs (photon.md, "Do not change a difficulty's friendlyGhost behind the game's back").
      if (!c.difficulty) return Why::NoDifficulty;
      return c.hunting ? Why::Hunting : Why::None;
    case Action::BumpFlames:     // the Onryo's count of blown-out flames (GhostAI.flamesBlownOut)
    case Action::TypeChangeNow:  // the game's own moment for the Mimic, Thaye and Obambo (typeChangeDue)
      if (!c.contract) return Why::NoContract;
      return c.ghost ? Why::None : Why::NoGhost;
    case Action::ChaseMe:  // GhostHuntState.SetTarget, the hunt's own call
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      if (!c.player) return Why::NoPlayer;
      return c.hunting ? Why::None : Why::NotHunting;
    case Action::MeToTruck:
      if (!c.contract) return Why::NoContract;
      return c.player ? Why::None : Why::NoPlayer;
    case Action::PickRetype:  // only the menu's own cursor
      return Why::None;
    case Action::Retype:
      // Init mid-hunt would restart the ghost under its own hunt; change it between hunts.
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.hunting ? Why::Hunting : Why::None;
    case Action::GhostToMe:
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.player ? Why::None : Why::NoPlayer;
    case Action::PickOption:  // only the menu's own cursor
      return Why::None;
    case Action::StepOption:
      // The friendly switch among them keeps its own rule: never mid-hunt.
      if (!c.difficulty) return Why::NoDifficulty;
      return Why::None;
    case Action::SetLoseEquipment:
    case Action::BumpEvidence:
    case Action::BumpType:
      return c.difficulty ? Why::None : Why::NoDifficulty;
    case Action::KillLocal:
      // The death animation's RPC refuses, and reports, a kill on the menu's scene, of an investigator marked
      // dead or on a friendly difficulty (sdk/systems/players.md). A hunt is fine; a kill already under way (the
      // hunt's own, or the last click's) is not, since its victim is about to be marked dead.
      if (!c.contract) return Why::NoContract;
      if (!c.player) return Why::NoPlayer;
      if (c.dead) return Why::Dead;
      if (c.dying) return Why::Dying;
      return c.friendly ? Why::Friendly : Why::None;
    case Action::ReviveLocal:
      // Player.RevivePlayer waits for the dead mark; the Revive RPC accepts a player dead or dying.
      if (!c.contract) return Why::NoContract;
      if (!c.player) return Why::NoPlayer;
      return (c.dead || c.dying) ? Why::None : Why::Alive;
    case Action::MeToGhost:
    case Action::MeToGhostRoom:
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      if (!c.player) return Why::NoPlayer;
      if (action == Action::MeToGhostRoom && c.rooms <= 0) return Why::NoRooms;
      return Why::None;
    case Action::SetSanity:
    case Action::ToggleFreeze:
    case Action::BumpPlayerSpeed:  // the game's own multiplier, on this client's rig
    case Action::ResetPlayerSpeed:
    case Action::SetNoclip:
    case Action::SetLockSanity:
      return c.player ? Why::None : Why::NoPlayer;
    case Action::CycleLockedSanity:  // only the menu's own number: the lock applies it when on
    case Action::SetGodMode:
    case Action::SetPauseWhenOpen:
    case Action::CycleTimeScale:
    case Action::SetDebugHud:  // the mod's own text on its own canvas: it needs nothing from a contract
    case Action::SetEspGhost:
    case Action::SetEspRooms:
    case Action::SetEspDoors:
    case Action::SetEspHiding:
    case Action::SetEspSwitches:
    case Action::SetUnlockAll:  // the store's own checks; the hooks also ask the sandbox gate each time
    case Action::SetFreeShop:
      return Why::None;
    case Action::SpawnDummy:
      if (!c.contract) return Why::NoContract;
      return c.player ? Why::None : Why::NoPlayer;
    case Action::DummyAction:  // move, Banshee target, revive, remove: all need the dummy
      if (!c.contract) return Why::NoContract;
      return c.dummy ? Why::None : Why::NoDummy;
  }
  return Why::None;
}

}  // namespace pm::devmenu
