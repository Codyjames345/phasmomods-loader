#include "resolver.h"

#include <algorithm>
#include <format>
#include <optional>
#include <unordered_map>

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

// A type or one of its members: member is -1 for a type.
struct Candidate {
  int type = -1;
  int member = -1;
  // A type name the candidate uses for an "@Key" type that several model types share, when the model
  // does not say which of them it means. Such a candidate cannot be accepted.
  std::string unsure;
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
      return missing(r, d.kind == Kind::Code ? "patterns not supported yet" : "facts not supported yet");
    }

    if (d.owner && entries_[index_.at(d.owner)]->kind != Kind::Type)
      return missing(r, std::format("owner {} is not a type entry", d.owner));
    if (!d.owner && d.kind != Kind::Type) return missing(r, "a member needs an owner");
    const int owner = d.owner ? result_of(d.owner).type : -1;
    if (d.kind == Kind::EnumValue && (owner < 0 || !model_.types[owner].isEnum))
      return missing(r, std::format("owner {} is not an enum", d.owner ? d.owner : "(none)"));
    if (!d.name) return missing(r, "shape matching not supported yet");

    // The pipeline: candidates, then every check the entry declares, then unique or report.
    std::vector<Candidate> candidates = by_name(d, owner);
    const size_t named = candidates.size();
    // A method named without its parameters cannot tell overloads apart, and no other check may
    // choose among them for it.
    if (d.kind == Kind::Method && d.params.empty() && named > 1) return decide(d, r, candidates, named, owner);
    std::erase_if(candidates, [&](Candidate& c) { return !fits(d, c); });
    decide(d, r, candidates, named, owner);
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
        return type.declaring == declaring;
      }
      case Kind::Method: {
        const ModelMethod& m = type.methods[c.member];
        if (m.isStatic != wantStatic) return false;
        if (d.type && !type_matches(d.type, m.ret, m.retIndex, c)) return false;
        // No params: matched by name alone, any arity.
        if (d.params.empty()) return true;
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

  void decide(const EntryDesc& d, Result& r, const std::vector<Candidate>& candidates, size_t named,
              int owner) const {
    const std::string where = owner >= 0 ? " in " + describe_type(owner) : "";
    if (candidates.empty()) {
      if (named == 0) return missing(r, std::format("no {} named {}{}", kind_name(d.kind), d.name, where));
      return missing(r, std::format("no {} named {}{} matches ({} by name)", kind_name(d.kind), d.name, where, named));
    }
    if (candidates.size() > 1) {
      r.status = Status::Ambiguous;
      r.detail = std::format("{} {}s named {}{} match", candidates.size(), kind_name(d.kind), d.name, where);
      for (const Candidate& c : candidates) r.candidates.push_back(describe(d.kind, c));
      return;
    }

    const Candidate& c = candidates.front();
    if (!c.unsure.empty()) {
      r.status = Status::Ambiguous;
      r.detail = std::format("{} types are named {}, and the model does not say which {} {}{} uses",
                             typesNamed_.at(c.unsure), c.unsure, kind_name(d.kind), d.name, where);
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

Resolution resolve(std::span<const EntryDesc* const> entries, const GameModel& model, const CodeImage* image) {
  return Resolver(entries, model, image).run();
}

}  // namespace pg::sdk
