// Offline test of a mod's side of the host contract (core/mod_glue.h) over the fake host. Run: build\mod_glue_test.exe
#include "mod_glue.h"

#include <string>

#include "check.h"
#include "fake_game.h"

namespace {
namespace fake = pm::fake;

const PmModInfo kInfo{PMMOD_API_VERSION, "someone.glue", "Glue", 210, 0};

// A mod's message goes to the host's one popup, as it was given; before bind it goes nowhere.
void a_message_reaches_the_hosts_popup() {
  fake::reset();
  pm::mod::show_message("Before", "bind");
  CHECK(fake::messages().empty());
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  pm::mod::show_message("Your deception score: 2 of 3", "$120 and 300 XP: 1 killed, 1 misled");
  CHECK(fake::messages().size() == 1);
  if (fake::messages().empty()) return;
  CHECK(fake::messages()[0].first == "Your deception score: 2 of 3");
  CHECK(fake::messages()[0].second == "$120 and 300 XP: 1 killed, 1 misled");
}

}  // namespace

int main() {
  a_message_reaches_the_hosts_popup();
  return g_failures ? 1 : 0;
}
