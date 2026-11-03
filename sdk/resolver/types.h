#pragma once

#include <string>
#include <string_view>

namespace pm::sdk {

// One spelling for a type name. The runtime writes "System.Collections.Generic.List<UnityEngine.Vector3>",
// "GhostAI/Nested" and "Player&"; the dump writes "List<Vector3>", "Nested" and "out Player". Both
// reach the same string: no namespaces or declaring types, no generic arity, C# keywords for the
// System primitives, a trailing '&' for by-reference, and no spaces.
std::string canonical_type(std::string_view raw);

// The runtime's name of the System value type a C# keyword stands for ("float" -> "Single"), or empty
// for any other word: the dump declares those structs by their keywords. "object", "string" and
// "void" are not value types and give empty.
std::string_view system_value_type_name(std::string_view keyword);

}  // namespace pm::sdk
