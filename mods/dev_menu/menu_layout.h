#pragma once

#include <vector>

// The dev menu's geometry, in canvas units about the canvas centre (the emotes wheel's convention), kept in a header
// so the offline test can check it: the live test of 2026-11-12 found rows reaching up into the hint line, because
// the layout was a handful of constants nothing was compared against.
//
// The look (the user's ask, 2026-11-12: a clean UI in the game's own manner that makes things easy to find): one
// slab like the truck's equipment rack. A header with the title and three status lamps; the sections down the left;
// the current section's groups as flat panels in two columns of a scrolling area; a footer with the refusal on the
// left and the key hint on the right. Every switch carries a small rectangular lamp, the EMF reader's own shape,
// lit in the flashlight's amber when on.
namespace pm::devmenu::layout {

// The slab: everything sits inside it, clear of a 1920 by 1080 canvas's edges.
inline constexpr float kSlabWide = 1800.0f;
inline constexpr float kSlabHigh = 1000.0f;

// The header band at the slab's top: the title on the left, the status lamps on the right, a rule under both.
inline constexpr float kHeaderY = 455.0f;    // the header's line
inline constexpr float kHeaderBox = 56.0f;
inline constexpr float kRuleY = 418.0f;      // the rule under the header
inline constexpr float kSlabLeft = -kSlabWide / 2.0f + 30.0f;   // the inner left edge text starts at
inline constexpr float kSlabRight = kSlabWide / 2.0f - 30.0f;   // the inner right edge

// The footer band at the slab's bottom: the refusal on the left, the hint on the right.
inline constexpr float kFooterY = -470.0f;
inline constexpr float kFooterBox = 34.0f;

// The section column.
inline constexpr float kNavLeft = kSlabLeft;   // its left edge
inline constexpr float kNavWide = 250.0f;
inline constexpr float kNavTop = 375.0f;       // the first section's line
inline constexpr float kNavPitch = 52.0f;      // section to section
inline constexpr float kNavBar = 6.0f;         // the lit bar beside the current section

// The scrolling area the groups live in, right of the section column.
inline constexpr float kViewLeft = kNavLeft + kNavWide + 24.0f;
inline constexpr float kViewRight = kSlabRight;
inline constexpr float kViewTop = kRuleY - 18.0f;
inline constexpr float kViewBottom = kFooterY + kFooterBox / 2.0f + 18.0f;
inline constexpr float kViewWide = kViewRight - kViewLeft;
inline constexpr float kViewHigh = kViewTop - kViewBottom;
inline constexpr float kViewX = (kViewLeft + kViewRight) / 2.0f;   // its centre
inline constexpr float kViewY = (kViewTop + kViewBottom) / 2.0f;

// The groups: flat panels, two columns of them.
inline constexpr int kColumns = 2;
inline constexpr float kColumnGap = 18.0f;
inline constexpr float kCardGap = 18.0f;       // panel to panel down a column
inline constexpr float kCardWide = (kViewWide - kColumnGap * (kColumns - 1)) / kColumns;
inline constexpr float kCardHead = 44.0f;      // the group's name
inline constexpr float kCardPad = 14.0f;       // inside a panel: its left and right margin, and below its last row
inline constexpr float kPitch = 40.0f;         // row to row
inline constexpr float kRowWide = kCardWide - 2.0f * kCardPad;

// A row's parts, from the row's own left edge.
inline constexpr float kLampWide = 26.0f;      // a switch's lamp
inline constexpr float kLampHigh = 12.0f;
inline constexpr float kStepWide = 40.0f;      // a picker's step buttons
inline constexpr float kStepHigh = 32.0f;
inline constexpr float kPickerWide = 330.0f;   // a picker's value with its two step buttons

// The header's three status lamps, from the slab's right edge leftward.
inline constexpr float kStatusPitch = 230.0f;

// A panel's height for `rows` rows.
inline constexpr float card_height(int rows) {
  return kCardHead + static_cast<float>(rows) * kPitch + kCardPad;
}

// A row's line inside its panel, measured down from the panel's top.
inline constexpr float row_line(int row) { return kCardHead + (static_cast<float>(row) + 0.5f) * kPitch; }

// The section column's line for the `index`th section.
inline constexpr float navOffset(int index) { return kNavTop - static_cast<float>(index) * kNavPitch; }

// The gap between two boxes given by their centre line and height: negative when they overlap.
inline constexpr float gap(float upperY, float upperBox, float lowerY, float lowerBox) {
  return (upperY - upperBox / 2.0f) - (lowerY + lowerBox / 2.0f);
}

/** Where a section's groups go: each in the column that is shorter so far (the first on a tie), in the order the
 *  section lists them, so related groups stay near each other and the two columns end close together. */
struct Placed {
  int column = 0;   // 0 left, 1 right
  float top = 0.0f; // the panel's top edge, measured down from the content's top
};
inline std::vector<Placed> flow(const std::vector<float>& heights) {
  std::vector<Placed> out;
  float columns[kColumns] = {};
  for (const float h : heights) {
    int c = 0;
    for (int i = 1; i < kColumns; ++i)
      if (columns[i] < columns[c] - 0.5f) c = i;
    out.push_back({c, columns[c]});
    columns[c] += h + kCardGap;
  }
  return out;
}

/** The content's height for a section's flowed groups: its taller column, without the gap after its last panel. */
inline float content_height(const std::vector<float>& heights) {
  const std::vector<Placed> placed = flow(heights);
  float bottom = 0.0f;
  for (size_t i = 0; i < placed.size(); ++i)
    if (placed[i].top + heights[i] > bottom) bottom = placed[i].top + heights[i];
  return bottom;
}

/** A panel's centre x within the content, whose centre is 0. */
inline constexpr float column_x(int column) {
  return -kViewWide / 2.0f + kCardWide / 2.0f + static_cast<float>(column) * (kCardWide + kColumnGap);
}

}  // namespace pm::devmenu::layout
