#pragma once

// The dev menu's geometry, in canvas units about the canvas centre (the emotes wheel's convention), kept in a
// header so the offline test can check it: the live test of 2026-11-12 found the menu placed too high, with
// the first row's box reaching up into the hint line's box, because the rows were laid out from a constant
// nothing was checked against. A row is a box kPitch tall centred on its line; a text is a box of its own
// height centred on its line.
namespace pm::devmenu::layout {

inline constexpr float kTitleY = 470.0f;   // the heading's line
inline constexpr float kTitleBox = 54.0f;  // what the 42 point heading is drawn in
inline constexpr float kHintY = 424.0f;    // the key hint under it
inline constexpr float kHintBox = 30.0f;
inline constexpr float kRefuseY = -420.0f;  // the refusal line, under the list
inline constexpr float kRefuseBox = 34.0f;

// The list of rows: a scroll view, so a section longer than the screen scrolls rather than running out from
// under the heading or over the refusal line.
inline constexpr float kViewX = 170.0f;     // its centre, clear of the section column on the left
inline constexpr float kViewY = 18.0f;      //
inline constexpr float kViewWide = 1460.0f;  // a row's width, and the clickable bar's
inline constexpr float kViewHigh = 756.0f;   // how much of the list shows at once
inline constexpr float kPitch = 34.0f;       // row to row

// The section column, which is not part of the scrolling list.
inline constexpr float kNavX = -950.0f;    // its left edge, on the canvas
inline constexpr float kNavTop = 372.0f;   // the first section's line
inline constexpr float kNavPitch = 40.0f;  // section to section

// The list's top edge on the canvas: where row 0 begins.
inline constexpr float listTop() { return kViewY + kViewHigh / 2.0f; }
// How far a row's line sits below the content's top edge, and the y place() wants for it (the content is
// pinned to the top edge, so a row's own position is negative from there).
inline constexpr float rowTop(int index) { return static_cast<float>(index) * kPitch; }
inline constexpr float rowOffset(int index) { return -(rowTop(index) + kPitch / 2.0f); }
// The content's height for a section of `rows` rows.
inline constexpr float listHeight(int rows) { return static_cast<float>(rows) * kPitch; }
// The section column's line for the `index`th section.
inline constexpr float navOffset(int index) { return kNavTop - static_cast<float>(index) * kNavPitch; }

// The gap between two boxes given by their centre line and height: negative when they overlap.
inline constexpr float gap(float upperY, float upperBox, float lowerY, float lowerBox) {
  return (upperY - upperBox / 2.0f) - (lowerY + lowerBox / 2.0f);
}

}  // namespace pm::devmenu::layout
