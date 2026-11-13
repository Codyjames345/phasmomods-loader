#pragma once

// Unity's events (UnityEngine.CoreModule, namespace UnityEngine.Events): the UnityEvent a button's click runs and the
// UnityAction a listener is. Not obfuscated, so every entry is by real name. The host makes the loader's menu controls
// with them: a copy of a game button gets a new event, with the loader's own action as its one listener
// (host/menu_panel.h, sdk/systems/menus.md). Notes read from decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// Unity's event with no arguments (UnityEngine.CoreModule, namespace UnityEngine.Events): what a Button's onClick and
/// a Doozy behaviour's ModyEvent.Event are. Not the generic UnityEvent<T0> and its kin, which share its name.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityEvent{.key = "UnityEvent", .flags = pm::sdk::kNotGeneric, .name = "UnityEvent",
                                               .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine.Events"};

/// A new event with no listeners: its base makes the lists that hold them, with the persistent ones to be read
/// at the first Invoke. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityEvent_ctor{.key = "UnityEvent.ctor", .kind = pm::sdk::Kind::Method,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "UnityEvent", .name = ".ctor"};

/// Adds a runtime listener: a new InvokableCall around the action, at the end of the event's runtime calls. A
/// runtime listener is never saved with the scene, so a copy of an object (Object.Instantiate) does not have its
/// original's. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityEvent_AddListener{.key = "UnityEvent.AddListener",
                                                           .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "UnityEvent", .name = "AddListener"};

/// Runs every listener in order: first the persistent ones (from the scene, rebuilt when they changed), then the
/// runtime ones. A listener whose target is a destroyed Unity object is skipped. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityEvent_Invoke{.key = "UnityEvent.Invoke", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "UnityEvent", .name = "Invoke"};

/// The base of Unity's events (UnityEngine.CoreModule, namespace UnityEngine.Events). Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc UnityEventBase{.key = "UnityEventBase", .name = "UnityEventBase",
                                                   .assembly = "UnityEngine.CoreModule",
                                                   .ns = "UnityEngine.Events"};

/// How many persistent listeners the event has: those saved with the scene or prefab (the inspector's list). This
/// build has no runtime way to remove one (UnityEventBase's SetPersistentListenerState is stripped, and
/// RemoveAllListeners clears the runtime listeners only), so an object that must not run them gets a new event.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc UnityEventBase_GetPersistentEventCount{
    .key = "UnityEventBase.GetPersistentEventCount", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "UnityEventBase", .name = "GetPersistentEventCount"};

/// The event's listeners saved with the scene or prefab, a PersistentCallGroup. With GetPersistentTarget stripped from
/// this build, reading it is how a mod finds what a saved listener calls (its targets). Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc UnityEventBase_m_PersistentCalls{
    .key = "UnityEventBase.m_PersistentCalls", .kind = pm::sdk::Kind::Field, .owner = "UnityEventBase",
    .name = "m_PersistentCalls", .type = "@PersistentCallGroup"};
/// The event's listener lists, an InvokableCallList: the saved listeners as the event built them, and those added at
/// run time. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc UnityEventBase_m_Calls{.key = "UnityEventBase.m_Calls", .kind = pm::sdk::Kind::Field,
                                                           .owner = "UnityEventBase", .name = "m_Calls",
                                                           .type = "@InvokableCallList"};
/// An event's listener lists (UnityEngine.CoreModule, namespace UnityEngine.Events). Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc InvokableCallList{.key = "InvokableCallList", .name = "InvokableCallList",
                                                      .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine.Events"};
/// The listeners added at run time (UnityEvent.AddListener, and a game's own adders such as
/// PhotonObjectInteract.AddUseEvent), a List<BaseInvokableCall>, in the order they were added. Inferred: Unity's
/// source.
inline constexpr pm::sdk::EntryDesc InvokableCallList_m_RuntimeCalls{
    .key = "InvokableCallList.m_RuntimeCalls", .kind = pm::sdk::Kind::Field, .owner = "InvokableCallList",
    .name = "m_RuntimeCalls", .type = "List<BaseInvokableCall>"};
/// A listener of no arguments added at run time (UnityEngine.CoreModule, namespace UnityEngine.Events): not the
/// generic InvokableCall<T0> and its kin. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc InvokableCall{.key = "InvokableCall", .flags = pm::sdk::kNotGeneric,
                                                  .name = "InvokableCall", .assembly = "UnityEngine.CoreModule",
                                                  .ns = "UnityEngine.Events"};
/// The UnityAction the listener calls. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc InvokableCall_Delegate{.key = "InvokableCall.Delegate", .kind = pm::sdk::Kind::Field,
                                                           .owner = "InvokableCall", .name = "Delegate"};
/// .NET's delegate base (mscorlib, namespace System). Inferred: the runtime's source.
inline constexpr pm::sdk::EntryDesc Delegate{.key = "Delegate", .name = "Delegate", .assembly = "mscorlib",
                                             .ns = "System"};
/// The object a delegate calls its method on, or null for a static method. Inferred: the runtime's source.
inline constexpr pm::sdk::EntryDesc Delegate_m_target{.key = "Delegate.m_target", .kind = pm::sdk::Kind::Field,
                                                      .owner = "Delegate", .name = "m_target", .type = "object"};
/// An event's saved listeners (UnityEngine.CoreModule, namespace UnityEngine.Events). Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc PersistentCallGroup{.key = "PersistentCallGroup", .name = "PersistentCallGroup",
                                                        .assembly = "UnityEngine.CoreModule",
                                                        .ns = "UnityEngine.Events"};
/// The saved listeners themselves, a List<PersistentCall>, in the inspector's order. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc PersistentCallGroup_m_Calls{
    .key = "PersistentCallGroup.m_Calls", .kind = pm::sdk::Kind::Field, .owner = "PersistentCallGroup",
    .name = "m_Calls", .type = "List<@PersistentCall>"};
/// One saved listener (UnityEngine.CoreModule, namespace UnityEngine.Events). Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc PersistentCall{.key = "PersistentCall", .name = "PersistentCall",
                                                   .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine.Events"};
/// The Unity object the saved listener calls its method on. Inferred: Unity's source.
inline constexpr pm::sdk::EntryDesc PersistentCall_m_Target{.key = "PersistentCall.m_Target",
                                                            .kind = pm::sdk::Kind::Field, .owner = "PersistentCall",
                                                            .name = "m_Target"};
/// Unity's delegate with no arguments (UnityEngine.CoreModule, namespace UnityEngine.Events): a UnityEvent's
/// listener. Not the generic UnityAction<T0> and its kin. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityAction{.key = "UnityAction", .flags = pm::sdk::kNotGeneric,
                                                .name = "UnityAction", .assembly = "UnityEngine.CoreModule",
                                                .ns = "UnityEngine.Events"};

/// The delegate's constructor, (object target, IntPtr method), as every delegate's: it keeps the target and the
/// method, and binds the method's code to the target, refusing a null target for an instance method. Invoke then
/// calls that code with the target and the method. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityAction_ctor{.key = "UnityAction.ctor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "UnityAction", .name = ".ctor"};

/// The delegate's Invoke, of no arguments. The host's listeners (host/runtime_services.h delegate_new) take their
/// method's name, class and types from it, since a delegate's method is the runtime's to read. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityAction_Invoke{.key = "UnityAction.Invoke", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "UnityAction", .name = "Invoke"};

}  // namespace pm::map

/// @file
/// @brief The game API for Unity's events (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's event with no arguments. `event` below is one.
struct UnityEvent {
  /// The type: make() for a new event (then ctor), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::UnityEvent> type{};
  /// ctor(event): makes a new event's listener lists. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityEvent_ctor, void(void*)> ctor{};
  /// AddListener(event, action): action a UnityAction. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityEvent_AddListener, void(void*, void*)> AddListener{};
  /// Invoke(event): runs its listeners. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityEvent_Invoke, void(void*)> Invoke{};
};

/// The base of Unity's events. `event` below is one.
struct UnityEventBase {
  /// GetPersistentEventCount(event): its listeners saved with the scene. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::UnityEventBase_GetPersistentEventCount, int32_t(void*)>
      GetPersistentEventCount{};
  /// Its saved listeners, a PersistentCallGroup: m_PersistentCalls.get(event). Unresolved: null.
  static constexpr pm::api::Field<pm::map::UnityEventBase_m_PersistentCalls, void*> m_PersistentCalls{};
  /// Its listener lists, an InvokableCallList: m_Calls.get(event). Unresolved: null.
  static constexpr pm::api::Field<pm::map::UnityEventBase_m_Calls, void*> m_Calls{};
};

/// An event's listener lists. `lists` below is one.
struct InvokableCallList {
  /// The listeners added at run time, a List<BaseInvokableCall>: m_RuntimeCalls.get(lists), read as a
  /// pm::api::List. Unresolved: null.
  static constexpr pm::api::Field<pm::map::InvokableCallList_m_RuntimeCalls, void*> m_RuntimeCalls{};
};

/// A run-time listener of no arguments. `call` below is one; check InvokableCall::type.is(call) first, since a list
/// holds the generic kinds too.
struct InvokableCall {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::InvokableCall> type{};
  /// The UnityAction it calls: Delegate.get(call). Unresolved: null.
  static constexpr pm::api::Field<pm::map::InvokableCall_Delegate, void*> Delegate{};
};

/// .NET's delegate base. `d` below is any delegate.
struct Delegate {
  /// The object it calls its method on, or null for a static method: m_target.get(d). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Delegate_m_target, void*> m_target{};
};

/// An event's saved listeners. `group` below is one.
struct PersistentCallGroup {
  /// The listeners, a List<PersistentCall>: m_Calls.get(group), read as a pm::api::List. Unresolved: null.
  static constexpr pm::api::Field<pm::map::PersistentCallGroup_m_Calls, void*> m_Calls{};
};

/// One saved listener. `call` below is one.
struct PersistentCall {
  /// The Unity object it calls its method on: m_Target.get(call). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PersistentCall_m_Target, void*> m_Target{};
};

/// Unity's delegate with no arguments.
struct UnityAction {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::UnityAction> type{};
  /// ctor(action, target, method): as a delegate's, for a method the runtime made. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityAction_ctor, void(void*, void*, const void*)> ctor{};
  /// Invoke(action). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityAction_Invoke, void(void*)> Invoke{};
};

/// @}
}  // namespace phasmo
