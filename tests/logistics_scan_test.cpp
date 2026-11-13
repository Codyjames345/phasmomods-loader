// The loader owns the logistics (the user's rule of 2026-10-28, widened 2026-10-29): no mod invents its own handling of
// what the loader or the SDK provides. This scan fails on any of these in the .cpp and .h files under mods/:
//   - loading or finding other modules: LoadLibrary, GetProcAddress, GetModuleHandle;
//   - reading another mod's files: a string naming a DLL, a manifest.json, or another mod's folder;
//   - asking about other players' or other mods' state: peer_has_mod, peer_has, room_agrees, service and register_service,
//     but for the uses listed in kAllowed, each with its reason;
//   - the game's own RPC or raise-event paths, which would carry a mod's own data past the loader's one channel
//     (PhotonView::RPC, RPC_toPlayer, RaiseEvent, RaiseEventOptions, SendOptions, LoadBalancingClient), under the
//     names they go by as well: the map's keys in strings ("PhotonView.RPC") and the entries' names
//     (PhotonNetwork_RaiseEvent) - but for kAllowed;
//   - writing a file outside what the SDK gives (sdk/guides/mod-files.md): an ofstream, CreateFile, an fopen in a
//     write mode, or std::filesystem::write_file - a mod's settings go through sdk/api/settings.h and its data
//     under the host's data directory - but for kAllowed;
//   - another mod's id as a literal (a dependency is declared in the manifest, which this does not read as code);
//   - a mod's own presence, handshake or version exchange: an identifier naming presence or a handshake.
// Comments are not read. What a mod puts in its own messages on its own channel is its own (user, 2026-11-01).
// usage: logistics_scan_test
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

// The uses a mod keeps, each by file, the text its line holds, and why.
struct Allowed {
  const char* file;  // relative to the source root
  const char* text;  // found on the line
  const char* why;
};
const Allowed kAllowed[] = {
    {"mods/playable_ghost/mod.cpp", "register_service(PM_SERVICE_PLAYABLE_GHOST",
     "playable-ghost.v1, what is not content: the host asks whether a player controls the ghost (core/services.h)"},
    {"mods/dev_menu/actions.cpp", "std::ofstream file(g_settingsFile",
     "the menu's switch file, in the mod's own folder of the host's data directory (mods/dev_menu/mod.cpp names it)"},
};

std::string read_file(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/** `source` without its comments, line by line; string literals are kept. */
std::vector<std::string> code_lines(const std::string& source) {
  std::vector<std::string> out;
  std::istringstream in(source);
  std::string line;
  bool block = false;
  while (std::getline(in, line)) {
    std::string code;
    bool inString = false;
    for (size_t i = 0; i < line.size(); ++i) {
      const char c = line[i];
      if (block) {
        if (c == '*' && i + 1 < line.size() && line[i + 1] == '/') block = false, ++i;
        continue;
      }
      if (inString) {
        code += c;
        if (c == '\\' && i + 1 < line.size()) code += line[++i];
        else if (c == '"') inString = false;
        continue;
      }
      if (c == '"') inString = true;
      else if (c == '/' && i + 1 < line.size() && line[i + 1] == '/') break;
      else if (c == '/' && i + 1 < line.size() && line[i + 1] == '*') {
        block = true;
        ++i;
        continue;
      }
      code += c;
    }
    out.push_back(code);
  }
  return out;
}

/** The string literals on a line of code. */
std::vector<std::string> literals(const std::string& code) {
  std::vector<std::string> out;
  static const std::regex literal(R"re("((?:[^"\\]|\\.)*)")re");
  for (std::sregex_iterator it(code.begin(), code.end(), literal), end; it != end; ++it) out.push_back((*it)[1]);
  return out;
}

struct Mod {
  std::string folder;  // mods/<folder>
  std::string id;      // from its manifest.json
};

/** Every mod under mods/ with its manifest's id. */
std::vector<Mod> mods_in(const fs::path& root) {
  std::vector<Mod> out;
  static const std::regex id(R"re("id"\s*:\s*"([^"]+)")re");
  for (const fs::directory_entry& entry : fs::directory_iterator(root / "mods")) {
    if (!entry.is_directory()) continue;
    const std::string manifest = read_file(entry.path() / "manifest.json");
    std::smatch m;
    if (std::regex_search(manifest, m, id)) out.push_back({entry.path().filename().string(), m[1]});
  }
  return out;
}

/** What is wrong in one mod file, "<line>: <category>" each. `self` is the mod the file belongs to. */
std::vector<std::string> problems_in(const std::string& relative, const std::string& source, const Mod& self,
                                     const std::vector<Mod>& all) {
  static const std::regex modules(R"(\b(LoadLibrary(Ex)?[AW]?|GetProcAddress|GetModuleHandle(Ex)?[AW]?)\s*\()");
  static const std::regex asks(R"(\b(peer_has_mod|peer_has|room_agrees|register_service)\b|\bservice\s*\()");
  // Send paths by any name they go by: the C++ wrappers, the map's keys as they sit in strings, and the map's
  // entry names (an underscore or dot between the owner and the member, so PhotonNetwork_RaiseEvent counts).
  static const std::regex sends(R"((?:^|[^A-Za-z0-9])((?:PhotonView|PhotonNetwork)[_.:]{1,2}(?:RPC|RaiseEvent|RpcSecure)|RPC_toPlayer|RaiseEventOptions|RaiseEvent|SendOptions|LoadBalancingClient|RpcSecure))");
  static const std::regex chatter(R"(\b\w*([Hh]andshake|[Pp]resence)\w*\b)");
  // Files a mod opens to write: an output stream, CreateFile, an fopen-family open in a w, a or + mode, or a
  // filesystem write. Reading its own files (ifstream, "r" and "rb") is fine.
  static const std::regex writes(R"((\bofstream\b|std::filesystem::write_file\b|\bCreateFile(A|W|2)?\s*\(|\b\w*fopen\s*\([^)]*"(?:[wa]\+?b?|[rw]\+b?)))");
  std::vector<std::string> out;
  const std::vector<std::string> lines = code_lines(source);
  for (size_t n = 0; n < lines.size(); ++n) {
    const std::string& code = lines[n];
    const auto allowed = [&] {
      for (const Allowed& a : kAllowed)
        if (relative == a.file && code.find(a.text) != std::string::npos) return true;
      return false;
    };
    const auto add = [&](const char* what) { out.push_back(std::to_string(n + 1) + ": " + what); };
    if (std::regex_search(code, modules)) add("loads or finds a module; the loader loads mods");
    if (std::regex_search(code, asks) && !allowed()) add("asks about other players or mods; matching and dependencies are the loader's");
    if (std::regex_search(code, sends) && !allowed()) add("uses the game's RPC or event path; a mod's messages go through its channel");
    if (std::regex_search(code, writes) && !allowed())
      add("writes a file; a mod's settings go through sdk/api/settings.h and its data under the host's data directory (sdk/guides/mod-files.md)");
    if (std::regex_search(code, chatter)) add("names presence or a handshake; the loader announces who runs what");
    for (const std::string& text : literals(code)) {
      if (text.find(".dll") != std::string::npos || text.find("manifest.json") != std::string::npos)
        add("names a DLL or a manifest; a mod never reads another mod's files");
      for (const Mod& other : all) {
        if (other.folder == self.folder) continue;
        const std::string shortId = other.id.substr(other.id.find('.') + 1);
        if (text == other.id || text == shortId || text.find(other.folder) != std::string::npos)
          add("names another mod; a dependency is declared in the manifest");
      }
    }
  }
  return out;
}

// Each category fails on its own, and a comment or an allowed use does not.
void each_category_is_found() {
  const std::vector<Mod> all{{"alpha", "someone.alpha"}, {"beta", "someone.beta"}};
  const Mod& alpha = all[0];
  const auto found = [&](const std::string& source) { return problems_in("mods/alpha/x.cpp", source, alpha, all).size(); };
  CHECK(found("HMODULE m = GetModuleHandleW(L\"x\");\n") == 1);
  CHECK(found("auto f = GetProcAddress(m, \"f\");\n") == 1);
  CHECK(found("LoadLibraryExW(path, nullptr, 0);\n") == 1);
  CHECK(found("std::ifstream in(\"Beta.dll\");\n") == 1);
  CHECK(found("open(\"mods/beta/manifest.json\");\n") >= 1);
  CHECK(found("if (api->peer_has_mod(actor, \"x\")) {}\n") == 1);
  CHECK(found("const void* t = api->service(\"beta.v1\");\n") == 2);  // a service, named after another mod
  CHECK(found("api::hook<phasmo::PhotonView::RPC>(&detour, \"rpc\");\n") == 1);
  CHECK(found("phasmo::PhotonNetwork::RaiseEvent(1, data, options, 0);\n") == 1);
  // The same paths under the names they go by elsewhere: a map key in a string, an entry's name, and the
  // game's own RPC sender (the final-review item 8's send names).
  CHECK(found("const void* e = api->game_entry(\"PhotonView.RPC\");\n") == 1);
  CHECK(found("use(pm::map::PhotonNetwork_RaiseEvent);\n") == 1);
  CHECK(found("phasmo::PhotonNetwork::RPC(view, \"x\", 0);\n") == 1);
  CHECK(found("phasmo::PhotonView::RpcSecure(info);\n") == 1);
  // A mod writes files only where the SDK gives it (the final-review item 8's file writes): its settings file
  // through sdk/api/settings.h, and its data under the host's data directory. Reading its own files is fine.
  CHECK(found("std::ofstream out(path);\n") == 1);
  CHECK(found("HANDLE f = CreateFileW(path, 0, 0, nullptr, OPEN_ALWAYS, 0, nullptr);\n") == 1);
  CHECK(found("FILE* f = _wfopen(path, L\"w\");\n") == 1);
  CHECK(found("std::filesystem::write_file(path, text);\n") == 1);
  CHECK(found("std::ifstream in(path);\nFILE* f = fopen(path, \"rb\");\n") == 0);  // reads, not writes
  CHECK(found("// ofstream, CreateFileW, _wfopen, \"PhotonView.RPC\"\n/* RaiseEvent( */ int x;\n") == 0);  // not in a comment
  CHECK(found("bool on = mod_started(\"someone.beta\");\n") == 1);
  CHECK(found("bool on = mod_started(\"beta\");\n") == 1);
  CHECK(found("void send_presence();\n") == 1);
  CHECK(found("enum { kHandshake = 1 };\n") == 1);
  // Not in a comment, not its own id, and not a word that only contains one.
  CHECK(found("// GetProcAddress, peer_has_mod, RaiseEvent, presence\n/* LoadLibraryW( */ int x;\n") == 0);
  CHECK(found("const char* kId = \"someone.alpha\";\n") == 0);
  CHECK(found("bool alphabetical = true;\n") == 0);
  // An allowed use passes only in its own file.
  const std::string serve = "api->register_service(PM_SERVICE_PLAYABLE_GHOST, &kService);\n";
  const Mod ghost{"playable_ghost", "phasmomods.playable-ghost"};
  CHECK(problems_in("mods/playable_ghost/mod.cpp", serve, ghost, {ghost}).empty());
  CHECK(problems_in("mods/playable_ghost/other.cpp", serve, ghost, {ghost}).size() == 1);
}

void no_mod_handles_the_loaders_logistics(const fs::path& root) {
  const std::vector<Mod> all = mods_in(root);
  CHECK(all.size() >= 4);  // the bundled mods, each with a manifest
  for (const Mod& mod : all)
    for (const fs::directory_entry& entry : fs::recursive_directory_iterator(root / "mods" / mod.folder)) {
      const fs::path& file = entry.path();
      if (!entry.is_regular_file() || (file.extension() != ".cpp" && file.extension() != ".h")) continue;
      const std::string relative = fs::relative(file, root).generic_string();
      for (const std::string& problem : problems_in(relative, read_file(file), mod, all)) {
        std::printf("FAIL %s:%s\n", relative.c_str(), problem.c_str());
        ++g_failures;
      }
    }
  // Every allowed use is still there: one that went is taken off the list.
  for (const Allowed& a : kAllowed)
    if (read_file(root / a.file).find(a.text) == std::string::npos) {
      std::printf("FAIL %s no longer holds %s: take it off kAllowed\n", a.file, a.text);
      ++g_failures;
    }
}

}  // namespace

int main() {
  each_category_is_found();
  no_mod_handles_the_loaders_logistics(PM_SOURCE_DIR);
  return g_failures ? 1 : 0;
}
