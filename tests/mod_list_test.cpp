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

void the_saved_choice_keeps_the_lines_the_launcher_does_not_own() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / (L"pg-choice-dev-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  const fs::path ini = root / "data" / "launcher.ini";
  fs::create_directories(ini.parent_path(), error);
  // Written as bytes, like Notepad would leave them: the launcher reads with CRLF lines.
  std::ofstream(ini, std::ios::binary) << "# My note\r\n# Mods the launcher starts. Written by the launcher.\r\nenabled=old.mod\r\ndeveloper=1\r\n";
  CHECK(save_choice(ini, {"a", "b"}));
  const std::string text = pm::text::read_file(ini);
  bool present = false;
  CHECK(parse_enabled(text, present) == (std::vector<std::string>{"a", "b"}) && present);
  CHECK(parse_developer(text));  // the hand-edited developer line survives the launcher's save
  CHECK(text.find("# My note") != std::string::npos);
  CHECK(text.find("old.mod") == std::string::npos);  // the old enabled line is gone, not shadowed
  size_t headers = 0;
  for (size_t at = text.find("# Mods the launcher starts"); at != std::string::npos; at = text.find("# Mods the launcher starts", at + 1))
    ++headers;
  CHECK(headers == 1);  // and the header comment is not doubled
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
Reading read_mod(const std::string& manifest, const std::string& ini, Place place, Manifest& m, std::string& why,
                 bool developer = false) {
  namespace fs = std::filesystem;
  const fs::path folder = fs::temp_directory_path() / (L"pg-reserved-" + std::to_wstring(GetCurrentProcessId())) / "mod";
  std::error_code error;
  fs::remove_all(folder.parent_path(), error);
  fs::create_directories(folder, error);
  if (!manifest.empty()) std::ofstream(folder / "manifest.json", std::ios::binary) << manifest;
  if (!ini.empty()) std::ofstream(folder / "mod.ini", std::ios::binary) << ini;
  const Reading reading = read_folder(folder, m, why, place, developer);
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

// The mod folders both the launcher and the host read: every root's sub-folders in name order, the roots
// in the order given, so the install folder's mods come first. A root that is not there adds nothing, and
// a root that is another root already read is read once, under the earlier root's place.
void mod_folders_are_read_in_root_order_and_name_order() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / (L"pg-folders-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  for (const char* name : {"install/mods/beta", "install/mods/alpha", "appdata/mods/gamma"})
    fs::create_directories(root / name, error);
  const auto folders = mod_folders({{root / "install" / "mods", Place::InstallFolder},
                                    {root / "appdata" / "mods", Place::Elsewhere}});
  CHECK(folders.size() == 3);
  CHECK(folders[0].first == root / "install" / "mods" / "alpha" && folders[0].second == Place::InstallFolder);
  CHECK(folders[1].first == root / "install" / "mods" / "beta");
  CHECK(folders[2].first == root / "appdata" / "mods" / "gamma" && folders[2].second == Place::Elsewhere);
  CHECK(mod_folders({{root / "not-there"}, {root / "appdata" / "mods", Place::Elsewhere}}).size() == 1);
  const auto twice = mod_folders({{root / "install" / "mods", Place::InstallFolder},
                                  {root / "install" / "mods" / ".." / "mods", Place::Elsewhere}});
  CHECK(twice.size() == 2 && twice[0].second == Place::InstallFolder && twice[1].second == Place::InstallFolder);
  fs::remove_all(root, error);
}

// A mod id found more than once, in one folder or across two: every copy is a repeat, and each names the
// folders of all the others (the shape SMAPI's duplicate refusal takes).
void every_copy_of_a_repeated_id_names_the_others() {
  const auto repeats = folders_of_repeats({"me.a", "me.b", "me.a", "me.a"}, {"one", "two", "three", "four"});
  CHECK(repeats.size() == 4);
  CHECK(repeats[0] == (std::vector<std::string>{"three", "four"}));
  CHECK(repeats[1].empty());
  CHECK(repeats[2] == (std::vector<std::string>{"one", "four"}));
  CHECK(repeats[3] == (std::vector<std::string>{"one", "three"}));
}

void the_developer_flag_shows_developer_mods_and_hides_them() {
  CHECK(!parse_developer(""));
  CHECK(!parse_developer("enabled=a.b\n"));              // no line at all: off
  CHECK(!parse_developer("developer=0\n"));
  CHECK(!parse_developer("developer=maybe\n"));          // anything but 1 or true is off
  CHECK(parse_developer("developer=1\n"));
  CHECK(parse_developer("developer=true\n"));
  CHECK(!parse_developer("# developer=1\n"));            // a comment is not the line
  CHECK(parse_developer("developer=0\ndeveloper=1\n"));  // the last line wins
  CHECK(!parse_developer("developerx=1\n"));             // not a prefix match
  CHECK(parse_developer("developer= 1 \n"));             // spaces around the value are no problem
  // A developerOnly mod nobody asked for reads as no mod at all: not refused, no reason, nothing kept.
  const std::string dev = R"({"id":"someone.tool","name":"N","author":"Someone","version":"1.0.0","description":"D","dll":"x.dll","developerOnly":true})";
  Manifest m;
  std::string why;
  CHECK(read_mod(dev, "", Place::Elsewhere, m, why) == Reading::None);
  CHECK(m.id.empty() && why.empty());
  CHECK(read_mod(dev, "", Place::Elsewhere, m, why, true) == Reading::Readable);
  CHECK(m.id == "someone.tool" && m.developerOnly);
  // A plain mod is unaffected by the flag, and an old-format mod cannot be developer-only.
  CHECK(read_mod(json("someone.tells", "Someone"), "", Place::Elsewhere, m, why) == Reading::Readable && !m.developerOnly);
  CHECK(read_mod("", "id=tool\nname=T\ndll=x.dll\n", Place::Elsewhere, m, why, true) == Reading::Readable &&
        !m.developerOnly);
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
  the_saved_choice_keeps_the_lines_the_launcher_does_not_own();
  reads_the_session_file();
  requires_is_parsed();
  third_party_mod_cannot_claim_the_reserved_id_author();
  third_party_mod_cannot_claim_the_reserved_author_name();
  mod_folders_are_read_in_root_order_and_name_order();
  every_copy_of_a_repeated_id_names_the_others();
  bundled_mod_may_use_the_reserved_author();
  the_developer_flag_shows_developer_mods_and_hides_them();
  return g_failures ? 1 : 0;
}
