#include "types.h"

#include <string_view>
#include <utility>

namespace pg::sdk {
namespace {

constexpr std::pair<std::string_view, std::string_view> kPrimitives[] = {
    {"Boolean", "bool"},  {"Byte", "byte"},     {"SByte", "sbyte"},     {"Char", "char"},
    {"Int16", "short"},   {"UInt16", "ushort"}, {"Int32", "int"},       {"UInt32", "uint"},
    {"Int64", "long"},    {"UInt64", "ulong"},  {"Single", "float"},    {"Double", "double"},
    {"Decimal", "decimal"}, {"String", "string"}, {"Object", "object"}, {"Void", "void"},
};

bool is_delimiter(char c) {
  return c == '<' || c == '>' || c == ',' || c == '[' || c == ']' || c == '&' || c == '*' || c == ' ';
}

// One dotted, possibly nested, possibly arity-suffixed name.
std::string canonical_name(std::string_view name) {
  std::string_view qualifier;
  if (const size_t cut = name.find_last_of("./"); cut != std::string_view::npos) {
    qualifier = name.substr(0, cut);
    name.remove_prefix(cut + 1);
  }
  if (const size_t tick = name.find('`'); tick != std::string_view::npos) name = name.substr(0, tick);
  // Only System's own names, or a bare name the dump wrote in full, are primitives. A bare "Object"
  // is UnityEngine.Object in the dump, so it stays.
  if ((qualifier.empty() && name != "Object") || qualifier == "System")
    for (const auto& [full, keyword] : kPrimitives)
      if (name == full) return std::string(keyword);
  return std::string(name);
}

}  // namespace

std::string canonical_type(std::string_view raw) {
  bool byRef = false;
  for (const std::string_view prefix : {std::string_view("out "), std::string_view("ref ")})
    if (raw.substr(0, prefix.size()) == prefix) {
      byRef = true;
      raw.remove_prefix(prefix.size());
      break;
    }

  std::string out;
  for (size_t i = 0; i < raw.size();) {
    if (raw[i] == ' ') {
      ++i;
    } else if (is_delimiter(raw[i])) {
      out += raw[i++];
    } else {
      size_t end = i;
      while (end < raw.size() && !is_delimiter(raw[end])) ++end;
      out += canonical_name(raw.substr(i, end - i));
      i = end;
    }
  }
  if (byRef) out += '&';
  return out;
}

}  // namespace pg::sdk
