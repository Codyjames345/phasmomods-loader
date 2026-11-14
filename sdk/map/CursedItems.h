#pragma once

// The cursed possessions, the summoning circle and the jackalope (a seasonal item): each one's networked use, the RPC every
// client runs when an investigator uses it, and the methods by which each starts the ghost's hunt or calls it
// (sdk/systems/ghost-ai.md, "Every route to a state change"). Their names are real. Notes read from decompiled code are
// from build 25811160.

#include "Equipment.h"
#include "Photon.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The Ouija board (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc OuijaBoard{.key = "OuijaBoard", .name = "OuijaBoard",
                                               .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

inline constexpr const char* OuijaBoard_PlayMessageSequence_params[] = {"string", "PhotonMessageInfo"};

/// The board spells out its answer, a string, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc OuijaBoard_PlayMessageSequence{
    .key = "OuijaBoard.PlayMessageSequence", .kind = pm::sdk::Kind::Method, .owner = "OuijaBoard",
    .name = "PlayMessageSequence", .params = OuijaBoard_PlayMessageSequence_params};

/// The haunted mirror (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HauntedMirror{.key = "HauntedMirror", .name = "HauntedMirror",
                                                  .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

/// The mirror is used (true) or put away, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HauntedMirror_UseNetworked{
    .key = "HauntedMirror.UseNetworked", .kind = pm::sdk::Kind::Method, .owner = "HauntedMirror",
    .name = "UseNetworked", .params = Equipment_bool_info};

/// The music box (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MusicBox{.key = "MusicBox", .name = "MusicBox", .assembly = "Assembly-CSharp",
                                             .ns = "", .base = &Equipment};

/// The box is used, on every client. A use the game accepts starts a new tween (tween), which a use it
/// refuses does not. Inferred: from its name and Playable Ghost's use since it was built.
inline constexpr pm::sdk::EntryDesc MusicBox_UseNetworked{.key = "MusicBox.UseNetworked",
                                                          .kind = pm::sdk::Kind::Method, .owner = "MusicBox",
                                                          .name = "UseNetworked", .params = Equipment_bool_info};

/// The box's tween, a DOTween Sequence: MusicBox's one Sequence field, replaced by each accepted use.
/// Inferred: from the class dump and Playable Ghost's use since it was built.
inline constexpr pm::sdk::EntryDesc MusicBox_tween{.key = "MusicBox.tween", .kind = pm::sdk::Kind::Field,
                                                   .owner = "MusicBox", .type = "Sequence"};

/// The tarot cards (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc TarotCards{.key = "TarotCards", .name = "TarotCards",
                                               .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

inline constexpr const char* TarotCards_ChangeState_params[] = {"int", "bool", "PhotonMessageInfo"};

/// A card is drawn, its number first, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc TarotCards_ChangeState{.key = "TarotCards.ChangeState",
                                                           .kind = pm::sdk::Kind::Method, .owner = "TarotCards",
                                                           .name = "ChangeState",
                                                           .params = TarotCards_ChangeState_params};

/// The voodoo doll (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc VoodooDoll{.key = "VoodooDoll", .name = "VoodooDoll",
                                               .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

inline constexpr const char* VoodooDoll_NetworkedUse_params[] = {"int", "PhotonMessageInfo"};

/// A pin is pushed, its number first, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc VoodooDoll_NetworkedUse{.key = "VoodooDoll.NetworkedUse",
                                                            .kind = pm::sdk::Kind::Method, .owner = "VoodooDoll",
                                                            .name = "NetworkedUse",
                                                            .params = VoodooDoll_NetworkedUse_params};

/// The monkey paw (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MonkeyPaw{.key = "MonkeyPaw", .name = "MonkeyPaw",
                                              .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

inline constexpr const char* MonkeyPaw_MakeWishNetworked_params[] = {"int", "int", "string", "PhotonMessageInfo"};

/// A wish is made, on every client: it returns the wish's coroutine, an IEnumerator. Inferred: from its
/// name.
inline constexpr pm::sdk::EntryDesc MonkeyPaw_MakeWishNetworked{
    .key = "MonkeyPaw.MakeWishNetworked", .kind = pm::sdk::Kind::Method, .owner = "MonkeyPaw",
    .name = "MakeWishNetworked", .params = MonkeyPaw_MakeWishNetworked_params};

inline constexpr const char* MonkeyPaw_ReviveWish_params[] = {"Player"};

// Of the paw's void methods that take a Player, only the revive wish revives anyone. Its Player is Photon's (the
// wisher), which the shape names only by its name: the model cannot tell Photon's Player from the game's.
inline constexpr pm::sdk::Fact MonkeyPaw_ReviveWish_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "Player.RevivePlayer"}};

/// The revive wish, given the wisher's Photon player; the wish's coroutine runs it on every client once the
/// wish is accepted and its use spent. The coroutine accepts it only while GameController.AmountOfDeadPlayers
/// is above 0 and the paw's revive is unused; otherwise the paw twitches and the use is kept. The method
/// walks Network.playerSpots for a dead player (Player.isDead) it may revive; with none, or with nobody else in
/// the room, the paw twitches and the use stays spent. Otherwise it marks the paw's revive used, and on the
/// master revives the first dead player who is not the wisher (Player.RevivePlayer with false), then rolls
/// 0 or 1 and on 1 kills the wisher (Player.BeginDeathSequence) if they are neither dead nor dying. Name
/// obfuscated. Decompiled.
inline constexpr pm::sdk::EntryDesc MonkeyPaw_ReviveWish{
    .key = "MonkeyPaw.ReviveWish", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "MonkeyPaw", .type = "void", .params = MonkeyPaw_ReviveWish_params,
    .facts = MonkeyPaw_ReviveWish_facts};

// Of the paw's void methods that take a Player, only this wish unlocks a door (Door.UnlockDoor).
inline constexpr pm::sdk::Fact MonkeyPaw_OpenExitsWish_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "Door.UnlockDoor"}};

/// The wish that opens every exit for good, given the wisher's Photon player; the wish's coroutine runs it on
/// every client, once, on its own once-only flag. For each door of LevelController.huntingDoors it clears the
/// door's timed-lock flag (Door.LockDoorForTime's), on the master calls Door.UnlockDoor(true) (the unlock's
/// click for everyone), on the client that owns the door's PhotonView calls Door.ToggleDoorLock(true) (the
/// door back on, Door.grabbable set on every client), and calls Door.ForceToggleHuntCollider(false). It
/// checks no hunt and sets no timer. Name obfuscated. Decompiled.
inline constexpr pm::sdk::EntryDesc MonkeyPaw_OpenExitsWish{
    .key = "MonkeyPaw.OpenExitsWish", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "MonkeyPaw", .type = "void", .params = MonkeyPaw_ReviveWish_params,
    .facts = MonkeyPaw_OpenExitsWish_facts};

inline constexpr const char* TarotCards_HighPriestess_params[] = {"PhotonMessageInfo"};

/// The High Priestess card, an RPC: it first checks that the sender owns the cards' view, with the report on
/// (sdk/systems/photon.md). On the master it walks Network.playerSpots for the first player marked dead
/// (Player.isDead) and not dying, and starts a coroutine that revives them (Player.RevivePlayer with false);
/// with no such player it sends `SetHasDrawnHighPriestess` to every client, which sets a flag of the
/// GameController; while it is set the death animation no longer refuses a kill of a player already marked
/// dead (what else reads the flag is not traced). Decompiled.
inline constexpr pm::sdk::EntryDesc TarotCards_HighPriestess{
    .key = "TarotCards.HighPriestess", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "TarotCards", .name = "HighPriestess", .params = TarotCards_HighPriestess_params};

/// A summoning circle (Assembly-CSharp, global namespace): lit, it teleports the ghost onto itself.
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc SummoningCircle{.key = "SummoningCircle", .name = "SummoningCircle",
                                                    .assembly = "Assembly-CSharp", .ns = "", .base = &Equipment};

/// The mirror breaks: while the ghost is not hunting, on the master, it counts the break in LevelStats, then a hunt,
/// forced (GhostAI.ChangeState). It sets a field of DifficultySettings before the change and calls another of its
/// methods after it. Protected and
/// virtual. A mod hooks it; refusing the change in GhostAI.ChangeState leaves the rest of it to run. Decompiled.
inline constexpr pm::sdk::EntryDesc HauntedMirror_BreakItem{
    .key = "HauntedMirror.BreakItem", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "HauntedMirror", .name = "BreakItem"};

/// The board breaks: on the master, while the ghost is not hunting, a hunt, forced (GhostAI.ChangeState), between
/// the same two DifficultySettings steps as HauntedMirror.BreakItem's. Private. A mod hooks it. Decompiled.
inline constexpr pm::sdk::EntryDesc OuijaBoard_BreakBoard{
    .key = "OuijaBoard.BreakBoard", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "OuijaBoard", .name = "BreakBoard"};

/// The box breaks: when its bool is true and the ghost is not hunting, on the master,
/// DifficultySettings.AddHuntDuration(20), then a hunt, forced (GhostAI.ChangeState). Private. A mod hooks it. Decompiled.
inline constexpr pm::sdk::EntryDesc MusicBox_BreakMusicBox{
    .key = "MusicBox.BreakMusicBox", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "MusicBox", .name = "BreakMusicBox"};

/// An RPC: after its check of the sender and while a flag of the box is set, the ghost is called to the box,
/// GhostAI.ChangeState to GhostState.musicBox with the box as the change's object, not forced. Never call it or send it (sdk/systems/photon.md); a mod hooks it. Decompiled.
inline constexpr pm::sdk::EntryDesc MusicBox_EnterMusicBoxState{
    .key = "MusicBox.EnterMusicBoxState", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "MusicBox", .name = "EnterMusicBoxState"};

/// An RPC: the doll's hunt. While the doll has uses left, a flag of it is set, the ghost is not hunting and the
/// sender passes its check, it spends a use, then on the master DifficultySettings.AddHuntDuration(20) and a hunt,
/// forced (GhostAI.ChangeState). Never call it or send it (sdk/systems/photon.md); a mod hooks it. Decompiled.
inline constexpr pm::sdk::EntryDesc VoodooDoll_StartHunt{
    .key = "VoodooDoll.StartHunt", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "VoodooDoll", .name = "StartHunt"};

/// The jackalope, a seasonal item (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc Jackalope{.key = "Jackalope", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "Jackalope", .assembly = "Assembly-CSharp", .ns = "",
                                              .base = &MonoBehaviourPun};

/// An RPC, its int an actor number: the jackalope turns to that player and plays its sound; then on the master it
/// clears GhostAI.postHuntCooldown, sets another bool of the GhostAI, and starts a hunt, not forced
/// (GhostAI.ChangeState). Refusing the change leaves those two writes made. Never call it or send it
/// (sdk/systems/photon.md); a mod hooks it. Decompiled.
inline constexpr pm::sdk::EntryDesc Jackalope_TriggerHunt{
    .key = "Jackalope.TriggerHunt", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Jackalope", .name = "TriggerHunt"};

/// The Hanged Man's kill, given the card's user, this client's Photon player. On the master it kills them
/// (Player.BeginDeathSequence, through Network.GetPlayerFromPhotonPlayer); elsewhere it asks the master by RPC. Decompiled.
inline constexpr pm::sdk::EntryDesc TarotCards_HangedMan{
    .key = "TarotCards.HangedMan", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "TarotCards", .name = "HangedMan"};

}  // namespace pm::map

/// @file
/// @brief The game API for the cursed possessions and the summoning circle (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup equipment
/// @{

/// The Ouija board.
struct OuijaBoard {
  /// PlayMessageSequence(board, message, &info): `message` a System.String. A mod hooks it. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::OuijaBoard_PlayMessageSequence, void(pm::api::Ref<pm::map::OuijaBoard>, pm::api::Object, pm::api::Object)>
      PlayMessageSequence{};
  /// BreakBoard(board): the board breaks and, on the master, the ghost hunts. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::OuijaBoard_BreakBoard, void(pm::api::Ref<pm::map::OuijaBoard>)> BreakBoard{};
};

/// The haunted mirror.
struct HauntedMirror {
  /// UseNetworked(mirror, on, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HauntedMirror_UseNetworked, void(pm::api::Ref<pm::map::HauntedMirror>, bool, pm::api::Object)> UseNetworked{};
  /// BreakItem(mirror): the mirror breaks and, on the master, the ghost hunts. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HauntedMirror_BreakItem, void(pm::api::Ref<pm::map::HauntedMirror>)> BreakItem{};
};

/// The music box. `box` below is one.
struct MusicBox {
  /// UseNetworked(box, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MusicBox_UseNetworked, void(pm::api::Ref<pm::map::MusicBox>, bool, pm::api::Object)> UseNetworked{};
  /// Its tween, a DOTween Sequence, or null: tween.get(box). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MusicBox_tween, pm::api::Object> tween{};
  /// BreakMusicBox(box, flag): the box breaks and, on the master, the ghost hunts. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::MusicBox_BreakMusicBox, void(pm::api::Ref<pm::map::MusicBox>, bool)> BreakMusicBox{};
  /// EnterMusicBoxState(box, &info), an RPC: the ghost is called to the box. A mod hooks it; never calls it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MusicBox_EnterMusicBoxState, void(pm::api::Ref<pm::map::MusicBox>, pm::api::Object)> EnterMusicBoxState{};
};

/// The tarot cards.
struct TarotCards {
  /// ChangeState(cards, card, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TarotCards_ChangeState, void(pm::api::Ref<pm::map::TarotCards>, int32_t, bool, pm::api::Object)> ChangeState{};
  /// HighPriestess(cards, &info), an RPC: the High Priestess card's revive. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::TarotCards_HighPriestess, void(pm::api::Ref<pm::map::TarotCards>, pm::api::Object)> HighPriestess{};
};

/// The voodoo doll.
struct VoodooDoll {
  /// NetworkedUse(doll, pin, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VoodooDoll_NetworkedUse, void(pm::api::Ref<pm::map::VoodooDoll>, int32_t, pm::api::Object)> NetworkedUse{};
  /// StartHunt(doll, &info), an RPC: the doll's hunt. A mod hooks it; never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VoodooDoll_StartHunt, void(pm::api::Ref<pm::map::VoodooDoll>, pm::api::Object)> StartHunt{};
};

/// The monkey paw.
struct MonkeyPaw {
  /// MakeWishNetworked(paw, wish, a, text, &info): the wish's IEnumerator. A mod hooks it. Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::MonkeyPaw_MakeWishNetworked, pm::api::Object(pm::api::Ref<pm::map::MonkeyPaw>, int32_t, int32_t, pm::api::Object, pm::api::Object)>
      MakeWishNetworked{};
  /// ReviveWish(paw, wisher): the revive wish, `wisher` a Photon player. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::MonkeyPaw_ReviveWish, void(pm::api::Ref<pm::map::MonkeyPaw>, pm::api::Object)> ReviveWish{};
  /// OpenExitsWish(paw, wisher): the wish that opens every exit, `wisher` a Photon player. A mod hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MonkeyPaw_OpenExitsWish, void(pm::api::Ref<pm::map::MonkeyPaw>, pm::api::Object)> OpenExitsWish{};
};

/// The jackalope, a seasonal item.
struct Jackalope {
  /// The type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Jackalope> type{};
  /// TriggerHunt(jackalope, actor, &info), an RPC: the jackalope's hunt. A mod hooks it; never calls it. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::Jackalope_TriggerHunt, void(pm::api::Ref<pm::map::Jackalope>, int32_t, pm::api::Object)> TriggerHunt{};
};

/// A summoning circle.
struct SummoningCircle {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SummoningCircle> type{};
};

/// @}
}  // namespace phasmo
