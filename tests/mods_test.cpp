// Offline tests for which mod DLLs the host loads (host/mods.cpp find). Run: build\mods_test.exe
#include <windows.h>

#include <filesystem>
#include <fstream>

#include "mods.h"

#include "check.h"

namespace fs = std::filesystem;

namespace {

void write(const fs::path& file, const std::string& text) {
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  std::ofstream(file, std::ios::binary) << text;
}

}  // namespace

int main() {
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

  fs::remove_all(root, error);
  return g_failures ? 1 : 0;
}
