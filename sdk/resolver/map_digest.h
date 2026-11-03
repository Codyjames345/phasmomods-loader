#pragma once

#include <cstdint>
#include <cstdio>
#include <span>
#include <string>

#include "entry.h"

// A digest of the map: of what each entry tells the resolver, so a new or changed entry changes it, and a
// note or a comment does not. A checked-build record holds it (core/build_record.h), and a record made
// with another map says nothing about this one's entries: the host treats it as no record, and the
// release refuses it. FNV-1a over the fields, as 16 hex digits: it tells maps apart and proves nothing to
// an adversary, which a record's trust never rests on.
namespace pm::sdk {

namespace detail {

class MapHash {
 public:
  void bytes(const void* data, size_t size) {
    const auto* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < size; ++i) {
      h_ ^= p[i];
      h_ *= 1099511628211ull;
    }
  }
  void number(int64_t v) { bytes(&v, sizeof v); }
  // Null, empty and each text's length all count, so moving a character between fields changes the digest.
  void text(const char* s) {
    if (!s) {
      number(-1);
      return;
    }
    size_t n = 0;
    while (s[n]) ++n;
    number(static_cast<int64_t>(n));
    bytes(s, n);
  }
  void list(std::span<const char* const> items) {
    number(static_cast<int64_t>(items.size()));
    for (const char* s : items) text(s);
  }
  uint64_t value() const { return h_; }

 private:
  uint64_t h_ = 1469598103934665603ull;
};

}  // namespace detail

/** Everything the resolver reads of `entries`, in their order. `confidence` says only how sure a note may sound, so it is left out. */
inline std::string map_digest(std::span<const EntryDesc* const> entries) {
  detail::MapHash h;
  h.number(static_cast<int64_t>(entries.size()));
  for (const EntryDesc* e : entries) {
    h.text(e->key);
    h.number(static_cast<int64_t>(e->kind));
    h.number(e->flags);
    for (const char* s : {e->owner, e->name, e->hint, e->assembly, e->ns, e->type}) h.text(s);
    h.list(e->params);
    h.list(e->fieldTypes);
    h.list(e->values);
    h.number(static_cast<int64_t>(e->facts.size()));
    for (const Fact& f : e->facts) {
      h.number(static_cast<int64_t>(f.is));
      h.text(f.method);
      h.number(f.value);
      h.number(f.negate ? 1 : 0);
    }
    for (const char* s : {e->pattern, e->within}) h.text(s);
    h.number(e->patternOffset);
    for (const char* s : {e->scene, e->path, e->component, e->sameFieldAs}) h.text(s);
    h.number(static_cast<int64_t>(e->access));
    h.list(e->typeArgs);
  }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", static_cast<unsigned long long>(h.value()));
  return hex;
}

}  // namespace pm::sdk
