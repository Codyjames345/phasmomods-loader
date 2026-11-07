#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

// The Squatter's bedsheet as a mesh for Unity's Cloth, kept free of game calls so it can be tested offline. A plain
// sheet generated in code, not an art asset (the user's ruling of 2026-11-04, in the Squatter's design).
//
// The sheet is a square grid laid over the head, its centre on the crown. It is made already hanging: each point keeps
// its distance from the centre measured along the sheet, and the sheet follows a head and shoulders profile (a cap
// over the head, a slope out to the shoulders, then straight down). So the cloth starts close to where it settles,
// instead of falling into place from a flat square, which the engine test showed drapes badly
// (sdk/internals/engine-experiments.md). The grid is fine around the head, where the eyes are cut, and coarse
// further out. The two eye holes are mirrored about the centre line, in front of the crown. Only the crown is
// pinned; everything else is free for the cloth to move.
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

/** The sheet's size, grid, eyes and the profile it is made hanging over. Metres. Starting values, tuned in game. */
struct SheetShape {
  float side = 2.0f;            // length of one side of the square sheet
  float fineSpacing = 0.02f;    // grid spacing within fineHalf of the centre lines, where the eyes are cut
  float fineHalf = 0.24f;       // a whole number of fineSpacing
  float coarseSpacing = 0.06f;  // the largest grid spacing further out
  float pinRadius = 0.08f;      // points this close to the centre, measured on the sheet, stay with the head
  float eyeAcross = 0.04f;      // each eye's centre from the centre line
  float eyeForward = 0.16f;     // the eyes' centres in front of the crown, measured on the sheet
  float eyeWidth = 0.04f;       // each eye's rectangle; a hole is it rounded out to whole cells
  float eyeHeight = 0.03f;
  float headRadius = 0.10f;      // the cap the sheet is made over, the crown its top
  float shoulderRadius = 0.24f;  // how far out the sheet hangs below the shoulders
  float shoulderDrop = 0.14f;    // from the bottom of the cap down to the shoulders
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

/**
 * The grid lines along either side of the sheet, from -side/2 to side/2: every fineSpacing within fineHalf of the
 * centre, then evenly to the edge at no more than coarseSpacing. Symmetric about 0, exactly, and holding 0.
 */
inline std::vector<float> sheet_axis(const SheetShape& s) {
  std::vector<float> half{0.0f};
  const int fine = static_cast<int>(std::lround(s.fineHalf / s.fineSpacing));
  for (int i = 1; i <= fine; ++i) half.push_back(static_cast<float>(i) * s.fineSpacing);
  const float rest = s.side / 2 - half.back();
  const int coarse = rest > 0 ? static_cast<int>(std::ceil(rest / s.coarseSpacing - 1e-4f)) : 0;
  for (int i = 1; i <= coarse; ++i) half.push_back(half[static_cast<size_t>(fine)] + rest * i / coarse);
  std::vector<float> axis;
  for (size_t i = half.size(); i-- > 1;) axis.push_back(-half[i]);
  axis.insert(axis.end(), half.begin(), half.end());
  return axis;
}

/** Whether the open cell [x0, x1] x [z0, z1] of the flat sheet overlaps an eye's rectangle, and so is cut. */
inline bool cuts_an_eye(const SheetShape& s, float x0, float x1, float z0, float z1) {
  for (const float side : {-1.0f, 1.0f}) {
    const float x = side * s.eyeAcross;
    if (x0 < x + s.eyeWidth / 2 && x1 > x - s.eyeWidth / 2 && z0 < s.eyeForward + s.eyeHeight / 2 &&
        z1 > s.eyeForward - s.eyeHeight / 2)
      return true;
  }
  return false;
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
    out = s.shoulderRadius;
    height = -r - s.shoulderDrop - (distance - cap - slope);
    normalOut = 1.0f;
    normalUp = 0.0f;
  }
}

/** The sheet for `s`: the grid less the eye cells, made hanging, its crown pinned. Vertices no triangle uses are left out. */
inline SheetMesh make_sheet(const SheetShape& s) {
  SheetMesh m;
  const std::vector<float> axis = sheet_axis(s);
  if (axis.size() < 2) return m;
  const size_t lines = axis.size(), cells = lines - 1;
  std::vector<bool> kept(cells * cells), used(lines * lines);
  for (size_t j = 0; j < cells; ++j)
    for (size_t i = 0; i < cells; ++i) {
      const bool keep = !cuts_an_eye(s, axis[i], axis[i + 1], axis[j], axis[j + 1]);
      kept[j * cells + i] = keep;
      if (keep)
        for (const size_t corner : {j * lines + i, j * lines + i + 1, (j + 1) * lines + i, (j + 1) * lines + i + 1})
          used[corner] = true;
    }
  std::vector<int32_t> index(lines * lines, -1);
  for (size_t j = 0; j < lines; ++j)
    for (size_t i = 0; i < lines; ++i) {
      if (!used[j * lines + i]) continue;
      index[j * lines + i] = static_cast<int32_t>(m.vertices.size());
      const float x = axis[i], z = axis[j];
      const float distance = std::hypot(x, z);
      const float across = distance > 0 ? x / distance : 0.0f, forward = distance > 0 ? z / distance : 0.0f;
      float out = 0, height = 0, normalOut = 0, normalUp = 1;
      hang(s, distance, out, height, normalOut, normalUp);
      if (distance == 0) normalOut = 0, normalUp = 1;
      m.vertices.push_back({out * across, height, out * forward});
      m.normals.push_back({normalOut * across, normalUp, normalOut * forward});
      m.uv.push_back({x / s.side + 0.5f, z / s.side + 0.5f});
      m.flat.push_back({x, z});
      m.coefficients.push_back({distance <= s.pinRadius ? 0.0f : kUnconstrained, kUnconstrained});
    }
  for (size_t j = 0; j < cells; ++j)
    for (size_t i = 0; i < cells; ++i) {
      if (!kept[j * cells + i]) continue;
      const int32_t a = index[j * lines + i], b = index[j * lines + i + 1];
      const int32_t c = index[(j + 1) * lines + i], d = index[(j + 1) * lines + i + 1];
      // Clockwise seen from above, which is Unity's front face.
      m.triangles.insert(m.triangles.end(), {a, c, b, b, c, d});
    }
  return m;
}

}  // namespace pm::custom::sheet
