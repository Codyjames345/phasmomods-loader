// Offline tests for whether this client's journal, and with it the pause screen, is open (sdk/api/journal.h): over
// the fake backend, the answer is JournalController.open of the local investigator's Player.journal, and nothing
// without an investigator or a journal. Run: build\journal_open_test.exe
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>

#include "JournalController.h"
#include "Player.h"
#include "check.h"
#include "fake_game.h"
#include "journal.h"
#include "runtime.h"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
}

namespace {

namespace map = pm::map;
namespace fake = pm::fake;

struct Obj {
  void* klass = nullptr;
  unsigned char bytes[0x20]{};
};
template <class T>
T& at(void* o, int32_t offset) {
  return *reinterpret_cast<T*>(static_cast<unsigned char*>(o) + offset);
}

constexpr int32_t kNetPlayer = 0x08;      // Network.localPlayer
constexpr int32_t kPlayerJournal = 0x08;  // Player.journal
constexpr int32_t kOpen = 0x08;           // JournalController.open

Obj g_net, g_player, g_journal;

void* fake_network(void*) { return &g_net; }

void give() {
  fake::reset();
  g_net = Obj{};
  g_player = Obj{};
  g_journal = Obj{};
  fake::method(map::Network_get_Instance, reinterpret_cast<void*>(&fake_network));
  fake::field(map::Network_localPlayer, kNetPlayer);
  fake::field(map::Player_journal, kPlayerJournal);
  fake::field(map::JournalController_open, kOpen);
  std::string missing;
  pm::api::bind(fake::host(), missing);
  at<void*>(&g_net, kNetPlayer) = &g_player;
  at<void*>(&g_player, kPlayerJournal) = &g_journal;
}

void an_open_journal_reads_open() {
  give();
  at<bool>(&g_journal, kOpen) = true;
  CHECK(pm::api::journal::open() == true);
}

void a_closed_journal_reads_closed() {
  give();
  at<bool>(&g_journal, kOpen) = false;
  CHECK(pm::api::journal::open() == false);
}

void no_investigator_gives_nothing() {
  give();
  at<void*>(&g_net, kNetPlayer) = nullptr;
  CHECK(!pm::api::journal::open().has_value());
}

void no_journal_gives_nothing() {
  give();
  at<void*>(&g_player, kPlayerJournal) = nullptr;
  CHECK(!pm::api::journal::open().has_value());
}

void a_destroyed_journal_gives_nothing() {
  give();
  at<bool>(&g_journal, kOpen) = true;
  fake::destroy(&g_journal);
  CHECK(!pm::api::journal::open().has_value());
}

}  // namespace

int main() {
  an_open_journal_reads_open();
  a_closed_journal_reads_closed();
  no_investigator_gives_nothing();
  no_journal_gives_nothing();
  a_destroyed_journal_gives_nothing();
  std::printf("%s\n", g_failures ? "journal open tests FAILED" : "journal open tests passed");
  return g_failures ? 1 : 0;
}
