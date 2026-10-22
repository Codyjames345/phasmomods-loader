// Offline tests for the Spectral Flame rules (mods/spectral_flame/flame_rules.h).
// Run: build\spectral_flame_test.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "flame_rules.h"

#include "check.h"

using namespace pg::flame;

namespace {

constexpr float kFrame = 1.0f / 30;

Inputs in_room(Kind kind = Kind::Tier1, bool evidence = true) {
  Inputs in{};
  in.lit = in.placed = in.ghostInRoom = true;
  in.hasEvidence = evidence;
  in.kind = kind;
  return in;
}

Inputs away(Kind kind = Kind::Tier1, bool evidence = true) {
  Inputs in = in_room(kind, evidence);
  in.ghostInRoom = false;
  return in;
}

State run(State s, const Inputs& in, float seconds) {
  for (float t = 0; t + kFrame / 2 < seconds; t += kFrame) s = step(s, in, kFrame);
  return s;
}

float blue_after(const Inputs& in, float seconds) { return run({}, in, seconds).blueness; }

bool near(float a, float b, float tolerance = 0.01f) { return std::fabs(a - b) <= tolerance; }
bool same(Rgb a, Rgb b) { return near(a.r, b.r) && near(a.g, b.g) && near(a.b, b.b); }

float saturation(Rgb c) {
  const float hi = std::max({c.r, c.g, c.b});
  return hi > 0 ? (hi - std::min({c.r, c.g, c.b})) / hi : 0;
}

// Hue in degrees (red 0, green 120, blue 240), and how far apart two colours' hues are round the wheel.
float hue(Rgb c) {
  const float hi = std::max({c.r, c.g, c.b}), d = hi - std::min({c.r, c.g, c.b});
  if (d <= 0) return 0;
  const float h = hi == c.r ? (c.g - c.b) / d : hi == c.g ? (c.b - c.r) / d + 2 : (c.r - c.g) / d + 4;
  return std::fmod(h * 60 + 360, 360.0f);
}
float hues_apart(Rgb a, Rgb b) {
  const float d = std::fabs(hue(a) - hue(b));
  return std::min(d, 360 - d);
}

// What a flame tint looks like on screen, roughly: the flame texture scales the tint, then a filmic
// tone curve (Narkowicz's ACES fit) maps HDR to the display. The game's own curve may differ.
Rgb on_screen(Rgb tint, float texture) {
  auto curve = [](float x) { return x * (2.51f * x + 0.03f) / (x * (2.43f * x + 0.59f) + 0.14f); };
  return {curve(tint.r * texture), curve(tint.g * texture), curve(tint.b * texture)};
}

void rises_only_with_the_ghost_in_the_room() {
  CHECK(blue_after(in_room(), 40) > 0.1f);
  CHECK(blue_after(away(), 60) == 0);  // a ghost elsewhere never turns it blue
  CHECK(run(State{0.5f}, away(), 10).blueness < 0.5f);
}

void every_second_in_the_room_counts() {
  CHECK(blue_after(in_room(), 5) > 0.05f);   // it starts to turn as soon as the ghost is there
  CHECK(blue_after(in_room(), 1) < 0.02f);   // a ghost passing through changes almost nothing
  // Short visits add up: ten 10 s visits with 10 s away between them turn a true flame well blue.
  State s{};
  for (int i = 0; i < 10; ++i) s = run(run(s, in_room(), 10), away(), 10);
  CHECK(s.blueness > 0.7f);
}

void full_blue_in_a_minute_faster_with_higher_tiers() {
  CHECK(build_seconds(Kind::Tier1) == 60 && build_seconds(Kind::Tier2) == 50 && build_seconds(Kind::Tier3) == 40 &&
        build_seconds(Kind::Candelabra) == 60);
  CHECK(blue_after(in_room(Kind::Tier1), 60.5f) > 0.99f && blue_after(in_room(Kind::Tier1), 58) < 0.99f);
  CHECK(blue_after(in_room(Kind::Tier2), 50.5f) > 0.99f && blue_after(in_room(Kind::Tier2), 48) < 0.99f);
  CHECK(blue_after(in_room(Kind::Tier3), 40.5f) > 0.99f && blue_after(in_room(Kind::Tier3), 38) < 0.99f);
  CHECK(blue_after(in_room(Kind::Candelabra), 58) < 0.99f);
}

void halfway_in_a_minute_without_the_evidence() {
  CHECK(kFalseCap == 0.5f);
  CHECK(near(blue_after(in_room(Kind::Tier1, false), 60.5f), kFalseCap, 0.002f));
  CHECK(blue_after(in_room(Kind::Tier1, false), 55) < 0.47f);
  CHECK(near(blue_after(in_room(Kind::Tier3, false), 40.5f), kFalseCap, 0.002f));
  CHECK(blue_after(in_room(Kind::Tier1, false), 600) == kFalseCap);  // and never further
  CHECK(run(State{0.9f}, in_room(Kind::Tier1, false), kFrame).blueness <= kFalseCap);
}

void a_true_flame_turns_twice_as_fast() {
  // Half a minute in, a true flame already looks the way a false one ends up.
  CHECK(near(blue_after(in_room(Kind::Tier1, true), 30), 0.5f));
  CHECK(near(blue_after(in_room(Kind::Tier1, false), 30), 0.25f));
}

void it_drops_back_to_yellow_at_half_the_speed_it_turns() {
  CHECK(near(run(State{1}, away(), 60).blueness, 0.5f));  // fully blue to none in two minutes
  CHECK(run(State{1}, away(), 121).blueness == 0);
  CHECK(run(State{1}, away(), 119).blueness > 0);
  CHECK(run(State{1}, away(Kind::Tier3), 81).blueness == 0 && run(State{1}, away(Kind::Tier3), 79).blueness > 0);
  // A false flame too: from its half to none in two minutes.
  CHECK(run(State{0.5f}, away(Kind::Tier1, false), 121).blueness == 0);
  CHECK(run(State{0.5f}, away(Kind::Tier1, false), 119).blueness > 0);
}

void a_false_flame_is_always_half_a_true_one() {
  // Whatever the ghost does, until the true flame is fully blue.
  State yes{}, no{};
  for (int i = 0; i < 6; ++i) {
    yes = run(run(yes, in_room(Kind::Tier1, true), 7), away(Kind::Tier1, true), 5);
    no = run(run(no, in_room(Kind::Tier1, false), 7), away(Kind::Tier1, false), 5);
    CHECK(yes.blueness < 1 && near(no.blueness, yes.blueness / 2, 0.005f));
  }
}

void picking_it_up_or_putting_it_out_starts_over() {
  State fullBlue{};
  fullBlue.blueness = 1;
  fullBlue.full = true;
  Inputs held = in_room();
  held.placed = false;
  const State a = step(fullBlue, held, kFrame);
  CHECK(a.blueness == 0 && !a.full);
  Inputs out = in_room();
  out.lit = false;
  const State b = step(fullBlue, out, kFrame);
  CHECK(b.blueness == 0 && !b.full);
}

void comfort_and_burn() {
  CHECK(kComfortLimit == 0.5f);
  CHECK(gives_comfort(0) && gives_comfort(kFalseCap) && !gives_comfort(0.51f));  // a false flame still comforts
  CHECK(burn_multiplier(0) == 1 && near(burn_multiplier(1), 2) && near(burn_multiplier(0.5f), 1.5f));
}

void the_extra_blow_out_comes_only_at_full_blue() {
  CHECK(blow_out_chance(0, 1) == 0);
  CHECK(blow_out_chance(0.99f, 1) == 0);
  CHECK(near(blow_out_chance(1, 1), 1.0f / 75, 0.0001f));
  CHECK(near(blow_out_chance(1, 0.5f), 0.5f / 75, 0.0001f));
}

void a_flame_ghost_cannot_blow_out_a_flame_until_it_is_fully_blue() {
  CHECK(!ghost_may_blow_out(true, true, run({}, in_room(), 30)));
  CHECK(!ghost_may_blow_out(true, true, run({}, in_room(), 58)));
  CHECK(ghost_may_blow_out(true, true, run({}, in_room(), 61)));
  CHECK(ghost_may_blow_out(false, true, run({}, in_room(), 30)));        // a flame in a hand is not a test
  CHECK(ghost_may_blow_out(true, false, run({}, in_room(Kind::Tier1, false), 30)));  // other ghosts as the game's
}

void once_fully_blue_it_may_be_blown_out_after_fading() {
  State s = run({}, in_room(), 61);
  CHECK(s.full);
  s = run(s, away(), 30);  // the ghost steps out; the flame fades a little
  CHECK(s.blueness < 1 && s.full && ghost_may_blow_out(true, true, s));
  Inputs held = in_room();
  held.placed = false;
  CHECK(!step(s, held, kFrame).full);  // picking it up starts the test over
}

void inert_without_a_carrier() {
  const int vanilla[] = {1, 2, 5, 3, 4, 7};
  CHECK(!active_for(vanilla, 2));
  const int withFlame[] = {1, 2, 5, 1, 6, 8};
  CHECK(active_for(withFlame, 2));
  CHECK(!active_for(nullptr, 0));
}

void a_flame_goes_pale_at_half_and_deep_blue_at_full() {
  const Rgb warm{10.681f, 3.593f, 1.461f};  // the game's "Candle flame" tint
  const Rgb deep = kFlameDeep;
  const Rgb pale = flame_pale(deep);
  CHECK(on_screen(warm, 1).r > on_screen(warm, 1).b + 0.1f);  // the model shows the warm flame warm
  const Rgb none = tint(warm, pale, deep, 0);
  CHECK(none.r == warm.r && none.g == warm.g && none.b == warm.b);  // untouched at none
  const Rgb half = tint(warm, pale, deep, kFalseCap);
  CHECK(near(half.r, pale.r) && near(half.g, pale.g) && near(half.b, pale.b));
  const Rgb full = tint(warm, pale, deep, 1);
  CHECK(near(full.r, deep.r) && near(full.g, deep.g) && near(full.b, deep.b));
  for (float texture : {0.5f, 1.0f}) {
    // On screen the false tell is a light blue, not white, and the true flame a deep blue.
    const Rgb paleShown = on_screen(half, texture), deepShown = on_screen(full, texture);
    CHECK(paleShown.b > paleShown.g && paleShown.g > paleShown.r);
    CHECK(saturation(paleShown) > 0.3f && saturation(paleShown) < 0.75f);
    CHECK(saturation(deepShown) > 0.95f && deepShown.b > deepShown.r + 0.5f);
    CHECK(deepShown.b > 2.5f * deepShown.g);  // blue, not the teal of the game's "Gas Flame Blue"
  }
  // Hotter on the way: orange, then white, then blue; never through pink (blue and red over green).
  for (float b = 0; b <= 1.0001f; b += 0.01f) {
    const Rgb c = tint(warm, pale, deep, b);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));
  }
  const Rgb hot = tint(warm, pale, deep, kFalseCap / 2);
  CHECK(near(hot.r, hot.g) && near(hot.g, hot.b));  // white-hot at a quarter
}

void a_lantern_goes_to_the_same_blue() {
  const Rgb warm{42.723f, 12.273f, 3.426f};  // the game's "Lantern Flame" _Color_2
  const Rgb pale = flame_pale(kFlameDeep);
  const Rgb paleShown = on_screen(tint(warm, pale, kFlameDeep, kFalseCap), 1);
  CHECK(paleShown.b > paleShown.g && paleShown.g > paleShown.r && saturation(paleShown) > 0.3f);
  CHECK(saturation(on_screen(tint(warm, pale, kFlameDeep, 1), 1)) > 0.95f);
}

void a_light_goes_pale_at_half_and_deep_blue_at_full() {
  const Rgb warm{1, 0.62f, 0.3f};
  const float b = brightness(warm);
  const Rgb half = tint(warm, light_pale(b), light_deep(b), kFalseCap);
  const Rgb full = tint(warm, light_pale(b), light_deep(b), 1);
  CHECK(saturation(half) < 0.5f && half.b >= half.g && half.g >= half.r);
  CHECK(saturation(full) > 0.95f && full.b > 3 * full.g);
  CHECK(near(brightness(full), b));  // a light keeps its strongest channel (and so looks darker)
  // The light a flame casts at full blue has the flame's own hue. Whether the room then looks as
  // blue as the flame is an in-game check (FINDINGS 20).
  CHECK(near(full.g / full.b, kFlameDeep.g / kFlameDeep.b, 0.001f) && near(full.r / full.b, kFlameDeep.r / kFlameDeep.b, 0.001f));
  for (float t = 0; t <= 1.0001f; t += 0.01f) {
    const Rgb c = tint(warm, light_pale(b), light_deep(b), t);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));  // never pink
  }
}

void the_hydras_flame_burns_green() {
  const Rgb warm{10.7f, 3.6f, 1.5f};  // the candle's own HDR tint
  const FlameColours blue = flame_colours(kFlameDeep, false), green = flame_colours(kFlameDeep, true);
  CHECK(same(blue.deep, kFlameDeep) && same(blue.pale, flame_pale(kFlameDeep)));  // other ghosts unchanged
  CHECK(green.deep.g > green.deep.b && green.deep.g > green.deep.r);    // green, not blue
  CHECK(green.pale.g > green.pale.b);                                    // the pale stop is pale green too
  CHECK(near(brightness(green.deep), brightness(kFlameDeep), 0.05f));
  CHECK(same(tint(warm, green.pale, green.deep, kFalseCap), green.pale));  // withheld evidence: the cap, pale green
  CHECK(same(tint(warm, green.pale, green.deep, 1), green.deep));
  for (float texture : {0.5f, 1.0f}) {
    // On screen the false tell is a mint green, as washed out as the pale blue and plainly not it, off
    // the warm side of the wheel (red no higher than blue) so it never passes for a lit candle; the
    // true flame a deep green, far more saturated than the pale one.
    const Rgb paleShown = on_screen(green.pale, texture), deepShown = on_screen(green.deep, texture);
    CHECK(paleShown.g > paleShown.r && paleShown.g > paleShown.b && paleShown.b >= paleShown.r - 0.02f);
    CHECK(saturation(paleShown) > 0.3f && saturation(paleShown) < 0.75f);
    CHECK(hues_apart(paleShown, on_screen(blue.pale, texture)) >= 60);
    CHECK(saturation(deepShown) > 0.95f && deepShown.g > deepShown.r + 0.5f);
    CHECK(saturation(deepShown) > saturation(paleShown) + 0.3f);
    // Nor like the warm and white-hot stops it passes through on the way.
    const Rgb warmShown = on_screen(warm, texture);
    const Rgb hotShown = on_screen(tint(warm, green.pale, green.deep, kFalseCap / 2), texture);
    CHECK(hues_apart(paleShown, warmShown) >= 60);
    CHECK(saturation(paleShown) > saturation(warmShown) + 0.15f && saturation(paleShown) > saturation(hotShown) + 0.3f);
  }
  // The same way there: orange, then white-hot, then green; never through pink.
  for (float b = 0; b <= 1.0001f; b += 0.01f) {
    const Rgb c = tint(warm, green.pale, green.deep, b);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));
  }
  const Rgb hot = tint(warm, green.pale, green.deep, kFalseCap / 2);
  CHECK(near(hot.r, hot.g) && near(hot.g, hot.b));
}

void the_hydras_light_turns_green() {
  const Rgb warm{1, 0.62f, 0.3f};
  const float b = brightness(warm);
  const FlameColours blue = light_colours(b, false), green = light_colours(b, true);
  CHECK(same(blue.pale, light_pale(b)) && same(blue.deep, light_deep(b)));  // other ghosts unchanged
  const Rgb half = tint(warm, green.pale, green.deep, kFalseCap);
  const Rgb full = tint(warm, green.pale, green.deep, 1);
  CHECK(saturation(half) < 0.5f && half.g > half.r && half.g > half.b && half.b >= half.r - 0.02f);
  CHECK(hues_apart(half, warm) >= 60 && hues_apart(half, light_pale(b)) >= 60);  // not warm, not the pale blue
  CHECK(saturation(full) > 0.95f && full.g > 3 * full.r && full.g > 3 * full.b);
  CHECK(near(brightness(full), b));
  // At the flame's own hue, as the blue light is.
  const Rgb flame = flame_colours(kFlameDeep, true).deep;
  CHECK(near(full.r / full.g, flame.r / flame.g, 0.001f) && near(full.b / full.g, flame.b / flame.g, 0.001f));
  for (float t = 0; t <= 1.0001f; t += 0.01f) {
    const Rgb c = tint(warm, green.pale, green.deep, t);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));  // never pink
  }
}

void messages_round_trip() {
  const std::vector<std::pair<int, float>> flames = {{1001, 0.0f}, {-7, 0.5f}, {2147483647, 1.0f}};
  const std::vector<uint8_t> bytes = encode(flames);
  std::vector<std::pair<int, float>> back;
  CHECK(decode(bytes.data(), bytes.size(), back));
  CHECK(back.size() == 3 && back[0].first == 1001 && back[0].second == 0);
  CHECK(back.size() == 3 && back[1].first == -7 && near(back[1].second, 0.5f));
  CHECK(back.size() == 3 && back[2].first == 2147483647 && back[2].second == 1);
  CHECK(!decode(bytes.data(), bytes.size() - 1, back));  // truncated
  CHECK(!decode(bytes.data(), 2, back));
  std::vector<uint8_t> wrongKind = bytes;
  wrongKind[0] = 9;
  CHECK(!decode(wrongKind.data(), wrongKind.size(), back));
  CHECK(encode({}).size() == 3 && decode(encode({}).data(), 3, back) && back.empty());
  CHECK(kMaxPerMessage * 5 + 3 <= 511);  // one message fits the host's limit
}

void a_false_flame_reads_the_same_on_every_client() {
  std::vector<std::pair<int, float>> back;
  CHECK(decode(encode({{1, kFalseCap}}).data(), 8, back) && back.size() == 1);
  CHECK(back.size() == 1 && back[0].second <= kFalseCap && gives_comfort(back[0].second));
  CHECK(decode(encode({{1, 1.0f}}).data(), 8, back) && back.size() == 1 && back[0].second == 1);
}

}  // namespace

int main() {
  rises_only_with_the_ghost_in_the_room();
  every_second_in_the_room_counts();
  full_blue_in_a_minute_faster_with_higher_tiers();
  halfway_in_a_minute_without_the_evidence();
  a_true_flame_turns_twice_as_fast();
  it_drops_back_to_yellow_at_half_the_speed_it_turns();
  a_false_flame_is_always_half_a_true_one();
  picking_it_up_or_putting_it_out_starts_over();
  comfort_and_burn();
  the_extra_blow_out_comes_only_at_full_blue();
  a_flame_ghost_cannot_blow_out_a_flame_until_it_is_fully_blue();
  once_fully_blue_it_may_be_blown_out_after_fading();
  inert_without_a_carrier();
  a_flame_goes_pale_at_half_and_deep_blue_at_full();
  a_lantern_goes_to_the_same_blue();
  a_light_goes_pale_at_half_and_deep_blue_at_full();
  the_hydras_flame_burns_green();
  the_hydras_light_turns_green();
  messages_round_trip();
  a_false_flame_reads_the_same_on_every_client();
  return g_failures ? 1 : 0;
}
