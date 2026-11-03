#include "sensor_copy.h"

#include "Sensor.h"
#include "UnityObject.h"
#include "game_api.h"
#include "managed_exception.h"  // Il2CppExceptionWrapper: the C++ type a managed exception is thrown as
#include "log.h"
#include "mod_glue.h"
#include "runtime.h"

namespace pm::custom::sensor_copy {

namespace {

void* type_object(Kind kind) {
  return kind == Kind::Range ? phasmo::RangeSensor::type.type_object() : phasmo::LOSSensor::type.type_object();
}

}  // namespace

bool make(Kind kind, const char* name, api::Held& object, api::Held& sensor, const char*& why) {
  why = nullptr;
  void* go = phasmo::GameObject::type.make();
  if (go) phasmo::GameObject::ctor(go, api::make_string(name));
  if (!go) {
    why = "no GameObject for it";
    return false;
  }
  object = go;
  phasmo::GameObject::SetActive(go, false);  // Awake and OnEnable wait until it is set up
  void* s = phasmo::GameObject::AddComponent(go, type_object(kind));
  if (!s) {
    why = "AddComponent made no sensor";
  } else {
    try {
      // Before Awake the routine's mode has no listeners, so this only stores Manual.
      if (kind == Kind::Range) phasmo::RangeSensor::set_PulseMode(s, phasmo::PulseMode::Manual());
      else phasmo::LOSSensor::set_PulseMode(s, phasmo::PulseMode::Manual());
    } catch (const Il2CppExceptionWrapper& e) {
      mod::host().log_exception("custom ghosts: setting a sensor copy's pulse mode threw", e.ex);
      why = "its pulse mode could not be set";
    } catch (...) {
      why = "its pulse mode could not be set";
    }
  }
  if (why) {
    drop(object, sensor);
    return false;
  }
  sensor = s;
  return true;
}

bool share_filters(void* copy, void* original) {
  // Its one public value field: the detection mode.
  if (!phasmo::BaseVolumeSensor::type.copy_settings(copy, original)) return false;
  phasmo::BaseVolumeSensor::signalFilter.set(copy, phasmo::BaseVolumeSensor::signalFilter.get(original));
  phasmo::Sensor::filter.set(copy, phasmo::Sensor::filter.get(original));
  return true;
}

bool copy_settings(Kind kind, void* copy, const void* original) {
  return kind == Kind::Range ? phasmo::RangeSensor::type.copy_settings(copy, original)
                             : phasmo::LOSSensor::type.copy_settings(copy, original);
}

bool pulse(Kind kind, void* sensor, const char* who) {
  try {
    if (kind == Kind::Range) phasmo::RangeSensor::PulseAll(sensor);
    else phasmo::LOSSensor::PulseAll(sensor);
    return true;
  } catch (const Il2CppExceptionWrapper& e) {
    mod::host().log_exception(who, e.ex);
  } catch (...) {
    PM_LOG("%s", who);
  }
  return false;
}

void drop(api::Held& object, api::Held& sensor) {
  if (api::alive(object)) phasmo::Object::Destroy(object, 0.0f);
  object = nullptr;
  sensor = nullptr;
}

}  // namespace pm::custom::sensor_copy
