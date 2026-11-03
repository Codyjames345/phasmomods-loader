# The journal

How the game's journal holds its ghosts, its pages and its texts, and how the host's content journal
(`host/content_journal.h`) adds custom ghosts to it. The entries are in `sdk/map/JournalController.h`,
`sdk/map/LocalisationSystem.h`, `sdk/map/UnityObject.h`, `sdk/map/UnityUI.h`, `sdk/map/Transform.h` and
`sdk/map/Scene.h`. The code facts below are from Steam build 25811160, read with `re/decomp.mjs`, and
the prefab facts from the PCPlayer prefab in `resources.assets` with UnityPy (as `tools/assetcheck.py`
reads it; a Button's onClick is read from its MonoBehaviour's raw data, whose typetree is stripped). The
entries resolve the code facts afresh on every build.

## The book

- Every player's journal is on every client: one `JournalController` per player's book, on the object
  `Journal2.0Canvas` of the player prefab. Its scene facts (`Scene.Journal*`) are read from PCPlayer.
- `JournalController.pages` is a `GameObject[]`; a page's number is its index. PCPlayer has 55 pages,
  `Contents A` (0) to `Yurei` (54).
- `GoToPage(int)` turns to a page locally and sends the page sync RPC (`PageSync`)
  to the other players' copies of this book, by number. So a page added on one client must
  have the same number on every client.
- `journalTabs` are `JournalTab` objects with `min` and `max`, floats. The tab of the
  current page is the first in order whose `min <= page <= max`, as `UpdateSelectedTab`
  works it out. PCPlayer's: Home 0 to 5, Items 6 to 13, Overview 14 to 15, Media 16 to 19, Evidence 20
  to 23, Ghosts 24 to 55: the Ghosts tab's `max` is the page count, one past its last page, and it is the
  tab that covers the last page. The `Ghosts Tab` button's onClick calls `JournalController.GoToPage`
  with the int 24.
- The book holds a page per vanilla ghost, named after it, each with the same seven children in the
  same order: `Title`, `------------`, `Description`, `Evidence`, `Evidence 1`, `Evidence 2 ` (a
  trailing space), `Evidence 3` (all 30 pages). Each text carries a `TextLocaliserUI`.
- The Ghosts tab's list (`Ghost Select Page/Ghost Types List`) holds one Button per ghost, named after
  it. The Spirit's entry's onClick calls `JournalController.GoToPage` with an object argument, the
  GameObject `Spirit`, its page (the overload taking a GameObject); each entry so names its
  own page. Its `GridLayoutGroup` ships switched off and the entries are placed by hand.
- The Home tab's two contents pages list the book: `Contents A/Contents Group` holds a row per section, then
  the ghosts from `Aswang` to `Goryo`, and `Contents B/Ghost Types` holds the rest. Each row shows a name
  through its `TextLocaliserUI` and has one child, a `TextMeshProUGUI` with a page number typed into the prefab;
  the rows aren't buttons. Both texts size themselves to their rectangles (55 at most, 18 at least). A
  `VerticalLayoutGroup` stacks the rows without changing their size.
- `PageSync` writes a page's number plus 1 in the left corner and plus 2 in the right. A ghost's page lies on the
  right, so that plus 2 is its number in the contents: the Yurei, page 54, shows 56. The prefab's numbers are typed
  in: its Jinn and Kormos rows have each other's numbers, and so do its Obake and Obambo rows, which it also lists
  out of order.
- With custom ghosts the book's ghost pages follow the contents, alphabetically, so a game's ghost can have another
  page number than in the unmodded game. Find a ghost's page by its object or its title's key, never by a number. The
  pages before the ghosts keep their numbers.

## The lobby's journal

The lobby is the `Main_Menu` scene (build scene 2, and its seasonal copies). The scene holds no
`JournalController` of its own (none among `level2`'s MonoBehaviours): its journals are the players'
PCPlayer books, the same prefab a contract's are.

- `MainManager`'s `Start` coroutine spawns the local player when the menu
  scene starts: back from a contract in a room through `Network.SpawnNetworkedPlayer(Transform)`
  and its overload taking a position and rotation, otherwise through `MainManager.SpawnLocalPlayer` or
  `Network.SpawnLocalPlayer`. `MainManager.OnLeftRoom` tail-jumps to
  `SpawnLocalPlayer` too. Each loads the Resources prefab `LocalPCPlayer` (`LocalVRPlayer (OpenXR)` in
  VR), instantiates it locally and calls its virtual `CreateNetworkedPlayer` (the only
  virtual method of `LocalPlayer` that returns a `Player`; its result is stored as `Network`'s local
  Player).
- `LocalPCPlayer.CreateNetworkedPlayer` instantiates `PCPlayer`: through
  `PhotonNetwork.Instantiate` while `PhotonNetwork.InRoom`, so every client in the room gets
  a copy, otherwise through `Resources.Load` and a local `Object.Instantiate`. It then points the local
  player's `LocalJournalController`'s journal at the `PCPlayer`'s `JournalController`
  (`Player.journal`), and a field of that journal back at the `LocalPCPlayer`.
- So in a lobby room there is one `JournalController` per player, on every client, each a PCPlayer's as
  in a contract. `JournalController.Awake` runs when its PCPlayer is instantiated: the local player's when
  the menu scene starts, another player's when that player's instantiation reaches this client.
- **The ghost list.** Every `JournalController` in `resources.assets` (the PCPlayer prefab's among them)
  names the one `GhostCollection` asset there, `All Ghost Collection`, as its `allGhostsData`; so do the
  lobby's `DifficultyUIManager.allGhostsData` (in `level2`) and a contract's
  `GhostController.allGhostsCollection` (read in Tanglewood's `level4`). The lobby journal's ghost list,
  guess buttons and pages are built by `RebuildGhosts` at `Awake` from the same collection the
  difficulty menu offers and a contract spawns from. The host does not rely on it being one object: it
  offers the custom ghosts to whatever collection each lobby journal lists.
- **The evidence page.** Its rows are the journal's own `evidenceButtons` (serialized in the
  PCPlayer prefab), the same rows as in a contract.
- **Sync.** `GoToPage(int)` sends `PageSync` with `RpcTarget` 3 (`AllBuffered`) while the
  journal's view is the local player's and `PhotonNetwork.InRoom`, with no check of the scene. The
  guess and evidence syncs send their RPC while `InRoom` and the view is
  the local player's. So lobby journals sync their pages and the players' guesses as a contract's do,
  and a custom page needs the same number on every client in the lobby too.

## Texts and languages

- `TextLocaliserUI` writes `TryGetLocalisedValue(key, out text, isUsingTextLanguageValue)`
  into its text, keeping what stands before the first `@`, then its suffix. It writes when it
  wakes, when its key is set (`TextLocaliserUI.set_Key`), when it is enabled after the language changed, and
  when the language changes (its `OnEnable` subscribes to the localisation's change event).
- `TryGetLocalisedValue` tail-jumps to `LookUp`, which looks the key up, one
  language at a time, in the player's language (`LocalisationSystem.get_language`) or, when its last argument is
  false, voice recognition's
  (`LocalisationSystem.get_voiceLanguage`), then in the language value 0, English; it writes the out slot through
  the GC write barrier. A key in neither gets a marker and the key, and false. `GetLocalisedValue` is this with the text returned. The overload `GetLocalisedValue(language, key)`
  ignores its language and looks in the player's. The host hooks `TryGetLocalisedValue`, which both
  public calls reach, once for all its content (`host/content_localised.h`: the host's hook table takes
  one detour per method, so its sides share it), and reads English by asking `LookUp` for English.
- `GetLanguageAbbreviationText(int)` is the game's two-letter abbreviation of a language
  ("EN"); regional variants share one. Content names a text's languages by it, in lower case.
  `GetNextOrPreviousLanguage` steps through the language values, staying put past either
  end, which gives the game's languages.
- A ghost's name key is `Journal_<GhostType name>Title` (`GhostData.ghostNameLocalisationKey`); its page
  text key is `Journal2_<GhostType name>`. The localisation table in `resources.assets` gives the
  English page text as the description, then `Strength: ...` and `Weakness: ...` paragraphs, separated by
  a line holding a space (`"\n \n "`); some ghosts have no strength paragraph (the Spirit:
  `"\n \n \n Weakness: ..."`). Each language writes its own labels. The host takes the separator and the
  two labels most game pages agree on (two at least, no tie) in the language asked, else in English;
  with no agreement it gives a custom page no text, and logs that once.

## The evidence page

The evidence spread's evidence page holds a row per evidence, each an `EvidenceButton` whose marks the
evidence sync controller (`JournalEvidenceSyncController`, the same generic base as the guess sync)
sends by evidence number, and whose state drives the evidence filter (`SetGhostTypes`). [Evidence](
evidence.md) describes the rows, their keys, the filter and how the host adds a custom evidence's row.

## Opening and closing a book

A player's own open or close is `JournalController.OpenCloseJournal`, which flips `JournalController.open`
and sends the networked state change, `JournalController.StateChangeSync`, an RPC every client runs. For a
book that is not this client's own, StateChangeSync runs `JournalRigController.OpenCloseJournalNetworked`
(which copies `JournalController.open` into `JournalRigController.opened`, puts the rig and its
`JournalRigController.allTabsObjects` on the open layer or on `JournalRigController.closedLayer`, and sets
the book Animator's open bool) and `JournalRigController.UpdateJournalTabs`; on the pause menu it selects the
first of `JournalController.pauseMenuTabs` through `JournalTab.button` and `JournalRigController.SelectTab`.
Then it runs `JournalController.ShowCanvas`, the canvas step: the canvas and its contents follow the book,
the pages' `JournalController.canvasRaycaster` is switched with them only for the client's own journal,
and the journal's pages or the pause menu show. Last it stops `JournalController.openSource` and plays
`JournalController.openClip` or `JournalController.closeClip`. The pause menu's player list,
`JournalController.playersList`, fills only while `PlayersList.isLocal` is set (`PlayersList.RefreshUI`).

The Playable Ghost mod runs these steps itself for the journal its ghost player keeps on the parked body
(mods/playable_ghost/journal.cpp), without the parts that need an investigator: the hands' animation and
hand points. While it keeps it, it sets each of the book's PhotonViews to `ViewSynchronization.Off` and
clears `PhotonView.isMine`, every frame, so its page turns stay on that client: each send the book makes, a
page turn or a mark, checks `PhotonView.get_IsMine` first. It puts both back as it gives the book back.

## How this was read

How these facts were read from the game, what the loader does with them and how the entries are found are in [the maintainers' page](../internals/journal.md).
