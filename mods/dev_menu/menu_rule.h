#pragma once

// The rules behind the dev menu, kept free of game calls so they can be tested offline
// (docs/superpowers/specs/2026-11-10-dev-menu-content-plan.md): the sandbox gate, the pause while the menu
// is open, which actions a row may run right now and what the mod's three hooks refuse. Everything a row
// *shows* is read from the game every frame (actions.cpp); only these rules decide.
namespace pm::devmenu {

// ---- The sandbox gate (the user's final call, 2026-11-10, third round) ----
// The menu opens only while PUN runs offline, which is the game's own single player state: no room exists,
// nothing is listed, nobody can join, and every action runs the game's single player path exactly as a
// vanilla contract does. An offline mode the game cannot answer (the entry did not resolve, or a room this
// client cannot judge) is not offline: the menu stays shut, like every loader gate.
inline bool may_open(bool offlineMode, bool inRoom) { return offlineMode && !inRoom; }

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
  bool pauseWhenOpen = true;    // freeze the game at timeScale 0 while the menu is open
  bool godMode = false;         // a hunt's kill of the local investigator is refused
  bool keepConsumables = false;  // a saved consumable stock never goes down
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
  UnlockDoor, Leave, Quit,                       // Contract
  ForceHunt, EndHunt, ClearCooldown, Appear, UnAppear, ApplyRoom, RandomRoom, PickRoom, BumpSpeed,  // Ghost
  ToggleEvents, FireEvent,                        // Ghost
  SetFriendly, SetLoseEquipment, BumpEvidence, BumpType,  // Difficulty
  SetSanity, ToggleFreeze, KillLocal, ReviveLocal,         // Player
  SetGodMode, SetKeepConsumables, SetPauseWhenOpen, CycleTimeScale,  // switches
  Info,                                          // a read-out: no click
};

enum class Why {
  None, NotOffline, NoContract, NoGhost, NoDifficulty, NoPlayer, Hunting, NotHunting, NoRooms, Dead, Alive
};

// What the game looks like to the menu right now, read every tick (actions.cpp fills it).
struct Can {
  bool offline = false;      // get_OfflineMode
  bool contract = false;     // a LevelController, so a contract runs
  bool ghost = false;        // the contract has a ghost
  bool difficulty = false;   // LevelValues and its difficulty object
  bool player = false;       // this client has an investigator
  bool hunting = false;      // the ghost hunts now
  bool dead = false;         // the local investigator is dead
  int rooms = 0;             // the level's rooms
};

/** Why running `action` now is refused, or Why::None. Being offline is assumed (the menu shut otherwise)
 *  and kept as a defensive check. */
inline Why refuse(Action action, const Can& c) {
  if (!c.offline) return Why::NotOffline;
  switch (action) {
    case Action::Info:
      return Why::None;
    case Action::Leave:
    case Action::Quit:
    case Action::UnlockDoor:
      return c.contract ? Why::None : Why::NoContract;
    case Action::ForceHunt:
    case Action::ClearCooldown:
    case Action::Appear:
    case Action::UnAppear:
    case Action::BumpSpeed:
    case Action::ToggleEvents:
    case Action::FireEvent:
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
      if (!c.player) return Why::NoPlayer;
      if (action == Action::KillLocal) return c.dead ? Why::Dead : Why::None;
      if (action == Action::ReviveLocal) return c.dead ? Why::None : Why::Alive;
      return Why::None;
    case Action::SetGodMode:
    case Action::SetKeepConsumables:
    case Action::SetPauseWhenOpen:
    case Action::CycleTimeScale:
      return Why::None;
  }
  return Why::None;
}

}  // namespace pm::devmenu
