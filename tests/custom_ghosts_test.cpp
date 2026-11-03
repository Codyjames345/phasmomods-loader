// Offline tests for the custom ghosts' data and rules (mods/custom_ghosts/ghosts.h, trail.h), and their
// registration with the content layer (registration.h) on the fake backend.
// Run: build\custom_ghosts_test.exe
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "apparition_rule.h"
#include "contract.h"
#include "draugr_rule.h"
#include "echo_rule.h"
#include "goryo_way.h"
#include "fake_game.h"
#include "game_api.h"
#include "ghosts.h"
#include "Equipment.h"
#include "GhostHuntState.h"
#include "LevelRoom.h"
#include "Player.h"
#include "gremlin.h"
#include "gremlin_rule.h"
#include "hunt.h"
#include "registration.h"
#include "Sensor.h"
#include "sensor_copy.h"
#include "asterion_rule.h"
#include "bogeyman_rule.h"
#include "hud_line.h"
#include "brownie_rule.h"
#include "hydra_rule.h"
#include "knell_rule.h"
#include "lethe_rule.h"
#include "lithobolia_rule.h"
#include "mesmer_rule.h"
#include "shuck_rule.h"
#include "sight_rule.h"
#include "tempest_rule.h"
#include "speed_rule.h"
#include "trail.h"
#include "voice.h"

#include "check.h"
#include "text_fixture.h"

using namespace pm::custom;

namespace {

// The game's hunt speed parts (speed_rule.h OwnSpeed): the base speed GhostAI.Awake gives every ghost (the
// constant at RVA 0x558CC90) and the line-of-sight speed-up at its fullest.
constexpr float kGameBase = 1.7f;
constexpr float kGameSightMax = 1.65f;

// The evidence the game's own 30 types use (the journal's ghost pages), as sorted triples.
std::set<std::set<int>> vanilla_evidence() {
  const int t[][3] = {{1, 2, 5}, {1, 2, 7}, {2, 3, 7}, {2, 3, 5}, {3, 4, 7}, {1, 3, 6}, {2, 4, 5}, {4, 5, 6},
                      {1, 5, 6}, {3, 5, 6}, {4, 6, 7}, {1, 6, 7}, {2, 4, 7}, {3, 4, 6}, {1, 3, 7}, {1, 3, 5},
                      {2, 4, 6}, {1, 2, 6}, {1, 4, 7}, {1, 3, 4}, {2, 3, 6}, {2, 5, 6}, {2, 5, 7}, {4, 5, 7},
                      {1, 2, 3}, {1, 2, 4}, {3, 5, 7}, {2, 3, 4}, {5, 6, 7}, {1, 5, 7}};
  std::set<std::set<int>> out;
  for (const auto& e : t) out.insert({e[0], e[1], e[2]});
  return out;
}

// The evidence ids the ghosts give, numbered as the table above numbers the game's (EvidenceType in the
// audited build) and Spectral Flame after them; 0 for an id that is neither.
int evidence_number(const char* id) {
  static const std::map<std::string, int> numbers{{kEmf, 1},  {kSpiritBox, 2}, {kUltraviolet, 3}, {kOrbs, 4},
                                                  {kWriting, 5}, {kFreezing, 6}, {kDots, 7}, {kSpectralFlame, 8}};
  const auto it = numbers.find(id ? id : "");
  return it == numbers.end() ? 0 : it->second;
}

bool gives(const Ghost& g, const char* evidence) {
  return std::any_of(std::begin(g.evidence), std::end(g.evidence),
                     [&](const char* e) { return e && std::strcmp(e, evidence) == 0; });
}

void every_custom_ghost_is_identifiable() {
  auto taken = vanilla_evidence();
  CHECK(taken.size() == 30);
  std::set<int> types;
  for (const Ghost& g : kGhosts) {
    CHECK(g.type > 0 && types.insert(g.type).second);
    std::set<int> e;
    for (const char* id : g.evidence) e.insert(evidence_number(id));
    CHECK(e.size() == 3 && !e.count(0));  // three different evidence types, each one the content layer knows
    CHECK(taken.insert(e).second);         // no other ghost, vanilla or custom, shares them
    CHECK(*g.name && *g.description && *g.strength && *g.weakness);
    // Its journal text is in the mod's table, each key there.
    for (const char* key : {title_key(g), g.description, g.strength, g.weakness}) CHECK(pm::i18n::table().has(key));
    if (g.ability) CHECK(pm::i18n::table().has(g.ability) && pm::i18n::table().has(g.abilityDescription));
  }
  CHECK(taken.size() == 30 + static_cast<size_t>(kGhostCount));  // every ghost has a combination of its own
}

// Each ghost's content id is its name in lower case, spaces as _, after the mod's prefix.
void every_ghost_has_its_id() {
  CHECK(id_of(*find(kBlackShuck)) == "phasmomods.custom-ghosts.black_shuck");
  CHECK(id_of(*find(kEcho)) == "phasmomods.custom-ghosts.echo");
  std::set<std::string> ids;
  for (const Ghost& g : kGhosts) {
    const std::string id = id_of(g);
    CHECK(ids.insert(id).second && find_id(id) == &g);
    CHECK(id.find(' ') == std::string::npos && id.rfind("phasmomods.custom-ghosts.", 0) == 0);
    for (char c : id) CHECK(!std::isupper(static_cast<unsigned char>(c)));
  }
  CHECK(!find_id("phasmomods.custom-ghosts.") && !find_id("vanilla.mare") && !find_id(""));
}

void every_page_tells_its_story() {
  // The journal applies no length limit; these are the current longest entries plus a margin.
  constexpr size_t kMaxDescription = 260, kMaxStrength = 180, kMaxLine = 150;
  const auto clean = [](const char* text, size_t most) {
    const std::string s = text ? text : "";
    return !s.empty() && s.size() <= most && s.find("investigator") == std::string::npos &&
           s.find("Investigator") == std::string::npos && s.find("\xE2\x80\x94") == std::string::npos;
  };
  for (const Ghost& g : kGhosts) {
    CHECK(clean(g.description, kMaxDescription));
    CHECK(clean(g.strength, kMaxStrength));  // strengths carry a mechanic or two
    CHECK(clean(g.weakness, kMaxLine));
    if (g.abilityDescription) CHECK(clean(g.abilityDescription, kMaxLine));
  }
  // The docs say "player" and carry no em dash either.
  std::ifstream in(std::string(PM_SOURCE_DIR) + "/docs/custom-ghosts.md", std::ios::binary);
  const std::string docs((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  CHECK(!docs.empty());
  CHECK(docs.find("investigator") == std::string::npos && docs.find("Investigator") == std::string::npos);
  CHECK(docs.find("\xE2\x80\x94") == std::string::npos);
}

// The user's own words, 2026-10-31: what a player reads of the two ghosts held to one target, from the mod's table.
void the_held_ghosts_say_who_they_hunt() {
  CHECK(pm::i18n::text(find(kDraugr)->strength) ==
        "Carry anything out of a Draugr's room and it may hunt while your sanity is still high, going only for the "
        "thief until they reach the truck or die.");
  CHECK(pm::i18n::text(find(kBogeyman)->strength) ==
        "A Bogeyman's hunts begin from the closet or locker nearest the youngest player, and they go only for that "
        "player until they reach the truck or die.");
}

void the_flame_ghosts_carry_the_flame() {
  const auto has = [](int type, const char* e) {
    const Ghost* g = find(type);
    return g && gives(*g, e);
  };
  CHECK(has(kDraugr, kSpectralFlame) && has(kDraugr, kEmf) && has(kDraugr, kFreezing) && !has(kDraugr, kOrbs));
  CHECK(has(kKikimora, kSpectralFlame) && has(kKikimora, kUltraviolet) && has(kKikimora, kWriting) &&
        !has(kKikimora, kOrbs));
  for (int type : {kEcho, kDullahan, kBlackShuck}) CHECK(!has(type, kSpectralFlame));
  CHECK(std::string(kSpectralFlame) == pm::flame::contract::kEvidenceId);  // the id Spectral Flame registers
  // Twelve carry it, so it turns up about as often as a vanilla evidence (90 / 7 = 13 of 30).
  int carriers = 0;
  std::map<std::string, int> beside;
  for (const Ghost& g : kGhosts) {
    if (!gives(g, kSpectralFlame)) continue;
    ++carriers;
    for (const char* e : g.evidence)
      if (std::strcmp(e, kSpectralFlame) != 0) ++beside[e];
  }
  CHECK(carriers == 12);
  CHECK(beside.size() == 7);  // every vanilla evidence, spread evenly beside it
  for (const auto& [e, n] : beside) CHECK(n == 3 || n == 4);
}

void the_knell_moves_on_a_strict_beat() {
  CHECK(knell_surging(0) && !knell_surging(1.01f) && knell_surging(2.01f));
  CHECK(std::fabs(knell_speed(kGameBase, 0.5f) - kGameBase * 1.75f) < 1e-5f);  // 2.98 m/s, no cap
  CHECK(std::fabs(knell_speed(kGameBase, 1.5f) - kGameBase * 0.35f) < 1e-5f);  // 0.6 m/s
}

void the_hydra_grows_heads() {
  CHECK(hydra_heads(0) == 0 && hydra_heads(1) == 1 && hydra_heads(3) == 3 && hydra_heads(7) == kHydraMaxHeads);
  CHECK(hydra_double_visible(0) && !hydra_double_visible(-0.1f) && !hydra_double_visible(kHydraFade));
  int shown = 0, early = 0;
  for (float t = 0; t < kHydraFade; t += 0.01f) {
    shown += hydra_double_visible(t);
    if (t < kHydraFade / 2) early += hydra_double_visible(t);
  }
  CHECK(early > shown - early);  // it fades: shown more in its first second than its last
  const std::vector<Head> heads = {{1, 2, 3, 90}, {-4, 0.5f, 6, 180}};
  const std::vector<uint8_t> b = encode_heads(heads);
  std::vector<Head> back;
  CHECK(decode_heads(b.data(), b.size(), back) && back.size() == 2 && back[1].x == -4 && back[1].yaw == 180);
  CHECK(!decode_heads(b.data(), b.size() - 1, back));
  CHECK(encode_heads({{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}})[4] == kHydraMaxHeads);
  Head d{};
  const std::vector<uint8_t> db = encode_double({5, 6, 7, 45});
  CHECK(decode_double(db.data(), db.size(), d) && d.z == 7 && d.yaw == 45);
  CHECK(!decode_double(b.data(), b.size(), d));  // a heads message is not a double
  const uint8_t echo[] = {'C', 'G', 2, 1};
  CHECK(net_msg::kind_of(echo, 4) == 0);  // the Echo's messages are never taken for these
}

void the_flame_ghost_messages_round_trip() {
  float f = 0;
  const std::vector<uint8_t> hit = net_msg::hit(12.5f);
  CHECK(net_msg::read_hit(hit.data(), hit.size(), f) && f == 12.5f);
  CHECK(!net_msg::read_hit(hit.data(), hit.size() - 1, f));
  const std::vector<uint8_t> strike = net_msg::strike();
  CHECK(net_msg::read_strike(strike.data(), strike.size()));
  CHECK(!net_msg::read_strike(strike.data(), strike.size() - 1));  // cut short
  std::vector<uint8_t> longer = strike;
  longer.push_back(1);
  CHECK(!net_msg::read_strike(longer.data(), longer.size()));       // a body it does not have
  CHECK(!net_msg::read_strike(hit.data(), hit.size()));             // another kind
  int list = -1, slot = -1;
  const std::vector<uint8_t> fade = net_msg::forget(2, 7);
  CHECK(net_msg::read_forget(fade.data(), fade.size(), list, slot) && list == 2 && slot == 7);
  const std::vector<uint8_t> bad = net_msg::forget(3, 0);
  CHECK(!net_msg::read_forget(bad.data(), bad.size(), list, slot));  // only photos, videos, sounds
  const std::vector<uint8_t> viewing = net_msg::header(net_msg::kViewing);
  CHECK(net_msg::kind_of(viewing.data(), viewing.size()) == net_msg::kViewing);
  bool angered = false;
  const std::vector<uint8_t> anger = net_msg::anger(true);
  CHECK(net_msg::read_anger(anger.data(), anger.size(), angered) && angered);
  const std::vector<uint8_t> calm = net_msg::anger(false);
  CHECK(net_msg::read_anger(calm.data(), calm.size(), angered) && !angered);
  CHECK(!net_msg::read_anger(anger.data(), anger.size() - 1, angered));  // cut short
  CHECK(!net_msg::read_anger(strike.data(), strike.size(), angered));    // another kind
}

void the_asterion_batters_doors() {
  CHECK(std::fabs(delta_angle(350, 10) - 20) < 0.001f && std::fabs(delta_angle(10, 350) + 20) < 0.001f);
  CHECK(door_closed(91, 90) && door_closed(-5, 355) && !door_closed(140, 90));
  // Passing an open door: it leaves it alone, now and once it has left it behind.
  DoorTrack open;
  CHECK(open.step(true, 3, false, true, 0.1f) == DoorAction::None);
  CHECK(open.step(true, 0.8f, false, true, 0.1f) == DoorAction::None);
  CHECK(open.step(true, 2.0f, false, false, 0.1f) == DoorAction::None);
  CHECK(open.step(true, 2.5f, false, false, 0.1f) == DoorAction::None && !open.battering());
  // A closed door in its way: bangs every 0.6 s for 3 s, then bursts, then stays jammed open.
  DoorTrack shut;
  CHECK(shut.step(true, 1.0f, true, true, 0.1f) == DoorAction::Bang && shut.battering());
  int bangs = 1, bursts = 0;
  for (int i = 0; i < 40; ++i) {
    const DoorAction a = shut.step(true, 1.0f, true, true, 0.1f);
    bangs += a == DoorAction::Bang;
    bursts += a == DoorAction::Burst;
  }
  CHECK(bursts == 1 && bangs == 5 && !shut.battering());
  CHECK(shut.step(true, 0.5f, true, true, 0.1f) == DoorAction::None);   // jammed: never again
  CHECK(shut.step(false, 0.5f, true, true, 0.1f) == DoorAction::None);  // not even next hunt
  // In its way means ahead with its heading crossing the leaf: a side door in a hallway is not.
  CHECK(door_ahead(1, 0, 0.9f, 0.1f, 0, 1));       // the doorway it walks through (leaf across its path)
  CHECK(door_ahead(1, 0.3f, 0.9f, 0.3f, 0, -1));   // the same, at a slant
  CHECK(!door_ahead(1, 0, 0.98f, 0.5f, 1, 0));     // a side door 0.5 m off the hallway, leaf along it
  CHECK(!door_ahead(1, 0, 0.4f, 1.0f, 0, 1));      // a doorway well off to one side
  CHECK(!door_ahead(1, 0, -1.0f, 0, 0, 1));        // behind it
  CHECK(!door_ahead(0, 0, 1.0f, 0, 0, 1));         // standing still
  // A closed door it is not walking into does not stop it.
  DoorTrack beside;
  CHECK(beside.step(true, 0.9f, true, false, 0.1f) == DoorAction::None && !beside.battering());
  // A hunt ending mid-battering frees the door.
  DoorTrack ended;
  ended.step(true, 1.0f, true, true, 0.1f);
  CHECK(ended.step(false, 1.0f, true, true, 0.1f) == DoorAction::None && !ended.battering());
}

void the_lithobolia_aims_true() {
  const Vec from{0, 1, 0}, to{6, 1.2f, 2};
  const Vec v = aim_velocity(from, to);
  const float t = length({to.x - from.x, to.y - from.y, to.z - from.z}) / kLithoboliaSpeed;
  const Vec landed = position_after(from, v, t);
  CHECK(std::fabs(landed.x - to.x) < 0.01f && std::fabs(landed.y - to.y) < 0.01f && std::fabs(landed.z - to.z) < 0.01f);
  CHECK(v.y > 0);  // lobbed a little against gravity
  const Vec none = aim_velocity(from, from);
  CHECK(none.x == 0 && none.y == 0 && none.z == 0);
  CHECK(lithobolia_hit({0, 1.2f, 0.5f}, 5, {0, 1.2f, 0}));
  CHECK(!lithobolia_hit({0, 1.2f, 0.5f}, 1, {0, 1.2f, 0}));  // rolled to a stop: no harm
  CHECK(!lithobolia_hit({0, 1.2f, 1.0f}, 5, {0, 1.2f, 0}));  // a miss
}

void the_lithobolia_only_throws_at_people() {
  CHECK(nearest_in_range({}, kLithoboliaRange) == -1);  // nobody in sight: no throw at all
  CHECK(nearest_in_range({9.0f, 12.0f}, kLithoboliaRange) == -1);
  CHECK(nearest_in_range({6.0f, 3.0f, 7.5f}, kLithoboliaRange) == 1);
}

// The ghost's vision sensor detects a player's PlayerLOSTarget objects (Player +0x80), which the game's
// own hunt sight test looks for; it never detects the Player's own GameObject. Asking about that object
// answered "not seen" for everyone, so the Lithobolia swallowed every throw.
void the_ghost_sees_a_player_through_their_sight_parts() {
  int root = 0, head = 0, chest = 0;
  void* parts[] = {&head, nullptr, &chest};
  auto detected = [&](void* only) { return [only](void* object) { return object == only; }; };
  CHECK(pm::game::sees_a_part(parts, 3, detected(&chest)));   // one part in view is enough
  CHECK(pm::game::sees_a_part(parts, 3, detected(&head)));
  CHECK(!pm::game::sees_a_part(parts, 3, detected(&root)));   // the Player's own object is never what it detects
  CHECK(!pm::game::sees_a_part(parts, 3, detected(nullptr)));  // an empty slot is not a part
  CHECK(!pm::game::sees_a_part(nullptr, 3, detected(&head)));  // a player with no parts is not seen
}

// A false head throws as the hunt throws (hunt update 0x2147380): on each tick of the hunt's own throw
// timer, after the hunt's own roll, the first object the game's test passes among those its interaction
// sensor would detect around the head, in random order.
void the_hydras_heads_throw_as_the_hunt_does() {
  // The tick: the same hunt's timer was set back up after running out.
  CHECK(hydra_throw_tick(true, 0.01f, 0.5f));
  CHECK(!hydra_throw_tick(true, 0.30f, 0.28f));  // counting down
  CHECK(!hydra_throw_tick(true, 0.30f, 0.30f));
  CHECK(!hydra_throw_tick(false, 0.01f, 0.5f));  // a new hunt's first read is no tick
  // The roll: the Poltergeist (type 3 in build 25811160, read from the game) is never held back; any
  // other loses on RandomRangeInt(0, 2) == 1.
  const int poltergeist = 3;
  CHECK(hydra_throw_rolls(3, poltergeist, 1) && hydra_throw_rolls(3, poltergeist, 0));
  CHECK(hydra_throw_rolls(40, poltergeist, 0));
  CHECK(!hydra_throw_rolls(40, poltergeist, 1));
  // The pick: the first shuffled candidate the game's test passes, skipping what is taken this tick, and
  // asking the test no further once one passes.
  int a = 0, b = 0, c = 0, d = 0;
  const std::vector<void*> shuffled{&a, &b, &c, &d};
  int asked = 0;
  auto passes = [&](void* prop) { ++asked; return prop != &a; };
  CHECK(hydra_head_pick(true, shuffled, {}, passes) == 1 && asked == 2);  // a fails, b passes, c and d unasked
  asked = 0;
  CHECK(hydra_head_pick(true, shuffled, {&b}, passes) == 2 && asked == 2);  // b taken (another head, or the ghost)
  asked = 0;
  CHECK(hydra_head_pick(false, shuffled, {}, passes) == -1 && asked == 0);  // the roll held it back
  CHECK(hydra_head_pick(true, {&a}, {}, passes) == -1);
  CHECK(hydra_head_pick(true, {}, {}, passes) == -1);
  // The head's sensor sits on the head as the ghost's sits on the ghost.
  const Quat none{0, 0, 0, 1}, quarter{0, 0.70710678f, 0, 0.70710678f};  // 90 degrees about y
  const SensorPose same = hydra_sensor_pose({1, 0, 1}, none, {1, 0.5f, 2}, none, {5, 0, 5}, none);
  CHECK(std::fabs(same.at.x - 5) < 1e-4f && std::fabs(same.at.y - 0.5f) < 1e-4f && std::fabs(same.at.z - 6) < 1e-4f);
  const SensorPose turned = hydra_sensor_pose({0, 0, 0}, none, {0, 0, 1}, none, {0, 0, 0}, quarter);
  CHECK(std::fabs(turned.at.x - 1) < 1e-4f && std::fabs(turned.at.z) < 1e-4f);  // forward turned to +x
  CHECK(std::fabs(turned.rotation.y - quarter.y) < 1e-4f && std::fabs(turned.rotation.w - quarter.w) < 1e-4f);
  const SensorPose back = hydra_sensor_pose({0, 0, 0}, quarter, {1, 0, 0}, quarter, {0, 0, 0}, none);
  CHECK(std::fabs(back.at.z - 1) < 1e-4f && std::fabs(back.at.x) < 1e-4f);  // undone from a turned ghost
  CHECK(std::fabs(back.rotation.w - 1) < 1e-4f);
}

// The Hydra's copy finds its Animator where the model's (GhostModel +0x48) sits in the model's hierarchy,
// on the root or on a child: the copy is a clone, so the same place in the same order.
void the_copy_finds_the_animator_where_the_model_keeps_it() {
  int root = 0, rig = 0, mesh = 0, other = 0;
  const std::vector<void*> model{&root, &rig, &mesh};
  CHECK(index_in(model, &rig) == 1);
  CHECK(index_in(model, &root) == 0);
  CHECK(index_in(model, &other) == -1);  // not in the model: the copy keeps looking on its root
  CHECK(index_in(model, nullptr) == -1);
  CHECK(index_in({}, &rig) == -1);
}

// The Gremlin's electronics are the game's equipment types by the map (sdk/map/Equipment.h), told
// apart by each object's class on the fake backend: cameras, sensors and torches, never a candle or a
// crucifix.
void the_gremlin_leaves_watched_equipment_alone() {
  namespace fake = pm::fake;
  namespace map = pm::map;
  fake::reset();
  int cctvClass = 0, motionClass = 0, torchClass = 0, candleClass = 0, crucifixClass = 0;
  fake::type(map::CCTV, &cctvClass);
  fake::type(map::MotionSensor, &motionClass);
  fake::type(map::Torch, &torchClass);
  int others[7] = {};
  const pm::sdk::EntryDesc* rest[] = {&map::SoundSensor,   &map::EMFReader,   &map::ParabolicMicrophone,
                                      &map::SoundRecorder, &map::Thermometer, &map::EVPRecorder,
                                      &map::IRLightSensor};
  for (int i = 0; i < 7; ++i) fake::type(*rest[i], &others[i]);
  std::string missing;
  pm::api::bind(fake::host(), missing);  // binds what is given; the rest stays unresolved
  // Each fake object is one pointer, its class.
  void* objects[5] = {&cctvClass, &motionClass, &torchClass, &candleClass, &crucifixClass};
  CHECK(gremlin::electronic(&objects[0]) && gremlin::electronic(&objects[1]) && gremlin::electronic(&objects[2]));
  CHECK(!gremlin::electronic(&objects[3]) && !gremlin::electronic(&objects[4]) && !gremlin::electronic(nullptr));
  fake::reset();
  CHECK(gremlin_may_sabotage(true, 5));
  CHECK(!gremlin_may_sabotage(false, 5));               // already off
  CHECK(!gremlin_may_sabotage(true, kGremlinGuard));     // someone beside it
  CHECK(!gremlin_may_sabotage(true, 0.4f));              // held
}

// The hunt's may-go-for test, rebuilt from the game's own (0x214FD80 in build 25811160) on the fake
// backend: a player with a room, alive, not in the killed list and not in a room outside the map; never
// one the game's test would throw for (no room, no killed list). The start delay is over below 0, and
// the seen list takes each player once.
void the_hunt_goes_only_for_whom_the_game_would() {
  namespace fake = pm::fake;
  namespace map = pm::map;
  fake::reset();
  fake::field(map::GhostHuntState_seen, 0x68);
  fake::field(map::GhostHuntState_killed, 0x70);
  fake::field(map::GhostHuntState_startDelay, 0x8C);
  fake::field(map::Player_isDead, 0x28);
  fake::field(map::Player_currentRoom, 0x50);
  fake::field(map::LevelRoom_kind, 0x58);
  fake::value(map::RoomKind_outsideMap, 1);
  std::string missing;
  pm::api::bind(fake::host(), missing);
  alignas(8) unsigned char hunt[0x100] = {}, player[0x100] = {}, room[0x100] = {};
  const auto put = [](unsigned char* object, size_t offset, auto value) {
    std::memcpy(object + offset, &value, sizeof value);
  };
  const int32_t inside = 0, outsideMap = 1, exterior = 2;
  CHECK(!hunt::may_go_for(hunt, player));  // no room: the game's test throws
  put(player, 0x50, static_cast<void*>(room));
  CHECK(!hunt::may_go_for(hunt, player));  // no killed list: the game's test throws
  void* killed = fake::list({});
  put(hunt, 0x70, killed);
  put(room, 0x58, inside);
  CHECK(hunt::may_go_for(hunt, player));
  CHECK(!hunt::may_go_for(nullptr, player) && !hunt::may_go_for(hunt, nullptr));
  put(room, 0x58, exterior);
  CHECK(hunt::may_go_for(hunt, player));  // outdoors is still the map
  put(room, 0x58, outsideMap);
  CHECK(!hunt::may_go_for(hunt, player));  // the truck
  put(room, 0x58, inside);
  put(player, 0x28, true);
  CHECK(!hunt::may_go_for(hunt, player));  // dead
  put(player, 0x28, false);
  CHECK(fake::list_insert(killed, 0, player));
  CHECK(!hunt::may_go_for(hunt, player));  // killed in this hunt

  put(hunt, 0x8C, 3.0f);
  CHECK(!hunt::begun(hunt));
  put(hunt, 0x8C, -0.1f);
  CHECK(hunt::begun(hunt) && !hunt::begun(nullptr));

  hunt::record_seen(hunt, player);  // no seen list: nothing
  void* seen = fake::list({});
  put(hunt, 0x68, seen);
  hunt::record_seen(hunt, player);
  hunt::record_seen(hunt, player);
  hunt::record_seen(hunt, nullptr);
  CHECK(fake::list_items_of(seen) == std::vector<void*>{player});
  fake::reset();
}

// The Hydra's heads and the Lithobolia's sight copy the ghost's own sensors (sensor_copy.h): a copy takes
// the class's public value fields (Type::copy_settings), a volume sensor's detection mode and both its
// filters, and it pulses through its own class's PulseAll, never another's.
int g_rangePulses = 0, g_losPulses = 0;
void fake_range_pulse(void*, const void*) { ++g_rangePulses; }
void fake_los_pulse(void*, const void*) { ++g_losPulses; }

void the_sensor_copies_take_the_ghosts_settings() {
  namespace fake = pm::fake;
  namespace map = pm::map;
  static int sensorClass, volumeClass, rangeClass, losClass;
  fake::reset();
  fake::type(map::RangeSensor, &rangeClass);
  fake::type(map::LOSSensor, &losClass);
  fake::type(map::BaseVolumeSensor, &volumeClass);
  fake::parent(&rangeClass, &volumeClass);
  fake::parent(&volumeClass, &sensorClass);
  fake::parent(&losClass, &sensorClass);
  fake::field(map::BaseVolumeSensor_signalFilter, 0xC0);
  fake::field(map::Sensor_filter, 0x80);
  fake::declare_fields(&volumeClass, {{.offset = 0xC8, .size = 4, .publicValue = true}});
  fake::declare_fields(&rangeClass, {{.offset = 0xF8, .size = 0x21, .publicValue = true}});
  fake::declare_fields(&losClass, {{.offset = 0xD0, .size = 4, .publicValue = true}});
  fake::method(map::RangeSensor_PulseAll, reinterpret_cast<void*>(&fake_range_pulse));
  fake::method(map::LOSSensor_PulseAll, reinterpret_cast<void*>(&fake_los_pulse));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  alignas(8) unsigned char ghosts[0x140] = {}, copy[0x140] = {}, los[0x140] = {};
  void* const range = &rangeClass;
  void* const sight = &losClass;
  std::memcpy(ghosts, &range, sizeof range);
  std::memcpy(copy, &range, sizeof range);
  std::memcpy(los, &sight, sizeof sight);
  int signalFilter = 0, filter = 0;
  void* refs[] = {&signalFilter, &filter};
  const int32_t mode = 2, shape = 1;
  std::memcpy(ghosts + 0xC8, &mode, 4);
  std::memcpy(ghosts + 0xC0, &refs[0], 8);
  std::memcpy(ghosts + 0x80, &refs[1], 8);
  std::memcpy(ghosts + 0xF8, &shape, 4);
  ghosts[0x118] = 1;  // the last of RangeSensor's settings, its flag

  CHECK(sensor_copy::share_filters(copy, ghosts));
  int32_t got = 0;
  void* gotRef = nullptr;
  std::memcpy(&got, copy + 0xC8, 4);
  CHECK(got == 2);  // the detection mode
  std::memcpy(&gotRef, copy + 0xC0, 8);
  CHECK(gotRef == &signalFilter);
  std::memcpy(&gotRef, copy + 0x80, 8);
  CHECK(gotRef == &filter);
  std::memcpy(&got, copy + 0xF8, 4);
  CHECK(got == 0);  // the settings wait for the pulse

  CHECK(sensor_copy::copy_settings(sensor_copy::Kind::Range, copy, ghosts));
  std::memcpy(&got, copy + 0xF8, 4);
  CHECK(got == 1 && copy[0x118] == 1);
  CHECK(!sensor_copy::copy_settings(sensor_copy::Kind::LineOfSight, copy, ghosts));  // not a LOSSensor
  CHECK(!sensor_copy::copy_settings(sensor_copy::Kind::Range, los, ghosts));

  CHECK(sensor_copy::pulse(sensor_copy::Kind::Range, copy, "test") && g_rangePulses == 1 && g_losPulses == 0);
  CHECK(sensor_copy::pulse(sensor_copy::Kind::LineOfSight, los, "test") && g_rangePulses == 1 && g_losPulses == 1);

  // A public value field the host cannot read (a size of 0 on the fake) refuses the whole copy, as the
  // host's does: share_filters fails and copies neither the mode nor the filters.
  fake::declare_fields(&volumeClass, {{.offset = 0xC8, .size = 4, .publicValue = true},
                                      {.offset = 0xCC, .size = 0, .publicValue = true}});
  alignas(8) unsigned char refused[0x140] = {};
  std::memcpy(refused, &range, sizeof range);
  CHECK(!sensor_copy::share_filters(refused, ghosts));
  std::memcpy(&got, refused + 0xC8, 4);
  CHECK(got == 0);
  std::memcpy(&gotRef, refused + 0xC0, 8);
  CHECK(gotRef == nullptr);
  std::memcpy(&gotRef, refused + 0x80, 8);
  CHECK(gotRef == nullptr);
  fake::reset();
}

// The game's sight test, faked: it records whether own_sight() was set for the call, then sees, or
// throws as the game's does for a ghost with no vision sensor.
bool g_seesThrows = false;
bool g_seesSawOwn = false;
bool fake_sees(void*, void*, bool, const void*) {
  g_seesSawOwn = hunt::own_sight();
  if (g_seesThrows) throw std::runtime_error("the game's sight test threw");
  return true;
}

void the_own_sight_flag_survives_a_throwing_sight_test() {
  namespace fake = pm::fake;
  fake::reset();
  fake::method(pm::map::GhostHuntState_Sees, reinterpret_cast<void*>(&fake_sees));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  alignas(8) unsigned char huntState[0x100] = {}, player[0x100] = {};
  CHECK(!hunt::own_sight());
  CHECK(hunt::own_sees(huntState, player) && g_seesSawOwn);
  CHECK(!hunt::own_sight());  // the sight detour filters the next call again
  g_seesThrows = true;
  bool threw = false;
  try {
    hunt::own_sees(huntState, player);
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw && g_seesSawOwn);
  CHECK(!hunt::own_sight());  // restored although the game's test threw
  g_seesThrows = false;
  hunt::clear_own_sight();
  CHECK(!hunt::own_sight());
  fake::reset();
}

void the_bogeyman_searches_the_hiding_places() {
  const std::vector<Place> places = {{10, 0, 0}, {2, 0, 0}, {3, 3, 0}, {6, 0, 0}};
  std::vector<bool> searched(places.size(), false);
  CHECK(next_hiding_place(places, searched, {0, 0, 0}) == 1);    // the nearest first
  searched[1] = true;
  CHECK(next_hiding_place(places, searched, {2, 0, 0}) == 3);    // upstairs counts as farther
  searched[3] = searched[2] = searched[0] = true;
  CHECK(next_hiding_place(places, searched, {0, 0, 0}) == -1);   // each once per hunt
  CHECK(next_hiding_place({}, {}, {0, 0, 0}) == -1);
  CHECK(open_room(40) && !open_room(12));
}

void the_bogeyman_stalks_the_youngest() {
  CHECK(youngest({}) == -1);
  CHECK(youngest({{0, 50, 3, false}}) == -1);                              // the dead are never picked
  CHECK(youngest({{1, 5, 2, true}, {0, 90, 9, true}}) == 1);               // prestige first
  CHECK(youngest({{0, 20, 9, true}, {0, 20, 2, true}, {0, 30, 1, true}}) == 1);  // then level, then the lowest actor
  // A player whose progress cannot be read comes after every player whose progress can.
  CHECK(youngest({{kUnknownProgress, kUnknownProgress, 1, true}, {3, 90, 9, true}}) == 1);
  CHECK(youngest({{kUnknownProgress, kUnknownProgress, 1, true}}) == 0);
  // In an open room it loses sight beyond 7 m: such a player does not stay its prey.
  CHECK(open_sight_lost(true, 7.5f) && !open_sight_lost(true, 7.0f) && !open_sight_lost(false, 20.0f));
}

// Its AI breaks a final tie between equally young players by join order, the lowest actor, as its player
// target does: never by distance, which differs with the ghost's position on each client. The nearer of
// two equal players (listed first, actor 7) loses to the farther one who joined first (actor 3).
void bogeyman_ai_tie_breaks_by_join_order() {
  const Candidate nearer{.prestige = 0, .level = 20, .actor = 7, .living = true};
  const Candidate farther{.prestige = 0, .level = 20, .actor = 3, .living = true};
  CHECK(youngest({nearer, farther}) == 1);
  CHECK(youngest({farther, nearer}) == 0);
  CHECK(youngest({nearer, farther}) == bogeyman_target({nearer, farther}));  // one rule for both
}

// The Bogeyman's target, to which its hunts are held as the Banshee's, is the youngest living player:
// the lowest prestige, then the lowest level, by the lobby's board ({prestige, level, actor, living}).
void bogeyman_target_is_youngest() {
  CHECK(bogeyman_target({}) == -1);
  CHECK(bogeyman_target({{2, 10, 3, true}, {1, 90, 9, true}, {1, 40, 20, true}}) == 2);
  CHECK(bogeyman_target({{kUnknownProgress, kUnknownProgress, 1, true}, {3, 1, 9, true}}) == 1);
}

// When the youngest dies, the target moves on to the next youngest, and with nobody alive there is none.
void bogeyman_target_moves_on_when_youngest_dies() {
  std::vector<Candidate> players = {{2, 10, 3, true}, {1, 90, 9, true}, {1, 40, 20, true}};
  CHECK(bogeyman_target(players) == 2);
  players[2].living = false;
  CHECK(bogeyman_target(players) == 1);
  players[1].living = false;
  CHECK(bogeyman_target(players) == 0);
  players[0].living = false;
  CHECK(bogeyman_target(players) == -1);
  // Where the game re-targets away from a player who is still alive (a kill, the Hanged Man card): the
  // next youngest other than them, equal progress by join order; nobody when none else lives.
  const std::vector<Candidate> lobby = {{1, 40, 20, true}, {1, 40, 9, true}, {2, 1, 3, true}};
  CHECK(bogeyman_target(lobby) == 1);
  const int32_t nine[] = {9}, twenty[] = {20}, both[] = {9, 20};
  CHECK(bogeyman_target(lobby, nine) == 0 && bogeyman_target(lobby, twenty) == 1);
  CHECK(bogeyman_target(lobby, both) == 2);  // the dying player and the current target both passed over
  CHECK(bogeyman_target({{1, 40, 20, true}}, twenty) == -1);
}

// Every client names the same player: the rule reads only what every client knows alike (the lobby's
// progress, deaths and actors), whatever order its lobby lists the players in, and equal progress goes to
// the lowest actor, never to the nearest, which differs with the ghost's position on each client.
void bogeyman_target_on_client() {
  const std::vector<Candidate> host = {{0, 1, 4, true}, {0, 1, 2, true}, {3, 50, 1, true}};
  const std::vector<Candidate> client = {{3, 50, 1, true}, {0, 1, 2, true}, {0, 1, 4, true}};  // another order
  const int onHost = bogeyman_target(host), onClient = bogeyman_target(client);
  CHECK(onHost == 1 && onClient == 1 && host[onHost].actor == 2 && client[onClient].actor == 2);
}

void the_bogeyman_lurks_in_the_nearest_closet() {
  CHECK(lurk_place({{0, 0, 12}}, {0, 0, 0}) == -1);                        // none within 10 m: it starts where it is
  CHECK(lurk_place({{0, 0, 8}, {0, 0, 3}}, {0, 0, 0}) == 1);
  CHECK(lurk_place({{0, 0, 10}}, {0, 0, 0}) == 0);                         // exactly 10 m is within reach
  CHECK(lurk_place({{4, 3.5f, 0}}, {0, 0, 0}) == -1);                      // 4 m along but 3.5 m up: another floor
  CHECK(lurk_place({{9, 1.5f, 0}}, {0, 0, 0}) == 0);                       // a step up on their floor: its true distance
}

// A player's hunt of the Bogeyman begins as its AI's does: the hunt they start is the same state change, and
// the rule that steps it out of a closet has no input for who steers, so it picks the closet nearest the
// youngest living player, its target, as for the AI.
void player_bogeyman_hunt_starts_at_youngest_closet() {
  // The lobby ({prestige, level, actor, living}) and where each stands; the youngest is the third.
  const std::vector<Candidate> lobby = {{3, 10, 1, true}, {2, 40, 2, true}, {0, 99, 3, true}};
  const std::vector<Place> at = {{20, 0, 0}, {0, 0, 20}, {-20, 0, 0}};
  const std::vector<Place> closets = {{18, 0, 1}, {1, 0, 18}, {-17, 0, 2}, {-25, 0, 0}};
  const int target = bogeyman_target(lobby);
  CHECK(target == 2 && youngest(lobby) == target);  // its hold and its hunt start name the same player
  CHECK(lurk_place(closets, at[static_cast<size_t>(target)]) == 2);  // 3.6 m from them, the nearest
  // The gate: a change of its own ghost to the hunting state, on the host, that made a new hunt state.
  CHECK(bogeyman_steps_out(true, true, true, false, true));
  CHECK(!bogeyman_steps_out(true, true, true, true, true));    // already hunting: the game ignores it
  CHECK(!bogeyman_steps_out(true, true, true, false, false));  // the difficulty turned the hunt into idling
  CHECK(!bogeyman_steps_out(true, false, true, false, true));  // not the host
  CHECK(!bogeyman_steps_out(false, true, true, false, true));  // another ghost
  CHECK(!bogeyman_steps_out(true, true, false, false, true));  // another state
}

// A player's door interaction on the Bogeyman runs the same idle door chance its AI's does, so the same rule
// turns 3 in 4 of them into a closet door in reach, and none in a hunt, off the host or with no closet door.
void player_bogeyman_door_share_matches_ai() {
  // The AI's pick and a player's aimed door alike: the door chance's own pick that found a door.
  CHECK(bogeyman_turns_door(true, true, true, true, false));
  CHECK(!bogeyman_turns_door(true, true, false, true, false));  // another caller's pick (the hunt state's)
  CHECK(!bogeyman_turns_door(true, true, true, false, false));  // the pick found no door
  CHECK(!bogeyman_turns_door(true, true, true, true, true));    // in a hunt
  CHECK(!bogeyman_turns_door(true, false, true, true, false));  // off the host
  CHECK(!bogeyman_turns_door(false, true, true, true, false));  // another ghost
  // Over evenly spread share rolls, 3 in 4 take a closet door, each of those in reach alike.
  int taken = 0;
  std::vector<int> each(3, 0);
  for (int i = 0; i < 400; ++i) {
    const int chosen = bogeyman_closet_door(3, static_cast<float>(i) / 400.0f, static_cast<size_t>(i));
    if (chosen < 0) continue;
    ++taken;
    ++each[static_cast<size_t>(chosen)];
  }
  CHECK(taken == 300 && each[0] == 100 && each[1] == 100 && each[2] == 100);
  CHECK(bogeyman_closet_door(0, 0.0f, 5) == -1);  // no closet door in reach: the game's own pick
}

void the_brownie_works_unseen() {
  // A player at the origin facing +z.
  CHECK(in_view(0, 0, 0, 1, 0, 5, kBrowniePlayerCone, kBrowniePlayerRange));            // straight ahead
  CHECK(in_view(0, 0, 0, 1, 3, 5, kBrowniePlayerCone, kBrowniePlayerRange));            // 31 degrees off
  CHECK(!in_view(0, 0, 0, 1, 5, 2, kBrowniePlayerCone, kBrowniePlayerRange));           // 68 degrees off
  CHECK(!in_view(0, 0, 0, 1, 0, -5, kBrowniePlayerCone, kBrowniePlayerRange));          // behind
  CHECK(!in_view(0, 0, 0, 1, 0, 20, kBrowniePlayerCone, kBrowniePlayerRange));          // too far
  CHECK(in_view(0, 0, 0, 1, 0.3f, -0.2f, kBrowniePlayerCone, kBrowniePlayerRange));     // touching them
  // Its own speed in place of the game's base x sight: the line-of-sight speed-up never reaches it.
  CHECK(brownie_speed(kGameBase, false) == kGameBase * kBrownieUnseen);
  CHECK(brownie_speed(kGameBase, true) == kGameBase * kBrownieWatched);
  // a slow walk, never the Deogen's crawl
  CHECK(brownie_speed(kGameBase, true) < kGameBase && brownie_speed(kGameBase, true) > 2 * 0.4f);
}

void the_brownie_steps_quietly_as_a_myling() {
  CHECK(quiet_step_distance(20.0f) == 12.0f);  // the game's Myling rule: 8 m less than the default
}

void the_lethe_makes_them_forget() {
  CHECK(!lethe_fades(179, false) && lethe_fades(180, false) && !lethe_fades(500, true));  // looked at: kept
  // Only an investigator looks: a client whose own investigator is out of the game's player list (a ghost player's parked body)
  // doesn't keep the media from fading, whatever its journal shows.
  CHECK(lethe_looking(true, true));
  CHECK(!lethe_looking(true, false) && !lethe_looking(false, true) && !lethe_looking(false, false));
  CHECK(!lethe_forgets(0.9f) && lethe_forgets(1.0f));
  CHECK(lethe_replay_at(10, 0) == 10 + kLetheReplayMin && lethe_replay_at(10, 1) == 10 + kLetheReplayMax);
  CHECK(lethe_replay_at(0, 0.5f) > kLetheReplayMin && lethe_replay_at(0, 0.5f) < kLetheReplayMax);
  const ShowingStates states{2, 8, 14};  // hunting, appear, randomEvent in build 25811160
  CHECK(lethe_state_shows_itself(2, states) && lethe_state_shows_itself(8, states) && lethe_state_shows_itself(14, states));
  CHECK(!lethe_state_shows_itself(0, states) && !lethe_state_shows_itself(1, states) &&
        !lethe_state_shows_itself(3, states));
  // Only what records its activity fades: the ghost, its writing, its sounds; not bones or cursed objects.
  // MediaSubject's numbers in build 25811160 (read from the game by name in lethe.cpp).
  const ActivitySubjects subjects{0,  2,  3,  5,  7,  13, 14, 15, 18, 22, 23, 24, 25, 26,
                                  27, 28, 29, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 43};
  const auto activity = [&](int subject) { return lethe_activity(subject, subjects); };
  CHECK(activity(5) && activity(13) && activity(26) && activity(43));
  CHECK(!activity(4) && !activity(6) && !activity(9) && !activity(16) && !activity(20));  // DNA, body, tarot, paw, none
}

void the_lethe_forgets_the_player_it_lost() {
  int a, b;
  LetheMemory<> m;
  CHECK(!m.forgets(&a, false, 0.5f));            // half a second out of the game's sight: still looked for
  CHECK(m.forgets(&a, false, 0.5f));             // a second: forgotten
  CHECK(!m.forgets(&a, false, 0.75f));           // looked for again: the count starts over
  CHECK(!m.forgets(&a, true, 0.5f));             // in sight: the count starts over
  CHECK(!m.forgets(&a, false, 0.75f));
  CHECK(!m.forgets(&b, false, 0.5f));            // another player: their own count, not a's
  CHECK(!m.forgets(nullptr, false, 5.0f));       // nobody looked for: nothing to forget
  CHECK(!m.forgets(&b, false, 0.75f) && m.forgets(&b, false, 0.25f));  // b's count started over
}

void the_tempest_strikes_with_the_weather() {
  // The contract's own weather (LevelValues' weather type) sets how often it strikes.
  // The weather types of build 25811160: rain 2, heavy rain 3 (WeatherType, read from the game).
  const auto sky = [](int type) { return tempest_sky(type, 2, 3); };
  CHECK(sky(0) == Sky::Clear && sky(1) == Sky::Clear && sky(4) == Sky::Clear);    // clear, fog, snow
  CHECK(sky(5) == Sky::Clear && sky(7) == Sky::Clear && sky(-1) == Sky::Clear);   // wind, blood moon, unread
  CHECK(sky(2) == Sky::Rain && sky(3) == Sky::HeavyRain);
  CHECK(tempest_next_strike(Sky::Clear, 0) == 9 && tempest_next_strike(Sky::Clear, 1) == 14);
  CHECK(tempest_next_strike(Sky::Rain, 0) == 6 && tempest_next_strike(Sky::Rain, 1) == 10);
  CHECK(tempest_next_strike(Sky::HeavyRain, 0) == 3 && tempest_next_strike(Sky::HeavyRain, 1) == 6);
  CHECK(tempest_next_strike(Sky::HeavyRain, 0.5f) == 4.5f);
  CHECK(tempest_next_strike(Sky::Rain, -2) == 6 && tempest_next_strike(Sky::Rain, 3) == 10);  // a draw out of range
  // Lit and fast for the strike's 1.5 s, dark and slow otherwise; never the line-of-sight speed-up.
  CHECK(!tempest_striking(-1) && tempest_striking(0) && tempest_striking(1.4f) && !tempest_striking(1.6f));
  CHECK(tempest_speed(kGameBase, true) == kGameBase * kTempestStrike);  // 3.83 m/s
  CHECK(tempest_speed(kGameBase, false) == kGameBase * kTempestDark);   // 1.02 m/s
  CHECK(tempest_speed(kGameBase, false) < kGameBase && tempest_speed(kGameBase, true) > 2 * kGameBase);
}

void the_tempest_opens_each_hunt_with_a_strike() {
  StrikeClock clock;
  CHECK(!tempest_strike_now(clock, false, 1000, 5000));  // no hunt, no strike
  CHECK(tempest_strike_now(clock, true, 2000, 5000));    // the hunt's first frame
  CHECK(!tempest_strike_now(clock, true, 2016, 5000));   // and only once as it opens
  CHECK(!tempest_strike_now(clock, true, 6999, 5000));   // the next waits out the gap drawn at the last
  CHECK(tempest_strike_now(clock, true, 7000, 3000));
  CHECK(!tempest_strike_now(clock, true, 9999, 3000));
  CHECK(tempest_strike_now(clock, true, 10000, 3000));
  CHECK(!tempest_strike_now(clock, false, 10500, 3000));  // the hunt ends with a strike still due
  CHECK(tempest_strike_now(clock, true, 11000, 3000));    // the next hunt opens with one all the same
  CHECK(!tempest_strike_now(clock, true, 11016, 3000));
}

void the_mesmer_shows_each_their_own() {
  // Different viewers of the same appearance see different things, and never the true form.
  int differ = 0;
  for (uint32_t appearance = 0; appearance < 20; ++appearance) {
    const int a = mesmer_form(mesmer_seed(appearance, 1), 6, 2), b = mesmer_form(mesmer_seed(appearance, 2), 6, 2);
    CHECK(a != 2 && b != 2 && a >= 0 && a < 6 && b >= 0 && b < 6);
    differ += a != b;
  }
  CHECK(differ >= 10);
  CHECK(mesmer_form(12345, 1, 0) == 0);  // only one form: it is that one
  // It turns to whoever sees it: 0 degrees looks along +z, 90 along +x, from wherever it stands.
  CHECK(std::fabs(mesmer_facing(2, 3, 2, 8)) < 0.001f);
  CHECK(std::fabs(mesmer_facing(2, 3, 7, 3) - 90.0f) < 0.001f);
  CHECK(std::fabs(std::fabs(mesmer_facing(2, 3, 2, -1)) - 180.0f) < 0.001f);
  CHECK(std::fabs(mesmer_facing(2, 3, -3, 3) + 90.0f) < 0.001f);
  float dx = 0, dz = 0;
  mesmer_offset(mesmer_seed(3, 7), dx, dz);
  const float r = std::sqrt(dx * dx + dz * dz);
  CHECK(r >= kMesmerOffsetMin - 0.001f && r <= kMesmerOffsetMax + 0.001f);
  CHECK(mesmer_shown_insanity(50) == 30 && mesmer_shown_insanity(10) == 0);  // the monitor flatters
  CHECK(std::fabs(mesmer_dim(0) - 1) < 0.001f && std::fabs(mesmer_dim(kMesmerDimPeriod / 2) - kMesmerDimLowest) < 0.001f);
  // Its true form is hidden from a player's eye by layer only where that hides nothing else.
  CHECK(mesmer_layers_safe(mesmer_layer_bit(10)) && mesmer_layers_safe(mesmer_layer_bit(10) | mesmer_layer_bit(11)));
  CHECK(!mesmer_layers_safe(mesmer_layer_bit(0)));                              // Default: the whole scene
  CHECK(!mesmer_layers_safe(mesmer_layer_bit(10) | mesmer_layer_bit(kMesmerFakeLayer)));  // the fake's own
  CHECK(!mesmer_layers_safe(0) && mesmer_layer_bit(-1) == 0 && mesmer_layer_bit(32) == 0);
}

void the_flame_ghosts_are_the_designed_ones() {
  CHECK(kGhostCount == 15);
  const int types[] = {kLithobolia, kGremlin, kBrownie, kBogeyman, kLethe, kMesmer, kKnell, kAsterion, kTempest, kHydra};
  for (int type : types) CHECK(find(type) != nullptr);
  const auto evidence_is = [](int type, const char* a, const char* b) {
    const Ghost* g = find(type);
    const std::set<std::string> e{g->evidence[0], g->evidence[1], g->evidence[2]};
    return e == std::set<std::string>{a, b, kSpectralFlame};
  };
  CHECK(evidence_is(kLithobolia, kSpiritBox, kFreezing));
  CHECK(evidence_is(kGremlin, kEmf, kDots));
  CHECK(evidence_is(kBrownie, kEmf, kWriting));
  CHECK(evidence_is(kBogeyman, kUltraviolet, kOrbs));
  CHECK(evidence_is(kLethe, kOrbs, kFreezing));
  CHECK(evidence_is(kMesmer, kSpiritBox, kDots));
  CHECK(evidence_is(kKnell, kSpiritBox, kOrbs));
  CHECK(evidence_is(kAsterion, kWriting, kDots));
  CHECK(evidence_is(kTempest, kSpiritBox, kUltraviolet));
  CHECK(evidence_is(kHydra, kOrbs, kDots));
  // Only the Hydra changes how its flame looks.
  for (const Ghost& g : kGhosts) CHECK((g.flameLook != nullptr) == (g.type == kHydra));
  CHECK(std::string(find(kHydra)->flameLook) == "green");
}

void the_dullahan_charges_straight_and_stalls_at_corners() {
  CHECK(kDullahanTurn == 45.0f);
  CHECK(std::fabs(dullahan_speed(kGameBase, 0) - kGameBase * 2.0f) < 0.001f);      // 3.4 m/s straight
  CHECK(std::fabs(dullahan_speed(kGameBase, 180) - kGameBase * 0.25f) < 0.001f);   // 0.43 facing away
  CHECK(dullahan_speed(kGameBase, 90) < kGameBase * 0.5f);
  CHECK(dullahan_speed(kGameBase, 30) > dullahan_speed(kGameBase, 60));
  CHECK(dullahan_speed(kGameBase, 0) < brownie_speed(kGameBase, false));          // the user's ceiling (spec)
}

void the_draugr_crawls_far_from_its_room() {
  CHECK(std::fabs(draugr_speed(0) - 2.8f) < 1e-5f);
  CHECK(std::fabs(draugr_speed(10) - 1.3f) < 1e-4f);      // 0.15 m/s per metre
  CHECK(std::fabs(draugr_speed(17) - 0.3f) < 1e-4f && draugr_speed(40) == 0.3f);
  CHECK(draugr_speed(40) < 0.4f);                          // below the Deogen's crawl
  CHECK(50 - kDraugrAngeredBonus == 35);                   // kept from the old test: hunts from 65% sanity
}

// The Draugr's wander picks run with its behaviour type reading as the Goryo's (goryo_way.h), so the game's
// picker skips its long-roam roll as for a Goryo; its own type is back after the pick. Without its
// GhostInfo, or without the Goryo's number, the pick runs untouched. On the fake: GhostAI.info at 0x38,
// the behaviour type at 0x2C, the Goryo 14, None 24.
namespace goryo_fake {
alignas(8) unsigned char ghost[0x100], info[0x100];
int32_t type_now() {
  int32_t t = 0;
  std::memcpy(&t, info + 0x2C, 4);
  return t;
}
void start(bool goryoResolves) {
  pm::fake::reset();
  pm::fake::field(pm::map::GhostAI_info, 0x38);
  pm::fake::field(pm::map::GhostInfo_behaviourType, 0x2C);
  if (goryoResolves) pm::fake::value(pm::map::GhostType_Goryo, 14);
  std::string missing;
  pm::api::bind(pm::fake::host(), missing);
  std::memset(ghost, 0, sizeof ghost);
  std::memset(info, 0, sizeof info);
  void* const at = info;
  std::memcpy(ghost + 0x38, &at, sizeof at);
  const int32_t none = 24;
  std::memcpy(info + 0x2C, &none, 4);
}
}  // namespace goryo_fake

void draugr_skips_long_roam_like_goryo() {
  using namespace goryo_fake;
  start(true);
  int32_t read = -1;
  CHECK(goryo_way::pick_as_goryo(ghost, [&] {
    read = type_now();  // what the game's picker reads
    return true;
  }));
  CHECK(read == 14 && type_now() == 24);
  unsigned char lone[0x100] = {};  // a ghost without its GhostInfo
  CHECK(!goryo_way::pick_as_goryo(lone, [&] {
    read = type_now();
    return false;
  }));
  CHECK(read == 24);
  start(false);  // no Goryo in this build: the pick runs as the game's own
  goryo_way::pick_as_goryo(ghost, [&] {
    read = type_now();
    return true;
  });
  CHECK(read == 24 && type_now() == 24);
  pm::fake::reset();
}

// A picker that throws (a managed exception goes on to the game) still finds the Draugr's own type put back.
void draugr_goryo_type_restored_when_picker_throws() {
  using namespace goryo_fake;
  start(true);
  bool threw = false;
  try {
    goryo_way::pick_as_goryo(ghost, []() -> bool { throw std::runtime_error("the game's picker threw"); });
  } catch (const std::runtime_error&) {
    threw = true;
  }
  CHECK(threw && type_now() == 24);
  pm::fake::reset();
}

// The Draugr no longer refuses room changes: the ChangeFavouriteRoomTo hook is gone from the mod, so a
// Monkey Paw's wish moves its room as it moves a Goryo's, and it keeps its room through the picker hook.
void draugr_room_change_hook_gone() {
  const auto source = [](const char* name) {
    std::ifstream in(std::string(PM_SOURCE_DIR) + "/mods/custom_ghosts/" + name, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  };
  const std::string behaviour = source("behaviour.cpp");
  CHECK(!behaviour.empty());
  CHECK(behaviour.find("ChangeFavouriteRoomTo") == std::string::npos);
  CHECK(behaviour.find("hook<phasmo::GhostWanderState::PickDestination>") != std::string::npos);
  for (const char* name : {"mod.cpp", "kikimora.cpp", "restraint.cpp", "goryo_way.h"})
    CHECK(source(name).find("ChangeFavouriteRoomTo") == std::string::npos);
}

// The Draugr takes back the thing out of its room longest that nobody carries, to the very place it lay
// when its rules started; something gone from the world is not missed.
void draugr_returns_prop_to_original_place() {
  using namespace draugr;
  std::vector<Thing<>> things(3);
  things[0].home = {1, 0, 2};
  things[1].home = {4, 1, 5};
  things[2].home = {7, 0, 8};
  int player = 0;
  std::vector<Seen> seen(3);
  CHECK(!survey(things, seen, 1000));  // everything lies in its room
  CHECK(to_take_back(things, seen) == -1);
  seen[1] = {true, true, &player};  // carried out
  CHECK(survey(things, seen, 2000));
  seen[2] = {true, true, nullptr};  // thrown out after it
  CHECK(survey(things, seen, 3000));
  seen[1].holder = nullptr;  // dropped outside
  const int first = to_take_back(things, seen);
  CHECK(first == 1);  // out longest
  CHECK(things[1].home.x == 4 && things[1].home.y == 1 && things[1].home.z == 5);
  seen[1].out = false;  // back where it lay
  CHECK(survey(things, seen, 4000));  // still angered: the other is out
  CHECK(to_take_back(things, seen) == 2);
  seen[2].out = false;
  CHECK(!survey(things, seen, 5000));
  seen[0] = {false, true, nullptr};  // gone from the world
  CHECK(!survey(things, seen, 6000) && to_take_back(things, seen) == -1);
}

// It never takes a thing a player carries, in a hand or put away, until they let it go.
void draugr_never_takes_a_held_prop() {
  using namespace draugr;
  std::vector<Thing<>> things(2);
  int a = 0, b = 0;
  std::vector<Seen> seen = {{true, true, &a}, {true, true, &b}};
  CHECK(survey(things, seen, 1000));
  CHECK(to_take_back(things, seen) == -1);
  seen[0].holder = &b;  // handed over outside the room: still carried
  CHECK(to_take_back(things, seen) == -1);
  seen[1].holder = nullptr;
  CHECK(to_take_back(things, seen) == 1);
}

// A hunt that starts while it is angered goes only for the most recent thief of a thing still out whom the
// test it is given lets it go for.
void draugr_hunts_most_recent_thief() {
  using namespace draugr;
  std::vector<Thing<>> things(3);
  int a = 0, b = 0;
  std::vector<Seen> seen(3);
  seen[0] = {true, true, &a};
  survey(things, seen, 1000);
  seen[1] = {true, true, &b};
  survey(things, seen, 2000);
  seen[2] = {true, true, nullptr};  // thrown out last: no thief
  survey(things, seen, 3000);
  seen[0].holder = seen[1].holder = nullptr;  // they put them down: still the thieves
  survey(things, seen, 4000);
  const auto anyone = [](void*) { return true; };
  CHECK(thief_to_hunt(things, anyone) == &b);
  CHECK(thief_to_hunt(things, [&](void* p) { return p != &b; }) == &a);  // b is in the truck
  CHECK(things[0].takenAt == 1000 && things[1].takenAt == 2000);       // when each left, not when seen since
}

// It names nobody with no thief, none the hunt may go for, or the thing back: the hunt is then unrestricted.
void draugr_names_no_thief_without_one() {
  using namespace draugr;
  std::vector<Thing<>> things(2);
  int a = 0;
  std::vector<Seen> seen = {{true, true, nullptr}, {true, false, nullptr}};
  survey(things, seen, 1000);
  const auto anyone = [](void*) { return true; };
  CHECK(!thief_to_hunt(things, anyone));  // thrown out: nobody carried it
  seen[1] = {true, true, &a};
  survey(things, seen, 2000);
  CHECK(!thief_to_hunt(things, [](void*) { return false; }));  // dead, in the truck or already killed
  seen[1].out = false;
  survey(things, seen, 3000);
  CHECK(!thief_to_hunt(things, anyone) && !things[1].thief);  // taken back: forgiven
  CHECK(!thief_to_hunt(std::vector<Thing<>>{}, anyone));
}

// While it is angered its hunts, its AI's and a player's alike, are held to its thief as the Banshee's to its
// target: the most recent thief of a thing still out who is alive.
void draugr_target_is_thief_while_angered() {
  using namespace draugr;
  std::vector<Thing<>> things(2);
  int a = 0, b = 0;
  std::vector<Seen> seen = {{true, true, &a}, {true, false, nullptr}};
  const bool angered = survey(things, seen, 1000);
  const auto living = [](void*) { return true; };
  CHECK(angered && target(things, angered, living) == &a);
  seen[1] = {true, true, &b};
  CHECK(survey(things, seen, 2000) && target(things, true, living) == &b);  // the most recent
  CHECK(target(things, true, [&](void* p) { return p != &b; }) == &a);      // b is dead
  // Where the game re-targets away from b (a kill, the Hanged Man card), b still alive: the next most
  // recent thief; with only b, nobody.
  const void* const onlyBOut[] = {&b};
  const void* const bothOut[] = {&b, &a};  // the dying player and the current target
  CHECK(target(things, true, living, onlyBOut) == &a);
  CHECK(!target(things, true, living, bothOut));
  std::vector<Thing<>> one(1);
  std::vector<Seen> onlyB = {{true, true, &b}};
  CHECK(survey(one, onlyB, 1000) && !target(one, true, living, onlyBOut));
}

// Calm, it has no target: its hunt goes for anyone the game lets it.
void draugr_target_null_when_calm() {
  using namespace draugr;
  std::vector<Thing<>> things(1);
  int a = 0;
  std::vector<Seen> seen = {{true, true, &a}};
  survey(things, seen, 1000);
  const auto living = [](void*) { return true; };
  CHECK(!target(things, false, living));
  seen[0].out = false;  // back in its room
  CHECK(!survey(things, seen, 2000) && !target(things, false, living) && !target(things, true, living));
  std::vector<Thing<>> thrown(1);  // out, but nobody carried it
  std::vector<Seen> loose = {{true, true, nullptr}};
  CHECK(survey(thrown, loose, 1000) && !target(thrown, true, living));
}

// When a Monkey Paw's wish moves its favourite room, the things of the old room are no longer its: they
// are forgotten and the new room is surveyed for its things.
void draugr_resurveys_after_room_change() {
  using namespace draugr;
  int roomA = 0, roomB = 0, a = 0;
  Hoard<> hoard;
  CHECK(hoard.follow(&roomA));  // its first room: survey it
  hoard.things.resize(2);
  CHECK(!hoard.follow(&roomA));  // the same room: nothing to do
  std::vector<Seen> seen = {{true, true, &a}, {true, false, nullptr}};
  hoard.angered = survey(hoard.things, seen, 1000);
  CHECK(hoard.angered && hoard.things[0].thief == &a);
  CHECK(hoard.follow(&roomB));  // the wish moved it
  CHECK(hoard.things.empty() && hoard.room == &roomB);
  hoard.things.resize(1);  // what lies in the new room
  CHECK(!hoard.follow(&roomB));
}

// After the move it is calm: no anger, no thief to hunt and no target, until something leaves the new room.
void draugr_calm_after_room_change() {
  using namespace draugr;
  int roomA = 0, roomB = 0, a = 0;
  Hoard<> hoard;
  hoard.follow(&roomA);
  hoard.things.resize(1);
  std::vector<Seen> seen = {{true, true, &a}};
  hoard.angered = survey(hoard.things, seen, 1000);
  const auto anyone = [](void*) { return true; };
  CHECK(target(hoard.things, hoard.angered, anyone) == &a);
  hoard.follow(&roomB);
  hoard.things.resize(1);  // the new room's thing, lying in it
  std::vector<Seen> now = {{true, false, nullptr}};
  hoard.angered = survey(hoard.things, now, 2000);
  CHECK(!hoard.angered && !target(hoard.things, hoard.angered, anyone) && !thief_to_hunt(hoard.things, anyone));
}

// A thing carried out and set down between two surveys still names its thief: its carrier is noted every
// frame while it lies in the room, and a thing lying there uncarried at a survey forgets it.
void draugr_thief_found_when_dropped_between_surveys() {
  using namespace draugr;
  std::vector<Thing<>> things(2);
  int a = 0, b = 0;
  std::vector<Seen> seen(2);
  survey(things, seen, 1000);
  note_carriers(things, {&a, nullptr});  // a frame: a picks the first up in the room
  seen[0] = {true, true, nullptr};       // next survey: it lies outside, nobody carries it
  CHECK(survey(things, seen, 2000) && things[0].thief == &a);
  note_carriers(things, {nullptr, &b});  // b picks the second up and puts it back down in the room
  survey(things, seen, 3000);            // it lies in the room uncarried: b is forgotten
  CHECK(!things[1].lastCarrier);
  seen[1] = {true, true, nullptr};       // later thrown out by nobody
  survey(things, seen, 4000);
  CHECK(!things[1].thief);
}

// A client-controlled Draugr's HUD shows its anger from the host's word: the host answers from its own things,
// another client from that message.
void draugr_hud_anger_on_client_from_anger_message() {
  using namespace draugr;
  bool angered = false;
  const std::vector<uint8_t> told = net_msg::anger(true);
  CHECK(net_msg::read_anger(told.data(), told.size(), angered));
  CHECK(hud_anger(false, -1, angered) == 1);  // a client: from the message
  CHECK(hud_line(kDraugr, hud_anger(false, -1, angered)).find("65% sanity") != std::string::npos);
  const std::vector<uint8_t> calm = net_msg::anger(false);
  CHECK(net_msg::read_anger(calm.data(), calm.size(), angered));
  CHECK(hud_anger(false, -1, angered) == 0 && hud_line(kDraugr, 0) == "Your room is whole");
  CHECK(hud_anger(true, 1, false) == 1 && hud_anger(true, 0, true) == 0);  // the host: its own things
  CHECK(hud_anger(true, -1, true) == -1);  // the host runs no Draugr: no state
  // A dormant Echo player may still try: most of it is lost, and the HUD says so.
  CHECK(hud_line(kEcho, 1).find("most of what you try is lost") != std::string::npos);
  CHECK(hud_line(kEcho, 1).find("cannot act") == std::string::npos);
}

// Each custom ghost's HUD line for the state its hudState gives on the host (hud_line.h).
void custom_ghost_hud_lines() {
  CHECK(hud_line(kEcho, 0) == "Voices: you hear the living near you");
  CHECK(hud_line(kEcho, 1).find("dormant") != std::string::npos);
  CHECK(hud_line(kDullahan, 140) == "Charge: 140% speed");
  CHECK(hud_line(kDullahan, 0).empty());  // not hunting
  CHECK(hud_line(kKikimora, 0).empty());
  CHECK(hud_line(kKikimora, 1).find("lights are on") != std::string::npos);
  CHECK(hud_line(kDraugr, 0) == "Your room is whole");
  CHECK(hud_line(kDraugr, 1).find("65% sanity") != std::string::npos);
  CHECK(hud_line(kEcho, -1).empty() && hud_line(kDraugr, -1).empty());  // no state
  CHECK(hud_line(kHydra, 1).empty());  // a ghost with nothing to show
}

// While angered its room shows kCold less than the game's own temperature system makes it, and that
// system keeps running on the room's own value: each update starts from it, and a value the game writes
// between updates becomes its own.
void draugr_room_colder_while_angered() {
  using namespace draugr;
  const auto near = [](float a, float b) { return std::fabs(a - b) < 1e-4f; };
  RoomCold cold;
  float room = 12.0f;
  room = cold.before_update(room);
  CHECK(room == 12.0f);
  room -= 0.1f;  // the game's update: the ghost is in the room
  room = cold.after_update(room, true);
  CHECK(cold.on && near(room, 8.9f));
  room = cold.before_update(room);
  CHECK(near(room, 11.9f));  // the game's next update starts from its own value
  room -= 0.1f;
  room = cold.after_update(room, true);
  CHECK(near(room, 8.8f));
  CHECK(cold.before_update(15.0f) == 15.0f);  // ResetTemperature wrote it: its own now
}

// The cold is a tell, not evidence (user, 2026-10-30). Without Freezing in the given evidence the game's
// update keeps a room at its floor, 1, or above, and the thermometer (0x7C6560 in build 25811160) reads the
// room plus Random.Range(0, 3), or (0, 2), giving photo evidence of freezing below 1. The Draugr's room
// still goes 3 below its own, past that floor, so its readings may be freezing even when Freezing is
// withheld, as the Mimic's orbs show on any evidence.
void draugr_cold_goes_below_freezing_when_withheld() {
  using namespace draugr;
  constexpr float kFloorWithheld = 1.0f, kFreezingBelow = 1.0f;
  RoomCold cold;
  const float shown = cold.after_update(kFloorWithheld, true);  // its own at the game's floor
  CHECK(shown == -2.0f && shown < kFreezingBelow);
  CHECK(shown + 2.9f < kFreezingBelow);  // even a high reading is freezing
  CHECK(cold.after_update(2.0f, true) == -1.0f);
  CHECK(cold.after_update(-10.0f, true) == -13.0f);  // and with Freezing given, colder than the game's floor
}

// When the anger ends the room goes back to its own temperature at once, and the game's update runs on it
// as on any room; a value the game wrote meanwhile stands.
void draugr_cold_ends_with_anger() {
  using namespace draugr;
  RoomCold cold;
  float room = cold.after_update(10.0f, true);
  CHECK(room == 7.0f);
  CHECK(cold.end(room) == 10.0f && !cold.on);
  CHECK(cold.before_update(10.0f) == 10.0f);
  cold.after_update(10.0f, true);
  CHECK(cold.end(12.0f) == 12.0f);
  CHECK(cold.after_update(9.5f, false) == 9.5f && !cold.on);  // not angered: its own
}

void the_shuck_follows_the_trail_and_salt_breaks_it() {
  const trail::Breaks none;
  trail::Trail t;
  for (int i = 0; i <= 10; ++i) t.add({static_cast<float>(i), 0, 0}, 1000u * i, none, false);
  trail::Point next;
  CHECK(trail::next_step({&t}, {0.2f, 0, 0.1f}, none, next));
  CHECK(next.x == 2.0f);                                     // steps within reach count as reached
  CHECK(trail::next_step({&t}, {5.0f, 0, 3.0f}, none, next) && next.x == 5.0f);  // first to the trail itself
  CHECK(!trail::next_step({&t}, {50, 0, 50}, none, next));         // too far to pick up a scent
  t.forget_older_than(3000 + trail::kTrailMs);               // the first three steps have gone cold
  CHECK(t.steps().front().at.x == 3.0f);
  t.add({12, 0, 0}, 12000, trail::Breaks{{{11, 0, 0.3f}}, {}}, false);                 // walked over salt
  CHECK(t.steps().size() == 1 && t.steps().front().at.x == 12.0f);
  trail::Trail fresh;
  fresh.add({0.5f, 0, 0}, 20000, none, false);
  trail::Trail old;
  old.add({0.5f, 0, 0}, 1000, none, false);
  old.add({-5, 0, 0}, 2000, none, false);
  CHECK(trail::next_step({&old, &fresh}, {0.5f, 0, 0}, none, next) && next.x == 0.5f);  // the fresher scent wins a tie
}

void water_and_rain_break_scent() {
  trail::Breaks none, tap{{}, {{5, 0, 0}}};
  trail::Trail t;
  for (int i = 0; i <= 3; ++i) t.add({float(i), 0, 0}, 1000u * i, none, false);
  t.add({4, 0, 0}, 4000, tap, false);                       // within 2 m of running water: broken, not laid
  CHECK(t.steps().empty());
  t.add({8, 0, 0}, 8000, tap, false);
  CHECK(t.steps().size() == 1);
  t.add({9, 0, 0}, 9000, none, true);                       // outdoors in rain: broken, not laid
  CHECK(t.steps().empty());
  trail::Trail w;
  for (int i = 0; i <= 6; ++i) w.add({float(i) * 2, 0, 0}, 1000u * i, none, false);
  w.wash({{4.5f, 0, 0}});                                   // a tap turned on beside the trail
  CHECK(w.steps().front().at.x == 8.0f);                    // only steps after the freshest washed one remain
}

void scent_does_not_cross_a_break() {
  trail::Breaks none, salt{{{2, 0, 0}}, {}};
  CHECK(!trail::scent_reaches({0, 0, 0}, {4, 0, 0}, salt));
  CHECK(trail::scent_reaches({0, 0, 1}, {0, 0, 4}, salt));
  trail::Trail prey;
  prey.add({4, 0, 0}, 1000, none, false);
  prey.add({5, 0, 0}, 2000, none, false);
  trail::Point next;
  CHECK(!trail::follow(prey, {0, 0, 0}, salt, next));      // salt between hound and prey: lost
  CHECK(trail::follow(prey, {0, 0, 0}, none, next) && next.x == 4.0f);
  CHECK(!trail::follow(trail::Trail{}, {0, 0, 0}, none, next));  // a prey with no trail (dead, gone)
}

void salt_or_water_laid_across_a_trail_stops_the_hound() {
  trail::Breaks none, salt{{{5.2f, 0, 0}}, {}}, water{{}, {{5.2f, 0, 1.9f}}};
  trail::Trail dense;
  for (int i = 0; i <= 12; ++i) dense.add({0.8f * i, 0, 0}, 500u * i, none, false);
  trail::Point next;
  // The hound stands on the last step the scent still reaches (4.0 m), within kArrived of it.
  CHECK(!trail::follow(dense, {4.0f, 0, 0}, salt, next));
  CHECK(!trail::follow(dense, {4.0f, 0, 0}, water, next));
  CHECK(trail::follow(dense, {4.0f, 0, 0}, none, next));
  for (const trail::Breaks* b : {&salt, &water})
    if (trail::next_step({&dense}, {4.0f, 0, 0}, *b, next)) CHECK(next.x <= 4.0f);  // never beyond the line
  trail::Trail other;
  other.add({0, 0, 2}, 1000, none, false);
  other.add({0, 0, 3}, 2000, none, false);
  trail::Trail blocked;
  blocked.add({4, 0, 0}, 1000, none, false);
  trail::Breaks wall{{{2, 0, 0}}, {}};
  CHECK(trail::next_step({&blocked, &other}, {0, 0, 0}, wall, next) && next.z == 2.0f);  // the reachable trail
}

void the_hound_picks_up_a_trail_it_can_follow() {
  const trail::Breaks none, salt{{{3.5f, 0, 1}}, {}};  // salt across the nearer trail, ahead of the hound
  trail::Trail near, far;
  for (int i = 0; i <= 6; ++i) near.add({1.0f * i, 0, 1}, 500u * i, none, false);
  for (int i = 0; i <= 6; ++i) far.add({1.0f * i, 0, -3}, 500u * i, none, false);
  CHECK(trail::pick_up({&near, &far}, {0, 0, 0}, none) == 0);    // the nearest trail
  CHECK(trail::pick_up({&near, &far}, {0, 0, 0}, salt) == 1);    // the nearer one is cut: the one it can follow
  CHECK(trail::pick_up({&near, &far}, {50, 0, 50}, none) == -1); // nothing within reach
}

void the_shuck_waits_out_a_smudge() {
  CHECK(shuck_move(true, true, true, false, true) == ShuckMove::Native);   // a smudge wins
  CHECK(shuck_move(false, true, true, false, false) == ShuckMove::Follow);
  CHECK(shuck_move(false, true, true, true, false) == ShuckMove::Search);  // its trail led to their closet
  CHECK(shuck_move(false, true, false, false, true) == ShuckMove::Sniff);  // salt or water lost them
  CHECK(shuck_move(false, false, false, false, false) == ShuckMove::Native);
  CHECK(shuck_move(false, false, false, false, true) == ShuckMove::Sniff);   // no prey: it casts about
}

void the_shuck_keeps_only_the_player_it_follows() {
  int a = 0, b = 0;
  void* prey = &a;
  void* other = &b;
  CHECK(shuck_target(ShuckMove::Follow, prey, prey) == prey);
  CHECK(shuck_target(ShuckMove::Follow, prey, other) == nullptr);    // anyone else in sight or earshot
  CHECK(shuck_target(ShuckMove::Follow, nullptr, other) == nullptr);
  CHECK(shuck_target(ShuckMove::Search, prey, prey) == nullptr);    // the game's closet search finds them unseen
  CHECK(shuck_target(ShuckMove::Sniff, prey, other) == nullptr);
  CHECK(shuck_target(ShuckMove::Native, prey, prey) == nullptr);
}

// 48 kHz stereo blocks of 20 ms, as Vivox delivers them: speech is a loud tone, quiet is near silence.
void feed(voice::Recorder& r, int milliseconds, bool speech) {
  std::vector<float> block(960 * 2);
  for (int done = 0; done < milliseconds; done += 20) {
    for (int i = 0; i < 960; ++i) {
      const float s = speech ? 0.3f * std::sin(static_cast<float>(i) * 0.2f) : 0.001f;
      block[2 * i] = block[2 * i + 1] = s;
    }
    r.push(block.data(), 960, 2, 48000);
  }
}

void the_echo_remembers_the_last_phrase() {
  voice::Recorder r;
  std::vector<float> phrase;
  CHECK(!r.last_phrase(phrase));       // nothing said
  feed(r, 1000, false);
  feed(r, 1200, true);
  CHECK(r.spoke_within(5));
  CHECK(!r.last_phrase(phrase));       // still talking: not finished yet
  feed(r, 500, false);
  CHECK(r.last_phrase(phrase));
  CHECK(phrase.size() >= 1100 * voice::kRate / 1000 && phrase.size() <= 1300 * voice::kRate / 1000);  // ~1.2 s at 8 kHz
  feed(r, 200, true);                  // a cough: too short to be a phrase of its own
  feed(r, 500, false);
  CHECK(r.last_phrase(phrase) == false);
  feed(r, 5000, true);                 // a long speech: only its last 3 s are kept
  feed(r, 500, false);
  CHECK(r.last_phrase(phrase) && phrase.size() == static_cast<size_t>(voice::kMaxPhraseFrames * voice::kFrame));
  feed(r, (voice::kKeepSeconds + 1) * 1000, false);  // over a minute of quiet: forgotten
  CHECK(!r.spoke_within(voice::kKeepSeconds * 50) && !r.last_phrase(phrase));
}

void voices_survive_the_trip() {
  for (float s : {0.0f, 0.5f, -0.5f, 0.99f, -0.01f, 0.25f}) CHECK(std::fabs(voice::decode(voice::encode(s)) - s) < 0.02f + std::fabs(s) * 0.07f);
  std::vector<float> clip(1000);
  for (size_t i = 0; i < clip.size(); ++i) clip[i] = 0.4f * std::sin(static_cast<float>(i) * 0.05f);
  auto parts = voice::chunks(clip);
  CHECK(parts.size() == 3 && parts[0].size() == voice::kChunkBytes && parts[2].size() == 40);
  voice::Assembler a;
  std::vector<float> out;
  CHECK(!a.add(7, 2, 3, parts[2].data(), parts[2].size(), out));  // out of order is fine
  CHECK(!a.add(7, 0, 3, parts[0].data(), parts[0].size(), out));
  CHECK(!a.add(7, 0, 3, parts[0].data(), parts[0].size(), out));  // a repeat changes nothing
  CHECK(a.add(7, 1, 3, parts[1].data(), parts[1].size(), out));
  CHECK(out.size() == clip.size() && std::fabs(out[500] - clip[500]) < 0.03f);
  CHECK(!a.add(8, 5, 3, parts[0].data(), parts[0].size(), out));  // a bad index is ignored
}

// The game's default branch with its multipliers, as RVA 0x2153C20 computes it (test emulation).
float game_hunt_speed(OwnSpeed s, float sight, float bonus, float multiplier) {
  return (s.base * (s.keepSight ? sight : 1.0f) + bonus) * multiplier;
}

void a_rule_that_stops_writes_the_games_own_default() {
  const OwnSpeed batter = scaled_speed(kGameBase, 0), roam = speed_when(false, batter, kGameBase);
  CHECK(speed_when(true, batter, kGameBase).base == 0);
  CHECK(roam.base == kGameBase && roam.keepSight);  // base 1.7 with the sight speed-up: a vanilla ghost's
}

void own_speed_goes_through_the_games_multipliers() {
  CHECK(flat_speed(3.0f).base == 3.0f && !flat_speed(3.0f).keepSight);
  CHECK(std::fabs(scaled_speed(kGameBase, 0.8f).base - 1.36f) < 1e-5f && scaled_speed(kGameBase, 0.8f).keepSight);
  CHECK(std::fabs(game_hunt_speed(flat_speed(3.0f), kGameSightMax, 0, 1.5f) - 4.5f) < 1e-4f);  // 150%: once, no sight
  CHECK(std::fabs(game_hunt_speed(scaled_speed(kGameBase, 0), kGameSightMax, 0, 1.0f)) < 1e-6f);  // the Asterion stops
}

// The rule's answer to the host (PmGhostRules.huntSpeed): its base, and the sight speed-up held at 1
// only where the rule has none; a kept speed-up stays as the game grew it.
void a_rule_gives_the_host_its_speed_inputs() {
  float base = kGameBase, sight = 1.4f;
  give_speed(flat_speed(3.0f), &base, &sight);
  CHECK(base == 3.0f && sight == 1.0f);
  base = kGameBase;
  sight = 1.4f;
  give_speed(scaled_speed(kGameBase, 0.8f), &base, &sight);
  CHECK(std::fabs(base - 1.36f) < 1e-5f && sight == 1.4f);
}

void the_echos_phrase_comes_from_a_client_that_holds_it() {
  Holders h;
  auto everyone = [](int) { return true; };
  CHECK(h.holder_of(2, 1, 1000, 5000, everyone) == 0);  // nobody reported it
  h.report(3, 2, 1000);
  h.report(4, 2, 1000);
  CHECK(h.holder_of(2, 1, 2000, 5000, everyone) == 3);
  h.report(9, 2, 1500);
  CHECK(h.holder_of(2, 9, 2000, 5000, everyone) == 9);  // the host itself first, whatever its number
  CHECK(h.holder_of(2, 1, 6600, 5000, everyone) == 0);  // every report has gone stale (the host's at 1500 too)
  h.report(2, 2, 7000);
  CHECK(h.holder_of(2, 1, 7000, 5000, everyone) == 0);  // nobody sends their own voice
  h.report(3, 5, 7000);
  CHECK(h.holder_of(5, 1, 7000, 5000, [](int c) { return c != 3; }) == 0);  // a client without the mod
  h.clear();
  CHECK(h.holder_of(5, 1, 7000, 5000, everyone) == 0);
}

void the_echo_repeats_what_players_do() {
  int a, b;
  Repeat r;
  r.used(&a, false, 0);
  CHECK(r.take(1000) == nullptr);                 // outside its room: ignored
  r.used(&a, true, 0);
  r.used(&b, true, 5000);
  CHECK(r.take(6000) == &b);                      // the most recent use
  CHECK(r.take(7000) == nullptr);                 // once
  r.used(&a, true, 10000);
  CHECK(r.take(10000 + kEchoRepeatMs + 1) == nullptr);  // too late
}

void a_repeat_waits_for_the_echos_next_interaction() {
  int a;
  Repeat r;
  CHECK(!r.pending(0));
  r.used(&a, false, 0);
  CHECK(!r.pending(0));                           // outside its room: nothing waits
  r.used(&a, true, 1000);
  CHECK(r.pending(1000 + kEchoRepeatMs) == &a);   // still within the minute
  CHECK(r.pending(2000) == &a);                   // looking does not take it
  CHECK(!r.pending(1000 + kEchoRepeatMs + 1));    // too late
  CHECK(r.take(2000) == &a && !r.pending(2000));  // taken: nothing waits any more
}

// A ghost player's aimed interaction on the Echo tries the same repeat its AI's roll does: when players have
// just used a switch and then a door in its room, the next interaction there, theirs or its AI's, is that door,
// once; a forced pulse (the voodoo doll's) never repeats, and nothing does off the host or for another ghost.
void player_echo_repeats_last_used() {
  CHECK(echo_tries_repeat(true, true, false));    // a player's aimed interaction, as its AI's roll
  CHECK(!echo_tries_repeat(true, true, true));    // the voodoo doll's forced pulse
  CHECK(!echo_tries_repeat(true, false, false));  // off the host
  CHECK(!echo_tries_repeat(false, true, false));  // another ghost
  int lightSwitch, door, elsewhere;
  Repeat r;  // one repeat, which its AI's roll and a player's aimed interaction take from alike
  r.used(&lightSwitch, true, 1000);
  r.used(&door, true, 2000);
  r.used(&elsewhere, false, 2500);  // a use outside its room changes nothing
  CHECK(r.take(3000) == &door);     // the player's next interaction: the last one used in its room
  CHECK(!r.take(3500));             // once: the next one, its AI's or theirs, is its own again
}

void a_use_outside_its_room_leaves_the_last_one_in_it() {
  int a, b;
  Repeat r;
  r.used(&a, true, 0);
  r.used(&b, false, 1000);                        // only uses in its room count
  CHECK(r.take(2000) == &a);
}

void a_repeat_replaces_the_dormancy_roll() {
  int rolls = 0;
  auto roll = [&rolls](int value) {
    return [&rolls, value] {
      ++rolls;
      return value;
    };
  };
  CHECK(echo_roll(true, true, roll(0)) == EchoRoll::Repeat && rolls == 0);  // the repeat is the interaction: no roll
  CHECK(echo_roll(false, true, roll(kEchoDormantLoss - 1)) == EchoRoll::Lost && rolls == 1);
  CHECK(echo_roll(false, true, roll(kEchoDormantLoss)) == EchoRoll::Game && rolls == 2);  // 3 in 4 lost, 1 kept
  CHECK(echo_roll(false, false, roll(0)) == EchoRoll::Game && rolls == 2);  // awake: the game's own, no roll
  CHECK(kEchoDormantLoss == 75);
}

void a_trail_no_longer_recorded_goes_cold() {
  trail::Trail t;
  const trail::Breaks none;
  t.add({0, 0, 0}, 1000, none, false);
  t.add({1, 0, 0}, 1500, none, false);
  t.forget_older_than(1500 + trail::kTrailMs + 1);  // the investigator died: nothing new is added
  CHECK(t.steps().empty());
  trail::Point next;
  CHECK(!trail::next_step({&t}, {0, 0, 0}, none, next));
}

// A player's walk along +x from `from` to `to` metres, a step every 0.75 m, laid as the hound's trail is.
trail::Trail walked(float from, float to, const trail::Breaks& breaks = {}) {
  trail::Trail t;
  uint64_t now = 1000;
  for (float x = from; x <= to + 1e-3f; x += 0.75f, now += 500) t.add({x, 0, 0}, now, breaks, false);
  return t;
}

// The way its agent walks with nothing in its path: straight there.
trail::Way open_floor(trail::Point to) { return {true, to, true}; }

// What a player steering the hunting Black Shuck from `at` in (dx, dz) is left to steer in.
std::pair<float, float> steered(const trail::Trail& t, trail::Point at, float dx, float dz,
                                const trail::Breaks& breaks = {}, bool hunting = true) {
  trail::confine(hunting, {&t}, at, breaks, dx, dz, open_floor);
  return {dx, dz};
}

// In a hunt a player steers the Black Shuck freely within 1 m of a scent trail: along it and across it within
// the corridor, sliding along it at the edge, and outside it only towards it.
void player_shuck_confined_to_trail_corridor() {
  const trail::Trail t = walked(0, 6);
  CHECK(steered(t, {3, 0, 0.4f}, 1, 0) == std::make_pair(1.0f, 0.0f));    // along the trail
  CHECK(steered(t, {3, 0, -0.2f}, 0, 1) == std::make_pair(0.0f, 1.0f));   // across it, within the corridor
  CHECK(steered(t, {3, 0, 0.8f}, 0, 1) == std::make_pair(0.0f, 0.0f));    // out of the corridor: it stops
  const auto [sx, sz] = steered(t, {3, 0, 0.8f}, 0.6f, 0.8f);              // at the edge, partly along it
  CHECK(std::fabs(sx - 0.6f) < 1e-5f && sz == 0.0f);                       // slides along the trail
  CHECK(steered(t, {7.2f, 0, 0}, 1, 0) == std::make_pair(0.0f, 0.0f));    // past the trail's end
  CHECK(steered(t, {3, 0, 3}, 0, 1) == std::make_pair(0.0f, 0.0f));       // outside it, away: held
  CHECK(steered(t, {3, 0, 3}, 0, -1) == std::make_pair(0.0f, -1.0f));     // outside it, towards it: kept
  CHECK(trail::corridor_distance({&t}, {3, 0, 1.0f}, {}) <= trail::kCorridor && trail::kCorridor == 1.0f);
  // Round a bend the corridor keeps its width on the outside of the corner.
  trail::Trail bend;
  uint64_t now = 1000;
  for (const trail::Point p : {trail::Point{0, 0, 0}, {1.5f, 0, 0}, {3, 0, 0}, {3, 0, 1.5f}, {3, 0, 3}})
    bend.add(p, now += 500, {}, false);
  CHECK(trail::corridor_distance({&bend}, {3.7f, 0, -0.5f}, {}) <= trail::kCorridor);
  CHECK(trail::corridor_distance({&bend}, {3.0f, 0, 4.5f}, {}) > trail::kCorridor);  // its end is flat
  // With no scent in reach to pick up, as its AI then roams by the game's own hunt, it steers freely.
  CHECK(steered(t, {30, 0, 30}, 0, 1) == std::make_pair(0.0f, 1.0f));
}

// Where salt, running water or rain breaks a trail, as it breaks the scent its AI follows, the corridor has a
// gap the player cannot cross, round the side or through it.
void player_shuck_blocked_at_salt_water_rain_gap() {
  {
    trail::Breaks salt;
    salt.salt.push_back({3.0f, 0, 0});  // poured across the trail after it was laid
    const trail::Trail t = walked(0, 6);
    CHECK(steered(t, {2.0f, 0, 0}, 1, 0, salt) == std::make_pair(0.0f, 0.0f));     // through it
    CHECK(steered(t, {2.0f, 0, 0.9f}, 1, 0, salt) == std::make_pair(0.0f, 0.0f));  // round the side
    CHECK(steered(t, {4.2f, 0, 0}, -1, 0, salt) == std::make_pair(0.0f, 0.0f));    // nor from the far side
    CHECK(steered(t, {1.0f, 0, 0}, 1, 0, salt) == std::make_pair(1.0f, 0.0f));     // short of the gap: free
  }
  {
    trail::Breaks water;
    water.water.push_back({4.5f, 0, 0});  // a running tap beside the trail
    const trail::Trail t = walked(0, 9);
    CHECK(steered(t, {2.0f, 0, 0}, 1, 0, water) == std::make_pair(0.0f, 0.0f));
    CHECK(steered(t, {7.0f, 0, 0}, -1, 0, water) == std::make_pair(0.0f, 0.0f));
  }
  {
    // Salt that only just reaches a slow walker's trail leaves a gap shorter than the step tested: round the
    // far side of the pile the step would land on the next run of the trail, and it may not.
    trail::Breaks salt;
    salt.salt.push_back({3.0f, 0, 0.59f});
    trail::Trail t;
    for (int i = 0; i <= 60; ++i) t.add({0.1f * static_cast<float>(i), 0, 0}, 1000 + 500 * static_cast<uint64_t>(i), {}, false);
    CHECK(steered(t, {2.7f, 0, -0.5f}, 1, 0, salt) == std::make_pair(0.0f, 0.0f));
  }
  {
    // Nor does a step walk into running water's reach from the edge of a corridor the water does not break.
    trail::Breaks water;
    water.water.push_back({3.0f, 0, 3.0f});
    const trail::Trail t = walked(0, 6);
    CHECK(steered(t, {3.0f, 0, 0.5f}, 0, 1, water) == std::make_pair(0.0f, 0.0f));
    CHECK(steered(t, {3.0f, 0, 0.5f}, 1, 0, water) == std::make_pair(1.0f, 0.0f));
  }
  {
    // Outdoors in rain no step is laid and the trail behind breaks: only the stretch walked after it is left.
    trail::Trail t;
    const trail::Breaks none;
    uint64_t now = 1000;
    for (float x = 0; x <= 3.01f; x += 0.75f, now += 500) t.add({x, 0, 0}, now, none, false);
    t.add({4.5f, 0, 0}, now += 500, none, true);  // out in the rain
    for (float x = 6; x <= 9.01f; x += 0.75f, now += 500) t.add({x, 0, 0}, now, none, false);
    CHECK(steered(t, {6.0f, 0, 0}, -1, 0) == std::make_pair(0.0f, 0.0f));  // back over the rained-out stretch
    CHECK(steered(t, {6.5f, 0, 0}, 1, 0) == std::make_pair(1.0f, 0.0f));   // on along what is left
    CHECK(trail::corridor_distance({&t}, {2.0f, 0, 0}, none) > trail::kCorridor);  // nothing left of before
  }
}

// Where its scent reaches no step its AI has no trail (next_step false) and roams by the game's own hunt, so a
// player is free too: with all scent in reach behind a break, or standing by running water or salt. Nothing stalls.
void player_shuck_free_when_no_scent_reaches_it() {
  const auto free_every_way = [](const trail::Trail& t, trail::Point at, const trail::Breaks& breaks) {
    trail::Point step;
    CHECK(!trail::next_step({&t}, at, breaks, step));  // what its AI goes by
    for (const auto& [dx, dz] : {std::pair{1.0f, 0.0f}, {-1.0f, 0.0f}, {0.0f, 1.0f}, {0.0f, -1.0f}})
      CHECK(steered(t, at, dx, dz, breaks) == std::make_pair(dx, dz));
  };
  {
    // Standing by running water that started beside it, in the trail's corridor.
    trail::Breaks water;
    water.water.push_back({4.5f, 0, 1.0f});
    free_every_way(walked(0, 9), {4.5f, 0, 0.2f}, water);
  }
  {
    // Out of the water's reach, and all the scent in reach lies behind it.
    trail::Breaks water;
    water.water.push_back({3.0f, 0, 0});
    free_every_way(walked(0, 3), {6.0f, 0, 0}, water);
  }
  {
    // Standing at a salt pile.
    trail::Breaks salt;
    salt.salt.push_back({3.0f, 0, 0.3f});
    free_every_way(walked(2.25f, 3.75f), {3.0f, 0, 0}, salt);
  }
  {
    // With no step in reach at all.
    free_every_way(walked(0, 3), {30.0f, 0, 30.0f}, {});
  }
  // A trail it ignores is no trail here: the same follows for what is left (tests/hunt_rules_test.cpp).
}

// Outside the corridor it may always walk towards the step its scent reaches: straight at it nothing can break.
void player_shuck_can_always_reach_the_scent_it_smells() {
  // The nearest stretch lies behind salt, and the only step it can smell is further off the other way.
  trail::Breaks salt;
  salt.salt.push_back({0, 0, 2.0f});
  trail::Trail near = walked(-1.5f, 1.5f);           // 3 m away, behind the salt
  trail::Trail t;
  uint64_t now = 1000;
  t.add({6.0f, 0, 3.0f}, now, {}, false);            // one lone step, 6 m off
  const std::vector<const trail::Trail*> both = {&near, &t};
  const trail::Point at{0, 0, 3.0f};
  float dx = -1, dz = 0;                             // away from all of it
  trail::confine(true, both, at, salt, dx, dz, open_floor);
  CHECK(dx == 0.0f && dz == 0.0f);
  dx = 1;
  dz = 0;                                            // towards the step its scent reaches
  trail::confine(true, both, at, salt, dx, dz, open_floor);
  CHECK(dx == 1.0f && dz == 0.0f);
  dx = 0;
  dz = -1;                                           // towards the nearer stretch: through the salt
  trail::confine(true, both, at, salt, dx, dz, open_floor);
  CHECK(dx == 0.0f && dz == 0.0f);
  // Having reached a lone step, all that is left of the scent, nothing holds it there.
  dx = 0;
  dz = 1;
  trail::confine(true, {&t}, {6.0f, 0, 3.0f}, {}, dx, dz, open_floor);
  CHECK(dx == 0.0f && dz == 1.0f);
}

// A wall between it and the trail, which scent reaches through as its AI's own test does, stops nobody: its AI's
// agent walks round by the door, and "nearer" is measured along that same way. A trail just beyond the wall is no
// corridor to hold it in, and with no complete path to ask about nothing holds it at all.
void player_shuck_not_stalled_by_wall() {
  const trail::Trail t = walked(0, 6);
  const auto confined = [&](trail::Point at, float dx, float dz, const trail::Way& way) {
    trail::confine(true, {&t}, at, {}, dx, dz, [&](trail::Point) { return way; });
    return std::make_pair(dx, dz);
  };
  // In a room beside the trail, 3 m off it, with the wall between and the door 5 m along.
  const trail::Point room{3, 0, 3};
  const trail::Way byTheDoor{true, {8, 0, 2}, false};
  CHECK(confined(room, 1, 0, byTheDoor) == std::make_pair(1.0f, 0.0f));    // along the wall to the door: on its way
  CHECK(confined(room, -1, 0, byTheDoor) == std::make_pair(0.0f, 0.0f));   // away from the door: held, though no nearer
                                                                           // or further by the straight line
  CHECK(confined(room, 0, 1, byTheDoor) == std::make_pair(0.0f, 0.0f));    // further off the trail, away from the door
  // Just beyond the wall from a trail, within a metre of it by the straight line: no corridor holds it there.
  const trail::Point beyond{3, 0, 0.8f};
  const trail::Way round{true, {-2, 0, 0.8f}, false};
  CHECK(confined(beyond, -1, 0, round) == std::make_pair(-1.0f, 0.0f));
  CHECK(confined(beyond, 0, 1, round) == std::make_pair(0.0f, 0.0f));      // not away from its way either
  // On open floor the corridor stands as ever.
  CHECK(confined(beyond, 0, 1, {true, {3, 0, 0}, true}) == std::make_pair(0.0f, 0.0f));
  // No complete path, or none could be asked for: outside the corridor it steers freely, whatever the direction.
  const trail::Way unknown{};
  CHECK(confined(room, -1, 0, unknown) == std::make_pair(-1.0f, 0.0f));
  CHECK(confined(room, 0, 1, unknown) == std::make_pair(0.0f, 1.0f));
  // In the corridor by the straight line the gaps and edges still hold, so salt is no way out.
  CHECK(confined(beyond, 0, 1, unknown) == std::make_pair(0.0f, 0.0f));
}

// Salt or running water on the agent's way round a wall stops nothing. Its AI's agent walks the game's path through
// them, which neither blocks, and its scent gives out when it comes too near. So a player walks on towards the
// way's next corner, as far as its scent reaches, and is then free.
void player_shuck_walks_through_a_break_on_its_way() {
  const trail::Trail t = walked(0, 6);
  const auto confined = [&](trail::Point at, float dx, float dz, const trail::Way& way, const trail::Breaks& breaks) {
    trail::confine(true, {&t}, at, breaks, dx, dz, [&](trail::Point) { return way; });
    return std::make_pair(dx, dz);
  };
  const trail::Point door{8, 0, 2.0f};
  const trail::Way byTheDoor{true, door, false};
  const auto towards_door = [&](trail::Point at) {
    const float x = door.x - at.x, z = door.z - at.z, n = std::sqrt(x * x + z * z);
    return std::make_pair(x / n, z / n);
  };
  {
    trail::Breaks salt;
    salt.salt.push_back({5.0f, 0, 2.6f});  // in the doorway the way goes through
    const trail::Point near{4.3f, 0, 2.7f};
    trail::Point step;
    CHECK(trail::next_step({&t}, near, salt, step));  // its scent still reaches it here
    const auto [dx, dz] = towards_door(near);
    CHECK(confined(near, dx, dz, byTheDoor, salt) == std::make_pair(dx, dz));
    CHECK(confined(near, -dx, -dz, byTheDoor, salt) == std::make_pair(0.0f, 0.0f));  // the way is still the way
  }
  {
    trail::Breaks water;
    water.water.push_back({7.0f, 0, 2.4f});  // a running sink by the door
    const trail::Point near{4.6f, 0, 2.7f};
    trail::Point step;
    CHECK(trail::next_step({&t}, near, water, step));
    const auto [dx, dz] = towards_door(near);
    CHECK(confined(near, dx, dz, byTheDoor, water) == std::make_pair(dx, dz));
  }
  {
    // Where nothing stands between, a step still does not cross a break: its AI's own way there never does.
    trail::Breaks salt;
    salt.salt.push_back({3.0f, 0, 0});
    CHECK(steered(t, {2.0f, 0, 0}, 1, 0, salt) == std::make_pair(0.0f, 0.0f));
  }
}

// The game's path is asked again only when the answer may have changed: another step, a walk of kWayMoved, the
// next corner close enough to pass, or kWayMaxAgeMs gone (a door may have shut). Whatever the answer was.
void player_shuck_way_is_kept_between_asks() {
  int asks = 0;
  trail::Way answer{true, {10, 0, 0}, false};
  const auto ask = [&](trail::Point) {
    ++asks;
    return answer;
  };
  trail::WayCache cache;
  const trail::Point step{5, 0, 5};
  uint64_t now = 1000;
  CHECK(cache.get({0, 0, 0}, step, now, ask).next.x == 10.0f && asks == 1);
  CHECK(cache.get({0, 0, 0}, step, now, ask).next.x == 10.0f && asks == 1);        // the same frame
  CHECK(cache.get({0.2f, 0, 0}, step, now + 16, ask).known && asks == 1);          // a short way on
  CHECK(cache.get({0.2f, 0, 0}, {5, 0, 6}, now + 32, ask).known && asks == 2);     // another step
  CHECK(cache.get({0.2f, 0, 0}, {5, 0, 6}, now + 48, ask).known && asks == 2);
  CHECK(cache.get({0.5f, 0, 0}, {5, 0, 6}, now + 64, ask).known && asks == 3);     // moved past kWayMoved
  CHECK(cache.get({0.5f, 0, 0}, {5, 0, 6}, now + 64 + trail::kWayMaxAgeMs - 1, ask).known && asks == 3);
  CHECK(cache.get({0.5f, 0, 0}, {5, 0, 6}, now + 64 + trail::kWayMaxAgeMs, ask).known && asks == 4);  // too old
  // Close to the next corner it asks every time, so it never stands at a corner on an old answer.
  answer = {true, {0.6f, 0, 0}, false};
  now += 1000;
  CHECK(cache.get({0.5f, 0, 0}, step, now, ask).known && asks == 5);
  CHECK(cache.get({0.5f, 0, 0}, step, now, ask).known && asks == 6);
  // No complete path is an answer like any other, kept as long, and free steering lasts no longer.
  answer = {};
  now += 1000;
  CHECK(!cache.get({0, 0, 0}, step, now, ask).known && asks == 7);
  CHECK(!cache.get({0, 0, 0}, step, now + 1, ask).known && asks == 7);
  cache = {};
  CHECK(!cache.get({0, 0, 0}, step, now + 2, ask).known && asks == 8);
}

// What the hound smells from where it stands is what it may pick up: close enough, and with scent across the way.
void the_hound_smells_only_what_it_may_pick_up() {
  trail::Breaks salt;
  salt.salt.push_back({3.0f, 0, 0});
  CHECK(trail::smells({0, 0, 0}, {2, 0, 0}, salt));
  CHECK(!trail::smells({0, 0, 0}, {4, 0, 0}, salt));  // behind the salt
  CHECK(trail::smells({0, 0, 0}, {0, 0, 7.9f}, {}));
  CHECK(!trail::smells({0, 0, 0}, {0, 0, 8.0f}, {}));  // as far as kPickUp is too far
  CHECK(trail::smells({0, 0, 0}, {0, 3.0f, 0}, {}));   // it smells a floor away, as next_step does
  const trail::Trail above = [] {
    trail::Trail t;
    t.add({0, 3.0f, 0}, 1000, {}, false);
    return t;
  }();
  trail::Point step;
  CHECK(trail::next_step({&above}, {0, 0, 0}, {}, step));
}

// The trail a player's hound walks is its prey's: the corridor it stands in, then the prey it has, then the nearest.
void player_shuck_prey_is_the_trail_it_walks() {
  const trail::Trail a = walked(0, 6);
  trail::Trail b;
  uint64_t now = 1000;
  for (float x = 0; x <= 6.01f; x += 0.75f, now += 500) b.add({x, 0, 4.0f}, now, {}, false);
  const std::vector<const trail::Trail*> both = {&a, &b};
  CHECK(trail::walked(both, -1, {3, 0, 0.5f}, {}) == 0);   // no prey yet: the corridor it stands in
  CHECK(trail::walked(both, -1, {3, 0, 3.5f}, {}) == 1);
  CHECK(trail::walked(both, 0, {3, 0, 2.0f}, {}) == 0);    // in neither: it keeps the prey it has
  CHECK(trail::walked(both, 1, {3, 0, 2.0f}, {}) == 1);
  CHECK(trail::walked(both, -1, {3, 0, 1.5f}, {}) == 0);   // nor has it one: the one a hound picks up
  CHECK(trail::walked(both, 0, {3, 0, 3.5f}, {}) == 1);    // in another's: that one
  CHECK(trail::walked(both, 1, {3, 0, 3.5f}, {}) == 1);
  CHECK(trail::walked(both, -1, {30, 0, 30}, {}) == -1);   // none within reach
  trail::Breaks salt;
  salt.salt.push_back({3.0f, 0, 0});                       // a's trail cut between it and them
  CHECK(trail::walked(both, 0, {1, 0, 0.5f}, salt) == 1);  // it can no longer follow a
}

// Its hiding places as its player's route reaches them: the nearest not yet searched, at its AI's arrival distance.
void player_bogeyman_reaches_a_hiding_place() {
  const std::vector<Place> places = {{0, 0, 0}, {0.5f, 0, 0}, {5, 0, 0}};
  CHECK(reached_hiding_place(places, {false, false, false}, {0.4f, 0, 0}) == 1);
  CHECK(reached_hiding_place(places, {false, true, false}, {0.4f, 0, 0}) == 0);
  CHECK(reached_hiding_place(places, {true, true, false}, {0.4f, 0, 0}) == -1);  // nothing left within reach
  CHECK(reached_hiding_place(places, {false, false, false}, {5.0f, 0.5f, 0.5f}) == 2);
  CHECK(reached_hiding_place(places, {false, false, false}, {5.0f, 1.5f, 0}) == -1);  // true distance
  CHECK(reached_hiding_place(places, {}, {5.0f, 0, kBogeymanArrived}) == -1);
}

// Outside hunts a player steers the Black Shuck freely, as its AI moves outside hunts.
void player_shuck_free_outside_hunts() {
  const trail::Trail t = walked(0, 6);
  CHECK(steered(t, {3, 0, 0.8f}, 0, 1, {}, false) == std::make_pair(0.0f, 1.0f));
  CHECK(steered(t, {3, 0, 3}, 0, 1, {}, false) == std::make_pair(0.0f, 1.0f));
  trail::Breaks salt;
  salt.salt.push_back({3.0f, 0, 0});
  CHECK(steered(t, {2.0f, 0, 0}, 1, 0, salt, false) == std::make_pair(1.0f, 0.0f));
}

// The mod's wiring (registration.h, from pmmod_start), on the fake backend.
namespace wiring {

namespace content = pm::api::content;
namespace fake = pm::fake;

std::vector<std::pair<char, int>> g_calls;  // the rules' calls: 's' start, 't' tick, 'p' stop, with the user
void rule_start(void* user) { g_calls.emplace_back('s', static_cast<int>(reinterpret_cast<intptr_t>(user))); }
void rule_tick(void* user) { g_calls.emplace_back('t', static_cast<int>(reinterpret_cast<intptr_t>(user))); }
void rule_stop(void* user) { g_calls.emplace_back('p', static_cast<int>(reinterpret_cast<intptr_t>(user))); }
bool rule_speed(float*, float*, void*) { return false; }
bool use(void*) { return true; }
int32_t hud(void*) { return 7; }
bool held(int32_t, void*) { return true; }
uint32_t view(float*, uint32_t, void*) { return 0; }
void* target(const void* const*, uint32_t, void*) { return nullptr; }
const char* line(int32_t, void*) { return nullptr; }

void start() {
  fake::reset();
  std::string missing;
  pm::api::bind(fake::host(), missing);
  g_calls.clear();
}

// Spectral Flame's evidence, as its mod registers it (mods/spectral_flame/contract.cpp).
void register_flame() {
  content::EvidenceDef flame;
  flame.id = pm::flame::contract::kEvidenceId;
  flame.name = content::Text("en", "Spectral Flame");
  flame.journalLine = content::Text("en", "Watch a placed candle or lantern burn blue from base to tip");
  CHECK(content::register_evidence(flame) > 0);
}

bool register_all() {
  content::GhostDef callbacks;
  callbacks.useAbility = &use;
  callbacks.hudState = &hud;
  callbacks.heldBack = &held;
  callbacks.playerView = &view;
  callbacks.target = &target;
  callbacks.hudLine = &line;
  content::GhostRules rules;
  rules.start = &rule_start;
  rules.tick = &rule_tick;
  rules.stop = &rule_stop;
  rules.huntSpeed = &rule_speed;
  return register_content(callbacks, rules);
}

// The English text the mod's table holds for `key` (mods/custom_ghosts/i18n/default.json).
pm::content::Text english(const char* key) { return {{"en", pm::i18n::text(key)}}; }

// Every ghost is registered by its id with its English page, its evidence and its ability, and its
// callbacks with its Type; the host numbers them, in the byte order of their ids, after the game's types.
void every_ghost_is_registered_by_its_id() {
  start();
  register_flame();
  CHECK(register_all());
  fake::commit_content();
  fake::room(1, {1});
  CHECK(content::active());
  const pm::content::Registry& r = fake::content_registry();
  std::vector<std::string> ids;
  for (const Ghost& g : kGhosts) {
    const std::string id = id_of(g);
    ids.push_back(id);
    const pm::content::Ghost* def = r.registered(id);
    CHECK(def != nullptr);
    if (!def) continue;
    CHECK(def->name == english(title_key(g)) && def->description == english(g.description) &&
          def->strength == english(g.strength) && def->weakness == english(g.weakness) && def->tells.empty());
    for (int i = 0; i < 3; ++i) CHECK(def->evidence[i] == g.evidence[i]);
    CHECK(reinterpret_cast<intptr_t>(def->user) == g.type);
    CHECK(def->hudState == &hud && def->heldBack == &held && def->playerView == &view && def->evidenceLook &&
          def->target == &target && def->hudLine == &line);
    if (g.ability) {
      CHECK(def->abilityName == english(g.ability) && def->abilityDescription == english(g.abilityDescription));
      CHECK(def->useAbility == &use);
    } else {
      CHECK(def->abilityName.empty() && def->abilityDescription.empty() && !def->useAbility);
    }
    const std::vector<PmGhostRules> rules = r.rules_for(id);
    CHECK(rules.size() == 1 && reinterpret_cast<intptr_t>(rules[0].user) == g.type && rules[0].tick == &rule_tick &&
          rules[0].huntSpeed == &rule_speed);
  }
  // Four of them have an ability: the Echo, the Kikimora, the Lithobolia and the Gremlin.
  CHECK(std::count_if(std::begin(kGhosts), std::end(kGhosts), [](const Ghost& g) { return g.ability; }) == 4);
  std::sort(ids.begin(), ids.end());
  const int32_t first = content::type_of(ids.front());
  CHECK(first > 0 && content::evidence_of(kSpectralFlame) > 0);
  for (size_t i = 0; i < ids.size(); ++i) {
    CHECK(content::type_of(ids[i]) == first + static_cast<int32_t>(i));
    CHECK(content::id_of(first + static_cast<int32_t>(i)) == ids[i]);
  }
}

// The Draugr's Freezing is its last evidence and keeps its place as the Goryo's D.O.T.S. does: it is
// registered with PM_GHOST_KEEPS_LAST_EVIDENCE, and no other ghost of the mod is.
void draugr_declares_freezing_priority() {
  const Ghost& draugr = *find(kDraugr);
  CHECK(std::strcmp(draugr.evidence[0], kEmf) == 0 && std::strcmp(draugr.evidence[1], kSpectralFlame) == 0 &&
        std::strcmp(draugr.evidence[2], kFreezing) == 0);
  start();
  register_flame();
  CHECK(register_all());
  fake::commit_content();
  fake::room(1, {1});
  for (const Ghost& g : kGhosts) {
    const pm::content::Ghost* def = fake::content_registry().registered(id_of(g));
    CHECK(def != nullptr);
    if (def) CHECK(((def->flags & PM_GHOST_KEEPS_LAST_EVIDENCE) != 0) == (g.type == kDraugr));
  }
}

// The Black Shuck is blind in a hunt, so a player controlling it sees only its scent then: it is
// registered with PM_GHOST_BLIND_IN_HUNTS, and no other ghost of the mod is.
void black_shuck_declares_itself_blind_in_hunts() {
  start();
  register_flame();
  CHECK(register_all());
  fake::commit_content();
  fake::room(1, {1});
  for (const Ghost& g : kGhosts) {
    const pm::content::Ghost* def = fake::content_registry().registered(id_of(g));
    CHECK(def != nullptr);
    if (def) CHECK(((def->flags & PM_GHOST_BLIND_IN_HUNTS) != 0) == (g.type == kBlackShuck));
  }
}

// Without Spectral Flame's evidence, every ghost that gives it is dropped once every mod has started:
// only the Echo, the Dullahan and the Black Shuck stay.
void without_spectral_flame_only_three_stay() {
  start();
  CHECK(register_all());
  fake::commit_content();
  fake::room(1, {1});
  int kept = 0;
  for (const Ghost& g : kGhosts) {
    const bool registered = fake::content_registry().registered(id_of(g)) != nullptr;
    CHECK(registered == !gives(g, kSpectralFlame));
    kept += registered;
  }
  CHECK(kept == 3);
}

// The rules run for the ghost the contract's ghost acts as, with its Type, and the Hydra's look for
// Spectral Flame is green, by its identity.
void the_rules_and_the_look_follow_the_ghost() {
  start();
  register_flame();
  CHECK(register_all());
  fake::commit_content();
  fake::room(1, {1});
  const int32_t none = fake::kAuditedNoGhost;
  const int32_t hydra = content::type_of(id_of(*find(kHydra))), echo = content::type_of(id_of(*find(kEcho)));
  fake::contract_ghost(hydra, none);
  fake::tick_rules();
  CHECK((g_calls == std::vector<std::pair<char, int>>{{'s', kHydra}, {'t', kHydra}}));
  CHECK(content::evidence_look(kSpectralFlame) == "green" && content::evidence_look("orbs").empty());
  g_calls.clear();
  fake::contract_ghost(echo, none);
  fake::tick_rules();
  CHECK((g_calls == std::vector<std::pair<char, int>>{{'p', kHydra}, {'s', kEcho}, {'t', kEcho}}));
  CHECK(content::evidence_look(kSpectralFlame).empty());  // its own flame
  const char* look = evidence_look(kSpectralFlame, reinterpret_cast<void*>(static_cast<intptr_t>(kHydra)));
  CHECK(look && std::string(look) == "green");
  CHECK(!evidence_look(nullptr, reinterpret_cast<void*>(static_cast<intptr_t>(kHydra))));
  CHECK(!evidence_look(kSpectralFlame, reinterpret_cast<void*>(static_cast<intptr_t>(kLethe))));
  fake::contract_ghost(-1, -1);
  fake::tick_rules();
}

}  // namespace wiring

}  // namespace

int main() {
  use_text({"mods/custom_ghosts/i18n/default.json", "mods/playable_ghost/i18n/default.json"});
  the_echo_remembers_the_last_phrase();
  voices_survive_the_trip();
  every_custom_ghost_is_identifiable();
  every_ghost_has_its_id();
  every_page_tells_its_story();
  the_held_ghosts_say_who_they_hunt();
  the_flame_ghosts_carry_the_flame();
  the_flame_ghosts_are_the_designed_ones();
  the_knell_moves_on_a_strict_beat();
  the_hydra_grows_heads();
  the_flame_ghost_messages_round_trip();
  the_asterion_batters_doors();
  the_lithobolia_aims_true();
  the_lithobolia_only_throws_at_people();
  the_ghost_sees_a_player_through_their_sight_parts();
  the_copy_finds_the_animator_where_the_model_keeps_it();
  the_hydras_heads_throw_as_the_hunt_does();
  the_gremlin_leaves_watched_equipment_alone();
  the_hunt_goes_only_for_whom_the_game_would();
  the_own_sight_flag_survives_a_throwing_sight_test();
  the_sensor_copies_take_the_ghosts_settings();
  the_bogeyman_searches_the_hiding_places();
  the_bogeyman_stalks_the_youngest();
  the_bogeyman_lurks_in_the_nearest_closet();
  player_bogeyman_hunt_starts_at_youngest_closet();
  player_bogeyman_door_share_matches_ai();
  bogeyman_ai_tie_breaks_by_join_order();
  bogeyman_target_is_youngest();
  bogeyman_target_moves_on_when_youngest_dies();
  bogeyman_target_on_client();
  the_brownie_works_unseen();
  the_brownie_steps_quietly_as_a_myling();
  the_lethe_makes_them_forget();
  the_lethe_forgets_the_player_it_lost();
  the_tempest_strikes_with_the_weather();
  the_tempest_opens_each_hunt_with_a_strike();
  the_mesmer_shows_each_their_own();
  the_dullahan_charges_straight_and_stalls_at_corners();
  the_draugr_crawls_far_from_its_room();
  draugr_skips_long_roam_like_goryo();
  draugr_goryo_type_restored_when_picker_throws();
  draugr_room_change_hook_gone();
  draugr_returns_prop_to_original_place();
  draugr_never_takes_a_held_prop();
  draugr_hunts_most_recent_thief();
  draugr_names_no_thief_without_one();
  draugr_target_is_thief_while_angered();
  draugr_target_null_when_calm();
  draugr_resurveys_after_room_change();
  draugr_calm_after_room_change();
  draugr_thief_found_when_dropped_between_surveys();
  draugr_hud_anger_on_client_from_anger_message();
  custom_ghost_hud_lines();
  draugr_room_colder_while_angered();
  draugr_cold_goes_below_freezing_when_withheld();
  draugr_cold_ends_with_anger();
  the_shuck_follows_the_trail_and_salt_breaks_it();
  water_and_rain_break_scent();
  scent_does_not_cross_a_break();
  salt_or_water_laid_across_a_trail_stops_the_hound();
  the_hound_picks_up_a_trail_it_can_follow();
  the_shuck_waits_out_a_smudge();
  the_shuck_keeps_only_the_player_it_follows();
  own_speed_goes_through_the_games_multipliers();
  a_rule_gives_the_host_its_speed_inputs();
  a_rule_that_stops_writes_the_games_own_default();
  the_echos_phrase_comes_from_a_client_that_holds_it();
  the_echo_repeats_what_players_do();
  a_repeat_waits_for_the_echos_next_interaction();
  player_echo_repeats_last_used();
  a_use_outside_its_room_leaves_the_last_one_in_it();
  a_repeat_replaces_the_dormancy_roll();
  a_trail_no_longer_recorded_goes_cold();
  player_shuck_confined_to_trail_corridor();
  player_shuck_walks_through_a_break_on_its_way();
  player_shuck_way_is_kept_between_asks();
  the_hound_smells_only_what_it_may_pick_up();
  player_shuck_blocked_at_salt_water_rain_gap();
  player_shuck_free_outside_hunts();
  player_shuck_free_when_no_scent_reaches_it();
  player_shuck_can_always_reach_the_scent_it_smells();
  player_shuck_not_stalled_by_wall();
  player_shuck_prey_is_the_trail_it_walks();
  player_bogeyman_reaches_a_hiding_place();
  wiring::every_ghost_is_registered_by_its_id();
  wiring::without_spectral_flame_only_three_stay();
  wiring::draugr_declares_freezing_priority();
  wiring::black_shuck_declares_itself_blind_in_hunts();
  wiring::the_rules_and_the_look_follow_the_ghost();
  pm::fake::reset();
  return g_failures ? 1 : 0;
}
