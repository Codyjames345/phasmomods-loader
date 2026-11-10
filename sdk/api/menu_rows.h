#pragma once

/// @file
/// @brief Rows of a mod's own in the lists of the game's journal menus (the keybinds page, the audio page): what
/// keybinds.h and settings.h share.
///
/// The menus are each player's journal's (sdk/systems/menus.md, "Where the menu is"). A list is a layout group, its rows
/// each a name text with a TextLocaliserUI and the row's controls, with a divider (an image with no name text) between
/// rows, in a scroll content sized to the game's rows. A mod's section goes at the end of a list: a heading with the
/// mod's name, then its rows, each a copy of a game row; the scroll content grows by what was added. space_out then
/// lays the whole list out with exactly one line between every pair of visible rows (the user, 2026-11-10), the list's
/// own dividers put away and a copied line between each pair; where the page's own dividers are heavier than the
/// keybinds page's lines, line_divider takes a line from another menu's list to copy.
///
/// Main thread. Local only.
/// @ingroup runtime
/// @since Contract 2.3

#include <string>
#include <vector>

#include "held.h"

namespace pm::api::rows {

/// How much larger a heading's name is than a row's (the user's ruling, 2026-11-10: bigger and bold).
inline constexpr float kHeadingScale = 1.25f;

/// The list `row` sits in: its parent, laid out by a HorizontalOrVerticalLayoutGroup, under the local player's own
/// PCPlayer when there is one. Null for anything else: the training whiteboards' rows sit on no layout, and another
/// player's journal holds a menu too.
/// @param row A component or Transform of the row's root.
/// @return The list's Transform, or null.
void* journal_list(void* row);

/// A divider beside `row` in its list: a sibling with no name text (no TextLocaliserUI under it). The game's lists put
/// one between rows.
/// @param row A component or Transform of the row's root.
/// @return The divider's GameObject, or null.
void* divider_beside(void* row);

/// A divider to copy between a mod's rows, one line to a line: the keybinds page's rows sit one line apart, which is how
/// the player wants a mod's rows on the audio page too (the user, 2026-11-10: the audio page's own dividers are double
/// rules between its groups). So it comes from another journal menu's list when one is loaded, and from beside `row`
/// when none is.
/// @param row A component or Transform of a row of the list to divide.
/// @param fromOtherPage Set to whether it came from another list, for the log; may be null.
/// @return The divider's GameObject, or null.
void* line_divider(void* row, bool* fromOtherPage = nullptr);

/// What space_out changed in a list, to give back with lines_back.
struct Lines {
  std::vector<Held> made;  ///< dividers it copied into the list
  std::vector<Held> hidden;  ///< the list's own dividers it switched off
  float added = 0.0f;      ///< the height the change adds, spacing counted (taken_height units)
};

/// Lays a list out with exactly one line between every pair of rows the player can see (the user, 2026-11-10: "the
/// double lines are the problem. Each row should have one line between them"). A row is a child with a name text or
/// children of its own, a divider a child with neither; a switched-off child is neither, since the layout gives it no
/// room. It switches the list's own dividers off and puts a copy of `line` before every row but the first, which is what
/// the audio page needs: it hides a row between two of its dividers, leaving two side by side, and it groups two of its
/// rows with no divider between them at all. The copies are switched on, so `line` may be
/// one of the list's own dividers. With no `line`, a run of dividers between two rows keeps its first and the rest go,
/// so the rows stay divided. Dividers at either end of the list go either way. Nothing moves in the list except the
/// copies.
/// @param list The list (journal_list).
/// @param line A divider to copy (line_divider, or one of the list's own); null keeps one of the list's own instead.
/// @return What was copied and switched off, and the height the change adds; give it back with lines_back.
Lines space_out(void* list, void* line);

/// Destroys the copies and switches back on the dividers space_out switched off, leaving the list as it was.
/// @param lines What space_out returned; emptied.
void lines_back(Lines& lines);

/// A row's name text: the TextMeshProUGUI of its first TextLocaliserUI.
/// @param row A component or Transform of the row's root.
/// @return The text, or null.
void* name_text(void* row);

/// Writes `utf8` on a copied row's name text, taking its TextLocaliserUI off first: it writes the game's words for its
/// key whenever it wakes, and a copy made while the menu is hidden wakes when the player opens it, after this.
/// @param label A name text (name_text).
/// @param utf8 The text; TextMeshPro rich text.
void label(void* label, const std::string& utf8);

/// Makes a heading of a copied row's name text: the mod's name, bold, at a fixed size kHeadingScale times the row
/// name's. The game's name text sizes itself to fit its row, which would undo a larger size.
/// @param label A name text (name_text) of the heading's copy.
/// @param title The heading, plain text.
/// @return The size it was given, or 0 when its size could not be read.
float heading(void* label, const std::string& title);

/// Hides every child of `root` but the one holding `keep`: a heading shows only its name.
/// @param root The copy's root Transform.
/// @param keep A Transform to keep shown (the name text's).
void show_only(void* root, void* keep);

/// The height a child takes in its list: its own and the layout's spacing before it.
/// @param object A GameObject in the list.
/// @param list The list (journal_list).
/// @return The height in the list's units, or 0.
float taken_height(void* object, void* list);

/// A scroll content made taller for a mod's rows, to give back when they go.
struct Grown {
  Held rect;  ///< the RectTransform made taller: a list's scroll content
  float by = 0.0f;  ///< by how much
};

/// Makes the list's scroll content taller by `by`, so the scroll reaches what was added at its end: the list itself, or
/// its parent when the list is stretched to its parent's height (the game's Layout in its Content, whose height is
/// fixed to the game's own rows).
/// @param list The list (journal_list).
/// @param by The height added.
/// @return What to give back with shrink; empty when nothing grew.
Grown grow(void* list, float by);

/// Gives back what grow made taller, if the content is still there.
/// @param grown What grow returned.
void shrink(const Grown& grown);

}  // namespace pm::api::rows
