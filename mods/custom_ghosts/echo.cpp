#include "echo.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <random>
#include <vector>

#include "Door.h"
#include "GhostAI.h"
#include "GhostInfo.h"
#include "LevelController.h"
#include "LevelRoom.h"
#include "LightSwitch.h"
#include "Photon.h"
#include "Player.h"
#include "PlayerVoice.h"
#include "Props.h"
#include "Sensor.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "actors.h"
#include "echo_rule.h"
#include "game.h"
#include "game_api.h"
#include "held.h"
#include "managed_exception.h"  // Il2CppExceptionWrapper: the C++ type a managed exception is thrown as
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "runtime.h"
#include "speed_rule.h"
#include "voice.h"

namespace pm::custom::echo {
namespace {

using phasmo::Vector3;

constexpr ULONGLONG kMapMs = 2000;             // how often taps are matched to players
constexpr ULONGLONG kQuietMs = 60000;          // nobody heard near it this long: the Echo is dormant
constexpr float kHearing = 10.0f;              // metres within which the Echo hears a voice
constexpr ULONGLONG kMinGapMs = 45000, kMaxGapMs = 90000;  // between the AI's words
constexpr int kChunksPerFrame = 10;
constexpr float kPitch = 0.96f;                // a shade low: almost, but not quite, their voice

// The repeat (echo_rule.h). Ghidra decompiles of build 25811160, 2026-10-22; the map entries name each
// call (sdk/map/GhostAI.h, Sensor.h, LightSwitch.h, Door.h).
// The sensor's pulse, as TryInteract runs it: RangeSensor.SetBoxShape(GhostActivity.sensorBox), then the
// sensor's pulse, whose code (BasePulsableSensor.Pulse) completes any pending pulse and runs the sensor's
// test. The test rebuilds the detections, then invokes the sensor's OnPulsed delegate at once, to which
// GhostActivity.Start adds the ghost's whole interaction and nothing else.
// LightSwitch.Use is the RPC UseLightSwitch sends to every client for each use, so the host sees every one;
// a player's use clears every flag, the ghost's set byGhost or counted. Door.NetworkedGrabbedDoor is the RPC
// Door.GrabbedDoor sends to every client when a player grabs an unlocked door.
// The ghost's own interactions with a given object, as the sensor's light-switch pick and the
// idle door chance (GhostActivity.doorChance) start them: GhostAI.ChangeState(light, the switch's
// PhotonObjectInteract, forced) and ChangeState(door, the door's PhotonObjectInteract). The light state
// switches a lit switch off, and an unlit one on when forced or 1 in 2, through
// UseLightSwitch(true, true, false): the game's prints and EMF. The door state moves an
// unlocked door through Door.GhostOpenDoorEvent or GhostOpenDoorAnimated, its own pick. Both return the AI
// to idle. Door.GhostForceDoor is not the call: it starts the door's force coroutine directly, as the
// Yurei's ability does, while the door state is the ghost's ordinary door interaction and picks the move as
// the game's own do. Neither state sends the grab RPC (the door's coroutines send ForceDropDoorNetworked and
// PlayDoorSlamNoise), so a repeat is never noted as a player's use.

// Messages on this mod's channel: 'C' 'G' version type, then:
//   SPEAK  speaker:i32 source:i32 clip:u16 x:f32 y:f32 z:f32    (host to all: who speaks, who sends, where)
//   CHUNK  clip:u16 index:u8 count:u8 mu-law bytes             (the source to all)
//   HEARD  n:u8 actors:i32[n]                                    (each client to the host: whose last phrase it holds)
constexpr uint8_t kVersion = 2;
enum : uint8_t { kSpeak = 1, kChunk = 2, kHeard = 3 };
constexpr ULONGLONG kHeardMs = 2000;           // how often each client reports whose phrases it holds
constexpr ULONGLONG kHeardFreshMs = 5000;      // a report older than this no longer counts
constexpr ULONGLONG kTapLiveMs = 500;          // a tap silent this long has stopped (death, leaving)

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
  api::Held ghost;                              // the Echo (host)
  ULONGLONG lastHeard = 0, nextWords = 0;
  Pending pending;                              // the last SPEAK, waiting for its audio
  voice::Assembler assembler;
  std::vector<std::vector<uint8_t>> outgoing;   // chunks still to send
  uint16_t outgoingClip = 0;
  size_t outgoingNext = 0;
  uint16_t nextClip = 1;
  Repeat<api::Held> repeat;                     // host: the switch or door its AI repeats next
} g;

std::mt19937 g_random{static_cast<unsigned>(GetTickCount64())};

/**
 * Match each other player's voice tap to their actor. A contract only begins once every player's mods are confirmed
 * (sdk/README.md, "The lobby's rules"), so every other player's client runs this mod and its player agreed to the notice.
 */
void map_taps() {
  g.tapActor.clear();
  const api::RefArray voices{phasmo::Object::FindObjectsOfType(phasmo::PlayerVoice::type.type_object())};
  for (size_t i = 0; i < voices.size(); ++i) {
    void* v = voices[i];
    void* tap = v ? phasmo::PlayerVoice::globalTap.get(v) : nullptr;
    const int actor = tap ? actor_of_player(phasmo::PlayerVoice::player.get(v)) : 0;
    if (actor && actor != game::local_actor()) g.tapActor[tap] = actor;
  }
}

int detour_tap_read(api::Original<phasmo::VivoxParticipantTap::DoAudioFilterRead> original, void* tap, int32_t tapId,
                    void* data, int32_t frames, int32_t channels, int32_t rate) {
  const int result = original(tap, tapId, data, frames, channels, rate);
  if (g.recording && data && result >= 0 && frames > 0 && channels > 0) {
    auto it = g.tapActor.find(tap);
    const api::Array<float> samples{data};
    if (it != g.tapActor.end() && samples.size() >= static_cast<size_t>(frames) * channels) {
      g.recorders[it->second].push(&samples[0], frames, channels, rate);
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
  if (clip.empty()) return;
  void* samples = phasmo::Single::type.make_array(clip.size());
  void* audio = samples ? phasmo::AudioClip::Create(api::make_string("Echo"), static_cast<int>(clip.size()), 1,
                                                    voice::kRate, false)
                        : nullptr;
  void* go = audio ? phasmo::GameObject::type.make() : nullptr;
  if (go) phasmo::GameObject::ctor(go, api::make_string("Echo voice"));
  void* source = go ? phasmo::GameObject::AddComponent(go, phasmo::AudioSource::type.type_object()) : nullptr;
  const api::Array<float> data{samples};
  if (!source || data.size() < clip.size()) {  // nothing to play: the GameObject made for it goes at once
    if (go) phasmo::Object::Destroy(go, 0.0f);
    return;
  }
  std::memcpy(&data[0], clip.data(), clip.size() * sizeof(float));
  phasmo::AudioClip::SetData(audio, samples, 0);
  const phasmo::Vector3 where{at.x, at.y, at.z};
  phasmo::Transform::set_position(phasmo::GameObject::get_transform(go), &where);
  phasmo::AudioSource::set_clip(source, audio);
  phasmo::AudioSource::set_spatialBlend(source, 1.0f);
  phasmo::AudioSource::set_minDistance(source, 1.0f);
  phasmo::AudioSource::set_maxDistance(source, 15.0f);
  phasmo::AudioSource::set_pitch(source, kPitch);
  phasmo::AudioSource::Play(source, 0);
  phasmo::Object::Destroy(go, static_cast<float>(clip.size()) / voice::kRate / kPitch + 1.0f);
  PM_LOG("echo: a voice from the ghost (%.1f s)", static_cast<float>(clip.size()) / voice::kRate);
}

/** This client sends the speaker's last phrase, if it heard one, and plays it itself. */
void send_phrase(const Pending& p) {
  auto it = g.recorders.find(p.speaker);
  std::vector<float> clip;
  if (it == g.recorders.end() || !it->second.last_phrase(clip)) {
    PM_LOG("echo: nothing recent from that investigator to repeat");
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

/** A client still in the room that reported, lately, holding the actor's last phrase (the host itself first), or 0. */
int holder_of(int actor, const std::vector<int>& room, ULONGLONG now) {
  return g.holders.holder_of(actor, game::local_actor(), now, kHeardFreshMs, [&](int client) {
    return std::find(room.begin(), room.end(), client) != room.end();
  });
}

/** Whom the Echo could speak as: living investigators whose last phrase a client holds. */
std::vector<int> candidates(const std::vector<int>& room, ULONGLONG now) {
  std::vector<int> out;
  for (int a : room) {
    void* player = game::player_of_actor(a);
    if (!api::alive(player) || phasmo::Player::isDead.get(player)) continue;
    if (holder_of(a, room, now)) out.push_back(a);
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
  void* room = api::alive(level) ? phasmo::LevelController::currentGhostRoom.get(level) : nullptr;
  if (api::alive(room) && phasmo::LevelRoom::ArePlayersTalkingInRoom(room, 0.1f)) g.lastHeard = now;  // the host's own voice too
}

/** The Echo's favourite room, on the host while it haunts the contract; null otherwise. */
void* favourite_room() {
  if (!g.recording || !g.ghost || !game::alive(g.ghost) || !game::is_master()) return nullptr;
  void* info = phasmo::GhostAI::info.get(g.ghost);
  void* room = info ? phasmo::GhostInfo::favouriteRoom.get(info) : nullptr;
  return api::alive(room) ? room : nullptr;
}

/** Whether `door` is one of the room's doors. */
bool door_of(void* room, void* door) {
  const api::RefArray doors{phasmo::LevelRoom::doors.get(room)};
  for (size_t i = 0; i < doors.size(); ++i)
    if (doors[i] == door) return true;
  return false;
}

/** Note a player's use of a light switch or door for the repeat; it counts when the object is in its room. */
void player_used(void* object, bool inItsRoom, const char* what) {
  g.repeat.used(object, inItsRoom, GetTickCount64());
  if (inItsRoom) PM_LOG("echo: a player used a %s in its room", what);
}

/** Every client runs each use of a switch; the host notes a player's (neither flag set) for the repeat. */
void detour_switch_use(api::Original<phasmo::LightSwitch::Use> original, void* lightSwitch, bool on, bool prints,
                       bool byGhost, bool counted) {
  original(lightSwitch, on, prints, byGhost, counted);
  if (void* room = !byGhost && !counted && lightSwitch ? favourite_room() : nullptr)
    player_used(lightSwitch, phasmo::LightSwitch::room.get(lightSwitch) == room, "light switch");
}

/**
 * Pulse the ghost's interaction sensor where the ghost stands now, as TryInteract does, and nothing more: its
 * OnPulsed delegate, which would run the ghost's whole interaction, is set aside for the pulse and put back
 * whatever happens. False when the sensor could not be pulsed.
 */
bool pulse_sensor(void* activity, void* sensor) {
  if (!phasmo::RangeSensor::type.is(sensor)) return false;  // the box and the delegate are a RangeSensor's
  const phasmo::Vector3 box = phasmo::GhostActivity::sensorBox.get(activity);
  phasmo::RangeSensor::SetBoxShape(sensor, &box);
  api::Held onPulsed(phasmo::RangeSensor::onPulsed.get(sensor));  // kept alive while set aside
  phasmo::RangeSensor::onPulsed.set(sensor, nullptr);
  bool pulsed = true;
  try {
    phasmo::BasePulsableSensor::Pulse(sensor);
  } catch (const Il2CppExceptionWrapper& e) {
    mod::host().log_exception("echo: pulsing the interaction sensor threw", e.ex);
    pulsed = false;
  } catch (...) {
    PM_LOG("echo: pulsing the interaction sensor threw");
    pulsed = false;
  }
  phasmo::RangeSensor::onPulsed.set(sensor, onPulsed.get());
  return pulsed;
}

/** Whether the ghost's interaction sensor, pulsed where the ghost stands now, detects `object`. */
bool in_reach(void* ghost, void* object) {
  void* activity = game::ghost_activity(ghost);
  void* sensor = activity ? phasmo::GhostActivity::interactionSensor.get(activity) : nullptr;
  return api::alive(sensor) && pulse_sensor(activity, sensor) &&
         game::sensor_detects(sensor, game::game_object_of(object));
}

/** Every client runs each player's grab of a door; the host notes it for the repeat. */
void detour_door_grabbed(api::Original<phasmo::Door::NetworkedGrabbedDoor> original, void* door) {
  original(door);
  if (void* room = door ? favourite_room() : nullptr) player_used(door, door_of(room, door), "door");
}

}  // namespace

bool resolve() { return true; }

bool create_hooks() {
  return api::hook<phasmo::VivoxParticipantTap::DoAudioFilterRead>(&detour_tap_read,
                                                                  "VivoxParticipantTap.DoAudioFilterRead",
                                                                  api::Twins::CanonicalOnly) &&
         api::hook<phasmo::LightSwitch::Use>(&detour_switch_use, "LightSwitch.Use", api::Twins::CanonicalOnly) &&
         api::hook<phasmo::Door::NetworkedGrabbedDoor>(&detour_door_grabbed, "Door.NetworkedGrabbedDoor",
                                                      api::Twins::CanonicalOnly);
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
  if (!ghost || !game::is_master()) return false;
  const ULONGLONG now = GetTickCount64();
  std::vector<int> room;
  game::room_actors(room);
  const std::vector<int> who = candidates(room, now);
  if (who.empty()) return false;  // nobody's words are held anywhere: the Echo has nothing to say
  if (!actor) actor = who[std::uniform_int_distribution<size_t>(0, who.size() - 1)(g_random)];
  // Sent by a client that holds the phrase: the host when it heard it, else one that reported it.
  const int source = holder_of(actor, room, now);
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
  if (steered || dormant() || game::ghost_state(g.ghost) == phasmo::GhostState::hunting() || now < g.nextWords) return;
  g.nextWords = now + kMinGapMs + std::uniform_int_distribution<ULONGLONG>(0, kMaxGapMs - kMinGapMs)(g_random);
  speak(g.ghost, 0);
}

void reset() {
  g = State{};
}

bool dormant_now() { return dormant(); }

bool repeat(void* ghost) {
  if (!ghost || !game::is_master() || game::ghost_state(ghost) == phasmo::GhostState::hunting()) return false;
  const ULONGLONG now = GetTickCount64();
  void* object = g.repeat.pending(now);
  if (!object || !game::alive(object) || !in_reach(ghost, object)) return false;  // it waits, within its minute
  g.repeat.take(now);
  const bool door = phasmo::Door::type.is(object);
  void* interact = door ? phasmo::Door::interact.get(object)
                        : phasmo::Component::GetComponent(object, phasmo::PhotonObjectInteract::type.type_object());
  if (!api::alive(interact)) return false;
  // Forced, as the voodoo doll's interaction is (NormalInteraction(true)): an unlit switch is used every time.
  phasmo::GhostAI::ChangeState(ghost, door ? phasmo::GhostState::door() : phasmo::GhostState::light(), interact, !door);
  PM_LOG("echo: repeats a player's %s", door ? "door" : "light switch");
  return true;
}

}  // namespace pm::custom::echo
