#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "model.h"

// Builds a small game model for the resolver's tests. Type strings are written canonically, as the
// model holds them.
struct ModelBuilder {
  pg::sdk::GameModel model;

  int type(std::string assembly, std::string ns, std::string name, int declaring = -1, bool isEnum = false) {
    pg::sdk::ModelType t;
    t.assembly = std::move(assembly);
    t.ns = std::move(ns);
    t.name = std::move(name);
    t.declaring = declaring;
    t.isEnum = isEnum;
    model.types.push_back(std::move(t));
    return static_cast<int>(model.types.size()) - 1;
  }

  void field(int type, std::string name, std::string type_, int32_t offset, bool isStatic = false) {
    pg::sdk::ModelField f;
    f.name = std::move(name);
    f.type = std::move(type_);
    f.offset = offset;
    f.isStatic = isStatic;
    model.types[type].fields.push_back(std::move(f));
  }

  // An enum's value: a static literal field of the enum's own type.
  void literal(int enumType, std::string name, int64_t value) {
    pg::sdk::ModelField f;
    f.name = std::move(name);
    f.type = model.types[enumType].name;
    f.isStatic = true;
    f.hasLiteral = true;
    f.literal = value;
    model.types[enumType].fields.push_back(std::move(f));
  }

  void method(int type, std::string name, std::string ret, std::vector<std::string> params, uint64_t rva,
              bool isStatic = false) {
    pg::sdk::ModelMethod m;
    m.name = std::move(name);
    m.ret = std::move(ret);
    m.params = std::move(params);
    m.rva = rva;
    m.isStatic = isStatic;
    model.types[type].methods.push_back(std::move(m));
  }

  pg::sdk::GameModel build() const { return model; }
};
