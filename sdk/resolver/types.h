#pragma once

#include <string>
#include <string_view>

namespace pg::sdk {

// One spelling for a type name. The runtime writes "System.Collections.Generic.List<UnityEngine.Vector3>",
// "GhostAI/Nested" and "Player&"; the dump writes "List<Vector3>", "Nested" and "out Player". Both
// reach the same string: no namespaces or declaring types, no generic arity, C# keywords for the
// System primitives, a trailing '&' for by-reference, and no spaces.
std::string canonical_type(std::string_view raw);

}  // namespace pg::sdk
