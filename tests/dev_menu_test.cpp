// Offline tests for the dev menu's rules (mods/dev_menu/menu_rule.h): the sandbox gate, the pause while
// open, the hook refusals and when each row may run. Run: build\dev_menu_test.exe
#include "menu_rule.h"

#include <initializer_list>

#include "check.h"

using namespace pm::devmenu;

namespace {

void the_menu_opens_only_offline() {
  CHECK(may_open(true, false));
  CHECK(!may_open(false, false));  // in a room, or judging it impossible: shut
  CHECK(!may_open(true, true));    // defensive: offline and in a room cannot both hold
  CHECK(!may_open(false, true));
}

void opening_the_menu_freezes_and_closing_restores() {
  Pause p;
  CHECK(pause_open(p, 1.0f, true) == 0.0f);
  CHECK(p.frozen && p.saved == 1.0f);
  // A second open while frozen keeps the first saved speed, not 0.
  CHECK(pause_open(p, 0.0f, true) == 0.0f);
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

void the_tick_speed_row_cycles_the_presets() {
  CHECK(next_time_scale(1.0f) == 2.0f);
  CHECK(next_time_scale(2.0f) == 0.0f);  // wraps to paused
  CHECK(next_time_scale(0.0f) == 0.5f);
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
  // A consumable stock is protected only while the switch is on.
  CHECK(keep_refuses_consume(true));
  CHECK(!keep_refuses_consume(false));
}

void a_full_offline_contract_allows_everything() {
  const Can c{.offline = true, .contract = true, .ghost = true, .difficulty = true, .player = true, .rooms = 4};
  for (const Action a : {Action::ForceHunt, Action::ClearCooldown, Action::Appear, Action::UnAppear,
                         Action::BumpSpeed, Action::ToggleEvents, Action::FireEvent, Action::UnlockDoor,
                         Action::Leave, Action::Quit, Action::SetFriendly, Action::SetLoseEquipment,
                         Action::BumpEvidence, Action::BumpType, Action::SetSanity, Action::ToggleFreeze,
                         Action::KillLocal, Action::SetGodMode, Action::SetKeepConsumables,
                         Action::SetPauseWhenOpen, Action::CycleTimeScale})
    CHECK(refuse(a, c) == Why::None);
  CHECK(refuse(Action::EndHunt, c) == Why::NotHunting);
  CHECK(refuse(Action::ReviveLocal, c) == Why::Alive);
}

void rows_refuse_for_what_the_contract_lacks() {
  Can nothing{};  // not even offline: the menu could not be open
  CHECK(refuse(Action::ForceHunt, nothing) == Why::NotOffline);

  // In the main menu (offline, no contract): only the switches and time work.
  const Can menu{.offline = true};
  CHECK(refuse(Action::ForceHunt, menu) == Why::NoContract);
  CHECK(refuse(Action::UnlockDoor, menu) == Why::NoContract);
  CHECK(refuse(Action::Leave, menu) == Why::NoContract);
  CHECK(refuse(Action::SetFriendly, menu) == Why::NoDifficulty);
  CHECK(refuse(Action::SetSanity, menu) == Why::NoPlayer);
  CHECK(refuse(Action::CycleTimeScale, menu) == Why::None);
  CHECK(refuse(Action::SetGodMode, menu) == Why::None);

  // A contract with no ghost (the first seconds) still runs the difficulty and player rows.
  const Can bare{.offline = true, .contract = true, .difficulty = true, .player = true};
  CHECK(refuse(Action::ForceHunt, bare) == Why::NoGhost);
  CHECK(refuse(Action::SetFriendly, bare) == Why::None);
}

void hunt_and_room_rows_follow_the_hunt() {
  const Can hunting{.offline = true, .contract = true, .ghost = true, .player = true, .hunting = true, .rooms = 3};
  CHECK(refuse(Action::EndHunt, hunting) == Why::None);
  CHECK(refuse(Action::ApplyRoom, hunting) == Why::Hunting);
  CHECK(refuse(Action::RandomRoom, hunting) == Why::Hunting);
  CHECK(refuse(Action::PickRoom, hunting) == Why::Hunting);

  const Can noRooms{.offline = true, .contract = true, .ghost = true, .rooms = 0};
  CHECK(refuse(Action::ApplyRoom, noRooms) == Why::NoRooms);
  CHECK(refuse(Action::ForceHunt, noRooms) == Why::None);  // a hunt needs no room list
}

void death_and_revival_answer_each_other() {
  const Can dead{.offline = true, .player = true, .dead = true};
  CHECK(refuse(Action::KillLocal, dead) == Why::Dead);
  CHECK(refuse(Action::ReviveLocal, dead) == Why::None);
  CHECK(refuse(Action::SetSanity, dead) == Why::None);  // sanity can still be set on a corpse
  CHECK(refuse(Action::ToggleFreeze, dead) == Why::None);
}

}  // namespace

int main() {
  the_menu_opens_only_offline();
  opening_the_menu_freezes_and_closing_restores();
  pausing_can_be_switched_off();
  the_tick_speed_row_cycles_the_presets();
  the_hooks_answer_the_switches();
  a_full_offline_contract_allows_everything();
  rows_refuse_for_what_the_contract_lacks();
  hunt_and_room_rows_follow_the_hunt();
  death_and_revival_answer_each_other();
  std::printf(g_failures ? "%d check(s) failed\n" : "all dev menu rule tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
