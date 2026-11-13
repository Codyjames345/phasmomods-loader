#include "emote.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "Animator.h"
#include "Camera.h"
#include "Input.h"
#include "JournalController.h"
#include "Playables.h"
#include "Player.h"
#include "Props.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "UnityWorld.h"
#include "UnityRendering.h"
#include "UnityObject.h"
#include "assets.h"
#include "bundles.h"
#include "catalogue.h"
#include "emote_assets.h"
#include "emote_net.h"
#include "emote_rule.h"
#include "emote_session.h"
#include "game.h"
#include "held.h"
#include "hooks.h"
#include "keybinds.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "packs.h"
#include "paused_actions.h"
#include "wheel_pages.h"
#include "player_anim.h"
#include "players.h"
#include "settings.h"
#include "input_device.h"
#include "join_prompt.h"
#include "sync_rule.h"
#include "journal.h"
#include "movement.h"
#include "talk_loudness.h"
#include "text_table.h"
#include "third_person.h"
#include "wheel.h"

namespace pm::emotes::play {

namespace {

namespace input = api::input;
namespace journal = api::journal;
namespace keys = api::keys;
namespace movement = api::movement;
namespace talk = api::talk;

// Starting values, judged with the user in game.
constexpr float kBlendIn = 0.25f;       // seconds the dance takes to blend in
constexpr float kBlendOut = 0.35f;      // and out, the body's facing with it
// Every song's volume at the player's Emotes volume of 100%, 0 to 1, before its fall with distance: the dancer's own and
// everyone else's alike (the user's ruling, 2026-11-10).
constexpr float kSongVolume = 0.05f;
constexpr const char* kVolumeSetting = "volume";
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

// Joining a synced emote (docs/superpowers/specs/2026-11-13-synced-emotes-design.md). The game's own Interact action
// asks to join (the user, 2026-11-13: "default to the interact button"); in VR, whose Interact this mod has not found,
// the right trigger the wheel picks with. The game's Interact ray reaches 1.8 m (PCPropGrab.OnInteract,
// sdk/systems/players.md): no prompt while it would act on something, so one press never does both.
constexpr const char* kInteractAction = "Interact";
constexpr float kInteractReach = 1.8f;
constexpr float kChestBelowHead = 0.25f;  // the prompt's look test aims this far below the head bone, metres
constexpr float kFloorSkin = 0.05f;       // the spot's test lifts the capsule this far, so the floor is not in the way
constexpr float kPartLengthTolerance = 1.0f / 60.0f;  // a synced emote's two parts are one length within this, seconds

// The controls that turn the wheel's pages (the user's, 2026-11-12): the scroll wheel and the gamepad's shoulder
// buttons. Every game action bound to one of them is paused while the wheel is open (the item swap on the scroll wheel,
// push-to-talk on the bumpers, sdk/systems/players.md), found by its binding's path rather than its name.
bool turns_pages(const std::string& path) {
  return path.rfind("<Mouse>/scroll", 0) == 0 || path == "<Gamepad>/leftShoulder" || path == "<Gamepad>/rightShoulder";
}

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
  // The journal (its JournalRigController's object), which the game moves each Update to its hanger, placed by Animation
  // Rigging from the body's pose: the dance plays over the rig, so it is kept where it hung on the waist bone when the
  // dance began. Never the hanger itself: the game saves and restores the hanger's place as the journal opens and
  // closes, so a change there stays (the journal came back in front of the face, the sixth run).
  api::Held journal, waist, book, rig;
  Pose onWaist{};
  // For the log: whose body this is, how long it has danced, and whether the journal's place mid-dance was logged (the
  // user, 2026-11-10: another player's journal shows in front of their face while they dance, though not on their own
  // screen; logged to see why before anything changes).
  int actor = -1;
  float age = 0.0f;
  bool reported = false;
};

// One dancer's song.
struct Song {
  api::Held source;  // its looping AudioSource; its GameObject goes with it
  api::Held intro;   // the intro song's AudioSource, played once before it, or null
  api::Held group;   // the mixer group both play through, or null before one is found
  float volume = 0.0f;  // the volume it plays at
  float gain = 1.0f;    // its song's gain to the one loudness every song plays at (song_gain)
};

// Every emote on the wheel, in wheel order (catalogue.h wheel_order): the mod's own, then each content pack's.
struct Assets {
  std::vector<api::Held> bundles;  // the mod's own bundle and each pack's, unloaded when the mod stops
  std::vector<EmoteDef> defs;
  std::vector<api::Held> clips, songs, icons;
  std::vector<api::Held> moveClips, introClips, introSongs;  // null where an emote has none
  std::vector<talk::Envelope> loudness, introLoudness;
  std::vector<float> gains;  // each song's gain to kSongLoudness, its intro song's too (song_gain)
  // A synced emote's second part and its synced song (null where none), and that song's loudness.
  std::vector<api::Held> partnerClips, partnerSongs;
  std::vector<talk::Envelope> partnerLoudness;
  std::vector<float> partnerGains;  // each synced song's own gain
  std::vector<std::string> names;  // each emote's name as the player reads it
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
  // The players whose death has begun and is not over: from the game's notice of a kill to its mark of the death
  // (Player.isDead). Their body is the game's death animation, so nothing of ours goes over it (the user, 2026-11-10).
  std::set<int> dying;
  std::map<int, Song> songs;
  std::map<int, talk::Pace> paces;
  std::vector<api::Held> endActions;
  std::vector<api::Held> wheelActions;  // what the wheel pauses while open: Look, and every action on a page control
  PausedActions<api::Held> paused;      // what it did pause, to give back on every close
  // The game's own glyph widgets for the shoulder buttons (sdk/systems/menus.md, "Input glyphs"): their Images show the
  // sprite the game picked for the pad in use, and turn clear on keyboard and mouse.
  api::Held previousGlyph, nextGlyph;
  std::map<int, void*> playing;  // by actor: the clip playing on that body, so a phase whose clip it already is plays on
  int page = 0;  // the page the wheel shows, kept while the mod runs (the user's: the same session only)
  api::Held controlsOf;  // the rig the actions were read from
  api::Held pcLook, propGrab, hand, head;
  std::vector<api::Held> movedItems;  // items put in the hand from the camera's slot while the view is outside
  bool logItemAfterView = false;      // for the log: the held item once the view is back in first person
  ULONGLONG lastTick = 0;
  ULONGLONG startTick = 0;  // the mod's start: its clock counts from here, so a float keeps its precision
  bool loggedRoot = false;
  // Joining a synced emote: the game's Interact action, the shoulder glyph's widget (whose Xbox and PS5 sets give
  // Interact's glyph), this client's glide into a partner's spot, and the first dancers whose song is the synced one.
  api::Held interactAction;
  api::Held glyphWidget;
  std::optional<Glide> glide;
  std::set<int> syncedSongs;
  bool loggedNoCapsule = false;
};
State g;

// ---- Assets ----

// One catalogue's emotes, loaded from its bundle, before they take their places on the wheel.
struct Loaded {
  EmoteDef def;
  api::Held clip, song, icon, moveClip, introClip, introSong, partnerClip, partnerSong;
  talk::Envelope loudness, introLoudness, partnerLoudness;
  float gain = 1.0f, partnerGain = 1.0f;
  std::string name;
  EmoteInfo info;
};

std::string read_text(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

// A song's loudness file, measured beforehand: its RMS amplitude per window.
std::vector<float> rms_of(const std::filesystem::path& file) { return talk::parse_rms(read_text(file)); }

// A song's loudness as it plays, for the ghost's hearing: its measured RMS at the song's base volume and its gain.
talk::Envelope loudness_as_played(std::vector<float> rms, float gain) {
  for (float& r : rms) r *= kSongVolume * gain;
  return talk::envelope_of_rms(rms);
}

// Loads the catalogue in `folder` (its emotes.json and bundle) for `packId`, naming each emote with `nameOf`. False, with
// `why` for the log, when anything the catalogue needs is missing: the caller then leaves the whole catalogue out.
template <class NameOf>
bool load_catalogue(const std::filesystem::path& folder, const std::string& packId, NameOf nameOf, Catalogue& cat,
                    std::vector<Loaded>& out, std::string& why) {
  if (!parse_catalogue(read_text(folder / "emotes.json"), packId, cat, why)) return false;
  for (const std::string& w : cat.warnings)
    PM_LOG("Emotes: %s's emotes.json has a field this mod does not know: %s", packId.c_str(), w.c_str());
  void* bundle = api::load_bundle(folder / cat.bundle, &why);
  if (!bundle) {
    why = cat.bundle + " cannot be loaded (" + why + ")";
    return false;
  }
  g.a.bundles.emplace_back(bundle);
  for (const EmoteDef& e : cat.emotes) {
    Loaded l;
    l.def = e;
    l.clip = api::load_bundle_asset(bundle, bundle_path(e.clip).c_str(), phasmo::AnimationClip::type.type_object());
    if (!l.clip) {
      why = "the bundle has no clip " + e.clip;
      return false;
    }
    // A walking or intro clip the catalogue names must be there, as the clip must: the emote would play wrong without it.
    // A moving clip that is the clip itself (a clip that walks as it is) is the same asset.
    for (auto [path, held] : {std::pair{&e.moveClip, &l.moveClip}, std::pair{&e.introClip, &l.introClip}}) {
      if (path->empty()) continue;
      *held = api::load_bundle_asset(bundle, bundle_path(*path).c_str(), phasmo::AnimationClip::type.type_object());
      if (!*held) {
        why = "the bundle has no clip " + *path;
        return false;
      }
    }
    // A synced emote's second part must be there too; its synced song may be missing, and the pair dance to the song.
    if (e.partner) {
      l.partnerClip =
          api::load_bundle_asset(bundle, bundle_path(e.partner->clip).c_str(), phasmo::AnimationClip::type.type_object());
      if (!l.partnerClip) {
        why = "the bundle has no clip " + e.partner->clip;
        return false;
      }
      if (!e.partner->song.empty()) {
        l.partnerSong =
            api::load_bundle_asset(bundle, bundle_path(e.partner->song).c_str(), phasmo::AudioClip::type.type_object());
        if (!l.partnerSong)
          PM_LOG("Emotes: %s has no synced song %s; a pair dance to its song", packId.c_str(), e.partner->song.c_str());
        const std::vector<float> rms = rms_of(folder / e.partner->loudness);
        l.partnerGain = song_gain(rms);
        l.partnerLoudness = loudness_as_played(rms, l.partnerGain);
      }
    }
    // Every song plays at one loudness (song_gain, the user's, 2026-11-13), from its own measured loudness file, so
    // any emote added later, a pack's too, is brought to it. An intro song takes its loop's gain, keeping the step
    // between them as the song has it.
    const std::vector<float> songRms = rms_of(folder / e.loudness);
    l.gain = song_gain(songRms);
    l.loudness = loudness_as_played(songRms, l.gain);
    if (!e.introSong.empty()) {
      l.introSong = api::load_bundle_asset(bundle, bundle_path(e.introSong).c_str(), phasmo::AudioClip::type.type_object());
      if (!l.introSong) PM_LOG("Emotes: %s has no intro song %s; its song starts with its loop", packId.c_str(), e.introSong.c_str());
      l.introLoudness = loudness_as_played(rms_of(folder / e.introLoudness), l.gain);
    }
    l.song = api::load_bundle_asset(bundle, bundle_path(e.song).c_str(), phasmo::AudioClip::type.type_object());
    if (!l.song) PM_LOG("Emotes: %s has no song %s; it plays silent", packId.c_str(), e.song.c_str());
    l.icon = api::load_bundle_asset(bundle, bundle_path(e.icon).c_str(), phasmo::Texture2D::type.type_object());
    if (!l.icon) PM_LOG("Emotes: %s has no icon %s; its segment shows bare", packId.c_str(), e.icon.c_str());
    l.name = nameOf(e.label);
    const float seconds = api::anim::clip_seconds(l.clip);
    l.info.clipSeconds = seconds;
    l.info.loops = api::anim::clip_loops(l.clip);
    l.info.song = e.songLoop.length > 0.0f ? e.songLoop : SongLoop{0.0f, seconds};
    l.info.key = e.key;
    l.info.hasMove = static_cast<bool>(l.moveClip);
    l.info.introSeconds = l.introClip ? api::anim::clip_seconds(l.introClip) : 0.0f;
    l.info.introSongSeconds = l.introSong ? phasmo::AudioClip::get_length(l.introSong) : 0.0f;
    l.info.synced = static_cast<bool>(l.partnerClip);
    l.info.partnerSeconds = l.partnerClip ? api::anim::clip_seconds(l.partnerClip) : 0.0f;
    // The two parts loop together, so they must be one length: a frame apart drifts a frame further each loop.
    if (l.partnerClip && std::fabs(l.info.partnerSeconds - seconds) > kPartLengthTolerance) {
      why = e.partner->clip + " is not as long as " + e.clip + ", so the two parts would drift apart";
      return false;
    }
    PM_LOG("Emotes: %s, %.3f s, %s, song loop %.3f s from %.3f s, song gain %.2f", e.id.c_str(), seconds,
           l.info.loops ? "looping" : "one-shot", l.info.song.length, l.info.song.start, l.gain);
    out.push_back(std::move(l));
  }
  return true;
}

// The mod's own catalogue, then each content pack made for it (sdk/api/packs.h). A broken pack is left out and switched
// off with its reason in the Mods panel; a broken catalogue of the mod's own keeps the mod off.
bool load_catalogues() {
  const std::filesystem::path folder = api::assets_folder();
  if (folder.empty()) {
    PM_LOG("Emotes inactive: its assets folder is missing");
    return false;
  }
  std::string why;
  Catalogue own;
  std::vector<Loaded> loaded;
  if (!load_catalogue(folder, mod::info().id, [](const std::string& key) { return i18n::text(key); }, own, loaded, why)) {
    PM_LOG("Emotes inactive: its own emotes cannot be loaded (%s)", why.c_str());
    return false;
  }
  const size_t ownBundles = g.a.bundles.size();
  std::vector<Catalogue> packs;
  for (const api::packs::Pack& p : api::packs::owned()) {
    i18n::Table table;
    std::string tableWhy;
    if (!table.load(p.text, tableWhy))
      PM_LOG("Emotes: %s has no string table (%s); its emotes show their keys", p.id.c_str(), tableWhy.c_str());
    Catalogue cat;
    std::vector<Loaded> mine;
    const size_t bundlesBefore = g.a.bundles.size();
    if (!load_catalogue(p.folder, p.id, [&](const std::string& key) { return table.get(key); }, cat, mine, why)) {
      PM_LOG("Emotes: the pack %s cannot be used (%s)", p.id.c_str(), why.c_str());
      while (g.a.bundles.size() > bundlesBefore) {
        api::unload_bundle(g.a.bundles.back(), true);
        g.a.bundles.pop_back();
      }
      api::packs::failed(p.id, i18n::text("emotes.pack-unusable"));
      continue;
    }
    PM_LOG("Emotes: the pack %s adds %zu emotes", p.id.c_str(), mine.size());
    packs.push_back(std::move(cat));
    loaded.insert(loaded.end(), std::make_move_iterator(mine.begin()), std::make_move_iterator(mine.end()));
  }
  std::vector<EmoteDef> order;
  if (!wheel_order(own, packs, order, why)) {
    // Two emotes share a key: keep the mod's own, and leave every pack out rather than guess which one meant it.
    PM_LOG("Emotes: the packs' emotes are left out (%s)", why.c_str());
    for (const Catalogue& c : packs) api::packs::failed(c.packId, i18n::text("emotes.pack-unusable"));
    // Their bundles go too, so a later start of the mods can load them again.
    while (g.a.bundles.size() > ownBundles) {
      api::unload_bundle(g.a.bundles.back(), true);
      g.a.bundles.pop_back();
    }
    wheel_order(own, {}, order, why);
  }
  std::vector<void*> icons;
  for (const EmoteDef& e : order)
    for (Loaded& l : loaded)
      if (l.def.key == e.key) {
        // Each loaded emote takes one place on the wheel, so its held assets move there.
        icons.push_back(l.icon.get());
        g.a.defs.push_back(l.def);
        g.a.clips.push_back(std::move(l.clip));
        g.a.songs.push_back(std::move(l.song));
        g.a.icons.push_back(std::move(l.icon));
        g.a.moveClips.push_back(std::move(l.moveClip));
        g.a.introClips.push_back(std::move(l.introClip));
        g.a.introSongs.push_back(std::move(l.introSong));
        g.a.loudness.push_back(l.loudness);
        g.a.introLoudness.push_back(l.introLoudness);
        g.a.gains.push_back(l.gain);
        g.a.partnerGains.push_back(l.partnerGain);
        g.a.partnerClips.push_back(std::move(l.partnerClip));
        g.a.partnerSongs.push_back(std::move(l.partnerSong));
        g.a.partnerLoudness.push_back(l.partnerLoudness);
        g.a.names.push_back(l.name);
        g.a.table.push_back(l.info);
        break;
      }
  PM_LOG("Emotes: %zu emotes on the wheel", g.a.defs.size());
  return wheel::load(icons, g.a.names);
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
  if (rig == g.controlsOf.get()) return;  // the same rig, or still none (the main menu): nothing new to read
  g.controlsOf = rig;
  g.endActions.clear();
  g.wheelActions.clear();
  g.previousGlyph = nullptr;
  g.nextGlyph = nullptr;
  g.interactAction = nullptr;
  g.glyphWidget = nullptr;
  std::set<std::string> paging;  // the names of the actions bound to a page control
  void* asset = game_actions_asset();
  const api::RefArray maps{asset ? phasmo::InputActionAsset::m_ActionMaps.get(asset) : nullptr};
  for (size_t m = 0; m < maps.size(); ++m) {
    // The action names bound to a page control in this map, by each binding's path as the player has it.
    paging.clear();
    const api::ValueArray bindings{maps[m] ? phasmo::InputActionMap::m_Bindings.get(maps[m]) : nullptr};
    for (size_t b = 0; b < bindings.size(); ++b) {
      std::string path = api::to_utf8(phasmo::InputBinding::m_OverridePath.get(bindings.at(b)));
      if (path.empty()) path = api::to_utf8(phasmo::InputBinding::m_Path.get(bindings.at(b)));
      if (turns_pages(path)) paging.insert(api::to_utf8(phasmo::InputBinding::m_Action.get(bindings.at(b))));
    }
    const api::RefArray actions{maps[m] ? phasmo::InputActionMap::m_Actions.get(maps[m]) : nullptr};
    for (size_t i = 0; i < actions.size(); ++i) {
      const std::string name = api::to_utf8(phasmo::InputAction::m_Name.get(actions[i]));
      for (const char* end : kEndActions)
        if (name == end) g.endActions.emplace_back(actions[i]);
      if (name == kLookAction || paging.count(name)) g.wheelActions.emplace_back(actions[i]);
      if (name == kInteractAction && !g.interactAction) g.interactAction = actions[i];
    }
  }
  // The game's glyph widgets for the shoulder buttons, by the bindings of the action each shows.
  const api::RefArray widgets{phasmo::Object::FindAllOfType(phasmo::GamepadButton::type.type_object(), true)};
  for (size_t w = 0; w < widgets.size() && (!g.previousGlyph || !g.nextGlyph); ++w) {
    void* ref = phasmo::GamepadButton::action.get(widgets[w]);
    void* action = ref ? phasmo::InputActionReference::get_action(ref) : nullptr;
    void* map = action ? phasmo::InputAction::m_ActionMap.get(action) : nullptr;
    void* image = phasmo::GamepadButton::image.get(widgets[w]);
    if (!map || !image) continue;
    const std::string name = api::to_utf8(phasmo::InputAction::m_Name.get(action));
    const api::ValueArray bindings{phasmo::InputActionMap::m_Bindings.get(map)};
    for (size_t b = 0; b < bindings.size(); ++b) {
      if (api::to_utf8(phasmo::InputBinding::m_Action.get(bindings.at(b))) != name) continue;
      const std::string path = api::to_utf8(phasmo::InputBinding::m_Path.get(bindings.at(b)));
      if (path == "<Gamepad>/leftShoulder" && !g.previousGlyph) {
        g.previousGlyph = image;
        g.glyphWidget = widgets[w];
      }
      if (path == "<Gamepad>/rightShoulder" && !g.nextGlyph) g.nextGlyph = image;
    }
  }
  PM_LOG("Emotes: the wheel pauses %zu of the game's actions while open; the shoulder glyphs %s; Interact %s",
         g.wheelActions.size(), g.previousGlyph && g.nextGlyph ? "were found" : "were not found",
         g.interactAction ? "was found" : "was not found");
}

// The sprite the game's glyph widget for a shoulder button holds (the Xbox or PS5 one it picked for the pad), or null
// (sdk/systems/menus.md, "Input glyphs").
void* glyph_of(const api::Held& image) {
  if (!image || !game::alive(image)) return nullptr;
  return phasmo::Image::get_sprite(image);
}

// The page glyphs on screen: the game's shoulder glyphs while the game's own check says a gamepad is in use
// (pm::api::input::keyboard_in_use, as its glyph widgets decide), else the scroll glyphs (the user's design,
// 2026-11-13: scroll up on the left, down on the right), which also show when the check cannot answer.
void show_screen_glyphs() {
  const bool pad = !input::keyboard_in_use().value_or(true);
  wheel::show_glyphs(pad ? glyph_of(g.previousGlyph) : nullptr, pad ? glyph_of(g.nextGlyph) : nullptr, !pad);
}

// A keyboard control's name as the join prompt's keycap shows it: "e" is E, "space" is Space.
std::string key_name(std::string control) {
  if (!control.empty()) control[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(control[0])));
  return control;
}

// Interact's bindings as the player has them now, a rebinding in the game's menu included (its override path): its
// gamepad control ("buttonWest") and its keyboard key's name ("E"), each empty when it has none.
void read_interact_binding(std::string& pad, std::string& key) {
  pad.clear();
  key.clear();
  void* map = g.interactAction ? phasmo::InputAction::m_ActionMap.get(g.interactAction) : nullptr;
  const api::ValueArray bindings{map ? phasmo::InputActionMap::m_Bindings.get(map) : nullptr};
  for (size_t b = 0; b < bindings.size(); ++b) {
    if (api::to_utf8(phasmo::InputBinding::m_Action.get(bindings.at(b))) != kInteractAction) continue;
    std::string path = api::to_utf8(phasmo::InputBinding::m_OverridePath.get(bindings.at(b)));
    if (path.empty()) path = api::to_utf8(phasmo::InputBinding::m_Path.get(bindings.at(b)));
    if (pad.empty() && path.rfind("<Gamepad>/", 0) == 0) pad = path.substr(10);
    if (key.empty() && path.rfind("<Keyboard>/", 0) == 0) key = key_name(path.substr(11));
  }
}

// The game's sprite for a gamepad control (its Input System path below <Gamepad>/), from the set the game picked for
// the pad in use: the PS5 set while the shoulder glyph shows the PS5 sprite, else the Xbox set (sdk/systems/menus.md,
// "Input glyphs"). Null for a control the sets have no sprite for.
void* pad_sprite(const std::string& control) {
  void* w = g.glyphWidget;
  if (!w || !game::alive(w)) return nullptr;
  void* shown = glyph_of(g.previousGlyph);
  const bool ps5 = shown && shown == phasmo::GamepadButton::ps5_leftShoulder.get(w);
  using B = phasmo::GamepadButton;
#define PM_PAD(path, name) \
  if (control == path) return ps5 ? B::ps5_##name.get(w) : B::xbox_##name.get(w)
  PM_PAD("buttonSouth", buttonSouth);
  PM_PAD("buttonNorth", buttonNorth);
  PM_PAD("buttonEast", buttonEast);
  PM_PAD("buttonWest", buttonWest);
  PM_PAD("start", startButton);
  PM_PAD("select", selectButton);
  PM_PAD("leftTrigger", leftTrigger);
  PM_PAD("rightTrigger", rightTrigger);
  PM_PAD("leftShoulder", leftShoulder);
  PM_PAD("rightShoulder", rightShoulder);
  PM_PAD("dpad", dpad);
  PM_PAD("dpad/up", dpadUp);
  PM_PAD("dpad/down", dpadDown);
  PM_PAD("dpad/left", dpadLeft);
  PM_PAD("dpad/right", dpadRight);
  PM_PAD("leftStick", leftStick);
  PM_PAD("rightStick", rightStick);
  PM_PAD("leftStickPress", leftStickPress);
  PM_PAD("rightStickPress", rightStickPress);
#undef PM_PAD
  return nullptr;
}

// Turns the wheel's page by `steps`, wrapping round.
void turn_page(int steps) {
  if (!steps) return;
  g.page = pages::turn(g.page, steps, static_cast<int>(g.a.defs.size()));
  wheel::set_page(g.page);
}

// Whether this client's journal or pause screen is open (pm::api::journal::open, JournalController.open): opening
// either ends an emote (the user's ruling) and closes the wheel.
bool journal_open() { return journal::open().value_or(false); }

bool end_action_pressed(bool vr) {
  for (const api::Held& a : g.endActions)
    if (a && phasmo::InputAction::WasPressedThisFrame(a)) return true;
  if (vr)
    for (const char* k : kVrEndKeys)
      if (keys::key_state(k).pressed) return true;
  return false;
}

// Pauses (false) or gives back (true) the game's actions the open wheel takes over: Look, and every action bound to a page
// control. Only actions that were on are paused, and exactly those come back.
void set_look(bool on) {
  if (on) {
    for (const api::Held& a : g.paused.resume())
      if (a && game::alive(a)) phasmo::InputAction::Enable(a);
    return;
  }
  std::vector<void*> enabled;
  for (const api::Held& a : g.wheelActions)
    if (a && game::alive(a) && phasmo::InputAction::get_enabled(a)) enabled.push_back(a.get());
  for (void* a : enabled) phasmo::InputAction::Disable(a);
  g.paused.pause(enabled);
}

// ---- A dancer's body, facing and song ----

void* animator_of(void* player) { return player ? phasmo::Player::animator.get(player) : nullptr; }

Pose pose_of(void* t) {
  const Vector3 p = game::position(t);
  const Quaternion q = game::rotation(t);
  return {{p.x, p.y, p.z}, {q.x, q.y, q.z, q.w}};
}

// The journal and the waist bone it hangs from, with where it hangs now, as the game's pose has it.
void hold_journal(Facing& f, void* player) {
  void* book = phasmo::Player::journal.get(player);
  void* rig = book && game::alive(book) ? phasmo::JournalController::rig.get(book) : nullptr;
  void* journal = rig && game::alive(rig) ? game::transform_of(rig) : nullptr;
  void* animator = animator_of(player);
  void* waist = animator ? phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Spine()) : nullptr;
  const bool open = book && game::alive(book) && phasmo::JournalController::open.get(book);
  const char* whose = f.actor == game::local_actor() ? "this client's" : "another player's";
  if (!journal || !waist) {
    PM_LOG("Emotes: actor %d (%s): the journal is not held while dancing: book %s, rig %s, waist bone %s", f.actor, whose,
           book ? "found" : "none", rig ? "found" : "none", waist ? "found" : "none");
    return;
  }
  const Pose w = pose_of(waist), j = pose_of(journal), rel = relative_to(w, j);
  const float feet = f.root && game::alive(f.root) ? game::position(f.root.get()).y : 0.0f;
  void* follow = phasmo::JournalRigController::followTarget.get(rig);
  const std::string moves = follow && game::alive(follow) ? "moves it to " + api::to_utf8(phasmo::Object::get_name(follow))
                                                          : std::string("does not move it (no follow target)");
  PM_LOG("Emotes: actor %d (%s): the journal at the dance's start is %.2f m above the feet, (%.2f, %.2f, %.2f) from %s; "
         "the game %s%s",
         f.actor, whose, j.p.y - feet, rel.p.x, rel.p.y, rel.p.z, api::to_utf8(phasmo::Object::get_name(waist)).c_str(),
         moves.c_str(), open ? "; open, so left to the game" : "");
  if (open) return;
  f.journal = journal;
  f.waist = waist;
  f.book = book;
  f.rig = rig;
  f.onWaist = rel;
}

// Puts the journal back where it hangs on the dancing waist. The mod's tick runs after the frame's animation, so this
// is the pose that renders; the game moves it to its hanger again in its next Update (JournalRigController.Update), so
// nothing stays. Not while the journal is open: the game holds it then. Half a second into a dance it logs where the
// game had put the book that frame and where it goes, once.
void keep_journal(Facing& f, float dt) {
  f.age += dt;
  const bool report = !f.reported && f.easeBack < 0.0f && f.age >= 0.5f;
  if (report) f.reported = true;
  const char* skip = !f.journal || !game::alive(f.journal) ? "no journal held"
                     : !f.waist || !game::alive(f.waist)   ? "the waist bone is gone"
                     : !f.book || !game::alive(f.book)     ? "the book is gone"
                     : phasmo::JournalController::open.get(f.book) ? "the journal is open"
                                                                   : nullptr;
  if (skip) {
    if (report) PM_LOG("Emotes: actor %d mid-dance: the journal is not moved (%s)", f.actor, skip);
    return;
  }
  const Pose at = placed_on(pose_of(f.waist), f.onWaist);
  if (report) {
    const Pose before = pose_of(f.journal);
    const float feet = f.root && game::alive(f.root) ? game::position(f.root.get()).y : 0.0f;
    const float off = std::sqrt((before.p.x - at.p.x) * (before.p.x - at.p.x) + (before.p.y - at.p.y) * (before.p.y - at.p.y) +
                                (before.p.z - at.p.z) * (before.p.z - at.p.z));
    void* follow = f.rig && game::alive(f.rig) ? phasmo::JournalRigController::followTarget.get(f.rig) : nullptr;
    PM_LOG("Emotes: actor %d mid-dance: the game had the journal %.2f m above the feet, %.2f m from where it hangs, %s; "
           "moved to %.2f m above the feet", f.actor, before.p.y - feet, off,
           follow ? "following its target" : "with no follow target", at.p.y - feet);
  }
  game::set_position(f.journal, {at.p.x, at.p.y, at.p.z});
  game::set_rotation(f.journal, {at.q.x, at.q.y, at.q.z, at.q.w});
}

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
  f.actor = actor;
  hold_journal(f, player);
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
    keep_journal(f, dt);
    ++it;
  }
}

// The Photon player a networked Player belongs to, or null: the game pairs them through its player spots
// (Network.playerSpots), as core/players.h does the other way round.
void* photon_player_of(void* player) {
  void* net = player ? game::network() : nullptr;
  const api::List spots{net ? phasmo::Network::playerSpots.get(net) : nullptr};
  for (size_t i = 0; i < spots.size(); ++i)
    if (void* spot = spots[i]; spot && phasmo::PlayerSpot::player.get(spot) == player)
      return phasmo::PlayerSpot::photonPlayer.get(spot);
  return nullptr;
}

// The actor a networked Player belongs to, 0 for none.
int actor_of(void* player) {
  void* photon = photon_player_of(player);
  return photon ? game::actor_number(photon) : 0;
}

// The mixer group a dancer's song plays through: the one the game gives the player's own sounds, footsteps among them,
// by the floor it has the player on (SoundController.GetPlayersAudioGroup), so the game's mixer muffles it as it muffles
// their steps. Null outside a level or when the player has no spot.
void* players_group(void* player) {
  void* sounds = phasmo::SoundController::instance.get();
  void* photon = photon_player_of(player);
  if (!sounds || !game::alive(sounds) || !photon) return nullptr;
  return phasmo::SoundController::GetPlayersAudioGroup(sounds, photon);
}

void stop_song(int actor) {
  auto it = g.songs.find(actor);
  if (it == g.songs.end()) return;
  for (const api::Held* s : {&it->second.source, &it->second.intro})
    if (*s && game::alive(*s)) game::destroy(game::game_object_of(*s));
  g.songs.erase(it);
  g.syncedSongs.erase(actor);
}

// A song's volume on this client: the player's Emotes volume setting (the audio page) of its base, at the song's gain
// to the one loudness every song plays at.
float song_volume(float gain) { return kSongVolume * gain * api::settings::value(kVolumeSetting); }

// Each song follows the setting as the player applies it, and the dancer's mixer group as the game moves them between
// floors (a walking emote can take them up the stairs).
void follow_settings() {
  for (auto& [actor, s] : g.songs) {
    if (!s.source || !game::alive(s.source)) continue;
    const bool introAlive = s.intro && game::alive(s.intro);
    if (s.volume != song_volume(s.gain)) {
      s.volume = song_volume(s.gain);
      phasmo::AudioSource::set_volume(s.source, s.volume);
      if (introAlive) phasmo::AudioSource::set_volume(s.intro, s.volume);
    }
    void* group = players_group(game::player_of_actor(actor));
    if (group && group != s.group.get()) {
      phasmo::AudioSource::set_outputAudioMixerGroup(s.source, group);
      if (introAlive) phasmo::AudioSource::set_outputAudioMixerGroup(s.intro, group);
      s.group = group;
    }
  }
}

// One of a dancer's song sources, under `parent`, playing `clip` as the player's voice would sound to others (its fall
// with distance) through `group`. The dancer's own client hears their song in neither ear (`own`): its listener stays on
// the head camera, which the mouse turns while the view is outside, so a positional song panned and swelled as the
// camera moved round. Null when it cannot be made.
void* song_source(void* parent, void* voice, void* clip, void* group, bool loop, bool own, float volume) {
  void* go = game::new_game_object("pm emote song");
  if (!go || !parent) return nullptr;
  phasmo::Transform::SetParent(game::object_transform(go), parent, false);
  game::set_local_position(game::object_transform(go), {0.0f, 0.0f, 0.0f});
  void* source = game::add_component(go, phasmo::AudioSource::type.type_object());
  if (!source) {
    game::destroy(go);
    return nullptr;
  }
  phasmo::AudioSource::set_clip(source, clip);
  phasmo::AudioSource::set_loop(source, loop);
  phasmo::AudioSource::set_spatialBlend(source, own ? 0.0f : 1.0f);
  phasmo::AudioSource::set_volume(source, volume);
  // It falls off with distance as the player's own voice does (the voice's AudioSource).
  if (voice) {
    const int32_t rolloff = phasmo::AudioSourceCurveType::CustomRolloff();
    if (void* curve = phasmo::AudioSource::GetCustomCurve(voice, rolloff))
      phasmo::AudioSource::SetCustomCurve(source, rolloff, curve);
    phasmo::AudioSource::set_minDistance(source, phasmo::AudioSource::get_minDistance(voice));
    phasmo::AudioSource::set_maxDistance(source, phasmo::AudioSource::get_maxDistance(voice));
    phasmo::AudioSource::set_rolloffMode(source, phasmo::AudioSource::get_rolloffMode(voice));
  }
  if (group) phasmo::AudioSource::set_outputAudioMixerGroup(source, group);
  return source;
}

// The sample rate AudioSource.Play's delay counts in (Unity's documented API: 44.1 kHz whatever the clip's own rate).
constexpr double kPlayDelayRate = 44100.0;

void start_song(int actor, void* player, int emote) {
  stop_song(actor);
  void* clip = g.a.songs[emote];
  if (!clip || !player) return;
  // From where the player's voice plays (PlayerAudio's voice source, which follows the head camera, not the dancing
  // head): a head bone carried it round the dancer's own listener, panning and swelling, and loud at the start.
  void* audio = phasmo::Player::audio.get(player);
  void* voice = audio ? phasmo::PlayerAudio::GetPlayerVoiceAudioSource(audio) : nullptr;
  if (voice && !game::alive(voice)) voice = nullptr;
  void* parent = voice ? game::transform_of(voice) : game::transform_of(player);
  // Through the group the game gives the player's own sounds. The group of the point the song plays from was the floor
  // above when a dancer stood on the stairs, muffling their song.
  const bool own = actor == game::local_actor();
  void* group = players_group(player);
  // The song in the bundle is cut to its loop, so the engine loops it sample for sample.
  const float gain = g.a.gains[emote];
  void* source = song_source(parent, voice, clip, group, true, own, song_volume(gain));
  if (!source) return;
  Song& song = g.songs[actor];
  song.source = source;
  song.group = group;
  song.gain = gain;
  song.volume = song_volume(gain);
  // An intro song plays once, and the looping song starts on the sample it ends: both start this frame, the loop with
  // a delay of the intro's length (AudioSource.PlayScheduled is stripped from this game build).
  void* intro = g.a.introSongs[emote];
  void* introSource = intro ? song_source(parent, voice, intro, group, false, own, song.volume) : nullptr;
  const float introSeconds = g.a.table[emote].introSongSeconds;
  if (introSource && introSeconds > 0.0f) {
    song.intro = introSource;
    phasmo::AudioSource::Play(introSource, 0);
    phasmo::AudioSource::Play(source, static_cast<uint64_t>(std::llround(introSeconds * kPlayDelayRate)));
  } else {
    phasmo::AudioSource::Play(source, 0);
  }
}

// The pair's song becomes the emote's synced song, when its catalogue names one, at the point the first dancer's song
// has reached (the design: "on joining, the pair's song switches to it at the same point"). Sway has none, and its song
// plays on. A pair formed while the intro song still plays keeps it (a join waits for the dance's intro, which the intro
// song can outlast).
void switch_to_synced_song(int first, int emote, double elapsed) {
  void* clip = emote >= 0 && emote < static_cast<int>(g.a.partnerSongs.size()) ? g.a.partnerSongs[emote].get() : nullptr;
  auto it = g.songs.find(first);
  if (!clip || it == g.songs.end() || !it->second.source || !game::alive(it->second.source)) return;
  const EmoteInfo& info = g.a.table[emote];
  const SongAt at = song_position(elapsed, info.introSongSeconds, info.song);
  if (at.intro) {
    PM_LOG("Emotes: a pair formed during the intro song; it keeps its song");
    return;
  }
  if (it->second.intro && game::alive(it->second.intro)) game::destroy(game::game_object_of(it->second.intro));
  it->second.intro = nullptr;
  void* source = it->second.source;
  it->second.gain = g.a.partnerGains[emote];  // the synced song is brought to the same loudness by its own gain
  it->second.volume = song_volume(it->second.gain);
  phasmo::AudioSource::set_volume(source, it->second.volume);
  phasmo::AudioSource::set_clip(source, clip);
  phasmo::AudioSource::Play(source, 0);
  const float length = phasmo::AudioClip::get_length(clip);
  const double t = length > 0.0f ? std::fmod(at.seconds, static_cast<double>(length)) : at.seconds;
  phasmo::AudioSource::set_time(source, static_cast<float>(t));
  g.syncedSongs.insert(first);
}

// Where a synced emote's second part starts on this client: where the first dancer's loop clip is on their body here
// (its Playable's own time, which the session's summed clock can trail after a long frame, and which counts from the
// end of any intro), wrapped to the second part's length; the session's clock when that clip is not on their body.
double second_part_start(int first, int emote, double fallback) {
  void* body = first == game::local_actor() ? game::local_player() : game::player_of_actor(first);
  const auto playing = g.playing.find(first);
  if (body && game::alive(body) && playing != g.playing.end() && playing->second == g.a.clips[emote].get()) {
    const double t = api::anim::clip_time(body);
    const float length = g.a.table[emote].partnerSeconds;
    if (t >= 0.0) return length > 0.0f ? std::fmod(t, static_cast<double>(length)) : t;
  }
  return fallback;
}

// ---- This client's own investigator ----

void send(const std::vector<EmoteMsg>& msgs) {
  for (const EmoteMsg& m : msgs) {
    const std::vector<uint8_t> bytes = pack(m);
    net::send(bytes.data(), bytes.size(), net::To::Others, true);
  }
}

// A dancer whose death has begun, let go of at once: their clip off with nothing blending, their song off, their body
// turned back as it was. The game's death animation has the body from the first frame of the kill, so a dance blending
// out over it hides the death (the user, 2026-11-10).
void release_body(int actor, void* player) {
  if (player && game::alive(player)) api::anim::stop_on_player(player, 0.0f);
  stop_song(actor);
  auto it = g.facings.find(actor);
  if (it == g.facings.end()) return;
  if (it->second.model && game::alive(it->second.model)) game::set_local_rotation(it->second.model, it->second.original);
  g.facings.erase(it);
}

bool is_vr(void* player) { return player && phasmo::VRPlayer::type.is(player); }

void* component_in_rig(void* type) {
  void* rig = game::local_player_component();
  return rig && type ? phasmo::Component::GetComponentInChildren(rig, type, true) : nullptr;
}

// For the log (the user, 2026-11-10: a held item sometimes sits offset in the first-person view after a dance): an
// item's parent, layer and local place, turn and size, and its size in the world.
void log_item(const char* when, void* item) {
  void* t = item && game::alive(item) ? game::transform_of(item) : nullptr;
  if (!t) return;
  void* parent = game::parent(t);
  void* go = game::game_object_of(item);
  const Vector3 p = phasmo::Transform::get_localPosition(t), s = phasmo::Transform::get_localScale(t),
                w = phasmo::Transform::get_lossyScale(t);
  const Quaternion q = phasmo::Transform::get_localRotation(t);
  PM_LOG("Emotes: %s: %s under %s, layer %d, at (%.3f, %.3f, %.3f), turned (%.3f, %.3f, %.3f, %.3f), local size "
         "(%.3f, %.3f, %.3f), world size (%.3f, %.3f, %.3f)",
         when, api::to_utf8(phasmo::Object::get_name(item)).c_str(),
         parent ? api::to_utf8(phasmo::Object::get_name(parent)).c_str() : "nothing",
         go ? phasmo::GameObject::get_layer(go) : -1, p.x, p.y, p.z, q.x, q.y, q.z, q.w, s.x, s.y, s.z, w.x, w.y, w.z);
}

// While the view is outside: an item in the camera's slot (the one held, or one switched to mid-dance) goes in the hand.
void hold_items_in_hand() {
  if (!g.propGrab || !game::alive(g.propGrab) || !g.hand || !game::alive(g.hand)) return;
  void* item = phasmo::PCPropGrab::GetItemInCameraSlot(g.propGrab);
  if (!item || !game::alive(item)) return;
  void* t = game::transform_of(item);
  if (game::parent(t) == g.hand.get()) return;
  log_item("into the hand from", item);
  game::set_parent(t, g.hand, false);
  game::set_local_position(t, {0.0f, 0.0f, 0.0f});
  game::set_local_rotation(t, {0.0f, 0.0f, 0.0f, 1.0f});
  api::view::draw_as_seen(item);  // the camera slot's first-person layer would draw it over the body
  log_item("now in the hand", item);
  for (const api::Held& m : g.movedItems)
    if (m.get() == item) return;
  g.movedItems.emplace_back(item);
}

void begin_local_body(void* player, bool vr) {
  if (vr) {
    api::view::show_own_body(true);
    void* animator = animator_of(player);
    g.head = animator ? phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Head()) : nullptr;
    return;
  }
  api::view::begin_third_person();  // or out again, when a new emote starts while it eases back in
  if (void* look = component_in_rig(phasmo::PCLook::type.type_object())) {
    g.pcLook = look;
    game::set_enabled(look, false);  // the spine and head follow the dance, not the camera
  }
  // The held item hangs in front of the camera on the player's own client: in the hand while the view is outside.
  g.propGrab = component_in_rig(phasmo::PCPropGrab::type.type_object());
  void* anims = phasmo::Player::characterAnimations.get(player);
  g.hand = anims ? phasmo::PlayerCharacterAnimations::parentingTargetRightHand.get(anims) : nullptr;
  hold_items_in_hand();
}

void end_local_body() {
  api::view::end_third_person();
  api::view::show_own_body(false);
  if (g.pcLook && game::alive(g.pcLook)) game::set_enabled(g.pcLook, true);
  g.pcLook = nullptr;
  // Back in the camera's slot, as the game had them: each item still in the hand. One the game moved since (dropped, or
  // put away by a switch) is left where the game put it.
  for (api::Held& item : g.movedItems) {
    if (item && game::alive(item) && g.hand && game::alive(g.hand) && g.propGrab && game::alive(g.propGrab) &&
        game::parent(game::transform_of(item)) == g.hand.get()) {
      phasmo::PCPropGrab::ParentItemOnLocalPlayer(g.propGrab, item);
      log_item("back from the hand to", item);
    } else {
      log_item("left where the game put it", item);
    }
  }
  g.logItemAfterView = !g.movedItems.empty();
  g.movedItems.clear();
  g.hand = nullptr;
  g.propGrab = nullptr;
  if (g.head && game::alive(g.head)) game::set_local_scale(g.head, {1.0f, 1.0f, 1.0f});
  g.head = nullptr;
}

// The clip of an emote's phase: its intro, its looping clip, or its walking clip (any it lacks falls back to the loop).
void* clip_of(int emote, Phase phase) {
  if (phase == Phase::Intro && g.a.introClips[emote]) return g.a.introClips[emote];
  if (phase == Phase::Moving && g.a.moveClips[emote]) return g.a.moveClips[emote];
  return g.a.clips[emote];
}

// The phase an emote starts in: its intro when it has one.
Phase first_phase(int emote) { return g.a.table[emote].introSeconds > 0.0f ? Phase::Intro : Phase::Loop; }

// Plays `clip` on `actor`'s body unless it already plays there (a walking clip that is the clip itself plays on).
void play_clip(int actor, void* player, void* clip, float blend) {
  if (!player || !clip || g.playing[actor] == clip) return;
  api::anim::play_on_player(player, clip, blend);
  g.playing[actor] = clip;
}

void apply_remote(const std::vector<RemoteOut>& outs);

void apply_local(const LocalOut& o, void* player, bool vr) {
  const int me = game::local_actor();
  // The pair this client left: the other dancer stops on their own client, and here at once.
  if (o.partnerLeft) apply_remote(g.remote->drop(*o.partnerLeft));
  if (o.start >= 0) {
    // A second dancer plays the second part from the pair's point, to the first dancer's song (only theirs plays).
    const bool second = g.local->role() == Role::Partner;
    void* clip = second ? g.a.partnerClips[o.start].get() : clip_of(o.start, o.phase.value_or(first_phase(o.start)));
    const bool played =
        second ? api::anim::play_on_player(player, clip, kBlendIn,
                                           second_part_start(g.local->partner().value_or(0), o.start, g.local->partner_time()))
               : api::anim::play_on_player(player, clip, kBlendIn);
    if (!second) g.glide.reset();  // a dance of its own ends any glide into a pair
    g.playing[me] = clip;
    if (!second) start_song(me, player, o.start);
    const auto song = g.songs.find(me);
    void* songClip = g.a.songs[o.start];
    PM_LOG("Emotes: %s starts (%s): clip %s, %s on the body; song %s, %s", g.a.defs[o.start].id.c_str(), vr ? "VR" : "screen",
           clip && game::alive(clip) ? "loaded" : "gone", played ? "playing" : "not playing",
           songClip && game::alive(songClip) ? "loaded" : "gone",
           song != g.songs.end() && song->second.source ? "playing" : "no source");
    begin_local_body(player, vr);
    begin_facing(me, player, o.bodyYaw.value_or(0.0f));
  }
  // A walking emote's walk, a stop, or an intro's end: the phase's clip blends in over the one playing.
  if (o.phase && o.start < 0 && !o.stop && g.local->active() && g.local->role() == Role::First) {
    const int emote = g.local->emote();
    const float blend = phase_blend_seconds(*o.phase, g.playing[me] == g.a.introClips[emote]);
    play_clip(me, player, clip_of(emote, *o.phase), blend);
  }
  if (o.stop) {
    g.playing.erase(me);
    g.glide.reset();
    PM_LOG(o.atOnce ? "Emotes: the emote ends at once" : "Emotes: the emote ends");
    if (o.atOnce) {
      // The game has an animation of its own for this body (a death): nothing of ours is left blending, and the view
      // goes home instead of easing back (the user, 2026-11-10).
      release_body(me, player);
      end_local_body();
      api::view::stop_third_person();
    } else {
      api::anim::stop_on_player(player, kBlendOut);
      stop_song(me);
      end_local_body();
      end_facing(me);
    }
  }
  if (o.bodyYaw && !o.stop) {
    auto it = g.facings.find(me);
    if (it != g.facings.end()) it->second.bodyYaw = *o.bodyYaw;
  }
  send(o.send);
  for (const auto& [actor, m] : o.sendTo) {
    const std::vector<uint8_t> bytes = pack(m);
    net::send(bytes.data(), bytes.size(), net::To::Actor, true, actor);
  }
  // This client took a second dancer: it shows them dancing beside it from now, as every other client does.
  if (o.acceptedActor && g.local->active())
    apply_remote(g.remote->add_partner(*o.acceptedActor, me, g.local->emote(), g.local->elapsed(), g.local->yaw()));
}

// ---- Joining a synced emote ----

// Moves this client's investigator so their body stands at `to`: both of the game's moves set the rig's own position
// (LocalPlayer.TeleportPlayer on PC, VRPlayer.Teleport in VR, sdk/systems/players.md), so the rig goes by the gap
// between the body and the spot.
void move_body_to(void* player, Point to) {
  void* rig = game::local_player_component();
  if (!rig || !game::alive(rig) || !player) return;
  const Vector3 body = game::position(game::transform_of(player)), at = game::position(game::transform_of(rig));
  const Vector3 dest{at.x + to.x - body.x, at.y + to.y - body.y, at.z + to.z - body.z};
  if (is_vr(player))
    phasmo::VRPlayer::Teleport(player, &dest);
  else
    phasmo::LocalPlayer::TeleportPlayer(rig, &dest);
}

// Each frame of the glide into the partner's spot: the body moved along it and turned with it, until it is there.
void step_glide(void* player, float dt) {
  if (!g.glide) return;
  if (!g.local->active() || !player || !game::alive(player)) {
    g.glide.reset();
    return;
  }
  g.glide->t += dt;
  move_body_to(player, g.glide->at());
  if (auto it = g.facings.find(game::local_actor()); it != g.facings.end()) it->second.bodyYaw = g.glide->yaw();
  if (g.glide->done()) g.glide.reset();
}

// Whether the game's own Interact would act on something now: its ray from the centre of the rig's camera, on its
// mask, hitting a PhotonObjectInteract or a ChildPhotonInteract (PCPropGrab.OnInteract, sdk/systems/players.md).
// True when that cannot be read, so the prompt fails closed.
bool interactable_ahead() {
  void* grab = component_in_rig(phasmo::PCPropGrab::type.type_object());
  void* cam = grab ? phasmo::PCPropGrab::playerCam.get(grab) : nullptr;
  void* t = cam && game::alive(cam) ? game::transform_of(cam) : nullptr;
  if (!t) return true;
  void* hit = game::raycast(game::position(t), game::forward(t), kInteractReach, phasmo::PCPropGrab::mask.get(grab));
  if (!hit) return false;
  for (void* type : {phasmo::PhotonObjectInteract::type.type_object(), phasmo::ChildPhotonInteract::type.type_object()})
    if (type && phasmo::Component::GetComponent(hit, type)) return true;
  return false;
}

// The rig's own capsule (PhysicsCharacterController: its radius, height and centre, and the layers that block and
// carry a player, sdk/systems/players.md), placed from where the investigator's body stands. None, logged once, when
// the controller cannot be read: then no join is offered.
struct Capsule {
  void* ctrlT = nullptr;  // the controller's Transform: the investigator's own colliders are under it or the body's
  void* bodyT = nullptr;
  int32_t mask = 0, ground = 0;
  float r = 0.0f, h = 0.0f;
  Vector3 centre{};  // from where the body stands to the capsule's centre
};

std::optional<Capsule> own_capsule(void* player) {
  void* rig = game::local_player_component();
  void* ctrl = rig && game::alive(rig) ? phasmo::LocalPlayer::characterController.get(rig) : nullptr;
  if (!ctrl || !game::alive(ctrl) || !player) {
    if (!g.loggedNoCapsule) PM_LOG("Emotes: the rig's character controller cannot be read; no join prompt");
    g.loggedNoCapsule = true;
    return std::nullopt;
  }
  using C = phasmo::PhysicsCharacterController;
  Capsule k;
  k.ctrlT = game::transform_of(ctrl);
  k.bodyT = game::transform_of(player);
  k.mask = C::collisionMask.get(ctrl);
  k.ground = C::get_groundLayers(ctrl);
  k.r = C::get_radius(ctrl);
  k.h = C::get_height(ctrl);
  if (k.r <= 0.0f || k.h <= 0.0f || !k.mask) return std::nullopt;
  const Vector3 body = game::position(k.bodyT), at = game::position(k.ctrlT), c = C::get_center(ctrl);
  k.centre = {at.x - body.x + c.x, at.y - body.y + c.y, at.z - body.z + c.z};
  return k;
}

Vector3 plus(Vector3 p, Vector3 v) { return {p.x + v.x, p.y + v.y, p.z + v.z}; }
Vector3 at_point(Point p) { return {p.x, p.y, p.z}; }

// Whether nothing that blocks a player lies on the straight line between two points: the first thing the ray meets
// decides, and the investigator's own colliders, or the target's (a Transform, or null), do not count as in the way.
bool line_clear(Vector3 from, Vector3 to, const Capsule& k, void* target) {
  const Vector3 d{to.x - from.x, to.y - from.y, to.z - from.z};
  const float length = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
  if (length < 0.001f) return true;
  void* hit = game::raycast(from, {d.x / length, d.y / length, d.z / length}, length, k.mask);
  return !hit || game::is_child_of(hit, k.ctrlT) || game::is_child_of(hit, k.bodyT) ||
         (target && game::is_child_of(hit, target));
}

// Whether this client's investigator fits at `spot` (where the body would stand), on ground: the capsule tested there
// against what blocks a player, its own colliders aside, with no other investigator standing in it, and its ground
// layers under it. Another investigator is tested by where they stand, since the game's collision mask need not hold
// the players' layers.
bool spot_clear(Point spot, const Capsule& k) {
  const Vector3 base = plus(at_point(spot), k.centre);
  const float low = base.y - k.h / 2.0f + kFloorSkin + k.r, high = std::max(low, base.y + k.h / 2.0f - k.r);
  for (float y = low;; y += k.r) {
    const Vector3 centre{base.x, std::min(y, high), base.z};
    const api::RefArray hits{
        phasmo::Physics::OverlapSphere(&centre, k.r, k.mask, phasmo::QueryTriggerInteraction::Ignore())};
    for (size_t i = 0; i < hits.size(); ++i) {
      void* t = hits[i] ? game::transform_of(hits[i]) : nullptr;
      if (t && !game::is_child_of(t, k.ctrlT) && !game::is_child_of(t, k.bodyT)) return false;
    }
    if (y >= high) break;
  }
  bool someone = false;
  game::each_player([&](void* p) {
    if (someone || !p || !game::alive(p) || game::transform_of(p) == k.bodyT) return;
    const Vector3 q = game::position(game::transform_of(p));
    someone = std::hypot(q.x - spot.x, q.z - spot.z) < 2.0f * k.r && std::fabs(q.y - spot.y) < k.h;
  });
  if (someone) return false;
  const Vector3 feet{base.x, base.y - k.h / 2.0f + 0.2f, base.z};
  return game::raycast(feet, {0.0f, -1.0f, 0.0f}, 0.5f, k.ground) != nullptr;
}

// Whether the spot and the glide to it stay on this side of every wall, floor and ceiling: clear lines, at the capsule's
// centre height, from the first dancer to the spot and from this investigator to it (the review, 2026-11-13).
bool spot_reachable(Point spot, void* firstPlayer, const Capsule& k) {
  if (!firstPlayer || !game::alive(firstPlayer)) return false;
  void* firstT = game::transform_of(firstPlayer);
  const Vector3 to = plus(at_point(spot), k.centre);
  return line_clear(plus(game::position(firstT), k.centre), to, k, firstT) &&
         line_clear(plus(game::position(k.bodyT), k.centre), to, k, nullptr);
}

// Where a first dancer's second part stands, from their body now and the facing they keep.
std::optional<Point> spot_beside(int first, int emote) {
  void* p = game::player_of_actor(first);
  if (!p || !game::alive(p) || emote < 0 || !g.a.defs[emote].partner) return std::nullopt;
  const Vector3 at = game::position(game::transform_of(p));
  const PartnerDef& def = *g.a.defs[emote].partner;
  return partner_spot({at.x, at.y, at.z}, g.remote->yaw(first), def.offsetRight, def.offsetForward);
}

int index_of_key(uint32_t key) {
  for (size_t i = 0; i < g.a.table.size(); ++i)
    if (g.a.table[i].key == key) return static_cast<int>(i);
  return -1;
}

// The join prompt over the nearest dancer of a synced emote this client may join now, and a press of Interact (the
// right trigger in VR) to ask (the user's design, 2026-11-13). Each client draws only its own prompt.
void update_join(void* player, bool vr, const LocalFrame& f) {
  const bool canDance = f.present && !f.dying && !g.local->active() && !g.local->waiting() && !wheel::is_open() &&
                        !g.wheelIn.open && !journal_open() && keys::keys_ready();
  void* cam = canDance ? game::local_camera() : nullptr;
  void* camT = cam && game::alive(cam) ? game::transform_of(cam) : nullptr;
  const std::optional<Capsule> k = camT ? own_capsule(player) : std::nullopt;
  if (!k) return join::hide();
  const Vector3 eye = game::position(camT), look = game::forward(camT);
  PromptCheck c;
  c.me = {eye.x, eye.y, eye.z};
  c.lookYaw = std::atan2(look.x, look.z) * 57.29577951f;
  c.lookPitch = std::asin(std::clamp(look.y, -1.0f, 1.0f)) * 57.29577951f;
  c.canDance = true;
  c.targetSynced = true;
  c.spotClear = true;  // tested below for the one dancer the prompt would show over
  c.interactableInWay = !vr && interactable_ahead();
  const bool meDown = f.dead || f.dying;
  int best = 0, bestEmote = -1;
  float bestDistance = 0.0f;
  void* bestHead = nullptr;
  for (const auto& [actor, emote] : g.remote->dancers()) {
    if (!g.a.table[emote].synced || g.remote->partner_of(actor) != 0) continue;  // a second dancer is no one's first
    void* p = game::player_of_actor(actor);
    if (!p || !game::alive(p)) continue;
    // Alive or dead as the dancer is: the two must be seen by the same players.
    if ((phasmo::Player::isDead.get(p) || g.dying.count(actor) != 0) != meDown) continue;
    void* animator = animator_of(p);
    void* head = animator ? phasmo::Animator::GetBoneTransform(animator, phasmo::HumanBodyBones::Head()) : nullptr;
    if (!head || !game::alive(head)) continue;
    const Vector3 h = game::position(head);
    c.target = {h.x, h.y - kChestBelowHead, h.z};
    c.partFree = !g.remote->paired(actor);
    c.targetInIntro = g.remote->in_intro(actor);
    if (!shows_prompt(c)) continue;
    // Seen, not through a wall, a floor or a ceiling: the first thing between the eye and the dancer's chest is them.
    if (!line_clear(eye, at_point(c.target), *k, game::transform_of(p))) continue;
    const float d = game::distance(eye, {c.target.x, c.target.y, c.target.z});
    if (best && d >= bestDistance) continue;
    best = actor;
    bestEmote = emote;
    bestDistance = d;
    bestHead = head;
  }
  const std::optional<Point> spot = best ? spot_beside(best, bestEmote) : std::nullopt;
  if (!spot || !spot_clear(*spot, *k) || !spot_reachable(*spot, game::player_of_actor(best), *k)) return join::hide();
  std::string pad, key;
  read_interact_binding(pad, key);
  // In VR the glyph is the game's sprite for the trigger that joins there, as the VR wheel shows the game's shoulder
  // glyphs (the user's choice for the wheel, 2026-11-13); on screen the pad's sprite while the game says a pad is in
  // use, else the keycap with the key's name.
  void* sprite = vr ? pad_sprite("rightTrigger") : !input::keyboard_in_use().value_or(true) ? pad_sprite(pad) : nullptr;
  join::show(bestHead, camT, sprite, key);
  const bool pressed = vr ? keys::key_state(kVrPickKey).pressed
                          : g.interactAction && phasmo::InputAction::WasPressedThisFrame(g.interactAction);
  if (!pressed) return;
  join::hide();
  PM_LOG("Emotes: asks actor %d to dance %s together", best, g.a.defs[bestEmote].id.c_str());
  apply_local(g.local->join_request(best, bestEmote, f), player, vr);
}

bool in_player_list(void* player);

// This client's investigator as the session judges a join: in the player list, dead, or dying.
LocalFrame standing_of(void* player) {
  LocalFrame f;
  f.present = player && game::alive(player) && in_player_list(player);
  f.dead = f.present && phasmo::Player::isDead.get(player);
  f.dying = g.dying.count(game::local_actor()) != 0;
  return f;
}

// The first dancer's Accept named this client: it takes the second part and glides to the spot beside them. An
// Accept it no longer waits for is answered with an End (LocalSession::accepted).
void accept_from(int first, const EmoteMsg& m) {
  void* player = game::local_player();
  if (!player || !game::alive(player)) return;
  const bool vr = is_vr(player);
  const int emote = index_of_key(m.key);
  const std::optional<Point> spot = spot_beside(first, emote);
  const float yaw = g.remote->yaw(first);
  const double elapsed = g.remote->elapsed(first);
  const Vector3 from = game::position(game::transform_of(player));
  const float fromYaw = yaw_of(game::transform_of(player));
  // The answer comes up to kJoinWaitSeconds after the press: this investigator, or someone else, may have moved since,
  // so the spot and the way to it are checked again, and a join that no longer fits is called off as one this client
  // cannot dance (an End, which frees the first dancer as either stopping does).
  LocalFrame standing = standing_of(player);
  const std::optional<Capsule> k = own_capsule(player);
  if (g.local->waiting() &&
      !(spot && k && spot_clear(*spot, *k) && spot_reachable(*spot, game::player_of_actor(first), *k))) {
    PM_LOG("Emotes: the spot beside actor %d is no longer free or reachable; the join is called off", first);
    standing.present = false;
  }
  const LocalOut o =
      g.local->accepted(first, m.key, elapsed, spot.value_or(Point{from.x, from.y, from.z}), yaw, standing);
  if (o.start >= 0 && spot) {
    Glide glide;
    glide.from = {from.x, from.y, from.z};
    glide.to = *spot;
    glide.fromYaw = fromYaw;
    glide.toYaw = yaw;
    g.glide = glide;
    PM_LOG("Emotes: joins actor %d in %s", first, g.a.defs[o.start].id.c_str());
  }
  apply_local(o, player, vr);
  if (o.start >= 0) {
    if (auto it = g.facings.find(game::local_actor()); it != g.facings.end() && g.glide) it->second.bodyYaw = fromYaw;
    switch_to_synced_song(first, o.start, elapsed);
  }
}

// When this client's partner is no longer dancing here (their End, a new dance of theirs, leaving, a death), this
// client stops too: either dancer stopping ends both (the user, 2026-11-13).
void end_with_partner() {
  const std::optional<int> partner = g.local->partner();
  if (!partner || g.remote->dancing(*partner)) return;
  void* player = game::local_player();
  apply_local(g.local->partner_ended(*partner), player && game::alive(player) ? player : nullptr,
              player && is_vr(player));
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
        g.page = pages::clamp_page(g.page, static_cast<int>(g.a.defs.size()));
        wheel::set_page(g.page);
        void* left = phasmo::VRPlayer::leftHandIK.get(player);
        g.vrWheel = wheel::open_vr(left);
        PM_LOG(g.vrWheel ? "Emotes: the VR wheel opens" : "Emotes: the VR wheel could not be built");
      }
      return std::nullopt;
    }
    void* cam = game::local_camera();
    wheel::face(cam ? game::transform_of(cam) : nullptr);
    // The VR page targets show the game's shoulder glyphs (the user's choice, 2026-11-13, until a VR glyph of their
    // own): the game shows no glyph in VR, so these are the sprites its widgets hold.
    wheel::show_glyphs(glyph_of(g.previousGlyph), glyph_of(g.nextGlyph), false);
    void* right = phasmo::VRPlayer::rightHandIK.get(player);
    const int pointed = wheel::pointed(right);
    wheel::show_pick(pointed);
    // A page glyph pointed at turns the page on the trigger (the wheel has no free VR button for it).
    if (const int glyph = wheel::pointed_glyph(right); glyph != 0 && keys::key_state(kVrPickKey).pressed) {
      turn_page(glyph);
      return std::nullopt;
    }
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
  // On screen: the journal or the pause screen (one UI, the game's JournalController.open) closes it with nothing played.
  const bool interrupted = journal_open();
  const bool wasOpen = g.wheelIn.open;
  int pickNow = -1;
  if (wasOpen) {
    float dx = 0.0f, dy = 0.0f;
    if (game::mouse_delta(dx, dy)) add_capped(g.mouseX, g.mouseY, dx, dy);
    pickNow = segment_for(g.mouseX, g.mouseY, kPickDeadZone);
    if (void* pad = phasmo::Gamepad::get_current()) {
      const phasmo::Vector2 stick = phasmo::Vector2Control::ReadValue(phasmo::Gamepad::get_rightStick(pad));
      if (const int s = segment_for(stick.x, stick.y, kStickDeadZone); s >= 0) pickNow = s;
      void* lb = phasmo::Gamepad::get_leftShoulder(pad);
      void* rb = phasmo::Gamepad::get_rightShoulder(pad);
      turn_page((rb && phasmo::ButtonControl::get_wasPressedThisFrame(rb) ? 1 : 0) -
                (lb && phasmo::ButtonControl::get_wasPressedThisFrame(lb) ? 1 : 0));
    }
    if (void* mouse = phasmo::Mouse::get_current())
      if (void* scroll = phasmo::Mouse::get_scroll(mouse))
        turn_page(pages::scroll_step(phasmo::Vector2Control::ReadValue(scroll).y));
  }
  const std::optional<int> played = wheel_step(g.wheelIn, key.held, key.released, interrupted, pickNow);
  if (g.wheelIn.open && !wasOpen) {
    g.mouseX = g.mouseY = 0.0f;
    g.page = pages::clamp_page(g.page, static_cast<int>(g.a.defs.size()));
    wheel::set_page(g.page);
    if (!wheel::open_screen()) {
      PM_LOG("Emotes: the wheel could not be built on screen");
      g.wheelIn = WheelInput{};
    } else {
      PM_LOG("Emotes: the wheel opens (icons %d of %zu, segments %d of %d)", wheel::icons_alive(), g.a.defs.size(),
             wheel::segments_alive(), kSegments);
      set_look(false);
    }
  }
  if (g.wheelIn.open) {
    wheel::show_pick(g.wheelIn.picked);
    show_screen_glyphs();
  }
  if (!g.wheelIn.open && wasOpen) {
    wheel::close();
    set_look(true);
  }
  return played;
}

// Ends this client's emote at once, with nothing left blending: the game's animation takes the body back from its very
// first frame. Both times this is needed the game has an animation of its own to show (the user's ruling,
// 2026-11-10): the journal opening, where ending a frame later left the book in front of the face (the seventh in-game
// run), and death, where the clip still blending out lay over the death animation and hid it (the eighth).
void end_at_once(const char* why) {
  if (!g.local || !g.local->active()) return;
  void* player = g.localPlayer.get();
  if (player && !game::alive(player)) player = nullptr;
  const LocalOut o = g.local->end_now();
  PM_LOG("Emotes: %s: the emote ends at once", why);
  release_body(game::local_actor(), player);
  end_local_body();
  api::view::stop_third_person();
  send(o.send);
}

// The wheel shut, its input cleared and the look back on.
void close_wheel() {
  set_look(true);  // whatever closed it, even a canvas the scene already took: what was paused comes back
  if (!wheel::is_open()) return;
  wheel::close();
  g.wheelIn = WheelInput{};
}

void detour_journal(api::Original<phasmo::LocalJournalController::OpenCloseJournal> original, void* input, bool flag) {
  end_at_once("the journal opens");
  original(input, flag);
}

// The start of a kill, which the game sends to every client (Player.NotifyOfDeathStart, sent by
// Player.BeginDeathSequence through the server). It marks the player dying and wakes their ragdoll; the game's mark of
// the death, Player.isDead, comes only when the death completes, a second or two later. So a dance has to let go of the
// body here, or the emote clip plays over the death animation and hides it (the user, 2026-11-10, in the eighth in-game
// run: killed while emoting, the death animation never showed and the view stayed outside).
void detour_death_start(api::Original<phasmo::Player::NotifyOfDeathStart> original, void* player, void* info) {
  original(player, info);  // the game's own first: it marks the dying player and puts their ragdoll in
  const int actor = actor_of(player);
  if (actor) g.dying.insert(actor);
  if (player == g.localPlayer.get()) {
    end_at_once("the investigator begins to die");
    close_wheel();  // a death takes the wheel away too: nothing may be picked while the body falls (the user, 2026-11-10)
  } else {
    release_body(actor, player);
  }
}

void end_everything_local() {
  join::hide();
  g.glide.reset();
  if (g.local && g.local->active()) {
    void* player = g.localPlayer.get();
    apply_local(g.local->end_now(), player && game::alive(player) ? player : nullptr, false);
  }
  close_wheel();
}

// ---- Everyone else ----

void apply_remote(const std::vector<RemoteOut>& outs) {
  for (const RemoteOut& o : outs) {
    void* player = game::player_of_actor(o.actor);
    switch (o.what) {
      case RemoteOut::Start:
        if (player && o.partner) {
          // A second dancer: the second part from the pair's point, no song of their own (only the first dancer's
          // plays), and the first dancer's song switched to the synced one if the emote has one.
          void* clip = g.a.partnerClips[o.emote];
          const int first = g.remote->partner_of(o.actor);
          api::anim::play_on_player(player, clip, kBlendIn,
                                    second_part_start(first, o.emote, g.remote->partner_time(o.actor)));
          g.playing[o.actor] = clip;
          begin_facing(o.actor, player, o.yaw);
          switch_to_synced_song(first, o.emote,
                                first == game::local_actor() ? g.local->elapsed() : g.remote->elapsed(first));
        } else if (player) {
          void* clip = clip_of(o.emote, first_phase(o.emote));
          api::anim::play_on_player(player, clip, kBlendIn);
          g.playing[o.actor] = clip;
          start_song(o.actor, player, o.emote);
          begin_facing(o.actor, player, o.yaw);
        }
        break;
      case RemoteOut::Phase:
        play_clip(o.actor, player, clip_of(o.emote, o.phase),
                  phase_blend_seconds(o.phase, g.playing[o.actor] == g.a.introClips[o.emote]));
        break;
      case RemoteOut::Stop:
        g.playing.erase(o.actor);
        if (o.atOnce) {
          // Their death animation is the game's own: their dance comes off at once rather than blending out over it
          // (the user, 2026-11-10).
          release_body(o.actor, player);
          break;
        }
        if (player) api::anim::stop_on_player(player, kBlendOut);
        stop_song(o.actor);
        end_facing(o.actor);
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

// The song's loudness where it is now, from the emote's own clock (its clips may switch; its song plays on).
void raise_loudness(int actor, void* player, int emote, double elapsed, float now) {
  if (!player || emote < 0 || !g.paces[actor].due(now)) return;
  const EmoteInfo& info = g.a.table[emote];
  const SongAt at = song_position(elapsed, info.introSongSeconds, info.song);
  const bool synced = g.syncedSongs.count(actor) != 0;
  const talk::Envelope& envelope =
      at.intro ? g.a.introLoudness[emote] : synced ? g.a.partnerLoudness[emote] : g.a.loudness[emote];
  talk::add_talk_loudness(player, envelope.at(static_cast<float>(at.seconds)));
}

}  // namespace

// A start that fails lets go of whatever it took (the bundles above all: the engine loads a bundle once, so a later
// start of the mods could not load them again), as stop() does; the host never calls stop() after a failed start.
bool fail_start() {
  stop();
  return false;
}

bool start() {
  if (!load_catalogues()) return fail_start();
  // In the mod's data folder; with none (it could not be made, logged), nothing is saved.
  const std::filesystem::path data = mod::data_folder();
  keys::begin_keys(mod::info().id, mod::shown_name(), data.empty() ? data : data / "keybinds.txt");
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
  // The player's Emotes volume, on the journal's audio page (the user, 2026-11-10).
  api::settings::begin_settings(mod::info().id, mod::shown_name(), data.empty() ? data : data / "settings.txt");
  api::settings::Slider volume;
  volume.id = kVolumeSetting;
  volume.label = i18n::text(kVolumeLabel);
  api::settings::add_slider(volume);
  g.local.emplace(g.a.table);
  g.remote.emplace(g.a.table);
  if (!join::load()) PM_LOG("Emotes: the join prompt shows no keycap");
  if (!api::hook<phasmo::LocalJournalController::OpenCloseJournal>(&detour_journal, "Emotes: the journal key") ||
      !api::hook<phasmo::Player::NotifyOfDeathStart>(&detour_death_start, "Emotes: a kill begins") ||
      !pm::hooks::enable_all()) {
    PM_LOG("Emotes inactive: the journal key and the death's start could not be hooked");
    return fail_start();
  }
  g.lastTick = g.startTick = GetTickCount64();
  return true;
}

void tick() {
  const ULONGLONG nowMs = GetTickCount64();
  const float dt = std::min(0.1f, static_cast<float>(nowMs - g.lastTick) / 1000.0f);
  g.lastTick = nowMs;
  const float now = static_cast<float>(nowMs - g.startTick) / 1000.0f;
  keys::tick_keys();
  api::settings::tick_settings();
  follow_settings();
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
  // A death that has completed, or a player gone from the list, leaves the dying set: the game's own rules have that
  // body from then on, and a player whose death is over may dance again.
  for (auto it = g.dying.begin(); it != g.dying.end();) {
    void* p = game::player_of_actor(*it);
    if (!p || !game::alive(p) || phasmo::Player::isDead.get(p))
      it = g.dying.erase(it);
    else
      ++it;
  }
  LocalFrame f;
  f.present = present;
  if (present) {
    f.dead = phasmo::Player::isDead.get(player);
    f.dying = g.dying.count(game::local_actor()) != 0;
    const Vector3 p = game::position(game::transform_of(player));
    f.pos = {p.x, p.y, p.z};
    f.viewYaw = api::view::view_yaw();
    f.clipTime = api::anim::clip_time(player);
    f.dt = dt;
    // The game's own Move input on the PC rig (FirstPersonController.moveInput): W or the move stick, rebinding applied.
    // The VR rig moves without it, so a VR walking emote acts as a standing one.
    void* rig = game::local_player_component();
    if (!vr && rig && phasmo::LocalPCPlayer::type.is(rig))
      if (void* mover = phasmo::LocalPCPlayer::firstPersonController.get(rig); mover && game::alive(mover)) {
        const phasmo::Vector2 move = phasmo::FirstPersonController::moveInput.get(mover);
        f.moveX = move.x;
        f.moveY = move.y;
      }
    // Sprinting as the game decides it (pm::api::movement::sprinting: Run held or toggled on, sprinting allowed and
    // the stamina able); nothing on VR, where a walking emote stands anyway.
    f.sprinting = movement::sprinting().value_or(false);
    f.gliding = g.glide.has_value();
    const bool dancing = g.local->active();
    f.endAction = dancing && ((end_action_pressed(vr) && !wheel::is_open()) || journal_open());
  }
  // The beginning of a death: the game's death animation takes the body back from its first frame, so the emote ends at
  // once instead of blending out over it, the wheel goes away and nothing is picked while the body falls (the user's
  // ruling, 2026-11-10, after the eighth in-game run, where the death animation never showed). The kill's own hook does
  // this the moment the game sends it; this is the same for a build whose entry did not resolve, a frame late.
  if (present && f.dying) {
    end_at_once("the investigator begins to die");
    close_wheel();
  }
  // Nothing else of the joiner's starts during the glide into a pair (the design): the wheel stays shut until it ends.
  if (present && !f.dying && keys::keys_ready() && !g.glide) {
    if (const std::optional<int> played = wheel_input(player, vr))
      if (const int emote = wheel::emote_at(*played); emote >= 0) apply_local(g.local->pick(emote, f), player, vr);
  } else {
    close_wheel();
  }
  if (present && !f.dying)
    update_join(player, vr, f);
  else
    join::hide();
  apply_local(g.local->tick(f), player, vr);
  step_glide(player, dt);
  if (g.local->active() && !vr) hold_items_in_hand();
  if (g.logItemAfterView && !api::view::in_third_person()) {
    g.logItemAfterView = false;
    void* grab = component_in_rig(phasmo::PCPropGrab::type.type_object());
    log_item("in first person again", grab ? phasmo::PCPropGrab::GetItemInCameraSlot(grab) : nullptr);
  }
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
    frames.push_back({actor, p && game::alive(p), p && phasmo::Player::isDead.get(p), g.dying.count(actor) != 0,
                      p ? api::anim::clip_time(p) : -1.0});
  }
  apply_remote(g.remote->tick(frames, f.dead || f.dying, dt));
  end_with_partner();
  apply_facings(dt);

  // The ghost hears the songs as the dancers talking, on the master, where it runs.
  if (game::is_master()) {
    // One song plays for a pair, the first dancer's, so only theirs counts.
    if (g.local->active() && g.local->role() == Role::First)
      raise_loudness(game::local_actor(), player, g.local->emote(), g.local->elapsed(), now);
    for (const auto& [actor, emote] : g.remote->dancers())
      if (g.remote->partner_of(actor) == 0)
        raise_loudness(actor, game::player_of_actor(actor), emote, g.remote->elapsed(actor), now);
  }
}

void receive(int sender, const uint8_t* data, size_t size) {
  if (!g.remote) return;
  const std::optional<EmoteMsg> m = unpack({data, size});
  if (!m) return;
  void* player = game::player_of_actor(sender);
  if (!player && m->kind == Kind::Start) return;
  void* local = game::local_player();
  const bool senderDown = player && (phasmo::Player::isDead.get(player) || g.dying.count(sender) != 0);
  const bool localDown = local && (phasmo::Player::isDead.get(local) || g.dying.count(game::local_actor()) != 0);
  // Joining: a Join is this client's to answer as a first dancer; an Accept naming this client is its answer as a
  // joiner (and goes on to the pairing below, as every client's does).
  if (m->kind == Kind::Join) {
    if (local && game::alive(local)) apply_local(g.local->on_join(sender, m->key), local, is_vr(local));
    return;
  }
  if (m->kind == Kind::Accept && static_cast<int>(m->x) == game::local_actor()) accept_from(sender, *m);
  // A new dance of this client's partner leaves the pair, which ends it for both.
  if (m->kind == Kind::Start && g.local->partner() == sender) {
    apply_local(g.local->partner_ended(sender), local && game::alive(local) ? local : nullptr, local && is_vr(local));
  }
  apply_remote(g.remote->receive(sender, *m, senderDown, localDown));
  end_with_partner();
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
  while (!g.songs.empty()) stop_song(g.songs.begin()->first);  // each song's sources, its intro's too
  set_look(true);  // the game's actions the open wheel paused come back before the mod lets go of them
  wheel::unload();
  join::unload();
  // The engine loads a bundle once, so each is unloaded here for the mod's next start (the lobby's rules restart mods).
  for (api::Held& b : g.a.bundles)
    if (b && game::alive(b)) api::unload_bundle(b, true);
  keys::clear_keys();
  api::settings::clear_settings();
  g = State{};
}

}  // namespace pm::emotes::play
