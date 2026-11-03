#include "runtime.h"

#include <atomic>

#include "held.h"
#include "log.h"

namespace pm::api {

namespace detail {

size_t array_length(const void* arr) {
  const PmHostApi* h = host();
  return h && arr ? h->array_length(arr) : 0;
}

void* array_data(void* arr) {
  const PmHostApi* h = host();
  return h && arr ? h->array_data(arr) : nullptr;
}

size_t list_count(const void* list) {
  const PmHostApi* h = host();
  const int32_t n = h && list ? h->list_count(list) : 0;
  return n > 0 ? static_cast<size_t>(n) : 0;
}

void* list_items(const void* list) {
  const PmHostApi* h = host();
  return h && list ? h->list_items(list) : nullptr;
}

bool list_add(void* list, void* item) {
  const PmHostApi* h = host();
  return h && h->list_add && list && h->list_add(list, item);
}

size_t collection_count(const void* collection) {
  const PmHostApi* h = host();
  const int32_t n = h && h->collection_count && collection ? h->collection_count(collection) : 0;
  return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t dictionary_int_entries(const void* dict, PmIntEntry* out, size_t max) {
  const PmHostApi* h = host();
  const int32_t limit = max > 0x7FFFFFFF ? 0x7FFFFFFF : static_cast<int32_t>(max);
  const int32_t n = h && h->dictionary_int_entries && dict && out ? h->dictionary_int_entries(dict, out, limit) : 0;
  return n > 0 ? static_cast<size_t>(n) : 0;
}

size_t array_element_size(const void* arr) {
  const PmHostApi* h = host();
  return h && h->array_element_size && arr ? h->array_element_size(arr) : 0;
}

bool list_remove_at(void* list, int32_t index) {
  const PmHostApi* h = host();
  return h && h->list_remove_at && list && h->list_remove_at(list, index);
}

void report_out_of_range() {
  static std::atomic<bool> reported{false};
  if (!reported.exchange(true)) PM_LOG("an array or list index was out of range; the access was ignored");
}

void store_reference(void* obj, size_t offset, void* value) {
  if (const PmHostApi* h = host()) h->set_reference(obj, static_cast<int32_t>(offset), value);
}

uint32_t gc_hold(void* object) {
  const PmHostApi* h = host();
  return h && object ? h->gc_hold(object) : 0;
}

void gc_release(uint32_t handle) {
  const PmHostApi* h = host();
  if (h && handle) h->gc_release(handle);
}

}  // namespace detail

std::string to_utf8(const void* str) {
  const PmHostApi* h = detail::host();
  if (!h || !str) return {};
  std::string text(h->string_utf8(str, nullptr, 0), '\0');
  if (!text.empty()) h->string_utf8(str, text.data(), text.size() + 1);  // std::string keeps room for the NUL
  return text;
}

bool string_is(const void* str, std::string_view text) {
  const PmHostApi* h = detail::host();
  char read[64];
  if (!h || !str || text.size() >= sizeof read) return false;
  const size_t length = h->string_utf8(str, read, sizeof read);
  return length == text.size() && text == std::string_view(read, length);
}

void* make_string(std::string_view text) {
  const PmHostApi* h = detail::host();
  return h ? h->string_new(std::string(text).c_str()) : nullptr;
}

bool alive(const void* unityObject) {
  const PmHostApi* h = detail::host();
  return h && unityObject && h->alive(unityObject);
}

}  // namespace pm::api
