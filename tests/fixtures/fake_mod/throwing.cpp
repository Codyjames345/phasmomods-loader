// A hand-written mod for tests/mods_test.cpp that breaks the host contract by throwing out of what the host asks before it
// starts: pmmod_info, or with PM_FAKE_THROW_IN_REQUIRES pmmod_requires. The SDK's glue guards these, so only a mod written
// without it can do this.
#include "mod_api.h"

namespace {
#ifdef PM_FAKE_THROW_IN_REQUIRES
constexpr PmModInfo kInfo{PMMOD_API_VERSION, "fake-throws", "Throws", 210, 0};
#endif
}  // namespace

extern "C" {
__declspec(dllexport) const PmModInfo* pmmod_info() {
#ifdef PM_FAKE_THROW_IN_REQUIRES
  return &kInfo;
#else
  throw 1;
#endif
}
__declspec(dllexport) bool pmmod_start(const PmHostApi*) { return true; }
__declspec(dllexport) void pmmod_tick() {}
__declspec(dllexport) void pmmod_stop() {}
#ifdef PM_FAKE_THROW_IN_REQUIRES
__declspec(dllexport) size_t pmmod_requires(const char* const**) { throw 1; }
#endif
}
