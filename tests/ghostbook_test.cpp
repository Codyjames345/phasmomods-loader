// Offline tests for the ghost player's journal text (src/ghostbook.h). Run: build\ghostbook_test.exe
#include "ghostbook.h"

#include <string>

#include "check.h"

using namespace pg;
using namespace pg::ghostbook;

namespace {

bool has(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

Facts facts() {
  Facts f;
  f.up = "W";
  f.left = "A";
  f.down = "S";
  f.right = "D";
  f.interact = "E";
  f.primary = "LMB";
  f.secondary = "RMB";
  f.fuse = "T";
  f.journal = "J";
  f.pause = "Esc";
  return f;
}

void guide_uses_the_players_own_bindings() {
  Facts f = facts();
  f.interact = "F";
  f.journal = "Tab";
  const Book b = write(f);
  CHECK(has(b.pages[kHomeLeft].body, "[W]") && has(b.pages[kHomeLeft].body, "[D]"));
  CHECK(has(b.pages[kHomeLeft].body, "[Tab]"));
  CHECK(has(b.pages[kItemsLeft].body, "[F]") && !has(b.pages[kItemsLeft].body, "[E]"));
  CHECK(has(b.pages[kHomeRight].body, "[LMB]"));
}

void sheet_waits_for_the_ghost_then_names_it() {
  Facts f = facts();
  CHECK(has(write(f).pages[kOverviewLeft].body, "chosen as the haunting begins"));
  f.type = "Demon";
  f.room = "Kitchen";
  const std::string body = write(f).pages[kOverviewLeft].body;
  CHECK(has(body, "Demon") && has(body, "Kitchen") && !has(body, "chosen as"));
}

void hunt_page_marks_low_enough_sanity() {
  Facts f = facts();
  CHECK(has(write(f).pages[kOverviewRight].body, "not known yet"));
  f.status.sanity = 62;
  f.status.huntBelow = 50;
  std::string body = write(f).pages[kOverviewRight].body;
  CHECK(has(body, "62%") && has(body, "50%") && !has(body, "low enough"));
  f.status.sanity = 48;
  body = write(f).pages[kOverviewRight].body;
  CHECK(has(body, "low enough"));
  f.status.flags = session::kFocusSanity | session::kHuntAvailable;
  body = write(f).pages[kOverviewRight].body;
  CHECK(has(body, "target's sanity") && has(body, "A hunt is available"));
}

void charges_count_whole_actions() {
  Facts f = facts();
  f.status.interact = 255;
  f.status.events = 1;
  f.status.fuses = 2;
  CHECK(has(write(f).pages[kOverviewRight].body, "<b>2</b> interaction, <b>1</b> event, <b>2</b> fuse box"));
  f.status.interact = 200;  // between one and two: one usable
  CHECK(has(write(f).pages[kOverviewRight].body, "<b>1</b> interaction"));
  f.status.interact = session::encode_charge(1.0f, 2.0f);  // exactly one
  CHECK(has(write(f).pages[kOverviewRight].body, "<b>1</b> interaction"));
  f.status.type = 9;  // a Demon: its ability is counted too
  f.ability = pg::ability::of(9);
  f.status.abilities = 1;
  CHECK(has(write(f).pages[kOverviewRight].body, "<b>1</b> ability"));
}

void the_ability_is_named_only_for_a_type_that_has_one() {
  Facts f = facts();
  f.status.type = 10;  // Yurei
  f.ability = pg::ability::of(10);
  std::string items = write(f).pages[kItemsLeft].body;
  CHECK(has(items, "[RMB]") && has(items, "Door slam") && has(items, "Slam a door"));
  f.status.type = 0;  // Spirit: no ability of its own
  f.ability = nullptr;
  items = write(f).pages[kItemsLeft].body;
  CHECK(!has(items, "[RMB]") && has(items, "no ability of its own"));
  CHECK(!has(write(f).pages[kOverviewRight].body, "ability"));
}

void the_fuse_box_uses_the_torch_key() {
  const std::string items = write(facts()).pages[kItemsLeft].body;
  CHECK(has(items, "[T]") && has(items, "Fuse box") && has(items, "from anywhere"));
}

void sheet_says_when_the_ghost_is_bound_to_its_room() {
  Facts f = facts();
  CHECK(!has(write(f).pages[kOverviewRight].body, "bound"));
  f.status.flags = session::kBound;
  CHECK(has(write(f).pages[kOverviewRight].body, "bound to your favourite room"));
  CHECK(has(write(f).pages[kHomeRight].body, "draw you back to your favourite room"));
}

void evidence_spread_reads_as_theirs() {
  const Book b = write(facts());
  CHECK(has(b.evidenceTitle, "Their") && has(b.ghostsTitle, "Their"));
  CHECK(has(b.ghostsLine, "investigators"));
}

}  // namespace

int main() {
  guide_uses_the_players_own_bindings();
  sheet_waits_for_the_ghost_then_names_it();
  hunt_page_marks_low_enough_sanity();
  charges_count_whole_actions();
  the_ability_is_named_only_for_a_type_that_has_one();
  the_fuse_box_uses_the_torch_key();
  sheet_says_when_the_ghost_is_bound_to_its_room();
  evidence_spread_reads_as_theirs();
  std::printf(g_failures ? "%d check(s) failed\n" : "all ghostbook tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
