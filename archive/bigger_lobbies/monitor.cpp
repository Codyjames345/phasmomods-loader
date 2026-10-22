#include "monitor.h"

#include <windows.h>

#include <vector>

#include "expand.h"
#include "game.h"
#include "il2cpp.h"
#include "log.h"
#include "paging.h"

namespace pg::lobbies::monitor {
namespace {

using il2cpp::Vector3;

// Build 25811160 (verified by the host at load); HeartRateData's lists, each one entry per player.
constexpr size_t kPanels = 0x30;    // GameObject[]: each player's panel in the grid
constexpr size_t kImages = 0x38;    // Image[]
constexpr size_t kNames = 0x40;     // TextMeshProUGUI[]
constexpr size_t kReadings = 0x48;  // TextMeshProUGUI[]
constexpr size_t kColourPlayer = 0x30;  // UpdatePlayerColor: which player's colour (slot)

struct Api {
  il2cpp::Object* monitorType = nullptr;
  il2cpp::Object* colourType = nullptr;
  void* (*findObject)(il2cpp::Object*, const void*) = nullptr;
} api;

struct Shown {
  void* monitor = nullptr;
  bool grown = false, failed = false;
  Vector3 scale{1, 1, 1};
  AutoPager pager;
} g;

il2cpp::Array* list(size_t offset) { return il2cpp::field<il2cpp::Array*>(g.monitor, offset); }
void* at(il2cpp::Array* a, int i) { return a && static_cast<uintptr_t>(i) < a->max_length ? a->data<void*>()[i] : nullptr; }

bool grow() {
  il2cpp::Array* panels = list(kPanels);
  void* last = at(panels, kVanilla - 1);
  if (!last) return false;
  std::vector<void*> morePanels, moreImages, moreNames, moreReadings;
  for (int k = kVanilla; k < kPlayers; ++k) {
    void* copy = expand::clone(last);
    if (!copy) return false;
    void* image = expand::counterpart(last, at(list(kImages), kVanilla - 1), copy);
    void* name = expand::counterpart(last, at(list(kNames), kVanilla - 1), copy);
    void* reading = expand::counterpart(last, at(list(kReadings), kVanilla - 1), copy);
    if (!image || !name || !reading) {
      PG_LOG("monitor: a player panel is not laid out as expected; the monitor shows four players");
      game::destroy(copy);
      return false;
    }
    if (void* colour = game::get_component(game::object_transform(copy), api.colourType)) il2cpp::set_field<int>(colour, kColourPlayer, k);
    morePanels.push_back(copy), moreImages.push_back(image), moreNames.push_back(name), moreReadings.push_back(reading);
  }
  const size_t offsets[] = {kPanels, kImages, kNames, kReadings};
  const std::vector<void*>* extras[] = {&morePanels, &moreImages, &moreNames, &moreReadings};
  il2cpp::Array* grownLists[4] = {};
  for (int i = 0; i < 4; ++i)
    if (!(grownLists[i] = expand::grown(list(offsets[i]), *extras[i]))) return false;
  for (int i = 0; i < 4; ++i) il2cpp::set_ref_field(g.monitor, offsets[i], grownLists[i]);
  g.scale = game::local_scale(game::object_transform(last));
  PG_LOG("monitor: the truck's sanity monitor has %d panels", kPlayers);
  return true;
}

void show_page(int page) {
  il2cpp::Array* panels = list(kPanels);
  for (int i = 0; panels && static_cast<uintptr_t>(i) < panels->max_length; ++i) {
    void* panel = at(panels, i);
    if (!panel || !game::alive(panel)) continue;
    const bool on = on_page(i, page);
    expand::ignore_layout(panel, !on);
    game::set_local_scale(game::object_transform(panel), on ? g.scale : Vector3{0, 0, 0});
  }
}

}  // namespace

bool resolve() {
  il2cpp::Class* object = il2cpp::cls("UnityEngine.CoreModule.dll", "UnityEngine", "Object");
  api.monitorType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "HeartRateData"));
  api.colourType = il2cpp::type_object(il2cpp::cls("Assembly-CSharp.dll", "", "UpdatePlayerColor"));
  const bool ok = api.monitorType && api.colourType && il2cpp::bind(api.findObject, object, "FindObjectOfType", 1);
  PG_LOG("monitor: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

void tick() {
  if (!game::level_controller()) {
    g = Shown{};
    return;
  }
  if (!g.monitor || !game::alive(g.monitor)) {
    g = Shown{};
    g.monitor = api.findObject(api.monitorType, nullptr);
    if (!g.monitor) return;
  }
  if (g.failed) return;
  if (!g.grown) {
    il2cpp::Array* panels = list(kPanels);
    if (!panels) return;
    g.grown = panels->max_length >= static_cast<uintptr_t>(kPlayers) || (panels->max_length == static_cast<uintptr_t>(kVanilla) && grow());
    g.failed = !g.grown;
    if (!g.grown) return;
  }
  il2cpp::Array* players = game::photon_player_list();
  show_page(g.pager.page(players ? static_cast<int>(players->max_length) : 0, GetTickCount64()));
}

void teardown() { g = Shown{}; }

}  // namespace pg::lobbies::monitor
