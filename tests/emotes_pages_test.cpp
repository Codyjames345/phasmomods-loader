// Offline tests for the Emotes wheel's pages (mods/emotes/wheel_pages.h): how many pages of kSegments emotes an emote
// list needs, turning pages both ways, which emote a page's segment holds, a page kept while the list shrinks, and what
// a mouse scroll does to the page. Run: build\emotes_pages_test.exe
#include <cstdio>

#include "check.h"
#include "emote_rule.h"
#include "wheel_pages.h"

namespace {

using namespace pm::emotes;

void page_count_rounds_up() {
  CHECK(pages::page_count(0) == 1);   // no emotes: one empty page
  CHECK(pages::page_count(1) == 1);
  CHECK(pages::page_count(kSegments) == 1);
  CHECK(pages::page_count(9) == 2);
  CHECK(pages::page_count(16) == 2);
  CHECK(pages::page_count(17) == 3);
}

void turn_wraps_both_ways() {
  // Seventeen emotes: three pages.
  CHECK(pages::turn(0, -1, 17) == 2);  // before the first page comes the last
  CHECK(pages::turn(2, 1, 17) == 0);   // past the last page comes the first
  CHECK(pages::turn(1, 5, 17) == 0);   // five turns on three pages: two rounds and a stop
  CHECK(pages::turn(1, 0, 17) == 1);   // no turn
  CHECK(pages::turn(0, -3, 9) == 1);   // two pages: three back is one back
}

void emote_at_is_minus_one_past_the_end() {
  // Twelve emotes: page 1 holds 8, 9, 10 and 11, and four spare segments.
  CHECK(pages::emote_at(1, 3, 12) == 11);
  CHECK(pages::emote_at(1, 4, 12) == -1);
  CHECK(pages::emote_at(1, 8, 12) == -1);   // off the wheel
  CHECK(pages::emote_at(1, -1, 12) == -1);  // before the first segment
  CHECK(pages::emote_at(0, 0, 12) == 0);
  CHECK(pages::emote_at(0, 7, 12) == 7);
  CHECK(pages::emote_at(0, 3, 0) == -1);  // no emotes at all
}

void clamp_page_after_emotes_shrink() {
  CHECK(pages::clamp_page(2, 9) == 1);   // the wheel held three pages and the list fell to two
  CHECK(pages::clamp_page(-3, 9) == 0);
  CHECK(pages::clamp_page(1, 9) == 1);   // already in range
  CHECK(pages::clamp_page(0, 0) == 0);
}

void scroll_down_is_next_page() {
  CHECK(pages::scroll_step(-120.0f) == 1);  // the game's scroll: scrolling down is negative
  CHECK(pages::scroll_step(120.0f) == -1);
  CHECK(pages::scroll_step(0.0f) == 0);
}

}  // namespace

int main() {
  page_count_rounds_up();
  turn_wraps_both_ways();
  emote_at_is_minus_one_past_the_end();
  clamp_page_after_emotes_shrink();
  scroll_down_is_next_page();
  std::printf("%s\n", g_failures ? "emotes_pages tests FAILED" : "emotes_pages tests passed");
  return g_failures ? 1 : 0;
}
