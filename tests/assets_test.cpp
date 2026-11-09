// Offline test of a mod finding its own files (sdk/api/assets.h, core/mod_assets.h). Run: build\assets_test.exe
#include "assets.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "check.h"
#include "mod_assets.h"

namespace {
namespace fs = std::filesystem;

fs::path this_module() {
  wchar_t path[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  return n && n < MAX_PATH ? fs::path(path) : fs::path();
}

// The folder is named after the DLL copy by replacing its extension, as the manifest and the string table are.
void the_folder_is_named_after_the_copy() {
  CHECK(pm::modassets::beside_copy(L"C:\\x\\bin\\Mod-42-7-0.dll") == fs::path(L"C:\\x\\bin\\Mod-42-7-0.assets"));
  CHECK(pm::modassets::beside_copy(L"Mod-42-7-0.dll") == fs::path(L"Mod-42-7-0.assets"));
}

// The helper finds the folder beside the running module, and is empty when the launcher made none.
void the_helper_finds_the_folder_beside_the_running_module() {
  const fs::path self = this_module();
  CHECK(!self.empty());
  const fs::path folder = pm::modassets::beside_copy(self);
  std::error_code error;
  fs::remove_all(folder, error);
  CHECK(pm::api::assets_folder().empty());  // a mod with no assets has none
  fs::create_directories(folder / "squatter", error);
  CHECK(!error);
  CHECK(pm::api::assets_folder() == folder);
  fs::remove_all(folder, error);
  CHECK(pm::api::assets_folder().empty());
  // A file of that name is not a folder.
  { std::ofstream(folder).put('x'); }
  CHECK(pm::api::assets_folder().empty());
  fs::remove(folder, error);
}

}  // namespace

int main() {
  the_folder_is_named_after_the_copy();
  the_helper_finds_the_folder_beside_the_running_module();
  return g_failures ? 1 : 0;
}
