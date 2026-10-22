#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// The contract's top moments, kept free of game calls so they can be tested offline: what the host
// records twice a second, how the moments are found in it, and the blob that carries them (with the
// level's room outlines) to every client for the replay in the lobby.
namespace pg::replay {

constexpr int kMaxPlayers = 8;
constexpr uint32_t kStepMs = 500;      // one frame every half second
constexpr int kMaxMoments = 5;
constexpr int kMaxMomentFrames = 40;   // 20 s
constexpr float kStorey = 2.5f;        // metres of height within which two things are on one floor

struct Point {
  float x = 0, y = 0, z = 0;
};

inline float flat_distance(Point a, Point b) { return std::hypot(a.x - b.x, a.z - b.z); }
inline bool same_storey(Point a, Point b) { return std::fabs(a.y - b.y) < kStorey; }

enum FrameFlag : uint8_t { kGhostPresent = 1, kHunting = 2, kEvent = 4 };

struct Frame {
  Point ghost;
  int8_t floor = -1;      // the floor the ghost's room is on (-1 unknown)
  uint8_t flags = 0;      // FrameFlag
  uint8_t present = 0;    // bit per player slot: in the contract
  uint8_t dead = 0;       // bit per player slot
  Point players[kMaxPlayers];
  bool operator==(const Frame& o) const {
    if (floor != o.floor || flags != o.flags || present != o.present || dead != o.dead) return false;
    auto same = [](Point a, Point b) { return std::fabs(a.x - b.x) < 0.11f && std::fabs(a.y - b.y) < 0.11f && std::fabs(a.z - b.z) < 0.11f; };
    if (!same(ghost, o.ghost)) return false;
    for (int i = 0; i < kMaxPlayers; ++i)
      if ((present >> i & 1) && !same(players[i], o.players[i])) return false;
    return true;
  }
};

struct Recording {
  std::vector<int> actors;            // slot -> Photon actor
  std::vector<Frame> frames;          // every kStepMs from the start
  std::vector<uint32_t> crucifixes;   // ms of each crucifix burn
};

enum MomentKind : uint8_t { kKill = 1, kEscape = 2, kChase = 3, kCloseEvent = 4, kCrucifixSave = 5 };
constexpr uint8_t kNoSubject = 0xFF;

struct Moment {
  uint8_t kind = 0;
  uint8_t subject = kNoSubject;  // the investigator's slot it is about
  int first = 0, last = 0;       // frame range shown
  int peak = 0;                  // the frame it is about
  float score = 0;
};

struct Box {  // a room collider seen from above
  int8_t floor = 0;
  float x0 = 0, z0 = 0, x1 = 0, z1 = 0;
};

/** The moments worth replaying, best first, then in the order they happened (at most kMaxMoments). */
inline std::vector<Moment> find_moments(const Recording& r) {
  std::vector<Moment> all;
  const int n = static_cast<int>(r.frames.size());
  auto window = [n](int peak, int before, int after) {
    return std::pair<int, int>{std::max(0, peak - before), std::min(n - 1, peak + after)};
  };
  auto add = [&](uint8_t kind, uint8_t subject, int peak, int before, int after, float score) {
    auto [a, b] = window(peak, before, after);
    all.push_back({kind, subject, a, b, peak, score});
  };
  const int slots = std::min<int>(kMaxPlayers, static_cast<int>(r.actors.size()));
  // Kills: the frame an investigator seen alive is first dead (one first seen dead was never alive here).
  for (int s = 0; s < slots; ++s)
    for (int i = 1; i < n; ++i)
      if ((r.frames[i].dead >> s & 1) && (r.frames[i - 1].present >> s & 1) && !(r.frames[i - 1].dead >> s & 1)) {
        add(kKill, static_cast<uint8_t>(s), i, 16, 4, 100);
        break;
      }
  // Hunts: close escapes and the longest chase in each.
  for (int i = 0; i < n;) {
    if (!(r.frames[i].flags & kHunting)) {
      ++i;
      continue;
    }
    int end = i;
    while (end + 1 < n && (r.frames[end + 1].flags & kHunting)) ++end;
    for (int s = 0; s < slots; ++s) {
      const bool survived = !(r.frames[std::min(n - 1, end + 1)].dead >> s & 1);
      float closest = 1e9f;
      int closestAt = -1, run = 0, bestRun = 0, bestRunEnd = -1;
      for (int f = i; f <= end; ++f) {
        const Frame& fr = r.frames[f];
        const bool here = (fr.present >> s & 1) && !(fr.dead >> s & 1) && same_storey(fr.ghost, fr.players[s]);
        const float d = here ? flat_distance(fr.ghost, fr.players[s]) : 1e9f;
        if (d < closest) closest = d, closestAt = f;
        run = d < 6.0f ? run + 1 : 0;
        if (run > bestRun) bestRun = run, bestRunEnd = f;
      }
      if (survived && closestAt >= 0 && closest < 2.0f) add(kEscape, static_cast<uint8_t>(s), closestAt, 10, 6, 60 + (2.0f - closest) * 10);
      if (bestRun >= 6) {
        const int start = bestRunEnd - bestRun + 1;
        const int shown = std::min(bestRun, kMaxMomentFrames - 4);
        all.push_back({kChase, static_cast<uint8_t>(s), std::max(0, bestRunEnd - shown - 1), std::min(n - 1, bestRunEnd + 2),
                       start, 40 + bestRun * 0.5f});
      }
    }
    i = end + 1;
  }
  // Events an investigator saw from close by.
  for (int i = 0; i < n;) {
    if (!(r.frames[i].flags & kEvent)) {
      ++i;
      continue;
    }
    int end = i;
    while (end + 1 < n && (r.frames[end + 1].flags & kEvent)) ++end;
    float closest = 1e9f;
    int at = -1, who = -1;
    for (int f = i; f <= end; ++f)
      for (int s = 0; s < slots; ++s) {
        const Frame& fr = r.frames[f];
        if (!(fr.present >> s & 1) || (fr.dead >> s & 1) || !same_storey(fr.ghost, fr.players[s])) continue;
        const float d = flat_distance(fr.ghost, fr.players[s]);
        if (d < closest) closest = d, at = f, who = s;
      }
    if (at >= 0 && closest < 5.0f) add(kCloseEvent, static_cast<uint8_t>(who), at, 6, 6, 30 + (5.0f - closest) * 2);
    i = end + 1;
  }
  // Crucifixes that stopped a hunt.
  for (uint32_t ms : r.crucifixes) {
    const int f = std::min(n - 1, static_cast<int>(ms / kStepMs));
    if (f >= 0) add(kCrucifixSave, kNoSubject, f, 8, 6, 50);
  }
  // The best that do not overlap, in the order they happened.
  std::stable_sort(all.begin(), all.end(), [](const Moment& a, const Moment& b) { return a.score > b.score; });
  std::vector<Moment> chosen;
  for (const Moment& m : all) {
    if (static_cast<int>(chosen.size()) >= kMaxMoments) break;
    bool overlaps = false;
    for (const Moment& c : chosen) overlaps |= m.first <= c.last && c.first <= m.last;
    if (!overlaps) chosen.push_back(m);
  }
  std::sort(chosen.begin(), chosen.end(), [](const Moment& a, const Moment& b) { return a.peak < b.peak; });
  return chosen;
}

/** The caption of a moment, given the name of the investigator it is about. */
inline std::string caption(uint8_t kind, const std::string& who) {
  switch (kind) {
    case kKill: return who + " was killed by the ghost";
    case kEscape: return who + " escaped the ghost by a hair";
    case kChase: return "The ghost chased " + who;
    case kCloseEvent: return "The ghost appeared right beside " + who;
    case kCrucifixSave: return "A crucifix stopped a hunt";
    default: return {};
  }
}

// The replay as it travels: who was who, the room outlines, and each moment with its frames.
struct Replay {
  std::vector<int> actors;  // slot -> actor
  int ghostActor = 0;       // the player who played the ghost (0: the game's own AI)
  std::vector<Box> rooms;
  struct Shown {
    Moment moment;           // first/last/peak index into frames below
    std::vector<Frame> frames;
  };
  std::vector<Shown> moments;
};

namespace detail {
inline void put(std::vector<uint8_t>& b, const void* p, size_t n) {
  const auto* c = static_cast<const uint8_t*>(p);
  b.insert(b.end(), c, c + n);
}
template <class T>
void put(std::vector<uint8_t>& b, T v) {
  put(b, &v, sizeof v);
}
inline int16_t dm(float metres) { return static_cast<int16_t>(std::clamp(std::lround(metres * 10.0f), -32767L, 32767L)); }
inline void put_point(std::vector<uint8_t>& b, Point p) {
  put<int16_t>(b, dm(p.x));
  put<int16_t>(b, dm(p.y));
  put<int16_t>(b, dm(p.z));
}
struct Reader {
  const uint8_t* p;
  const uint8_t* end;
  template <class T>
  bool get(T& v) {
    if (static_cast<size_t>(end - p) < sizeof v) return false;
    std::memcpy(&v, p, sizeof v);
    p += sizeof v;
    return true;
  }
  bool point(Point& out) {
    int16_t x, y, z;
    if (!get(x) || !get(y) || !get(z)) return false;
    out = {x / 10.0f, y / 10.0f, z / 10.0f};
    return true;
  }
};
}  // namespace detail

constexpr uint8_t kBlobVersion = 1;

/** The replay for a recording: its best moments with their frames. */
inline Replay make_replay(const Recording& r, int ghostActor, const std::vector<Box>& rooms) {
  Replay out;
  out.actors.assign(r.actors.begin(), r.actors.begin() + std::min<size_t>(r.actors.size(), kMaxPlayers));
  out.ghostActor = ghostActor;
  out.rooms = rooms;
  for (const Moment& m : find_moments(r)) {
    Replay::Shown s;
    const int first = std::max(m.first, m.last - kMaxMomentFrames + 1);
    s.frames.assign(r.frames.begin() + first, r.frames.begin() + m.last + 1);
    s.moment = m;
    s.moment.first = 0;
    s.moment.last = static_cast<int>(s.frames.size()) - 1;
    s.moment.peak = std::clamp(m.peak - first, 0, s.moment.last);
    out.moments.push_back(std::move(s));
  }
  return out;
}

inline std::vector<uint8_t> encode(const Replay& r) {
  using namespace detail;
  std::vector<uint8_t> b = {'R', 'P', kBlobVersion};
  put<uint8_t>(b, static_cast<uint8_t>(r.actors.size()));
  for (int a : r.actors) put<int32_t>(b, a);
  put<int32_t>(b, r.ghostActor);
  put<uint16_t>(b, static_cast<uint16_t>(std::min<size_t>(r.rooms.size(), 1000)));
  for (size_t i = 0; i < r.rooms.size() && i < 1000; ++i) {
    const Box& x = r.rooms[i];
    put<int8_t>(b, x.floor);
    put<int16_t>(b, dm(x.x0));
    put<int16_t>(b, dm(x.z0));
    put<int16_t>(b, dm(x.x1));
    put<int16_t>(b, dm(x.z1));
  }
  put<uint8_t>(b, static_cast<uint8_t>(r.moments.size()));
  for (const Replay::Shown& s : r.moments) {
    put<uint8_t>(b, s.moment.kind);
    put<uint8_t>(b, s.moment.subject);
    put<uint8_t>(b, static_cast<uint8_t>(s.moment.peak));
    put<uint8_t>(b, static_cast<uint8_t>(s.frames.size()));
    for (const Frame& f : s.frames) {
      put_point(b, f.ghost);
      put<int8_t>(b, f.floor);
      put<uint8_t>(b, f.flags);
      put<uint8_t>(b, f.present);
      put<uint8_t>(b, f.dead);
      for (int i = 0; i < kMaxPlayers; ++i)
        if (f.present >> i & 1) put_point(b, f.players[i]);
    }
  }
  return b;
}

inline bool decode(const uint8_t* data, size_t size, Replay& out) {
  detail::Reader r{data, data + size};
  uint8_t magic[3], actors, moments;
  if (!r.get(magic[0]) || !r.get(magic[1]) || !r.get(magic[2]) || magic[0] != 'R' || magic[1] != 'P' || magic[2] != kBlobVersion)
    return false;
  Replay x;
  if (!r.get(actors) || actors > kMaxPlayers) return false;
  for (int i = 0; i < actors; ++i) {
    int32_t a;
    if (!r.get(a)) return false;
    x.actors.push_back(a);
  }
  uint16_t rooms;
  if (!r.get(x.ghostActor) || !r.get(rooms) || rooms > 1000) return false;
  for (int i = 0; i < rooms; ++i) {
    Box box;
    int16_t x0, z0, x1, z1;
    if (!r.get(box.floor) || !r.get(x0) || !r.get(z0) || !r.get(x1) || !r.get(z1)) return false;
    box.x0 = x0 / 10.0f, box.z0 = z0 / 10.0f, box.x1 = x1 / 10.0f, box.z1 = z1 / 10.0f;
    x.rooms.push_back(box);
  }
  if (!r.get(moments) || moments > kMaxMoments) return false;
  for (int m = 0; m < moments; ++m) {
    Replay::Shown s;
    uint8_t peak, frames;
    if (!r.get(s.moment.kind) || !r.get(s.moment.subject) || !r.get(peak) || !r.get(frames) || frames == 0 ||
        frames > kMaxMomentFrames || peak >= frames)
      return false;
    for (int i = 0; i < frames; ++i) {
      Frame f;
      if (!r.point(f.ghost) || !r.get(f.floor) || !r.get(f.flags) || !r.get(f.present) || !r.get(f.dead)) return false;
      for (int p = 0; p < kMaxPlayers; ++p)
        if ((f.present >> p & 1) && !r.point(f.players[p])) return false;
      s.frames.push_back(f);
    }
    s.moment.first = 0;
    s.moment.last = frames - 1;
    s.moment.peak = peak;
    x.moments.push_back(std::move(s));
  }
  if (r.p != r.end) return false;
  out = std::move(x);
  return true;
}

// Chunks of a blob, each small enough for one message, and their reassembly.
constexpr size_t kChunkBytes = 480;
inline std::vector<std::vector<uint8_t>> chunks(const std::vector<uint8_t>& blob) {
  std::vector<std::vector<uint8_t>> out;
  for (size_t at = 0; at < blob.size(); at += kChunkBytes)
    out.emplace_back(blob.begin() + at, blob.begin() + std::min(blob.size(), at + kChunkBytes));
  return out;
}

class Assembler {
 public:
  /** Add chunk `index` of `count` of blob `id`; true (and `out`) once the blob is whole. */
  bool add(uint16_t id, uint16_t index, uint16_t count, const uint8_t* data, size_t size, std::vector<uint8_t>& out) {
    if (count == 0 || count > 256 || index >= count || size > kChunkBytes) return false;
    if (id != id_ || count != parts_.size()) {
      id_ = id;
      parts_.assign(count, {});
      have_.assign(count, false);
    }
    parts_[index].assign(data, data + size);
    have_[index] = true;
    if (std::find(have_.begin(), have_.end(), false) != have_.end()) return false;
    out.clear();
    for (const auto& p : parts_) out.insert(out.end(), p.begin(), p.end());
    parts_.clear();
    have_.clear();
    return true;
  }

 private:
  uint16_t id_ = 0;
  std::vector<std::vector<uint8_t>> parts_;
  std::vector<bool> have_;
};

}  // namespace pg::replay
