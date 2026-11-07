// Offline tests for the Squatter's rules (mods/custom_ghosts/squatter_rule.h).
// Run: build\squatter_test.exe
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <utility>
#include <string>
#include <vector>

#include "sheet_mesh.h"
#include "squatter_rule.h"
#include "squatter_voice.h"
#include "wav.h"

#include "check.h"

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
  const Move start = move_at(0.0f);
  CHECK(move_at(std::numeric_limits<float>::quiet_NaN()) == start);
  CHECK(move_at(std::numeric_limits<float>::infinity()) == start);
  CHECK(move_at(-std::numeric_limits<float>::infinity()) == start);
  const int index = static_cast<int>(move_at(1e9f));
  CHECK(index >= 0 && index <= static_cast<int>(Move::Stumble));
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

void speed_stays_just_under_sprint() {
  for (const float s : {2.0f, 3.0f, 4.0f}) {
    CHECK(average_chase_speed(s) < s);
    CHECK(average_chase_speed(s) > 0.85f * s);
    CHECK(move_speed(s, Move::Stumble) == 0.0f);
    CHECK(move_speed(s, Move::Roll) > move_speed(s, Move::Run));
    CHECK(move_speed(s, Move::Duck) < move_speed(s, Move::Run));
  }
  // The cycle holds every move, Run most of all, and repeats; a negative time reads as the start.
  int seen[6] = {};
  for (float t = 0; t < kMoveCycleSeconds; t += 0.25f) ++seen[static_cast<int>(move_at(t))];
  for (const int n : seen) CHECK(n > 0);
  for (const int n : seen) CHECK(n <= seen[static_cast<int>(Move::Run)]);
  CHECK(move_at(kMoveCycleSeconds) == move_at(0.0f));
  CHECK(move_at(-3.0f) == move_at(0.0f));
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
  const char* expected[] = {"lurk", "chase", "search", "hiding", "catch", "door", "tired", "mope", "excited", "friendly"};
  std::set<std::string> seen;
  for (int i = 0; i < 10; ++i) {
    const char* name = voice::folder_name(static_cast<Lines>(i));
    CHECK(std::string(name) == expected[i]);
    seen.insert(name);
  }
  CHECK(seen.size() == 10);
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
  for (int i = 0; i < 10; ++i) {
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
  for (int state = -1; state <= 30; ++state) CHECK(keeps_quiet(s, state) == !(state == 0 || state == 1 || state == 3));
  const QuietStates unread{};
  CHECK(keeps_quiet(unread, -1) && keeps_quiet(unread, 0) && keeps_quiet(unread, 2));
}

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

bool inside_an_eye(const sh::SheetShape& s, sh::Vec2 p) {
  for (const float side : {-1.0f, 1.0f}) {
    const float x = side * s.eyeAcross;
    if (std::abs(p.x - x) < s.eyeWidth / 2 && std::abs(p.y - s.eyeForward) < s.eyeHeight / 2) return true;
  }
  return false;
}

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
  for (size_t i = 0; i < cells; ++i)
    for (size_t j = 0; j < cells; ++j)
      if (overlaps_an_eye(shape, axis[i], axis[i + 1], axis[j], axis[j + 1])) ++cut;
  CHECK(cut > 0);
  CHECK(m.triangles.size() == 6 * (cells * cells - cut));
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
  // Each vertex lies on the grid, and its uv is its place on the sheet from 0 to 1.
  for (size_t v = 0; v < n; ++v) {
    CHECK(std::find(axis.begin(), axis.end(), m.flat[v].x) != axis.end());
    CHECK(std::find(axis.begin(), axis.end(), m.flat[v].y) != axis.end());
    CHECK(std::abs(m.uv[v].x - (m.flat[v].x / shape.side + 0.5f)) < 1e-5f);
    CHECK(std::abs(m.uv[v].y - (m.flat[v].y / shape.side + 0.5f)) < 1e-5f);
  }
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
    for (int j = 0; j < cells; ++j)
      if (!present.contains({i, j})) holes.emplace_back(i, j);
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

}  // namespace

int main() {
  sheet_is_closed_grid_with_eye_holes();
  sheet_pins_only_the_top();
  eyes_are_mirrored();
  sheet_hangs_from_the_crown();
  the_mod_loads_its_voice_from_its_assets();
  lurks_until_it_sees_someone();
  only_idling_wandering_and_its_room_pass();
  search_and_chase_alternate();
  door_locks_only_when_everyone_is_out();
  walking_back_in_keeps_the_door_open();
  a_dropped_target_does_not_stop_the_door();
  chase_ends_locked_out_when_everyone_is_out();
  locked_out_is_final();
  a_hiding_target_is_safe();
  hearing_a_target_resumes_the_chase();
  an_unknown_count_never_locks();
  the_move_clock_survives_bad_times();
  friendly_never_chases_or_kicks();
  kick_needs_a_living_target_in_reach();
  speed_stays_just_under_sprint();
  tired_taunt_fires_once_per_lapse();
  lines_match_the_phase();
  picks_never_repeat_and_handle_nothing();
  wav_decodes_mono_and_stereo();
  wav_refuses_malformed();
  every_set_has_a_folder_name();
  sets_list_their_files_sorted_by_name();
  missing_sets_stay_silent();
  every_voice_file_is_in_the_payload();
  return g_failures ? 1 : 0;
}
