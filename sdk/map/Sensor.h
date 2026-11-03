#pragma once

// SensorToolkit's sensors (the Micosmo.SensorToolkit assembly), which the game's ghost uses to find
// what it may interact with: GhostActivity.interactionSensor is a RangeSensor. Notes read from
// decompiled code are from build 25811160.

#include "entry.h"
#include "game_api.h"
#include "Transform.h"

namespace pm::map {

/// The base of SensorToolkit's sensors that can be pulsed (Micosmo.SensorToolkit). Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc BasePulsableSensor{.key = "BasePulsableSensor", .name = "BasePulsableSensor",
                                                       .assembly = "Micosmo.SensorToolkit",
                                                       .ns = "Micosmo.SensorToolkit"};

/// Pulses the sensor: it completes any pending pulse and runs the sensor's own test, which for a
/// RangeSensor rebuilds its detections, then invokes its OnPulsed delegate at once. Pulse is not
/// virtual and does not read its MethodInfo. GhostActivity.TryInteract pulses the interaction
/// sensor through the virtual PulseAll, and RangeSensor's PulseAll is a thunk to this code that
/// calls Pulse(this, null). So a direct Pulse(sensor) of a RangeSensor is TryInteract's own call.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc BasePulsableSensor_Pulse{.key = "BasePulsableSensor.Pulse",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "BasePulsableSensor", .name = "Pulse"};

/// A sensor that detects what lies within a shape around it (Micosmo.SensorToolkit). Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc RangeSensor{.key = "RangeSensor", .name = "RangeSensor",
                                                .assembly = "Micosmo.SensorToolkit", .ns = "Micosmo.SensorToolkit"};

inline constexpr const char* RangeSensor_Vector3[] = {"Vector3"};

/// Makes the sensor's shape a box with these half extents. Decompiled.
inline constexpr pm::sdk::EntryDesc RangeSensor_SetBoxShape{.key = "RangeSensor.SetBoxShape",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "RangeSensor", .name = "SetBoxShape",
                                                            .params = RangeSensor_Vector3};

/// The sensor's OnPulsed delegate, an Action, invoked after every pulse: RangeSensor's one Action field
/// (add_OnPulsed adds to it). GhostActivity.Start adds the ghost's whole interaction to the interaction
/// sensor's (a handler: the door chance, a light switch, a prop, else ChangeState(favouriteRoom)) and
/// nothing else; OnDestroy removes it. Decompiled.
inline constexpr pm::sdk::EntryDesc RangeSensor_onPulsed{.key = "RangeSensor.onPulsed", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "RangeSensor",
                                                         .type = "Action"};

// The sensor copies Custom Ghosts makes for the Hydra's heads and the Lithobolia's sight (a sensor of
// the class, set up as the ghost's own and pulsed by hand). Read from the class dump and decompiles of
// build 25811160.

/// A sensor (Micosmo.SensorToolkit): BasePulsableSensor with detections, signal processors and a
/// filter; RangeSensor and LOSSensor derive from it. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Sensor{.key = "Sensor", .name = "Sensor", .assembly = "Micosmo.SensorToolkit",
                                           .ns = "Micosmo.SensorToolkit"};

/// The sensor's signal processors, a List<SignalProcessor> each signal passes through before it
/// counts. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Sensor_signalProcessors{.key = "Sensor.signalProcessors",
                                                            .kind = pm::sdk::Kind::Field, .owner = "Sensor",
                                                            .name = "signalProcessors"};

inline constexpr const char* Sensor_GetDetections_params[] = {"List<GameObject>"};

/// The GameObjects the sensor detects now, into the list given (a new one for null), and returns it.
/// The overload without a tag or a filter. Inferred: from the method's name.
inline constexpr pm::sdk::EntryDesc Sensor_GetDetections{.key = "Sensor.GetDetections", .kind = pm::sdk::Kind::Method,
                                                         .owner = "Sensor", .name = "GetDetections",
                                                         .params = Sensor_GetDetections_params};

/// The sensor's SignalFilter: Sensor's one field of that type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Sensor_filter{.key = "Sensor.filter", .kind = pm::sdk::Kind::Field,
                                                  .owner = "Sensor", .type = "SignalFilter"};

/// A sensor that detects within a volume (Micosmo.SensorToolkit): RangeSensor derives from it. Its one
/// public value field is the detection mode (DetectionMode). Inferred: from its name and the class dump.
inline constexpr pm::sdk::EntryDesc BaseVolumeSensor{.key = "BaseVolumeSensor", .name = "BaseVolumeSensor",
                                                     .assembly = "Micosmo.SensorToolkit",
                                                     .ns = "Micosmo.SensorToolkit"};

/// The volume's own SignalFilter. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc BaseVolumeSensor_signalFilter{.key = "BaseVolumeSensor.signalFilter",
                                                                  .kind = pm::sdk::Kind::Field,
                                                                  .owner = "BaseVolumeSensor", .name = "signalFilter"};

inline constexpr const char* DetectionMode_values[] = {"Colliders", "RigidBodies", "Either"};

/// What a volume sensor detects (name obfuscated, value names real): Colliders, RigidBodies, Either.
/// Found by its values. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc DetectionMode{.key = "DetectionMode",
                                                  .assembly = "Micosmo.SensorToolkit", .ns = "",
                                                  .values = DetectionMode_values};

/// The volume sensor's detection mode, a DetectionMode number: BaseVolumeSensor's one field of that
/// type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc BaseVolumeSensor_detectionMode{.key = "BaseVolumeSensor.detectionMode",
                                                                   .kind = pm::sdk::Kind::Field,
                                                                   .owner = "BaseVolumeSensor",
                                                                   .type = "@DetectionMode"};

inline constexpr const char* RangeShape_values[] = {"Sphere", "Box", "Capsule"};

/// The shape of a range sensor, an enum nested in RangeSensor (name obfuscated, value names real):
/// Sphere, Box, Capsule. Found by its values. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RangeShape{.key = "RangeShape", .owner = "RangeSensor",
                                               .values = RangeShape_values};

/// The range sensor's shape, a RangeShape number: RangeSensor's one field of that type. With its
/// sphere, box and capsule sizes, its LayerMask and a flag, it is one of the public value fields
/// RangeSensor declares, which Type::copy_settings copies. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc RangeSensor_shape{.key = "RangeSensor.shape", .kind = pm::sdk::Kind::Field,
                                                      .owner = "RangeSensor", .type = "@RangeShape"};

/// The layers the range sensor detects on: RangeSensor's one LayerMask field. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc RangeSensor_layers{.key = "RangeSensor.layers", .kind = pm::sdk::Kind::Field,
                                                       .owner = "RangeSensor", .type = "LayerMask"};

/// Pulses the sensor: RangeSensor's override of the virtual PulseAll, a thunk to
/// BasePulsableSensor.Pulse(this, null), the code GhostActivity.TryInteract runs through the vtable.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc RangeSensor_PulseAll{.key = "RangeSensor.PulseAll", .kind = pm::sdk::Kind::Method,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "RangeSensor", .name = "PulseAll"};

/// How a sensor's pulse routine pulses it (an enum nested in PulseRoutine, name obfuscated, value names
/// real): Manual, FixedInterval, EachFrame. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PulseRoutine{.key = "PulseRoutine", .name = "PulseRoutine",
                                                 .assembly = "Micosmo.SensorToolkit", .ns = "Micosmo.SensorToolkit"};

inline constexpr const char* PulseMode_values[] = {"Manual", "FixedInterval", "EachFrame"};

/// The pulse modes, nested in PulseRoutine. Found by its values. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PulseMode{.key = "PulseMode", .owner = "PulseRoutine",
                                              .values = PulseMode_values};

/// Manual: the routine starts nothing when the sensor is enabled, so the sensor pulses only when it is
/// told to (RangeSensor.OnEnable). Inferred: from the value's name and OnEnable.
inline constexpr pm::sdk::EntryDesc PulseMode_Manual{.key = "PulseMode.Manual", .kind = pm::sdk::Kind::EnumValue,
                                                     .owner = "PulseMode", .name = "Manual"};

inline constexpr const char* PulseMode_param[] = {"@PulseMode"};

/// Sets the pulse mode: the value of the sensor's pulse routine's observable mode, whose change runs
/// its listeners. The routine is made by the constructor and its listeners join in Awake, so before a
/// sensor's GameObject is first activated this only stores the mode. Decompiled.
inline constexpr pm::sdk::EntryDesc RangeSensor_set_PulseMode{.key = "RangeSensor.set_PulseMode",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "RangeSensor", .name = "set_PulseMode",
                                                              .params = PulseMode_param};

/// A sensor that tests its input sensor's detections for line of sight (Micosmo.SensorToolkit): the
/// ghost's vision (GhostAI.vision). Its settings are the public value fields LOSSensor declares (three
/// ScalingFunction structs among them, each holding an AnimationCurve), which Type::copy_settings
/// copies; its public HashSet is a reference and is left alone (only the editor's OnDrawGizmosSelected
/// reads it). Inferred: from its name and the dump.
inline constexpr pm::sdk::EntryDesc LOSSensor{.key = "LOSSensor", .name = "LOSSensor", .assembly = "Micosmo.SensorToolkit",
                                              .ns = "Micosmo.SensorToolkit"};

/// The constructor: it makes the input observable and the pulse routine and stores the settings'
/// defaults. Inferred: by name.
inline constexpr pm::sdk::EntryDesc LOSSensor_ctor{.key = "LOSSensor.ctor", .kind = pm::sdk::Kind::Method,
                                                   .owner = "LOSSensor", .name = ".ctor"};

/// Pulses the sensor: it first pulses its input sensor through that sensor's virtual PulseAll, then
/// tests the input's signals for line of sight (TestSignal, each signal with the sensor's
/// settings). Decompiled.
inline constexpr pm::sdk::EntryDesc LOSSensor_PulseAll{.key = "LOSSensor.PulseAll", .kind = pm::sdk::Kind::Method,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "LOSSensor", .name = "PulseAll"};

/// The sensor whose detections it tests: the value of its input observable. Decompiled.
inline constexpr pm::sdk::EntryDesc LOSSensor_get_InputSensor{.key = "LOSSensor.get_InputSensor",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "LOSSensor", .name = "get_InputSensor"};

/// Sets the sensor whose detections it tests: the input observable's value, whose change subscribes
/// the sensor to the input's signals once it is enabled. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc LOSSensor_set_InputSensor{.key = "LOSSensor.set_InputSensor",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .owner = "LOSSensor", .name = "set_InputSensor"};

/// Sets the pulse mode, as RangeSensor.set_PulseMode does for its own routine. Decompiled.
inline constexpr pm::sdk::EntryDesc LOSSensor_set_PulseMode{.key = "LOSSensor.set_PulseMode",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "LOSSensor", .name = "set_PulseMode",
                                                            .params = PulseMode_param};

inline constexpr pm::sdk::Fact LOSSensor_window_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "LOSSensor.ctor", .value = 10}};

/// The window of its moving-average visibility (10 by default): the int the constructor sets to
/// 10. TestSignal hands it to the test job with the averaging flag and the visibility a target
/// needs; with averaging on, the job scores a target by its history over that many pulses.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LOSSensor_window{.key = "LOSSensor.window", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "LOSSensor", .type = "int",
                                                     .facts = LOSSensor_window_facts};

/// The game's signal processor that passes a signal by the ghost's hunting (Assembly-CSharp, global
/// namespace). Its Process passes a signal while GhostAI.isHunting equals its requirement, or
/// always for a ghost of type 18 when its allowRaijuSkip is set. It holds settings only.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntingProcessor{.key = "GhostHuntingProcessor",
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .name = "GhostHuntingProcessor", .assembly = "Assembly-CSharp",
                                                          .ns = ""};

/// Whether the processor passes signals in a hunt or out of one. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc GhostHuntingProcessor_requirement{.key = "GhostHuntingProcessor.requirement",
                                                                      .kind = pm::sdk::Kind::Field,
                                                                      .owner = "GhostHuntingProcessor",
                                                                      .name = "ghostHuntingRequirement", .type = "bool"};

/// Whether a type 18 ghost's signals always pass. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc GhostHuntingProcessor_allowRaijuSkip{
    .key = "GhostHuntingProcessor.allowRaijuSkip", .kind = pm::sdk::Kind::Field, .owner = "GhostHuntingProcessor",
    .name = "allowRaijuSkip", .type = "bool"};

/// The game's signal processor for players (Assembly-CSharp, global namespace). It declares no fields. Its Process
/// takes the sight part on the signal's GameObject and passes the signal when the part's Player is not marked dead
/// (Player.isDead) and that player's room is not outside the map (RoomKind.outsideMap); for a ghost of type 28 it
/// then also needs the part within a fixed distance of the sensor it is given, and fails it when a flag reached
/// through one of the player's fields is set. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerProcessor{.key = "PlayerProcessor", .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "PlayerProcessor", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* Sensor_GameObject[] = {"GameObject"};

/// Whether the sensor detects the GameObject now: whether it is one of the sensor's signal objects, those
/// whose components GetDetectedComponents gives. Inferred: SensorToolkit's documented API.
inline constexpr pm::sdk::EntryDesc Sensor_IsDetected{.key = "Sensor.IsDetected", .kind = pm::sdk::Kind::Method,
                                                      .owner = "Sensor", .name = "IsDetected",
                                                      .params = Sensor_GameObject};

inline constexpr const char* Sensor_ListOfT[] = {"List<T>"};
inline constexpr const char* Sensor_GetDetectedComponents_args[] = {"@PhotonObjectInteract"};

/// The sensor's detected components of the type T, into the list given (and returned): the generic method
/// GetDetectedComponents<T>(List<T>), here for the T a ghost's interaction detects. Every reference type's T shares
/// this code and tells it apart by the MethodInfo it is given, so a hook on it sees each such call in the game, for any
/// T. Checked at startup only: the instance's code is the runtime's. Inferred: SensorToolkit's documented API; the
/// sharing Decompiled from the runtime's generic method lookup.
inline constexpr pm::sdk::EntryDesc Sensor_GetDetectedComponents{
    .key = "Sensor.GetDetectedComponents<PhotonObjectInteract>", .kind = pm::sdk::Kind::Inflated,
    .flags = pm::sdk::kGenericMethod, .owner = "Sensor", .name = "GetDetectedComponents", .type = "List<T>",
    .params = Sensor_ListOfT, .typeArgs = Sensor_GetDetectedComponents_args};

}  // namespace pm::map

// The game API for SensorToolkit's sensors (sdk/api/game_api.h). Every member is called on Unity's main
// thread. One that did not resolve in the running game build logs "<key> is not available in this game
// build" once and returns its type's default (null, false) without touching the game.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// A pulsable sensor. `sensor` below is one.
struct BasePulsableSensor {
  /// Pulse(sensor). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::BasePulsableSensor_Pulse, void(void*)> Pulse{};
};

/// A range sensor. `sensor` below is one.
struct RangeSensor {
  /// The type: is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::RangeSensor> type{};
  /// SetBoxShape(sensor, &halfExtents). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RangeSensor_SetBoxShape, void(void*, const Vector3*)> SetBoxShape{};
  /// Its OnPulsed delegate: onPulsed.get(sensor), onPulsed.set(sensor, action) (through the GC write
  /// barrier). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::RangeSensor_onPulsed, void*> onPulsed{};
  /// Its shape, a RangeShape number: shape.get(sensor). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::RangeSensor_shape, int32_t> shape{};
  /// The layers it detects on, a LayerMask's bits: layers.get(sensor). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::RangeSensor_layers, int32_t> layers{};
  /// PulseAll(sensor): RangeSensor's pulse, as the game's virtual call reaches it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::RangeSensor_PulseAll, void(void*)> PulseAll{};
  /// set_PulseMode(sensor, mode), a PulseMode number. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RangeSensor_set_PulseMode, void(void*, int32_t)> set_PulseMode{};
};

/// A sensor. `sensor` below is one.
struct Sensor {
  /// The components it detects: GetDetectedComponents(sensor, list), the list holding the T the call was made for and
  /// null for a new list. The code every reference type's T shares, for a hook on all of them. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Sensor_GetDetectedComponents, void*(void*, void*)> GetDetectedComponents{};
  /// The GameObjects it detects now, a List<GameObject>: GetDetections(sensor, list), null for a new
  /// list. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Sensor_GetDetections, void*(void*, void*)> GetDetections{};
  /// Its signal processors, a List<SignalProcessor>: signalProcessors.get(sensor), .set(sensor, list)
  /// (through the GC write barrier). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::Sensor_signalProcessors, void*> signalProcessors{};
  /// Its SignalFilter: filter.get(sensor), filter.set(sensor, f). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::Sensor_filter, void*> filter{};
  /// IsDetected(sensor, gameObject): whether it detects the GameObject now. Unresolved: false.
  static constexpr pm::api::Method<pm::map::Sensor_IsDetected, bool(void*, void*)> IsDetected{};
};

/// A volume sensor. `sensor` below is one.
struct BaseVolumeSensor {
  /// The type: copy_settings(dst, src) copies its detection mode. Unresolved: false.
  static constexpr pm::api::Type<pm::map::BaseVolumeSensor> type{};
  /// Its own SignalFilter: signalFilter.get(sensor), .set(sensor, f). Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::BaseVolumeSensor_signalFilter, void*> signalFilter{};
  /// Its detection mode, a DetectionMode number: detectionMode.get(sensor). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::BaseVolumeSensor_detectionMode, int32_t> detectionMode{};
};

/// The pulse modes. Unresolved values are 0, which is Manual: check use::resolved() first.
namespace PulseMode {
/// Manual(): pulsed only when told to. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::PulseMode_Manual> Manual{};
}  // namespace PulseMode

/// A line-of-sight sensor. `sensor` below is one.
struct LOSSensor {
  /// The type: is(obj), type_object(), copy_settings(dst, src). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::LOSSensor> type{};
  /// PulseAll(sensor): pulses its input sensor, then tests the input's signals. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::LOSSensor_PulseAll, void(void*)> PulseAll{};
  /// get_InputSensor(sensor): the sensor it tests. Unresolved: null.
  static constexpr pm::api::Method<pm::map::LOSSensor_get_InputSensor, void*(void*)> get_InputSensor{};
  /// set_InputSensor(sensor, input). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LOSSensor_set_InputSensor, void(void*, void*)> set_InputSensor{};
  /// set_PulseMode(sensor, mode), a PulseMode number. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LOSSensor_set_PulseMode, void(void*, int32_t)> set_PulseMode{};
  /// The window of its moving-average visibility: window.get(sensor). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LOSSensor_window, int32_t> window{};
};

/// The game's hunting signal processor. `processor` below is one.
struct GhostHuntingProcessor {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::GhostHuntingProcessor> type{};
  /// Whether it passes signals in a hunt: requirement.get(processor). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostHuntingProcessor_requirement, bool> requirement{};
  /// Whether a type 18 ghost's signals always pass: allowRaijuSkip.get(processor). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostHuntingProcessor_allowRaijuSkip, bool> allowRaijuSkip{};
};

/// The game's player signal processor.
struct PlayerProcessor {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::PlayerProcessor> type{};
};

/// @}
}  // namespace phasmo
