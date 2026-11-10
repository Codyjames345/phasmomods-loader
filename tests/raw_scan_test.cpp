// No raw game values in the sources. The SDK's job is to keep addresses, offsets and obfuscated names out of the mods
// and the host: this test fails on any raw site in mods/, core/ and host/ outside the runtime's own files (stage 2b Task
// 14 made it enforcing; there is no baseline). A site is, outside comments:
//   - a hex literal of 0x100 or more;
//   - a non-ASCII identifier or string (an obfuscated name);
//   - a call into il2cpp:: (a lower-case member) or a p_il2cpp_ pointer;
//   - il2cpp::field< or static_get;
//   - MH_CreateHook outside host/hooks.cpp;
//   - a string literal naming FindObjectOfType, NameToLayer or Find.
// A line ending in "// pm:not-game-data: <why>" is exempt (a colour, a protocol byte, a mask). The files
// in kExemptFiles are the runtime itself, which has to hold raw values.
// Separately, and with no baseline: a file under mods/ fails when it is assembly, has "stub" as a word of its
// name, or names a native site's entry or a function that writes code. Native code is the host's
// (native_in_mod). It is a name rule against accidents; the host contract gives a mod no native site's address.
// And the IL2CPP layer is the host's: a file a mod compiles (mods/, core/, sdk/api, sdk/map) that includes host/il2cpp.h or
// calls il2cpp:: fails, as does a build that puts il2cpp in pmcore or pmmod (runtime_outside_host, runtime_linked_by_mods).
// And the host keeps to its own helpers: a file under host/ that includes core's game.h or names game:: fails
// (core_game_in_host), since core/game.h holds what mods need and host/host_game.h what the host does.
// Comments (including a // comment continued by a trailing backslash), multi-line raw strings and
// digit separators are handled. The limit: a character literal is read only in the forms 'x' and '\x'.
// usage: raw_scan_test           check the tree: any raw site fails
//        raw_scan_test --print   write the current counts, to replace the baseline
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"

namespace {
namespace fs = std::filesystem;

struct Exempt {
  const char* prefix;  // a file, or a family of files by name prefix, relative to the source root
  const char* why;
};
const Exempt kExemptFiles[] = {
    {"host/il2cpp.", "the il2cpp wrapper itself, the host's alone"},
    {"host/game_map.", "resolves the map's entries, so it names the game"},
    {"host/hooks.cpp", "the one place that creates hooks"},
    {"host/code_memory.h", "reads and patches code, so it holds addresses"},
    {"host/scene_engine.cpp", "the scene engine reads the game's scene facts"},
    {"host/runtime_services.", "the runtime services' bodies: the IL2CPP layer's face to the mods and the host, naming no "
                               "game member but the journal sync controllers' generic base's Initialise, which "
                               "sdk/map/JournalController.h documents"},
};

struct Counts {
  int hex = 0, nonAscii = 0, il2cpp = 0, field = 0, hook = 0, find = 0;
  int total() const { return hex + nonAscii + il2cpp + field + hook + find; }
  bool operator==(const Counts&) const = default;
};

bool exempt_file(const std::string& relative) {
  for (const Exempt& e : kExemptFiles)
    if (relative.rfind(e.prefix, 0) == 0) return true;
  return false;
}

bool exempt_line(const std::string& line) {
  static const std::regex kMark(R"((//|;)\s*pm:not-game-data:\s*\S[^\r\n]*$)");
  return std::regex_search(line, kMark);
}

/** One line without its comments. `strings` keeps the text of each string literal, and `code` has the
 *  literals' contents blanked (their quotes stay). `text` has both comments removed and literals kept. */
struct Line {
  std::string code, text;
  std::vector<std::string> strings;
};

/** What carries from one line to the next: a block comment, a // comment continued by a trailing
 *  backslash, and a raw string R"delimiter( ... )delimiter" (the closing text is kept in `rawEnd`). */
struct State {
  bool inBlock = false, continuedComment = false;
  std::string rawEnd;
};

Line strip_line(const std::string& line, State& state, bool asmFile) {
  Line out;
  if (state.continuedComment) {
    state.continuedComment = !line.empty() && line.back() == '\\';
    return out;
  }
  for (size_t i = 0; i < line.size(); ++i) {
    const char c = line[i];
    if (!state.rawEnd.empty()) {
      const size_t end = line.find(state.rawEnd, i);
      if (end == std::string::npos) return out;
      i = end + state.rawEnd.size() - 1;
      state.rawEnd.clear();
      out.code += "\"\"";
      out.text += "\"\"";
      continue;
    }
    if (state.inBlock) {
      if (c == '*' && i + 1 < line.size() && line[i + 1] == '/') {
        state.inBlock = false;
        ++i;
      }
      continue;
    }
    if (asmFile ? c == ';' : (c == '/' && i + 1 < line.size() && line[i + 1] == '/')) {
      state.continuedComment = !asmFile && !line.empty() && line.back() == '\\';
      break;
    }
    if (!asmFile && c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
      state.inBlock = true;
      ++i;
      continue;
    }
    if (!asmFile && c == '"' && i > 0 && line[i - 1] == 'R') {  // a raw string
      const size_t open = line.find('(', i);
      if (open != std::string::npos) {
        state.rawEnd = ")" + line.substr(i + 1, open - i - 1) + "\"";
        i = open;
        continue;
      }
    }
    // A character literal, unless the ' follows a hex digit, where it separates digits (0x1'0000).
    if (c == '\'' && !asmFile && !(i > 0 && std::isxdigit(static_cast<unsigned char>(line[i - 1])))) {
      const size_t end = line[i + 1] == '\\' ? line.find('\'', i + 3) : (i + 2 < line.size() && line[i + 2] == '\'' ? i + 2 : std::string::npos);
      if (end != std::string::npos) {
        out.code.append(end - i + 1, ' ');
        out.text.append(end - i + 1, ' ');
        i = end;
        continue;
      }
    }
    if (c == '"') {
      std::string literal;
      size_t j = i + 1;
      for (; j < line.size() && line[j] != '"'; ++j) {
        if (line[j] == '\\' && j + 1 < line.size()) literal += line[j++];
        literal += line[j];
      }
      out.strings.push_back(literal);
      out.code += "\"" + std::string(literal.size(), ' ') + "\"";
      out.text += "\"" + literal + "\"";
      i = j;
      continue;
    }
    out.code += c;
    out.text += c;
  }
  return out;
}

int count_matches(const std::string& text, const std::regex& re) {
  return static_cast<int>(std::distance(std::sregex_iterator(text.begin(), text.end(), re), std::sregex_iterator()));
}

/** Whether a hex literal's digits make 0x100 or more. */
bool at_least_0x100(std::string digits) {
  digits.erase(std::remove(digits.begin(), digits.end(), '\''), digits.end());
  const size_t first = digits.find_first_not_of('0');
  return first != std::string::npos && digits.size() - first > 2;
}

/** Hex literals of 0x100 or more: 0x1400, 0x290u, 0x1400ull, 0x1'0000, and in assembly 290h. */
int count_hex(const std::string& code, bool asmFile) {
  static const std::regex kHex(R"(\b0[xX]([0-9A-Fa-f']+)[uUlLzZ]*\b)");
  static const std::regex kMasm(R"(\b([0-9][0-9A-Fa-f]*)[hH]\b)");
  int n = 0;
  for (std::sregex_iterator it(code.begin(), code.end(), kHex), end; it != end; ++it)
    if (at_least_0x100((*it)[1].str())) ++n;
  if (asmFile)
    for (std::sregex_iterator it(code.begin(), code.end(), kMasm), end; it != end; ++it)
      if (at_least_0x100((*it)[1].str())) ++n;
  return n;
}
/** Runs of identifier characters that include a non-ASCII byte. */
int count_non_ascii(const std::string& text) {
  int n = 0;
  bool inRun = false, hasHigh = false;
  for (const char ch : text) {
    const unsigned char c = static_cast<unsigned char>(ch);
    if (c >= 0x80 || std::isalnum(c) || c == '_') {
      inRun = true;
      hasHigh = hasHigh || c >= 0x80;
    } else {
      if (inRun && hasHigh) ++n;
      inRun = hasHigh = false;
    }
  }
  return inRun && hasHigh ? n + 1 : n;
}

Counts scan_text(const std::string& source, const std::string& relative) {
  static const std::regex kIl2cppCall(R"(\bil2cpp::(?!field<)[a-z_]\w*|\bp_il2cpp_\w+)");
  static const std::regex kField(R"(\bil2cpp::field<|\bstatic_get\b)");
  static const std::regex kHook(R"(\bMH_CreateHook\b)");
  const bool asmFile = fs::path(relative).extension() == ".asm";
  const bool hookAllowed = relative == "host/hooks.cpp";
  Counts counts;
  State state;
  std::istringstream in(source);
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const bool exempt = exempt_line(raw);
    const Line line = strip_line(raw, state, asmFile);
    if (exempt) continue;
    counts.hex += count_hex(line.code, asmFile);
    counts.nonAscii += count_non_ascii(line.text);
    counts.il2cpp += count_matches(line.code, kIl2cppCall);
    counts.field += count_matches(line.code, kField);
    if (!hookAllowed) counts.hook += count_matches(line.code, kHook);
    for (const std::string& s : line.strings)
      if (s.find("FindObjectOfType") != std::string::npos || s.find("NameToLayer") != std::string::npos || s == "Find" ||
          s.find("Find(") != std::string::npos)
        ++counts.find;
  }
  return counts;
}

std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  return buffer.str();
}

/** Why a file under mods/ holds what only the host may (its steering, for example): an assembly file, a file named as a
 *  stub, or code that names a native site's entry or writes code. Empty when it holds none. A mod asks the game API
 *  for the effect instead. */
std::string native_in_mod(const std::string& relative, const std::string& source) {
  const fs::path path(relative);
  const std::string ext = path.extension().string();
  if (ext == ".asm" || ext == ".masm" || ext == ".s" || ext == ".S") return "assembly";
  // "stub" as a word of the name (steer_stub, stub-patch, stubs), not inside another word (stubborn).
  static const std::regex kStubName(R"((^|[_\-. ])stubs?($|[_\-. ]))", std::regex::icase);
  if (std::regex_search(path.stem().string(), kStubName)) return "named as a native stub";
  static const std::regex kNative(
      R"(\b(UnityPlayer_\w+|GameAssembly_\w+|VirtualProtect|VirtualProtectEx|VirtualAlloc|VirtualAllocEx|WriteProcessMemory|)"
      R"(FlushInstructionCache|NtProtectVirtualMemory|ZwProtectVirtualMemory|NtAllocateVirtualMemory|)"
      R"(ZwAllocateVirtualMemory|NtWriteVirtualMemory|ZwWriteVirtualMemory)\b)");
  State state;
  std::istringstream in(source);
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const Line line = strip_line(raw, state, false);
    std::smatch found;
    if (std::regex_search(line.code, found, kNative)) return "names " + found[1].str();
  }
  return {};
}

/** Every file under mods/ that holds what only the host may, with why. */
std::vector<std::string> native_in_mods(const fs::path& root) {
  std::vector<std::string> problems;
  for (const auto& entry : fs::recursive_directory_iterator(root / "mods")) {
    if (!entry.is_regular_file()) continue;
    const std::string relative = fs::relative(entry.path(), root).generic_string();
    const std::string why = native_in_mod(relative, read_file(entry.path()));
    if (!why.empty()) problems.push_back(relative + ": native code belongs in the host, and a mod asks the game API for the effect (" + why + ")");
  }
  return problems;
}

/** Why a file a mod compiles reaches the IL2CPP layer: an include of il2cpp.h, or a name in il2cpp::. Empty when not. */
std::string runtime_outside_host(const std::string& source) {
  static const std::regex kInclude(R"(^\s*#\s*include\s*["<][^">]*\bil2cpp\.h[">])");
  static const std::regex kName(R"(\bil2cpp::\w)");
  State state;
  std::istringstream in(source);
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const Line line = strip_line(raw, state, false);
    // The raw line: stripping blanks a string, and an include's name in quotes is one.
    if (line.code.find('#') != std::string::npos && std::regex_search(raw, kInclude)) return "includes il2cpp.h";
    if (std::regex_search(line.code, kName)) return "names il2cpp::";
  }
  return {};
}

/** Every file a mod compiles that reaches the IL2CPP layer, with why. */
std::vector<std::string> runtime_in_mods(const fs::path& root) {
  std::vector<std::string> problems;
  for (const char* dir : {"mods", "core", "sdk/api", "sdk/map"})
    for (const auto& entry : fs::recursive_directory_iterator(root / dir)) {
      const auto ext = entry.path().extension();
      if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
      const std::string relative = fs::relative(entry.path(), root).generic_string();
      const std::string why = runtime_outside_host(read_file(entry.path()));
      if (!why.empty()) problems.push_back(relative + ": the IL2CPP layer is the host's (" + why + ")");
    }
  return problems;
}

/** Why a host file reaches core's helpers for the mods: an include of game.h, or a name in game::. Empty when not. */
std::string core_game_in_host(const std::string& source) {
  static const std::regex kInclude(R"(^\s*#\s*include\s*["<]([^">]*/)?game\.h[">])");
  static const std::regex kName(R"((^|[^\w:])(pm::)?game::\w)");
  State state;
  std::istringstream in(source);
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const Line line = strip_line(raw, state, false);
    if (line.code.find('#') != std::string::npos && std::regex_search(raw, kInclude)) return "includes game.h";
    if (std::regex_search(line.code, kName)) return "names game::";
  }
  return {};
}

/** Every host file that reaches core's helpers for the mods, with why. */
std::vector<std::string> core_game_in_hosts(const fs::path& root) {
  std::vector<std::string> problems;
  for (const auto& entry : fs::recursive_directory_iterator(root / "host")) {
    const auto ext = entry.path().extension();
    if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
    const std::string why = core_game_in_host(read_file(entry.path()));
    if (!why.empty())
      problems.push_back(fs::relative(entry.path(), root).generic_string() + ": the host keeps to host/host_game.h (" + why + ")");
  }
  return problems;
}

/** Why a host file reads the mouse itself: a use of mouse_position or mouse_left_pressed (the host's own or core's), or
 *  of the game's Mouse, Pointer.get_position or ButtonControl directly. The loader's menu controls are the game's
 *  buttons, clicked by the game's input (host/menu_panel.h). Empty when not. */
std::string mouse_polling_in_host(const std::string& source) {
  static const std::regex kName(R"((^|[^\w])(mouse_position(?=\s*\()|mouse_left_pressed(?=\s*\()|Mouse::|Pointer::get_position|ButtonControl::))");
  State state;
  std::istringstream in(source);
  std::string raw;
  while (std::getline(in, raw)) {
    if (!raw.empty() && raw.back() == '\r') raw.pop_back();
    const std::string code = strip_line(raw, state, false).code;
    std::smatch m;
    if (std::regex_search(code, m, kName)) return "uses " + m[2].str();
  }
  return {};
}

/** Every host file that reads the mouse itself, with why. */
std::vector<std::string> mouse_polling_in_hosts(const fs::path& root) {
  std::vector<std::string> problems;
  for (const auto& entry : fs::recursive_directory_iterator(root / "host")) {
    const auto ext = entry.path().extension();
    if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
    const std::string why = mouse_polling_in_host(read_file(entry.path()));
    if (!why.empty())
      problems.push_back(fs::relative(entry.path(), root).generic_string() +
                         ": the game's input clicks the host's menu controls (" + why + ")");
  }
  return problems;
}

/** Why the build gives a mod the IL2CPP layer: the libraries every mod links, as CMakeLists.txt lists them. */
std::vector<std::string> runtime_linked_by_mods(const std::string& cmake) {
  std::vector<std::string> problems;
  for (const char* library : {"pmcore", "pmmod"}) {
    const std::string head = std::string("add_library(") + library + " ";
    const size_t start = cmake.find(head);
    const size_t end = start == std::string::npos ? start : cmake.find(')', start);
    if (end == std::string::npos) {
      problems.push_back(std::string("CMakeLists.txt has no library ") + library);
      continue;
    }
    if (cmake.substr(start, end - start).find("il2cpp") != std::string::npos)
      problems.push_back(std::string(library) + " compiles the IL2CPP layer, and every mod links it");
  }
  return problems;
}

void the_runtime_outside_the_host_is_found() {
  CHECK(runtime_outside_host("#include \"il2cpp.h\"\n") == "includes il2cpp.h");
  CHECK(runtime_outside_host("  # include <host/il2cpp.h>\n") == "includes il2cpp.h");
  CHECK(runtime_outside_host("void f() { il2cpp::cls(a, b, c); }\n") == "names il2cpp::");
  CHECK(runtime_outside_host("il2cpp::Held held;\n") == "names il2cpp::");
  CHECK(runtime_outside_host("// il2cpp::Held and \"il2cpp.h\" in a comment\nint a;\n").empty());
  CHECK(runtime_outside_host("struct Il2CppExceptionWrapper;\nvoid* p_il2cpp;\n").empty());
  CHECK(!runtime_linked_by_mods("add_library(pmcore STATIC\n  core/il2cpp.cpp)\nadd_library(pmmod STATIC a.cpp)\n").empty());
  CHECK(!runtime_linked_by_mods("add_library(pmcore STATIC a.cpp)\nadd_library(pmmod STATIC host/il2cpp.cpp)\n").empty());
  CHECK(!runtime_linked_by_mods("add_library(pmmod STATIC a.cpp)\n").empty());
  CHECK(runtime_linked_by_mods("add_library(pmcore STATIC a.cpp)\nadd_library(pmmod STATIC b.cpp)\nadd_library(ModHost SHARED host/il2cpp.cpp)\n").empty());
}

// The host's menu code reads no mouse: no mouse_position or mouse_left_pressed in host/, the scan finding a use but not a
// mention in a comment or another name that contains them.
void no_manual_mouse_polling_remains(const fs::path& root) {
  CHECK(mouse_polling_in_host("bool p = hostgame::mouse_position(x, y);\n") == "uses mouse_position");
  CHECK(mouse_polling_in_host("if (game::mouse_left_pressed ()) {}\n") == "uses mouse_left_pressed");
  CHECK(mouse_polling_in_host("bool mouse_left_pressed();\n") == "uses mouse_left_pressed");
  CHECK(mouse_polling_in_host("// mouse_position() was read here\nint a;\n").empty());
  CHECK(mouse_polling_in_host("bool my_mouse_position_cache;\nvoid read_mouse_positions();\n").empty());
  CHECK(mouse_polling_in_host("void* m = phasmo::Mouse::get_current();\n") == "uses Mouse::");
  CHECK(mouse_polling_in_host("auto at = phasmo::Pointer::get_position(m);\n") == "uses Pointer::get_position");
  CHECK(mouse_polling_in_host("bool b = phasmo::ButtonControl::get_wasPressedThisFrame(c);\n") == "uses ButtonControl::");
  CHECK(mouse_polling_in_host("// phasmo::Mouse::get_current()\nstruct NoMouse::Inner;\nvoid MyPointer::get_position();\n").empty());
  const std::vector<std::string> problems = mouse_polling_in_hosts(root);
  CHECK(problems.empty());
  for (const std::string& problem : problems) std::printf("  %s\n", problem.c_str());
}

void cores_game_in_the_host_is_found() {
  CHECK(core_game_in_host("#include \"game.h\"\n") == "includes game.h");
  CHECK(core_game_in_host("  # include <core/game.h>\n") == "includes game.h");
  CHECK(core_game_in_host("void f() { game::alive(x); }\n") == "names game::");
  CHECK(core_game_in_host("void f() { pm::game::alive(x); }\n") == "names game::");
  CHECK(core_game_in_host("// game::alive and \"game.h\" in a comment\nint a;\n").empty());
  CHECK(core_game_in_host("#include \"host_game.h\"\nvoid f() { hostgame::alive(x); }\n").empty());
  CHECK(core_game_in_host("#include \"game_map.h\"\n#include \"game_api.h\"\n").empty());
}

using Table = std::map<std::string, int>;  // path to site count

/** What is wrong with `now` against `baseline`, one message per file: the map notes' ratchet, until Task 14 Step 1d's sweep. */
std::vector<std::string> ratchet(const Table& now, const Table& baseline) {
  std::vector<std::string> problems;
  for (const auto& [file, count] : now) {
    const auto it = baseline.find(file);
    if (it == baseline.end())
      problems.push_back(file + ": " + std::to_string(count) + " raw sites, and the baseline has no line for it");
    else if (count > it->second)
      problems.push_back(file + ": " + std::to_string(count) + " raw sites, up from " + std::to_string(it->second));
    else if (count < it->second)
      problems.push_back(file + ": " + std::to_string(count) + " raw sites, down from " + std::to_string(it->second) +
                         "; lower its baseline line in this commit");
  }
  for (const auto& [file, count] : baseline)
    if (!now.count(file))
      problems.push_back(file + ": no raw sites any more (or no file); remove its baseline line, which says " +
                         std::to_string(count));
  return problems;
}

/** The baseline's lines, "<path> <count>". A line that is not that goes to `errors`, by number. */
Table parse_table(const std::string& text, std::vector<std::string>& errors) {
  Table table;
  std::istringstream in(text);
  std::string line;
  for (int number = 1; std::getline(in, line); ++number) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    std::istringstream fields(line);
    std::string file, extra;
    int count = 0;
    if (!(fields >> file)) continue;  // a blank line
    if (!(fields >> count) || count < 0 || (fields >> extra)) {
      errors.push_back("baseline line " + std::to_string(number) + ": expected \"<path> <count>\", found \"" + line + "\"");
      continue;
    }
    table[file] = count;
  }
  return table;
}

/** A baseline the test wrote itself: any malformed line is a failure of the test. */
Table table_of(const std::string& text) {
  std::vector<std::string> errors;
  const Table table = parse_table(text, errors);
  CHECK(errors.empty());
  return table;
}

/** Every raw site is a failure: one message per file that has any. */
std::vector<std::string> enforce(const Table& now) {
  std::vector<std::string> problems;
  for (const auto& [file, count] : now)
    problems.push_back(file + ": " + std::to_string(count) + " raw sites; reach the game through the map (sdk/README.md)");
  return problems;
}

/** Each file's count under mods/, core/ and host/, without the exempt files and the files at 0. */
Table scan_tree(const fs::path& root) {
  Table table;
  for (const char* dir : {"mods", "core", "host"})
    for (const auto& entry : fs::recursive_directory_iterator(root / dir)) {
      const auto ext = entry.path().extension();
      if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h" && ext != ".asm")) continue;
      const std::string relative = fs::relative(entry.path(), root).generic_string();
      if (exempt_file(relative)) continue;
      const int total = scan_text(read_file(entry.path()), relative).total();
      if (total > 0) table[relative] = total;
    }
  return table;
}

void scan_counts_each_kind(const fs::path& root) {
  const std::string source = read_file(root / "tests/fixtures/raw_scan/kinds.cpp.fixture");
  CHECK(!source.empty());
  const Counts counts = scan_text(source, "mods/example/kinds.cpp");
  std::printf("  hex %d, non-ASCII %d, il2cpp %d, field %d, hook %d, find %d\n", counts.hex, counts.nonAscii,
              counts.il2cpp, counts.field, counts.hook, counts.find);
  CHECK(counts.hex == 4);       // 0x1400, 0x1400ull, 0x1'0000, 0x290u; not 0xFF, 0x20u, the exempt mask, a comment, a string or a raw string
  CHECK(counts.nonAscii == 1);  // the obfuscated identifier
  CHECK(counts.il2cpp == 2);    // il2cpp::cls and p_il2cpp_thread_attach; not il2cpp::Held, a raw string or a continued comment
  CHECK(counts.field == 2);     // il2cpp::field< and static_get
  CHECK(counts.hook == 1);
  CHECK(counts.find == 3);  // FindObjectOfType, NameToLayer and Find; not Finder
  // The hook is allowed in the file that creates hooks, and nowhere else.
  CHECK(scan_text("void f() { MH_CreateHook(a, b, &c); }", "host/hooks.cpp").hook == 0);
  CHECK(scan_text("void f() { MH_CreateHook(a, b, &c); }", "host/other.cpp").hook == 1);
  // An assembly file's comments start with ';'.
  CHECK(scan_text("mov rax, 0x1400 ; 0x2000\n", "mods/example/stub.asm").hex == 1);
  // MASM hex counts from 100h; a line ending in CRLF is read like one ending in LF.
  CHECK(scan_text("movss xmm0, dword ptr [rbx+290h]\r\nmov [rsp+24h], rax\r\n", "mods/example/stub.asm").hex == 1);
  CHECK(scan_text("int x = 290h;\n", "mods/example/stub.cpp").hex == 0);
}

void a_native_stub_in_a_mod_is_found() {
  CHECK(native_in_mod("mods/example/steer_stub.asm", "") == "assembly");
  CHECK(native_in_mod("mods/example/any.asm", "") == "assembly");
  CHECK(native_in_mod("mods/example/detour_stub.cpp", "int x;\n") == "named as a native stub");
  CHECK(native_in_mod("mods/example/Stub.h", "int x;\n") == "named as a native stub");
  CHECK(native_in_mod("mods/example/stubs-patch.cpp", "int x;\n") == "named as a native stub");
  CHECK(native_in_mod("mods/example/stubborn.cpp", "int x;\n").empty());  // a word that only begins the same
  CHECK(native_in_mod("mods/example/patch.cpp", "NtProtectVirtualMemory(h, &at, &n, kRwx, &old);\n") ==
        "names NtProtectVirtualMemory");
  CHECK(native_in_mod("mods/example/patch.cpp", "VirtualProtect(at, 8, kRwx, &old);\n") == "names VirtualProtect");
  CHECK(native_in_mod("mods/example/patch.cpp", "use(map::UnityPlayer_SteerSite);\r\n") == "names UnityPlayer_SteerSite");
  CHECK(native_in_mod("mods/example/patch.cpp", "use(map::GameAssembly_il2cpp_shutdown);\n") == "names GameAssembly_il2cpp_shutdown");
  // Not in a comment or a string, and not a name that only begins the same.
  CHECK(native_in_mod("mods/example/fine.cpp", "// VirtualProtect\nconst char* s = \"VirtualAlloc\";\nint VirtualProtected;\n").empty());
  CHECK(native_in_mod("mods/example/fine.h", "phasmo::NavMeshAgent::steer(agent, 1, 0);\n").empty());
}

// Any raw site fails, in a file of its own or one with others, whatever its count: there is no baseline.
void scan_fails_on_any_raw_site(const fs::path& root) {
  const std::string source = read_file(root / "tests/fixtures/raw_scan/kinds.cpp.fixture");
  const int total = scan_text(source, "mods/example/kinds.cpp").total();
  CHECK(total > 0);
  const auto problems = enforce(Table{{"mods/example/kinds.cpp", total}, {"core/example.cpp", 1}});
  CHECK(problems.size() == 2 && problems[0].find("core/example.cpp: 1 raw sites") == 0 &&
        problems[1].find("mods/example/kinds.cpp") == 0);
  CHECK(enforce(Table{}).empty());
  // The notes' ratchet still reads a baseline (Step 1d).
  const auto rose = ratchet(Table{{"sdk/map/A.h", 2}}, table_of("sdk/map/A.h 1\n"));
  CHECK(rose.size() == 1 && rose[0].find("up from") != std::string::npos);
}

void baseline_reports_a_malformed_line() {
  std::vector<std::string> errors;
  const Table table = parse_table("mods/a.cpp 3\n\nmods/b.cpp three\nmods/c.cpp 4 extra\nmods/d.cpp 5\r\n", errors);
  CHECK(table.size() == 2 && table.at("mods/a.cpp") == 3 && table.at("mods/d.cpp") == 5);
  CHECK(errors.size() == 2 && errors[0].find("line 3") != std::string::npos && errors[1].find("line 4") != std::string::npos);
}

}  // namespace

// Addresses in the SDK's documentation, which cites map keys instead (the user's rule of 2026-10-30): each offset (+0x..)
// and each hex number of 0x100 or more, on a comment line of a source in sdk/map, sdk/api, sdk/examples, mods, host or core, and on any line
// of a page in sdk/README.md, sdk/systems, sdk/guides, sdk/internals or sdk/examples. A line that says
// "pm:not-game-data: <why>" (in Markdown as an HTML comment) is not counted. Older notes still hold some until they are
// cited by key; tests/note_addresses_baseline.txt keeps each file's count from rising, as the raw sites' baseline does.
int note_addresses(const std::string& source, bool everyLine = false) {
  static const std::regex kAddress(R"(\+0x[0-9A-Fa-f]+|\b0x[0-9A-Fa-f]{3,})");
  int count = 0;
  std::istringstream in(source);
  std::string line;
  while (std::getline(in, line)) {
    if (line.find("pm:not-game-data:") != std::string::npos) continue;
    const size_t start = line.find_first_not_of(" \t");
    const bool comment = start != std::string::npos && (line.compare(start, 2, "//") == 0 || line.compare(start, 2, "/*") == 0 || line[start] == '*');
    if (!everyLine && !comment) continue;
    count += static_cast<int>(std::distance(std::sregex_iterator(line.begin(), line.end(), kAddress), std::sregex_iterator()));
  }
  return count;
}

/** Each documentation file's count of addresses, without the files at 0. */
Table scan_notes(const fs::path& root) {
  Table table;
  auto add = [&](const fs::path& file) {
    const std::string ext = file.extension().string();
    const bool page = ext == ".md";
    if (!page && ext != ".h" && ext != ".cpp") return;
    const int count = note_addresses(read_file(file), page);
    if (count > 0) table[fs::relative(file, root).generic_string()] = count;
  };
  for (const char* folder : {"sdk/map", "sdk/api", "sdk/systems", "sdk/guides", "sdk/internals", "sdk/examples", "mods", "host", "core"})
    if (fs::exists(root / folder))
      for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder))
        if (entry.is_regular_file()) add(entry.path());
  if (fs::exists(root / "sdk/README.md")) add(root / "sdk/README.md");
  return table;
}

void notes_count_addresses() {
  CHECK(note_addresses("/// Its one field (+0x118 in build 25811160).\n") == 1);
  CHECK(note_addresses("/// Inferred: the documented API (0x3D1C2E0).\nconstexpr int k = 0x1234;\n") == 1);
  CHECK(note_addresses("/// GhostAI.ChangeState stores it. Unresolved: 0x0.\n") == 0);
  CHECK(note_addresses("/// Its virtual slot (vtable +0x188), a byte compare with 0x80.\n") == 1);
  CHECK(note_addresses("/// Its slot is 0x188.\n") == 1);
  CHECK(note_addresses("`GoToPage(int)` (0x22C3140) turns to a page\n", true) == 1);
  CHECK(note_addresses("`GoToPage(int)` (0x22C3140) turns to a page\n") == 0);  // a source line that is not a comment
  CHECK(note_addresses("A raw site is a hex literal of 0x100 or more <!-- pm:not-game-data: the scan's rule -->\n", true) == 0);
  CHECK(note_addresses(" * as the game's own test (0x214FD80 in build 25811160)\n") == 1);  // a block comment's lines
  CHECK(note_addresses("/** at 0x214FD80 */\n") == 1);
}
// A DLL loaded in this process may be a mod, anybody's file, so it loads through core/quiet_load.h, where a bad image cannot
// put up a blocking dialog. The launcher's remote load names LoadLibraryW only as a string: it loads the host's own DLL.
void dlls_load_only_through_the_quiet_load(const fs::path& root) {
  static const std::regex call(R"(\bLoadLibrary(Ex)?[AW]?\s*\()");
  for (const char* folder : {"core", "host", "sdk", "tools", "mods"})
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder)) {
      const fs::path& file = entry.path();
      if (!entry.is_regular_file() || (file.extension() != ".cpp" && file.extension() != ".h")) continue;
      if (file.filename() == "quiet_load.h") continue;
      if (std::regex_search(read_file(file), call)) {
        std::printf("FAIL %s loads a DLL without core/quiet_load.h\n", file.string().c_str());
        ++g_failures;
      }
    }
}

// Every mod treats loading the same, and no mod does any matching of its own: the host's list is the lobby's, and the loader
// keeps everyone to it. The two calls that could only ask about other players stay in the contract so 2.x mods load.
void no_mod_has_matching_logic(const fs::path& root) {
  static const std::regex asks(R"(\b(room_agrees|peer_has_mod|peer_has)\b)");
  for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / "mods")) {
    const fs::path& file = entry.path();
    if (!entry.is_regular_file() || (file.extension() != ".cpp" && file.extension() != ".h")) continue;
    if (std::regex_search(read_file(file), asks)) {
      std::printf("FAIL %s asks about other players' mods: matching is the loader's\n", file.string().c_str());
      ++g_failures;
    }
  }
}

// The old prefix's names, which the rename to pm took out (stage 2b Task 14 Step 0). Built from parts, so this file holds none.
const std::string kOld = std::string("p") + "g";
const std::regex& old_prefix() {
  static const std::regex re("(?:^|[^A-Za-z0-9_])" + kOld + "::|(?:^|[^A-Za-z0-9])P" + kOld.substr(1) + "[A-Z]|" +
                             "(?:^|[^A-Za-z0-9]|[-/]D)P" + std::string("G") + "_|" + kOld + "mod|" + kOld + ":not-game-data");
  return re;
}

// The old prefix's names in `text`, line by line: "<line number>: <line>" for each. A line marked "pm:old-name: <why>" is the
// host's fallback for a mod built before the rename, and is allowed.
std::vector<std::string> old_names_in(const std::string& text) {
  std::vector<std::string> found;
  std::istringstream lines(text);
  std::string line;
  for (int number = 1; std::getline(lines, line); ++number)
    if (std::regex_search(line, old_prefix()) && line.find("pm:old-name:") == std::string::npos)
      found.push_back(std::to_string(number) + ": " + line);
  return found;
}

// No old name is left in the code, the scripts or the docs authors read. core/mod_exports.h is the one place the old export
// prefix is written, for the host's fallback. The plans and specs under docs/superpowers are history, and are not read.
void no_old_prefix_left(const fs::path& root) {
  CHECK(!old_names_in(kOld + "::api::bind();").empty());
  CHECK(!old_names_in("P" + kOld.substr(1) + "HostApi api;").empty());
  CHECK(!old_names_in("-DP" + std::string("G") + "_HOST").empty());
  CHECK(!old_names_in(kOld + "mod_info").empty());
  CHECK(old_names_in(kOld + "mod_info  // pm:old-name: a test").empty());
  CHECK(old_names_in("const auto* pm = Spg::x; PNG_ok; ping_me").empty());
  std::vector<fs::path> files;
  for (const char* folder : {"core", "host", "mods", "sdk", "tests", "tools", ".github"})
    if (fs::exists(root / folder))
      for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder))
        if (entry.is_regular_file()) files.push_back(entry.path());
  for (const char* file : {"CMakeLists.txt", "build.ps1", "validate.ps1", "package.ps1", "README.md", "CLAUDE.md"})
    if (fs::exists(root / file)) files.push_back(root / file);
  for (const fs::directory_entry& entry : fs::directory_iterator(root / "docs"))
    if (entry.is_regular_file() && entry.path().extension() == ".md") files.push_back(entry.path());
  static const std::set<std::string> kText{".cpp", ".h", ".hpp", ".asm", ".txt", ".md", ".ps1", ".psd1", ".cmake", ".json", ".yml", ".mjs", ".rc"};
  for (const fs::path& file : files) {
    if (!kText.count(file.extension().string()) || file.filename() == "mod_exports.h") continue;
    for (const std::string& line : old_names_in(read_file(file))) {
      std::printf("FAIL %s:%s: an old prefix's name (the prefix is pm)\n", fs::relative(file, root).string().c_str(), line.c_str());
      ++g_failures;
    }
  }
}

// Whether a line holds a Malayalam character, the script of the game's obfuscated names (UTF-8 lead E0 B4 or E0 B5).
bool has_obfuscated_name(const std::string& line) {
  for (size_t i = 0; i + 1 < line.size(); ++i)
    if (static_cast<unsigned char>(line[i]) == 0xE0 &&  // pm:not-game-data: a UTF-8 lead byte
        (static_cast<unsigned char>(line[i + 1]) == 0xB4 || static_cast<unsigned char>(line[i + 1]) == 0xB5))  // pm:not-game-data: UTF-8
      return true;
  return false;
}

// The game's obfuscated names are shuffled on every update (the user, 2026-11-03), so the map, its notes and the docs describe an
// obfuscated member by its shape and function, never by its name: one means nothing after the next update. No exception, an
// entry's hint included (the user removed them all, 2026-11-03). Test fixtures stand for a dump, and are not read.
void no_obfuscated_name_in_the_docs(const fs::path& root) {
  CHECK(has_obfuscated_name("class \xE0\xB4\x9D\xE0\xB4\xB8"));
  CHECK(!has_obfuscated_name("class GhostWanderState // caf\xC3\xA9"));
  std::vector<fs::path> files;
  for (const char* folder : {"sdk/systems", "sdk/internals", "sdk/guides", "sdk/examples", "sdk/map", "sdk/api"})
    if (fs::exists(root / folder))
      for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder))
        if (entry.is_regular_file()) files.push_back(entry.path());
  for (const char* file : {"sdk/README.md", "README.md", "CLAUDE.md", "AI_POLICY.md"})
    if (fs::exists(root / file)) files.push_back(root / file);
  for (const fs::directory_entry& entry : fs::directory_iterator(root / "docs"))
    if (entry.is_regular_file() && entry.path().extension() == ".md") files.push_back(entry.path());
  for (const fs::path& file : files) {
    std::istringstream lines(read_file(file));
    std::string line;
    for (int number = 1; std::getline(lines, line); ++number) {
      if (!has_obfuscated_name(line)) continue;
      std::printf("FAIL %s:%d names an obfuscated member; describe it by its shape and function\n",
                  fs::relative(file, root).generic_string().c_str(), number);
      ++g_failures;
    }
  }
}

// The vanilla-UI rule: no mod or host code draws a window of its own. The canvases that remain wait for the author's design
// (FINDINGS.md 31) and are listed here; a new caller of ui::create_canvas fails, so it goes to the author as a question first.
void no_unapproved_canvas(const fs::path& root) {
  static const std::set<std::string> kWaiting{"core/notice.cpp", "mods/playable_ghost/hud.cpp", "mods/playable_ghost/panorama.cpp",
                                              "mods/playable_ghost/perceive_view.cpp", "mods/playable_ghost/picker.cpp"};
  // Canvases the author designed and asked for (the design names each): not windows waiting for a design.
  static const std::set<std::string> kAuthorDesigned{
      "mods/emotes/wheel.cpp",  // the emote wheel (docs/superpowers/specs/2026-11-10-emotes-design.md, "The wheel")
      "mods/dev_menu/menu.cpp",  // the dev menu's plain rows, no vanilla dressing by the author's decision 6
                                 // (docs/superpowers/specs/2026-11-10-dev-menu-content-plan.md, Decisions)
  };
  static const std::regex call(R"(\bcreate_canvas\s*\()");
  std::set<std::string> found;
  for (const char* folder : {"core", "host", "mods"})
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder)) {
      const fs::path& file = entry.path();
      if (!entry.is_regular_file() || file.extension() != ".cpp") continue;
      const std::string relative = fs::relative(file, root).generic_string();
      if (relative == "core/ui.cpp") continue;  // where it is defined
      std::istringstream lines(read_file(file));
      std::string line;
      while (std::getline(lines, line)) {
        const size_t comment = line.find("//");
        if (std::regex_search(comment == std::string::npos ? line : line.substr(0, comment), call)) found.insert(relative);
      }
    }
  for (const std::string& file : found)
    if (!kWaiting.count(file) && !kAuthorDesigned.count(file)) {
      std::printf("FAIL %s draws a window of its own; use the game's UI, or ask the author first\n", file.c_str());
      ++g_failures;
    }
  for (const std::string& file : kWaiting)
    if (!found.count(file)) {
      std::printf("FAIL %s no longer draws a canvas: take it off kWaiting and FINDINGS.md 31\n", file.c_str());
      ++g_failures;
    }
  for (const std::string& file : kAuthorDesigned)
    if (!found.count(file)) {
      std::printf("FAIL %s no longer draws a canvas: take it off kAuthorDesigned\n", file.c_str());
      ++g_failures;
    }
}

// The method a line lies in: the last line above it that starts a definition at the first column, up to its parameters.
std::string function_around(const std::string& text, size_t at) {
  size_t line = text.rfind('\n', at);
  while (line != std::string::npos && line > 0) {
    const size_t before = text.rfind('\n', line - 1);
    const size_t start = before == std::string::npos ? 0 : before + 1;
    const std::string head = text.substr(start, line - start);
    if (!head.empty() && head[0] != ' ' && head[0] != '/' && head[0] != '#' && head[0] != '}' && head.find('(') != std::string::npos)
      return head.substr(0, head.find('('));
    if (before == std::string::npos) break;
    line = before;
  }
  return {};
}

// Who loads and starts a mod, by name: `what` called anywhere in the host's or the tools' code must lie in a function ending
// in one of `allowed`.
void only_called_from(const fs::path& root, const std::regex& what, const char* name, const std::vector<std::string>& allowed) {
  for (const char* folder : {"host", "core", "sdk/api", "sdk/runtime"})
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / folder)) {
      const fs::path& file = entry.path();
      if (!entry.is_regular_file() || (file.extension() != ".cpp" && file.extension() != ".h")) continue;
      const std::string text = read_file(file);
      for (auto it = std::sregex_iterator(text.begin(), text.end(), what); it != std::sregex_iterator(); ++it) {
        const size_t at = static_cast<size_t>(it->position());
        const size_t lineStart = text.rfind('\n', at) + 1;
        if (text.compare(lineStart, 2, "//") == 0 || text.substr(lineStart, at - lineStart).find("//") != std::string::npos) continue;
        const std::string around = function_around(text, at);
        const bool fine = std::any_of(allowed.begin(), allowed.end(), [&](const std::string& a) { return around.ends_with(a); });
        if (!fine) {
          std::printf("FAIL %s calls %s in \"%s\": no mod's DLL may load or start outside the player's agreement's path\n",
                      file.string().c_str(), name, around.c_str());
          ++g_failures;
        }
      }
    }
}

// No mod code runs before the player agrees (host/mod_session.h). The tests call the mods' API directly, so what keeps a later
// call out of the host's start is this.
void mods_load_and_start_only_after_agreement(const fs::path& root) {
  only_called_from(root, std::regex(R"(\bdll::load_mod\s*\()"), "dll::load_mod", {"load_dll"});
  only_called_from(root, std::regex(R"(\bmods::start\s*\()"), "mods::start", {"start_mods"});
  // Inside host/mods.cpp the namespace is left out: restart's own call, and nothing else there.
  const std::string mods = read_file(root / "host/mods.cpp");
  static const std::regex plain(R"(\n[ ]+start\(api, )");
  int calls = 0;
  for (auto it = std::sregex_iterator(mods.begin(), mods.end(), plain); it != std::sregex_iterator(); ++it) {
    ++calls;
    const std::string around = function_around(mods, static_cast<size_t>(it->position()) + 1);
    if (!around.ends_with("restart")) {
      std::printf("FAIL host/mods.cpp starts the mods in \"%s\", not only in restart\n", around.c_str());
      ++g_failures;
    }
  }
  CHECK(calls == 1);
  // The one caller of start_mods is what mod_session::begin runs on "I agree".
  const std::string suite = read_file(root / "host/suite.cpp");
  static const std::regex startMods(R"(\bstart_mods\()");
  int starts = 0;
  for (auto it = std::sregex_iterator(suite.begin(), suite.end(), startMods); it != std::sregex_iterator(); ++it) ++starts;
  CHECK(starts == 2);  // its definition, and the session's steps
  CHECK(suite.find("[] { start_mods(mods::chosen()); }") != std::string::npos);
}

// Before the player answers, the host's only hook is the frame's: suite::start makes none, and the worker makes that one. What
// goes in on "I agree" is reached only from the session's steps. suite.cpp has no unit test, so this keeps it so.
void the_host_hooks_only_the_frame_before_the_answer(const fs::path& root) {
  static const std::regex hooking(R"(\b(create_hooks?|enable_all|hook<|hooks::create)\b)");
  const std::string suite = read_file(root / "host/suite.cpp");
  const size_t start = suite.find("bool start(void* gameAssembly"), tick = suite.find("bool tick() {");
  CHECK(start != std::string::npos && tick != std::string::npos && start < tick);
  const std::string body = suite.substr(start, tick - start);
  CHECK(!std::regex_search(body, hooking));
  CHECK(suite.find("[] { start_mods(mods::chosen()); },\n                                     &install_required_hooks}") != std::string::npos);
  CHECK(suite.find("const mod_session::Steps steps{&install_session_hooks, &hooks::mark,") != std::string::npos);
  const std::string main = read_file(root / "host/main.cpp");
  const size_t install = main.find("bool install_hooks() {"), worker = main.find("DWORD WINAPI worker(void*) {");
  CHECK(install != std::string::npos && worker != std::string::npos && install < worker);
  const std::string made = main.substr(install, worker - install);
  CHECK(made.find("hooks::create") != std::string::npos && made.find("PhotonHandler.LateUpdate") != std::string::npos);
  CHECK(made.find("suite::") == std::string::npos && made.find("hostnet::") == std::string::npos);
  // Declining takes the frame hook off whatever stopping the suite does: the stop can't throw past the removal.
  const size_t aside = main.find("void stand_aside() {"), asideEnd = main.find("\n}\n", aside);
  CHECK(aside != std::string::npos && asideEnd != std::string::npos);
  const std::string stands = main.substr(aside, asideEnd - aside);
  const size_t guarded = stands.find("try {"), release = stands.find("main_thread_release();"), caught = stands.find("catch (...)"),
               rollback = stands.find("hooks::rollback_to(0);");
  CHECK(guarded != std::string::npos && guarded < release && release < caught && caught < rollback);
}

int main(int argc, char** argv) {
  const fs::path root = PM_SOURCE_DIR;
  if (argc == 2 && std::strcmp(argv[1], "--print") == 0) {
    for (const auto& [file, count] : scan_tree(root)) std::printf("%s %d\n", file.c_str(), count);
    return 0;
  }
  if (argc == 2 && std::strcmp(argv[1], "--print-notes") == 0) {
    for (const auto& [file, count] : scan_notes(root)) std::printf("%s %d\n", file.c_str(), count);
    return 0;
  }
  scan_counts_each_kind(root);
  scan_fails_on_any_raw_site(root);
  baseline_reports_a_malformed_line();
  a_native_stub_in_a_mod_is_found();
  notes_count_addresses();
  the_runtime_outside_the_host_is_found();
  cores_game_in_the_host_is_found();
  no_manual_mouse_polling_remains(root);
  dlls_load_only_through_the_quiet_load(root);
  mods_load_and_start_only_after_agreement(root);
  the_host_hooks_only_the_frame_before_the_answer(root);
  no_mod_has_matching_logic(root);
  no_old_prefix_left(root);
  no_obfuscated_name_in_the_docs(root);
  no_unapproved_canvas(root);
  // The host's restart of the mods from the panel waits until it is out of the room, whatever the panel hands it.
  CHECK(read_file(root / "host/suite.cpp").find("mods_panel::take_choice(choice) && hostgame::out_of_room()") != std::string::npos);
  for (const std::string& problem : core_game_in_hosts(root)) {
    std::printf("FAIL %s\n", problem.c_str());
    ++g_failures;
  }
  for (const std::string& problem : runtime_in_mods(root)) {
    std::printf("FAIL %s\n", problem.c_str());
    ++g_failures;
  }
  for (const std::string& problem : runtime_linked_by_mods(read_file(root / "CMakeLists.txt"))) {
    std::printf("FAIL %s\n", problem.c_str());
    ++g_failures;
  }
  for (const std::string& problem : native_in_mods(root)) {
    std::printf("FAIL %s\n", problem.c_str());
    ++g_failures;
  }
  for (const std::string& problem : enforce(scan_tree(root))) {
    std::printf("FAIL %s\n", problem.c_str());
    ++g_failures;
  }
  std::vector<std::string> errors;
  const Table notesBaseline = parse_table(read_file(root / "tests/note_addresses_baseline.txt"), errors);
  for (const std::string& error : errors) {
    std::printf("FAIL notes: %s\n", error.c_str());
    ++g_failures;
  }
  for (const std::string& problem : ratchet(scan_notes(root), notesBaseline)) {
    std::printf("FAIL notes: %s\n", problem.c_str());
    ++g_failures;
  }
  return g_failures ? 1 : 0;
}
