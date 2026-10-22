#pragma once

#include <vector>

#include "il2cpp.h"

// Growing the game's four-player UI: copies of one of its own elements, and the arrays that list them.
// Main thread.
namespace pg::lobbies::expand {

bool resolve();

// A copy of `original` (a component or GameObject) under the same parent, active if it was; returns
// the copy as the same kind of object. Unity remaps the copy's references to its own children.
void* clone(void* original);
// The component in `copyRoot` at the place `component` has under `root` (by child indices); null
// when `component` is not under `root`. All three are components or GameObjects.
void* counterpart(void* root, void* component, void* copyRoot);
// A new array of `array`'s element type: its items followed by `extra`.
il2cpp::Array* grown(il2cpp::Array* array, const std::vector<void*>& extra);
// Whether a layout group (grid, row) places the element; a LayoutElement is added when missing.
void ignore_layout(void* component, bool ignore);
bool list_add(il2cpp::List* list, void* item);  // List<T>.Add for a reference type T
// A scene PhotonView ID no PhotonView in the scene uses, the same on every client (above the
// highest one in use), plus `offset`.
int free_view_id(int offset);
void set_view_id(void* gameObjectOrComponent, int id);  // before the copy's first activation

}  // namespace pg::lobbies::expand
