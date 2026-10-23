// Offline tests for which mod DLLs the host loads (host/mods.cpp find), which versions of the host
// contract it accepts, and which mods it starts. Run: build\mods_test.exe
#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "log.h"
#include "mods.h"

#include "check.h"

namespace fs = std::filesystem;

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
  CHECK(pg_api_compatible(host, (2u << 16) | 3u));
  CHECK(pg_api_compatible(host, (2u << 16) | 0u));
  CHECK(!pg_api_compatible(host, (2u << 16) | 4u));
  CHECK(!pg_api_compatible(host, (1u << 16) | 0u));
  CHECK(!pg_api_compatible(host, (3u << 16) | 0u));
  CHECK(!pg_api_compatible(host, 1u));  // version 1 had no major number
  CHECK(pg_api_compatible(PGMOD_API_VERSION, PGMOD_API_VERSION));
}

// The game entries a fake host has resolved: only those named in g_resolved.
std::vector<std::string> g_resolved;
const PgGameEntry kEntry{PG_ENTRY_METHOD};
const PgGameEntry* game_entry(const char* key) {
  for (const std::string& k : g_resolved)
    if (k == key) return &kEntry;
  return nullptr;
}

// The fake mod (tests/fixtures/fake_mod) requires Camera and GhostAI.UnAppear. It stays off while the
// host lacks one of them, and the log names what it lacks.
void mod_with_unresolved_requirement_stays_off(const fs::path& data) {
  pg::log::set_sink(&capture, "");
  pg::mods::load(data, {{PG_FAKE_MOD, "", ""}});
  CHECK(pg::mods::loaded().size() == 1);
  CHECK(pg::mods::loaded().size() == 1 &&
        pg::mods::loaded()[0].entries == (std::vector<std::string>{"Camera", "GhostAI.UnAppear"}));
  PgHostApi api{};
  api.version = PGMOD_API_VERSION;
  api.game_entry = &game_entry;

  g_resolved = {"Camera"};
  g_lines.clear();
  pg::mods::start(&api, {"fake-mod"});
  CHECK(pg::mods::started_ids().empty());
  CHECK(logged("mods: fake-mod needs game entries this game build lacks (GhostAI.UnAppear); it stays off"));

  g_resolved = {"Camera", "GhostAI.UnAppear"};
  pg::mods::start(&api, {"fake-mod"});
  CHECK(pg::mods::started_ids() == std::vector<std::string>{"fake-mod"});
  pg::mods::stop_all();
  pg::log::set_sink(nullptr, nullptr);
}

}  // namespace

int main() {
  api_versions();
  wchar_t temporary[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, temporary) > 0);
  const fs::path root = fs::path(temporary) / (L"pg-mods-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const fs::path data = root / "data", package = root / "package";
  write(package / "mods" / "b" / "mod.ini", "id=second\nname=Second\ndll=Second.dll\ndescription=two\nrequires=first\n");
  write(package / "mods" / "a" / "mod.ini", "id=first\nname=First\ndll=First.dll\n");
  write(package / "mods" / "c" / "mod.ini", "id=Bad\nname=Bad\ndll=Bad.dll\n");
  const fs::path copy = root / "copies" / "First-42.dll";
  write(fs::path(copy).replace_extension(".ini"), "id=first\nname=First\ndll=First.dll\ndescription=one\nrequires=other\n");
  write(data / "bin" / "session-42.txt", copy.string() + "\n");

  // Loaded beside a package without the launcher: every valid mod, in folder order, whatever
  // launcher.ini says (it decides which start, not which load). The session file is not the
  // launcher's list for this host, so it is ignored (and kept).
  write(data / "launcher.ini", "enabled=second\n");
  auto found = pg::mods::find(data, package, 42);
  CHECK(found.size() == 2 && found[0].dll == package / "mods" / "a" / "First.dll" && found[1].dll == package / "mods" / "b" / "Second.dll");
  CHECK(found.size() == 2 && found[0].description.empty() && found[1].description == "two");
  CHECK(found.size() == 2 && found[0].required.empty() && found[1].required == "first");  // its requirement too
  CHECK(fs::exists(data / "bin" / "session-42.txt"));

  // The launcher's copy of the host in bin runs exactly the list for its own process, read once, each
  // copy's description read from the mod.ini copied beside it.
  found = pg::mods::find(data, data / "bin", 42);
  CHECK(found.size() == 1 && found[0].dll == copy && found[0].description == "one" && found[0].required == "other");
  CHECK(!fs::exists(data / "bin" / "session-42.txt"));
  CHECK(pg::mods::find(data, data / "bin", 42).empty());  // no list: the empty mods folder beside it
  CHECK(pg::mods::find(data, data / "bin", 43).empty());  // another process's list is never used

  // Nothing loaded: the saved choice is read as it is.
  pg::mods::load(data.wstring());
  CHECK(pg::mods::chosen() == std::vector<std::string>{"second"});
  CHECK(pg::mods::save_chosen({"first", "second"}));
  CHECK(pg::mods::chosen() == (std::vector<std::string>{"first", "second"}));
  CHECK(pg::mods::started_allow_vanilla());  // nothing runs

  mod_with_unresolved_requirement_stays_off(data);

  fs::remove_all(root, error);
  return g_failures ? 1 : 0;
}
