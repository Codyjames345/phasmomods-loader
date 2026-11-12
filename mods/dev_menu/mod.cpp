// The dev menu as a mod of the suite (core/mod_api.h): a single player sandbox for the loader's own
// development, shipped in every release but kept out of sight for players (docs/superpowers/specs/
// 2026-11-10-dev-menu-content-plan.md). The manifest marks it developerOnly, so the loader hides it from
// every list and never loads it unless the player's launcher.ini sets developer=1. The rows are in
// menu.cpp, their actions in actions.cpp, and the rules that decide them in menu_rule.h.
#include <filesystem>

#include "actions.h"
#include "game.h"
#include "keybinds.h"
#include "log.h"
#include "menu.h"
#include "overlay.h"
#include "mod_glue.h"
#include "text_table.h"
#include "ui.h"

namespace pm::devmenu {
namespace {

constexpr PmModInfo kInfo{PMMOD_API_VERSION, "phasmomods.dev-menu", "Dev Menu", 8, 0};
constexpr const char* kToggleKey = "toggle";  // the keybinds row's id, within this mod
bool g_running = false;

bool start(const PmHostApi* api) {
  using namespace pm;
  if (!mod::bind(api, &kInfo)) return false;
  if (!(game::resolve() && ui::resolve())) {
    PM_LOG("Dev Menu inactive: game binding resolution failed");
    return false;
  }
  // The key is the one left of the 1 on the game's own defaults, rebindable through the game's keybinds
  // menu (the user's call, 2026-11-10). minus and equals fall back when the game already has the first.
  const std::filesystem::path data =
      std::filesystem::path(mod::host().dataDirectory) / mod::info().id / "keybinds.txt";
  api::keys::begin_keys(mod::info().id, mod::shown_name(), data);
  api::keys::ModKey key;
  key.id = kToggleKey;
  key.label = i18n::text("dev-menu.key-label");
  key.paths[0] = "<Keyboard>/backquote";
  key.fallbacks[0] = {"<Keyboard>/minus", "<Keyboard>/equals"};
  api::keys::add_key(key);
  // The hooks come after begin_keys, which hooks the keybinds page itself (sdk/api/keybinds.h).
  if (!actions::start()) {
    PM_LOG("Dev Menu inactive: hook installation failed");
    return false;
  }
  return true;
}

void tick() {
  using namespace pm;
  api::keys::tick_keys();
  if (api::keys::key_state(kToggleKey).pressed) {
    if (menu::is_open())
      menu::close();
    else if (actions::can_open())
      menu::open();
  }
  // Defensive: whatever happens to the room state, the menu never shows outside the single player sandbox.
  if (menu::is_open() && !actions::can_open()) menu::close();
  menu::tick();
  actions::tick();  // the sanity lock and noclip's rig, menu open or not
  overlay::tick();  // the debug HUD and ESP stay up while the menu is closed
}

}  // namespace
}  // namespace pm::devmenu

extern "C" __declspec(dllexport) const PmModInfo* pmmod_info() { return &pm::devmenu::kInfo; }

extern "C" __declspec(dllexport) bool pmmod_start(const PmHostApi* api) {
  return pm::mod::guarded_start("DevMenu", pm::devmenu::g_running, &pm::devmenu::start, api);
}

extern "C" __declspec(dllexport) void pmmod_tick() {
  pm::mod::guarded_tick(pm::devmenu::g_running, &pm::devmenu::tick);
}

extern "C" __declspec(dllexport) void pmmod_stop() {
  pm::mod::guarded_stop(pm::devmenu::g_running, [] {
    pm::devmenu::menu::destroy();
    pm::devmenu::overlay::destroy();
    pm::devmenu::actions::stop();
    pm::api::keys::clear_keys();
  });
}
