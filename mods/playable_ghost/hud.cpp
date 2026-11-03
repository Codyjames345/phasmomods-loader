#include "hud.h"

#include <cstdio>
#include <initializer_list>

#include "Player.h"
#include "UnityUI.h"
#include "ability.h"
#include "game.h"
#include "held.h"
#include "text_table.h"
#include "ui.h"

namespace pm::hud {
namespace {

using ui::Color;

constexpr Color kText{1, 1, 1, 0.9f};
constexpr Color kBarBack{1, 1, 1, 0.15f};
constexpr Color kBarFill{1, 1, 1, 0.8f};
constexpr Color kMark{1, 1, 1, 0.9f};
constexpr float kBarWidth = 220;
constexpr int kMaxCharges = 2;  // actions::kMaxCharges
// The investigator's crosshair (LocalPCPlayer.canvas, PCCanvas.crosshair) is copied so the ghost aims
// with the same dot.
constexpr ui::Vec2 kCrosshairSize = {48, 48};  // its size on the investigator's HUD
// The team's average sanity, with a mark where the ghost may start to hunt.
constexpr float kSanityWidth = 320;
constexpr float kSanityY = 220;
constexpr Color kSanityLow{0.85f, 0.25f, 0.2f, 0.9f};  // low enough to hunt

struct Hud {
  api::Held canvas;
  api::Held aim;
  api::Held restlessLabel;
  api::Held barBack;
  api::Held barFill;
  api::Held actions;
  api::Held notice;
  api::Held identity;  // top left: the ghost's type and favourite room
  api::Held alert;     // top centre: something done to the ghost
  api::Held sanityLabel;
  api::Held sanityBack;
  api::Held sanityFill;
  api::Held sanityMark;
  std::string aimShown, actionsShown, noticeShown, sanityShown, identityShown, alertShown;
  int fillShown = -1, sanityFillShown = -1, markShown = -1;
  bool lowShown = false;
} g;

void set_if_changed(void* text, std::string& shown, const std::string& value) {
  if (value == shown) return;
  shown = value;
  ui::set_text(text, value.c_str());
}

void add_crosshair() {
  void* player = game::local_player_component();
  void* canvas = phasmo::LocalPCPlayer::type.is(player) && game::alive(player) ? phasmo::LocalPCPlayer::canvas.get(player)
                                                                              : nullptr;
  void* crosshair = phasmo::PCCanvas::type.is(canvas) ? phasmo::PCCanvas::crosshair.get(canvas) : nullptr;
  if (!phasmo::Image::type.is(crosshair) || !game::alive(crosshair)) return;
  void* copy = game::instantiate(crosshair, game::transform_of(g.canvas), false);
  if (!copy) return;
  ui::place(copy, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, 0}, kCrosshairSize);
  game::set_active(game::game_object_of(copy), true);
}

void build() {
  g.canvas = ui::create_canvas("PlayableGhostHud", 50);
  if (!g.canvas) return;
  add_crosshair();
  g.aim = ui::create_text(g.canvas, "Aim", 26);
  ui::place(g.aim, {0.5f, 0.5f}, {0.5f, 0.5f}, {0, -70}, {700, 40});

  g.barBack = ui::create_image(g.canvas, "RestlessBack", kBarBack);
  ui::place(g.barBack, {0.5f, 0}, {0, 0.5f}, {-kBarWidth / 2, 110}, {kBarWidth, 6});
  g.barFill = ui::create_image(g.canvas, "RestlessFill", kBarFill);
  ui::place(g.barFill, {0.5f, 0}, {0, 0.5f}, {-kBarWidth / 2, 110}, {0, 6});
  g.restlessLabel = ui::create_text(g.canvas, "Restless", 24);
  ui::place(g.restlessLabel, {0.5f, 0}, {0, 0.5f}, {kBarWidth / 2 + 16, 110}, {200, 36});
  ui::set_text(g.restlessLabel, ("<align=left>" + i18n::text("hud.restless")).c_str());

  g.actions = ui::create_text(g.canvas, "Actions", 26);
  ui::place(g.actions, {0.5f, 0}, {0.5f, 0.5f}, {0, 64}, {1400, 40});
  g.notice = ui::create_text(g.canvas, "Notice", 28);
  ui::place(g.notice, {0.5f, 0}, {0.5f, 0.5f}, {0, 160}, {1600, 40});
  g.identity = ui::create_text(g.canvas, "Identity", 26);
  ui::place(g.identity, {0, 1}, {0, 1}, {48, -40}, {900, 160});
  g.alert = ui::create_text(g.canvas, "Alert", 30);
  ui::place(g.alert, {0.5f, 1}, {0.5f, 1}, {0, -48}, {1400, 44});
  g.sanityBack = ui::create_image(g.canvas, "SanityBack", kBarBack);
  ui::place(g.sanityBack, {0.5f, 0}, {0.5f, 0.5f}, {0, kSanityY}, {kSanityWidth, 6});
  g.sanityFill = ui::create_image(g.canvas, "SanityFill", kBarFill);
  ui::place(g.sanityFill, {0.5f, 0}, {0, 0.5f}, {-kSanityWidth / 2, kSanityY}, {0, 6});
  g.sanityMark = ui::create_image(g.canvas, "SanityHuntMark", kMark);
  ui::place(g.sanityMark, {0.5f, 0}, {0.5f, 0.5f}, {0, kSanityY}, {3, 18});
  g.sanityLabel = ui::create_text(g.canvas, "Sanity", 24);
  ui::place(g.sanityLabel, {0.5f, 0}, {0.5f, 0.5f}, {0, kSanityY + 28}, {1000, 36});
  for (void* t : std::initializer_list<void*>{g.aim, g.restlessLabel, g.actions, g.notice, g.sanityLabel, g.identity, g.alert})
    ui::set_color(t, kText);
}

// The investigators' average sanity, or the ghost's target's as a Banshee's is shown, and once the host
// has seen an idle roll, where hunts begin (judged on what the hunt check measures).
void show_sanity(const session::Status& st) {
  const bool known = st.sanity != session::kUnknown;
  const bool threshold = known && st.huntBelow != session::kUnknown;
  const bool low = threshold && session::hunt_sanity(st) <= st.huntBelow;
  for (void* part : std::initializer_list<void*>{g.sanityBack, g.sanityFill}) ui::set_active(part, known);
  ui::set_active(g.sanityMark, threshold);
  std::string label;
  if (known) {
    label = st.flags & session::kFocusSanity ? i18n::text("hud.sanity-target", {{"sanity", std::to_string(st.sanity)}})
                                             : i18n::text("hud.sanity-team", {{"sanity", std::to_string(st.sanity)}});
    if (low) label += "     " + i18n::text("hud.low-enough");
    else if (threshold) label += "     <alpha=#AA>" + i18n::text("hud.hunts-at", {{"sanity", std::to_string(st.huntBelow)}});
    if (st.huntSanity != session::kUnknown) label += "<alpha=#AA> " + i18n::text("hud.team", {{"sanity", std::to_string(st.huntSanity)}});
  }
  set_if_changed(g.sanityLabel, g.sanityShown, label);
  const int fill = known ? static_cast<int>(kSanityWidth * st.sanity / 100) : 0;
  if (fill != g.sanityFillShown) {
    g.sanityFillShown = fill;
    ui::place(g.sanityFill, {0.5f, 0}, {0, 0.5f}, {-kSanityWidth / 2, kSanityY}, {static_cast<float>(fill), 6});
  }
  const int mark = threshold ? static_cast<int>(kSanityWidth * st.huntBelow / 100) : -1;
  if (mark != g.markShown && threshold) {
    ui::place(g.sanityMark, {0.5f, 0}, {0.5f, 0.5f}, {-kSanityWidth / 2 + mark, kSanityY}, {3, 18});
  }
  g.markShown = mark;
  if (low != g.lowShown) {  // built with the plain colours (not low)
    g.lowShown = low;
    ui::set_color(g.sanityFill, low ? kSanityLow : kBarFill);
    ui::set_color(g.sanityLabel, low ? kSanityLow : kText);
  }
}

// An action with how many of it the native roll has given (count -1: not counted); unavailable or
// used-up actions are dimmed rather than hidden so the controls stay learnable.
std::string action(const std::string& key, const std::string& name, int count, bool available) {
  std::string text = std::string(available && count != 0 ? "<alpha=#FF>" : "<alpha=#55>") + i18n::text("hud.action", {{"key", key}, {"action", name}});
  if (count >= 0) text += " " + std::to_string(count);
  return text;
}

}  // namespace

void show(const View& v) {
  if (!g.canvas) build();
  if (!g.canvas) return;

  set_if_changed(g.aim, g.aimShown, v.aimed ? v.aimed : "");
  show_sanity(v.status);
  std::string identity;
  if (!v.ghostType.empty()) identity = "<align=left>" + i18n::text("hud.you-are", {{"type", v.ghostType}});
  if (!v.favouriteRoom.empty())
    identity += std::string(identity.empty() ? "<align=left>" : "\n") + i18n::text("hud.favourite-room", {{"room", v.favouriteRoom}});
  if (!v.senses.empty()) identity += std::string(identity.empty() ? "<align=left>" : "\n") + v.senses;
  set_if_changed(g.identity, g.identityShown, identity);
  set_if_changed(g.notice, g.noticeShown, v.notice);
  set_if_changed(g.alert, g.alertShown, v.alert);

  const int fill = static_cast<int>(kBarWidth * v.status.restless / 255);
  if (fill != g.fillShown) {
    g.fillShown = fill;
    ui::place(g.barFill, {0.5f, 0}, {0, 0.5f}, {-kBarWidth / 2, 110}, {static_cast<float>(fill), 6});
  }

  const session::Status& st = v.status;
  const bool hunting = (st.flags & session::kHunting) != 0;
  const bool ownMove = (st.flags & session::kNativeMove) != 0;  // the native AI moves the ghost
  const bool canAct = !hunting && !ownMove;
  const int interactions = session::whole_charges(st.interact, kMaxCharges);
  const std::string sep = "<alpha=#FF>     ";
  std::string line = action(v.interactKey, i18n::text("hud.interact"), interactions,
                            canAct && v.aimed && (st.more & session::kInteractAllowed)) +
                     sep + action(v.primaryKey, i18n::text("hud.manifest"), st.events, canAct && (st.more & session::kEventAllowed));
  line += sep + action(i18n::text("hud.keys-together", {{"first", v.crouchKey}, {"second", v.primaryKey}}), i18n::text("hud.disguise"), st.events, canAct && (st.more & session::kEventAllowed));
  if (const ability::Ability* a = v.ability)
    line += sep + action(v.secondaryKey, a->name, st.abilities, canAct && (st.flags & session::kAbilityAvailable));
  if (st.more & (session::kFuseTurnOff | session::kFuseTurnOn))
    line += sep + action(v.fuseKey, st.more & session::kFuseTurnOff ? i18n::text("hud.power-off") : i18n::text("hud.power-on"), st.fuses, canAct);
  line += sep + action(i18n::text("hud.hold-key", {{"key", v.primaryKey}}), i18n::text("hud.hunt"), -1, (st.flags & session::kHuntAvailable) && canAct);
  set_if_changed(g.actions, g.actionsShown, line);
}

void hide() {
  if (!g.canvas) return;
  ui::destroy(g.canvas);
  g = Hud{};
}

}  // namespace pm::hud
