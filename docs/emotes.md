# Emotes

A wheel of dances, eight to a page. Hold the wheel's key, pick a dance and let go: your investigator dances it with
its song, every other player sees and hears it, and the ghost hears the song as you talking. The mod's own dances sit
on the wheel first; other mods' emote packs add theirs after, each pack in its file order and the packs in id order.
The design is `docs/superpowers/specs/2026-11-10-emotes-design.md`.

## The dances

Clockwise from the top of the wheel, the first page: Disco Fever, Electro Swing, Floss, Groove Jam, Orange Justice,
Scenario, Smooth Moves and Take The L. The second page: Build Up, Get Griddy, Lavish, Never Gonna, Running Man, Savor
The W, Sprinkler and Stride. Each repeats until something ends it, and each song loops on its own, cut to the loop the
emote pack tool measured, whatever its dance's length. Build Up and Never Gonna start with an intro (Build Up's song has
one too), and Get Griddy, Running Man and Stride walk while they
dance.

## Controls

| | Open the wheel | Pick | Play | Turn pages |
| --- | --- | --- | --- | --- |
| Keyboard and mouse | hold X | move the mouse toward a dance | let go of X | scroll the wheel |
| Gamepad | hold D-pad up | push the right stick toward a dance | let go of D-pad up | left and right bumpers |
| VR | press X on the left controller | point the right controller at a dance | pull the right trigger | point beside the page number and pull the right trigger |

The key's default is B on a keyboard and D-pad down on a gamepad, but the game already uses both (B is the walkie-talkie,
D-pad down the torch), so the defaults fall back to X and D-pad up. Rebind it in the game's own keybinds menu, where it
has a row of its own; the mod remembers the rebinding. Letting go with the mouse or stick at rest plays nothing. Pressing
the VR button again closes the wheel. The wheel stays shut while the journal or the pause screen is open.

With more than eight dances the wheel turns pages. Scrolling down and the right bumper go to the next page, scrolling
up and the left bumper to the previous, and the pages wrap: one turn past the last shows the first. In VR, point the
right controller at the game's bumper glyph on one side of the page number and pull the right trigger. A row over the
wheel shows the page number ("2 / 3") between a glyph for each page control: a mouse scrolling up on the left and
one scrolling down on the right on keyboard and mouse, the game's own bumper glyphs on a gamepad. It hides while the
wheel has a single page. The wheel reopens on the page you last viewed while the mod has been running. While the wheel
is open, the game actions bound to those page controls (the item swap on the scroll wheel, push-to-talk on the bumpers) are paused and
come back when the wheel closes for any reason. A paused action is paused on every key it is bound to, so push-to-talk's
keyboard key does not talk while the wheel is open either.

## While you dance

- The camera eases out behind you so you can watch, and back in when you stop. The mouse still turns it: within 90
  degrees either side your body keeps facing the way it started, and past that it turns to keep the camera 90 degrees
  behind. In VR the view never moves; your own body shows instead, so look down to watch.
- What you hold stays in your hand.
- Moving more than half a metre, using, placing, picking up, dropping or throwing an item, opening a door, flipping a
  light switch, crouching, or dying ends the dance. Switching items does not.
- Some dances walk while they dance. Hold forward and the dancer takes its walking clip, and the half-metre rule moves
  along with you: walking forward never ends the dance. Let go of forward for 0.2 seconds and the standing clip comes
  back, a sprint ends the dance, and a sideways or backward step is measured from where the walk stopped. In VR the
  mod cannot read the game's move input, so a walking dance stands.
- A dance with an intro plays its intro clip once before its loop, and a dance with an intro song plays that song once
  before its looping song starts. Every player times the intro's end on their own clock, and walking forward during
  a walking dance's intro skips the rest of it.
- The song plays from you and falls off with distance as your voice does. The ghost hears it through the same check it
  uses for voices, so dancing near it is as risky as shouting.
- The dead may dance too: other dead players see it, and the living do not.

## Files

The mod is `mods/emotes`. Its dances, songs and icons are in `emotes.bundle`, built in the private assets repository
(`unity/emotes-bundle`) and pinned in `tools/payload.psd1`. Each song's measured loudness is
`assets/loudness/<dance>.txt`, which the emote pack tool (`tools/emote_pack.ps1`, `sdk/unity/com.phasmomods.emote-tools`) writes with the bundle, and the dances are listed in `assets/emotes.json`. The wheel's colours and sizes are placeholders in
`mods/emotes/wheel_style.h`, and the dances' names are in its string table. Other mods' emote packs, data-only mods
this mod reads, are documented in `sdk/guides/emotes.md` ("An emote pack"), with a worked example in
`sdk/examples/example_emote_pack/`.
