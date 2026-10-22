#include "perceive_view.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "alerts.h"
#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "perceive.h"
#include "ui.h"

namespace pg::perceive_view {
namespace {

using il2cpp::Vector3;
using perceive::Mark;

constexpr size_t kVoicePlayer = 0x20;     // PlayerVoice: its Player
constexpr size_t kVoiceLocalTap = 0x70;   // PlayerVoice: the tap carrying the player's positional voice
constexpr int kYokai = 12, kKormos = 28;
constexpr int kBlackShuck = 35;           // the Custom Ghosts Black Shuck, by identity: blind in a hunt, as the Kormos
constexpr float kYokaiHearing = 2.5f;     // metres: the Yokai hears voices only this close in a hunt
constexpr ULONGLONG kMarksStaleMs = 1000; // marks not refreshed this long are gone
constexpr int kSortingOrder = 45;         // under the HUD (50)
constexpr float kDarkness = 0.88f;

struct Api {
  Vector3 (*worldToScreen)(void*, Vector3, const void*) = nullptr;
  float (*scaleFactor)(void*, const void*) = nullptr;
  void (*setMute)(void*, bool, const void*) = nullptr;
  il2cpp::Array* (*findObjects)(il2cpp::Object*, const void*) = nullptr;
  il2cpp::Object* sourceType = nullptr;
  il2cpp::Object* voiceType = nullptr;
} api;

struct View {
  il2cpp::Held canvas;
  il2cpp::Held dark;
  std::vector<il2cpp::Held> dots;
} g;

perceive::Recent g_recent;
perceive::Alert g_alert;
ULONGLONG g_alertUntil = 0;
std::vector<il2cpp::Held> g_muted;  // voice AudioSources silenced for the Yokai's hunt

ui::Color colour(uint8_t kind, float pulse) {
  switch (kind) {
    case perceive::kStepWalk: return {0.70f, 0.85f, 1.00f, 0.45f + 0.45f * pulse};
    case perceive::kStepSprint: return {0.80f, 0.92f, 1.00f, 0.55f + 0.45f * pulse};
    case perceive::kStepCreep: return {0.60f, 0.75f, 0.95f, 0.30f + 0.30f * pulse};
    case perceive::kPrey: return {0.90f, 0.20f, 0.15f, 0.85f};
    case perceive::kMover: return {1.00f, 0.55f, 0.15f, 0.90f};
    case perceive::kStill: return {0.55f, 0.65f, 0.80f, 0.70f};
    case perceive::kCharge: return {1.00f, 0.90f, 0.30f, 0.50f + 0.40f * pulse};
    case perceive::kScent: return {0.75f, 0.55f, 0.25f, 0.55f};
    default: return {1, 1, 1, 0.5f};
  }
}

float size_of(uint8_t kind) {
  switch (kind) {
    case perceive::kStepSprint: return 26;
    case perceive::kStepWalk: return 18;
    case perceive::kStepCreep: return 11;
    case perceive::kScent: return 8;
    default: return 16;
  }
}

void create() {
  if (g.canvas) return;
  g.canvas = ui::create_canvas("PlayableGhostSenses", kSortingOrder);
  g.dark = ui::create_image(g.canvas, "Darkness", {0, 0, 0, kDarkness});
  ui::place(g.dark, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {6000, 6000});  // past every screen's edges
  ui::set_active(g.dark, false);
}

void* dot(size_t i) {
  while (g.dots.size() <= i) g.dots.push_back(ui::create_image(g.canvas, "Sense", {1, 1, 1, 0}));
  return g.dots[i];
}

void unmute_voices() {
  for (void* source : g_muted)
    if (game::alive(source)) api.setMute(source, false, nullptr);
  g_muted.clear();
}

/** The Yokai in a hunt hears investigators' voices only within 2.5 m of itself. */
void yokai_hearing(void* ghost) {
  const Vector3 at = game::position(game::transform_of(ghost));
  il2cpp::Array* voices = api.findObjects(api.voiceType, nullptr);
  for (uintptr_t i = 0; voices && i < voices->max_length; ++i) {
    void* voice = voices->data<void*>()[i];
    void* player = voice ? il2cpp::field<void*>(voice, kVoicePlayer) : nullptr;
    void* tap = voice ? il2cpp::field<void*>(voice, kVoiceLocalTap) : nullptr;
    void* source = tap ? game::get_component(tap, api.sourceType) : nullptr;
    if (!source || !player || !game::alive(player)) continue;
    const Vector3 p = game::position(game::transform_of(player));
    const bool distant = std::hypot(p.x - at.x, p.z - at.z) > kYokaiHearing || std::fabs(p.y - at.y) > 2.5f;
    api.setMute(source, distant, nullptr);
    if (distant && std::find(g_muted.begin(), g_muted.end(), source) == g_muted.end()) g_muted.push_back(source);
  }
}

void draw_marks(const session::Session& s, void* camera, bool darkened, ULONGLONG now) {
  const std::vector<Mark>& marks = now - s.marks_at() < kMarksStaleMs ? s.marks() : std::vector<Mark>{};
  const float scale = std::max(0.01f, api.scaleFactor(g.canvas, nullptr));  // screen pixels per canvas unit
  const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now % 100000) / 160.0f);
  size_t used = 0;
  for (const Mark& m : marks) {
    const Vector3 screen = api.worldToScreen(camera, {m.x, m.y + 0.2f, m.z}, nullptr);
    if (screen.z <= 0.1f) continue;  // behind the ghost
    void* d = dot(used++);
    const float size = size_of(m.kind);
    ui::place(d, {0, 0}, {0.5f, 0.5f}, {screen.x / scale, screen.y / scale}, {size, size});
    ui::set_color(d, colour(m.kind, pulse));
    ui::set_active(d, true);
  }
  for (size_t i = used; i < g.dots.size(); ++i) ui::set_active(g.dots[i], false);
  ui::set_active(g.dark, darkened);
}

std::string alert_line(void* camera, void* ghost, ULONGLONG now) {
  perceive::Alert a;
  while (alerts::take(a)) {
    if (g_recent.fresh(a.kind, now)) g_alert = a, g_alertUntil = now + perceive::kAlertMs;
  }
  if (now >= g_alertUntil) return {};
  std::string line = perceive::alert_text(g_alert);
  if (g_alert.located && ghost) {
    const Vector3 p = game::position(game::transform_of(ghost));
    const Vector3 f = game::forward(game::transform_of(camera));
    const float yaw = std::atan2(f.x, f.z) * 180.0f / 3.14159265f;
    line += " (" + perceive::bearing(p.x, p.z, yaw, g_alert.x, g_alert.z) + ")";
  }
  return line;
}

}  // namespace

bool resolve() {
  il2cpp::Class* camera = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Camera");
  il2cpp::Class* canvas = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
  il2cpp::Class* source = il2cpp::cls("UnityEngine.AudioModule.dll", "UnityEngine", "AudioSource");
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  bool ok = il2cpp::bind(api.worldToScreen, camera, "WorldToScreenPoint", 1);
  ok &= il2cpp::bind(api.scaleFactor, canvas, "get_scaleFactor", 0);
  ok &= il2cpp::bind(api.setMute, source, "set_mute", 1);
  ok &= il2cpp::bind(api.findObjects, object, "FindObjectsOfType", 1);
  api.sourceType = il2cpp::type_object(source);
  api.voiceType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "PlayerVoice"));
  ok &= api.sourceType && api.voiceType;
  PG_LOG("perceive: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

std::string tick(session::Session& s, void* camera, void* ghost, bool shown) {
  const ULONGLONG now = GetTickCount64();
  perceive::Alert relayed;
  while (s.take_alert(relayed))
    if (g_recent.fresh(relayed.kind, now)) g_alert = relayed, g_alertUntil = now + perceive::kAlertMs;
  const std::string line = alert_line(camera, ghost, now);
  const session::Status& st = s.status();
  const bool hunting = (st.flags & session::kHunting) != 0;
  if (ghost && hunting && st.type == kYokai) yokai_hearing(ghost);
  else unmute_voices();
  if (!shown || !camera || !ghost) {
    if (g.canvas) ui::set_active(g.canvas, false);
    return line;
  }
  create();
  ui::set_active(g.canvas, true);
  draw_marks(s, camera, hunting && (st.type == kKormos || st.identity == kBlackShuck), now);
  return line;
}

void hide() {
  unmute_voices();
  if (g.canvas) ui::destroy(g.canvas);
  g = View{};
  g_recent.clear();
  g_alertUntil = 0;
}

}  // namespace pg::perceive_view
