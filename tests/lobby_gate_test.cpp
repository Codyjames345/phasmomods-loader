// Offline test of what the host keeps from going out of a lobby (host/lobby_gate.cpp) over the fake game: a contract's start,
// at the press and at the send, and a custom value while a player is unverified. Run: build\lobby_gate_test.exe
#include "lobby_gate.h"

#include <string>
#include <vector>

#include "JournalController.h"
#include "Photon.h"
#include "ServerManager.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "check.h"
#include "content_registry.h"
#include "entry_hooks.h"
#include "fake_game.h"
#include "game_api.h"
#include "hooks.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
// The press's hook, as host/entry_hooks.cpp reaches it: recorded, and called by hand below.
void* g_madeDetour = nullptr;
bool g_makes = true;
bool create(void*, void* detour, void** original, const char*) {
  if (!g_makes) return false;
  g_madeDetour = detour;
  if (original) *original = nullptr;
  return true;
}
void rollback_to(size_t) {}
size_t mark() { return 0; }
}  // namespace pm::hooks

const PmGameEntry* pm::api::host_only::game_entry(const char* key) { return pm::fake::host_entry(key); }

namespace {
namespace fake = pm::fake;
namespace map = pm::map;
namespace gate = pm::lobby_gate;

int buttonClass;
struct Thing {
  void* klass = nullptr;
};
Thing startButton, otherButton, journalView, ghostSyncView, evidenceSyncView;

bool startAllowedAnswer = true, startThrows = false, verifiedAnswer = true, verifiedThrows = false;
constexpr int32_t kBookPages = 55;  // the book as the game made it, in this test
int startAsks = 0;
bool is_start(void* button) { return button == &startButton; }
bool start_allowed() {
  ++startAsks;
  if (startThrows) throw 1;
  return startAllowedAnswer;
}
// A start underway (host/start_close.h), as the suite answers it.
bool pendingAnswer = false;
int sendAnswer = static_cast<int>(gate::Send::AsSent);
int sendAsks = 0;
bool start_pending() { return pendingAnswer; }
int start_send() {
  ++sendAsks;
  return sendAnswer;
}
bool privateAnswer = true, privateThrows = false;
bool private_room() {
  if (privateThrows) throw 1;
  return privateAnswer;
}
bool room_verified() {
  if (verifiedThrows) throw 1;
  return verifiedAnswer;
}
void* customPageView = nullptr;  // the view custom_page was last asked about
bool custom_page(void* view, int32_t page) {
  customPageView = view;
  return page >= kBookPages;
}
bool boxed_int(const void* boxed, int32_t& out) {
  out = *static_cast<const int32_t*>(boxed);
  return true;
}

int rpcClass;
void* component_of(void* on, void* type, const void*) {
  if (on == &ghostSyncView && type == phasmo::JournalGhostSyncController::type.type_object()) return &ghostSyncView;
  if (on == &evidenceSyncView && type == phasmo::JournalEvidenceSyncController::type.type_object()) return &evidenceSyncView;
  return nullptr;
}

pm::content::Registry registry;

void give() {
  fake::reset();
  fake::type(map::Button, &buttonClass);
  fake::type(map::JournalGhostSyncController, &rpcClass);
  fake::type(map::JournalEvidenceSyncController, &buttonClass);
  fake::method(map::Component_GetComponent, reinterpret_cast<void*>(&component_of));
  std::string missing;
  pm::api::bind(fake::host(), missing);
  pm::content::GameEnums enums;
  CHECK(pm::content::read_game_enums(fake::audited_ghost_types(), fake::audited_evidence(), fake::kAuditedNoGhost,
                                     fake::kAuditedNoEvidence, enums).empty());
  registry.set_game_enums(std::move(enums));
  gate::Config config;
  config.isStartButton = &is_start;
  config.startAllowed = &start_allowed;
  config.roomVerified = &room_verified;
  config.customPage = &custom_page;
  config.boxedInt = &boxed_int;
  config.registry = &registry;
  config.startPending = &start_pending;
  config.startSend = &start_send;
  config.privateRoom = &private_room;
  // Only the config is wanted here: no entry for the press means nothing is hooked.
  gate::create_hooks(config);
  startAllowedAnswer = verifiedAnswer = true;
  startThrows = verifiedThrows = false;
  startAsks = 0;
  pendingAnswer = false;
  sendAnswer = static_cast<int>(gate::Send::AsSent);
  sendAsks = 0;
  privateAnswer = true;
  privateThrows = false;
  customPageView = nullptr;
}

void* name_of(const char* text) { return pm::api::make_string(text); }
void* marked(int32_t* value) {
  void* args = fake::array(sizeof(void*), 1);
  pm::api::RefArray{args}.set(0, value);
  return args;
}

// The press that starts a contract: refused while the guard says wait, before any of the game's start runs. No other button
// asks anything.
void start_waits_for_an_unverified_player() {
  give();
  startAllowedAnswer = false;
  CHECK(!gate::press_goes_on(&startButton));
  CHECK(startAsks == 1);
  CHECK(gate::press_goes_on(&otherButton));
  CHECK(startAsks == 1);
  CHECK(gate::press_goes_on(nullptr));
}

void start_goes_once_everyone_is_verified() {
  give();
  CHECK(gate::press_goes_on(&startButton));
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::AsSent);
}

// In a public lobby the rules don't hold: the start goes as the game sent it, with no closing and no marker for anyone there. A
// room that can't be told is taken for a private one, so the start's own checks still answer.
void a_public_lobbys_start_goes_untouched() {
  give();
  privateAnswer = false;
  startAllowedAnswer = false;  // nothing in a public lobby asks this
  sendAnswer = static_cast<int>(gate::Send::Held);
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::AsSent);
  CHECK(sendAsks == 0 && startAsks == 0);
  privateAnswer = true;
  privateThrows = true;
  startAllowedAnswer = true;
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Held);
  CHECK(sendAsks == 1);
}

// The send is refused too: the start can wait on a task while a player joins, and a future build may start another way.
void the_send_itself_is_refused() {
  give();
  startAllowedAnswer = false;
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Refused);
  // Any other RPC goes as the game sent it, whatever the room.
  verifiedAnswer = false;
  CHECK(gate::decide_rpc(&journalView, name_of("NetworkedReady"), nullptr) == gate::Send::AsSent);
}

// The page arrows send the page sync from their own copy of the code, and never call GoToPage (decompiled). So the RPC itself is
// judged, by its name and its page, with nothing raised around it, on the view it goes out on.
void the_page_sync_is_judged_by_its_page_alone() {
  give();
  int32_t lastGamePage = kBookPages - 1, firstCustomPage = kBookPages;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&lastGamePage)) == gate::Send::AsSent);
  CHECK(customPageView == &journalView);
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&firstCustomPage)) == gate::Send::NotBuffered);
  verifiedAnswer = false;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&lastGamePage)) == gate::Send::AsSent);
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&firstCustomPage)) == gate::Send::HereAlone);
  // A page that cannot be read could be custom: it stays off the wire, and from the buffer.
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), nullptr) == gate::Send::HereAlone);
  verifiedAnswer = true;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), nullptr) == gate::Send::NotBuffered);
  // No other RPC is asked about, whatever it carries.
  customPageView = nullptr;
  CHECK(gate::decide_rpc(&journalView, name_of("NetworkedReady"), marked(&firstCustomPage)) == gate::Send::AsSent);
  CHECK(customPageView == nullptr);
}

// While a player is unverified: a turn to a custom page runs here alone, never reaching them, and a mark on a custom row or guess
// does the same. A game's own page and mark go as the game sent them.
void custom_values_held_back_while_a_player_is_unverified() {
  give();
  verifiedAnswer = false;
  int32_t customPage = kBookPages + 1, gamePage = 3;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&customPage)) == gate::Send::HereAlone);
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&gamePage)) == gate::Send::AsSent);
  int32_t customGhost = 31, gameGhost = 6, customEvidence = 8, gameEvidence = 3;
  const char* const mark = "ToEveryone_NotifyOfButtonStateChange_Rpc";
  CHECK(gate::decide_rpc(&ghostSyncView, name_of(mark), marked(&customGhost)) == gate::Send::HereAlone);
  CHECK(gate::decide_rpc(&ghostSyncView, name_of(mark), marked(&gameGhost)) == gate::Send::AsSent);
  CHECK(gate::decide_rpc(&evidenceSyncView, name_of(mark), marked(&customEvidence)) == gate::Send::HereAlone);
  CHECK(gate::decide_rpc(&evidenceSyncView, name_of(mark), marked(&gameEvidence)) == gate::Send::AsSent);
  // A game ghost's number is past the first custom evidence: which sync it is decides what is custom.
  CHECK(gate::decide_rpc(&ghostSyncView, name_of(mark), marked(&customEvidence)) == gate::Send::AsSent);
  // On a view holding neither, it is judged the safe way.
  CHECK(gate::decide_rpc(&journalView, name_of(mark), marked(&customEvidence)) == gate::Send::HereAlone);
  // What cannot be read goes unbuffered nowhere: a check that fails counts as a player unverified.
  verifiedAnswer = true;
  verifiedThrows = true;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&customPage)) == gate::Send::HereAlone);
}

// Once everyone is verified: a custom page goes to everyone, but never buffered, so a later joiner replays no custom number. A
// custom mark goes as sent.
void custom_values_flow_once_everyone_is_verified() {
  give();
  int32_t customPage = kBookPages;
  CHECK(gate::decide_rpc(&journalView, name_of("PageSync"), marked(&customPage)) == gate::Send::NotBuffered);
  int32_t customGhost = 31;
  CHECK(gate::decide_rpc(&ghostSyncView, name_of("ToEveryone_NotifyOfButtonStateChange_Rpc"), marked(&customGhost)) == gate::Send::AsSent);
}

// An allowed start's send is held while the room closes for it, and the suite's answer is what happens to it; a refused start
// never asks. While the start is underway a press starts nothing, and a send is answered by the closer alone, with no second
// check: the held send, let go, goes as the game made it.
void start_is_held_while_the_room_closes() {
  give();
  sendAnswer = static_cast<int>(gate::Send::Held);
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Held);
  CHECK(startAsks == 1 && sendAsks == 1);
  startAllowedAnswer = false;
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Refused);
  CHECK(sendAsks == 1);  // a refused start holds nothing
  // Underway: the press waits, and the send is the closer's to answer.
  pendingAnswer = true;
  startAllowedAnswer = true;
  startAsks = 0;
  CHECK(!gate::press_goes_on(&startButton));
  CHECK(gate::press_goes_on(&otherButton));
  sendAnswer = static_cast<int>(gate::Send::Refused);
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Refused);
  sendAnswer = static_cast<int>(gate::Send::AsSent);
  startAllowedAnswer = false;  // the release is not checked again
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::AsSent);
  CHECK(startAsks == 0);
  // Nothing is held here (the hook was not made): there is nothing to let go.
  CHECK(!gate::holding() && !gate::release_held());
}

// Without any of these a contract's start could not be held, so no mod starts: the host's gate names the key.
void the_starts_entries_are_required() {
  give();
  const pm::sdk::EntryDesc* const needed[] = {&map::ServerManager_startGameButton, &map::Button_Press, &map::PhotonView_RPC,
                                              &map::ServerManager_LoadScene, &map::PhotonNetwork_get_CurrentRoom,
                                              &map::PhotonRoom_set_IsOpen};
  for (const pm::sdk::EntryDesc* without : needed) {
    fake::reset();
    fake::type(map::ServerManager, &buttonClass);
    fake::type(map::Button, &buttonClass);
    fake::type(map::PhotonView, &buttonClass);
    fake::type(map::PhotonNetwork, &buttonClass);
    fake::type(map::PhotonRoom, &buttonClass);
    fake::field(map::ServerManager_startGameButton, 0x10);
    fake::method(map::Button_Press, reinterpret_cast<void*>(&is_start));
    fake::method(map::PhotonView_RPC, reinterpret_cast<void*>(&is_start));
    fake::method(map::ServerManager_LoadScene, reinterpret_cast<void*>(&is_start));
    fake::method(map::PhotonNetwork_get_CurrentRoom, reinterpret_cast<void*>(&is_start));
    fake::method(map::PhotonRoom_set_IsOpen, reinterpret_cast<void*>(&is_start));
    CHECK([] {
      std::string missing;
      pm::api::bind(fake::host(), missing);
      return gate::missing().empty();
    }());
    fake::forget(*without);
    std::string missing;
    pm::api::bind(fake::host(), missing);
    CHECK(gate::missing().find(without->key) != std::string::npos);
  }
}

// A check that fails refuses the start, and nothing it could not read stops the game's other RPCs.
void a_failed_check_refuses_the_start() {
  give();
  startThrows = true;
  CHECK(!gate::press_goes_on(&startButton));
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Refused);
  gate::Config none;
  gate::create_hooks(none);
  CHECK(gate::decide_rpc(&journalView, name_of("LoadScene"), nullptr) == gate::Send::Refused);  // no check given: no start
  CHECK(gate::press_goes_on(&startButton));  // nothing says it is the start; the send still answers for it
}

}  // namespace

int main() {
  start_waits_for_an_unverified_player();
  start_goes_once_everyone_is_verified();
  a_public_lobbys_start_goes_untouched();
  the_send_itself_is_refused();
  the_page_sync_is_judged_by_its_page_alone();
  a_failed_check_refuses_the_start();
  custom_values_held_back_while_a_player_is_unverified();
  custom_values_flow_once_everyone_is_verified();
  the_starts_entries_are_required();
  start_is_held_while_the_room_closes();
  std::printf(g_failures ? "%d check(s) failed\n" : "all lobby gate tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
