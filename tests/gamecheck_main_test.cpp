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
inline constexpr EntryDesc kChild{.key = "Child", .base = &kBase};
inline constexpr EntryDesc kPlain{.key = "Plain", .base = &kObject};
inline constexpr EntryDesc kLost{.key = "Lost", .base = &kBase};
inline constexpr EntryDesc kOdd{.key = "Odd", .base = &kBase};
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
inline constexpr EntryDesc kToggle{.key = "Toggle", .base = &kBase};
inline constexpr EntryDesc kPlainer{.key = "Plainer", .base = &kBase};
inline constexpr EntryDesc kLoop{.key = "Loop", .base = &kBase};

// A class of the model whose base list names `bases`, each written without type arguments.
ModelType model_class(const char* assembly, const char* ns, const char* name, std::vector<std::string> bases,
                      bool isInterface = false, int declaring = -1) {
  ModelType t;
  t.assembly = assembly;
  t.ns = ns;
  t.name = name;
  t.isInterface = isInterface;
  t.declaring = declaring;
  t.baseArities.assign(bases.size(), 0);
  t.bases = std::move(bases);
  return t;
}

bool names_line(const BaseCheck& check, const std::string& start) {
  return std::any_of(check.wrong.begin(), check.wrong.end(), [&](const std::string& l) { return l.starts_with(start); });
}

// Each parent on the way to a base must be the one type its name and arity can mean, or the entry is named: the check
// never picks among namesakes, even ones that would all lead to the base.
void bases_check_reads_each_parent_exactly() {
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
      model_class("A", "", "Child", {"Event", "IThing"}),                             // 11: either Event, so unproved
      model_class("A", "", "Lost", {"Missing"}),                                      // 12
      model_class("A", "Y", "Dual", {}),                                              // 13
      model_class("A", "Z", "Dual", {}, true),                                        // 14
      model_class("A", "", "Odd", {"Dual"}),                                          // 15: a class or an interface
      // A top-level class finds a class of its own namespace first: VRPlayer's Player is the game's, not Photon's.
      model_class("Game", "", "Player", {}),                                          // 16
      model_class("Photon", "Photon.Realtime", "Player", {}),                         // 17
      model_class("Game", "", "VRPlayer", {"Player"}),                                // 18
      // Two UIBehaviours, neither in Selectable's namespace: unproved.
      model_class("UI", "UnityEngine.EventSystems", "UIBehaviour", {}),               // 19
      model_class("Doozy", "Doozy", "UIBehaviour", {}),                               // 20
      model_class("UI", "UnityEngine.UI", "Selectable", {"UIBehaviour"}),             // 21
      // Signal and Signal<T> share a name: an arity tells them apart, even against the own namespace's.
      model_class("A", "N", "Signal", {}),                                            // 22
      model_class("A", "G", "Signal", {"Base"}),                                      // 23: Signal<T>
      model_class("A", "N", "Toggle", {"Signal"}),                                    // 24: : Signal<bool>
      model_class("A", "G", "Plainer", {"Signal"}),                                   // 25: : Signal
      // A loop no game has: refused at the depth limit, never followed for ever.
      model_class("A", "", "LoopA", {"LoopB"}),                                       // 26
      model_class("A", "", "LoopB", {"LoopA"}),                                       // 27
  };
  model.types[23].genericParams = 1;
  model.types[24].baseArities = {1};
  // A nested enum sharing a class's name is no class anything derives from.
  ModelType nestedEnum = model_class("B", "Q", "Behaviour", {}, false, 7);
  nestedEnum.isEnum = nestedEnum.isValueType = true;
  model.types.push_back(nestedEnum);  // 28
  const auto at = [](const EntryDesc& d, int type) { return Result{.desc = &d, .status = Status::Resolved, .type = type}; };
  Resolution resolution;
  resolution.results = {at(kObject, 0),     at(kComponent, 2), at(kBehaviour, 3),    at(kGhost, 4),      at(kCamera, 5),
                        at(kWrong, 4),      at(kBase, 10),     at(kChild, 11),       at(kPlain, 7),      at(kLost, 12),
                        at(kOdd, 15),       at(kHangs, 4),     at(kMethod, 4),       at(kOnMethod, 4),   at(kOnStray, 4),
                        Result{.desc = &kUnresolved},          at(kPlayer, 16),      at(kVRPlayer, 18),  at(kUIBehaviour, 19),
                        at(kSelectable, 21), at(kToggle, 24),  at(kPlainer, 25),     at(kLoop, 26)};
  const BaseCheck check = check_bases(model, resolution);
  CHECK(check.checked == 16);  // Hangs waits for its base, and entries without one are not checked
  CHECK(check.wrong.size() == 10);
  for (const char* key : {"Component ", "Behaviour ", "GhostAI ", "Camera ", "Hangs ", "VRPlayer ", "Toggle "})
    CHECK(!names_line(check, std::string("BASE ") + key));
  CHECK(names_line(check, "BASE WrongGhost does not derive from Camera: the chain ends at UnityEngine.Object"));
  CHECK(names_line(check, "BASE Child does not derive from Base: Child derives from Event, which 2 types of the model share"));
  CHECK(names_line(check, "BASE Plain does not derive from Object: the chain ends at Plain"));
  CHECK(names_line(check, "BASE Lost does not derive from Base: Lost derives from Missing, which is no type of the model"));
  CHECK(names_line(check, "BASE Odd does not derive from Base: Odd derives from Dual, which 2 types of the model share"));
  CHECK(names_line(check, "BASE OnMethod: its base Method is not a type entry of the map"));
  CHECK(names_line(check, "BASE OnStray: its base Stray is not a type entry of the map"));
  CHECK(names_line(check, "BASE Selectable does not derive from UIBehaviour: UnityEngine.UI.Selectable derives from "
                          "UIBehaviour, which 2 types of the model share"));
  CHECK(names_line(check, "BASE Plainer does not derive from Base: the chain ends at N.Signal"));
  CHECK(names_line(check, "BASE Loop does not derive from Base: the chain from LoopB is deeper than 64 classes"));
}
}  // namespace

int main() {
  std::error_code ignored;
  std::filesystem::remove_all(scratch(), ignored);
  std::filesystem::create_directories(scratch());
  exception_exits_2();
  unreadable_entry_stops_the_listing();
  bases_check_reads_each_parent_exactly();
  std::filesystem::remove_all(scratch(), ignored);
  return g_failures ? 1 : 0;
}
