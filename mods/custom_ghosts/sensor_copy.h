#pragma once

#include "held.h"

// Copies of the ghost's own SensorToolkit sensors (sdk/map/Sensor.h), shared by the Hydra's heads
// (hydra.cpp) and the Lithobolia's sight (lithobolia.cpp). A copy is a sensor of the same class on a
// GameObject of its own, made inactive so its Awake and OnEnable run only once it is set up as the
// ghost's, with its pulse mode Manual: it pulses only when the mod pulses it, through the class's
// PulseAll, as the game's own call reaches it. Main thread.
namespace pm::custom::sensor_copy {

enum class Kind { Range, LineOfSight };  // RangeSensor, LOSSensor

/**
 * A sensor of `kind` on a new inactive GameObject named `name`, its pulse mode Manual. False, with `why`
 * set and nothing kept, when the engine would not make it or its pulse mode could not be set.
 */
bool make(Kind kind, const char* name, api::Held& object, api::Held& sensor, const char*& why);

/**
 * The volume sensor `copy` takes the ghost's `original`'s detection mode and both its signal filters
 * (BaseVolumeSensor's and Sensor's, shared: a filter is the game's object and the copy only reads it).
 * False, sharing nothing, when the detection mode could not be copied: the copy would detect otherwise.
 */
bool share_filters(void* copy, void* original);

/**
 * `copy` takes `original`'s settings now: the public value fields its class declares (Type::
 * copy_settings): a RangeSensor's shape, sizes, layers and flag, a LOSSensor's line-of-sight settings
 * with their curves shared. False when either is not of the kind.
 */
bool copy_settings(Kind kind, void* copy, const void* original);

/** Pulses the copy through its class's PulseAll; false, logged with `who`, when the game's code threw. */
bool pulse(Kind kind, void* sensor, const char* who);

/** Destroys the copy's GameObject, if it is still there, and forgets both. */
void drop(api::Held& object, api::Held& sensor);

}  // namespace pm::custom::sensor_copy
