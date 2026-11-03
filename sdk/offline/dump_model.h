#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <vector>

#include "model.h"

namespace pm::sdk {

// Builds the game model from a Cpp2IL dump (--output-as diffable-cs, with the attribute injector).
// Reads only the named assembly folders under the root. A type's assembly is its top folder and its
// namespace the path below it. Methods take their rva from [Address(RVA = ...)] (0 without one) and
// their token from [Token(Token = ...)], fields their offset from the "Field offset" comment, and enum
// members become literal static fields. A dump writes a shared instantiation's address on a generic
// definition's method, which has no code of its own at runtime: apply_method_tables settles that.
// Type strings are canonical; parameter names and default values are dropped.
//
// typeIndex, retIndex and paramIndex name the model type whose short name the string is, when exactly
// one type has that name, and are -1 otherwise.
//
// Every line that looks like a member yet is neither a field, a method nor an enum member is counted in
// *unparsed when given; the caller reports it. A non-zero count means the model may be missing members.
// The runtime's conventions apply: constructors are ".ctor" and ".cctor", constants are literals (an
// integer or enum constant's literal is its number, any other's is 0), and a value type's instance
// field offsets include the 0x10 object header (the dump prints them unboxed).
//
// On failure it returns false with the reason, naming the path, in error.
bool load_dump(const std::filesystem::path& diffableCsRoot, std::span<const std::string> assemblies,
               GameModel& out, std::string& error, size_t* unparsed = nullptr);

// The method pointer tables of GameAssembly's code-gen modules, where the runtime takes each method's
// address: for each assembly, the RVA of each method row (the row of token 0x06000001 first), 0 for a
// method with no code of its own. An abstract, extern or generic definition's row holds 0.
using MethodTables = std::map<std::string, std::vector<uint64_t>, std::less<>>;

// Reads the named assemblies' tables from an image laid out by RVA. A module is the one record that holds
// the address of "<assembly>.dll" as its name, then a plausible count and table. On failure, none found
// or several for an assembly, it returns false with the reason in error.
bool read_method_tables(std::span<const uint8_t> mapped, std::span<const std::string> assemblies, MethodTables& out,
                        std::string& error);

// Gives each method the address the runtime gives it: its row of its assembly's table, or 0 when it has
// no such row. The dump model then answers as the runtime model does, whatever the dump wrote.
void apply_method_tables(const MethodTables& tables, GameModel& model);

}  // namespace pm::sdk
