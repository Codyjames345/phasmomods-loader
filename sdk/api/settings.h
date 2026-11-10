#pragma once

/// @file
/// @brief A mod's own settings on the journal's audio page: sliders the player sets as they set the game's master
/// volume (the Emotes mod's volume is one).
///
/// The rows go at the end of the audio page's list, under a heading with the mod's name (menu_rows.h): each a copy of
/// the game's master volume row (GameMyAudioManager.masterVolumeSlider's row: a name, a number and the slider), its
/// slider given a change event of its own so none of the game's listeners run for it, its number following the slider
/// as a percentage. A setting counts as the game's do (GameMyAudioManager): the page's Apply keeps the slider's value,
/// saved to the file the mod names, and leaving the page without Apply puts the slider back. begin_settings hooks the
/// page for that, so enable the mod's hooks after it; where the hooks cannot be made, a change counts at once.
///
/// Main thread. Local only: nothing is sent. Call begin_settings and add_slider from pmmod_start, tick_settings every
/// frame, and clear_settings from pmmod_stop.
/// @ingroup runtime
/// @since Contract 2.3

#include <filesystem>
#include <string>
#include <string_view>

namespace pm::api::settings {

/// A slider a mod declares.
struct Slider {
  std::string id;      ///< unique within the mod; it names the value in the saved file
  std::string label;   ///< its row's name: player-facing text from the mod's string table
  float min = 0.0f;    ///< the lowest value
  float max = 1.0f;    ///< the highest value
  float initial = 1.0f;  ///< the value before the player sets one
};

/// Starts a mod's settings: the heading over its rows and the file its values are saved in (its folder is made when
/// the file is written). Forgets any sliders added before, and hooks the audio page's Apply and closing.
/// @param owner The mod's id, for the log.
/// @param title The heading of the mod's section on the audio page, player-facing: the mod's name as its manifest gives
///        it (pm::mod::shown_name). Empty: no heading.
/// @param file The values' file, normally in the mod's own folder of the host's data directory.
void begin_settings(std::string_view owner, std::string_view title, std::filesystem::path file);

/// Declares a slider, with its saved value if the file has one.
/// @param slider The slider.
/// @return False for an empty id, an id already added, or a range with max not above min.
bool add_slider(const Slider& slider);

/// Each frame: puts the rows on the audio page wherever it is loaded and has none, and shows each slider's value in its
/// number.
void tick_settings();

/// A slider's value as last applied (or saved, or its initial value).
/// @param id The slider's id.
/// @return The value, clamped to its range; 0 for an id not added.
float value(std::string_view id);

/// Takes the rows off the page and forgets the sliders. Their saved values stay in the file.
void clear_settings();

}  // namespace pm::api::settings
