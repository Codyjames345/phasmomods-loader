// Offline tests for the mod list formats (core/mod_list.h). Run: build\mod_list_test.exe
#include "mod_list.h"

#include "check.h"
#include "text_fixture.h"

#include <windows.h>

#include <filesystem>
#include <fstream>

using namespace pm::modlist;

namespace {

void reads_a_manifest() {
  Manifest m;
  std::string error;
  CHECK(parse_mod_ini("# comment\r\n id = player-ghost \r\nname=Playable Ghost\r\ndll=PlayableGhost.dll\r\n"
                      "description=Play as the ghost\r\nunknown=ignored\r\n",
                      m, error));
  CHECK(m.id == "player-ghost" && m.name == "Playable Ghost" && m.dll == "PlayableGhost.dll");
  CHECK(m.description == "Play as the ghost");
  CHECK(m.oldFormat && m.version.empty() && m.author.empty());
}

void rejects_unsafe_manifests() {
  Manifest m;
  std::string error;
  CHECK(!parse_mod_ini("name=A\ndll=a.dll\n", m, error) && error.find("id") != std::string::npos);
  CHECK(!parse_mod_ini("id=Upper\nname=A\ndll=a.dll\n", m, error));
  CHECK(!parse_mod_ini("id=a.b\nname=A\ndll=a.dll\n", m, error));  // an old-format id has no author
  // Saved under this id, a tick would mean the bundled mod after a restart, and this one could never start.
  for (const auto& bundled : pm::modid::kOldBundled) {
    error.clear();
    CHECK(!parse_mod_ini("id=" + std::string(bundled.id) + "\nname=A\ndll=a.dll\n", m, error) && error.find("bundled") != std::string::npos);
  }
  error.clear();
  CHECK(!parse_mod_ini("id=vanilla\nname=A\ndll=a.dll\n", m, error) && error.find("vanilla") != std::string::npos);
  CHECK(!parse_mod_ini("id=a\ndll=a.dll\n", m, error) && error.find("name") != std::string::npos);
  CHECK(!parse_mod_ini("id=a\nname=<size=0>A\ndll=a.dll\n", m, error) && error.find("Name") != std::string::npos);
  CHECK(!parse_mod_ini("id=a\nname=" + std::string(65, 'x') + "\ndll=a.dll\n", m, error));
  CHECK(!parse_mod_ini("id=a\nname=A\ndll=a.dll\ndescription=<size=0>x\n", m, error));
  for (const char* dll : {"..\\host.dll", "C:\\x.dll", "sub/x.dll", "x.exe", "x", ".dll"}) {
    error.clear();
    CHECK(!parse_mod_ini(std::string("id=a\nname=A\ndll=") + dll + "\n", m, error) && !error.empty());
  }
  CHECK(parse_mod_ini("id=a\nname=A\ndll=Upper.DLL\n", m, error));
}

void reads_and_writes_the_selection() {
  bool present = true;
  CHECK(parse_enabled("", present).empty() && !present);
  auto ids = parse_enabled("# x\nenabled= a , b-2,,Bad Id,c,me.new-mod,two.dots.here\n", present);
  CHECK(present && ids == (std::vector<std::string>{"a", "b-2", "c", "me.new-mod"}));
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
  CHECK(chosen("enabled=me.tells\n", {"me.tells", "me.new-mod"}) == std::vector<std::string>{"me.tells"});
}

void the_first_choice_is_saved_so_later_mods_start_off() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / (L"pg-choice-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const fs::path ini = root / "data" / "launcher.ini";
  CHECK(load_choice(ini, {}).empty());  // no mods yet: nothing to choose, nothing saved
  CHECK(!fs::exists(ini));
  CHECK(load_choice(ini, {"a", "b"}) == (std::vector<std::string>{"a", "b"}));  // the first run: every mod
  bool present = false;
  CHECK(parse_enabled(pm::text::read_file(ini), present) == (std::vector<std::string>{"a", "b"}) && present);
  CHECK(load_choice(ini, {"a", "b", "c"}) == (std::vector<std::string>{"a", "b"}));  // an update brought c
  CHECK(save_choice(ini, {"c"}));
  CHECK(load_choice(ini, {"a", "b", "c"}) == std::vector<std::string>{"c"});
  fs::remove_all(root, error);
}

void requires_is_parsed() {
  Manifest m;
  std::string error;
  CHECK(parse_mod_ini("id=b\nname=B\ndll=a.dll\nrequires=other\n", m, error));
  CHECK(m.dependencies.size() == 1 && m.dependencies[0].id == "other" && m.dependencies[0].required &&
        m.dependencies[0].minimumVersion.empty());
  CHECK(parse_mod_ini("id=b\nname=B\ndll=a.dll\nrequires=me.new-mod\n", m, error) && m.dependencies.size() == 1 &&
        m.dependencies[0].id == "me.new-mod");
  // An old-format mod that needs a bundled mod names it as it was then.
  CHECK(parse_mod_ini("id=b\nname=B\ndll=a.dll\nrequires=spectral-flame\n", m, error));
  CHECK(m.dependencies.size() == 1 && m.dependencies[0].id == "phasmomods.spectral-flame");
  CHECK(parse_mod_ini("id=a\nname=A\ndll=a.dll\n", m, error) && m.dependencies.empty());
  error.clear();
  CHECK(!parse_mod_ini("id=a\nname=A\ndll=a.dll\nrequires=Bad Id\n", m, error) && error.find("requires") != std::string::npos);
  error.clear();
  CHECK(!parse_mod_ini("id=a\nname=A\ndll=a.dll\nrequires=a\n", m, error) && !error.empty());  // not itself
}

// A mod folder holding `manifest` (manifest.json) or `ini` (mod.ini), under a fresh folder for this process, read from
// `place`.
Reading read_mod(const std::string& manifest, const std::string& ini, Place place, Manifest& m, std::string& why) {
  namespace fs = std::filesystem;
  const fs::path folder = fs::temp_directory_path() / (L"pg-reserved-" + std::to_wstring(GetCurrentProcessId())) / "mod";
  std::error_code error;
  fs::remove_all(folder.parent_path(), error);
  fs::create_directories(folder, error);
  if (!manifest.empty()) std::ofstream(folder / "manifest.json", std::ios::binary) << manifest;
  if (!ini.empty()) std::ofstream(folder / "mod.ini", std::ios::binary) << ini;
  const Reading reading = read_folder(folder, m, why, place);
  fs::remove_all(folder.parent_path(), error);
  return reading;
}

std::string json(const std::string& id, const std::string& author) {
  return R"({"id":")" + id + R"(","name":"N","author":")" + author + R"(","version":"1.0.0","description":"D","dll":"x.dll"})";
}

void third_party_mod_cannot_claim_the_reserved_id_author() {
  Manifest m;
  std::string why;
  for (const char* id : {"phasmomods.ghost-tells", "phasmomods.anything"}) {
    CHECK(read_mod(json(id, "Someone"), "", Place::Elsewhere, m, why) == Reading::Refused);
    CHECK(why == "id uses the author \"phasmomods\", which is reserved for bundled mods.");
    CHECK(m.id.empty());  // nothing of it is kept
  }
  // An old-format mod has no author: its flat id can still claim it.
  CHECK(read_mod("", "id=phasmomods\nname=N\ndll=x.dll\n", Place::Elsewhere, m, why) == Reading::Refused && !why.empty());
  // Only the whole author: these are someone else's.
  CHECK(read_mod(json("phasmomods-fan.tells", "Someone"), "", Place::Elsewhere, m, why) == Reading::Readable);
  CHECK(read_mod(json("someone.phasmomods", "Someone"), "", Place::Elsewhere, m, why) == Reading::Readable);
  CHECK(read_mod("", "id=phasmomods-x\nname=N\ndll=x.dll\n", Place::Elsewhere, m, why) == Reading::Readable);
  CHECK(pm::modid::names_project_author("phasmomods") && pm::modid::names_project_author("  PHASMOMODS "));
  CHECK(!pm::modid::names_project_author("phasmo mods") && !pm::modid::names_project_author("phasmomodsx") &&
        !pm::modid::names_project_author(""));
}

void third_party_mod_cannot_claim_the_reserved_author_name() {
  Manifest m;
  std::string why;
  for (const char* author : {"PhasmoMods", "phasmomods", " PhasmoMods  ", "PHASMOMODS"}) {
    CHECK(read_mod(json("someone.tells", author), "", Place::Elsewhere, m, why) == Reading::Refused);
    CHECK(why == "Author is PhasmoMods, which is reserved for bundled mods.");
  }
  CHECK(read_mod(json("someone.tells", "PhasmoMods fan"), "", Place::Elsewhere, m, why) == Reading::Readable &&
        m.author == "PhasmoMods fan");
}

// The mods shipped in the install folder are the project's own, and both claims are theirs.
void bundled_mod_may_use_the_reserved_author() {
  Manifest m;
  std::string why;
  CHECK(read_mod(json("phasmomods.ghost-tells", "PhasmoMods"), "", Place::InstallFolder, m, why) == Reading::Readable);
  CHECK(m.id == "phasmomods.ghost-tells" && m.author == "PhasmoMods");
  CHECK(reserved_author_refusal(m, Place::InstallFolder).empty() && !reserved_author_refusal(m, Place::Elsewhere).empty());
  // A broken manifest says what is broken, wherever it is.
  CHECK(read_mod("{", "", Place::Elsewhere, m, why) == Reading::Refused && why.find("not valid JSON") != std::string::npos);
}

void reads_the_session_file() {
  const auto paths = parse_session("C:\\a\\one.dll\r\n\r\nC:\\b c\\two.dll\n");
  CHECK(paths == (std::vector<std::string>{"C:\\a\\one.dll", "C:\\b c\\two.dll"}));
  CHECK(parse_session("").empty());
  // A mod the launcher refused is no path, and reaches the host's Mods panel with why.
  const std::string text = "C:\\a\\one.dll\n" + session_refused_line({"broken", "Its id is bad."}) + "C:\\b\\two.dll\n";
  CHECK(parse_session(text) == (std::vector<std::string>{"C:\\a\\one.dll", "C:\\b\\two.dll"}));
  const auto refused = parse_session_refused(text);
  CHECK(refused.size() == 1 && refused[0].name == "broken" && refused[0].why == "Its id is bad.");
  CHECK(parse_session_refused("?no tab here\n").empty());
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  reads_a_manifest();
  rejects_unsafe_manifests();
  reads_and_writes_the_selection();
  chooses_the_saved_mods_or_all_of_them();
  a_new_mod_stays_off_after_a_saved_choice();
  the_first_choice_is_saved_so_later_mods_start_off();
  reads_the_session_file();
  requires_is_parsed();
  third_party_mod_cannot_claim_the_reserved_id_author();
  third_party_mod_cannot_claim_the_reserved_author_name();
  bundled_mod_may_use_the_reserved_author();
  return g_failures ? 1 : 0;
}
