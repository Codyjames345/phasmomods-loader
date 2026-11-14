# Synced emotes

Design for the Emotes mod's synced emotes: two players dancing one emote together, the second joining the first. It
builds on `2026-11-10-emotes-design.md` and its later decisions, and on the second round's plan
(`docs/superpowers/plans/2026-11-12-emotes-pages-packs-walking.md`). Agreed with the user on 2026-11-13.

## Why

Friends in a lobby want to dance together, not side by side out of step. Some emotes are made for two: each dancer has
a part, and the parts only make sense in step. The user picked Sway as the first, replacing Double Up, whose package had
no partner animation.

## Goals

- **A synced emote** has a part for each of two dancers. The first dancer starts it from the wheel like any emote and
  plays the first part. Sway is the first synced emote: its first part is `Sway (Player 1)`, its second
  `Sway (Player 2)`, both 15.9 s loops, with one song (63.6 s, exactly four loops of the dance).
- **Joining.** Another player near the dancer, looking at them, sees a prompt above the dancer's head: the button's
  glyph with "Join" beside it (the user's words). Pressing it moves the joiner smoothly to the partner's spot and starts
  the second part at the point the first dancer has reached, so the two move in step from then on. The first dancer
  carries on without a jump.
- **The button is the game's own Interact action**, on every device: E on keyboard by default, and whatever the
  player's gamepad and VR controller have Interact bound to, rebinding included (the user, 2026-11-13: "default to the
  interact button"). Nothing new goes in the keybinds menu.
- **The partner's spot** is where the emote expects them: for Sway, side by side with the first dancer, facing the same
  way (the user), about 1.3 m apart centre to centre, as in the user's reference picture
  (kept in the private assets repository, `local/reference/sway.png`) measured against the dancers' height. The animation turns
  them toward each other; at the moment pictured, one dancer has turned to the other and reaches toward them. The
  partner stands on the side that reach goes to.
- **The music.** A synced emote may name a synced song. On joining, the pair's song switches to it at the same point.
  Sway has one song, which plays on unchanged.
- **Ending.** When either dancer stops the emote, for any reason the mod already ends an emote for (moving away, an
  action, the journal or pause screen, a death, leaving), both stop at once (the user).
- **Everyone sees it in step.** Every client times both parts from the first dancer's clock, as each client already
  times a dancer's intro and song.
- **Packs can make synced emotes too.** A catalogue entry declares its second part, its partner spot and any synced
  song, so an emote pack's synced emote works as the mod's own does. The author's guide documents it.

## Non-goals

- More than two dancers in one synced emote. Once someone has joined, nobody else is offered the prompt.
- Swapping parts, or choosing which part to take.
- Props.
- Joining a dancer who is walking (a synced emote is never a walking emote).

## Anti-goals

- Putting a joiner inside a wall, a prop or another player.
- Clients seeing the pair out of step, or one dancer left dancing alone after the other stopped.
- A prompt that offers a join that cannot happen: no free part, a blocked spot, or a player who cannot dance now.
- A new screen or overlay. The prompt is the user's own design: a world-space label over the dancer, not a window.
- Any behaviour of the game invented rather than read from it (see Constraints).

## How it looks and behaves (decided with the user)

**The prompt** floats above the dancer's head, faces the viewer's camera and is seen in perspective. It shows the
Interact button's glyph and "Join" beside it, white like the wheel's text.

- On a gamepad, the glyph is the game's own sprite for the button Interact is bound to: Xbox or PS5, as the game picks
  for the pad (`sdk/systems/menus.md`, "Input glyphs").
- On keyboard, the game has no key glyphs, so the key's name (E) is drawn in a white outlined square, styled like the
  wheel's scroll glyphs (a vector drawing in code, as `mods/emotes/scroll_glyph.h` is). This was Claude's proposal,
  accepted by the user.
- In VR, the prompt is the same, over the dancer, with the VR controller's Interact button.

**When it shows:** the local player is close to someone dancing a synced emote, is looking at them, the second part is
free, the partner spot is clear, and the local player could start an emote now (the mod's existing rules: alive or dead
as the dancer is, not already dancing, wheel shut, no menu or journal open).

**Joining:** the joiner glides into the partner spot over a short time (under a second, so it does not unsettle a VR
player), turned to face the way the first dancer's spot faces. Their camera eases out behind them, as when any emote
starts. Nothing else of theirs can start during the glide, and if the first dancer stops during it, the joiner stops
too.

**A blocked spot** shows no prompt.

## Constraints

Every rule in `CLAUDE.md` holds. The ones this design leans on:

- **Never invent game behaviour.** Before it is built, these are reverse engineered and cited by map key:
  - how the local player's rig can be moved to a spot and turned, the game's own way;
  - what the game itself does when Interact is pressed while looking at another player, so joining never fights it (if
    the game does act on it, the builder stops and asks the user);
  - where the game's gamepad sprite for an arbitrary action's button comes from (the shoulder glyphs come from live
    `GamepadButton` widgets; Interact may need another source);
  - how to test that a spot is clear (the game's own physics queries and layers).
- **The loader owns logistics.** Joining is a message on the mod's one SDK channel; no RPC of the game's is sent, and
  the game's checks on who sent an RPC are untouched.
- **Player-facing text in the string table**: "Join" is the user's word (`emotes.join`, or the builder's key name).
- **Tests first** for everything that can be tested offline: the join rule, the timing, the message, the spot.
- **Dynamic over hard-coded**: the Interact action is found by name in the game's input asset, as the mod already
  finds its ending actions; its bindings are read, never assumed.

## Left to the builder

- The catalogue's field names and shape for a second part, a partner spot (as an offset in the first dancer's frame,
  so a pack can express any placement) and a synced song, and their rules in `mods/emotes/catalogue.h`.
- Which side Sway's partner stands on, found by posing both clips on a humanoid in the private Unity project at the
  pose in the user's picture. The spacing (about 1.3 m) and the glide time are starting values, tuned in game.
- The prompt's range and the "looking at" test, the prompt's size and height above the head: starting values.
- The message format: a new message kind after the existing four, sent on the mod's channel.
- Whether the joiner's client adds the song's loudness for the ghost: one song plays for the pair, from the first
  dancer, so Claude's call is that only the first dancer's song counts.

## Claude's calls (from the builder check, 2026-11-13)

A read of this design as its builder found these gaps a wrong guess would matter for. Each is settled here as Claude's
call, for the user to check:

1. **The first dancer's client decides who joins.** The joiner asks on the mod's channel; the first dancer's client
   accepts the first request while the second part is free and tells everyone, and the joiner's glide starts on that
   answer. Two players pressing at once cannot both join.
2. **While paired, neither body turns with its camera.** Alone, a dancer's body turns to keep the camera within 90
   degrees; paired, both bodies keep the facing they joined with, so the partner spot never moves. Cameras still orbit.
3. **No joining during an intro.** A synced emote whose parts start with intro clips offers the prompt only once the
   first dancer's intro has ended. Sway has none.
4. **The joiner's half-metre rule starts at the spot**, once the glide has put them there, so the glide itself never
   ends the emote.
5. **The prompt is the local player's own.** Each client draws it only for itself, over the dancer it could join.

## Changes after the first live test (user, 2026-11-14)

These replace what the sections above say where they differ.

1. **The button is the game's Grab action, not Interact.** The game names it `Pickup`: E on keyboard, the west button
   on a gamepad (`sdk/systems/players.md`, "What the player does, by input action"). The user had asked for Interact
   thinking it was E, but Interact is the left mouse button and the right trigger. Its keyboard binding is therefore no
   key, so the keycap showed empty. Everything said above about Interact (the glyph, the bindings read with rebinding,
   no prompt while the game's own action would act on something) now applies to Pickup. VR keeps the right trigger.
   What Pickup does when pressed while looking at another player, and what its own ray reaches, is reverse engineered
   before it is relied on. If the game acts on a player, the builder stops and asks.
2. **The two dancers face each other.** The joiner's spot stays where the live test put it, about 1.3 m to the first
   dancer's left, as the user's screenshot shows. On joining, both bodies turn to face each other: the joiner glides in
   already facing the first dancer, and the first dancer turns toward the joiner over the same glide, never in a jump.
   The spot is fixed at the moment of the join, so the first dancer's turn never moves it. This replaces "side by side,
   facing the same way" above, and Claude's call 2 now keeps that facing each other for the rest of the dance. A pack
   says this per synced emote in its catalogue, so other synced emotes may still face the same way.
3. **Each dancer hears the song as their own.** A dancer's own emote song plays to them from their own body, as for
   any emote: not positioned, in both ears. On the joiner's client, the joiner's body plays the pair's song as its own,
   in step with the first dancer's, and the first dancer's copy is silent there, so the joiner no longer hears it from
   one side. The first dancer's client plays it the same way, from their own body. Every other client hears one copy,
   from the midpoint between the two dancers, moving as they move (the user, 2026-11-14), instead of from the first
   dancer. Only one copy counts for the ghost, as before (Left to the builder, last item).
4. **Head gear follows the head during an emote** (the user, 2026-11-14), for every emote, not only synced ones.
   Whatever the player wears on their head stays on the dancing head on every client, then goes back to the game's own
   placement when the emote ends. That covers cosmetic hats such as the antlers, and head equipment such as the head
   torch or head camera. In the live test it stayed where the head would be without the dance. How the game attaches and
   moves each kind is reverse engineered first, and the fix uses the game's own attachment where it can.

