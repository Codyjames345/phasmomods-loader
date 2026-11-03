# The journal: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [The journal](../systems/journal.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## Ghost lists, the evidence filter and the guess sync

- `JournalController.Awake` runs `RebuildGhosts`, then `Initialise` on the ghost sync controller
  (`JournalController.ghostSync`) and the evidence one (`JournalController.evidenceSync`).
- `RebuildGhosts` walks `allGhostsData.ghosts` (a `GhostCollection`; in a contract the host
  extends a journal only when it is the contract's `GhostController.allGhostsCollection`, in the lobby
  only once the custom ghosts are in the journal's own): for each `GhostData` it adds
  `GetGhostData()` to `allGhosts` and, with the first button in
  `allGhostTypeButtons` whose `GhostButton.type` is that type, to `ghostButtons`
  (a `Dictionary<Ghost, GhostButton>`) with `Add`, which throws on a ghost already there. A
  ghost with no button of its type gets no entry. The pairing is by type alone, so a copied button of a
  custom type pairs with the custom ghost's `GhostData`.
- `SetGhostTypes` clears the other `List<Ghost>` and walks `ghostButtons`: it
  compares each ghost's evidence with the chosen evidence and the ruled-out evidence,
  and sets each button's look through a virtual method of the button. An obfuscated
  copy of the same length is never called.
- `JournalGhostSyncController`'s generic base: `Initialise` reads the buttons through a
  virtual getter (which returns `JournalController.allGhostTypeButtons`) into its own array, subscribes
  to each button's `OnUserStateChanged` (the delegate field `NetworkThreeStateButton.userStateChanged`),
  and adds a listener to the Network's player-left event through a helper that also adds it to a
  tracker. The listener runs `NetworkThreeStateButton.RemovePlayerSelection` on every button for the
  player who left, which is idempotent.
  `SetSelectedButtonState` matches received guesses against that array by type; a change is sent
  with the button's type. `OnDestroy` unsubscribes the buttons in
  that array and removes the listener through a helper that drops every matching listener but only the
  tracker's first matching entry.
- The base is generic, so the map has no entry for that array or `Initialise`; the host runs `Initialise`
  through the runtime, by name, after the journal's arrays hold the copies. Each game button's delegate
  is kept before and put back after, so it keeps one subscription. The second `Initialise` also adds a
  second player-left listener and tracker entry: the listener runs `RemovePlayerSelection` twice, which
  changes nothing more, and `OnDestroy` removes both listeners but one tracker entry, so one stale
  tracker reference stays per extended journal until the tracker goes. The game offers no removal the
  host could make first without building the controller's own delegate.

## Clicks

- `Button.OnPointerClick` returns unless the event's button is the left one,
  then jumps to `Press`; `Button.OnSubmit` calls `Press`, then, while the
  button is active and interactable, shows the pressed state and starts its release. `Press` invokes
  `onClick` only while the button is active (`IsActive`, virtual) and interactable (`IsInteractable`,
  virtual).
- A copied list entry keeps the Spirit's onClick, which names the Spirit's page. The host's hook on
  `Press` takes that call for the custom entries only: active and interactable, it turns the journal to
  the custom page with `GoToPage(int)`; otherwise it does nothing, as `Press` would. `OnSubmit`'s pressed
  state runs as for any button.

## What the host adds

Per custom ghost shown (a contract's that holds them, else, in the lobby, the ones offered while
content is active there, which first join the collection the journal lists), in type order, once per
decision: a copy of the Spirit's guess button
(type set, name key on its localiser, alphabetical by object name), a copy of the Spirit's page
(name key, a description key in the form most of the game's pages agree on (`Journal2_` and the name: the Poltergeist's
`Journal2_Polter` and the Mimic's `Journal2_TheMimic` differ) with the type's number, and the
game's own key for each evidence, the one key every vanilla page of a ghost giving that evidence shows,
or for a custom evidence its row's name key (`host/content_evidence.h`); an evidence no key names hides
its text), a copy of the Spirit's list entry and a copy of the Spirit's row on the Home tab's contents,
whose number is the one its page shows (`JournalController.PageSync`). A row goes on the first
page when its name comes before the second page's first ghost, and the ghost rows of each page go in
alphabetical order (rows whose key is no ghost's name key name the book's sections and stay above). The book's
ghost pages then take the order of those rows, from where the first ghost's page lay, and every ghost row's
number is written from its page, the game's rows too, so the contents count up. The names alone decide that order, so every client with the
same mods numbers the pages alike. The game's tabs name pages before the ghosts by number and its list entries name
their pages by object (`GoToPage(GameObject)` walks `pages` for the object), and its only two calls of
`GoToPage(int)` in code pass 0, so nothing else names a ghost's page by number. The Ghosts tab's
`max` moves by the number of pages added. The list's grid is set to its hand placement (the first
entry's size, as many columns as share its row) and switched on, and both grids' rows get shorter so
every ghost fits where the game's did. The contents' rows and their numbers get shorter by one factor across
both pages, the fuller page's, and the texts size themselves to fit. Every copy is made, and every runtime method the change needs is
found, before the journal changes; a failure takes the copies out (destroyed through the engine's
`Object::Destroy`, which `Object.Destroy` calls) and puts the journal's arrays, tab and lists
back as the game made them. One step cannot be undone: when `allGhosts` cannot be emptied after
`ghostButtons` was, the game's builder runs again over the old buttons, so `allGhosts` holds its old
ghosts and then the whole collection again, the custom ghosts included and paired with no button
(logged). When the guess sync cannot read the new buttons, what putting the journal back did is
logged step by step.

## Putting a journal back

What the journals show changes when content stops being active in the lobby (a player joins with other
mods, say), when a contract loads or ends, and when the mods stop. A journal still alive that was given
custom ghosts or evidence rows under the last decision is then put back as the game made it, before it
is given the new ones (the custom entries have left its collection first): the copies are taken out,
its button, page and evidence arrays and the Ghosts tab's `max` are the ones it had, its lists are
emptied and made again by `RebuildGhosts`, its guess and evidence syncs read the game's buttons again
(each game button's delegate kept and put back, as when extending), the guess grid's cell, the list
grid's cell, constraint and switch, the evidence column's spacing and the contents' rows (order, sizes and numbers) are as they were, and
`SetGhostTypes` runs. A grid layout places each child by writing its anchors (both to the parent's
top-left corner), its size (the cell) and its position (`GridLayoutGroup.SetCellsAlongAxis`,
`LayoutGroup.SetChildAlongAxisWithScale`), so switching the list's grid on moved the game's
own entries; switching it off leaves them where it put them. So each entry's anchors, size and position
are read before the grid goes on and written back after it goes off. Each `Initialise` adds a
player-left listener and tracker entry (above), the guess sync's and the evidence sync's alike: each
sync controller extended and put back k times holds 2k more listeners while its journal lives (each
runs the idempotent `RemovePlayerSelection` when a player leaves) and leaves 2k stale tracker
references when it goes with its scene, so up to 4k per journal that also had evidence rows.

What the game keeps by number is put back too:

- **Evidence marks.** `AddEvidence` appends an evidence number to the found list,
  `FilterOutEvidence` to the ruled-out list, and `SetGhostTypes` keeps a ghost only when every
  found number is one of its evidence and none of the ruled-out ones is. A mark on a custom row would
  stay in a list with its row gone, crossing out every game ghost. The host calls the game's
  `JournalController.RemoveEvidence` (the number off both lists, then the filter) for each custom evidence the
  journal got a row for; a number on neither list changes nothing.
- **The guess.** `SelectGhost` copies the chosen ghost, by type, from `allGhosts` into the
  journal; `GetGhostType` returns its type and `UnselectGhost` sets it
  to None. `RebuildGhosts` leaves it alone, so a guess on a custom ghost would stay by number with no
  button. The host calls `UnselectGhost` when the guess is one of the custom types the journal was
  given. A guess on a game ghost stays.
- **The current page.** The page sync stores its number (`JournalController.currentPage`) before it bounds-checks it, and
  `NextPage` and `PreviousPage` step from that number. Each holds its own copy of
  `GoToPage(int)`'s code and never calls it: the bound `[0, page count]`, a count that is itself past the
  last page, then the page sync. A reader left on a custom page would so send numbers past the book on
  every turn. When the local player's own book (its
  `photonView`'s `IsMine`, what `GoToPage` reads to decide whether to send) is on a page whose number
  changed, whether custom pages just came or just went, the host calls `GoToPage(int)` with the page its Ghosts tab opens: the tab's `min`, the page the
  tab's own button names (24 in PCPlayer, above). The page sync then tells every copy. Another
  player's copy is left to its owner's next turn.
- **A reader on the last game page.** The page arrows are set by the page sync against the book it
  ran on: a local reader who was on the last game page while the custom pages followed keeps a next
  arrow after the book is put back. A press sends a page sync for the page count, past the book; every
  modded copy skips it as the owner's (below), and the arrow clears with the next turn back or once
  content is active again. The host does not re-show that page: the game's only way to do so is a page
  turn, with its sound and a buffered sync for each.

## Page syncs beyond a book

`PageSync` first checks that the sender owns the book; a sender who does not is reported
by the game, and the call changes nothing. For the owner it then shows or hides the page arrows, writes
the page labels and plays the page-turn sound, stores the number as the current page, deactivates every
page, and throws IndexOutOfRangeException for a number outside this client's pages, leaving the book
blank; PUN's dispatch catches the throw and rethrows the first after the loop (logged). Contracts never
mix books of different lengths, but the lobby does: a joiner replays every buffered sync (`AllBuffered`)
before its content is active, and a client can be a moment behind another around a join.

A number that means another page than in the book as the game made it (any from the first page the custom ghosts moved on) is not buffered at all, and goes only to verified players (`sdk/systems/photon.md`). The host's hook on
`PhotonView.RPC` knows the sync by its name and reads its page, whichever of `GoToPage(int)`, `NextPage` and `PreviousPage` sent
it. It turns the `AllBuffered` of such a number into `All`, or into a run on this client alone while
a player is unverified, so a joiner replays only the game's own numbers. A number past the book can still arrive from a client that is a moment behind.

The host's hook on `PageSync` skips a call only when this client is in a private room, the message's
sender (`PhotonMessageInfo.Sender`) is positively the book's owner (`PhotonView.owner` of the journal's
view), and the number is outside this client's own pages (logged once): the owner's call the game would
take as far as the throw. The copy keeps its last page. Every other call reaches the game untouched,
its sender check included: a page inside the book, a sender who is not the owner, a sender or owner
not known here, any call out of a private room. A player without the mods still gets the game's throw
until the join guard removes them.
