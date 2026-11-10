// Offline tests for the mod manifest and the ids and versions it holds (core/mod_ids.h,
// core/mod_manifest.h), and for the saved ids that move with it (core/mod_list.h).
// Run: build\mod_manifest_test.exe
#include "mod_manifest.h"

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#include "check.h"
#include "text_fixture.h"
#include "mod_ids.h"
#include "mod_list.h"

using namespace pm::modlist;
using namespace pm::modid;

namespace {

namespace fs = std::filesystem;

const std::string kFull = R"({
  "id": "phasmomods.ghost-tells",
  "name": "Ghost Tells",
  "author": "PhasmoMods",
  "version": "1.0.0",
  "description": "New tells for the Spirit, Mare, Shade, Yurei, Goryo and Onryo",
  "dll": "GhostTells.dll",
  "website": "https://example.com/tells",
  "dependencies": [
    { "id": "phasmomods.spectral-flame", "minimumVersion": "1.2.3", "required": true },
    { "id": "someone.optional", "required": false },
    { "id": "someone.plain" }
  ],
  "incompatibilities": ["other.mod", "flat-mod"]
})";

// The smallest manifest that parses, with `extra` added to it.
std::string minimal(const std::string& extra = "") {
  return R"({"id":"a.b","name":"A","author":"Me","version":"1.0.0","description":"D","dll":"a.dll")" + extra + "}";
}

// The minimal manifest with one field's whole text (`"id":"a.b"`) replaced.
std::string with(const std::string& field, const std::string& replacement) {
  std::string text = minimal();
  const size_t at = text.find(field);
  CHECK(at != std::string::npos);
  return text.replace(at, field.size(), replacement);
}

bool parses(const std::string& text) {
  Manifest m;
  std::string why;
  return parse_manifest_json(text, m, why);
}

// Why `text` is refused, empty when it parses. A refusal always gives a reason.
std::string why_refused(const std::string& text) {
  Manifest m;
  std::string why;
  if (parse_manifest_json(text, m, why)) return {};
  CHECK(!why.empty());
  CHECK(display_safe(why, 200));  // the Mods panel draws it as rich text
  return why;
}

void manifest_reads_every_field() {
  Manifest m;
  std::string why;
  CHECK(parse_manifest_json(kFull, m, why));
  CHECK(why.empty());
  CHECK(m.id == "phasmomods.ghost-tells");
  CHECK(m.name == "Ghost Tells");
  CHECK(m.author == "PhasmoMods");
  CHECK(m.version == "1.0.0");
  CHECK(m.description == "New tells for the Spirit, Mare, Shade, Yurei, Goryo and Onryo");
  CHECK(m.dll == "GhostTells.dll");
  CHECK(m.website == "https://example.com/tells");
  CHECK(!m.oldFormat && m.unknownFields.empty());
  CHECK(m.dependencies.size() == 3);
  if (m.dependencies.size() == 3) {
    CHECK(m.dependencies[0].id == "phasmomods.spectral-flame" && m.dependencies[0].minimumVersion == "1.2.3" &&
          m.dependencies[0].required);
    CHECK(m.dependencies[1].id == "someone.optional" && m.dependencies[1].minimumVersion.empty() &&
          !m.dependencies[1].required);
    CHECK(m.dependencies[2].id == "someone.plain" && m.dependencies[2].required);  // required defaults to true
  }
  CHECK(m.incompatibilities == (std::vector<std::string>{"other.mod", "flat-mod"}));

  // website, dependencies and incompatibilities are optional.
  CHECK(parse_manifest_json(minimal(), m, why));
  CHECK(m.website.empty() && m.dependencies.empty() && m.incompatibilities.empty());
  CHECK(parse_manifest_json(minimal(R"(,"website":"")"), m, why) && m.website.empty());

  // A byte order mark from a Windows editor is still UTF-8 JSON, and a name may be any language's.
  CHECK(parse_manifest_json("\xEF\xBB\xBF" + with(R"("name":"A")", R"("name":"\u5e7d\u970a Caf\u00e9")"), m, why));
  CHECK(m.name == "\xE5\xB9\xBD\xE9\x9C\x8A Caf\xC3\xA9");
}

// What the notice shows comes from here: false unless the manifest says so, and anything but true or false refuses the mod.
void manifest_says_whether_it_records_voice() {
  Manifest m;
  std::string why;
  CHECK(parse_manifest_json(minimal(), m, why) && !m.recordsVoice);
  CHECK(parse_manifest_json(minimal(R"(,"recordsVoice":true)"), m, why) && m.recordsVoice && m.unknownFields.empty());
  CHECK(parse_manifest_json(minimal(R"(,"recordsVoice":false)"), m, why) && !m.recordsVoice);
  for (const char* bad : {R"(,"recordsVoice":"yes")", R"(,"recordsVoice":1)", R"(,"recordsVoice":null)"}) {
    CHECK(!parse_manifest_json(minimal(bad), m, why));
    CHECK(why == "'recordsVoice' field is not true or false.");
  }
}

void manifest_rejects_bad_id() {
  for (const char* id : {"noauthor", "two.dots.here", "Upper.mod", "author.Mod", ".mod", "author.", "a b.c", "a_b.c", "",
                         "vanilla.mod", "a.b\\u0000c"}) {
    CHECK(!why_refused(with(R"("id":"a.b")", std::string(R"("id":")") + id + "\"")).empty());
  }
  // 64 characters in all at most.
  const std::string longest = std::string(30, 'a') + "." + std::string(33, 'b');
  CHECK(longest.size() == 64 && valid_id(longest));
  CHECK(parses(with(R"("id":"a.b")", R"("id":")" + longest + "\"")));
  CHECK(!parses(with(R"("id":"a.b")", R"("id":")" + longest + "b\"")));
  CHECK(why_refused(with(R"("id":"a.b")", R"("id":"vanilla.mod")")).find("vanilla") != std::string::npos);
  // Not a string, or missing.
  CHECK(!parses(with(R"("id":"a.b")", R"("id":7)")));
  CHECK(why_refused(with(R"("id":"a.b",)", "")).find("id") != std::string::npos);
  CHECK(valid_id("phasmomods.custom-ghosts") && !valid_id("custom-ghosts") && valid_old_id("custom-ghosts"));
  CHECK(!valid_old_id("a.b") && !valid_old_id(std::string(41, 'a')) && valid_old_id(std::string(40, 'a')));
}

// A bundled mod's old flat id always means that mod, in a manifest as in a mod.ini: a dependency on it finds the mod, and an
// incompatibility with it fires.
void old_bundled_ids_in_a_manifest_mean_the_mod() {
  Manifest m;
  std::string why;
  CHECK(parse_manifest_json(minimal(R"(,"dependencies":[{"id":"spectral-flame"},{"id":"custom-ghosts","required":false},{"id":"flat-mod"}],"incompatibilities":["ghost-tells","other-flat"])"), m, why));
  CHECK(m.dependencies.size() == 3);
  if (m.dependencies.size() == 3) {
    CHECK(m.dependencies[0].id == "phasmomods.spectral-flame" && m.dependencies[1].id == "phasmomods.custom-ghosts");
    CHECK(m.dependencies[2].id == "flat-mod");  // not a bundled mod's: it stays as it is
  }
  CHECK(m.incompatibilities == (std::vector<std::string>{"phasmomods.ghost-tells", "other-flat"}));
  // One mod under both of its ids is one mod twice, and a mod naming its own old id names itself.
  CHECK(why_refused(minimal(R"(,"dependencies":[{"id":"spectral-flame"},{"id":"phasmomods.spectral-flame"}])")).find("twice") != std::string::npos);
  CHECK(why_refused(minimal(R"(,"incompatibilities":["ghost-tells","phasmomods.ghost-tells"])")).find("twice") != std::string::npos);
  std::string own = minimal(R"(,"dependencies":[{"id":"spectral-flame"}])");
  own.replace(own.find("a.b"), 3, "phasmomods.spectral-flame");
  CHECK(why_refused(own).find("itself") != std::string::npos);
}

void manifest_rejects_bad_version() {
  for (const char* v : {"1", "1.0", "1.0.0.0", "1.0.0-beta", "1.0.0+build", "v1.0.0", "01.0.0", "1.00.0", "1.0.", ".1.0", "1..0",
                        " 1.0.0", "1.0.0 ", "-1.0.0", "4294967296.0.0", "", "unknown"}) {
    CHECK(!parse_version(v));
    CHECK(why_refused(with(R"("version":"1.0.0")", std::string(R"("version":")") + v + "\"")).find("Version") !=
          std::string::npos);
  }
  CHECK(!parses(with(R"("version":"1.0.0")", R"("version":1)")));
  CHECK(!parses(with(R"("version":"1.0.0",)", "")));
  CHECK(parse_version("0.0.0") && parse_version("4294967295.4294967295.4294967295"));
  // A dependency's minimum version obeys the same rule.
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"x.y","minimumVersion":"1.0"}])")));
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"x.y","minimumVersion":""}])")));
}

void version_compares_as_numbers() {
  const auto v = [](const char* text) { return *parse_version(text); };
  CHECK(v("1.0.10") > v("1.0.1"));
  CHECK(v("1.0.9") < v("1.0.10"));
  CHECK(v("1.10.0") > v("1.9.9"));
  CHECK(v("2.0.0") > v("1.99.99"));
  CHECK(v("1.2.3") == v("1.2.3"));
}

void manifest_malformed_skips_mod_with_reason() {
  for (const char* text : {"", "   ", "{", "}", "not json", "{\"id\":", "[]", "\"a\"", "7", "null", "{\"id\":\"a.b\",}",
                           "{'id':'a.b'}", "// comment\n{}", "\xEF\xBB\xBF"}) {
    CHECK(!why_refused(text).empty());
  }
  // A string that is not UTF-8.
  CHECK(!why_refused(with(R"("name":"A")", "\"name\":\"\xFF\xFE\"")).empty());
  // Over 64 KiB; exactly 64 KiB is fine.
  const std::string pad = R"(,"pad":")";
  std::string big = minimal(pad + "\"");
  big.insert(big.size() - 2, kMaxManifestBytes - big.size(), 'x');
  CHECK(big.size() == kMaxManifestBytes && parses(big));
  big.insert(big.size() - 2, "x");
  CHECK(why_refused(big).find("64 KiB") != std::string::npos);
  // Nesting far past any manifest's, at the size limit: refused, without running out of stack.
  std::string deep = R"({"pad":)" + std::string(32000, '[') + std::string(32000, ']') + "}";
  CHECK(deep.size() <= kMaxManifestBytes);
  CHECK(why_refused(deep).find("nest") != std::string::npos);
  CHECK(!why_refused(std::string(kMaxManifestBytes, '[')).empty());  // and never closed
  // One field named twice would mean one thing here and another in an editor.
  CHECK(!why_refused(minimal(R"(,"id":"c.d")")).empty());
  // In any object: a field named twice inside a dependency too, and the same name in two objects is no repeat.
  CHECK(why_refused(minimal(R"(,"dependencies":[{"id":"e.f","id":"c.d"}])")).find("twice") != std::string::npos);
  CHECK(why_refused(minimal(R"(,"dependencies":[{"id":"e.f","required":true,"required":false}])")).find("twice") != std::string::npos);
  CHECK(parses(minimal(R"(,"dependencies":[{"id":"e.f"},{"id":"g.h"}])")));
  // Text the game draws can neither hold rich text nor forge lines.
  for (const char* field : {"name", "author", "description"}) {
    const std::string key = std::string("\"") + field + "\":\"";
    const std::string original = field == std::string("name") ? "A" : field == std::string("author") ? "Me" : "D";
    const std::string whole = key + original + "\"";
    CHECK(!parses(with(whole, key + "<size=0>x\"")));
    CHECK(!parses(with(whole, key + "a\\nb\"")));
    CHECK(!parses(with(whole, key + "\"")));  // required, so not empty
    CHECK(!parses(with(whole, key + std::string(field == std::string("description") ? 513 : 65, 'x') + "\"")));
    CHECK(!parses(with(whole, std::string("\"") + field + "\":7")));
    CHECK(!parses(with(whole + ",", "")));
  }
  // dll: only a file beside the manifest.
  for (const char* dll : {"..\\\\host.dll", "C:\\\\x.dll", "sub/x.dll", "x.exe", "x", ".dll", "a:b.dll", "x\\u0000y.dll", "a|b.dll",
                          "NUL.dll", "nul.DLL", "con.dll", "Prn.dll", "aux.dll", "COM1.dll", "lpt9.dll", "NUL.x.dll"}) {
    CHECK(!parses(with(R"("dll":"a.dll")", std::string(R"("dll":")") + dll + "\"")));
  }
  CHECK(parses(with(R"("dll":"a.dll")", R"("dll":"Upper.DLL")")));
  CHECK(parses(with(R"("dll":"a.dll")", R"("dll":"Null.dll")")) && parses(with(R"("dll":"a.dll")", R"("dll":"COM10.dll")")) &&
        parses(with(R"("dll":"a.dll")", R"("dll":"console.dll")")));
  // website, dependencies and incompatibilities, when given, are what they say.
  CHECK(!parses(minimal(R"(,"website":7)")));
  CHECK(!parses(minimal(R"x(,"website":"javascript:alert(1)")x")));
  CHECK(!parses(minimal(R"(,"dependencies":{})")));
  CHECK(!parses(minimal(R"(,"dependencies":["x.y"])")));
  CHECK(!parses(minimal(R"(,"dependencies":[{}])")));
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"Bad Id"}])")));
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"a.b"}])")));  // itself
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"x.y"},{"id":"x.y"}])")));
  CHECK(!parses(minimal(R"(,"dependencies":[{"id":"x.y","required":"yes"}])")));
  CHECK(!parses(minimal(R"(,"incompatibilities":"x.y")")));
  CHECK(!parses(minimal(R"(,"incompatibilities":[7])")));
  CHECK(!parses(minimal(R"(,"incompatibilities":["Bad Id"])")));
  CHECK(!parses(minimal(R"(,"incompatibilities":["a.b"])")));  // itself
  CHECK(!parses(minimal(R"(,"incompatibilities":["x.y","x.y"])")));
  // An old-format id is a real mod to name; a list has an end.
  CHECK(parses(minimal(R"(,"dependencies":[{"id":"flat-mod"}],"incompatibilities":["other-flat"])")));
  std::string many = R"(,"incompatibilities":[)";
  for (int i = 0; i <= static_cast<int>(kMaxListEntries); ++i) many += (i ? ",\"m." : "\"m.") + std::to_string(i) + "\"";
  CHECK(!parses(minimal(many + "]")));

  // What failed to parse leaves nothing a caller could use.
  Manifest m;
  std::string why;
  CHECK(!parse_manifest_json(with(R"("version":"1.0.0")", R"("version":"bad")"), m, why));
  CHECK(m.id.empty() && m.name.empty() && m.dll.empty());
}

void manifest_unknown_field_is_ignored() {
  Manifest m;
  std::string why;
  CHECK(parse_manifest_json(minimal(R"(,"laterField":{"a":[1,2]},"another":1,"dependencies":[{"id":"x.y","alsoLater":true}])"), m, why));
  CHECK(m.id == "a.b");
  CHECK(m.unknownFields == (std::vector<std::string>{"another", "dependencies.alsoLater", "laterField"}));
  // An editor's pointer to the schema is known, as SMAPI's manifests have it.
  CHECK(parse_manifest_json(minimal(R"(,"$schema":"manifest.schema.json")"), m, why) && m.unknownFields.empty());
  // A field's name goes to the log: one that could not be a name is not repeated, and the list has an end.
  CHECK(parse_manifest_json(minimal(R"(,"bad\nname":1,"<b>":2)"), m, why));
  CHECK(m.unknownFields == (std::vector<std::string>{"(a field with an unprintable name)", "(a field with an unprintable name)"}));
  std::string lots;
  for (int i = 0; i < 100; ++i) lots += ",\"f" + std::to_string(i) + "\":1";
  CHECK(parse_manifest_json(minimal(lots), m, why));
  CHECK(m.unknownFields.size() == kMaxUnknownFieldsListed);
}

void content_id_splits_at_the_last_dot() {
  const auto split = [](const char* id) { return split_content_id(id); };
  auto c = split("phasmomods.custom-ghosts.draugr");
  CHECK(c && c->mod == "phasmomods.custom-ghosts" && c->name == "draugr");
  c = split("phasmomods.custom-ghosts.black_shuck");
  CHECK(c && c->mod == "phasmomods.custom-ghosts" && c->name == "black_shuck");
  c = split("flat-mod.flame");  // an old-format mod's content
  CHECK(c && c->mod == "flat-mod" && c->name == "flame");
  c = split("vanilla.mare");
  CHECK(c && c->mod == "vanilla" && c->name == "mare");
  for (const char* bad : {"", "nodot", ".name", "mod.", "a.b.", "a.b.c.d", "a..b", "Upper.x", "a_b.x", "custom_ghosts.brownie", "vanilla.a.b"})
    CHECK(!split(bad));
}

void old_bundled_ids_migrate_once() {
  // The table: four old ids, each with the prefix its content ids used.
  CHECK(migrated_id("playable-ghost") == "phasmomods.playable-ghost");
  CHECK(migrated_id("custom-ghosts") == "phasmomods.custom-ghosts");
  CHECK(migrated_id("spectral-flame") == "phasmomods.spectral-flame");
  CHECK(migrated_id("ghost-tells") == "phasmomods.ghost-tells");
  CHECK(migrated_id("someone-else") == "someone-else");
  CHECK(migrated_id("phasmomods.ghost-tells") == "phasmomods.ghost-tells");
  CHECK(migrated_content_id("custom_ghosts.black_shuck") == "phasmomods.custom-ghosts.black_shuck");
  CHECK(migrated_content_id("phasmomods.custom-ghosts.black_shuck") == "phasmomods.custom-ghosts.black_shuck");
  CHECK(migrated_content_id("vanilla.mare") == "vanilla.mare");
  CHECK(migrated_content_id("custom_ghosts") == "custom_ghosts");  // no name: not a content id

  // launcher.ini moves once: rewritten with the new ids, and a mod switched off later stays off.
  const fs::path root = fs::temp_directory_path() / (L"pg-manifest-" + std::to_wstring(GetCurrentProcessId()));
  std::error_code error;
  fs::remove_all(root, error);
  fs::create_directories(root, error);
  const fs::path ini = root / "launcher.ini";
  std::ofstream(ini, std::ios::binary) << "enabled=ghost-tells,someone-else,custom-ghosts,phasmomods.custom-ghosts\n";
  const std::vector<std::string> installed{"phasmomods.ghost-tells", "phasmomods.custom-ghosts", "someone-else"};
  const std::vector<std::string> moved{"phasmomods.ghost-tells", "someone-else", "phasmomods.custom-ghosts"};
  CHECK(load_choice(ini, installed) == moved);
  bool present = false;
  CHECK(parse_enabled(pm::text::read_file(ini), present) == moved && present);
  CHECK(save_choice(ini, {"someone-else"}));
  CHECK(load_choice(ini, installed) == std::vector<std::string>{"someone-else"});
  // A file already in the new form is not written again.
  const std::string before = pm::text::read_file(ini);
  std::ofstream(ini, std::ios::binary | std::ios::trunc) << "# Kept by hand.\n" << before;
  const std::string kept = pm::text::read_file(ini);
  CHECK(load_choice(ini, installed) == std::vector<std::string>{"someone-else"});
  CHECK(pm::text::read_file(ini) == kept);
  fs::remove_all(root, error);
}

// A game entry's key, shown in the Mods panel: its angle brackets (a generic instance's, VolumeParameter<float>) as the
// look-alike single quotation marks, which the panel's rich text does not read as a tag; anything else unsafe stays so.
void an_entry_key_is_shown_safely() {
  const std::string shown = shown_key("VolumeParameter<float>.set_value");
  CHECK(shown == "VolumeParameter\xE2\x80\xB9" "float\xE2\x80\xBA.set_value");
  CHECK(display_safe(shown, 128));
  CHECK(shown_key("GhostAI.UnAppear") == "GhostAI.UnAppear");
  CHECK(!display_safe(shown_key("a\nb"), 128));
}

// Text from another player's client: drawn in the game's menus, so only what the manifest would allow.
void display_text_is_safe() {
  CHECK(display_safe("Ghost Tells", 64));
  CHECK(display_safe("\xE5\xB9\xBD\xE9\x9C\x8A", 64));  // UTF-8
  CHECK(!display_safe("", 64));
  CHECK(!display_safe("a<b", 64) && !display_safe("a>b", 64) && !display_safe("a\nb", 64) && !display_safe("a\x7f", 64));
  CHECK(!display_safe(std::string_view("a\0b", 3), 64));
  CHECK(!display_safe("abcde", 4) && display_safe("abcd", 4));
  CHECK(!display_safe("\xFF", 64));                  // not UTF-8
  CHECK(!display_safe("\xC0\x80", 64));              // an overlong encoding
  CHECK(!display_safe("\xE5\xB9", 64));              // cut short
  CHECK(!display_safe("\xED\xA0\x80", 64));          // a surrogate
  CHECK(!display_safe("\xF4\x90\x80\x80", 64));      // past U+10FFFF
  CHECK(display_safe("\xF4\x8F\xBF\xBF", 64));       // U+10FFFF
  // What another player could use to forge a line in a notice.
  CHECK(!display_safe("a\xC2\x80z", 64));            // U+0080
  CHECK(!display_safe("a\xC2\x85z", 64));            // U+0085, next line
  CHECK(!display_safe("a\xC2\x9Fz", 64));            // U+009F
  CHECK(!display_safe("a\xE2\x80\xA8z", 64));        // U+2028
  CHECK(!display_safe("a\xE2\x80\xA9z", 64));        // U+2029
  CHECK(display_safe("a\xC2\xA0z", 64));             // U+00A0, the first character after them
  CHECK(display_safe("a\xE2\x80\xA7z", 64) && display_safe("a\xE2\x80\xAAz", 64));  // U+2027 and U+202A, beside them
}

std::string read_source(const std::string& relative) {
  std::ifstream in(std::string(PM_SOURCE_DIR) + "/" + relative, std::ios::binary);
  std::stringstream text;
  text << in.rdbuf();
  return text.str();
}

// What ships: each bundled mod's manifest parses, names its DLL, and agrees with the DLL's compiled id.
void bundled_manifests_are_valid() {
  struct Bundled {
    const char* folder;
    const char* id;
    const char* name;
    const char* dll;
  };
  for (const Bundled& b : {Bundled{"custom_ghosts", "phasmomods.custom-ghosts", "Custom Ghosts", "CustomGhosts.dll"},
                           Bundled{"ghost_tells", "phasmomods.ghost-tells", "Ghost Tells", "GhostTells.dll"},
                           Bundled{"playable_ghost", "phasmomods.playable-ghost", "Playable Ghost", "PlayableGhost.dll"},
                           Bundled{"spectral_flame", "phasmomods.spectral-flame", "Spectral Flame", "SpectralFlame.dll"},
                           Bundled{"emotes", "phasmomods.emotes", "Emotes", "Emotes.dll"}}) {
    Manifest m;
    std::string why;
    CHECK(parse_manifest_json(read_source(std::string("mods/") + b.folder + "/manifest.json"), m, why));
    CHECK(m.id == b.id && m.name == b.name && m.dll == b.dll && m.author == "PhasmoMods" && m.version == "1.0.0");
    CHECK(m.unknownFields.empty());
    CHECK(read_source(std::string("mods/") + b.folder + "/mod.cpp").find(std::string("\"") + b.id + "\"") != std::string::npos);
    CHECK(!fs::exists(fs::path(PM_SOURCE_DIR) / "mods" / b.folder / "mod.ini"));
  }
}

// An editor checks a manifest against sdk/manifest.schema.json: it says what the parser says.
void schema_agrees_with_the_parser() {
  nlohmann::json schema = nlohmann::json::parse(read_source("sdk/manifest.schema.json"), nullptr, false);
  CHECK(schema.is_object());
  if (!schema.is_object()) return;
  const auto& properties = schema["properties"];
  const std::regex id(properties["id"]["pattern"].get<std::string>());
  CHECK(properties["id"]["maxLength"].get<size_t>() == kMaxIdBytes);
  for (const char* text : {"a.b", "phasmomods.ghost-tells", "x-1.y-2", "noauthor", "two.dots.here", "Upper.mod", "a_b.c", ".b", "a.", "vanilla.mod"})
    CHECK(std::regex_match(std::string(text), id) == valid_id(text));
  const std::regex version(properties["version"]["pattern"].get<std::string>());
  for (const char* text : {"1.0.0", "0.0.0", "10.20.30", "1.0", "1.0.0.0", "01.0.0", "1.0.0-beta", "v1.0.0"})
    CHECK(std::regex_match(std::string(text), version) == parse_version(text).has_value());
  std::vector<std::string> required;
  for (const auto& r : schema["required"]) required.push_back(r.get<std::string>());
  std::sort(required.begin(), required.end());
  CHECK(required == (std::vector<std::string>{"author", "description", "dll", "id", "name", "version"}));
  CHECK(properties.contains("website") && properties.contains("dependencies") && properties.contains("incompatibilities"));
  // The schema's text limits name what display_safe refuses, and its dll pattern refuses the reserved device names.
  for (const char* field : {"name", "author", "description"}) {
    const std::string pattern = properties[field]["pattern"].get<std::string>();
    for (const char* written : {"\\u0000-\\u001f", "\\u007f-\\u009f", "\\u2028", "\\u2029", "<>"})
      CHECK(pattern.find(written) != std::string::npos);
  }
  const std::regex dllPattern(properties["dll"]["pattern"].get<std::string>());
  for (const char* dll : {"a.dll", "NUL.dll", "nul.DLL", "con.dll", "Prn.dll", "aux.dll", "COM1.dll", "lpt9.dll", "Null.dll", "COM10.dll", "console.dll"})
    CHECK(std::regex_match(std::string(dll), dllPattern) == pm::modlist::valid_dll(dll));
}

}  // namespace

int main() {
  use_text({"i18n/default.json"});
  manifest_says_whether_it_records_voice();
  manifest_reads_every_field();
  manifest_rejects_bad_id();
  old_bundled_ids_in_a_manifest_mean_the_mod();
  manifest_rejects_bad_version();
  version_compares_as_numbers();
  manifest_malformed_skips_mod_with_reason();
  manifest_unknown_field_is_ignored();
  content_id_splits_at_the_last_dot();
  old_bundled_ids_migrate_once();
  display_text_is_safe();
  an_entry_key_is_shown_safely();
  bundled_manifests_are_valid();
  schema_agrees_with_the_parser();
  std::printf(g_failures ? "%d check(s) failed\n" : "all mod manifest tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
