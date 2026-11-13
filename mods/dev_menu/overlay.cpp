// The dev menu's overlay (overlay.h): the debug HUD and the ESP labels, the mod's own text on its own canvas.
// It only reads the game, through actions.h, and changes nothing in it.
#include "overlay.h"

#include <windows.h>

#include <cstdio>
#include <string>
#include <vector>

#include "Camera.h"
#include "ClosetZone.h"
#include "Door.h"
#include "LevelController.h"
#include "LightSwitch.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "actions.h"
#include "dummy.h"
#include "game.h"
#include "held.h"
#include "log.h"
#include "runtime.h"
#include "text_table.h"
#include "ui.h"

namespace pm::devmenu::overlay {
namespace {

namespace api = pm::api;
namespace i18n = pm::i18n;
using phasmo::Vector3;

// Every word the overlay shows is a placeholder in the mod's table for the user to write (AI_POLICY.md).
constexpr const char* kStateKey = i18n::key("dev-menu.hud.state");
constexpr const char* kTypeKey = i18n::key("dev-menu.hud.type");
constexpr const char* kRoomKey = i18n::key("dev-menu.hud.room");
constexpr const char* kEvidenceKey = i18n::key("dev-menu.hud.evidence");
constexpr const char* kSpeedKey = i18n::key("dev-menu.hud.speed");
constexpr const char* kTargetKey = i18n::key("dev-menu.hud.target");
constexpr const char* kDistanceKey = i18n::key("dev-menu.hud.distance");
constexpr const char* kSanityKey = i18n::key("dev-menu.hud.sanity");
constexpr const char* kPlayerSpeedKey = i18n::key("dev-menu.hud.player-speed");
constexpr const char* kClockKey = i18n::key("dev-menu.hud.clock");
constexpr const char* kNoclipKey = i18n::key("dev-menu.hud.noclip");
constexpr const char* kOnKey = i18n::key("dev-menu.on");
constexpr const char* kOffKey = i18n::key("dev-menu.off");
constexpr const char* kEspGhostKey = i18n::key("dev-menu.esp.ghost");
constexpr const char* kEspDummyKey = i18n::key("dev-menu.esp.dummy");
constexpr const char* kWeatherKey = i18n::key("dev-menu.hud.weather");
constexpr const char* kDialsKey = i18n::key("dev-menu.hud.dials");
constexpr const char* kHudDummyKey = i18n::key("dev-menu.hud.dummy");
constexpr const char* kEspDoorKey = i18n::key("dev-menu.esp.door");
constexpr const char* kEspDoorOpenKey = i18n::key("dev-menu.esp.door-open");
constexpr const char* kEspDoorShutKey = i18n::key("dev-menu.esp.door-shut");
constexpr const char* kEspDoorLockedKey = i18n::key("dev-menu.esp.door-locked");
constexpr const char* kEspHidingKey = i18n::key("dev-menu.esp.hiding");
constexpr const char* kEspSwitchKey = i18n::key("dev-menu.esp.switch");
constexpr const char* kEspFuseboxKey = i18n::key("dev-menu.esp.fusebox");

const ui::Color kText{0.92f, 0.92f, 0.92f, 1.0f};
const ui::Color kGhostColour{1.0f, 0.35f, 0.35f, 1.0f};
const ui::Color kRoomColour{0.55f, 0.85f, 1.0f, 1.0f};
const ui::Color kDummyColour{0.55f, 1.0f, 0.55f, 1.0f};
const ui::Color kDoorColour{1.0f, 0.8f, 0.4f, 1.0f};
const ui::Color kHidingColour{0.8f, 0.6f, 1.0f, 1.0f};
const ui::Color kSwitchColour{1.0f, 1.0f, 0.6f, 1.0f};

api::Held g_canvas;            // one canvas carries the HUD text and the ESP labels, over the game's own HUD
void* g_hud = nullptr;         // the debug read-out's single text
std::vector<void*> g_labels;   // the ESP text pool, grown as needed and switched off when unused
bool g_canvasTold = false;     // a canvas that cannot be made is worth one log, not one every frame

// A scene search for every door, hiding place, switch or fuse box is costly, so each kind's objects are
// held between searches and re-found at most every kEspScanMs, and only while that kind's switch is on.
// A cached object a scene change destroyed is skipped, and the next scan replaces it.
constexpr uint64_t kEspScanMs = 2000;
struct EspCache {
  std::vector<api::Held> items;
  uint64_t nextScan = 0;
};
EspCache g_doors;
EspCache g_hiding;
EspCache g_lights;
EspCache g_fuseboxes;

std::string number(float v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f", static_cast<double>(v));
  return buf;
}

// The debug read-out: one label and value per line, every value read from the game now.
std::string hud_text() {
  std::string out;
  const auto add = [&](const char* lineKey, std::string value) {
    if (!out.empty()) out += "\n";
    out += i18n::text(lineKey) + ": " + value;
  };
  const float dist = actions::ghost_distance();
  add(kStateKey, actions::ghost_line());
  add(kTypeKey, actions::ghost_identity());
  add(kRoomKey, actions::ghost_room_now() + " / " + actions::favourite_room_name());
  add(kEvidenceKey, actions::given_evidence());
  add(kSpeedKey, number(actions::base_speed()) + " / " + number(actions::ghost_agent_speed()));
  add(kTargetKey, actions::hunt_target_room());
  add(kDistanceKey, dist < 0.0f ? std::string() : number(dist));
  add(kSanityKey, number(actions::sanity()));
  add(kPlayerSpeedKey, number(actions::speed_multiplier()));
  add(kClockKey, number(actions::time_scale()));
  add(kNoclipKey, i18n::text(actions::switches().noclip ? kOnKey : kOffKey));
  add(kWeatherKey, actions::weather_now());
  add(kDialsKey, actions::type_dials());
  add(kHudDummyKey, i18n::text(dummy::available() ? kOnKey : kOffKey));
  return out;
}

// The pool's i'th ESP label, made when first needed; null when the game could not make one.
void* label(size_t i) {
  while (g_labels.size() <= i) {
    void* t = ui::create_text(g_canvas, "pm dev menu esp", 20.0f);
    if (t) ui::set_alignment(t, ui::Align::Center);
    g_labels.push_back(t);
  }
  return g_labels[i];
}

// Draws one ESP label over a world point. Screen pixels divided by the canvas's scale give canvas units, so
// the label lands on the point whatever the window size (the pattern in perceive_view.cpp).
void place_label(void* camera, float scale, const Vector3& world, float above, float width, float height,
                 const std::string& text, const ui::Color& colour, size_t& used) {
  const Vector3 screen = phasmo::Camera::WorldToScreenPoint(camera, {world.x, world.y + above, world.z});
  if (screen.z <= 0.1f) return;  // behind the camera, or the entry did not answer
  void* t = label(used++);
  if (!t) return;
  ui::place(t, {0, 0}, {0.5f, 0.5f}, {screen.x / scale, screen.y / scale}, {width, height});
  ui::set_text(t, text.c_str());
  ui::set_color(t, colour);
  ui::set_active(t, true);
}

// Refinds the cache's objects of the System.Type `type` when its scan is due (`now` from
// GetTickCount64). Objects the game keeps switched off are left out. The type being unresolved only
// leaves the cache empty; every member then answers its unresolved default.
void esp_rescan(EspCache& cache, void* type, uint64_t now) {
  if (now < cache.nextScan) return;
  cache.nextScan = now + kEspScanMs;
  cache.items.clear();
  const api::RefArray found{type ? phasmo::Object::FindAllOfType(type, false) : nullptr};
  for (size_t i = 0; i < found.size(); ++i)
    if (void* o = found[i]; game::alive(o)) cache.items.emplace_back(o);
}

// Draws one ESP label per live cached object, `above` metres over its own transform, its text made from
// the object by `make_text`.
template <typename Text>
void esp_labels(EspCache& cache, void* camera, float scale, float above, float width, float height,
                const ui::Color& colour, size_t& used, Text make_text) {
  for (const api::Held& item : cache.items) {
    void* o = item.get();
    if (!game::alive(o)) continue;
    void* t = game::transform_of(o);
    if (!t) continue;
    place_label(camera, scale, game::position(t), above, width, height, make_text(o), colour, used);
  }
}

}  // namespace

void tick() {
  const auto& s = actions::switches();
  if (!s.debugHud && !s.espGhost && !s.espRooms && !s.espDoors && !s.espHiding && !s.espSwitches) {
    if (g_canvas && game::alive(g_canvas)) ui::set_active(g_canvas, false);
    return;
  }
  if (!g_canvas || !game::alive(g_canvas)) {
    // A scene change took the canvas, and everything under it, away: rebuild and forget the old objects.
    g_canvas = ui::create_canvas("pm dev menu overlay", 29000);
    g_hud = nullptr;
    g_labels.clear();
    if (!g_canvas) {
      if (!g_canvasTold) {
        g_canvasTold = true;
        PM_LOG("dev-menu: the overlay's canvas could not be made");
      }
      return;
    }
  }
  ui::set_active(g_canvas, true);

  if (s.debugHud) {
    if (!g_hud || !game::alive(g_hud)) {
      // Pinned to the screen's top-left corner, the lines growing downward. (It was placed 930 units left of the
      // left edge, off the screen, which is why the user saw the switch do nothing, 2026-11-13.)
      g_hud = ui::create_text(g_canvas, "pm dev menu hud text", 22.0f);
      if (g_hud) {
        ui::set_color(g_hud, kText);
        ui::set_alignment(g_hud, ui::Align::TopLeft);
        ui::place(g_hud, {0.0f, 1.0f}, {0.0f, 1.0f}, {30.0f, -30.0f}, {860.0f, 400.0f});
      }
    }
    if (g_hud) {
      ui::set_active(g_hud, true);
      ui::set_text(g_hud, hud_text().c_str());
    }
  } else if (g_hud && game::alive(g_hud)) {
    ui::set_active(g_hud, false);
  }

  // ESP: labels over the ghost and each room. A read that fails only skips its own label.
  size_t used = 0;
  if (void* camera = game::local_camera()) {
    const float sf = phasmo::Canvas::get_scaleFactor(g_canvas);
    const float scale = sf < 0.01f ? 0.01f : sf;
    if (s.espGhost) {
      Vector3 p;
      if (actions::ghost_at(p)) {
        place_label(camera, scale, p, 1.8f, 420.0f, 40.0f,
                    i18n::text(kEspGhostKey) + " " + number(actions::ghost_distance()) + "m", kGhostColour,
                    used);
      }
    }
    // The dummy shows with the ghost's ESP: its own body may sit on the layer the local camera leaves out.
    if (s.espGhost) {
      Vector3 p;
      if (dummy::position(p))
        place_label(camera, scale, p, 1.9f, 320.0f, 36.0f, i18n::text(kEspDummyKey), kDummyColour, used);
    }
    if (s.espRooms) {
      const int n = actions::room_count();
      for (int i = 0; i < n; ++i) {
        Vector3 p;
        if (actions::room_centre(i, p)) place_label(camera, scale, p, 0.0f, 360.0f, 34.0f,
                                                    actions::room_name(i), kRoomColour, used);
      }
    }
    // Doors, hiding places and switches come from the cached scene searches: each kind is re-found at
    // most every kEspScanMs, and only while its switch is on.
    const uint64_t now = GetTickCount64();
    if (s.espDoors) {
      esp_rescan(g_doors, phasmo::Door::type.type_object(), now);
      esp_labels(g_doors, camera, scale, 1.0f, 260.0f, 32.0f, kDoorColour, used, [](void* door) {
        std::string text = i18n::text(kEspDoorKey) + " " +
                           i18n::text(phasmo::Door::closed.get(door) ? kEspDoorShutKey : kEspDoorOpenKey);
        if (phasmo::Door::locked.get(door)) text += ", " + i18n::text(kEspDoorLockedKey);
        return text;
      });
    }
    if (s.espHiding) {
      esp_rescan(g_hiding, phasmo::ClosetZone::type.type_object(), now);
      esp_labels(g_hiding, camera, scale, 0.5f, 300.0f, 32.0f, kHidingColour, used,
                 [](void*) { return i18n::text(kEspHidingKey); });
    }
    if (s.espSwitches) {
      // The fuse box shares the switch's ESP and its colour: one switch shows all the level's electrics.
      esp_rescan(g_lights, phasmo::LightSwitch::type.type_object(), now);
      esp_rescan(g_fuseboxes, phasmo::FuseBox::type.type_object(), now);
      // get_IsSwitchOn is what the phasmo::LightSwitch wrapper answers for "is it on"; the wrapper does
      // not expose the map's get_isOn, which additionally requires the switch's fuse not blown.
      esp_labels(g_lights, camera, scale, 0.2f, 200.0f, 30.0f, kSwitchColour, used, [](void* light) {
        return i18n::text(kEspSwitchKey) + " " +
               i18n::text(phasmo::LightSwitch::get_IsSwitchOn(light) ? kOnKey : kOffKey);
      });
      esp_labels(g_fuseboxes, camera, scale, 0.2f, 260.0f, 32.0f, kSwitchColour, used, [](void* box) {
        return i18n::text(kEspFuseboxKey) + " " +
               i18n::text(phasmo::FuseBox::powerOn.get(box) ? kOnKey : kOffKey);
      });
    }
  }
  // Whatever the pool holds past what this frame drew is not wanted now, ESP off or camera missing alike.
  for (size_t i = used; i < g_labels.size(); ++i) {
    if (g_labels[i]) ui::set_active(g_labels[i], false);
  }
}

void destroy() {
  if (g_canvas) ui::destroy(g_canvas);
  g_canvas = nullptr;
  g_hud = nullptr;
  g_labels.clear();
  g_doors.items.clear();
  g_hiding.items.clear();
  g_lights.items.clear();
  g_fuseboxes.items.clear();
  g_doors.nextScan = g_hiding.nextScan = g_lights.nextScan = g_fuseboxes.nextScan = 0;
}

}  // namespace pm::devmenu::overlay
