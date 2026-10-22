// Offline tests for eight-player paging and the room limit (mods/bigger_lobbies/paging.h).
// Run: build\lobbies_test.exe
#include "paging.h"

#include "check.h"

using namespace pg::lobbies;

namespace {

void four_players_fit_one_page() {
  CHECK(pages(1) == 1 && pages(4) == 1 && pages(5) == 2 && pages(8) == 2);
  CHECK(on_page(0, 0) && on_page(3, 0) && !on_page(4, 0) && on_page(4, 1) && on_page(7, 1) && !on_page(3, 1));
  CHECK(tab_label(0, 4).empty());
  CHECK(tab_label(0, 6) == "Players 5-8 >" && tab_label(1, 6) == "< Players 1-4");
}

void the_monitor_turns_its_page() {
  AutoPager p;
  CHECK(p.page(6, 1000) == 0);
  CHECK(p.page(6, 1000 + kMonitorPageMs - 1) == 0);
  CHECK(p.page(6, 1000 + kMonitorPageMs) == 1);
  CHECK(p.page(6, 1000 + 2 * kMonitorPageMs) == 0);
  CHECK(p.page(3, 1000 + 3 * kMonitorPageMs) == 0);  // back to one page
  CHECK(p.page(5, 1000 + 3 * kMonitorPageMs + 10) == 0);
}

void the_room_takes_eight_only_in_private() {
  CHECK(room_limit(4, true, 3, false) == 8);
  CHECK(room_limit(4, false, 3, false) == 4);  // public rooms stay vanilla
  CHECK(room_limit(3, true, 3, true) == 3);    // the contract's lock to who is there
  CHECK(room_limit(4, true, 4, true) == 4);    // the same lock with four players
  CHECK(room_limit(4, true, 4, false) == 8);   // the lobby's own four, with four players in
}

}  // namespace

int main() {
  four_players_fit_one_page();
  the_monitor_turns_its_page();
  the_room_takes_eight_only_in_private();
  std::printf(g_failures ? "%d check(s) failed\n" : "all lobbies tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
