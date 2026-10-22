# The ghost player keeps their real journal

**Decided by the user (2026-10-19):** the ghost player has a working journal (the game's own,
including its pause menu, Media tab kept) with a custom page for choosing the ghost type and
favourite room; the same page is used when a native favourite-room change allows a new choice. The
mod's own pause overlay goes. They can see the investigators' evidence and ghost selections but not
change them. The HUD/journal shows the team's average sanity and when hunting becomes possible.

Supersedes the copied-journal picker (2026-10-19-ghost-pause-and-journal-picker.md): the copy's pages
never rendered over the 3D book, whatever was replicated of the journal's opening.

## Facts (build 25811160)

- The journal is `PCPlayer/NetworkedJournal` on the networked body: `NetworkedJournal` (layer 8,
  `JournalRigController` + PhotonView), `Journal_anim` (Animator `BookController`, bool `isOpened`),
  `Journal_geo` (3D book, layer 0) and `Journal2.0Canvas` (world-space canvas, layer 5,
  `JournalController`, `PauseMenuController`, PhotonView, ...). Evidence.1 / Evidence.2 pages carry
  `JournalEvidenceSyncController` / `JournalGhostSyncController` and PhotonViews.
- Opening: `LocalJournalController.OpenCloseJournal` (rig) -> `JournalController.OpenCloseJournal`
  (0x22C3CF0) -> RPC `StateChangeSync` (0x22C5940) to everyone on the canvas's PhotonView, which
  enables canvas/content/raycaster (0x22CCF30), updates tabs, runs `OpenCloseJournalNetworked`
  (Animator `isOpened`, tab layers) and `NetworkedPropGrab.HideShowHandPointsForJournal` on the
  sender's Player (none for the ghost player).
- Investigators' selections: `JournalTypedButtonSyncController.ToEveryone_NotifyOfButtonStateChange_Rpc`
  (0x3A9DD00) updates the journal of every `Network+0x38` PlayerSpot's Player (`+0xC8`); the ghost
  player has none, so their journal must be updated explicitly.
- `PhotonView`: `Synchronization` +0x48 (0 = Off), `ObservedComponents` +0x58.
- Pause menu: `PauseMenuController.Leave` (0x2369A70) / `Quit` (0x236A0D0) read `Network+0x28`.

## Stages

1. **Keep the journal.** `body.cpp` detaches `NetworkedJournal` before `PhotonNetwork.Destroy`, sets
   its PhotonViews' synchronisation off, and switches `JournalRigController` off (its parents are
   gone). RPCs from these views run locally only (hook on `PhotonView.RPC`); the hand-points call is
   skipped for a null player. The journal is held in front of the ghost camera while open.
   Escape (Pause binding) opens it on the pause tab through `JournalController.OpenCloseJournal`;
   Leave/Quit are hooked to the body-free leave/quit (moved from `pause.cpp`, whose overlay goes).
   Evidence and ghost buttons are made non-interactable. Released with the contract.
2. **Pick page in the real journal.** At the start the journal opens on the evidence spread with the
   map page in place of Evidence.1; ghost buttons show the pick (ticks/crosses restored afterwards).
   The same page opens during a native favourite-room change window; the claim may be any
   candidate room on the map (host validation updated).
3. **Sanity.** Host adds the team's average sanity and the hunt threshold to STATUS (protocol
   version 3); the HUD shows it with an indicator once hunts are possible.
4. **Investigators' selections.** After the native RPC handler, apply the same state to the ghost
   player's journal's sync controller.

Each stage is validated offline where logic allows, reviewed, then tested live by the user.
