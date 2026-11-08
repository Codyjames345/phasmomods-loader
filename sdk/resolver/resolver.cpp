#include "resolver.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>

#include "code.h"
#include "image.h"
#include "types.h"

namespace pm::sdk {
namespace {

// A type string "@Key" or "@Key&": the type the entry Key resolved to, and what follows the key.
struct EntryRef {
  std::string_view key, suffix;
};

std::optional<EntryRef> entry_ref(const char* type) {
  if (!type || type[0] != '@') return std::nullopt;
  const std::string_view rest(type + 1);
  const size_t end = std::min(rest.find_first_of("&[]<>,*"), rest.size());
  return EntryRef{rest.substr(0, end), rest.substr(end)};
}

// What ends the key of an "@Key" inside a type string.
constexpr const char* kRefEnd = "&[]<>,* ";

// Calls f with the key of every "@Key" in a type string: at its start, or inside a generic's
// arguments ("List<@Key>", "Dictionary<@Key, int>").
template <typename F>
void for_each_ref(const char* type, F&& f) {
  if (!type) return;
  const std::string_view text(type);
  for (size_t at = text.find('@'); at != std::string_view::npos; at = text.find('@', at + 1)) {
    const std::string_view rest = text.substr(at + 1);
    f(rest.substr(0, std::min(rest.find_first_of(kRefEnd), rest.size())));
  }
}

// Calls f with the key of every entry d refers to.
template <typename F>
void for_each_dependency(const EntryDesc& d, F&& f) {
  if (d.owner) f(std::string_view(d.owner));
  if (d.sameFieldAs) f(std::string_view(d.sameFieldAs));
  for_each_ref(d.type, f);
  for (const std::span<const char* const> list : {d.params, d.fieldTypes, d.typeArgs})
    for (const char* type : list) for_each_ref(type, f);
  for (const Fact& fact : d.facts)
    if (fact.method) f(std::string_view(fact.method));
  // A native site's `within` names a UnityPlayer export or section, never an entry.
  if (d.within && d.kind != Kind::NativeSite) f(std::string_view(d.within));
}

// UnityPlayer.dll's name without ".dll", a native site's assembly, and the section a native site
// searches when it names no export.
constexpr std::string_view kUnityPlayer = "UnityPlayer";
constexpr std::string_view kGameAssembly = "GameAssembly";
constexpr std::string_view kTextSection = ".text";
// A pattern searched in the whole .text section must have this many bytes that are not wildcards: a
// shorter one could match by chance in 25 MB of code.
constexpr size_t kMinTextPattern = 16;
// The scan for a native site stops at this many matches: more would say no more than that it is
// ambiguous.
constexpr size_t kMaxNativeMatches = 8;

const char* kind_name(Kind kind) {
  switch (kind) {
    case Kind::Type: return "type";
    case Kind::Method: return "method";
    case Kind::Field: return "field";
    case Kind::EnumValue: return "enum value";
    case Kind::Code: return "code";
    case Kind::Icall: return "icall";
    case Kind::Export: return "export";
    case Kind::Scene: return "scene";
    case Kind::Layer: return "layer";
    case Kind::AnimatorParam: return "animator parameter";
    case Kind::NativeSite: return "native site";
    case Kind::ShaderProperty: return "shader property";
    case Kind::Inflated: return "inflated method";
  }
  return "entry";
}

const char* fact_name(Fact::Is is) {
  switch (is) {
    case Fact::Is::Stores: return "Stores";
    case Fact::Is::StoresConst: return "StoresConst";
    case Fact::Is::Loads: return "Loads";
    case Fact::Is::Calls: return "Calls";
    case Fact::Is::CalledBy: return "CalledBy";
    case Fact::Is::Called: return "Called";
    case Fact::Is::Compares: return "Compares";
    case Fact::Is::MovesConst: return "MovesConst";
    case Fact::Is::IsNot: return "IsNot";
  }
  return "fact";
}

int hex_digit(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// A pattern's bytes, -1 for a wildcard: two hex digits or "??" per byte, the bytes separated by
// spaces.
std::optional<std::vector<int>> parse_pattern(std::string_view text) {
  std::vector<int> out;
  for (size_t i = 0; i < text.size();) {
    if (text[i] == ' ') {
      ++i;
      continue;
    }
    if (i + 1 >= text.size() || (i + 2 < text.size() && text[i + 2] != ' ')) return std::nullopt;
    if (text[i] == '?' && text[i + 1] == '?') {
      out.push_back(-1);
    } else {
      const int hi = hex_digit(text[i]), lo = hex_digit(text[i + 1]);
      if (hi < 0 || lo < 0) return std::nullopt;
      out.push_back(hi << 4 | lo);
    }
    i += 2;
  }
  if (out.empty()) return std::nullopt;
  return out;
}

// What a fact says of a candidate. Unread: the code it needs could not be fully read, so it says
// nothing either way.
enum class Verdict { Yes, No, Unread };

// A type or one of its members: member is -1 for a type.
struct Candidate {
  int type = -1;
  int member = -1;
  // A type name the candidate uses for an "@Key" type that several model types share, when the model
  // does not say which of them it means. Such a candidate cannot be accepted.
  std::string unsure;
  // Why a fact could not judge the candidate: the code it needs could not be fully read. Such a
  // candidate cannot be accepted either.
  std::string unread;
  // A method's same behaviour: the owner's other methods of its signature at its own RVA (aliases)
  // or with a body that reads the same (twins).
  std::vector<int> same;
  // A field of a struct embedded in a class (an entry with `within`): the class, and where the field
  // lies in it. Facts read the class's accesses at that offset. -1 for any other candidate.
  int outer = -1;
  int32_t outerOffset = 0;
};

// A value type's instance fields are reported with the 0x10 object header a boxed copy has; embedded
// in a class, the struct's data starts at the class field's own offset.
constexpr int32_t kBoxedHeader = 0x10;

class Resolver {
 public:
  Resolver(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image,
           const IcallLookup& icalls, const CodeImage* unityPlayer)
      : entries_(entries), model_(model), image_(image), icalls_(icalls), unityPlayer_(unityPlayer) {
    out_.results.resize(entries.size());
    for (size_t i = 0; i < entries.size(); ++i) {
      out_.results[i].desc = entries[i];
      index_.try_emplace(entries[i]->key, i);
    }
    for (const ModelType& type : model.types) ++typesNamed_[type.name];
  }

  Resolution run() && {
    order_.assign(entries_.size(), -1);
    low_.assign(entries_.size(), 0);
    onStack_.assign(entries_.size(), false);
    for (size_t i = 0; i < entries_.size(); ++i)
      if (order_[i] < 0) visit(i);
    return std::move(out_);
  }

 private:
  // Tarjan's strongly connected components. A component is complete only after every entry it
  // depends on, so components are resolved in dependency order whatever order the entries came in.
  void visit(size_t i) {
    order_[i] = low_[i] = next_++;
    stack_.push_back(i);
    onStack_[i] = true;
    bool selfLoop = false;
    for_each_dependency(*entries_[i], [&](std::string_view key) {
      const auto it = index_.find(key);
      if (it == index_.end()) return;
      const size_t j = it->second;
      if (j == i) selfLoop = true;
      if (order_[j] < 0) {
        visit(j);
        low_[i] = std::min(low_[i], low_[j]);
      } else if (onStack_[j]) {
        low_[i] = std::min(low_[i], order_[j]);
      }
    });
    if (low_[i] != order_[i]) return;

    std::vector<size_t> component;
    size_t j;
    do {
      j = stack_.back();
      stack_.pop_back();
      onStack_[j] = false;
      component.push_back(j);
    } while (j != i);
    if (component.size() > 1 || selfLoop) {
      for (const size_t k : component) missing(out_.results[k], "cycle");
    } else {
      resolve_one(i);
    }
  }

  void resolve_one(size_t i) {
    const EntryDesc& d = *entries_[i];
    Result& r = out_.results[i];
    if (index_.at(d.key) != i) return missing(r, "duplicate key");
    if (blocked(d, r)) return;
    if (d.kind == Kind::Icall || d.kind == Kind::Export) return resolve_by_name_outside_metadata(d, r);
    if (d.kind == Kind::Scene || d.kind == Kind::Layer || d.kind == Kind::AnimatorParam ||
        d.kind == Kind::ShaderProperty)
      return check_scene_fact(d, r);
    if (d.kind == Kind::NativeSite) return resolve_native_site(d, r);
    if (d.kind == Kind::Inflated) return resolve_inflated(d, r);
    if (d.kind == Kind::Code || !d.facts.empty()) {
      if (!image_) return missing(r, "no code image");
      if (d.kind == Kind::Code) return resolve_code(d, r);
    }
    for (const Fact& fact : d.facts) {
      const bool aboutField = fact.is != Fact::Is::Calls && fact.is != Fact::Is::CalledBy &&
                              fact.is != Fact::Is::Called && fact.is != Fact::Is::Compares &&
                              fact.is != Fact::Is::MovesConst;
      if (fact.is == Fact::Is::MovesConst) {
        if (d.kind != Kind::Method) return missing(r, "a MovesConst fact applies to a method");
        if (fact.method || fact.negate) return missing(r, "a MovesConst fact names only a number");
        continue;
      }
      if (fact.is == Fact::Is::Compares) {
        if (!fact.method || entries_[index_.at(fact.method)]->kind != Kind::EnumValue)
          return missing(r, "a Compares fact names an enum value entry");
        if (d.kind != Kind::Method) return missing(r, "a Compares fact applies to a method");
        // "Does not compare" cannot fail closed: the reader sees only cmp with an immediate, not a test
        // against 0, a compare of two registers, a sub or dec before a jump, or a jump table.
        if (fact.negate) return missing(r, "negated Compares facts are not supported");
        continue;
      }
      if (fact.is == Fact::Is::IsNot) {
        if (d.kind != Kind::Field) return missing(r, "an IsNot fact applies to a field");
        const EntryDesc* other = fact.method ? entries_[index_.at(fact.method)] : nullptr;
        if (!other || other->kind != Kind::Field || !other->owner || !d.owner ||
            std::string_view(other->owner) != d.owner)
          return missing(r, "an IsNot fact names a field entry of the same owner");
        if (fact.negate) return missing(r, "a negated IsNot fact is not supported");
        continue;
      }
      if (fact.is == Fact::Is::Called) {
        if (fact.method) return missing(r, "a Called fact names no method");
        if (d.kind != Kind::Method) return missing(r, "a Called fact applies to a method");
        if (fact.negate) return missing(r, "negated call facts are not supported yet");
        continue;
      }
      if (!fact.method) return missing(r, "a fact needs a method");
      if (entries_[index_.at(fact.method)]->kind != Kind::Method)
        return missing(r, std::format("the fact's {} is not a method entry", fact.method));
      if (d.kind != (aboutField ? Kind::Field : Kind::Method))
        return missing(r, std::format("a {} fact applies to a {}", fact_name(fact.is),
                                      aboutField ? "field" : "method"));
      // "Does not call" cannot fail closed yet: the call targets miss indirect and virtual calls and
      // conditional tail jumps, and a call to one of the candidate's own twins is not counted.
      if (!aboutField && fact.negate) return missing(r, "negated call facts are not supported yet");
    }

    if (d.owner && entries_[index_.at(d.owner)]->kind != Kind::Type)
      return missing(r, std::format("owner {} is not a type entry", d.owner));
    if (!d.owner && d.kind != Kind::Type) return missing(r, "a member needs an owner");
    const int owner = d.owner ? result_of(d.owner).type : -1;
    if (d.kind == Kind::EnumValue && (owner < 0 || !model_.types[owner].isEnum))
      return missing(r, std::format("owner {} is not an enum", d.owner ? d.owner : "(none)"));
    const bool declared = d.kind == Kind::Type && d.within;  // the one interface another type declares
    if (declared && entries_[index_.at(d.within)]->kind != Kind::Type)
      return missing(r, std::format("a type's within names a type entry, and {} is not one", d.within));
    if (!d.name) {
      if (d.kind == Kind::EnumValue) return missing(r, "an enum value is found by name only");
      if (d.kind == Kind::Type && d.values.empty() && d.fieldTypes.empty() && !d.type && !declared)
        return missing(r, "a type found by shape needs values, field types, a static method's shape or a "
                          "type that declares it");
    }
    if ((d.flags & kExactFields) && (d.kind != Kind::Type || d.name || d.fieldTypes.empty() || !d.values.empty()))
      return missing(r, "an exact field list applies to a type found by its field types");
    if ((d.flags & kFieldOrder) && !(d.flags & kExactFields))
      return missing(r, "a field order applies to an exact field list");
    // The dump gives a top-level type's arity only (by its file's name), so a nested type's is not known.
    if ((d.flags & kNotGeneric) && (d.kind != Kind::Type || d.owner))
      return missing(r, "kNotGeneric applies to a top-level type entry");
    if (!d.name) {
      if (d.kind != Kind::Type && !d.type)
        return missing(r, std::format("a {} found by shape needs its type", kind_name(d.kind)));
    }

    // A field within an embedded struct is a member of the struct the `within` field holds, placed in
    // the owner at that field's offset.
    int members = owner;
    int32_t holderOffset = 0;
    if (d.kind == Kind::Field && d.within) {
      std::string why;
      members = embedded_struct(d, owner, holderOffset, why);
      if (members < 0) return missing(r, why);
    }
    // sameFieldAs names another field within a struct: this one is that very struct member.
    const Result* same = nullptr;
    if (d.sameFieldAs) {
      const EntryDesc& t = *entries_[index_.at(d.sameFieldAs)];
      same = &result_of(d.sameFieldAs);
      if (members == owner || t.kind != Kind::Field || !t.within || same->inner < 0)
        return missing(r, std::format("sameFieldAs needs this entry and {} to be fields within a struct",
                                      d.sameFieldAs));
      if (same->innerType != members)
        return missing(r, std::format("{} is a field of another struct type than {}", d.sameFieldAs,
                                      describe_type(members)));
    }

    // The pipeline: candidates, then every check the entry declares, then unique or report. The hint
    // takes no part: every candidate is checked, and it only names what changed.
    std::vector<Candidate> candidates;
    if (declared) {
      std::string why;
      if (!declared_interfaces(result_of(d.within).type, candidates, why)) return missing(r, why);
      if (d.name) std::erase_if(candidates, [&](const Candidate& c) { return model_.types[c.type].name != d.name; });
    } else {
      candidates = d.name ? by_name(d, members) : by_shape(d, members);
    }
    if (members != owner)
      for (Candidate& c : candidates) {
        c.outer = owner;
        c.outerOffset = holderOffset + model_.types[members].fields[c.member].offset - kBoxedHeader;
      }
    const size_t named = candidates.size();
    // A method named without its parameters cannot tell overloads apart, and no other check may
    // choose among them for it.
    if (d.kind == Kind::Method && d.name && d.params.empty() && named > 1)
      return decide(d, r, candidates, named, owner);
    std::erase_if(candidates, [&](Candidate& c) { return !fits(d, c); });
    if (same) std::erase_if(candidates, [&](const Candidate& c) { return c.member != same->inner; });
    for (const Fact& fact : d.facts) std::erase_if(candidates, [&](Candidate& c) { return !keeps(fact, c); });
    if (d.kind == Kind::Method && image_) same_behaviour(candidates);
    decide(d, r, candidates, named, owner);
    if (r.status == Status::Resolved && d.hint && r.currentName != d.hint)
      r.detail = std::format("renamed from {}", d.hint);
  }

  // The struct a `within` field entry's members come from: the type of the owner's instance field that
  // `within` names, which must be a struct (not an enum). -1, with why, when it is anything else.
  int embedded_struct(const EntryDesc& d, int owner, int32_t& holderOffset, std::string& why) const {
    const EntryDesc& w = *entries_[index_.at(d.within)];
    if ((d.flags & kStatic) || w.kind != Kind::Field || (w.flags & kStatic) || w.within) {
      why = std::format("{} is not an instance field entry holding a struct", d.within);
      return -1;
    }
    const Result& holder = result_of(d.within);
    if (holder.type != owner || holder.member < 0) {
      why = std::format("{} is not a field of {}", d.within, d.owner);
      return -1;
    }
    const ModelField& field = model_.types[holder.type].fields[holder.member];
    const int t = field.typeIndex;
    if (t < 0 || !model_.types[t].isValueType || model_.types[t].isEnum) {
      why = std::format("{} is not of a struct type the model knows", d.within);
      return -1;
    }
    holderOffset = field.offset;
    return t;
  }

  // An entry depends on others: it is Blocked unless each of them resolved.
  bool blocked(const EntryDesc& d, Result& r) {
    std::string why;
    for_each_dependency(d, [&](std::string_view key) {
      if (!why.empty()) return;
      const auto it = index_.find(key);
      if (it == index_.end())
        why = std::format("depends on {}, which is not in the map", key);
      else if (out_.results[it->second].status != Status::Resolved)
        why = std::format("depends on {}, which did not resolve", key);
    });
    if (why.empty()) return false;
    r.status = Status::Blocked;
    r.detail = std::move(why);
    return true;
  }

  // Candidates by real name: types by name, or the owner's members by name.
  std::vector<Candidate> by_name(const EntryDesc& d, int owner) const {
    std::vector<Candidate> out;
    if (d.kind == Kind::Type) {
      for (size_t t = 0; t < model_.types.size(); ++t)
        if (model_.types[t].name == d.name) out.push_back({static_cast<int>(t), -1});
      return out;
    }
    const ModelType& type = model_.types[owner];
    if (d.kind == Kind::Method || d.kind == Kind::Inflated) {
      for (size_t m = 0; m < type.methods.size(); ++m)
        if (type.methods[m].name == d.name) out.push_back({owner, static_cast<int>(m)});
    } else {
      for (size_t f = 0; f < type.fields.size(); ++f)
        if (type.fields[f].name == d.name) out.push_back({owner, static_cast<int>(f)});
    }
    return out;
  }

  // Candidates by shape: every type the compiler did not generate, or every member of the owner of
  // the entry's kind. The checks in fits then keep those of the declared shape.
  std::vector<Candidate> by_shape(const EntryDesc& d, int owner) const {
    std::vector<Candidate> out;
    if (d.kind == Kind::Type) {
      const bool generated = (d.flags & kCompilerGenerated) != 0;
      for (size_t t = 0; t < model_.types.size(); ++t)
        if (compiler_generated(model_.types[t]) == generated) out.push_back({static_cast<int>(t), -1});
      return out;
    }
    const ModelType& type = model_.types[owner];
    const size_t count = d.kind == Kind::Method ? type.methods.size() : type.fields.size();
    for (size_t m = 0; m < count; ++m) out.push_back({owner, static_cast<int>(m)});
    return out;
  }

  // Every model type of a short name, in model order; empty when none has it.
  const std::vector<int>& types_named(const std::string& name) const {
    if (typesByName_.empty())
      for (size_t i = 0; i < model_.types.size(); ++i) typesByName_[model_.types[i].name].push_back(static_cast<int>(i));
    static const std::vector<int> kNone;
    const auto it = typesByName_.find(name);
    return it == typesByName_.end() ? kNone : it->second;
  }

  // The candidates for a type `within` names: the interfaces among the bases model type t declares. A
  // base that names no model type could be an interface the model lacks, so it decides nothing and the
  // entry fails (false, with why). A base name several model types share marks its interfaces unsure.
  bool declared_interfaces(int t, std::vector<Candidate>& out, std::string& why) const {
    for (const std::string& name : model_.types[t].bases) {
      const std::vector<int>& named = types_named(name);
      if (named.empty()) {
        why = std::format("{} declares {}, which is no type of the model, so its interfaces are not known",
                          describe_type(t), name);
        return false;
      }
      for (const int i : named)
        if (model_.types[i].isInterface) out.push_back({i, -1, named.size() > 1 ? name : std::string()});
    }
    return true;
  }

  // An Inflated entry: the generic definition's method, found as a Method entry would be but for its code,
  // and each type argument the one model type it names. Offline, and in the host's first pass, it is then
  // Missing with kCheckedAtStartup, its type arguments kept: the host makes the instance in the running
  // game on the main thread (host/game_map.h, resolve_inflated).
  void resolve_inflated(const EntryDesc& d, Result& r) {
    if (!d.owner || entries_[index_.at(d.owner)]->kind != Kind::Type)
      return missing(r, "an inflated method's owner is its generic definition's type entry");
    if (!d.name) return missing(r, "an inflated method is found by its definition's name");
    if (d.typeArgs.empty()) return missing(r, "an inflated method needs its type arguments");
    if (!d.facts.empty() || d.pattern || d.within || !d.fieldTypes.empty() || !d.values.empty() || d.sameFieldAs)
      return missing(r, "an inflated method is found by its name and signature alone");
    std::vector<int> args;
    for (const char* arg : d.typeArgs) {
      const auto ref = entry_ref(arg);
      if (ref && ref->suffix.empty()) {
        const auto it = index_.find(ref->key);
        if (entries_[it->second]->kind != Kind::Type)
          return missing(r, std::format("the type argument {} is not a type entry", arg));
        args.push_back(out_.results[it->second].type);
        continue;
      }
      const std::string name = canonical_type(arg);
      if (std::strchr(arg, '@') || name.find_first_of("<>[]&*,") != std::string::npos)
        return missing(r, std::format("the type argument {} names no single type", arg));
      const std::vector<int>& named = types_named(name);
      if (named.empty()) return missing(r, std::format("no model type is named {}", name));
      if (named.size() > 1) {
        r.status = Status::Ambiguous;
        r.detail = std::format("{} model types are named {}, the type argument", named.size(), name);
        for (const int t : named) r.candidates.push_back(describe(Kind::Type, {t, -1}));
        return;
      }
      args.push_back(named.front());
    }
    const int owner = result_of(d.owner).type;
    std::vector<Candidate> candidates = by_name(d, owner);
    const size_t named = candidates.size();
    if (!(d.params.empty() && named > 1)) std::erase_if(candidates, [&](Candidate& c) { return !fits(d, c); });
    decide(d, r, candidates, named, owner);
    if (r.status != Status::Resolved) return;
    r.typeArgs = std::move(args);
    missing(r, std::string(kCheckedAtStartup));
  }

  // A coroutine or closure class. The obfuscator renames these but keeps their fields' names, such as
  // "<>1__state" and "<>4__this", so either one marks them.
  static bool compiler_generated(const ModelType& type) {
    return type.name.starts_with('<') || std::any_of(type.fields.begin(), type.fields.end(), [](const ModelField& f) {
             return f.name.starts_with("<>");
           });
  }

  // Whether a candidate passes the checks the entry declares.
  bool fits(const EntryDesc& d, Candidate& c) const {
    const ModelType& type = model_.types[c.type];
    const bool wantStatic = (d.flags & kStatic) != 0;
    switch (d.kind) {
      case Kind::Type: {
        if (d.assembly && type.assembly != d.assembly) return false;
        if ((d.flags & kNotGeneric) && type.genericParams != 0) return false;
        // A null namespace means the global one for a top-level type, and any for a nested type.
        if (d.ns ? type.ns != d.ns : (!d.owner && !type.ns.empty())) return false;
        const int declaring = d.owner ? result_of(d.owner).type : -1;
        if (type.declaring != declaring) return false;
        // Values make it an enum; a type found by shape without them is not one.
        if (!d.values.empty() ? !type.isEnum : (!d.name && type.isEnum)) return false;
        for (const char* value : d.values)
          if (std::none_of(type.fields.begin(), type.fields.end(),
                           [&](const ModelField& f) { return f.hasLiteral && f.name == value; }))
            return false;
        if (!has_field_types(d.fieldTypes, type, c)) return false;
        // A type (a static class, which has no fields to go by) found by a static method it declares.
        if (d.type && std::none_of(type.methods.begin(), type.methods.end(), [&](const ModelMethod& m) {
              if (!m.isStatic || m.params.size() != d.params.size() || !type_matches(d.type, m.ret, m.retIndex, c))
                return false;
              for (size_t p = 0; p < d.params.size(); ++p)
                if (!type_matches(d.params[p], m.params[p], p < m.paramIndex.size() ? m.paramIndex[p] : -1, c))
                  return false;
              return true;
            }))
          return false;
        // Exact: every instance field the type declares is one of those listed. The models list a type's
        // declared fields only, statics and constants marked, so the count decides.
        if (!(d.flags & kExactFields)) return true;
        std::vector<const ModelField*> instance;
        for (const ModelField& f : type.fields)
          if (!f.isStatic && !f.hasLiteral) instance.push_back(&f);
        if (instance.size() != d.fieldTypes.size()) return false;
        if (!(d.flags & kFieldOrder)) return true;
        // Ordered: by offset, the fields are of the listed types in the listed order.
        std::stable_sort(instance.begin(), instance.end(),
                         [](const ModelField* a, const ModelField* b) { return a->offset < b->offset; });
        for (size_t i = 0; i < instance.size(); ++i) {
          Candidate trial = c;
          if (!type_matches(d.fieldTypes[i], instance[i]->type, instance[i]->typeIndex, trial)) return false;
        }
        return true;
      }
      case Kind::Method:
      case Kind::Inflated: {
        const ModelMethod& m = type.methods[c.member];
        if (m.isStatic != wantStatic) return false;
        if (d.type && !type_matches(d.type, m.ret, m.retIndex, c)) return false;
        // No params: matched by name alone, any arity. Found by shape, it takes none.
        if (d.name && d.params.empty()) return true;
        if (d.params.size() != m.params.size()) return false;
        for (size_t p = 0; p < d.params.size(); ++p) {
          const int index = p < m.paramIndex.size() ? m.paramIndex[p] : -1;
          if (!type_matches(d.params[p], m.params[p], index, c)) return false;
        }
        return true;
      }
      case Kind::Field: {
        const ModelField& f = type.fields[c.member];
        return !f.hasLiteral && f.isStatic == wantStatic && (d.access == Visibility::Any || f.access == d.access) &&
               (!d.type || type_matches(d.type, f.type, f.typeIndex, c));
      }
      case Kind::EnumValue:
        return type.fields[c.member].hasLiteral;
      case Kind::Code:
      case Kind::Icall:
      case Kind::Export:
      case Kind::Scene:
      case Kind::Layer:
      case Kind::AnimatorParam:
      case Kind::NativeSite:
      case Kind::ShaderProperty:
        break;
    }
    return false;
  }

  // Whether the type has an instance field of each listed type, a field of its own for each. A type
  // naming an "@Key" matches fewer fields than a plain one of the same name, so those are placed first.
  bool has_field_types(std::span<const char* const> wants, const ModelType& type, Candidate& c) const {
    std::vector<const char*> order(wants.begin(), wants.end());
    std::stable_partition(order.begin(), order.end(), [](const char* want) { return std::strchr(want, '@'); });
    std::vector<bool> used(type.fields.size(), false);
    for (const char* want : order) {
      // A field the model says is that type comes before one only its name says may be.
      bool found = false;
      for (const bool allowUnsure : {false, true}) {
        for (size_t f = 0; f < type.fields.size() && !found; ++f) {
          const ModelField& field = type.fields[f];
          if (used[f] || field.isStatic || field.hasLiteral) continue;
          Candidate trial = c;
          if (!type_matches(want, field.type, field.typeIndex, trial)) continue;
          if (!allowUnsure && trial.unsure != c.unsure) continue;
          c = std::move(trial);
          found = used[f] = true;
        }
      }
      if (!found) return false;
    }
    return true;
  }

  // How the entry looks for what it names: "named X", or its shape.
  std::string wanted(const EntryDesc& d) const {
    if (d.kind == Kind::Type && d.within)
      return d.name ? std::format("named {} declared by {}", d.name, d.within) : std::format("declared by {}", d.within);
    if (d.name) return std::format("named {}", d.name);
    const char* scope = (d.flags & kStatic) ? "static " : "";
    switch (d.kind) {
      case Kind::Method: {
        std::string params;
        for (const char* p : d.params) params += (params.empty() ? "" : ",") + std::string(p);
        return std::format("of shape {}{}({})", scope, d.type, params);
      }
      case Kind::Field:
        return std::format("of shape {}{}", scope, d.type);
      default:
        return "of that shape";
    }
  }

  void decide(const EntryDesc& d, Result& r, const std::vector<Candidate>& candidates, size_t named,
              int owner) const {
    const std::string where = owner >= 0 ? " in " + describe_type(owner) : "";
    const std::string what = wanted(d);
    if (candidates.empty()) {
      if (named == 0 || !d.name) return missing(r, std::format("no {} {}{}", kind_name(d.kind), what, where));
      return missing(r, std::format("no {} {}{} matches ({} by name)", kind_name(d.kind), what, where, named));
    }
    const auto unread = std::find_if(candidates.begin(), candidates.end(),
                                     [](const Candidate& c) { return !c.unread.empty(); });
    if (candidates.size() > 1 || unread != candidates.end()) {
      r.status = Status::Ambiguous;
      r.detail = std::format("{} {}{} {}{} match", candidates.size(), kind_name(d.kind),
                             candidates.size() > 1 ? "s" : "", what, where);
      if (unread != candidates.end()) r.detail += ", and " + unread->unread;
      for (const Candidate& c : candidates) r.candidates.push_back(describe(d.kind, c));
      return;
    }

    const Candidate& c = candidates.front();
    if (!c.unsure.empty()) {
      r.status = Status::Ambiguous;
      r.detail = std::format("{} types are named {}, and the model does not say which the {} {}{} uses",
                             typesNamed_.at(c.unsure), c.unsure, kind_name(d.kind), what, where);
      r.candidates.push_back(describe(d.kind, c));
      return;
    }
    const ModelType& type = model_.types[c.type];
    // rva 0 is no address: published, it would be the module's base, a call into its PE header.
    if (d.kind == Kind::Method && type.methods[c.member].rva == 0)
      return missing(r, "has no code of its own (abstract, extern or generic)");
    r.status = Status::Resolved;
    r.type = c.type;
    r.member = c.member;
    switch (d.kind) {
      case Kind::Type:
        r.currentName = type.name;
        break;
      case Kind::Method:
        r.rva = type.methods[c.member].rva;
        r.currentName = type.methods[c.member].name;
        for (const int m : c.same) {
          if (type.methods[m].rva == r.rva) r.aliases.push_back(type.methods[m].name);
          else r.twins.push_back(type.methods[m].rva);
        }
        std::sort(r.twins.begin(), r.twins.end());
        r.twins.erase(std::unique(r.twins.begin(), r.twins.end()), r.twins.end());
        break;
      case Kind::Inflated:  // the definition's method; the instance's code is the running game's
        r.currentName = type.methods[c.member].name;
        break;
      case Kind::Field:
        r.offset = type.fields[c.member].offset;
        r.currentName = type.fields[c.member].name;
        // A struct's own instance field is published at its place in the struct, as a method that
        // receives the struct by address (a PhotonMessageInfo, say) reads it: without the boxed header.
        if (type.isValueType && c.outer < 0 && !type.fields[c.member].isStatic) r.offset -= kBoxedHeader;
        // A field within an embedded struct is published as a field of the class at its place there;
        // the struct's own field has no FieldInfo of the class's, so no member is given.
        if (c.outer >= 0) {
          r.innerType = c.type;
          r.inner = c.member;
          r.type = c.outer;
          r.member = -1;
          r.offset = c.outerOffset;
        }
        break;
      case Kind::EnumValue:
        r.value = type.fields[c.member].literal;
        r.currentName = type.fields[c.member].name;
        break;
      case Kind::Code:
      case Kind::Icall:
      case Kind::Export:
      case Kind::Scene:
      case Kind::Layer:
      case Kind::AnimatorParam:
      case Kind::NativeSite:
      case Kind::ShaderProperty:
        break;
    }
  }

  // The facts of the function that starts at rva, read once. Null when no function starts there.
  const CodeFacts* code_at(uint64_t rva) const {
    if (rva == 0 || image_->function_start(rva) != rva) return nullptr;
    auto it = code_.find(rva);
    if (it == code_.end()) it = code_.emplace(rva, read_code(image_->function(rva), rva)).first;
    return &it->second;
  }

  // The facts of a leaf function at rva (read_leaf), read once. Null when a listed function holds rva.
  const CodeFacts* leaf_at(uint64_t rva) const {
    const std::span<const uint8_t> bytes = image_->leaf(rva);
    if (bytes.empty()) return nullptr;
    auto it = leaves_.find(rva);
    if (it == leaves_.end()) it = leaves_.emplace(rva, read_leaf(bytes, rva)).first;
    return &it->second;
  }

  // Whether the function at rva was read in full, so that facts about it can decide.
  bool readable(uint64_t rva) const {
    const CodeFacts* code = code_at(rva);
    return code && code->complete;
  }

  // A resolved method's bodies: its own, then its twins'.
  static std::vector<uint64_t> bodies(const Result& method) {
    std::vector<uint64_t> out{method.rva};
    out.insert(out.end(), method.twins.begin(), method.twins.end());
    return out;
  }

  // Whether a fact keeps the candidate. A fact that cannot judge it keeps it, marked so that it is
  // never accepted.
  bool keeps(const Fact& fact, Candidate& c) const {
    std::string why;
    const Verdict v = judge(fact, c, why);
    if (v == Verdict::Unread) {
      if (c.unread.empty()) c.unread = std::move(why);
      return true;
    }
    return (v == Verdict::Yes) != fact.negate;
  }

  // What a fact says of a candidate, before negate; why says what kept it from judging. The field
  // facts read the fact method's accesses, Calls the candidate's calls, and CalledBy the fact
  // method's calls. The fact method counts with its twins.
  //
  // A field fact cannot tell which object an access reaches unless its base register holds this.
  // Where rcx came in as the owner's this, a fact counts only an access through this, either way:
  // a hit through any other register leaves the candidate undecided, since after an update another
  // object's field at the offset could make a lone wrong match. Elsewhere (a static method, or a
  // method of another type) no register is known to hold the owner, so a positive fact counts every
  // access, which makes it weak, and a negated one decides nothing.
  Verdict judge(const Fact& fact, const Candidate& c, std::string& why) const {
    const ModelType& type = model_.types[c.type];
    const auto unreadable = [&](const std::string& whose) {
      why = std::format("the code of {} could not be fully read for its {} fact", whose, fact_name(fact.is));
      return Verdict::Unread;
    };
    // Called: a call or jump to the candidate in any other method of its type. One found decides;
    // none found decides only when every other method of the type was read in full.
    if (fact.is == Fact::Is::Called) {
      const uint64_t rva = type.methods[c.member].rva;
      std::string unread;
      for (size_t m = 0; m < type.methods.size(); ++m) {
        const uint64_t other = type.methods[m].rva;
        if (static_cast<int>(m) == c.member || other == 0 || other == rva) continue;
        const CodeFacts* code = readable(other) ? code_at(other) : leaf_at(other);
        if (!code || !code->complete) {
          if (unread.empty()) unread = type.methods[m].name;
          continue;
        }
        const std::vector<uint64_t>& targets = code->callTargets;
        if (std::find(targets.begin(), targets.end(), rva) != targets.end()) return Verdict::Yes;
      }
      return unread.empty() ? Verdict::No : unreadable(unread);
    }
    // IsNot: the candidate is not the field the other entry resolved to. It reads no code. An entry that
    // did not resolve blocks this one before any fact runs, as every entry a fact names does.
    if (fact.is == Fact::Is::IsNot) {
      const Result& other = result_of(fact.method);
      return other.type == c.type && other.member == c.member ? Verdict::No : Verdict::Yes;
    }
    // Compares: the candidate's own code, read in full, holds a cmp with the enum value's number.
    // A leaf the exception table does not list is read for its calls only, so it cannot decide.
    if (fact.is == Fact::Is::Compares) {
      const ModelMethod& self = type.methods[c.member];
      if (self.rva == 0 || !readable(self.rva)) return unreadable(self.name);
      const CodeFacts* own = code_at(self.rva);
      if (!own || !own->complete) return unreadable(self.name);
      const int64_t value = result_of(fact.method).value;
      const auto& seen = own->compares;
      return std::any_of(seen.begin(), seen.end(), [&](const CodeFacts::Compare& c) { return c.is(value); })
                 ? Verdict::Yes
                 : Verdict::No;
    }
    // MovesConst: the candidate's own code, read in full, moves the number into a register or a place.
    if (fact.is == Fact::Is::MovesConst) {
      const ModelMethod& self = type.methods[c.member];
      if (self.rva == 0 || !readable(self.rva)) return unreadable(self.name);
      const CodeFacts* own = code_at(self.rva);
      if (!own || !own->complete) return unreadable(self.name);
      const auto& seen = own->moves;
      return std::find(seen.begin(), seen.end(), fact.value) != seen.end() ? Verdict::Yes : Verdict::No;
    }
    const Result& method = result_of(fact.method);
    bool yes = false;
    if (fact.is == Fact::Is::Calls) {
      // The candidate's own code: a function the exception table lists, or a leaf it does not (a
      // constructor's stub that tail-jumps to its base, say), read in full either way.
      const ModelMethod& self = type.methods[c.member];
      if (self.rva == 0) return unreadable(self.name);  // no code of its own to read
      const CodeFacts* own = readable(self.rva) ? code_at(self.rva) : leaf_at(self.rva);
      if (!own || !own->complete) return unreadable(self.name);
      const std::vector<uint64_t> targets = bodies(method);
      for (const uint64_t target : own->callTargets)
        yes = yes || std::find(targets.begin(), targets.end(), target) != targets.end();
      return yes ? Verdict::Yes : Verdict::No;
    }
    // A struct's own methods address its fields at the offset minus 0x10 (no object header), so a
    // field fact on a struct would compare the wrong displacement.
    // A field within an embedded struct is judged as a field of the class that embeds it.
    const bool embedded = c.outer >= 0;
    if (fact.is != Fact::Is::CalledBy && type.isValueType && !embedded) {
      why = std::format("field facts on a struct are not supported ({} is a struct)", describe_type(c.type));
      return Verdict::Unread;
    }
    // An access's this is the candidate's owner only when rcx came in as that owner's this.
    const int fieldOwner = embedded ? c.outer : c.type;
    const bool ownersThis = method.type == fieldOwner && rcx_is_this(method.type, method.member);
    bool throughThis = false;
    for (const uint64_t body : bodies(method)) {
      // A function the exception table lists, or a leaf it does not (a constructor that stores its
      // defaults and tail-jumps to its base, say), read in full either way.
      const CodeFacts* read = readable(body) ? code_at(body) : leaf_at(body);
      if (!read || !read->complete) return unreadable(fact.method);
      const CodeFacts& code = *read;
      if (fact.is == Fact::Is::CalledBy) {
        const uint64_t rva = type.methods[c.member].rva;
        yes = yes || std::find(code.callTargets.begin(), code.callTargets.end(), rva) != code.callTargets.end();
        continue;
      }
      const int32_t offset = embedded ? c.outerOffset : type.fields[c.member].offset;
      for (const Access& a : code.accesses) {
        if (a.global) continue;  // a runtime metadata slot's class, never an object of the owner
        // Through fields of this whose types prove the object is of another family than the owner's.
        if (!a.thisBase && ownersThis && other_object(a.chain, fieldOwner)) continue;
        const bool hit = a.disp == offset && (fact.is == Fact::Is::Stores        ? a.store
                                              : fact.is == Fact::Is::StoresConst ? a.hasImm && a.imm == fact.value
                                                                                 : !a.store);
        yes = yes || hit;
        throughThis = throughThis || (hit && a.thisBase && ownersThis);
      }
    }
    if (yes && !throughThis && (fact.negate || ownersThis) && fact.is != Fact::Is::CalledBy) {
      why = std::format("the {}{} fact matches {} only through a register not proven to hold this",
                        fact.negate ? "negated " : "", fact_name(fact.is), fact.method);
      return Verdict::Unread;
    }
    return yes ? Verdict::Yes : Verdict::No;
  }

  // Whether an access whose base came from these field loads off this (Access::chain) provably reaches
  // an object that cannot be of the owner's type. Each load must read an instance field the type
  // before it declares at that offset, of a known class (not a struct) type; and the last type and
  // the owner must be of unrelated families: neither among the other's possible ancestors, both of
  // which must be known. Anything less is no proof, and the access counts as before.
  bool other_object(const std::vector<int32_t>& chain, int owner) const {
    if (chain.empty()) return false;
    int t = owner;
    for (const int32_t disp : chain) {
      const ModelField* f = nullptr;
      for (const ModelField& x : model_.types[t].fields)
        if (!x.isStatic && !x.hasLiteral && x.offset == disp) f = &x;
      if (!f || f->typeIndex < 0 || model_.types[f->typeIndex].isValueType) return false;
      t = f->typeIndex;
    }
    // An interface may be implemented by a class that derives from the owner, though the owner does
    // not implement it: the ancestry of the two says nothing then.
    if (t == owner || model_.types[t].isInterface || model_.types[owner].isInterface) return false;
    const std::optional<std::set<int>>& ofLast = possible_ancestors(t);
    const std::optional<std::set<int>>& ofOwner = possible_ancestors(owner);
    return ofLast && ofOwner && !ofLast->contains(owner) && !ofOwner->contains(t);
  }

  // Every model type that may be an ancestor of type `t` (a base class or an interface, at any
  // depth): each name in a base list stands for every model type of that short name. Null when a name
  // names no model type, since its ancestors cannot be known.
  const std::optional<std::set<int>>& possible_ancestors(int t) const {
    if (const auto it = ancestors_.find(t); it != ancestors_.end()) return it->second;
    return ancestors_[t] = read_ancestors(t);
  }

  std::optional<std::set<int>> read_ancestors(int t) const {
    std::set<int> out;
    std::vector<int> todo{t};
    while (!todo.empty()) {
      const int x = todo.back();
      todo.pop_back();
      for (const std::string& name : model_.types[x].bases) {
        const std::vector<int>& named = types_named(name);
        if (named.empty()) return std::nullopt;
        for (const int c : named)
          if (out.insert(c).second) todo.push_back(c);
      }
    }
    return out;
  }

  // Whether the method gets `this` in rcx: an instance method, and its return value comes back in a
  // register rather than through a hidden buffer, which takes rcx and moves `this` to rdx. A struct's
  // this points at the value, not at an object, so rcx is never taken as the owner's this. The model
  // has no sizes, so only returns known to fit a register count: nothing, a primitive other than
  // decimal, an enum, a reference (string, object, an array, a pointer, a by-reference return).
  // Any other type may be a struct.
  bool rcx_is_this(int owner, int member) const {
    const ModelType& type = model_.types[owner];
    const ModelMethod& m = type.methods[member];
    if (m.isStatic || type.isValueType) return false;
    static constexpr std::string_view kInRegister[] = {
        "void", "bool", "byte", "sbyte", "char",  "short",  "ushort", "int",    "uint",
        "long", "ulong", "float", "double", "string", "object", "IntPtr", "UIntPtr"};
    const std::string_view ret = m.ret;
    if (std::find(std::begin(kInRegister), std::end(kInRegister), ret) != std::end(kInRegister)) return true;
    if (ret.ends_with("[]") || ret.ends_with('*') || ret.ends_with('&')) return true;
    return m.retIndex >= 0 && model_.types[m.retIndex].isEnum;
  }

  // Whether two methods are one behaviour: one body (an alias), or fully read bodies of one length
  // that read the same once placed (twins). Code not fully read is never a twin, not even byte for
  // byte: what the reader could not decode may name an address, and the same bytes at another
  // address name another target.
  bool one_behaviour(const ModelMethod& a, const ModelMethod& b) const {
    if (a.rva == 0 || b.rva == 0) return false;
    if (a.rva == b.rva) return true;
    if (!readable(a.rva) || !readable(b.rva)) return false;
    return image_->function(a.rva).size() == image_->function(b.rva).size() &&
           code_at(a.rva)->normalized == code_at(b.rva)->normalized;
  }

  // Method candidates that are all one behaviour count as one, and the lowest RVA is canonical. The
  // one left gathers every other method of the owner with its signature that is the same behaviour.
  void same_behaviour(std::vector<Candidate>& candidates) const {
    if (candidates.empty()) return;
    const ModelType& type = model_.types[candidates.front().type];
    const auto method = [&](const Candidate& c) -> const ModelMethod& { return type.methods[c.member]; };
    const auto canonical = std::min_element(candidates.begin(), candidates.end(),
                                            [&](const Candidate& a, const Candidate& b) {
                                              return std::pair(method(a).rva, a.member) <
                                                     std::pair(method(b).rva, b.member);
                                            });
    for (const Candidate& c : candidates)
      if (!one_behaviour(method(*canonical), method(c))) return;
    Candidate kept = std::move(*canonical);
    for (const Candidate& c : candidates) {
      if (kept.unread.empty()) kept.unread = c.unread;
      if (kept.unsure.empty()) kept.unsure = c.unsure;
    }
    const ModelMethod& self = method(kept);
    for (size_t m = 0; m < type.methods.size(); ++m) {
      const ModelMethod& other = type.methods[m];
      if (static_cast<int>(m) == kept.member || other.ret != self.ret || other.params != self.params ||
          other.isStatic != self.isStatic)
        continue;
      if (one_behaviour(self, other)) kept.same.push_back(static_cast<int>(m));
    }
    candidates.assign(1, std::move(kept));
  }

  // A code entry: the one place in the method `within` where the pattern matches.
  void resolve_code(const EntryDesc& d, Result& r) const {
    if (!d.pattern || !d.within) return missing(r, "a code entry needs a pattern and a method to search");
    if (entries_[index_.at(d.within)]->kind != Kind::Method)
      return missing(r, std::format("{} is not a method entry", d.within));
    const auto pattern = parse_pattern(d.pattern);
    if (!pattern) return missing(r, std::format("bad pattern \"{}\"", d.pattern));
    const Result& method = result_of(d.within);
    if (method.rva == 0 || image_->function_start(method.rva) != method.rva)
      return missing(r, std::format("no function starts at {} ({:#x})", d.within, method.rva));
    const std::span<const uint8_t> fn = image_->function(method.rva);
    std::vector<uint64_t> matches;
    for (size_t at = 0; at + pattern->size() <= fn.size(); ++at) {
      size_t i = 0;
      while (i < pattern->size() && ((*pattern)[i] < 0 || (*pattern)[i] == fn[at + i])) ++i;
      if (i == pattern->size()) matches.push_back(method.rva + at);
    }
    if (matches.empty()) return missing(r, std::format("the pattern is not in {}", d.within));
    if (matches.size() > 1) {
      r.status = Status::Ambiguous;
      r.detail = std::format("the pattern is in {} {} times", d.within, matches.size());
      for (const uint64_t at : matches) r.candidates.push_back(std::format("{:#x}", at));
      return;
    }
    // The result must be an instruction of the method, as read in full: a place inside an
    // instruction or past the method's end names code the entry never meant.
    const uint64_t at = matches.front() + d.patternOffset;
    if (at < method.rva || at >= method.rva + fn.size())
      return missing(r, std::format("the pattern's offset {} puts it outside {}", d.patternOffset, d.within));
    if (!readable(method.rva))
      return missing(r, std::format("the code of {} could not be fully read, so the pattern's place is not known "
                                    "to start an instruction",
                                    d.within));
    const std::vector<uint64_t>& starts = code_at(method.rva)->starts;
    if (!std::binary_search(starts.begin(), starts.end(), at))
      return missing(r, std::format("the pattern's place {:#x} in {} is not where an instruction starts", at, d.within));
    r.status = Status::Resolved;
    r.type = method.type;
    r.rva = at;
  }

  // An icall, through the running runtime, or an export, from the image's export table: found by
  // its name alone, with no owner, shape or facts.
  void resolve_by_name_outside_metadata(const EntryDesc& d, Result& r) const {
    if (!d.name) return missing(r, std::format("an {} entry needs its name", kind_name(d.kind)));
    if (d.owner || !d.facts.empty() || d.pattern)
      return missing(r, std::format("an {} entry is found by its name alone", kind_name(d.kind)));
    if (d.kind == Kind::Icall) {
      if (!icalls_) return missing(r, std::string(kCheckedAtStartup));
      const void* code = icalls_(d.name);
      if (!code) return missing(r, std::format("the runtime has no icall {}", d.name));
      r.address = reinterpret_cast<uintptr_t>(code);
    } else {
      if (!image_) return missing(r, "no code image");
      r.rva = image_->exported(d.name);
      if (r.rva == 0) return missing(r, std::format("the image exports no {}", d.name));
    }
    r.status = Status::Resolved;
    r.currentName = d.name;
  }

  // A native site: the one place in UnityPlayer's code where the pattern matches, inside the function
  // of the export `within` names, or anywhere in its .text section. The resolver knows no instruction
  // bounds there, so the site must lie inside the bytes the pattern matched, and patch_site checks the
  // bytes it replaces before it writes. The scan stops at kMaxNativeMatches.
  void resolve_native_site(const EntryDesc& d, Result& r) const {
    if (d.owner || d.name || d.hint || d.ns || d.type || !d.params.empty() || !d.fieldTypes.empty() ||
        !d.values.empty() || !d.facts.empty() || d.scene || d.path || d.component || (d.flags & kStatic))
      return missing(r, "a native site is found by its pattern alone");
    const std::string_view assembly = d.assembly ? d.assembly : "";
    if (assembly != kUnityPlayer && assembly != kGameAssembly)
      return missing(r, "a native site's assembly must be UnityPlayer or GameAssembly");
    if (!d.pattern || !d.within) return missing(r, "a native site needs a pattern and where to search it");
    const auto pattern = parse_pattern(d.pattern);
    if (!pattern) return missing(r, std::format("bad pattern \"{}\"", d.pattern));
    if (d.patternOffset < 0 || static_cast<size_t>(d.patternOffset) >= pattern->size())
      return missing(r, std::format("the pattern's offset {} is outside the pattern", d.patternOffset));
    const std::string_view within = d.within;
    const bool text = within == kTextSection;
    const auto fixed = std::count_if(pattern->begin(), pattern->end(), [](int b) { return b >= 0; });
    if (text && static_cast<size_t>(fixed) < kMinTextPattern)
      return missing(r, std::format("a pattern searched in .text needs at least {} bytes that are not wildcards",
                                    kMinTextPattern));
    const CodeImage* image = assembly == kUnityPlayer ? unityPlayer_ : image_;
    if (!image) return missing(r, std::format("no {} image", assembly));
    uint64_t start = 0;
    std::span<const uint8_t> code;
    if (text) {
      code = image->section(kTextSection, start);
      if (code.empty()) return missing(r, std::format("{} has no single .text section", assembly));
    } else {
      start = image->exported(within);
      if (start == 0) return missing(r, std::format("{} exports no {}", assembly, within));
      if (image->function_start(start) != start)
        return missing(r, std::format("no function starts at the export {} ({:#x})", within, start));
      code = image->function(start);
    }
    std::vector<uint64_t> matches;
    for (size_t at = 0; at + pattern->size() <= code.size() && matches.size() < kMaxNativeMatches; ++at) {
      size_t i = 0;
      while (i < pattern->size() && ((*pattern)[i] < 0 || (*pattern)[i] == code[at + i])) ++i;
      if (i == pattern->size()) matches.push_back(start + at);
    }
    if (matches.empty()) return missing(r, std::format("the pattern is not in {}", within));
    if (matches.size() > 1) {
      r.status = Status::Ambiguous;
      r.detail = matches.size() < kMaxNativeMatches
                     ? std::format("the pattern is in {} {} times", within, matches.size())
                     : std::format("the pattern is in {} at least {} times", within, matches.size());
      for (const uint64_t at : matches) r.candidates.push_back(std::format("{:#x}", at));
      return;
    }
    r.status = Status::Resolved;
    r.rva = matches.front() + d.patternOffset;
  }

  // A Scene, Layer, AnimatorParam or ShaderProperty entry: a fact of the scene and asset files, which
  // the resolver only checks for being well formed. A Scene entry names where it is (scene), the
  // component its lookup starts from and its path; a Layer entry its name; an AnimatorParam entry its
  // name, where it is and its controller (component); a ShaderProperty entry its name, where it is and
  // its shader (component).
  static void check_scene_fact(const EntryDesc& d, Result& r) {
    if (d.owner || d.hint || d.assembly || d.ns || d.type || !d.params.empty() || !d.fieldTypes.empty() ||
        !d.values.empty() || !d.facts.empty() || d.pattern || d.within || (d.flags & kStatic))
      return missing(r, std::format("a {} entry has no owner, shape, facts or pattern", kind_name(d.kind)));
    const bool scene = d.kind == Kind::Scene;
    const bool layer = d.kind == Kind::Layer;
    const bool wellFormed = scene   ? d.scene && d.path && d.component && !d.name
                            : layer ? d.name && !d.scene && !d.path && !d.component
                                    : d.name && d.scene && d.component && !d.path;
    if (!wellFormed) {
      return missing(r, scene   ? "a scene entry needs its scene, path and component, and no name"
                        : layer ? "a layer entry needs its name alone"
                        : d.kind == Kind::ShaderProperty
                            ? "a shader property entry needs its name, scene and component, and no path"
                            : "an animator parameter entry needs its name, scene and component, and no path");
    }
    for (const char* text : {d.scene, d.path, d.component, d.name})
      if (text && (!*text || std::string_view(text).find_first_of("\t\r\n") != std::string_view::npos))
        return missing(r, std::format("a {} entry's names must not be empty or hold a tab or line break",
                                      kind_name(d.kind)));
    missing(r, std::string(kCheckedInScene));
    r.currentName = scene ? d.path : d.name;
  }

  // An entry's type string against a model type string and, when the model knows it, the index of
  // the type that string names. "@Key" means the very type entry Key resolved to: a known index must
  // be that type, and an unknown one is trusted by name only when no other model type shares the
  // name. Otherwise the candidate is marked unsure.
  bool type_matches(const char* want, const std::string& have, int haveIndex, Candidate& c) const {
    const auto ref = entry_ref(want);
    if (!ref) {
      if (std::strchr(want, '@')) return generic_matches(want, have, c);
      const std::string wanted = canonical_type(want);
      return wanted == have || (model_.objectArraysLookAlike && wanted == "object[]" && have == "Object[]");
    }
    const auto it = index_.find(ref->key);
    if (it == index_.end() || entries_[it->second]->kind != Kind::Type) return false;
    const Result& target = out_.results[it->second];
    if (target.status != Status::Resolved) return false;
    const std::string& name = model_.types[target.type].name;
    if (canonical_type(name + std::string(ref->suffix)) != have) return false;
    // The index names the type itself, by reference or not, so it is only known for those forms.
    if (haveIndex >= 0 && (ref->suffix.empty() || ref->suffix == "&")) return haveIndex == target.type;
    if (typesNamed_.at(name) > 1) c.unsure = name;
    return true;
  }

  // A type string with "@Key" inside a generic's arguments ("List<@Key>"): each one stands for the name
  // of the type its entry resolved to. The model gives no index for a generic's arguments, so such a
  // type is known only by name: a name several types share makes the candidate unsure.
  bool generic_matches(std::string_view want, const std::string& have, Candidate& c) const {
    std::string spelled;
    std::vector<std::string_view> names;
    for (size_t i = 0; i < want.size();) {
      if (want[i] != '@') {
        spelled += want[i++];
        continue;
      }
      const std::string_view rest = want.substr(i + 1);
      const std::string_view key = rest.substr(0, std::min(rest.find_first_of(kRefEnd), rest.size()));
      const auto it = index_.find(key);
      if (it == index_.end() || entries_[it->second]->kind != Kind::Type) return false;
      const Result& target = out_.results[it->second];
      if (target.status != Status::Resolved) return false;
      const std::string& name = model_.types[target.type].name;
      spelled += name;
      names.push_back(name);
      i += 1 + key.size();
    }
    if (canonical_type(spelled) != have) return false;
    for (const std::string_view name : names)
      if (typesNamed_.at(std::string(name)) > 1) c.unsure = std::string(name);
    return true;
  }

  // "UnityEngine.Physics" or "GhostAI/GhostState".
  std::string describe_type(int t) const {
    const ModelType& type = model_.types[t];
    if (type.declaring >= 0) return describe_type(type.declaring) + "/" + type.name;
    return type.ns.empty() ? type.name : type.ns + "." + type.name;
  }

  std::string describe(Kind kind, const Candidate& c) const {
    const ModelType& type = model_.types[c.type];
    switch (kind) {
      case Kind::Type:
        return std::format("{} ({})", describe_type(c.type), type.assembly);
      case Kind::Method:
      case Kind::Inflated: {
        const ModelMethod& m = type.methods[c.member];
        std::string params;
        for (const std::string& p : m.params) params += (params.empty() ? "" : ",") + p;
        return std::format("{}({})", m.name, params);
      }
      case Kind::Field:
        return std::format("{}: {} at {:#x}", type.fields[c.member].name, type.fields[c.member].type,
                           c.outer >= 0 ? c.outerOffset : type.fields[c.member].offset);
      case Kind::EnumValue:
        return std::format("{} = {}", type.fields[c.member].name, type.fields[c.member].literal);
      case Kind::Code:
      case Kind::Icall:
      case Kind::Export:
      case Kind::Scene:
      case Kind::Layer:
      case Kind::AnimatorParam:
      case Kind::NativeSite:
      case Kind::ShaderProperty:
        break;
    }
    return {};
  }

  const Result& result_of(const char* key) const { return out_.results[index_.at(key)]; }

  static void missing(Result& r, std::string detail) {
    r.status = Status::Missing;
    r.detail = std::move(detail);
  }

  std::span<const EntryDesc* const> entries_;
  const GameModel& model_;
  const CodeImage* image_;
  const IcallLookup& icalls_;
  const CodeImage* unityPlayer_;
  std::unordered_map<std::string_view, size_t> index_;  // the first entry with each key
  std::unordered_map<std::string, int> typesNamed_;     // how many model types have each name
  mutable std::unordered_map<uint64_t, CodeFacts> code_;  // read once per function start
  mutable std::unordered_map<uint64_t, CodeFacts> leaves_;  // read once per leaf function
  mutable std::map<std::string, std::vector<int>> typesByName_;  // every model type of a short name
  mutable std::map<int, std::optional<std::set<int>>> ancestors_;  // possible_ancestors, read once
  Resolution out_;
  std::vector<int> order_, low_;
  std::vector<bool> onStack_;
  std::vector<size_t> stack_;
  int next_ = 0;
};

}  // namespace

const Result* Resolution::find(std::string_view key) const {
  for (const Result& r : results)
    if (r.desc && r.desc->key && key == r.desc->key) return &r;
  return nullptr;
}

bool Resolution::required_resolved() const {
  return std::all_of(results.begin(), results.end(), [](const Result& r) {
    return (r.desc->flags & kOptional) || r.status == Status::Resolved;
  });
}

const char* status_word(Status status) {
  switch (status) {
    case Status::Resolved: return "OK";
    case Status::Missing: return "MISSING";
    case Status::Ambiguous: return "AMBIGUOUS";
    case Status::Blocked: return "BLOCKED";
  }
  return "?";
}

std::vector<std::string> assemblies_of(std::span<const EntryDesc* const> entries) {
  std::vector<std::string> out;
  for (const EntryDesc* e : entries)
    if (e->assembly && e->kind != Kind::NativeSite) out.emplace_back(e->assembly);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

std::vector<std::string> optional_assemblies_of(std::span<const EntryDesc* const> entries) {
  std::set<std::string> optional, required;
  for (const EntryDesc* e : entries) {
    if (!e->assembly || e->kind == Kind::NativeSite) continue;
    (e->kind == Kind::Type && (e->flags & kOptional) ? optional : required).insert(e->assembly);
  }
  std::vector<std::string> out;
  for (const std::string& a : optional)
    if (!required.contains(a)) out.push_back(a);
  return out;
}

std::vector<std::string> present_assemblies(std::span<const EntryDesc* const> entries,
                                            const std::function<bool(const std::string&)>& present) {
  const std::vector<std::string> optional = optional_assemblies_of(entries);
  std::vector<std::string> out;
  for (std::string& a : assemblies_of(entries))
    if (!std::binary_search(optional.begin(), optional.end(), a) || present(a)) out.push_back(std::move(a));
  return out;
}

Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image,
                   const IcallLookup& icalls, const CodeImage* unityPlayer) {
  return Resolver(entries, model, image, icalls, unityPlayer).run();
}

}  // namespace pm::sdk
