// Offline tests for the Squatter's rules (mods/custom_ghosts/squatter_rule.h).
// Run: build\squatter_test.exe
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <span>
#include <utility>
#include <string>
#include <vector>

#include "Animator.h"
#include "UnityAssets.h"
#include "UnityCloth.h"
#include "Scene.h"
#include "UnityObject.h"
#include "UnityRendering.h"
#include "fake_game.h"
#include "game_api.h"
#include "inertia.h"
#include "runtime.h"
#include "sheet_mesh.h"
#include "squatter_chase.h"
#include "squatter_fall.h"
#include "squatter_figure.h"
#include "squatter_net.h"
#include "squatter_rule.h"
#include "squatter_voice.h"
#include "wav.h"

#include "check.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};  // the count of hooked bodies running, which hooks.cpp keeps in the game
}

using namespace pm::custom::squatter;
namespace wav = pm::custom::wav;
namespace fs = std::filesystem;

namespace {

constexpr Phase kChaseStates[] = {Phase::Chasing, Phase::Searching, Phase::AtHidingPlace};
constexpr Phase kAllPhases[] = {Phase::Lurking,  Phase::Chasing,   Phase::Searching, Phase::AtHidingPlace,
                                Phase::LockedOut, Phase::Moping,   Phase::Excited,   Phase::Following};

// The facts of a frame that is not friendly, with `inside` living players in the house and nothing else going on.
Facts hostile(int inside = 1) {
  Facts f{};
  f.livingInside = inside;
  return f;
}

void lurks_until_it_sees_someone() {
  Facts f = hostile();
  CHECK(next_phase(Phase::Lurking, f) == Phase::Lurking);
  f.seesPlayer = true;
  CHECK(next_phase(Phase::Lurking, f) == Phase::Chasing);
  CHECK(first_phase(false) == Phase::Lurking);
}

void search_and_chase_alternate() {
  Facts f = hostile();
  f.tracksPlayer = true;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Chasing);
  f.tracksPlayer = false;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Searching);

  Facts seen = hostile();
  seen.seesPlayer = true;
  CHECK(next_phase(Phase::Searching, seen) == Phase::Chasing);
  Facts hiding = hostile();
  hiding.targetHiding = true;
  CHECK(next_phase(Phase::Searching, hiding) == Phase::AtHidingPlace);
  CHECK(next_phase(Phase::Searching, hostile()) == Phase::Searching);

  CHECK(next_phase(Phase::AtHidingPlace, hiding) == Phase::AtHidingPlace);
  CHECK(next_phase(Phase::AtHidingPlace, seen) == Phase::Chasing);
  CHECK(next_phase(Phase::AtHidingPlace, hostile()) == Phase::Searching);  // stepped out and out of sight
}

void door_locks_only_when_everyone_is_out() {
  for (const Phase p : kChaseStates) {
    CHECK(!doors_lock(p, hostile(1)));
    CHECK(!doors_lock(p, hostile(3)));
    CHECK(doors_lock(p, hostile(0)));
  }
  CHECK(!doors_lock(Phase::Lurking, hostile(0)));  // nobody was chased
}

void walking_back_in_keeps_the_door_open() {
  for (const Phase p : kChaseStates) {
    CHECK(!doors_lock(p, hostile(1)));
    CHECK(next_phase(p, hostile(1)) != Phase::LockedOut);
  }
  Facts f = hostile(1);
  f.seesPlayer = true;
  f.tracksPlayer = true;
  const Phase p = next_phase(Phase::Chasing, f);
  f.livingInside = 2;
  CHECK(!doors_lock(p, f));
  CHECK(next_phase(p, f) == Phase::Chasing);
}

void a_dropped_target_does_not_stop_the_door() {
  // The chased player disconnecting is only livingInside falling to 0.
  Facts f = hostile(1);
  f.seesPlayer = true;
  f.tracksPlayer = true;
  CHECK(next_phase(Phase::Chasing, f) == Phase::Chasing);
  f.seesPlayer = false;
  f.tracksPlayer = false;
  f.livingInside = 0;
  CHECK(doors_lock(Phase::Chasing, f));
  CHECK(next_phase(Phase::Chasing, f) == Phase::LockedOut);
}

void chase_ends_locked_out_when_everyone_is_out() {
  for (const Phase p : kChaseStates) {
    const Facts f = hostile(0);
    CHECK(next_phase(p, f) == Phase::LockedOut);
    CHECK(doors_lock(p, f));
  }
}

void locked_out_is_final() {
  for (int mask = 0; mask < 32; ++mask) {
    for (const int inside : {-1, 0, 1, 4}) {
      Facts f{};
      f.friendly = mask & 16;
      f.seesPlayer = mask & 1;
      f.tracksPlayer = mask & 2;
      f.targetHiding = mask & 4;
      f.actFinished = mask & 8;
      f.livingInside = inside;
      CHECK(next_phase(Phase::LockedOut, f) == Phase::LockedOut);
      CHECK(!doors_lock(Phase::LockedOut, f));
    }
  }
}

void a_hiding_target_is_safe() {
  for (const Phase p : kChaseStates) {
    Facts f = hostile();
    f.tracksPlayer = true;
    f.targetHiding = true;
    const Phase n = next_phase(p, f);
    CHECK(n == Phase::AtHidingPlace);
    CHECK(!may_kick(n, false, true, 0.5f, 1.0f));
    f.seesPlayer = true;
    CHECK(next_phase(p, f) == Phase::AtHidingPlace);
  }
}

// After kKickInterestMs of kicking at a hiding place it loses interest and goes back to lurking, where the ghost's own
// idling and wandering move it, so the player has a chance to get away (user, 2026-11-09). Everyone out still locks.
void it_loses_interest_at_a_hiding_place() {
  CHECK(kKickInterestMs == 10000);  // the user's 10 seconds
  Facts f = hostile();
  f.tracksPlayer = true;
  f.targetHiding = true;
  CHECK(next_phase(Phase::AtHidingPlace, f) == Phase::AtHidingPlace);
  f.lostInterest = true;
  CHECK(next_phase(Phase::AtHidingPlace, f) == Phase::Lurking);
  CHECK(next_phase(Phase::Chasing, f) == Phase::AtHidingPlace);  // only a wait at a hiding place runs out
  f.livingInside = 0;
  CHECK(next_phase(Phase::AtHidingPlace, f) == Phase::LockedOut);
  Interest i;
  CHECK(!i.lost(0) && !i.lost(1u << 30));  // never lost before its first kick
  i.kicked(1000);
  i.kicked(5000);  // a later kick never restarts the count
  CHECK(!i.lost(1000 + kKickInterestMs - 1));
  CHECK(i.lost(1000 + kKickInterestMs));
  CHECK(!i.lost(500));  // a time before the first kick
  i.reset();
  CHECK(!i.lost(1000 + kKickInterestMs));
  // Back to lurking, the mod lets go of the agent: the master idles the ghost once, so idling's start gives the agent
  // the ghost's own speed and its roll moves on to wandering.
  CHECK(steering_ends(true, Phase::Lurking));
  CHECK(!steering_ends(true, Phase::AtHidingPlace) && !steering_ends(false, Phase::Lurking));
  CHECK(!steering_ends(true, std::nullopt) && !steering_ends(false, std::nullopt));
}

// Its way into a hiding place (user, 2026-11-09: it never passes through a door): it walks in through a door that is
// not shut and already lets it through; else it opens the nearest door that is not shut, with the opening clip, and
// walks in once it lets it through; it never opens a shut door, so with no other way in it kicks at the doors.
void its_way_into_a_hiding_place() {
  using D = HidingDoor;
  CHECK(way_in({}).how == WayIn::Kick);                                         // no doors read
  CHECK(way_in(std::vector<D>{{true, false, true, 1.0f}}).how == WayIn::Kick);  // shut: never opened
  const std::vector<D> wide = {{true, false, true, 1.0f}, {false, true, false, 2.0f}};
  CHECK(way_in(wide).how == WayIn::Walk);
  const std::vector<D> ajar = {{false, false, true, 2.0f}, {false, false, true, 1.0f}, {true, false, true, 0.5f}};
  const Entry e = way_in(ajar);
  CHECK(e.how == WayIn::Open && e.door == 1);  // the nearest ajar door, never the shut one beside it
  const std::vector<D> stuck = {{false, false, false, 1.0f}};  // ajar but not one it may open: it kicks
  CHECK(way_in(stuck).how == WayIn::Kick);
  CHECK(waits_at_hiding_place(true, WayIn::Kick));
  CHECK(!waits_at_hiding_place(true, WayIn::Open) && !waits_at_hiding_place(true, WayIn::Walk));
  CHECK(!waits_at_hiding_place(false, WayIn::Kick));
}

void hearing_a_target_resumes_the_chase() {
  Facts heard = hostile();
  heard.tracksPlayer = true;
  CHECK(next_phase(Phase::Searching, heard) == Phase::Chasing);
  CHECK(next_phase(Phase::AtHidingPlace, heard) == Phase::Chasing);  // target out and tracked
}

void an_unknown_count_never_locks() {
  for (const Phase p : kChaseStates) {
    const Facts f = hostile(-1);
    CHECK(!doors_lock(p, f));
    CHECK(next_phase(p, f) == p);
  }
}

void the_move_clock_survives_bad_times() {
  CHECK(move_hold_seconds(Move::Roll, std::numeric_limits<float>::quiet_NaN()) == kMoveSlotSeconds);
  CHECK(move_hold_seconds(Move::Roll, std::numeric_limits<float>::infinity()) == kMoveSlotSeconds);
}

// A move that plays once holds for its whole clip, so the next move never cuts it off (user, 2026-11-09: the roll cut
// off early); a looping one holds a slot. Every state of the table has its clip's length.
void a_move_that_plays_once_holds_for_its_clip() {
  CHECK(plays_once(Move::Roll) && plays_once(Move::Stumble) && plays_once(Move::Parkour));
  CHECK(!plays_once(Move::Run) && !plays_once(Move::Walk) && !plays_once(Move::Stand));
  CHECK(move_hold_seconds(Move::Roll, 2.4f) == 2.4f);
  CHECK(move_hold_seconds(Move::Parkour, 0.2f) == kMoveSlotSeconds);  // a slot at least
  CHECK(move_hold_seconds(Move::Run, 0.7f) == kMoveSlotSeconds);
  for (const char* state : all_states()) CHECK(seconds_of(state) > 0.0f);
  CHECK(seconds_of("no such state") == 0.0f);
}

void friendly_never_chases_or_kicks() {
  CHECK(first_phase(true) == Phase::Moping);
  Facts f{};
  f.friendly = true;
  f.livingInside = 1;
  CHECK(next_phase(Phase::Moping, f) == Phase::Moping);
  f.seesPlayer = true;
  CHECK(next_phase(Phase::Moping, f) == Phase::Excited);
  CHECK(next_phase(Phase::Excited, f) == Phase::Excited);  // the get-up clip has not ended
  f.actFinished = true;
  CHECK(next_phase(Phase::Excited, f) == Phase::Following);

  // Under every combination of facts a friendly ghost stays among the friendly phases, whatever phase
  // it is handed but LockedOut, and never locks a door.
  for (int mask = 0; mask < 16; ++mask) {
    for (const int inside : {0, 1, 3}) {
      Facts g{};
      g.friendly = true;
      g.seesPlayer = mask & 1;
      g.tracksPlayer = mask & 2;
      g.targetHiding = mask & 4;
      g.actFinished = mask & 8;
      g.livingInside = inside;
      for (const Phase p : kAllPhases) {
        if (p == Phase::LockedOut) continue;  // final under every facts (locked_out_is_final)
        const Phase n = next_phase(p, g);
        CHECK(n == Phase::Moping || n == Phase::Excited || n == Phase::Following);
        CHECK(!doors_lock(p, g));
      }
    }
  }
  for (const Phase p : kAllPhases) CHECK(!may_kick(p, true, true, 0.0f, 1.0f));
}

void kick_needs_a_living_target_in_reach() {
  CHECK(!may_kick(Phase::Chasing, false, false, 0.0f, 1.0f));
  CHECK(!may_kick(Phase::Chasing, false, true, 1.5f, 1.0f));
  CHECK(may_kick(Phase::Chasing, false, true, 0.5f, 1.0f));
  CHECK(!may_kick(Phase::Lurking, false, true, 0.5f, 1.0f));
  CHECK(!may_kick(Phase::AtHidingPlace, false, true, 0.5f, 1.0f));  // it cannot open the door
  CHECK(!may_kick(Phase::LockedOut, false, true, 0.5f, 1.0f));
}

// With the clumsy moves coming only from the way ahead, it runs a little slower (user, 2026-11-09), about the old
// cycle's average: just under the sprint, never at it.
void speed_stays_just_under_sprint() {
  for (const float s : {2.0f, 3.0f, 4.0f}) {
    CHECK(move_speed(s, Move::Run) < 0.95f * s);
    CHECK(move_speed(s, Move::Run) > 0.8f * s);
    CHECK(move_speed(s, Move::Stumble) == 0.0f);
  }
}

void tired_taunt_fires_once_per_lapse() {
  const float cooled = kTauntCooldown + 1.0f;
  CHECK(tired_taunt_due(false, true, cooled));                   // the edge, after a cooldown
  CHECK(!tired_taunt_due(false, false, cooled));                 // still tired: no new edge
  CHECK(!tired_taunt_due(true, true, cooled));                   // can sprint
  CHECK(!tired_taunt_due(true, false, cooled));
  CHECK(!tired_taunt_due(false, true, kTauntCooldown - 1.0f));   // too soon after the last one
}

void lines_match_the_phase() {
  CHECK(lines_for(Phase::Lurking) == Lines::Lurk);
  CHECK(lines_for(Phase::Chasing) == Lines::Chase);
  CHECK(lines_for(Phase::Searching) == Lines::Search);
  CHECK(lines_for(Phase::AtHidingPlace) == Lines::Hiding);
  CHECK(lines_for(Phase::LockedOut) == Lines::Door);
  CHECK(lines_for(Phase::Moping) == Lines::Mope);
  CHECK(lines_for(Phase::Excited) == Lines::Excited);
  CHECK(lines_for(Phase::Following) == Lines::Friendly);
}

void picks_never_repeat_and_handle_nothing() {
  for (const uint32_t r : {0u, 1u, 7u, 0xFFFFFFFFu}) {
    CHECK(pick_line(0, -1, r) == -1);
    CHECK(pick_line(0, 3, r) == -1);
    CHECK(pick_line(1, 0, r) == 0);
    CHECK(pick_line(1, -1, r) == 0);
  }
  for (int count = 2; count <= 6; ++count) {
    for (int last = -1; last < count; ++last) {
      for (uint32_t r = 0; r < 40; ++r) {
        const int pick = pick_line(count, last, r);
        CHECK(pick >= 0 && pick < count);
        CHECK(pick != last);
      }
    }
  }
}

// ---- Recordings (wav.h, squatter_voice.h) ----

using Bytes = std::vector<uint8_t>;

void put32(Bytes& b, uint32_t v) {
  for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void put16(Bytes& b, uint16_t v) {
  b.push_back(static_cast<uint8_t>(v));
  b.push_back(static_cast<uint8_t>(v >> 8));
}
void tag(Bytes& b, const char* id) { b.insert(b.end(), id, id + 4); }

Bytes fmt_chunk(uint16_t format, uint16_t channels, uint32_t rate, uint16_t bits) {
  Bytes b;
  tag(b, "fmt ");
  put32(b, 16);
  put16(b, format);
  put16(b, channels);
  put32(b, rate);
  put32(b, rate * channels * bits / 8);
  put16(b, static_cast<uint16_t>(channels * bits / 8));
  put16(b, bits);
  return b;
}

// `claim` >= 0 sets the length the chunk states, whatever it holds.
Bytes data_chunk(const std::vector<int16_t>& s, int64_t claim = -1) {
  Bytes b;
  tag(b, "data");
  put32(b, claim < 0 ? static_cast<uint32_t>(s.size() * 2) : static_cast<uint32_t>(claim));
  for (int16_t v : s) put16(b, static_cast<uint16_t>(v));
  return b;
}

Bytes riff(const std::vector<Bytes>& chunks) {
  Bytes body;
  tag(body, "WAVE");
  for (const Bytes& c : chunks) body.insert(body.end(), c.begin(), c.end());
  Bytes b;
  tag(b, "RIFF");
  put32(b, static_cast<uint32_t>(body.size()));
  b.insert(b.end(), body.begin(), body.end());
  return b;
}

void wav_decodes_mono_and_stereo() {
  wav::Pcm pcm;
  CHECK(wav::decode(riff({fmt_chunk(1, 1, 22050, 16), data_chunk({0, 16384, -16384, 32767, -32768})}), &pcm));
  CHECK(pcm.channels == 1 && pcm.frequency == 22050 && pcm.frames() == 5);
  CHECK(pcm.samples.size() == 5 && pcm.samples[0] == 0.0f && pcm.samples[1] == 0.5f && pcm.samples[2] == -0.5f);
  CHECK(pcm.samples[3] == 32767.0f / 32768.0f && pcm.samples[4] == -1.0f);

  CHECK(wav::decode(riff({fmt_chunk(1, 2, 44100, 16), data_chunk({8192, -8192, 0, 16384})}), &pcm));
  CHECK(pcm.channels == 2 && pcm.frequency == 44100 && pcm.frames() == 2);
  CHECK(pcm.samples.size() == 4 && pcm.samples[0] == 0.25f && pcm.samples[1] == -0.25f && pcm.samples[3] == 0.5f);

  // Unknown chunks before fmt and between fmt and data are skipped, odd lengths padded to even.
  Bytes odd;
  tag(odd, "LIST");
  put32(odd, 3);
  odd.insert(odd.end(), {1, 2, 3, 0});
  Bytes junk;
  tag(junk, "junk");
  put32(junk, 0);
  CHECK(wav::decode(riff({odd, fmt_chunk(1, 1, 8000, 16), odd, junk, data_chunk({100, 200})}), &pcm));
  CHECK(pcm.frames() == 2 && pcm.frequency == 8000 && pcm.samples[1] == 200.0f / 32768.0f);

  // A partial frame at the end of the data is dropped.
  CHECK(wav::decode(riff({fmt_chunk(1, 2, 8000, 16), data_chunk({1, 2, 3})}), &pcm));
  CHECK(pcm.frames() == 1);
}

void wav_refuses_malformed() {
  wav::Pcm pcm;
  std::string why;
  const Bytes good = riff({fmt_chunk(1, 1, 8000, 16), data_chunk({1, 2, 3, 4})});
  CHECK(wav::decode(good, &pcm, &why));

  // Cut at every length: refused or decoded, never a crash or an out-of-range read.
  for (size_t n = 0; n < good.size(); ++n) {
    const Bytes cut(good.begin(), good.begin() + static_cast<std::ptrdiff_t>(n));
    pcm = wav::Pcm{};
    why.clear();
    const bool ok = wav::decode(cut, &pcm, &why);
    CHECK(!ok || pcm.frames() > 0);
    if (!ok) CHECK(pcm.samples.empty() && !why.empty());
  }
  CHECK(!wav::decode(Bytes(good.begin(), good.begin() + 20), &pcm));  // truncated header

  CHECK(!wav::decode({}, &pcm));
  Bytes notRiff = good;
  notRiff[0] = 'X';
  CHECK(!wav::decode(notRiff, &pcm));
  CHECK(!wav::decode(riff({fmt_chunk(3, 1, 8000, 32), data_chunk({1, 2})}), &pcm));  // float
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 8), data_chunk({1, 2})}), &pcm));   // 8-bit
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 24), data_chunk({1, 2})}), &pcm));  // 24-bit
  CHECK(!wav::decode(riff({fmt_chunk(1, 3, 8000, 16), data_chunk({1, 2})}), &pcm));  // three channels
  CHECK(!wav::decode(riff({fmt_chunk(1, 0, 8000, 16), data_chunk({1, 2})}), &pcm));  // no channels
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 0, 16), data_chunk({1, 2})}), &pcm));     // no rate
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 16), data_chunk({1, 2}, 400)}), &pcm));         // data past the end
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 16), data_chunk({1, 2}, 0xffffffffLL)}), &pcm));  // far past the end
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 16), data_chunk({}, 0)}), &pcm));               // no audio
  CHECK(!wav::decode(riff({fmt_chunk(1, 2, 8000, 16), data_chunk({7}, 2)}), &pcm));              // under one frame
  CHECK(!wav::decode(riff({data_chunk({1, 2}), fmt_chunk(1, 1, 8000, 16)}), &pcm));              // data before fmt
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 16)}), &pcm));                                  // no data
  CHECK(!wav::decode(riff({data_chunk({1, 2})}), &pcm));                                         // no fmt
  Bytes shortFmt;
  tag(shortFmt, "fmt ");
  put32(shortFmt, 8);
  shortFmt.resize(shortFmt.size() + 8);
  CHECK(!wav::decode(riff({shortFmt, data_chunk({1, 2})}), &pcm));
  Bytes bigFmt = fmt_chunk(1, 1, 8000, 16);
  bigFmt[4] = 0xff;  // fmt claims more than the file holds
  bigFmt[5] = 0xff;
  CHECK(!wav::decode(riff({bigFmt, data_chunk({1, 2})}), &pcm));
  Bytes hugeUnknown;  // an unknown chunk claiming 4 GB
  tag(hugeUnknown, "junk");
  put32(hugeUnknown, 0xffffffffu);
  CHECK(!wav::decode(riff({hugeUnknown, fmt_chunk(1, 1, 8000, 16), data_chunk({1, 2})}), &pcm));
  CHECK(!wav::decode(riff({fmt_chunk(1, 1, 8000, 16), hugeUnknown}), &pcm));
  CHECK(pcm.samples.empty());
}

void every_set_has_a_folder_name() {
  const char* expected[] = {"lurk", "chase",   "search",   "hiding", "catch", "door",
                            "tired", "mope", "excited", "friendly", "trip"};
  CHECK(kLinesValues == 11);
  std::set<std::string> seen;
  for (int i = 0; i < kLinesValues; ++i) {
    const char* name = voice::folder_name(static_cast<Lines>(i));
    CHECK(std::string(name) == expected[i]);
    seen.insert(name);
  }
  CHECK(seen.size() == 11);
}

// A trip says one of the user's trip lines as it falls (user, 2026-11-09: the trip folder); no other move speaks.
void a_trip_says_its_line() {
  CHECK(line_for_move(Move::Stumble) == Lines::Trip);
  for (int m = 0; m < kMoveValues; ++m)
    if (static_cast<Move>(m) != Move::Stumble) CHECK(!line_for_move(static_cast<Move>(m)));
}

void sets_list_their_files_sorted_by_name() {
  const fs::path dir = fs::temp_directory_path() / "pm_squatter_voice_test";
  fs::remove_all(dir);
  fs::create_directories(dir / "lurk");
  for (const char* n : {"b.wav", "a.wav", "C.WAV", "10.wav", "2.wav", "notes.txt"}) std::ofstream(dir / "lurk" / n).put('x');
  fs::create_directories(dir / "lurk" / "sub.wav");  // a folder is not a line
  std::vector<std::string> names;
  for (const auto& f : voice::list_wavs(dir / "lurk")) names.push_back(f.filename().string());
  const std::vector<std::string> sorted{"10.wav", "2.wav", "C.WAV", "a.wav", "b.wav"};
  CHECK(names == sorted);
  CHECK(voice::list_wavs(dir / "missing").empty());
  fs::remove_all(dir);
}

void missing_sets_stay_silent() {
  const fs::path dir = fs::temp_directory_path() / "pm_squatter_voice_empty";
  fs::remove_all(dir);
  fs::create_directories(dir / "chase");
  CHECK(!voice::load(dir));  // nothing to build
  for (int i = 0; i < 10; ++i) {
    const Lines set = static_cast<Lines>(i);
    CHECK(voice::count(set) == 0);
    CHECK(voice::play(set, {0, 0, 0}, 7) == -1);
    voice::play_index(set, 0, {0, 0, 0});
    voice::play_index(set, -1, {0, 0, 0});
    voice::play_index(set, 99, {0, 0, 0});
  }
  CHECK(!voice::load(dir / "no_such_folder"));

  // Files that cannot become clips keep their slots and stay silent, and nothing crashes.
  fs::create_directories(dir / "door");
  std::ofstream(dir / "door" / "a.wav") << "not a wav";
  const Bytes ok = riff({fmt_chunk(1, 1, 8000, 16), data_chunk({1, 2, 3})});
  std::ofstream(dir / "door" / "b.wav", std::ios::binary)
      .write(reinterpret_cast<const char*>(ok.data()), static_cast<std::streamsize>(ok.size()));
  voice::load(dir);
  CHECK(voice::count(Lines::Door) == 2);
  CHECK(voice::count(Lines::Lurk) == 0);
  for (uint32_t i = 0; i < 6; ++i) CHECK(voice::play(Lines::Door, {0, 0, 0}, i) >= 0);
  voice::play_index(Lines::Door, 0, {0, 0, 0});
  voice::play_index(Lines::Door, 2, {0, 0, 0});
  fs::remove_all(dir);
}

std::string read_text(const fs::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), {});
}

// Every file in a voice set's folder is a payload entry (tools/payload.psd1 lists files, not folders).
void every_voice_file_is_in_the_payload() {
  const fs::path root = PM_SOURCE_DIR;
  const std::string payload = read_text(root / "tools" / "payload.psd1");
  CHECK(!payload.empty());
  const std::string prefix = "mods/custom_ghosts/assets/squatter/voice/";
  for (int i = 0; i < kLinesValues; ++i) {
    const std::string set = voice::folder_name(static_cast<Lines>(i));
    const fs::path folder = root / "mods" / "custom_ghosts" / "assets" / "squatter" / "voice" / set;
    CHECK(fs::is_directory(folder));
    for (const auto& e : fs::directory_iterator(folder)) {
      const std::string name = e.path().filename().string();
      // The .gitkeep too: the launcher copies every file it finds, and the mod refuses to start without the voice folder,
      // which a release with no file in it would not have.
      CHECK(payload.find("Source = '" + prefix + set + "/" + name + "'") != std::string::npos);
    }
  }
}

// The mod loads the voice folder from its assets folder and a missing one stops its start (a mod with an error is off).
void the_mod_loads_its_voice_from_its_assets() {
  const std::string mod = read_text(fs::path(PM_SOURCE_DIR) / "mods" / "custom_ghosts" / "mod.cpp");
  CHECK(!mod.empty());
  CHECK(mod.find("SQUATTER_VOICE_TODO") == std::string::npos);
  CHECK(mod.find("squatter::voice::load(") != std::string::npos);
  CHECK(mod.find("pm::api::assets_folder()") != std::string::npos);
  CHECK(mod.find("\"squatter\" / \"voice\"") != std::string::npos);
}

// It may only idle, wander and go to its favourite room: every other GhostState (the game's numbers in build
// 25811160, 0 to 23) and any it adds later, or an unread one, is refused.
void only_idling_wandering_and_its_room_pass() {
  const QuietStates s{0, 1, 3};
  for (int state = -1; state <= 30; ++state)
    CHECK(keeps_quiet(s, state, false) == !(state == 0 || state == 1 || state == 3));
  const QuietStates unread{};
  CHECK(keeps_quiet(unread, -1, false) && keeps_quiet(unread, 0, false) && keeps_quiet(unread, 2, false));
}

// While the mod steers its agent, wandering and the favourite room would take the agent back (each sets its own
// destination, sdk/systems/ghost-ai.md "Moving the ghost outside a hunt"), so only idling passes; unread, nothing does.
void only_idling_passes_while_it_steers() {
  const QuietStates s{0, 1, 3};
  for (int state = -1; state <= 30; ++state) CHECK(keeps_quiet(s, state, true) == (state != 0));
  const QuietStates unread{};
  CHECK(keeps_quiet(unread, -1, true) && keeps_quiet(unread, 0, true));
}

// The mod steers the agent in every phase but Lurking, where the ghost's own idling, wandering and favourite room move
// it as they move any ghost (user, 2026-11-09); the master idles the ghost once as steering begins, and again only
// after it has stopped.
void steering_covers_every_phase_but_lurking() {
  for (int p = 0; p < kPhaseValues; ++p) CHECK(steers(static_cast<Phase>(p)) == (static_cast<Phase>(p) != Phase::Lurking));
  CHECK(steering_begins(false, Phase::Chasing) && steering_begins(false, Phase::Moping));
  CHECK(!steering_begins(false, Phase::Lurking));  // it lurks as a ghost moves, by its own states (user, 2026-11-09)
  CHECK(!steering_begins(true, Phase::Chasing) && !steering_begins(true, Phase::Lurking));
  CHECK(!steering_begins(false, std::nullopt) && !steering_begins(true, std::nullopt));
}

#include "squatter_chase_cases.inc"

// The Squatter's sheet (sheet_mesh.h). A cell is cut for an eye when it overlaps the eye's rectangle, so a hole is the
// rectangle rounded out to whole cells.
namespace sh = pm::custom::sheet;

bool overlaps_an_eye(const sh::SheetShape& s, float x0, float x1, float z0, float z1) {
  for (const float side : {-1.0f, 1.0f}) {
    const float x = side * s.eyeAcross;
    if (x0 < x + s.eyeWidth / 2 && x1 > x - s.eyeWidth / 2 && z0 < s.eyeForward + s.eyeHeight / 2 &&
        z1 > s.eyeForward - s.eyeHeight / 2)
      return true;
  }
  return false;
}

// How far `p` lies out on its eye's oval: 1 on the oval, under 1 inside it.
float eye_oval(const sh::SheetShape& s, sh::Vec2 p) {
  const float x = (p.x < 0 ? -1.0f : 1.0f) * s.eyeAcross;
  const float u = (p.x - x) / (s.eyeWidth / 2), v = (p.y - s.eyeForward) / (s.eyeHeight / 2);
  return std::sqrt(u * u + v * v);
}

bool inside_an_eye(const sh::SheetShape& s, sh::Vec2 p) { return eye_oval(s, p) < 1.0f - 1e-4f; }

// The cell (column, row) of the axis a point lies in, or -1 for a coordinate on no cell.
std::pair<int, int> cell_of(const std::vector<float>& axis, sh::Vec2 p) {
  const auto at = [&](float v) {
    for (size_t i = 0; i + 1 < axis.size(); ++i)
      if (v > axis[i] && v < axis[i + 1]) return static_cast<int>(i);
    return -1;
  };
  return {at(p.x), at(p.y)};
}

sh::Vec2 centroid(const sh::SheetMesh& m, size_t triangle) {
  sh::Vec2 c{0, 0};
  for (size_t k = 0; k < 3; ++k) {
    const sh::Vec2 p = m.flat[static_cast<size_t>(m.triangles[triangle * 3 + k])];
    c.x += p.x / 3;
    c.y += p.y / 3;
  }
  return c;
}

// Unity's cloth simulates a particle per point and, with virtual particles, one per triangle, so the sheet's grid is
// kept inside a point budget even for the widest shape the figure cuts it (a tall body's drop to the floor): the cost of
// the cloth a client runs is what that grid is paid in (user, 2026-11-10: getting up lagged badly).
void the_sheet_stays_inside_its_point_budget() {
  sh::SheetShape wide;
  wide.shoulderRadius = 0.42f;
  wide.side = 4.4f;  // as wide as the figure makes it, for a tall body
  const sh::SheetMesh m = sh::make_sheet(wide);
  CHECK(m.vertices.size() <= 6000);
  CHECK(m.triangles.size() <= 2 * m.vertices.size() * 3);
  // And the grid is no finer than it says: fine only within fineHalf of the centre, where the eyes are cut, and no step
  // wider than coarseSpacing beyond it.
  const std::vector<float> ax = sh::sheet_axis(wide);
  for (size_t i = 1; i < ax.size(); ++i) {
    const float step = ax[i] - ax[i - 1];
    const bool fine = std::fabs(ax[i]) <= wide.fineHalf + 1e-4f && std::fabs(ax[i - 1]) <= wide.fineHalf + 1e-4f;
    CHECK(step <= (fine ? wide.fineSpacing : wide.coarseSpacing) + 1e-4f);
  }
}

void sheet_is_closed_grid_with_eye_holes() {
  const sh::SheetShape shape;
  const sh::SheetMesh m = sh::make_sheet(shape);
  const std::vector<float> axis = sh::sheet_axis(shape);
  CHECK(axis.size() >= 3);
  if (axis.size() < 3) return;
  CHECK(std::abs(axis.front() + shape.side / 2) < 1e-5f && std::abs(axis.back() - shape.side / 2) < 1e-5f);
  CHECK(std::find(axis.begin(), axis.end(), 0.0f) != axis.end());
  for (size_t i = 0; i < axis.size(); ++i) {
    CHECK(axis[i] == -axis[axis.size() - 1 - i]);  // symmetric about the centre, exactly
    if (i) CHECK(axis[i] > axis[i - 1]);
  }
  const size_t cells = axis.size() - 1;
  size_t cut = 0;
  size_t round = 0;  // the cells of the round sheet: those whose middle lies within its radius
  for (size_t i = 0; i < cells; ++i)
    for (size_t j = 0; j < cells; ++j) {
      const float x = (axis[i] + axis[i + 1]) / 2, z = (axis[j] + axis[j + 1]) / 2;
      if (std::hypot(x, z) > shape.side / 2) continue;
      ++round;
      if (overlaps_an_eye(shape, axis[i], axis[i + 1], axis[j], axis[j + 1])) ++cut;
    }
  CHECK(cut > 0 && round < cells * cells);
  CHECK(m.triangles.size() == 6 * (round - cut));
  const size_t n = m.vertices.size();
  CHECK(n > 0 && m.normals.size() == n && m.uv.size() == n && m.flat.size() == n && m.coefficients.size() == n);
  if (n == 0 || m.flat.size() != n || m.uv.size() != n) return;
  std::vector<bool> used(n, false);
  bool inRange = true, clearOfEyes = true;
  for (const int32_t index : m.triangles) {
    if (index < 0 || static_cast<size_t>(index) >= n) {
      inRange = false;
      continue;
    }
    used[static_cast<size_t>(index)] = true;
    if (inside_an_eye(shape, m.flat[static_cast<size_t>(index)])) clearOfEyes = false;
  }
  CHECK(inRange);
  CHECK(clearOfEyes);
  CHECK(std::count(used.begin(), used.end(), false) == 0);  // no vertex left over in a hole
  // Each vertex lies on the grid but an eye's rim, which lies on the eye's oval (user, 2026-11-09: oval eye holes,
  // not rectangles), eight to an eye, the two at its ends on the grid too; and its uv is its place on the sheet
  // from 0 to 1.
  size_t rim = 0;
  for (size_t v = 0; v < n; ++v) {
    const bool onGrid = std::find(axis.begin(), axis.end(), m.flat[v].x) != axis.end() &&
                        std::find(axis.begin(), axis.end(), m.flat[v].y) != axis.end();
    const bool onOval = std::abs(eye_oval(shape, m.flat[v]) - 1.0f) < 1e-4f;
    CHECK(onGrid || onOval);
    if (onOval) ++rim;
    CHECK(std::abs(m.uv[v].x - (m.flat[v].x / shape.side + 0.5f)) < 1e-5f);
    CHECK(std::abs(m.uv[v].y - (m.flat[v].y / shape.side + 0.5f)) < 1e-5f);
  }
  CHECK(rim == 16);
}

void sheet_pins_only_the_top() {
  const sh::SheetShape shape;
  const sh::SheetMesh m = sh::make_sheet(shape);
  CHECK(!m.coefficients.empty() && m.coefficients.size() == m.flat.size());
  if (m.coefficients.empty() || m.coefficients.size() != m.flat.size()) return;
  size_t pinned = 0, free = 0;
  for (size_t v = 0; v < m.flat.size(); ++v) {
    if (std::hypot(m.flat[v].x, m.flat[v].y) <= shape.pinRadius) {
      CHECK(m.coefficients[v].maxDistance == 0.0f);
      ++pinned;
    } else {
      CHECK(m.coefficients[v].maxDistance > 0.0f);
      ++free;
    }
    CHECK(m.coefficients[v].collisionSphereDistance >= 0.0f);
  }
  CHECK(pinned > 0 && free > pinned);
}

// As worn cloth is set up in games (user, 2026-11-09: copy how other games do it): each point may move only a little
// from where the body carries it, nothing at the crown and growing toward the hem to kHemSway, with a backstop, so the
// sheet keeps its hanging shape and only sways.
void sheet_sways_only_near_its_shape() {
  const sh::SheetShape shape;
  const sh::SheetMesh m = sh::make_sheet(shape);
  float far = 0.0f;
  for (const sh::Vec2& f : m.flat) far = std::max(far, std::hypot(f.x, f.y));
  float nearSway = -1.0f, farSway = -1.0f, lastDistance = -1.0f, lastSway = -1.0f;
  bool growing = true;
  std::vector<std::pair<float, float>> byDistance;
  for (size_t v = 0; v < m.flat.size(); ++v) {
    const float d = std::hypot(m.flat[v].x, m.flat[v].y);
    const float sway = m.coefficients[v].maxDistance;
    CHECK(sway >= 0.0f && sway <= sh::kHemSway + 1e-5f);
    CHECK(m.coefficients[v].collisionSphereDistance == sh::kBackstop);
    byDistance.push_back({d, sway});
    if (d == far) farSway = sway;
    if (d <= shape.pinRadius) nearSway = sway;
  }
  std::sort(byDistance.begin(), byDistance.end());
  for (const auto& [d, sway] : byDistance) {
    if (d > lastDistance + 1e-4f && sway < lastSway - 1e-5f) growing = false;
    lastDistance = d;
    lastSway = sway;
  }
  CHECK(nearSway == 0.0f && std::fabs(farSway - sh::kHemSway) < 1e-5f && growing);
}

// Each point of the sheet follows the bones nearest it, as worn clothing is skinned in games (user, 2026-11-09: the
// arms and legs came out of the sheet): over an arm the arm carries it, between two bones both do, and the four that
// pull most carry it, their weights adding up to 1.
void sheet_follows_the_bones_nearest_it() {
  const sh::Segment bones[] = {{{0, -0.2f, 0}, {0, 0, 0}},               // a spine, under the crown
                               {{0.3f, -0.4f, 0}, {0.3f, -0.9f, 0}},     // an arm out to the right
                               {{-0.1f, -1.0f, 0}, {-0.1f, -1.8f, 0}},   // a leg
                               {{0.1f, -1.0f, 0}, {0.1f, -1.8f, 0}},     // the other leg
                               {{-0.3f, -0.4f, 0}, {-0.3f, -0.9f, 0}}};  // the other arm
  CHECK(sh::distance_to({0.3f, -0.6f, 0.1f}, bones[1]) - 0.1f < 1e-5f);
  CHECK(std::fabs(sh::distance_to({0, 0.5f, 0}, bones[0]) - 0.5f) < 1e-5f);  // past its end: from the end
  sh::Weight w = sh::weight_by_nearness({0.32f, -0.6f, 0.05f}, bones);  // on the right arm
  CHECK(w.bone[0] == 1 && w.weight[0] > 0.8f);
  float sum = 0;
  for (const float x : w.weight) sum += x;
  CHECK(std::fabs(sum - 1.0f) < 1e-5f);
  for (int k = 1; k < 4; ++k) CHECK(w.weight[k] <= w.weight[k - 1]);
  w = sh::weight_by_nearness({0, -1.4f, 0.1f}, bones);  // between the legs: both, alike
  CHECK(((w.bone[0] == 2 && w.bone[1] == 3) || (w.bone[0] == 3 && w.bone[1] == 2)) &&
        std::fabs(w.weight[0] - w.weight[1]) < 1e-4f);
  const sh::Weight none = sh::weight_by_nearness({0, 0, 0}, {});
  CHECK(none.bone[0] == 0 && none.weight[0] == 1.0f && none.weight[1] == 0.0f);  // no bones read: bone 0 alone
}

// The sheet reaches the floor (user, 2026-11-09: make it larger so it reaches the floor): a sheet as wide as
// reach_distance gives for a drop hangs its hem that far below the crown, flaring a little out from the shoulders, and
// its hem is round, every rim point hanging at the same height.
void sheet_reaches_the_floor() {
  sh::SheetShape shape;
  const float drop = 1.8f;
  shape.side = 2.0f * sh::reach_distance(shape, drop);
  CHECK(sh::reach_distance(shape, shape.headRadius) == 0.0f);  // above the shoulders: no length
  const sh::SheetMesh m = sh::make_sheet(shape);
  float lowest = 0, widest = 0;
  for (const sh::Vec3& v : m.vertices) {
    lowest = std::min(lowest, v.y);
    widest = std::max(widest, std::hypot(v.x, v.z));
  }
  CHECK(std::fabs(lowest + drop) < 1e-3f);
  CHECK(widest > shape.shoulderRadius && widest < shape.shoulderRadius + shape.flare * drop + 1e-3f);
  size_t rim = 0;
  for (size_t v = 0; v < m.vertices.size(); ++v)
    if (std::hypot(m.flat[v].x, m.flat[v].y) >= shape.side / 2) {
      ++rim;
      CHECK(std::fabs(m.vertices[v].y + drop) < 1e-3f);
    }
  CHECK(rim > 8);
}

void eyes_are_mirrored() {
  const sh::SheetShape shape;
  const sh::SheetMesh m = sh::make_sheet(shape);
  const std::vector<float> axis = sh::sheet_axis(shape);
  CHECK(axis.size() >= 3 && !m.flat.empty());
  if (axis.size() < 3 || m.flat.empty()) return;
  const int cells = static_cast<int>(axis.size()) - 1;
  std::set<std::pair<int, int>> present;
  for (size_t t = 0; t < m.triangles.size() / 3; ++t) present.insert(cell_of(axis, centroid(m, t)));
  CHECK(!present.contains({-1, -1}));
  std::vector<std::pair<int, int>> holes;
  for (int i = 0; i < cells; ++i)
    for (int j = 0; j < cells; ++j) {
      const size_t x = static_cast<size_t>(i), z = static_cast<size_t>(j);
      if (!sh::in_sheet(shape, axis[x], axis[x + 1], axis[z], axis[z + 1])) continue;  // beyond the round edge
      if (!present.contains({i, j})) holes.emplace_back(i, j);
    }
  CHECK(!holes.empty());
  bool left = false, right = false;
  for (const auto& [i, j] : holes) {
    CHECK(!present.contains({cells - 1 - i, j}));  // its mirror across the centre line is a hole too
    const float x = (axis[static_cast<size_t>(i)] + axis[static_cast<size_t>(i) + 1]) / 2;
    const float z = (axis[static_cast<size_t>(j)] + axis[static_cast<size_t>(j) + 1]) / 2;
    left = left || x < 0;
    right = right || x > 0;
    CHECK(z > 0);  // in front of the crown
  }
  CHECK(left && right);
  // The sheet stays whole between the eyes: the cells either side of the centre line at eye height are kept.
  const int eyeRow = cell_of(axis, {0.001f, shape.eyeForward + 0.001f}).second;  // off the grid line
  CHECK(eyeRow >= 0);
  CHECK(present.contains({cells / 2 - 1, eyeRow}) && present.contains({cells / 2, eyeRow}));
}

// The sheet is made already hanging over a head and shoulders, the crown at its origin, so the cloth does not fall
// into place from a flat square: the further a point is from the centre on the flat sheet, the lower it hangs.
void sheet_hangs_from_the_crown() {
  const sh::SheetShape shape;
  const sh::SheetMesh m = sh::make_sheet(shape);
  CHECK(!m.vertices.empty() && m.flat.size() == m.vertices.size() && m.normals.size() == m.vertices.size());
  if (m.vertices.empty() || m.flat.size() != m.vertices.size() || m.normals.size() != m.vertices.size()) return;
  std::vector<std::pair<float, float>> byDistance;  // (flat distance, height)
  for (size_t v = 0; v < m.vertices.size(); ++v) {
    CHECK(m.vertices[v].y <= 1e-5f);
    const sh::Vec3 n = m.normals[v];
    CHECK(std::abs(std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z) - 1.0f) < 1e-3f);
    byDistance.emplace_back(std::hypot(m.flat[v].x, m.flat[v].y), m.vertices[v].y);
  }
  std::sort(byDistance.begin(), byDistance.end());
  for (size_t k = 1; k < byDistance.size(); ++k) CHECK(byDistance[k].second <= byDistance[k - 1].second + 1e-5f);
  CHECK(byDistance.front().first == 0.0f && std::abs(byDistance.front().second) < 1e-6f);
  CHECK(byDistance.back().second < -(shape.headRadius + shape.shoulderDrop));
}

// The figure's state table (squatter_figure.h): the names the bundle's controller must have, exactly.
// A trip picks one of two at random (user, 2026-11-09): Soccer Trip or Tripping. Each loses its footing partway
// through its own clip (kFooting), where its ragdoll takes over, and each get-up stands alone, played from where the
// body lands: the trips no longer run on into them.
void the_get_ups_stand_alone() {
  CHECK(states_of(Move::Stumble).size() == 2);
  CHECK(seconds_of("Stumble") == 1.6f && seconds_of("Trip") == 2.1f);
  CHECK(kFooting[0] > 0.0f && kFooting[0] < seconds_of("Stumble"));
  CHECK(kFooting[1] > 0.0f && kFooting[1] < seconds_of("Trip"));
  const std::vector<const char*> all = all_states();
  CHECK(std::count_if(all.begin(), all.end(), [](const char* x) { return std::string(x) == kStandUpState; }) == 1);
  CHECK(std::count_if(all.begin(), all.end(), [](const char* x) { return std::string(x) == kTripGetUpState; }) == 1);
  CHECK(seconds_of(kStandUpState) > 1.0f && seconds_of(kTripGetUpState) > 1.0f);  // get-ups, not pops
}

void every_move_and_act_has_its_states() {
  const std::vector<const char*> all = all_states();
  std::set<std::string> names;
  for (const char* s : all) names.insert(s);
  CHECK(names.size() == all.size());  // each name once
  CHECK((names == std::set<std::string>{"Run", "Duck_A", "Duck_B", "Weave_A", "Weave_B", "Roll_A", "Roll_B",
                                        "Parkour_A", "Stumble", "Sit", "GetUp", "Kick", "Bang", "Bump",
                                        "OpenDoorOutwards", "OpenDoorInwards", "Walk", "Stand", "Sneak", "TurnLeft",
                                        "TurnRight", "Turn180", "StartWalk", "StandUp", "Trip", "TripGetUp"}));
  for (const Move m : kAllMoves) {
    const auto states = states_of(m);
    const bool two = m == Move::Duck || m == Move::Weave || m == Move::Roll || m == Move::Stumble;
    CHECK(states.size() == (two ? 2u : 1u));
  }
  CHECK(std::string(state_of(Act::Bang)) == "Bang" && std::string(state_of(Act::OpenDoorInwards)) == "OpenDoorInwards");
  for (const Act a : kAllActs) CHECK(names.contains(state_of(a)));
}

// The master sends a variant and every client turns it into the same state.
void move_variants_are_deterministic() {
  CHECK(std::string(state_for(Move::Duck, 0)) == "Duck_A" && std::string(state_for(Move::Duck, 1)) == "Duck_B");
  CHECK(std::string(state_for(Move::Parkour, 7)) == "Parkour_A");  // one state: the jump runs on into the landing
  for (const Move m : kAllMoves)
    for (uint32_t v : {0u, 1u, 2u, 3u, 1000001u, 0xFFFFFFFFu}) {  // pm:not-game-data: variants
      CHECK(std::string(state_for(m, v)) == state_for(m, v + static_cast<uint32_t>(states_of(m).size())));
      CHECK(std::string(state_for(m, v)) == states_of(m)[v % states_of(m).size()]);
    }
  for (uint32_t v = 0; v < 8; ++v) CHECK(std::string(state_for(Move::Run, v)) == "Run");
}

// What the master shows every client (squatter_net.h): each field comes back as it went, for every phase, move, line
// set and act, and with no line or act at all.
void shown_round_trips() {
  for (const Phase p : kAllPhases)
    for (const Move m : kAllMoves) {
      Shown s;
      s.phase = p;
      s.move = m;
      s.variant = 3;
      s.lineSet = static_cast<int8_t>(Lines::Chase);
      s.lineIndex = 7;
      s.lineCue = 250;
      s.act = static_cast<int8_t>(Act::Bang);
      s.actCue = 9;
      s.lookAt = 200;  // the player it is after, by actor number
      const std::vector<uint8_t> bytes = shown_message(s);
      Shown back;
      CHECK(read_shown(bytes.data(), bytes.size(), back) && back == s);
      CHECK(pm::custom::net_msg::kind_of(bytes.data(), bytes.size()) == pm::custom::net_msg::kSquatter);
    }
  for (const Act a : kAllActs) {
    Shown s;
    s.act = static_cast<int8_t>(a);
    const std::vector<uint8_t> bytes = shown_message(s);
    Shown back;
    CHECK(read_shown(bytes.data(), bytes.size(), back) && back == s);
  }
  const Shown none;  // no line and no act
  CHECK(none.lineSet == -1 && none.lineIndex == -1 && none.act == -1);
  const std::vector<uint8_t> bytes = shown_message(none);
  Shown back;
  back.phase = Phase::Following;
  CHECK(read_shown(bytes.data(), bytes.size(), back) && back == none);
}

// A Shown is taken only whole, in range and from the master: a short or long message, a value no client knows, and one
// any other player sent are dropped, and the last good one is kept.
// A fall goes to every client in the Shown: the move, which way it is pushed and what knocked it over. A cause no
// client knows is dropped; a heading survives its byte to within half a step.
void a_fall_is_shared() {
  for (int cause = 0; cause < kFallCauseValues; ++cause) {
    Shown s;
    s.phase = Phase::Chasing;
    s.move = Move::Fall;
    s.fallYaw = 77;
    s.fallCause = static_cast<FallCause>(cause);
    const std::vector<uint8_t> bytes = shown_message(s);
    Shown back;
    CHECK(read_shown(bytes.data(), bytes.size(), back) && back == s);
  }
  Shown s;
  s.move = Move::Fall;
  std::vector<uint8_t> bytes = shown_message(s);
  bytes.back() = static_cast<uint8_t>(kFallCauseValues);  // the cause is the last byte
  Shown out;
  CHECK(!read_shown(bytes.data(), bytes.size(), out));
  const float step = 2.0f * std::numbers::pi_v<float> / 256.0f;
  for (float yaw = -3.1f; yaw < 3.1f; yaw += 0.05f)
    CHECK(std::abs(wrap_angle(byte_yaw(yaw_byte(yaw)) - yaw)) <= step / 2 + 1e-4f);
}

void bad_shown_is_dropped() {
  Shown s;
  s.phase = Phase::Chasing;
  s.lineSet = static_cast<int8_t>(Lines::Chase);
  s.lineIndex = 2;
  const std::vector<uint8_t> good = shown_message(s);
  Shown out;
  for (size_t n = 0; n < good.size(); ++n) CHECK(!read_shown(good.data(), n, out));
  std::vector<uint8_t> longer = good;
  longer.push_back(0);
  CHECK(!read_shown(longer.data(), longer.size(), out));
  CHECK(!read_shown(nullptr, good.size(), out));
  const auto broken = [&](size_t at, uint8_t value) {
    std::vector<uint8_t> b = good;
    b[at] = value;
    Shown o;
    return !read_shown(b.data(), b.size(), o);
  };
  CHECK(broken(4, 8));      // no phase 8
  CHECK(broken(5, static_cast<uint8_t>(kMoveValues)));  // no move past the last
  CHECK(broken(7, static_cast<uint8_t>(kLinesValues)));  // no line set past the last
  CHECK(broken(7, 0xFE));   // a set below -1
  CHECK(broken(8, 0xFE));   // a line below -1
  CHECK(broken(10, 7));     // no act 7
  CHECK(broken(3, pm::custom::net_msg::kSquatterTry));  // another kind
  {
    std::vector<uint8_t> b = good;
    b[7] = 0xFF;  // a line with no set
    Shown o;
    CHECK(!read_shown(b.data(), b.size(), o));
  }
  // Only the master's word counts: not another player's, nor any while the master is unknown (0).
  ShownInbox inbox;
  CHECK(!inbox.take(3, 1, good.data(), good.size()));
  CHECK(!inbox.take(0, 0, good.data(), good.size()));
  CHECK(!inbox.last);
  CHECK(inbox.take(1, 1, good.data(), good.size()) && inbox.last && *inbox.last == s);
  Shown later = s;
  later.phase = Phase::Searching;
  const std::vector<uint8_t> forged = shown_message(later);
  CHECK(!inbox.take(2, 1, forged.data(), forged.size()));
  CHECK(!inbox.take(1, 1, good.data(), 5));
  CHECK(*inbox.last == s);  // the master's last good word stands
}

// A player's try at a locked exit reaches the master once per input: the interact and grab paths of one input, within
// the window, are one try, and a later input is another.
void one_try_is_sent_per_input() {
  TryTeller teller;
  CHECK(teller.due(1000));
  CHECK(!teller.due(1000));                     // the grab path of the same input
  CHECK(!teller.due(1000 + kTryWindowMs - 1));  // still within the window
  CHECK(teller.due(1000 + kTryWindowMs));       // the next input
  teller = TryTeller{};
  CHECK(teller.due(0));  // a clock at 0 still sends the first
  const std::vector<uint8_t> message = try_message();
  CHECK(message.size() == 4 && pm::custom::net_msg::kind_of(message.data(), 4) == pm::custom::net_msg::kSquatterTry);
}

// The master takes another player's try, and dedupes a double report from one player too; each player's tries are
// their own, and taking them empties the queue.
void a_non_masters_try_reaches_the_master() {
  const std::vector<uint8_t> message = try_message();
  const std::vector<int> room{1, 2, 3};
  CHECK(try_from(2, true, room, message.data(), message.size()));
  TryInbox inbox;
  CHECK(inbox.report(2, 5000));
  CHECK(!inbox.report(2, 5000 + kTryWindowMs - 1));
  CHECK(inbox.report(3, 5001));
  CHECK(inbox.report(2, 5000 + kTryWindowMs));
  CHECK((inbox.take() == std::vector<int>{2, 3, 2}));
  CHECK(inbox.take().empty());
  CHECK(inbox.report(0, 6000));  // the master's own player with no actor number (single player)
  CHECK((inbox.take() == std::vector<int>{0}));
}

// The sender is Photon's, from the event, never from the message, so a try cannot speak for another player: one with
// a body (naming someone) is dropped, as is a sender that is not in the room or unknown (0), and any try that reaches a
// client that is not the master.
void a_forged_try_is_ignored() {
  const std::vector<uint8_t> message = try_message();
  const std::vector<int> room{1, 2, 3};
  std::vector<uint8_t> naming = message;
  naming.insert(naming.end(), {3, 0, 0, 0});
  CHECK(!try_from(2, true, room, naming.data(), naming.size()));
  CHECK(!try_from(9, true, room, message.data(), message.size()));
  CHECK(!try_from(0, true, room, message.data(), message.size()));
  CHECK(!try_from(2, false, room, message.data(), message.size()));
  CHECK(!try_from(2, true, room, message.data(), 3));
  CHECK(!try_from(2, true, room, nullptr, message.size()));
}

// Each client's sprint report: whether its player may sprint and the speed it would sprint at, worked out as the game
// works out its own (LocalPCPlayer.UpdatePlayerSpeed): the multiplier applies but to a dead rig, and the sprint is off
// when Difficulty.sprinting is 0 or the stamina cannot sprint. The message comes back as it went and refuses a bad one.
void sprint_report_round_trips() {
  CHECK(sprint_speed_if_on(3.0f, 1.5f, false) == 4.5f);
  CHECK(sprint_speed_if_on(3.0f, 1.5f, true) == 3.0f);
  CHECK(sprint_speed_if_on(3.0f, 0.0f, false) == 0.0f);
  CHECK(can_sprint(true, 1) && can_sprint(true, 2));
  CHECK(!can_sprint(true, 0) && !can_sprint(false, 1) && !can_sprint(false, 2));
  for (const Sprint s : {Sprint{true, 4.5f}, Sprint{false, 3.0f}, Sprint{false, 0.0f}}) {
    const std::vector<uint8_t> bytes = sprint_message(s);
    CHECK(pm::custom::net_msg::kind_of(bytes.data(), bytes.size()) == pm::custom::net_msg::kSquatterSprint);
    Sprint back{!s.canSprint, -1.0f};
    CHECK(read_sprint(bytes.data(), bytes.size(), back) && back == s);
    for (size_t n = 0; n < bytes.size(); ++n) CHECK(!read_sprint(bytes.data(), n, back));
  }
  std::vector<uint8_t> bad = sprint_message({true, 4.5f});
  bad[4] = 2;  // a flag neither 0 nor 1
  Sprint out{};
  CHECK(!read_sprint(bad.data(), bad.size(), out));
  for (const float speed : {-1.0f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
                            kMaxReportedSpeed * 2}) {
    const std::vector<uint8_t> b = sprint_message({true, speed});
    CHECK(!read_sprint(b.data(), b.size(), out));
  }
  // A client tells the master on a change, and again at a modest rate while nothing changes.
  SprintTeller teller;
  CHECK(teller.due({true, 4.5f}, 100));
  CHECK(!teller.due({true, 4.5f}, 101));
  CHECK(teller.due({false, 4.5f}, 102));  // the sprint ran out
  CHECK(teller.due({false, 3.0f}, 103));  // the multiplier changed
  CHECK(!teller.due({false, 3.0f}, 103 + kSprintResendMs - 1));
  CHECK(teller.due({false, 3.0f}, 103 + kSprintResendMs));
}

// A sprint report is its sender's own: the master takes one from a player in the room, never one from an unknown
// sender (0) or one not in the room, and a client that is not the master takes none.
void a_sprint_report_is_the_senders_own() {
  const std::vector<uint8_t> message = sprint_message({true, 4.5f});
  const std::vector<int> room{1, 2, 3};
  Sprint s;
  CHECK(sprint_from(2, true, room, message.data(), message.size(), s) && s == (Sprint{true, 4.5f}));
  Sprint untouched{false, 1.0f};
  CHECK(!sprint_from(0, true, room, message.data(), message.size(), untouched));
  CHECK(!sprint_from(9, true, room, message.data(), message.size(), untouched));
  CHECK(!sprint_from(2, false, room, message.data(), message.size(), untouched));
  CHECK(!sprint_from(2, true, room, message.data(), message.size() - 1, untouched));
  CHECK(untouched == (Sprint{false, 1.0f}));
}

// The master's book of reports: each player's last word, which goes stale once that player has not reported for
// kSprintStaleMs (a player who left, or whose client stopped), and is then dropped.
void a_stale_sprint_report_is_dropped() {
  SprintBook book;
  CHECK(!book.of(2, 0));
  book.report(2, {true, 4.5f}, 1000);
  book.report(3, {false, 3.0f}, 1000);
  CHECK(book.of(2, 1000 + kSprintStaleMs) && *book.of(2, 1000 + kSprintStaleMs) == (Sprint{true, 4.5f}));
  book.report(3, {true, 3.0f}, 1000 + kSprintStaleMs);
  CHECK(!book.of(2, 1001 + kSprintStaleMs));  // stale
  CHECK(book.of(3, 1001 + kSprintStaleMs) && book.of(3, 1001 + kSprintStaleMs)->canSprint);
  book.drop_stale(1001 + kSprintStaleMs);
  CHECK(book.size() == 1);
  book.clear();
  CHECK(book.size() == 0 && !book.of(3, 1001 + kSprintStaleMs));
}

// After a host migration the new master continues from the last Shown: the phase it followed stays, Chasing or
// LockedOut, and a LockedOut Squatter never locks the exits a second time (doors_lock; squatter_door_test checks that
// lock_all calls nothing more on a client that already holds them).
void a_new_master_continues_from_the_last_shown_phase() {
  Shown chasing;
  chasing.phase = Phase::Chasing;
  std::optional<Phase> kept = first_phase(false);
  kept = kept_phase(kept, chasing, false);  // a client that is not the master follows the master's word
  CHECK(kept == Phase::Chasing);
  kept = kept_phase(kept, chasing, true);  // it becomes the master
  CHECK(kept == Phase::Chasing);
  CHECK(next_phase(*kept, hostile(2)) == Phase::Searching);  // its own tick goes on from the chase, not the start
  CHECK(kept_phase(Phase::Searching, chasing, true) == Phase::Searching);  // the master's own phase is its own
  Shown out;
  out.phase = Phase::LockedOut;
  kept = kept_phase(Phase::Chasing, out, false);
  CHECK(kept == Phase::LockedOut);
  kept = kept_phase(kept, out, true);
  CHECK(kept == Phase::LockedOut);
  CHECK(next_phase(*kept, hostile(0)) == Phase::LockedOut && !doors_lock(*kept, hostile(0)));
  // Before any word from the master a client keeps its own first phase, and with none at all, none.
  CHECK(kept_phase(Phase::Lurking, std::nullopt, false) == Phase::Lurking);
  CHECK(!kept_phase(std::nullopt, std::nullopt, false));
}

// A fake game for figure::load: the bundle and its controller, and an Animator that knows the controller's states.
namespace fig {
int kGameObjectClass, kAnimatorClass, kControllerClass, kClothClass;
struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x40]{};
};
Obj g_bundle, g_controller{&kControllerClass}, g_animator{&kAnimatorClass};
std::set<std::string> g_states;
std::map<int32_t, std::string> g_names;
void* g_controllerSet = nullptr;
bool g_unloaded = false;
void* g_controllerType = nullptr;

void* load_from_file(void*, uint32_t, uint64_t) { return &g_bundle; }
void* load_asset(void*, void* name, void* type) {
  return pm::api::to_utf8(name) == "assets/squatter.controller" && type == g_controllerType ? &g_controller : nullptr;
}
void unload(void*, bool) { g_unloaded = true; }
void ctor(void*, void*, const void*) {}
void* add_component(void*, void*, const void*) { return &g_animator; }
void set_controller(void*, void* c, const void*) { g_controllerSet = c; }
bool initialised(void*) { return g_controllerSet == &g_controller; }
void rebind(void*, bool) {}
int32_t hash(void* name, const void*) {
  const std::string s = pm::api::to_utf8(name);
  const auto h = static_cast<int32_t>(std::hash<std::string>{}(s) & 0x7fffffff);  // pm:not-game-data: a test hash
  g_names[h] = s;
  return h;
}
bool has_state(void*, int32_t layer, int32_t h) { return layer == 0 && g_states.contains(g_names[h]); }
void destroy_immediate(void*, bool) {}
void cloth_setter(void*, void*) {}
void cloth_flag(void*, bool) {}
void cloth_value(void*, float) {}
void capsules(void*, void*, const void*) {}

// The optional entries the figure's sheet needs, each of which the start checks for.
const pm::sdk::EntryDesc* const kOptionalNeeds[] = {
    &pm::map::Cloth, &pm::map::Cloth_set_capsuleColliders, &pm::map::Cloth_set_coefficients,
    &pm::map::Cloth_set_useGravity, &pm::map::Cloth_set_bendingStiffness, &pm::map::Cloth_set_stretchingStiffness,
    &pm::map::Cloth_set_damping, &pm::map::Layer_IgnoreRaycast, &pm::map::ShaderProperty_Lit_BaseColor,
    &pm::map::ShaderProperty_Lit_Cull};

void start() {
  namespace map = pm::map;
  pm::fake::reset();
  pm::fake::type(map::GameObject, &kGameObjectClass);
  pm::fake::type(map::Animator, &kAnimatorClass);
  pm::fake::type(map::RuntimeAnimatorController, &kControllerClass);
  pm::fake::type(map::Cloth, &kClothClass);
  pm::fake::icall(map::AssetBundle_LoadFromFile, reinterpret_cast<void*>(&load_from_file));
  pm::fake::icall(map::AssetBundle_LoadAsset, reinterpret_cast<void*>(&load_asset));
  pm::fake::icall(map::AssetBundle_Unload, reinterpret_cast<void*>(&unload));
  pm::fake::method(map::GameObject_ctor, reinterpret_cast<void*>(&ctor));
  pm::fake::method(map::GameObject_AddComponent, reinterpret_cast<void*>(&add_component));
  pm::fake::method(map::Animator_set_runtimeAnimatorController, reinterpret_cast<void*>(&set_controller));
  pm::fake::icall(map::Animator_get_isInitialized, reinterpret_cast<void*>(&initialised));
  pm::fake::icall(map::Animator_Rebind, reinterpret_cast<void*>(&rebind));
  pm::fake::icall(map::Animator_HasState, reinterpret_cast<void*>(&has_state));
  pm::fake::method(map::Animator_StringToHash, reinterpret_cast<void*>(&hash));
  pm::fake::icall(map::UnityObject_DestroyImmediate, reinterpret_cast<void*>(&destroy_immediate));
  pm::fake::method(map::Cloth_set_capsuleColliders, reinterpret_cast<void*>(&capsules));
  pm::fake::icall(map::Cloth_set_coefficients, reinterpret_cast<void*>(&cloth_setter));
  pm::fake::icall(map::Cloth_set_useGravity, reinterpret_cast<void*>(&cloth_flag));
  pm::fake::icall(map::Cloth_set_bendingStiffness, reinterpret_cast<void*>(&cloth_value));
  pm::fake::icall(map::Cloth_set_stretchingStiffness, reinterpret_cast<void*>(&cloth_value));
  pm::fake::icall(map::Cloth_set_damping, reinterpret_cast<void*>(&cloth_value));
  pm::fake::layer_names({"Default", "TransparentFX", "Ignore Raycast"});
  pm::fake::layer(map::Layer_IgnoreRaycast);
  pm::fake::shader_property(map::ShaderProperty_Lit_BaseColor);
  pm::fake::shader_property(map::ShaderProperty_Lit_Cull);
  std::string missing;
  pm::api::bind(pm::fake::host(), missing);
  g_controllerType = phasmo::RuntimeAnimatorController::type.type_object();
  g_controllerSet = nullptr;
  g_unloaded = false;
  g_states.clear();
  for (const char* s : all_states()) g_states.insert(s);
}
}  // namespace fig

// The mod's start loads the bundle and refuses one whose controller lacks any state of the table, or that is missing:
// a mod with an error is off (user, 2026-11-06).
void the_start_check_refuses_a_bundle_missing_a_state() {
  const fs::path assets = fs::temp_directory_path() / "pm_squatter_figure_test";
  std::error_code ignored;
  fs::remove_all(assets, ignored);
  fs::create_directories(assets);
  fig::start();
  std::string why;
  CHECK(!figure::load(assets, why));
  CHECK(why.find("squatter.bundle") != std::string::npos && why.find("missing") != std::string::npos);
  std::ofstream(assets / "squatter.bundle") << "bundle";
  CHECK(figure::load(assets, why));
  figure::unload();
  CHECK(fig::g_unloaded);
  for (const char* state : all_states()) {
    fig::start();
    fig::g_states.erase(state);
    CHECK(!figure::load(assets, why));
    CHECK(why == std::string("assets/squatter.controller lacks the state ") + state);
    CHECK(fig::g_unloaded);  // nothing is kept
  }
  // A build lacking any optional entry the sheet needs (the Cloth module, say) fails the start, naming the entry.
  for (const pm::sdk::EntryDesc* d : fig::kOptionalNeeds) {
    fig::start();
    pm::fake::forget(*d);
    pm::api::bind(pm::fake::host(), why);
    CHECK(!figure::load(assets, why));
    CHECK(why == std::string(d->key) + " is not available in this game build");
  }
  figure::unload();
  fs::remove_all(assets, ignored);
}

}  // namespace

// A new clip starts at once, and the old pose's difference from it dies away over kInertiaSeconds with no jump at
// either end (user, 2026-11-09: no pose ever snaps or floats between clips).
void inertialization_starts_from_the_old_pose_and_dies_away() {
  using pm::custom::qmul;
  using pm::custom::PoseInertia;
  using pm::custom::kInertiaSeconds;
  const auto turn = [](float x, float y, float z, float degrees) {
    const float n = std::sqrt(x * x + y * y + z * z), h = degrees * 3.14159265f / 360.0f;
    return phasmo::Quaternion{x / n * std::sin(h), y / n * std::sin(h), z / n * std::sin(h), std::cos(h)};
  };
  const auto near = [](phasmo::Quaternion a, phasmo::Quaternion b) {
    const float dot = std::fabs(a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w);
    return dot > 1.0f - 1e-4f;
  };
  const auto angle = [](phasmo::Quaternion q) { return 2.0f * std::acos(std::min(1.0f, std::fabs(q.w))); };
  const phasmo::Quaternion from[] = {turn(1, 0, 0, 40), turn(0, 1, 0, -170), turn(1, 1, 0, 5), {0, 0, 0, 1}};
  const phasmo::Quaternion to[] = {turn(0, 0, 1, 30), turn(0, 1, 0, 170), turn(1, 1, 0, 5), turn(0, 1, 1, 90)};
  PoseInertia inertia;
  CHECK(!inertia.active(0.0f));
  inertia.begin(from, to, 10.0f);
  CHECK(inertia.active(10.0f));
  for (size_t i = 0; i < std::size(from); ++i) CHECK(near(qmul(inertia.offset(i, 10.0f), to[i]), from[i]));
  for (size_t i = 0; i < std::size(from); ++i) {
    float last = angle(inertia.offset(i, 10.0f));
    for (float t = 10.0f; t <= 10.0f + kInertiaSeconds + 0.05f; t += 0.01f) {
      const float now = angle(inertia.offset(i, t));
      CHECK(now <= last + 1e-5f);
      last = now;
    }
  }
  CHECK(!inertia.active(10.0f + kInertiaSeconds));
  for (size_t i = 0; i < std::size(from); ++i) CHECK(near(inertia.offset(i, 10.0f + kInertiaSeconds), {0, 0, 0, 1}));
  CHECK(near(inertia.offset(std::size(from), 10.0f), {0, 0, 0, 1}));  // a bone it never saw
  // Spans of different lengths are not two poses of one body: nothing starts.
  PoseInertia odd;
  odd.begin(std::span(from, 3), to, 10.0f);
  CHECK(!odd.active(10.0f));
  CHECK(near(odd.offset(0, 10.0f), {0, 0, 0, 1}));
  inertia.clear();
  CHECK(!inertia.active(10.0f));
}

// The pass after the animation runs for the figure's own IKExecutionOrder alone: Final IK's LateUpdate runs for every
// model that has one, and a missing figure is never its own.
// While the figure's body falls, Final IK runs nothing on it: neither its solvers (LateUpdate) nor the reset that puts
// the bones back in their stored pose each frame (Update calls each solver's FixTransforms), which would stand the
// ragdoll back up every frame. Every other model's IK runs as the game runs it.
void the_ik_stands_aside_while_the_body_falls() {
  int own = 0, other = 0;
  CHECK(!figure::ik_runs(&own, &own, true));
  CHECK(figure::ik_runs(&own, &own, false));
  CHECK(figure::ik_runs(&other, &own, true));
  CHECK(figure::ik_runs(&other, nullptr, true));
}

void the_pass_after_the_animator_is_the_figures_own() {
  int a = 0, b = 0;
  CHECK(figure::is_own_ik(&a, &a));
  CHECK(!figure::is_own_ik(&a, &b));
  CHECK(!figure::is_own_ik(nullptr, nullptr));
  CHECK(!figure::is_own_ik(&a, nullptr));
}

int main() {
  every_move_and_act_has_its_states();
  move_variants_are_deterministic();
  the_start_check_refuses_a_bundle_missing_a_state();
  the_pass_after_the_animator_is_the_figures_own();
  the_ik_stands_aside_while_the_body_falls();
  inertialization_starts_from_the_old_pose_and_dies_away();
  a_fall_settles_then_gets_up();
  the_get_up_follows_how_it_landed();
  it_gets_up_where_it_lies();
  what_knocks_it_over();
  a_fall_never_stacks();
  a_leaf_meets_the_body_at_its_nearest_point();
  a_move_lands_where_the_world_needs_it();
  the_clips_travel_as_built();
  a_drop_falls_as_the_jump_lands();
  its_steps_are_as_loud_as_a_players();
  an_open_doorway_does_not_muffle_its_voice();
  it_stands_facing_the_way_its_body_faces();
  the_standing_roll_hands_back_to_the_run();
  its_voice_is_pitched_down();
  every_bone_under_the_hips_blends();
  it_turns_with_its_clips_before_running_on();
  every_fall_says_its_line();
  a_root_jump_is_a_warp_not_a_step();
  the_frames_of_a_trip_are_timed_past_the_get_up();
  a_clip_change_is_a_jump_by_turn_or_by_place();
  the_sheet_stays_inside_its_point_budget();
  sheet_is_closed_grid_with_eye_holes();
  sheet_pins_only_the_top();
  sheet_sways_only_near_its_shape();
  sheet_follows_the_bones_nearest_it();
  sheet_reaches_the_floor();
  eyes_are_mirrored();
  sheet_hangs_from_the_crown();
  the_mod_loads_its_voice_from_its_assets();
  lurks_until_it_sees_someone();
  only_idling_wandering_and_its_room_pass();
  only_idling_passes_while_it_steers();
  steering_covers_every_phase_but_lurking();
  search_and_chase_alternate();
  door_locks_only_when_everyone_is_out();
  walking_back_in_keeps_the_door_open();
  a_dropped_target_does_not_stop_the_door();
  chase_ends_locked_out_when_everyone_is_out();
  locked_out_is_final();
  a_hiding_target_is_safe();
  it_loses_interest_at_a_hiding_place();
  its_way_into_a_hiding_place();
  hearing_a_target_resumes_the_chase();
  an_unknown_count_never_locks();
  the_move_clock_survives_bad_times();
  a_move_that_plays_once_holds_for_its_clip();
  the_get_ups_stand_alone();
  friendly_never_chases_or_kicks();
  kick_needs_a_living_target_in_reach();
  speed_stays_just_under_sprint();
  tired_taunt_fires_once_per_lapse();
  lines_match_the_phase();
  picks_never_repeat_and_handle_nothing();
  shown_round_trips();
  a_fall_is_shared();
  bad_shown_is_dropped();
  one_try_is_sent_per_input();
  a_non_masters_try_reaches_the_master();
  a_forged_try_is_ignored();
  sprint_report_round_trips();
  a_sprint_report_is_the_senders_own();
  a_stale_sprint_report_is_dropped();
  a_new_master_continues_from_the_last_shown_phase();
  hears_as_the_hunts_voice_test();
  the_target_is_the_nearest_sensed_player_inside();
  tracking_lapses_after_the_window();
  a_hiding_place_is_known_from_tracking();
  lines_come_at_a_random_gap();
  stuck_for_a_while_is_a_bump();
  search_points_stay_near_the_last_place();
  the_door_clip_follows_the_side();
  a_fall_stops_it_where_it_stands();
  walking_and_running_keep_pace_with_the_ground();
  its_head_turns_to_the_player_it_is_after();
  it_turns_on_the_spot_by_its_clips();
  the_way_ahead_picks_its_clumsy_move();
  a_clumsy_move_waits_until_it_faces_its_way();
  a_kick_at_a_hiding_place_sounds_its_door();
  it_reaches_a_hider_at_the_closets_spot();
  a_closets_obstacle_opens_only_while_it_goes_in();
  a_door_line_is_said_at_the_nearest_exit();
  the_pace_keeps_the_distance_of_a_frame_with_no_time();
  the_way_ahead_is_read_across_its_width();
  its_feet_meet_what_it_trips_over();
  voice_lines_never_talk_over_each_other();
  a_door_line_comes_from_behind_the_door();
  a_door_is_reached_within_reach();
  the_chase_speed_comes_from_the_target();
  a_target_who_cannot_sprint_is_taunted();
  cues_play_once_and_never_replay_on_joining();
  the_chase_tunables_are_sane();
  the_kick_never_reaches_through_a_floor();
  with_no_last_place_it_searches_around_itself();
  an_unseen_figure_never_starts_a_chase();
  a_step_plays_as_each_foot_plants();
  moping_moves_to_the_closest_place_outside_a_closed_hiding_place();
  it_stands_still_and_walks_with_its_own_clips();
  it_opens_only_the_door_it_walks_into();
  friendly_starts_seated_in_the_favourite_room();
  gets_up_on_first_sight_and_says_its_line_once();
  follows_the_nearest_living_inside_and_stops_at_exits();
  waits_and_talks_at_a_hiding_place_without_banging();
  wav_decodes_mono_and_stereo();
  wav_refuses_malformed();
  every_set_has_a_folder_name();
  a_trip_says_its_line();
  sets_list_their_files_sorted_by_name();
  missing_sets_stay_silent();
  every_voice_file_is_in_the_payload();
  return g_failures ? 1 : 0;
}
