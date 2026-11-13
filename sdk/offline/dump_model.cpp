#include "dump_model.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "types.h"

namespace fs = std::filesystem;

namespace pm::sdk {
namespace {

constexpr std::string_view kModifiers[] = {"public", "private",  "internal", "protected", "static",   "abstract",
                                           "sealed", "override", "virtual",  "extern",    "readonly", "unsafe",
                                           "new",    "partial",  "async",    "volatile",  "const"};

bool is_modifier(std::string_view word) {
  return std::find(std::begin(kModifiers), std::end(kModifiers), word) != std::end(kModifiers);
}

std::string_view trim(std::string_view s) {
  while (!s.empty() && (s.front() == ' ' || s.front() == '\t' || s.front() == '\r')) s.remove_prefix(1);
  while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r')) s.remove_suffix(1);
  return s;
}

bool starts_with_word(std::string_view s, std::string_view word) {
  return s.starts_with(word) && (s.size() == word.size() || s[word.size()] == ' ');
}

// Splits at `separator` where no angle bracket, parenthesis or quote is open.
std::vector<std::string_view> split_top(std::string_view s, char separator) {
  std::vector<std::string_view> parts;
  int depth = 0;
  bool quoted = false;
  size_t start = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '"' && (i == 0 || s[i - 1] != '\\')) quoted = !quoted;
    if (quoted) continue;
    if (c == '<' || c == '(') ++depth;
    if (c == '>' || c == ')') --depth;
    if (c == separator && depth == 0) {
      parts.push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }
  parts.push_back(s.substr(start));
  return parts;
}

// The text before a top-level '=' (a default value, a constant's value).
std::string_view before_equals(std::string_view s) {
  int depth = 0;
  bool quoted = false;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    if (c == '"' && (i == 0 || s[i - 1] != '\\')) quoted = !quoted;
    if (quoted) continue;
    if (c == '<') ++depth;
    if (c == '>') --depth;
    if (c == '=' && depth == 0) return trim(s.substr(0, i));
  }
  return trim(s);
}

// The words of a declaration, split where no generic argument list is open.
std::vector<std::string_view> words(std::string_view s) {
  std::vector<std::string_view> out;
  for (const std::string_view w : split_top(s, ' '))
    if (!w.empty()) out.push_back(w);
  return out;
}

std::string join_words(const std::vector<std::string_view>& w, size_t from, size_t to) {
  std::string out;
  for (size_t i = from; i < to; ++i) {
    if (!out.empty()) out += ' ';
    out += w[i];
  }
  return out;
}

bool parse_hex(std::string_view text, uint64_t& value) {
  if (text.starts_with("0x") || text.starts_with("0X")) text.remove_prefix(2);
  if (text.empty()) return false;
  return std::from_chars(text.data(), text.data() + text.size(), value, 16).ec == std::errc();
}

// The value of `key = "0x..."` inside an attribute line.
bool attribute_hex(std::string_view line, std::string_view key, uint64_t& value) {
  const size_t at = line.find(key);
  if (at == std::string_view::npos) return false;
  const size_t open = line.find('"', at);
  if (open == std::string_view::npos) return false;
  const size_t close = line.find('"', open + 1);
  if (close == std::string_view::npos) return false;
  return parse_hex(line.substr(open + 1, close - open - 1), value);
}

// An integer constant's number, from the whole text; one past the i64 range wraps like the compiler's
// own constant. "0.5", "1E-05" and "True" are no integers.
bool parse_literal(std::string_view text, int64_t& value) {
  text = trim(text);
  if (text.empty()) return false;
  const char* end = text.data() + text.size();
  if (const auto r = std::from_chars(text.data(), end, value); r.ec == std::errc() && r.ptr == end) return true;
  uint64_t big = 0;
  if (const auto r = std::from_chars(text.data(), end, big); r.ec != std::errc() || r.ptr != end) return false;
  value = static_cast<int64_t>(big);
  return true;
}

// One parameter: its type, written as the runtime would ("ref"/"out" kept, name and default dropped).
std::string parameter_type(std::string_view text) {
  std::vector<std::string_view> w = words(before_equals(text));
  std::string prefix;
  while (!w.empty() && (w.front() == "out" || w.front() == "ref" || w.front() == "in" || w.front() == "params" ||
                        w.front() == "this")) {
    if (w.front() == "out" || w.front() == "ref" || w.front() == "in")
      prefix = w.front() == "out" ? "out " : "ref ";
    w.erase(w.begin());
  }
  if (w.size() > 1) w.pop_back();  // the parameter's name
  return canonical_type(prefix + join_words(w, 0, w.size()));
}

// Whether a declaration with a string value continues on the next line: the dump writes the value raw,
// so a value holding a line break spans lines, and one holding quotes cannot be told apart by counting
// them. A constant string's declaration ends at the line that carries the field offset comment; a
// method's whose default argument is such a string ends at the line that carries its empty body
// (Unity.InputSystem's StringHelpers.Escape).
bool continues(std::string_view line) {
  if (line.find("= \"") == std::string_view::npos) return false;
  if (line.find("const string ") != std::string_view::npos) return line.find("//Field offset:") == std::string_view::npos;
  // An attribute ("[Address(...)]", a default value's) is one line: only a declaration continues.
  const std::string_view declaration = trim(line);
  return !declaration.starts_with('[') && line.find('(') != std::string_view::npos && !declaration.ends_with("{ }") &&
         !declaration.ends_with(';');
}

// Whether a constant of this canonical type has an integer value: an integer type, or a named type,
// since a constant of a named type is an enum value (or null).
bool integer_or_enum(std::string_view type) {
  constexpr std::string_view kIntegers[] = {"sbyte", "byte", "short", "ushort", "int", "uint", "long", "ulong"};
  constexpr std::string_view kOthers[] = {"bool", "char", "float", "double", "decimal", "string", "object"};
  if (std::find(std::begin(kIntegers), std::end(kIntegers), type) != std::end(kIntegers)) return true;
  if (std::find(std::begin(kOthers), std::end(kOthers), type) != std::end(kOthers)) return false;
  return !type.empty() && type.find_first_of("<[*&") == std::string_view::npos;
}

struct Declaration {
  std::string name;
  std::string ret;
  bool isStatic = false;
  std::vector<std::string> params;
};

// "public static bool TryFind(string a, out Ghost b = null)" and the like; a constructor has one word
// before its parenthesis.
bool parse_method(std::string_view line, Declaration& out) {
  const size_t open = line.find('(');
  const size_t close = line.rfind(')');
  if (open == std::string_view::npos || close == std::string_view::npos || close < open) return false;
  std::vector<std::string_view> w = words(line.substr(0, open));
  for (const std::string_view word : w)
    if (word == "static") out.isStatic = true;
  while (!w.empty() && is_modifier(w.front())) w.erase(w.begin());
  if (w.empty()) return false;
  const auto op = std::find(w.begin(), w.end(), "operator");
  if (op != w.end()) {
    const size_t k = static_cast<size_t>(op - w.begin());
    out.name = join_words(w, k, w.size());
    size_t retEnd = k;
    if (retEnd > 0 && (w[retEnd - 1] == "implicit" || w[retEnd - 1] == "explicit")) --retEnd;
    out.ret = canonical_type(join_words(w, 0, retEnd));
  } else if (w.size() == 1) {
    out.name = out.isStatic ? ".cctor" : ".ctor";
    out.ret = "void";
  } else {
    out.name = std::string(w.back());
    out.ret = canonical_type(join_words(w, 0, w.size() - 1));
  }
  const std::string_view list = trim(line.substr(open + 1, close - open - 1));
  if (!list.empty())
    for (const std::string_view p : split_top(list, ','))
      if (!trim(p).empty()) out.params.push_back(parameter_type(trim(p)));
  return true;
}

// The part of a type declaration line before its base list: "private sealed class Name<T> : Base".
// The base list after a type declaration's name, ": Base, IThing<T>", as short names without generic
// arguments, and in `arities` the number of type arguments each is written with (0 and 1); empty when
// there is none.
std::vector<std::string> base_names(std::string_view line, std::vector<int>& arities) {
  std::vector<std::string> out;
  arities.clear();
  const size_t colon = line.find(" : ");
  if (colon == std::string_view::npos) return out;
  int depth = 0, arity = 0;
  std::string current;
  auto flush = [&] {
    std::string name = canonical_type(trim(current));
    if (const size_t lt = name.find('<'); lt != std::string::npos) name.resize(lt);
    if (!name.empty()) {
      out.push_back(name == "object" ? std::string("Object") : std::move(name));
      arities.push_back(arity);
    }
    current.clear();
    arity = 0;
  };
  for (const char c : line.substr(colon + 3)) {
    if (c == '<' && depth++ == 0) arity = 1;
    if (c == '>') --depth;
    if (c == ',' && depth == 1) ++arity;
    if (c == ',' && depth == 0) flush();
    else current += c;
  }
  flush();
  return out;
}

// A generic definition's number of type parameters from its file's name: Cpp2IL writes the declaration without them
// ("public class UnityEvent : UnityEventBase") but names the file "UnityEvent`1.cs". 0 for a name without an arity.
int file_arity(std::string_view stem) {
  const size_t tick = stem.rfind('`');
  if (tick == std::string_view::npos || tick + 1 >= stem.size() || stem.size() - tick > 4) return 0;
  int arity = 0;
  for (const char c : stem.substr(tick + 1)) {
    if (c < '0' || c > '9') return 0;
    arity = arity * 10 + (c - '0');
  }
  return arity;
}

bool parse_type_line(std::string_view line, std::string& name, bool& isEnum, bool& isStruct, bool& isInterface) {
  if (line.find('(') != std::string_view::npos || line.find(';') != std::string_view::npos) return false;
  const std::vector<std::string_view> w = words(line);
  size_t k = 0;
  while (k < w.size() && is_modifier(w[k])) ++k;
  if (k + 1 >= w.size()) return false;
  const std::string_view keyword = w[k];
  if (keyword != "class" && keyword != "struct" && keyword != "enum" && keyword != "interface") return false;
  std::string_view n = w[k + 1];
  // A compiler-generated name starts with its own angle brackets: "<>c__DisplayClass5_0".
  const size_t skip = n.starts_with('<') ? n.find('>') + 1 : 0;
  n = n.substr(0, std::min(n.size(), n.find_first_of("<`:", skip)));
  if (n.empty()) return false;
  // Cpp2IL declares System.Object and UnityEngine.Object as "class object" (and writes UnityEngine.Object
  // as `object` inside UnityEngine's own files); no C# type can be named so, and the runtime names both
  // "Object".
  name = n == "object" ? std::string("Object") : std::string(n);
  isEnum = keyword == "enum";
  isStruct = keyword == "struct";
  isInterface = keyword == "interface";
  return true;
}

struct Frame {
  int depth;
  int type;
};

// Reads one file's types into the model, nesting by tab depth.
// A line that looks like a member but is neither a field, a method nor an enum member is counted in
// `unparsed`.
void parse_file(const std::string& text, const std::string& assembly, const std::string& ns, int arity,
                GameModel& model, size_t& unparsed) {
  std::vector<Frame> stack;
  uint64_t pendingRva = 0, pendingToken = 0;
  int64_t nextLiteral = 0;
  std::istringstream in(text);
  std::string raw;
  while (std::getline(in, raw)) {
    for (std::string more; continues(raw) && std::getline(in, more);) raw += "\n" + more;
    int depth = 0;
    while (depth < static_cast<int>(raw.size()) && raw[depth] == '\t') ++depth;
    std::string_view line = trim(raw);
    if (line.empty() || line.starts_with("//") || line == "{" || line == "}") continue;
    if (line.starts_with('[')) {
      uint64_t rva = 0;
      if (line.starts_with("[Address(") && attribute_hex(line, "RVA", rva)) pendingRva = rva;
      if (line.starts_with("[Token(") && attribute_hex(line, "Token", rva)) pendingToken = rva;
      continue;
    }
    const uint64_t rva = std::exchange(pendingRva, 0);
    const uint64_t token = std::exchange(pendingToken, 0);

    while (!stack.empty() && stack.back().depth >= depth) stack.pop_back();

    std::string name;
    bool isEnum = false, isStruct = false, isInterface = false;
    if (parse_type_line(line, name, isEnum, isStruct, isInterface)) {
      // "public struct float" in System is the runtime's Single: named as the runtime names it.
      if (stack.empty() && ns == "System")
        if (const std::string_view full = system_value_type_name(name); !full.empty()) name = full;
      ModelType t;
      t.assembly = assembly;
      t.ns = ns;
      t.name = std::move(name);
      t.declaring = stack.empty() ? -1 : stack.back().type;
      t.isEnum = isEnum;
      t.isValueType = isEnum || isStruct;
      t.isInterface = isInterface;
      // The file's name gives the arity of its top-level type only; a nested type's is not known, so kNotGeneric
      // applies to top-level types (sdk/resolver).
      t.genericParams = stack.empty() ? arity : 0;
      if (!isEnum) t.bases = base_names(line, t.baseArities);  // an enum's ": byte" names its storage, not a base
      model.types.push_back(std::move(t));
      stack.push_back({depth, static_cast<int>(model.types.size()) - 1});
      nextLiteral = 0;
      continue;
    }
    if (stack.empty()) {
      if (!line.starts_with("namespace ")) ++unparsed;  // a line outside any type
      continue;
    }
    // A member sits one tab inside its type; deeper lines are a property's accessors.
    if (depth != stack.back().depth + 1) continue;
    ModelType& type = model.types[stack.back().type];

    if (type.isEnum) {
      const std::string_view member = line.substr(0, line.find(','));
      const size_t eq = member.find('=');
      int64_t value = nextLiteral;
      if (eq != std::string_view::npos && !parse_literal(member.substr(eq + 1), value)) {
        ++unparsed;
        continue;
      }
      ModelField f;
      f.name = std::string(trim(member.substr(0, eq)));
      f.type = type.name;
      f.isStatic = true;
      f.hasLiteral = true;
      f.access = Visibility::Public;  // an enum's values are public, though the dump writes no modifier
      f.literal = value;
      f.typeIndex = static_cast<int>(&type - model.types.data());
      nextLiteral = value + 1;
      type.fields.push_back(std::move(f));
      continue;
    }

    constexpr std::string_view kOffset = "//Field offset:";
    if (const size_t mark = line.rfind(kOffset); mark != std::string_view::npos) {
      uint64_t offset = 0;
      const std::string_view tail = trim(line.substr(mark + kOffset.size()));
      parse_hex(tail.substr(0, tail.find(' ')), offset);
      std::string_view decl = trim(line.substr(0, mark));
      if (decl.ends_with(';')) decl.remove_suffix(1);
      std::vector<std::string_view> w = words(before_equals(decl));
      if (w.size() < 2) {
        ++unparsed;
        continue;
      }
      ModelField f;
      f.name = std::string(w.back());
      f.offset = static_cast<int32_t>(offset);
      bool isPrivate = false, isProtected = false, isInternal = false, isPublic = false;
      for (const std::string_view word : w) {
        if (word == "static") f.isStatic = true;
        if (word == "const") f.isStatic = f.hasLiteral = true;
        isPrivate = isPrivate || word == "private";
        isProtected = isProtected || word == "protected";
        isInternal = isInternal || word == "internal";
        isPublic = isPublic || word == "public";
      }
      f.access = isPublic                     ? Visibility::Public
                 : isProtected && isInternal  ? Visibility::ProtectedInternal
                 : isPrivate && isProtected   ? Visibility::PrivateProtected
                 : isProtected                ? Visibility::Protected
                 : isInternal                 ? Visibility::Internal
                 : isPrivate                  ? Visibility::Private
                                              : Visibility::Any;
      // The runtime counts a value type's instance fields from its boxed form, after the object header.
      if (type.isValueType && !f.isStatic) f.offset += 0x10;
      size_t first = 0;
      while (first + 1 < w.size() && is_modifier(w[first])) ++first;
      f.type = canonical_type(join_words(w, first, w.size() - 1));
      // An integer or enum constant's literal is its number. Any other constant's stays 0, as in the
      // runtime model: a float's "360" is no integer.
      if (f.hasLiteral && integer_or_enum(f.type)) {
        const std::string_view value = trim(decl.substr(before_equals(decl).size()));
        if (value.starts_with('=')) parse_literal(value.substr(1), f.literal);
      }
      type.fields.push_back(std::move(f));
      continue;
    }

    // Methods end in "{ }" in the dump (";" for an interface's or an abstract one), properties and
    // events have no parenthesis.
    Declaration d;
    std::string_view decl = line;
    if (const size_t mark = decl.rfind("//Length:"); mark != std::string_view::npos) decl = trim(decl.substr(0, mark));
    if (decl.find('(') == std::string_view::npos) continue;
    if (!parse_method(decl, d)) {
      ++unparsed;
      continue;
    }
    ModelMethod m;
    m.name = std::move(d.name);
    m.ret = std::move(d.ret);
    m.params = std::move(d.params);
    m.paramIndex.assign(m.params.size(), -1);
    m.isStatic = d.isStatic;
    m.rva = rva;
    m.token = static_cast<uint32_t>(token);
    type.methods.push_back(std::move(m));
  }
}

// Names the model type a canonical type string is, by short name, when exactly one type has it.
class TypeNames {
 public:
  explicit TypeNames(const GameModel& model) {
    for (size_t i = 0; i < model.types.size(); ++i) {
      const ModelType& t = model.types[i];
      add(t.name, static_cast<int>(i));
      // A System value type is spelled with its keyword where it is used ("float" for Single).
      if (t.ns == "System" && t.declaring < 0)
        if (const std::string word = canonical_type("System." + t.name); word != t.name) add(word, static_cast<int>(i));
    }
  }

  int find(std::string_view type) const {
    if (type.ends_with('&')) type.remove_suffix(1);
    if (type.find_first_of("<[*,") != std::string_view::npos) return -1;
    const auto it = index_.find(std::string(type));
    return it == index_.end() ? -1 : it->second;
  }

 private:
  void add(const std::string& name, int i) {
    int& slot = index_.try_emplace(name, i).first->second;
    if (slot != i) slot = -1;
  }

  std::map<std::string, int> index_;
};

bool read_file(const fs::path& path, std::string& text) {
  std::ifstream in(path, std::ios::binary);
  if (!in) return false;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  text = buffer.str();
  if (text.starts_with("\xEF\xBB\xBF")) text.erase(0, 3);
  return true;
}

}  // namespace

bool load_dump(const fs::path& diffableCsRoot, std::span<const std::string> assemblies, GameModel& out,
               std::string& error, size_t* unparsed) {
  std::error_code ec;
  if (!fs::is_directory(diffableCsRoot, ec)) {
    error = "cannot read the dump folder " + diffableCsRoot.generic_string();
    return false;
  }
  GameModel model;
  model.objectArraysLookAlike = true;
  size_t count = 0;
  for (const std::string& assembly : assemblies) {
    const fs::path folder = diffableCsRoot / fs::path(std::u8string(assembly.begin(), assembly.end()));
    if (!fs::is_directory(folder, ec)) {
      error = "cannot read the assembly folder " + folder.generic_string();
      return false;
    }
    std::vector<fs::path> files;
    for (fs::recursive_directory_iterator it(folder, ec), end; !ec && it != end; it.increment(ec))
      if (it->is_regular_file() && it->path().extension() == ".cs") files.push_back(it->path());
    if (ec) {
      error = "cannot list " + folder.generic_string() + ": " + ec.message();
      return false;
    }
    std::sort(files.begin(), files.end());
    for (const fs::path& file : files) {
      std::string text;
      if (!read_file(file, text)) {
        error = "cannot read " + file.generic_string();
        return false;
      }
      std::string ns;
      for (const fs::path& part : fs::relative(file.parent_path(), folder)) {
        if (part == ".") continue;
        if (!ns.empty()) ns += '.';
        for (const char8_t c : part.u8string()) ns += static_cast<char>(c);
      }
      std::string stem;
      for (const char8_t c : file.stem().u8string()) stem += static_cast<char>(c);
      parse_file(text, assembly, ns, file_arity(stem), model, count);
    }
  }

  const TypeNames names(model);
  for (ModelType& type : model.types) {
    for (ModelField& f : type.fields)
      if (f.typeIndex < 0) f.typeIndex = names.find(f.type);  // an enum's members already name it
    for (ModelMethod& m : type.methods) {
      m.retIndex = names.find(m.ret);
      for (size_t i = 0; i < m.params.size(); ++i) m.paramIndex[i] = names.find(m.params[i]);
    }
  }
  if (unparsed) *unparsed = count;
  out = std::move(model);
  return true;
}

bool read_method_tables(std::span<const uint8_t> mapped, std::span<const std::string> assemblies, MethodTables& out,
                        std::string& error) {
  const auto u32 = [&](size_t at) {
    uint32_t v = 0;
    std::memcpy(&v, mapped.data() + at, sizeof v);
    return v;
  };
  const auto u64 = [&](size_t at) {
    uint64_t v = 0;
    std::memcpy(&v, mapped.data() + at, sizeof v);
    return v;
  };
  if (mapped.size() < 0x40 || u32(0x3C) > mapped.size() - 0x40) {
    error = "the image has no PE32+ headers";
    return false;
  }
  const uint64_t base = u64(u32(0x3C) + 24 + 24);  // ImageBase of the optional header
  const auto in_image = [&](uint64_t va) { return va >= base && va - base < mapped.size(); };

  // Where each assembly's module name lies: "<assembly>.dll" as a whole C string.
  const std::string_view bytes(reinterpret_cast<const char*>(mapped.data()), mapped.size());
  std::map<uint64_t, std::string> nameAt;  // by address, as the module holds it
  for (const std::string& assembly : assemblies) {
    const std::string needle = assembly + ".dll" + std::string(1, '\0');
    for (size_t at = bytes.find(needle); at != std::string_view::npos; at = bytes.find(needle, at + 1))
      if (at == 0 || bytes[at - 1] == '\0') nameAt.emplace(base + at, assembly);
  }

  // The module begins { const char* moduleName; uint32_t methodPointerCount; Il2CppMethodPointer* methodPointers; }.
  constexpr uint64_t kMaxMethods = 2000000;
  MethodTables found;
  std::set<std::string> several;
  for (size_t at = 0; at + 24 <= mapped.size(); at += 8) {
    const auto name = nameAt.find(u64(at));
    if (name == nameAt.end()) continue;
    const uint64_t count = u32(at + 8), table = u64(at + 16);
    if (count == 0 || count > kMaxMethods || !in_image(table) || table % 8 != 0) continue;
    if (table - base + count * 8 > mapped.size()) continue;
    std::vector<uint64_t> rvas(count);
    bool pointers = true;
    for (uint64_t i = 0; i < count && pointers; ++i) {
      const uint64_t va = u64(table - base + i * 8);
      pointers = va == 0 || in_image(va);
      rvas[i] = va ? va - base : 0;
    }
    if (!pointers) continue;
    if (!found.emplace(name->second, std::move(rvas)).second) several.insert(name->second);
  }
  for (const std::string& assembly : assemblies) {
    if (several.contains(assembly)) {
      error = "the image holds several code-gen modules for " + assembly;
      return false;
    }
    if (!found.contains(assembly)) {
      error = "the image holds no code-gen module for " + assembly;
      return false;
    }
  }
  out = std::move(found);
  return true;
}

void apply_method_tables(const MethodTables& tables, GameModel& model) {
  constexpr uint32_t kMethodDef = 0x06000000;
  for (ModelType& type : model.types) {
    const auto table = tables.find(type.assembly);
    for (ModelMethod& m : type.methods) {
      const uint32_t row = m.token & 0x00FFFFFF;
      const bool listed = table != tables.end() && (m.token & 0xFF000000) == kMethodDef && row >= 1 &&
                          row <= table->second.size();
      m.rva = listed ? table->second[row - 1] : 0;
    }
  }
}

}  // namespace pm::sdk
