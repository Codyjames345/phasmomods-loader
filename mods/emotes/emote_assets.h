#pragma once

// The Emotes mod's own text keys. Its emotes, and each content pack's, are listed in a catalogue (catalogue.h): the mod's
// own is assets/emotes.json, with each song's measured loop. The songs mark no loop of their own: from the author's
// rough points, each was measured where the waveform repeats its start (normalised cross-correlation, 0.983 to 1.000),
// and the bundle holds each song cut to its loop, sample for sample, which the game loops itself.

#include "text_table.h"

namespace pm::emotes {

/// The key of the label of the mod's key in the game's keybinds menu.
inline constexpr const char* kKeyLabel = pm::i18n::key("emotes.key-label");

/// The key of the label of the mod's volume slider on the journal's audio page.
inline constexpr const char* kVolumeLabel = pm::i18n::key("emotes.volume-label");

}  // namespace pm::emotes
