# Mod ideas

Ideas for future mods, not yet planned or built. Each is recorded as the user gave it, with notes
on what already exists in the repository that touches it.

## Dead players through the Spirit Box

Dead players are audible through the Spirit Box, in a distorted voice.

Notes: in vanilla the game silences a dead player's voice for living listeners
(`VoiceVolume.Update`; see `mods/playable_ghost/senses.cpp`). The Echo in Custom Ghosts already
plays investigators' recorded phrases back through their own voice settings, which may be a
starting point for the voice pipeline. Afterlife (`archive/afterlife`) is the other archived
dead-player mod.

## Doppelganger event

A ghost can take the form of a player for a few seconds.

Status: partly done. Playable Ghost has it as the ghost player's "Disguise" (`docs/design.md`,
`mods/playable_ghost/form.cpp`): any type may take the form of the investigator nearest where it
looks for five seconds, at the cost of an event charge. The game's own `GhostEventPlayer` is only a
particle cloud, so every client bakes that investigator's body into a still figure that moves and
turns with the ghost. There is no version for the game's own AI ghosts yet; this idea would bring
the event to normal contracts without a ghost player.

## Squatter ghost type

A custom ghost type whose model is just a person in a bedsheet making ghost noises. It has no
evidence and no paranormal activity of any kind.

Notes: a ghost type with no evidence would be the first of its kind, so the journal's evidence
filtering and the stage 2b content layer's ghost registration (`sdk/api/content.h`, still to be
written) need to allow a type with zero evidence.

## A ghost that reacts to how close the investigators are

A custom ghost that changes behaviour depending on how close the investigators are to determining
its type. This includes circled evidence, media, and the ghost type circled.

Notes: recorded 2026-11-08; how it changes and what counts as "close" are for the user to decide.
The SDK already maps the three signals: the journal's evidence buttons and their sync
(`JournalController.evidenceButtons`, `sdk/map/JournalController.h`), each player's ghost guess and its
sync (the guess entries in the same header), and the journal's media (`Journal_PhotoPage.photos`,
`.sounds` and `.videos`, `sdk/systems/media.md`). Every player's journal is on every client
(`sdk/systems/journal.md`), so the ghost could read all players' choices, not only the host's.
