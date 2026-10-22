#include <vector>

#include "flame_rules.h"
#include "flames.h"
#include "game.h"
#include "hooks.h"
#include "il2cpp.h"
#include "journal.h"
#include "log.h"
#include "mod_glue.h"
#include "net.h"
#include "services.h"
#include "ui.h"

// Spectral Flame, an eighth evidence type, as a mod of the suite (core/mod_api.h): a flame set down
// where the ghost spends its time slowly turns blue. It acts only in a private room in which every
// client runs it, and only in a contract where some ghost type carries it (Custom Ghosts adds those);
// otherwise the game is as it was. Players without the mod may share the lobby: the contract is then
// plain vanilla.
namespace {

constexpr const char* kId = "spectral-flame";
constexpr PgModInfo kInfo{PGMOD_API_VERSION, kId, "Spectral Flame", 5, PG_MOD_VANILLA_PEERS};
constexpr size_t kControllerCollection = 0x90;  // GhostController.allGhostsCollection
constexpr size_t kCollectionGhosts = 0x18;      // GhostCollection.ghosts: List<GhostData>
constexpr size_t kDataEvidence = 0x28;          // GhostData.evidence1..3 (int each)

bool g_running = false;
il2cpp::Class* g_ghostController = nullptr;
std::vector<int> g_actors;
std::vector<int> g_evidence;
il2cpp::Held g_contract;  // the LevelController the flames were switched on for
bool g_on = false;           // switched on for this contract: held until it unloads

bool ready() {
  using namespace pg;
  if (!game::private_room()) return false;
  game::room_actors(g_actors);
  if (g_actors.empty()) return false;
  for (int actor : g_actors)
    if (!mod::peer_has(actor, kId)) return false;
  return true;
}

/** Whether a ghost type in the contract's collection carries Spectral Flame. */
bool contract_carries_flame() {
  using namespace pg;
  void* level = game::level_controller();
  void* controller = il2cpp::static_field_by_type(g_ghostController, "GhostController");
  if (!level || !game::alive(level) || !controller || !game::alive(controller)) return false;
  void* collection = il2cpp::field<void*>(controller, kControllerCollection);
  auto* ghosts = collection ? il2cpp::field<il2cpp::List*>(collection, kCollectionGhosts) : nullptr;
  g_evidence.clear();
  for (int i = 0; ghosts && ghosts->items && i < ghosts->size; ++i)
    if (void* data = ghosts->items->data<void*>()[i])
      for (int k = 0; k < 3; ++k) g_evidence.push_back(il2cpp::field<int>(data, kDataEvidence + 4 * k));
  return flame::active_for(g_evidence.data(), static_cast<int>(g_evidence.size() / 3));
}

bool service_ready() {
  try {
    return g_running && ready();  // a stopped mod shows no flames, whatever the room
  } catch (...) {
    return false;  // nothing thrown may reach another mod
  }
}

constexpr PgSpectralFlameService kService{1, &service_ready};

bool start(const PgHostApi* api) {
  using namespace pg;
  if (!mod::bind(api, &kInfo)) return false;
  g_ghostController = il2cpp::cls("Assembly-CSharp.dll", "", "GhostController");
  if (!(g_ghostController && game::resolve() && ui::resolve() && flame::flames::resolve() && flame::journal::resolve())) {
    PG_LOG("SpectralFlame inactive: game binding resolution failed");
    return false;
  }
  if (!(flame::flames::create_hooks() && hooks::enable_all())) {
    PG_LOG("SpectralFlame inactive: hook installation failed");
    return false;
  }
  net::set_handler(&flame::flames::receive);
  api->register_service(PG_SERVICE_SPECTRAL_FLAME, &kService);
  return true;
}

void tick() {
  using namespace pg;
  // Decided per contract and then held, as Custom Ghosts' registry does: a peer's presence can lapse
  // for a frame (a hitch, a late announcement), and switching off would wipe every flame's blueness.
  void* level = game::level_controller();
  const bool loaded = level && game::alive(level);
  if (level != g_contract) {
    g_contract = level;
    g_on = false;
  }
  if (loaded && !g_on && ready() && contract_carries_flame()) {
    g_on = true;
    PG_LOG("SpectralFlame: on for this contract");
  }
  const bool on = loaded && g_on;
  flame::flames::tick(on);
  flame::journal::tick(on);
}

}  // namespace

extern "C" __declspec(dllexport) const PgModInfo* pgmod_info() { return &kInfo; }

extern "C" __declspec(dllexport) bool pgmod_start(const PgHostApi* api) {
  return pg::mod::guarded_start("SpectralFlame", g_running, &start, api);
}

extern "C" __declspec(dllexport) void pgmod_tick() { pg::mod::guarded_tick(g_running, &tick); }

extern "C" __declspec(dllexport) void pgmod_stop() {
  pg::mod::guarded_stop(g_running, [] {
    g_contract = nullptr;
    g_on = false;
    pg::flame::journal::teardown();
    pg::flame::flames::teardown();
  });
}
