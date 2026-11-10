#pragma once

/// @file
/// @brief Rows of a mod's own in the lists of the game's journal menus (the keybinds page, the audio page): what
/// keybinds.h and settings.h share.
///
/// The menus are each player's journal's (sdk/systems/menus.md, "Where the menu is"). A list is a layout group, its rows
/// each a name text with a TextLocaliserUI and the row's controls, with a divider (an image with no name text) between
/// rows, in a scroll content sized to the game's rows. A mod's section goes at the end of a list: a divider, a heading
/// with the mod's name, then a divider before each of its rows, each a copy of a game row; the scroll content grows by
/// what was added.
///
/// Main thread. Local only.
/// @ingroup runtime
/// @since Contract 2.3

#include <string>

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
