#pragma once

#include <string>
#include <vector>

#include "il2cpp.h"
#include "ui.h"

// The truck's map (MapController's map camera and its floors) shown inside the picker, with the
// level's favourite-room candidates clickable. Everything here is local to the ghost player's
// client: MapController.ChangeFloor and the camera's enabled state are not networked. Main thread
// only.
namespace pg::mapview {

bool resolve();

// Show the map in a RawImage under parent; false when this level's map cannot be shown. eventCamera
// renders parent's canvas when it is in world space (null for an overlay canvas).
bool show(void* parent, ui::Vec2 position, ui::Vec2 size, void* eventCamera);
void hide();  // restores the map camera and floor the truck had

void change_floor(bool up);
std::string floor_name();  // the floor the map is showing

// The favourite-room candidate (index into the level's list) under a screen point, or -1.
int room_at(ui::Vec2 screenPoint);
// A candidate's colliders on the shown map, each as a rectangle 0..1 from the bottom left; empty
// when the room is on another floor.
std::vector<ui::Rect> room_outline(int index);
void* image();  // the RawImage, for placing markers
// The centre of a candidate's first collider in world space; false when it has none.
bool room_center(int index, il2cpp::Vector3& out);

}  // namespace pg::mapview
