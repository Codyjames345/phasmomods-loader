#pragma once

// While the ghost player picks, their camera (the investigator's, detached by src/body.cpp) stands
// in the chosen favourite room and turns slowly on the spot, the way the haunted mirror's camera
// shows the ghost's room, under a light of its own near the ceiling so the room can be seen. It
// starts as soon as the investigator leaves, so the truck is never shown. The light exists only on
// the ghost player's client. Main thread only.
namespace pg::panorama {

bool resolve();
// Every frame until the ghost spawns: room is a favourite-room candidate index (negative: the room
// already shown, or the first one).
void show(void* camera, int room);
void hide();  // removes the light; the camera is the ghost's again once it spawns

// A black screen while the ghost player is in a contract but still has their investigator (the
// moments after loading, before every player has loaded and the panorama can begin): otherwise
// they see the truck. Every frame; on is false the rest of the time.
void cover(bool on);

}  // namespace pg::panorama
