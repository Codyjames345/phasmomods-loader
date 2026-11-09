#pragma once

// The contract's way in and out: GameController's wait for every player to load and its way back to
// the menu, and ExitLevel, whose networked exit ends the contract on every client. GameController's
// type and its other methods are in LocalisationSystem.h. Their names are real; GameController's static,
// and its loaded flag are obfuscated, so they are found by their types and facts.
// Notes read from decompiled code are from build 25811160.

#include "LocalisationSystem.h"
#include "entry.h"
#include "game_api.h"

namespace pm::map {

/// The level's GameController: the class's one static field of its own type. Null, or a destroyed one,
/// outside a level. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc GameController_instance{
    .key = "GameController.instance", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kStatic,
    .owner = "GameController", .type = "@GameController"};

/// Takes this client back to the menu scene: it marks Network's scene change and loads the menu.
/// ExitLevel.Exit calls it at the end of a contract. Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_LoadBackToMenu{
    .key = "GameController.LoadBackToMenu", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameController", .name = "LoadBackToMenu"};

/// The lobby-size item switch (Assembly-CSharp, global namespace), a MonoBehaviour that turns items on
/// for the number of players. Found for the fact that names allPlayersLoaded. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCountItemEnabler{.key = "PlayerCountItemEnabler",
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .name = "PlayerCountItemEnabler",
                                                          .assembly = "Assembly-CSharp", .ns = ""};

/// Its Start: it reads GameController.instance's loaded flag and, while it is clear, waits for the
/// controller's loaded event instead. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerCountItemEnabler_Start{
    .key = "PlayerCountItemEnabler.Start", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "PlayerCountItemEnabler", .name = "Start"};

// PlayerCountItemEnabler.Start reads one bool of GameController, the loaded flag, through the class's
// static: another type's method, so the fact matches that offset through any register.
inline constexpr pm::sdk::Fact GameController_allPlayersLoaded_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "PlayerCountItemEnabler.Start"}};

/// Whether every player has loaded into the level. GameController's load coroutine waits until each of
/// Network.playerSpots holds a live Player, then sets it once and raises the class's loaded event;
/// LiftButton.AttemptUse refuses the truck's button until it is set.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_allPlayersLoaded{
    .key = "GameController.allPlayersLoaded", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameController", .type = "bool",
    .facts = GameController_allPlayersLoaded_facts, .access = pm::sdk::Visibility::Public};

/// The truck's exit (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc ExitLevel{.key = "ExitLevel", .confidence = pm::sdk::Confidence::Decompiled,
                                              .name = "ExitLevel", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* ExitLevel_Exit_params[] = {"PhotonMessageInfo"};

/// The contract's end when the truck leaves, an RPC every client runs (GameController.Exit is the end
/// when every player has died). In a room that is online it first refuses a call from any sender but the room's first actor (the lowest
/// actor number among PhotonNetwork.get_PlayerList's players). A refusal is not silent: the game flags the
/// sender, and on the sender's own client it counts against the account (sdk/systems/photon.md). It marks
/// Network's scene change, syncs the master's stats, sets LevelValues.completed, then takes one of two paths. While
/// LevelValues.eventsBlocked is set it clears completed, loads the main menu and disconnects.
/// Otherwise it checks the missions and challenges and goes back to the lobby through
/// GameController.LoadBackToMenu. The stats sync and the lobby path read this client's investigator.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ExitLevel_Exit{.key = "ExitLevel.Exit", .kind = pm::sdk::Kind::Method,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "ExitLevel", .name = "Exit", .params = ExitLevel_Exit_params};

/// The contract's end when every player has died, an RPC every client runs. GameController.PlayerDied
/// starts it on the master, once every player's Player is dead, on GameController's own view. It has the
/// same sender check as ExitLevel.Exit, then sets LevelValues.completed, the death status and a mission
/// status by the room's size, runs CheckMissions and CheckChallenges, closes this client's journal if it
/// has an investigator, and goes back to the menu as LoadBackToMenu does. CheckMissions and CheckChallenges
/// read this client's investigator. Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_Exit{.key = "GameController.Exit", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GameController", .name = "Exit",
                                                        .params = ExitLevel_Exit_params};

/// The local player's main objective, an async void static: it completes the identify objective when
/// the journal's choice names the ghost, and sets LevelStats' flag for it. ExitLevel.Exit calls it, after
/// the mission status and before CheckChallenges. It reads this client's investigator. Decompiled.
inline constexpr pm::sdk::EntryDesc ExitLevel_CheckMissions{
    .key = "ExitLevel.CheckMissions", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "ExitLevel", .name = "CheckMissions"};

/// The contract's challenge progress for this client (ChallengesController), a static. ExitLevel.Exit
/// calls it after CheckMissions and before the objective counts. It reads this client's investigator.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc ExitLevel_CheckChallenges{
    .key = "ExitLevel.CheckChallenges", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "ExitLevel", .name = "CheckChallenges"};

/// The loading screen's controller (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc LoadingController{.key = "LoadingController",
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .name = "LoadingController", .assembly = "Assembly-CSharp",
                                                      .ns = ""};

/// The one LoadingController: the class's one static field of its own type. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc LoadingController_instance{
    .key = "LoadingController.instance", .kind = pm::sdk::Kind::Field, .flags = pm::sdk::kStatic,
    .owner = "LoadingController", .type = "@LoadingController"};

/// Whether the loading screen is ready: the class's one bool. PauseMenuController's leave waits for it, and
/// a frame more, before it disconnects. Decompiled.
inline constexpr pm::sdk::EntryDesc LoadingController_ready{.key = "LoadingController.ready",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "LoadingController",
                                                            .type = "bool"};

inline constexpr const char* LoadingController_LoadScene_params[] = {"bool", "int", "string", "bool"};

/// Shows the loading screen and loads a scene: to the main menu with (false, 0, "", true), as
/// PauseMenuController.Leave calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc LoadingController_LoadScene{
    .key = "LoadingController.LoadScene", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LoadingController", .name = "LoadScene", .params = LoadingController_LoadScene_params};

/// The pause menu's buttons (Assembly-CSharp, global namespace), in a player's own journal. Decompiled.
inline constexpr pm::sdk::EntryDesc PauseMenuController{.key = "PauseMenuController",
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .name = "PauseMenuController",
                                                        .assembly = "Assembly-CSharp", .ns = ""};

/// The Leave button: it clears LevelValues.completed, sets Network.sceneChanging, silences the listener and
/// loads the main menu, and its coroutine disconnects Photon once the loading screen is ready. It reads the
/// local investigator. Decompiled.
inline constexpr pm::sdk::EntryDesc PauseMenuController_Leave{
    .key = "PauseMenuController.Leave", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PauseMenuController", .name = "Leave"};

/// The Quit button: it clears LevelValues.completed and quits the application. It reads the local
/// investigator. Decompiled.
inline constexpr pm::sdk::EntryDesc PauseMenuController_Quit{
    .key = "PauseMenuController.Quit", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PauseMenuController", .name = "Quit"};

/// The truck's door button (Assembly-CSharp, global namespace), a MonoBehaviourPun. Decompiled.
inline constexpr pm::sdk::EntryDesc LiftButton{.key = "LiftButton", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "LiftButton", .assembly = "Assembly-CSharp", .ns = ""};

inline constexpr const char* LiftButton_AttemptUse_params[] = {"PhotonMessageInfo"};

/// A player's press of the button, an RPC the master runs: it refuses it until
/// GameController.allPlayersLoaded and goes on to the networked use. Decompiled.
inline constexpr pm::sdk::EntryDesc LiftButton_AttemptUse{
    .key = "LiftButton.AttemptUse", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LiftButton", .name = "AttemptUse", .params = LiftButton_AttemptUse_params};

inline constexpr const char* LiftButton_NetworkedUse_params[] = {"bool", "PhotonMessageInfo"};

/// The door's use on every client, by RPC: the truck's first use warps the ghost to its favourite room
/// (GhostController.WarpGhostToFavouriteRoom), which only this method and its uncalled copies do. Tracked by
/// the host (pm::api::inside). Decompiled.
inline constexpr pm::sdk::EntryDesc LiftButton_NetworkedUse{
    .key = "LiftButton.NetworkedUse", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LiftButton", .name = "NetworkedUse", .params = LiftButton_NetworkedUse_params};

/// Whether the doors are closed. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc LiftButton_isClosed{.key = "LiftButton.isClosed", .kind = pm::sdk::Kind::Field,
                                                        .owner = "LiftButton", .name = "isClosed", .type = "bool"};

inline constexpr const char* Application_int[] = {"int"};

/// Unity's application (UnityEngine.CoreModule). Inferred: Unity's documented API.
inline constexpr pm::sdk::EntryDesc Application{.key = "Application", .name = "Application",
                                                .assembly = "UnityEngine.CoreModule", .ns = "UnityEngine"};

/// Quits the application with an exit code: Unity's Quit() is this with 0 (both reach one engine function),
/// and a name alone cannot tell the two overloads apart. Decompiled.
inline constexpr pm::sdk::EntryDesc Application_Quit{.key = "Application.Quit", .kind = pm::sdk::Kind::Method,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .flags = pm::sdk::kStatic, .owner = "Application", .name = "Quit",
                                                     .params = Application_int};

/// A player's death reaches the controller (it has no direct caller, so how is not shown). Its body runs on the master only: once every spot's live Player is dead, it starts
/// the wait that sends GameController.Exit to every client (sdk/systems/contract.md). Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_PlayerDied{
    .key = "GameController.PlayerDied", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GameController", .name = "PlayerDied"};

/// How many players are dead: of Network.playerSpots, the spots whose Photon player is still in the room
/// (PhotonNetwork.get_PlayerList) and whose Player is marked dead (Player.isDead). A spot without a Player
/// throws. It counts on any client and sends nothing. Its live callers are the Monkey Paw's wish (a revive
/// wish with nobody dead only twitches the paw, its use unspent) and, for the Onryo only, a roll and a random
/// wait of FireSource's flame; the rest are uncalled decoys (sdk/systems/players.md). Decompiled.
inline constexpr pm::sdk::EntryDesc GameController_AmountOfDeadPlayers{
    .key = "GameController.AmountOfDeadPlayers", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameController", .name = "AmountOfDeadPlayers"};

/// Starts the truck's leaving: its coroutine sends ExitLevel.Exit, and LiftButton.NetworkedUse starts it only on the master
/// with the doors closing. Decompiled.
inline constexpr pm::sdk::EntryDesc ExitLevel_StartAttemptExitLevel{
    .key = "ExitLevel.StartAttemptExitLevel", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "ExitLevel", .name = "StartAttemptExitLevel"};

/// A player's contract statistics, on their Player. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerStats{.key = "PlayerStats", .confidence = pm::sdk::Confidence::Decompiled,
    .name = "PlayerStats", .assembly = "Assembly-CSharp", .ns = ""};

/// Sends this player's statistics to the others by RPC, running nothing locally. Only ExitLevel.Exit calls it, on the master. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerStats_SyncStats{
    .key = "PlayerStats.SyncStats", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerStats", .name = "SyncStats"};

/// The receiver of PlayerStats.SyncStats: it has the first-actor sender check with a refusal (sdk/systems/photon.md), and
/// copies the bytes without validating them. Decompiled.
inline constexpr pm::sdk::EntryDesc PlayerStats_SyncStatsNetworked{
    .key = "PlayerStats.SyncStatsNetworked", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "PlayerStats", .name = "SyncStatsNetworked"};

/// The reward screen back in the lobby. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager{.key = "RewardManager", .confidence = pm::sdk::Confidence::Decompiled,
    .name = "RewardManager", .assembly = "Assembly-CSharp", .ns = ""};

/// Copies GameplaySettings.PlayerDied for its results when LevelValues says the contract was played and completed. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_Awake{
    .key = "RewardManager.Awake", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .name = "Awake"};

/// The screen's skip button: one of SingleplayerProfile.AddXPAndFunds's three callers, so a player can reach the pay at once. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_Skip{
    .key = "RewardManager.Skip", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .name = "Skip"};

inline constexpr const char* RewardManager_Results_fieldTypes[] = {"int", "AsyncVoidMethodBuilder", "@RewardManager",
                                                                  "TaskAwaiter"};

/// The reward screen's results, an async method's state machine (a struct nested in RewardManager, name
/// obfuscated): the screen's one nested struct of exactly a state, an AsyncVoidMethodBuilder, the screen and
/// a TaskAwaiter. Its first step runs the screen's challenge and unlock checks, saves the screen's money into
/// the player's saved money, plays the totals, progresses the money and experience challenges, clears
/// GameplaySettings.PlayerDied and starts RewardManager.EventRewards (sdk/systems/contract.md). Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_Results{.key = "RewardManager.Results",
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .flags = pm::sdk::kExactFields | pm::sdk::kCompilerGenerated,
                                                          .owner = "RewardManager",
                                                          .fieldTypes = RewardManager_Results_fieldTypes};

/// Runs the results from where they last waited: its first run does all the screen's own reward work.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_Results_MoveNext{
    .key = "RewardManager.Results.MoveNext", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RewardManager.Results", .name = "MoveNext"};

inline constexpr const char* RewardManager_EventRewards_fieldTypes[] = {"AsyncTaskMethodBuilder", "@RewardManager"};

/// The seasonal event's part of the results, an async method's state machine (a struct nested in
/// RewardManager, name obfuscated): the screen's one nested struct with an AsyncTaskMethodBuilder. It saves the
/// event's score with the game's server (SeasonalEventController.SaveEventProgress), shows the themes and
/// cosmetics that unlocks, and then progresses the event's challenge. Its later steps run when the server
/// answers. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_EventRewards{.key = "RewardManager.EventRewards",
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .flags = pm::sdk::kCompilerGenerated,
                                                               .owner = "RewardManager",
                                                               .fieldTypes = RewardManager_EventRewards_fieldTypes};

/// Runs the event's part from where it last waited. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_EventRewards_MoveNext{
    .key = "RewardManager.EventRewards.MoveNext", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "RewardManager.EventRewards", .name = "MoveNext"};

/// The screen's next button: it finishes the screen. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_Next{
    .key = "RewardManager.Next", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .name = "Next"};

/// .NET's builder of an async void method (mscorlib). Decompiled.
inline constexpr pm::sdk::EntryDesc AsyncVoidMethodBuilder{.key = "AsyncVoidMethodBuilder",
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .name = "AsyncVoidMethodBuilder", .assembly = "mscorlib",
                                                           .ns = "System.Runtime.CompilerServices"};

/// What every async void method calls first. Found so that RewardManager.StartResults can be told apart. Decompiled.
inline constexpr pm::sdk::EntryDesc AsyncVoidMethodBuilder_Create{
    .key = "AsyncVoidMethodBuilder.Create", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .flags = pm::sdk::kStatic, .owner = "AsyncVoidMethodBuilder",
    .name = "Create"};

inline constexpr pm::sdk::Fact RewardManager_StartResults_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "AsyncVoidMethodBuilder.Create"}};

/// Starts the screen's results (name obfuscated): RewardManager's async void method, which runs the first step of
/// RewardManager.Results before it returns. RewardManager.Awake adds it as a listener of an event of the screen's
/// SingleplayerProfile, raised once the card has loaded its player. Nothing else starts the results. The body it
/// listens with is one of two twins of the canonical one, so a hook takes all of them. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_StartResults{
    .key = "RewardManager.StartResults", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .type = "void", .facts = RewardManager_StartResults_facts};

inline constexpr pm::sdk::Fact RewardManager_BuildRows_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "RewardManager.Results.MoveNext"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "LevelValues.GetInvestigationBonusReward"}};

/// Builds the screen's rows and adds each row's money and experience to the screen's totals (name obfuscated): the
/// results' first step calls it, and saves the money total straight after it returns (sdk/systems/contract.md).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_BuildRows{
    .key = "RewardManager.BuildRows", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .type = "void", .facts = RewardManager_BuildRows_facts};

inline constexpr const char* RewardManager_SetTotals_params[] = {"int", "int"};

inline constexpr pm::sdk::Fact RewardManager_SetTotals_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "RewardManager.BuildRows"}};

/// Sets the screen's money and experience totals to new values and joins a count from the old to the new to the
/// screen's animation (name obfuscated). RewardManager.BuildRows calls it for its multipliers, so the shown total ends
/// at what the results save. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_SetTotals{
    .key = "RewardManager.SetTotals", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .type = "void", .params = RewardManager_SetTotals_params,
    .facts = RewardManager_SetTotals_facts};

/// The seasonal events (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc SeasonalEventController{.key = "SeasonalEventController",
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .name = "SeasonalEventController",
                                                            .assembly = "Assembly-CSharp", .ns = ""};

/// The seasonal event running now. Decompiled.
inline constexpr pm::sdk::EntryDesc SeasonalEventController_GetSeasonalEvent{
    .key = "SeasonalEventController.GetSeasonalEvent", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SeasonalEventController", .name = "GetSeasonalEvent"};

inline constexpr const char* SeasonalEventController_SaveEventProgress_params[] = {"Dictionary<ScoreType, uint>"};

/// Sends an event's score to the game's server and answers its response. RewardManager.EventRewards and a method of
/// one seasonal puzzle call it. It builds a request from the score it is given and awaits the server. Decompiled.
inline constexpr pm::sdk::EntryDesc SeasonalEventController_SaveEventProgress{
    .key = "SeasonalEventController.SaveEventProgress", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "SeasonalEventController", .name = "SaveEventProgress",
    .params = SeasonalEventController_SaveEventProgress_params};

inline constexpr const char* RewardManager_EventScore_fieldTypes[] = {"SeasonalEventType",
                                                                      "Dictionary<ScoreType, uint>", "int"};

/// The score RewardManager.EventRow answers for RewardManager.EventRewards to send: the event, the score by type and
/// a total (a struct nested in RewardManager, name obfuscated). Decompiled. Its fields and their order are exact: they
/// fix the struct's size, which RewardEventRow copies, so a reordered or longer struct is not found.
inline constexpr pm::sdk::EntryDesc RewardManager_EventScore{.key = "RewardManager.EventScore",
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .flags = pm::sdk::kExactFields | pm::sdk::kFieldOrder,
                                                             .owner = "RewardManager",
                                                             .fieldTypes = RewardManager_EventScore_fieldTypes};

inline constexpr pm::sdk::Fact RewardManager_EventRow_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "RewardManager.Results.MoveNext"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "SeasonalEventController.GetSeasonalEvent"}};

/// The screen's seasonal event row and score (name obfuscated): a method the results' first step calls straight after
/// saving the money. It answers an empty Nullable when the contract had no seasonal event. Otherwise it shows the row
/// and answers what RewardManager.EventRewards then sends. The results run RewardManager.EventRewards only for a
/// filled answer. Decompiled.
inline constexpr pm::sdk::EntryDesc RewardManager_EventRow{
    .key = "RewardManager.EventRow", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "RewardManager", .type = "Nullable<@RewardManager.EventScore>",
    .facts = RewardManager_EventRow_facts};

/// The player's challenges (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc ChallengesController{.key = "ChallengesController",
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .name = "ChallengesController",
                                                         .assembly = "Assembly-CSharp", .ns = ""};

/// Adds an amount to every challenge of one type: the controller's three challenge lists and its one more
/// challenge. On a difficulty with a reward multiplier of 0 it can skip every type but three. It calls no
/// Steam method itself. Every challenge the reward screen progresses goes through it. Decompiled.
inline constexpr pm::sdk::EntryDesc ChallengesController_ChangeChallengeProgression{
    .key = "ChallengesController.ChangeChallengeProgression", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ChallengesController",
    .name = "ChangeChallengeProgression"};

/// The game's saved settings (Assembly-CSharp, global namespace, name obfuscated): the only class with a
/// static method that takes nothing and returns its nested GameplaySettings. Decompiled.
inline constexpr pm::sdk::EntryDesc GameSettings{.key = "GameSettings", .confidence = pm::sdk::Confidence::Decompiled,
                                                 .assembly = "Assembly-CSharp", .ns = "",
                                                 .type = "GameplaySettings"};

/// The settings that outlive a level's scene and carry its end to the reward screen, nested in GameSettings.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GameplaySettings{.key = "GameplaySettings",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "GameSettings", .name = "GameplaySettings",
                                                     .assembly = "Assembly-CSharp"};

/// The death status: GameController.Exit sets it on every client; ExitLevel.Exit sets it when this client's
/// investigator is dead. RewardManager.Awake copies it for the board, which then shows the death row, halves
/// the totals and, on a difficulty that loses items, resets the committed equipment (sdk/systems/contract.md).
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GameplaySettings_PlayerDied{
    .key = "GameplaySettings.PlayerDied", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GameplaySettings", .name = "PlayerDied", .type = "bool"};

/// The mission status, beside the death status: both ends and both pause buttons write it (1 when the contract
/// ended alone, 2 with others, among the values read). A MainManager method sends a signal for 1, 2 or 3 and
/// sets it back to 0 (sdk/systems/contract.md). Decompiled.
inline constexpr pm::sdk::EntryDesc GameplaySettings_MissionStatus{
    .key = "GameplaySettings.MissionStatus", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GameplaySettings", .name = "MissionStatus",
    .type = "int"};

inline constexpr pm::sdk::Fact GameSettings_gameplay_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GameController.Exit"}};

/// The GameplaySettings the game reads and writes: the getter GameController.Exit calls for both statuses.
/// Name obfuscated. Decompiled.
inline constexpr pm::sdk::EntryDesc GameSettings_gameplay{
    .key = "GameSettings.gameplay", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "GameSettings", .type = "@GameplaySettings",
    .facts = GameSettings_gameplay_facts};

}  // namespace pm::map

/// @file
/// @brief The game API for the contract's way in and out (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default without touching the game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// The level's controller. `controller` below is the GameController.
struct GameController {
  /// The level's GameController: instance.get(), or null; check it with pm::api::alive, since a
  /// destroyed one stays after the level. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::GameController_instance, void*> instance{};
  /// LoadBackToMenu(controller): back to the menu scene. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameController_LoadBackToMenu, void(void*)> LoadBackToMenu{};
  /// Exit(controller, &info): the contract's end when every player has died, on every client. A mod hooks
  /// it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GameController_Exit, void(void*, void*)> Exit{};
  /// AmountOfDeadPlayers(controller): how many players are dead. A mod hooks it. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::GameController_AmountOfDeadPlayers, int32_t(void*)> AmountOfDeadPlayers{};
  /// Whether every player has loaded: allPlayersLoaded.get(controller). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GameController_allPlayersLoaded, bool> allPlayersLoaded{};
};

/// The truck's exit.
struct ExitLevel {
  /// Exit(exit, &info): the contract's end on every client. A mod hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ExitLevel_Exit, void(void*, void*)> Exit{};
  /// CheckMissions(): the main objective's check at the contract's end. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::ExitLevel_CheckMissions, void()> CheckMissions{};
  /// CheckChallenges(): the challenges' check at the contract's end. A mod hooks it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::ExitLevel_CheckChallenges, void()> CheckChallenges{};
};

/// The loading screen's controller. `loading` below is the LoadingController.
struct LoadingController {
  /// The one LoadingController: instance.get(), or null. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::LoadingController_instance, void*> instance{};
  /// Whether the loading screen is ready: ready.get(loading). Unresolved: false.
  static constexpr pm::api::Field<pm::map::LoadingController_ready, bool> ready{};
  /// LoadScene(loading, flag, scene, sceneName, flag2): shows the loading screen and loads a scene.
  /// sceneName is a managed string. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LoadingController_LoadScene, void(void*, bool, int32_t, void*, bool)>
      LoadScene{};
};

/// The pause menu's buttons. `menu` below is the PauseMenuController. A mod hooks them.
struct PauseMenuController {
  /// Leave(menu): the game's own leave. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PauseMenuController_Leave, void(void*)> Leave{};
  /// Quit(menu): the game's own quit. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PauseMenuController_Quit, void(void*)> Quit{};
};

/// The truck's door button. `button` below is the LiftButton.
struct LiftButton {
  /// AttemptUse(button, &info), an RPC the master runs: a press of the button. A mod hooks it. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::LiftButton_AttemptUse, void(void*, void*)> AttemptUse{};
  /// NetworkedUse(button, flag, &info), by RPC on every client: a mod asks pm::api::inside whether one is
  /// running, the host tracking it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::LiftButton_NetworkedUse, void(void*, bool, void*)> NetworkedUse{};
  /// Whether the doors are closed: isClosed.get(button). Unresolved: false.
  static constexpr pm::api::Field<pm::map::LiftButton_isClosed, bool> isClosed{};
};

/// What RewardManager.EventRow answers: a Nullable of the score to send, 32 bytes in this build. All zero is no event.
struct RewardEventRow {
  uint64_t words[4]{};  ///< the Nullable's bytes, copied whole; all zero is no event
};

/// The reward screen. `screen` below is the RewardManager. A mod hooks these to act around the screen's own work.
struct RewardManager {
  /// StartResults(screen): starts the results, running their first step. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_StartResults, void(void*)> StartResults{};
  /// BuildRows(screen): builds the rows and their totals. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_BuildRows, void(void*)> BuildRows{};
  /// SetTotals(screen, money, experience): sets the totals, and the shown total counts to them. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_SetTotals, void(void*, int32_t, int32_t)> SetTotals{};
  /// EventRow(screen): the seasonal event's row and the score to send, or all zero with no event. Unresolved: all zero.
  static constexpr pm::api::Method<pm::map::RewardManager_EventRow, RewardEventRow(void*)> EventRow{};
  /// Skip(screen): the skip button. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_Skip, void(void*)> Skip{};
  /// Next(screen): the next button. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_Next, void(void*)> Next{};
};

/// The reward screen's results. `machine` below is the state machine's own data.
struct RewardResults {
  /// MoveNext(machine): runs the results on from where they last waited. A mod hooks it to act around the
  /// screen's own reward work. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_Results_MoveNext, void(void*)> MoveNext{};
};

/// The seasonal event's part of the reward screen's results. `machine` below is the state machine's own data.
struct RewardEventResults {
  /// MoveNext(machine): runs it on from where it last waited, once a server answer has come. A mod hooks
  /// it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::RewardManager_EventRewards_MoveNext, void(void*)> MoveNext{};
};

/// The player's challenges. `controller` below is the ChallengesController.
struct ChallengesController {
  /// ChangeChallengeProgression(controller, type, amount): adds amount to every challenge of that type. A mod
  /// hooks it to keep progress from a player. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ChallengesController_ChangeChallengeProgression,
                                   void(void*, int32_t, int32_t)>
      ChangeChallengeProgression{};
};

/// The settings a level's end leaves for the reward screen. `settings` below is a GameplaySettings.
struct GameplaySettings {
  /// The GameplaySettings the game uses: get(), or null. Unresolved: null.
  static constexpr pm::api::Method<pm::map::GameSettings_gameplay, void*()> get{};
  /// The death status: PlayerDied.get(settings), set(settings, value). Unresolved: reads false, writes nothing.
  static constexpr pm::api::Field<pm::map::GameplaySettings_PlayerDied, bool> PlayerDied{};
};

/// Unity's application.
struct Application {
  /// Quit(exitCode): Quit(0) is Unity's Quit(). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::Application_Quit, void(int32_t)> Quit{};
};

/// @}
}  // namespace phasmo
