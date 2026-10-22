#include "control.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>
#include <utility>

#include "custom_link.h"
#include "form.h"
#include "marks.h"
#include "mimic.h"
#include "perceive_view.h"
#include "typestate.h"
#include "typestate_host.h"
#include "actions.h"
#include "bindings.h"
#include "body.h"
#include "game.h"
#include "ghostbook.h"
#include "hud.h"
#include "journal.h"
#include "leave.h"
#include "log.h"
#include "panorama.h"
#include "picker.h"
#include "round.h"
#include "steer.h"
#include "tether.h"

namespace pg::control {
namespace {

using il2cpp::Quaternion;
using il2cpp::Vector3;

constexpr ULONGLONG kInputSendMs = 50;       // ghost player input rate (20 Hz)
constexpr ULONGLONG kKeysRefreshMs = 3000;   // pick up rebinds made in the pause menu
constexpr ULONGLONG kTapMs = 400;            // Primary Use released sooner: manifest
constexpr ULONGLONG kHoldMs = 1000;          // Primary Use held this long: hunt
constexpr float kAimReach = 4.0f;
constexpr float kLookDegreesPerPixel = 0.1f;  // mouse look sensitivity
constexpr float kMaxPitch = 85.0f;
constexpr float kDegrees = 3.14159265f / 180.0f;
constexpr ULONGLONG kCoverMs = 30000;  // longest the screen stays black waiting for every player to load

// The ghost's controls, on the game's own actions so the player's bindings apply.
struct Controls {
  bindings::Key up, down, left, right, interact, primary, secondary, crouch, pause, journal, fuse;
  ULONGLONG refreshAt = 0;
};

// Ghost player side.
bool g_controlling = false;
float g_yaw = 0, g_pitch = 0;  // degrees; the ghost player's view
Controls g_keys;
bool g_interactHeld = false, g_secondaryHeld = false, g_fuseHeld = false, g_huntFired = false;
bool g_crouchTap = false;  // this frame Crouch was let go without making a disguise
bool g_mimicKeyHeld = false;  // Crouch, for the Mimic (room_claim tracks its own press)
bool g_crouchUsed = false;    // this Crouch press went into a disguise, so it picks no Mimic copy
bool g_pauseHeld = false, g_journalHeld = false;
bool g_wasReading = false;  // the journal or picker had the cursor last frame
bool g_swallowClick = false;  // the click that closed it is not the ghost's until released
ULONGLONG g_primarySince = 0;
ULONGLONG g_nextInputSend = 0;

// Authority side.
il2cpp::Held g_steeredGhost;
int g_lastState = -1;
ULONGLONG g_nextStatus = 0;
ULONGLONG g_lastTick = 0;
tether::Reason g_lastReason = tether::kSteer;
il2cpp::Held g_reachState;  // the wander state object g_reach belongs to
tether::Reach g_reach;
bool g_atReachEdge = false;
tether::SearchPace g_search;

bool game_focused() {
  DWORD pid = 0;
  GetWindowThreadProcessId(GetForegroundWindow(), &pid);
  return pid == GetCurrentProcessId();
}

// Ghost input only while the game window has focus and no menu owns the cursor.
bool accepting_input() { return game_focused() && game::cursor_locked(); }

void refresh_keys(ULONGLONG now) {
  if (now < g_keys.refreshAt) return;
  void* lp = game::local_player_component();
  g_keys.up = bindings::for_action(lp, "Move", "up");
  g_keys.down = bindings::for_action(lp, "Move", "down");
  g_keys.left = bindings::for_action(lp, "Move", "left");
  g_keys.right = bindings::for_action(lp, "Move", "right");
  g_keys.interact = bindings::for_action(lp, "Interact");
  g_keys.primary = bindings::for_action(lp, "Primary Use");
  g_keys.secondary = bindings::for_action(lp, "Secondary Use");
  g_keys.crouch = bindings::for_action(lp, "Crouch");
  g_keys.pause = bindings::for_action(lp, "Pause");
  if (!g_keys.pause.vk) g_keys.pause = {VK_ESCAPE, "Esc"};
  g_keys.journal = bindings::for_action(lp, "Journal");
  if (!g_keys.journal.vk) g_keys.journal = {'J', "J"};
  // The fuse box is the ghost's power switch: the investigators' torch control.
  g_keys.fuse = bindings::for_action(lp, "Torch");
  if (!g_keys.fuse.vk) g_keys.fuse = {'T', "T"};
  g_keys.refreshAt = now + kKeysRefreshMs;
}

// World-space horizontal movement direction from the Move bindings, relative to the camera's
// facing; zero when no movement key is held.
void read_direction(void* camera, float& dx, float& dz) {
  dx = dz = 0;
  if (!accepting_input()) return;
  float f = static_cast<float>(bindings::down(g_keys.up)) - static_cast<float>(bindings::down(g_keys.down));
  float r = static_cast<float>(bindings::down(g_keys.right)) - static_cast<float>(bindings::down(g_keys.left));
  if (f == 0.0f && r == 0.0f) return;
  Vector3 fwd = game::forward(game::transform_of(camera));
  float len = std::sqrt(fwd.x * fwd.x + fwd.z * fwd.z);
  if (len < 1e-4f) return;
  const float fx = fwd.x / len, fz = fwd.z / len;  // right = (fz, -fx) in Unity's left-handed frame
  float x = fx * f + fz * r;
  float z = fz * f - fx * r;
  float m = std::sqrt(x * x + z * z);
  dx = x / m;
  dz = z / m;
}

void release_ghost_player() {
  picker::hide();  // the picker, journal and panorama come before control starts: close them regardless
  journal::close();
  panorama::hide();
  if (!g_controlling) return;
  hud::hide();
  perceive_view::hide();
  PG_LOG("control: ghost player released");
  g_controlling = false;
  g_keys = {};
  g_interactHeld = g_secondaryHeld = g_fuseHeld = g_huntFired = g_crouchTap = false;
  g_mimicKeyHeld = g_crouchUsed = false;
  g_pauseHeld = g_journalHeld = false;
  g_wasReading = g_swallowClick = false;
  g_primarySince = 0;
}

// The key went down this frame (focus already checked).
bool pressed(const bindings::Key& key, bool focused, bool& held) {
  const bool down = focused && bindings::down(key);
  const bool press = down && !held;
  held = down;
  return press;
}

constexpr uint8_t kMimicType = 20;

/** The Mimic's next copy: the next type it may copy, after the last one the game's own choice. */
void next_mimic_copy(session::Session& s) {
  const std::vector<uint8_t>& offer = s.mimic_offer();
  if (offer.empty()) return;
  const uint8_t current = s.status().mimicNext;
  size_t i = 0;
  while (i < offer.size() && offer[i] != current) ++i;
  const uint8_t next = current == session::kNoType ? offer[0] : i + 1 < offer.size() ? offer[i + 1] : session::kNoType;
  s.send_mimic_pick(next);
}

/** The ghost's own state, and a Mimic's copies, for the HUD. */
std::string senses_line(const session::Status& st) {
  const int type = st.type == session::kNoType ? -1 : st.type;
  const int identity = st.identity == session::kNoType ? -1 : st.identity;
  std::string line = typestate::describe(typestate::subject(type, identity), st.trait);
  if (identity == kMimicType && type >= 0) {
    if (!line.empty()) line += "\n";
    line += "Copying: <b>" + round::ghost_type_name(type) + "</b>. Next copy: <b>" +
            (st.mimicNext == session::kNoType ? std::string("the game's choice") : round::ghost_type_name(st.mimicNext)) +
            "</b> [" + g_keys.crouch.label + "]";
  }
  return line;
}

// Interact, Secondary Use and the torch (the fuse box) act on press; Primary Use manifests on a tap
// and hunts when held.
void read_actions(session::Session& s, const float aim[3], ULONGLONG now) {
  const bool on = accepting_input() && !g_swallowClick;
  const bool interact = on && bindings::down(g_keys.interact);
  const bool secondary = on && bindings::down(g_keys.secondary);
  const bool fuse = on && bindings::down(g_keys.fuse);
  const bool primary = on && bindings::down(g_keys.primary);
  if (interact && !g_interactHeld) s.send_trigger(actions::kInteract, aim);
  if (secondary && !g_secondaryHeld) s.send_trigger(actions::kAbility, aim);
  if (fuse && !g_fuseHeld) s.send_trigger(actions::kFuse, aim);
  // Crouch held while Primary Use is tapped makes the event a disguise. Let go without one, it is a
  // tap: it claims the favourite room while a claim is open (room_claim), else picks a Mimic's next copy.
  const bool crouch = on && bindings::down(g_keys.crouch);
  const bool claimOpen = s.room_offer().valid && now < s.room_offer().deadline;
  g_crouchTap = !crouch && g_mimicKeyHeld && !g_crouchUsed;
  if (g_crouchTap && s.status().identity == kMimicType && !claimOpen) next_mimic_copy(s);
  if (!crouch) g_crouchUsed = false;
  g_mimicKeyHeld = crouch;
  g_interactHeld = interact;
  g_secondaryHeld = secondary;
  g_fuseHeld = fuse;
  if (primary) {
    if (!g_primarySince) g_primarySince = now;
    if (!g_huntFired && now - g_primarySince >= kHoldMs) {
      s.send_trigger(actions::kHunt, aim);
      g_huntFired = true;
    }
  } else if (g_primarySince) {
    if (!g_huntFired && now - g_primarySince < kTapMs) {
      s.send_trigger(crouch ? actions::kDisguise : actions::kEvent, aim);
      if (crouch) g_crouchUsed = true;
    }
    g_primarySince = 0;
    g_huntFired = false;
  }
}

void release_authority() {
  if (!g_steeredGhost) return;
  steer::clear();
  actions::tick(nullptr);
  PG_LOG("control: ghost returned to native AI");
  g_steeredGhost = nullptr;
  typestate::forget();
  g_lastState = -1;
  g_lastTick = 0;
  g_lastReason = tether::kSteer;
  g_reachState = nullptr;
  g_atReachEdge = false;
  g_search = {};
}

std::string two_digits(unsigned v) { return (v < 10 ? "0" : "") + std::to_string(v); }

// Just after a native favourite-room change the ghost player may choose another favourite room: on
// their journal's map (the Journal key opens it, src/picker.h), or without a journal with Crouch, for
// the room the ghost is in. Returns the HUD prompt, or an empty string when there is nothing to claim.
std::string room_claim(session::Session& s, ULONGLONG now) {
  const session::RoomOffer& offer = s.room_offer();
  const bool press = std::exchange(g_crouchTap, false);  // read_actions' tap, this frame only
  if (!offer.valid || now >= offer.deadline) return {};
  const unsigned left = static_cast<unsigned>((offer.deadline - now + 999) / 1000);
  const std::string time = " (0:" + two_digits(left) + ")";
  if (journal::kept()) return "Your favourite room moved: [" + g_keys.journal.label + "] choose another on your map" + time;
  const int here = round::current_ghost_room();
  const bool claimable = here >= 0 && here != round::favourite_room();
  if (press && claimable) s.send_room_claim(here);
  if (!claimable) return "Your favourite room moved: go to another room to choose it instead" + time;
  return "Your favourite room moved: [" + g_keys.crouch.label + "] make " + round::room_name(here) +
         " your favourite room instead" + time;
}

// Why the ghost moves on its own or holds still, as the ghost would feel it.
std::string movement_notice(const session::Status& st) {
  if (st.flags & session::kBlinded) return "The smoke blinds you";
  if (st.flags & session::kDrawnHome) return "Your favourite room draws you back";
  if (st.more & session::kRestlessActing) return "Restless, the ghost acts on its own until you try to act";
  if (st.more & session::kRoamEdge) return "You may not stray further this time";
  if (st.flags & session::kBound) return "You are bound to your favourite room";
  if (st.more & session::kResting) return "You rest until the urge to wander comes";
  return {};
}

// What the host reports about the ghost (the host as ghost reads it directly).
/** Host: the ghost's status as the ghost player sees it (sent to them, or shown when the host is them). */
session::Status host_status() {
  session::Status st = actions::status();
  st.room = static_cast<int16_t>(round::favourite_room());
  if (void* ghost = game::ghost_ai(); ghost && game::alive(ghost))
    typestate::read(ghost, st.type == session::kNoType ? -1 : st.type, st.identity == session::kNoType ? -1 : st.identity, st.trait);
  st.mimicNext = mimic::next();
  if (g_atReachEdge) st.more |= session::kRoamEdge;
  return st;
}

session::Status ghost_status(session::Session& s) { return s.local_is_master() ? host_status() : s.status(); }

// The ghost player's own journal pages, from their bindings and the ghost's state.
/** What the ghost is, by name (a custom ghost and a Mimic by what they are, not what they act as). */
std::string type_name(const session::Status& st) {
  const uint8_t type = session::named_type(st);
  return type != session::kNoType ? round::ghost_type_name(type) : std::string();
}

const ability::Ability* own_ability(const session::Status& st) {
  return custom_link::own_ability(st.type == session::kNoType ? -1 : st.type,
                                  st.identity == session::kNoType ? -1 : st.identity);
}

void write_journal(const session::Status& st) {
  ghostbook::Facts f;
  f.up = g_keys.up.label;
  f.left = g_keys.left.label;
  f.down = g_keys.down.label;
  f.right = g_keys.right.label;
  f.interact = g_keys.interact.label;
  f.primary = g_keys.primary.label;
  f.secondary = g_keys.secondary.label;
  f.fuse = g_keys.fuse.label;
  f.journal = g_keys.journal.label;
  f.pause = g_keys.pause.label;
  f.type = type_name(st);
  if (st.room >= 0) f.room = round::room_name(st.room);
  f.status = st;
  f.ability = own_ability(st);
  journal::write(ghostbook::write(f));
}

// Unity's Quaternion.Euler(pitch, yaw, 0): yaw about Y, then pitch about X.
Quaternion view_rotation(float yawDegrees, float pitchDegrees) {
  const float hy = yawDegrees * kDegrees * 0.5f, hp = pitchDegrees * kDegrees * 0.5f;
  const float sy = std::sin(hy), cy = std::cos(hy), sp = std::sin(hp), cp = std::cos(hp);
  return {cy * sp, sy * cp, -sy * sp, cy * cp};
}

// The ghost player's eyes are their own camera (the investigator camera, detached when they stepped
// out of their investigator): it sits at the ghost's head and turns with the mouse.
void look_from_ghost(void* camera, void* ghost) {
  void* t = game::transform_of(camera);
  if (!g_controlling) {
    const Vector3 f = game::forward(t);
    g_yaw = std::atan2(f.x, f.z) / kDegrees;
    g_pitch = -std::asin(std::clamp(f.y, -1.0f, 1.0f)) / kDegrees;
    g_controlling = true;
    PG_LOG("control: playing as the ghost");
  }
  float mx = 0, my = 0;
  if (accepting_input() && game::mouse_delta(mx, my)) {
    g_yaw += mx * kLookDegreesPerPixel;
    g_pitch = std::clamp(g_pitch - my * kLookDegreesPerPixel, -kMaxPitch, kMaxPitch);
  }
  const Vector3 p = game::position(game::transform_of(ghost));
  game::set_position(t, {p.x, p.y + game::kGhostEyeHeight, p.z});
  game::set_rotation(t, view_rotation(g_yaw, g_pitch));
}

// Ghost player side. Outputs this frame's movement for the host-as-ghost case.
void ghost_player(session::Session& s, void* ghost, float& dx, float& dz) {
  void* camera = body::camera();
  if (!camera) {  // the investigator has not left the contract yet
    release_ghost_player();
    return;
  }
  const ULONGLONG now = GetTickCount64();
  refresh_keys(now);
  const bool focused = game_focused();
  picker::Input in;
  in.focused = focused;
  in.pausePressed = pressed(g_keys.pause, focused, g_pauseHeld);
  in.journalPressed = pressed(g_keys.journal, focused, g_journalHeld);
  const picker::Keys pickKeys{g_keys.up, g_keys.down, g_keys.left, g_keys.right, g_keys.interact};
  const bool picking = picker::tick(s, pickKeys, in);  // the picker manages the cursor itself
  if (!picking && journal::kept()) {  // the journal and its pause menu, as an investigator opens them
    if (in.pausePressed) journal::is_open() ? journal::close() : journal::open(true);
    else if (in.journalPressed) journal::is_open() ? journal::close() : journal::open(false);
  }
  journal::tick(camera);  // after any opening this frame, so the book is in front of the camera at once
  const bool reading = picking || journal::is_open();
  const session::Status status = ghost_status(s);
  if (journal::is_open()) write_journal(status);
  if (g_wasReading && !reading) g_swallowClick = true;  // e.g. the pause menu's Resume
  g_wasReading = reading;
  if (g_swallowClick && !game::mouse_left_held()) g_swallowClick = false;
  if (!picking && focused) reading ? game::free_cursor() : game::lock_cursor();  // no investigator rig keeps it for us
  if (reading && !s.local_is_master() && now >= g_nextInputSend) {
    s.send_input(0, 0);  // the ghost holds still while its player reads their journal
    g_nextInputSend = now + kInputSendMs;
  }
  if (!ghost) {
    hud::hide();  // it has not appeared yet
    if (!picking) panorama::show(camera, -1);  // not the truck while waiting for the picker or the ghost
    return;
  }
  panorama::hide();
  look_from_ghost(camera, ghost);  // no mouse look while the cursor is free
  const std::string alert = perceive_view::tick(s, camera, ghost, !reading);
  if (reading) {
    hud::hide();
    return;
  }
  read_direction(camera, dx, dz);
  if (!s.local_is_master() && now >= g_nextInputSend) {
    s.send_input(dx, dz);  // zero direction is sent too: it means "hold still"
    g_nextInputSend = now + kInputSendMs;
  }

  const Vector3 view = game::forward(game::transform_of(camera));
  const float aim[3] = {view.x, view.y, view.z};
  read_actions(s, aim, now);

  // What the ghost is looking at, from its own eyes (the replicated ghost on this client).
  const Vector3 p = game::position(game::transform_of(ghost));
  void* hit = game::raycast({p.x, p.y + game::kGhostEyeHeight, p.z}, view, kAimReach);
  hud::View v;
  v.status = status;
  v.ghostType = type_name(v.status);
  v.ability = own_ability(v.status);
  v.senses = senses_line(v.status);
  if (v.status.room >= 0) v.favouriteRoom = round::room_name(v.status.room);
  v.aimed = game::interactable_label(hit);
  v.interactKey = g_keys.interact.label;
  v.primaryKey = g_keys.primary.label;
  v.crouchKey = g_keys.crouch.label;
  v.secondaryKey = g_keys.secondary.label;
  v.fuseKey = g_keys.fuse.label;
  v.alert = alert;
  v.notice = room_claim(s, now);
  if (v.notice.empty()) v.notice = movement_notice(status);
  hud::show(v);
}

// The hunt state (obfuscated name, build 25811160): +0x48 the player it chases, +0x50 the player it
// lost sight of and searches for (src/tether.h SearchPace).
constexpr const char* kHuntStateClass = "ധജഫനഴയഭഥഷ";
constexpr size_t kHuntTarget = 0x48;
constexpr size_t kHuntSearched = 0x50;
// The wander states (1 and 21, same layout): +0x24 radius (int), +0x28 has a destination, +0x2C it.
constexpr size_t kWanderRadius = 0x24;
constexpr size_t kWanderHasDestination = 0x28;
constexpr size_t kWanderDestination = 0x2C;

// A vanilla ghost's search ends once it has walked to the search point; a ghost player walking
// elsewhere has their search end where they are after the same walk (tether::SearchPace).
void pace_search(void* ghost, void* agent, float dt) {
  void* hunt = game::ghost_state_object(ghost);
  const bool searching = hunt && std::strcmp(il2cpp::class_name(il2cpp::class_of(hunt)), kHuntStateClass) == 0 &&
                         !game::alive(il2cpp::field<void*>(hunt, kHuntTarget)) &&
                         game::alive(il2cpp::field<void*>(hunt, kHuntSearched));
  const Vector3 p = game::position(game::transform_of(ghost));
  const Vector3 d = game::agent_destination(agent);
  if (g_search.tick(searching, p.x, p.z, d.x, d.z, game::agent_path_length(agent), game::agent_speed(agent), dt)) {
    game::set_destination(agent, p);  // the native arrival check (closet search, re-acquire or give up) runs here
    PG_LOG("control: hunt search ends where the ghost is, as a vanilla walk there would have");
  }
}

// How far the ghost player may walk in this wander (tether::Reach), learned from the wander state
// the first frame it runs.
void roam(void* ghost, float& dx, float& dz) {
  void* wander = game::ghost_state_object(ghost);
  const Vector3 p = game::position(game::transform_of(ghost));
  if (wander && wander != g_reachState) {
    g_reachState = wander;
    const Vector3 dest = il2cpp::field<Vector3>(wander, kWanderDestination);
    g_reach = tether::reach_of(p.x, p.z, il2cpp::field<int>(wander, kWanderRadius),
                               il2cpp::field<bool>(wander, kWanderHasDestination), dest.x, dest.z);
    PG_LOG("control: wander reach %.1f m", g_reach.radius);
  }
  if (!g_reachState) return;  // no wander state to read: its native walk is not limited further
  const float before = dx * dx + dz * dz;
  tether::keep_within(g_reach, p.x, p.z, dx, dz);
  g_atReachEdge = before > 0 && dx * dx + dz * dz < before * 0.99f;
}

// Authority side (master): apply the ghost player's movement and chosen actions.
void authority(session::Session& s, void* ghost, float localDx, float localDz) {
  void* agent = game::ghost_agent(ghost);
  if (!agent) {
    release_authority();
    return;
  }
  float dx = localDx, dz = localDz;
  bool live = s.local_is_ghost();
  if (!live) {
    session::Input in;
    live = s.fresh_input(GetTickCount64(), in);
    dx = in.dx;
    dz = in.dz;
  }
  if (!live) {  // the ghost player stopped sending: the native AI takes over until they return
    release_authority();
    return;
  }
  if (g_steeredGhost != ghost) {
    release_authority();
    g_steeredGhost = ghost;
    PG_LOG("control: steering ghost %p for actor %d", ghost, s.ghost_actor());
  }

  const int state = game::ghost_state(ghost);
  const ULONGLONG now = GetTickCount64();
  if (state != g_lastState) {
    PG_LOG("control: ghost state %d, speed %.2f", state, game::agent_speed(agent));
    g_lastState = state;
  }
  if (now >= g_nextStatus) {
    Vector3 p = game::position(game::transform_of(ghost));
    PG_LOG("control: ghost (%.2f,%.2f,%.2f) state %d speed %.2f dir (%.2f,%.2f)", p.x, p.y, p.z, state,
           game::agent_speed(agent), dx, dz);
    g_nextStatus = now + 1000;
  }

  actions::tick(ghost);
  const float dt = g_lastTick ? std::min(0.25f, static_cast<float>(now - g_lastTick) / 1000.0f) : 0.0f;
  g_lastTick = now;
  const tether::Reason reason = actions::movement();
  if (reason != g_lastReason) {
    static constexpr const char* kWhy[] = {"the player steers", "the player roams within the wander",
                                           "resting", "drawn home to its favourite room",
                                           "blinded by smudging", "in an event", "native (ability or settling in)"};
    PG_LOG("control: movement: %s", kWhy[reason]);
    g_lastReason = reason;
  }
  if (reason != tether::kRoaming) {
    g_reachState = nullptr;
    g_atReachEdge = false;
  }
  if (tether::native_moves(reason)) {
    // The native AI moves the ghost (src/tether.h): walking it home, blinded by smudging in a hunt,
    // an event or an ability. Its own path and speed apply until the reason ends.
    steer::clear();
  } else {
    // Otherwise only the direction is the player's; the destination, speed and acceleration stay
    // native. Resting, it holds still (the agent decelerates natively).
    if (reason == tether::kResting) dx = dz = 0;
    if (reason == tether::kRoaming) roam(ghost, dx, dz);
    steer::set(steer::crowd_agent_for(agent), dx, dz);
  }
  if (state == game::kStateHunting) pace_search(ghost, agent, dt);
  else g_search = {};

  for (const session::Trigger& t : s.take_triggers()) actions::perform(t.kind, t.aim);
  if (const int actor = actions::take_disguise()) s.send_form({actor, form::kFormMs});
  const session::Status st = host_status();
  s.set_status(st);
  marks::tick(s, ghost, st.type == session::kNoType ? -1 : st.type, st.identity == session::kNoType ? -1 : st.identity);
}

}  // namespace

void tick(session::Session& s) {
  // The truck is not shown to the ghost player while their investigator is still in the contract
  // (at most kCoverMs, so a removal that never happens cannot leave the screen black).
  static ULONGLONG coverSince = 0;
  const bool waiting = s.local_is_ghost() && game::level_controller() && !body::camera() && !leave::leaving();
  const ULONGLONG now = GetTickCount64();
  if (!waiting) coverSince = 0;
  else if (!coverSince) coverSince = now;
  panorama::cover(waiting && now - coverSince < kCoverMs);
  void* ghost = s.ghost_actor() > 0 ? game::ghost_ai() : nullptr;
  float dx = 0, dz = 0;
  if (s.local_is_ghost() && game::level_controller()) {
    ghost_player(s, ghost, dx, dz);
  } else {
    release_ghost_player();
  }
  if (s.local_is_master()) mimic::tick(s, ghost);
  if (ghost && s.local_is_master()) {
    authority(s, ghost, dx, dz);
  } else {
    release_authority();
    s.take_triggers();  // triggers only mean something to the host steering the ghost
    marks::tick(s, nullptr, -1, -1);  // nothing to perceive
  }
  if (!game::level_controller()) actions::clear();  // what was saved up ends with its contract
}

void teardown() {
  panorama::cover(false);
  release_ghost_player();
  release_authority();
}

}  // namespace pg::control
