// Offline tests for the Emotes mod's emote pack catalogue (mods/emotes/catalogue.h): reading a pack's emotes.json, the
// rules every field must follow, the warnings for unknown fields, the bundle name a project path becomes and the key an
// emote id hashes to, and the wheel's order across packs. The plan is
// docs/superpowers/plans/2026-11-12-emotes-pages-packs-walking.md. Run: build\emotes_catalogue_test.exe
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include "../third_party/nlohmann/json.hpp"
#include "catalogue.h"
#include "check.h"
#include "text_table.h"

namespace {

using namespace pm::emotes;

// One pack's emotes.json with every field set, as the plan shows it.
const char* kExample = R"JSON({ "format": 1, "bundle": "emotes.bundle",
  "emotes": [ { "name": "floss", "clip": "Assets/Emotes/Clips/Floss.anim", "song": "Assets/Emotes/Loops/floss.wav",
                "icon": "Assets/Emotes/Icons/floss.png", "label": "emotes.name.floss",
                "loudness": "loudness/floss.txt", "loops": true, "songLoop": { "start": 0, "seconds": 16.692 },
                "moveClip": "Assets/Emotes/Clips/FlossWalk.anim",
                "introClip": "Assets/Emotes/Clips/FlossStart.anim", "introSong": "Assets/Emotes/Songs/floss_intro.wav",
                "introLoudness": "loudness/floss.intro.txt" } ] })JSON";

bool near(float a, float b, float tol = 1e-3f) { return a >= b - tol && a <= b + tol; }

// The example, ready to change one field.
nlohmann::json example() { return nlohmann::json::parse(kExample); }

// Refuses `text`: the reader says no, gives a why, and reads nothing.
void refused(const std::string& text) {
  Catalogue cat;
  std::string why;
  CHECK(!parse_catalogue(text, "p.x", cat, why));
  CHECK(cat.emotes.empty());
  CHECK(!why.empty());
}

// A valid pack's JSON with these emote names, for the wheel order tests.
std::string pack_json(const std::vector<std::string>& names) {
  nlohmann::json doc;
  doc["format"] = 1;
  doc["bundle"] = "e.bundle";
  doc["emotes"] = nlohmann::json::array();
  for (const std::string& n : names)
    doc["emotes"].push_back({{"name", n},
                             {"clip", "Assets/" + n + ".anim"},
                             {"song", "Assets/" + n + ".wav"},
                             {"icon", "Assets/" + n + ".png"},
                             {"label", "emotes.name." + n},
                             {"loudness", "loudness/" + n + ".txt"}});
  return doc.dump();
}

// A catalogue read from that JSON under `packId`, checked as read.
Catalogue make_pack(const std::string& packId, const std::vector<std::string>& names) {
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(pack_json(names), packId, cat, why));
  return cat;
}

// Whether the warnings name this field.
bool warned(const Catalogue& cat, const std::string& field) {
  for (const std::string& w : cat.warnings)
    if (w == field) return true;
  return false;
}

void the_example_parses() {
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(kExample, "p.x", cat, why));
  CHECK(cat.packId == "p.x");
  CHECK(cat.bundle == "emotes.bundle");
  CHECK(cat.emotes.size() == 1);
  if (cat.emotes.size() != 1) return;
  const EmoteDef& e = cat.emotes[0];
  CHECK(e.id == "p.x.floss");
  CHECK(e.name == "floss");
  CHECK(e.clip == "Assets/Emotes/Clips/Floss.anim");
  CHECK(e.moveClip == "Assets/Emotes/Clips/FlossWalk.anim");
  CHECK(e.introClip == "Assets/Emotes/Clips/FlossStart.anim");
  CHECK(e.song == "Assets/Emotes/Loops/floss.wav");
  CHECK(e.introSong == "Assets/Emotes/Songs/floss_intro.wav");
  CHECK(e.icon == "Assets/Emotes/Icons/floss.png");
  CHECK(e.label == "emotes.name.floss");
  CHECK(e.loudness == "loudness/floss.txt");
  CHECK(e.introLoudness == "loudness/floss.intro.txt");
  CHECK(e.loops);
  CHECK(near(e.songLoop.start, 0.0f));
  CHECK(near(e.songLoop.length, 16.692f));
  CHECK(e.key == emote_key("p.x.floss"));
  CHECK(cat.warnings.empty());
}

// The SDK's example pack (sdk/examples/example_emote_pack) parses as a pack of its own: one emote named by its pack
// id, a bundle named after the pack so it cannot collide with another pack's, no warnings for its tool-only fields,
// and its emote's label in the pack's own string table.
void the_example_pack_parses() {
  std::ifstream in(std::filesystem::path(PM_TEXT_DIR) / "sdk" / "examples" / "example_emote_pack" / "assets" / "emotes.json",
                   std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(text, "example.emote-pack", cat, why));
  CHECK(cat.warnings.empty());
  CHECK(cat.bundle == "example.emote-pack.bundle");
  CHECK(cat.emotes.size() == 1);
  if (cat.emotes.size() != 1) return;
  CHECK(cat.emotes[0].id == "example.emote-pack.example-dance");
  pm::i18n::Table table;
  CHECK(table.load(std::filesystem::path(PM_TEXT_DIR) / "sdk" / "examples" / "example_emote_pack" / "i18n" / "default.json",
                   why));
  CHECK(table.has(cat.emotes[0].label));
}

void format_other_than_1_is_refused() {
  nlohmann::json doc = example();
  doc["format"] = 0;
  refused(doc.dump());
  doc = example();
  doc["format"] = 2;
  refused(doc.dump());
  doc = example();
  doc["format"] = "1";
  refused(doc.dump());
  doc = example();
  doc.erase("format");
  refused(doc.dump());
}

void a_bundle_name_with_a_path_is_refused() {
  for (const char* bad : {"a/b.bundle", "..\\b.bundle", "C:b.bundle", "b.zip"}) {
    nlohmann::json doc = example();
    doc["bundle"] = bad;
    refused(doc.dump());
  }
}

void a_path_outside_Assets_or_with_dotdot_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"][0]["clip"] = "Other/x.anim";
  refused(doc.dump());
  doc = example();
  doc["emotes"][0]["clip"] = "Assets/../x.anim";
  refused(doc.dump());
  doc = example();
  doc["emotes"][0]["icon"] = "Assets/x.jpg";
  refused(doc.dump());
}

void a_loudness_path_with_dotdot_is_refused() {
  for (const char* bad : {"../x.txt", "/x.txt", "a\\x.txt", "x.wav"}) {
    nlohmann::json doc = example();
    doc["emotes"][0]["loudness"] = bad;
    refused(doc.dump());
  }
}

void a_duplicate_name_is_refused() {
  nlohmann::json doc = example();
  const nlohmann::json copy = doc["emotes"][0];  // copied first: pushing an element back into its own array is unsafe
  doc["emotes"].push_back(copy);
  refused(doc.dump());
}

void a_bad_name_is_refused() {
  const std::vector<std::string> badNames = {"Floss", "two words", std::string(41, 'a')};
  for (const std::string& bad : badNames) {
    nlohmann::json doc = example();
    doc["emotes"][0]["name"] = bad;
    refused(doc.dump());
  }
}

void a_moveClip_on_a_one_shot_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"][0]["loops"] = false;
  refused(doc.dump());
}

void an_introClip_on_a_one_shot_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"][0]["loops"] = false;
  doc["emotes"][0].erase("moveClip");
  refused(doc.dump());
}

void an_introSong_without_introLoudness_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"][0].erase("introLoudness");
  refused(doc.dump());
  doc = example();
  doc["emotes"][0].erase("introSong");
  refused(doc.dump());
}

void intro_fields_are_optional() {
  nlohmann::json doc = example();
  for (const char* field : {"moveClip", "introClip", "introSong", "introLoudness"}) doc["emotes"][0].erase(field);
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(doc.dump(), "p.x", cat, why));
  CHECK(cat.emotes.size() == 1);
  if (cat.emotes.size() != 1) return;
  CHECK(cat.emotes[0].moveClip.empty());
  CHECK(cat.emotes[0].introClip.empty());
  CHECK(cat.emotes[0].introSong.empty());
  CHECK(cat.emotes[0].introLoudness.empty());
}

void moveClip_may_equal_clip() {
  nlohmann::json doc = example();
  doc["emotes"][0]["moveClip"] = doc["emotes"][0]["clip"];
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(doc.dump(), "p.x", cat, why));
  CHECK(cat.emotes.size() == 1);
  if (cat.emotes.size() != 1) return;
  CHECK(cat.emotes[0].moveClip == cat.emotes[0].clip);
}

void a_negative_or_infinite_songLoop_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"][0]["songLoop"]["start"] = -1;
  refused(doc.dump());
  doc = example();
  doc["emotes"][0]["songLoop"]["seconds"] = -0.5;
  refused(doc.dump());
  // A number too big for a double: infinity, or no JSON at all. Either way the pack is refused.
  refused(R"JSON({ "format": 1, "bundle": "e.bundle", "emotes": [ { "name": "floss", "clip": "Assets/f.anim",
    "song": "Assets/f.wav", "icon": "Assets/f.png", "label": "f", "loudness": "f.txt",
    "songLoop": { "start": 0, "seconds": 1e999 } } ] })JSON");
}

void songLoop_is_optional() {
  nlohmann::json doc = example();
  doc["emotes"][0].erase("songLoop");
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(doc.dump(), "p.x", cat, why));
  CHECK(cat.emotes.size() == 1);
  if (cat.emotes.size() != 1) return;
  CHECK(near(cat.emotes[0].songLoop.start, 0.0f));
  CHECK(near(cat.emotes[0].songLoop.length, 0.0f));
}

void tool_fields_are_accepted_silently() {
  nlohmann::json doc = example();
  nlohmann::json& e = doc["emotes"][0];
  e["clipSource"] = "C:/Dev/clip.fbx";
  e["clipLoopGuess"] = 1.5;
  e["songSource"] = "C:/Dev/song.wav";
  e["songLoopGuess"] = {{"start", 0}, {"seconds", 16.692}};
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(doc.dump(), "p.x", cat, why));
  CHECK(cat.emotes.size() == 1);
  CHECK(cat.warnings.empty());
}

void an_unknown_field_is_a_warning() {
  nlohmann::json doc = example();
  doc["emotes"][0]["colour"] = "pink";
  doc["extra"] = true;
  Catalogue cat;
  std::string why;
  CHECK(parse_catalogue(doc.dump(), "p.x", cat, why));
  CHECK(cat.emotes.size() == 1);
  CHECK(warned(cat, "emotes[0].colour"));
  CHECK(warned(cat, "extra"));
}

void more_than_64_emotes_is_refused() {
  std::vector<std::string> names;
  for (int i = 0; i < 65; ++i) names.push_back("e" + std::to_string(i));
  refused(pack_json(names));
}

void an_empty_emotes_list_is_refused() {
  nlohmann::json doc = example();
  doc["emotes"] = nlohmann::json::array();
  refused(doc.dump());
}

void broken_json_is_refused_without_throwing() {
  for (const char* bad : {"{", "", "[]", "null"}) refused(bad);
}

void bundle_path_lowercases() {
  CHECK(bundle_path("Assets/Emotes/Clips/Floss.anim") == "assets/emotes/clips/floss.anim");
}

void emote_key_is_fnv1a() {
  CHECK(emote_key("") == 2166136261u);
  CHECK(emote_key("a") == 3826002220u);
}

void wheel_order_puts_own_first_then_packs_by_id() {
  const Catalogue own = make_pack("phasmomods.emotes", {"one", "two"});
  std::vector<Catalogue> packs;
  packs.push_back(make_pack("z.pack", {"zed"}));
  packs.push_back(make_pack("a.pack", {"ay"}));
  std::vector<EmoteDef> wheel;
  std::string why;
  CHECK(wheel_order(own, std::move(packs), wheel, why));
  const std::vector<std::string> want = {"phasmomods.emotes.one", "phasmomods.emotes.two", "a.pack.ay", "z.pack.zed"};
  CHECK(wheel.size() == want.size());
  for (size_t i = 0; i < wheel.size() && i < want.size(); ++i) CHECK(wheel[i].id == want[i]);
}

void wheel_order_refuses_a_key_collision() {
  const Catalogue own = make_pack("p.x", {"same"});
  std::vector<Catalogue> packs;
  packs.push_back(make_pack("p.x", {"same"}));
  std::vector<EmoteDef> wheel;
  std::string why;
  CHECK(!wheel_order(own, std::move(packs), wheel, why));
  CHECK(wheel.empty());
  CHECK(!why.empty());
}

void a_broken_pack_is_reported_and_skipped() {
  // What a caller does at startup: read every pack, keep the ones that parse, and load the wheel from those.
  const std::vector<std::pair<std::string, std::string>> files = {{"a.pack", pack_json({"ay"})},
                                                                  {"b.pack", "{ not json"},
                                                                  {"c.pack", pack_json({"see"})}};
  std::vector<Catalogue> good;
  std::string brokenWhy;
  for (const auto& [packId, text] : files) {
    Catalogue cat;
    std::string why;
    if (parse_catalogue(text, packId, cat, why))
      good.push_back(std::move(cat));
    else
      brokenWhy = why;
  }
  CHECK(!brokenWhy.empty());
  CHECK(good.size() == 2);
  if (good.size() != 2) return;
  std::vector<EmoteDef> wheel;
  std::string why;
  CHECK(wheel_order(good[0], {std::move(good[1])}, wheel, why));
  CHECK(wheel.size() == 2);
}

}  // namespace

int main() {
  the_example_parses();
  the_example_pack_parses();
  format_other_than_1_is_refused();
  a_bundle_name_with_a_path_is_refused();
  a_path_outside_Assets_or_with_dotdot_is_refused();
  a_loudness_path_with_dotdot_is_refused();
  a_duplicate_name_is_refused();
  a_bad_name_is_refused();
  a_moveClip_on_a_one_shot_is_refused();
  an_introClip_on_a_one_shot_is_refused();
  an_introSong_without_introLoudness_is_refused();
  intro_fields_are_optional();
  moveClip_may_equal_clip();
  a_negative_or_infinite_songLoop_is_refused();
  songLoop_is_optional();
  tool_fields_are_accepted_silently();
  an_unknown_field_is_a_warning();
  more_than_64_emotes_is_refused();
  an_empty_emotes_list_is_refused();
  broken_json_is_refused_without_throwing();
  bundle_path_lowercases();
  emote_key_is_fnv1a();
  wheel_order_puts_own_first_then_packs_by_id();
  wheel_order_refuses_a_key_collision();
  a_broken_pack_is_reported_and_skipped();
  std::printf("%s\n", g_failures ? "emotes_catalogue tests FAILED" : "emotes_catalogue tests passed");
  return g_failures ? 1 : 0;
}
