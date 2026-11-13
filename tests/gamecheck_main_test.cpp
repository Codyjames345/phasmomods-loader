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

// The base data the bases check proves: Unity's chain, a wrong base, and the cases the dump cannot prove.
inline constexpr EntryDesc kObject{.key = "Object"};
inline constexpr EntryDesc kComponent{.key = "Component", .base = &kObject};
inline constexpr EntryDesc kBehaviour{.key = "Behaviour", .base = &kComponent};
inline constexpr EntryDesc kGhost{.key = "GhostAI", .base = &kBehaviour};
inline constexpr EntryDesc kCamera{.key = "Camera", .base = &kBehaviour};
inline constexpr EntryDesc kWrong{.key = "WrongGhost", .base = &kCamera};
inline constexpr EntryDesc kBase{.key = "Base"};
inline constexpr EntryDesc kEvent{.key = "Event"};
inline constexpr EntryDesc kChild{.key = "Child", .base = &kBase};
inline constexpr EntryDesc kChildOfEvent{.key = "ChildOfEvent", .base = &kEvent};
inline constexpr EntryDesc kPlain{.key = "Plain", .base = &kObject};
inline constexpr EntryDesc kLost{.key = "Lost", .base = &kBase};
inline constexpr EntryDesc kOdd{.key = "Odd", .base = &kBase};
inline constexpr EntryDesc kSender{.key = "Sender", .base = &kBase};
inline constexpr EntryDesc kUnresolved{.key = "Unresolved"};
inline constexpr EntryDesc kHangs{.key = "Hangs", .base = &kUnresolved};
inline constexpr EntryDesc kMethod{.key = "Method", .kind = Kind::Method};
inline constexpr EntryDesc kOnMethod{.key = "OnMethod", .base = &kMethod};
inline constexpr EntryDesc kStray{.key = "Stray"};
inline constexpr EntryDesc kOnStray{.key = "OnStray", .base = &kStray};
inline constexpr EntryDesc kPlayer{.key = "Player"};
inline constexpr EntryDesc kVRPlayer{.key = "VRPlayer", .base = &kPlayer};
inline constexpr EntryDesc kUIBehaviour{.key = "UIBehaviour"};
inline constexpr EntryDesc kSelectable{.key = "Selectable", .base = &kUIBehaviour};
inline constexpr EntryDesc kSharedP{.key = "SharedP"};
inline constexpr EntryDesc kUser{.key = "User", .base = &kSharedP};

ModelType model_class(const char* assembly, const char* ns, const char* name, std::vector<std::string> bases,
                      bool isInterface = false, int declaring = -1) {
  ModelType t;
  t.assembly = assembly;
  t.ns = ns;
  t.name = name;
  t.isInterface = isInterface;
  t.declaring = declaring;
  t.bases = std::move(bases);
  return t;
}

bool names_line(const BaseCheck& check, const std::string& start) {
  return std::any_of(check.wrong.begin(), check.wrong.end(), [&](const std::string& l) { return l.starts_with(start); });
}

// Each base must lie on every chain of parents the dump allows from its entry's class, or the entry is named.
void bases_check_walks_every_parent_chain() {
  GameModel model;
  model.types = {
      model_class("UnityEngine.CoreModule", "UnityEngine", "Object", {}),               // 0
      model_class("mscorlib", "System", "Object", {}),                                // 1: never a base in a list
      model_class("UnityEngine.CoreModule", "UnityEngine", "Component", {"Object"}),     // 2
      model_class("UnityEngine.CoreModule", "UnityEngine", "Behaviour", {"Component"}),  // 3
      model_class("Assembly-CSharp", "", "GhostAI", {"Behaviour"}),                   // 4
      model_class("UnityEngine.CoreModule", "UnityEngine", "Camera", {"Behaviour"}),     // 5
      model_class("A", "", "IThing", {}, true),                                       // 6
      model_class("A", "", "Plain", {"IThing"}),                                      // 7: derives from System.Object
      model_class("A", "X", "Event", {"Base"}),                                       // 8
      model_class("A", "Y", "Event", {"Base"}),                                       // 9
      model_class("A", "", "Base", {}),                                               // 10
      model_class("A", "", "Child", {"Event", "IThing"}),                             // 11: either Event, both from Base
      model_class("A", "", "Lost", {"Missing"}),                                      // 12
      model_class("A", "Y", "Dual", {}),                                              // 13
      model_class("A", "Z", "Dual", {}, true),                                        // 14
      model_class("A", "", "Odd", {"Dual"}),                                          // 15: a class or an interface
      model_class("A", "X", "Signal", {"Base"}),                                      // 16
      model_class("A", "Y", "Signal", {}),                                            // 17
      model_class("A", "", "Sender", {"Signal"}),                                     // 18: one Signal not from Base
      // A top-level class finds a class of its own namespace first: VRPlayer's Player is the game's, not Photon's.
      model_class("Game", "", "Player", {}),                                          // 19
      model_class("Photon", "Photon.Realtime", "Player", {}),                         // 20
      model_class("Game", "", "VRPlayer", {"Player"}),                                // 21
      // UI's classes cannot derive from Doozy's, whose UISelectable derives from UI's Selectable.
      model_class("UI", "UnityEngine.EventSystems", "UIBehaviour", {}),               // 22
      model_class("Doozy", "Doozy", "UIBehaviour", {}),                               // 23
      model_class("UI", "UnityEngine.UI", "Selectable", {"UIBehaviour"}),             // 24
      model_class("Doozy", "Doozy.Components", "UISelectable", {"Selectable"}),       // 25
      // P and Q reference each other as far as the model shows, so neither Shared is ruled out for User.
      model_class("P", "P", "FromQ", {"QOnly"}),                                      // 26
      model_class("Q", "Q", "QOnly", {}),                                             // 27
      model_class("Q", "Q2", "FromP", {"POnly"}),                                     // 28
      model_class("P", "P3", "POnly", {}),                                            // 29
      model_class("P", "PS", "Shared", {}),                                           // 30
      model_class("Q", "QS", "Shared", {}),                                           // 31
      model_class("P", "PU", "User", {"Shared"}),                                     // 32
  };
  // A nested enum sharing a class's name is no class anything derives from.
  ModelType nestedEnum = model_class("B", "Q", "Behaviour", {}, false, 7);
  nestedEnum.isEnum = nestedEnum.isValueType = true;
  model.types.push_back(nestedEnum);  // 33
  const auto at = [](const EntryDesc& d, int type) { return Result{.desc = &d, .status = Status::Resolved, .type = type}; };
  Resolution resolution;
  resolution.results = {at(kObject, 0),  at(kComponent, 2), at(kBehaviour, 3), at(kGhost, 4),         at(kCamera, 5),
                        at(kWrong, 4),   at(kBase, 10),     at(kEvent, 8),     at(kChild, 11),        at(kChildOfEvent, 11),
                        at(kPlain, 7),   at(kLost, 12),     at(kOdd, 15),      at(kSender, 18),       at(kHangs, 4),
                        at(kMethod, 4),  at(kOnMethod, 4),  at(kOnStray, 4),   Result{.desc = &kUnresolved},
                        at(kPlayer, 19), at(kVRPlayer, 21), at(kUIBehaviour, 22), at(kSelectable, 24),
                        at(kSharedP, 30), at(kUser, 32)};
  const BaseCheck check = check_bases(model, resolution);
  CHECK(check.checked == 16);  // Hangs waits for its base, and entries without one are not checked
  CHECK(check.wrong.size() == 9);
  for (const char* key : {"Component ", "Behaviour ", "GhostAI ", "Camera ", "Child ", "Hangs ", "VRPlayer ", "Selectable "})
    CHECK(!names_line(check, std::string("BASE ") + key));
  CHECK(names_line(check, "BASE WrongGhost does not derive from Camera: the chain ends at UnityEngine.Object"));
  CHECK(names_line(check, "BASE ChildOfEvent does not derive from Event: the chain ends at Base"));
  CHECK(names_line(check, "BASE Plain does not derive from Object: the chain ends at Plain"));
  CHECK(names_line(check, "BASE Lost does not derive from Base: Lost derives from Missing, which is no type of the model"));
  CHECK(names_line(check, "BASE Odd does not derive from Base: Odd derives from Dual, which names a class and an interface"));
  CHECK(names_line(check, "BASE Sender does not derive from Base: the chain ends at Y.Signal"));
  CHECK(names_line(check, "BASE OnMethod: its base Method is not a type entry of the map"));
  CHECK(names_line(check, "BASE OnStray: its base Stray is not a type entry of the map"));
  CHECK(names_line(check, "BASE User does not derive from SharedP: the chain ends at QS.Shared"));
}

}  // namespace

int main() {
  std::error_code ignored;
  std::filesystem::remove_all(scratch(), ignored);
  std::filesystem::create_directories(scratch());
  exception_exits_2();
  unreadable_entry_stops_the_listing();
  bases_check_walks_every_parent_chain();
  std::filesystem::remove_all(scratch(), ignored);
  return g_failures ? 1 : 0;
}
