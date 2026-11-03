// Offline test of the start of a mod session (host/mod_session.cpp): only "I agree" makes the host's hooks for
// the mods, before the mods' mark, then starts the mods; a hook that cannot be made stops nothing.
// Run: build\mod_session_test.exe
#include <cstdio>
#include <string>
#include <vector>

#include "check.h"
#include "text_fixture.h"
#include "log.h"
#include "mod_session.h"

namespace {

std::vector<std::string> g_steps;
bool g_installWorks = true;
std::vector<std::string> g_log;

bool install() {
  g_steps.push_back("install");
  return g_installWorks;
}
size_t mark() {
  g_steps.push_back("mark");
  return 42;
}
void start() { g_steps.push_back("start"); }

void capture(const char* line) { g_log.push_back(line); }

const pm::mod_session::Steps kSteps{&install, &mark, &start};
bool g_requiredWorks = true;
bool required() {
  g_steps.push_back("required");
  return g_requiredWorks;
}
const pm::mod_session::Steps kWithRules{&install, &mark, &start, &required};

void reset() {
  g_steps.clear();
  g_log.clear();
  g_installWorks = true;
  g_requiredWorks = true;
}

// A declined (or not yet answered) session makes no hook and starts nothing.
void a_declined_session_makes_nothing() {
  for (const pm::consent::Answer answer : {pm::consent::Answer::Declined, pm::consent::Answer::Pending}) {
    reset();
    size_t modHooks = 7;
    CHECK(!pm::mod_session::begin(answer, kSteps, modHooks));
    CHECK(g_steps.empty());
    CHECK(modHooks == 7);
  }
}

// "I agree": the host's hooks, then the mark the mods' hooks are counted from, then the mods.
void an_agreed_session_hooks_then_marks_then_starts() {
  reset();
  size_t modHooks = 0;
  CHECK(pm::mod_session::begin(pm::consent::Answer::Agreed, kSteps, modHooks));
  CHECK((g_steps == std::vector<std::string>{"install", "mark", "start"}));
  CHECK(modHooks == 42);
  CHECK(g_log.empty());
}

// A host hook that cannot be made is logged; the mods still start.
void a_failed_hook_stops_nothing() {
  reset();
  g_installWorks = false;
  size_t modHooks = 0;
  CHECK(pm::mod_session::begin(pm::consent::Answer::Agreed, kSteps, modHooks));
  CHECK((g_steps == std::vector<std::string>{"install", "mark", "start"}));
  CHECK(g_log.size() == 1 && g_log[0].find("the mods start without them") != std::string::npos);
}

// The hooks the lobby's rules need come first, and without them no mod starts and no other hook is made: the rules could not
// hold. Declined or not yet answered, not even they are made.
void the_rules_hooks_come_first_and_stop_everything_without_them() {
  reset();
  size_t modHooks = 7;
  CHECK(pm::mod_session::begin(pm::consent::Answer::Agreed, kWithRules, modHooks));
  CHECK((g_steps == std::vector<std::string>{"required", "install", "mark", "start"}));
  reset();
  g_requiredWorks = false;
  modHooks = 7;
  CHECK(!pm::mod_session::begin(pm::consent::Answer::Agreed, kWithRules, modHooks));
  CHECK((g_steps == std::vector<std::string>{"required"}));
  CHECK(modHooks == 7);
  CHECK(g_log.size() == 1 && g_log[0].find("so no mod starts") != std::string::npos);
  for (const pm::consent::Answer answer : {pm::consent::Answer::Declined, pm::consent::Answer::Pending}) {
    reset();
    CHECK(!pm::mod_session::begin(answer, kWithRules, modHooks));
    CHECK(g_steps.empty());
  }
}

// The start gate: no notice without a mod to agree to, or on a build that lacks the join guard's entries (the suite's
// messages or the lobby's leave and kick). The line names each key that did not resolve.
void the_start_gate_declines_without_the_join_guard() {
  using pm::consent::Answer;
  reset();
  std::string cause = "left over";
  CHECK(pm::mod_session::opening(true, "", "", cause) == Answer::Pending);
  CHECK(g_log.empty() && cause.empty());
  CHECK(pm::mod_session::opening(false, "", "", cause) == Answer::Declined);
  CHECK(g_log.size() == 1 && g_log[0].find("no mods to start") != std::string::npos);
  CHECK(cause.empty());  // nothing to say to a player with no mods
  reset();
  CHECK(pm::mod_session::opening(true, "EventData.Code", "", cause) == Answer::Declined);
  CHECK(g_log.size() == 1 && g_log[0].find("did not resolve: EventData.Code") != std::string::npos);
  CHECK(cause == "This game build lacks one or more required functions, mods are disabled. Check %LOCALAPPDATA%\\PhasmoMods\\mods.log for details.");
  reset();
  CHECK(pm::mod_session::opening(true, "", "ServerManager.KickPlayer", cause) == Answer::Declined);
  CHECK(g_log.size() == 1 && g_log[0].find("did not resolve: ServerManager.KickPlayer") != std::string::npos);
  CHECK(!cause.empty());
  reset();
  CHECK(pm::mod_session::opening(true, "LoadBalancingClient.OnEvent", "ServerManager", cause) == Answer::Declined);
  CHECK(g_log.size() == 1 &&
        g_log[0].find("did not resolve: LoadBalancingClient.OnEvent, ServerManager") != std::string::npos);
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  pm::log::set_sink(&capture, "");
  a_declined_session_makes_nothing();
  an_agreed_session_hooks_then_marks_then_starts();
  a_failed_hook_stops_nothing();
  the_rules_hooks_come_first_and_stop_everything_without_them();
  the_start_gate_declines_without_the_join_guard();
  std::printf("mod_session: %d failures\n", g_failures);
  return g_failures ? 1 : 0;
}
