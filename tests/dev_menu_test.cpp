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
// heading". Every box here has to clear its neighbours, and everything stays on a 1920 by 1080 canvas.
void the_rows_clear_the_heading_and_the_hint() {
  CHECK(gap(kHeaderY, kHeaderBox, kRuleY, 0.0f) >= 4.0f);      // the header sits above its rule
  CHECK(gap(kRuleY, 0.0f, kViewTop, 0.0f) >= 4.0f);             // the groups start under the rule
  CHECK(gap(kViewBottom, 0.0f, kFooterY, kFooterBox) >= 4.0f);  // and end above the footer
  CHECK(kSlabWide <= 1920.0f && kSlabHigh <= 1080.0f);
  CHECK(kHeaderY + kHeaderBox / 2.0f <= kSlabHigh / 2.0f);      // the header inside the slab
  CHECK(kFooterY - kFooterBox / 2.0f >= -kSlabHigh / 2.0f);     // and the footer
  CHECK(kViewRight <= kSlabWide / 2.0f && kNavLeft >= -kSlabWide / 2.0f);
  CHECK(kNavLeft + kNavWide < kViewLeft);                         // the section column clears the groups
  CHECK(navOffset(6) - kNavPitch / 2.0f > kFooterY + kFooterBox / 2.0f);  // all seven sections fit
  // A panel's row has room for its label beside a picker's value and its two step buttons.
  CHECK(kRowWide - kPickerWide >= 240.0f);
}

// The groups flow into two columns, each into the shorter so far, so the columns end close together and a section
// uses the width of the screen instead of one long list. The scrolling area still reaches the last row of a section
// far taller than the view.
void the_groups_fill_two_columns() {
  const std::vector<float> heights = {card_height(5), card_height(6), card_height(4), card_height(8)};
  const std::vector<Placed> placed = flow(heights);
  CHECK(placed.size() == 4);
  CHECK(placed[0].column == 0 && placed[0].top == 0.0f);
  CHECK(placed[1].column == 1 && placed[1].top == 0.0f);
  CHECK(placed[2].column == 0 && placed[2].top == card_height(5) + kCardGap);  // the left column was shorter
  CHECK(placed[3].column == 1);
  const float tall = content_height(heights);
  CHECK(tall == placed[3].top + card_height(8));
  // Panels in the same column never overlap.
  for (size_t i = 0; i < placed.size(); ++i)
    for (size_t j = i + 1; j < placed.size(); ++j)
      if (placed[i].column == placed[j].column)
        CHECK(placed[j].top >= placed[i].top + heights[i] + kCardGap - 0.01f);
  // Both columns stay inside the scrolling area.
  CHECK(column_x(0) - kCardWide / 2.0f >= -kViewWide / 2.0f - 0.01f);
  CHECK(column_x(1) + kCardWide / 2.0f <= kViewWide / 2.0f + 0.01f);
  // A section much taller than the view: its last row can still be scrolled into view.
  const std::vector<float> many(12, card_height(8));
  const float height = content_height(many);
  CHECK(height > kViewHigh);
  const float lastRow = flow(many).back().top + row_line(7) - kPitch / 2.0f;
  const float top = pm::ui::scroll_top_needed(height, kViewHigh, 0.0f, lastRow, kPitch);
  CHECK(lastRow >= top && lastRow + kPitch <= top + kViewHigh);
}

// The player speed rows step the game's own speed multiplier (LocalPCPlayer.SetPlayerSpeedMultiplier, the
// call a Monkey Paw wish makes). The step and its bounds are the menu's, not the game's: a developer wants
// room to run, and a multiplier of 0 would leave the row with no way back up.
void the_new_rows_refuse_for_what_the_game_lacks() {
  Can c;
  c.sandbox = true;
  CHECK(refuse(Action::ResetPlayerSpeed, c) == Why::NoPlayer);
  c.player = true;
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

void the_hooks_answer_the_switches() {
  // God mode refuses only the local investigator's kill.
  CHECK(god_refuses_kill(true, true));
  CHECK(!god_refuses_kill(true, false));
  CHECK(!god_refuses_kill(false, true));
  // It also refuses a death started outside a hunt (a custom ghost's catch, the Monkey Paw, the Hanged Man), but never
  // the menu's own Kill you row.
  CHECK(god_refuses_death(true, true, false));
  CHECK(!god_refuses_death(true, true, true));
  CHECK(!god_refuses_death(true, false, false));
  CHECK(!god_refuses_death(false, true, false));
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
                         Action::ToggleEvents, Action::FireEvent, Action::FireState,
                         Action::GhostToMe, Action::UnlockDoor,
                         Action::EndContract, Action::Leave, Action::Quit, Action::SetFriendly,
                         Action::SetLoseEquipment,
                         Action::BumpEvidence, Action::BumpType, Action::ToggleFreeze,
                         Action::KillLocal, Action::SetGodMode,
                         Action::SetNoclip, Action::MeToGhost, Action::MeToGhostRoom,
                         Action::SetLockSanity, Action::SanityToThreshold, Action::OptionToggle,
                         Action::OptionChoice, Action::Slide,
                         Action::SetPauseWhenOpen, Action::SetEspGhost,
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
  CHECK(refuse(Action::SetLockSanity, menu) == Why::NoPlayer);
  CHECK(refuse(Action::Slide, menu) == Why::None);  // a slider row asks what its own number needs
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
  CHECK(refuse(Action::SetLockSanity, dead) == Why::None);  // sanity can still be held on a corpse
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

// The lock only puts the value back once the game has moved it by more than half a point, so the game's setter
// (which sends the value on) is not asked every frame.
void the_sanity_lock_waits_for_real_drift() {
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
}

// The ghost sight row (the user's ask, 2026-11-12: also line of sight from anywhere, like the Deogen): the setting
// cycles normal, nobody, everyone; "everyone" passes every living player as the Deogen's branch does, never a dead one.
void the_ghost_sight_setting_answers_the_sight_test() {
  CHECK(next_ghost_sight(kGhostSightNormal) == kGhostSightNobody);
  CHECK(next_ghost_sight(kGhostSightNobody) == kGhostSightEveryone);
  CHECK(next_ghost_sight(kGhostSightEveryone) == kGhostSightNormal);
  CHECK(Switches{}.ghostSight == kGhostSightNormal);
  CHECK(sight_answer(kGhostSightNormal, true, false) && !sight_answer(kGhostSightNormal, false, false));
  CHECK(!sight_answer(kGhostSightNobody, true, false));
  CHECK(sight_answer(kGhostSightEveryone, false, false));
  CHECK(!sight_answer(kGhostSightEveryone, false, true));  // the dead stay unseen
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

// The custom difficulty's options, each on its own row (2026-11-13), step on the contract's Difficulty alone.
void every_difficulty_option_needs_only_the_difficulty() {
  for (const Action a : {Action::OptionToggle, Action::OptionChoice}) {
    CHECK(refuse(a, Can{.sandbox = true}) == Why::NoDifficulty);
    CHECK(refuse(a, Can{.sandbox = true, .difficulty = true}) == Why::None);
  }
}

// The sanity rows that sit on the ghost's hunt line need the ghost (its line) and the investigator (their sanity).
void the_hunt_line_rows_need_the_ghost_and_the_investigator() {
  CHECK(refuse(Action::SanityToThreshold, Can{.sandbox = true}) == Why::NoContract);
  CHECK(refuse(Action::SanityToThreshold, Can{.sandbox = true, .contract = true}) == Why::NoGhost);
  CHECK(refuse(Action::SanityToThreshold, Can{.sandbox = true, .contract = true, .ghost = true}) == Why::NoPlayer);
  CHECK(refuse(Action::SanityToThreshold, Can{.sandbox = true, .contract = true, .ghost = true, .player = true}) ==
        Why::None);
  static_assert(static_cast<int>(Why::NoThreshold) == static_cast<int>(Why::DoorsShut) + 1,
                "menu.cpp indexes the reasons' text keys in Why order");
}

// The settings kept between runs (the user's report, 2026-11-13): what is written reads back the same, a line this
// build does not know is skipped, and a value out of range falls back rather than reaching the game.
void the_settings_read_back_what_was_written() {
  Switches s;
  s.godMode = true;
  s.debugHud = true;
  s.espDoors = true;
  s.lockSanity = true;
  s.lockedSanity = 37;
  s.ghostSight = kGhostSightEveryone;
  s.freeShop = true;
  Switches back;
  read_settings(settings_text(s), back);
  CHECK(back.godMode && back.debugHud && back.espDoors && back.lockSanity && back.freeShop);
  CHECK(back.lockedSanity == 37);
  CHECK(back.ghostSight == kGhostSightEveryone);
  CHECK(!back.noclip);  // what the game holds is never kept
  Switches odd;
  read_settings("lockedSanity=400\nghostSight=9\nsomethingNew=1\ngodMode=x\n", odd);
  CHECK(odd.lockedSanity == 100);
  CHECK(odd.ghostSight == kGhostSightNormal);
  CHECK(!odd.godMode);
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
  the_groups_fill_two_columns();
  the_new_rows_refuse_for_what_the_game_lacks();
  the_debug_read_out_is_a_switch_like_the_others();
  the_hooks_answer_the_switches();
  a_full_offline_contract_allows_everything();
  rows_refuse_for_what_the_contract_lacks();
  hunt_and_room_rows_follow_the_hunt();
  a_dead_investigator_cannot_be_killed_again();
  the_kill_row_refuses_everywhere_the_game_would_report();
  a_friendly_ghost_refuses_forced_hunts_and_switches_only_between_hunts();
  the_movement_rows_need_a_ghost_and_a_player();
  the_sanity_lock_waits_for_real_drift();
  the_new_switches_start_off();
  no_revive_reason_is_left();
  the_store_is_free_only_in_the_sandbox_and_the_save_stays();
  the_ghost_sight_setting_answers_the_sight_test();
  the_menu_never_stops_the_clock_dead();
  the_weather_and_type_rows_follow_the_contract();
  the_dummy_rows_need_a_contract_and_the_dummy();
  the_hunt_target_row_needs_a_hunt();
  every_difficulty_option_needs_only_the_difficulty();
  the_hunt_line_rows_need_the_ghost_and_the_investigator();
  the_settings_read_back_what_was_written();
  CHECK(refuse(Action::PickMod, Can{.sandbox = true}) == Why::None);  // the Loader page reads, anywhere
  // The objectives, the spawned item and the switches of this round.
  CHECK(refuse(Action::CompleteObjective, Can{.sandbox = true}) == Why::NoContract);
  CHECK(refuse(Action::SpawnItem, Can{.sandbox = true, .contract = true}) == Why::NoPlayer);
  CHECK(refuse(Action::CycleSightHold, Can{.sandbox = true}) == Why::None);
  CHECK(next_sight_hold(kSightGame) == kSightNone && next_sight_hold(kSightNone) == kSightFrozen &&
        next_sight_hold(kSightFrozen) == kSightGame);
  CHECK(!Switches{}.neverRunOut && Switches{}.sightHold == kSightGame);
  std::printf(g_failures ? "%d check(s) failed\n" : "all dev menu rule tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
