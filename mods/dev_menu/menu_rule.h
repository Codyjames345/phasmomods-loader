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
 *  first (0 paused, 0.5, 1 real time, 2). A value between presets moves to the next one above it. */
inline float next_time_scale(float current) {
  static constexpr float kScales[] = {0.0f, 0.5f, 1.0f, 2.0f};
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
  bool godMode = false;         // a hunt's kill of the local investigator is refused
  bool keepConsumables = false;  // a saved consumable stock never goes down
  // The debug read-out stays on screen while the menu is closed: the ghost's state, its type, its speed and
  // who its hunt is set on (the user's original brief). Off until a developer asks for it.
  bool debugHud = false;
};

/** God mode: only the local investigator's kill is refused; a hunt of anybody else (a dummy, another
 *  player) runs. The kill's network body is never touched, and offline the kill decision is this client's. */
inline bool god_refuses_kill(bool godMode, bool targetIsLocal) { return godMode && targetIsLocal; }

/** Progression suppression (the user's rule, 2026-11-10): while a dev session is on, the contract's mission
 *  and challenge checks are skipped, so the session earns nothing towards XP, money, stats, quests, the
 *  apocalypse or achievements. The mod does not start when those hooks cannot be made, so a running mod
 *  with the session on always has them. Coverage past these two checks waits on the RE pass the plan names. */
inline bool suppress_progress(bool devSessionOn) { return devSessionOn; }

/** Never lose a consumable: a decrement of the saved stock is skipped while the switch is on. */
inline bool keep_refuses_consume(bool keepConsumables) { return keepConsumables; }

// ---- What the rows may run right now ----
// A row's click asks this first; the game's own guards still decide, and a game refusal shows its reason.
enum class Action {
  UnlockDoor, EndContract, Leave, Quit,          // Contract
  ForceHunt, EndHunt, ClearCooldown, Appear, UnAppear, ApplyRoom, RandomRoom, PickRoom, BumpSpeed,  // Ghost
  ToggleEvents, FireEvent, SetGhostBlind,         // Ghost
  SetFriendly, SetLoseEquipment, BumpEvidence, BumpType,  // Difficulty
  SetSanity, ToggleFreeze, KillLocal, ReviveLocal,         // Player
  BumpPlayerSpeed, ResetPlayerSpeed,                     // Player: the game's own speed multiplier
  SetGodMode, SetKeepConsumables, SetPauseWhenOpen, CycleTimeScale, SetDebugHud,  // switches
  Info,                                          // a read-out: no click
};

enum class Why {
  None, NotOffline, NoContract, NoGhost, NoDifficulty, NoPlayer, Hunting, NotHunting, NoRooms, Dead, Alive, NoTruck
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
  int rooms = 0;             // the level's rooms
};

/** Why running `action` now is refused, or Why::None. The sandbox gate is assumed (the menu shut otherwise)
 *  and kept as a defensive check. Why::NoTruck is not one of these rules: refuse() never returns it, it
 *  is what the end-contract action reports when the running scene has no ExitLevel object to ask. */
inline Why refuse(Action action, const Can& c) {
  if (!c.sandbox) return Why::NotOffline;
  switch (action) {
    case Action::Info:
      return Why::None;
    case Action::Leave:
    case Action::Quit:
    case Action::UnlockDoor:
    case Action::EndContract:
      return c.contract ? Why::None : Why::NoContract;
    case Action::ForceHunt:
    case Action::ClearCooldown:
    case Action::Appear:
    case Action::UnAppear:
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
    case Action::SetLoseEquipment:
    case Action::BumpEvidence:
    case Action::BumpType:
      return c.difficulty ? Why::None : Why::NoDifficulty;
    case Action::SetSanity:
    case Action::ToggleFreeze:
    case Action::KillLocal:
    case Action::ReviveLocal:
    case Action::BumpPlayerSpeed:    // the game's own multiplier, on this client's rig
    case Action::ResetPlayerSpeed:
      if (!c.player) return Why::NoPlayer;
      if (action == Action::KillLocal) return c.dead ? Why::Dead : Why::None;
      if (action == Action::ReviveLocal) return c.dead ? Why::None : Why::Alive;
      return Why::None;
    case Action::SetGodMode:
    case Action::SetKeepConsumables:
    case Action::SetPauseWhenOpen:
    case Action::CycleTimeScale:
    case Action::SetDebugHud:  // the mod's own text on its own canvas: it needs nothing from a contract
      return Why::None;
  }
  return Why::None;
}

}  // namespace pm::devmenu
