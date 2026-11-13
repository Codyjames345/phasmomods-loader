#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <span>
#include <vector>

// The Squatter's bedsheet as a mesh for Unity's Cloth, kept free of game calls so it can be tested offline. A plain
// sheet generated in code, not an art asset (the user's ruling of 2026-11-04, in the Squatter's design).
//
// The sheet is round, its centre on the crown. It is made already hanging: it follows a head and shoulders profile (a
// cap over the head, a slope out to the shoulders, then down to the floor, flaring a little), so a sheet as wide as the
// body reaches the floor all round (user, 2026-11-09). So the cloth starts close to where it settles, instead of
// falling into place from a flat square, which the engine test showed drapes badly
// (sdk/internals/engine-experiments.md).
//
// Its points lie in rings round the crown, each ring a step down the sheet from the last, and each holding as many
// points as its own length on the hanging shape needs at that step, so every triangle is about as wide as it is long,
// as cloth meshes are built in games. (It was a square grid cut round and squeezed onto the hanging shape: every ring
// at the hem squeezed to a quarter of its length, so most triangles were needles, which Unity's cloth folded into
// jagged shards and lit in facets; user, 2026-11-13.) The rings are close round the head, where the eyes are cut, and
// further apart lower down. The two eye holes are mirrored about the centre line, in front of the crown. Only the crown
// is pinned; everything else is free for the cloth to move.
//
// `flat` is where a point lies on the sheet laid flat: its distance from the centre measured along the sheet, in the
// direction it hangs, as a sheet is measured.
//
// Space: the sheet's own, in metres. The crown is the origin, y is up and z is forward (the way the face looks), x
// is across. Unity's winding: each triangle's front faces out of the sheet (up at the crown).
namespace pm::custom::sheet {

/** Two floats, as Unity's Vector2. */
struct Vec2 {
  float x, y;
};
/** Three floats, as Unity's Vector3. */
struct Vec3 {
  float x, y, z;
};
/**
 * One vertex's cloth constraint, laid out as Unity's ClothSkinningCoefficient: how far the vertex may move from where
 * the skinning puts it (0: it stays there, pinned), and the depth of the sphere it may not enter.
 */
struct Coefficient {
  float maxDistance, collisionSphereDistance;
};

/**
 * The sheet's size, grid, eyes and the profile it is made hanging over. Metres. Starting values, tuned in game; the
 * figure sets the sheet's size, the shoulders' width and drop from the body it covers (squatter_figure.cpp).
 */
struct SheetShape {
  float side = 2.0f;            // the round sheet's diameter, measured along the sheet
  float fineSpacing = 0.02f;    // the spacing of the points within fineHalf of the centre, where the eyes are cut
  float fineHalf = 0.24f;       // how far from the centre, along the sheet, the fine spacing holds
  float coarseSpacing = 0.05f;  // the largest spacing further out, which it widens to by kSpacingGrowth
  float pinRadius = 0.08f;      // points this close to the centre, measured on the sheet, stay with the head
  float eyeAcross = 0.04f;      // each eye's centre from the centre line
  float eyeForward = 0.16f;     // the eyes' centres in front of the crown, measured on the sheet
  float eyeWidth = 0.04f;       // each eye's oval, across and forward: the cells it overlaps are cut, and the points
  float eyeHeight = 0.03f;      // round the hole are moved onto the oval (onto_eye_oval)
  float headRadius = 0.12f;      // the cap the sheet is made over, the crown its top
  float shoulderRadius = 0.30f;  // how far out the sheet hangs at the shoulders
  float shoulderDrop = 0.14f;    // from the bottom of the cap down to the shoulders
  float flare = 0.10f;           // how much further out it hangs for each metre below the shoulders
};

/**
 * The sheet: per vertex its position and normal in the sheet's space, its uv, where it lies on the sheet laid flat
 * (`flat`: x across, y forward, metres from the centre) and its cloth coefficient; then three indices per triangle.
 */
struct SheetMesh {
  std::vector<Vec3> vertices, normals;
  std::vector<Vec2> uv, flat;
  std::vector<int32_t> triangles;
  std::vector<Coefficient> coefficients;
};

/** What an unconstrained coefficient holds: no limit (Unity's own default for a vertex left free). */
inline constexpr float kUnconstrained = std::numeric_limits<float>::max();

// How much wider the spacing grows beyond fineHalf, per metre down the sheet, until it reaches coarseSpacing: a gentle
// widening, so two rings next to each other differ in spacing by no more than about a quarter.
inline constexpr float kSpacingGrowth = 0.25f;

/** The spacing of the sheet's points `distance` metres from its centre, measured along the sheet. */
inline float spacing_at(const SheetShape& s, float distance) {
  if (distance <= s.fineHalf) return s.fineSpacing;
  return std::min(s.coarseSpacing, s.fineSpacing + (distance - s.fineHalf) * kSpacingGrowth);
}

/**
 * The rings' distances from the centre, measured along the sheet: 0 (the crown, one point), then a step of
 * spacing_at each, the last one on the hem at side / 2 exactly.
 */
inline std::vector<float> ring_distances(const SheetShape& s) {
  const float far = s.side / 2;
  std::vector<float> rings{0.0f};
  if (far <= 0) return rings;
  for (;;) {
    const float step = spacing_at(s, rings.back());
    if (rings.back() + 1.5f * step >= far) break;
    rings.push_back(rings.back() + step);
  }
  rings.push_back(far);
  return rings;
}

/** How far `p` (on the flat sheet) lies out on its eye's oval, the eye on its side of the centre line: 1 on the oval. */
inline float eye_reach(const SheetShape& s, Vec2 p) {
  const float cx = (p.x < 0 ? -1.0f : 1.0f) * s.eyeAcross;
  const float u = (p.x - cx) / (s.eyeWidth / 2), v = (p.y - s.eyeForward) / (s.eyeHeight / 2);
  return std::sqrt(u * u + v * v);
}

/** Whether `p` lies inside an eye's oval, and not on it. */
inline bool in_an_eye(const SheetShape& s, Vec2 p) { return eye_reach(s, p) < 1.0f - 1e-4f; }

/**
 * A point round an eye's hole on the flat sheet, `p`, moved along the line from its eye's centre onto the eye's oval
 * (eyeWidth across, eyeHeight forward), so the hole is an oval rather than the rectangle its cut cells make (user,
 * 2026-11-09). The eye is the one on `p`'s side of the centre line. `p` itself at the eye's centre stays where it is.
 */
inline Vec2 onto_eye_oval(const SheetShape& s, Vec2 p) {
  const float cx = (p.x < 0 ? -1.0f : 1.0f) * s.eyeAcross, cz = s.eyeForward;
  const float dx = p.x - cx, dz = p.y - cz;
  const float u = dx / (s.eyeWidth / 2), v = dz / (s.eyeHeight / 2), out = std::sqrt(u * u + v * v);
  if (out <= 0.0f) return p;
  return {cx + dx / out, cz + dz / out};
}

/**
 * Where the point `distance` metres from the centre, measured along the sheet, hangs: its distance out from the
 * vertical through the crown, its height below the crown (negative), and the outward normal there in the same two
 * directions.
 */
inline void hang(const SheetShape& s, float distance, float& out, float& height, float& normalOut, float& normalUp) {
  const float r = s.headRadius;
  const float cap = r * std::numbers::pi_v<float> / 2;  // from the crown round to the side of the head
  const float slopeOut = s.shoulderRadius - r;
  const float slope = std::sqrt(slopeOut * slopeOut + s.shoulderDrop * s.shoulderDrop);
  if (distance <= cap) {
    const float angle = distance / r;
    out = r * std::sin(angle);
    height = r * std::cos(angle) - r;
    normalOut = std::sin(angle);
    normalUp = std::cos(angle);
  } else if (distance <= cap + slope) {
    const float t = (distance - cap) / slope;
    out = r + t * slopeOut;
    height = -r - t * s.shoulderDrop;
    normalOut = s.shoulderDrop / slope;
    normalUp = slopeOut / slope;
  } else {
    const float n = std::sqrt(1.0f + s.flare * s.flare);  // along the sheet per metre down
    const float down = (distance - cap - slope) / n;
    out = s.shoulderRadius + s.flare * down;
    height = -r - s.shoulderDrop - down;
    normalOut = 1.0f / n;
    normalUp = s.flare / n;
  }
}

/**
 * How far from the centre, measured along the sheet, a point hangs `drop` metres below the crown (hang): what the
 * sheet's radius must be for its hem to reach a floor `drop` below the crown. 0 for a drop above the shoulders.
 */
inline float reach_distance(const SheetShape& s, float drop) {
  const float r = s.headRadius;
  const float slopeOut = s.shoulderRadius - r;
  const float slope = std::sqrt(slopeOut * slopeOut + s.shoulderDrop * s.shoulderDrop);
  const float below = drop - r - s.shoulderDrop;
  if (below <= 0) return 0.0f;
  return r * std::numbers::pi_v<float> / 2 + slope + below * std::sqrt(1.0f + s.flare * s.flare);
}

/** How far the cap over the head reaches from the centre, measured along the sheet: the part the head alone carries. */
inline float cap_distance(const SheetShape& s) { return s.headRadius * std::numbers::pi_v<float> / 2; }

// How worn cloth is set up in games (Unity's Cloth maxDistance, Unreal's painted max distance, the Polygon Treehouse
// poncho): each point may move only a little from where the body carries it, nothing where it is worn and more toward
// the hem, so the cloth keeps its shape and only sways. Starting values, judged with the user in game.
// A bedsheet hangs heavier than a thin cloth (user, 2026-11-09: thicker and stiffer), so its hem sways less.
inline constexpr float kHemSway = 0.12f;  // how far the hem may move from where the body carries it, metres
inline constexpr float kBackstop = 0.05f;  // how far a point may sink behind where the body carries it, metres

/** How far a point `distance` from the centre may sway: 0 within the pin, growing evenly to kHemSway at `far`. */
inline float sway_at(const SheetShape& s, float distance, float far) {
  if (distance <= s.pinRadius || far <= s.pinRadius) return 0.0f;
  return kHemSway * std::min(1.0f, (distance - s.pinRadius) / (far - s.pinRadius));
}

/** Which bones carry a point, and how much: up to four bones by index, heaviest first, their weights adding up to 1. */
struct Weight {
  int32_t bone[4] = {0, 0, 0, 0};
  float weight[4] = {1.0f, 0.0f, 0.0f, 0.0f};
};

/** A bone as a line from its joint to the next joint along the limb (a point when both ends are one), in the sheet's space. */
struct Segment {
  Vec3 from, to;
};

/** How far `p` lies from the segment `b`. */
inline float distance_to(Vec3 p, const Segment& b) {
  const float dx = b.to.x - b.from.x, dy = b.to.y - b.from.y, dz = b.to.z - b.from.z;
  const float length2 = dx * dx + dy * dy + dz * dz;
  float t = length2 > 0 ? ((p.x - b.from.x) * dx + (p.y - b.from.y) * dy + (p.z - b.from.z) * dz) / length2 : 0.0f;
  t = std::clamp(t, 0.0f, 1.0f);
  const float x = b.from.x + t * dx - p.x, y = b.from.y + t * dy - p.y, z = b.from.z + t * dz - p.z;
  return std::sqrt(x * x + y * y + z * z);
}

/** Where the point at `flat` on the flat sheet hangs, in the sheet's space, and its outward normal there. */
inline void hang_point(const SheetShape& s, Vec2 flat, Vec3& at, Vec3& normal) {
  const float d = std::hypot(flat.x, flat.y), distance = std::min(d, s.side / 2);
  const float across = d > 0 ? flat.x / d : 0.0f, forward = d > 0 ? flat.y / d : 0.0f;
  float out = 0, height = 0, normalOut = 0, normalUp = 1;
  hang(s, distance, out, height, normalOut, normalUp);
  if (distance == 0) normalOut = 0, normalUp = 1;
  at = {out * across, height, out * forward};
  normal = {normalOut * across, normalUp, normalOut * forward};
}

// How a loose garment is skinned (user, 2026-11-13: the sheet clung to the arms and folded into itself). Starting
// values, judged with the user in game.
inline constexpr float kLimbShare = 0.5f;  // the most of any point the arms, hands, legs and feet together carry
inline constexpr int kSmoothPasses = 12;   // times each point's weights are blended with its neighbours'
inline constexpr float kSmoothBlend = 0.5f;  // how much of the neighbours' average each pass takes
// How sharply a nearer bone outweighs a further one before the blending (by the inverse of its distance to this power):
// sharp enough that each part of the sheet starts with one or two bones of its own rather than five or six alike, which
// the four bones a point keeps would cut between at random, and the blending then spreads them over the seams between.
inline constexpr float kLooseNearnessPower = 4.0f;

/** Scales the limbs' share of `w` (one weight per bone, adding up to 1; `chain` -1 for a bone of the body) down to
 *  kLimbShare at most, the body's up. */
inline void cap_limbs(std::vector<float>& w, std::span<const int> chain) {
  float limbs = 0.0f, body = 0.0f;
  for (size_t i = 0; i < w.size(); ++i) (chain[i] >= 0 ? limbs : body) += w[i];
  if (limbs <= kLimbShare || body <= 0.0f) return;
  for (size_t i = 0; i < w.size(); ++i) w[i] *= chain[i] >= 0 ? kLimbShare / limbs : (1.0f - kLimbShare) / body;
}

/**
 * Every point's weights for the sheet `m` worn over `bones` (in the sheet's space), as a loose garment is skinned.
 * `chain` gives each bone's limb: -1 for the body down the middle (bone 0 the head), and one number for each arm and
 * each leg, shared by its bones. Each bone pulls a point by nearness (kLooseNearnessPower), the limbs together carry at
 * most kLimbShare of it, and each point's weights are then blended with its neighbours' kSmoothPasses times, so the pull
 * of a limb spreads over the sheet around it and fades rather than stopping at a seam. A point within `cap` of the
 * centre, measured along the sheet, follows the head alone and is held so while the others are blended. Then the
 * body's weights at a point go to its heaviest bone of the body there, as the trunk moves as one, so the four bones a
 * point keeps are not spent on bones of the body pulling alike while a limb's are cut (the middle of the hem keeps both
 * legs). Each point keeps its four heaviest bones, that trunk bone among them, their weights adding up to 1, a limb's
 * share kLimbShare at most. No bones, or `chain` not one per bone: bone 0 alone everywhere.
 */
inline std::vector<Weight> skin_sheet(const SheetMesh& m, std::span<const Segment> bones, std::span<const int> chain,
                                      float cap) {
  const size_t n = m.vertices.size(), nb = bones.size();
  std::vector<Weight> out(n);
  if (nb == 0 || chain.size() != nb) return out;
  std::vector<std::vector<float>> w(n, std::vector<float>(nb, 0.0f));
  std::vector<bool> fixed(n, false);
  for (size_t v = 0; v < n; ++v) {
    if (std::hypot(m.flat[v].x, m.flat[v].y) <= cap) {
      fixed[v] = true;
      w[v][0] = 1.0f;
      continue;
    }
    float sum = 0.0f;
    for (size_t i = 0; i < nb; ++i) {
      w[v][i] = 1.0f / std::pow(std::max(distance_to(m.vertices[v], bones[i]), 0.01f), kLooseNearnessPower);
      sum += w[v][i];
    }
    for (float& x : w[v]) x /= sum;
    cap_limbs(w[v], chain);
  }
  // Each point's neighbours along the triangles' edges.
  std::vector<std::vector<size_t>> near(n);
  for (size_t t = 0; t + 2 < m.triangles.size(); t += 3)
    for (size_t k = 0; k < 3; ++k) {
      const size_t a = static_cast<size_t>(m.triangles[t + k]), b = static_cast<size_t>(m.triangles[t + (k + 1) % 3]);
      if (std::find(near[a].begin(), near[a].end(), b) == near[a].end()) near[a].push_back(b);
      if (std::find(near[b].begin(), near[b].end(), a) == near[b].end()) near[b].push_back(a);
    }
  // Blending weights that each add up to 1 with a limb share of kLimbShare at most keeps both true.
  for (int pass = 0; pass < kSmoothPasses; ++pass) {
    std::vector<std::vector<float>> next = w;
    for (size_t v = 0; v < n; ++v) {
      if (fixed[v] || near[v].empty()) continue;
      for (size_t i = 0; i < nb; ++i) {
        float avg = 0.0f;
        for (const size_t u : near[v]) avg += w[u][i];
        avg /= static_cast<float>(near[v].size());
        next[v][i] = (1.0f - kSmoothBlend) * w[v][i] + kSmoothBlend * avg;
      }
    }
    w = std::move(next);
  }
  for (size_t v = 0; v < n; ++v) {
    if (fixed[v]) continue;  // the head alone, as Weight starts
    // The body's weight to its heaviest bone here, which the trunk moves as one with.
    std::vector<float> merged = w[v];
    size_t trunk = nb;
    for (size_t i = 0; i < nb; ++i)
      if (chain[i] < 0 && (trunk == nb || w[v][i] > w[v][trunk])) trunk = i;
    for (size_t i = 0; i < nb; ++i)
      if (chain[i] < 0 && i != trunk) merged[trunk] += merged[i], merged[i] = 0.0f;
    std::vector<size_t> order(nb);
    for (size_t i = 0; i < nb; ++i) order[i] = i;
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return merged[a] > merged[b]; });
    std::vector<size_t> kept(order.begin(), order.begin() + static_cast<std::ptrdiff_t>(std::min<size_t>(4, nb)));
    const auto body = std::find_if(order.begin(), order.end(), [&](size_t i) { return chain[i] < 0; });
    if (body != order.end() && std::find(kept.begin(), kept.end(), *body) == kept.end()) kept.back() = *body;
    std::vector<float> share(nb, 0.0f);
    float sum = 0.0f;
    for (const size_t i : kept) sum += merged[i];
    for (const size_t i : kept) share[i] = sum > 0.0f ? merged[i] / sum : 0.0f;
    cap_limbs(share, chain);
    std::sort(kept.begin(), kept.end(), [&](size_t a, size_t b) { return share[a] > share[b]; });
    for (size_t k = 0; k < 4; ++k) {
      out[v].bone[k] = static_cast<int32_t>(k < kept.size() ? kept[k] : kept[0]);
      out[v].weight[k] = k < kept.size() ? share[kept[k]] : 0.0f;
    }
  }
  return out;
}

/**
 * The sheet for `s`: its rings (ring_distances), each with as many points as its length round the hanging shape takes
 * at its spacing (at least six), evenly round from half a step off straight ahead, one half the mirror of the other
 * about the centre line, so the sheet is exactly symmetric. Each ring is joined to the next by triangles, each time taking the shorter of the
 * two diagonals the next triangle could have. Then the eye holes: a triangle with a corner inside an eye's oval, or its
 * middle inside it, is cut, and the points round each hole are moved onto the oval (onto_eye_oval). Points no
 * triangle uses are left out. The crown is pinned.
 */
inline SheetMesh make_sheet(const SheetShape& s) {
  SheetMesh m;
  const std::vector<float> rings = ring_distances(s);
  if (rings.size() < 2) return m;
  // Every point on the flat sheet, ring by ring.
  std::vector<Vec2> flat;
  std::vector<size_t> first, count;
  for (const float d : rings) {
    first.push_back(flat.size());
    if (d == 0) {
      flat.push_back({0, 0});
      count.push_back(1);
      continue;
    }
    float out = 0, height = 0, normalOut = 0, normalUp = 0;
    hang(s, d, out, height, normalOut, normalUp);
    const size_t n = std::max<size_t>(6, static_cast<size_t>(std::lround(
                                              2 * std::numbers::pi_v<float> * out / spacing_at(s, d))));
    count.push_back(n);
    // Half a step round from straight ahead, so no point lies on the centre line in front, where it would belong to
    // neither eye; the second half mirrors the first, and an odd ring's middle point lies straight behind.
    const size_t base = flat.size();
    flat.resize(base + n);
    for (size_t j = 0; j < (n + 1) / 2; ++j) {
      const float angle = 2 * std::numbers::pi_v<float> * (static_cast<float>(j) + 0.5f) / static_cast<float>(n);
      flat[base + j] = n % 2 == 1 && j == n / 2 ? Vec2{0.0f, -d} : Vec2{d * std::sin(angle), d * std::cos(angle)};
      flat[base + n - 1 - j] = {-flat[base + j].x, flat[base + j].y};
    }
  }
  std::vector<Vec3> at(flat.size()), normal(flat.size());
  for (size_t v = 0; v < flat.size(); ++v) hang_point(s, flat[v], at[v], normal[v]);
  const auto gap = [&](size_t a, size_t b) {
    const float x = at[a].x - at[b].x, y = at[a].y - at[b].y, z = at[a].z - at[b].z;
    return x * x + y * y + z * z;
  };
  // Ring k joined to ring k + 1, clockwise seen from outside, which is Unity's front face.
  std::vector<int32_t> triangles;
  for (size_t k = 0; k + 1 < rings.size(); ++k) {
    const size_t na = count[k], nb = count[k + 1];
    const auto a = [&](size_t i) { return first[k] + i % na; };
    const auto b = [&](size_t j) { return first[k + 1] + j % nb; };
    const auto add = [&](size_t p, size_t q, size_t r) {
      triangles.insert(triangles.end(), {static_cast<int32_t>(p), static_cast<int32_t>(q), static_cast<int32_t>(r)});
    };
    size_t i = 0, j = 0;
    while (i < na || j < nb) {
      bool inner;  // the next triangle moves on round the inner ring
      if (na == 1 || i == na) inner = false;
      else if (j == nb) inner = true;
      else inner = gap(a(i + 1), b(j)) < gap(a(i), b(j + 1));
      if (inner) {
        add(a(i), b(j), a(i + 1));
        ++i;
      } else {
        add(a(i), b(j), b(j + 1));
        ++j;
      }
      if (na == 1 && j == nb) break;
    }
  }
  // The eye holes.
  std::vector<bool> cut(triangles.size() / 3, false), kept(flat.size(), false), rim(flat.size(), false);
  for (size_t t = 0; t < cut.size(); ++t) {
    const Vec2 p = flat[static_cast<size_t>(triangles[t * 3])], q = flat[static_cast<size_t>(triangles[t * 3 + 1])];
    const Vec2 r = flat[static_cast<size_t>(triangles[t * 3 + 2])];
    const Vec2 middle{(p.x + q.x + r.x) / 3, (p.y + q.y + r.y) / 3};
    cut[t] = in_an_eye(s, p) || in_an_eye(s, q) || in_an_eye(s, r) || in_an_eye(s, middle);
  }
  for (size_t t = 0; t < cut.size(); ++t)
    for (size_t k = 0; k < 3; ++k) {
      const size_t v = static_cast<size_t>(triangles[t * 3 + k]);
      (cut[t] ? rim : kept)[v] = true;
    }
  std::vector<int32_t> index(flat.size(), -1);
  const float far = s.side / 2;  // the hem
  for (size_t v = 0; v < flat.size(); ++v) {
    if (!kept[v]) continue;
    index[v] = static_cast<int32_t>(m.vertices.size());
    const Vec2 p = rim[v] ? onto_eye_oval(s, flat[v]) : flat[v];  // a point round an eye's hole lies on its oval
    Vec3 point{}, n{};
    hang_point(s, p, point, n);
    m.vertices.push_back(point);
    m.normals.push_back(n);
    m.uv.push_back({p.x / s.side + 0.5f, p.y / s.side + 0.5f});
    m.flat.push_back(p);
    m.coefficients.push_back({sway_at(s, std::min(std::hypot(p.x, p.y), far), far), kBackstop});
  }
  for (size_t t = 0; t < cut.size(); ++t)
    if (!cut[t])
      for (size_t k = 0; k < 3; ++k) m.triangles.push_back(index[static_cast<size_t>(triangles[t * 3 + k])]);
  return m;
}

// The sheet's self-collision (user, 2026-11-13: it clipped into itself). Starting values, judged with the user in game.
inline constexpr float kSelfCollisionSpacing = 0.04f;  // the closest spacing (spacing_at) of a point that takes part
inline constexpr float kSelfCollisionShare = 0.8f;      // of the shortest edge between two of them: the distance kept

/**
 * The points of `m` (made for `s`) that take part in its self-collision: those where its spacing is
 * kSelfCollisionSpacing or more, the part that hangs and folds below the shoulders, and not the close-set cap round the
 * head and eyes, whose points lie nearer each other than any distance that would keep folds apart.
 */
inline std::vector<uint32_t> self_collision_points(const SheetMesh& m, const SheetShape& s) {
  std::vector<uint32_t> out;
  for (size_t v = 0; v < m.flat.size(); ++v)
    if (spacing_at(s, std::hypot(m.flat[v].x, m.flat[v].y)) >= kSelfCollisionSpacing)
      out.push_back(static_cast<uint32_t>(v));
  return out;
}

/**
 * The distance `points` of `m` keep from each other (ClothSettings::selfCollisionDistance): kSelfCollisionShare of the
 * shortest edge between two of them as the sheet is made, so none push apart at rest. 0 when no edge joins two of them.
 */
inline float self_collision_distance(const SheetMesh& m, std::span<const uint32_t> points) {
  std::vector<bool> in(m.vertices.size(), false);
  for (const uint32_t v : points)
    if (v < in.size()) in[v] = true;
  float shortest = std::numeric_limits<float>::max();
  for (size_t t = 0; t + 2 < m.triangles.size(); t += 3)
    for (size_t k = 0; k < 3; ++k) {
      const size_t a = static_cast<size_t>(m.triangles[t + k]), b = static_cast<size_t>(m.triangles[t + (k + 1) % 3]);
      if (!in[a] || !in[b]) continue;
      const Vec3 p = m.vertices[a], q = m.vertices[b];
      shortest = std::min(shortest, std::sqrt((p.x - q.x) * (p.x - q.x) + (p.y - q.y) * (p.y - q.y) + (p.z - q.z) * (p.z - q.z)));
    }
  return shortest == std::numeric_limits<float>::max() ? 0.0f : kSelfCollisionShare * shortest;
}

}  // namespace pm::custom::sheet
