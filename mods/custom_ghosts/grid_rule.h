#pragma once

#include <cmath>
#include <vector>

// Arithmetic for the journal's ghost grids, kept free of game calls so it can be tested offline.
namespace pg::custom {

struct PlacedCell {
  float x, y;           // anchored position
  float width, height;  // size
};

struct GridShape {
  float cellWidth = 0, cellHeight = 0;
  int columns = 0;  // none when nothing is placed
};

/** The grid that lays children out as they were placed by hand, in rows from the first child: its
 * size is a cell, and a row holds as many children as share its height. */
inline GridShape baked_grid(const std::vector<PlacedCell>& children) {
  GridShape s;
  if (children.empty()) return s;
  s.cellWidth = children.front().width;
  s.cellHeight = children.front().height;
  for (const PlacedCell& c : children)
    if (std::fabs(c.y - children.front().y) < 0.5f) ++s.columns;
  return s;
}

/** The cell height at which `after` children fill the height `before` children did, `columns` to a row. */
inline float fitted_cell_height(float cellHeight, float spacing, int columns, int before, int after) {
  if (columns <= 0 || before <= 0 || after <= before) return cellHeight;
  const int rowsBefore = (before + columns - 1) / columns, rowsAfter = (after + columns - 1) / columns;
  if (rowsAfter <= rowsBefore) return cellHeight;
  return (cellHeight + spacing) * rowsBefore / rowsAfter - spacing;
}

}  // namespace pg::custom
