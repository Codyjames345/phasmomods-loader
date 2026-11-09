#pragma once

#include <cmath>

#include "Transform.h"

// Unity's rotation arithmetic on phasmo::Quaternion and phasmo::Vector3, kept free of game calls so the Squatter's door
// and sheet can be tested offline. The formulas are Unity's own (Quaternion's operators and Quaternion.Euler).
namespace pm::custom {

/** Unity's quaternion product a * b: b's rotation, then a's. */
inline phasmo::Quaternion qmul(phasmo::Quaternion a, phasmo::Quaternion b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y + a.y * b.w + a.z * b.x - a.x * b.z,
          a.w * b.z + a.z * b.w + a.x * b.y - a.y * b.x, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

/** The inverse of a unit quaternion. */
inline phasmo::Quaternion qinv(phasmo::Quaternion q) { return {-q.x, -q.y, -q.z, q.w}; }

/** `v` turned by the unit quaternion `q`, as Unity's q * v. */
inline phasmo::Vector3 qrot(phasmo::Quaternion q, phasmo::Vector3 v) {
  const phasmo::Quaternion r = qmul(qmul(q, {v.x, v.y, v.z, 0}), qinv(q));
  return {r.x, r.y, r.z};
}

/** Quaternion.Euler for `e` degrees: z first, then x, then y. */
inline phasmo::Quaternion euler_quat(phasmo::Vector3 e) {
  constexpr float half = 3.14159265358979f / 360.0f;
  const phasmo::Quaternion x{std::sin(e.x * half), 0, 0, std::cos(e.x * half)};
  const phasmo::Quaternion y{0, std::sin(e.y * half), 0, std::cos(e.y * half)};
  const phasmo::Quaternion z{0, 0, std::sin(e.z * half), std::cos(e.z * half)};
  return qmul(qmul(y, x), z);
}

}  // namespace pm::custom
