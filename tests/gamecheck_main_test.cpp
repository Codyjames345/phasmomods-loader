// The offline check's own failure paths (sdk/offline/gamecheck.cpp), which tests/gamecheck_test.cmake
// cannot reach through the built tool: an exception thrown anywhere in the check exits 2 with the
// reason, and a folder entry that cannot be read stops the listing. Moving to the next entry of a
// folder fails only on a device or network error, which no offline test can bring about, so the
// listing's own error path is pinned through the entry callback instead.
// Run: build\gamecheck_main_test.exe
#define wmain gamecheck_wmain
#include "../sdk/offline/gamecheck.cpp"
#undef wmain

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>

#include "check.h"

// The dump reader of this test: it throws, as a reader meeting a filesystem failure could.
bool pm::sdk::load_dump(const std::filesystem::path&, std::span<const std::string>, GameModel&, std::string&,
                        size_t*) {
  throw std::runtime_error("the disk went away");
}

namespace {

std::filesystem::path scratch() { return std::filesystem::temp_directory_path() / "pm_gamecheck_main_test"; }

// Whatever the check throws ends it with exit 2 and the reason, never an unhandled exception.
void exception_exits_2() {
  const std::filesystem::path log = scratch() / "stderr.txt";
  std::fflush(stderr);
  FILE* redirected = nullptr;
  CHECK(_wfreopen_s(&redirected, log.c_str(), L"w", stderr) == 0);
  wchar_t name[] = L"gamecheck", dump[] = L"--dump", dir[] = L"dump", image[] = L"--image", dll[] = L"image.dll";
  wchar_t* argv[] = {name, dump, dir, image, dll};
  const int code = gamecheck_wmain(5, argv);
  std::fflush(stderr);
  _wfreopen_s(&redirected, L"CONOUT$", L"w", stderr);
  CHECK(code == 2);
  std::ifstream in(log);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(text == "gamecheck: stopped by an exception: the disk went away\n");
}

// A folder that cannot be listed, or an entry whose kind cannot be read, ends the listing as unread.
void unreadable_entry_stops_the_listing() {
  const std::filesystem::path dir = scratch() / "mods";
  std::filesystem::create_directories(dir);
  std::ofstream(dir / "a.dll") << "x";
  std::ofstream(dir / "b.dll") << "x";
  int seen = 0;
  CHECK(!each_entry(dir, [&](const std::filesystem::directory_entry&) {
    ++seen;
    return false;
  }));
  CHECK(seen == 1);  // stopped at the first entry that could not be read
  seen = 0;
  CHECK(each_entry(dir, [&](const std::filesystem::directory_entry&) {
    ++seen;
    return true;
  }));
  CHECK(seen == 2);
  CHECK(!each_entry(scratch() / "no-such-folder", [](const std::filesystem::directory_entry&) { return true; }));
  std::vector<std::filesystem::path> dlls;
  CHECK(!dlls_in(scratch() / "no-such-folder", dlls));
  CHECK(dlls_in(dir, dlls) && dlls.size() == 2);
}

}  // namespace

int main() {
  std::error_code ignored;
  std::filesystem::remove_all(scratch(), ignored);
  std::filesystem::create_directories(scratch());
  exception_exits_2();
  unreadable_entry_stops_the_listing();
  std::filesystem::remove_all(scratch(), ignored);
  return g_failures ? 1 : 0;
}
