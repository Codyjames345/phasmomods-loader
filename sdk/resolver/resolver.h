#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "entry.h"
#include "model.h"

namespace pg::sdk {

// Resolved: exactly one candidate survived every check. Missing: none did. Ambiguous: more than one
// did, and the resolver never picks among them. Blocked: an entry this one depends on did not resolve.
enum class Status { Resolved, Missing, Ambiguous, Blocked };

struct Result {
  const EntryDesc* desc = nullptr;
  Status status = Status::Missing;
  std::string detail;                   // why it is not Resolved, or what changed
  std::vector<std::string> candidates;  // the candidates of an Ambiguous entry, readable
  int type = -1;    // the type, or a member's owner type, as an index into GameModel::types
  int member = -1;  // index into the owner's fields (Field, EnumValue) or methods (Method)
  uint64_t rva = 0;
  std::vector<uint64_t> twins;
  std::vector<std::string> aliases;
  int32_t offset = 0;  // a field's offset
  int64_t value = 0;   // an enum value's number
  std::string currentName;  // the name in this build
};

struct Resolution {
  std::vector<Result> results;  // one per entry, in the order the entries were given
  const Result* find(std::string_view key) const;
  // Every entry not marked kOptional resolved.
  bool required_resolved() const;
};

class CodeImage;

// Resolves every entry against the model, each after the entries it refers to. Without an image,
// entries that need machine code are Missing with the detail "no code image".
Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image);

}  // namespace pg::sdk
