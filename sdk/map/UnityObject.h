#pragma once

// UnityEngine.Object, GameObject, Component and Behaviour: Unity's own base classes of every scene
// object. Not obfuscated, so every entry is by real name. The host's journal (host/content_journal.h)
// clones the game's widgets with them.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* UnityObject_string[] = {"string"};
inline constexpr const char* UnityObject_bool[] = {"bool"};
inline constexpr const char* UnityObject_Type[] = {"Type"};
inline constexpr const char* UnityObject_Type_bool[] = {"Type", "bool"};

/// Unity's base class of every engine object (UnityEngine.CoreModule). Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc UnityObject{.key = "Object", .name = "Object", .assembly = "UnityEngine.CoreModule",
                                                .ns = "UnityEngine"};

/// The native engine object behind the managed one, or null once it is destroyed. Host only: the host's
/// steering reads it (pm::map::kSteeringEntries), and phasmo:: offers no wrapper for it. Inferred: Unity's own
/// field, by its real name.
inline constexpr pm::sdk::EntryDesc UnityObject_m_CachedPtr{.key = "Object.m_CachedPtr", .kind = pm::sdk::Kind::Field,
                                                            .owner = "Object", .name = "m_CachedPtr", .type = "IntPtr"};

/// The object's name: for a component, its GameObject's. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc UnityObject_get_name{.key = "Object.get_name", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Object", .name = "get_name"};

/// Renames the object: for a component, its GameObject. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc UnityObject_set_name{.key = "Object.set_name", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Object", .name = "set_name",
                                                         .params = UnityObject_string};

/// A copy of `data` under the Transform `parent`: for a component, a copy of its whole GameObject with
/// its children, and the copy's component of the same type comes back. References inside the copied
/// hierarchy point into the copy; others stay. With `worldPositionStays` false the copy keeps the
/// original's local position. The copy wakes (Awake, OnEnable) at once when it is active. Static and
/// private: Object.Instantiate(Object, Transform, bool) with a parent is this engine call after its
/// null checks (decompiled; the dump writes UnityEngine.Object there as `object`, so the overload
/// cannot be told apart by its parameters offline). Decompiled.
inline constexpr pm::sdk::EntryDesc UnityObject_Instantiate{
    .key = "Object.Internal_CloneSingleWithParent", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "Object",
    .name = "Internal_CloneSingleWithParent"};

/// The first loaded object of the System.Type given, inactive ones too when `includeInactive`, or null: a search of
/// every loaded object. Static. The custom difficulty's menu sits on a board the game keeps inactive unless the player
/// is on it, so only this overload finds it then. The overload taking only the Type is this one with `includeInactive`
/// false (the same code, decompiled). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityObject_FindObjectOfType{
    .key = "Object.FindObjectOfType", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "Object",
    .name = "FindObjectOfType", .params = UnityObject_Type_bool};

/// Every loaded, active object of the System.Type given, an Object[]: a search of every loaded object.
/// Static. Inferred: Unity's documented API (the overload taking only the Type).
inline constexpr pm::sdk::EntryDesc UnityObject_FindObjectsOfType{
    .key = "Object.FindObjectsOfType", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "Object",
    .name = "FindObjectsOfType", .params = UnityObject_Type};

/// Every loaded object of the System.Type given, inactive ones too when `includeInactive`, an Object[]: a search of every
/// loaded object. Static. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityObject_FindObjectsOfType_inactive{
    .key = "Object.FindObjectsOfType(Type,bool)", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "Object",
    .name = "FindObjectsOfType", .params = UnityObject_Type_bool};

/// Whether an Object reference is alive (not null and not destroyed): C#'s implicit conversion of an
/// Object to bool. Static. The map names it so that facts can find the game methods that call it; a mod
/// tests an object with pm::api::alive. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityObject_op_Implicit{.key = "Object.op_Implicit", .kind = pm::sdk::Kind::Method,
                                                            .flags = pm::sdk::kStatic, .owner = "Object",
                                                            .name = "op_Implicit"};

/// Whether two Object references differ, as Unity counts it (a destroyed object equals null): C#'s
/// operator != on Objects. Static. The map names it so that facts can find the game methods that call
/// it. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc UnityObject_op_Inequality{
    .key = "Object.op_Inequality", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kStatic, .owner = "Object",
    .name = "op_Inequality"};

/// Unity's flags of an object's visibility and lifetime, a mask (UnityEngine.CoreModule). Inferred:
/// Unity's documented API.
inline constexpr pm::sdk::EntryDesc HideFlags{.key = "HideFlags", .name = "HideFlags",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The flag that keeps an asset nothing in a scene uses from being unloaded. Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc HideFlags_DontUnloadUnusedAsset{
    .key = "HideFlags.DontUnloadUnusedAsset", .kind = pm::sdk::Kind::EnumValue, .flags = pm::sdk::kOptional,
    .owner = "HideFlags", .name = "DontUnloadUnusedAsset"};

inline constexpr const char* UnityObject_HideFlags[] = {"HideFlags"};

/// Sets the object's HideFlags. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc UnityObject_set_hideFlags{
    .key = "Object.set_hideFlags", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional, .owner = "Object",
    .name = "set_hideFlags", .params = UnityObject_HideFlags};

inline constexpr const char* UnityObject_Destroy_params[] = {"Object", "float"};

/// Destroys an engine object (a GameObject with its children, or a component) at the end of the
/// frame, or after `t` seconds: the engine function behind Object.Destroy(Object, float), whose own
/// body only resolves and calls it by this name. Decompiled.
inline constexpr pm::sdk::EntryDesc UnityObject_Destroy{
    .key = "Object.Destroy", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .name = "UnityEngine.Object::Destroy(UnityEngine.Object,System.Single)",
    .params = UnityObject_Destroy_params};

inline constexpr const char* UnityObject_DestroyImmediate_params[] = {"Object", "bool"};

/// Destroys an engine object now, before anything else can run on it (assets only when the second
/// argument is true): the engine function behind Object.DestroyImmediate(Object, bool), whose own
/// body only resolves and calls it by this name; DestroyImmediate(Object) calls it with false.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc UnityObject_DestroyImmediate{
    .key = "Object.DestroyImmediate", .kind = pm::sdk::Kind::Icall, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .name = "UnityEngine.Object::DestroyImmediate(UnityEngine.Object,System.Boolean)",
    .params = UnityObject_DestroyImmediate_params};

/// Unity's scene object (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GameObject{.key = "GameObject", .name = "GameObject",
                                               .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The constructor GameObject(name): a new, active, empty GameObject in the scene at the world's
/// origin, with a Transform. Run it on an object the GameObject type made. Inferred: Unity's documented
/// API.
inline constexpr pm::sdk::EntryDesc GameObject_ctor{.key = "GameObject.ctor", .kind = pm::sdk::Kind::Method,
                                                    .owner = "GameObject", .name = ".ctor",
                                                    .params = UnityObject_string};

inline constexpr const char* UnityObject_int[] = {"int"};

/// Puts the GameObject (not its children) on a rendering and physics layer. Inferred: Unity's
/// documented property setter.
inline constexpr pm::sdk::EntryDesc GameObject_set_layer{.key = "GameObject.set_layer", .kind = pm::sdk::Kind::Method,
                                                         .owner = "GameObject", .name = "set_layer",
                                                         .params = UnityObject_int};

/// Every component of the System.Type given on the GameObject, a Component[] (empty when none).
/// Inferred: Unity's documented API (the overload taking a Type).
inline constexpr pm::sdk::EntryDesc GameObject_GetComponents{.key = "GameObject.GetComponents",
                                                             .kind = pm::sdk::Kind::Method, .owner = "GameObject",
                                                             .name = "GetComponents", .params = UnityObject_Type};

/// Adds a new component of the System.Type given to the GameObject, and gives it. Inferred: Unity's
/// documented API (the overload taking a Type).
inline constexpr pm::sdk::EntryDesc GameObject_AddComponent{.key = "GameObject.AddComponent",
                                                            .kind = pm::sdk::Kind::Method, .owner = "GameObject",
                                                            .name = "AddComponent", .params = UnityObject_Type};

/// Switches the GameObject on or off. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc GameObject_SetActive{.key = "GameObject.SetActive", .kind = pm::sdk::Kind::Method,
                                                         .owner = "GameObject", .name = "SetActive",
                                                         .params = UnityObject_bool};

/// The GameObject's Transform. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GameObject_get_transform{
    .key = "GameObject.get_transform", .kind = pm::sdk::Kind::Method, .owner = "GameObject", .name = "get_transform"};

/// Whether the GameObject itself is switched on (its parents aside). Inferred: Unity's documented
/// property getter.
inline constexpr pm::sdk::EntryDesc GameObject_get_activeSelf{
    .key = "GameObject.get_activeSelf", .kind = pm::sdk::Kind::Method, .owner = "GameObject", .name = "get_activeSelf"};

/// Whether the GameObject is active in the scene: it and every parent switched on. Optional: only
/// diagnostics read it. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GameObject_get_activeInHierarchy{
    .key = "GameObject.get_activeInHierarchy", .kind = pm::sdk::Kind::Method, .flags = pm::sdk::kOptional,
    .owner = "GameObject", .name = "get_activeInHierarchy"};

/// Unity's base class of the components of a GameObject (UnityEngine.CoreModule). Inferred: Unity's
/// documented API.
inline constexpr pm::sdk::EntryDesc Component{.key = "Component", .name = "Component",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The component's Transform. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Component_get_transform{
    .key = "Component.get_transform", .kind = pm::sdk::Kind::Method, .owner = "Component", .name = "get_transform"};

/// The component's GameObject. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Component_get_gameObject{
    .key = "Component.get_gameObject", .kind = pm::sdk::Kind::Method, .owner = "Component", .name = "get_gameObject"};

/// The GameObject's first component of the System.Type given, or null. Inferred: Unity's documented
/// API (the overload taking a Type).
inline constexpr pm::sdk::EntryDesc Component_GetComponent{.key = "Component.GetComponent",
                                                           .kind = pm::sdk::Kind::Method, .owner = "Component",
                                                           .name = "GetComponent", .params = UnityObject_Type};

/// The first component of the System.Type given on the GameObject or its children, depth first,
/// inactive ones included when asked, or null. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Component_GetComponentInChildren{
    .key = "Component.GetComponentInChildren", .kind = pm::sdk::Kind::Method, .owner = "Component",
    .name = "GetComponentInChildren", .params = UnityObject_Type_bool};

/// Unity's base class of the components that can be switched on and off (UnityEngine.CoreModule).
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Behaviour{.key = "Behaviour", .name = "Behaviour",
                                              .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Whether the component is switched on. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc Behaviour_get_enabled{
    .key = "Behaviour.get_enabled", .kind = pm::sdk::Kind::Method, .owner = "Behaviour", .name = "get_enabled"};

/// Switches the component on or off. Inferred: Unity's documented property setter.
inline constexpr pm::sdk::EntryDesc Behaviour_set_enabled{.key = "Behaviour.set_enabled",
                                                          .kind = pm::sdk::Kind::Method, .owner = "Behaviour",
                                                          .name = "set_enabled", .params = UnityObject_bool};

/// Unity's base class of scripts (UnityEngine.CoreModule): every game script on an object is one.
/// Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc MonoBehaviour{.key = "MonoBehaviour", .name = "MonoBehaviour",
                                                  .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// The GameObject's layer, 0 to 31. Inferred: Unity's documented property getter.
inline constexpr pm::sdk::EntryDesc GameObject_get_layer{
    .key = "GameObject.get_layer", .kind = pm::sdk::Kind::Method, .owner = "GameObject", .name = "get_layer"};

/// The first component of the System.Type given on the component's GameObject or one above it, inactive
/// ones too when the flag is set, or null. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Component_GetComponentInParent{
    .key = "Component.GetComponentInParent", .kind = pm::sdk::Kind::Method, .owner = "Component",
    .name = "GetComponentInParent", .params = UnityObject_Type_bool};

inline constexpr const char* MonoBehaviour_IEnumerator[] = {"IEnumerator"};

/// Starts a coroutine, an IEnumerator, on the behaviour: it runs until it ends or the behaviour's
/// GameObject is deactivated or destroyed. Returns the Coroutine. Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc MonoBehaviour_StartCoroutine{
    .key = "MonoBehaviour.StartCoroutine", .kind = pm::sdk::Kind::Method, .owner = "MonoBehaviour",
    .name = "StartCoroutine", .params = MonoBehaviour_IEnumerator};

}  // namespace pm::map

// The game API for Unity's base classes (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this game
// build" once and returns its type's default (0, false, null) without touching the game. Every object
// they return is a managed reference: keep it across frames only in a pm::api::Held.
namespace phasmo {
/// @addtogroup runtime
/// @{

/// Unity's base class of every engine object. `object` below is any engine object.
struct Object {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::UnityObject> type{};
  /// Its name, a System.String: get_name(object). Unresolved: null.
  static constexpr pm::api::Method<pm::map::UnityObject_get_name, void*(void*)> get_name{};
  /// Renames it: set_name(object, string). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityObject_set_name, void(void*, void*)> set_name{};
  /// A copy under the Transform `parent` (never null): Instantiate(original, parent, worldPositionStays).
  /// Unresolved: null.
  static constexpr pm::api::Method<pm::map::UnityObject_Instantiate, void*(void*, void*, bool)> Instantiate{};
  /// Destroy(object, seconds): gone at the end of the frame (seconds 0). Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::UnityObject_Destroy, void(void*, float)> Destroy{};
  /// DestroyImmediate(object, allowDestroyingAssets): gone now. Unresolved: does nothing.
  static constexpr pm::api::Icall<pm::map::UnityObject_DestroyImmediate, void(void*, bool)> DestroyImmediate{};
  /// The first loaded object of a System.Type, inactive ones too when asked:
  /// FindObjectOfType(type, includeInactive). It searches every loaded object, so call it sparingly.
  /// Unresolved: null.
  static constexpr pm::api::Method<pm::map::UnityObject_FindObjectOfType, void*(void*, bool)> FindObjectOfType{};
  /// Every loaded, active object of a System.Type, an Object[]: FindObjectsOfType(type). It searches
  /// every loaded object, so call it sparingly. Unresolved: null.
  static constexpr pm::api::Method<pm::map::UnityObject_FindObjectsOfType, void*(void*)> FindObjectsOfType{};
  /// Every loaded object of a System.Type, inactive ones too when asked, an Object[]: FindAllOfType(type,
  /// includeInactive). It searches every loaded object, so call it sparingly. Unresolved: null.
  static constexpr pm::api::Method<pm::map::UnityObject_FindObjectsOfType_inactive, void*(void*, bool)> FindAllOfType{};
  /// set_hideFlags(object, flags). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::UnityObject_set_hideFlags, void(void*, int32_t)> set_hideFlags{};
};

/// Unity's object flags: combine their numbers for Object::set_hideFlags.
namespace HideFlags {
/// Keeps an asset nothing in a scene uses from being unloaded: DontUnloadUnusedAsset(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::HideFlags_DontUnloadUnusedAsset> DontUnloadUnusedAsset{};
}  // namespace HideFlags

/// Unity's scene object. `gameObject` below is the GameObject.
struct GameObject {
  /// The type: klass(), is(obj), type_object(), make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GameObject> type{};
  /// SetActive(gameObject, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameObject_SetActive, void(void*, bool)> SetActive{};
  /// get_activeSelf(gameObject). Unresolved: false.
  static constexpr pm::api::Method<pm::map::GameObject_get_activeSelf, bool(void*)> get_activeSelf{};
  /// get_activeInHierarchy(gameObject). Optional. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GameObject_get_activeInHierarchy, bool(void*)> get_activeInHierarchy{};
  /// get_transform(gameObject). Unresolved: null.
  static constexpr pm::api::Method<pm::map::GameObject_get_transform, void*(void*)> get_transform{};
  /// ctor(gameObject, name): `gameObject` from type.make(), `name` a managed string. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::GameObject_ctor, void(void*, void*)> ctor{};
  /// set_layer(gameObject, layer). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameObject_set_layer, void(void*, int32_t)> set_layer{};
  /// GetComponents(gameObject, systemType): a Component[] (pm::api::RefArray). Unresolved: null.
  static constexpr pm::api::Method<pm::map::GameObject_GetComponents, void*(void*, void*)> GetComponents{};
  /// AddComponent(gameObject, systemType): the new component. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GameObject_AddComponent, void*(void*, void*)> AddComponent{};
  /// get_layer(go): its layer, 0 to 31. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::GameObject_get_layer, int32_t(void*)> get_layer{};
};

/// Unity's base class of a GameObject's components. `component` below is any component.
struct Component {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Component> type{};
  /// get_transform(component). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Component_get_transform, void*(void*)> get_transform{};
  /// get_gameObject(component). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Component_get_gameObject, void*(void*)> get_gameObject{};
  /// GetComponent(component, systemType): systemType is a Type wrapper's type_object(). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Component_GetComponent, void*(void*, void*)> GetComponent{};
  /// GetComponentInChildren(component, systemType, includeInactive). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Component_GetComponentInChildren, void*(void*, void*, bool)>
      GetComponentInChildren{};
  /// GetComponentInParent(c, type, includeInactive): the first component of the System.Type on c's
  /// GameObject or above it, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Component_GetComponentInParent, void*(void*, void*, bool)>
      GetComponentInParent{};
};

/// Unity's switchable components. `behaviour` below is one.
struct Behaviour {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Behaviour> type{};
  /// get_enabled(behaviour). Unresolved: false.
  static constexpr pm::api::Method<pm::map::Behaviour_get_enabled, bool(void*)> get_enabled{};
  /// set_enabled(behaviour, on). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Behaviour_set_enabled, void(void*, bool)> set_enabled{};
};

/// Unity's base class of scripts.
struct MonoBehaviour {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::MonoBehaviour> type{};
  /// StartCoroutine(behaviour, routine): runs the IEnumerator as a coroutine on it; the Coroutine, or null.
  /// Unresolved: null, and nothing runs.
  static constexpr pm::api::Method<pm::map::MonoBehaviour_StartCoroutine, void*(void*, void*)> StartCoroutine{};
};

/// @}
}  // namespace phasmo
