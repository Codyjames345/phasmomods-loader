#pragma once

#include <vector>

// The apparition's rules, kept free of game calls so they can be tested offline. A copy is a clone of
// the model, so its transforms come in the same order as the model's: whatever sits at one place in
// the model sits at the same place in the copy.
namespace pm::custom {

/** Where `item` is in `list`, or -1 when it is not (or is null). */
inline int index_in(const std::vector<void*>& list, const void* item) {
  if (!item) return -1;
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i] == item) return static_cast<int>(i);
  return -1;
}

}  // namespace pm::custom
