// Offline tests for the mod list formats (core/mod_list.h). Run: build\mod_list_test.exe
#include "mod_list.h"

#include "check.h"

using namespace pg::modlist;

namespace {

void reads_a_manifest() {
  Manifest m;
  std::string error;
  CHECK(parse_manifest("# comment\r\n id = playable-ghost \r\nname=Playable Ghost\r\ndll=PlayableGhost.dll\r\n"
                       "description=Play as the ghost\r\nunknown=ignored\r\n",
                       m, error));
  CHECK(m.id == "playable-ghost" && m.name == "Playable Ghost" && m.dll == "PlayableGhost.dll");
  CHECK(m.description == "Play as the ghost");
}

void rejects_unsafe_manifests() {
  Manifest m;
  std::string error;
  CHECK(!parse_manifest("name=A\ndll=a.dll\n", m, error) && error.find("id") != std::string::npos);
  CHECK(!parse_manifest("id=Upper\nname=A\ndll=a.dll\n", m, error));
  CHECK(!parse_manifest("id=a\ndll=a.dll\n", m, error) && error.find("name") != std::string::npos);
  for (const char* dll : {"..\\host.dll", "C:\\x.dll", "sub/x.dll", "x.exe", "x", ".dll"}) {
    error.clear();
    CHECK(!parse_manifest(std::string("id=a\nname=A\ndll=") + dll + "\n", m, error) && !error.empty());
  }
  CHECK(parse_manifest("id=a\nname=A\ndll=Upper.DLL\n", m, error));
}

void reads_and_writes_the_selection() {
  bool present = true;
  CHECK(parse_enabled("", present).empty() && !present);
  auto ids = parse_enabled("# x\nenabled= a , b-2,,Bad Id,c\n", present);
  CHECK(present && ids == (std::vector<std::string>{"a", "b-2", "c"}));
  ids = parse_enabled("enabled=\n", present);
  CHECK(present && ids.empty());
  ids = parse_enabled(write_enabled({"x", "y"}), present);
  CHECK(present && ids == (std::vector<std::string>{"x", "y"}));
  CHECK(enabled(ids, "y") && !enabled(ids, "z"));
}

void chooses_the_saved_mods_or_all_of_them() {
  const std::vector<std::string> installed = {"a", "b"};
  CHECK(chosen("", installed) == installed);  // nothing saved: every mod
  CHECK(chosen("enabled=b\n", installed) == std::vector<std::string>{"b"});
  CHECK(chosen("enabled=\n", installed).empty());  // an empty choice stays empty
}

void a_new_mod_stays_off_after_a_saved_choice() {
  CHECK(chosen("enabled=ghost-tells\n", {"ghost-tells", "new-mod"}) == std::vector<std::string>{"ghost-tells"});
}

Manifest manifest(const char* id, const char* required = "") {
  Manifest m;
  m.id = id;
  m.name = id;
  m.dll = "x.dll";
  m.required = required;
  return m;
}

void requires_is_parsed() {
  Manifest m;
  std::string error;
  CHECK(parse_manifest("id=custom-ghosts\nname=Custom Ghosts\ndll=a.dll\nrequires=spectral-flame\n", m, error));
  CHECK(m.required == "spectral-flame");
  CHECK(parse_manifest("id=a\nname=A\ndll=a.dll\n", m, error) && m.required.empty());
  error.clear();
  CHECK(!parse_manifest("id=a\nname=A\ndll=a.dll\nrequires=Bad Id\n", m, error) && error.find("requires") != std::string::npos);
  error.clear();
  CHECK(!parse_manifest("id=a\nname=A\ndll=a.dll\nrequires=a\n", m, error) && !error.empty());  // not itself
}

void requirements_follow_a_toggle() {
  const std::vector<Manifest> all = {manifest("playable-ghost"), manifest("custom-ghosts", "spectral-flame"),
                                     manifest("spectral-flame")};
  using Ids = std::vector<std::string>;
  // Switching on a mod switches on what it needs.
  CHECK(switched({"playable-ghost"}, all, "custom-ghosts", true) == (Ids{"playable-ghost", "custom-ghosts", "spectral-flame"}));
  CHECK(switched({"spectral-flame"}, all, "custom-ghosts", true) == (Ids{"spectral-flame", "custom-ghosts"}));
  // Switching off a mod switches off what needs it; the rest stays.
  CHECK(switched({"playable-ghost", "custom-ghosts", "spectral-flame"}, all, "spectral-flame", false) ==
        (Ids{"playable-ghost"}));
  CHECK(switched({"custom-ghosts", "spectral-flame"}, all, "custom-ghosts", false) == (Ids{"spectral-flame"}));
  // Repeats and unknown ids change nothing.
  CHECK(switched({"spectral-flame"}, all, "spectral-flame", true) == (Ids{"spectral-flame"}));
  CHECK(switched({"spectral-flame"}, all, "unknown", false) == (Ids{"spectral-flame"}));
}

void start_order_puts_requirements_first() {
  const std::vector<Manifest> all = {manifest("custom-ghosts", "spectral-flame"), manifest("replay"),
                                     manifest("spectral-flame")};
  CHECK(start_order({"custom-ghosts", "replay", "spectral-flame"}, all) ==
        (std::vector<std::string>{"spectral-flame", "custom-ghosts", "replay"}));
}

void blocked_by_missing_requirement() {
  const std::vector<Manifest> all = {manifest("custom-ghosts", "spectral-flame"), manifest("spectral-flame"),
                                     manifest("loop-a", "loop-b"), manifest("loop-b", "loop-a")};
  CHECK(start_order({"custom-ghosts", "replay"}, all) == (std::vector<std::string>{"replay"}));
  CHECK(start_order({"loop-a", "loop-b"}, all).empty());  // a cycle can never be met
}

void a_saved_choice_gains_its_requirements() {
  // A choice saved before a mod gained a requirement (or before the requirement existed) still starts it.
  const std::vector<Manifest> all = {manifest("playable-ghost"), manifest("custom-ghosts", "spectral-flame"),
                                     manifest("spectral-flame")};
  CHECK(with_requirements({"playable-ghost", "custom-ghosts"}, all) ==
        (std::vector<std::string>{"playable-ghost", "custom-ghosts", "spectral-flame"}));
  CHECK(with_requirements({"spectral-flame"}, all) == std::vector<std::string>{"spectral-flame"});
  CHECK(with_requirements({}, all).empty());
}

void a_switch_says_what_else_it_changed() {
  std::vector<Manifest> all = {manifest("custom-ghosts", "spectral-flame"), manifest("spectral-flame"), manifest("other", "spectral-flame")};
  all[0].name = "Custom Ghosts";
  all[1].name = "Spectral Flame";
  all[2].name = "Other";
  CHECK(switch_note(all, {}, {"custom-ghosts", "spectral-flame"}, "custom-ghosts", true) ==
        "Custom Ghosts needs Spectral Flame, so they are switched on together.");
  CHECK(switch_note(all, {"custom-ghosts", "spectral-flame"}, {}, "spectral-flame", false) ==
        "Custom Ghosts needs Spectral Flame, so they are switched off together.");
  CHECK(switch_note(all, {"custom-ghosts", "spectral-flame", "other"}, {}, "spectral-flame", false) ==
        "Custom Ghosts, Other need Spectral Flame, so they are switched off together.");
  CHECK(switch_note(all, {"spectral-flame"}, {}, "spectral-flame", false).empty());  // only itself
}

void a_requirement_must_be_running() {
  CHECK(requirement_met("", {}));                                   // needs nothing
  CHECK(requirement_met("spectral-flame", {"spectral-flame", "x"}));
  CHECK(!requirement_met("spectral-flame", {"x"}));                 // chosen but failed to start, or absent
}

void reads_the_session_file() {
  const auto paths = parse_session("C:\\a\\one.dll\r\n\r\nC:\\b c\\two.dll\n");
  CHECK(paths == (std::vector<std::string>{"C:\\a\\one.dll", "C:\\b c\\two.dll"}));
  CHECK(parse_session("").empty());
}

}  // namespace

int main() {
  reads_a_manifest();
  rejects_unsafe_manifests();
  reads_and_writes_the_selection();
  chooses_the_saved_mods_or_all_of_them();
  a_new_mod_stays_off_after_a_saved_choice();
  reads_the_session_file();
  requires_is_parsed();
  requirements_follow_a_toggle();
  start_order_puts_requirements_first();
  blocked_by_missing_requirement();
  a_saved_choice_gains_its_requirements();
  a_requirement_must_be_running();
  a_switch_says_what_else_it_changed();
  return g_failures ? 1 : 0;
}
