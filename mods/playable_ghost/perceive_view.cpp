#include "perceive_view.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "Camera.h"
#include "UnityUI.h"
#include "alerts.h"
#include "content.h"
#include "game.h"
#include "held.h"
#include "Transform.h"
#include "log.h"
#include "perceive.h"
#include "runtime.h"
#include "ui.h"
#include "yokai_ears.h"

namespace pm::perceive_view {
namespace {

using phasmo::Vector3;
using perceive::Mark;

constexpr ULONGLONG kMarksStaleMs = 1000; // marks not refreshed this long are gone
constexpr int kSortingOrder = 45;         // under the HUD (50)
constexpr float kDarkness = 0.88f;

struct View {
  api::Held canvas;
  api::Held dark;
  std::vector<api::Held> dots;
} g;

perceive::Recent g_recent;
perceive::Alert g_alert;
ULONGLONG g_alertUntil = 0;
std::vector<api::Held> g_muted;  // voice AudioSources silenced for the Yokai's hunt (yokai_ears.h)

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

void draw_marks(const session::Session& s, void* camera, bool darkened, ULONGLONG now) {
  const std::vector<Mark>& marks = now - s.marks_at() < kMarksStaleMs ? s.marks() : std::vector<Mark>{};
  // Screen pixels per canvas unit.
  const float scale = std::max(0.01f, phasmo::Canvas::get_scaleFactor(g.canvas));
  const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now % 100000) / 160.0f);
  size_t used = 0;
  for (const Mark& m : marks) {
    const phasmo::Vector3 screen = phasmo::Camera::WorldToScreenPoint(camera, {m.x, m.y + 0.2f, m.z});
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

// The game members it uses are map entries, which the host checks before the mod starts.
bool resolve() { return true; }

std::string tick(session::Session& s, void* camera, void* ghost, bool shown) {
  const ULONGLONG now = GetTickCount64();
  perceive::Alert relayed;
  while (s.take_alert(relayed))
    if (g_recent.fresh(relayed.kind, now)) g_alert = relayed, g_alertUntil = now + perceive::kAlertMs;
  const std::string line = alert_line(camera, ghost, now);
  const session::Status& st = s.status();
  const bool hunting = (st.flags & session::kHunting) != 0;
  const std::string type = api::content::id_of(st.type == session::kNoType ? -1 : st.type);
  if (ghost && hunting && type == "vanilla.yokai") yokai_ears::hear(ghost, g_muted);
  else yokai_ears::unmute(g_muted);
  if (!shown || !camera || !ghost) {
    if (g.canvas) ui::set_active(g.canvas, false);
    return line;
  }
  create();
  ui::set_active(g.canvas, true);
  // The Kormos, and a registered ghost blind in a hunt (the Black Shuck), see only their marks then.
  const bool blind = type == "vanilla.kormos" || (api::content::flags() & PM_GHOST_BLIND_IN_HUNTS) != 0;
  draw_marks(s, camera, hunting && blind, now);
  return line;
}

void hide() {
  yokai_ears::unmute(g_muted);
  if (g.canvas) ui::destroy(g.canvas);
  g = View{};
  g_recent.clear();
  g_alertUntil = 0;
}

}  // namespace pm::perceive_view
