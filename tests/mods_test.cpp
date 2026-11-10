// Offline tests for which mods the host finds (host/mods.cpp find), that it loads no DLL until a mod starts, which versions
// of the host contract it accepts, and which mods it starts. Run: build\mods_test.exe
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "hooks.h"
#include "consent_text.h"
#include "log.h"
#include "mod_session.h"
#include "mods.h"
#include "quiet_load.h"

#include "check.h"
#include "text_fixture.h"

namespace fs = std::filesystem;

namespace il2cpp {
void log_exception(const char*, const void*) {}  // what the host's containment logs a managed exception with
}  // namespace il2cpp

namespace {

void write(const fs::path& file, const std::string& text) {
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  std::ofstream(file, std::ios::binary) << text;
}

std::vector<std::string> g_lines;
void capture(const char* line) { g_lines.emplace_back(line); }

bool logged(const std::string& text) {
  for (const std::string& l : g_lines)
    if (l.find(text) != std::string::npos) return true;
  return false;
}

// Same major and a minor no newer than the host's: accepted. Anything else: refused.
void api_versions() {
  constexpr uint32_t host = (2u << 16) | 3u;
  CHECK(pm_api_compatible(host, (2u << 16) | 3u));
  CHECK(pm_api_compatible(host, (2u << 16) | 0u));
  CHECK(!pm_api_compatible(host, (2u << 16) | 4u));
  CHECK(!pm_api_compatible(host, (1u << 16) | 0u));
  CHECK(!pm_api_compatible(host, (3u << 16) | 0u));
  CHECK(!pm_api_compatible(host, 1u));  // version 1 had no major number
  CHECK(pm_api_compatible(PMMOD_API_VERSION, PMMOD_API_VERSION));
}

// The fake mod's manifest: it says id "fake-mod", the old format's flat id, so `id` differs only when a test asks.
pm::modlist::Manifest fake_manifest(const std::string& id = "fake-mod") {
  pm::modlist::Manifest m;
  m.id = id;
  m.name = "Fake Mod";
  m.dll = "fake.dll";
  m.oldFormat = true;
  return m;
}
pm::mods::Installed fake_install(const char* dll, const std::string& id = "fake-mod") { return {{{dll, fake_manifest(id)}}, {}}; }

// The game entries a fake host has resolved: only those named in g_resolved.
std::vector<std::string> g_resolved;
const PmGameEntry kEntry{PM_ENTRY_METHOD};
const PmGameEntry* game_entry(const char* key) {
  for (const std::string& k : g_resolved)
    if (k == key) return &kEntry;
  return nullptr;
}

// The fake mod (tests/fixtures/fake_mod) requires Camera and GhostAI.UnAppear. It stays off while the
// host lacks one of them, and the log names what it lacks.
void mod_with_unresolved_requirement_stays_off(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD));
  CHECK(pm::mods::installed().size() == 1);
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;

  g_resolved = {"Camera"};
  g_lines.clear();
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::installed().size() == 1 &&
        pm::mods::installed()[0].entries == (std::vector<std::string>{"Camera", "GhostAI.UnAppear"}));
  CHECK(pm::mods::started_ids().empty());
  CHECK(logged("mods: fake-mod stays off: it needs game entries this game build lacks (GhostAI.UnAppear)"));
  CHECK(pm::mods::off_reason("fake-mod") == "Needs GhostAI.UnAppear, which this game version lacks");
  g_resolved = {};
  g_lines.clear();
  pm::mods::start(&api, {"fake-mod"});
  CHECK(logged("mods: fake-mod stays off: it needs game entries this game build lacks (Camera, GhostAI.UnAppear)"));
  CHECK(pm::mods::off_reason("fake-mod") == "Needs Camera and 1 more, which this game version lacks");

  // Every entry resolved, but one is a method whose calls the host could not track: the mod stays off.
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  g_lines.clear();
  pm::mods::Group lost;
  lost.untracked = [](const char* key) { return std::string(key) == "GhostAI.UnAppear"; };
  pm::mods::start(&api, {"fake-mod"}, lost);
  CHECK(pm::mods::started_ids().empty());
  CHECK(logged("mods: fake-mod stays off: it needs calls of game methods the host could not track (GhostAI.UnAppear)"));

  pm::mods::Group tracked;
  tracked.untracked = [](const char*) { return false; };
  pm::mods::start(&api, {"fake-mod"}, tracked);
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A hook a mod made, and content it gave, as a restart sees them.
#pragma optimize("", off)
int target_answer(int v) { return v + 1; }
#pragma optimize("", on)
using Target = int (*)(int);
Target g_target_original = nullptr;
int detour_target(int v) { return g_target_original(v) + 100; }
size_t g_mark = 0;
int g_content = 0;
std::vector<std::string> g_order;
bool g_runningWhileWithdrawing = false, g_stoppedBeforeCleanup = false;
void begin_mod(const std::string& = {}) {
  CHECK(pm::hooks::create(reinterpret_cast<void*>(&target_answer), reinterpret_cast<void*>(&detour_target),
                          reinterpret_cast<void**>(&g_target_original), "target"));
  CHECK(pm::hooks::enable_all());
  ++g_content;
}

// The lobby's rules run the mods without a suspended one, and all of them on leaving, with no game restart: its hooks and
// content come out, the others stay, and they go back. What it saved of the player's choice does not change.
void suspend_and_resume_in_a_room(const fs::path& data) {
  CHECK(pm::hooks::init());
  g_mark = pm::hooks::mark();
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  const std::vector<std::string> everyMod{"fake-mod"};
  CHECK(pm::mods::save_chosen(everyMod));
  pm::mods::Group group;
  group.begin = &begin_mod;
  pm::mods::Restart restart;
  restart.withdraw = [] {
    g_order.push_back("withdraw");
    g_runningWhileWithdrawing = !pm::mods::started_ids().empty();
    g_content = 0;
  };
  restart.stopped = [] {
    g_order.push_back("stopped");
    g_stoppedBeforeCleanup = pm::mods::started_ids().empty();
    pm::hooks::rollback_to(g_mark);
  };
  pm::mods::start(&api, everyMod, group);
  CHECK(!pm::mods::started_ids().empty() && pm::mods::started_builds().size() == 1);
  CHECK(target_answer(1) == 102 && g_content == 1);

  // Suspended: everything it gave is gone.
  pm::mods::restart(&api, {}, group, restart);
  CHECK((g_order == std::vector<std::string>{"withdraw", "stopped"}) && g_runningWhileWithdrawing && g_stoppedBeforeCleanup);
  CHECK(pm::mods::started_ids().empty() && pm::mods::started_builds().empty());
  CHECK(target_answer(1) == 2 && g_content == 0);
  CHECK(pm::mods::chosen() == everyMod);

  // Resumed: it all goes back, afresh.
  pm::mods::restart(&api, everyMod, group, restart);
  CHECK(!pm::mods::started_ids().empty() && pm::mods::started_builds().size() == 1);
  CHECK(target_answer(1) == 102 && g_content == 1);
  pm::mods::stop_all();
  pm::hooks::rollback_to(g_mark);
  CHECK(target_answer(1) == 2);
}

// The host refused content the mod registered during its start (a ghost while custom content is off, say): the host keeps
// the mod off itself, whatever its pmmod_start answered, so no mod has to check what its registrations returned. Its
// hooks and content from that start come out, and the log says why.
std::string g_refusal;
bool g_endedStarted = true;
void refused_content_keeps_the_mod_off(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  g_mark = pm::hooks::mark();
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::Group group;
  group.begin = &begin_mod;
  group.refusal = [] { return g_refusal; };
  group.end = [](bool started) {
    g_endedStarted = started;
    if (!started) g_content = 0;
  };
  g_refusal = "the ghost \"a.b\" is refused: custom content is off: its hooks on the game could not be made";
  g_content = 0;
  g_lines.clear();
  pm::mods::start(&api, {"fake-mod"}, group);
  CHECK(pm::mods::started_ids().empty() && !pm::mods::started("fake-mod"));
  // The manifest's name, the Mods panel's, whether or not the mod runs (the host contract's mod_name).
  CHECK(pm::mods::name_of("fake-mod") && std::string(pm::mods::name_of("fake-mod")) == "Fake Mod");
  CHECK(!pm::mods::name_of("not-installed") && !pm::mods::name_of(nullptr));
  CHECK(!g_endedStarted && g_content == 0 && target_answer(1) == 2);
  CHECK(logged("mods: fake-mod stays off: the host refused its content (the ghost \"a.b\" is refused: custom content is off"));
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why");

  g_refusal.clear();  // nothing refused: it starts
  pm::mods::start(&api, {"fake-mod"}, group);
  CHECK(pm::mods::started("fake-mod") && g_endedStarted && target_answer(1) == 102);
  pm::mods::stop_all();
  pm::hooks::rollback_to(g_mark);
  pm::log::set_sink(nullptr, nullptr);
}

// Any failure of the host's during a mod's start keeps it off, whatever its pmmod_start answered: a hook that could not be
// made, and a host call that failed (here mod_started, standing in for one), each logged with why.
bool g_failCall = false;
bool failing_mod_started(const char*) {
  if (g_failCall) pm::mods::start_error("the service a.b is already registered");
  return false;
}
void host_failures_in_a_start_keep_the_mod_off(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  g_mark = pm::hooks::mark();
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &failing_mod_started;
  g_resolved = {"Camera", "GhostAI.UnAppear"};

  // A hook made in the start (the group's begin stands in for the mod) has no target.
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD));
  pm::mods::Group group;
  group.begin = [](const std::string& id) {
    CHECK(id == "fake-mod");
    CHECK(!pm::hooks::create(nullptr, reinterpret_cast<void*>(&detour_target), nullptr, "lost"));
  };
  g_lines.clear();
  pm::mods::start(&api, {"fake-mod"}, group);
  CHECK(!pm::mods::started("fake-mod"));
  CHECK(logged("mods: fake-mod stays off: a hook could not be made (lost has no target)"));
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why");
  pm::mods::start(&api, {"fake-mod"});  // a start with no failure, after one: it starts
  CHECK(pm::mods::started("fake-mod"));
  pm::mods::stop_all();

  // A host call that failed during the start, of a mod that asks the host whether another mod runs.
  pm::mods::set_installed(data, {{{PM_FAKE_MOD_OPTIONAL, fake_manifest("fake-optional")}}, {}});
  g_failCall = true;
  g_lines.clear();
  pm::mods::start(&api, {"fake-optional"});
  CHECK(!pm::mods::started("fake-optional"));
  CHECK(logged("mods: fake-optional stays off: a call to the host failed (the service a.b is already registered)"));
  g_failCall = false;
  pm::mods::start_error("outside a start");  // counts for nothing
  pm::mods::start(&api, {"fake-optional"});
  CHECK(pm::mods::started("fake-optional"));
  pm::mods::stop_all();
  pm::hooks::rollback_to(g_mark);
  pm::log::set_sink(nullptr, nullptr);
}

// Content the host drops once every mod has started keeps its owner off: the mods start again without it, and again while
// that drops more (a ghost whose evidence came from the mod just left off), until nothing is dropped.
void dropped_content_keeps_its_owner_off(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;  // fake-optional asks it as it starts
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::set_installed(data, {{{PM_FAKE_MOD_OPTIONAL, fake_manifest("fake-optional")}, {PM_FAKE_MOD, fake_manifest()}}, {}});
  const std::vector<std::string> both{"fake-mod", "fake-optional"};
  pm::mods::start(&api, both);
  CHECK(pm::mods::started("fake-mod") && pm::mods::started("fake-optional"));
  int round = 0;
  std::vector<std::vector<std::string>> restarted;
  g_lines.clear();
  pm::mods::settle(
      both,
      [&]() -> std::vector<pm::mods::Dropped> {
        ++round;
        if (round == 1)  // one owned by nobody, one by a mod not running: neither is anyone's to keep off
          return {{"fake-mod", "the ghost \"fake-mod.a\" is dropped: no mod that started registered its evidence \"x.e\""},
                  {"", "rules on \"x.g\" are dropped: no mod that started registered that ghost"},
                  {"nobody", "the ghost \"nobody.n\" is dropped"}};
        if (round == 2) return {{"fake-optional", "the ghost \"fake-optional.b\" is dropped: its evidence went with fake-mod"}};
        return {};
      },
      [&](const std::vector<std::string>& ids) {
        restarted.push_back(ids);
        pm::mods::restart(&api, ids, {}, {});
      });
  CHECK(round == 3 && restarted.size() == 2);
  CHECK(restarted.size() == 2 && restarted[0] == std::vector<std::string>{"fake-optional"} && restarted[1].empty());
  CHECK(!pm::mods::started("fake-mod") && !pm::mods::started("fake-optional"));
  CHECK(logged("mods: fake-mod stays off: the host dropped its content (the ghost \"fake-mod.a\" is dropped"));
  CHECK(logged("mods: fake-optional stays off: the host dropped its content (the ghost \"fake-optional.b\""));
  CHECK(!logged("mods: nobody stays off"));
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why" &&
        pm::mods::off_reason("fake-optional") == "It did not start. mods.log says why");
  pm::mods::start(&api, both);  // a later start tries them again
  CHECK(pm::mods::started("fake-mod") && pm::mods::off_reason("fake-mod").empty());
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A mod built against an older minor version of contract 2 (`minor`, from `dll`) loads and starts in
// the 2.3 host: the contract only grew at the end.
void older_minor_still_loads(const fs::path& data, const char* dll, uint32_t minor) {
  CHECK(PMMOD_API_VERSION == ((2u << 16) | 3u));
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, fake_install(dll));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::installed().size() == 1 && pm::mods::installed()[0].info && pm::mods::installed()[0].info->apiVersion == ((2u << 16) | minor));
  CHECK(!logged("built for another version"));
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

void mod_built_for_2_0_still_loads(const fs::path& data) { older_minor_still_loads(data, PM_FAKE_MOD_2_0, 0); }

// Contract 2.2 added the content functions at the end; a 2.1 mod never reaches them.
void mod_built_for_2_1_still_loads(const fs::path& data) { older_minor_still_loads(data, PM_FAKE_MOD_2_1, 1); }

// Contract 2.3 added PM_GHOST_NO_MIMIC_COPY; a 2.2 mod (the released contract) still loads.
void mod_built_for_2_2_still_loads(const fs::path& data) { older_minor_still_loads(data, PM_FAKE_MOD_2_2, 2); }

// A mod exporting the new names loads and logs nothing about them.
void loads_mod_with_new_exports(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  CHECK(!logged("exports the names of contract 2.1"));
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A mod built before the rename exports the old names: it loads and starts, and the log says so once, however often it
// starts again (its DLL is read once).
void loads_mod_with_old_exports_and_logs_once(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, fake_install(PM_FAKE_MOD_OLD_EXPORTS));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  pm::mods::stop_all();
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  pm::mods::stop_all();
  size_t told = 0;
  for (const std::string& line : g_lines)
    if (line.find("fake-mod exports the names of contract 2.1 and before") != std::string::npos) ++told;
  CHECK(told == 1);
  pm::log::set_sink(nullptr, nullptr);
}

// A manifest's id must be the one the DLL compiled in: nothing else may answer to it. The mod stays off, its row says
// why, and the mods after it still start. That is known only once its DLL has loaded.
void manifest_id_must_match_dll(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::Installed both = fake_install(PM_FAKE_MOD, "someone.else");
  both.found.push_back({PM_FAKE_MOD_2_0, fake_manifest()});
  pm::mods::set_installed(data, both);
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"someone.else", "fake-mod"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  CHECK(pm::mods::off_reason("someone.else") == "manifest and DLL disagree on the mod id");
  CHECK(logged("mods: someone.else stays off: the manifest says someone.else, the DLL says fake-mod"));
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A DLL that is not a mod, and one that is not there, stay off with a reason and stop nothing. Refused at load, a mod that
// requires it reads as needing a mod that did not start, not as one that is not installed, and is never loaded itself.
// A refusal lasts: the next start does not try that DLL again, and its row says why even with the mod unticked.
void a_dll_that_cannot_run_is_refused_with_a_reason(const fs::path& data, const fs::path& root) {
  const fs::path notADll = root / "copies" / "NotADll.dll";
  const fs::path neverLoaded = root / "copies" / "NeedsTheBadOne.dll";
  write(notADll, "not a library");
  std::error_code error;
  CHECK(fs::copy_file(PM_FAKE_MOD_OPTIONAL, neverLoaded, fs::copy_options::overwrite_existing, error));
  pm::modlist::Manifest bad = fake_manifest("not-a-dll");
  bad.name = "Bad Mod";
  pm::modlist::Manifest dependent = fake_manifest("fake-optional");
  dependent.dependencies = {{"not-a-dll", "", true}};
  pm::mods::Installed all = fake_install(PM_FAKE_MOD);
  all.found.insert(all.found.begin(), {notADll, bad});
  all.found.push_back({root / "copies" / "IsNotThere.dll", fake_manifest("not-there")});
  all.found.push_back({neverLoaded, dependent});
  all.refused.push_back({"from-the-launcher", "Its id is bad."});
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, all);
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  const std::vector<std::string> every{"not-a-dll", "fake-mod", "not-there", "fake-optional"};
  pm::mods::start(&api, every);
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  CHECK(pm::mods::refused().size() == 1 && pm::mods::refused()[0].name == "from-the-launcher");
  CHECK(pm::mods::off_reason("not-a-dll") == "DLL could not be loaded");
  CHECK(logged("mods: not-a-dll stays off: its DLL could not be loaded (Windows error 193)"));
  CHECK(pm::mods::off_reason("not-there") == "DLL could not be loaded");
  CHECK(pm::mods::off_reason("fake-optional") == "Depends on Bad Mod, which contains an error. Please check mods.log.");
  CHECK(GetModuleHandleW(neverLoaded.c_str()) == nullptr);
  pm::mods::stop_all();
  // The file is now a good DLL, and the refusal still stands until the game restarts: loaded or not, it is not tried again.
  CHECK(fs::copy_file(PM_FAKE_MOD_PLAIN, notADll, fs::copy_options::overwrite_existing, error));
  pm::mods::start(&api, every);
  CHECK(GetModuleHandleW(notADll.c_str()) == nullptr);
  CHECK(pm::mods::off_reason("not-a-dll") == "DLL could not be loaded");
  pm::mods::stop_all();
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::off_reason("not-a-dll") == "DLL could not be loaded");  // unticked, the row still says why
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// One id found twice is one mod: the second stays off, named, before any DLL could say so.
void a_repeated_id_is_refused_from_the_manifests(const fs::path& data) {
  pm::log::set_sink(&capture, "");
  pm::mods::Installed twice = fake_install(PM_FAKE_MOD);
  pm::modlist::Manifest again = fake_manifest();
  again.name = "Fake Again";
  twice.found.push_back({PM_FAKE_MOD_2_0, again});
  pm::mods::set_installed(data, twice);
  CHECK(pm::mods::installed().size() == 1 && pm::mods::installed()[0].manifest.name == "Fake Mod");
  CHECK(pm::mods::refused().size() == 1 && pm::mods::refused()[0].name == "Fake Again" &&
        pm::mods::refused()[0].why == "Repeats the id of another mod.");
  pm::log::set_sink(nullptr, nullptr);
}

// A fresh copy of a fixture DLL, at a path no test has loaded: whether this path is mapped says what the host did.
fs::path fresh_copy(const fs::path& root, const char* from, const std::wstring& name) {
  const fs::path copy = root / "copies" / (name + L".dll");
  std::error_code error;
  fs::create_directories(copy.parent_path(), error);
  CHECK(fs::copy_file(from, copy, fs::copy_options::overwrite_existing, error));
  return copy;
}
bool mapped(const fs::path& dll) { return GetModuleHandleW(dll.c_str()) != nullptr; }

pm::modlist::Manifest named_manifest(const std::string& id, const std::string& name) {
  pm::modlist::Manifest m = fake_manifest(id);
  m.name = name;
  return m;
}

// Before the player agrees the host reads manifests alone. Finding the mods, listing them for the notice and the panel,
// reading the choice and asking about them all map no DLL.
void no_mod_dll_loads_before_agreement(const fs::path& data, const fs::path& root) {
  const fs::path package = root / "before-agreement";
  std::error_code error;
  fs::create_directories(package / "mods" / "fake", error);
  const fs::path dll = package / "mods" / "fake" / "Before.dll";
  CHECK(fs::copy_file(PM_FAKE_MOD, dll, fs::copy_options::overwrite_existing, error));
  write(package / "mods" / "fake" / "manifest.json",
        R"({"id":"me.fake","name":"Fake","author":"Me","version":"1.0.0","description":"D","dll":"Before.dll"})");
  pm::log::set_sink(&capture, "");
  const pm::mods::Installed found = pm::mods::find(data, package, 77);
  CHECK(found.found.size() == 1);
  CHECK(!mapped(dll));
  pm::mods::set_installed(data, found);
  CHECK(!mapped(dll));
  CHECK(pm::mods::installed().size() == 1 && pm::mods::installed()[0].manifest.name == "Fake");
  CHECK(pm::mods::installed().size() == 1 && !pm::mods::installed()[0].info);
  CHECK(pm::mods::manifests().size() == 1 && pm::mods::notice_list().size() == 1);
  CHECK(pm::mods::off_reason("me.fake").empty() && !pm::mods::started("me.fake"));
  CHECK(pm::mods::started_ids().empty() && pm::mods::started_builds().empty());
  pm::mods::chosen();
  CHECK(!mapped(dll));
  pm::mods::stop_all();  // nothing to stop, and it loads nothing
  pm::mods::tick_all();
  CHECK(!mapped(dll));
  pm::log::set_sink(nullptr, nullptr);
}

// "Play without mods", or a notice that could not be shown, or one not answered yet, loads none. "I agree" loads them.
void declined_loads_no_mod_dll(const fs::path& data, const fs::path& root) {
  const fs::path dll = fresh_copy(root, PM_FAKE_MOD, L"Declined");
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{dll, fake_manifest()}}, {}});
  static PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  const pm::mod_session::Steps steps{[] { return true; }, [] { return size_t{0}; }, [] { pm::mods::start(&api, {"fake-mod"}); }};
  size_t hooks = 0;
  for (const pm::consent::Answer no : {pm::consent::Answer::Declined, pm::consent::Answer::Pending}) {
    CHECK(!pm::mod_session::begin(no, steps, hooks));
    CHECK(!mapped(dll) && pm::mods::started_ids().empty());
  }
  CHECK(pm::mod_session::begin(pm::consent::Answer::Agreed, steps, hooks));
  CHECK(mapped(dll) && pm::mods::started("fake-mod"));
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// Only what starts loads. A mod switched on later loads then, and switching it off again leaves it stopped, still mapped.
// Dependencies the plan cannot meet load nothing: a mod left off by its manifest never reaches its DLL.
void unticked_mod_is_not_loaded_until_switched_on(const fs::path& data, const fs::path& root) {
  const fs::path ticked = fresh_copy(root, PM_FAKE_MOD, L"Ticked");
  const fs::path later = fresh_copy(root, PM_FAKE_MOD_PLAIN, L"Later");
  const fs::path needsAbsent = fresh_copy(root, PM_FAKE_MOD_OPTIONAL, L"NeedsAbsent");
  pm::modlist::Manifest lacking = fake_manifest("fake-optional");
  lacking.dependencies = {{"nobody-has-this", "", true}};
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{ticked, fake_manifest()}, {later, fake_manifest("fake-plain")}, {needsAbsent, lacking}}, {}});
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-mod", "fake-optional"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  CHECK(mapped(ticked) && !mapped(later) && !mapped(needsAbsent));
  CHECK(pm::mods::off_reason("fake-optional") == "Depends on nobody-has-this");
  pm::mods::restart(&api, {"fake-mod", "fake-plain"}, {}, {});
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-mod", "fake-plain"}));
  CHECK(mapped(later) && !mapped(needsAbsent));
  pm::mods::restart(&api, {"fake-mod"}, {}, {});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  CHECK(mapped(later));  // a loaded mod cannot be unloaded safely: it is only stopped
  pm::mods::restart(&api, {"fake-mod", "fake-plain"}, {}, {});
  CHECK(pm::mods::started("fake-plain"));  // and starts again from the same DLL
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A mod left off keeps its place in the saved choice: only the player's own Apply changes that (host/mods_panel.h).
void a_chosen_mod_that_could_not_run_stays_chosen(const fs::path& data, const fs::path& root) {
  const fs::path lacks = fresh_copy(root, PM_FAKE_MOD, L"StaysChosen");
  const fs::path plain = fresh_copy(root, PM_FAKE_MOD_PLAIN, L"StaysChosenPlain");
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{lacks, fake_manifest()}, {plain, fake_manifest("fake-plain")}}, {}});
  const std::vector<std::string> choice{"fake-mod", "fake-plain"};
  CHECK(pm::mods::save_chosen(choice));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {};
  pm::mods::start(&api, pm::mods::chosen());
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-plain"});
  CHECK(pm::mods::chosen() == choice);
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A game update that takes an entry from one mod: it stays off, named on its row and in the log, a mod that requires it
// reads as needing a mod that did not start, and the mods whose entries all resolve run.
void unresolved_mod_stays_off_and_is_named(const fs::path& data, const fs::path& root) {
  const fs::path lacks = fresh_copy(root, PM_FAKE_MOD, L"Lacks");
  const fs::path plain = fresh_copy(root, PM_FAKE_MOD_PLAIN, L"Plain");
  const fs::path dependent = fresh_copy(root, PM_FAKE_MOD_OPTIONAL, L"Dependent");
  pm::modlist::Manifest flame = named_manifest("fake-mod", "Spectral Flame");
  pm::modlist::Manifest ghosts = named_manifest("fake-optional", "Custom Ghosts");
  ghosts.dependencies = {{"fake-mod", "", true}};
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, {{{lacks, flame}, {dependent, ghosts}, {plain, fake_manifest("fake-plain")}}, {}});
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;
  g_resolved = {"Camera"};
  pm::mods::start(&api, {"fake-mod", "fake-optional", "fake-plain"});
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-plain"});
  CHECK(pm::mods::off_reason("fake-mod") == "Needs GhostAI.UnAppear, which this game version lacks");
  CHECK(logged("mods: fake-mod stays off: it needs game entries this game build lacks (GhostAI.UnAppear)"));
  CHECK(pm::mods::off_reason("fake-optional") == "Depends on Spectral Flame, which contains an error. Please check mods.log.");
  CHECK(logged("mods: fake-optional stays off: it needs Spectral Flame, which stays off: Needs GhostAI.UnAppear, which this game version lacks"));
  CHECK(pm::mods::off_reason("fake-plain").empty());
  CHECK(!mapped(dependent));  // it never reached its DLL
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// The notice names a mod as recording voice on its manifest's word, and has no DLL to ask.
void notice_shows_voice_from_the_manifest(const fs::path& data, const fs::path& root) {
  const fs::path voice = fresh_copy(root, PM_FAKE_MOD_VOICE, L"VoiceForNotice");
  const fs::path silent = fresh_copy(root, PM_FAKE_MOD_PLAIN, L"SilentForNotice");
  pm::modlist::Manifest says = named_manifest("fake-voice", "Says So");
  says.recordsVoice = true;
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{voice, says}, {silent, named_manifest("fake-plain", "Says Nothing")}}, {}});
  const pm::consent::Notice n = pm::consent::notice(pm::mods::notice_list());
  CHECK(n.body.find("Says So keeps what other players say") != std::string::npos);
  CHECK(n.body.find("Says Nothing keeps") == std::string::npos);
  CHECK(n.body.find("Says So, Says Nothing") != std::string::npos);
  CHECK(!mapped(voice) && !mapped(silent));
  // A manifest that says nothing shows no such paragraph, whatever its DLL would have said.
  pm::mods::set_installed(data, {{{voice, named_manifest("fake-voice", "Says Nothing")}}, {}});
  CHECK(pm::consent::notice(pm::mods::notice_list()).body.find("Voices are recorded") == std::string::npos);
  CHECK(!mapped(voice));
  pm::log::set_sink(nullptr, nullptr);
}

// The player agreed on the manifest's word, so a DLL that says it records voice when its manifest does not stays off, with
// a reason, and is never started. With the manifest saying so it runs. A manifest that says so over a DLL that does not
// only warns too much, so it runs too.
void dll_recording_voice_against_its_manifest_stays_off(const fs::path& data, const fs::path& root) {
  const fs::path voice = fresh_copy(root, PM_FAKE_MOD_VOICE, L"VoiceAgainst");
  const fs::path dependent = fresh_copy(root, PM_FAKE_MOD_OPTIONAL, L"NeedsVoice");
  pm::modlist::Manifest needs = fake_manifest("fake-optional");
  needs.dependencies = {{"fake-voice", "", true}};
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, {{{voice, named_manifest("fake-voice", "Voice")}, {dependent, needs}}, {}});
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-voice", "fake-optional"});
  CHECK(pm::mods::started_ids().empty());
  CHECK(mapped(voice));  // it took loading it to find out, and it stays mapped
  CHECK(pm::mods::off_reason("fake-voice") == "Undeclared voice recording, please check with the author.");
  CHECK(logged("mods: fake-voice stays off: its DLL records voice, which its manifest does not say"));
  CHECK(pm::mods::off_reason("fake-optional") == "Depends on Voice, which contains an error. Please check mods.log.");
  CHECK(!mapped(dependent));
  CHECK(pm::mods::started_builds().empty());

  pm::modlist::Manifest says = named_manifest("fake-voice", "Voice");
  says.recordsVoice = true;
  pm::modlist::Manifest overWarns = fake_manifest("fake-plain");
  overWarns.recordsVoice = true;
  pm::mods::set_installed(data, {{{voice, says}, {fresh_copy(root, PM_FAKE_MOD_PLAIN, L"OverWarns"), overWarns}}, {}});
  pm::mods::start(&api, {"fake-voice", "fake-plain"});
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-voice", "fake-plain"}));
  CHECK(pm::mods::off_reason("fake-voice").empty());
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A mod whose host contract is another version says so on its row, once its DLL has told it.
void a_mod_for_another_contract_gets_its_reason_once_loaded(const fs::path& data, const fs::path& root) {
  const fs::path old = fresh_copy(root, PM_FAKE_MOD_1, L"OldContract");
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{old, fake_manifest()}}, {}});
  CHECK(!mapped(old) && pm::mods::off_reason("fake-mod").empty());
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  pm::mods::start(&api, {"fake-mod"});
  CHECK(mapped(old) && pm::mods::started_ids().empty());
  CHECK(pm::mods::off_reason("fake-mod") == "Mod was built for another version of the PhasmoMods Loader.");
  pm::log::set_sink(nullptr, nullptr);
}

// Two mods with one channel: the first to start has it, and the other says so. Each order of the choice decides which.
void a_repeated_channel_is_judged_in_start_order(const fs::path& data, const fs::path& root) {
  const fs::path plain = fresh_copy(root, PM_FAKE_MOD_PLAIN, L"ChannelPlain");
  const fs::path same = fresh_copy(root, PM_FAKE_MOD_SAME_CHANNEL, L"ChannelSame");
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  pm::log::set_sink(&capture, "");
  for (const bool plainFirst : {true, false}) {
    g_lines.clear();
    pm::mods::set_installed(data, {{{plain, fake_manifest("fake-plain")}, {same, fake_manifest("fake-same")}}, {}});
    const std::string first = plainFirst ? "fake-plain" : "fake-same", second = plainFirst ? "fake-same" : "fake-plain";
    pm::mods::start(&api, {first, second});
    CHECK(pm::mods::started_ids() == std::vector<std::string>{first});
    CHECK(pm::mods::off_reason(second) == "Repeats the network channel of a mod already loaded");
    CHECK(logged("mods: " + second + " stays off: it repeats channel 203 of a mod already loaded"));
    pm::mods::stop_all();
  }
  pm::log::set_sink(nullptr, nullptr);
}

// Windows answers a bad image with a blocking dialog unless the loading thread's error mode says not to, so it says so while
// a mod's DLL loads, and only then: a corrupt or wrong-architecture mod is refused with a reason (the case above), not a
// dialog over the game. A dialog cannot be asserted, so this reads the mode from inside the load, in the fake mod's DllMain,
// on a fresh copy so that the DLL really does load here. The thread's mode, and whatever else it held, comes back after.
void a_mod_loads_with_no_error_dialog(const fs::path& data, const fs::path& root) {
  const fs::path copy = root / "copies" / "QuietFake.dll";
  std::error_code error;
  fs::create_directories(copy.parent_path(), error);
  CHECK(fs::copy_file(PM_FAKE_MOD, copy, fs::copy_options::overwrite_existing, error));
  DWORD before = 0;
  CHECK(SetThreadErrorMode(SEM_NOGPFAULTERRORBOX, &before));
  pm::log::set_sink(&capture, "");
  pm::mods::set_installed(data, {{{copy, fake_manifest()}}, {}});
  CHECK(GetModuleHandleW(copy.c_str()) == nullptr);  // not yet
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::mods::start(&api, {"fake-mod"});
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
  CHECK(GetThreadErrorMode() == SEM_NOGPFAULTERRORBOX);  // restored
  HMODULE module = GetModuleHandleW(copy.c_str());
  CHECK(module != nullptr);
  const auto modeAtLoad = module ? reinterpret_cast<DWORD (*)()>(GetProcAddress(module, "fake_mod_error_mode_at_load")) : nullptr;
  CHECK(modeAtLoad != nullptr);
  if (modeAtLoad) {
    CHECK((modeAtLoad() & SEM_FAILCRITICALERRORS) != 0 && (modeAtLoad() & SEM_NOOPENFILEERRORBOX) != 0);
    CHECK((modeAtLoad() & SEM_NOGPFAULTERRORBOX) != 0);  // what the thread already held stays on
  }
  // A failed load restores it too, and leaves its error for the reason.
  const fs::path notADll = root / "copies" / "NotADllEither.dll";
  write(notADll, "not a library");
  CHECK(pm::dll::load_mod(notADll.c_str()) == nullptr);
  CHECK(GetLastError() == ERROR_BAD_EXE_FORMAT);
  CHECK(GetThreadErrorMode() == SEM_NOGPFAULTERRORBOX);
  SetThreadErrorMode(before, nullptr);
}

// A folder with only a mod.ini loads, with its flat id, its version unknown, and one log line saying so.
void old_mod_ini_still_loads(const fs::path& data, const fs::path& root) {
  const fs::path package = root / "old-package";
  std::error_code error;
  fs::create_directories(package / "mods" / "old", error);
  fs::copy_file(PM_FAKE_MOD, package / "mods" / "old" / "FakeMod.dll", fs::copy_options::overwrite_existing, error);
  CHECK(!error);
  write(package / "mods" / "old" / "mod.ini", "id=fake-mod\nname=Fake Old\ndll=FakeMod.dll\ndescription=old\nrequires=ghost-tells\n");
  const pm::mods::Installed installed = pm::mods::find(data, package, 42);
  CHECK(installed.found.size() == 1 && installed.refused.empty());
  CHECK(installed.found.size() == 1 && installed.found[0].manifest.oldFormat && installed.found[0].manifest.version.empty());
  CHECK(installed.found.size() == 1 && installed.found[0].manifest.dependencies.size() == 1 &&
        installed.found[0].manifest.dependencies[0].id == "phasmomods.ghost-tells");
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, installed);
  CHECK(pm::mods::installed().size() == 1 && pm::mods::installed()[0].manifest.name == "Fake Old");
  CHECK(logged("mods: fake-mod has a mod.ini, the old format; its version shows as unknown"));
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::modlist::Manifest noRequirement = installed.found[0].manifest;  // its requires names a mod not here
  noRequirement.dependencies.clear();
  pm::mods::set_installed(data, {{{installed.found[0].dll, noRequirement}}, {}});
  pm::mods::start(&api, {"fake-mod"});
  CHECK(logged("mods: loaded fake-mod (version unknown) (Fake Old"));
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// An unknown field is said once, as the manifest is taken; it refuses nothing.
void unknown_fields_are_logged_once(const fs::path& data) {
  pm::mods::Installed one = fake_install(PM_FAKE_MOD);
  one.found[0].manifest.unknownFields = {"later", "another"};
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, one);
  CHECK(pm::mods::installed().size() == 1);
  int said = 0;
  for (const std::string& l : g_lines) said += l.find("does not know, which it ignores: later, another") != std::string::npos;
  CHECK(said == 1);
  pm::log::set_sink(nullptr, nullptr);
}

template <class Fn>
Fn export_of(const char* dll, const char* name) {
  HMODULE module = GetModuleHandleA(dll);
  return module ? reinterpret_cast<Fn>(GetProcAddress(module, name)) : nullptr;
}

size_t line_of(const std::string& text) {
  for (size_t i = 0; i < g_lines.size(); ++i)
    if (g_lines[i].find(text) != std::string::npos) return i;
  return g_lines.size();
}

// The second fake mod uses the first when it is there, and asks the host in its pmmod_start whether it started. The host
// starts the first before it whichever order the choice holds, so the answer is already final; one that did not start is
// false, and a required one that did not start takes the other with it, saying why on its row.
void mod_started_answers_for_optional_dependency(const fs::path& data) {
  pm::modlist::Manifest user = fake_manifest("fake-optional");
  user.name = "Fake Optional";
  user.dependencies = {{"fake-mod", "", false}};
  pm::mods::set_installed(data, {{{PM_FAKE_MOD_OPTIONAL, user}, {PM_FAKE_MOD, fake_manifest()}}, {}});
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  const std::vector<std::string> both{"fake-optional", "fake-mod"};
  pm::mods::start(&api, both);
  // The DLLs load with that start.
  const auto fail = export_of<void (*)(bool)>(PM_FAKE_MOD, "fake_mod_fail_start");
  const auto asked = export_of<int (*)()>(PM_FAKE_MOD_OPTIONAL, "fake_mod_asked");
  CHECK(fail && asked);
  if (!fail || !asked) return;
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-mod", "fake-optional"}));
  CHECK(asked() == 1);
  CHECK(pm::mods::started("fake-mod") && pm::mods::started("fake-optional"));
  CHECK(!pm::mods::started("nobody") && !pm::mods::started(nullptr));
  // Stopped in reverse start order: the mod that uses the other stops first.
  g_lines.clear();
  pm::mods::stop_all();
  CHECK(line_of("mods: fake-optional stopped") < line_of("mods: fake-mod stopped") && line_of("mods: fake-mod stopped") < g_lines.size());
  CHECK(!pm::mods::started("fake-mod"));

  // Its optional dependency does not start: it starts all the same, and hears so.
  fail(true);
  pm::mods::start(&api, both);
  CHECK(pm::mods::started_ids() == std::vector<std::string>{"fake-optional"} && asked() == 0);
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why" && pm::mods::off_reason("fake-optional").empty());
  pm::mods::stop_all();

  // A restart of the mods asks only about those it starts, as a room's does: what it never tried keeps its reason.
  fail(true);
  pm::mods::start(&api, {"fake-mod"});
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why");
  pm::mods::restart(&api, {"fake-optional"}, {}, {});
  CHECK(pm::mods::started("fake-optional"));
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why");
  pm::mods::restart(&api, {}, {}, {});
  CHECK(pm::mods::off_reason("fake-mod") == "It did not start. mods.log says why");
  fail(false);  // and one that is tried and starts has no reason left
  pm::mods::restart(&api, {"fake-mod"}, {}, {});
  CHECK(pm::mods::started("fake-mod") && pm::mods::off_reason("fake-mod").empty());
  pm::mods::stop_all();
  fail(true);

  // Required, it stays off, is never started, and its row says why.
  user.dependencies[0].required = true;
  pm::mods::set_installed(data, {{{PM_FAKE_MOD_OPTIONAL, user}, {PM_FAKE_MOD, fake_manifest()}}, {}});
  g_lines.clear();
  pm::mods::start(&api, both);
  CHECK(pm::mods::started_ids().empty());
  CHECK(pm::mods::off_reason("fake-optional") == "Depends on Fake Mod, which contains an error. Please check mods.log.");
  CHECK(logged("mods: fake-optional stays off: it needs Fake Mod, which stays off: It did not start. mods.log says why"));
  fail(false);
  pm::mods::start(&api, both);
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-mod", "fake-optional"}));
  CHECK(pm::mods::off_reason("fake-optional").empty());
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// A DLL that throws out of what the host asks before it starts is refused with a reason, and the others carry on.
void a_dll_that_throws_when_asked_is_refused(const fs::path& data) {
  pm::mods::Found first{PM_FAKE_MOD_THROWS, fake_manifest()};
  first.manifest.id = "fake-throws";
  first.manifest.name = "Throws";
  pm::mods::Found second{PM_FAKE_MOD_THROWS_REQUIRES, fake_manifest()};
  second.manifest.id = "fake-throws";
  second.manifest.name = "Throws too";
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::set_installed(data, {{first, {PM_FAKE_MOD, fake_manifest()}}, {}});
  pm::mods::start(&api, {"fake-throws", "fake-mod"});
  CHECK(pm::mods::off_reason("fake-throws") == "DLL failed when performing handshake. Check %LOCALAPPDATA%\\PhasmoMods\\mods.log for details.");
  CHECK(logged("mods: fake-throws stays off: its pmmod_info threw"));
  CHECK(!pm::mods::started("fake-throws") && pm::mods::started("fake-mod"));
  pm::mods::stop_all();
  pm::mods::set_installed(data, {{second}, {}});
  pm::mods::start(&api, {"fake-throws"});
  CHECK(pm::mods::off_reason("fake-throws") == "DLL failed when querying dependencies. Check %LOCALAPPDATA%\\PhasmoMods\\mods.log for details.");
  CHECK(logged("mods: fake-throws stays off: its pmmod_requires threw"));
  CHECK(!pm::mods::started("fake-throws"));
  pm::log::set_sink(nullptr, nullptr);
}

// A dependency that gets no place in the order may start after the mod that uses it, so the answer in pmmod_start is not final
// and the docs say not to keep it. Both ways that can happen, and that a mod already stopped answers false to one stopping
// after it.
void mod_started_can_turn_true_after_pmmod_start(const fs::path& data) {
  const auto asked = export_of<int (*)()>(PM_FAKE_MOD_OPTIONAL, "fake_mod_asked");
  const auto askNow = export_of<int (*)()>(PM_FAKE_MOD_OPTIONAL, "fake_mod_ask_now");
  const auto askedAtStop = export_of<int (*)()>(PM_FAKE_MOD_OPTIONAL, "fake_mod_asked_at_stop");
  CHECK(asked && askNow && askedAtStop);
  if (!asked || !askNow || !askedAtStop) return;
  PmHostApi api{};
  api.version = PMMOD_API_VERSION;
  api.game_entry = &game_entry;
  api.mod_started = &pm::mods::started;
  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pm::log::set_sink(&capture, "");
  const auto load = [&](pm::modlist::Dependency uses, std::vector<pm::modlist::Dependency> usedNeeds) {
    pm::modlist::Manifest user = fake_manifest("fake-optional");
    user.dependencies = {std::move(uses)};
    pm::modlist::Manifest used = fake_manifest();
    used.version = "1.0.0";
    used.oldFormat = false;
    used.dependencies = std::move(usedNeeds);
    pm::mods::set_installed(data, {{{PM_FAKE_MOD_OPTIONAL, user}, {PM_FAKE_MOD, used}}, {}});
  };

  // The edge that would close a cycle: fake-mod requires fake-optional, which uses it optionally. So fake-optional goes first,
  // hears false, and then fake-mod runs. It stops last, by when fake-mod has stopped.
  load({"fake-mod", "", false}, {{"fake-optional", "", true}});
  pm::mods::start(&api, {"fake-mod", "fake-optional"});
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-optional", "fake-mod"}));
  CHECK(asked() == 0 && askNow() == 1);
  pm::mods::stop_all();
  CHECK(askedAtStop() == 0);
  CHECK(askNow() == 0);

  // Below its minimum version: no place in the order, so the choice decides, and the answer with it.
  load({"fake-mod", "2.0.0", false}, {});
  pm::mods::start(&api, {"fake-optional", "fake-mod"});
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-optional", "fake-mod"}));
  CHECK(asked() == 0 && askNow() == 1);
  pm::mods::stop_all();
  pm::mods::start(&api, {"fake-mod", "fake-optional"});
  CHECK(pm::mods::started_ids() == (std::vector<std::string>{"fake-mod", "fake-optional"}));
  CHECK(asked() == 1);
  pm::mods::stop_all();
  pm::log::set_sink(nullptr, nullptr);
}

// The host reads the mods folder beside itself as the install folder, except when it is a copy in the data folder's bin,
// which has none: a mod there that claims the project's author stays off, with a row saying why.
void a_mods_folder_beside_a_copied_host_is_elsewhere(const fs::path& data, const fs::path& root) {
  const std::string tells = R"({"id":"phasmomods.ghost-tells","name":"Ghost Tells","author":"PhasmoMods","version":"1.0.0",)"
                            R"("description":"D","dll":"GhostTells.dll"})";
  write(data / "bin" / "mods" / "tells" / "manifest.json", tells);
  pm::log::set_sink(&capture, "");
  g_lines.clear();
  pm::mods::Installed installed = pm::mods::find(data, data / "bin", 77);
  CHECK(installed.found.empty() && installed.refused.size() == 1 && installed.refused[0].name == "tells");
  CHECK(installed.refused.size() == 1 && installed.refused[0].why.find("reserved for bundled mods") != std::string::npos);
  CHECK(logged("mods: tells stays off: id uses the author"));
  // Beside a host in its package, the same mod is the project's own.
  write(root / "installed" / "mods" / "tells" / "manifest.json", tells);
  installed = pm::mods::find(data, root / "installed", 77);
  CHECK(installed.found.size() == 1 && installed.refused.empty() && installed.found[0].manifest.id == "phasmomods.ghost-tells");
  pm::log::set_sink(nullptr, nullptr);
  std::error_code error;
  fs::remove_all(data / "bin" / "mods", error);
}

// Where the mods folder beside the host counts as being: an error that cannot rule out a real bin counts as bin, so a mods
// folder the host cannot judge is never trusted. No bin at all is the usual first run.
void a_folder_that_cannot_be_told_from_bin_counts_as_elsewhere(const fs::path& root) {
  using pm::modlist::Place;
  const fs::path package = root / "where" / "package", bin = root / "where" / "bin", gone = root / "where" / "gone";
  std::error_code error;
  fs::create_directories(package, error);
  CHECK(pm::mods::place_beside(package, bin) == Place::InstallFolder);  // no bin yet
  fs::create_directories(bin, error);
  CHECK(pm::mods::place_beside(package, bin) == Place::InstallFolder);
  CHECK(pm::mods::place_beside(bin, bin) == Place::Elsewhere);
  CHECK(pm::mods::place_beside(bin / ".." / "bin", bin) == Place::Elsewhere);
  CHECK(pm::mods::place_beside(gone, bin) == Place::Elsewhere);  // the comparison fails, and a bin is there
  CHECK(pm::mods::place_beside(gone, gone) == Place::InstallFolder);  // neither exists: nothing to find anyway
}

// A host whose own folder cannot be read finds nothing, and never reads a relative "mods" folder in the game's folder.
void a_host_with_no_folder_finds_nothing(const fs::path& data, const fs::path& root) {
  const fs::path game = root / "game-folder";
  write(game / "mods" / "a" / "manifest.json",
        R"({"id":"me.a","name":"N","author":"Me","version":"1.0.0","description":"D","dll":"A.dll"})");
  std::error_code error;
  const fs::path before = fs::current_path(error);
  fs::current_path(game, error);
  CHECK(!error);
  CHECK(pm::mods::find(data, game, 77).found.size() == 1);  // the same folder, named, has it
  const pm::mods::Installed none = pm::mods::find(data, fs::path{}, 77);
  CHECK(none.found.empty() && none.refused.empty());
  fs::current_path(before, error);
}

std::string read_source(const std::string& relative) {
  std::ifstream in(fs::path(PM_SOURCE_DIR) / relative, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// Custom Ghosts cannot run without Spectral Flame, and Playable Ghost uses Custom Ghosts when it is there; the loader
// enforces both from these declarations alone.
void bundled_mods_declare_their_dependencies() {
  pm::modlist::Manifest m;
  std::string why;
  CHECK(pm::modlist::parse_manifest_json(read_source("mods/custom_ghosts/manifest.json"), m, why));
  CHECK(m.dependencies.size() == 1 && m.dependencies[0].id == "phasmomods.spectral-flame" &&
        m.dependencies[0].minimumVersion == "1.0.0" && m.dependencies[0].required);
  CHECK(pm::modlist::parse_manifest_json(read_source("mods/playable_ghost/manifest.json"), m, why));
  CHECK(m.dependencies.size() == 1 && m.dependencies[0].id == "phasmomods.custom-ghosts" &&
        m.dependencies[0].minimumVersion.empty() && !m.dependencies[0].required);
  for (const char* folder : {"spectral_flame", "ghost_tells", "emotes"}) {
    CHECK(pm::modlist::parse_manifest_json(read_source(std::string("mods/") + folder + "/manifest.json"), m, why));
    CHECK(m.dependencies.empty() && m.incompatibilities.empty());
  }
}

// Each bundled mod's manifest says what its DLL does about voice, or the host would keep it off.
void bundled_manifests_say_what_their_dlls_do_about_voice() {
  for (const char* folder : {"custom_ghosts", "playable_ghost", "spectral_flame", "ghost_tells", "emotes"}) {
    pm::modlist::Manifest m;
    std::string why;
    CHECK(pm::modlist::parse_manifest_json(read_source(std::string("mods/") + folder + "/manifest.json"), m, why));
    const bool flagged = read_source(std::string("mods/") + folder + "/mod.cpp").find("PM_MOD_RECORDS_VOICE") != std::string::npos;
    CHECK(m.recordsVoice == flagged);
    CHECK(m.recordsVoice == (std::string(folder) == "custom_ghosts"));
  }
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  api_versions();
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  const fs::path root = fs::path(temporary) / (L"pg-mods-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const fs::path data = root / "data", package = root / "package";
  const auto json = [](const std::string& fields) {
    return R"({"name":"N","author":"Me","version":"1.0.0","description":"two",)" + fields + "}";
  };
  write(package / "mods" / "b" / "manifest.json", json(R"("id":"me.second","dll":"Second.dll","dependencies":[{"id":"me.first"}])"));
  write(package / "mods" / "a" / "manifest.json", json(R"("id":"me.first","dll":"First.dll")"));
  write(package / "mods" / "c" / "mod.ini", "id=Bad\nname=Bad\ndll=Bad.dll\n");
  write(package / "mods" / "d" / "manifest.json", std::string(pm::modlist::kMaxManifestBytes + 1, ' '));
  write(package / "mods" / "e" / "readme.txt", "no mod here");
  const fs::path copy = root / "copies" / "First-42-7-0.dll";
  write(fs::path(copy).replace_extension(".json"), json(R"("id":"me.first","dll":"First.dll","dependencies":[{"id":"me.other"}])"));
  const fs::path oldCopy = root / "copies" / "Old-42-7-0.dll";
  write(fs::path(oldCopy).replace_extension(".ini"), "id=old\nname=Old\ndll=Old.dll\n");
  const fs::path badCopy = root / "copies" / "Broken-42-7-0.dll";
  write(fs::path(badCopy).replace_extension(".json"), "[]");
  const fs::path bareCopy = root / "copies" / "Bare-42-7-0.dll";
  write(data / "bin" / "session-42.txt", copy.string() + "\n" + oldCopy.string() + "\n" + badCopy.string() + "\n" + bareCopy.string() + "\n?launcher-refused\tIts id is bad.\n!builds\t" + (root / "package" / "builds").string() + "\n");

  // Found beside a package without the launcher: every valid mod, in folder order, whatever
  // launcher.ini says (it decides which start, not which are listed). The session file is not the
  // launcher's list for this host, so it is ignored (and kept).
  write(data / "launcher.ini", "enabled=second\n");
  auto installed = pm::mods::find(data, package, 42);
  auto* found = &installed.found;
  CHECK(found->size() == 2 && (*found)[0].dll == package / "mods" / "a" / "First.dll" && (*found)[1].dll == package / "mods" / "b" / "Second.dll");
  CHECK(found->size() == 2 && (*found)[0].manifest.id == "me.first" && (*found)[1].manifest.description == "two");
  CHECK(found->size() == 2 && (*found)[0].manifest.dependencies.empty() && (*found)[1].manifest.dependencies.size() == 1 &&
        (*found)[1].manifest.dependencies[0].id == "me.first");  // its requirement too
  // The bad mod.ini and the oversized manifest stay off, named; the folder with no mod in it is not a refusal.
  CHECK(installed.refused.size() == 2 && installed.refused[0].name == "c" && installed.refused[1].name == "d");
  CHECK(installed.refused.size() == 2 && installed.refused[1].why.find("64 KiB") != std::string::npos);
  CHECK(fs::exists(data / "bin" / "session-42.txt"));
  CHECK(installed.buildRecords == package / "builds");  // the checked-build records beside it

  // The launcher's copy of the host in bin runs exactly the list for its own process, read once, each
  // copy's manifest read from what was copied beside it. A copy with a bad manifest or none stays off, and
  // what the launcher itself refused arrives with it.
  installed = pm::mods::find(data, data / "bin", 42);
  found = &installed.found;
  CHECK(found->size() == 2 && (*found)[0].dll == copy && (*found)[0].manifest.dependencies.size() == 1 &&
        (*found)[0].manifest.dependencies[0].id == "me.other");
  CHECK(found->size() == 2 && (*found)[1].dll == oldCopy && (*found)[1].manifest.oldFormat);
  CHECK(installed.refused.size() == 3 && installed.refused[0].name == "Broken" && installed.refused[1].name == "Bare" &&
        installed.refused[2].name == "launcher-refused" && installed.refused[2].why == "Its id is bad.");
  CHECK(installed.buildRecords == root / "package" / "builds");  // the launcher names the records in the package
  CHECK(!fs::exists(data / "bin" / "session-42.txt"));
  CHECK(pm::mods::find(data, data / "bin", 42).found.empty());  // no list: the empty mods folder beside it
  CHECK(pm::mods::find(data, data / "bin", 43).found.empty());  // another process's list is never used

  // Nothing found: the saved choice is read as it is.
  pm::mods::read_installed(data.wstring());
  CHECK(pm::mods::chosen() == std::vector<std::string>{"second"});
  CHECK(pm::mods::save_chosen({"first", "second"}));
  CHECK(pm::mods::chosen() == (std::vector<std::string>{"first", "second"}));
  CHECK(pm::mods::started_ids().empty());  // nothing runs

  mod_with_unresolved_requirement_stays_off(data);
  suspend_and_resume_in_a_room(data);
  refused_content_keeps_the_mod_off(data);
  host_failures_in_a_start_keep_the_mod_off(data);
  dropped_content_keeps_its_owner_off(data);
  mod_built_for_2_0_still_loads(data);
  mod_built_for_2_1_still_loads(data);
  mod_built_for_2_2_still_loads(data);
  loads_mod_with_new_exports(data);
  loads_mod_with_old_exports_and_logs_once(data);
  manifest_id_must_match_dll(data);
  a_mod_loads_with_no_error_dialog(data, root);
  a_dll_that_cannot_run_is_refused_with_a_reason(data, root);
  a_repeated_id_is_refused_from_the_manifests(data);
  a_dll_that_throws_when_asked_is_refused(data);
  no_mod_dll_loads_before_agreement(data, root);
  declined_loads_no_mod_dll(data, root);
  unticked_mod_is_not_loaded_until_switched_on(data, root);
  unresolved_mod_stays_off_and_is_named(data, root);
  a_chosen_mod_that_could_not_run_stays_chosen(data, root);
  notice_shows_voice_from_the_manifest(data, root);
  dll_recording_voice_against_its_manifest_stays_off(data, root);
  a_mod_for_another_contract_gets_its_reason_once_loaded(data, root);
  a_repeated_channel_is_judged_in_start_order(data, root);
  bundled_manifests_say_what_their_dlls_do_about_voice();
  old_mod_ini_still_loads(data, root);
  unknown_fields_are_logged_once(data);
  mod_started_answers_for_optional_dependency(data);
  mod_started_can_turn_true_after_pmmod_start(data);
  bundled_mods_declare_their_dependencies();
  a_mods_folder_beside_a_copied_host_is_elsewhere(data, root);
  a_folder_that_cannot_be_told_from_bin_counts_as_elsewhere(root);
  a_host_with_no_folder_finds_nothing(data, root);

  fs::remove_all(root, error);
  return g_failures ? 1 : 0;
}
