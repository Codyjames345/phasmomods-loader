#pragma once

#include <cstddef>

#include "runtime.h"

// The aimed interaction's filter of what the ghost's sensor reports (Sensor.GetDetectedComponents): only
// the aimed object stays. Main thread.
namespace pm::aim {

// Keeps in the List `found` only the items `keep` passes. The rest go last first through the List's own
// RemoveAt, which keeps the ones left in their order and never reallocates.
template <class Keep>
void keep_only(const api::List& found, Keep&& keep) {
  for (size_t i = found.size(); i-- > 0;)
    if (!keep(found[i])) found.remove_at(i);
}

}  // namespace pm::aim
