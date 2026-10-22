#include "echo.h"

#include <windows.h>

#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <vector>

#include "echo_rule.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "speed_rule.h"
#include "voice.h"

namespace pg::custom::echo {
namespace {

using il2cpp::bind;
using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kVoicePlayer = 0x20;          // PlayerVoice: its Player
constexpr size_t kVoiceLocalTap = 0x70;        // PlayerVoice: the tap carrying the player's positional voice
constexpr size_t kNetworkSpots = 0x38;         // Network: List<PlayerSpot>
constexpr size_t kSpotPhotonPlayer = 0x18;     // PlayerSpot: Photon.Realtime.Player
constexpr size_t kSpotPlayer = 0x38;           // PlayerSpot: networked Player
constexpr size_t kPlayerDead = 0x28;           // Player: dead
constexpr size_t kLevelGhostRoom = 0x38;       // LevelController: the room the ghost is in
constexpr uintptr_t kTapReadRva = 0x4F5DF70;   // VivoxParticipantTap.DoAudioFilterRead (main thread, Vivox's coroutine)
constexpr uintptr_t kTalkingRva = 0x8A9AC0;    // LevelRoom.ArePlayersTalkingInRoom(float)
constexpr ULONGLONG kMapMs = 2000;             // how often taps are matched to players
constexpr ULONGLONG kQuietMs = 60000;          // nobody heard near it this long: the Echo is dormant
constexpr float kHearing = 10.0f;              // metres within which the Echo hears a voice
constexpr ULONGLONG kMinGapMs = 45000, kMaxGapMs = 90000;  // between the AI's words
constexpr int kChunksPerFrame = 10;
constexpr float kPitch = 0.96f;                // a shade low: almost, but not quite, their voice

// The repeat (echo_rule.h). Ghidra decompiles of build 25811160, 2026-10-22.
constexpr size_t kGhostInfo = 0x38;            // GhostAI: GhostInfo
constexpr size_t kInfoFavouriteRoom = 0x70;    // GhostInfo: favourite LevelRoom
constexpr size_t kSwitchRoom = 0xD0;           // LightSwitch: its LevelRoom
constexpr size_t kRoomDoors = 0x28;            // LevelRoom: Door[] (the Yurei's ability picks from it)
constexpr size_t kDoorInteract = 0x98;         // Door: its PhotonObjectInteract
// LightSwitch.Use(bool on, bool prints, bool byGhost, bool counted): the RPC LightSwitch.UseLightSwitch sends
// to every client for each use, a player's (byGhost false) or the ghost's, so the host sees every one.
constexpr uintptr_t kSwitchUseRva = 0x26E5940;
// Door.NetworkedGrabbedDoor(): the RPC Door.GrabbedDoor sends to every client when a player grabs an unlocked
// door (its only callers are DragRigidbodyUse, the player's grab; ClothDoor's override calls it first).
constexpr uintptr_t kDoorGrabbedRva = 0x264D150;
// The ghost's own interactions with a given object, as the sensor's light-switch pick (0x2014DA0) and the
// idle door chance (0x2007C60) start them: GhostAI.ChangeState(light, the switch's PhotonObjectInteract,
// forced) and ChangeState(door, the door's PhotonObjectInteract). The light state (Enter 0x21899B0) switches
// a lit switch off, and an unlit one on when forced or 1 in 2, through UseLightSwitch(true, true, false):
// the game's prints and EMF. The door state (Enter 0x20D1920) moves an unlocked door through
// Door.GhostOpenDoorEvent or GhostOpenDoorAnimated, its own pick. Both return the AI to idle.
constexpr int kStateLight = 4, kStateDoor = 5;  // GhostAI.State

// Messages on this mod's channel: 'C' 'G' version type, then:
//   SPEAK  speaker:i32 source:i32 clip:u16 x:f32 y:f32 z:f32    (host to all: who speaks, who sends, where)
//   CHUNK  clip:u16 index:u8 count:u8 mu-law bytes             (the source to all)
//   HEARD  n:u8 actors:i32[n]                                    (each client to the host: whose last phrase it holds)
constexpr uint8_t kVersion = 2;
enum : uint8_t { kSpeak = 1, kChunk = 2, kHeard = 3 };
constexpr ULONGLONG kHeardMs = 2000;           // how often each client reports whose phrases it holds
constexpr ULONGLONG kHeardFreshMs = 5000;      // a report older than this no longer counts
constexpr ULONGLONG kTapLiveMs = 500;          // a tap silent this long has stopped (death, leaving)

struct Api {
  il2cpp::Object* voiceType = nullptr;
  il2cpp::Object* audioSourceType = nullptr;
  il2cpp::Class* single = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  void* (*createClip)(il2cpp::String*, int, int, int, bool, const void*) = nullptr;
  bool (*setData)(void*, il2cpp::Array*, int, const void*) = nullptr;
  void (*setClip)(void*, void*, const void*) = nullptr;
  void (*setSpatialBlend)(void*, float, const void*) = nullptr;
  void (*setMinDistance)(void*, float, const void*) = nullptr;
  void (*setMaxDistance)(void*, float, const void*) = nullptr;
  void (*setPitch)(void*, float, const void*) = nullptr;
  void (*play)(void*, const void*) = nullptr;
  void (*destroyAfter)(void*, float, const void*) = nullptr;
  bool (*talking)(void*, float, const void*) = nullptr;
  il2cpp::Object* interactType = nullptr;  // PhotonObjectInteract, the object a ghost's interaction is given
  bool ok = false;
} api;

using TapRead_t = int (*)(void*, int, il2cpp::Array*, int, int, int, const void*);
using SwitchUse_t = void (*)(void*, bool, bool, bool, bool, const void*);
using DoorGrabbed_t = void (*)(void*, const void*);
using ChangeState_t = void (*)(void*, int, void*, bool, const void*);
TapRead_t g_tapRead = nullptr;
SwitchUse_t g_switchUse = nullptr;
DoorGrabbed_t g_doorGrabbed = nullptr;

struct Pending {
  int speaker = 0, source = 0;
  uint16_t clip = 0;
  Vector3 at{};
};

struct State {
  bool recording = false;
  std::map<void*, int> tapActor;                // tap -> the actor whose voice it carries
  std::map<int, voice::Recorder> recorders;     // actor -> what they said
  std::map<int, ULONGLONG> lastAudio;           // actor -> when their tap last delivered audio
  Holders holders;                              // host: who holds whose phrase (speed_rule.h)
  ULONGLONG nextHeard = 0;
  ULONGLONG nextMap = 0;
  il2cpp::Held ghost;                           // the Echo (host)
  ULONGLONG lastHeard = 0, nextWords = 0;
  Pending pending;                              // the last SPEAK, waiting for its audio
  voice::Assembler assembler;
  std::vector<std::vector<uint8_t>> outgoing;   // chunks still to send
  uint16_t outgoingClip = 0;
  size_t outgoingNext = 0;
  uint16_t nextClip = 1;
  Repeat<il2cpp::Held> repeat;                  // host: the switch or door its AI repeats next
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

int actor_of_player(void* player) {
  void* net = game::network();
  auto* spots = net ? il2cpp::field<il2cpp::List*>(net, kNetworkSpots) : nullptr;
  for (int i = 0; spots && spots->items && i < spots->size; ++i) {
    void* spot = spots->items->data<void*>()[i];
    if (spot && il2cpp::field<void*>(spot, kSpotPlayer) == player)
      return game::actor_number(il2cpp::field<void*>(spot, kSpotPhotonPlayer));
  }
  return 0;
}

/** Match each voice tap to its player's actor; only players whose client runs this mod are kept. */
void map_taps() {
  g.tapActor.clear();
  il2cpp::Array* voices = api.findObjects(api.voiceType, nullptr);
  for (uintptr_t i = 0; voices && i < voices->max_length; ++i) {
    void* v = voices->data<void*>()[i];
    void* tap = v ? il2cpp::field<void*>(v, kVoiceLocalTap) : nullptr;
    const int actor = tap ? actor_of_player(il2cpp::field<void*>(v, kVoicePlayer)) : 0;
    if (actor && actor != game::local_actor() && mod::peer_has(actor, mod::info().id)) g.tapActor[tap] = actor;
  }
}

int detour_tap_read(void* tap, int tapId, il2cpp::Array* data, int frames, int channels, int rate, const void* method) {
  hooks::Guard guard;
  const int result = g_tapRead(tap, tapId, data, frames, channels, rate, method);
  if (g.recording && data && result >= 0) {
    auto it = g.tapActor.find(tap);
    if (it != g.tapActor.end() && data->max_length >= static_cast<uintptr_t>(frames) * channels) {
      g.recorders[it->second].push(data->data<float>(), frames, channels, rate);
      g.lastAudio[it->second] = GetTickCount64();
    }
  }
  return result;
}

bool dormant() { return g.ghost && GetTickCount64() - g.lastHeard > kQuietMs; }

void put(std::vector<uint8_t>& b, const void* p, size_t n) {
  const auto* c = static_cast<const uint8_t*>(p);
  b.insert(b.end(), c, c + n);
}

/** Play a clip from `at`, through a 3D audio source made for it and gone once it has played. */
void play(const std::vector<float>& clip, Vector3 at) {
  if (clip.empty() || !api.ok) return;
  il2cpp::Array* samples = il2cpp::array_new(api.single, clip.size());
  void* audio = samples ? api.createClip(il2cpp::string_new("Echo"), static_cast<int>(clip.size()), 1, voice::kRate, false, nullptr) : nullptr;
  void* go = audio ? game::new_game_object("Echo voice") : nullptr;
  void* source = go ? game::add_component(go, api.audioSourceType) : nullptr;
  if (!source) return;
  std::memcpy(samples->data<float>(), clip.data(), clip.size() * sizeof(float));
  api.setData(audio, samples, 0, nullptr);
  game::set_position(game::object_transform(go), at);
  api.setClip(source, audio, nullptr);
  api.setSpatialBlend(source, 1.0f, nullptr);
  api.setMinDistance(source, 1.0f, nullptr);
  api.setMaxDistance(source, 15.0f, nullptr);
  api.setPitch(source, kPitch, nullptr);
  api.play(source, nullptr);
  api.destroyAfter(go, static_cast<float>(clip.size()) / voice::kRate / kPitch + 1.0f, nullptr);
  PG_LOG("echo: a voice from the ghost (%.1f s)", static_cast<float>(clip.size()) / voice::kRate);
}

/** This client sends the speaker's last phrase, if it heard one, and plays it itself. */
void send_phrase(const Pending& p) {
  auto it = g.recorders.find(p.speaker);
  std::vector<float> clip;
  if (it == g.recorders.end() || !it->second.last_phrase(clip)) {
    PG_LOG("echo: nothing recent from that investigator to repeat");
    return;
  }
  g.outgoing = voice::chunks(clip);
  g.outgoingClip = p.clip;
  g.outgoingNext = 0;
  play(clip, p.at);
}

void flush_outgoing() {
  for (int n = 0; n < kChunksPerFrame && g.outgoingNext < g.outgoing.size(); ++n, ++g.outgoingNext) {
    const auto& c = g.outgoing[g.outgoingNext];
    std::vector<uint8_t> b = {'C', 'G', kVersion, kChunk};
    put(b, &g.outgoingClip, 2);
    b.push_back(static_cast<uint8_t>(g.outgoingNext));
    b.push_back(static_cast<uint8_t>(g.outgoing.size()));
    b.insert(b.end(), c.begin(), c.end());
    net::send(b.data(), b.size(), net::To::Others, true);
  }
}

/** The audio from an actor's tap is still arriving: a tap that stopped says nothing about now. */
bool tap_live(int actor, ULONGLONG now) {
  auto it = g.lastAudio.find(actor);
  return it != g.lastAudio.end() && now - it->second < kTapLiveMs;
}

/** Whose last phrase this client holds and could send now (heard within the last minute). */
std::vector<int> held_phrases(ULONGLONG now) {
  std::vector<int> out;
  std::vector<float> clip;
  for (auto& [actor, recorder] : g.recorders) {
    auto heard = g.lastAudio.find(actor);
    if (heard != g.lastAudio.end() && now - heard->second < kQuietMs && recorder.last_phrase(clip)) out.push_back(actor);
  }
  return out;
}

/** Each client tells the host whose phrases it holds; the host notes its own directly. */
void report_heard(ULONGLONG now) {
  if (now < g.nextHeard) return;
  g.nextHeard = now + kHeardMs;
  const std::vector<int> held = held_phrases(now);
  if (game::is_master()) {
    for (int a : held) g.holders.report(game::local_actor(), a, now);
    return;
  }
  std::vector<uint8_t> b = {'C', 'G', kVersion, kHeard, static_cast<uint8_t>(std::min<size_t>(held.size(), 32))};
  for (size_t i = 0; i < held.size() && i < 32; ++i) put(b, &held[i], 4);
  net::send(b.data(), b.size(), net::To::Master, false);
}

/** A client that reported, lately, holding the actor's last phrase (the host itself first), or 0. */
int holder_of(int actor, ULONGLONG now) {
  return g.holders.holder_of(actor, game::local_actor(), now, kHeardFreshMs,
                             [](int client) { return mod::peer_has(client, mod::info().id); });
}

/** Whom the Echo could speak as: living investigators who agreed and whose last phrase a client holds. */
std::vector<int> candidates(ULONGLONG now) {
  std::vector<int> out;
  std::vector<int> actors;
  game::room_actors(actors);
  for (int a : actors) {
    void* player = game::player_of_actor(a);
    if (!player || !game::alive(player) || il2cpp::field<bool>(player, kPlayerDead) || !mod::peer_has(a, mod::info().id)) continue;
    if (holder_of(a, now)) out.push_back(a);
  }
  return out;
}

void listen(ULONGLONG now) {
  const Vector3 ghostAt = game::position(game::transform_of(g.ghost));
  for (auto& [actor, recorder] : g.recorders) {
    void* player = game::player_of_actor(actor);
    if (!player || !tap_live(actor, now) || !recorder.spoke_within(50)) continue;  // spoke in the last second
    const Vector3 p = game::position(game::transform_of(player));
    const float dx = p.x - ghostAt.x, dy = p.y - ghostAt.y, dz = p.z - ghostAt.z;
    if (dx * dx + dy * dy + dz * dz <= kHearing * kHearing) g.lastHeard = now;
  }
  void* level = game::level_controller();
  void* room = level && game::alive(level) ? il2cpp::field<void*>(level, kLevelGhostRoom) : nullptr;
  if (room && game::alive(room) && api.talking(room, 0.1f, nullptr)) g.lastHeard = now;  // the host's own voice too
}

/** The Echo's favourite room, on the host while it haunts the contract; null otherwise. */
void* favourite_room() {
  if (!g.recording || !g.ghost || !game::alive(g.ghost) || !game::is_master()) return nullptr;
  void* info = il2cpp::field<void*>(g.ghost, kGhostInfo);
  void* room = info ? il2cpp::field<void*>(info, kInfoFavouriteRoom) : nullptr;
  return room && game::alive(room) ? room : nullptr;
}

/** Whether `door` is one of the room's doors. */
bool door_of(void* room, void* door) {
  auto* doors = il2cpp::field<il2cpp::Array*>(room, kRoomDoors);
  for (uintptr_t i = 0; doors && i < doors->max_length; ++i)
    if (doors->data<void*>()[i] == door) return true;
  return false;
}

/** Note a player's use of a light switch or door for the repeat; it counts when the object is in its room. */
void player_used(void* object, bool inItsRoom, const char* what) {
  g.repeat.used(object, inItsRoom, GetTickCount64());
  if (inItsRoom) PG_LOG("echo: a player used a %s in its room", what);
}

/** Every client runs each use of a switch; the host notes a player's (not the ghost's) for the repeat. */
void detour_switch_use(void* lightSwitch, bool on, bool prints, bool byGhost, bool counted, const void* method) {
  hooks::Guard guard;
  g_switchUse(lightSwitch, on, prints, byGhost, counted, method);
  if (void* room = !byGhost && lightSwitch ? favourite_room() : nullptr)
    player_used(lightSwitch, il2cpp::field<void*>(lightSwitch, kSwitchRoom) == room, "light switch");
}

/** Every client runs each player's grab of a door; the host notes it for the repeat. */
void detour_door_grabbed(void* door, const void* method) {
  hooks::Guard guard;
  g_doorGrabbed(door, method);
  if (void* room = door ? favourite_room() : nullptr) player_used(door, door_of(room, door), "door");
}

}  // namespace

bool resolve() {
  constexpr const char* kAudio = "UnityEngine.AudioModule.dll";
  il2cpp::Class* clip = il2cpp::cls(kAudio, "UnityEngine", "AudioClip");
  il2cpp::Class* source = il2cpp::cls(kAudio, "UnityEngine", "AudioSource");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  api.voiceType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PlayerVoice"));
  api.audioSourceType = source ? il2cpp::type_object(source) : nullptr;
  api.single = il2cpp::cls("mscorlib.dll", "System", "Single");
  api.interactType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PhotonObjectInteract"));
  bool ok = api.voiceType && api.audioSourceType && api.single && api.interactType;
  ok &= bind(api.findObjects, object, "FindObjectsOfType", 1);
  ok &= bind(api.createClip, clip, "Create", 5);
  ok &= bind(api.setData, clip, "SetData", 2);
  ok &= bind(api.setClip, source, "set_clip", 1);
  ok &= bind(api.setSpatialBlend, source, "set_spatialBlend", 1);
  ok &= bind(api.setMinDistance, source, "set_minDistance", 1);
  ok &= bind(api.setMaxDistance, source, "set_maxDistance", 1);
  ok &= bind(api.setPitch, source, "set_pitch", 1);
  ok &= bind(api.play, source, "Play", 0);
  ok &= bind(api.destroyAfter, object, "Destroy", 2);
  api.talking = reinterpret_cast<decltype(api.talking)>(game::game_assembly_address(kTalkingRva));
  api.ok = ok && api.talking;
  PG_LOG("echo: resolve %s", api.ok ? "ok" : "FAILED");
  return api.ok;
}

bool create_hooks() {
  return hooks::create(game::game_assembly_address(kTapReadRva), reinterpret_cast<void*>(&detour_tap_read),
                       reinterpret_cast<void**>(&g_tapRead), "VivoxParticipantTap.DoAudioFilterRead") &&
         hooks::create(game::game_assembly_address(kSwitchUseRva), reinterpret_cast<void*>(&detour_switch_use),
                       reinterpret_cast<void**>(&g_switchUse), "LightSwitch.Use") &&
         hooks::create(game::game_assembly_address(kDoorGrabbedRva), reinterpret_cast<void*>(&detour_door_grabbed),
                       reinterpret_cast<void**>(&g_doorGrabbed), "Door.NetworkedGrabbedDoor");
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (size < 4 || data[0] != 'C' || data[1] != 'G' || data[2] != kVersion || !g.recording) return;
  if (data[3] == kSpeak && size >= 4 + 4 + 4 + 2 + 12 && sender == game::master_actor()) {
    Pending p;
    std::memcpy(&p.speaker, data + 4, 4);
    std::memcpy(&p.source, data + 8, 4);
    std::memcpy(&p.clip, data + 12, 2);
    std::memcpy(&p.at, data + 14, 12);
    g.pending = p;
    if (p.source == game::local_actor()) send_phrase(p);
  } else if (data[3] == kHeard && size >= 5 && game::is_master() && size >= 5 + 4u * data[4]) {
    const ULONGLONG now = GetTickCount64();
    for (int i = 0; i < data[4]; ++i) {
      int actor = 0;
      std::memcpy(&actor, data + 5 + 4 * i, 4);
      g.holders.report(sender, actor, now);
    }
  } else if (data[3] == kChunk && size > 8 && sender == g.pending.source) {
    uint16_t clip = 0;
    std::memcpy(&clip, data + 4, 2);
    std::vector<float> samples;
    if (clip == g.pending.clip && g.assembler.add(clip, data[6], data[7], data + 8, size - 8, samples)) play(samples, g.pending.at);
  }
}

bool speak(void* ghost, int actor) {
  if (!ghost || !api.ok || !game::is_master()) return false;
  const ULONGLONG now = GetTickCount64();
  const std::vector<int> who = candidates(now);
  if (who.empty()) return false;  // nobody's words are held anywhere: the Echo has nothing to say
  if (!actor) actor = who[std::uniform_int_distribution<size_t>(0, who.size() - 1)(g_random)];
  // Sent by a client that holds the phrase: the host when it heard it, else one that reported it.
  const int source = holder_of(actor, now);
  if (!source) return false;
  Pending p{actor, source, g.nextClip++, game::position(game::transform_of(ghost))};
  p.at.y += game::kGhostEyeHeight;
  std::vector<uint8_t> b = {'C', 'G', kVersion, kSpeak};
  put(b, &p.speaker, 4);
  put(b, &p.source, 4);
  put(b, &p.clip, 2);
  put(b, &p.at, 12);
  if (!net::send(b.data(), b.size(), net::To::Others, true)) return false;
  g.pending = p;
  if (source == game::local_actor()) send_phrase(p);
  return true;
}

void tick(void* ghost, bool steered, bool recording) {
  if (recording != g.recording) {
    if (!recording) {
      g.recorders.clear();  // nothing is kept beyond the contract
      g.tapActor.clear();
      g.lastAudio.clear();
      g.holders.clear();
      g.repeat = {};
    }
    g.recording = recording;
  }
  if (!recording) return;
  const ULONGLONG now = GetTickCount64();
  if (now >= g.nextMap) {
    g.nextMap = now + kMapMs;
    map_taps();
  }
  flush_outgoing();
  report_heard(now);
  if (ghost != g.ghost) {
    g.ghost = ghost;
    g.lastHeard = now;  // a new Echo starts awake, with nothing to repeat
    g.nextWords = now + kMinGapMs;
    g.repeat = {};
  }
  if (!g.ghost) return;
  listen(now);
  if (steered || dormant() || game::ghost_state(g.ghost) == game::kStateHunting || now < g.nextWords) return;
  g.nextWords = now + kMinGapMs + std::uniform_int_distribution<ULONGLONG>(0, kMaxGapMs - kMinGapMs)(g_random);
  speak(g.ghost, 0);
}

void reset() {
  g = State{};
}

bool dormant_now() { return dormant(); }

bool repeat_pending() { return g.ghost && g.repeat.pending(GetTickCount64()); }

bool repeat(void* ghost) {
  if (!api.ok || !ghost || !game::is_master() || game::ghost_state(ghost) == game::kStateHunting) return false;
  void* object = g.repeat.take(GetTickCount64());
  if (!object || !game::alive(object)) return false;
  const bool door = il2cpp::is_kind(object, "Door");
  void* interact = door ? il2cpp::field<void*>(object, kDoorInteract) : game::get_component(object, api.interactType);
  if (!interact || !game::alive(interact)) return false;
  // Forced, as the voodoo doll's interaction is (NormalInteraction(true)): an unlit switch is used every time.
  reinterpret_cast<ChangeState_t>(game::change_state())(ghost, door ? kStateDoor : kStateLight, interact, !door, nullptr);
  PG_LOG("echo: repeats a player's %s", door ? "door" : "light switch");
  return true;
}

}  // namespace pg::custom::echo
