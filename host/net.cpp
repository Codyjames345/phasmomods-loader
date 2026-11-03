#include "net.h"

#include <cstring>

#include "Photon.h"
#include "contained.h"
#include "game_api.h"
#include "hooks.h"
#include "host_game.h"
#include "log.h"
#include "runtime.h"
#include "runtime_services.h"

namespace pm::hostnet {
namespace {

using phasmo::EventData;
using phasmo::PhotonNetwork;
using phasmo::RaiseEventOptions;

constexpr size_t kMaxPayload = PM_MAX_MESSAGE + 1;  // the channel byte, then the message

PmReceiver g_receivers[256] = {};
Gate g_gate = nullptr;
using OnEvent_t = void (*)(void* client, void* eventData, const void* method);
OnEvent_t g_onEvent = nullptr;

// Whether a message can be read as the suite's. Without each of these an event of the suite's code stays unread: it is not
// the game's either, so it is dropped rather than handed to a receiver with a sender it cannot name.
bool can_read() {
  return api::all_resolved<EventData::get_Sender, EventData::get_CustomData, phasmo::Byte::type>();
}

void detour_on_event(void* client, void* eventData, const void* method) {
  hooks::Guard guard;
  // Before the host's bind, and in a build without the field, the code cannot be read: the event is the game's.
  if (eventData && api::all_resolved<EventData::Code>() && EventData::Code.get(eventData) == kEventCode) {
    try {
      void* data = can_read() ? EventData::get_CustomData(eventData) : nullptr;
      if (data && runtime_services::is_array_of(data, phasmo::Byte::type.desc.key)) {
        const api::Array<uint8_t> payload{data};
        const size_t length = payload.size();
        if (length >= 1 && length <= kMaxPayload) {
          const uint8_t* bytes = &payload[0];
          const int32_t sender = EventData::get_Sender(eventData);
          PmReceiver receiver = g_receivers[bytes[0]];
          if (receiver && (bytes[0] == kHostChannel || (g_gate && g_gate(bytes[0], sender)))) receiver(sender, bytes + 1, length - 1);
        }
      }
    } catch (...) {
      PM_LOG("net: exception while reading a mod event");
    }
    return;  // mod events are not game events; do not dispatch them to game callbacks
  }
  g_onEvent(client, eventData, method);
}

}  // namespace

bool create_hooks() {
  const PmGameEntry* e = api::host_only::game_entry(map::LoadBalancingClient_OnEvent.key);
  if (!e || !e->code) {
    PM_LOG("net: %s did not resolve; the suite's messages are off", map::LoadBalancingClient_OnEvent.key);
    return true;  // the game runs as it would without us: available() stays false
  }
  return hooks::create(e->code, reinterpret_cast<void*>(&detour_on_event), reinterpret_cast<void**>(&g_onEvent),
                       map::LoadBalancingClient_OnEvent.key);
}

std::string missing() {
  namespace P = phasmo;
  std::string out = api::host_only::game_entry(map::LoadBalancingClient_OnEvent.key) ? "" : map::LoadBalancingClient_OnEvent.key;
  const std::string entries =
      hostgame::unresolved<EventData::get_Sender, EventData::get_CustomData, P::Byte::type, EventData::Code,
                           PhotonNetwork::RaiseEvent, RaiseEventOptions::type, RaiseEventOptions::ctor,
                           RaiseEventOptions::TargetActors, RaiseEventOptions::Receivers, P::ReceiverGroup::Others,
                           P::ReceiverGroup::MasterClient, P::ReceiverGroup::All, P::DeliveryMode::Unreliable,
                           P::DeliveryMode::Reliable,
                           P::SendOptions::type, P::SendOptions::DeliveryMode, P::Int32::type>();
  if (!entries.empty()) out += (out.empty() ? "" : ", ") + entries;
  return out;
}

bool available() { return g_onEvent && missing().empty(); }

void set_receiver(uint8_t channel, PmReceiver receiver) { g_receivers[channel] = receiver; }

void set_gate(Gate gate) { g_gate = gate; }

namespace {

// Raises the message, `address` writing who it goes to in the options. False, with nothing sent, when `address` is.
template <typename Address>
bool raise(uint8_t channel, const uint8_t* data, size_t size, bool reliable, Address address) {
  // Who it goes to is written by `address`, and a default standing in for an entry that did not resolve would send it elsewhere.
  if (size > PM_MAX_MESSAGE || !available()) return false;
  void* payload = phasmo::Byte::type.make_array(size + 1);
  void* options = RaiseEventOptions::type.make();
  if (!payload || !options) return false;
  const api::Array<uint8_t> bytes{payload};
  if (bytes.size() != size + 1) return false;
  uint8_t* out = &bytes[0];
  out[0] = channel;
  if (size) std::memcpy(out + 1, data, size);
  RaiseEventOptions::ctor(options);
  if (!address(options)) return false;
  // SendOptions is 8 bytes (its entry's exact fields) and passed by value, its other fields zero.
  uint64_t sendOptions = 0;
  phasmo::SendOptions::DeliveryMode.set(&sendOptions,
                                        reliable ? phasmo::DeliveryMode::Reliable() : phasmo::DeliveryMode::Unreliable());
  return PhotonNetwork::RaiseEvent(kEventCode, payload, options, sendOptions);
}

}  // namespace

bool send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor) {
  return raise(channel, data, size, reliable, [&](void* options) {
    if (to == PM_TO_ACTOR) {
      void* targets = phasmo::Int32::type.make_array(1);
      if (!targets) return false;
      api::Array<int32_t>{targets}[0] = actor;
      RaiseEventOptions::TargetActors.set(options, targets);
    } else {
      const int32_t group = to == PM_TO_MASTER ? phasmo::ReceiverGroup::MasterClient() : phasmo::ReceiverGroup::Others();
      RaiseEventOptions::Receivers.set(options, static_cast<uint8_t>(group));
    }
    return true;
  });
}

bool send_through_server_to_all(uint8_t channel, const uint8_t* data, size_t size) {
  return raise(channel, data, size, true, [](void* options) {
    RaiseEventOptions::Receivers.set(options, static_cast<uint8_t>(phasmo::ReceiverGroup::All()));
    return true;
  });
}

bool send_to_verified(uint8_t channel, const uint8_t* data, size_t size, bool reliable, const std::vector<int>& verified, bool everyone) {
  if (everyone) return send(channel, data, size, PM_TO_OTHERS, reliable, 0);
  bool sent = true;
  for (int actor : verified) sent = send(channel, data, size, PM_TO_ACTOR, reliable, actor) && sent;
  return sent;
}

}  // namespace pm::hostnet
