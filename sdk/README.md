# The game SDK

This directory holds everything a mod needs to reach Phasmophobia without a raw address or field
offset in its code, and everything a person needs to add to that.

Terms used throughout:

- **Game SDK**: the whole kit, this project and the `sdk/` directory. It holds the game API, the
  behaviour notes and overviews, the fake backend for tests, the offline check and the reverse
  engineering (RE) tooling that reads the map.
- **Game API**: what a mod calls. It lives in the namespace `phasmo` (`phasmo::GhostAI::UnAppear`).
- **Game map**: the declaration of every mapped game type, method, field, enum value and inlined code
  place. The headers in `sdk/map` are the map.
- **Host contract**: `core/mod_api.h`, the contract between `ModHost.dll` and each mod DLL.

The audited game is Steam build 25811160. Notes marked as read from decompiled code are from that
build.

## What exists today, and what does not

Exists: the map and its resolver (at startup in the game, and offline), the game API, host contract
2.2 with its runtime services, static fields, scene facts and content registry, hooks by entry, the fake backend,
`tools/gamecheck.ps1` with its scene check `tools/assetcheck.py`, and the mapped types (GhostAI with
17 entries; the ghost-type system: GhostInfo, GhostController, GhostCollection, GhostData, LevelStats,
EvidenceController and the GhostTraits struct with the GhostType and EvidenceType enums; the ghost's
hunt state with its speed inputs; LocalisationSystem's text by key; Unity's Camera
with 8, LevelController with its static instance, Unity's Animator with one icall, and the Unity types
the host's scene facts use: Animator, AnimatorControllerParameter, Transform, LayerMask and
SceneManager), two scene facts (the journal's book and the PlayerView layer),
and two native sites in
UnityPlayer.dll (the crowd steering site and `NavMeshAgent::GetInternalAgent`) behind the host's
`steer_agent`. Ghost Tells reaches the game only through the game API and content rules on six vanilla
ids (players, light switches, doors, incense, flames, the hunt state and Unity's world queries:
`Player.h`, `LightSwitch.h`, `Door.h`, `UnityWorld.h` and additions to the ghost's headers), and
Spectral Flame reaches the game only through the game API and the content functions, over the flames, candles, rooms, sanity, Photon
and Unity rendering entries (`FireSource.h`, `Candle.h`, `Equipment.h`, `LevelRoom.h`,
`PlayerSanity.h`, `Photon.h`, `UnityRendering.h`; [flames](systems/flames.md)). Custom Ghosts registers
its sixteen ghosts and their rules through the content functions alone (`mods/custom_ghosts/registration.h`);
its ghosts' behaviour rules (`apparition`, `asterion`, `behaviour` with `hunt`, `bogeyman`, `brownie`, `echo`,
`gremlin`, `hydra`, `kikimora`, `lethe`, `lithobolia`, `mesmer`, `restraint`, `sound` and `tempest`, with
`sensor_copy` for the Hydra's and the Lithobolia's copies of the ghost's sensors) reach the game only through
the game API and `core/`'s plain helpers (`core/game.h`, `core/ui.h`, `core/players.h`, `core/bindings.h`),
which are themselves written on the game API.

Does not exist yet. These arrive with stage 2, when every bundled mod is on the game API:

- the downloadable SDK zip with a template mod;
- the generated game API reference. Until then this README, the headers and `sdk/systems` are the
  documentation, and each header's `///` notes are the reference. The pages include
  [the vanilla ghosts, type by type](systems/vanilla-ghosts.md) (what each ghost type does differently),
  [the contract](systems/contract.md) (its start, both ends, every way out) and
  [Photon](systems/photon.md) (rooms, views and the cleanup, and the game's RPC checks),
  [the menus](systems/menus.md) (how a menu button is clicked, and how a copy of one keeps working),
  [weather and lightning](systems/weather.md), [doors, exits and keys](systems/doors.md) (finding, opening and
  locking doors, every map's exits, and who is out of the map),
  [navigation](systems/navigation.md) (routes over the level's navigation mesh), [voice chat](systems/voice.md),
  [photos, video and screens](systems/media.md) and
  [Steam achievements](systems/achievements.md) (how the game unlocks one).

Until then, mods are built inside this repository, as the bundled ones are (see `CMakeLists.txt`, the
`GhostTells` target), and the host loads the mods that sit in a `mods` folder beside it. The guides walk through
whole mods: [adding a ghost](guides/adding-a-ghost.md) builds `sdk/examples/example_ghost/`;
[an emote pack](guides/emotes.md#an-emote-pack) builds `sdk/examples/example_emote_pack/`;
[adding evidence](guides/adding-evidence.md) follows Spectral Flame; [tells for vanilla
ghosts](guides/tells-for-vanilla-ghosts.md) follows Ghost Tells; [the Mimic and your
ghost](guides/mimic-and-your-ghost.md) and [lobbies, saves and players](guides/lobbies-saves-and-players.md)
say what the loader guarantees; [a mod's own files](guides/mod-files.md) says where a mod puts data files
(an `assets` folder beside its DLL, copied for each launch and read-only, found with `pm::api::assets_folder()`, and a mod
whose files cannot be copied is off); [custom animation](guides/custom-animation.md) shows a figure of a mod's own
(an AssetBundle of clips, a script-free copy of the investigator's body and Unity Cloth, with the helpers in
`sdk/api/bundles.h` and `sdk/api/models.h`); [emotes](guides/emotes.md) covers a key in the game's keybinds menu, a clip on
a player's own body, a third-person view, a sound the ghost hears as talking, a screen blur and an emote pack of your
own; and [equipment](guides/equipment.md) describes the game's equipment system. Everything below
describes what is in the code now.

## Making a mod

### The rules every mod follows

- **Main thread only.** The host calls `pmmod_start`, `pmmod_tick` and `pmmod_stop` on Unity's main
  thread, and the game API runs there too. Do not call it from another thread. Only the host contract's `log` may be
  called from any thread; everything else runs on the main thread.
- **Never keep a raw managed reference across frames.** The garbage collector moves and frees
  managed objects. Keep one in a `pm::api::Held` (`sdk/api/held.h`), which holds it through the host
  contract's `gc_hold`, as `mods/ghost_tells/shade.cpp` does with its ghost. `tests/held_scan_test.cpp` fails the build on a raw `void*` kept at namespace
  or struct scope. It scans `core/`, `host/` and the four bundled mod folders by name, so a new mod
  folder must be added to the list in that test.
- **Private lobbies only.** Play with mods in private lobbies only. Playing with mods risks a ban on
  the account. Nothing in the SDK changes that, and `send` already refuses outside a private room.
- **Nothing in the SDK touches the anti-cheat.** `tests/map_test.cpp` rejects an entry naming
  `SelfReport` or `Moderation`.
- **The host hooks nothing but the frame before the player agrees.** The frame is what shows the notice. Its messages and
  content hooks go in on "I agree", and "Play without mods" takes the frame hook off too, so a mod author can rely on
  nothing of the host's being in the game before a mod starts.
- **Nothing of yours runs before the player agrees.** Before the consent notice is answered the host reads your
  `manifest.json` and nothing else, and your DLL is not mapped into the game. It loads the first time your mod
  starts, which is never if the player declines or leaves your mod off. So what the notice or the Mods panel needs
  goes in the manifest, and `DllMain` is not a place to do work: start in `pmmod_start`. A DLL that has loaded stays
  mapped until the game exits, even if the host then refuses it, because unloading one is not safe.
- **A refused game RPC is not harmless.** Many of the game's RPCs check their sender and their state. A refusal
  flags the sender, and on the sender's own client it counts against the account. Reach a game RPC only through
  the game's own method, on the client the game sends it from, and never invoke an RPC's body or send its name
  yourself. [photon](systems/photon.md) has the rules.
- **Nothing thrown crosses the host contract.** Only plain pointers, integers and C strings do. A mod
  catches its own exceptions. `pm::mod::guarded_start`, `guarded_tick` and `guarded_stop`
  (`core/mod_glue.h`) do that for the three exports.
- **A struct argument whose size is not 1, 2, 4 or 8 bytes is passed by address**, as the x64 calling
  convention does, and the callee may modify it. In a `phasmo::` method signature write such a
  parameter as a pointer. Pass a copy when the original must stay unchanged.

### Player-facing text

Keep everything a player reads in your mod's string table, `i18n/default.json` beside your `manifest.json`, as SMAPI mods do:
a flat JSON object of key to text, `//` comments allowed, with a `{{token}}` for each value your code fills in.

```json
{
  "hud.charge": "Charge: {{speed}}% speed"
}
```

```cpp
#include "text_table.h"
const std::string line = pm::i18n::text("hud.charge", {{"speed", std::to_string(speed)}});
```

`pm::mod::bind` loads the table (`core/text_table.h`), so read text from `pmmod_start` onwards. The launcher copies the table
beside your DLL's copy as it copies the manifest. A key the table lacks shows as `[key]` and is logged once, so a missing
text is seen, never blank. Where your code keeps a key to read later, name it with `pm::i18n::key("...")`. A test can load
the shipped table with `tests/text_fixture.h`, and `tests/text_table_test.cpp` checks that every key your code reads is in the
table with its tokens, and that no key goes unused. A translation would be another file beside it (`i18n/<language>.json`);
none is read yet.

### What the loader does for you

The loader owns every piece of logistics, so a mod does none of it, and `tests/logistics_scan_test.cpp` fails on a mod that
tries:

- **Loading and starting.** The host reads your manifest, loads your DLL once the player agrees, starts mods in dependency
  order and stops them in reverse. Never load or look up another module (`LoadLibrary`, `GetProcAddress`,
  `GetModuleHandle`), and never read another mod's files.
- **Dependencies.** Declare them in `manifest.json`. The host starts what you require first, keeps you off with the reason
  when it cannot, and answers `mod_started` for an optional one. Never name another mod's id in your code.
- **Matching the lobby.** The host's list is the lobby's list ([The host's list](#the-hosts-list)). The host keeps every
  player to it, and a contract only begins once every player's mods are confirmed. `peer_has_mod` and `room_agrees` stay
  in the contract so older mods load, and are not for checking other players; `service` and `register_service` are not
  for checking other mods.
- **Messages between players.** `send` and `set_receiver` on your own channel, which reach only verified players. Never
  send a mod's data through the game's RPCs or Photon events, and never announce yourself, shake hands or exchange
  versions: the loader announces who runs what. What you put in your own messages is your own.
- **Content, saves and sync.** Register ghosts, evidence and journal lines through `sdk/api/content.h`. The host puts them
  into the game, numbers them, keeps them out of saves and away from players without them, and takes them out again.
- **Lobby rules.** Private lobbies only, a vanilla host, a joiner missing a mod, extras switched off: the host does all of
  it, for every mod the same way.

### The shape of a mod

A mod is a DLL and a `manifest.json` in its own folder, `mods/<folder>/` beside `ModHost.dll` (see
`mods/ghost_tells/`). The DLL links the static library `pmmod` (`core/mod_glue.cpp` and the game API).

`manifest.json` is the one file that says who the mod is. It is standard JSON in UTF-8, at most 64 KiB,
and `sdk/manifest.schema.json` lets an editor check it (point `"$schema"` at it). The loader reads it with
`core/mod_manifest.h`:

```json
{
  "id": "phasmomods.ghost-tells",
  "name": "Ghost Tells",
  "author": "PhasmoMods",
  "version": "1.0.0",
  "description": "New tells for the Spirit, Mare, Shade, Yurei, Goryo and Onryo",
  "dll": "GhostTells.dll",
  "website": "",
  "dependencies": [{ "id": "phasmomods.spectral-flame", "minimumVersion": "1.0.0", "required": true }],
  "incompatibilities": []
}
```

- `id`: `<author>.<mod>`, as SMAPI's `UniqueID` is: exactly one dot, each part `a-z`, `0-9` and `-`, 64
  characters at most. It must equal the `id` in `PmModInfo`, or the mod is refused. The author `vanilla` is
  reserved, since the game's own content uses it. There is no registry: a second mod with an id already
  found is refused. The bundled mods use the author `phasmomods`, which is reserved for the mods that come
  with PhasmoMods Loader (below).
- `name` and `author`: what players read, 1 to 64 bytes. `description`: one line for the Mods panel, 1 to
  512 bytes. None of the three may hold a control character, a line separator or an angle bracket, since the
  game draws text as rich text.
- `version`: strictly `MAJOR.MINOR.PATCH`, as Semantic Versioning means them, with no pre-release tag and
  no leading zero, and each part fits 32 bits. Versions compare part by part as numbers, so `1.0.10` is
  newer than `1.0.1`.
- `dll`: the file name of the DLL beside the manifest, with no folder, and not a name Windows keeps for a device
  (`NUL.dll`, `COM1.dll` and the like).
- `website`: optional, an `http` or `https` address, or empty.
- `dependencies`: optional, at most 64, each with an `id` (either format; a bundled mod's old flat id means that
  mod), an optional `minimumVersion` and `required` (`true` unless it says `false`: the mod uses that one when it
  is there). No mod twice, and not the mod itself. The loader enforces them: see "Dependencies" below.
- `incompatibilities`: optional, at most 64 ids of mods it cannot run beside, enforced as "Dependencies" says.
- `recordsVoice`: optional, `true` when the mod keeps other players' speech in memory during a contract. The consent
  notice then adds a "Voices are recorded." paragraph that names the mod. The player agrees before your DLL loads, so
  this is what they agreed to: a DLL that sets `PM_MOD_RECORDS_VOICE` when its manifest does not say `true` stays off,
  with a reason on its row. An old-format `mod.ini` cannot say it, so such a mod that records voice needs a `manifest.json`.
- `developerOnly`: optional, `true` when the mod is a tool for mod authors rather than for players. Players never see it:
  with the developer setting off (the default in launcher.ini), the launcher, the Mods panel, the loading order and every
  lobby list treat it as not installed. A developer puts `developer=1` in launcher.ini to make it appear; it is then chosen
  like any mod and follows every lobby rule while chosen.
- A field the loader does not know is named once in the log and ignored, so a later loader can add fields.

An old-format `mod.ini` may not use the id `vanilla`, or a bundled mod's old flat id (`custom-ghosts` and the
like), which always means that bundled mod.

A manifest that is not valid JSON, is larger than 64 KiB, nests deeper than 16 levels, names one field
twice in one object, or breaks any rule above leaves that mod off. It never stops the game or the other mods: the log
and a greyed row of the Mods panel say why, and so does the launcher's window.

**The `phasmomods` author is reserved.** Only the mods in the loader's install folder, beside PhasmoMods Loader,
may use the id author `phasmomods` or the manifest author `PhasmoMods` (compared ignoring case and surrounding
spaces). A mod anywhere else that claims either stays off, with a greyed row saying why, and the real mod is
unaffected whichever folder is read first. Pick your own author. This stops honest mistakes and casual
impersonation. It does not prove where a file came from: nothing is signed, and anyone who can write to the install
folder can replace a mod there.

What stays in the DLL. `apiVersion`, `channel` and `flags` stay in `PmModInfo`, because they are
compiled in and cannot drift from the code; the DLL's `apiVersion` answers what SMAPI's
`MinimumApiVersion` would. The host reads them once it has loaded the DLL, so a mod they refuse has shown in the
notice and the Mods panel, and its row gets the reason then. A lobby compares mods by id and by the build (a hash of the DLL), since an
author can rebuild without changing the version; the version and name are what players read.

Old mods. A folder with a `mod.ini` and no `manifest.json` still loads, with its flat id (1 to 40
characters of `a-z`, `0-9` and `-`), its version shown as unknown and one log line. It has `id=`, `name=`,
`dll=`, `description=` and an optional `requires=`, one `key=value` per line. This goes at the next major
contract version. A folder with a DLL and neither file is refused.

Where the host finds mods. When the launcher started the game, the host runs the DLLs the launcher
listed for that process (each copy's manifest sits beside it as `<copy>.json`, or `<copy>.ini` for an old
mod; the mods the launcher refused are listed too, with why). Otherwise it reads every folder in `mods`
beside the host, in name order. Which mods start is the player's choice, saved in `launcher.ini` (`enabled=`
and the mod ids) and changed from the Mods panel. A mod chosen but unable to run on this game build stays chosen,
so it runs again once it can. The host loads a mod's DLL when the mod first starts, after the player agreed, and it
then stays loaded until the game exits, whether the mod runs or not.

Once it has loaded a DLL, the host refuses one that lacks any of `pmmod_info`, `pmmod_start`, `pmmod_tick` or
`pmmod_stop` ("is not a mod for this loader"), one whose `apiVersion` is not compatible with the host's
(`pm_api_compatible`), one whose `id` is not its manifest's, one that names a channel it may not (1 to 63 belong to
the loader's own mods, and 64 to 255 are the ones the loader gives out from ids), and one that sets
`PM_MOD_RECORDS_VOICE` when its manifest does not say `recordsVoice`.
The mod stays off until the game restarts, with its row and the log saying why, and a mod that requires it reads
"Needs <name>, which did not start". Channels are settled before any mod starts: every planned mod's DLL is loaded and
asked what it is first, and every mod not yet running on a channel two or more mods land on is refused, each row naming
the others, so a colliding mod never runs. A mod already running from an earlier start keeps its channel.

`PmModInfo` (`core/mod_api.h`), returned by `pmmod_info`, callable at any time:

| Member | Meaning |
| --- | --- |
| `apiVersion` | `PMMOD_API_VERSION`, the version the mod was built against. |
| `id` | The manifest's `id`, `<author>.<mod>`. |
| `name` | For the log. Players read the manifest's `name`. |
| `channel` | The mod's own network channel. A mod outside the loader's own declares 0 and the host gives it one from 64 to 255, derived from a stable FNV-1a hash of its id, so every client computes the same one with nothing to negotiate; declaring 64 to 255 yourself is refused. 1 to 63 are reserved: only the loader's own mods, whose id author is the project's, name a fixed channel. Channel 0 on the wire stays the host's own: no mod ever runs on it. |
| `flags` | `PmModFlags`, below. |

Flags:

- `PM_MOD_VANILLA_PEERS`: reserved. A mod built for an earlier contract may set it and still loads, and
  the host ignores it: every mod is required by every player in a lobby (see "The lobby's rules" below). A
  new mod leaves it clear.
- `PM_MOD_RECORDS_VOICE`: the mod keeps other players' speech in memory during a contract. Say so in the
  manifest too (`recordsVoice`), which is what the consent notice reads: a DLL that sets this under a manifest
  that does not say so stays off.

The lifecycle, every call on the main thread:

- `pmmod_start(api)`: once, after the player agreed to the consent notice. It returns false to stay
  inactive, and the host then removes the hooks this start created. Call `pm::mod::bind(api, &info)`
  first.
- `pmmod_tick()`: every frame once started.
- `pmmod_stop()`: once, when the host stops the mod, in reverse start order, so a mod stops before the mods it
  depends on. The mod may be started again later, and its hooks stay until the host removes them.
- `pmmod_requires`: the game entries the mod needs (see "What a mod requires"), in an array the mod
  keeps, unchanged, for as long as its DLL stays loaded.
- `pm::mod::guarded_start`, `guarded_tick` and `guarded_stop` (`core/mod_glue.h`) wrap the three
  bodies so that nothing thrown leaves the mod; `mods/ghost_tells/mod.cpp` shows all of it.

Networking. `send(channel, data, size, to, reliable, actor)` sends a message on the mod's own channel,
at most `PM_MAX_MESSAGE` (511) bytes, `to` being `PM_TO_OTHERS`, `PM_TO_MASTER` or `PM_TO_ACTOR` (with
`actor`). Payloads ride on Photon event 197 behind a one-byte channel. It returns false on the host's
own channel 0 and outside a private room, where the suite behaves as if it were not installed.
`set_receiver(channel, receiver)` registers `void receiver(int32_t sender, const uint8_t* data, size_t
size)`; null stops receiving. `sender` is the sending client's Photon actor number, taken from the
event, never the payload, so check it for any message only the master may send. A message goes out only to
verified players, and reaches it only from one, on a client that matches the host, so a send before then
is refused and is not delivered later: a mod that announces something repeats it, as Playable Ghost does. The host drops
the receivers when it stops. `peer_has_mod(actor, modId)` says whether that client is verified and the host runs the mod; a
mod has no need of it.

Services. `register_service(name, table)` publishes a table of C function pointers for other mods and
`service(name)` returns it, or null when nobody registered that name. The table must stay valid for the
process lifetime, so version it in the name (`"playable-ghost.v1"`) and make the caller check for null
(`core/services.h` lists the bundled ones). A name is published once: the host refuses a second
`register_service` of a name already registered, logs it, and keeps the first table, so no mod can
take over another's service. The host forgets the services when it stops.

### Dependencies

A mod names the mods it needs, and those it can use, in its manifest's `dependencies`, and the mods it cannot
run beside in `incompatibilities`. The loader does the rest from the manifests (`host/mod_deps.h`), so the launcher
and the Mods panel know them without loading a DLL. A mod never checks for another mod itself. A mod that declares
none depends on nothing, so a mod built for 2.0 to 2.2 without them loads as before.

```json
"dependencies": [
  { "id": "phasmomods.spectral-flame", "minimumVersion": "1.0.0" },
  { "id": "phasmomods.custom-ghosts", "required": false }
],
"incompatibilities": ["someone.other-flames"]
```

- **Start order.** A mod starts after every dependency it names that starts too, required or optional. Otherwise
  the order of the player's choice decides (launcher.ini's `enabled=` line, which the launcher and the Mods panel
  save). So a later mod's hooks on a method run before an earlier one's (core/mod_api.h `create_hook`).
- **Required.** A required dependency that is not installed, is switched off, is older than `minimumVersion`, or
  does not start leaves your mod off. A dependency does not start when its DLL is refused once loaded, its
  `pmmod_start` returns false, a game entry its `pmmod_requires` names did not resolve, or its own dependencies leave
  it off; that cascades down the
  chain. The log names each mod left off with the chain of why, and its Mods panel row says so first: "Needs
  Spectral Flame", "Needs Spectral Flame, which did not start", or "Needs Spectral Flame 1.2.0 or newer, and you
  have 1.0.0".
- **Optional.** A missing optional dependency changes nothing but the order. Ask whether it runs with
  `pm::mod::mod_started(id)` (the host contract's `mod_started`) from `pmmod_start` onwards, each time you need to
  know, and do not keep the answer from `pmmod_start`. A dependency older than your `minimumVersion`, or on an edge that
  would close a cycle, gets no place in the order and may start after you, so the answer can turn true later. It says
  nothing of the other mod's version, so one older than your `minimumVersion` still answers true once it runs.
- **Versions.** `minimumVersion` compares as numbers, part by part. An old-format mod's version is unknown, so it
  never meets one.
- **Incompatibilities.** A mod that names an installed mod the player switched on stays off, its row saying "Can't
  run beside <name>". The other mod is unaffected. Two that name each other both stay off.
- **Refused declarations.** Mods joined by a cycle of required dependencies stay off, and the log names the cycle.
  An optional dependency that would close a cycle is ignored for the order, logged. A mod naming itself or one id
  twice stays off (the manifest is refused before that). A dependency on an id no installed mod has counts as
  missing. At most 256 mods start, in the order of the choice, which is as many as another player's client hears.
- **Switching.** Switching a mod on in the Mods panel or the launcher switches on every mod it requires; switching
  one off switches off every mod that requires it. Optional dependencies switch nothing.
- **Stopping.** Mods stop in reverse start order, and a restart starts them in the same order again.
- **Lobbies.** A dependency decides only what starts on this client. A lobby's rules are unchanged (see "The
  lobby's rules"): every player runs the same mods. A mod those rules switch off takes the mods that require it
  with it, each named in the notice. One the lobby's host runs cannot run here, so the player leaves and is shown
  the mods the lobby requires.

### Calling the game

A map header ends with the ready-made `phasmo::` wrappers for its type. Include the header and call
them:

```cpp
#include "GhostAI.h"

phasmo::GhostAI::UnAppear(ghost);                          // a method: the object comes first
const bool hunting = phasmo::GhostAI::isHunting.get(ghost);  // a field
const int32_t state = phasmo::GhostAI::state.get(ghost);     // compare it with phasmo::GhostState::idle()
// Change state through the method: it also starts the state's behaviour, which writing `state` skips.
phasmo::GhostAI::ChangeState(ghost, phasmo::GhostState::idle(), nullptr, false);
```

Every `phasmo::` wrapper reaches every mod, Unity's own included: `Transform.h` (the hierarchy, but
`Find`), `UnityObject.h` (`Object`, `GameObject`, `Component`, `Behaviour`: names, copies, destruction,
components, switching on and off) and `UnityUI.h` (`GridLayoutGroup`, `Button.Press`, `UIBehaviour`,
`Selectable`), mapped for the host's journal. A scene object a mod reaches through them is the game's
own: what it changes, it changes for the game.

Eleven kinds of wrapper, from `sdk/api/game_api.h`:

| Wrapper | Declared as | Use |
| --- | --- | --- |
| `pm::api::Method<D, R(A...)>` | `static constexpr ... UnAppear{}` | `UnAppear(ghost)`; `available()` says whether it resolved. For an instance method the first parameter is the object. A call runs the declared body; `virtual_call(ghost, ...)` runs the override the object's class runs, as C# calls a virtual method, and refuses (logged once, nothing called) a null object or one not of the declaring type. |
| `pm::api::Field<D, T>` | `Field<D, int32_t>` | `get(obj)`, `set(obj, v)`. `T` is a plain value or `void*` for a managed reference, whose `set` goes through the GC write barrier (the host's `set_reference`). |
| `pm::api::StaticField<D, T>` | `StaticField<D, void*>` | `get()`, `set(v)`: a static field, one value for the class, read and written by the host (`static_get`, `static_set`). A reference's `set` goes through the GC write barrier. The first access runs the class's static constructor if it has not run, as the game's own first use would, so use it while the game plays (a tick, a detour), never at startup. When the host cannot read it (a `T` of the wrong size, a static constructor that threw) it logs once and gives the default; `read(out)` answers false instead, for a gate that must tell a null value from a read that failed. `T` is a number, a bool, an enum or `void*`: a struct does not compile, since a whole copy would store any reference inside it without the write barrier. |
| `pm::api::Icall<D, R(A...)>` | `static constexpr ... SetBoolID{}` | `SetBoolID(animator, id, value)`: a Unity engine function the runtime registered by its icall name. It is native code, so it takes no MethodInfo. |
| `pm::api::EnumValue<D>` | `idle{}` | `idle()` gives the value's number in this build. |
| `pm::api::SceneObject<D>` | `BookContent{}` | `find(root)`: the Transform at the Scene entry's path under the Transform `root`, as Transform.Find finds it; null when it is not there, logged once per scene load. The host keeps one answer per entry, for the last root, until the next scene load: repeated calls with the same root cost no search, while alternating roots searches each time. A null root answers null at once. |
| `pm::api::Layer<D>` | `PlayerView{}` | `index()`: the layer's index, resolved by its name when the host activates; -1 (never 0, the Default layer) when no layer has the name. |
| `pm::api::ShaderProperty<D>` | `TintColour{}` | `id()`: the shader property's id (Shader.PropertyToID) for a material's by-id functions, read by its name when the host activates; 0 (logged once) when unresolved, so check `available()`. Whether a material has it is the material's to answer (`Material::HasProperty`). |
| `pm::api::AnimatorParam<D>` | `someParam{}` (hypothetical: no AnimatorParam entry is mapped yet) | `hash(animator)`: the parameter's hash for the Animator's by-id setters, checked against that Animator's own parameters when that Animator is first asked (and again after a scene load); repeated calls with the same Animator cost no read, while alternating Animators reads each time. 0 when it lacks the parameter, logged once per scene load. |
| `pm::api::AgentSteering` | `NavMeshAgent::steer{}` | `steer(agent, dx, dz)`: walks the NavMeshAgent in that world direction this frame, on this client, the game's destination and speed its own; ask every frame in `pmmod_tick`, and stop asking to hand it back. An object that is not a live NavMeshAgent changes nothing. `steer.available()`: whether the host can in this game build, asked in `pmmod_start`. The host does the native work (the host contract's `steer_agent`) with entries of its own: this wrapper wraps no entry, and the mod requires none for it. |
| `pm::api::Type<D>` | `type{}` | `klass()` gives the `Il2CppClass*`; `is(obj)` tells whether an object is of the type or a derived one (an interface type never matches); `type_object()` gives its `System.Type` (C#'s `typeof`); `make()` a new object, fields zeroed and no constructor run (not for engine objects, which come from `AddComponent` or `Instantiate`); `make_array(n)` a new array of the type. |

Every wrapper but `AgentSteering`, which wraps no entry, has two members besides its calls: `desc`, the map entry it wraps (its key, kind and
flags; `hook<M>` and `inside<M>` read it), and `use`, the entry's `pm::api::Use<D>`, whose `entry()`
and `resolved()` make the entry required (see "What a mod requires").

A wrapper's shape must be its entry's, or it does not compile. Where the entry lists `params`, a
`Method` or `Icall` takes one argument per param, after the object unless the entry is `kStatic`
(a method named without `params` is matched at any arity, so its wrapper is not checked). Where a
field's `type` is a C# value type by name (`bool`, `int`, `float` and the like), a `Field` or
`StaticField` whose `T` has another size does not compile. `tests/compile_fail_test.cmake` builds
wrong wrappers of every kind and expects each to fail with its own `static_assert`.

A call is never virtual: `UnAppear(ghost)` runs the body the map entry names, even when the object's
class overrides it. Use `virtual_call` to run the override.

A managed reference a wrapper gives (a `void*` field or return value, `type_object()`, `make()`,
`make_array()`) is valid for the current frame only. Do not keep it across frames: keep the object
in a `pm::api::Held`.

The game API gives no lookup by a raw path string and no setter that takes a raw layer number: a path,
a layer and an Animator parameter are map entries, so these facts stay out of mod code.

A call that cannot reach the game never jumps to null. An entry that did not resolve logs
`<key> is not available in this game build` once and returns the type's default (0, false, null) or
does nothing. A field read of a null object gives the default; a write to one does nothing. "Once"
holds across threads: each line's flag is atomic.

GameAssembly's exports are not for mods. Only the host's own code reaches an export entry, through
`pm::api::host_only::game_entry`, which exists where `PM_HOST` is defined (ModHost). The host
contract's `game_entry` gives a mod null for one, so a mod that requires an export is never started
(gamecheck's `--mods` reports it as switched off). The map holds no export entry at present.

### Strings, arrays, lists and engine objects

A mod never reads the runtime's object layouts; the host does, through the host contract's runtime
services. `sdk/api/runtime.h`:

```cpp
const std::string name = pm::api::to_utf8(str);         // a System.String as UTF-8; "" for null
void* text = pm::api::make_string("Bloody Mary");       // a new System.String
const pm::api::Array<int32_t> numbers{arr};              // value elements: numbers.size(), numbers[i]
const pm::api::RefArray objects{arr};                     // references: objects[i], objects.set(i, obj)
const pm::api::List players{list};                        // a List of references: players.size(), players[i]
players.add(player);                                      // the List's own Add; players.contains(player)
if (pm::api::alive(camera)) ...                          // UnityEngine.Object's implicit bool
```

An index past the end logs once and touches nothing (a reference reads null, a value element is a
zeroed scratch copy). `RefArray::set` stores through the GC write barrier. These views and the raw
pointers they wrap are valid for the current frame only: keep the object itself in a `pm::api::Held`
and make the view again. An element reference from `Array` and the array `List::items()` gives are
valid only until the next managed call: a List replaces its backing array when it grows, so take
`items()` again after any game call, and never store either. An object of the wrong kind (a list
passed as a string, say) reads nothing: the host logs it once and the service answers 0 or null.
Before `bind` every service answers as for a null object.

### What a mod requires

What a mod requires is what its code calls. Every wrapper member is a template, so an entry is
registered only when code in the mod uses it: only `Use<D>::resolved` names the entry's
registration, so naming a wrapper or `Use<D>` registers nothing (MSVC would otherwise register an
entry wherever its `Use<D>` is named). A call, a field `get` or `set`,
an enum value, `klass()`, `is()`, `type_object()`, `make()`, `make_array()`, `virtual_call`, `available()`, `hook<M>` and `inside<M>` all use it. The mod's
`pmmod_requires` export lists exactly those entries' keys, less the entries whose map entry carries
`kOptional`. That flag belongs to the entry, so it holds for every mod; a mod cannot make an entry
optional for itself. Check an optional entry with `available()` before using it. `pm::mod::bind` (called
first in `pmmod_start`) checks `pm_api_compatible(host, mod)`, looks every used entry up through the
host's `game_entry`, and returns false with the missing keys logged when a required one did not
resolve. The host does not start a mod whose required entries are missing: the log names them all, and its Mods
panel row names the first ("This game build lacks GhostAI.UnAppear and 3 more"). A mod without `pmmod_requires`
requires nothing. This is what lets a game update reach players without a new release: whatever build they are on,
the mods that still find what they require run, and the rest stay off, named.

Ghost Tells lists every entry its tells use (`build\gamecheck.exe --mods` prints any it would miss).
Including a header requires nothing by itself; only the members the mod's code actually uses become
required.

### Hooking and caller context

```cpp
// The detour takes the method's own arguments after `original`; the MethodInfo is passed on for it.
void detour(pm::api::Original<phasmo::GhostAI::ChangeState> original, void* ghost, int32_t state, void* interact,
            bool force) {
  original(ghost, state, interact, force);  // the rest of the chain of the body the game called
}
const bool hooked = pm::api::hook<phasmo::GhostAI::ChangeState>(&detour, "my hook", pm::api::Twins::All);
```

`hook<M>` creates a MinHook detour on the method through the host's single MinHook instance. It is
created, not enabled: call `enable_hooks` (or `hooks::enable_all()`). Hooks chain: the newest detour
runs first and its `original` reaches the previous one. A mod hooks a method once; a second `hook<M>`
in the same mod is refused and the first keeps its detour.

`pm::api::below_hook<M>()` gives what a detour's `original` calls on M's canonical body, for the mod's own code outside
a detour: a call under this mod's hook and over no hook made after it, which a later hook on M by another mod does not
see. Before `hook<M>` it does nothing. After the mods stop it is invalid until `hook<M>` succeeds again, since the host
has removed the hook its `original` points through.

The obfuscator makes same-behaviour copies of some methods (twins). With `Twins::All` (the default)
every twin is hooked as well, so the detour runs whichever body the game calls; with
`Twins::CanonicalOnly` only the canonical body is hooked, which is what existing code hooked by raw
address and so what a migration uses. Each body has its own chain, and `original` continues the chain
of the body the call came in through, so typed hooks of either policy, and raw hooks on one body,
coexist on one method in any order: a hook on the canonical body alone never runs for a call through
a twin. That stops holding once a 2.0 mod's all-twins `hook_entry` is on the method: its one detour
continues every call on the canonical body's chain, so a call through a twin also runs the hooks on
the canonical body below it (the limit `hook_entry` documents in `core/mod_api.h`). The
game API makes one entry function per body for this, up to `pm::api::kMaxBodies` (8); an all-twins
hook on a method with more bodies is refused and logged, with nothing hooked. A twin folded onto the
canonical address is one body, hooked once.

`pm::api::inside<M>()` tells whether a call of M is running on this thread, from a thread-local depth
the host's own hook on M keeps (`inside_entry` in the host contract). It answers only for methods the
host tracks (`PM_TRACK` in `host/context.cpp`): `GhostController.Spawn`, `GhostWanderState.Exit` and
`LiftButton.NetworkedUse`. The host hooks them once the player agrees to the mods, before any mod starts. A mod
that requires one whose hook could not be made stays off, and the log names the method, so the answer is never
false for that reason alone while a mod runs.
It replaces return-address checks, which break when several mods chain detours on one function.

### The host contract, version 2.6

`PMMOD_API_VERSION` is a major number in the high 16 bits and a minor in the low 16 (2.6 today).
`pm_api_compatible(host, mod)` accepts a mod of the same major and a minor no newer than the host's.
Within a major, `PmHostApi` and every published struct only grow at the end; a new major refuses
older mods. Version 2.0 added these members, after the services of version 1:

- `game_entry(key)`: the resolved `PmGameEntry` for a map key (type, method, code, twins, field
  offset, enum value), or null when unknown or unresolved, and for a GameAssembly export or a native site,
  which only the host uses.
- `hook_entry(key, detour, original, name, allTwins)`: a hook by entry with one detour. Since 2.1 it is
  made from per-body hooks: without `allTwins` it hooks body 0, with it every body, and `original`
  receives body 0's chain. So a call that entered through a twin goes on along the canonical body's
  chain, past the twin's own older detours. The game API no longer calls it; `hook_body` replaced it.
- `set_reference(obj, offset, value)`: stores a managed reference through the GC write barrier. The
  only correct way to write a reference field; `Field<D, void*>::set` calls it.
- `inside_entry(key)`: the thread-local depth above.

Version 2.1 added the runtime services, after `inside_entry`. A mod built for 2.0 still loads. Each
runs on the main thread, never throws, and answers null, 0 or false for a null argument and for a key
that is unknown, unresolved or of the wrong kind:

- `string_new(utf8)` and `string_utf8(str, out, size)`: a managed string from UTF-8, and its UTF-8
  text (the whole length is returned, so a caller can size its buffer).
- `array_length(arr)`, `array_data(arr)` and `array_new(elementTypeKey, length)`; `list_count(list)`
  and `list_items(list)`.
- `type_object(typeKey)`, `object_new(typeKey)` and `is_instance(obj, typeKey)` (which follows the
  class's parents).
- `alive(unityObject)`: UnityEngine.Object's implicit bool.
- `virtual_method(obj, methodKey)`: the MethodInfo the object runs for the method; its first member is
  the address of its code.
- `gc_hold(obj)` and `gc_release(handle)`: strong GC handles, which `pm::api::Held` uses.
- `log_exception(where, exception)`: logs a caught managed exception with its stack.
- `static_get(key, out, size)` and `static_set(key, in, size)`, after `log_exception`: a static
  field's value, `size` bytes, read into `out` or written from `in` (a reference through the GC write
  barrier, `in` pointing at the pointer). Each runs the class's static constructor if it has not run
  yet, so the host never calls them while it resolves the map at startup, and a mod calls them only
  while the game plays. False, touching nothing, for a key that is not a resolved static field, a
  `size` other than the field's (checked before the class is initialised, so no static constructor
  runs), or a static constructor that threw (logged once per key).
- `hook_body(key, bodyIndex, detour, original, name)` and `body_count(key)`, after `static_set`: a
  method's bodies are its canonical body (index 0) and each distinct twin, and `hook_body` hooks one of
  them, chained on that body alone. `body_count` is 0 for an entry that is unknown, unresolved or not a
  method. `hook<M>` is built on these.
- `scene_find(key, root)` and `animator_param(key, animator, hash)`, after `body_count`: the scene
  facts. `scene_find` gives the Transform at a Scene entry's path under `root`, kept until the next
  scene load; `animator_param` says whether the Animator has an AnimatorParam entry's parameter, with
  its hash. Both log a miss once per scene load and answer null or false. A layer needs no service:
  `game_entry` gives its index in `value`. Nor, since 2.2, does a shader property (the entry kind
  `PM_ENTRY_SHADER_PROPERTY`): `game_entry` gives its id in `value`.

Version 2.2 added content, after `animator_param`: the ghosts and evidence a mod adds to the game,
through `sdk/api/content.h` (`pm::api::content`). A mod built for 2.0 or 2.1 still loads. A definition
(`PmGhostDef`, `PmEvidenceDef`) begins with its own size so it can grow, and holds text in several
languages (`PmTextSet`, English required). The host copies it when it is registered; `user` and the
callbacks must stay valid while the mod runs. Registration only records content: nothing reaches the
game yet. A ghost's `flags` (after `evidenceLook`) take `PM_GHOST_KEEPS_LAST_EVIDENCE`: its third
evidence is never the one a difficulty withholds, through the game's own evidence pick, as for the
Goryo (`sdk/systems/evidence.md`). Its `target` (after `flags`) answers the one player its hunts go for,
never a dead player nor one of the players it is told to exclude, or null for no restriction, as the
game's Banshee keeps to one target (`sdk/internals/ghost-ai.md`, "The Banshee's one target"). The host
holds every hunt of the ghost to it, its AI's and a player's alike (`host/content_target.h`): it asks it
at the start of each hunt with nobody excluded, and again wherever the game re-targets a Banshee (a kill,
the Hanged Man card), excluding the dying player, and when the target leaves the game, excluding nobody;
it keeps the answer in between. Playable Ghost shows it on its player's HUD.

- `ghost_register(def)` and `evidence_register(def)`, from `pmmod_start` only: a handle greater than
  0, or 0 (logged with the id and why) for a call outside `pmmod_start`, a repeated or malformed id,
  text without English or over the limits (64 tells, 64 languages, 64 KB a text), or an evidence that
  is neither vanilla nor a content id. A custom evidence a ghost gives may come from any mod that
  starts, before or after it: once every mod has started the host settles the content, and drops a
  ghost whose evidence nobody registered. Since 2.3 the mod that registered it stays off for that start, logged
  and on its Mods panel row, and the others start again without it. A mod that fails to start takes its
  content with it.
  Since 2.3 the host also refuses every ghost and evidence while custom content is off on this client (the
  game's enums unreadable, or a game entry or hook its content needs missing), and keeps off any mod whose
  `pmmod_start` registered something it refused, whatever `pmmod_start` returned: it stops the mod, removes
  its hooks and content, and logs why. The same holds for every host call that fails during `pmmod_start`:
  a hook that cannot be made or switched on, a game entry that cannot be hooked, a service name already
  taken. A mod need not check what such a call returned: no mod runs with part of itself missing.
- `content_type(ghostId)`, `content_ghost_id(type)` and `content_evidence(evidenceId)`: a mod names
  content by its stable id, `"<mod id>.<name>"` (`phasmomods.custom-ghosts.draugr`), and never chooses a
  number. A name holds no dot, so the id splits at its last dot (`core/mod_ids.h`). Nothing here fixes the
  game's counts: at startup the host reads the game's GhostType and EvidenceType enums whole, every
  value with its name (`sdk/runtime/game_enums.h`), and numbers custom ghost types from one past the
  largest GhostType value and custom evidence from one past the largest EvidenceType value (31 and 8 in
  the audited build), in byte order of the ids, answering them only while content is active or a contract
  that loaded with content active holds them (until it ends, whatever the room's state does). Vanilla
  ids come from the values' names: `"vanilla.mare"` is 6; the evidence keeps the contract's ids,
  `"emf"` to `"dots"`, and evidence a game update adds is named by its value in lower case. A value
  whose name is not plain ASCII gets no id (logged). Enums that cannot be read keep content off
  (logged). `gamecheck` prints the ranges as its `CONTENT` line ("GhostType 0..30, EvidenceType 0..7").
- `content_active()`: content is in play only in a private room where every player's client sent a
  content manifest (its ids, each with a hash of its definition) equal to this client's. Manifests
  travel on the host's own channel as players join and every 2 s (`sdk/runtime/content_registry.h` has
  the format). A player without a content mod, or with another build of one, withdraws all custom
  content for that room; one not heard from yet keeps it pending for up to 10 s. A client whose
  content goes away sends an empty manifest. The host's own content code reads the three states
  (active, pending, off) through `suite::content_state()` and acts on off only.
- `content_player_controlled()`: whether a player controls the contract's ghost, read from the Playable
  Ghost mod's `playable-ghost.v1` service.
- `content_roll_taken()`: whether that player takes the actions of the game's idle roll running now, read from
  the same service (version 3). A rule on the AI's attempts asks it rather than assume an order of hooks: such a
  roll only grants the player a charge, and their attempt comes later.
- `steer_agent(agent, dx, dz)`, after `content_roll_taken`: walks a NavMeshAgent in that world direction, of
  length at most 1, until the mods tick next frame, on this client. Its destination, speed, acceleration and
  avoidance stay the game's. An object that is not a live NavMeshAgent changes nothing. True when the host can
  steer, so a null `agent` asks only that; false (logged) when it cannot in this game build, and after a refusal the
  host tries again only when the mods start afresh. The game entries it needs are the host's own, which no mod
  requires. `phasmo::NavMeshAgent::steer` is built on it. See
  [Steering an agent](systems/ghost-ai.md#steering-an-agent).

Ghost types in the game, also 2.2, after `content_player_controlled`. While content is active the host
puts each custom ghost into the game as a ghost type (`host/content_types.h`, following
[ghost types](systems/ghost-types.md)): a GhostData in the contract's GhostCollection in its
alphabetical place, its name for the game's name key, the custom type as its identity and the game's
"no type" as its AI's behaviour type on every client, and the custom types after the game's own in the
Mimic's copy list (not one marked `PM_GHOST_NO_MIMIC_COPY`). Whether a contract has them is decided once, when it loads: content active then
puts them in, and the decision holds until the contract ends (content going pending or off in between
changes nothing, and the contract's custom rules keep running). They leave at the contract's end and
when the mods stop, only the types this client's content numbers; a removal the game's List refused is
logged and tried again until it is done. In the lobby the custom ghosts follow the room instead: while
content is active there they are offered, put the same way into the difficulty menu's collection and
the collection each lobby journal lists, and they leave those again as soon as content stops being
active there or a contract loads. The manifest also carries the numbers the
game's enums give the first custom ghost and evidence, so two clients whose games read different
enums never agree; an enum value whose name would give another value's id (a future `Orbs` beside
`GhostOrb`'s `orbs`) gets no id, logged; and the host logs the enum ranges it read
(`content: GhostType 0..30, EvidenceType 0..7`).

A ghost may give no evidence at all: its three slots empty, null or `""`. Its GhostData then holds the game's own
"no evidence" in each, the game's pick gives only that, and the journal's filter lists it until any evidence is found
([evidence](systems/evidence.md#a-ghost-that-gives-no-evidence)). Version 2.3 added `PM_GHOST_NO_MIMIC_COPY`, a bit
of `PmGhostDef.flags`: a ghost with it is never on the Mimic's copy list, so a Mimic never copies it.

- `ghost_rules(ghostId, rules)`, from `pmmod_start` only: attaches a `PmGhostRules` (`start`, `tick`,
  `stop`, `huntSpeed`, `heldBack`, each optional) to a custom ghost's id or a vanilla one
  (`"vanilla.mare"`); several mods may attach to one id. They run only while this client's mods
  match the host's list (below). The wrapper is `pm::api::content::attach_rules(id, GhostRules)`.
- `content_acting_as()` and `content_identity()`: the type the contract's ghost acts as (its own, or
  a Mimic's copy, custom ones included) and its identity, -1 with no ghost.
- `journal_lines(vanillaGhostId, lines, count)`, from `pmmod_start` only: lines after a vanilla ghost's
  journal page text, each a `PmTextSet` (English required); several mods may add to one ghost. They
  show in a private room while this client's mods match the host's list (below). The wrapper is
  `pm::api::content::add_journal_lines(id, {Text...})`.

#### The host's list

**The host's list is the lobby's list.** Every mod treats loading the same, and no mod has any matching logic: it is
registered, gated and started by the host in one way. The list is what the room's master runs and announces. Nothing a joiner
runs, says or fails to say changes it, or changes what any other player's mods do.

The one question each client asks (`sdk/runtime/mod_agreement.h`) is whether its own running mods equal the host's list, in the
same builds (the presence that the join guard reads), after its extras are off. The host's answer is yes from the moment
its lobby opens, with no waiting on anyone, and so is a player's alone in single player. Outside a private room it is no.

**No answer changes during a contract.** A contract holds the answer it loaded with until it ends (`ContentTypes::contract_loaded`
says when), so rules never start or stop partway through, whoever leaves. A contract's start waits until everyone is verified, so
nobody gets there unmatched ([Photon](systems/photon.md)). **A new host** takes over with its own list, which equals the old
one, since it was verified; a client whose list differs follows the same rules as a joiner, in the lobby.

**A player is verified** when what they announced equals the host's list. What a player announced counts for as long as they
stay, until they announce again: a client whose mods stop while it is in a room announces once that it runs none
(`host/presence.h`), so it stops counting. Being verified decides who gets a custom value or a mod's message, and nothing else.
A message on a mod's channel goes only to verified players and reaches the mod only from one, so nothing a mod receives comes
from a client that lacks it, or from an older suite, whose presence reads as nothing here. A mod's own messages in single
player pass.

#### Content packs

A content pack is a mod folder of data alone, as SMAPI's: a manifest.json whose `contentPackFor` names the
mod it is for (`{"id": "<mod id>", "minimumVersion": "1.2.0"}`), an `assets` folder, an `i18n/default.json`,
and no `dll`. A pack lists no `dependencies` of its own: its framework is its one dependency, so the framework
never holds a pack that then cannot start. The loader treats a pack as a mod in every way but code: the Mods panel lists it, it needs its
framework (a required dependency), it starts right after the framework and stops with it, and the lobby's
list includes it, so a joiner without it is removed. Its build is a hash of its files. The framework reads
its packs from its own `pmmod_start` with `pm::api::packs::owned()` and reports a pack it cannot use with
`pm::api::packs::failed(id, why)`, which keeps that pack off with the reason in the Mods panel. What a pack
holds is the framework's own format: the Emotes mod reads an `emotes.json` catalogue and a bundle.

#### The lobby's rules

The host enforces these for every mod; no mod does any of it (`host/join_guard.h`):

- **Every mod on the host's list is required of every player.** `PM_MOD_VANILLA_PEERS` is reserved and ignored.
- **A joiner never changes what anyone else runs.** The host's mods and the other players' stay on while a joiner is being
  checked, and what the room would send a custom value in holds it back meanwhile.
- **A contract's start waits until everyone is verified.** Pressing start removes nobody: the normal timing below still applies,
  so the wait ends when the player is verified, leaves or is removed. The host hears who it is waiting for through the
  game's own popup, and presses again.
- **A contract only begins once every player's mods are confirmed.** On Start the host closes the room, waits for the server
  to confirm it, and reads the room again: then the start goes, or the room opens again and the host hears why
  (`host/start_close.h`, [Photon](systems/photon.md)). Nobody joins late, so every player in a contract runs the host's list
  and agreed to the notice. A mod relies on that and checks nothing itself: a mod that records voices (`recordsVoice`) records
  only players who agreed, with no check of its own.
- **Nobody changes the list from inside a room.** The Mods panel's changes wait until out of the room, the host's too.
- **A joiner lacking a host mod**, or running it in another build, leaves by itself and is then shown every
  mod the lobby requires. The host removes it with the game's own kick if it stays, and removes anyone it
  never hears from. A kick comes only from a master who is the room's first actor. The game's kick can do
  nothing without a word, so it counts only when the player leaves: the host tries a few times, then tells its
  player through the game's popup, and a contract cannot start while that player is here.
- **A joiner's extra mods** are off for that lobby, with a notice naming them. **A host running no mods**, or
  never heard, switches off all of a joiner's.
- **In a public lobby every mod is off**, the host's too, and nobody is removed. Mods run only in single player
  and private lobbies. The game unlists every room for a contract and can list it again in the lobby
  ([Photon](systems/photon.md)), so a room read as public stays public until it is left.
- **Mods come back on leaving the room**, or in the lobby when the host's presence arrives late.

Your mod is stopped and started again by these, as by the Mods panel: `pmmod_stop`, then its hooks and
network receiver removed, its content withdrawn, then `pmmod_start`. That happens only out of a room or on
the lobby's screen, and never during a contract.

Everything the host dispatches for the mods follows that one answer, so a mod gets it without writing a
check:

- `journal_lines` lines show only while this client matches the host. The pages are written again each time the
  answer changes.
- `ghost_rules` rules (`start`, `tick`, `stop`, `huntSpeed`, `heldBack`) run only while it matches. They never start partway
  through a contract.
- Content (`ghost_register`, `evidence_register`) stays pending until it matches, so a
  contract that loads then has none of it, the lobby's journals show none of it and the difficulty
  menu offers none of it.

A mod has no need of `room_agrees` or `peer_has_mod`, which stay so 2.x mods load, and keeps no "every client
runs the mod" state of its own: `tests/raw_scan_test.cpp` (`no_mod_has_matching_logic`) fails on either in `mods/`.
A mod that needs every client to take part in something can rely on this: whoever is in a contract runs its mod, in its build.

A player controlling the ghost reaches the ghost's own definition through the content layer, for any
registered ghost and by the type the ghost acts as (`content_use_ability`, `content_ability_text`,
`content_hud_state` with the definition's `hudLine`, `content_held_back`, which also asks the rules
attached to the type, a vanilla type's included, `content_player_view`, `content_target`,
`content_flags`, `content_player_move`, the definition's `playerMove` limit on where its player steers, and
`content_interaction_lost`, its `interactionLost`, which loses an interaction the player has chosen and leaves it
paid for; `pm::api::content::use_ability()` and the rest). Playable Ghost names no other mod's
ghost or type number.

The journal (`host/content_journal.h`, `systems/journal.md`). On every journal of every client, the
custom ghosts shown (a contract's that holds some, else, in the lobby, the ones offered while content is
active there) each get a guess button on the evidence spread's ghost page, an entry in the Ghosts tab's
list and a page after the book's last, copies of the game's own for the Spirit, in type order, so page
numbers agree between clients as the game's page sync needs. The lobby's journals are the players'
PCPlayer books, as a contract's are, so players can read a custom ghost's page before and after a
contract; when content stops being active in the lobby, or the mods stop, each journal is put back as
the game made it (its list's hand placement, the evidence marks and a guess on a custom ghost included,
and the local reader's book turned to its Ghosts tab when it was on a page that went). Since books of
different lengths now meet in one room, the book owner's page sync for a page outside this client's
book is skipped rather than left to the game's throw; every other page sync reaches the game
untouched, its check that the sender owns the book included. Every copied text keeps the
game's `TextLocaliserUI` with a key the host answers through `LocalisationSystem.TryGetLocalisedValue`:
the name in the player's language (its abbreviation, `GetLanguageAbbreviationText`, in lower case, as a
`PmText` names it), else English, as the game falls back. A page is laid out as the game's own: the
description, then strength and weakness under the game's labels in that language, separated as the
game separates them (as most game pages agree in that language, else in English), then the ability and
the tells. The game's localiser shows what stands before a text's first `@`.

The custom difficulty (`host/content_difficulty.h`, `systems/difficulty.md`). While content is active
the Ghost Type option of Custom 1 to 3 offers every custom ghost: they join the menu's `GhostCollection`
and the option's count follows it. Each preset's pick is kept by ghost id in
`%LOCALAPPDATA%\PhasmoMods\custom_difficulty.txt`, so new content that shifts the numbers leaves every
pick on its ghost. Any difficulty the lobby holds at Random for a preset with a pick (the presets, the
lobby's own, its copy and the next contract's) gets the pick back, and the host sends it. No save and no
sync but the host's own sends, while content is active and everyone in the room is verified, ever holds a custom type; while
content is not active (off, or pending: the host not heard from yet or this client's mods not matching it) the menu
offers the game's ghosts only and the lobby's picks go back to Random, each coming back once content is
active again. The summary names the custom ghost.

Custom evidence (`host/content_evidence.h`, `systems/evidence.md`), also 2.2, after `journal_lines`. A
custom ghost gives a custom evidence it lists through the game's own evidence pick: the evidence's
number stands in the ghost's `GhostData`, so a difficulty that gives fewer evidence withholds it as it
withholds the game's own, and the game's sync carries it to every client as a number. Nothing counts
the evidence: the pick, the sync and the journal's filter work on lists of numbers. On every journal,
whenever the custom ghosts shown give a custom evidence (in a contract, or in the lobby while content is
active), it gets a copy of the game's Spirit Box row
after the game's rows: its name and the line under it (`PmEvidenceDef::name` and `journalLine`) with
keys in the game's own form, read from its rows, answered in the player's language, else English. It
joins the journal's evidence buttons and its evidence sync reads them again, so it is marked, synced
and filters the ghost buttons as the game's rows do, and the column's spacing shrinks so every row fits
where the game's did. A custom ghost's page names it as the game's pages name theirs. What reveals the
evidence in the world stays its mod's job.

- `content_evidence_look(evidenceId)`: how the contract's ghost changes the evidence's look, what its
  `PmGhostDef::evidenceLook` answers (the Hydra's Spectral Flame is "green"), asked by the ghost's
  identity, so a Mimic that copies a ghost keeps the Mimic's look; null for the evidence's own. A
  contract that holds custom ghosts answers for its ghost until it ends. The wrapper is
  `pm::api::content::evidence_look(id)`.
- `list_add(list, item)`, after `content_evidence_look`: a runtime service, not content. It adds a
  reference at the end of a List<T> through the Add of the list's own class, as the game's code adds to
  its lists (`pm::api::List::add`); false for anything that is not a List.
- `copy_settings(typeKey, dst, src)` and `fields_of_type(typeKey, obj, fieldTypeKey, arrayOf, out, max)`,
  after `list_add`: runtime services that read a class's own fields from the game's metadata, so a mod
  names none of them. The first copies the public value fields a class declares (the settings Unity's
  inspector shows; a struct whole, its references through the write barrier), as Custom Ghosts' copies
  of SensorToolkit sensors need: their settings are a dozen obfuscated fields that no code fact tells
  apart. The second reads the references in every field a class declares of one type, as the Mesmer
  needs for GhostAI's two GhostModel arrays, which every method that reads one reads with the other.
  The wrappers are `Type::copy_settings` and `Type::fields_of`.
- `collection_count(collection)`, `array_element_size(arr)` and `list_remove_at(list, index)`, after
  `room_agrees`: runtime services that read what they need from the runtime, so no layout is assumed.
  The first counts a List, HashSet, Dictionary, Queue or Stack through its own `get_Count`, found by
  name on its class (`pm::api::count`; the Deildegast's moved props are a HashSet). The second is the
  bytes one element of an array takes as the runtime lays it out, a struct's own unboxed size, so a
  List<T> of a game struct is read as a `pm::api::ValueArray` (`List::values()`), each element's fields
  through the struct's field entries, whose offsets are the unboxed ones (GhostHearing's heard noises).
  The third removes an item through the List's own RemoveAt (`List::remove_at`). Each answers 0 or
  false, touching nothing, for an object of another kind (logged once).
- `mod_started(modId)`: whether that mod has started on this client and runs now, for an
  optional dependency, from `pmmod_start` onwards (`pm::mod::mod_started` in `core/mod_glue.h`). See
  "Dependencies". The fake backend answers it from `pm::fake::started_mods`.
- `show_message(title, body)`: a one-button message in the game's own popup on the menu's and the lobby's
  board (`pm::mod::show_message` in `core/mod_glue.h`). One asked for in a contract shows once the lobby is back. It
  replaces any message showing or waiting, so keep it for news a player needs. This is how a mod tells a player something
  outside a contract: never draw a window of your own. The fake backend records each in `pm::fake::messages`.
- Version 2.3 added `content_flags_of(type)`, a custom ghost's `PM_GHOST_*` flags, and `mod_name(modId)`, the name the
  installed mod's manifest gives it (`pm::mod::shown_name` in `core/mod_glue.h`).
- Version 2.4 added `unity_action_new(target, listener)`, a UnityAction made the way the game makes its own listeners, so a
  mod's button takes the game's own click (`sdk/api/buttons.h`).
- Version 2.5 added `mod_entry(index, out)`: the installed mod at an index (id, name, version, author,
  whether it runs, developer-only, records voice, and why it is off), filling a `PmModEntry` up to the `size` the mod sets.
  `pm::mod::mod_entries()` in `core/mod_glue.h` copies every one out. Read-only; it says nothing of other players' mods.
- Version 2.6 added `content_packs(modId, out, max, itemSize)` and `content_pack_failed(packId, why)`, the last
  members: the content packs made
  for a mod (wrapper: `sdk/api/packs.h`, `pm::api::packs`; see "Content packs", below). `content_packs`
  fills `out`, at most `max` entries, with the packs whose framework is `modId`, in id order: entries sit
  `itemSize` bytes apart, the host writes at most `sizeof(PmContentPack)` bytes into each and sets its
  `size`, and the answer is how many there are, which may be more than `max`; 0 for a null `modId` or an
  `itemSize` smaller than `sizeof(PmContentPack)`. `content_pack_failed(packId, why)` is for the framework,
  from its `pmmod_start` while its pack has not started: that pack stays off for this start, its Mods panel
  row says `why`, and it is not in the lobby's list; asked anywhere else it logs and changes nothing. The
  fake backend answers the first from `pm::fake::content_packs` and lists every second call, in order, in
  `pm::fake::failed_packs`.

Which rules run. The host keeps the contract ghost's acting-as type (`sdk/runtime/content_acting.h`):
its behaviour type, or its identity when the behaviour type is "no type" (a custom ghost). The Mimic's
copy is made on the host only; when it copies a custom type the host records the copy and sets the
behaviour type back to "no type", so the generic AI runs it. The game sends nothing of the copy; the
host sends the acting-as type to every client on its own channel when it changes and once to each
player who joins (the channel is reliable, so only a failed send is repeated), so a rule's client-side
effects run everywhere. It sends only when the player agreed to the mods and some rules are attached
or content is active. The rules of the acting-as type start when the ghost begins acting as it, tick
every frame, and stop when it stops; a switch stops the old rules and starts the new ones in one call.
A custom type's rules run in a contract that loaded with content active, for the whole contract; no
rules run outside a private room.

Hunt speed. A rule's `huntSpeed` runs on the host every frame of the ghost's hunt, just before the
game's own speed update (`GhostHuntState.UpdateSpeed`), with the game's base for this hunt and its
line-of-sight speed-up. What it returns true for is written as those two inputs, never as the speed:
the game then applies the multipliers every type shares once, and the footsteps and animations read
the real speed. Each frame the rule gets the game's own base (the host tells the game's writes from
its own), so a scaled rule never compounds, and the game's base comes back the frame the rule stops
asking. `heldBack` is for the content layer's readers: Ghost Tells answers it from the rules that hold
its six ghosts back, and Playable Ghost asks it before a player's action; nothing in the host itself asks it.

A mod's exports are `pmmod_info`, `pmmod_start`, `pmmod_tick`, `pmmod_stop` and (version 2 onwards,
exported by linking `pmmod`) `pmmod_requires`.

### Testing without the game

`sdk/fake/fake_game.h` is a host contract in which only the game functions are live. A test says what
each entry resolves to, binds the game API to it and calls the mod's code:

```cpp
pm::fake::reset();
pm::fake::method(pm::map::GhostAI_UnAppear, reinterpret_cast<void*>(&fake_unappear));
std::string missing;
pm::api::bind(pm::fake::host(), missing);
phasmo::GhostAI::UnAppear(ghost);  // runs fake_unappear(ghost, nullptr)
```

The other givers are `field(d, offset)`, `static_field(d, storage, size, reference)`, `icall(d, fn)`,
`value(d, n)`, `type(d, klass)`, `scene_fact(d)` and `layer(d)` (resolved through the names given to
`layer_names`) and `shader_property(d)` (to the id the fake's Shader.PropertyToID gives its name,
`property_id(name)`). A static field reads and writes the test's own storage, and a
`size` other than the one given answers as the host does for a field it cannot reach. An entry not
given stays
unresolved, as in a build that lacks it. `hooked()` lists the keys hooked through `hook_entry` or `hook_body`, and
`barrier_writes()` counts reference stores. A fake object begins with its class pointer;
`parent(klass, parentClass)` declares inheritance, which `Type::is` and `virtual_call` follow, and
`override_method(d, klass, fn)` gives a class its override. `method(d, fn, owner)` names the
declaring class, so `virtual_call` refuses an object of another class. The runtime services work on the fake's
own memory: `array(elementSize, n)` and `list(items)` make arrays and lists, `destroy(obj)` makes
`alive` false, and `held()` counts the GC holds not yet released. The scene facts run the host's own
logic over the fake's scene: `child(root, path, found)` says what Transform.Find answers,
`parameters(animator, list)` what an Animator has, `load_scene()` loads a new scene, and `finds()`
and `parameter_reads()` count the searches. `tests/game_api_test.cpp` shows every path. The content
functions run the host's own registry: registration is open from `reset()`, as while a mod starts,
until `commit_content()` settles it as the host does once every mod has started. `room(local, actors,
now)` places this client in a room, `host_message(sender, data, size)` delivers another client's
manifest (a test fakes that client with a `pm::content::Registry` of its own),
`host_messages()` gives what the host would send, `content_state()` says whether content is active,
pending or off, and `player_controlled(bool)` sets what `content_player_controlled` answers.
`ghost_rules` records rules, and `journal_lines` lines for vanilla ghosts, in the same registry; `contract_ghost(identity, behaviour, master)`
drives the host's acting-as dispatch as the host does every frame (rules start and stop as it
changes), `tick_rules()` ticks them, and `content_registry()` and `acting()` hand both to code the
host runs over them. The fake gates nothing on this client's match with the host until a test gives one:
`content_registry().use_agreement(&agreement)` with a `pm::content::Agreement` the test observes,
as the host does every frame. `value_list(values)`, `list_values`, `list_items_of`, `list_insert`,
`list_remove_at` and `list_add_value` make and change fake lists as the game's own List methods would. `collection(count)` makes a fake HashSet that `collection_count` counts, and `struct_list(elementSize, count)` a List of structs whose array `array_element_size` sizes.
`tests/content_test.cpp` shows them, with the host's game side over a faked game.

### Checking a mod against a game build

`tools/gamecheck.ps1` with `-GameDir` and `-Cpp2Il` resolves the map against a build without
running the game. Add `-Mods <folder>` to learn which mods the host would not start on that build
(see [the commands](internals/mapping.md#commands)). A build need not pass for your mod to run on it: the host resolves the map against the
running game, and a passed build only spares the player the notice's update paragraph ([game builds](internals/mapping.md#game-builds)).

## Mapping the game

The map, its entries and the resolver are documented for whoever adds entries in
[Mapping the game](internals/mapping.md): what an entry is, how the resolver finds one, a note's
confidence, the commands (`tools/gamecheck.ps1` and the build ledger), game builds, the order to add an
entry in, and the checks that keep raw values out of `mods/`, `core/` and `host/`. The tools and the
pitfalls of reading the game's code are in [reverse engineering](internals/reverse-engineering.md).
