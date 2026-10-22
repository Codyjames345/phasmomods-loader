#pragma once

#include <memory>
#include <utility>
#include <vector>

// Common ownership and hierarchy rules for the tests' simulated Unity objects.
namespace ui_fixture {
template <class Node>
Node* make_node(std::vector<std::unique_ptr<Node>>& store, const char* name = "", Node* parent = nullptr) {
  auto value = std::make_unique<Node>();
  value->name = name;
  value->parent = parent;
  Node* result = value.get();
  store.push_back(std::move(value));
  if (parent) parent->children.push_back(result);
  return result;
}

template <class Node>
bool active_in_hierarchy(Node* value) {
  for (; value; value = value->parent)
    if (!value->alive || !value->active) return false;
  return true;
}
}  // namespace ui_fixture
