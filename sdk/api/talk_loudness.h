#pragma once

/// @file
/// @brief A mod's sound counted as a player talking, so the ghost hears it as it hears a voice: through the value its
/// voice checks read, PlayerAudio.loudness, with every rule they already apply (range, floor, room, hunts).
///
/// The game refreshes a player's loudness once a second on every client, from the Vivox voice's audio energy less 0.4,
/// and the ghost's checks (PlayerAudio.IsTalkingLoudly from GhostHuntState.Hears and LevelRoom.ArePlayersTalkingInRoom)
/// read it on the master, the client that runs the ghost, and zero it when they answer true. A mod measures its sound
/// in the same scale (loudness_of, or an Envelope made once from the sound's samples) and raises the player's loudness
/// on the master at the game's own pace (Pace), never lowering a louder voice. The game's side is in
/// sdk/systems/voice.md, "How loud a player talks".
///
/// Main thread for add_talk_loudness; the rest is free of game calls.
/// @ingroup runtime
/// @since Contract 2.3

#include <span>
#include <vector>

namespace pm::api::talk {

/// The loudness above which the game counts a player as talking loudly: the threshold its checks pass to
/// PlayerAudio.IsTalkingLoudly.
inline constexpr float kTalkingLoudly = 0.1f;
/// What the game takes from a voice's energy to make its loudness (PlayerAudio.GetPlayerVoiceAmplitude).
inline constexpr float kEnergyOffset = 0.4f;
/// How often the game refreshes a player's loudness, in seconds (PlayerAudio.Update).
inline constexpr float kRefreshSeconds = 1.0f;
/// The energy, 0 to 1, a sound of RMS amplitude 1 counts as, scaled down evenly: a starting value, judged with the user
/// in game, since a Vivox voice's energy is not a plain RMS.
inline constexpr float kEnergyPerRms = 3.0f;
/// The length of one window of an Envelope, in seconds.
inline constexpr float kWindowSeconds = 0.1f;

/// A sound's loudness in the game's scale: its RMS amplitude times kEnergyPerRms, held to 0..1 as an energy, less
/// kEnergyOffset.
/// @param samples Samples from -1 to 1 (one channel, or interleaved: the measure is the same).
/// @return From -0.4 (silence, or no samples) to 0.6.
float loudness_of(std::span<const float> samples);

/// A sound's loudness over its length, measured once, in windows of kWindowSeconds.
struct Envelope {
  std::vector<float> windows;  ///< loudness_of each window, in order

  /// The loudness at a point of the sound.
  /// @param seconds From the sound's start; before it reads the first window.
  /// @return The window's loudness; past the end, or with no windows, silence (-kEnergyOffset).
  float at(float seconds) const;
};

/// Measures a sound's Envelope.
/// @param samples Mono samples from -1 to 1.
/// @param sampleRate Samples a second; 0 or less gives an empty envelope.
/// @return Its envelope.
Envelope envelope_of(std::span<const float> samples, int sampleRate);

/// Raises a player's talking loudness to `loudness` when it is lower, as if they talked that loud. Call it on the
/// master, at Pace's rate, for the ghost there to hear it; the game's next refresh sets the voice's own value again.
/// @param player A Player (any client's); null does nothing.
/// @param loudness In the game's scale (loudness_of).
void add_talk_loudness(void* player, float loudness);

/// The game's refresh pace: due once at the start, then once every kRefreshSeconds.
struct Pace {
  float last = -1.0f;  ///< when it was last due; negative before the first

  /// Whether a raise is due now; when it is, the next is a refresh later.
  /// @param now Seconds on any steady clock.
  /// @return True at the first call and once each kRefreshSeconds after.
  bool due(float now) {
    if (last >= 0.0f && now - last < kRefreshSeconds) return false;
    last = now;
    return true;
  }
};

}  // namespace pm::api::talk
