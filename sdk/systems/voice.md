# Voice chat

What a mod author needs about the players' voices: where each is heard, how to read it, and who hears whom. The entries are
in `sdk/map/PlayerVoice.h`; their `///` notes are the authority. Facts were read from decompiled code of Steam build
25811160 (`re/decomp.mjs <key>`) unless said otherwise.

**Recording a voice needs the player's consent.** A mod that keeps what players say sets `recordsVoice` in its manifest, so
the consent notice says so before anyone agrees. A contract only begins once every player's mods are confirmed
([The lobby's rules](../README.md#the-lobbys-rules)), so every voice in it belongs to a player who agreed.

## Where a voice is heard

- **The game's voice chat is Vivox.** Each player's `PlayerVoice` holds two taps on that player's Vivox participant, each a
  `VivoxParticipantTap` on an object with its own `AudioSource`:
  - `PlayerVoice.localTap`, the positional voice, heard where the player stands, with the volume, spatial blend and
    rolloff of the player's own voice source;
  - `PlayerVoice.globalTap`, made by `PlayerVoice.CreateGlobalAudioTap`, which plays in the channel mix: the walkie-talkie.
- **Reading a voice.** `VivoxParticipantTap.DoAudioFilterRead` fills the tap's buffer with the participant's samples,
  interleaved, and returns a negative status on failure. A hook on it sees every other player's voice as samples inside the
  game, on the main thread. Custom Ghosts' Echo records this way. A client's own voice is not in its taps: it is never played
  back to the speaker.
- **Push-to-talk.** `PlayerVoice` keeps the player's voice mode (push to talk, or toggle) and whether it is pressed, and the
  walkie-talkie has its own press. None of these is mapped yet. `VivoxServiceInternal.MuteInputDevice` and
  `VivoxServiceInternal.UnmuteInputDevice` mute and unmute this client's microphone in every channel.

## Who hears whom

- **The dead are silent to the living.** `VoiceVolume.Update` sets a dead player's `VoiceVolume.deathMultiplier` to 0 while
  this client's investigator is alive, and to 1 otherwise. So a dead player is heard by the dead, and by nobody alive.
- `VoiceVolume.Update` reads this client's local Player with no null check, so it throws every frame on a client with none.
  A mod that takes the local player out (as Playable Ghost does for the ghost player) answers for it.

## Speech recognition

Each client runs the game's own offline voice recognition on its own player. Its only caller of
`PhraseListenerController.OnPhraseRecognized` is the game's speech recognition controller, and it runs for every phrase. In a
contract the game's handling returns at once unless the ghost hunts (`GhostAI.isHunting`); only then, after checks on the ghost's
room, does it compare the phrase with the ghost's name (`GhostInfo.name`), its list of phrases and the ghost's type name. So
a hook still sees the phrases said outside a hunt, which is how Ghost Tells' Spirit hears its name.
`PhraseListenerController.NetworkedReaction` is the ghost's reaction to its name, on the client that runs the ghost
(inferred from its name, not decompiled).

## The game's sound as a whole

Not run in game: these were read while planning the archived Replay mod (`archive/replay`).

- Unity's `AudioRenderer` mutes the live output while it records, so it is no way to hear the game.
- Windows' per-process loopback needs Windows 11, or Windows 10 build 20348 or later.
- The game's mix can be read from `AudioListener`'s output data, through the engine's own call, on Windows 10 too.
