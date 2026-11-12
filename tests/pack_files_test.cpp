// Offline tests for the files of a content pack (core/pack_files.h): the stand-in path its folder is known by,
// which paths are stand-ins, and what a pack copy's build covers and changes with. Run: build\pack_files_test.exe
#include <windows.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

#include "pack_files.h"

#include "check.h"

namespace fs = std::filesystem;

namespace {

fs::path g_root;  // this test's temp folder, made in main and removed at the end

void write(const fs::path& file, const std::string& text) {
  std::error_code error;
  fs::create_directories(file.parent_path(), error);
  std::ofstream(file, std::ios::binary) << text;
}

// A pack copy as the launcher puts it in bin (tools/inject.cpp): an empty "<stem>.pack" stand-in with its
// manifest as "<stem>.json", its string table as "<stem>.i18n.json" and its assets as "<stem>.assets\",
// where `assets` are paths with forward slashes. Returns the ".pack" path build_files and build_of take.
fs::path make_copy(const fs::path& dir, const std::wstring& stem,
                   const std::vector<std::pair<std::string, std::string>>& assets) {
  const fs::path pack = dir / (stem + L".pack");
  write(pack, "");
  write(dir / (stem + L".json"), R"({"id":"someone.pack","name":"Pack","author":"Someone","version":"1.0.0","description":"D","contentPackFor":{"id":"me.framework"}})");
  write(dir / (stem + L".i18n.json"), R"({"clip.one":"One"})");
  for (const auto& [name, content] : assets) write(dir / (stem + L".assets") / fs::u8path(name), content);
  return pack;
}

std::vector<std::string> names_of(const std::vector<std::pair<std::string, fs::path>>& files) {
  std::vector<std::string> names;
  for (const auto& [name, path] : files) names.push_back(name);
  return names;
}

// A build string: 16 lower-case hex digits.
bool build_shaped(const std::string& text) {
  if (text.size() != 16) return false;
  for (const char c : text)
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}

// The stand-in of a pack folder is named after the folder itself: no file anyone writes, the path every
// part of the loader knows a pack by.
void stand_in_names_the_pack_after_its_folder() {
  CHECK(pm::packfiles::stand_in(fs::path(L"C:\\m\\MyPack")) == fs::path(L"C:\\m\\MyPack\\MyPack.pack"));
  CHECK(pm::packfiles::stand_in(g_root / L"Packs" / L"Emote Pack") ==
        g_root / L"Packs" / L"Emote Pack" / L"Emote Pack.pack");
}

// The extension says a path is a pack stand-in, whatever its case; a DLL never is, and neither is a
// manifest, a string table or the folder itself.
void is_stand_in_ignores_case_and_refuses_dll() {
  CHECK(pm::packfiles::is_stand_in(L"C:\\m\\MyPack\\MyPack.pack"));
  CHECK(pm::packfiles::is_stand_in(L"C:\\m\\MyPack\\MyPack.PACK"));
  CHECK(pm::packfiles::is_stand_in(L"C:\\m\\bin\\Emote-42-7-0.Pack"));
  CHECK(!pm::packfiles::is_stand_in(L"C:\\m\\a\\Mod.dll"));
  CHECK(!pm::packfiles::is_stand_in(L"C:\\m\\bin\\Mod-42-7-0.DLL"));
  CHECK(!pm::packfiles::is_stand_in(L"C:\\m\\MyPack\\manifest.json"));
  CHECK(!pm::packfiles::is_stand_in(L"C:\\m\\MyPack\\i18n\\default.json"));
  CHECK(!pm::packfiles::is_stand_in(g_root));  // a folder, with no extension at all
}

// A pack copy's build covers its manifest, its string table and every file of its assets, named as in the
// pack's own folder with forward slashes, sorted by that name.
void build_files_lists_manifest_text_and_assets_sorted() {
  const fs::path dir = g_root / L"listed";
  const fs::path pack = make_copy(dir, L"X", {{"b.txt", "bee"}, {"a/c.txt", "cee"}});
  const auto files = pm::packfiles::build_files(pack);
  CHECK(names_of(files) == (std::vector<std::string>{"assets/a/c.txt", "assets/b.txt", "i18n/default.json", "manifest.json"}));
  for (const auto& [name, path] : files) CHECK(fs::exists(path));  // each name comes with a real file
  // Only what is there: a copy without a string table or assets lists its manifest alone.
  const fs::path bare = dir / L"Y-1-2-0.pack";
  write(bare, "");
  write(dir / L"Y-1-2-0.json", "{}");
  CHECK(names_of(pm::packfiles::build_files(bare)) == std::vector<std::string>{"manifest.json"});
}

// The build is the pack's files, not the copy's name: two copies of one pack, named differently, say the
// same thing. A readable pack with files is never the all-zero stand-in for "no build".
void a_pack_build_ignores_the_copy_name() {
  const fs::path dir = g_root / L"same-build";
  const std::vector<std::pair<std::string, std::string>> both{{"b.txt", "bee"}, {"a/c.txt", "cee"}};
  const fs::path one = make_copy(dir, L"Emote-1-2-0", both);
  const fs::path other = make_copy(dir, L"Emote-9-9-9", both);
  const std::string first = pm::packfiles::build_of(one), second = pm::packfiles::build_of(other);
  CHECK(first == second);
  CHECK(build_shaped(first));
  CHECK(first != std::string(16, '0'));
}

// One changed byte in any file is another pack build...
void a_pack_build_changes_with_any_file() {
  const fs::path dir = g_root / L"changed-content";
  const std::vector<std::pair<std::string, std::string>> oneAsset{{"b.txt", "bee"}};
  const fs::path one = make_copy(dir, L"P-1-2-0", oneAsset);
  const fs::path other = make_copy(dir, L"P-1-2-1", oneAsset);
  write(dir / L"P-1-2-1.assets" / "b.txt", "ber");
  CHECK(pm::packfiles::build_of(one) != pm::packfiles::build_of(other));
}

// ...and so, with the same bytes, is a file renamed: a pack's file names are part of its build.
void a_pack_build_changes_when_a_file_is_renamed() {
  const fs::path dir = g_root / L"renamed";
  const fs::path one = make_copy(dir, L"R-1-2-0", {{"b.txt", "bee"}});
  const fs::path other = make_copy(dir, L"R-1-2-1", {{"c.txt", "bee"}});
  CHECK(pm::packfiles::build_of(one) != pm::packfiles::build_of(other));
}

// ...and a file added.
void a_pack_build_changes_when_a_file_is_added() {
  const fs::path dir = g_root / L"added";
  const fs::path one = make_copy(dir, L"A-1-2-0", {{"b.txt", "bee"}});
  const fs::path other = make_copy(dir, L"A-1-2-1", {{"b.txt", "bee"}, {"new.txt", "new"}});
  CHECK(pm::packfiles::build_of(one) != pm::packfiles::build_of(other));
}

}  // namespace

int main() {
  std::error_code error;
  g_root = fs::temp_directory_path(error) / (L"pg-packs-" + std::to_wstring(GetCurrentProcessId()));
  if (error) {
    std::printf("FAIL: there is no temp folder to test in\n");
    return 1;
  }
  fs::remove_all(g_root, error);
  fs::create_directories(g_root, error);
  stand_in_names_the_pack_after_its_folder();
  is_stand_in_ignores_case_and_refuses_dll();
  build_files_lists_manifest_text_and_assets_sorted();
  a_pack_build_ignores_the_copy_name();
  a_pack_build_changes_with_any_file();
  a_pack_build_changes_when_a_file_is_renamed();
  a_pack_build_changes_when_a_file_is_added();
  fs::remove_all(g_root, error);
  return g_failures ? 1 : 0;
}
