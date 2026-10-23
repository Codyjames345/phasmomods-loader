#pragma once

#include <filesystem>
#include <span>
#include <string>

#include "model.h"

namespace pg::sdk {

// Builds the game model from a Cpp2IL dump (--output-as diffable-cs, with the attribute injector).
// Reads only the named assembly folders under the root. A type's assembly is its top folder and its
// namespace the path below it. Methods take their rva from [Address(RVA = ...)] (0 without one),
// fields their offset from the "Field offset" comment, and enum members become literal static fields.
// Type strings are canonical; parameter names and default values are dropped.
//
// typeIndex, retIndex and paramIndex name the model type whose short name the string is, when exactly
// one type has that name, and are -1 otherwise.
//
// Every line that looks like a member yet is neither a field, a method nor an enum member is counted in
// *unparsed when given; the caller reports it. A non-zero count means the model may be missing members.
// The runtime's conventions apply: constructors are ".ctor" and ".cctor", constants are literals, and a
// value type's instance field offsets include the 0x10 object header (the dump prints them unboxed).
//
// On failure it returns false with the reason, naming the path, in error.
bool load_dump(const std::filesystem::path& diffableCsRoot, std::span<const std::string> assemblies,
               GameModel& out, std::string& error, size_t* unparsed = nullptr);

}  // namespace pg::sdk
