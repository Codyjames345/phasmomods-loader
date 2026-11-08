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
