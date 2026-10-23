// Every managed object the host and the mods keep between frames must be an il2cpp::Held (see
// core/il2cpp.h): the GC does not scan this code's memory. This test reads the sources and fails on a
// raw void* (or std::vector<void*>) declared at namespace or struct scope, the places that outlive a
// call. Pointers that are not managed objects (code addresses, views of runtime memory read in one
// call) are listed in kNotObjects with why.
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"

namespace {
namespace fs = std::filesystem;

struct Exempt {
  const char* file;  // relative to the source root, forward slashes
  const char* name;
  const char* why;
};
const Exempt kNotObjects[] = {
    {"core/il2cpp.h", "ex", "the layout of the runtime's exception wrapper"},
    {"core/il2cpp.h", "klass", "the layout of a managed object's header"},
    {"core/il2cpp.h", "monitor", "the layout of a managed object's header"},
    {"core/il2cpp.h", "bounds", "the layout of a managed array"},
    {"core/il2cpp.h", "object_", "Held's own storage, kept with its handle"},
    {"core/mod_api.h", "gameAssembly", "a module handle in the host's C interface"},
    {"core/mod_api.h", "unityPlayer", "a module handle in the host's C interface"},
    {"core/mod_api.h", "klass", "a game entry's Il2CppClass*, runtime metadata"},
    {"core/mod_api.h", "code", "a game entry's code address"},
    {"core/mod_api.h", "field", "a game entry's FieldInfo*, runtime metadata"},
    {"host/game_map.cpp", "twins", "a resolved method's twins' code addresses"},
    {"core/il2cpp.cpp", "g_thread", "the runtime's record of the attached worker thread, kept by the runtime"},
    {"core/game.cpp", "handlerLateUpdate", "a method's code address, hooked"},
    {"core/game.cpp", "changeState", "a method's code address, hooked"},
    {"core/game.cpp", "normalInteraction", "a method's code address"},
    {"core/game.cpp", "twinInteraction", "a method's code address"},
    {"core/game.cpp", "tryInteract", "a method's code address, hooked"},
    {"core/game.cpp", "randomRangeInt", "a method's code address, hooked"},
    {"host/hooks.cpp", "target", "a hooked function's code address"},
    {"host/hooks.cpp", "at", "a hooked function's code address"},
    {"host/hooks.cpp", "detour", "a detour's code address"},
    {"host/hooks.cpp", "earlier", "a detour's code address"},
    {"host/entry_hooks.h", "detour", "a tracking detour's code address"},
    {"host/net.cpp", "onEvent", "a method's code address, hooked"},
    {"mods/playable_ghost/equipment.cpp", "addLoadout", "a method's code address, hooked"},
    {"mods/playable_ghost/equipment.cpp", "addItem", "a method's code address, hooked"},
    {"mods/playable_ghost/senses.cpp", "unmute", "a method's code address, hooked"},
    {"mods/playable_ghost/marks.cpp", "noise", "the layout of GhostHearing's sound entry, read in one call"},
    {"mods/playable_ghost/round.cpp", "sender", "the layout of PhotonMessageInfo, read in one call"},
    {"mods/playable_ghost/round.cpp", "photonView", "the layout of PhotonMessageInfo, read in one call"},
    {"mods/playable_ghost/journal.cpp", "openCloseJournal", "a method's code address, hooked"},
    {"mods/playable_ghost/leave.cpp", "pauseLeave", "a method's code address, hooked"},
    {"mods/playable_ghost/leave.cpp", "pauseQuit", "a method's code address, hooked"},
    {"mods/playable_ghost/leave.cpp", "shutdown", "GameAssembly's il2cpp_shutdown export, hooked"},
};

struct Hit {
  std::string file, name;
  int line;
};

/** Source text with comments, literals and preprocessor lines blanked (line breaks kept). */
std::string code_only(const std::string& s) {
  std::string out(s);
  bool lineStart = true;
  for (size_t i = 0; i < out.size(); ++i) {
    if (out[i] == '\n') {
      lineStart = true;
      continue;
    }
    if (lineStart && out[i] == '#') {  // a directive, with its continuation lines
      bool continued = false;          // the line so far ends in a backslash
      for (; i < out.size(); ++i) {
        if (out[i] == '\n') {
          if (!continued) break;
          continued = false;
          continue;
        }
        if (out[i] != ' ' && out[i] != '\t' && out[i] != '\r') continued = out[i] == '\\';
        out[i] = ' ';
      }
      --i;
      continue;
    }
    if (out[i] != ' ' && out[i] != '\t' && out[i] != '\r') lineStart = false;
    if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '/') {
      while (i < out.size() && out[i] != '\n') out[i++] = ' ';
    } else if (out[i] == '/' && i + 1 < out.size() && out[i + 1] == '*') {
      for (; i + 1 < out.size() && !(out[i] == '*' && out[i + 1] == '/'); ++i)
        if (out[i] != '\n') out[i] = ' ';
      if (i + 1 < out.size()) out[i] = out[i + 1] = ' ';
    } else if (out[i] == 'R' && i + 2 < out.size() && out[i + 1] == '"' && out[i + 2] == '(') {
      const size_t end = out.find(")\"", i + 3);
      for (size_t j = i; j < (end == std::string::npos ? out.size() : end + 2); ++j)
        if (out[j] != '\n') out[j] = ' ';
    } else if (out[i] == '"' || out[i] == '\'') {
      const char quote = out[i];
      for (++i; i < out.size() && out[i] != quote; ++i) {
        if (out[i] == '\\') out[i++] = ' ';
        if (i < out.size() && out[i] != '\n') out[i] = ' ';
      }
    }
  }
  return out;
}

bool opens_data_scope(const std::string& header) {
  static const std::regex kData(R"(\b(struct|class|union|namespace)\b[^()]*$|\bextern\s*"[^"]*"\s*$)");
  return std::regex_search(header, kData);
}

/** Raw pointers declared where they outlive a call: namespace scope and struct members. A statement
 *  can carry text left before its declaration (a macro call, a function body, an access label), so
 *  the declaration is matched after the last such boundary. */
void scan_text(const std::string& source, const std::string& relative, std::vector<Hit>& hits) {
  const std::string text = code_only(source);
  static const std::regex kRaw(R"((?:^|[)}:])\s*(?:(?:static|inline|thread_local|mutable)\s+)*(?:std::vector<\s*void\s*\*\s*>|void\s*\*)\s*(\w+)\s*(?:\[[^\]]*\]\s*)?(?:=[^;]*|\{[^;]*\})?\s*$)");
  std::vector<char> scopes;  // 'd' namespace or struct, 'c' function body or initialiser
  std::string statement;
  int line = 1, statementLine = 1;
  for (char c : text) {
    const bool data = scopes.empty() || scopes.back() == 'd';
    if (c == '\n') ++line;
    if (c == '{') {
      scopes.push_back(data && opens_data_scope(statement) ? 'd' : 'c');
      if (scopes.back() == 'd') statement.clear();
      else if (data) statement += c;
      continue;
    }
    if (c == '}') {
      const char closed = scopes.empty() ? 'd' : scopes.back();
      if (!scopes.empty()) scopes.pop_back();
      const bool nowData = scopes.empty() || scopes.back() == 'd';
      if (closed == 'd') statement.clear();
      else if (nowData) statement += c;
      continue;
    }
    if (!data) continue;
    if (c == ';') {
      std::smatch m;
      if (std::regex_search(statement, m, kRaw)) {
        const auto before = statement.begin() + m.position(1);
        hits.push_back({relative, m[1].str(), statementLine + static_cast<int>(std::count(statement.begin(), before, '\n'))});
      }
      statement.clear();
      continue;
    }
    if (statement.find_first_not_of(" \t\r\n") == std::string::npos) statementLine = line;
    statement += c;
  }
}

void scan(const fs::path& path, const std::string& relative, std::vector<Hit>& hits) {
  std::ifstream in(path, std::ios::binary);
  std::stringstream buffer;
  buffer << in.rdbuf();
  scan_text(buffer.str(), relative, hits);
}

/** The scan finds a kept raw pointer wherever its declaration sits, and nothing else. */
void scan_catches_known_cases() {
  const char* source = R"src(
#define LIST(x) \
  void* not_a_declaration;
API(void, first, (int)) API(void, second, (int))
void* afterMacro = nullptr;
void helper() { void* local = nullptr; (void)local; }
void* afterFunction;
std::vector<void*> list;
void* braced{};
void* array[3] = {};
extern "C" {
void* inExtern = nullptr;
}
class Kept {
 public:
  void method() {}
 private:
  void* member = nullptr;
  void* (*function)(void*) = nullptr;
  struct Inner { void* nested; } inner;
};
void* declaredFunction(int);
const char* text = "void* inString;";
)src";
  std::vector<Hit> hits;
  scan_text(source, "known.cpp", hits);
  std::set<std::string> found;
  for (const Hit& hit : hits) found.insert(hit.name);
  const std::set<std::string> expected{"afterMacro", "afterFunction", "list", "braced", "array",
                                       "inExtern", "member", "nested"};
  for (const std::string& name : found)
    if (!expected.count(name)) std::printf("  scan reported a non-declaration: %s\n", name.c_str());
  for (const std::string& name : expected)
    if (!found.count(name)) std::printf("  scan missed: %s\n", name.c_str());
  CHECK(found == expected);
}
}  // namespace

int main() {
  scan_catches_known_cases();
  const fs::path root = PG_SOURCE_DIR;
  std::vector<Hit> hits;
  for (const char* dir : {"core", "host", "mods/playable_ghost", "mods/afterlife", "mods/custom_ghosts",
                          "mods/spectral_flame", "mods/ghost_tells"})
    for (const auto& entry : fs::recursive_directory_iterator(root / dir)) {
      const auto ext = entry.path().extension();
      if (!entry.is_regular_file() || (ext != ".cpp" && ext != ".h")) continue;
      scan(entry.path(), fs::relative(entry.path(), root).generic_string(), hits);
    }
  std::set<std::string> used;
  for (const Hit& hit : hits) {
    bool exempt = false;
    for (const Exempt& e : kNotObjects)
      if (hit.file == e.file && hit.name == e.name) exempt = true;
    if (exempt) {
      used.insert(hit.file + ":" + hit.name);
      continue;
    }
    std::printf("FAIL %s:%d: %s is kept as a raw pointer; hold it with il2cpp::Held\n", hit.file.c_str(), hit.line,
                hit.name.c_str());
    ++g_failures;
  }
  for (const Exempt& e : kNotObjects)  // a stale exemption would hide the next pointer of that name
    CHECK(used.count(std::string(e.file) + ":" + e.name));
  return g_failures ? 1 : 0;
}
