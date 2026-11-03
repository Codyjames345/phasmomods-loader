// Offline tests for what the ghost perceives (mods/playable_ghost/perceive.h). Run: build\perceive_test.exe
#include "perceive.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::perceive;

namespace {

void directions_read_from_where_the_ghost_looks() {
  CHECK(bearing(0, 0, 0, 0, 10) == "ahead, 10 m");
  CHECK(bearing(0, 0, 0, 0, -10) == "behind, 10 m");
  CHECK(bearing(0, 0, 0, 10, 0) == "to your right, 10 m");
  CHECK(bearing(0, 0, 0, -10, 0) == "to your left, 10 m");
  CHECK(bearing(0, 0, 90, 10, 0) == "ahead, 10 m");      // looking along +x
  CHECK(bearing(0, 0, -170, 0, 5) == "behind, 5 m");     // wraps around 180
  CHECK(bearing(5, 5, 0, 5.5f, 5.5f) == "right here");
  CHECK(bearing(0, 0, 0, 7.07f, 7.07f) == "ahead, 10 m");  // 45 degrees still counts as ahead
}

void alerts_say_what_happened() {
  Alert a;
  a.kind = kSalt;
  CHECK(alert_text(a) == "You walked through salt");
  a.kind = kOuija;
  CHECK(alert_text(a) == "The Ouija board spoke for you");
  a.text = "hide and seek";
  CHECK(alert_text(a) == "The Ouija board answered: \"hide and seek\"");
  a = {};
  a.kind = kTarot;
  a.arg = kDeath;
  CHECK(alert_text(a) == "The Death card: you are made to hunt");
  a.arg = kHermit;
  CHECK(alert_text(a).find("sent home") != std::string::npos);
  a.kind = kSmudged;
  CHECK(alert_text(a) == "You have been smudged");  // no timer, ever
  a.kind = 200;
  CHECK(alert_text(a).empty());
  CHECK(!tarot_affects_ghost(0) && !tarot_affects_ghost(6) && tarot_affects_ghost(kTower) && tarot_affects_ghost(kHermit));
}

void the_same_alert_twice_is_shown_once() {
  Recent r;
  CHECK(r.fresh(kCrucifix, 1000));
  CHECK(!r.fresh(kCrucifix, 1000 + kRepeatMs - 1));  // seen here and relayed by the host
  CHECK(r.fresh(kSalt, 1100));                       // another kind is another alert
  CHECK(r.fresh(kCrucifix, 1000 + kRepeatMs));
  r.clear();
  CHECK(r.fresh(kSalt, 1200));
}

}  // namespace

// The Yokai player hears a voice only where the game's hunt hears one for the Yokai: within its 2.5 m, on
// the floor of the ghost's room, never from outside the map; a player a Monkey Paw wish marked from 1000 m
// and any floor, but still not from outside the map.
void yokai_hears_as_the_games_hunt_does() {
  using pm::perceive::yokai_hears;
  CHECK(yokai_hears(2.5f, true, false, false) && yokai_hears(0.0f, true, false, false));
  CHECK(!yokai_hears(2.51f, true, false, false));  // farther than the Yokai's reach
  CHECK(!yokai_hears(1.0f, false, false, false));  // another floor, however close
  CHECK(!yokai_hears(1.0f, true, true, false));    // the truck
  CHECK(yokai_hears(1000.0f, false, false, true) && yokai_hears(40.0f, true, false, true));  // wished
  CHECK(!yokai_hears(1000.5f, false, false, true) && !yokai_hears(1.0f, true, true, true));
}

int main() {
  use_text({"mods/playable_ghost/i18n/default.json"});
  directions_read_from_where_the_ghost_looks();
  alerts_say_what_happened();
  the_same_alert_twice_is_shown_once();
  yokai_hears_as_the_games_hunt_does();
  std::printf(g_failures ? "%d check(s) failed\n" : "all perceive tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
