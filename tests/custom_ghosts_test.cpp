// Offline tests for the custom ghosts' data and rules (mods/custom_ghosts/ghosts.h, trail.h).
// Run: build\custom_ghosts_test.exe
#include <cmath>
#include <set>
#include <string>
#include <vector>

#include "difficulty_rule.h"
#include "echo_rule.h"
#include "ghosts.h"
#include "grid_rule.h"
#include "gremlin_rule.h"
#include "asterion_rule.h"
#include "bogeyman_rule.h"
#include "brownie_rule.h"
#include "hydra_rule.h"
#include "knell_rule.h"
#include "lethe_rule.h"
#include "lithobolia_rule.h"
#include "mesmer_rule.h"
#include "shuck_rule.h"
#include "tempest_rule.h"
#include "speed_rule.h"
#include "trail.h"
#include "voice.h"

#include "check.h"

using namespace pg::custom;

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

void every_custom_ghost_is_identifiable() {
  auto taken = vanilla_evidence();
  CHECK(taken.size() == 30);
  std::set<int> types;
  for (const Ghost& g : kGhosts) {
    CHECK(g.type >= kFirstType && types.insert(g.type).second);
    const std::set<int> e{g.evidence[0], g.evidence[1], g.evidence[2]};
    CHECK(e.size() == 3);                  // three different evidence types
    CHECK(taken.insert(e).second);         // no other ghost, vanilla or custom, shares them
    CHECK(*g.name && *g.description && *g.strength && *g.weakness);
    for (int x : e) CHECK(*evidence_name(x));
  }
  CHECK(taken.size() == 30 + static_cast<size_t>(kGhostCount));  // every ghost has a combination of its own
}

void every_page_tells_its_story() {
  // pages.cpp applies no length limit; these are the current longest entries plus a margin.
  constexpr size_t kMaxDescription = 260, kMaxStrength = 180, kMaxLine = 140;
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
}

void the_flame_ghosts_carry_the_flame() {
  const auto has = [](int type, int e) {
    const Ghost* g = find(type);
    return g && (g->evidence[0] == e || g->evidence[1] == e || g->evidence[2] == e);
  };
  CHECK(has(kDraugr, kSpectralFlame) && has(kDraugr, kEmf) && has(kDraugr, kFreezing) && !has(kDraugr, kOrbs));
  CHECK(has(kKikimora, kSpectralFlame) && has(kKikimora, kUltraviolet) && has(kKikimora, kWriting) &&
        !has(kKikimora, kOrbs));
  for (int type : {kEcho, kDullahan, kBlackShuck}) CHECK(!has(type, kSpectralFlame));
  CHECK(std::string(evidence_name(kSpectralFlame)) == "Spectral Flame");
  CHECK(kSpectralFlame == 8);  // the game's evidence value after D.O.T.S. (7)
  // Twelve carry it, so it turns up about as often as a vanilla evidence (90 / 7 = 13 of 30).
  int carriers = 0;
  int beside[9] = {};
  for (const Ghost& g : kGhosts) {
    if (!has(g.type, kSpectralFlame)) continue;
    ++carriers;
    for (int e : g.evidence)
      if (e != kSpectralFlame) ++beside[e];
  }
  CHECK(carriers == 12);
  for (int e = kEmf; e <= kDots; ++e) CHECK(beside[e] == 3 || beside[e] == 4);  // spread evenly beside it
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

void the_gremlin_leaves_watched_equipment_alone() {
  CHECK(gremlin_target_class("CCTV") && gremlin_target_class("MotionSensor") && gremlin_target_class("Torch"));
  CHECK(!gremlin_target_class("Candle") && !gremlin_target_class("Crucifix") && !gremlin_target_class(nullptr));
  CHECK(gremlin_may_sabotage(true, 5));
  CHECK(!gremlin_may_sabotage(false, 5));               // already off
  CHECK(!gremlin_may_sabotage(true, kGremlinGuard));     // someone beside it
  CHECK(!gremlin_may_sabotage(true, 0.4f));              // held
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
  CHECK(youngest({{0, 20, 9, true}, {0, 20, 2, true}, {0, 30, 1, true}}) == 1);  // then level, then nearest
  // A player whose progress cannot be read comes after every player whose progress can.
  CHECK(youngest({{kUnknownProgress, kUnknownProgress, 1, true}, {3, 90, 9, true}}) == 1);
  CHECK(youngest({{kUnknownProgress, kUnknownProgress, 1, true}}) == 0);
  // In an open room it loses sight beyond 7 m: such a player neither stays its prey nor counts among those
  // it picks the youngest from.
  CHECK(open_sight_lost(true, 7.5f) && !open_sight_lost(true, 7.0f) && !open_sight_lost(false, 20.0f));
}

void the_bogeyman_lurks_in_the_nearest_closet() {
  CHECK(lurk_place({{0, 0, 12}}, {0, 0, 0}) == -1);                        // none within 10 m: it starts where it is
  CHECK(lurk_place({{0, 0, 8}, {0, 0, 3}}, {0, 0, 0}) == 1);
  CHECK(lurk_place({{0, 0, 10}}, {0, 0, 0}) == 0);                         // exactly 10 m is within reach
  CHECK(lurk_place({{4, 3.5f, 0}}, {0, 0, 0}) == -1);                      // 4 m along but 3.5 m up: another floor
  CHECK(lurk_place({{9, 1.5f, 0}}, {0, 0, 0}) == 0);                       // a step up on their floor: its true distance
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
  CHECK(!lethe_forgets(0.9f) && lethe_forgets(1.0f));
  CHECK(lethe_replay_at(10, 0) == 10 + kLetheReplayMin && lethe_replay_at(10, 1) == 10 + kLetheReplayMax);
  CHECK(lethe_replay_at(0, 0.5f) > kLetheReplayMin && lethe_replay_at(0, 0.5f) < kLetheReplayMax);
  CHECK(lethe_state_shows_itself(2) && lethe_state_shows_itself(8) && lethe_state_shows_itself(14));
  CHECK(!lethe_state_shows_itself(0) && !lethe_state_shows_itself(1) && !lethe_state_shows_itself(3));
  // Only what records its activity fades: the ghost, its writing, its sounds; not bones or cursed objects.
  CHECK(lethe_activity(5) && lethe_activity(13) && lethe_activity(26) && lethe_activity(43));
  CHECK(!lethe_activity(4) && !lethe_activity(6) && !lethe_activity(9) && !lethe_activity(16) && !lethe_activity(20));
}

void the_tempest_strikes_with_the_weather() {
  // The contract's own weather (LevelValues' weather type) sets how often it strikes.
  CHECK(tempest_sky(0) == Sky::Clear && tempest_sky(1) == Sky::Clear && tempest_sky(4) == Sky::Clear);  // clear, fog, snow
  CHECK(tempest_sky(5) == Sky::Clear && tempest_sky(7) == Sky::Clear && tempest_sky(-1) == Sky::Clear);  // wind, blood moon, unread
  CHECK(tempest_sky(2) == Sky::Rain && tempest_sky(3) == Sky::HeavyRain);
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
  for (int i = 0; i < 10; ++i) CHECK(types[i] == 36 + i && is_custom(types[i]));
  const auto evidence_is = [](int type, int a, int b) {
    const Ghost* g = find(type);
    const std::set<int> e{g->evidence[0], g->evidence[1], g->evidence[2]};
    return e == std::set<int>{a, b, kSpectralFlame};
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
  CHECK(type_of_title_key("Journal_45Title") == kHydra);
  CHECK(type_of_title_key("Journal_36Title") == kLithobolia);
  CHECK(type_of_title_key("Journal_46Title") == 0);
}

void names_answer_two_digit_keys() {
  CHECK(maybe_title_key(u"Journal_31Title"));
  CHECK(maybe_title_key(u"Journal_45Title"));  // the flame ghosts reach the forties
  CHECK(!maybe_title_key(u"Journal_SpiritTitle"));
  CHECK(!maybe_title_key(u"Journal_3Title"));
  CHECK(!maybe_title_key(u"Journal_31Titles"));
  CHECK(!maybe_title_key(u""));
}

void names_answer_the_games_keys() {
  CHECK(title_key(kEcho) == "Journal_31Title");
  CHECK(type_of_title_key("Journal_34Title") == kDraugr);
  CHECK(type_of_title_key("Journal_SpiritTitle") == 0);
  CHECK(type_of_title_key(nullptr) == 0);
  CHECK(is_custom(kBlackShuck) && !is_custom(0) && !is_custom(kBehaviourType));
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

// The Ghosts tab list as build 25811160 ships it: 30 entries placed by hand, its GridLayoutGroup off.
void the_ghosts_tab_list_becomes_a_grid_that_keeps_its_layout() {
  std::vector<PlacedCell> shipped;
  const float columnX[3] = {125.0f, 375.5f, 626.0f};
  for (int i = 0; i < 30; ++i) shipped.push_back({columnX[i % 3], -50.0f - 102.5f * (i / 3), 250.0f, 100.0f});
  const GridShape s = baked_grid(shipped);
  CHECK(s.cellWidth == 250.0f && s.cellHeight == 100.0f && s.columns == 3);
  // Laid out by the grid (upper left, no padding, its own 0.5 x 2.5 spacing), every entry stays put.
  for (int i = 0; i < 30; ++i) {
    const float x = (i % s.columns) * (s.cellWidth + 0.5f) + s.cellWidth / 2;
    const float y = -((i / s.columns) * (s.cellHeight + 2.5f) + s.cellHeight / 2);
    CHECK(std::fabs(x - shipped[i].x) < 0.01f && std::fabs(y - shipped[i].y) < 0.01f);
  }
  // Fifteen more entries fit in the height the thirty filled.
  const float height = fitted_cell_height(s.cellHeight, 2.5f, s.columns, 30, 30 + kGhostCount);
  CHECK(std::fabs(15 * (height + 2.5f) - 10 * 102.5f) < 0.01f);
  CHECK(fitted_cell_height(100.0f, 2.5f, 3, 30, 30) == 100.0f);  // nothing added
  CHECK(fitted_cell_height(100.0f, 2.5f, 3, 30, 31) < 100.0f);   // one more starts a row
  CHECK(baked_grid({}).columns == 0);
}

// The custom difficulty presets (Custom 1..3 are Difficulty.difficulty 6..8) and their custom ghosts.
void each_preset_remembers_its_custom_ghost() {
  Picks picks;
  CHECK(picks.of(6) == kNoGhostType);
  CHECK(picks.choose(6, kBlackShuck));    // a custom ghost is remembered
  CHECK(!picks.choose(6, kBlackShuck));   // choosing it again changes nothing
  CHECK(picks.choose(8, kHydra));
  CHECK(picks.of(6) == kBlackShuck && picks.of(7) == kNoGhostType && picks.of(8) == kHydra);
  CHECK(picks.choose(6, 3));              // a vanilla ghost replaces it: nothing to remember
  CHECK(picks.of(6) == kNoGhostType);
  CHECK(!picks.choose(7, kNoGhostType));  // nothing was remembered
  CHECK(!picks.choose(2, kEcho));         // Professional is not a custom preset
  CHECK(picks.of(2) == kNoGhostType);
  // Kept as text, one "preset=type" line each; anything else in the file is ignored.
  const Picks back = Picks::parse(picks.text());
  CHECK(back.of(8) == kHydra && back.of(6) == kNoGhostType);
  const Picks odd = Picks::parse("7=35\r\n6=3\nnonsense\n9=31\n8=\n=40\n6=36x\n");
  CHECK(odd.of(7) == kBlackShuck);
  CHECK(odd.of(6) == kNoGhostType);  // a vanilla type is never a remembered pick
  CHECK(odd.of(9) == kNoGhostType);  // not a custom preset
  CHECK(odd.of(8) == kNoGhostType);
}

void a_custom_ghost_never_leaves_the_game() {
  // The save file never holds one, with or without everyone running the custom ghosts.
  CHECK(outbound_type(kEcho, true, true) == kNoGhostType);
  CHECK(outbound_type(kEcho, true, false) == kNoGhostType);
  // The lobby sync carries it only to a room where everyone runs them.
  CHECK(outbound_type(kEcho, false, true) == kEcho);
  CHECK(outbound_type(kEcho, false, false) == kNoGhostType);
  // The game's own choices go out unchanged.
  CHECK(outbound_type(5, true, false) == 5 && outbound_type(kNoGhostType, false, false) == kNoGhostType);
}

void the_ghost_option_counts_random_and_every_ghost() {
  CHECK(ghost_choices(30) == 31);                // as the menu scene ships it
  CHECK(ghost_choices(30 + kGhostCount) == 46);  // with the custom ghosts offered
  CHECK(ghost_index(5, 31) == 5 && ghost_index(31, 31) == 0 && ghost_index(-1, 31) == 0);
}

}  // namespace

int main() {
  the_echo_remembers_the_last_phrase();
  voices_survive_the_trip();
  every_custom_ghost_is_identifiable();
  every_page_tells_its_story();
  names_answer_the_games_keys();
  the_flame_ghosts_carry_the_flame();
  the_flame_ghosts_are_the_designed_ones();
  the_knell_moves_on_a_strict_beat();
  the_hydra_grows_heads();
  the_flame_ghost_messages_round_trip();
  the_asterion_batters_doors();
  the_lithobolia_aims_true();
  the_lithobolia_only_throws_at_people();
  the_gremlin_leaves_watched_equipment_alone();
  the_bogeyman_searches_the_hiding_places();
  the_bogeyman_stalks_the_youngest();
  the_bogeyman_lurks_in_the_nearest_closet();
  the_brownie_works_unseen();
  the_brownie_steps_quietly_as_a_myling();
  the_lethe_makes_them_forget();
  the_tempest_strikes_with_the_weather();
  the_tempest_opens_each_hunt_with_a_strike();
  the_mesmer_shows_each_their_own();
  names_answer_two_digit_keys();
  the_dullahan_charges_straight_and_stalls_at_corners();
  the_draugr_crawls_far_from_its_room();
  the_shuck_follows_the_trail_and_salt_breaks_it();
  water_and_rain_break_scent();
  scent_does_not_cross_a_break();
  salt_or_water_laid_across_a_trail_stops_the_hound();
  the_hound_picks_up_a_trail_it_can_follow();
  the_shuck_waits_out_a_smudge();
  the_shuck_keeps_only_the_player_it_follows();
  own_speed_goes_through_the_games_multipliers();
  a_rule_that_stops_writes_the_games_own_default();
  the_echos_phrase_comes_from_a_client_that_holds_it();
  the_echo_repeats_what_players_do();
  a_repeat_waits_for_the_echos_next_interaction();
  a_use_outside_its_room_leaves_the_last_one_in_it();
  a_repeat_replaces_the_dormancy_roll();
  a_trail_no_longer_recorded_goes_cold();
  the_ghosts_tab_list_becomes_a_grid_that_keeps_its_layout();
  each_preset_remembers_its_custom_ghost();
  a_custom_ghost_never_leaves_the_game();
  the_ghost_option_counts_random_and_every_ghost();
  return g_failures ? 1 : 0;
}
