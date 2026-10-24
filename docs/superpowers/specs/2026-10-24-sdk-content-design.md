# Game SDK: content functions and full coverage of the current mods

Status: design, 2026-10-24. It extends `2026-10-23-game-sdk-design.md` (the game SDK design, called
"the SDK design" below) and changes what its stage 2 plans build. Where the two disagree, this
document wins.

## Intent

The user, after stage 2a:

> "As part of the SDK, you should make sure things like adding custom ghosts have a documented path
> and properly adds them to the journal evidence page, ghosts page (with the list of all ghosts and
> their page number to read a description), custom difficulty page, and have a
> description/evidence/tells etc. At least all the features used in the current mods should have
> documented functions, but make the set of SDK tools comprehensive."

The picture the user gave: someone outside the project reads the SDK documentation and, using its
functions, creates a custom ghost and defines its behaviour, evidence and description. They install
the mod, it shows up in the in-game Mods menu, and the ghost is usable. It is consistent with the
vanilla ghosts: for example, the Mimic can mimic it.

The user's main goal: **no mod uses a memory address or an offset.** Behaviour may differ slightly
after the move. That is acceptable.

## What exists today (why this is needed)

- **Custom Ghosts** (`mods/custom_ghosts`, about 7,200 lines) does the ghost plumbing by hand. It adds
  15 ghost types to the game:
  - It numbers them 31 to 45, hard-coded.
  - It adds their `GhostData` to the contract's `GhostCollection`, which the type roll, the evidence
    a ghost gives and the journal lists all read.
  - It answers their names through the `Journal_<type>Title` localisation keys.
  - It clones a guess button onto the evidence spread's ghost page, and shortens the grid to fit.
  - It adds an entry to the Ghosts tab list, with a page of its own.
  - It offers them in the custom difficulty's Ghost Type option, keeping a custom pick out of the
    save file and out of the lobby sync while anyone lacks the mod.
  - `docs/custom-ghosts.md` describes all of it.
- **Spectral Flame** adds an eighth evidence (value 8), with a journal button.
- **Ghost Tells** adds behaviour and journal lines to six vanilla ghosts. It keys its rules on the AI's
  behaviour type, so a Mimic copying a Mare shows Mare tells.
- **Playable Ghost** lets a player be the ghost, including custom ghosts and their abilities.
- **Raw values in mod code:** about 660 hex literals across the four mods, from code addresses,
  field offsets and object layouts.
- **The Mimic gap:** a custom ghost's AI runs with behaviour type `GhostType.None`, and Custom
  Ghosts applies its rules by the ghost's *identity* type (`GhostInfo +0x28`). The game's Mimic
  copies a type by changing the *behaviour* type (`+0x2C`). So a Mimic copying a custom ghost would
  most likely get none of its rules. This has not been confirmed by reverse engineering; see
  "Left to the builder".

## Goals

1. **Every game feature the four mods use has a documented SDK function or map entry.** After the
   migration, no mod, and no host or `core/` file outside the game API's own runtime, contains:
   - a code address;
   - a field offset;
   - an object layout;
   - an obfuscated name;
   - a direct IL2CPP call.

   The SDK design's source scan enforces this.
2. **Adding a ghost is one documented path.** A mod describes a ghost, and the SDK makes it a real
   ghost type everywhere the game treats ghost types generically. Ghost content covers:
   - name and evidence;
   - journal text: description, strength, weakness, tells;
   - behaviour rules;
   - an optional ability for a player controlling it.
3. **Adding an evidence type is one documented path**, because custom ghosts may use it (Spectral
   Flame today).
4. **Vanilla consistency.** Within the game's mechanics, a custom ghost behaves like a vanilla one:
   - the Mimic can copy it and then follows its rules;
   - the journal pages look like the game's own;
   - guessing it in the journal is judged right or wrong like any ghost.
5. **The bundled mods use only the public path.** Custom Ghosts, Spectral Flame and Ghost Tells
   register their content through the same functions an outside author uses, with nothing private.
6. **The toolset is comprehensive.** Beyond what the current mods need, the main systems the SDK
   design lists are mapped and documented. Custom equipment is documented as a system, with a worked
   example, but gets no registration function yet.

## Non-goals

- **A function to register custom equipment.** It is documented only (see "Custom equipment").
- **Custom maps**, and custom cursed possessions. Both are out of scope.
- **New windows or overlays.** The vanilla-UI rule stands: content appears in the game's own
  screens and widgets.
- **Keeping behaviour exactly as it is today.** The SDK design's Migration section said "Each mod's
  behaviour stays exactly as it is". The user has relaxed this: small differences are acceptable.
  Each known difference is recorded in the plan's ledger and reported. The offline tests keep their
  coverage.

## Anti-goals (failure even if it works)

- **A custom ghost or custom evidence value reaches the save file or a player without the mod.**
  The existing rule stands and now applies to every content mod.
- **Content looks bolted on.** Examples: a journal entry out of alphabetical order, a page laid out
  differently from the game's own, or a guess button that the evidence filter does not cross out.
- **A mod still needs an address, offset or layout** to do something the current mods do.
- **The content functions work for the bundled mods but not for an outside author.** Examples: a
  fixed type number, or a special case keyed on a bundled mod id.

## Decided by the user

- The priority is implementing the functions the current mods use. The main goal is that no mod
  uses an address or an offset. Small behaviour differences are acceptable.
- **Custom ghosts.** An author defines the ghost's behaviour, evidence and description through
  documented SDK functions. The ghost then appears in:
  - the journal's evidence page, as a guess button;
  - the Ghosts tab list of all ghosts, each entry leading to the page number of its description;
  - the custom difficulty page.

  Each ghost carries its description, evidence and tells.
- Custom ghosts are consistent with vanilla ghosts. The Mimic can mimic them.
- The SDK owns the plumbing:
  - type registration and the type roll;
  - the evidence page;
  - the Ghosts list and pages;
  - the difficulty pick;
  - names;
  - sync between players.

  The mod only describes its content and behaviour. Custom Ghosts becomes one ordinary user of this
  public path (proposed in conversation and confirmed by the user).
- Custom evidence is included.
- **Custom equipment** (the user's example: dowsing rods, with a place in the truck, a shop listing
  with a price and description, a use in the investigation, perhaps an evidence type) is
  **documented now and not wired in**.
- **The first public SDK release waits for full mapping** (user, 2026-10-24). The SDK zip for
  outside authors ships only when the rest of the main systems are also mapped and documented:
  - weather;
  - players;
  - the level and rooms;
  - the lobby and menus in full;
  - the equipment classes.

  This changes the SDK design's "When it ships" (after stage 2) to: after stage 2 and the remaining
  mapping (the SDK design's stage 3), when the coverage report for those systems has nothing left.
  The order of work is unchanged: the current mods and the content functions come first.
- The SDK design's earlier decisions stand. Mods are toggled in game like the bundled ones, and
  third-party mods live in `%LOCALAPPDATA%\PhasmoMods\mods`. Every release carries an SDK zip, and
  versions are library-style.

## The design (Claude's calls unless marked)

### A content layer above the game API

The game API maps the game's classes. The content layer is a set of host services, built on the
game API, that own the shared plumbing. Several mods may add ghosts and evidence at once, so one
owner must:
- assign their numbers;
- extend the collections and the journal;
- keep them out of saves.

That owner is the host, not any one mod. Mods reach it through new host contract functions, each
with a typed C++ wrapper in `sdk/api` and an equivalent in the fake backend for offline tests. It
follows the contract rules: plain pointers, integers and C strings only, nothing thrown across, and
growth only at the end.

### Registering a ghost

At start, a mod registers each ghost with a definition:
- **A stable id** made of the mod id and a ghost name, for example `custom_ghosts.brownie`. It never
  changes between versions and is what players' clients agree on. Mods never see or choose a type
  number.
- **The display name.**
- **Three pieces of evidence**, each a vanilla evidence or a custom evidence id. A ghost may need a
  fourth forced evidence, as the Mimic's orbs do, or none; see "Left to the builder".
- **Journal text:** description, strength, weakness, and its tells as separate lines. The SDK lays
  these out on the ghost's page the way the game's own pages are laid out.
- **Behaviour:** the ghost's rules (next section).
- **Optionally, how it plays when a player controls it** (Playable Ghost). Today Custom Ghosts
  passes this to Playable Ghost and Spectral Flame through a private service (`custom-ghosts.v1` in
  `core/services.h`). Goal 5 forbids a private path, so the ghost definition carries all of it
  publicly, and an outside author's ghost gets everything the bundled ones do:
  - **An ability:** its name, its description, and a callback that performs it, such as the Echo's
    "Speak" or the Kikimora's "Tidy away".
  - **A held-back check:** the ghost's own rules forbid acting now, so the player cannot either. An
    example is a dormant Echo.
  - **A state line for the ghost player's HUD.**
  - **A player-only view,** such as the Black Shuck's scent trail.
  - **A "player-controlled?" query,** so a rule can behave differently for a player, as the
    Bogeyman, the Echo and the Black Shuck do.

  Playable Ghost drives these for any registered ghost, including ghosts it has never heard of.
- **Optionally, how it changes the look of an evidence,** such as the Hydra's green Spectral Flame.
  This is a value the evidence's mod reads through the content layer, not through a private service.
- **Text in more than one language** (Claude's call). Each piece of text is a table keyed by the
  game's language. English is required, and any missing language falls back to English. The pages
  follow the game's language switch. The table is built into the ABI from the start, because the
  contract can only grow at the end.

The host then does the following:
- **Type numbers.** It numbers the registered ghosts from 31 upwards, in the order of their stable
  ids, among the ghosts every player in the lobby has. All clients therefore reach the same numbers
  without negotiation.
- **The game's collection.** It adds each `GhostData` to the contract's `GhostCollection` and to the
  difficulty menu's, in alphabetical order.
- **Names.** It answers the name keys.
- **The evidence spread.** It adds a guess button to the evidence spread's ghost page. The button is
  crossed out by the game's evidence filter and synchronised by the journal's own controller. The
  grid is fitted to the total number of ghosts.
- **The Ghosts tab.** It adds a list entry, in its alphabetical place, leading to a page of the
  ghost's own. Page numbers are the same on every client, because the journal syncs by page number.
- **Custom difficulty.** It offers the ghost in the Ghost Type option. A custom pick never reaches a
  save file or a player without the mod, as `docs/custom-ghosts.md` describes today.
- **The contract's ghost.** Its identity becomes the custom type for everything that names or judges
  it: the roll, the evidence given, the journal guess, the results.

### Behaviour rules, and the Mimic

The SDK keeps the behaviour-type lookup itself, so a mod's rules follow the type the AI is acting as.
- **Attaching rules.** A mod attaches rules to a ghost type: its own custom types, or a vanilla type
  such as Ghost Tells' six.
- **Which rules run.** When the ghost is acting as type T, the SDK runs T's rules. For a plain ghost,
  T is its own type. For a Mimic, T is the type it has copied.
- **Custom types in the Mimic's list.** The custom ghosts join the list of types the Mimic may copy.
  A Mimic copying one then runs that ghost's rules. It keeps the Mimic's own traits, such as the
  orbs, as for any copy.
- **What is still unconfirmed.** How the game's Mimic copies a type, and how a custom ghost's AI
  should be set up so the game's generic code still runs for it, are to be confirmed by reverse
  engineering at planning time. The requirement is the user's: the Mimic can mimic a custom ghost.

The rules themselves are the game API's existing tools: hooks by entry, caller context, and the
mapped ghost, hunt and evidence classes. The content layer adds:
- the "acting as" dispatch, so a rule runs only for its type;
- a per-type hunt speed setting, since several custom ghosts set the hunt's base speed. The
  arithmetic in `docs/custom-ghosts.md` (write the inputs, never the result) becomes an SDK function.

### Registering evidence

A mod registers a custom evidence with:
- a stable id;
- a name;
- its journal button text.

The host gives it a number from 8 upwards, by stable id among the evidence every player in the lobby
has, using the same rule as ghost types (Claude's call). Spectral Flame registers through this path
too, so it is 8 only while it is the only custom evidence. Saves never hold the number, and every
client runs the same content, so nothing needs a fixed value. The host then:
- adds the evidence button to the journal's evidence page;
- includes the evidence in the game's evidence filter;
- lets ghosts that list it give it, through the game's own evidence selection.

What reveals the evidence in the world stays the mod's job. For Spectral Flame, that is the candles,
the lantern and the candelabras.

### Journal text for vanilla ghosts

A mod may add lines to a vanilla ghost's journal page, as Ghost Tells does. The SDK adds them in the
game's own layout. Several mods may add lines to one ghost.

### Lobbies and saves

Custom content is active only in a private lobby where every player runs the same content mods. This
generalises today's Custom Ghosts rule from "the mod" to "every mod that registers content". When a
player without one joins:
- that content is withdrawn;
- custom difficulty picks revert to Random;
- the host sends Random.

Saves never hold a custom type or a custom evidence value.

### Coverage of the current mods

The SDK design's stage 2b moves the four mods onto the game API. With this design, 2b also builds
the content layer. Custom Ghosts, Spectral Flame and Ghost Tells move onto it, and so does Playable
Ghost wherever it names ghost types or abilities.

The plan's first step is an **inventory**: every address, offset, layout, obfuscated name, scene
path and raw IL2CPP call in the four mods, the host and `core/`. Each one ends as one of:
- a map entry;
- a content function;
- something removed because the content layer now does it.

The SDK design's source scan then proves that none is left.

### Custom equipment (documented only)

`sdk/systems/equipment.md` describes how the game's equipment works:
- the truck's slots;
- the shop listing, with its price and description;
- loadouts and their save;
- how a held item is picked up, used, dropped and synced;
- how equipment reveals evidence.

The map covers those classes, as the SDK design already plans ("Evidence and equipment"). The
document ends with a worked example: what a dowsing-rods mod would need. It names the map entries it
would use, what is missing for a registration function (like the ghost and evidence functions), and
the save and lobby rules it would have to follow. No equipment registration function is built now.

### Documentation

The SDK design's documentation rules apply to everything here: a generated reference, guides, and a
test that fails on anything undocumented. New guides:
- **"Adding a ghost"**: the full path, with a complete small example ghost that builds from the
  template;
- **"Adding evidence"**;
- **"Adding tells to a vanilla ghost"**;
- **"How the Mimic treats your ghost"**;
- **"Lobbies, saves and players without your mod"**.

Each guide states the rules the bundled mods follow.

## Constraints

- Everything in the SDK design's Constraints and anti-goals holds. Above all:
  - nothing touches or describes the anti-cheat;
  - no loader files go in the game folder;
  - the user starts the game for live tests;
  - the repository holds no machine paths, credentials or derived game data.
- Host contract growth follows the library-style versioning rule.
- The vanilla-UI rule: the game's own screens and widgets only.

## Left to the builder

- The exact ABI and structs of the content functions, and whether a rule is a callback or data.
- **How the Mimic copies, confirmed by reverse engineering before the design is fixed.** Settle:
  - which field the game switches;
  - where it gets its list of types;
  - how the Mimic's own evidence is given;
  - what setup lets a custom ghost run the game's generic AI while the SDK dispatches its rules.

  Record the findings in the map's notes.
- Whether a ghost definition may force a fourth evidence, or have fewer than three. Follow what the
  game supports.
- Where the type and evidence numbers are agreed between clients. The host's existing peer checks,
  or the lobby sync, are candidates.
- The plan structure. Fold this into 2b or split it into 2b and a content plan, as long as the
  inventory comes first and the source scan comes last.
