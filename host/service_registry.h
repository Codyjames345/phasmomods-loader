#pragma once

#include <functional>
#include <map>
#include <string>

// The tables of C function pointers running mods publish for each other (the host contract's
// register_service and service, core/services.h). A name is published once: a second publish of
// it is refused and the first table stays, so a mod that comes later cannot take over a service
// other mods already found. Pure logic so it can be tested offline; the host logs a refusal. Main
// thread.
namespace pg::services {

class Registry {
 public:
  // Publishes `table` under `name`. False, publishing nothing, for a null name and for a name already
  // published, even with the same table.
  bool publish(const char* name, const void* table) { return name && tables_.try_emplace(name, table).second; }

  // The table published under `name`, or null when nobody published it or `name` is null.
  const void* find(const char* name) const {
    const auto it = name ? tables_.find(name) : tables_.end();
    return it == tables_.end() ? nullptr : it->second;
  }

  // Forgets every table, as when the mods that published them stop.
  void clear() { tables_.clear(); }

 private:
  std::map<std::string, const void*, std::less<>> tables_;
};

}  // namespace pg::services
