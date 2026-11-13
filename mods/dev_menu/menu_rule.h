#pragma once

// The rules behind the dev menu, kept free of game calls so they can be tested offline
// (docs/superpowers/specs/2026-11-10-dev-menu-content-plan.md): the sandbox gate, the pause while the menu
// is open, which actions a row may run right now and what the mod's three hooks refuse. Everything a row
// *shows* is read from the game every frame (actions.cpp); only these rules decide.
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>

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

/** The time scale the menu calls paused: the smallest it ever writes, never 0. The game itself never writes the time
 *  scale, and its movement divides by the frame's scaled time: PhysicsCharacterController.Move stores the frame's walk
 *  divided by Time.deltaTime, so at a scale of 0 every frame of a pause stored 0/0, and the first physical step after
 *  it moved the local rig to NaN (the live test of 2026-11-12: every rig collider NaN, the picture flashing, then
 *  black, the game stuck). At this scale ten minutes of pause pass as a twentieth of a second of game time. */
inline constexpr float kPausedScale = 0.0001f;

struct Pause {
  bool frozen = false;  // the menu has the game frozen right now
  float saved = 1.0f;   // the timeScale to go back to when it closes
};

/** The timeScale when the menu opens: kPausedScale when pausing is on, saving what ran. A second open (a re-entry
 *  while already frozen) keeps the first saved value. */
inline float pause_open(Pause& p, float current, bool pauseWhenOpen) {
  if (!pauseWhenOpen) return current;
  if (!p.frozen) p.saved = current;
  p.frozen = true;
  return kPausedScale;
}

/** The timeScale when the menu closes: what ran before it opened; false, changing nothing, when the menu
 *  never froze the game. */
inline bool pause_close(Pause& p, float& restore) {
  if (!p.frozen) return false;
  p.frozen = false;
  restore = p.saved;
  return true;
}

// ---- The switches the mod's hooks read ----
struct Switches {
  // Freeze the game (kPausedScale) while the menu is open. Off by default since 2026-11-12 (the user's call
  // after three live tries): the game's own code never writes the time scale at all, and its picture adapts
  // to the light using the frame's scaled time, so a frozen clock behind the menu is the one thing every
  // broken screen after a close shared. The row and the tick-speed row stay for a developer who wants them.
  bool pauseWhenOpen = false;
  bool godMode = false;  // a kill of the local investigator is refused, in a hunt or outside one
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
  // Candles burn and crucifixes are used up as the game says, but the menu puts each back where it started.
  bool neverRunOut = false;
  // The hunt's sight assist (GhostHuntState.sightSpeedUp): the game's own, held at none (1, the multiplier that
  // changes nothing), or held where it stood when the hold began.
  int sightHold = 0;  // SightHold below
  // What the hunt's sight test answers (GhostSight below): the game's own, nobody, or every living player wherever
  // they are, as the Deogen's own branch of GhostHuntState.Sees answers for that type.
  int ghostSight = 0;
};

/** The idle check's base line: a hunt is possible while the insanity the check uses is at least this (GhostIdleState's
 *  map note; Playable Ghost's tether.h kHuntInsanity). In sanity, the base line is 100 less it, which is also 50. */
inline constexpr int kHuntInsanity = 50;

/** The ghost sight row's three settings, in the order the row cycles them. */
enum GhostSight { kGhostSightNormal = 0, kGhostSightNobody = 1, kGhostSightEveryone = 2 };
inline int next_ghost_sight(int current) { return (current + 1) % 3; }

/** What the hunt's sight test answers under the setting, given the game's own answer and whether the player is
 *  dead: nobody is seen while the ghost sees nobody; every living player is seen while it sees everyone (the Deogen's
 *  rule; a dead player, as one in the hunt's killed list, is never seen); otherwise the game's own answer. */
inline bool sight_answer(int setting, bool gameSees, bool playerDead) {
  if (setting == kGhostSightNobody) return false;
  if (setting == kGhostSightEveryone) return !playerDead;
  return gameSees;
}

/** The sight assist row's three settings, in the order the row cycles them. */
enum SightHold { kSightGame = 0, kSightNone = 1, kSightFrozen = 2 };
inline int next_sight_hold(int current) { return (current + 1) % 3; }

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

/** God mode also refuses a death of the local investigator started outside a hunt's kill, at the game's death call
 *  (Player.BeginDeathSequence): a custom ghost's catch (the Squatter's kick), the Monkey Paw's wish and the Hanged Man
 *  call it directly, never through GhostHuntState.Kill. The menu's own Kill you row (`menuKill`) is never refused.
 *  Skipping the call sends nothing, so no RPC body is asked. */
inline bool god_refuses_death(bool godMode, bool targetIsLocal, bool menuKill) {
  return god_refuses_kill(godMode, targetIsLocal) && !menuKill;
}

/** Progression suppression (the user's rule, 2026-11-10): while a dev session is on, every progress write the
 *  map knows is skipped: the contract's mission and challenge checks, the reward screen's results (its saved
 *  money, its challenge progress and the seasonal event's score), its experience pay, and the decrement of a
 *  saved consumable stock. The mod does not start when those hooks cannot be made, so a running mod with the
 *  session on always has them. Stats, quests and the apocalypse counter wait on the RE pass the plan names. */
inline bool suppress_progress(bool devSessionOn) { return devSessionOn; }

/** The reward screen's loss of the brought equipment (a death with the difficulty's lose-items set) becomes
 *  the game's own return of it while a dev session is on, so a dev session never costs a saved item. */
inline bool return_instead_of_losing(bool devSessionOn) { return devSessionOn; }

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
  ForceHunt, EndHunt, ClearCooldown, Appear, UnAppear, ApplyRoom, RandomRoom, PickRoom,  // Ghost
  ToggleEvents, FireEvent, SetGhostBlind,         // Ghost
  FireState,                                      // Ghost: one named attempt (interact, ability, throw)
  GhostToMe,                                      // Ghost: the NavMeshAgent warp to the investigator
  PickRetype, Retype,                             // Ghost: the type, changed now
  BumpFlames, TypeChangeNow, ChaseMe,             // Ghost: the type's own dials, and the hunt's target
  MeToTruck,                                      // Player: back to the truck
  SetFriendly, SetLoseEquipment, BumpEvidence, BumpType,  // Difficulty
  ToggleFreeze, KillLocal, ReviveLocal,           // Player
  ResetPlayerSpeed,                               // Player: the game's own speed multiplier
  SetNoclip, MeToGhost, MeToGhostRoom,            // Player: movement
  SetLockSanity,                                  // Player: the sanity lock
  SetGodMode, SetPauseWhenOpen, SetDebugHud, SetEspGhost, SetEspRooms,  // switches
  SetUnlockAll, SetFreeShop,                     // Equipment: the store
  SpawnDummy, DummyAction,                       // Player: the dummy investigator
  SetEspDoors, SetEspHiding, SetEspSwitches,     // Game: more ESP
  PickMod,                                       // Loader: the mod picker
  PickObjective, CompleteObjective, FailObjective,  // Contract: the objectives, the game's own way
  SetNeverRunOut,                                // Equipment: candles and crucifixes
  PickItem, SpawnItem,                           // Equipment: one of the truck's items at your feet
  CycleSightHold,                                // Ghost: the hunt's sight assist
  OptionToggle, OptionChoice,                    // Difficulty: one custom option's own row (its switch or choice)
  Slide,                                         // a slider's step or drag: the row says which number it sets
  SanityToThreshold,                             // Player: sanity put on, or just above, the ghost's hunt line
  Info,                                          // a read-out: no click
};

// The order is the order of the reasons' text keys in menu.cpp (kWhyKeys): add at the end only.
enum class Why {
  None, NotOffline, NoContract, NoGhost, NoDifficulty, NoPlayer, Hunting, NotHunting, NoRooms, Dead, NoTruck,
  Friendly, NoRig, Alive, Dying, NoDummy, DoorsShut, NoThreshold, NoSpare,
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
    case Action::CompleteObjective:
    case Action::FailObjective:
      return c.contract ? Why::None : Why::NoContract;
    case Action::SpawnItem:
      if (!c.contract) return Why::NoContract;
      return c.player ? Why::None : Why::NoPlayer;
    case Action::SetNeverRunOut:  // a switch: it acts on whatever the contract holds
    case Action::CycleSightHold:  // a switch: it acts only while a hunt runs
    case Action::PickObjective:   // only the menu's own cursor
    case Action::PickItem:        // only the menu's own cursor
    case Action::PickMod:     // only the menu's own cursor
      return Why::None;
    case Action::OptionToggle:
    case Action::OptionChoice:
      // The friendly switch among them keeps its own rule: never mid-hunt (menu.cpp asks it by the option's key).
      if (!c.difficulty) return Why::NoDifficulty;
      return Why::None;
    case Action::Slide:  // the row asks what its own number needs (a difficulty, the investigator or the ghost)
      return Why::None;
    case Action::SanityToThreshold:
      if (!c.contract) return Why::NoContract;
      if (!c.ghost) return Why::NoGhost;
      return c.player ? Why::None : Why::NoPlayer;
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
    case Action::ToggleFreeze:
    case Action::ResetPlayerSpeed:
    case Action::SetNoclip:
    case Action::SetLockSanity:
      return c.player ? Why::None : Why::NoPlayer;
    case Action::SetGodMode:
    case Action::SetPauseWhenOpen:
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

// ---- The settings kept between runs ----

/** The switches a developer sets once and expects again, as the lines of the menu's settings file ("name=number").
 *  What the game holds (noclip, the dummy) and what belongs to one contract (the forced type, the difficulty) are not
 *  kept. */
inline std::string settings_text(const Switches& s) {
  std::ostringstream out;
  out << "pauseWhenOpen=" << s.pauseWhenOpen << "\n"
      << "godMode=" << s.godMode << "\n"
      << "debugHud=" << s.debugHud << "\n"
      << "espGhost=" << s.espGhost << "\n"
      << "espRooms=" << s.espRooms << "\n"
      << "espDoors=" << s.espDoors << "\n"
      << "espHiding=" << s.espHiding << "\n"
      << "espSwitches=" << s.espSwitches << "\n"
      << "lockSanity=" << s.lockSanity << "\n"
      << "lockedSanity=" << s.lockedSanity << "\n"
      << "sightHold=" << s.sightHold << "\n"
      << "ghostSight=" << s.ghostSight << "\n"
      << "unlockAll=" << s.unlockAll << "\n"
      << "freeShop=" << s.freeShop << "\n"
      << "neverRunOut=" << s.neverRunOut << "\n";
  return out.str();
}

/** Reads settings_text's lines back into `s`. A line this build does not know, or whose number does not read, leaves
 *  that switch as it was; a number out of range takes the switch's default. */
inline void read_settings(std::string_view text, Switches& s) {
  const Switches defaults;
  while (!text.empty()) {
    const size_t eol = text.find('\n');
    std::string_view line = text.substr(0, eol);
    text = eol == std::string_view::npos ? std::string_view{} : text.substr(eol + 1);
    if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
    const size_t eq = line.find('=');
    if (eq == std::string_view::npos || eq + 1 >= line.size()) continue;
    const std::string_view key = line.substr(0, eq);
    const std::string number(line.substr(eq + 1));
    char* end = nullptr;
    const long v = std::strtol(number.c_str(), &end, 10);
    if (end == number.c_str() || *end != '\0') continue;
    const bool on = v != 0;
    if (key == "pauseWhenOpen") s.pauseWhenOpen = on;
    else if (key == "godMode") s.godMode = on;
    else if (key == "debugHud") s.debugHud = on;
    else if (key == "espGhost") s.espGhost = on;
    else if (key == "espRooms") s.espRooms = on;
    else if (key == "espDoors") s.espDoors = on;
    else if (key == "espHiding") s.espHiding = on;
    else if (key == "espSwitches") s.espSwitches = on;
    else if (key == "lockSanity") s.lockSanity = on;
    else if (key == "lockedSanity") s.lockedSanity = v >= 0 && v <= 100 ? static_cast<int>(v) : defaults.lockedSanity;
    else if (key == "sightHold") s.sightHold = v >= 0 && v < 3 ? static_cast<int>(v) : defaults.sightHold;
    else if (key == "ghostSight") s.ghostSight = v >= 0 && v < 3 ? static_cast<int>(v) : defaults.ghostSight;
    else if (key == "unlockAll") s.unlockAll = on;
    else if (key == "freeShop") s.freeShop = on;
    else if (key == "neverRunOut") s.neverRunOut = on;
  }
}

}  // namespace pm::devmenu
