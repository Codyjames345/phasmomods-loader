#include "net.h"

#include <cstring>

#include "hooks.h"
#include "il2cpp.h"
#include "log.h"

namespace pg::hostnet {
namespace {

constexpr size_t kMaxPayload = PG_MAX_MESSAGE + 1;  // the channel byte, then the message

// Photon.Realtime.RaiseEventOptions fields (PhotonRealtime build in this game).
constexpr size_t kOptTargetActors = 0x18;  // int[]
constexpr size_t kOptReceivers = 0x20;     // ReceiverGroup: Others 0, All 1, MasterClient 2
// ExitGames.Client.Photon.EventData
constexpr size_t kEventCodeOffset = 0x10;  // byte Code

struct Api {
  il2cpp::Class* byteClass = nullptr;
  il2cpp::Class* intClass = nullptr;
  il2cpp::Class* optionsClass = nullptr;
  void (*optionsCtor)(void*, const void*) = nullptr;
  bool (*raiseEvent)(uint8_t, void*, void*, uint64_t, const void*) = nullptr;
  void* onEvent = nullptr;
  int (*sender)(void*, const void*) = nullptr;
  void* (*customData)(void*, const void*) = nullptr;
} api;

PgReceiver g_receivers[256] = {};
using OnEvent_t = void (*)(void* client, void* eventData, const void* method);
OnEvent_t g_onEvent = nullptr;

using il2cpp::bind;

void detour_on_event(void* client, void* eventData, const void* method) {
  hooks::Guard guard;
  if (eventData && il2cpp::field<uint8_t>(eventData, kEventCodeOffset) == kEventCode) {
    try {
      void* data = api.customData(eventData, nullptr);
      const int sender = api.sender(eventData, nullptr);
      if (data && il2cpp::class_of(data) == il2cpp::class_of_array(api.byteClass)) {
        auto* arr = static_cast<il2cpp::Array*>(data);
        if (arr->max_length >= 1 && arr->max_length <= kMaxPayload) {
          const uint8_t* bytes = arr->data<uint8_t>();
          if (PgReceiver receiver = g_receivers[bytes[0]]) receiver(sender, bytes + 1, arr->max_length - 1);
        }
      }
    } catch (...) {
      PG_LOG("net: exception while reading a mod event");
    }
    return;  // mod events are not game events; do not dispatch them to game callbacks
  }
  g_onEvent(client, eventData, method);
}

}  // namespace

bool resolve() {
  api.byteClass = il2cpp::cls("mscorlib.dll", "System", "Byte");
  api.intClass = il2cpp::cls("mscorlib.dll", "System", "Int32");
  api.optionsClass = il2cpp::cls("PhotonRealtime.dll", "Photon.Realtime", "RaiseEventOptions");
  il2cpp::Class* pn = il2cpp::cls("PhotonUnityNetworking.dll", "Photon.Pun", "PhotonNetwork");
  il2cpp::Class* lbc = il2cpp::cls("PhotonRealtime.dll", "Photon.Realtime", "LoadBalancingClient");
  il2cpp::Class* ev = il2cpp::cls("Photon3Unity3D.dll", "ExitGames.Client.Photon", "EventData");
  bool ok = api.byteClass && api.intClass;
  ok &= bind(api.optionsCtor, api.optionsClass, ".ctor", 0);
  ok &= bind(api.raiseEvent, pn, "RaiseEvent", 4);
  ok &= bind(api.onEvent, lbc, "OnEvent", 1);
  ok &= bind(api.sender, ev, "get_Sender", 0);
  ok &= bind(api.customData, ev, "get_CustomData", 0);
  PG_LOG("net: resolve %s", ok ? "ok" : "FAILED");
  return ok;
}

bool create_hooks() {
  return hooks::create(api.onEvent, reinterpret_cast<void*>(&detour_on_event), reinterpret_cast<void**>(&g_onEvent),
                       "LoadBalancingClient.OnEvent");
}

void set_receiver(uint8_t channel, PgReceiver receiver) { g_receivers[channel] = receiver; }

bool send(uint8_t channel, const uint8_t* data, size_t size, int32_t to, bool reliable, int32_t actor) {
  if (size > PG_MAX_MESSAGE) return false;
  il2cpp::Array* payload = il2cpp::array_new(api.byteClass, size + 1);
  il2cpp::Object* options = il2cpp::object_new(api.optionsClass);
  if (!payload || !options) return false;
  payload->data<uint8_t>()[0] = channel;
  if (size) std::memcpy(payload->data<uint8_t>() + 1, data, size);
  api.optionsCtor(options, nullptr);
  if (to == PG_TO_ACTOR) {
    il2cpp::Array* targets = il2cpp::array_new(api.intClass, 1);
    if (!targets) return false;
    targets->data<int32_t>()[0] = actor;
    il2cpp::set_ref_field(options, kOptTargetActors, targets);
  } else {
    il2cpp::set_field<uint8_t>(options, kOptReceivers, to == PG_TO_MASTER ? 2 : 0);
  }
  // SendOptions { DeliveryMode (int) @0, bool Encrypt @4, byte Channel @5 } passed by value.
  const uint64_t sendOptions = reliable ? 1 : 0;
  return api.raiseEvent(kEventCode, payload, options, sendOptions, nullptr);
}

}  // namespace pg::hostnet
