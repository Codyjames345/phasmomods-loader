# Emotes and keys of your own

What the SDK gives a mod that makes players dance, or that needs a key players rebind: the Emotes mod (`mods/emotes`) is
built from these and nothing else. No reverse engineering is needed. The game's side of each is in the systems pages
linked below.

## A key in the game's keybinds menu

`sdk/api/keybinds.h`. Declare a key with `begin_keys(modId, pm::mod::shown_name(), file)` and `add_key(ModKey)` in `pmmod_start`, call
`tick_keys()` every frame, and read it with `key_state(id)`.

- A `ModKey` has a default path per device (keyboard, gamepad, VR, in Input System syntax: `"<Keyboard>/x"`,
  `"<Gamepad>/dpad/up"`, `"<XRController>{LeftHand}/{PrimaryButton}"`) and fallbacks for each. A default the game
  already binds falls back to the first free one; the game's menu-navigation actions (named `UI...`) do not count as
  taken. VR defaults are not checked against the game.
- The keyboard and gamepad bindings each get a row in the game's keybinds menu, labelled with your text, and the game's
  own rebinding runs on it. Your rows sit at the end of each list, under a heading with your mod's name (the manifest's,
  which `shown_name` reads from the host), with the list's own dividers between them; the list's scroll grows to reach
  them. Rows are copied only from the local player's own keybinds menu, never from a map's training whiteboards. While
  a row is on screen the key's actions are off, so the game's rebinding can run on them; `key_state` reads nothing then. A rebinding is kept as the game keeps its own:
  the page's Apply saves it to the file you name, Reset takes it off, and leaving the page without Apply drops it.
  `begin_keys` hooks the page for this, so enable your hooks after it.
- The key is ready (`keys_ready`) once the game's bindings can be read: the keybinds menu or a local player is loaded.

How the game's actions and the menu work: [menus](../systems/menus.md#key-bindings).

## A setting on the audio page

`sdk/api/settings.h`. Declare a slider with `begin_settings(modId, pm::mod::shown_name(), file)` and `add_slider(Slider)`
in `pmmod_start`, call `tick_settings()` every frame, and read it with `value(id)`. Its row is a copy of the master
volume's, under your mod's heading on the journal's audio page. Its value counts on the page's Apply and is saved to your
file; leaving the page without Apply puts the slider back. The whole page reads with one line between every pair of rows
the player can see, yours and the game's alike: the SDK puts the page's own dividers away and copies one line in
between every pair of rows, the line taken from the keybinds page when one is loaded
([menus](../systems/menus.md#the-audio-page)). The Emotes mod's volume is one.

## A clip on a player's own body

`sdk/api/player_anim.h`. `play_on_player(player, clip, blendIn)` plays an `AnimationClip` (from your bundle,
[custom animation](custom-animation.md)) over the animation the game gives any player's body, on your client only;
`stop_on_player(player, blendOut)` gives the body back exactly as it was. A second clip blends in over the first.
`clip_time(player)` is the clip's own time, which a song can follow. Call `tick_player_anims(dt)` every frame. Humanoid
clips play on the investigator whatever skeleton they were made for. Each client plays what you tell it through your
channel. A kill takes the body: hook `Player.NotifyOfDeathStart` and call `stop_on_player(player, 0.0f)` there, or your
clip plays over the game's death animation and hides it
([players](../systems/players.md#the-mark)). How the body is animated: [players](../systems/players.md#the-body).

## A third-person view, and the player's own body shown

`sdk/api/third_person.h`. `begin_third_person()` eases a camera of your own out behind the local player's head, looking
where they look, and `end_third_person()` eases it back; the mouse still turns it. `show_own_body(true)` shows the local
player their own body on their own camera instead, for VR. Call `tick_third_person(dt)` every frame. How it renders as
the game does: [media](../systems/media.md#a-camera-of-a-mods-own-and-screen-effects).

## A sound the ghost hears as talking

`sdk/api/talk_loudness.h`. The ghost hears players through their talking loudness. Measure your sound in the game's
scale (`loudness_of` from samples, or `envelope_of_rms` from RMS values measured beforehand) and raise it on the master
with `add_talk_loudness(player, loudness)` once a second (`Pace`), as the game refreshes a voice. Every rule of the
ghost's voice checks then applies to it. How the checks work: [voice](../systems/voice.md#how-loud-a-player-talks).

## A blur and a vignette

`sdk/api/screen_effects.h`. `set_screen_effects(blur, vignette)` puts a blur and a vignette over the game, under
overlay canvases; `set_screen_effects(0, 0)` removes them. A canvas of your own (`core/ui.h`) is a new window, which
the project's vanilla-UI rule leaves to the author's design; ask before drawing one.
