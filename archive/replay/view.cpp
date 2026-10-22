#include "view.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "ui.h"

namespace pg::replay::view {
namespace {

constexpr int kSortingOrder = 60;
constexpr float kPanelW = 1100, kPanelH = 780, kMapW = 1000, kMapH = 580;
constexpr ULONGLONG kLobbyDelayMs = 10000;  // after the contract, so the lobby and its popups come first
constexpr ULONGLONG kBetweenMs = 1500;      // a still moment after each one
constexpr ui::Color kRoom{0.22f, 0.24f, 0.28f, 1}, kGhost{0.9f, 0.15f, 0.12f, 1}, kDead{0.45f, 0.45f, 0.45f, 0.9f};
constexpr ui::Color kSlotColours[kMaxPlayers] = {{0.30f, 0.65f, 1.00f, 1}, {0.35f, 0.90f, 0.45f, 1}, {1.00f, 0.80f, 0.25f, 1},
                                                 {0.80f, 0.45f, 1.00f, 1}, {0.20f, 0.90f, 0.90f, 1}, {1.00f, 0.55f, 0.75f, 1},
                                                 {0.95f, 0.95f, 0.95f, 1}, {0.65f, 0.50f, 0.30f, 1}};

struct Api {
  il2cpp::String* (*nickName)(void*, const void*) = nullptr;
} api;

struct Shown {
  bool pending = false;
  ULONGLONG showAt = 0;
  Replay replay;
  il2cpp::Held canvas;
  il2cpp::Held title;
  il2cpp::Held caption;
  il2cpp::Held footer;
  il2cpp::Held map;  // the map area: rooms and dots are placed inside it
  std::vector<il2cpp::Held> rooms;
  il2cpp::Held ghost;
  std::vector<il2cpp::Held> players;
  int moment = -1;
  ULONGLONG momentStart = 0;
  float minX = 0, minZ = 0, scale = 1, offX = 0, offZ = 0;
} g;

std::string name_of(int actor) {
  il2cpp::Array* list = game::photon_player_list();
  for (uintptr_t i = 0; list && i < list->max_length; ++i) {
    void* p = list->data<void*>()[i];
    if (!p || game::actor_number(p) != actor) continue;
    char buf[64] = {};
    if (il2cpp::String* s = api.nickName ? api.nickName(p, nullptr) : nullptr) il2cpp::to_utf8(s, buf, sizeof buf);
    if (buf[0]) return buf;
  }
  return "Player " + std::to_string(actor);
}

std::string slot_name(uint8_t slot) {
  return slot < g.replay.actors.size() ? name_of(g.replay.actors[slot]) : std::string("an investigator");
}

void clear_moment() {
  for (void* r : g.rooms) ui::destroy(r);
  g.rooms.clear();
}

/** Fit the floor's rooms into the map area, and draw them. */
void layout(int floor) {
  clear_moment();
  float x0 = 1e9f, z0 = 1e9f, x1 = -1e9f, z1 = -1e9f;
  std::vector<const Box*> boxes;
  for (const Box& b : g.replay.rooms)
    if (floor < 0 || b.floor == floor) boxes.push_back(&b);
  for (const Box* b : boxes) x0 = std::min(x0, b->x0), z0 = std::min(z0, b->z0), x1 = std::max(x1, b->x1), z1 = std::max(z1, b->z1);
  if (boxes.empty()) x0 = z0 = -10, x1 = z1 = 10;
  g.scale = std::min(kMapW / std::max(1.0f, x1 - x0), kMapH / std::max(1.0f, z1 - z0));
  g.minX = x0, g.minZ = z0;
  g.offX = (kMapW - (x1 - x0) * g.scale) / 2, g.offZ = (kMapH - (z1 - z0) * g.scale) / 2;
  for (const Box* b : boxes) {
    void* r = ui::create_image(g.map, "Room", kRoom);
    ui::place(r, {0, 0}, {0, 0}, {g.offX + (b->x0 - x0) * g.scale, g.offZ + (b->z0 - z0) * g.scale},
              {std::max(2.0f, (b->x1 - b->x0) * g.scale - 2), std::max(2.0f, (b->z1 - b->z0) * g.scale - 2)});
    g.rooms.push_back(r);
  }
  // The dots are made after the rooms, so they are drawn above them.
  for (void* p : g.players) ui::destroy(p);
  g.players.clear();
  ui::destroy(g.ghost);
  for (int i = 0; i < kMaxPlayers; ++i) g.players.push_back(ui::create_image(g.map, "Investigator", kSlotColours[i]));
  g.ghost = ui::create_image(g.map, "Ghost", kGhost);
}

void place_dot(void* dot, Point p, float size) {
  ui::place(dot, {0, 0}, {0.5f, 0.5f}, {g.offX + (p.x - g.minX) * g.scale, g.offZ + (p.z - g.minZ) * g.scale}, {size, size});
}

Point lerp(Point a, Point b, float t) { return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t}; }

void start_moment(int index) {
  g.moment = index;
  g.momentStart = GetTickCount64();
  const Replay::Shown& s = g.replay.moments[index];
  layout(s.frames[s.moment.peak].floor);
  const std::string text = "Moment " + std::to_string(index + 1) + " of " + std::to_string(g.replay.moments.size()) + ":  " +
                           caption(s.moment.kind, slot_name(s.moment.subject));
  ui::set_text(g.caption, text.c_str());
}

void draw(const Replay::Shown& s, ULONGLONG elapsed) {
  const float at = static_cast<float>(elapsed) / kStepMs;
  const int i = std::min(static_cast<int>(at), static_cast<int>(s.frames.size()) - 1);
  const int j = std::min(i + 1, static_cast<int>(s.frames.size()) - 1);
  const float t = std::clamp(at - static_cast<float>(i), 0.0f, 1.0f);
  const Frame& a = s.frames[i];
  const Frame& b = s.frames[j];
  const Point ghost = lerp(a.ghost, b.ghost, t);
  ui::set_active(g.ghost, (a.flags & kGhostPresent) != 0);
  place_dot(g.ghost, ghost, (a.flags & (kHunting | kEvent)) ? 26.0f : 18.0f);
  for (int p = 0; p < kMaxPlayers; ++p) {
    void* dot = g.players[p];
    if (!(a.present >> p & 1)) {
      ui::set_active(dot, false);
      continue;
    }
    const Point at2 = lerp(a.players[p], (b.present >> p & 1) ? b.players[p] : a.players[p], t);
    ui::set_active(dot, true);
    ui::Color c = (a.dead >> p & 1) ? kDead : kSlotColours[p];
    if (!same_storey(at2, ghost)) c.a = 0.25f;  // on another floor
    ui::set_color(dot, c);
    place_dot(dot, at2, 14);
  }
}

void build() {
  g.canvas = ui::create_canvas("ReplayTopMoments", kSortingOrder);
  ui::block_pointer(g.canvas);
  void* panel = ui::create_image(g.canvas, "Panel", {0.04f, 0.04f, 0.05f, 0.94f});
  ui::place(panel, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, {kPanelW, kPanelH});
  ui::set_raycast_target(panel, true);
  g.title = ui::create_text(g.canvas, "Title", 34);
  ui::place(g.title, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, kPanelH / 2 - 40}, {kPanelW - 40, 44});
  g.caption = ui::create_text(g.canvas, "Caption", 26);
  ui::place(g.caption, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, kPanelH / 2 - 88}, {kPanelW - 40, 36});
  g.map = ui::create_image(g.canvas, "Map", {0, 0, 0, 0.6f});
  ui::place(g.map, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -20}, {kMapW, kMapH});
  g.footer = ui::create_text(g.canvas, "Footer", 22);
  ui::place(g.footer, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -kPanelH / 2 + 30}, {kPanelW - 40, 32});
  const std::string who = g.replay.ghostActor ? "The ghost was played by <b>" + name_of(g.replay.ghostActor) + "</b>"
                                              : std::string("The ghost was the game's own");
  ui::set_text(g.title, ("Top moments.  " + who).c_str());
  ui::set_text(g.footer, "<alpha=#99>Click to close");
}

}  // namespace

bool resolve() {
  il2cpp::bind(api.nickName, il2cpp::cls("PhotonRealtime.dll", "Photon.Realtime", "Player"), "get_NickName", 0);
  PG_LOG("replay: names %s", api.nickName ? "ok" : "unavailable (players are shown by number)");
  return true;
}

void show(const Replay& replay) {
  hide();
  if (replay.moments.empty()) return;
  g.replay = replay;
  g.pending = true;
  g.showAt = 0;
}

void tick() {
  if (!g.pending && !g.canvas) return;
  const ULONGLONG now = GetTickCount64();
  if (game::level_controller()) {  // a contract runs: never over it
    if (g.canvas) hide();
    return;
  }
  if (g.pending) {
    if (!g.showAt) g.showAt = now + kLobbyDelayMs;
    if (now < g.showAt) return;
    g.pending = false;
    build();
    start_moment(0);
  }
  float mx = 0, my = 0;
  if (game::mouse_left_pressed() && game::mouse_position(mx, my)) {
    hide();
    return;
  }
  const Replay::Shown& s = g.replay.moments[g.moment];
  const ULONGLONG elapsed = now - g.momentStart;
  const ULONGLONG length = static_cast<ULONGLONG>(s.frames.size()) * kStepMs + kBetweenMs;
  if (elapsed >= length) {
    if (g.moment + 1 < static_cast<int>(g.replay.moments.size())) start_moment(g.moment + 1);
    else hide();
    return;
  }
  draw(s, elapsed);
}

void hide() {
  if (g.canvas) ui::destroy(g.canvas);
  g = Shown{};
}

}  // namespace pg::replay::view
