#include "runtime_model.h"

#include <windows.h>

#include <functional>
#include <string_view>
#include <unordered_map>

#include "types.h"

namespace pg::sdk {
namespace {

// Il2CppTypeEnum values and metadata flags (ECMA-335 II.23.1.5, II.23.1.10).
constexpr int kTypeI1 = 0x04, kTypeU1 = 0x05, kTypeI2 = 0x06, kTypeU2 = 0x07, kTypeI4 = 0x08, kTypeU4 = 0x09,
              kTypeI8 = 0x0A, kTypeU8 = 0x0B, kTypeValueType = 0x11, kTypeClass = 0x12;
constexpr int kFieldStatic = 0x10, kFieldLiteral = 0x40;
constexpr uint32_t kMethodStatic = 0x10;

template <class... F>
bool all_set(F... exports) {
  return ((exports != nullptr) && ...);
}

bool complete(const RuntimeApi& a) {
  return all_set(a.domain_get, a.domain_get_assemblies, a.assembly_get_image, a.image_get_name,
                 a.image_get_class_count, a.image_get_class, a.class_get_name, a.class_get_namespace,
                 a.class_get_declaring_type, a.class_is_enum, a.class_is_valuetype, a.class_enum_basetype,
                 a.class_from_type, a.class_get_fields, a.class_get_methods, a.method_get_name,
                 a.method_get_param_count, a.method_get_param, a.method_get_return_type, a.method_get_flags,
                 a.field_get_name, a.field_get_offset, a.field_get_flags, a.field_get_type,
                 a.field_static_get_value, a.type_get_type, a.type_get_name, a.free_memory);
}

// A generic definition's name without its arity: "List`1" is "List".
std::string type_name(const char* raw) {
  std::string_view name = raw ? raw : "";
  if (const size_t tick = name.rfind('`'); tick != std::string_view::npos && tick + 1 < name.size() &&
                                           name.find_first_not_of("0123456789", tick + 1) == std::string_view::npos)
    name = name.substr(0, tick);
  return std::string(name);
}

// The width and signedness of an integer type, 0 for any other.
int integer_width(int kind, bool& isSigned) {
  isSigned = kind == kTypeI1 || kind == kTypeI2 || kind == kTypeI4 || kind == kTypeI8;
  switch (kind) {
    case kTypeI1: case kTypeU1: return 1;
    case kTypeI2: case kTypeU2: return 2;
    case kTypeI4: case kTypeU4: return 4;
    case kTypeI8: case kTypeU8: return 8;
  }
  return 0;
}

class Builder {
 public:
  Builder(const RuntimeApi& api, std::span<const uint8_t> module, const std::function<bool()>& proceed)
      : api_(api), module_(module), proceed_(proceed) {}

  // Every class of every named image, before any member, so members can name any of them.
  bool add_types(std::span<const std::string> assemblies) {
    size_t count = 0;
    void** loaded = api_.domain_get_assemblies(api_.domain_get(), &count);
    for (const std::string& assembly : assemblies) {
      void* image = find_image(loaded, count, assembly + ".dll");
      if (!image) return false;
      const size_t classes = api_.image_get_class_count(image);
      for (size_t i = 0; i < classes; ++i) {
        if (!proceeding()) return false;
        void* klass = api_.image_get_class(image, i);
        if (!klass || index_.contains(klass)) continue;
        index_.emplace(klass, static_cast<int>(model_.types.size()));
        ModelType t;
        t.assembly = assembly;
        t.name = type_name(api_.class_get_name(klass));
        t.isEnum = api_.class_is_enum(klass);
        t.isValueType = api_.class_is_valuetype(klass);
        t.runtime = klass;
        model_.types.push_back(std::move(t));
      }
    }
    return true;
  }

  bool add_members() {
    for (ModelType& t : model_.types) {
      if (!proceeding()) return false;
      void* klass = t.runtime;
      void* declaring = api_.class_get_declaring_type(klass);
      t.declaring = index_of(declaring);
      // A nested type's namespace is its outermost declaring type's, as the dump's folders give it.
      void* outer = klass;
      while (void* up = api_.class_get_declaring_type(outer)) outer = up;
      const char* ns = api_.class_get_namespace(outer);
      t.ns = ns ? ns : "";
      add_fields(t);
      add_methods(t);
    }
    return true;
  }

  GameModel take() { return std::move(model_); }

 private:
  void* find_image(void** loaded, size_t count, const std::string& file) const {
    for (size_t i = 0; loaded && i < count; ++i) {
      void* image = api_.assembly_get_image(loaded[i]);
      const char* name = image ? api_.image_get_name(image) : nullptr;
      if (name && file == name) return image;
    }
    return nullptr;
  }

  bool proceeding() const { return !proceed_ || proceed_(); }

  int index_of(const void* klass) const {
    const auto it = index_.find(klass);
    return it == index_.end() ? -1 : it->second;
  }

  // The model type a type is, by reference or not: only a class or a struct can be one.
  int index_of_type(const void* type) const {
    if (!type) return -1;
    const int kind = api_.type_get_type(type);
    if (kind != kTypeClass && kind != kTypeValueType) return -1;
    return index_of(api_.class_from_type(type));
  }

  std::string type_string(const void* type) const {
    char* raw = type ? api_.type_get_name(type) : nullptr;
    if (!raw) return {};
    std::string out = canonical_type(raw);
    api_.free_memory(raw);
    return out;
  }

  // An integer or enum constant's number; 0 for any other constant, whose value is never read.
  int64_t literal(void* field, const void* type) const {
    int kind = api_.type_get_type(type);
    if (kind == kTypeValueType) {
      void* klass = api_.class_from_type(type);
      const void* base = klass && api_.class_is_enum(klass) ? api_.class_enum_basetype(klass) : nullptr;
      if (!base) return 0;
      kind = api_.type_get_type(base);
    }
    bool isSigned = false;
    const int width = integer_width(kind, isSigned);
    if (!width) return 0;
    uint64_t raw = 0;
    api_.field_static_get_value(field, &raw);
    const int unused = 64 - 8 * width;
    if (unused == 0) return static_cast<int64_t>(raw);
    raw &= ~uint64_t{0} >> unused;
    return isSigned ? static_cast<int64_t>(raw << unused) >> unused : static_cast<int64_t>(raw);
  }

  void add_fields(ModelType& t) const {
    void* iter = nullptr;
    while (void* field = api_.class_get_fields(t.runtime, &iter)) {
      const int flags = api_.field_get_flags(field);
      // An enum's instance field value__: the dump does not list it.
      if (t.isEnum && !(flags & kFieldStatic)) continue;
      const void* type = api_.field_get_type(field);
      ModelField f;
      const char* name = api_.field_get_name(field);
      f.name = name ? name : "";
      f.type = type_string(type);
      f.typeIndex = index_of_type(type);
      f.hasLiteral = (flags & kFieldLiteral) != 0;
      f.isStatic = (flags & kFieldStatic) != 0 || f.hasLiteral;
      // A constant has no storage; the dump gives it offset 0.
      f.offset = f.hasLiteral ? 0 : static_cast<int32_t>(api_.field_get_offset(field));
      if (f.hasLiteral && type) f.literal = literal(field, type);
      f.runtime = field;
      t.fields.push_back(std::move(f));
    }
  }

  void add_methods(ModelType& t) const {
    void* iter = nullptr;
    while (const void* method = api_.class_get_methods(t.runtime, &iter)) {
      ModelMethod m;
      const char* name = api_.method_get_name(method);
      m.name = name ? name : "";
      const void* ret = api_.method_get_return_type(method);
      m.ret = type_string(ret);
      m.retIndex = index_of_type(ret);
      const uint32_t params = api_.method_get_param_count(method);
      for (uint32_t i = 0; i < params; ++i) {
        const void* param = api_.method_get_param(method, i);
        m.params.push_back(type_string(param));
        m.paramIndex.push_back(index_of_type(param));
      }
      uint32_t implFlags = 0;
      m.isStatic = (api_.method_get_flags(method, &implFlags) & kMethodStatic) != 0;
      // MethodInfo's first member is its code. A generic method's shared code can lie outside the
      // module, and a method without code has none.
      const auto code = *static_cast<const uintptr_t*>(method);
      const auto begin = reinterpret_cast<uintptr_t>(module_.data());
      if (code >= begin && code - begin < module_.size()) m.rva = code - begin;
      m.runtime = method;
      t.methods.push_back(std::move(m));
    }
  }

  const RuntimeApi& api_;
  std::span<const uint8_t> module_;
  const std::function<bool()>& proceed_;
  GameModel model_;
  std::unordered_map<const void*, int> index_;
};

}  // namespace

std::span<const uint8_t> loaded_module(uintptr_t base) {
  if (!base) return {};
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0) return {};
  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<uint32_t>(dos->e_lfanew));
  if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) return {};
  return {reinterpret_cast<const uint8_t*>(base), nt->OptionalHeader.SizeOfImage};
}

bool load_runtime(const RuntimeApi& api, uintptr_t gameAssemblyBase, std::span<const std::string> assemblies,
                  GameModel& out, const std::function<bool()>& proceed) {
  const std::span<const uint8_t> module = loaded_module(gameAssemblyBase);
  if (module.empty() || !complete(api)) return false;
  Builder builder(api, module, proceed);
  if (!builder.add_types(assemblies) || !builder.add_members()) return false;
  out = builder.take();
  return true;
}

}  // namespace pg::sdk
