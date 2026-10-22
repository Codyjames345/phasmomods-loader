# Ghost pause overlay and journal-style picker

**Decided by the user (2026-10-19):** the ghost player gets the mod's own pause overlay (Resume,
Leave contract, Quit game; no Settings). The type/room picker becomes the game's open journal:
the journal's own ghost list on the left page, the truck map with clickable rooms on the right.

## Facts this rests on (build 25811160)

- The journal and its pause menu live on the networked body
  (`PCPlayer/NetworkedJournal/Journal_anim/Journal2.0Canvas`, resources.assets), which the ghost
  player's client destroys at all-loaded (`body.cpp`). Native Leave/Quit also read `Network+0x28`.
- Vanilla Leave = `LoadingController.LoadScene(false, 0, "", true)` then `PhotonNetwork.Disconnect()`
  (coroutine `PauseMenuController/…MoveNext`, 0x2395950). Vanilla Quit ends in `Application.Quit`.
- Journal `Content` holds `Book Image` (1280x720 open book) and `GhostHuntingBookContent`
  (scale 0.419) with the page-turn arrows `PreviousButtonL` / `NextButtonR` and the journal's ghost
  page `Evidence.2`, whose `All Ghosts` grid has the `GhostButton`s (type at +0x78), each a
  `ThreeStateButton` with named `tickedImage` / `crossedOutImage`. (`Ghost Select Page` looks
  similar but its buttons are plain `Button`s.) `Evidence.2` itself carries
  `JournalGhostSyncController` and a `PhotonView`: in the copy its children move to a plain page and
  it is destroyed without waking; the copy is made inside an inactive parent and the button scripts
  are disabled before anything wakes.
- `MapController` (static instance) owns `mapCamera` (+0x68), `allFloors` (+0x30, `Floor{floorType,
  floor}`) and the current floor index (+0x38). `ChangeFloor(bool up)` is local only. The camera is
  only enabled while someone is in the truck. `LevelRoom` has `roomColliders` (+0x30) and its floor
  (+0x68, same enum as `Floor.floorType`).

## Tasks

1. **Pointer and UI primitives** (`game`, `ui`): mouse position and left-press edge from the Input
   System, cursor free/locked, `RectTransformUtility.RectangleContainsScreenPoint` hit test,
   `RawImage` creation. Offline: none (engine boundary); covered by live logging.
2. **Pause overlay** (`pause.{h,cpp}`): Escape (the player's own Pause binding) during a contract
   opens it for the ghost player; ghost input stops and the cursor is freed; Resume/Escape closes;
   Leave runs the vanilla leave calls; Quit calls `Application.Quit`. Pure open/close/choice state
   in `pause_model.h` with `tests/pause_test.cpp`.
3. **Journal capture** (`journal.{h,cpp}`): before `body.cpp` destroys the networked body, copy
   the journal `Content` into an inactive holder with every `NetworkThreeStateButton` disabled, and
   keep only the book, the ghost page and the arrows active. Released with the contract.
4. **Map view** (`mapview.{h,cpp}`): show `mapCamera`'s texture in a `RawImage`, enable the camera
   while shown and restore it after, floor arrows through `ChangeFloor`, and map a click to the
   favourite-room candidate whose collider bounds contain the ray point on the current floor. The
   point-in-room choice is pure (`room_pick.h`) with `tests/room_pick_test.cpp`.
5. **Picker** (`picker.cpp` rewritten): open book over a dark backdrop; left page ghost buttons
   ticked (chosen) / crossed out (not allowed, or not the forced type); right page heading, map,
   floor arrows, chosen room name, countdown and a confirm line. Clicks only; the existing session
   messages are unchanged. Falls back to the current list picker if the journal or map is missing.
6. **Review, validate, package**, then a live test by the user with screenshots.

Exit hang: separate; needs a stack of the hung main thread before any change.
