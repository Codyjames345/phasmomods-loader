#pragma once

// The Emotes wheel's pages: the wheel shows kSegments emotes at a time (emote_rule.h), so a list with more than that
// turns pages. How many pages a list needs, which page the wheel shows after a turn, a shrink or a scroll, and which
// emote a page's segment holds. Free of game calls.

#include "emote_rule.h"

namespace pm::emotes::pages {

/// How many pages `emotes` emotes need, at kSegments to a page. An empty list still has one empty page, so the wheel
/// always has a page to show.
/// @param emotes How many emotes the wheel holds.
inline int page_count(int emotes) {
  if (emotes <= 0) return 1;
  return (emotes + kSegments - 1) / kSegments;
}

/// A page brought into the wheel's pages. The wheel keeps its page number while the list changes, so a shrink can
/// leave it past the last page.
/// @param page The page wanted, possibly out of range.
/// @param emotes How many emotes the wheel holds.
/// @return The page, within 0 to page_count(emotes) - 1.
inline int clamp_page(int page, int emotes) {
  const int last = page_count(emotes) - 1;
  if (page < 0) return 0;
  return page > last ? last : page;
}

/// The page `steps` away from `page`, wrapping both ways: one turn before the first page shows the last, one past the
/// last shows the first.
/// @param page The page now.
/// @param steps Pages to turn, negative turning back.
/// @param emotes How many emotes the wheel holds.
inline int turn(int page, int steps, int emotes) {
  const int count = page_count(emotes);
  const int wanted = clamp_page(page, emotes) + steps;
  const int wrapped = wanted % count;
  return wrapped < 0 ? wrapped + count : wrapped;
}

/// Which emote a page's segment holds, as an index into the wheel's list. The last page's spare segments, and any
/// segment off the wheel, hold nothing.
/// @param page The page shown.
/// @param segment The segment of the wheel, 0 to kSegments - 1.
/// @param emotes How many emotes the wheel holds.
/// @return The index, or -1.
inline int emote_at(int page, int segment, int emotes) {
  if (segment < 0 || segment >= kSegments) return -1;
  const int index = page * kSegments + segment;
  return index >= 0 && index < emotes ? index : -1;
}

/// What a mouse scroll does to the page. The game's scroll is negative scrolling down, and down turns to the next
/// page.
/// @param scrollY The wheel's scroll this frame, the game's sign.
/// @return +1 for the next page, -1 for the previous, 0 for none.
inline int scroll_step(float scrollY) {
  if (scrollY < 0.0f) return 1;
  if (scrollY > 0.0f) return -1;
  return 0;
}

}  // namespace pm::emotes::pages
