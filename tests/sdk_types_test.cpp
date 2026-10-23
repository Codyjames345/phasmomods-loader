#include <string>

#include "check.h"
#include "entry.h"
#include "types.h"

using pg::sdk::canonical_type;

namespace {
// Both spellings of a type, the runtime's and the dump's, must reach the same canonical form.
void same(const char* runtime, const char* dump, const char* canonical) {
  CHECK(canonical_type(runtime) == canonical);
  CHECK(canonical_type(dump) == canonical);
}
}  // namespace

int main() {
  same("System.Boolean", "bool", "bool");
  same("System.Single", "float", "float");
  CHECK(canonical_type("Single") == "float");
  same("System.Int32[]", "int[]", "int[]");
  CHECK(canonical_type("Int32[]") == "int[]");
  same("System.Collections.Generic.List<UnityEngine.Vector3>", "List<Vector3>", "List<Vector3>");
  same("System.Collections.Generic.Dictionary`2<System.String,Player>", "Dictionary<string, Player>",
       "Dictionary<string,Player>");
  same("GhostAI/ദശസറദബഹജഭ", "ദശസറദബഹജഭ", "ദശസറദബഹജഭ");
  same("Player&", "out Player", "Player&");
  CHECK(canonical_type("ref Player") == "Player&");
  same("UnityEngine.Vector3", "Vector3", "Vector3");
  same("System.Void", "void", "void");

  same("System.Object", "object", "object");
  same("System.String", "string", "string");
  same("System.Byte", "byte", "byte");
  same("System.Int64", "long", "long");
  same("System.Double", "double", "double");
  same("System.UInt32", "uint", "uint");
  same("System.Char", "char", "char");

  // A game class that shares a primitive's name is not the primitive.
  CHECK(canonical_type("UnityEngine.Object") == "Object");
  // Nested generics, nested types and multi-dimensional arrays.
  same("System.Collections.Generic.List`1<System.Collections.Generic.List`1<System.Int32>>",
       "List<List<int>>", "List<List<int>>");
  same("Outer`1/Inner`1<System.String>", "Inner<string>", "Inner<string>");
  same("System.Int32[,]", "int[,]", "int[,]");
  same("Player[]&", "ref Player[]", "Player[]&");
  CHECK(canonical_type("") == "");

  // The entry vocabulary: map headers name only what an entry needs.
  constexpr pg::sdk::EntryDesc entry{.key = "Ghost.speed",
                                     .kind = pg::sdk::Kind::Field,
                                     .confidence = pg::sdk::Confidence::Live,
                                     .flags = pg::sdk::kOptional | pg::sdk::kTrack,
                                     .type = "float"};
  CHECK(entry.owner == nullptr && entry.params.empty() && entry.facts.empty());
  CHECK(entry.patternOffset == 0 && (entry.flags & pg::sdk::kStatic) == 0);

  return g_failures ? 1 : 0;
}
