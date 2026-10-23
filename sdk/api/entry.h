#pragma once

#include <cstdint>
#include <span>

namespace pg::sdk {

// What a map entry names in the game.
enum class Kind : uint8_t { Type, Method, Field, EnumValue, Code };

// How an entry's identification was established. A note never claims more than this.
enum class Confidence : uint8_t { Inferred, Decompiled, Live };

enum EntryFlags : uint32_t {
  kStatic = 1,    // a static method or field
  kOptional = 2,  // the entry may stay unresolved without failing the build
  kTrack = 4,     // the entry is recorded in the checked-build record
};

// A code fact about a method's body, used to tell look-alike candidates apart.
struct Fact {
  enum class Is : uint8_t { Stores, StoresConst, Loads, Calls, CalledBy };
  Is is;
  const char* method;
  int64_t value = 0;
  bool negate = false;
};

// One entry of the game map. Every field has a default so map headers can use designated
// initialisers and name only what an entry needs.
struct EntryDesc {
  const char* key = nullptr;
  Kind kind = Kind::Type;
  Confidence confidence = Confidence::Inferred;
  uint32_t flags = 0;
  const char* owner = nullptr;     // key of the declaring Type entry
  const char* name = nullptr;      // the real name, or null when obfuscated
  const char* hint = nullptr;      // the obfuscated name in the last mapped build
  const char* assembly = nullptr;  // image name without ".dll"
  const char* ns = nullptr;
  // A field's type or a method's return type. "@Key" means the type that entry Key resolved to.
  const char* type = nullptr;
  std::span<const char* const> params{};
  std::span<const char* const> fieldTypes{};  // a Type found by shape: field types it must have
  std::span<const char* const> values{};      // an enum Type found by shape: value names it must have
  std::span<const Fact> facts{};
  const char* pattern = nullptr;  // hex bytes, "??" for a wildcard
  const char* within = nullptr;   // key of the Method the pattern is searched in
  int32_t patternOffset = 0;
};

}  // namespace pg::sdk
