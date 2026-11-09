#include "emote.h"

#include <windows.h>

#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Animator.h"
#include "Camera.h"
#include "Input.h"
#include "Playables.h"
#include "Player.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "assets.h"
#include "bundles.h"
#include "emote_assets.h"
#include "emote_net.h"
#include "emote_rule.h"
#include "emote_session.h"
#include "game.h"
#include "held.h"
#include "keybinds.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "player_anim.h"
#include "players.h"
#include "talk_loudness.h"
#include "text_table.h"
#include "third_person.h"
#include "wheel.h"

namespace pm::emotes::play {

namespace {

namespace keys = api::keys;
namespace talk = api::talk;

// Starting values, judged with the user in game.
constexpr float kBlendIn = 0.25f;       // seconds the dance takes to blend in
constexpr float kBlendOut = 0.35f;      // and out, the body's facing with it
constexpr float kSongVolume = 0.8f;     // the song's volume, 0 to 1, before its fall with distance
constexpr float kHeadClearance = 0.3f;  // in VR, the head stays hidden while this close to the headset, in metres

// The key's id and its defaults (the user's: B, D-pad down, a free left-controller button; each falls back when the
// game binds it, keybinds.h), and the VR controllers' buttons the mod reads.
constexpr const char* kWheelKey = "wheel";
constexpr const char* kVrPickKey = "vr-pick";
constexpr const char* kVrEndKeys[] = {"vr-left-grip", "vr-right-grip", "vr-left-trigger", "vr-right-trigger"};

// The game's actions that end an emote (the user's list: using or placing an item, picking up, dropping, throwing, a
// door, a light switch, crouching), by their names in the game's controls (sdk/systems/players.md). Switching items
// (InventorySwap, its scroll and the slots) is not among them.
constexpr const char* kEndActions[] = {"Pickup", "Primary Use", "Secondary Use", "HoldPrimaryUse", "Interact", "Drop",
                                       "Crouch"};
constexpr const char* kLookAction = "Look";

using phasmo::Quaternion;
using phasmo::Vector3;

float yaw_of(void* transform) {
  const Vector3 f = game::forward(transform);
  return std::atan2(f.x, f.z) * 57.29577951f;
}
Quaternion yaw_quat(float degrees) {
  const float h = degrees / 57.29577951f / 2.0f;
  return {0.0f, std::sin(h), 0.0f, std::cos(h)};
}
Quaternion mul(Quaternion a, Quaternion b) {
  return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
          a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

// The visible body of one dancer: its Animator's transform, turned to the facing rule's yaw under its root.
struct Facing {
  api::Held model, root;
  Quaternion original{0, 0, 0, 1};  // the model's local rotation before the emote
  float bodyYaw = 0.0f;
  float easeBack = -1.0f;  // seconds left easing back to `original` after the emote, -1 while dancing
  float offset = 0.0f;     // the yaw the model is turned by under its root, degrees
};

// One dancer's song.
struct Song {
  api::Held source;  // its AudioSource; its GameObject goes with it
};

struct Assets {
  api::Held bundle;
  std::array<api::Held, kSegments> clips, songs;
  std::array<talk::Envelope, kSegments> loudness;
  EmoteTable table{};
};

struct State {
  Assets a;
  std::optional<LocalSession> local;
  std::optional<RemoteSessions> remote;
  api::Held localPlayer;
  WheelInput wheelIn;
  float mouseX = 0.0f, mouseY = 0.0f;  // the mouse's movement since the wheel opened
  bool vrWheel = false;
  std::map<int, Facing> facings;  // by actor; this client's own under its own actor
  std::map<int, Song> songs;
  std::map<int, talk::Pace> paces;
  std::vector<api::Held> endActions;
  api::Held look;
  api::Held controlsOf;  // the rig the actions were read from
  api::Held pcLook, propGrab, heldItem, head;
  bool lookOff = false;
  ULONGLONG lastTick = 0;
  bool loggedRoot = false;
};
State g;

// ---- Assets ----

bool load_assets() {
  const std::filesystem::path folder = api::assets_folder();
  if (folder.empty()) {
    PM_LOG("Emotes inactive: its assets folder is missing");
    return false;
  }
  std::string why;
  void* bundle = api::load_bundle(folder / kBundleFile, &why);
  if (!bundle) {
    PM_LOG("Emotes inactive: %s cannot be loaded (%s)", kBundleFile, why.c_str());
    return false;
  }
  g.a.bundle = bundle;
  for (int i = 0; i < kSegments; ++i) {
    const EmoteAsset& e = kEmotes[i];
    void* clip = api::load_bundle_asset(bundle, clip_path(e).c_str(), phasmo::AnimationClip::type.type_object());
    void* song = api::load_bundle_asset(bundle, song_path(e).c_str(), phasmo::AudioClip::type.type_object());
    if (!clip) {
      PM_LOG("Emotes inactive: the bundle has no clip %s", clip_path(e).c_str());
      return false;
    }
    if (!song) PM_LOG("Emotes: the bundle has no song %s; its emote plays silent", song_path(e).c_str());
    g.a.clips[i] = clip;
    g.a.songs[i] = song;
    const float seconds = api::anim::clip_seconds(clip);
    g.a.table[i] = {seconds, api::anim::clip_loops(clip), song_loop_of(e, seconds)};
    std::ifstream in(folder / loudness_path(e), std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::vector<float> rms = talk::parse_rms(text);
    for (float& r : rms) r *= kSongVolume;  // as loud as it plays
    g.a.loudness[i] = talk::envelope_of_rms(rms);
    if (rms.empty()) PM_LOG("Emotes: no loudness for %s; the ghost does not hear it", e.folder);
    PM_LOG("Emotes: %s, %.3f s, %s, song loop %.3f s from %.3f s", e.clip, seconds,
           g.a.table[i].loops ? "looping" : "one-shot", g.a.table[i].song.length, g.a.table[i].song.start);
  }
  return wheel::load(bundle);
}

// ---- The game's actions ----

void* game_actions_asset() {
  void* rig = game::local_player_component();
  void* controls = rig ? phasmo::LocalPCPlayer::controls.get(rig) : nullptr;
  return controls && game::alive(controls) ? phasmo::PCControls::actions.get(controls) : nullptr;
}

// Reads the game's ending actions and Look by name from the local rig's controls, once per rig.
void read_actions() {
  void* rig = game::local_player_component();
  if (rig == g.controlsOf.get() && rig) return;
  g.controlsOf = rig;
  g.endActions.clear();
  g.look = nullptr;
  void* asset = game_actions_asset();
  const api::RefArray maps{asset ? phasmo::InputActionAsset::m_ActionMaps.get(asset) : nullptr};
  for (size_t m = 0; m < maps.size(); ++m) {
    const api::RefArray actions{maps[m] ? phasmo::InputActionMap::m_Actions.get(maps[m]) : nullptr};
    for (size_t i = 0; i < actions.size(); ++i) {
      const std::string name = api::to_utf8(phasmo::InputAction::m_Name.get(actions[i]));
      for (const char* end : kEndActions)
        if (name == end) g.endActions.emplace_back(actions[i]);
      if (name == kLookAction) g.look = actions[i];
    }
  }
}

bool end_action_pressed(bool vr) {
  for (const api::Held& a : g.endActions)
    if (a && phasmo::InputAction::WasPressedThisFrame(a)) return true;
  if (vr)
    for (const char* k : kVrEndKeys)
      if (keys::key_state(k).pressed) return true;
  return false;
}

void set_look(bool on) {
  if (!g.look || g.lookOff == !on) return;
  if (on)
    phasmo::InputAction::Enable(g.look);
  else
    phasmo::InputAction::Disable(g.look);
  g.lookOff = !on;
}

// ---- A dancer's body, facing and song ----

void* animator_of(void* player) { return player ? phasmo::Player::animator.get(player) : nullptr; }

void begin_facing(int actor, void* player, float yaw) {
  void* animator = animator_of(player);
  void* model = animator ? game::transform_of(animator) : nullptr;
  void* root = player ? game::transform_of(player) : nullptr;
  if (!model || !root) return;
  if (model == root) {
    if (!g.loggedRoot) PM_LOG("Emotes: the body's Animator is on the player's root; its facing follows the root");
    g.loggedRoot = true;
    return;
  }
  auto it = g.facings.find(actor);
  if (it != g.facings.end() && it->second.model.get() == model) {
    it->second.bodyYaw = yaw;
    it->second.easeBack = -1.0f;
    return;
  }
  Facing f;
  f.model = model;
  f.root = root;
  f.original = game::local_rotation(model);
  f.bodyYaw = yaw;
  g.facings[actor] = std::move(f);
}

void end_facing(int actor) {
  auto it = g.facings.find(actor);
  if (it != g.facings.end() && it->second.easeBack < 0.0f) it->second.easeBack = kBlendOut;
}

// Each frame: the model turned to its body yaw under its root, or easing back after the emote.
void apply_facings(float dt) {
  for (auto it = g.facings.begin(); it != g.facings.end();) {
    Facing& f = it->second;
    if (!f.model || !game::alive(f.model) || !f.root || !game::alive(f.root)) {
      it = g.facings.erase(it);
      continue;
    }
    if (f.easeBack >= 0.0f) {
      f.easeBack -= dt;
      f.offset *= std::max(0.0f, f.easeBack) / kBlendOut;
      if (f.easeBack <= 0.0f) {
        game::set_local_rotation(f.model, f.original);
        it = g.facings.erase(it);
        continue;
      }
    } else {
      f.offset = wrap_degrees(f.bodyYaw - yaw_of(f.root));
    }
    game::set_local_rotation(f.model, mul(yaw_quat(f.offset), f.original));
    ++it;
  }
}

void stop_song(int actor) {
  auto it = g.songs.find(actor);
  if (it == g.songs.end()) return;
  if (it->second.source && game::alive(it->second.source)) game::destroy(game::game_object_of(it->second.source));
  g.songs.erase(it);
}

void start_song(int actor, void* player, int emote) {
  stop_song(actor);
  void* clip = g.a.songs[emote];
  if (!clip || !player) return;
  void* animator = animator_of(player);
  void* head = animator ? phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Head()) : nullptr;
  void* parent = head ? head : game::transform_of(player);
  void* go = game::new_game_object("pm emote song");
  if (!go || !parent) return;
  phasmo::Transform::SetParent(game::object_transform(go), parent, false);
  game::set_local_position(game::object_transform(go), {0.0f, 0.0f, 0.0f});
  void* source = game::add_component(go, phasmo::AudioSource::type.type_object());
  if (!source) {
    game::destroy(go);
    return;
  }
  phasmo::AudioSource::set_clip(source, clip);
  phasmo::AudioSource::set_spatialBlend(source, 1.0f);
  phasmo::AudioSource::set_volume(source, kSongVolume);
  // It falls off with distance as the player's own voice does (the voice's AudioSource).
  void* audio = phasmo::Player::audio.get(player);
  void* voice = audio ? phasmo::PlayerAudio::GetPlayerVoiceAudioSource(audio) : nullptr;
  if (voice && game::alive(voice)) {
    const int32_t rolloff = phasmo::AudioSourceCurveType::CustomRolloff();
    if (void* curve = phasmo::AudioSource::GetCustomCurve(voice, rolloff))
      phasmo::AudioSource::SetCustomCurve(source, rolloff, curve);
    phasmo::AudioSource::set_minDistance(source, phasmo::AudioSource::get_minDistance(voice));
    phasmo::AudioSource::set_maxDistance(source, phasmo::AudioSource::get_maxDistance(voice));
    phasmo::AudioSource::set_rolloffMode(source, phasmo::AudioSource::get_rolloffMode(voice));
  }
  // Through the game's mixer group for where the player stands, as the game's own sounds go (muffled on another floor).
  void* sounds = phasmo::SoundController::instance.get();
  const Vector3 at = game::position(parent);
  if (sounds && game::alive(sounds))
    if (void* group = phasmo::SoundController::GetFloorAudioSnapshot(sounds, &at))
      phasmo::AudioSource::set_outputAudioMixerGroup(source, group);
  phasmo::AudioSource::Play(source, 0);
  g.songs[actor].source = source;
}

void restart_song(int actor) {
  auto it = g.songs.find(actor);
  if (it == g.songs.end() || !it->second.source || !game::alive(it->second.source)) return;
  phasmo::AudioSource::Stop(it->second.source, true);
  phasmo::AudioSource::Play(it->second.source, 0);  // every loop starts at 0 (emote_assets.h)
}

// ---- This client's own investigator ----

void send(const std::vector<EmoteMsg>& msgs) {
  for (const EmoteMsg& m : msgs) {
    const std::vector<uint8_t> bytes = pack(m);
    net::send(bytes.data(), bytes.size(), net::To::Others, true);
  }
}

bool is_vr(void* player) { return player && phasmo::VRPlayer::type.is(player); }

void* component_in_rig(void* type) {
  void* rig = game::local_player_component();
  return rig && type ? phasmo::Component::GetComponentInChildren(rig, type, true) : nullptr;
}

void begin_local_body(void* player, bool vr) {
  if (vr) {
    api::view::show_own_body(true);
    void* animator = animator_of(player);
    g.head = animator ? phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Head()) : nullptr;
    return;
  }
  if (!api::view::in_third_person()) api::view::begin_third_person();
  if (void* look = component_in_rig(phasmo::PCLook::type.type_object())) {
    g.pcLook = look;
    game::set_enabled(look, false);  // the spine and head follow the dance, not the camera
  }
  // The held item hangs in front of the camera on the player's own client: in the hand while the view is outside.
  if (void* grab = component_in_rig(phasmo::PCPropGrab::type.type_object())) {
    g.propGrab = grab;
    void* item = phasmo::PCPropGrab::GetItemInCameraSlot(grab);
    void* anims = phasmo::Player::characterAnimations.get(player);
    void* hand = anims ? phasmo::PlayerCharacterAnimations::parentingTargetRightHand.get(anims) : nullptr;
    if (item && hand && game::alive(item) && !g.heldItem) {
      void* t = game::transform_of(item);
      game::set_parent(t, hand, false);
      game::set_local_position(t, {0.0f, 0.0f, 0.0f});
      game::set_local_rotation(t, {0.0f, 0.0f, 0.0f, 1.0f});
      g.heldItem = item;
    }
  }
}

void end_local_body() {
  api::view::end_third_person();
  api::view::show_own_body(false);
  if (g.pcLook && game::alive(g.pcLook)) game::set_enabled(g.pcLook, true);
  g.pcLook = nullptr;
  if (g.heldItem && game::alive(g.heldItem) && g.propGrab && game::alive(g.propGrab))
    phasmo::PCPropGrab::ParentItemOnLocalPlayer(g.propGrab, g.heldItem);  // back in the camera's slot, as the game had it
  g.heldItem = nullptr;
  g.propGrab = nullptr;
  if (g.head && game::alive(g.head)) game::set_local_scale(g.head, {1.0f, 1.0f, 1.0f});
  g.head = nullptr;
}

void apply_local(const LocalOut& o, void* player, bool vr) {
  const int me = game::local_actor();
  if (o.start >= 0) {
    api::anim::play_on_player(player, g.a.clips[o.start], kBlendIn);
    start_song(me, player, o.start);
    begin_local_body(player, vr);
    begin_facing(me, player, o.bodyYaw.value_or(0.0f));
  }
  if (o.stop) {
    api::anim::stop_on_player(player, kBlendOut);
    stop_song(me);
    end_local_body();
    end_facing(me);
  }
  if (o.restartSong) restart_song(me);
  if (o.bodyYaw && !o.stop) {
    auto it = g.facings.find(me);
    if (it != g.facings.end()) it->second.bodyYaw = *o.bodyYaw;
  }
  send(o.send);
}

// Whether the local investigator is one of the game's players (Playable Ghost's ghost player is not).
bool in_player_list(void* player) {
  bool found = false;
  game::each_player([&](void* p) { found |= p == player; });
  return found;
}

// The wheel's key, its pick, and what it plays.
std::optional<int> wheel_input(void* player, bool vr) {
  const keys::KeyState key = keys::key_state(kWheelKey);
  if (vr) {
    if (!wheel::is_open()) {
      if (key.pressed) {
        void* left = phasmo::VRPlayer::leftHandIK.get(player);
        g.vrWheel = wheel::open_vr(left);
      }
      return std::nullopt;
    }
    void* cam = game::local_camera();
    wheel::face(cam ? game::transform_of(cam) : nullptr);
    const int pointed = wheel::pointed(phasmo::VRPlayer::rightHandIK.get(player));
    wheel::show_pick(pointed);
    if (key.pressed) {  // the button again, nothing played
      wheel::close();
      return std::nullopt;
    }
    if (pointed >= 0 && keys::key_state(kVrPickKey).pressed) {
      wheel::close();
      return pointed;
    }
    return std::nullopt;
  }
  // On screen: a menu with the cursor free (the pause screen, the journal) closes it with nothing played.
  const bool interrupted = phasmo::Cursor::get_lockState() != phasmo::CursorLockMode::Locked();
  const bool wasOpen = g.wheelIn.open;
  int pickNow = -1;
  if (wasOpen) {
    float dx = 0.0f, dy = 0.0f;
    if (game::mouse_delta(dx, dy)) {
      g.mouseX += dx;
      g.mouseY += dy;
    }
    pickNow = segment_for(g.mouseX, g.mouseY, kPickDeadZone);
    if (void* pad = phasmo::Gamepad::get_current()) {
      const phasmo::Vector2 stick = phasmo::Vector2Control::ReadValue(phasmo::Gamepad::get_rightStick(pad));
      if (const int s = segment_for(stick.x, stick.y, kStickDeadZone); s >= 0) pickNow = s;
    }
  }
  const std::optional<int> played = wheel_step(g.wheelIn, key.held, key.released, interrupted, pickNow);
  if (g.wheelIn.open && !wasOpen) {
    g.mouseX = g.mouseY = 0.0f;
    if (!wheel::open_screen()) g.wheelIn = WheelInput{};
    else set_look(false);
  }
  if (g.wheelIn.open) wheel::show_pick(g.wheelIn.picked);
  if (!g.wheelIn.open && wasOpen) {
    wheel::close();
    set_look(true);
  }
  return played;
}

void end_everything_local() {
  if (g.local && g.local->active()) {
    void* player = g.localPlayer.get();
    apply_local(g.local->end_now(), player && game::alive(player) ? player : nullptr, false);
  }
  if (wheel::is_open()) wheel::close();
  g.wheelIn = WheelInput{};
  set_look(true);
}

// ---- Everyone else ----

void apply_remote(const std::vector<RemoteOut>& outs) {
  for (const RemoteOut& o : outs) {
    void* player = game::player_of_actor(o.actor);
    switch (o.what) {
      case RemoteOut::Start:
        if (player) {
          api::anim::play_on_player(player, g.a.clips[o.emote], kBlendIn);
          start_song(o.actor, player, o.emote);
          begin_facing(o.actor, player, o.yaw);
        }
        break;
      case RemoteOut::Stop:
        if (player) api::anim::stop_on_player(player, kBlendOut);
        stop_song(o.actor);
        end_facing(o.actor);
        break;
      case RemoteOut::RestartSong:
        restart_song(o.actor);
        break;
      case RemoteOut::Yaw: {
        auto it = g.facings.find(o.actor);
        if (it != g.facings.end()) it->second.bodyYaw = o.yaw;
        break;
      }
    }
  }
}

// ---- The ghost's hearing, on the master ----

void raise_loudness(int actor, void* player, int emote, float now) {
  if (!player || emote < 0) return;
  const double t = api::anim::clip_time(player);
  if (t < 0.0 || !g.paces[actor].due(now)) return;
  const float at = static_cast<float>(song_position(t, g.a.table[emote].song));
  talk::add_talk_loudness(player, g.a.loudness[emote].at(at));
}

}  // namespace

bool start() {
  if (!load_assets()) return false;
  const std::filesystem::path data = std::filesystem::path(mod::host().dataDirectory) / mod::info().id / "keybinds.txt";
  keys::begin_keys(mod::info().id, data);
  keys::ModKey wheel;
  wheel.id = kWheelKey;
  wheel.label = i18n::text(kKeyLabel);
  wheel.paths = {"<Keyboard>/b", "<Gamepad>/dpad/down", "<XRController>{LeftHand}/{PrimaryButton}"};
  wheel.fallbacks[0] = {"<Keyboard>/x"};
  wheel.fallbacks[1] = {"<Gamepad>/dpad/up"};
  keys::add_key(wheel);
  keys::ModKey pick;
  pick.id = kVrPickKey;
  pick.paths[2] = "<XRController>{RightHand}/{TriggerButton}";
  keys::add_key(pick);
  const char* ends[] = {"<XRController>{LeftHand}/{GripButton}", "<XRController>{RightHand}/{GripButton}",
                        "<XRController>{LeftHand}/{TriggerButton}", "<XRController>{RightHand}/{TriggerButton}"};
  for (size_t i = 0; i < std::size(kVrEndKeys); ++i) {
    keys::ModKey k;
    k.id = kVrEndKeys[i];
    k.paths[2] = ends[i];
    keys::add_key(k);
  }
  g.local.emplace(g.a.table);
  g.remote.emplace(g.a.table);
  g.lastTick = GetTickCount64();
  return true;
}

void tick() {
  const ULONGLONG nowMs = GetTickCount64();
  const float dt = std::min(0.1f, static_cast<float>(nowMs - g.lastTick) / 1000.0f);
  g.lastTick = nowMs;
  const float now = static_cast<float>(nowMs / 10) / 100.0f;
  keys::tick_keys();
  api::anim::tick_player_anims(dt);
  api::view::tick_third_person(dt);

  // This client's investigator: none in the menus, and none for Playable Ghost's ghost player.
  void* player = game::local_player();
  const bool present = player && in_player_list(player);
  if (player != g.localPlayer.get()) {
    end_everything_local();
    read_actions();
    g.localPlayer = player;
  }
  read_actions();
  const bool vr = present && is_vr(player);
  LocalFrame f;
  f.present = present;
  if (present) {
    f.dead = phasmo::Player::isDead.get(player);
    const Vector3 p = game::position(game::transform_of(player));
    f.pos = {p.x, p.y, p.z};
    f.viewYaw = api::view::view_yaw();
    f.clipTime = api::anim::clip_time(player);
    f.endAction = end_action_pressed(vr) && g.local->active() && !wheel::is_open();
  }
  if (present && keys::keys_ready()) {
    if (const std::optional<int> played = wheel_input(player, vr)) apply_local(g.local->pick(*played, f), player, vr);
  } else if (wheel::is_open()) {
    wheel::close();
    g.wheelIn = WheelInput{};
    set_look(true);
  }
  apply_local(g.local->tick(f), player, vr);
  // In VR the head stays hidden while it would sit over the view.
  if (g.head && game::alive(g.head)) {
    void* cam = game::local_camera();
    const bool overView = cam && game::distance(game::position(g.head), game::position(game::transform_of(cam))) < kHeadClearance;
    game::set_local_scale(g.head, overView ? Vector3{0.001f, 0.001f, 0.001f} : Vector3{1.0f, 1.0f, 1.0f});
  }

  // Everyone else's dances.
  std::vector<RemoteFrame> frames;
  for (const auto& [actor, emote] : g.remote->dancers()) {
    void* p = game::player_of_actor(actor);
    frames.push_back({actor, p && game::alive(p), p && phasmo::Player::isDead.get(p),
                      p ? api::anim::clip_time(p) : -1.0});
  }
  apply_remote(g.remote->tick(frames, f.dead));
  apply_facings(dt);

  // The ghost hears the songs as the dancers talking, on the master, where it runs.
  if (game::is_master()) {
    if (g.local->active()) raise_loudness(game::local_actor(), player, g.local->emote(), now);
    for (const auto& [actor, emote] : g.remote->dancers()) raise_loudness(actor, game::player_of_actor(actor), emote, now);
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (!g.remote) return;
  const std::optional<EmoteMsg> m = unpack({data, size});
  if (!m) return;
  void* player = game::player_of_actor(sender);
  if (!player && m->kind == Kind::Start) return;
  void* local = game::local_player();
  const bool senderDead = player && phasmo::Player::isDead.get(player);
  const bool localDead = local && phasmo::Player::isDead.get(local);
  apply_remote(g.remote->receive(sender, *m, senderDead, localDead));
}

void stop() {
  end_everything_local();
  if (g.remote) {
    for (const auto& [actor, emote] : g.remote->dancers()) stop_song(actor);
    g.remote->clear();
  }
  api::anim::stop_all_player_anims();
  api::view::stop_third_person();
  for (auto& [actor, f] : g.facings)
    if (f.model && game::alive(f.model)) game::set_local_rotation(f.model, f.original);
  for (auto& [actor, s] : g.songs)
    if (s.source && game::alive(s.source)) game::destroy(game::game_object_of(s.source));
  wheel::unload();
  keys::clear_keys();
  g = State{};
}

}  // namespace pm::emotes::play
