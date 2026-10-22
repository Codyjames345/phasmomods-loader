#include "cards.h"

#include <string>
#include <vector>

#include "expand.h"
#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "paging.h"
#include "ui.h"

namespace pg::lobbies::cards {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); obfuscated names, so by offset.
constexpr size_t kManagerCards = 0xC8;    // ServerManager: List<MultiplayerProfile>, the lobby's ID cards
constexpr size_t kCardInLobby = 0xA8;     // MultiplayerProfile: set by ServerManager.Start for its cards
constexpr size_t kCardColour = 0x118;     // MultiplayerProfile: UpdatePlayerColor
constexpr size_t kColourPlayer = 0x30;    // UpdatePlayerColor: which player's colour (slot)
constexpr float kTabFont = 0.0055f;       // board units (a card face is 0.0767 wide)
constexpr ui::Vec2 kTabSize = {0.06f, 0.008f};
constexpr float kTabGap = 0.004f;

struct Api {
  il2cpp::Object* managerType = nullptr;
  il2cpp::Object* canvasType = nullptr;
  void* (*findObject)(il2cpp::Object*, const void*) = nullptr;
  void* (*worldCamera)(void*, const void*) = nullptr;
  void* (*mainCamera)(const void*) = nullptr;
  ui::Vec2 (*anchoredPosition)(void*, const void*) = nullptr;
  ui::Vec2 (*sizeDelta)(void*, const void*) = nullptr;
} api;

struct Board {
  void* manager = nullptr;
  Vector3 scale{1, 1, 1};  // the cards' own
  void* tab = nullptr;
  void* canvas = nullptr;  // the board's world-space Canvas
  std::string tabShown;
  int page = 0;
  bool grown = false;
} g;

il2cpp::List* card_list() { return g.manager && game::alive(g.manager) ? il2cpp::field<il2cpp::List*>(g.manager, kManagerCards) : nullptr; }
void* card(il2cpp::List* list, int i) { return list && list->items && i < list->size ? list->items->data<void*>()[i] : nullptr; }

int players() {
  il2cpp::Array* list = game::photon_player_list();
  return list ? static_cast<int>(list->max_length) : 0;
}

/** Four more cards copied from the game's last one, waking only once their PhotonView IDs are free. */
bool grow(il2cpp::List* list) {
  void* source = card(list, kVanilla - 1);
  if (!source || !game::alive(source)) return false;
  void* sourceObject = game::game_object_of(source);
  const bool wasActive = game::active_self(sourceObject);
  if (wasActive) game::set_active(sourceObject, false);  // copies made inactive are not woken yet
  for (int k = kVanilla; k < kPlayers; ++k) {
    void* copy = expand::clone(source);
    if (!copy) break;
    expand::set_view_id(copy, expand::free_view_id(0));
    il2cpp::set_field<bool>(copy, kCardInLobby, true);
    if (void* colour = il2cpp::field<void*>(copy, kCardColour)) il2cpp::set_field<int>(colour, kColourPlayer, k);
    expand::list_add(list, copy);
  }
  if (wasActive) game::set_active(sourceObject, true);
  g.scale = game::local_scale(game::transform_of(source));
  PG_LOG("cards: the lobby has %d ID cards", list->size);
  return list->size == kPlayers;
}

void make_tab(void* anyCard) {
  void* idCards = game::parent(game::transform_of(anyCard));
  void* board = idCards ? game::parent(idCards) : nullptr;
  g.canvas = board ? game::get_component(board, api.canvasType) : nullptr;
  if (!g.canvas) return;
  g.tab = ui::create_text(g.canvas, "BiggerLobbies page tab", kTabFont);
  // Beside the cards, on the right, in the board's own units.
  const ui::Vec2 at = api.anchoredPosition(idCards, nullptr);
  const ui::Vec2 size = api.sizeDelta(idCards, nullptr);
  ui::place(g.tab, {0.5f, 0.5f}, {0, 0.5f}, {at.x + size.x / 2 + kTabGap, at.y}, kTabSize);
  ui::set_alignment(g.tab, ui::Align::Left);
  ui::set_raycast_target(g.tab, true);
}

void show_page() {
  il2cpp::List* list = card_list();
  for (int i = 0; list && i < list->size; ++i) {
    void* c = card(list, i);
    if (!c || !game::alive(c)) continue;
    const bool on = on_page(i, g.page);
    expand::ignore_layout(c, !on);  // off the page: out of the board's layout, and too small to see
    game::set_local_scale(game::transform_of(c), on ? g.scale : Vector3{0, 0, 0});
  }
}

void click_tab() {
  float x = 0, y = 0;
  if (!g.tab || !game::mouse_left_pressed() || !game::mouse_position(x, y)) return;
  void* camera = api.worldCamera(g.canvas, nullptr);
  if (!camera) camera = api.mainCamera(nullptr);
  if (ui::contains(g.tab, {x, y}, camera)) g.page = 1 - g.page;
}

}  // namespace

bool resolve() {
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  il2cpp::Class* canvas = il2cpp::cls("UnityEngine.UIModule.dll", "UnityEngine", "Canvas");
  il2cpp::Class* camera = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Camera");
  il2cpp::Class* rect = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "RectTransform");
  api.managerType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "ServerManager"));
  api.canvasType = canvas ? il2cpp::type_object(canvas) : nullptr;
  bool ok = api.managerType && api.canvasType;
  ok &= il2cpp::bind(api.findObject, object, "FindObjectOfType", 1);
  ok &= il2cpp::bind(api.worldCamera, canvas, "get_worldCamera", 0);
  ok &= il2cpp::bind(api.mainCamera, camera, "get_main", 0);
  ok &= il2cpp::bind(api.anchoredPosition, rect, "get_anchoredPosition", 0);
  ok &= il2cpp::bind(api.sizeDelta, rect, "get_sizeDelta", 0);
  PG_LOG("cards: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

void tick() {
  if (game::level_controller()) return;  // the lobby's board only
  if (!g.manager || !game::alive(g.manager)) {
    teardown();
    g.manager = api.findObject(api.managerType, nullptr);
    if (!g.manager) return;
  }
  il2cpp::List* list = card_list();
  if (!list) return;
  if (!g.grown) {
    if (list->size == kVanilla) g.grown = grow(list);
    else g.grown = list->size >= kPlayers;
    if (!g.grown) return;
    make_tab(card(list, 0));
  }
  const int count = players();
  if (pages(count) < 2) g.page = 0;
  click_tab();
  const std::string label = tab_label(g.page, count);
  if (g.tab && label != g.tabShown) {
    g.tabShown = label;
    ui::set_text(g.tab, label.c_str());
  }
  show_page();
}

void teardown() {
  if (g.tab) ui::destroy(g.tab);
  g = Board{};
}

}  // namespace pg::lobbies::cards
