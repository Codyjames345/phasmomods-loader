# Emotes

A wheel of eight dances. Hold the wheel's key, pick a dance and let go: your investigator dances it with its song, every
other player sees and hears it, and the ghost hears the song as you talking. The design is
`docs/superpowers/specs/2026-11-10-emotes-design.md`.

## The dances

Clockwise from the top of the wheel: Disco Fever, Electro Swing, Floss, Groove Jam, Orange Justice, Scenario, Smooth
Moves and Take The L. Each repeats until something ends it. A song repeats with its dance, except Floss's, which repeats
every 16.692 seconds, and Take The L's, every 12.631 seconds (measured from the tracks).

## Controls

| | Open the wheel | Pick | Play |
| --- | --- | --- | --- |
| Keyboard and mouse | hold X | move the mouse toward a dance | let go of X |
| Gamepad | hold D-pad up | push the right stick toward a dance | let go of D-pad up |
| VR | press X on the left controller | point the right controller at a dance | pull the right trigger |

The key's default is B on a keyboard and D-pad down on a gamepad, but the game already uses both (B is the walkie-talkie,
D-pad down the torch), so the defaults fall back to X and D-pad up. Rebind it in the game's own keybinds menu, where it
has a row of its own; the mod remembers the rebinding. Letting go with the mouse or stick at rest plays nothing. Pressing
the VR button again closes the wheel. The wheel stays shut while a menu or the journal is open.

## While you dance

- The camera eases out behind you so you can watch, and back in when you stop. The mouse still turns it: within 90
  degrees either side your body keeps facing the way it started, and past that it turns to keep the camera 90 degrees
  behind. In VR the view never moves; your own body shows instead, so look down to watch.
- What you hold stays in your hand.
- Moving more than half a metre, using, placing, picking up, dropping or throwing an item, opening a door, flipping a
  light switch, crouching, or dying ends the dance. Switching items does not.
- The song plays from you and falls off with distance as your voice does. The ghost hears it through the same check it
  uses for voices, so dancing near it is as risky as shouting.
- The dead may dance too: other dead players see it, and the living do not.

## Files

The mod is `mods/emotes`. Its dances, songs and icons are in `emotes.bundle`, built in the private assets repository
(`unity/emotes-bundle`) and pinned in `tools/payload.psd1`. Each song's measured loudness is
`assets/loudness/<dance>.txt`, which the emote pack tool (`tools/emote_pack.ps1`, `sdk/unity/com.phasmomods.emote-tools`) writes with the bundle, and the dances are listed in `assets/emotes.json`. The wheel's colours and sizes are placeholders in
`mods/emotes/wheel_style.h`, and the dances' names are in its string table.
