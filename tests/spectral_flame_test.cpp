// Offline tests for the Spectral Flame rules (mods/spectral_flame/flame_rules.h).
// Run: build\spectral_flame_test.exe
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "flame_rules.h"
#include "urp_display.h"

#include "check.h"

using namespace pg::flame;

namespace {

constexpr float kFrame = 1.0f / 30;
constexpr Rgb kCandleTint{10.681f, 3.593f, 1.461f};  // the game's "Candle flame" _TintColour
constexpr Rgb kLanternTint{42.723f, 12.273f, 3.426f}; // its "Lantern Flame" _Color_2
constexpr Rgb kLanternBase{0, 0.297f, 1.149f};        // and _Color_1

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

/** A flame fully blue, or as far as a false one goes: its ghost time and colour both at the top. */
State at(float blueness) { return {blueness, blueness, blueness >= 1}; }

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

// An 8-bit sRGB channel, as a texture stores it, in linear light, as the shader samples it.
float linear(uint8_t v) {
  const float c = v / 255.0f;
  return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// A flame tint (as set on its material) on screen, through the game's picture pipeline, over a grey
// sprite texel of linear brightness `grey` (its core about 0.85, its body 0.5, its rim 0.2), or over
// a texel of a coloured sprite, the shader multiplying the two channel by channel.
Rgb on_screen(Rgb tint, float grey, float ev = 0) { return urp::display(scaled(urp::linear_light(tint), grey), ev); }
Rgb on_screen(Rgb tint, const uint8_t* texel) {
  const Rgb t = urp::linear_light(tint);
  return urp::display({t.r * linear(texel[0]), t.g * linear(texel[1]), t.b * linear(texel[2])});
}

// A wall lit by a light of colour `light` (as set on the Light), `level` its intensity times the
// wall's albedo and falloff: a candle's light is dim, so the wall sits low on the tone curve.
Rgb wall_lit(Rgb light, float level) { return urp::display(scaled(urp::linear_light(light), level)); }

void rises_only_with_the_ghost_in_the_room() {
  CHECK(blue_after(in_room(), 40) > 0.1f);
  CHECK(blue_after(away(), 60) == 0);  // a ghost elsewhere never turns it blue
  CHECK(run(at(0.5f), away(), 10).blueness < 0.5f);
}

void every_second_in_the_room_counts() {
  CHECK(run({}, in_room(), 5).ghostTime > 0.05f);  // the ghost time builds as soon as the ghost is there
  CHECK(run({}, in_room(), 1).ghostTime < 0.02f);  // a ghost passing through changes almost nothing
  // Short visits add up: ten 10 s visits with 10 s away between them take a true flame well on.
  State s{};
  for (int i = 0; i < 10; ++i) s = run(run(s, in_room(), 10), away(), 10);
  CHECK(s.ghostTime > 0.8f && s.blueness > 0.6f);
}

void the_colour_comes_late() {
  // A true Tier 1: a quarter of the way to white-hot after half a minute, white-hot at about 42 s,
  // the pale blue at about 50 s and the deep blue at a minute: half its colours in the last ten seconds.
  CHECK(blue_after(in_room(), 30) < 0.07f);
  CHECK(near(blue_after(in_room(), 42.4f), kFalseCap / 2, 0.02f));
  CHECK(near(blue_after(in_room(), 50.5f), kFalseCap, 0.02f));
  CHECK(blue_after(in_room(), 50) < kFalseCap && blue_after(in_room(), 60.5f) > 0.99f);
  // Every tier the same way, in its own time.
  for (Kind kind : {Kind::Tier2, Kind::Tier3, Kind::Candelabra}) {
    const float build = build_seconds(kind);
    CHECK(near(blue_after(in_room(kind), build / 2), blue_after(in_room(), 30), 0.005f));
    CHECK(near(blue_after(in_room(kind), build * 0.84f), blue_after(in_room(), 60 * 0.84f), 0.005f));
  }
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
  CHECK(run(at(0.9f), in_room(Kind::Tier1, false), kFrame).blueness <= kFalseCap);
}

void a_true_flame_turns_twice_as_fast() {
  // Half a minute in, a true flame has the ghost time a false one ends up with, and shows twice its colour.
  CHECK(near(run({}, in_room(Kind::Tier1, true), 30).ghostTime, 0.5f));
  CHECK(near(run({}, in_room(Kind::Tier1, false), 30).ghostTime, 0.25f));
  CHECK(near(blue_after(in_room(Kind::Tier1, false), 30), blue_after(in_room(Kind::Tier1, true), 30) / 2, 0.001f));
}

void it_drops_back_to_yellow_at_half_the_speed_it_turns() {
  // Evenly, however late the colour came: fully blue to none in two minutes.
  CHECK(near(run(at(1), away(), 60).blueness, 0.5f));
  CHECK(near(run(at(1), away(), 30).blueness, 0.75f));
  CHECK(run(at(1), away(), 121).blueness == 0);
  CHECK(run(at(1), away(), 119).blueness > 0);
  CHECK(run(at(1), away(Kind::Tier3), 81).blueness == 0 && run(at(1), away(Kind::Tier3), 79).blueness > 0);
  // A false flame too: from its half to none in two minutes.
  CHECK(run(at(0.5f), away(Kind::Tier1, false), 121).blueness == 0);
  CHECK(run(at(0.5f), away(Kind::Tier1, false), 119).blueness > 0);
}

void the_colour_never_follows_the_ghost_in_and_out() {
  // Fully blue, the ghost steps out for five seconds and comes back: the flame dims by no more than
  // its fade, then holds while the ghost time catches up, rather than jumping back with the ghost.
  const State full = run({}, in_room(), 61);
  const State out = run(full, away(), 5);
  CHECK(out.blueness < 1 && out.blueness > 1 - 5.1f / 120);
  const State back = run(out, in_room(), 1);
  CHECK(near(back.blueness, out.blueness, 0.0001f));
  CHECK(run(back, in_room(), 5).blueness > 0.99f);
  // Nor halfway: the colour a flame shows never drops faster than its fade while the ghost comes and goes.
  State s = run({}, in_room(), 52);
  for (int i = 0; i < 20; ++i) {
    const State next = run(s, i % 2 ? in_room() : away(), 2);
    CHECK(next.blueness >= s.blueness - 2.1f / 120);
    s = next;
  }
}

void a_false_flame_is_always_half_a_true_one() {
  // Whatever the ghost does, until the true flame is fully blue.
  State yes{}, no{};
  for (int i = 0; i < 6; ++i) {
    yes = run(run(yes, in_room(Kind::Tier1, true), 9), away(Kind::Tier1, true), 4);
    no = run(run(no, in_room(Kind::Tier1, false), 9), away(Kind::Tier1, false), 4);
    CHECK(yes.blueness < 1 && near(no.blueness, yes.blueness / 2, 0.002f) && near(no.ghostTime, yes.ghostTime / 2, 0.002f));
  }
}

void picking_it_up_or_putting_it_out_starts_over() {
  const State fullBlue = at(1);
  Inputs held = in_room();
  held.placed = false;
  const State a = step(fullBlue, held, kFrame);
  CHECK(a.blueness == 0 && a.ghostTime == 0 && !a.full);
  Inputs out = in_room();
  out.lit = false;
  const State b = step(fullBlue, out, kFrame);
  CHECK(b.blueness == 0 && b.ghostTime == 0 && !b.full);
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

void the_model_shows_the_games_own_flames() {
  // The game's candle flame is white-hot at the core and warm at its rim; its "Gas Flame Blue" on its
  // grey sprite shows teal, as players saw it in the game.
  const uint8_t core[4] = {245, 237, 214, 232}, rim[4] = {164, 77, 10, 83};
  CHECK(saturation(on_screen(kCandleTint, core)) < 0.1f);
  CHECK(on_screen(kCandleTint, rim).r > on_screen(kCandleTint, rim).b + 0.1f);
  const float teal = hue(on_screen({0, 0.552f, 1}, 0.85f));
  CHECK(teal > 195 && teal < 215);
}

void a_flame_goes_pale_at_half_and_deep_blue_at_full() {
  const FlameColours blue = flame_colours(false);
  const Rgb warm = kCandleTint;
  CHECK(same(tint(warm, blue.pale, blue.deep, 0), warm));  // untouched at none
  CHECK(same(tint(warm, blue.pale, blue.deep, kFalseCap), blue.pale));
  CHECK(same(tint(warm, blue.pale, blue.deep, 1), blue.deep));
  for (float grey : {0.85f, 0.5f}) {
    // The false tell a light sky blue, the true flame a fully saturated deep blue.
    const Rgb pale = on_screen(blue.pale, grey);
    CHECK(hue(pale) > 212 && hue(pale) < 232 && saturation(pale) > 0.2f && saturation(pale) < 0.5f);
  }
  for (float grey : {0.85f, 0.5f, 0.2f}) {
    for (float ev : {-1.0f, 0.0f, 1.0f}) {
      // Deep blue core to rim, a stop darker or brighter too: never teal, never purple, never washed out.
      const Rgb deep = on_screen(blue.deep, grey, ev);
      CHECK(hue(deep) > 228 && hue(deep) < 242 && saturation(deep) > 0.95f);
    }
  }
  CHECK(on_screen(blue.deep, 0.5f).b > 0.55f && on_screen(blue.deep, 0.85f).b > 0.7f);  // bright enough for a flame
  // Hotter on the way: orange, then white, then blue; never through pink (blue and red over green).
  for (float b = 0; b <= 1.0001f; b += 0.01f) {
    const Rgb c = tint(warm, blue.pale, blue.deep, b);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));
  }
  const Rgb hot = tint(warm, blue.pale, blue.deep, kFalseCap / 2);
  CHECK(near(hot.r, hot.g) && near(hot.g, hot.b) && saturation(on_screen(hot, 0.5f)) < 0.02f);  // white-hot
}

void a_lantern_goes_through_the_same_colours() {
  // Both its colours, from the lantern's own: the same stops as a candle's, so the same on screen.
  for (bool green : {false, true}) {
    const FlameColours stops = flame_colours(green);
    for (Rgb own : {kLanternTint, kLanternBase}) {
      CHECK(same(tint(own, stops.pale, stops.deep, 0), own));
      CHECK(same(tint(own, stops.pale, stops.deep, kFalseCap), stops.pale));
      CHECK(same(tint(own, stops.pale, stops.deep, 1), stops.deep));
      const Rgb hot = tint(own, stops.pale, stops.deep, kFalseCap / 2);
      CHECK(near(hot.r, hot.g) && near(hot.g, hot.b));
    }
  }
}

void a_light_shows_its_flames_colour() {
  // A light keeps its own strongest channel (a Tier 1 or 2 candle's 0.86, the lantern's 1), and a wall
  // it lights shows its flame's own hue on screen at each stop, blue or green.
  for (bool green : {false, true}) {
    const FlameColours flame = flame_colours(green);
    for (float b : {0.86f, 1.0f}) {
      const FlameColours light = light_colours(b, green);
      CHECK(near(brightness(light.pale), b) && near(brightness(light.deep), b));
      for (float level : {0.1f, 0.4f}) {
        CHECK(hues_apart(wall_lit(light.pale, level), on_screen(flame.pale, 0.5f)) < 8);
        CHECK(hues_apart(wall_lit(light.deep, level), on_screen(flame.deep, 0.5f)) < 8);
        CHECK(saturation(wall_lit(light.deep, level)) > 0.95f);
      }
      const Rgb warm{b, 0.628f * b / 0.86f, 0.127f * b / 0.86f};  // the game's candle light
      CHECK(same(tint(warm, light.pale, light.deep, 0), warm));  // its own at none
      for (float t = 0; t <= 1.0001f; t += 0.01f) {
        const Rgb c = tint(warm, light.pale, light.deep, t);
        CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));  // never pink
      }
    }
  }
}

void the_hydras_flame_burns_green() {
  const FlameColours blue = flame_colours(false), green = flame_colours(true);
  CHECK(same(blue.pale, kFlamePale) && same(blue.deep, kFlameDeep));  // other ghosts unchanged
  CHECK(same(tint(kCandleTint, green.pale, green.deep, kFalseCap), green.pale));  // withheld evidence: pale green
  CHECK(same(tint(kCandleTint, green.pale, green.deep, 1), green.deep));
  for (float grey : {0.85f, 0.5f}) {
    // On screen the false tell is a mint green, as washed out as the pale blue and plainly not it, off
    // the warm side of the wheel (red no higher than blue) so it never passes for a lit candle; the
    // true flame a deep green, never lime, far more saturated than the pale one.
    const Rgb pale = on_screen(green.pale, grey), deep = on_screen(green.deep, grey);
    CHECK(hue(pale) > 135 && hue(pale) < 155 && pale.r <= pale.b);
    CHECK(saturation(pale) > 0.2f && saturation(pale) < 0.5f);
    CHECK(hues_apart(pale, on_screen(blue.pale, grey)) >= 60);
    CHECK(hue(deep) > 115 && hue(deep) < 135 && saturation(deep) > 0.95f);
    CHECK(saturation(deep) > saturation(pale) + 0.3f);
    const uint8_t rim[4] = {164, 77, 10, 83};
    CHECK(hues_apart(pale, on_screen(kCandleTint, rim)) >= 60);  // nor like a lit candle's warm rim
    for (float ev : {-1.0f, 1.0f}) {
      // A stop darker or brighter the deep green stays green, never lime, if paler at a bright core.
      const float h = hue(on_screen(green.deep, grey, ev));
      CHECK(h > 112 && h < 135);
    }
  }
  for (float b = 0; b <= 1.0001f; b += 0.01f) {
    const Rgb c = tint(kCandleTint, green.pale, green.deep, b);
    CHECK(!(c.r > c.g + 0.01f && c.b > c.g + 0.01f));  // never pink
  }
}

void a_grey_texel_keeps_its_brightness_and_alpha() {
  uint8_t orange[4] = {255, 140, 30, 200};
  to_grey(orange);
  CHECK(orange[0] == orange[1] && orange[1] == orange[2] && orange[3] == 200);  // grey, alpha kept
  CHECK(orange[0] > 30 && orange[0] < 255);  // between its darkest and brightest channels
  uint8_t white[4] = {255, 255, 255, 255}, black[4] = {0, 0, 0, 0};
  to_grey(white);
  to_grey(black);
  CHECK(white[0] == 255 && white[1] == 255 && white[2] == 255);
  CHECK(black[0] == 0 && black[1] == 0 && black[2] == 0);
}

void unity_takes_set_colours_to_linear_light() {
  CHECK(to_linear(0) == 0 && near(to_linear(0.5f), 0.214f, 0.001f) && near(to_linear(1), 1, 0.0001f));
  CHECK(near(to_linear(2), std::pow(2.0f, 2.2f), 0.001f));  // pow 2.2 above 1, as UnityPlayer's
  for (float v : {0.0f, 0.02f, 0.3f, 0.99f, 1.0f, 1.3f, 10.681f, 42.7f}) CHECK(near(to_gamma(to_linear(v)), v, 0.001f * std::max(v, 1.0f)));
}

std::vector<uint8_t> sprite_of(std::initializer_list<std::array<uint8_t, 4>> texels, int each = 1) {
  std::vector<uint8_t> out;
  for (const auto& t : texels)
    for (int i = 0; i < each; ++i) out.insert(out.end(), t.begin(), t.end());
  return out;
}

Rgb match_of(const std::vector<uint8_t>& sprite) { return grey_match(sprite.data(), sprite.size() / 4); }

void the_grey_sprite_starts_in_the_games_own_colours() {
  // A sprite of one colour: its grey copy times the match is that colour exactly.
  const std::array<uint8_t, 4> tip{241, 197, 43, 160};
  const Rgb k = match_of(sprite_of({tip}, 16));
  uint8_t grey[4] = {tip[0], tip[1], tip[2], tip[3]};
  to_grey(grey);
  CHECK(near(k.r * linear(grey[0]), linear(tip[0]), 0.001f));
  CHECK(near(k.g * linear(grey[1]), linear(tip[1]), 0.001f));
  CHECK(near(k.b * linear(grey[2]), linear(tip[2]), 0.001f));
  // A grey sprite needs nothing; clear texels count for nothing; no sprite at all changes nothing.
  CHECK(same(match_of(sprite_of({{90, 90, 90, 255}, {200, 200, 200, 120}}, 8)), Rgb{1, 1, 1}));
  CHECK(same(match_of(sprite_of({tip, tip, tip, tip, {0, 0, 255, 0}, {0, 0, 255, 0}})), k));
  CHECK(same(match_of({}), Rgb{1, 1, 1}) && same(match_of(sprite_of({{255, 0, 0, 0}})), Rgb{1, 1, 1}));
  // The game's own sprite, white at the core, yellow in the body, orange at the tip (sharedassets2
  // "Candle flame", sampled down its middle), matches warmer than grey: more red, less blue. On screen
  // its grey copy with the matched tint shows as the game's own flame did, within a few levels of
  // 255 from core to base; only its faint rim (a third opaque) shows a little warmer.
  const auto flame = sprite_of({{245, 237, 214, 232}, {250, 235, 202, 229}, {251, 229, 152, 211}, {241, 197, 43, 160},
                                {236, 187, 40, 154}, {209, 148, 32, 129}, {164, 77, 10, 83}});
  const Rgb m = match_of(flame);
  CHECK(m.r > 1 && m.b < 1 && m.r > m.g && m.g > m.b);
  const Rgb warm = grey_tint(kCandleTint, m);
  for (size_t i = 0; i + 4 < flame.size(); i += 4) {  // all but the rim
    uint8_t g[4] = {flame[i], flame[i + 1], flame[i + 2], flame[i + 3]};
    to_grey(g);
    const Rgb game = on_screen(kCandleTint, flame.data() + i), ours = on_screen(warm, linear(g[0]));
    CHECK(std::fabs(game.r - ours.r) < 5 / 255.0f && std::fabs(game.g - ours.g) < 5 / 255.0f && std::fabs(game.b - ours.b) < 5 / 255.0f);
  }
  CHECK(same(grey_tint(kCandleTint, {1, 1, 1}), kCandleTint));  // nothing to match: the game's own tint
}

void the_sprites_yellow_shows_blue_once_grey() {
  // Texels of the game's coloured sprite: the yellow-orange of its tip and of the band at its base.
  // Under the pale blue they show yellow-green, and under the deep blue they all but vanish, so the
  // flame looked like a tinted orange flame shrinking to its core. Grey, they take each stop's own hue.
  const uint8_t tip[4] = {241, 197, 43, 160}, base[4] = {236, 187, 40, 154};
  const FlameColours blue = flame_colours(false), green = flame_colours(true);
  for (const uint8_t* texel : {tip, base}) {
    uint8_t g[4] = {texel[0], texel[1], texel[2], texel[3]};
    to_grey(g);
    CHECK(hue(on_screen(blue.pale, texel)) < 150);                      // the problem
    CHECK(brightness(on_screen(blue.deep, texel)) < 0.2f);
    CHECK(hues_apart(on_screen(blue.pale, linear(g[0])), on_screen(blue.pale, 0.5f)) < 5);
    CHECK(hues_apart(on_screen(blue.deep, linear(g[0])), on_screen(blue.deep, 0.5f)) < 5);
    CHECK(brightness(on_screen(blue.deep, linear(g[0]))) > 0.55f);
    CHECK(hues_apart(on_screen(green.deep, linear(g[0])), on_screen(green.deep, 0.5f)) < 8);  // the Hydra's too
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
  the_colour_comes_late();
  full_blue_in_a_minute_faster_with_higher_tiers();
  halfway_in_a_minute_without_the_evidence();
  a_true_flame_turns_twice_as_fast();
  it_drops_back_to_yellow_at_half_the_speed_it_turns();
  the_colour_never_follows_the_ghost_in_and_out();
  a_false_flame_is_always_half_a_true_one();
  picking_it_up_or_putting_it_out_starts_over();
  comfort_and_burn();
  the_extra_blow_out_comes_only_at_full_blue();
  a_flame_ghost_cannot_blow_out_a_flame_until_it_is_fully_blue();
  once_fully_blue_it_may_be_blown_out_after_fading();
  inert_without_a_carrier();
  the_model_shows_the_games_own_flames();
  a_flame_goes_pale_at_half_and_deep_blue_at_full();
  a_lantern_goes_through_the_same_colours();
  a_light_shows_its_flames_colour();
  the_hydras_flame_burns_green();
  a_grey_texel_keeps_its_brightness_and_alpha();
  unity_takes_set_colours_to_linear_light();
  the_grey_sprite_starts_in_the_games_own_colours();
  the_sprites_yellow_shows_blue_once_grey();
  messages_round_trip();
  a_false_flame_reads_the_same_on_every_client();
  return g_failures ? 1 : 0;
}
