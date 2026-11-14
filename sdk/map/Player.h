#pragma once

// The investigators: the game's Player (one per investigator, networked), the Network that lists them
// by their PlayerSpot, and the voice recognition's phrase handler. Network and PlayerSpot keep their
// real names, and so do PlayerSpot's fields; Network's and Player's own fields are obfuscated, so they
// are found by shape and facts. Notes read from decompiled code are from build 25811160.

#include <bit>
#include <cstdint>

#include "Animator.h"
#include "Camera.h"
#include "ClosetZone.h"
#include "GhostSenses.h"
#include "Input.h"
#include "JournalController.h"
#include "LevelRoom.h"
#include "Photon.h"
#include "PlayerSanity.h"
#include "Props.h"
#include "Transform.h"
#include "UnityAudio.h"
#include "UnityCloth.h"
#include "UnityObject.h"
#include "UnityUI.h"
#include "UnityWorld.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The game's networking hub (Assembly-CSharp, global namespace), one per process. Decompiled.
inline constexpr pm::sdk::EntryDesc Network{.key = "Network", .confidence = pm::sdk::Confidence::Decompiled,
                                            .name = "Network", .assembly = "Assembly-CSharp", .ns = "",
                                            .base = &MonoBehaviourPun};

/// The one Network, from its static field. Static. LightSwitch.UseLightSwitch reads the local player through it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Network_get_Instance{
    .key = "Network.get_Instance", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "Network", .name = "get_Instance", .type = "@Network"};

/// The room's players, a List<PlayerSpot>, one spot each: Network's one list of them. Inferred: from
/// the field's type.
inline constexpr pm::sdk::EntryDesc Network_playerSpots{.key = "Network.playerSpots", .kind = pm::sdk::Kind::Field,
                                                        .owner = "Network",
                                                        .type = "List<@PlayerSpot>"};

/// One player of the room as Network keeps it, a class nested in Network. Inferred: from its fields'
/// names.
inline constexpr pm::sdk::EntryDesc PlayerSpot{.key = "PlayerSpot", .owner = "Network", .name = "PlayerSpot",
                                               .assembly = "Assembly-CSharp"};

/// The spot's Photon player (Photon.Realtime.Player), whose actor number names its client. Inferred:
/// from the field's name.
inline constexpr pm::sdk::EntryDesc PlayerSpot_photonPlayer{.key = "PlayerSpot.photonPlayer",
                                                            .kind = pm::sdk::Kind::Field, .owner = "PlayerSpot",
                                                            .name = "photonPlayer"};

/// The spot's investigator, the game's Player, or null before it spawns. Inferred: from the field's
/// name.
inline constexpr pm::sdk::EntryDesc PlayerSpot_player{.key = "PlayerSpot.player", .kind = pm::sdk::Kind::Field,
                                                      .owner = "PlayerSpot", .name = "player"};

/// Whether the player pressed Ready, a bool. ServerManager.NetworkedReady writes it, and ServerManager.UpdateUI clears it for
/// everyone as a player joins or leaves. A spot exists only some while after the player is in the room. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSpot_playerReady{.key = "PlayerSpot.playerReady", .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "PlayerSpot", .name = "playerReady"};

/// An investigator (Assembly-CSharp, global namespace), an abstract MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc Player{.key = "Player", .confidence = pm::sdk::Confidence::Decompiled,
                                           .name = "Player", .assembly = "Assembly-CSharp", .ns = "",
                                           .base = &MonoBehaviourPun};

// UseLightSwitch reads one bool of the local Player, through Network: it refuses a dead player's use.
// It is another type's method, so the fact matches that offset through any register.
inline constexpr pm::sdk::Fact Player_isDead_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LightSwitch.UseLightSwitch"}};

/// Whether the investigator is dead. UseLightSwitch returns at once for a local player with it set
/// (unless its second argument is set), Player.CompleteDeathSequenceNetworked tests it on the local player,
/// and Player.FinishPlayerRevive acts only while it is clear. Which rules pass over a dead player and which act on one: sdk/systems/players.md. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_isDead{.key = "Player.isDead", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                  .type = "bool", .facts = Player_isDead_facts};

// The death start's RPC marks the victim dying on every client; of Player's bools it stores only this one.
inline constexpr pm::sdk::Fact Player_dying_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "Player.NotifyOfDeathStart"}};

/// Whether the investigator is dying: a kill has started (Player.NotifyOfDeathStart sets it on every client) and has
/// not finished. Player.FinishPlayerRevive clears it with isDead. The Revive RPC accepts a player dead or dying, and
/// the Monkey Paw's wish kills only a wisher neither dead nor dying (sdk/systems/players.md). Decompiled.
inline constexpr pm::sdk::EntryDesc Player_dying{.key = "Player.dying", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                 .type = "bool", .facts = Player_dying_facts};

inline constexpr const char* DeathType_values[] = {"HangedMan", "SnappedNeck"};

/// The kinds of death a kill gives (Player.BeginDeathSequence's argument), an enum (name obfuscated, value names
/// real): HangedMan and SnappedNeck. The victim's body plays the death animation of its kind
/// (PlayerCharacterAnimations keeps one override controller per kind) and the body left behind lies as that kind.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DeathType{.key = "DeathType", .confidence = pm::sdk::Confidence::Decompiled,
                                              .assembly = "Assembly-CSharp", .ns = "", .values = DeathType_values};

/// The Hanged Man tarot card's death. Decompiled: TarotCards.HangedMan passes it.
inline constexpr pm::sdk::EntryDesc DeathType_HangedMan{.key = "DeathType.HangedMan", .kind = pm::sdk::Kind::EnumValue,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "DeathType", .name = "HangedMan"};

/// A snapped neck: the death a hunt's kill gives, and the Monkey Paw's revive wish's. Decompiled.
inline constexpr pm::sdk::EntryDesc DeathType_SnappedNeck{.key = "DeathType.SnappedNeck",
                                                          .kind = pm::sdk::Kind::EnumValue,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "DeathType", .name = "SnappedNeck"};

/// The player's body object, a GameObject: Player's one public GameObject field.
/// The hunt's voice test measures a talking player's distance to it. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_body{.key = "Player.body", .kind = pm::sdk::Kind::Field,
                                                .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                .type = "GameObject",
                                                .access = pm::sdk::Visibility::Public};

// Of the player's bools the hunt's voice test reads only this one.
inline constexpr pm::sdk::Fact Player_wished_facts[] = {{.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.Hears"}};

/// Whether a Monkey Paw wish marked the player: a public bool the wish's handler sets on every client,
/// from the wish's networked coroutine, before its master check. While it is set the hunt's voice test
/// (GhostHuntState.Hears) hears the player from 1000 m instead of its range for the type, and on any
/// floor; the electronics test (GhostHuntState.SensesGear) widens its range the same way. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_wished{.key = "Player.wished", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                  .type = "bool", .facts = Player_wished_facts,
                                                  .access = pm::sdk::Visibility::Public};

/// The player's stamina and movement: Player's one NetworkedStamina field.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Player_stamina{.key = "Player.stamina", .kind = pm::sdk::Kind::Field,
                                                   .owner = "Player", .type = "NetworkedStamina"};

/// The closet the investigator hides in, a ClosetZone, or null: Player's one field of that type. The
/// closet's trigger sets it as a living player's body enters and clears it as the body leaves (ClosetZone's
/// OnTriggerEnter and OnTriggerExit, on each client its physics runs on), and the hunt's closet search,
/// which runs on the master, reads it of the player it targets. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_closet{.key = "Player.closet", .kind = pm::sdk::Kind::Field,
                                                  .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                  .type = "@ClosetZone"};

/// The room the investigator is in, a LevelRoom: Player's one field of that type, on every client. It is
/// the last room whose trigger the player's camera collider entered (LevelRoom.OnTriggerEnter sets it;
/// nothing else in the room code clears it), so a player beyond the map's exits is in
/// LevelController.outsideRoom (RoomKind.outsideMap) once they have entered its trigger. The hunt's tests
/// read it and throw for a player with none. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_currentRoom{.key = "Player.currentRoom", .kind = pm::sdk::Kind::Field,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "Player", .type = "@LevelRoom"};

/// The keys the investigator holds, a List<KeyInfo>: Player's one field of that type. Key.GrabbedKey adds
/// to the local player's list, on that player's client (no other adder was traced), and
/// Door.AttemptToUnlockDoor reads the local player's list: a door whose kind matches one of its keys
/// unlocks, outside a hunt. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_keys{.key = "Player.keys", .kind = pm::sdk::Kind::Field,
                                                .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                .type = "List<@KeyInfo>"};

/// The investigator's parts the ghost's sight looks for, a PlayerLOSTarget[]: Player's one field of
/// that type. The hunt's sight test passes when the ghost's vision sensor detects any of them,
/// and throws for a player without them. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_losTargets{.key = "Player.losTargets", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "Player",
                                                      .type = "PlayerLOSTarget[]"};

/// The player's sanity: Player's one PlayerSanity field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Player_sanity{.key = "Player.sanity", .kind = pm::sdk::Kind::Field, .owner = "Player",
                                                  .type = "@PlayerSanity"};

/// This client's player rig (Assembly-CSharp, global namespace), an abstract MonoBehaviour: the
/// controls, camera, hands and HUD of the local investigator. LocalPCPlayer is the PC one. Inferred: from
/// the class dump.
inline constexpr pm::sdk::EntryDesc LocalPlayer{.key = "LocalPlayer", .name = "LocalPlayer",
                                                .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// The local player on PC (Assembly-CSharp, global namespace), a LocalPlayer: Network's local player
/// component. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer{.key = "LocalPCPlayer", .name = "LocalPCPlayer",
                                                  .assembly = "Assembly-CSharp", .ns = "", .base = &LocalPlayer};

/// The local player's HUD canvas: LocalPCPlayer's one PCCanvas field.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_canvas{.key = "LocalPCPlayer.canvas", .kind = pm::sdk::Kind::Field,
                                                         .owner = "LocalPCPlayer",
                                                         .type = "@PCCanvas"};

/// The PC rig's movement controller, the local player's movement brain (Assembly-CSharp, global namespace), a
/// MonoBehaviour: it moves the local PC investigator from the Move input. Decompiled.
inline constexpr pm::sdk::EntryDesc FirstPersonController{.key = "FirstPersonController",
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .name = "FirstPersonController",
                                                          .assembly = "Assembly-CSharp", .ns = "",
                                                          .base = &MonoBehaviour};

/// The rig's movement controller: LocalPCPlayer's one FirstPersonController field. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_firstPersonController{
    .key = "LocalPCPlayer.firstPersonController", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPCPlayer", .type = "FirstPersonController"};

/// The Move input as the game last read it, a Vector2 (x right, y forward): FirstPersonController's one Vector2
/// field. FirstPersonController.Move, the Move action's callback, stores the action's value there, scaled down to
/// length 1 when longer, and zero when the action is cancelled or both axes are inside a small dead zone. So W alone
/// reads (0, 1) and up on the move stick reads its tilt, with the player's own rebinding applied. Local PC player
/// only; the VR rig moves without it. Decompiled.
inline constexpr pm::sdk::EntryDesc FirstPersonController_moveInput{
    .key = "FirstPersonController.moveInput", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "FirstPersonController", .type = "Vector2"};

/// The Move action's callback (an InputAction.CallbackContext): stores the input in FirstPersonController.moveInput
/// as described there. Decompiled.
inline constexpr pm::sdk::EntryDesc FirstPersonController_Move{.key = "FirstPersonController.Move",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "FirstPersonController", .name = "Move"};

/// The Run action's callback (an InputAction.CallbackContext): sets FirstPersonController.running. In the game's hold
/// mode it sets it while the action is in any phase but Canceled, so it holds while Run is held; in its toggle mode
/// (FirstPersonController.get_m_ToggleSprint) it flips it each time the action is Performed. Then it invokes one of the
/// controller's UnityEvents. Decompiled.
inline constexpr pm::sdk::EntryDesc FirstPersonController_OnRun{.key = "FirstPersonController.OnRun",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "FirstPersonController", .name = "OnRun"};

// OnRun stores one bool of the controller, and the speed update reads it through the rig's controller field.
inline constexpr pm::sdk::Fact FirstPersonController_running_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "FirstPersonController.OnRun"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "LocalPCPlayer.UpdatePlayerSpeed"}};

/// Whether the player asks to sprint: Run held, or toggled on in the game's toggle sprint mode
/// (FirstPersonController.OnRun sets it). Not the sprint itself: LocalPCPlayer.UpdatePlayerSpeed gives the sprint
/// speed only while this is set, sprinting is allowed and PlayerStamina.canSprint is set (pm::api::movement::sprinting
/// reads the three). On the player's own client only. Decompiled.
inline constexpr pm::sdk::EntryDesc FirstPersonController_running{
    .key = "FirstPersonController.running", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "FirstPersonController", .type = "bool", .facts = FirstPersonController_running_facts};

/// The local player's HUD (Assembly-CSharp, global namespace), a MonoBehaviour. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc PCCanvas{.key = "PCCanvas", .name = "PCCanvas", .assembly = "Assembly-CSharp",
                                             .ns = "", .base = &MonoBehaviour};

/// The HUD's crosshair, the dot the player aims with: PCCanvas's one Image field. Inferred: from the class
/// dump.
inline constexpr pm::sdk::EntryDesc PCCanvas_crosshair{.key = "PCCanvas.crosshair", .kind = pm::sdk::Kind::Field,
                                                       .owner = "PCCanvas", .type = "@Image"};

inline constexpr const char* PCCanvas_bool[] = {"bool"};

/// PausePlayerInPlace(paused): the game's hold for a full-screen overlay. It runs on the local player's HUD
/// canvas and reaches everything through that canvas's own LocalPlayer. The game calls it with true from
/// JournalController.OpenCloseJournal, from the options page it opens and from the voice recognition
/// overlay's own toggle, and with false when those close. Decompiled, with true it: calls
/// FirstPersonController.EnableMovement false (no walking and no looking); writes a zero to a float on the
/// investigator's character-animation component, reached through LocalPlayer.player; switches the HUD's
/// shown state; sets the crosshair's GameObject inactive; resets held item sway (PCItemSway.ResetSway);
/// switches the game HUD's CanvasGroup interactable off; and frees the mouse cursor. With false it does the
/// same in reverse and locks the cursor again, but first reads LocalPlayer.journalController: no journal
/// controller throws there, and one that says its own journal is open returns without releasing anything.
/// A piece missing after the movement switch (the investigator, its animation component, the crosshair, the
/// sway) throws a NullReferenceException with the movement already switched, so a mod should check those
/// before it asks. The call skips itself when the movement already matches the request, reading the two
/// flags EnableMovement writes. It never moves the player and never touches the game's clock. Ask once when
/// an overlay opens and release once when it closes, exactly as the journal toggle does; asking every frame
/// re-runs all of this whenever the game has put the player's state back.
inline constexpr pm::sdk::EntryDesc PCCanvas_PausePlayerInPlace{
    .key = "PCCanvas.PausePlayerInPlace", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PCCanvas", .name = "PausePlayerInPlace", .params = PCCanvas_bool};

inline constexpr const char* FirstPersonController_bool[] = {"bool"};

/// EnableMovement(on): the local player's walk-and-look switch. Decompiled: it writes its bool into two
/// flags of its own, and with false it also freezes the player's Rigidbody rotation; enabling clears the
/// flags but never unfreezes it. PCCanvas.PausePlayerInPlace reads those two flags to decide whether the
/// player is already held.
inline constexpr pm::sdk::EntryDesc FirstPersonController_EnableMovement{
    .key = "FirstPersonController.EnableMovement", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "FirstPersonController", .name = "EnableMovement",
    .params = FirstPersonController_bool};

/// The investigator's physical step on PC (Assembly-CSharp, global namespace), a MonoBehaviour beside the
/// FirstPersonController, which holds it. Decompiled: its FixedUpdate is the whole physical step. It copies its
/// transform's position to its Rigidbody, runs its passes that push the capsule out of what it overlaps and
/// slide it along what it meets, applies gravity (Physics.gravity scaled by its gravity multiplier) while it is
/// not grounded, and then writes the Rigidbody's position and rotation back to its transform and zeroes the
/// Rigidbody's velocities. So a write to its transform's position is where the next step starts (a teleport), and
/// while the behaviour is disabled nothing moves the investigator but such writes: no walls, floors or gravity. Its
/// capsule (a CapsuleCollider) moves against a layer mask of its own (PhysicsCharacterController.collisionMask).
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController{.key = "PhysicsCharacterController",
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .name = "PhysicsCharacterController",
                                                               .assembly = "Assembly-CSharp", .ns = "",
                                                               .base = &MonoBehaviour};

inline constexpr const char* PhysicsCharacterController_Move_params[] = {"Vector3"};

/// Move(displacement): the walk the controller is asked for this frame, in metres. Decompiled: it stores the
/// displacement divided by Time.deltaTime as the velocity the next FixedUpdate applies, and sets the flag that
/// step reads; it moves nothing itself. The FirstPersonController calls it from its movement each frame with
/// the walk or sprint the player's input gives (the rig's speed multiplier included), and the VR rig's
/// physics move provider calls it too. A mod hooks it to read or replace the frame's walk.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_Move{
    .key = "PhysicsCharacterController.Move", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhysicsCharacterController", .name = "Move",
    .params = PhysicsCharacterController_Move_params};

/// The controller's Rigidbody: the body its physical step moves. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_Rigidbody{
    .key = "PhysicsCharacterController.get_Rigidbody", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_Rigidbody", .type = "@Rigidbody"};

/// The controller's CapsuleCollider: the investigator's body in the physics world, what walls and floors meet.
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_CapsuleCollider{
    .key = "PhysicsCharacterController.get_CapsuleCollider", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_CapsuleCollider", .type = "@CapsuleCollider"};

/// The player's journal: Player's one JournalController field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Player_journal{.key = "Player.journal", .kind = pm::sdk::Kind::Field, .owner = "Player",
                                                   .type = "@JournalController"};

/// What an investigator carries, on every client (Assembly-CSharp, global namespace), a MonoBehaviourPun
/// beside each Player. Its RPC GrabPropRPC, which every client runs for a grab, freezes the item's Rigidbody and
/// adds it to `inventory` (SyncInventoryItem); DropNetworked takes it out again. Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkedPropGrab{.key = "NetworkedPropGrab",
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .name = "NetworkedPropGrab", .assembly = "Assembly-CSharp",
                                                      .ns = "", .base = &MonoBehaviourPun};

/// The items the investigator carries, a List<PhotonObjectInteract>: in a hand or put away, every
/// item they picked up and have not dropped (NetworkedPropGrab's one list of them). The grab RPC adds
/// to it and DropNetworked removes from it on every client, so every client knows who carries what.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkedPropGrab_inventory{
    .key = "NetworkedPropGrab.inventory", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "NetworkedPropGrab", .type = "List<@PhotonObjectInteract>"};

/// The investigator's NetworkedPropGrab: Player's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_propGrab{.key = "Player.propGrab", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "Player",
                                                    .type = "@NetworkedPropGrab"};

/// A player's own voice and sounds (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerAudio{.key = "PlayerAudio", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "PlayerAudio", .assembly = "Assembly-CSharp", .ns = "",
                                                .base = &MonoBehaviour};

inline constexpr const char* PlayerAudio_float[] = {"float"};

/// Whether the player is talking above a loudness (0.1 by default), or talks on a walkie-talkie. When it
/// answers true it also clears the loudness it read, so the next asker needs new sound. The hunt's voice
/// test (GhostHuntState.Hears) calls it for a player within its hearing, and LevelRoom's
/// ArePlayersTalkingInRoom, which only the idle state asks, for each player in a room. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerAudio_IsTalkingLoudly{.key = "PlayerAudio.IsTalkingLoudly",
                                                                .kind = pm::sdk::Kind::Method,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "PlayerAudio", .name = "IsTalkingLoudly",
                                                                .params = PlayerAudio_float};

/// The investigator's own voice and sounds, a PlayerAudio: Player's one field of that type, on every client.
/// The hunt's voice test asks it of the player it tests. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_audio{.key = "Player.audio", .kind = pm::sdk::Kind::Field,
                                                 .confidence = pm::sdk::Confidence::Decompiled, .owner = "Player",
                                                 .type = "@PlayerAudio"};

/// The level the lobby board shows for the player, within their prestige (1 for a new player):
/// ServerManager.UpdatePlayerLevelInformation, the buffered RPC each player sends with their saved experience,
/// level and prestige, writes it; ProfileCard.UpdateUI shows it. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSpot_level{.key = "PlayerSpot.level", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "PlayerSpot", .name = "level"};

/// The player's prestige (0 before the first; that RPC clamps it to 0..3999). Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerSpot_prestige{.key = "PlayerSpot.prestige", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "PlayerSpot", .name = "prestige"};

inline constexpr const char* PhraseListenerController_OnPhraseRecognized_params[] = {"string"};

/// The voice recognition's handler of a recognised phrase (Assembly-CSharp, global namespace), on
/// each client for its own player. Decompiled.
inline constexpr pm::sdk::EntryDesc PhraseListenerController{.key = "PhraseListenerController",
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .name = "PhraseListenerController",
                                                             .assembly = "Assembly-CSharp", .ns = "",
                                                             .base = &MonoBehaviour};

/// The ghost's reaction to its name, on the client that runs the ghost. Inferred: from its name and
/// Playable Ghost's use since it was built.
inline constexpr pm::sdk::EntryDesc PhraseListenerController_NetworkedReaction{
    .key = "PhraseListenerController.NetworkedReaction", .kind = pm::sdk::Kind::Method,
    .owner = "PhraseListenerController", .name = "NetworkedReaction"};

/// A phrase this client's voice recognition recognised: its only caller is SpeechRecognitionController, and it runs for every
/// phrase, in a hunt or not. In a contract the game's handling returns at once unless the ghost hunts (GhostAI.isHunting); only then
/// does it compare the phrase with the ghost's name (GhostInfo.name) and its other phrases and the ghost's type name. So a hook sees
/// phrases outside a hunt that the game itself ignores. Decompiled.
inline constexpr pm::sdk::EntryDesc PhraseListenerController_OnPhraseRecognized{
    .key = "PhraseListenerController.OnPhraseRecognized", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhraseListenerController", .name = "OnPhraseRecognized",
    .params = PhraseListenerController_OnPhraseRecognized_params};

/// This client's investigator, the game's Player: Network's one Player field. The game's own code reads
/// the local player through it (ExitLevel.Exit, LightSwitch.UseLightSwitch); null while this client
/// plays none. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_localPlayer{.key = "Network.localPlayer", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Network", .type = "Player"};

/// This client's player rig, a LocalPlayer: Network's one field of that type. Inferred: from the class
/// dump.
inline constexpr pm::sdk::EntryDesc Network_localRig{.key = "Network.localRig", .kind = pm::sdk::Kind::Field,
                                                     .owner = "Network", .type = "@LocalPlayer"};

// GameController.LoadBackToMenu sets one bool of Network, through get_Instance: another type's method, so
// the fact matches that offset through any register.
inline constexpr pm::sdk::Fact Network_sceneChanging_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GameController.LoadBackToMenu", .value = 1}};

/// Set by every scene change the game makes on its way out of a level: GameController.LoadBackToMenu and
/// PauseMenuController.Leave both set it. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_sceneChanging{
    .key = "Network.sceneChanging", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .type = "bool", .facts = Network_sceneChanging_facts};

/// The rig's networked investigator, the game's Player: LocalPlayer's one Player field. Inferred: from
/// the class dump.
inline constexpr pm::sdk::EntryDesc LocalPlayer_player{.key = "LocalPlayer.player", .kind = pm::sdk::Kind::Field,
                                                       .owner = "LocalPlayer", .type = "Player"};

/// The rig's eyes, a Camera: LocalPlayer's one Camera field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LocalPlayer_camera{.key = "LocalPlayer.camera", .kind = pm::sdk::Kind::Field,
                                                       .owner = "LocalPlayer", .type = "Camera"};

/// The rig's journal page controller: LocalPlayer's one JournalController field. Decompiled role, from
/// the class dump: PCCanvas.PausePlayerInPlace checks it when releasing its hold (a rig with no journal
/// controller throws at the release; a journal that says itself open refuses the release), and
/// JournalController.OpenCloseJournal keeps its own open state on that controller.
inline constexpr pm::sdk::EntryDesc LocalPlayer_journalController{
    .key = "LocalPlayer.journalController", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LocalPlayer", .type = "@JournalController"};

// How fast the local investigator moves. LocalPlayer declares the speeds and LocalPCPlayer (and the VR
// rig) applies them; sdk/systems/players.md, "Speed and stamina", ties them together.

/// The rig's constructor, which LocalPCPlayer's shares: it gives the walk and sprint speeds their
/// defaults, which the rig's prefab then overrides. Found only for those facts. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_ctor{.key = "LocalPlayer.ctor", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "LocalPlayer", .name = ".ctor"};

// The constructor stores two float constants, 1.6 into the walk speed and 3.0 into the sprint speed.
inline constexpr pm::sdk::Fact LocalPlayer_walkSpeed_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "LocalPlayer.ctor", .value = std::bit_cast<int32_t>(1.6f)}};
inline constexpr pm::sdk::Fact LocalPlayer_sprintSpeed_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "LocalPlayer.ctor", .value = std::bit_cast<int32_t>(3.0f)}};

/// The rig's walking speed in metres a second, before the speed multiplier: a serialized float the
/// prefab sets (the constructor's default is 1.6). LocalPCPlayer.UpdatePlayerSpeed takes it whenever the
/// player does not sprint. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_walkSpeed{.key = "LocalPlayer.walkSpeed", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "LocalPlayer", .type = "float",
                                                          .facts = LocalPlayer_walkSpeed_facts};

/// The rig's sprinting speed in metres a second, before the speed multiplier: a serialized float the
/// prefab sets (the constructor's default is 3.0). LocalPCPlayer.UpdatePlayerSpeed takes it only while the
/// player asks to sprint (FirstPersonController.running), sprinting is allowed (Difficulty.sprinting not 0, or no
/// GameController.instance) and the stamina
/// can sprint (PlayerStamina.canSprint); it holds the value even when sprinting is off. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_sprintSpeed{
    .key = "LocalPlayer.sprintSpeed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LocalPlayer", .type = "float", .facts = LocalPlayer_sprintSpeed_facts};

/// Applies the rig's speed to its FirstPersonController, on the local player's client: the sprint speed
/// while FirstPersonController.running is set, sprinting is allowed (Difficulty.sprinting of
/// LevelValues.difficulty is not 0, or no GameController.instance exists) and PlayerStamina.canSprint is set, otherwise the walk speed; times the speed multiplier, which
/// in a contract it skips while LocalPlayer.isDead is set. LocalPlayer.Start and the multiplier's setters run
/// it. The VR rig's stamina applies its speed instead (VRStamina.Update). Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_UpdatePlayerSpeed{
    .key = "LocalPCPlayer.UpdatePlayerSpeed", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPCPlayer", .name = "UpdatePlayerSpeed"};

inline constexpr const char* LocalPCPlayer_float[] = {"float"};

/// Sets the rig's speed multiplier (LocalPlayer.speedMultiplier) and applies it through UpdatePlayerSpeed,
/// on this client only. A Monkey Paw wish calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_SetPlayerSpeedMultiplier{
    .key = "LocalPCPlayer.SetPlayerSpeedMultiplier", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPCPlayer", .name = "SetPlayerSpeedMultiplier",
    .params = LocalPCPlayer_float};

/// Puts the rig's speed multiplier back to the copy it kept when its speed was first set (LocalPlayer.Start
/// copies the contract's Difficulty.playerSpeed into the multiplier and keeps that copy) and applies the speed
/// again, on this client only. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_ResetPlayerSpeedMultiplier{
    .key = "LocalPCPlayer.ResetPlayerSpeedMultiplier", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPCPlayer", .name = "ResetPlayerSpeedMultiplier"};

// Of the rig's floats the multiplier's setter stores only this one, and the speed update reads it with
// the two speeds. Both methods are LocalPCPlayer's, so each fact matches the offset through any register.
inline constexpr pm::sdk::Fact LocalPlayer_speedMultiplier_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "LocalPCPlayer.SetPlayerSpeedMultiplier"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "LocalPCPlayer.UpdatePlayerSpeed"}};

/// The factor the rig's walk and sprint speeds are multiplied by, on this client: LocalPlayer.Start sets it
/// from the contract's Difficulty.playerSpeed (and keeps a copy that ResetPlayerSpeedMultiplier restores);
/// LocalPCPlayer.SetPlayerSpeedMultiplier changes it. In a contract the speed skips it while LocalPlayer.isDead
/// is set. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_speedMultiplier{
    .key = "LocalPlayer.speedMultiplier", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LocalPlayer", .type = "float", .facts = LocalPlayer_speedMultiplier_facts};

/// The rig's start, on its own client: in a contract it copies Difficulty.playerSpeed into
/// LocalPlayer.speedMultiplier (and keeps the copy ResetPlayerSpeedMultiplier restores), then applies the speed
/// (UpdatePlayerSpeed). Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_Start{.key = "LocalPlayer.Start", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "LocalPlayer", .name = "Start"};

/// The end of the local player's death, on its own client: among other steps it counts the death in
/// LevelStats and sets LocalPlayer.isDead. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_CompleteLocalPlayerDeath{
    .key = "LocalPlayer.CompleteLocalPlayerDeath", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPlayer", .name = "CompleteLocalPlayerDeath"};

// The speed update reads one bool of the rig, this one. (The death's end stores it too, but through a register
// the reader cannot prove holds the rig, so that fact cannot decide.)
inline constexpr pm::sdk::Fact LocalPlayer_isDead_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LocalPCPlayer.UpdatePlayerSpeed"}};

/// Whether the rig's player has died, on its own client only: LocalPlayer.CompleteLocalPlayerDeath sets it and
/// the rig's revive clears it. While it is set in a contract the rig's speed skips LocalPlayer.speedMultiplier
/// (LocalPCPlayer.UpdatePlayerSpeed, and VRStamina.Update on VR), and PCStamina.OutOfBreath works even with
/// sprinting off (Difficulty.sprinting 0). Other clients read Player.isDead. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_isDead{.key = "LocalPlayer.isDead", .kind = pm::sdk::Kind::Field,
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .owner = "LocalPlayer", .type = "bool",
                                                       .facts = LocalPlayer_isDead_facts};

/// A player's stamina on its own client (Assembly-CSharp, global namespace), abstract; PCStamina is the PC
/// one. It reports sprinting and walking to every client through the player's NetworkedStamina. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerStamina{.key = "PlayerStamina", .confidence = pm::sdk::Confidence::Decompiled,
                                                  .name = "PlayerStamina", .assembly = "Assembly-CSharp", .ns = "",
                                                  .base = &MonoBehaviour};

/// The PC player's stamina (Assembly-CSharp, global namespace), a PlayerStamina. Decompiled.
inline constexpr pm::sdk::EntryDesc PCStamina{.key = "PCStamina", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "PCStamina", .assembly = "Assembly-CSharp", .ns = "",
                                              .base = &PlayerStamina};

/// The VR player's stamina (Assembly-CSharp, global namespace), a PlayerStamina. Decompiled.
inline constexpr pm::sdk::EntryDesc VRStamina{.key = "VRStamina", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "VRStamina", .assembly = "Assembly-CSharp", .ns = "",
                                              .base = &PlayerStamina};

/// The VR stamina's update, every frame on the player's own client. On the branch a flag of VRStamina picks
/// (not traced) it gives the VR movement the rig's sprint speed while the stamina's sprint flag is set and the
/// walk speed otherwise, then, in a contract, multiplies it by LocalPlayer.speedMultiplier unless
/// LocalPlayer.isDead is set. Outside a contract it applies no multiplier, where the PC's
/// LocalPCPlayer.UpdatePlayerSpeed does. Decompiled.
inline constexpr pm::sdk::EntryDesc VRStamina_Update{.key = "VRStamina.Update", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "VRStamina", .name = "Update"};

/// The rig's input component, a PlayerInput (Unity.InputSystem): LocalPlayer's one field of that type. The game's
/// input checks (InputDevices) read it. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_playerInput{.key = "LocalPlayer.playerInput",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "LocalPlayer", .type = "PlayerInput"};

/// The rig's stamina, a PlayerStamina: LocalPlayer's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_stamina{.key = "LocalPlayer.stamina", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "LocalPlayer", .type = "@PlayerStamina"};

// Of PCStamina's void() methods only the out-of-breath step calls NetworkedStamina.ForceOutOfBreatheLocal
// (its look-alikes send other RPCs or none).
inline constexpr pm::sdk::Fact PCStamina_OutOfBreath_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "NetworkedStamina.ForceOutOfBreatheLocal"}};

/// The PC player's stamina running out, on its own client: in a contract with sprinting off
/// (Difficulty.sprinting 0) it does nothing unless LocalPlayer.isDead is set; otherwise it clears
/// PlayerStamina.canSprint and a flag of the FirstPersonController, then plays the out-of-breath sound:
/// in a room by RPC to every client (NetworkedStamina's PlayOutOfBreathSound, which checks its sender),
/// alone through NetworkedStamina.ForceOutOfBreatheLocal. No direct call reaches it. Decompiled.
inline constexpr pm::sdk::EntryDesc PCStamina_OutOfBreath{.key = "PCStamina.OutOfBreath",
                                                          .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "PCStamina", .type = "void",
                                                          .facts = PCStamina_OutOfBreath_facts};

// The out-of-breath step clears one bool of the stamina (the FirstPersonController's flag it also
// clears is another type's); the speed update reads that bool through the rig's stamina field.
inline constexpr pm::sdk::Fact PlayerStamina_canSprint_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "PCStamina.OutOfBreath"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "LocalPCPlayer.UpdatePlayerSpeed"}};

/// Whether the player may sprint now, on that player's own client only (no other client holds it):
/// PCStamina.OutOfBreath clears it as the stamina runs out, and other PCStamina methods set it again (which
/// of them is the recovery was not traced). On VR the stamina's out-of-breath step (a VRStamina method that
/// also plays the out-of-breath sound) clears it too. LocalPCPlayer.UpdatePlayerSpeed gives the walk speed
/// while it is clear. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerStamina_canSprint{
    .key = "PlayerStamina.canSprint", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerStamina", .type = "bool", .facts = PlayerStamina_canSprint_facts};

inline constexpr const char* Player_RevivePlayer_params[] = {"bool"};

/// Revives the player: it starts a coroutine on this client that waits, a frame at a time, until the player
/// is marked dead (Player.isDead), then sends the `Revive` RPC to every client through the server and starts
/// the revive animation. With the flag set it also sends `SendRemoveHighPriestess` to the master client.
/// Its two callers pass false, both on the master: the Monkey Paw's revive wish (MonkeyPaw.ReviveWish), on
/// the first dead player who is not the wisher, and the High Priestess card's coroutine
/// (TarotCards.HighPriestess), on the player that card chose (sdk/systems/players.md). The Revive RPC's body
/// refuses, with the game's report on the sender even offline, unless a list held by LevelValues contains one
/// of two values (very likely the contract's cursed possessions: the paw or the cards) and the player is
/// marked dead or dying; never call it where the game itself would not. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_RevivePlayer{.key = "Player.RevivePlayer", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Player", .name = "RevivePlayer",
                                                        .params = Player_RevivePlayer_params};

/// Photon's callback for a leaver. It looks for the leaver's PlayerSpot and returns at once when there is none.
/// Otherwise it removes the spot before anything else reads it, calls a virtual method of the spot's Player with
/// true while that Player is alive, sorts the list again, raises the event GameController's handler runs the
/// all-dead test from (so a leaver can end the contract, sdk/systems/contract.md), and on the master removes the
/// leaver's RPCs from the server (PhotonNetwork.RemoveRPCs). Decompiled.
inline constexpr pm::sdk::EntryDesc Network_OnPlayerLeftRoom{
    .key = "Network.OnPlayerLeftRoom", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "OnPlayerLeftRoom"};

/// Photon's callback when this client leaves a room. It empties a string of Network's, reads the store manager's
/// instance (and throws without one) to clear the store's multiplayer activity, raises an event of Network when
/// one is set, then empties Network.playerSpots (its count set to 0 and its array cleared) and resets LevelStats.
/// So once a client is out of a room its list holds no spot at all. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_OnLeftRoom{
    .key = "Network.OnLeftRoom", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "OnLeftRoom"};

/// The Player in the spot of a Photon player: it calls Network.GetSpotPlayerFromPhotonPlayer directly and answers
/// that spot's PlayerSpot.player, or Network.localPlayer when it answers null: this client's own investigator, or
/// null on a client without one (sdk/systems/players.md, "A player with no spot"). So a hook of the spot lookup
/// reaches every caller of this one. The Monkey Paw and TarotCards.HangedMan find their victim through it; the
/// other killing card uses an obfuscated sibling. Network has a second method of this shape, obfuscated, that does
/// the spot's search in place (a FirstOrDefault over the list with the same predicate and the same fallback) and
/// does not call the spot lookup, so no hook of either lookup reaches it. In this build every one of its 84
/// callers is reached only from methods with no caller at all, up to six callers up (the Krampus present's copy,
/// LiftButton's, the journal's, the footsteps' and the audio animators' among them). A build that calls it from
/// live code must be checked again. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_GetPlayerFromPhotonPlayer{
    .key = "Network.GetPlayerFromPhotonPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "GetPlayerFromPhotonPlayer"};

/// The PlayerSpot of a Photon player, or null when no spot holds them: the first of Network.playerSpots whose
/// PlayerSpot.photonPlayer is that Photon player, compared by reference. Network.GetPlayerFromPhotonPlayer and
/// Network.GetIndexOfPlayer call it directly, and so do the game's other lookups of a player's spot (voice,
/// cosmetics, the journal's buttons, the profile cards). The obfuscator made three copies of it with the same
/// body; every method that calls a copy is reached only from methods with no caller and no delegate made of them,
/// up to three callers up. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_GetSpotPlayerFromPhotonPlayer{
    .key = "Network.GetSpotPlayerFromPhotonPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "GetSpotPlayerFromPhotonPlayer", .type = "@PlayerSpot"};

/// A Photon player's position in Network.playerSpots: it calls Network.GetSpotPlayerFromPhotonPlayer and answers 0
/// when that answers null, otherwise the list's IndexOf of that spot, which is -1 for a spot not in the list.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Network_GetIndexOfPlayer{
    .key = "Network.GetIndexOfPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "GetIndexOfPlayer"};

/// This player's statistics: the class's one PlayerStats field. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_stats{
    .key = "Player.stats", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Player", .type = "@PlayerStats"};

/// Starts a kill: it sends the death to every client, and the animation to the view's owner by RPC. Its callers are the hunt's
/// kill, the Monkey Paw (the wisher) and the tarot cards (the card's user). Reach it only as they do: the animation's
/// coroutine refuses, with a report on the sender, a kill of a dead player or on a friendly ghost's difficulty. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_BeginDeathSequence{
    .key = "Player.BeginDeathSequence", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Player", .name = "BeginDeathSequence"};

/// The end of a kill, by RPC from the view's owner: refused on the menu's scene and on a friendly ghost's difficulty. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_CompleteDeathSequenceNetworked{
    .key = "Player.CompleteDeathSequenceNetworked", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Player", .name = "CompleteDeathSequenceNetworked"};

inline constexpr const char* Player_NotifyOfDeathStart_params[] = {"PhotonMessageInfo"};

/// The start of a kill, by RPC to every client (Player.BeginDeathSequence sends it, through the server). On every
/// client it marks the player dying, turns that player's own collider off and switches on the object that holds their
/// ragdoll; the death animation is separate, sent to the view's owner alone. The dying mark is a bool the map cannot
/// name, its dump name being obfuscated and shuffled every build, so this is how a mod sees a death begin: a little
/// before `Player.isDead`, which only the death's completion sets. The body first refuses a sender that may not send it
/// and a victim already marked dead. `info` is the message's PhotonMessageInfo, which the RPC system fills. Hook it to
/// watch a death begin; never call it, which would be sending the killer's RPC. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_NotifyOfDeathStart{
    .key = "Player.NotifyOfDeathStart", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Player", .name = "NotifyOfDeathStart", .params = Player_NotifyOfDeathStart_params};

inline constexpr const char* Network_AddPlayerSpot_params[] = {"PlayerSpot"};

/// Adds a player's PlayerSpot to Network.playerSpots. Before it appends the spot it gives the spot's equipment
/// list one new entry for each item of a store list, so adding the same spot twice adds those entries twice.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Network_AddPlayerSpot{
    .key = "Network.AddPlayerSpot", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "AddPlayerSpot", .params = Network_AddPlayerSpot_params};

// Photon's player first, then the game's networked Player; the model names both "Player".
inline constexpr const char* Network_SetSpotPlayer_params[] = {"Player", "Player"};

/// Gives a Photon player's spot its networked Player: it starts a coroutine that waits until Network.playerSpots
/// holds as many spots as the room has Photon players (PhotonNetwork.get_PlayerList), then finds the Photon
/// player's PlayerSpot, stores the Player in it, updates that spot's voice block and raises an event of Network.
/// With no spot for that Photon player by then, the coroutine throws. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_SetSpotPlayer{
    .key = "Network.SetSpotPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "SetSpotPlayer", .params = Network_SetSpotPlayer_params};

/// The first spot of Network.playerSpots that Network's own predicate accepts (by its name, this client's), or
/// null. The reward screen, the loadout and the truck computer read it. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_GetLocalPlayerSpot{
    .key = "Network.GetLocalPlayerSpot", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Network", .name = "GetLocalPlayerSpot"};

/// Sets whether this client hears and is heard by one player (VivoxVoiceManager.SetBlockVivoxCommunication): blocked
/// when their PlayerSpot is marked blocked or flagged by a refused RPC (sdk/systems/photon.md), or the platform's
/// mute or block is set. Decompiled.
inline constexpr pm::sdk::EntryDesc Network_UpdateVoiceBlockForPlayer{
    .key = "Network.UpdateVoiceBlockForPlayer", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Network", .name = "UpdateVoiceBlockForPlayer"};

/// The Winter's Jest present (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc KrampusPresent{.key = "KrampusPresent", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "KrampusPresent", .assembly = "Assembly-CSharp", .ns = "",
                                                   .base = &MonoBehaviourPun};

/// Where the present spawns: it walks the room's Photon players (PhotonNetwork.get_PlayerList), finds each one's
/// Player through Network.GetPlayerFromPhotonPlayer and reads its position, and throws when that answers null. For
/// each of the present's spawn points it takes the smallest distance to any of those positions, then answers the
/// spawn point with the largest such distance. So a position counted twice changes nothing. With no players or no
/// spawn points it answers the present's own position. WintersJestManager calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc KrampusPresent_GetFurthestSpawnPositionFromPlayers{
    .key = "KrampusPresent.GetFurthestSpawnPositionFromPlayers", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "KrampusPresent",
    .name = "GetFurthestSpawnPositionFromPlayers"};

// The readers of a player's position in Network.playerSpots (sdk/systems/players.md, "A player's position in the
// list").

/// A piece of UI coloured for one position in the list (Assembly-CSharp, global namespace): the journal's marks and
/// vote icons, the wristwatch's sanity bars and the ID cards. Decompiled.
inline constexpr pm::sdk::EntryDesc UpdatePlayerColor{.key = "UpdatePlayerColor",
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .name = "UpdatePlayerColor", .assembly = "Assembly-CSharp",
                                                      .ns = "", .base = &MonoBehaviour};

/// The position this piece of UI is coloured for: its Start takes the colour at that place in Network's array of
/// player colours, and follows that place when the colour changes. The PCPlayer prefab sets 0 to 3 on the marks P1
/// to P4 and the matching vote icons and sanity bars. Decompiled.
inline constexpr pm::sdk::EntryDesc UpdatePlayerColor_playerID{.key = "UpdatePlayerColor.playerID",
                                                               .kind = pm::sdk::Kind::Field,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "UpdatePlayerColor", .name = "playerID",
                                                               .type = "int"};

/// Another player's choice on a journal button, on every client that receives it: it stores the choice by Photon
/// player, then, unless the button is that player's own, finds their spot, skips a dead player's while this client's
/// player is alive, and sets the mark at the spot's List.IndexOf in NetworkThreeStateButton.otherPlayerButtons. A
/// spot not in the list gives -1, which throws. Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkThreeStateButton_SetOtherPlayerButtonState{
    .key = "NetworkThreeStateButton.SetOtherPlayerButtonState", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "NetworkThreeStateButton",
    .name = "SetOtherPlayerButtonState"};

/// A journal button's four marks, one per position in the list (P1 to P4 in the prefab). Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkThreeStateButton_otherPlayerButtons{
    .key = "NetworkThreeStateButton.otherPlayerButtons", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "NetworkThreeStateButton", .name = "otherPlayerButtons"};

/// Clears a dead player's mark on a journal button, at Network.GetIndexOfPlayer in
/// NetworkThreeStateButton.otherPlayerButtons, while this client's player is alive. Its callers are obfuscated methods
/// of a generic journal class; whether they run was not proven. Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkThreeStateButton_ValidatePlayerSelection{
    .key = "NetworkThreeStateButton.ValidatePlayerSelection", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "NetworkThreeStateButton",
    .name = "ValidatePlayerSelection"};

/// One player's vote on removing a photo, video or sound, from Journal_PhotoPage's RemovePhotoRPC, RemoveVideoRPC and
/// RemoveSoundRPC: it records the vote by Photon player and shows the tick or cross at Network.GetIndexOfPlayer in
/// the entry's vote icons. An index outside them throws. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalMedia_MediaVote{
    .key = "JournalMedia.MediaVote", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalMedia", .name = "MediaVote"};

/// Fills the entry's votes with one row for each Photon player of Network.playerSpots; the votes needed are worked
/// out from how many rows there are. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalMedia_SetupVotes{
    .key = "JournalMedia.SetupVotes", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalMedia", .name = "SetupVotes"};

/// The wristwatch (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc Wristwatch{.key = "Wristwatch", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "Wristwatch", .assembly = "Assembly-CSharp", .ns = "",
                                               .base = &MonoBehaviour};

/// Sets each sanity bar from the Player at the same position in Network.playerSpots. Wristwatch.Start repeats it, by
/// name, in a level on a difficulty that shows the bars. A bar past the list's end is not written. Decompiled.
inline constexpr pm::sdk::EntryDesc Wristwatch_UpdateSanity{
    .key = "Wristwatch.UpdateSanity", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Wristwatch", .name = "UpdateSanity"};

/// Shows or hides each sanity bar by whether the Player at the same position in Network.playerSpots is dead. Two
/// events of the level's controller run it. Decompiled.
inline constexpr pm::sdk::EntryDesc Wristwatch_UpdateBarVisibility{
    .key = "Wristwatch.UpdateBarVisibility", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "Wristwatch", .name = "UpdateBarVisibility"};

/// Gives an ID card its PlayerSpot and shows it. It passes Network.GetIndexOfPlayer to the card's photo step, which
/// reads the local file IDPhoto{position}.png. PlayersList.RefreshUI calls it for each other player in the list, and
/// JournalController.OpenCloseJournal runs that, so the pause menu's cards are read in a contract. Decompiled.
inline constexpr pm::sdk::EntryDesc ProfileCard_AssignPlayer{
    .key = "ProfileCard.AssignPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ProfileCard", .name = "AssignPlayer"};

/// The player's character model (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCharacter{.key = "PlayerCharacter",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "PlayerCharacter", .assembly = "Assembly-CSharp", .ns = "",
                                                    .base = &MonoBehaviourPun};

/// Sets a bool of the character from a body type's alternate-prefabs flag. Every enabled character listens for it on
/// an event of Network that carries a position. With a position in the list, only the character whose view's owner is
/// that spot's Photon player changes, from that spot's Player. With -1, every listening character takes this client's
/// own player's body type. The game raises -1 only from obfuscated methods of the customisation screen. What reads
/// the bool was not traced. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCharacter_SetPlayerBodyType{
    .key = "PlayerCharacter.SetPlayerBodyType", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerCharacter", .name = "SetPlayerBodyType"};

/// The head's renderers (hair, eyes, teeth and the head's cosmetics), a Renderer[]. The game draws them as shadows
/// only on the player's own client, so the head does not fill the view: PlayerCharacter's methods that change the
/// head's shadow casting and its layer walk this list. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCharacter_headRenderersToTurnOff{
    .key = "PlayerCharacter.headRenderersToTurnOff", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerCharacter", .name = "headRenderersToTurnOff",
    .type = "Renderer[]"};

inline constexpr const char* PlayerCharacter_ChangeHeadObjectsShadowCastingModeShadowsOnly_params[] = {"bool"};

/// Sets every renderer of PlayerCharacter.headRenderersToTurnOff to shadows only (true) or drawn (false), whatever each
/// had. The game calls it on Network.localPlayer's character, the client's own head: the photo camera's networked
/// AddPhotoToJournal coroutine shows the head for the photo's render and hides it after, on every client the photo
/// reaches, and CameraFocusUI's Focus and UnFocus switch it too. So a mod that draws its own player's head (a
/// third-person view) finds it hidden again mid-way, and draws it again each frame. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCharacter_ChangeHeadObjectsShadowCastingModeShadowsOnly{
    .key = "PlayerCharacter.ChangeHeadObjectsShadowCastingModeShadowsOnly", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerCharacter",
    .name = "ChangeHeadObjectsShadowCastingModeShadowsOnly",
    .params = PlayerCharacter_ChangeHeadObjectsShadowCastingModeShadowsOnly_params};

/// The player's cosmetics (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCosmetics{.key = "PlayerCosmetics",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "PlayerCosmetics", .assembly = "Assembly-CSharp", .ns = "",
                                                    .base = &MonoBehaviourPun};

/// The Animator of the investigator's body, a serialized field. On the PCPlayer prefab (Resources.Load("PCPlayer"),
/// which LocalPCPlayer.CreateNetworkedPlayer spawns each investigator from) it sits on the body, "Rigged PC Model
/// Base": a humanoid model every client has the same, whoever is in the room. Copy that body, never a live player's,
/// which carries that player's outfit and the game's shadow-only and layer settings (sdk/api/models.h). Decompiled:
/// read from the prefab; the Squatter's engine test copied the body through it in game (2026-11-05).
inline constexpr pm::sdk::EntryDesc PlayerCosmetics_playerAnimator{
    .key = "PlayerCosmetics.playerAnimator", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerCosmetics", .name = "playerAnimator", .type = "Animator"};

/// A player's outfit, by RPC: it stores it on the sender's spot and loads it, then raises Network's body-type event
/// with Network.GetIndexOfPlayer. The only live code that names this RPC is PlayerCustomizationScreenManager.SaveOutfit.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCosmetics_SetPlayerCosmeticsNetworked{
    .key = "PlayerCosmetics.SetPlayerCosmeticsNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerCosmetics", .name = "SetPlayerCosmeticsNetworked"};

/// A player's lobby information, by RPC: after its awaited steps it loads their outfit and then raises Network's
/// body-type event with Network.GetIndexOfPlayer. ServerManager's RequestPlayerInfo and one of its coroutines send it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ServerManager_SetPlayerInformation{
    .key = "ServerManager.SetPlayerInformation", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ServerManager", .name = "SetPlayerInformation"};

/// The game's multiplayer helpers (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc MultiplayerController{.key = "MultiplayerController", .confidence = pm::sdk::Confidence::Decompiled, .name = "MultiplayerController",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// The end of a contract's destroy: it destroys this client's own networked objects
/// (PhotonNetwork.DestroyPlayerObjects) and sets a custom property of its Photon player. It reads no Network.localPlayer.
inline constexpr pm::sdk::EntryDesc MultiplayerController_NetworkDestroyObjects{
    .key = "MultiplayerController.NetworkDestroyObjects", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "MultiplayerController", .name = "NetworkDestroyObjects"};

/// The scene's MultiplayerController: the class's one static field of its own type. Decompiled.
inline constexpr pm::sdk::EntryDesc MultiplayerController_instance{
    .key = "MultiplayerController.instance", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "MultiplayerController", .hint = "instance",
    .type = "@MultiplayerController"};

/// Where the local investigator starts the contract, a List<Transform>: in a contract, the controller makes the local
/// player at a random one of them (Object.Instantiate of the player's prefab at its position), so they stand in the
/// truck. FallTeleportBox sends a fallen player to the first one when no NavMesh point is near. The class's one field
/// of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc MultiplayerController_spawnPoints{
    .key = "MultiplayerController.spawnPoints", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "MultiplayerController", .hint = "spawnPoints",
    .type = "List<Transform>"};

inline constexpr const char* LocalPlayer_TeleportPlayer_params[] = {"Vector3"};

/// TeleportPlayer(position): the game's own move of the local investigator, which PCPlayer.Teleport (the PC body of
/// the abstract Player.Teleport) runs, as does the training room's hunt (TrainingHuntRoom). Decompiled: it raises a flag of the rig's, switches the rig's
/// PhysicsCharacterController off, sets the rig's own transform (the root the prefab was made at) to the position,
/// and switches the controller back on, so it also undoes a mod's switching that controller off. Local; it sends
/// nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_TeleportPlayer{
    .key = "LocalPlayer.TeleportPlayer", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LocalPlayer", .name = "TeleportPlayer", .type = "void", .params = LocalPlayer_TeleportPlayer_params};

/// The Winter's Jest seasonal event (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc WintersJestManager{.key = "WintersJestManager", .confidence = pm::sdk::Confidence::Decompiled, .name = "WintersJestManager",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviourPun};

/// The event's RPC: after the first-actor check (sdk/systems/photon.md), unless the weather is the blood moon or
/// GhostAI.speedFlag is already set, it acts on Network.localPlayer's sanity, and it then sets GhostAI.speedFlag. A client
/// with no Network.localPlayer throws before it sets the flag.
inline constexpr pm::sdk::EntryDesc WintersJestManager_RPC_MakeKrampusStronger{
    .key = "WintersJestManager.RPC_MakeKrampusStronger", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "WintersJestManager", .name = "RPC_MakeKrampusStronger"};

/// A saved loadout of the truck's equipment (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc Loadout{.key = "Loadout", .confidence = pm::sdk::Confidence::Decompiled, .name = "Loadout",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// The loadout's equip: it calls InventoryManager.AddLoadoutToLobby, the way a loadout reaches the truck.
inline constexpr pm::sdk::EntryDesc Loadout_EquipLoadout{
    .key = "Loadout.EquipLoadout", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Loadout", .name = "EquipLoadout"};

/// The lobby computer's player list, coloured by Network.GetIndexOfPlayer. GhostOS.OnLeftRoom, its PlayerJoinedRoom and a
/// ServerManager coroutine call it.
inline constexpr pm::sdk::EntryDesc GhostOS_UpdatePlayers{
    .key = "GhostOS.UpdatePlayers", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostOS", .name = "UpdatePlayers"};

/// A player colour changed on the lobby computer: a listener GhostOS.Start adds.
inline constexpr pm::sdk::EntryDesc GhostOS_UpdatePlayerColor{
    .key = "GhostOS.UpdatePlayerColor", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostOS", .name = "UpdatePlayerColor"};

/// The customisation screen's ID card preview (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc ProfileCardPreview{.key = "ProfileCardPreview", .confidence = pm::sdk::Confidence::Decompiled, .name = "ProfileCardPreview",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviourPun};

/// Refreshes the preview from the local player's cosmetics, reading Network.GetIndexOfPlayer.
inline constexpr pm::sdk::EntryDesc ProfileCardPreview_UpdateCosmeticData{
    .key = "ProfileCardPreview.UpdateCosmeticData", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ProfileCardPreview", .name = "UpdateCosmeticData"};

/// The customisation screen (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCustomizationScreenManager{.key = "PlayerCustomizationScreenManager", .confidence = pm::sdk::Confidence::Decompiled, .name = "PlayerCustomizationScreenManager",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// Saves the outfit and sends PlayerCosmetics.SetPlayerCosmeticsNetworked, the only live code that names that RPC.
inline constexpr pm::sdk::EntryDesc PlayerCustomizationScreenManager_SaveOutfit{
    .key = "PlayerCustomizationScreenManager.SaveOutfit", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerCustomizationScreenManager", .name = "SaveOutfit"};

/// Resets the screen's selections. Its callback raises Network's body-type event with Network.GetIndexOfPlayer.
inline constexpr pm::sdk::EntryDesc PlayerCustomizationScreenManager_ResetSelections{
    .key = "PlayerCustomizationScreenManager.ResetSelections", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerCustomizationScreenManager", .name = "ResetSelections"};

/// This client's own journal input (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc LocalJournalController{.key = "LocalJournalController", .confidence = pm::sdk::Confidence::Decompiled, .name = "LocalJournalController",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviour};

/// Opens or closes the journal, unless a flag of the player's menu or of their Player stops it, and calls
/// JournalController.OpenCloseJournal. Neither flag was shown to be a load check.
inline constexpr pm::sdk::EntryDesc LocalJournalController_OpenCloseJournal{
    .key = "LocalJournalController.OpenCloseJournal", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LocalJournalController", .name = "OpenCloseJournal"};

/// A player asks for this client's lobby information, by RPC: it answers with ServerManager.SetPlayerInformation.
inline constexpr pm::sdk::EntryDesc ServerManager_RequestPlayerInfo{
    .key = "ServerManager.RequestPlayerInfo", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ServerManager", .name = "RequestPlayerInfo"};

/// The Players the room holds, kept apart from Network.playerSpots (sdk/systems/players.md).
inline constexpr pm::sdk::EntryDesc LevelRoom_GetPlayersInRoom{
    .key = "LevelRoom.GetPlayersInRoom", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelRoom", .name = "GetPlayersInRoom"};

/// The game's sound groups (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc SoundController{.key = "SoundController", .confidence = pm::sdk::Confidence::Decompiled, .name = "SoundController",
                                          .assembly = "Assembly-CSharp", .ns = "", .base = &MonoBehaviourPun};

/// The mixer group a player's own sounds go through: the group of the floor (FloorMixer) whose snapshot is that player's
/// current one (PlayerAudio's, which the game moves as the player goes between rooms), else the exterior group when that
/// snapshot is the exterior one, else the controller's other group. It takes a Photon player and finds the Player with
/// Network.GetPlayerFromPhotonPlayer. The game routes a player's footsteps (FootstepController.AttemptPlaySound),
/// equipment sounds (Equipment.PlayOnOffAudio) and the music box's song through it, so a player standing on stairs is
/// heard as on the floor the game has them on, where SoundController.GetFloorAudioSnapshot of the same point may pick the
/// floor above. Decompiled.
inline constexpr pm::sdk::EntryDesc SoundController_GetPlayersAudioGroup{
    .key = "SoundController.GetPlayersAudioGroup", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "SoundController", .name = "GetPlayersAudioGroup"};

inline constexpr const char* SoundController_GetFloorAudioSnapshot_params[] = {"Vector3"};

/// The mixer group a sound at a point plays through, the group Noise.PlaySound gives a noise that changes its mixer
/// on play (Noise.changeMixerOnPlay), so the game's mixer muffles it as the place it plays from calls for: the group of
/// an audio group override covering the point, else the exterior group on a level flagged outside the map, else the
/// group of the floor (FloorMixer) whose bounds hold the point, else the first floor's, with an error logged. Despite
/// its name it returns an AudioMixerGroup. Decompiled.
inline constexpr pm::sdk::EntryDesc SoundController_GetFloorAudioSnapshot{
    .key = "SoundController.GetFloorAudioSnapshot", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SoundController", .name = "GetFloorAudioSnapshot",
    .params = SoundController_GetFloorAudioSnapshot_params};

/// The level's SoundController: the class's one static field of its own type. Door.PlayDoorSlamNoise reads its
/// shared slam clips through it. Decompiled.
inline constexpr pm::sdk::EntryDesc SoundController_instance{
    .key = "SoundController.instance", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "SoundController", .type = "@SoundController"};

// Door.PlayDoorSlamNoise reads one of SoundController's AudioClip[] fields: the shared slam clips.
inline constexpr pm::sdk::Fact SoundController_doorSlamClips_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "Door.PlayDoorSlamNoise"}};

/// The game's shared door slam clips, an AudioClip[]: Door.PlayDoorSlamNoise plays one of them for a door with no
/// slam clips of its own (Door.slamClips). SoundController's one AudioClip[] that method reads. Decompiled.
inline constexpr pm::sdk::EntryDesc SoundController_doorSlamClips{
    .key = "SoundController.doorSlamClips", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "SoundController", .type = "AudioClip[]", .facts = SoundController_doorSlamClips_facts};


inline constexpr const char* Player_bool[] = {"bool"};

/// Freezes or frees the investigator's own movement on this client (the truck's wait uses it). Inferred:
/// from its name.
inline constexpr pm::sdk::EntryDesc Player_ToggleFreezePlayerMovement{
    .key = "Player.ToggleFreezePlayerMovement", .kind = pm::sdk::Kind::Method, .owner = "Player",
    .name = "ToggleFreezePlayerMovement", .params = Player_bool};

/// The PC player's input settings and actions (Assembly-CSharp, global namespace), a MonoBehaviour.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PCControls{.key = "PCControls", .name = "PCControls", .assembly = "Assembly-CSharp",
                                               .ns = "", .base = &MonoBehaviour};

/// The PC player's PCControls: LocalPCPlayer's one field of that type. Inferred: from the class
/// dump.
inline constexpr pm::sdk::EntryDesc LocalPCPlayer_controls{.key = "LocalPCPlayer.controls", .kind = pm::sdk::Kind::Field,
                                                           .owner = "LocalPCPlayer",
                                                           .type = "@PCControls"};

/// The input actions the player's controls run, an InputActionAsset holding every action map: PCControls'
/// one field of that type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PCControls_actions{.key = "PCControls.actions", .kind = pm::sdk::Kind::Field,
                                                       .owner = "PCControls",
                                                       .type = "InputActionAsset"};

/// The PC player's hand for grabbing and using things (Assembly-CSharp, global namespace), a
/// MonoBehaviour on the local player's rig. Decompiled.
inline constexpr pm::sdk::EntryDesc DragRigidbodyUse{.key = "DragRigidbodyUse",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .name = "DragRigidbodyUse", .assembly = "Assembly-CSharp",
                                                     .ns = "", .base = &MonoBehaviour};

inline constexpr const char* DragRigidbodyUse_OnInteract_params[] = {"CallbackContext"};

/// The interact input's callback, on the local player's client: when the action is performed it casts a
/// ray from the centre of the player's camera and, for a collider tagged Door, calls
/// Door.AttemptToUnlockDoor on that door (which does nothing on an unlocked one). Decompiled.
inline constexpr pm::sdk::EntryDesc DragRigidbodyUse_OnInteract{
    .key = "DragRigidbodyUse.OnInteract", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DragRigidbodyUse", .name = "OnInteract",
    .params = DragRigidbodyUse_OnInteract_params};

/// Unity's physics step for the hand, on the local player's client. While the interact input is held
/// (DragRigidbodyUse.OnInteract marks its start and clears it on release) and nothing is held yet, it
/// casts a ray from the camera's centre and, for a collider tagged Door, takes the door's
/// PhotonObjectInteract: it refuses a door that is locked (Door.locked) or not grabbable
/// (Door.grabbable), and a locked DoorKind.Main door during a hunt. Otherwise it calls Door.GrabbedDoor,
/// requests the door's PhotonView in a room (PhotonView.RequestOwnership) and frees the leaf's Rigidbody
/// to follow the hand. While holding it drives the leaf toward the aim. Decompiled.
inline constexpr pm::sdk::EntryDesc DragRigidbodyUse_FixedUpdate{
    .key = "DragRigidbodyUse.FixedUpdate", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DragRigidbodyUse", .name = "FixedUpdate"};

// The investigator's body as a mod animates it (sdk/api/player_anim.h, sdk/api/third_person.h): its Animator, its
// renderers, its hand anchors, and the local rig's parts that place the view and the held item. How the game animates,
// hides and turns the body is in sdk/systems/players.md, "The body".

/// The body's Animator: Player's one Animator field, on every client. Its controller is the game's (with override
/// controllers per held item); Animation Rigging and PhotonAnimatorView act on it too. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Player_animator{.key = "Player.animator", .kind = pm::sdk::Kind::Field,
                                                    .owner = "Player", .type = "Animator"};

/// The body's renderers, a Renderer[]: Player's one field of that type. Player.SetPlayerModelLayer moves each one's
/// GameObject between two named layers, which is how the local player's own camera leaves its body out. Decompiled.
inline constexpr pm::sdk::EntryDesc Player_renderers{.key = "Player.renderers", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Player", .type = "Renderer[]"};

/// The body's animation scripts (Assembly-CSharp, global namespace): its Animator, hand anchors, rigs and the
/// parameters it sets each frame. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerCharacterAnimations{.key = "PlayerCharacterAnimations",
                                                             .name = "PlayerCharacterAnimations",
                                                             .assembly = "Assembly-CSharp", .ns = "",
                                                              .base = &MonoBehaviourPun};

/// The investigator's PlayerCharacterAnimations: Player's one field of that type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc Player_characterAnimations{.key = "Player.characterAnimations",
                                                               .kind = pm::sdk::Kind::Field, .owner = "Player",
                                                               .type = "@PlayerCharacterAnimations"};

/// The right hand's anchor, a Transform the held item hangs from on the body. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerCharacterAnimations_parentingTargetRightHand{
    .key = "PlayerCharacterAnimations.parentingTargetRightHand", .kind = pm::sdk::Kind::Field,
    .owner = "PlayerCharacterAnimations", .name = "parentingTargetRightHand"};

/// The left hand's anchor, a Transform. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PlayerCharacterAnimations_parentingTargetLeftHand{
    .key = "PlayerCharacterAnimations.parentingTargetLeftHand", .kind = pm::sdk::Kind::Field,
    .owner = "PlayerCharacterAnimations", .name = "parentingTargetLeftHand"};

/// How loud the player is talking, the value PlayerAudio.IsTalkingLoudly compares: PlayerAudio's one public float.
/// PlayerAudio.Update, on every client for every player, refreshes it once a second from GetPlayerVoiceAmplitude: the
/// louder of the player's two Vivox participants' audio energy, less 0.4. IsTalkingLoudly zeroes it when it answers
/// true. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerAudio_loudness{.key = "PlayerAudio.loudness", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "PlayerAudio", .type = "float",
                                                         .access = pm::sdk::Visibility::Public};

/// The player's positional voice, the AudioSource of its local Vivox participant's tap, or null before it has one.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerAudio_GetPlayerVoiceAudioSource{
    .key = "PlayerAudio.GetPlayerVoiceAudioSource", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerAudio", .name = "GetPlayerVoiceAudioSource",
    .type = "@AudioSource"};

/// The PC player's hands (Assembly-CSharp, global namespace): its input handlers for picking up, using, dropping and
/// switching items, and the slot in front of the camera the held item hangs in on its own client. Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc PCPropGrab{.key = "PCPropGrab", .name = "PCPropGrab", .assembly = "Assembly-CSharp",
                                              .ns = "", .base = &MonoBehaviour};

/// The item in the camera slot (the held item, on the PC player's own client), a PhotonObjectInteract, or null.
/// Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PCPropGrab_GetItemInCameraSlot{
    .key = "PCPropGrab.GetItemInCameraSlot", .kind = pm::sdk::Kind::Method, .owner = "PCPropGrab",
    .name = "GetItemInCameraSlot", .type = "@PhotonObjectInteract"};

inline constexpr const char* PCPropGrab_PhotonObjectInteract[] = {"PhotonObjectInteract"};

/// Puts an item back in the camera slot, at the item's own local offset and turn: what the game does on its own client
/// when the player takes an item in hand. Local only: the held item's sync component is off while held
/// (NetworkedPropGrab.GrabPropRPC), so nothing is sent. Decompiled.
inline constexpr pm::sdk::EntryDesc PCPropGrab_ParentItemOnLocalPlayer{
    .key = "PCPropGrab.ParentItemOnLocalPlayer", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PCPropGrab", .name = "ParentItemOnLocalPlayer",
    .params = PCPropGrab_PhotonObjectInteract};

inline constexpr const char* Player_Vector3[] = {"Vector3"};

/// Moves the local VR rig to a point, on this client only: it sets the position of the player's own LocalPlayer (the
/// rig's transform, as LocalPlayer.TeleportPlayer does on PC, without switching the controller off). No RPC.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc VRPlayer_Teleport{.key = "VRPlayer.Teleport", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "VRPlayer", .name = "Teleport",
                                                      .params = Player_Vector3};

/// The rig's movement controller, a PhysicsCharacterController: LocalPlayer's one field of that type, which
/// LocalPlayer.TeleportPlayer switches off for the move. Decompiled.
inline constexpr pm::sdk::EntryDesc LocalPlayer_characterController{
    .key = "LocalPlayer.characterController", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LocalPlayer", .type = "@PhysicsCharacterController"};

/// Its Awake: it works out the controller's collision mask from its Rigidbody's layer and stores it
/// (PhysicsCharacterController.collisionMask). Decompiled.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_Awake{
    .key = "PhysicsCharacterController.Awake", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhysicsCharacterController", .name = "Awake"};

// Of the controller's three LayerMask fields, Awake stores only the collision mask (the door and ground masks are set
// in the prefab).
inline constexpr pm::sdk::Fact PhysicsCharacterController_collisionMask_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "PhysicsCharacterController.Awake"}};

/// The layers the player's capsule collides with as it moves, a LayerMask (read as its int): what blocks the player.
/// Awake works it out from the Rigidbody's layer. Decompiled.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_collisionMask{
    .key = "PhysicsCharacterController.collisionMask", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PhysicsCharacterController", .type = "LayerMask",
    .facts = PhysicsCharacterController_collisionMask_facts};

/// The layers the player stands on, a LayerMask (read as its int). Inferred: the property's name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_groundLayers{
    .key = "PhysicsCharacterController.get_groundLayers", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_groundLayers"};

/// The player's capsule radius, metres. Inferred: the property's name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_radius{
    .key = "PhysicsCharacterController.get_radius", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_radius"};
/// The player's capsule height, metres. Inferred: the property's name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_height{
    .key = "PhysicsCharacterController.get_height", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_height"};
/// The player's capsule centre, local to the rig. Inferred: the property's name.
inline constexpr pm::sdk::EntryDesc PhysicsCharacterController_get_center{
    .key = "PhysicsCharacterController.get_center", .kind = pm::sdk::Kind::Method,
    .owner = "PhysicsCharacterController", .name = "get_center"};

/// The PC interact's layer mask (read as its int): PCPropGrab.OnInteract's step casts a ray 1.8 m (a constant of its
/// code) from the centre of PCPropGrab.playerCam on these layers, triggers ignored, and acts only on what it hits that
/// has a PhotonObjectInteract, or a ChildPhotonInteract leading to one (a door, a light switch, an item). Anything
/// else, such as another player, gets nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc PCPropGrab_mask{.key = "PCPropGrab.mask", .kind = pm::sdk::Kind::Field,
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .owner = "PCPropGrab", .name = "mask", .type = "LayerMask"};

/// The camera the PC interact casts from, a Camera. Decompiled.
inline constexpr pm::sdk::EntryDesc PCPropGrab_playerCam{.key = "PCPropGrab.playerCam", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "PCPropGrab", .name = "playerCam", .type = "Camera"};

/// A collider's link to the interactable it belongs to (Assembly-CSharp, global namespace): the PC interact follows it
/// to the PhotonObjectInteract. Decompiled.
inline constexpr pm::sdk::EntryDesc ChildPhotonInteract{.key = "ChildPhotonInteract",
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .name = "ChildPhotonInteract", .assembly = "Assembly-CSharp",
                                                        .ns = "", .base = &MonoBehaviour};

/// The PC player's look on its own body (Assembly-CSharp, global namespace): in LateUpdate it bends the spine and head
/// toward where the camera looks. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc PCLook{.key = "PCLook", .name = "PCLook", .assembly = "Assembly-CSharp", .ns = "",
                                           .base = &MonoBehaviour};

/// The VR player's investigator (Assembly-CSharp, global namespace), a Player. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc VRPlayer{.key = "VRPlayer", .name = "VRPlayer", .assembly = "Assembly-CSharp",
                                            .ns = "", .base = &Player};

/// The left hand's IK target, a Transform that follows the left controller. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc VRPlayer_leftHandIK{.key = "VRPlayer.leftHandIK", .kind = pm::sdk::Kind::Field,
                                                        .owner = "VRPlayer", .name = "leftHandIK"};

/// The right hand's IK target, a Transform that follows the right controller. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc VRPlayer_rightHandIK{.key = "VRPlayer.rightHandIK", .kind = pm::sdk::Kind::Field,
                                                         .owner = "VRPlayer", .name = "rightHandIK"};

}  // namespace pm::map

/// @file
/// @brief The game API for the investigators (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (false, null) without touching the game. A
/// Player it returns is a managed reference: keep it across frames only in a pm::api::Held.
namespace phasmo {
/// @addtogroup players
/// @{

/// The game's networking hub. `network` below is the Network.
struct Network {
  /// The one Network: get_Instance(), or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Network_get_Instance, pm::api::Ref<pm::map::Network>()> get_Instance{};
  /// The room's players, a List<PlayerSpot>: playerSpots.get(network), read as a pm::api::List.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::Network_playerSpots, pm::api::Object> playerSpots{};
  /// This client's investigator, a Player: localPlayer.get(network), or null. A write goes through the
  /// GC write barrier. Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::Network_localPlayer, pm::api::Object> localPlayer{};
  /// This client's player rig, a LocalPlayer: localRig.get(network). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Network_localRig, pm::api::Ref<pm::map::LocalPlayer>> localRig{};
  /// Set by every scene change the game makes on its way out of a level: sceneChanging.set(network, true).
  /// Unresolved: false, writes nothing.
  static constexpr pm::api::Field<pm::map::Network_sceneChanging, bool> sceneChanging{};
  /// GetPlayerFromPhotonPlayer(network, photonPlayer): the Player in that Photon player's spot, or
  /// Network.localPlayer when no spot holds them (map note). Unresolved: null.
  static constexpr pm::api::Method<pm::map::Network_GetPlayerFromPhotonPlayer, pm::api::Object(pm::api::Ref<pm::map::Network>, pm::api::Object)>
      GetPlayerFromPhotonPlayer{};
  /// GetSpotPlayerFromPhotonPlayer(network, photonPlayer): that Photon player's PlayerSpot, or null (map
  /// note). A mod hooks it: Network.GetPlayerFromPhotonPlayer calls it. Unresolved: null.
  static constexpr pm::api::Method<pm::map::Network_GetSpotPlayerFromPhotonPlayer,
                                   pm::api::Ref<pm::map::PlayerSpot>(pm::api::Ref<pm::map::Network>, pm::api::Object)>
      GetSpotPlayerFromPhotonPlayer{};
  /// OnPlayerLeftRoom(network, photonPlayer): Photon's callback for a leaver (map note). A mod hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Network_OnPlayerLeftRoom, void(pm::api::Ref<pm::map::Network>, pm::api::Object)> OnPlayerLeftRoom{};
};

/// The scene's multiplayer helper.
struct MultiplayerController {
  /// The scene's controller: instance.get(), or null. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::MultiplayerController_instance, pm::api::Ref<pm::map::MultiplayerController>> instance{};
  /// Where the local investigator starts a contract, a List<Transform>: spawnPoints.get(controller). Unresolved: null.
  static constexpr pm::api::Field<pm::map::MultiplayerController_spawnPoints, pm::api::Object> spawnPoints{};
};

/// This client's player rig. `rig` below is the LocalPlayer.
struct LocalPlayer {
  /// TeleportPlayer(rig, &position): the game's own move of the local investigator, by the rig's root; it switches
  /// the rig's PhysicsCharacterController back on. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LocalPlayer_TeleportPlayer, void(pm::api::Ref<pm::map::LocalPlayer>, const Vector3*)> TeleportPlayer{};
  /// Its networked investigator, a Player: player.get(rig). A write goes through the GC write barrier.
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::LocalPlayer_player, pm::api::Object> player{};
  /// Its Camera: camera.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPlayer_camera, pm::api::Ref<pm::map::Camera>> camera{};
  /// Its journal page controller, a JournalController: journalController.get(rig). This is what
  /// PCCanvas.PausePlayerInPlace checks on release: a rig this answers null for has no journal, and the
  /// release throws there. Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPlayer_journalController, pm::api::Ref<pm::map::JournalController>> journalController{};
  /// Its walking speed in metres a second, before the multiplier: walkSpeed.get(rig). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LocalPlayer_walkSpeed, float> walkSpeed{};
  /// Its sprinting speed in metres a second, before the multiplier, whether sprinting is on or not:
  /// sprintSpeed.get(rig). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LocalPlayer_sprintSpeed, float> sprintSpeed{};
  /// The factor both speeds are multiplied by on this client: speedMultiplier.get(rig). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::LocalPlayer_speedMultiplier, float> speedMultiplier{};
  /// Its stamina, a PlayerStamina: stamina.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPlayer_stamina, pm::api::Ref<pm::map::PlayerStamina>> stamina{};
  /// The rig's PlayerInput: playerInput.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPlayer_playerInput, pm::api::Ref<pm::map::PlayerInput>> playerInput{};
  /// Whether its player has died, on this client only (others read Player.isDead): isDead.get(rig). Unresolved:
  /// false.
  static constexpr pm::api::Field<pm::map::LocalPlayer_isDead, bool> isDead{};
  /// The rig's PhysicsCharacterController: characterController.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPlayer_characterController, pm::api::Ref<pm::map::PhysicsCharacterController>> characterController{};
};

/// A player's stamina on its own client. `stamina` below is the PlayerStamina.
struct PlayerStamina {
  /// Whether the player may sprint now (cleared as the stamina runs out): canSprint.get(stamina). Only the
  /// player's own client holds it. Unresolved: false.
  static constexpr pm::api::Field<pm::map::PlayerStamina_canSprint, bool> canSprint{};
};

/// An investigator's own voice and sounds. `audio` below is the PlayerAudio.
struct PlayerAudio {
  /// IsTalkingLoudly(audio, loudness): whether the player talks above `loudness` (the game passes 0.1) or on a
  /// walkie-talkie. A true answer clears the loudness it read (map note). Unresolved: false.
  static constexpr pm::api::Method<pm::map::PlayerAudio_IsTalkingLoudly, bool(pm::api::Ref<pm::map::PlayerAudio>, float)> IsTalkingLoudly{};
  /// How loud the player talks, the value IsTalkingLoudly compares: loudness.get(audio). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PlayerAudio_loudness, float> loudness{};
  /// GetPlayerVoiceAudioSource(audio): the positional voice's AudioSource, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PlayerAudio_GetPlayerVoiceAudioSource,
                                   pm::api::Ref<pm::map::AudioSource>(pm::api::Ref<pm::map::PlayerAudio>)>
      GetPlayerVoiceAudioSource{};
};

/// One player of the room. `spot` below is the PlayerSpot.
struct PlayerSpot {
  /// The type: is(obj). Unresolved: false.
  static constexpr pm::api::Type<pm::map::PlayerSpot> type{};
  /// Its Photon player: photonPlayer.get(spot). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerSpot_photonPlayer, pm::api::Object> photonPlayer{};
  /// Its investigator, a Player: player.get(spot), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerSpot_player, pm::api::Object> player{};
  /// The level the lobby board shows: level.get(spot). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PlayerSpot_level, int32_t> level{};
  /// The prestige the lobby board shows: prestige.get(spot). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PlayerSpot_prestige, int32_t> prestige{};
};

/// The kinds of death a kill gives: the numbers Player::BeginDeathSequence takes.
namespace DeathType {
/// HangedMan(): the Hanged Man tarot card's. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DeathType_HangedMan> HangedMan{};
/// SnappedNeck(): a hunt's kill's, and the Monkey Paw's revive wish's. Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DeathType_SnappedNeck> SnappedNeck{};
}  // namespace DeathType

/// An investigator. `player` below is the Player.
struct Player {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Player> type{};
  /// Whether it is dead: isDead.get(player). Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::Player_isDead, bool> isDead{};
  /// Whether a kill of it has started and not finished: dying.get(player). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Player_dying, bool> dying{};
  /// The closet it hides in, or null: closet.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_closet, pm::api::Ref<pm::map::ClosetZone>> closet{};
  /// Its body object, a GameObject: body.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_body, pm::api::Ref<pm::map::GameObject>> body{};
  /// Whether a Monkey Paw wish marked it to be heard everywhere in hunts: wished.get(player).
  /// Unresolved: false.
  static constexpr pm::api::Field<pm::map::Player_wished, bool> wished{};
  /// Its stamina and movement, a NetworkedStamina: stamina.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_stamina, pm::api::Ref<pm::map::NetworkedStamina>> stamina{};
  /// The room it is in, or null: currentRoom.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_currentRoom, pm::api::Ref<pm::map::LevelRoom>> currentRoom{};
  /// The keys it holds, a List<KeyInfo> (Door.h): keys.get(player), read as a pm::api::List. Read it
  /// for the local player: Key.GrabbedKey fills that client's own. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_keys, pm::api::Object> keys{};
  /// The parts the ghost's sight looks for, a PlayerLOSTarget[], or null: losTargets.get(player).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_losTargets, pm::api::Object> losTargets{};
  /// Its PlayerSanity: sanity.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_sanity, pm::api::Ref<pm::map::PlayerSanity>> sanity{};
  /// Its JournalController: journal.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_journal, pm::api::Ref<pm::map::JournalController>> journal{};
  /// Its NetworkedPropGrab, what it carries: propGrab.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_propGrab, pm::api::Ref<pm::map::NetworkedPropGrab>> propGrab{};
  /// Its PlayerAudio, its voice: audio.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_audio, pm::api::Ref<pm::map::PlayerAudio>> audio{};
  /// Its body's Animator: animator.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_animator, pm::api::Ref<pm::map::Animator>> animator{};
  /// Its body's Renderer[]: renderers.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_renderers, pm::api::Object> renderers{};
  /// Its PlayerCharacterAnimations: characterAnimations.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Player_characterAnimations, pm::api::Ref<pm::map::PlayerCharacterAnimations>> characterAnimations{};
  /// RevivePlayer(player, flag): revives it once it is marked dead, by RPC to every client. A mod hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Player_RevivePlayer, void(pm::api::Ref<pm::map::Player>, bool)> RevivePlayer{};
  /// BeginDeathSequence(player, deathType): starts a kill of it, by RPC. A mod hooks it; call it only as the
  /// game does (map note). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Player_BeginDeathSequence, void(pm::api::Ref<pm::map::Player>, int32_t)> BeginDeathSequence{};
  /// ToggleFreezePlayerMovement(player, frozen): freezes or frees its movement on this client. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::Player_ToggleFreezePlayerMovement, void(pm::api::Ref<pm::map::Player>, bool)>
      ToggleFreezePlayerMovement{};
  /// NotifyOfDeathStart(player, info): the start of a kill, which the killer's client sends to every client. `info`
  /// is the message's PhotonMessageInfo, which the RPC system fills. A mod hooks it to see a death begin, before
  /// isDead is set; it never calls it (map note). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Player_NotifyOfDeathStart, void(pm::api::Ref<pm::map::Player>, pm::api::Object)> NotifyOfDeathStart{};
};

/// The local player on PC.
struct LocalPCPlayer {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::LocalPCPlayer> type{};
  /// Its HUD canvas, a PCCanvas: canvas.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPCPlayer_canvas, pm::api::Ref<pm::map::PCCanvas>> canvas{};
  /// Its PCControls: controls.get(localPlayer). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPCPlayer_controls, pm::api::Ref<pm::map::PCControls>> controls{};
  /// Its movement controller: firstPersonController.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LocalPCPlayer_firstPersonController, pm::api::Ref<pm::map::FirstPersonController>> firstPersonController{};
  /// UpdatePlayerSpeed(player): applies its walk or sprint speed. A mod usually hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::LocalPCPlayer_UpdatePlayerSpeed, void(pm::api::Ref<pm::map::LocalPCPlayer>)> UpdatePlayerSpeed{};
  /// SetPlayerSpeedMultiplier(player, factor): sets its speed multiplier on this client. A mod usually hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LocalPCPlayer_SetPlayerSpeedMultiplier, void(pm::api::Ref<pm::map::LocalPCPlayer>, float)>
      SetPlayerSpeedMultiplier{};
  /// ResetPlayerSpeedMultiplier(player): puts its speed multiplier back to the copy kept from the contract's
  /// difficulty and applies the speed again, on this client. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LocalPCPlayer_ResetPlayerSpeedMultiplier, void(pm::api::Ref<pm::map::LocalPCPlayer>)>
      ResetPlayerSpeedMultiplier{};
};

/// The PC rig's movement controller. `rig` below is the FirstPersonController.
struct FirstPersonController {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::FirstPersonController> type{};
  /// The Move input as the game last read it, two floats: moveInput.get(rig). Unresolved: zero.
  static constexpr pm::api::Field<pm::map::FirstPersonController_moveInput, Vector2> moveInput{};
  /// Whether the player asks to sprint (Run held, or toggled on): running.get(rig). Unresolved: false.
  static constexpr pm::api::Field<pm::map::FirstPersonController_running, bool> running{};
  /// EnableMovement(rig, on): the walk-and-look switch. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::FirstPersonController_EnableMovement, void(pm::api::Ref<pm::map::FirstPersonController>, bool)> EnableMovement{};
};

/// The local player's HUD.
struct PCCanvas {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PCCanvas> type{};
  /// Its crosshair, an Image: crosshair.get(canvas). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PCCanvas_crosshair, pm::api::Ref<pm::map::Image>> crosshair{};
  /// PausePlayerInPlace(canvas, paused): holds the local player still while an overlay is up, or lets them
  /// go again; the game uses it for the journal and the voice overlay. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PCCanvas_PausePlayerInPlace, void(pm::api::Ref<pm::map::PCCanvas>, bool)> PausePlayerInPlace{};
};

/// The investigator's physical step on PC: walls, floors and gravity. `controller` below is the
/// PhysicsCharacterController, found on the local rig with Component.GetComponentInChildren.
struct PhysicsCharacterController {
  /// The type: klass(), is(obj), type_object() for GetComponent. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PhysicsCharacterController> type{};
  /// Move(controller, displacement): the frame's walk, in metres; the next physical step applies it. A mod
  /// hooks it to read or replace the walk. Local only. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_Move, void(pm::api::Ref<pm::map::PhysicsCharacterController>, Vector3)> Move{};
  /// get_Rigidbody(controller): the body its step moves. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_Rigidbody,
                                   pm::api::Ref<pm::map::Rigidbody>(pm::api::Ref<pm::map::PhysicsCharacterController>)> get_Rigidbody{};
  /// get_CapsuleCollider(controller): the investigator's collider. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_CapsuleCollider,
                                   pm::api::Ref<pm::map::CapsuleCollider>(pm::api::Ref<pm::map::PhysicsCharacterController>)>
      get_CapsuleCollider{};
  /// What blocks the player, as an int layer mask: collisionMask.get(controller). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PhysicsCharacterController_collisionMask, int32_t> collisionMask{};
  /// get_groundLayers(controller): what the player stands on, as an int layer mask. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_groundLayers, int32_t(pm::api::Ref<pm::map::PhysicsCharacterController>)>
      get_groundLayers{};
  /// get_radius(controller): the capsule's radius, metres. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_radius, float(pm::api::Ref<pm::map::PhysicsCharacterController>)> get_radius{};
  /// get_height(controller): the capsule's height, metres. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_height, float(pm::api::Ref<pm::map::PhysicsCharacterController>)> get_height{};
  /// get_center(controller): the capsule's centre, local to the rig. Unresolved: zero.
  static constexpr pm::api::Method<pm::map::PhysicsCharacterController_get_center, Vector3(pm::api::Ref<pm::map::PhysicsCharacterController>)> get_center{};
};

/// The body's animation scripts.
struct PlayerCharacterAnimations {
  /// The right hand's anchor, a Transform: parentingTargetRightHand.get(anims). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerCharacterAnimations_parentingTargetRightHand, pm::api::Object>
      parentingTargetRightHand{};
  /// The left hand's anchor, a Transform: parentingTargetLeftHand.get(anims). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerCharacterAnimations_parentingTargetLeftHand, pm::api::Object>
      parentingTargetLeftHand{};
};

/// A collider's link to its interactable.
struct ChildPhotonInteract {
  /// The type: type_object() for GetComponent. Unresolved: null.
  static constexpr pm::api::Type<pm::map::ChildPhotonInteract> type{};
};

/// The PC player's hands.
struct PCPropGrab {
  /// The type: type_object() for GetComponentInChildren. Unresolved: null.
  static constexpr pm::api::Type<pm::map::PCPropGrab> type{};
  /// The interact ray's layers, as an int: mask.get(grab). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::PCPropGrab_mask, int32_t> mask{};
  /// The camera the interact ray starts from: playerCam.get(grab). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PCPropGrab_playerCam, pm::api::Ref<pm::map::Camera>> playerCam{};
  /// GetItemInCameraSlot(grab): the held item, a PhotonObjectInteract, or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::PCPropGrab_GetItemInCameraSlot, pm::api::Ref<pm::map::PhotonObjectInteract>(pm::api::Ref<pm::map::PCPropGrab>)> GetItemInCameraSlot{};
  /// ParentItemOnLocalPlayer(grab, item): puts the item back in the camera slot. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PCPropGrab_ParentItemOnLocalPlayer, void(pm::api::Ref<pm::map::PCPropGrab>, pm::api::Ref<pm::map::PhotonObjectInteract>)>
      ParentItemOnLocalPlayer{};
};

/// The PC player's look on its own body.
struct PCLook {
  /// The type: type_object() for GetComponentInChildren. Unresolved: null.
  static constexpr pm::api::Type<pm::map::PCLook> type{};
};

/// The VR player's investigator.
struct VRPlayer {
  /// The type: is(player). Unresolved: false.
  static constexpr pm::api::Type<pm::map::VRPlayer> type{};
  /// The left hand's IK target, a Transform: leftHandIK.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::VRPlayer_leftHandIK, pm::api::Object> leftHandIK{};
  /// The right hand's IK target, a Transform: rightHandIK.get(player). Unresolved: null.
  static constexpr pm::api::Field<pm::map::VRPlayer_rightHandIK, pm::api::Object> rightHandIK{};
  /// Teleport(player, &point): moves the local VR rig there, on this client. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::VRPlayer_Teleport, void(pm::api::Ref<pm::map::VRPlayer>, const Vector3*)> Teleport{};
};

/// What an investigator carries. `grab` below is a NetworkedPropGrab.
struct NetworkedPropGrab {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::NetworkedPropGrab> type{};
  /// The items it carries, a List<PhotonObjectInteract>: inventory.get(grab), read as a pm::api::List.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::NetworkedPropGrab_inventory, pm::api::Object> inventory{};
};

/// The Winter's Jest present. `present` below is the KrampusPresent.
struct KrampusPresent {
  /// GetFurthestSpawnPositionFromPlayers(present): the spawn point furthest from the room's players (map note). A mod
  /// hooks it. Unresolved: the zero vector.
  static constexpr pm::api::Method<pm::map::KrampusPresent_GetFurthestSpawnPositionFromPlayers, Vector3(pm::api::Ref<pm::map::KrampusPresent>)>
      GetFurthestSpawnPositionFromPlayers{};
};

/// A player's cosmetics. `cosmetics` below is the PlayerCosmetics.
struct PlayerCosmetics {
  /// The type: type_object() for Component.GetComponentInChildren. Unresolved: null.
  static constexpr pm::api::Type<pm::map::PlayerCosmetics> type{};
  /// playerAnimator.get(cosmetics): the body's Animator. Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerCosmetics_playerAnimator, pm::api::Ref<pm::map::Animator>> playerAnimator{};
};

/// This client's own journal input. `input` below is the LocalJournalController.
struct LocalJournalController {
  /// OpenCloseJournal(input, flag): the journal key's open or close, through JournalController.OpenCloseJournal. A mod
  /// hooks it to act before the journal opens. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LocalJournalController_OpenCloseJournal, void(pm::api::Ref<pm::map::LocalJournalController>, bool)>
      OpenCloseJournal{};
};

/// The player's character model. `character` below is the PlayerCharacter.
struct PlayerCharacter {
  /// SetPlayerBodyType(character, position): takes a body type from the player at that position, or with -1 from
  /// this client's own player (map note). A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayerCharacter_SetPlayerBodyType, void(pm::api::Ref<pm::map::PlayerCharacter>, int32_t)>
      SetPlayerBodyType{};
  /// The type: type_object() for Component.GetComponentInChildren. Unresolved: null.
  static constexpr pm::api::Type<pm::map::PlayerCharacter> type{};
  /// The head's Renderer[]: headRenderersToTurnOff.get(character). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PlayerCharacter_headRenderersToTurnOff, pm::api::Object> headRenderersToTurnOff{};
  /// ChangeHeadObjectsShadowCastingModeShadowsOnly(character, shadowsOnly): the head shadows only (true) or drawn
  /// (false), every head renderer (map note: what calls it). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayerCharacter_ChangeHeadObjectsShadowCastingModeShadowsOnly,
                                   void(pm::api::Ref<pm::map::PlayerCharacter>, bool)>
      ChangeHeadObjectsShadowCastingModeShadowsOnly{};
};

/// The voice recognition's phrase handler. `listener` below is the PhraseListenerController.
struct PhraseListenerController {
  /// OnPhraseRecognized(listener, phrase), `phrase` a System.String. A mod usually hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhraseListenerController_OnPhraseRecognized, void(pm::api::Ref<pm::map::PhraseListenerController>, pm::api::Object)>
      OnPhraseRecognized{};
  /// NetworkedReaction(listener): the ghost reacts to its name. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PhraseListenerController_NetworkedReaction, void(pm::api::Ref<pm::map::PhraseListenerController>)>
      NetworkedReaction{};
};

/// The PC player's input settings. `controls` below is the PCControls.
struct PCControls {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PCControls> type{};
  /// The InputActionAsset its actions come from: actions.get(controls). Unresolved: null.
  static constexpr pm::api::Field<pm::map::PCControls_actions, pm::api::Ref<pm::map::InputActionAsset>> actions{};
};

/// The level's sounds. `sounds` below is the SoundController.
struct SoundController {
  /// The type: klass(), is(obj). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::SoundController> type{};
  /// The level's one: instance.get(). Null outside a level. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::SoundController_instance, pm::api::Ref<pm::map::SoundController>> instance{};
  /// GetFloorAudioSnapshot(sounds, &position): the AudioMixerGroup a sound at `position` plays through, as the game's
  /// noises take theirs. Unresolved: null.
  static constexpr pm::api::Method<pm::map::SoundController_GetFloorAudioSnapshot, pm::api::Object(pm::api::Ref<pm::map::SoundController>, const Vector3*)>
      GetFloorAudioSnapshot{};
  /// GetPlayersAudioGroup(sounds, photonPlayer): the AudioMixerGroup that Photon player's own sounds play through, by
  /// the floor the game has them on (map note). Unresolved: null.
  static constexpr pm::api::Method<pm::map::SoundController_GetPlayersAudioGroup, pm::api::Object(pm::api::Ref<pm::map::SoundController>, pm::api::Object)>
      GetPlayersAudioGroup{};
  /// The shared door slam clips, an AudioClip[]: doorSlamClips.get(sounds). Unresolved: null.
  static constexpr pm::api::Field<pm::map::SoundController_doorSlamClips, pm::api::Object> doorSlamClips{};
};

/// @}
}  // namespace phasmo
