// Offline tests for the dev menu's rules (mods/dev_menu/menu_rule.h) and its layout (mods/dev_menu/
// menu_layout.h): the sandbox gate, the pause while open, the hook refusals, when each row may run, and that
// the rows and the headings clear each other. Run: build\dev_menu_test.exe
#include "menu_layout.h"
#include "menu_rule.h"
#include "ui.h"

#include <initializer_list>

#include "check.h"

using namespace pm::devmenu;
using namespace pm::devmenu::layout;

namespace {

void the_menu_opens_only_in_the_sandbox() {
  CHECK(may_open(true, false));   // offline mode, before its room is joined
  CHECK(may_open(true, true));    // a single player contract: the game joins a room even offline
  CHECK(may_open(false, false));  // the main menu: connected to Photon, in no room
  CHECK(!may_open(false, true));  // an online room, private or public: shut
}

void opening_the_menu_freezes_and_closing_restores() {
  Pause p;
  CHECK(pause_open(p, 1.0f, true) == kPausedScale);
  CHECK(p.frozen && p.saved == 1.0f);
  // A second open while frozen keeps the first saved speed, not 0.
  CHECK(pause_open(p, kPausedScale, true) == kPausedScale);
  CHECK(p.saved == 1.0f);
  float restore = 0.0f;
  CHECK(pause_close(p, restore) && restore == 1.0f && !p.frozen);
  CHECK(!pause_close(p, restore));  // a second close restores nothing
}

void pausing_can_be_switched_off() {
  Pause p;
  CHECK(pause_open(p, 2.0f, false) == 2.0f && !p.frozen);
  float restore = 0.0f;
  CHECK(!pause_close(p, restore));
}

void the_menu_does_not_pause_the_clock_by_default() {
  // The user's call, 2026-11-12, after three live tries: the game's own code never writes the time scale,
  // and its picture adapts to light using the scaled frame time, so holding the clock at 0 behind the menu
  // is the one thing every broken screen shared. The pause stays a row a developer switches on, and the
  // clock is left alone until they do.
  const Switches s;
  CHECK(!s.pauseWhenOpen);
  Pause p;
  CHECK(pause_open(p, 1.0f, s.pauseWhenOpen) == 1.0f && !p.frozen);
  float restore = 2.0f;
  CHECK(!pause_close(p, restore) && restore == 2.0f);  // closing changes a clock it never touched
}

// The user's report of 2026-11-12: "the menu UI is placed too high and some text appears over the 'Dev Menu'
// heading". The rows were laid out from one constant, and nothing checked what that put next to what, so the
// first row's box reached up into the hint line's box. Every box here has to clear the one above it.
void the_rows_clear_the_heading_and_the_hint() {
  CHECK(gap(kTitleY, kTitleBox, kHintY, kHintBox) >= 4.0f);   // the heading and the hint under it
  CHECK(gap(kHintY, kHintBox, listTop(), 0.0f) >= 4.0f);       // the hint and the list's first row (what the
                                                               // live test of 2026-11-12 found overlapping)
  CHECK(gap(kViewY - kViewHigh / 2.0f, 0.0f, kRefuseY, kRefuseBox) >= 4.0f);  // the list and the refusal line
  CHECK(listTop() - kPitch >= kViewY - kViewHigh / 2.0f);      // a whole row fits at the top of the view
  CHECK(kViewX + kViewWide / 2.0f <= 960.0f);                  // the list stays on a 1920 wide canvas
  CHECK(kNavX >= -960.0f);                                     // and so does the section column
}

// The list scrolls, so its length is not limited by the screen: the last row of a list far longer than the
// view is still reachable, and a section that already fits is never scrolled out of place.
void a_long_section_reaches_its_last_row() {
  const int rows = 60;
  const float height = listHeight(rows);
  CHECK(height > kViewHigh);  // a list the view cannot show at once
  float top = 0.0f;
  for (int i = 0; i < rows; ++i) top = pm::ui::scroll_top_needed(height, kViewHigh, top, rowTop(i), kPitch);
  // The last row is on screen: its top is at or below the view's top edge, and its bottom above the bottom.
  CHECK(rowTop(rows - 1) >= top);
  CHECK(rowTop(rows - 1) + kPitch <= top + kViewHigh);
  CHECK(top <= height - kViewHigh);  // never scrolled past the end
  // A section that fits is left alone, wherever it was.
  const float fits = listHeight(8);
  CHECK(pm::ui::scroll_top_needed(fits, kViewHigh, 0.0f, rowTop(7), kPitch) == 0.0f);
  // Scrolling only moves as far as it must: a row already showing leaves the list where it is.
  CHECK(pm::ui::scroll_top_needed(height, kViewHigh, 200.0f, rowTop(6), kPitch) == 200.0f);
}

// The player speed rows step the game's own speed multiplier (LocalPCPlayer.SetPlayerSpeedMultiplier, the
// call a Monkey Paw wish makes). The step and its bounds are the menu's, not the game's: a developer wants
// room to run, and a multiplier of 0 would leave the row with no way back up.
void the_speed_rows_step_within_the_menu_bounds() {
  CHECK(next_speed_multiplier(1.0f, +1) == 1.25f);
  CHECK(next_speed_multiplier(1.0f, -1) == 0.75f);
  CHECK(next_speed_multiplier(1.0f, +4) == 2.0f);
  CHECK(next_speed_multiplier(0.25f, -1) == 0.25f);  // the floor
  CHECK(next_speed_multiplier(0.0f, -1) == 0.25f);   // a rig stopped dead still steps up to the floor
  CHECK(next_speed_multiplier(4.0f, +1) == 4.0f);    // and the ceiling
  CHECK(next_speed_multiplier(1.6f, +1) == 1.75f);   // an odd value the game left there still steps
}

void the_new_rows_refuse_for_what_the_game_lacks() {
  Can c;
  c.sandbox = true;
  CHECK(refuse(Action::BumpPlayerSpeed, c) == Why::NoPlayer);
  CHECK(refuse(Action::ResetPlayerSpeed, c) == Why::NoPlayer);
  c.player = true;
  CHECK(refuse(Action::BumpPlayerSpeed, c) == Why::None);
  CHECK(refuse(Action::ResetPlayerSpeed, c) == Why::None);
  // Blinding the ghost asks its vision sensor, so it needs a ghost.
  CHECK(refuse(Action::SetGhostBlind, c) == Why::NoContract);
  c.contract = true;
  CHECK(refuse(Action::SetGhostBlind, c) == Why::NoGhost);
  c.ghost = true;
  CHECK(refuse(Action::SetGhostBlind, c) == Why::None);
  // The debug read-out is the mod's own text on its own canvas: it needs nothing from a contract.
  Can free;
  free.sandbox = true;  // the sandbox gate still covers it: the menu only opens there
  CHECK(refuse(Action::SetDebugHud, free) == Why::None);
  CHECK(refuse(Action::SetDebugHud, Can{}) == Why::NotOffline);
}

void the_debug_read_out_is_a_switch_like_the_others() {
  const Switches s;
  CHECK(!s.debugHud);  // off until a developer asks for it
}

void the_tick_speed_row_cycles_the_presets() {
  CHECK(next_time_scale(1.0f) == 2.0f);
  CHECK(next_time_scale(2.0f) == 4.0f);
  CHECK(next_time_scale(4.0f) == kPausedScale);  // wraps to paused
  CHECK(next_time_scale(kPausedScale) == 0.25f);
  CHECK(next_time_scale(0.0f) == 0.25f);  // a clock someone else stopped still steps up
  CHECK(next_time_scale(0.25f) == 0.5f);
  CHECK(next_time_scale(0.5f) == 1.0f);
  CHECK(next_time_scale(1.3f) == 2.0f);  // between presets: the next above
}

void the_hooks_answer_the_switches() {
  // God mode refuses only the local investigator's kill.
  CHECK(god_refuses_kill(true, true));
  CHECK(!god_refuses_kill(true, false));
  CHECK(!god_refuses_kill(false, true));
  // Progression checks are skipped exactly while a dev session is on.
  CHECK(suppress_progress(true));
  CHECK(!suppress_progress(false));
  // A dev session returns the brought equipment instead of losing it, and only while it is on.
  CHECK(return_instead_of_losing(true));
  CHECK(!return_instead_of_losing(false));
}

void a_full_offline_contract_allows_everything() {
  const Can c{.sandbox = true, .contract = true, .ghost = true, .difficulty = true, .player = true, .rooms = 4};
  for (const Action a : {Action::ForceHunt, Action::ClearCooldown, Action::Appear, Action::UnAppear,
                         Action::BumpSpeed, Action::ToggleEvents, Action::FireEvent, Action::FireState,
                         Action::GhostToMe, Action::UnlockDoor,
                         Action::EndContract, Action::Leave, Action::Quit, Action::SetFriendly,
                         Action::SetLoseEquipment,
                         Action::BumpEvidence, Action::BumpType, Action::SetSanity, Action::ToggleFreeze,
                         Action::KillLocal, Action::SetGodMode,
                         Action::SetNoclip, Action::MeToGhost, Action::MeToGhostRoom,
                         Action::SetLockSanity, Action::CycleLockedSanity,
                         Action::SetPauseWhenOpen, Action::CycleTimeScale, Action::SetEspGhost,
                         Action::SetEspRooms})
    CHECK(refuse(a, c) == Why::None);
  CHECK(refuse(Action::EndHunt, c) == Why::NotHunting);
}

void rows_refuse_for_what_the_contract_lacks() {
  Can nothing{};  // not even in the sandbox: the menu could not be open
  CHECK(refuse(Action::ForceHunt, nothing) == Why::NotOffline);

  // In the sandbox with no contract (the main menu): only the switches and time work.
  const Can menu{.sandbox = true};
  CHECK(refuse(Action::ForceHunt, menu) == Why::NoContract);
  CHECK(refuse(Action::UnlockDoor, menu) == Why::NoContract);
  CHECK(refuse(Action::EndContract, menu) == Why::NoContract);
  CHECK(refuse(Action::Leave, menu) == Why::NoContract);
  CHECK(refuse(Action::SetFriendly, menu) == Why::NoDifficulty);
  CHECK(refuse(Action::SetSanity, menu) == Why::NoPlayer);
  CHECK(refuse(Action::CycleTimeScale, menu) == Why::None);
  CHECK(refuse(Action::SetGodMode, menu) == Why::None);

  // A contract with no ghost (the first seconds) still runs the difficulty and player rows.
  const Can bare{.sandbox = true, .contract = true, .difficulty = true, .player = true};
  CHECK(refuse(Action::ForceHunt, bare) == Why::NoGhost);
  CHECK(refuse(Action::SetFriendly, bare) == Why::None);
}

void hunt_and_room_rows_follow_the_hunt() {
  const Can hunting{.sandbox = true, .contract = true, .ghost = true, .player = true, .hunting = true, .rooms = 3};
  CHECK(refuse(Action::EndHunt, hunting) == Why::None);
  CHECK(refuse(Action::ApplyRoom, hunting) == Why::Hunting);
  CHECK(refuse(Action::RandomRoom, hunting) == Why::Hunting);
  CHECK(refuse(Action::PickRoom, hunting) == Why::Hunting);

  const Can noRooms{.sandbox = true, .contract = true, .ghost = true, .rooms = 0};
  CHECK(refuse(Action::ApplyRoom, noRooms) == Why::NoRooms);
  CHECK(refuse(Action::ForceHunt, noRooms) == Why::None);  // a hunt needs no room list
}

void a_dead_investigator_cannot_be_killed_again() {
  const Can dead{.sandbox = true, .contract = true, .player = true, .dead = true};
  CHECK(refuse(Action::KillLocal, dead) == Why::Dead);
  CHECK(refuse(Action::SetSanity, dead) == Why::None);  // sanity can still be set on a corpse
  CHECK(refuse(Action::ToggleFreeze, dead) == Why::None);
  // The revive answers the kill: a dead or dying investigator in any contract (the action lends the paw).
  CHECK(refuse(Action::ReviveLocal, dead) == Why::None);
  const Can dying{.sandbox = true, .contract = true, .player = true, .dying = true};
  CHECK(refuse(Action::ReviveLocal, dying) == Why::None);
  const Can alive{.sandbox = true, .contract = true, .player = true};
  CHECK(refuse(Action::ReviveLocal, alive) == Why::Alive);
  const Can lobby{.sandbox = true, .player = true, .dead = true};
  CHECK(refuse(Action::ReviveLocal, lobby) == Why::NoContract);
}

// The kill row refuses before the game is asked, everywhere the game's own RPC body refuses and reports the
// sender (sdk/systems/players.md): no contract, a friendly difficulty, or a kill already under way. A hunt alone
// is no reason: the user's call, 2026-11-12.
void the_kill_row_refuses_everywhere_the_game_would_report() {
  const Can lobby{.sandbox = true, .player = true};
  CHECK(refuse(Action::KillLocal, lobby) == Why::NoContract);
  const Can friendly{.sandbox = true, .contract = true, .player = true, .friendly = true};
  CHECK(refuse(Action::KillLocal, friendly) == Why::Friendly);
  const Can hunting{.sandbox = true, .contract = true, .player = true, .hunting = true};
  CHECK(refuse(Action::KillLocal, hunting) == Why::None);
  const Can dying{.sandbox = true, .contract = true, .player = true, .hunting = true, .dying = true};
  CHECK(refuse(Action::KillLocal, dying) == Why::Dying);
  const Can normal{.sandbox = true, .contract = true, .player = true};
  CHECK(refuse(Action::KillLocal, normal) == Why::None);
}

// The hunt's RPC body refuses a friendly difficulty, and a hunt that started unfriendly would meet a
// friendly check in its own kill: never force a hunt on a friendly ghost, never switch mid-hunt.
void a_friendly_ghost_refuses_forced_hunts_and_switches_only_between_hunts() {
  const Can friendly{.sandbox = true, .contract = true, .ghost = true, .friendly = true};
  CHECK(refuse(Action::ForceHunt, friendly) == Why::Friendly);
  const Can switchMidHunt{.sandbox = true, .difficulty = true, .hunting = true};
  CHECK(refuse(Action::SetFriendly, switchMidHunt) == Why::Hunting);
  const Can betweenHunts{.sandbox = true, .difficulty = true};
  CHECK(refuse(Action::SetFriendly, betweenHunts) == Why::None);
  const Can hunting{.sandbox = true, .contract = true, .ghost = true, .hunting = true};
  CHECK(refuse(Action::Appear, hunting) == Why::Hunting);
}

// The movement rows warp the investigator or the ghost, so they need a ghost and this client's investigator;
// the room row also needs the level to have rooms. Noclip only moves the investigator.
void the_movement_rows_need_a_ghost_and_a_player() {
  const Can menu{.sandbox = true};
  CHECK(refuse(Action::MeToGhost, menu) == Why::NoContract);
  CHECK(refuse(Action::GhostToMe, menu) == Why::NoContract);
  CHECK(refuse(Action::MeToGhostRoom, menu) == Why::NoContract);
  const Can bare{.sandbox = true, .contract = true};
  CHECK(refuse(Action::MeToGhost, bare) == Why::NoGhost);
  CHECK(refuse(Action::GhostToMe, bare) == Why::NoGhost);
  CHECK(refuse(Action::MeToGhostRoom, bare) == Why::NoGhost);
  const Can noPlayer{.sandbox = true, .contract = true, .ghost = true};
  CHECK(refuse(Action::MeToGhost, noPlayer) == Why::NoPlayer);
  CHECK(refuse(Action::GhostToMe, noPlayer) == Why::NoPlayer);
  CHECK(refuse(Action::MeToGhostRoom, noPlayer) == Why::NoPlayer);
  CHECK(refuse(Action::SetNoclip, noPlayer) == Why::NoPlayer);
  const Can noRooms{.sandbox = true, .contract = true, .ghost = true, .player = true, .rooms = 0};
  CHECK(refuse(Action::MeToGhostRoom, noRooms) == Why::NoRooms);
  CHECK(refuse(Action::MeToGhost, noRooms) == Why::None);  // only the room row needs the room list
  const Can rig{.sandbox = true, .player = true};
  CHECK(refuse(Action::SetNoclip, rig) == Why::None);
}

// The locked sanity cycles 100, 50, 0, and the lock only puts the value back once the game has moved it by
// more than half a point, so the game's setter (which sends the value on) is not asked every frame.
void the_sanity_lock_cycles_and_waits_for_real_drift() {
  CHECK(next_locked_sanity(100) == 50);
  CHECK(next_locked_sanity(50) == 0);
  CHECK(next_locked_sanity(0) == 100);
  CHECK(next_locked_sanity(75) == 50);  // any value above 50 steps to 50
  CHECK(!sanity_needs_reset(50.0f, 50));
  CHECK(!sanity_needs_reset(50.4f, 50));  // a drift under half a point waits
  CHECK(sanity_needs_reset(51.0f, 50));
  CHECK(sanity_needs_reset(0.0f, 100));
}

void the_new_switches_start_off() {
  const Switches s;
  CHECK(!s.noclip);
  CHECK(!s.lockSanity);
  CHECK(!s.espGhost);
  CHECK(!s.espRooms);
  CHECK(s.lockedSanity == 100);
}

// menu.cpp's kWhyKeys array indexes the reasons' text in enum order, so the reasons only ever grow at the end.
void no_revive_reason_is_left() {
  static_assert(static_cast<int>(Why::NoRig) == static_cast<int>(Why::Friendly) + 1,
                "menu.cpp indexes the reasons' text keys in Why order");
  static_assert(static_cast<int>(Why::Dying) == static_cast<int>(Why::Alive) + 1,
                "menu.cpp indexes the reasons' text keys in Why order");
}

// The live test of 2026-11-12 (pause while open): every collider of the local rig went to NaN and the picture
// flashed, then went black. PhysicsCharacterController.Move divides the frame's walk by Time.deltaTime, and a time
// scale of 0 makes that 0/0 for every frame of the pause; the first physical step after it moves the rig by that NaN.
// The game itself never writes the time scale, so the menu never writes 0: "paused" is the smallest scale that is not.
void the_menu_never_stops_the_clock_dead() {
  CHECK(kPausedScale > 0.0f && kPausedScale <= 0.001f);
  Pause p;
  CHECK(pause_open(p, 1.0f, true) > 0.0f);
  float scale = 1.0f;
  for (int i = 0; i < 12; ++i) {  // twice round every preset
    scale = next_time_scale(scale);
    CHECK(scale > 0.0f);
  }
}

// The save freeze follows the dev session and never lets go within a run; the store switches answer only inside
// the sandbox, so an online lobby later in the same run sees the game's own answers (the user's ask, 2026-11-12).
void the_store_is_free_only_in_the_sandbox_and_the_save_stays() {
  CHECK(freeze_save(true));
  CHECK(!freeze_save(false));
  CHECK(store_switch_applies(true, true));
  CHECK(!store_switch_applies(true, false));
  CHECK(!store_switch_applies(false, true));
  const Switches s;
  CHECK(!s.unlockAll && !s.freeShop);
  const Can menu{.sandbox = true};
  CHECK(refuse(Action::SetUnlockAll, menu) == Why::None);
  CHECK(refuse(Action::SetFreeShop, menu) == Why::None);
  CHECK(refuse(Action::SetFreeShop, Can{}) == Why::NotOffline);
}

// The dummy: it is added beside the investigator in a contract, and every other dummy row needs it there. The
// switches that share the plain answer are not caught by the dummy's tests (a fall-through once did that).
void the_dummy_rows_need_a_contract_and_the_dummy() {
  const Can menu{.sandbox = true, .player = true};
  CHECK(refuse(Action::SpawnDummy, menu) == Why::NoContract);
  const Can contract{.sandbox = true, .contract = true, .player = true};
  CHECK(refuse(Action::SpawnDummy, contract) == Why::None);
  CHECK(refuse(Action::DummyAction, contract) == Why::NoDummy);
  const Can withDummy{.sandbox = true, .contract = true, .player = true, .dummy = true};
  CHECK(refuse(Action::DummyAction, withDummy) == Why::None);
  for (const Action a : {Action::SetGodMode, Action::SetEspRooms, Action::SetUnlockAll, Action::SetFreeShop})
    CHECK(refuse(a, Can{.sandbox = true}) == Why::None);
}

// The custom difficulty's options step on the contract's Difficulty alone; the picker's cursor needs nothing.
void every_difficulty_option_needs_only_the_difficulty() {
  CHECK(refuse(Action::PickOption, Can{.sandbox = true}) == Why::None);
  CHECK(refuse(Action::StepOption, Can{.sandbox = true}) == Why::NoDifficulty);
  CHECK(refuse(Action::StepOption, Can{.sandbox = true, .difficulty = true}) == Why::None);
}

// The hunt's target can be pointed only at a hunt that runs; the type's dials need only the ghost.
void the_hunt_target_row_needs_a_hunt() {
  const Can quiet{.sandbox = true, .contract = true, .ghost = true, .player = true};
  CHECK(refuse(Action::ChaseMe, quiet) == Why::NotHunting);
  CHECK(refuse(Action::BumpFlames, quiet) == Why::None);
  CHECK(refuse(Action::TypeChangeNow, quiet) == Why::None);
  CHECK(refuse(Action::MeToTruck, quiet) == Why::None);
  const Can hunting{.sandbox = true, .contract = true, .ghost = true, .player = true, .hunting = true};
  CHECK(refuse(Action::ChaseMe, hunting) == Why::None);
  CHECK(refuse(Action::MeToTruck, Can{.sandbox = true, .player = true}) == Why::NoContract);
}

// The weather and the re-type: a contract for both, a difficulty for the weather, and no hunt for the re-type.
void the_weather_and_type_rows_follow_the_contract() {
  const Can menu{.sandbox = true};
  CHECK(refuse(Action::ApplyWeather, menu) == Why::NoContract);
  CHECK(refuse(Action::PickWeather, menu) == Why::None);
  const Can contract{.sandbox = true, .contract = true, .ghost = true, .difficulty = true, .player = true};
  CHECK(refuse(Action::ApplyWeather, contract) == Why::None);
  CHECK(refuse(Action::Retype, contract) == Why::None);
  const Can hunting{.sandbox = true, .contract = true, .ghost = true, .difficulty = true, .hunting = true};
  CHECK(refuse(Action::Retype, hunting) == Why::Hunting);
  CHECK(refuse(Action::ApplyWeather, hunting) == Why::None);
}

}  // namespace

int main() {
  the_menu_opens_only_in_the_sandbox();
  opening_the_menu_freezes_and_closing_restores();
  pausing_can_be_switched_off();
  the_menu_does_not_pause_the_clock_by_default();
  the_rows_clear_the_heading_and_the_hint();
  a_long_section_reaches_its_last_row();
  the_speed_rows_step_within_the_menu_bounds();
  the_new_rows_refuse_for_what_the_game_lacks();
  the_debug_read_out_is_a_switch_like_the_others();
  the_tick_speed_row_cycles_the_presets();
  the_hooks_answer_the_switches();
  a_full_offline_contract_allows_everything();
  rows_refuse_for_what_the_contract_lacks();
  hunt_and_room_rows_follow_the_hunt();
  a_dead_investigator_cannot_be_killed_again();
  the_kill_row_refuses_everywhere_the_game_would_report();
  a_friendly_ghost_refuses_forced_hunts_and_switches_only_between_hunts();
  the_movement_rows_need_a_ghost_and_a_player();
  the_sanity_lock_cycles_and_waits_for_real_drift();
  the_new_switches_start_off();
  no_revive_reason_is_left();
  the_store_is_free_only_in_the_sandbox_and_the_save_stays();
  the_menu_never_stops_the_clock_dead();
  the_weather_and_type_rows_follow_the_contract();
  the_dummy_rows_need_a_contract_and_the_dummy();
  the_hunt_target_row_needs_a_hunt();
  every_difficulty_option_needs_only_the_difficulty();
  CHECK(refuse(Action::PickMod, Can{.sandbox = true}) == Why::None);  // the Loader page reads, anywhere
  // The objectives, the spawned item and the switches of this round.
  CHECK(refuse(Action::CompleteObjective, Can{.sandbox = true}) == Why::NoContract);
  CHECK(refuse(Action::SpawnItem, Can{.sandbox = true, .contract = true}) == Why::NoPlayer);
  CHECK(refuse(Action::CycleSightHold, Can{.sandbox = true}) == Why::None);
  CHECK(next_sight_hold(kSightGame) == kSightNone && next_sight_hold(kSightNone) == kSightFrozen &&
        next_sight_hold(kSightFrozen) == kSightGame);
  CHECK(!Switches{}.neverRunOut && Switches{}.sightHold == kSightGame);
  CHECK(navOffset(6) - kNavPitch / 2.0f > kRefuseY + kRefuseBox / 2.0f);  // the seventh section clears the refusal line
  std::printf(g_failures ? "%d check(s) failed\n" : "all dev menu rule tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
