// Offline test of the suite's messages (host/net.cpp) over the fake game: who a message goes to and how it is delivered,
// that nothing is sent while an entry it needs did not resolve (a default standing in would send it elsewhere), and what the
// hook on LoadBalancingClient.OnEvent does with the game's events and the suite's. Run: build\net_test.exe
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "Photon.h"
#include "check.h"
#include "fake_game.h"
#include "game_api.h"
#include "log.h"
#include "net.h"
#include "runtime.h"
#include "runtime_services.h"

namespace {
namespace fake = pm::fake;
namespace map = pm::map;

int byteClass, intClass, optionsClass;
constexpr int32_t kTargetActors = 0x18, kReceivers = 0x20;
constexpr int kOthers = 7, kMaster = 9, kAll = 11;  // not the build's numbers: what is sent comes from the map
constexpr int kUnreliable = 3, kReliable = 5;
constexpr int32_t kDelivery = 4;            // not at the struct's start, so the offset is the map's

// What the fake RaiseEvent last got.
int raises = 0;
uint8_t raisedCode = 0;
std::vector<uint8_t> raisedBytes;
void* raisedOptions = nullptr;
uint64_t raisedSend = 0;
bool raiseAnswer = true;
int ctors = 0;

bool raise_event(uint8_t code, void* payload, void* options, uint64_t sendOptions, const void*) {
  ++raises;
  raisedCode = code;
  const pm::api::Array<uint8_t> bytes{payload};
  raisedBytes.assign(&bytes[0], &bytes[0] + bytes.size());
  raisedOptions = options;
  raisedSend = sendOptions;
  return raiseAnswer;
}
void options_ctor(void*, const void*) { ++ctors; }

struct FakeEvent {
  void* klass = nullptr;
  uint8_t code = 0;
  void* data = nullptr;
  int32_t sender = 0;
};
constexpr int32_t kCode = offsetof(FakeEvent, code);
int32_t get_sender(void* e, const void*) { return static_cast<FakeEvent*>(e)->sender; }
void* get_data(void* e, const void*) { return static_cast<FakeEvent*>(e)->data; }

int originals = 0;
void original(void*, void*, const void*) { ++originals; }

int received = 0, receivedSender = 0;
std::vector<uint8_t> receivedBytes;
void receiver(int32_t sender, const uint8_t* data, size_t size) {
  ++received;
  receivedSender = sender;
  receivedBytes.assign(data, data + size);
}

void* g_onEventCode = reinterpret_cast<void*>(&original);
void* hookedTarget = nullptr;
bool hookAnswer = true;
}  // namespace

#include "../host/net.cpp"

namespace pm::hooks {
std::atomic<int> g_inflight{0};
bool create(void* target, void* detour, void** out, const char*) {
  hookedTarget = target;
  CHECK(detour == reinterpret_cast<void*>(&hostnet::detour_on_event));
  if (hookAnswer) *out = reinterpret_cast<void*>(&original);
  return hookAnswer;
}
}  // namespace pm::hooks

namespace pm::runtime_services {
// The real one compares the object's class with T[] through the runtime; the fake's arrays carry no class.
const void* g_byteArray = nullptr;
bool is_array_of(const void* obj, const char* key) { return obj == g_byteArray && std::strcmp(key, "Byte") == 0; }
}  // namespace pm::runtime_services

const PmGameEntry* pm::api::host_only::game_entry(const char* key) { return fake::host_entry(key); }

namespace {

using namespace pm;

// The entries a send and a receive need, every one given to give().
const pm::sdk::EntryDesc* const kEntries[] = {
    &map::PhotonNetwork_RaiseEvent, &map::RaiseEventOptions, &map::RaiseEventOptions_ctor,
    &map::RaiseEventOptions_TargetActors, &map::RaiseEventOptions_Receivers, &map::ReceiverGroup_Others,
    &map::ReceiverGroup_MasterClient, &map::ReceiverGroup_All, &map::DeliveryMode_Unreliable, &map::DeliveryMode_Reliable,
    &map::SendOptions,
    &map::SendOptions_DeliveryMode, &map::EventData_Code, &map::EventData_get_Sender, &map::EventData_get_CustomData,
    &map::Byte, &map::Int32};

void give(const pm::sdk::EntryDesc* without = nullptr) {
  fake::reset();
  fake::method(map::PhotonNetwork_RaiseEvent, reinterpret_cast<void*>(&raise_event));
  fake::type(map::RaiseEventOptions, &optionsClass);
  fake::method(map::RaiseEventOptions_ctor, reinterpret_cast<void*>(&options_ctor));
  fake::field(map::RaiseEventOptions_TargetActors, kTargetActors);
  fake::field(map::RaiseEventOptions_Receivers, kReceivers);
  fake::value(map::ReceiverGroup_Others, kOthers);
  fake::value(map::ReceiverGroup_MasterClient, kMaster);
  fake::value(map::ReceiverGroup_All, kAll);
  fake::value(map::DeliveryMode_Unreliable, kUnreliable);
  fake::value(map::DeliveryMode_Reliable, kReliable);
  static int sendOptionsClass;
  fake::type(map::SendOptions, &sendOptionsClass);
  fake::field(map::SendOptions_DeliveryMode, kDelivery);
  fake::field(map::EventData_Code, kCode);
  fake::method(map::EventData_get_Sender, reinterpret_cast<void*>(&get_sender));
  fake::method(map::EventData_get_CustomData, reinterpret_cast<void*>(&get_data));
  fake::type(map::Byte, &byteClass);
  fake::type(map::Int32, &intClass);
  fake::method(map::LoadBalancingClient_OnEvent, g_onEventCode);
  if (without) fake::forget(*without);
  std::string missing;
  api::bind(fake::host(), missing);
  hookAnswer = true;
  hostnet::g_onEvent = nullptr;
  CHECK(hostnet::create_hooks());
  raises = ctors = 0;
  raisedBytes.clear();
  raisedOptions = nullptr;
  raiseAnswer = true;
}

uint8_t receivers_of(void* options) { return *(static_cast<uint8_t*>(options) + kReceivers); }
void* targets_of(void* options) { return *reinterpret_cast<void**>(static_cast<char*>(options) + kTargetActors); }

void send_writes_who_and_how() {
  give();
  CHECK(hostnet::available());
  CHECK(hostnet::missing().empty());
  const uint8_t message[] = {1, 2, 3};
  CHECK(hostnet::send(5, message, sizeof message, PM_TO_OTHERS, true, 0));
  CHECK(raises == 1 && raisedCode == hostnet::kEventCode && ctors == 1);
  CHECK((raisedBytes == std::vector<uint8_t>{5, 1, 2, 3}));  // the channel, then the message
  CHECK(receivers_of(raisedOptions) == kOthers && !targets_of(raisedOptions));
  uint64_t reliable = 0;
  std::memcpy(reinterpret_cast<char*>(&reliable) + kDelivery, &kReliable, sizeof kReliable);
  CHECK(raisedSend == reliable);

  CHECK(hostnet::send(5, message, sizeof message, PM_TO_MASTER, false, 0));
  CHECK(receivers_of(raisedOptions) == kMaster);
  uint64_t unreliable = 0;
  std::memcpy(reinterpret_cast<char*>(&unreliable) + kDelivery, &kUnreliable, sizeof kUnreliable);
  CHECK(raisedSend == unreliable);

  CHECK(hostnet::send(5, nullptr, 0, PM_TO_ACTOR, true, 42));
  CHECK((raisedBytes == std::vector<uint8_t>{5}));
  void* targets = targets_of(raisedOptions);
  CHECK(targets && api::Array<int32_t>{targets}[0] == 42);

  raiseAnswer = false;  // PUN refused it
  CHECK(!hostnet::send(5, nullptr, 0, PM_TO_OTHERS, true, 0));
}

// The closed-room start's marker goes where the game's via-server sends go, reliably, and names no target list.
void through_the_server_goes_to_all_reliably() {
  give();
  const uint8_t message[] = {1, 2};
  CHECK(hostnet::send_through_server_to_all(5, message, sizeof message));
  CHECK(raises == 1 && (raisedBytes == std::vector<uint8_t>{5, 1, 2}));
  CHECK(receivers_of(raisedOptions) == kAll && !targets_of(raisedOptions));
  uint64_t reliable = 0;
  std::memcpy(reinterpret_cast<char*>(&reliable) + kDelivery, &kReliable, sizeof kReliable);
  CHECK(raisedSend == reliable);
  raiseAnswer = false;  // PUN refused it
  CHECK(!hostnet::send_through_server_to_all(5, message, sizeof message));
}

void too_long_is_refused() {
  give();
  std::vector<uint8_t> big(PM_MAX_MESSAGE + 1);
  CHECK(!hostnet::send(5, big.data(), big.size(), PM_TO_OTHERS, true, 0));
  CHECK(raises == 0);
}

// A receivers number left at its default sends to the room's others what was meant for the master, and no target list
// sends to everyone what was meant for one player: nothing may be raised with an entry missing.
void each_entry_missing_sends_nothing() {
  for (const pm::sdk::EntryDesc* d : kEntries) {
    give(d);
    CHECK(!hostnet::available());
    CHECK(hostnet::missing().find(d->key) != std::string::npos);  // the start gate's line names it
    CHECK(!hostnet::send(5, nullptr, 0, PM_TO_MASTER, true, 0));
    CHECK(!hostnet::send(5, nullptr, 0, PM_TO_ACTOR, true, 3));
    CHECK(raises == 0);
    if (raises) std::printf("  sent without %s\n", d->key);
  }
}

void no_hook_no_messages() {
  give(&map::LoadBalancingClient_OnEvent);  // unresolved: no hook, and the host still starts
  CHECK(hostnet::g_onEvent == nullptr);
  CHECK(!hostnet::available());
  CHECK(hostnet::missing() == std::string(map::LoadBalancingClient_OnEvent.key));
  CHECK(!hostnet::send(5, nullptr, 0, PM_TO_OTHERS, true, 0));
  CHECK(raises == 0);
  give();
  CHECK(hookedTarget == g_onEventCode);
  hookAnswer = false;
  hostnet::g_onEvent = nullptr;
  CHECK(!hostnet::create_hooks());  // a resolved method it could not hook is a failure
  CHECK(!hostnet::available());
}

// A mod's message goes as one to the others when everyone is verified, else to each verified player alone, and to nobody else.
void a_mods_messages_go_only_to_verified_players() {
  give();
  const uint8_t message[] = {1};
  CHECK(hostnet::send_to_verified(5, message, sizeof message, true, {2, 3}, true));
  CHECK(raises == 1);
  CHECK(!phasmo::RaiseEventOptions::TargetActors.get(raisedOptions));
  raises = 0;
  CHECK(hostnet::send_to_verified(5, message, sizeof message, true, {2, 4}, false));
  CHECK(raises == 2);
  const api::Array<int32_t> last{phasmo::RaiseEventOptions::TargetActors.get(raisedOptions)};
  CHECK(last.size() == 1 && last[0] == 4);
  raises = 0;
  CHECK(hostnet::send_to_verified(5, message, sizeof message, true, {}, false));  // nobody to send to is no failure
  CHECK(raises == 0);
  raiseAnswer = false;
  CHECK(!hostnet::send_to_verified(5, message, sizeof message, true, {2}, false));
  raiseAnswer = true;
}

// The notice is asked before the hook is made: what the build lacks is about its entries alone, and a hook not made yet is
// no lack. No message goes out until it is made, so a client that has not agreed sends nothing.
void missing_is_asked_before_the_hook() {
  give();
  hostnet::g_onEvent = nullptr;
  CHECK(hostnet::missing().empty());
  CHECK(!hostnet::available());
  CHECK(!hostnet::send(5, nullptr, 0, PM_TO_OTHERS, true, 0));
  CHECK(raises == 0);
  give();
  CHECK(hostnet::available());
}

FakeEvent mod_event(int sender, const std::vector<uint8_t>& bytes) {
  void* arr = fake::array(1, bytes.size());
  if (!bytes.empty()) std::memcpy(&api::Array<uint8_t>{arr}[0], bytes.data(), bytes.size());
  runtime_services::g_byteArray = arr;
  FakeEvent e;
  e.code = hostnet::kEventCode;
  e.data = arr;
  e.sender = sender;
  return e;
}

uint8_t gateChannel = 0;
int32_t gateSender = 0;
bool open_gate(uint8_t, int32_t) { return true; }
bool shut_gate(uint8_t channel, int32_t sender) {
  gateChannel = channel;
  gateSender = sender;
  return false;
}

void the_hook_tells_the_events_apart() {
  give();
  received = originals = 0;
  hostnet::set_receiver(5, &receiver);
  hostnet::set_gate(&open_gate);
  FakeEvent game;
  game.code = 200;
  hostnet::detour_on_event(nullptr, &game, nullptr);
  CHECK(originals == 1 && received == 0);
  hostnet::detour_on_event(nullptr, nullptr, nullptr);
  CHECK(originals == 2 && received == 0);

  FakeEvent e = mod_event(11, {5, 8, 9});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(originals == 2 && received == 1 && receivedSender == 11);
  CHECK((receivedBytes == std::vector<uint8_t>{8, 9}));

  // Not game events, and not the suite's either: dropped whole.
  e = mod_event(11, {6, 8});  // no receiver on channel 6
  hostnet::detour_on_event(nullptr, &e, nullptr);
  e = mod_event(11, {});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  e = mod_event(11, std::vector<uint8_t>(PM_MAX_MESSAGE + 2, 5));
  hostnet::detour_on_event(nullptr, &e, nullptr);
  e = mod_event(11, {5, 8});
  runtime_services::g_byteArray = nullptr;  // not a byte[]
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(originals == 2 && received == 1);
  hostnet::set_receiver(5, nullptr);
  hostnet::set_gate(nullptr);
}

// A mod's channel takes a message only as the gate says, told the channel and the sender; the event is never the game's. The
// host's own channel has no gate, since its receiver reads what any client says about itself.
void a_mods_channel_is_gated() {
  give();
  received = originals = 0;
  hostnet::set_receiver(5, &receiver);
  hostnet::set_receiver(hostnet::kHostChannel, &receiver);
  hostnet::set_gate(nullptr);
  FakeEvent e = mod_event(11, {5, 8});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(received == 0 && originals == 0);  // no gate: dropped
  e = mod_event(11, {hostnet::kHostChannel, 8});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(received == 1);
  received = 0;
  hostnet::set_gate(&shut_gate);
  gateChannel = 0;
  e = mod_event(11, {5, 8});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(received == 0 && originals == 0 && gateChannel == 5 && gateSender == 11);
  e = mod_event(11, {hostnet::kHostChannel, 8});
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(received == 1);
  hostnet::set_receiver(5, nullptr);
  hostnet::set_receiver(hostnet::kHostChannel, nullptr);
  hostnet::set_gate(nullptr);
}

// Whatever cannot read the code leaves the event to the game; whatever cannot name the sender never reaches a receiver.
void the_hook_with_entries_missing() {
  give(&map::EventData_Code);
  received = originals = 0;
  hostnet::set_receiver(5, &receiver);
  FakeEvent e = mod_event(11, {5, 8});
  hostnet::g_onEvent = &original;
  hostnet::detour_on_event(nullptr, &e, nullptr);
  CHECK(originals == 1 && received == 0);
  for (const pm::sdk::EntryDesc* d : {&map::EventData_get_Sender, &map::EventData_get_CustomData, &map::Byte}) {
    give(d);
    originals = 0;
    hostnet::g_onEvent = &original;
    e = mod_event(11, {5, 8});
    hostnet::detour_on_event(nullptr, &e, nullptr);
    CHECK(received == 0 && originals == 0);
  }
  hostnet::set_receiver(5, nullptr);
}

}  // namespace

int main() {
  send_writes_who_and_how();
  through_the_server_goes_to_all_reliably();
  too_long_is_refused();
  each_entry_missing_sends_nothing();
  no_hook_no_messages();
  missing_is_asked_before_the_hook();
  a_mods_messages_go_only_to_verified_players();
  the_hook_tells_the_events_apart();
  a_mods_channel_is_gated();
  the_hook_with_entries_missing();
  std::printf(g_failures ? "%d check(s) failed\n" : "all net tests passed\n", g_failures);
  return g_failures ? 1 : 0;
}
