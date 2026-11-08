#pragma once

// The cursed possessions and the summoning circle: each one's networked use, the RPC every client runs
// when an investigator uses it. Their names are real. Notes read from decompiled code are from build
// 25811160.

#include "Equipment.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The Ouija board (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc OuijaBoard{.key = "OuijaBoard", .name = "OuijaBoard",
                                               .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* OuijaBoard_PlayMessageSequence_params[] = {"string", "PhotonMessageInfo"};

/// The board spells out its answer, a string, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc OuijaBoard_PlayMessageSequence{
    .key = "OuijaBoard.PlayMessageSequence", .kind = pm::sdk::Kind::Method, .owner = "OuijaBoard",
    .name = "PlayMessageSequence", .params = OuijaBoard_PlayMessageSequence_params};

/// The haunted mirror (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HauntedMirror{.key = "HauntedMirror", .name = "HauntedMirror",
                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// The mirror is used (true) or put away, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc HauntedMirror_UseNetworked{
    .key = "HauntedMirror.UseNetworked", .kind = pm::sdk::Kind::Method, .owner = "HauntedMirror",
    .name = "UseNetworked", .params = Equipment_bool_info};

/// The music box (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MusicBox{.key = "MusicBox", .name = "MusicBox", .assembly = "Assembly-CSharp",
                                             .ns = ""};

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
                                               .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* TarotCards_ChangeState_params[] = {"int", "bool", "PhotonMessageInfo"};

/// A card is drawn, its number first, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc TarotCards_ChangeState{.key = "TarotCards.ChangeState",
                                                           .kind = pm::sdk::Kind::Method, .owner = "TarotCards",
                                                           .name = "ChangeState",
                                                           .params = TarotCards_ChangeState_params};

/// The voodoo doll (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc VoodooDoll{.key = "VoodooDoll", .name = "VoodooDoll",
                                               .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* VoodooDoll_NetworkedUse_params[] = {"int", "PhotonMessageInfo"};

/// A pin is pushed, its number first, on every client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc VoodooDoll_NetworkedUse{.key = "VoodooDoll.NetworkedUse",
                                                            .kind = pm::sdk::Kind::Method, .owner = "VoodooDoll",
                                                            .name = "NetworkedUse",
                                                            .params = VoodooDoll_NetworkedUse_params};

/// The monkey paw (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MonkeyPaw{.key = "MonkeyPaw", .name = "MonkeyPaw",
                                              .assembly = "Assembly-CSharp", .ns = ""};

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
                                                    .assembly = "Assembly-CSharp", .ns = ""};

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
  static constexpr pm::api::Method<pm::map::OuijaBoard_PlayMessageSequence, void(void*, void*, void*)>
      PlayMessageSequence{};
};

/// The haunted mirror.
struct HauntedMirror {
  /// UseNetworked(mirror, on, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::HauntedMirror_UseNetworked, void(void*, bool, void*)> UseNetworked{};
};

/// The music box. `box` below is one.
struct MusicBox {
  /// UseNetworked(box, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::MusicBox_UseNetworked, void(void*, bool, void*)> UseNetworked{};
  /// Its tween, a DOTween Sequence, or null: tween.get(box). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MusicBox_tween, void*> tween{};
};

/// The tarot cards.
struct TarotCards {
  /// ChangeState(cards, card, flag, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TarotCards_ChangeState, void(void*, int32_t, bool, void*)> ChangeState{};
  /// HighPriestess(cards, &info), an RPC: the High Priestess card's revive. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::TarotCards_HighPriestess, void(void*, void*)> HighPriestess{};
};

/// The voodoo doll.
struct VoodooDoll {
  /// NetworkedUse(doll, pin, &info). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VoodooDoll_NetworkedUse, void(void*, int32_t, void*)> NetworkedUse{};
};

/// The monkey paw.
struct MonkeyPaw {
  /// MakeWishNetworked(paw, wish, a, text, &info): the wish's IEnumerator. A mod hooks it. Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::MonkeyPaw_MakeWishNetworked, void*(void*, int32_t, int32_t, void*, void*)>
      MakeWishNetworked{};
  /// ReviveWish(paw, wisher): the revive wish, `wisher` a Photon player. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::MonkeyPaw_ReviveWish, void(void*, void*)> ReviveWish{};
};

/// A summoning circle.
struct SummoningCircle {
  /// The type: type_object() for Object.FindObjectsOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::SummoningCircle> type{};
};

/// @}
}  // namespace phasmo
