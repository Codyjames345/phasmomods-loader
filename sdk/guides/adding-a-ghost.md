# Adding a ghost

This guide walks through `sdk/examples/example_ghost/`, a complete mod that adds one ghost type. The ghost is the
Knell from Custom Ghosts, built as a mod of its own so every part fits on a page: its journal page, its evidence and
one tell. CMake builds it as `ExampleGhost.dll`, and `tests/content_test.cpp`
(`the_example_ghost_registers_and_keeps_its_beat`) loads it over the fake host, so it stays correct as the SDK changes.

Read "Making a mod" in the [SDK's README](../README.md#making-a-mod) first. This guide shows those rules in use, and
links back to them rather than repeating them.

## What the loader does for you

A ghost mod registers its ghost and its rules once, from `pmmod_start`, and nothing else. The loader does the rest
([README, "What the loader does for you"](../README.md#what-the-loader-does-for-you)):

- It numbers the ghost after the game's own types, read from the game's `GhostType` enum, so you never pick a number.
- It puts the ghost into the game as a ghost type (a `GhostData` in the contract's `GhostCollection`), and into the
  Mimic's copy list ([ghost types](../systems/ghost-types.md)).
- It gives the ghost a journal page, a Ghosts tab entry and a guess button on every client's journal, in the lobby too
  ([journal](../systems/journal.md)).
- It offers the ghost in the Ghost Type option of the custom difficulties ([difficulty](../systems/difficulty.md)).
- It picks the ghost's evidence through the game's own evidence pick, so a difficulty that gives fewer evidence
  withholds one of it as the game does ([evidence](../systems/evidence.md)).
- It runs the ghost's rules only in a private room where every player runs the host's list of mods, and only once
  every player's mods are confirmed ([README, "The lobby's rules"](../README.md#the-lobbys-rules)). Your mod checks
  none of this.
- It keeps a custom ghost type out of every save and away from every player without the content.

## The files

```
sdk/examples/example_ghost/
  manifest.json        who the mod is, and the mod it needs
  i18n/default.json    every text a player reads
  ghost.h              the ghost: its definition and its rules
  mod.cpp              the four exports
```

### manifest.json

```json
{
  "id": "example.ghost",
  "name": "Example Ghost",
  "author": "Example",
  "version": "1.0.0",
  "description": "...",
  "dll": "ExampleGhost.dll",
  "recordsVoice": false,
  "dependencies": [{ "id": "phasmomods.spectral-flame", "minimumVersion": "1.0.0", "required": true }],
  "incompatibilities": []
}
```

The id is `<author>.<mod>`. Pick your own author: `phasmomods` belongs to the bundled mods
([README, "The shape of a mod"](../README.md#the-shape-of-a-mod)). The Knell gives Spectral Flame's evidence, so the
manifest requires that mod. The loader starts Spectral Flame first, and leaves the example off, with the reason in the
Mods panel, when Spectral Flame is missing ([README, "Dependencies"](../README.md#dependencies)).

### i18n/default.json

```json
{
  "ghost.knell.name": "Knell",
  "ghost.knell.description": "...",
  "ghost.knell.strength": "...",
  "ghost.knell.weakness": "..."
}
```

A player reads the ghost's name and its journal page, so they live in the mod's string table, never in the code
([README, "Player-facing text"](../README.md#player-facing-text)). `pm::mod::bind` loads the table, and
`pm::i18n::text("ghost.knell.name")` reads an entry. `tests/text_table_test.cpp` fails when the code reads a key the
table lacks, or the table keeps a key nothing reads.

### ghost.h: the definition

```cpp
inline constexpr const char* kGhostId = "example.ghost.knell";

inline pm::api::content::GhostDef definition() {
  pm::api::content::GhostDef def;
  def.id = kGhostId;
  def.name = {"en", pm::i18n::text("ghost.knell.name")};
  def.description = {"en", pm::i18n::text("ghost.knell.description")};
  def.strength = {"en", pm::i18n::text("ghost.knell.strength")};
  def.weakness = {"en", pm::i18n::text("ghost.knell.weakness")};
  def.evidence = {"spirit_box", "orbs", "phasmomods.spectral-flame.flame"};
  return def;
}
```

- `id` is `"<mod id>.<name>"`, with no dot in the name. Never change it once released: the custom difficulty keeps a
  player's pick by this id.
- Each text needs English (`"en"`). The journal shows the player's language when you give it, else English.
- `evidence` holds three ids: the game's own (`"emf"`, `"spirit_box"`, `"ultraviolet"`, `"orbs"`, `"writing"`,
  `"freezing"`, `"dots"`) or a custom evidence any started mod registers. `""` leaves a slot empty, and no evidence may
  appear twice.

`GhostDef` has more members, each optional and each documented in `sdk/api/content.h`: tells (lines on its journal
page), `flags` (`PM_GHOST_KEEPS_LAST_EVIDENCE`, `PM_GHOST_BLIND_IN_HUNTS`, and `PM_GHOST_NO_MIMIC_COPY` for a ghost a
Mimic never copies), `rollable` to keep it out of the type roll while it cannot run, a single `target` for its hunts, the
evidence's look (`evidenceLook`), and the members a player who controls the ghost uses (an ability, a HUD line, a limit
on steering).

### ghost.h: the rules

A ghost's behaviour comes from rules attached to its id. The host runs a type's rules while the contract's ghost acts as
that type, on the main thread ([README, "Which rules run"](../README.md#the-host-contract-version-23)):

| Rule | When the host calls it |
| --- | --- |
| `start` | once, when the ghost begins acting as the type |
| `tick` | every frame while it acts as the type, on every client |
| `stop` | once, when it stops (a Mimic's switch, or no ghost any more) |
| `huntSpeed` | on the host, every frame of a hunt, just before the game's own speed update |
| `heldBack` | when a reader asks whether the type's own rules forbid an action now |

The Knell's tell is its pace in a hunt: a surge for one second, then a crawl for one second, over and over. Its rule
writes the hunt's base speed, never the speed itself:

```cpp
inline bool hunt_speed(float* base, float* losScale, void* user) {
  if (!base || !losScale || !user) return false;
  *base = static_cast<Beat*>(user)->speed(*base, seconds_now());
  *losScale = 1.0f;
  return true;
}
```

`*base` comes in as the game's own base speed for this hunt. The rule scales it and sets the line-of-sight speed-up to 1
(none). The game then applies the multipliers every type shares, so its footsteps and animations follow the real speed.
Each frame the rule gets the game's base again, so a scaled base never compounds
([ghost AI, "Hunt speed"](../systems/ghost-ai.md#hunt-speed)). Returning false leaves both inputs as the game set them.

The beat needs to know when a hunt starts. The example reads it from the rules alone, with no game call: `huntSpeed`
runs every frame of a hunt and `tick` every frame, so a frame with no `huntSpeed` call means no hunt.

```cpp
struct Beat {
  double start = -1;   // when this hunt's beat began; -1 between hunts
  bool asked = false;  // whether the host asked the hunt speed since the last tick

  float speed(float base, double now) {
    if (start < 0) start = now;
    asked = true;
    return base * (std::fmod(now - start, kCycle) < kCycle / 2 ? kFast : kSlow);
  }

  void frame() {  // from tick
    if (!asked) start = -1;
    asked = false;
  }
};
```

`stop` clears the beat, so a Mimic that copies the Knell later starts on a fresh one. `register_knell` registers the
ghost and attaches the three rules:

```cpp
inline bool register_knell(Beat& beat) {
  if (pm::api::content::register_ghost(definition()) <= 0) return false;
  pm::api::content::GhostRules rules;
  rules.user = &beat;
  rules.tick = &tick;
  rules.stop = &stop;
  rules.huntSpeed = &hunt_speed;
  return pm::api::content::attach_rules(kGhostId, rules) > 0;
}
```

Both calls work only from `pmmod_start`. Each answers 0 and logs why when the host refuses (a repeated or malformed id,
text without English, an evidence that is neither the game's nor a content id).

Rules attach to a vanilla type too (`"vanilla.mare"`), and several mods may attach to one type. Ghost Tells does this
for six of the game's ghosts (`mods/ghost_tells/`).

### mod.cpp: the exports

```cpp
constexpr PmModInfo kInfo{PMMOD_API_VERSION, "example.ghost", "Example Ghost", 220, 0};
bool g_running = false;
example::Beat g_beat;

bool start(const PmHostApi* api) {
  if (!pm::mod::bind(api, &kInfo)) return false;
  return example::register_knell(g_beat);
}

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &kInfo; }
extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("ExampleGhost", g_running, &start, api);
}
extern "C" __declspec(dllexport) void pmmod_tick() {}
extern "C" __declspec(dllexport) void pmmod_stop() { pm::mod::guarded_stop(g_running, [] { g_beat = {}; }); }
```

`PmModInfo`'s id must equal the manifest's. Its channel (220 here) is the mod's own network channel: other authors'
mods take one from 64 to 255, and the loader refuses a second mod on a channel already taken
([README, "The shape of a mod"](../README.md#the-shape-of-a-mod)). The example sends nothing, so the number matters only
for that check. The guarded calls keep anything thrown inside the mod from reaching the game or another mod.

## Building it

The example is a CMake target of this repository:

```cmake
add_library(ExampleGhost SHARED sdk/examples/example_ghost/mod.cpp)
target_include_directories(ExampleGhost PRIVATE sdk/examples/example_ghost)
target_link_libraries(ExampleGhost PRIVATE pmmod)
```

`pmmod` is the static library every mod links: the SDK's glue (`core/mod_glue.cpp`), the string table and the game
API. Build with MSVC for x64, as the bundled mods are. To try a build, put `ExampleGhost.dll`, `manifest.json` and the
`i18n` folder together in a folder of their own under the `mods` folder beside PhasmoMods Loader, then choose the mod
in the Mods panel outside a lobby. That is the only mods folder today, and a loader update may replace it; a folder
for other authors' mods in `%LOCALAPPDATA%\PhasmoMods\mods` is planned (README, "What exists today, and what does
not"). Never put a mod or any loader file in the game's folder.

## Testing it without the game

`tests/content_test.cpp` runs the example over the fake host (`sdk/fake/fake_game.h`), where the host's own content
registry is live:

```cpp
use_text({"sdk/examples/example_ghost/i18n/default.json"});  // the shipped table
start();
CHECK(content::register_evidence(evidence("phasmomods.spectral-flame.flame")) > 0);
example::Beat beat;
CHECK(example::register_knell(beat));
fake::commit_content();  // settle the content, as the host does once every mod has started
const registry::Ghost* g = fake::content_registry().registered(example::kGhostId);
```

The test then checks the registered name, evidence and rules, and walks the beat frame by frame on a clock of its own.
The README's "Testing without the game" lists everything else the fake can do.

## An ability

The Knell has no ability. A ghost a player can control may have one: give `GhostDef` an `abilityName` and an
`abilityDescription` (both with English, from your table) and a `useAbility` callback, which the host calls on the host
when the player uses it, and which returns false when it could not. Custom Ghosts' Echo is the worked case: its ability
"Speak" (`mods/custom_ghosts/i18n/default.json`), dispatched by `use_ability` in `mods/custom_ghosts/mod.cpp` to
`echo::speak` in `mods/custom_ghosts/echo.cpp`, with its state on the player's HUD through `hudState` and `hudLine`.
Playable Ghost reaches the ability through the content layer by the type the ghost acts as, so it names no ghost of
yours.

A ghost's player-controlled and AI versions follow one ruleset: give the AI the same ability on the same terms, so a
player and the AI are told apart by nothing but their choices
([ghost AI, "A player and its AI"](../systems/ghost-ai.md#a-player-and-its-ai-the-rules-and-the-exceptions)).

## Making a ghost of your own

- Every claim your ghost makes about the game comes from the game's code. Read `sdk/systems/` for what is mapped, and
  cite the map key in your notes.
- Give it a tell that identifies it on zero evidence, as every bundled custom ghost has
  (`docs/custom-ghosts.md` lists theirs).
- Change the game's own systems through its own inputs, as the Knell does through the hunt's base speed, so the rest of
  the game (footsteps, animations, other mods' rules) follows.
