# Replay in 3D: design and plan

Status: design approved 2026-10-21. Game build 25811160. Replaces the 2D map panel of `mods/replay`.

## Intent

After a round, the group walks over to the parked truck in the lobby, slides a cassette into the
truck's CCTV unit and watches that moment of the round together on its monitor: real footage of the
house with the game's sound and everyone's voices, the same clip on every screen at the same time.
It is a quick shared look back, not a replacement for recording software.

Today the mod finds the moments (kills, close escapes, chases, events seen up close, crucifix saves)
but shows them as coloured squares on a schematic map in a panel of its own, which is an invented
window. That panel goes.

## Decisions taken (user, 2026-10-21)

1. Live footage filmed during the round (not an in-engine replay rebuilt from positions).
2. Captions burned into the footage.
3. All audio, voice chat included, and the recording player's own voice too.
4. The truck monitor in the lobby, with the cassette props choosing the clip.
5. The point is to view clips quickly after a round as a group.
6. Windows 10 is supported, not only Windows 11.
7. Nothing plays by itself. The truck is off to the side of where players spawn, so the monitor
   waits until someone puts a cassette in.
8. Four people watching that one monitor must not be too cramped (see Watching together).

## What other games do, and why this design

| Family | Examples | Stored | Viewed |
| --- | --- | --- | --- |
| Input films | Halo Theater, StarCraft | Every player's inputs; the engine re-simulates | In the game, free camera; broken by patches |
| Network stream | CS2 demos, Rocket League, Unreal's DemoNetDriver, Minecraft ReplayMod | What the client received | Replayed into a loaded level; ReplayMod renders MP4 through FFmpeg |
| State snapshots | Overwatch kill cam and highlights | Chosen entities' states | Re-rendered in the engine; reset every patch |
| Captured video | Steam Game Recording timeline, ShadowPlay | Video cut at game-marked events | Any player |

The first three need the house loaded to watch, and the lobby is a different scene, so after a
round they could not be watched at all. They also need every ghost, investigator, flashlight and
door rebuilt. Filming while it happens, with the game's own video-camera rig, gives real 3D for a
fraction of the work. The game already does a one-frame version of it (a video camera's "video" is
one `Camera.Render` read back, CCTV 0x27790F0). Steam's timeline API is not available: the game's
`steam_api64.dll` has no `SteamAPI_SteamTimeline_*` exports.

## Goals

- Real footage of each top moment in the game's video-camera look, with captions burned in and the
  round's full sound, on Windows 10 and 11.
- In the lobby, one cassette per moment on the truck's desk; putting one in plays that moment on the
  truck monitor for everyone in sync, and every player can see it properly even with four in the
  truck.
- Each clip is also a normal MP4 with the mod's metadata, so it can be shared and shown again.
- Cheap: no puppets, no re-simulation, no new window or overlay, no package beyond Windows' own
  Media Foundation and WASAPI.

## Non-goals

- Archiving, editing, trimming, exporting reels, choosing angles. Recording software does that.
- Watching during a contract. Footage is only shown after the round, so it can never help an
  investigation (it shows the ghost's model, orbs on night vision, its room).
- Sending video between clients. Every client running the mod films its own copy.
- Journal media. Journal videos are single stills that earn money and objectives; replay clips never
  touch rewards.

## Anti-goals

- A hitch or frame-rate drop a player notices during a hunt.
- Anything written into the game folder (vanilla writes `SavedScreen*.png` there; the mod does not).
- Recording the voice of anyone who has not agreed to the mods' notice.
- The monitor showing different clips to different players at the same time.
- Anything playing before a player chooses it.

## How it plays

During the round nothing visible changes. While the ghost hunts or is visible, or a crucifix burns,
each modded client films it from a hidden camera mounted like a security camera in a top corner of
the room, cutting between corners and rooms as the ghost moves, and records the game's sound and
its own player's voice while they transmit.

Back in the lobby the truck looks as it always does until the round's moments are known; then one
cassette per moment lies in a row on the desk under the monitor, left to right in the order the
moments happened, and the monitor keeps showing the vanilla CCTV screensaver. A player who walks up
and uses a cassette (the game's interact input, within reach) slides it into the unit, and that
moment plays on the monitor for everyone, with sound from the monitor. The footage carries the
vanilla camcorder's own screen markings: the REC mark, the game's clock and a caption line. When the
moment ends the cassette comes back out and the screensaver returns. Using another cassette swaps
tapes; using the playing one ejects it. A player who arrives during a clip sees it already running.
Anyone can zoom onto the monitor the way the lobby's boards zoom in, so the people at the back see
the whole screen. The clips are saved in `Videos\Phasmophobia Replays`, named after their best
moment.

## Facts this rests on (REA and scene reads, 2026-10-21)

- **Capture** CCTV 0x27790F0: targetTexture swap, optional `TurnLocalNightVisionOn`,
  `Camera.Render`, `ReadPixels`, night vision off, target restored. Manual renders work in this URP
  build.
- **Night vision** 0x2776D70 / 0x2776CC0: Volume (CCTV +0x418) profile (+0x48) set to +0x428 or
  +0x420, IR Light (+0x3E8) enabled or disabled, one layer added to or removed from the culling mask.
- **Rig** every map pools `---SYSTEMS---/ObjectPooler/Video Camera I/II/III`; child `Render
  Texture Camcorder` (layer 17) holds a disabled Camera (mask 0x377e7fb7, FOV 50), URP camera data,
  the Volume, `ClipDistanceController`, the IR `Spot Light` and a `CameraCollider` child. The
  camcorder body carries `Canvas Camcorder ScreenUI` with `REC Icon`, `REC Text`, `Time` and
  `EvidenceText` (TextMeshPro); CCTV.Update fills Time from `CCTVController.GetTimerString`.
- **Lobby** (level2): parked truck `---------- Environment ----------/MediumTruck01_Anim/CCTV
  Unit.1`, about 14 m from the lobby's corkboards. `CCTV Screen.1/_TV CCTV`: a 1.14 x 0.66
  m screen tilted 17 degrees up toward the rear doors, a world-space canvas showing only
  `CCTVScreensaver`, an AudioSource on `CCTV Screen.1`, and a 1.58 x 1.91 x 3.65 m box in front of
  it (the same viewing box the contract truck's monitor has). The desk (`CCTV_Unit`) spans the 2.33
  m bay; twelve decor `Cassette` props (mesh and BoxCollider, no interaction) are piled on it.
- **Zoom**: in contracts the truck's `CCTV Screen` carries `CameraFocusUI`; the lobby's
  `CameraFocusController` focuses its 25 boards and has `RegisterCamera(CameraFocusUI)` and
  `SetCamera(CameraFocusUI)`; `CameraFocusUI` +0x30 is the camera's spot. The host already reads
  `CameraFocusUI.get_isFocused` for the consent notice.
- **Video playback**: vanilla plays MP4s with VideoPlayer on map TVs. Managed setters are stripped,
  but `UnityPlayer.dll` registers `set_source`, `set_url`, `set_renderMode`, `set_targetTexture`,
  `set_targetMaterialRenderer`, `set_targetMaterialProperty`, `set_audioOutputMode`,
  `SetTargetAudioSource`, `set_time`, `get_time`, `get_isPlaying`, `Prepare`, `Play`, `Pause`,
  `Stop`.
- **Readback**: icalls `AsyncGPUReadback::Request_Internal_Texture_1_Injected` and
  `AsyncGPUReadbackRequest::IsDone_/HasError_/GetDataRaw_Injected` are registered.
- **Game sound**: `AudioListener::GetOutputDataHelper`, `AudioSettings::GetSampleRate`,
  `GetSpeakerMode` and `get_dspTime` are registered: Unity hands out the last block of its final
  mix, on any Windows. Unity's `AudioRenderer` mutes the game while it records, and Windows' process
  loopback needs Windows 11 or Windows 10 build 20348, so neither is used.
- **Own voice**: `PlayerVoice` +0x80 is the voice mode (0 push-to-talk, 2 toggle); push-to-talk
  pressed (0xDD4F70) sets +0x38 and released (0xDD5150) clears it; toggle flips +0x39. Walkie talk
  goes through `OnGlobalPushToTalkPressed` (0xDD4DE0, flag to confirm). The local player's own voice
  is never played back to them, so it is not in the game's sound.
- **Consent**: the host offers `peer_has_mod(actor, id)` and `PG_MOD_RECORDS_VOICE`; today's notice
  says voice-recording mods write nothing to disk, which Replay would break.

## Design

**Recording, on every modded client.** The existing 2 Hz state sampling (moments.h `Frame`) runs on
every client, stamped with Photon server time (`PhotonNetwork.ServerTimestamp`), so every client's
footage and the host's moment list share one timeline.

**Triggers** (already synced by the game): the ghost hunting (GhostAI +0xF3), the ghost's model
visible (GhostModel renderers enabled), a crucifix burn (`CrucifixUseNetworked` hook, kept). A
capture starts on a trigger, ends 3 s after the last one, merges triggers less than 5 s apart and
stops at 120 s.

**Director.** A copy of the pooled `Render Texture Camcorder` (Instantiate with no parent; its
colliders and `CameraCollider` destroyed; the camera disabled so URP never renders it by itself;
target a 640x360 BGRA RenderTexture). Mounts are the top corners of each box of the subject's room
(`game::room_bounds`), 0.35 m in and 0.3 m down, rejected when a ray from the room's centre hits
geometry first. The subject is the ghost (the crucifix for a burn). The chosen mount sees the
subject's head and, when it can, the nearest living investigator, at 2.5 to 7 m. A mount is held at
least 1.5 s; the camera cuts when sight is lost for 0.7 s or the subject changes room; the aim
eases between subject and investigator. Without a usable mount: 2 m behind and 1.8 m up, pulled in
to the first wall.

**Captions.** A copy of the camcorder's `Canvas Camcorder ScreenUI` sits in front of the director
camera, enabled only around its render (after `Canvas.ForceUpdateCanvases`), so no player camera
ever sees it. It shows the REC mark, the game's clock (`GetTimerString`) and one line of what is
true at that moment: `Hunt` with the nearest investigator and distance (`Hunt: Sam 1.4 m`), `Ghost
event` the same way, `Sam was killed by the ghost` from that frame on, `A crucifix stopped a hunt`.
Whether a near miss was the top moment is only known after the round, so the line shows the
distance rather than a verdict; the moment's caption goes into the file's metadata and name.

**Filming a frame** (main thread, in the tick, every other frame for 30 fps at 60): night vision on
as vanilla does it, the local player's body renderers made visible to it (they may be shadow-only
for their owner; to verify), captions enabled, `Camera.Render`, all of it back, then a readback
request. Up to four in flight; finished frames go to a bounded queue and are dropped, never waited
for, if the encoder falls behind.

**Sound.** The game's sound comes from Unity itself: each tick the mod reads the newest block of the
final mix with `GetOutputDataHelper` (one managed float array per channel, held), finds where the
previous block's last 64 samples reappear in it and keeps only what follows, so the stream joins
without gaps or repeats; a hitch longer than the block becomes silence and `dspTime` keeps sound and
picture together. This is exactly what the player heard, voices included if the game plays them
through Unity (the voice taps suggest it does; to verify). If voices turn out to bypass Unity, the
fallback is a hook on the game process's own WASAPI output. The player's own voice comes from a
WASAPI shared-mode capture of the microphone the voice chat uses (Vivox's active input device
matched to a Windows endpoint by name, else the default communications microphone), mixed in only
while the player transmits: mode 0 while +0x38 is set, mode 2 while +0x39 is set, the walkie while
it is held, and any other mode always. Sound is recorded only when every player in the room runs
Replay (`peer_has_mod`), so everyone recorded saw the notice; otherwise the clip is silent.

**Encoding** (worker thread, no IL2CPP calls): Media Foundation Sink Writer, H.264 640x360 30 fps
about 2 Mbit/s with a keyframe every second, AAC 48 kHz stereo 128 kbit/s, hardware encoder when
offered. Each capture is a temporary MP4 in `%LOCALAPPDATA%\PhasmoMods\replays\tmp`. Without Media
Foundation (Windows N) the mod logs it and records nothing.

**The moment list.** At the round's end the host runs `find_moments` on its recording and sends one
message on channel 4, again whenever a player joins the lobby: the top moments (kind, the
investigator's actor, in, peak and out in server milliseconds) and who played the ghost. Each client
keeps the captures that cover those windows, writes the metadata, moves them to
`Videos\Phasmophobia Replays` and deletes the rest. If no list arrives within 60 s (a host without
Replay), the client uses its own `find_moments` and its tapes work only on its own monitor. The
folder keeps the newest 5 rounds; only files carrying the mod's metadata are ever deleted. Leftover
temporary files are deleted at the next start.

**File format.** A plain MP4 plus one top-level ISO BMFF `uuid` box appended after it is finalised
(players skip unknown boxes), holding UTF-8 JSON:

    {"format":"phasmo-replay","version":1,"build":"25811160","map":"Tanglewood_Street_House",
     "recorded":"2026-10-21T21:43:07Z","ghostPlayedBy":null,
     "moments":[{"kind":"kill","subject":"Sam","caption":"Sam was killed by the ghost",
                 "in":3.5,"peak":12.0,"out":16.0}]}

`in`, `peak` and `out` are seconds into the file. A file without the box plays whole.

**Lobby.** When the lobby has loaded and the moment list is known, the mod finds `CCTV Unit.1`
under the parked truck, lays up to five of the twelve desk cassettes in a row (the rest stay in the
pile), and adds a VideoPlayer (URL source) that renders into the `_TV CCTV` screen the way vanilla
shows CCTV feeds on that mesh (the property is read from vanilla `CCTVUI` while building), with its
sound sent to a 3D AudioSource on the monitor. Nothing is prepared or played until a cassette goes
in. A cassette is used by the game's interact input on its collider within reach; it slides toward
the unit while its moment plays and slides back when the moment ends or is ejected. The screensaver
canvas is hidden only while a clip plays. Leaving the lobby destroys the player and the AudioSource
and puts the screen and the cassettes back.

**Group sync.** The monitor's state is either nothing, or a moment index and the server time it
started. Using a cassette sends that pair (start 0.25 s ahead) to everyone and applies it locally;
using the playing cassette sends "nothing". The later start wins, then the lower actor number. Each
client plays the moment from `in` to `out` at the offset the pair gives, seeking when it drifts more
than 0.25 s, and returns to the screensaver at `out`. Clients without the host's list ignore pairs.
A client missing a moment's capture keeps its screensaver and its cassette moves anyway.

**Watching together.** Four players fit in the truck but tightly: one row across the 2.33 m bay at
1.5 to 2.5 m from the screen gives each about 0.6 m, and a second row watches between heads. This
is the same monitor and the same space as the contract truck, where the game lets each player zoom
onto the monitor. So the lobby monitor gets the same zoom: a `CameraFocusUI` on `CCTV Screen.1`,
registered with the lobby's `CameraFocusController`, with its spot 1.1 m in front of the screen,
square to it. A player zooms with whatever the lobby uses to focus a board and leaves the way they
leave a board; how a walking player starts a focus is read from a lobby board before building. If
that cannot be done without a new input, the zoom is dropped and the standing room above is what
there is. Cassettes are used while walking, not while zoomed.

**Consent.** Replay gets `PG_MOD_RECORDS_VOICE` and a new flag for mods that save recordings. The
notice gets a paragraph when Replay is installed: Replay films the round's top moments with the
game's sound and voice chat, including your own voice while you transmit, and saves them as video
files on this computer; sound is only recorded when everyone in the lobby agreed to this notice.
The existing voice paragraph keeps its promise for the mods it names. The notice's fingerprint
changes, so everyone agrees again once.

**Removed.** `view.cpp` and `view.h`, the replay blob (`Box`, `Replay`, `encode`, `decode`,
`chunks`, `Assembler`) and its tests. `find_moments`, `caption` and their tests stay.

## Build order

1. **Pure rules** (`mods/replay/clips.h`, tests first): capture windows from triggers, moment
   windows onto captures, the moment-list and monitor-state messages and which state wins, the
   playing offset and drift rule, the sample-block join, the JSON and `uuid` bytes, file names,
   retention, the transmit gate, the caption line.
2. **Recorder**: every client, server-time stamps, triggers, the host's list.
3. **Director** (`director.cpp`, mount rules in `director_rules.h`): rig copy, mounts, cuts,
   captions, filming, readback through `il2cpp::resolve_icall` (signatures checked by disassembly
   first, per the IL2CPP ABI gotcha).
4. **Sound** (`sound.cpp`): the mix reader and join, the microphone, the gate, the mix, timestamps.
5. **Encoder** (`encoder.cpp`): Sink Writer thread, queues, finalise, `uuid`, move, cleanup. Links
   `mfplat`, `mfreadwrite`, `mfuuid`, `ole32` (Windows SDK).
6. **Lobby** (`screen.cpp`, `tapes.cpp`): monitor, VideoPlayer, AudioSource, cassettes, use,
   insert and eject, sync, the zoom, restore.
7. **Consent and docs**: the flag and paragraph (`host/consent_text.h`, consent tests), `mod.ini`,
   `docs/design.md` Replay section, FINDINGS 16 rewritten as the live checks below.

## Tests and checks

Offline: everything in step 1, the mount and cut rules on fake rooms and rays, the consent text with
and without Replay, a missing or broken `uuid` box, retention never picking a file without the
metadata, two clients reaching the same playing moment and offset from the same pair and list, the
sample join across overlapping, gapped and silent blocks. `held_scan_test` covers the new held
pointers.

Live (FINDINGS 16, private room, ban risk raised first): frame time with a capture running in a
hunt; readable footage in a dark room; the local player visible in their own clip; the ghost
animating far from the local player; the captions never visible to a player; game sound, friends'
voices and your own voice (push-to-talk, toggle, walkie) in the clip and in sync, on Windows 10 and
11; nothing on the monitor until a cassette goes in; four players in the truck able to see the
screen, with and without the zoom; two players' monitors on the same moment within a quarter second
after a cassette; eject and swap; the screensaver back after leaving; nothing in the game folder.

## Claude's calls (change any of them)

640x360 at 30 fps; night vision always on; corner mounts with the shoulder fallback; 120 s cap per
capture; top 5 moments, one cassette each; a cassette plays its moment once and then comes back
out; 5 rounds kept in `Videos\Phasmophobia Replays`; sound only when everyone runs Replay; captions
show live facts (distance, kill, crucifix) rather than a verdict; the newest monitor state wins; the
zoom spot 1.1 m in front of the screen.
