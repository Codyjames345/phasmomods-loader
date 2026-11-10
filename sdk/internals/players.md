# Players: death and revival: how it was read and how the loader uses it

For maintainers and for anyone expanding the SDK. A mod author needs [Players: death and revival](../systems/players.md), and this page only to look under it. How to read code like this: [reverse engineering](reverse-engineering.md).

## A player's position in the list

Much of the UI that shows one player per slot picks the slot by that player's position in `Network.playerSpots`. Each
slot has one colour: a piece of UI carries an `UpdatePlayerColor.playerID`, and takes the colour at that place in
Network's array of player colours. So a player's colour is their position, and it moves when they do. Taking any spot out
moves every later player down one place. Photon's departure does this too (`Network.OnPlayerLeftRoom`).

### Readers in a contract

| Reader | How it reads the position | What the position picks |
| --- | --- | --- |
| `NetworkThreeStateButton.SetOtherPlayerButtonState` | `List.IndexOf` of the spot | The mark (P1 to P4, `NetworkThreeStateButton.otherPlayerButtons`) that shows another player's choice on each evidence and ghost button |
| `NetworkThreeStateButton.ValidatePlayerSelection` | `Network.GetIndexOfPlayer` | The mark it clears for a dead player. Its callers are obfuscated methods of a generic class, so whether it runs was not proven |
| `JournalMedia.MediaVote` | `Network.GetIndexOfPlayer` | The tick, cross and colour of a player's vote on removing a photo, video or sound |
| `Wristwatch.UpdateSanity`, `Wristwatch.UpdateBarVisibility` | The loop's own count over the list | The sanity bar that shows each player; a bar past the end of the list is not written, so it keeps its last value |
| `ProfileCard.AssignPlayer`, from `PlayersList.RefreshUI`, each time the journal opens | `Network.GetIndexOfPlayer` | The pause menu card's photo, the local file `IDPhoto{position}.png` |

`HeartRateData.UpdateUI`, the truck's sanity monitor, builds one row per spot in list order. Whether its rows are coloured
by position was not traced.

### Readers outside a contract

`GhostOS.UpdatePlayers` and `GhostOS.UpdatePlayerColor` (the lobby's computer), `ProfileCardPreview.UpdateCosmeticData` and a
callback of `PlayerCustomizationScreenManager.ResetSelections` (the customisation screen) run from lobby code. That
they cannot run in a level comes from their callers, and was not checked against the scene files.

### The body-type event

`PlayerCharacter.SetPlayerBodyType` listens on an event of Network that carries a position (its field is not mapped).
Only three things raise it:

- the callback `PlayerCosmetics.SetPlayerCosmeticsNetworked` hands to the outfit's load, an RPC only
  `PlayerCustomizationScreenManager.SaveOutfit` sends from live code;
- the callback `ServerManager.SetPlayerInformation` hands to it, an RPC sent by `ServerManager`'s `RequestPlayerInfo` and one
  of its coroutines;
- a private method of `Player` that sets its own character the same way. Its only reference is a coroutine whose starter has
  no caller, no delegate and no name in the string literals, so it never runs in this build.

A position in the list changes only the character whose owner holds that spot. **-1 changes every listening character,
to this client's own body type.** `Network.GetIndexOfPlayer` answers -1 for a spot held out of the list while its lookup
still finds it, so raising this event for a held spot would change investigators. Both senders are lobby code, and so is
the game's own use of -1 (the customisation screen's obfuscated methods). Playable Ghost still ignores -1 at
`PlayerCharacter.SetPlayerBodyType` while it holds a spot, so a sender that was missed cannot change a model.

### The other 41 call sites

`Network.GetIndexOfPlayer` has 52 calling methods. 11 are named above or are their helpers (`ProfileCard`'s photo step and the
button's clear among them). The other 41 are obfuscated copies, in `JournalMedia`, `GhostOS`, `NetworkThreeStateButton`,
`ProfileCard`, `ProfileCardPreview` and `PlayerCosmetics`. No named method calls them. The ones with no direct caller were
also checked for delegates and reflection; `GhostOS`'s are referenced only from its own obfuscated methods. A virtual call
through an obfuscated base method cannot be ruled out this way.

### When a spot can come out

A spot cannot come out before this client's `GameController.allPlayersLoaded`. `Network.SetSpotPlayer`'s coroutine waits
until the list holds as many spots as the room has Photon players, so a spot taken out earlier stalls the setup of every
player still loading. Once the flag is set, every spot has its Player, so that wait is over.

Nothing proves that a mark cannot land first. `LocalJournalController.OpenCloseJournal` checks a flag of the player's menu and a
flag of their `Player`, and neither was shown to be a load check. Each client's load wait also ends at its own moment, so a
press from a client that has loaded could reach one still waiting. Such a mark stays in the old slot.

### The shift while a spot is out (known behaviour, provisional)

While Playable Ghost holds the ghost player's spot out, each investigator after them in the list:

- shows their evidence marks, votes and sanity bar in the previous slot, with that slot's colour, which is not the colour on
  their lobby card;
- leaves the old last sanity bar unwritten, at its last value;
- has a pause card that can read another position's photo.

A mark or vote that reached a client before it took the spot out stays where it landed. The user accepted this for now
and judges it in the first in-game test (2026-11-01). It is not final.

## The body, the voice's loudness and the input actions (2026-11-10)

Read for the Emotes mod ([players](../systems/players.md#the-body), [voice](../systems/voice.md#how-loud-a-player-talks),
[menus](../systems/menus.md#key-bindings), [media](../systems/media.md#a-camera-of-a-mods-own-and-screen-effects)).

- `PlayerAudio.IsTalkingLoudly`, `PlayerAudio.Update` and `PlayerAudio.GetPlayerVoiceAudioSource` were decompiled. The
  0.4 taken from the Vivox energy is a float literal in the game's read-only data, read from a copy of the binary.
  `PlayerAudio.loudness` is found as PlayerAudio's one public float.
- `Player.SetPlayerModelLayer`, `NetworkedPropGrab.GrabPropRPC`, `PCPropGrab.ParentItemOnLocalPlayer` and the camera
  slot's enabler were decompiled. `Player.animator`, `Player.renderers` and `Player.characterAnimations` are found as
  Player's one field of each type.
- The input actions and their bindings are not in code: they are serialized in `resources.assets`. Their names, ids and
  default paths were read as the length-prefixed strings around the action names; the XR Interaction actions' paths were
  counted across the game's data files. Nothing there is a map entry: a mod reads the actions at run time by name.
- The Playables, `Camera.CopyFrom`, `AnimationClip.get_length` and `Motion.get_isLooping` are icalls whose exact names
  were read from `GameAssembly.dll`'s strings; they are checked at startup only. `VolumeParameter<float>.set_value` and
  `VolumeParameter<DepthOfFieldMode>.set_value` were mapped first, as inflated entries checked at startup only: the
  offline check cannot tell whether a generic instance has code, and the running game found that neither does
  (2026-11-10), so Emotes stayed off. The parameters' own constructors replace them. An inflated entry's instance
  may have no code at all; prefer a non-generic member.

## Not covered

- A Player held in another object's field (a hunt's lists, a ghost's target, `LevelRoom.GetPlayersInRoom`, the truck's
  trigger) is outside the survey above. For a mod that parks a body, those still find it, alive.
- With a parked body alive to the lookups, `SoundController.GetPlayersAudioGroup` and Vivox's dead-player switch treat its
  player's voice as a living player's. Only the mod's own mic mute keeps it from being heard.
- The death-animation flag beside the dying flag, and the readers of the High Priestess's flag other than the death
  animation, are not traced.
