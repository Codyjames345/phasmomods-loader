#pragma once

/// @file
/// @brief How a mod catches a managed exception the game throws.
/// @ingroup runtime

/// @brief The C++ type the game's runtime throws a managed exception as (libil2cpp's Il2CppExceptionWrapper).
/// A game method called through the game API that throws unwinds through the mod as this type, so a mod
/// that must survive one catches it by name: `catch (const Il2CppExceptionWrapper& e)`. It is declared in
/// the global namespace with the runtime's own name, which is what makes the catch match.
/// @note `ex` is the managed exception object. Log it with the host contract's log_exception
/// (`pm::mod::host().log_exception(where, e.ex)`); never keep it past the catch.
/// @since Contract 2.2
/// @code
/// try {
///   phasmo::RangeSensor::PulseAll(sensor);
/// } catch (const Il2CppExceptionWrapper& e) {
///   pm::mod::host().log_exception("my mod: pulsing threw", e.ex);
/// }
/// @endcode
struct Il2CppExceptionWrapper {
  void* ex;  ///< the managed exception object, valid only inside the catch
};
