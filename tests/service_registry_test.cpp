// The host's service registry (host/service_registry.h): the tables running mods publish for each
// other through the host contract's register_service. Run: build\service_registry_test.exe
#include "check.h"
#include "service_registry.h"

namespace {

const int kFirst = 1, kSecond = 2;

// A name already published is refused, so a later mod cannot take over a table that other mods
// already found under it: the first table stays.
void publish_twice_is_refused() {
  pg::services::Registry services;
  CHECK(services.publish("ghost-tells.v1", &kFirst));
  CHECK(!services.publish("ghost-tells.v1", &kSecond));
  CHECK(services.find("ghost-tells.v1") == &kFirst);
  CHECK(!services.publish("ghost-tells.v1", &kFirst));  // the same table again is no exception
  CHECK(services.publish("ghost-tells.v2", &kSecond));  // another name is another service
  CHECK(services.find("ghost-tells.v2") == &kSecond);
}

// Nobody published the name, or the name is null: null. Once the mods stop the host forgets every
// table, and a restarted mod publishes its own again.
void unknown_and_cleared_names_are_null() {
  pg::services::Registry services;
  CHECK(services.find("nobody.v1") == nullptr && services.find(nullptr) == nullptr);
  CHECK(!services.publish(nullptr, &kFirst));
  CHECK(services.publish("ghost-tells.v1", &kFirst));
  services.clear();
  CHECK(services.find("ghost-tells.v1") == nullptr);
  CHECK(services.publish("ghost-tells.v1", &kSecond) && services.find("ghost-tells.v1") == &kSecond);
}

}  // namespace

int main() {
  publish_twice_is_refused();
  unknown_and_cleared_names_are_null();
  return g_failures ? 1 : 0;
}
