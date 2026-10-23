#include "fake_game.h"

#include <cstring>
#include <map>

namespace pg::fake {
namespace {

std::map<std::string, PgGameEntry> g_entries;  // by key; a map never moves its values
std::vector<std::string> g_hooked;
int g_barrierWrites = 0;

PgGameEntry& entry_for(const sdk::EntryDesc& d, uint32_t kind) {
  PgGameEntry& e = g_entries[d.key];
  e = {};
  e.kind = kind;
  return e;
}

const PgGameEntry* game_entry(const char* key) {
  const auto it = key ? g_entries.find(key) : g_entries.end();
  return it == g_entries.end() ? nullptr : &it->second;
}

bool hook_entry(const char* key, void*, void** original, const char*, bool) {
  const PgGameEntry* e = game_entry(key);
  if (!e || e->kind != PG_ENTRY_METHOD) return false;
  g_hooked.emplace_back(key);
  if (original) *original = e->code;
  return true;
}

void set_reference(void* obj, int32_t offset, void* value) {
  ++g_barrierWrites;
  std::memcpy(static_cast<char*>(obj) + offset, &value, sizeof value);
}

int32_t inside_entry(const char*) { return 0; }

// The rest of the contract does nothing.
void log(const char*) {}
bool create_hook(void*, void*, void**, const char*) { return false; }
bool enable_hooks() { return false; }
bool send(uint8_t, const uint8_t*, size_t, int32_t, bool, int32_t) { return false; }
void set_receiver(uint8_t, PgReceiver) {}
bool peer_has_mod(int32_t, const char*) { return false; }
void register_service(const char*, const void*) {}
const void* service(const char*) { return nullptr; }

const PgHostApi kHost{PGMOD_API_VERSION, nullptr, nullptr, L"", &log, &create_hook, &enable_hooks, &send,
                      &set_receiver, &peer_has_mod, &register_service, &service, &game_entry, &hook_entry,
                      &set_reference, &inside_entry};

}  // namespace

void reset() {
  g_entries.clear();
  g_hooked.clear();
  g_barrierWrites = 0;
}

const PgHostApi* host() { return &kHost; }

void method(const sdk::EntryDesc& d, void* fn) { entry_for(d, PG_ENTRY_METHOD).code = fn; }

void field(const sdk::EntryDesc& d, int32_t offset) { entry_for(d, PG_ENTRY_FIELD).offset = offset; }

void value(const sdk::EntryDesc& d, int64_t v) { entry_for(d, PG_ENTRY_ENUM_VALUE).value = v; }

void type(const sdk::EntryDesc& d, void* klass) { entry_for(d, PG_ENTRY_TYPE).klass = klass; }

std::vector<std::string> hooked() { return g_hooked; }

int barrier_writes() { return g_barrierWrites; }

}  // namespace pg::fake
