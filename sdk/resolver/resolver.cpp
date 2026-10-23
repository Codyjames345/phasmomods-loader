#include "resolver.h"

#include <algorithm>
#include <format>
#include <optional>
#include <unordered_map>

#include "code.h"
#include "image.h"
#include "types.h"

namespace pg::sdk {
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

// Calls f with the key of every entry d refers to.
template <typename F>
void for_each_dependency(const EntryDesc& d, F&& f) {
  if (d.owner) f(std::string_view(d.owner));
  if (const auto ref = entry_ref(d.type)) f(ref->key);
  for (const std::span<const char* const> list : {d.params, d.fieldTypes})
    for (const char* type : list)
      if (const auto ref = entry_ref(type)) f(ref->key);
  for (const Fact& fact : d.facts)
    if (fact.method) f(std::string_view(fact.method));
  if (d.within) f(std::string_view(d.within));
}

const char* kind_name(Kind kind) {
  switch (kind) {
    case Kind::Type: return "type";
    case Kind::Method: return "method";
    case Kind::Field: return "field";
    case Kind::EnumValue: return "enum value";
    case Kind::Code: return "code";
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
};

class Resolver {
 public:
  Resolver(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image)
      : entries_(entries), model_(model), image_(image) {
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
    if (d.kind == Kind::Code || !d.facts.empty()) {
      if (!image_) return missing(r, "no code image");
      if (d.kind == Kind::Code) return resolve_code(d, r);
    }
    for (const Fact& fact : d.facts) {
      if (!fact.method) return missing(r, "a fact needs a method");
      if (entries_[index_.at(fact.method)]->kind != Kind::Method)
        return missing(r, std::format("the fact's {} is not a method entry", fact.method));
      const bool aboutField = fact.is != Fact::Is::Calls && fact.is != Fact::Is::CalledBy;
      if (d.kind != (aboutField ? Kind::Field : Kind::Method))
        return missing(r, std::format("a {} fact applies to a {}", fact_name(fact.is),
                                      aboutField ? "field" : "method"));
    }

    if (d.owner && entries_[index_.at(d.owner)]->kind != Kind::Type)
      return missing(r, std::format("owner {} is not a type entry", d.owner));
    if (!d.owner && d.kind != Kind::Type) return missing(r, "a member needs an owner");
    const int owner = d.owner ? result_of(d.owner).type : -1;
    if (d.kind == Kind::EnumValue && (owner < 0 || !model_.types[owner].isEnum))
      return missing(r, std::format("owner {} is not an enum", d.owner ? d.owner : "(none)"));
    if (!d.name) {
      if (d.kind == Kind::EnumValue) return missing(r, "an enum value is found by name only");
      if (d.kind == Kind::Type && d.values.empty() && d.fieldTypes.empty())
        return missing(r, "a type found by shape needs values or field types");
      if (d.kind != Kind::Type && !d.type)
        return missing(r, std::format("a {} found by shape needs its type", kind_name(d.kind)));
    }

    // The pipeline: candidates, then every check the entry declares, then unique or report. The hint
    // takes no part: every candidate is checked, and it only names what changed.
    std::vector<Candidate> candidates = d.name ? by_name(d, owner) : by_shape(d, owner);
    const size_t named = candidates.size();
    // A method named without its parameters cannot tell overloads apart, and no other check may
    // choose among them for it.
    if (d.kind == Kind::Method && d.name && d.params.empty() && named > 1)
      return decide(d, r, candidates, named, owner);
    std::erase_if(candidates, [&](Candidate& c) { return !fits(d, c); });
    for (const Fact& fact : d.facts) std::erase_if(candidates, [&](Candidate& c) { return !keeps(fact, c); });
    if (d.kind == Kind::Method && image_) same_behaviour(candidates);
    decide(d, r, candidates, named, owner);
    if (r.status == Status::Resolved && d.hint && r.currentName != d.hint)
      r.detail = std::format("renamed from {}", d.hint);
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
    if (d.kind == Kind::Method) {
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
      for (size_t t = 0; t < model_.types.size(); ++t)
        if (!compiler_generated(model_.types[t])) out.push_back({static_cast<int>(t), -1});
      return out;
    }
    const ModelType& type = model_.types[owner];
    const size_t count = d.kind == Kind::Method ? type.methods.size() : type.fields.size();
    for (size_t m = 0; m < count; ++m) out.push_back({owner, static_cast<int>(m)});
    return out;
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
        return has_field_types(d.fieldTypes, type, c);
      }
      case Kind::Method: {
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
        return !f.hasLiteral && f.isStatic == wantStatic && (!d.type || type_matches(d.type, f.type, f.typeIndex, c));
      }
      case Kind::EnumValue:
        return type.fields[c.member].hasLiteral;
      case Kind::Code:
        break;
    }
    return false;
  }

  // Whether the type has an instance field of each listed type, a field of its own for each. An
  // "@Key" type matches fewer fields than a plain one of the same name, so those are placed first.
  bool has_field_types(std::span<const char* const> wants, const ModelType& type, Candidate& c) const {
    std::vector<const char*> order(wants.begin(), wants.end());
    std::stable_partition(order.begin(), order.end(), [](const char* want) { return entry_ref(want).has_value(); });
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
      case Kind::Field:
        r.offset = type.fields[c.member].offset;
        r.currentName = type.fields[c.member].name;
        break;
      case Kind::EnumValue:
        r.value = type.fields[c.member].literal;
        r.currentName = type.fields[c.member].name;
        break;
      case Kind::Code:
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
  // Counting every access only widens a positive fact, which is safe; a negated one removes a
  // candidate only for an access through this, and one through any other register leaves it
  // undecided.
  Verdict judge(const Fact& fact, const Candidate& c, std::string& why) const {
    const Result& method = result_of(fact.method);
    const ModelType& type = model_.types[c.type];
    const auto unreadable = [&](const std::string& whose) {
      why = std::format("the code of {} could not be fully read for its {} fact", whose, fact_name(fact.is));
      return Verdict::Unread;
    };
    bool yes = false;
    if (fact.is == Fact::Is::Calls) {
      const ModelMethod& self = type.methods[c.member];
      if (!readable(self.rva)) return unreadable(self.name);
      const std::vector<uint64_t> targets = bodies(method);
      for (const uint64_t target : code_at(self.rva)->callTargets)
        yes = yes || std::find(targets.begin(), targets.end(), target) != targets.end();
      return yes ? Verdict::Yes : Verdict::No;
    }
    // A struct's own methods address its fields at the offset minus 0x10 (no object header), so a
    // field fact on a struct would compare the wrong displacement.
    if (fact.is != Fact::Is::CalledBy && type.isValueType) {
      why = std::format("field facts on a struct are not supported ({} is a struct)", describe_type(c.type));
      return Verdict::Unread;
    }
    // An access's this is the candidate's owner only when rcx came in as that owner's this.
    const bool ownersThis = method.type == c.type && rcx_is_this(method.type, method.member);
    bool throughThis = false;
    for (const uint64_t body : bodies(method)) {
      if (!readable(body)) return unreadable(fact.method);
      const CodeFacts& code = *code_at(body);
      if (fact.is == Fact::Is::CalledBy) {
        const uint64_t rva = type.methods[c.member].rva;
        yes = yes || std::find(code.callTargets.begin(), code.callTargets.end(), rva) != code.callTargets.end();
        continue;
      }
      const int32_t offset = type.fields[c.member].offset;
      for (const Access& a : code.accesses) {
        const bool hit = a.disp == offset && (fact.is == Fact::Is::Stores        ? a.store
                                              : fact.is == Fact::Is::StoresConst ? a.hasImm && a.imm == fact.value
                                                                                 : !a.store);
        yes = yes || hit;
        throughThis = throughThis || (hit && a.thisBase && ownersThis);
      }
    }
    if (fact.negate && yes && !throughThis && fact.is != Fact::Is::CalledBy) {
      why = std::format("the negated {} fact matches {} only through a register not proven to hold this",
                        fact_name(fact.is), fact.method);
      return Verdict::Unread;
    }
    return yes ? Verdict::Yes : Verdict::No;
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
    r.status = Status::Resolved;
    r.type = method.type;
    r.rva = matches.front() + d.patternOffset;
  }

  // An entry's type string against a model type string and, when the model knows it, the index of
  // the type that string names. "@Key" means the very type entry Key resolved to: a known index must
  // be that type, and an unknown one is trusted by name only when no other model type shares the
  // name. Otherwise the candidate is marked unsure.
  bool type_matches(const char* want, const std::string& have, int haveIndex, Candidate& c) const {
    const auto ref = entry_ref(want);
    if (!ref) return canonical_type(want) == have;
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
      case Kind::Method: {
        const ModelMethod& m = type.methods[c.member];
        std::string params;
        for (const std::string& p : m.params) params += (params.empty() ? "" : ",") + p;
        return std::format("{}({})", m.name, params);
      }
      case Kind::Field:
        return std::format("{}: {} at {:#x}", type.fields[c.member].name, type.fields[c.member].type,
                           type.fields[c.member].offset);
      case Kind::EnumValue:
        return std::format("{} = {}", type.fields[c.member].name, type.fields[c.member].literal);
      case Kind::Code:
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
  std::unordered_map<std::string_view, size_t> index_;  // the first entry with each key
  std::unordered_map<std::string, int> typesNamed_;     // how many model types have each name
  mutable std::unordered_map<uint64_t, CodeFacts> code_;  // read once per function start
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
    if (e->assembly) out.emplace_back(e->assembly);
  std::sort(out.begin(), out.end());
  out.erase(std::unique(out.begin(), out.end()), out.end());
  return out;
}

Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image) {
  return Resolver(entries, model, image).run();
}

}  // namespace pg::sdk
