#include "runtime.h"

#include <atomic>

#include "held.h"
#include "log.h"

namespace pg::api {

namespace detail {

size_t array_length(const void* arr) {
  const PgHostApi* h = host();
  return h && arr ? h->array_length(arr) : 0;
}

void* array_data(void* arr) {
  const PgHostApi* h = host();
  return h && arr ? h->array_data(arr) : nullptr;
}

size_t list_count(const void* list) {
  const PgHostApi* h = host();
  const int32_t n = h && list ? h->list_count(list) : 0;
  return n > 0 ? static_cast<size_t>(n) : 0;
}

void* list_items(const void* list) {
  const PgHostApi* h = host();
  return h && list ? h->list_items(list) : nullptr;
}

void report_out_of_range() {
  static std::atomic<bool> reported{false};
  if (!reported.exchange(true)) PG_LOG("an array or list index was out of range; the access was ignored");
}

void store_reference(void* obj, size_t offset, void* value) {
  if (const PgHostApi* h = host()) h->set_reference(obj, static_cast<int32_t>(offset), value);
}

uint32_t gc_hold(void* object) {
  const PgHostApi* h = host();
  return h && object ? h->gc_hold(object) : 0;
}

void gc_release(uint32_t handle) {
  const PgHostApi* h = host();
  if (h && handle) h->gc_release(handle);
}

}  // namespace detail

std::string to_utf8(const void* str) {
  const PgHostApi* h = detail::host();
  if (!h || !str) return {};
  std::string text(h->string_utf8(str, nullptr, 0), '\0');
  if (!text.empty()) h->string_utf8(str, text.data(), text.size() + 1);  // std::string keeps room for the NUL
  return text;
}

void* make_string(std::string_view text) {
  const PgHostApi* h = detail::host();
  return h ? h->string_new(std::string(text).c_str()) : nullptr;
}

bool alive(const void* unityObject) {
  const PgHostApi* h = detail::host();
  return h && unityObject && h->alive(unityObject);
}

}  // namespace pg::api
