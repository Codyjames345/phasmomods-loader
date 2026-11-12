#pragma once

// The custom difficulty: Difficulty (one ScriptableObject per preset, and the lobby's own), the lobby's
// difficulty menu (DifficultyUIManager) with its Ghost Type option (a DifficultyOption), the contract's
// difficulty (LevelValues.difficulty), the EasySave3 type that writes a Difficulty to a save or a
// lobby sync, and System.Enum's ToString and GetHashCode, which the menu's summary uses to name a ghost
// type. Types and most members keep their real names; the obfuscated ones are found by shape and
// machine-code facts. Notes read from decompiled code are from build 25811160.
// sdk/systems/difficulty.md ties them together.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* DifficultyParams_difficulty[] = {"@Difficulty"};

// ApplyDifficulty writes the lobby's difficulty (the amateur preset when the player's level is too
// low) and reads the presets and the menu's other difficulties; it writes no other Difficulty field.
inline constexpr pm::sdk::Fact DifficultyUIManager_current_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyUIManager.ApplyDifficulty"}};
// SyncDifficultyValues writes the lobby's difficulty and the copy it settles; ApplyDifficulty writes
// only the first.
inline constexpr pm::sdk::Fact DifficultyUIManager_applied_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyUIManager.SyncDifficultyValues"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyUIManager.ApplyDifficulty", .negate = true}};
// The option's change handler names the chosen ghost through its GhostData (GetGhostData) and the
// localised "Random" through GetLocalisedValue; its look-alikes call obfuscated localisation methods.
inline constexpr pm::sdk::Fact DifficultyUIManager_GhostTypeChosen_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostData.GetGhostData"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "LocalisationSystem.GetLocalisedValue"}};
// Of the void() methods SyncDifficultyValues calls, one asks whether this client is the master: the
// buttons' refresh.
inline constexpr pm::sdk::Fact DifficultyUIManager_RefreshButtons_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "DifficultyUIManager.SyncDifficultyValues"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "PhotonNetwork.get_IsMasterClient"}};
// Of the void() methods SyncDifficultyValues calls, one calls the buttons' refresh: the summary
// panel's toggle.
inline constexpr pm::sdk::Fact DifficultyUIManager_ToggleSummary_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "DifficultyUIManager.SyncDifficultyValues"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "DifficultyUIManager.RefreshButtons"}};
// The toggle calls three string(Difficulty) builders; one names the ghost with Enum.ToString. Its
// look-alike (which calls Enum.ToString too) is called only from methods nothing calls.
inline constexpr pm::sdk::Fact DifficultyUIManager_SummaryText_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "DifficultyUIManager.ToggleSummary"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "Enum.ToString"}};
// Next writes the index through this (it reads the count, never writes it); the arrows read both.
inline constexpr pm::sdk::Fact DifficultyOption_index_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyOption.Next"}};
inline constexpr pm::sdk::Fact DifficultyOption_count_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "DifficultyOption.Next"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyOption.Next", .negate = true}};
// LevelValues.Start loads the last difficulty into this field when no menu is running, and the lobby
// sync writes it; the class's other Difficulty field is written by the menu's Awake only.
inline constexpr pm::sdk::Fact LevelValues_difficulty_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "LevelValues.Start"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "DifficultyUIManager.SyncDifficultyValues"}};

/// A difficulty (Assembly-CSharp, global namespace), a ScriptableObject: a preset, the lobby's own, or
/// a copy. ScriptableObject.CreateInstance makes them, never a scene, so one outlives the menu that
/// made it. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty{.key = "Difficulty", .confidence = pm::sdk::Confidence::Decompiled,
                                               .name = "Difficulty", .assembly = "Assembly-CSharp", .ns = ""};

/// Which preset it is, an enum read as an int: the menu's Awake numbers its custom presets from its
/// presets array (6, 7 and 8 for Custom 1 to 3). The preset choice makes the lobby's difficulty a
/// copy of the preset (Clone(false)); the custom preset's save button saves the lobby's difficulty
/// and puts that very object into the presets array. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_difficulty{.key = "Difficulty.difficulty", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "Difficulty", .name = "difficulty"};

/// The ghost type the contract is forced to, a GhostType: None (the option's Random) lets the roll
/// decide. Only the constructor and the option's change handler write None to it; the contract's spawn
/// (GhostController's type roll) replaces its rolled type with LevelValues.difficulty's when it is not
/// None, and GetRewardMultiplier gives 0 for any other. EasySave3 writes and reads it with the rest of
/// the difficulty. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_ghostTypeOverride{
    .key = "Difficulty.ghostTypeOverride", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "ghostTypeOverride", .type = "@GhostType"};

/// How many of its evidence the contract's ghost gives, an int. The evidence pick
/// (GhostController.PickEvidence) reads it from LevelValues.difficulty: below 1 the ghost gives none,
/// otherwise the pick removes 3 less this many from the ghost's three. GhostInfo.SyncValues sends this
/// many of what the pick left to every client. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_evidenceGiven{.key = "Difficulty.evidenceGiven",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Difficulty", .name = "evidenceGiven",
                                                             .type = "int"};

/// One option of the difficulty menu (Assembly-CSharp, global namespace), a MonoBehaviour with arrows.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyOption{.key = "DifficultyOption",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .name = "DifficultyOption", .assembly = "Assembly-CSharp", .ns = ""};

/// The option's right arrow: the index one up, clamped to the count less one (wrapping past a duplicate
/// when the option allows none), then its value-changed actions run. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyOption_Next{.key = "DifficultyOption.Next", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "DifficultyOption", .name = "Next"};

/// Sets the arrows for the index: left while it is not 0, right while it is not the count less one.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyOption_ButtonCheck{.key = "DifficultyOption.ButtonCheck",
                                                                 .kind = pm::sdk::Kind::Method,
                                                                 .confidence = pm::sdk::Confidence::Decompiled,
                                                                 .owner = "DifficultyOption", .name = "ButtonCheck"};

/// The choice the option shows, an int from 0. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyOption_index{
    .key = "DifficultyOption.index", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DifficultyOption", .type = "int", .facts = DifficultyOption_index_facts};

/// How many choices the option has, an int the scene sets and no code writes. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyOption_count{
    .key = "DifficultyOption.count", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DifficultyOption", .type = "int", .facts = DifficultyOption_count_facts};

/// The lobby's difficulty menu (Assembly-CSharp, global namespace), on the "Difficulty Selection"
/// board of the menu scene, which the game hides (the object inactive) unless the player is on it.
/// One per menu scene, made again with every load of it. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager{.key = "DifficultyUIManager",
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .name = "DifficultyUIManager", .assembly = "Assembly-CSharp",
                                                        .ns = ""};

/// The Ghost Type option. Its index 0 is Random (GhostType.None) and its index i the ghost i - 1 of
/// allGhostsData (the change handler); its count is set in the scene, never by code. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_ghostType{.key = "DifficultyUIManager.ghostType",
                                                                  .kind = pm::sdk::Kind::Field,
                                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                                  .owner = "DifficultyUIManager", .name = "ghostType"};

/// The GhostCollection whose ghosts the Ghost Type option offers, in its order. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_allGhostsData{.key = "DifficultyUIManager.allGhostsData",
                                                                      .kind = pm::sdk::Kind::Field,
                                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                                      .owner = "DifficultyUIManager",
                                                                      .name = "allGhostsData"};

/// The custom presets, a Difficulty[]: Awake makes it and loads each from the save (ES3.Load with an
/// amateur copy for a preset never saved), numbering them 6 up. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_presets{
    .key = "DifficultyUIManager.presets", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DifficultyUIManager", .type = "Difficulty[]"};

/// The lobby's difficulty: a copy of the preset the player chose, the very preset object once the
/// custom preset's save button put it into the presets array, the save's last difficulty once the
/// board is first shown (master, in a room; read into the live object its saved reference names), or
/// what a sync made. Null in a menu whose board was never shown and that nothing synced. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_current{
    .key = "DifficultyUIManager.current", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DifficultyUIManager", .type = "@Difficulty", .facts = DifficultyUIManager_current_facts};

/// The copy of the lobby's difficulty that SyncDifficultyValues settles (Clone(true)) and gives to
/// LevelValues.difficulty. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_applied{
    .key = "DifficultyUIManager.applied", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "DifficultyUIManager", .type = "@Difficulty", .facts = DifficultyUIManager_applied_facts};

/// Applies the lobby's difficulty: saves it under "LastDifficulty" (ES3.Save), then sends it to every
/// player, this one included, with the SyncDifficultyValues RPC. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_ApplyDifficulty{.key = "DifficultyUIManager.ApplyDifficulty",
                                                                        .kind = pm::sdk::Kind::Method,
                                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                                        .owner = "DifficultyUIManager",
                                                                        .name = "ApplyDifficulty"};

/// The lobby sync's receiver, an RPC: the bytes made the lobby's difficulty (ES3.Deserialize, which
/// reads into the live object its saved reference names), its settled copy (Clone(true)) the applied
/// one and LevelValues.difficulty, then the display refreshed. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_SyncDifficultyValues{
    .key = "DifficultyUIManager.SyncDifficultyValues", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager", .name = "SyncDifficultyValues"};

/// Sends a difficulty to every player, this one included, with the SyncDifficultyValues RPC
/// (ES3.Serialize). Nothing in the game calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_ForceSyncDifficulty{
    .key = "DifficultyUIManager.ForceSyncDifficulty", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager", .name = "ForceSyncDifficulty",
    .params = DifficultyParams_difficulty};

// Two never-called look-alikes write other constants.
/// The Ghost Type option's change handler, a private void(): index 0 writes None to the lobby
/// difficulty's ghostTypeOverride, index i the type of allGhostsData's ghost i - 1; then it names the
/// choice on the option. The option's arrows and dice run it; a loaded or synced difficulty is shown by
/// writing the option's index alone. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_GhostTypeChosen{
    .key = "DifficultyUIManager.GhostTypeChosen", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager",
    .type = "void", .facts = DifficultyUIManager_GhostTypeChosen_facts};

/// Refreshes the menu's buttons, a private void(): the edit and randomise buttons and the player and
/// ghost toggles show as the lobby's difficulty and being the master allow. Only a fact for
/// ToggleSummary. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_RefreshButtons{
    .key = "DifficultyUIManager.RefreshButtons", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager",
    .type = "void", .facts = DifficultyUIManager_RefreshButtons_facts};

/// The summary panel's toggle, a private void(): it flips the panel's flag, shows or hides the panel,
/// refreshes the buttons, and when the panel is now shown writes into it the custom difficulty's
/// summary (SummaryText) or another builder's text. ApplyDifficulty, SyncDifficultyValues and the
/// preset choice run it while the panel is shown, which closes it. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_ToggleSummary{
    .key = "DifficultyUIManager.ToggleSummary", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager",
    .type = "void", .facts = DifficultyUIManager_ToggleSummary_facts};

/// The custom difficulty's summary, a private string(Difficulty): its Ghost line is "Random" for None,
/// else the difficulty's ghostTypeOverride by Enum.ToString on a GhostType boxed on the stack, which
/// prints a type with no enum name as its number. The toggle (shown panel) and the display's refresh
/// (through a helper, into the same panel while it may be hidden) call it; an obfuscated copy is
/// called only from methods nothing calls. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultyUIManager_SummaryText{
    .key = "DifficultyUIManager.SummaryText", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultyUIManager",
    .type = "string", .params = DifficultyParams_difficulty, .facts = DifficultyUIManager_SummaryText_facts};

/// EasySave3's type for Difficulty (Assembly-CSharp, namespace ES3Types). Every save (ES3.Save, which
/// the cache location serialises at once, ES3File.Save) and every lobby sync (ES3.Serialize) of a
/// Difficulty writes it through this type. Decompiled.
inline constexpr pm::sdk::EntryDesc ES3UserType_Difficulty{.key = "ES3UserType_Difficulty",
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .name = "ES3UserType_Difficulty",
                                                           .assembly = "Assembly-CSharp", .ns = "ES3Types"};

/// Writes a Difficulty's fields, ghostTypeOverride among them (boxed as a GhostType), to an EasySave3
/// writer: a save's or a sync's. Decompiled.
inline constexpr pm::sdk::EntryDesc ES3UserType_Difficulty_WriteScriptableObject{
    .key = "ES3UserType_Difficulty.WriteScriptableObject", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "ES3UserType_Difficulty", .name = "WriteScriptableObject"};

/// The values of the running contract (Assembly-CSharp, global namespace), a MonoBehaviour kept across
/// scenes (DontDestroyOnLoad; a later copy destroys itself). Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues{.key = "LevelValues", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "LevelValues", .assembly = "Assembly-CSharp", .ns = ""};

/// The one LevelValues, made on a new GameObject when there is none. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_get_Instance{.key = "LevelValues.get_Instance",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .flags = pm::sdk::kStatic, .owner = "LevelValues",
                                                             .name = "get_Instance"};

/// The one LevelValues: the class's one static field of its own type, which get_Instance returns
/// (making one first when it is null or destroyed). Reading it makes nothing. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_instance{
    .key = "LevelValues.instance", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "LevelValues", .type = "@LevelValues"};

/// The money the contract's objectives pay: the main objective's ObjectiveValues.moneyReward, plus the
/// moneyReward of every side objective whose own bool flag reads true (the objective's flags are
/// obfuscated, so the map knows the class by its fields, not by them). Decompiled. It throws a
/// NullReferenceException when LevelValues.mainObjective is null,
/// when the main objective has no values, or when the side objectives' list (a field of LevelValues not
/// mapped yet) is null or holds a null entry. At the main menu it throws: read LevelValues.mainObjective
/// first, and ask only while a contract runs (the dev menu's live test, 2026-11-11).
inline constexpr pm::sdk::EntryDesc LevelValues_GetInvestigationBonusReward{
    .key = "LevelValues.GetInvestigationBonusReward", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "LevelValues", .name = "GetInvestigationBonusReward"};

// GetInvestigationBonusReward reads the main objective and the side objectives' list; the class's other
// Objective field, which IsPerfectGame also reads, it never does.
inline constexpr pm::sdk::Fact LevelValues_mainObjective_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "LevelValues.GetInvestigationBonusReward"}};

/// The contract's main objective, an Objective: the one whose money GetInvestigationBonusReward counts
/// before the side objectives'. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_mainObjective{
    .key = "LevelValues.mainObjective", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelValues", .type = "@Objective", .facts = LevelValues_mainObjective_facts};

/// What the difficulty multiplies a contract's reward by. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_GetRewardMultiplier{
    .key = "Difficulty.GetRewardMultiplier", .kind = pm::sdk::Kind::Method, .owner = "Difficulty",
    .name = "GetRewardMultiplier"};

/// LevelValues.Start: with no menu running it loads the last difficulty. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_Start{.key = "LevelValues.Start", .kind = pm::sdk::Kind::Method,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "LevelValues", .name = "Start"};

/// The difficulty the next or running contract plays: the lobby sync's settled copy. It outlives the
/// menu, so after a contract the lobby keeps it until a sync replaces it. The spawn reads its
/// ghostTypeOverride. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_difficulty{
    .key = "LevelValues.difficulty", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelValues", .type = "@Difficulty", .facts = LevelValues_difficulty_facts};

// GhostActivity.GhostAbility reads one bool of LevelValues, through get_Instance: another type's
// method, so the fact matches that offset through any register.
inline constexpr pm::sdk::Fact LevelValues_eventsBlocked_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostActivity.GhostAbility"}};

/// While set, GhostActivity.GhostAbility does not change the ghost to
/// randomEvent on its last path, where it would otherwise, and a ghost's push of a door (Door.GhostForceDoor)
/// plays no rattle or door sound and leaves no EMF reading, though it still counts in LevelStats.doorsMoved.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_eventsBlocked{
    .key = "LevelValues.eventsBlocked", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelValues", .type = "bool", .facts = LevelValues_eventsBlocked_facts};

/// The contract's weather, a WeatherProfile (sdk/map/Weather.h): LevelValues' one field of that type.
/// RandomWeather's Start sets it from the difficulty's weather, and a Monkey Paw weather wish sets it again
/// mid-contract (RandomWeather's TransitionWeatherProfile, which writes it before its dead-player check), so
/// read it each time rather than once. LightningController's update reads its weather type at each of the
/// storm's strikes. The dead players' profile never goes here. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_weather{.key = "LevelValues.weather", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "LevelValues",
                                                        .type = "WeatherProfile"};

// ExitLevel.Exit sets one bool of LevelValues to true, through get_Instance: another type's method, so
// the fact matches that offset through any register.
inline constexpr pm::sdk::Fact LevelValues_completed_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "ExitLevel.Exit", .value = 1}};

/// Whether the contract ended the game's own way: ExitLevel.Exit sets it on its usual path (and clears
/// it on its other), and PauseMenuController.Leave and PauseMenuController.Quit clear it. Decompiled.
inline constexpr pm::sdk::EntryDesc LevelValues_completed{
    .key = "LevelValues.completed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "LevelValues", .type = "bool", .facts = LevelValues_completed_facts};

/// Whether the difficulty's ghost is friendly: Door.HuntingCloseDoor shuts and locks the door only while it is
/// clear, and Player.CompleteDeathSequenceNetworked tests it. Decompiled. GhostAI.ChangeState turns a hunt into idle
/// while it is set, before anything is sent. GhostAI.Hunting, Door's hunt close and every kill refuse, with a report
/// on the sender (sdk/systems/photon.md), so never go round those guards.
inline constexpr pm::sdk::EntryDesc Difficulty_friendlyGhost{
    .key = "Difficulty.friendlyGhost", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "friendlyGhost"};

inline constexpr const char* CursedItemType_values[] = {"none",       "tarotCards",   "ouijaBoard", "mirror",
                                                        "musicBox",   "summoningCircle", "voodooDoll", "monkeyPaw"};

/// The cursed possessions a contract can carry (Assembly-CSharp, global namespace, name obfuscated): none 0,
/// tarotCards 1, ouijaBoard 2, mirror 3, musicBox 4, summoningCircle 5, voodooDoll 6, monkeyPaw 7. Decompiled.
inline constexpr pm::sdk::EntryDesc CursedItemType{.key = "CursedItemType", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .assembly = "Assembly-CSharp", .ns = "",
                                                   .values = CursedItemType_values};

/// The tarot cards. One of the two possessions the Revive RPC accepts (Difficulty.actualCursedItems). Decompiled.
inline constexpr pm::sdk::EntryDesc CursedItemType_tarotCards{
    .key = "CursedItemType.tarotCards", .kind = pm::sdk::Kind::EnumValue, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "CursedItemType", .name = "tarotCards"};

/// The Monkey Paw. One of the two possessions the Revive RPC accepts (Difficulty.actualCursedItems). Decompiled.
inline constexpr pm::sdk::EntryDesc CursedItemType_monkeyPaw{
    .key = "CursedItemType.monkeyPaw", .kind = pm::sdk::Kind::EnumValue, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "CursedItemType", .name = "monkeyPaw"};

/// The cursed possessions the contract carries, a CursedItemType[]. Decompiled: the Revive RPC's body (Player's Revive
/// coroutine) goes on only when it holds tarotCards or monkeyPaw, and otherwise raises the game's report on the sender
/// and revives nobody, offline too (sdk/systems/players.md).
inline constexpr pm::sdk::EntryDesc Difficulty_actualCursedItems{
    .key = "Difficulty.actualCursedItems", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "actualCursedItems", .type = "@CursedItemType[]"};

/// Whether a death costs the equipment brought: the reward screen, when its copy of the death status is set, calls
/// InventoryManager.ResetAllEquipment while this is set and InventoryManager.ReturnItemstoInventory otherwise, and
/// shows its insurance row only while it is set (sdk/systems/contract.md). Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_loseItemsOnDeath{
    .key = "Difficulty.loseItemsOnDeath", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "loseItemsOnDeath", .type = "bool"};

/// Whether players may sprint: 0 off, 1 on, 2 infinite (its tooltip). With 0, LocalPCPlayer.UpdatePlayerSpeed
/// gives the walk speed in a contract and PCStamina.OutOfBreath does nothing (unless LocalPlayer.isDead is set).
/// In a contract the stamina's update (PlayerStamina's) works only while it is 1, so with 2 a player never tires.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_sprinting{
    .key = "Difficulty.sprinting", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "sprinting", .type = "int"};

/// The contract's player speed factor: LocalPlayer.Start copies it into the rig's LocalPlayer.speedMultiplier
/// in a contract, which multiplies the walk and the sprint speed. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_playerSpeed{
    .key = "Difficulty.playerSpeed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "Difficulty", .name = "playerSpeed", .type = "float"};

/// The custom difficulty's sanity pill restoration amount, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_sanityPillRestorationAmount{
    .key = "Difficulty.sanityPillRestorationAmount", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "sanityPillRestorationAmount", .type = "int"};

/// The custom difficulty's starting sanity, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_startingSanity{
    .key = "Difficulty.startingSanity", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "startingSanity",
    .type = "int"};

/// The custom difficulty's sanity drain, a float. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_sanityDrain{
    .key = "Difficulty.sanityDrain", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "sanityDrain",
    .type = "float"};

/// The custom difficulty's flashlights option, a bool. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_flashlights{
    .key = "Difficulty.flashlights", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "flashlights",
    .type = "bool"};

/// The custom difficulty's changing favourite room option, a ValueType number (None 0, Low 1, Medium 2, High 3,
/// VeryHigh 4). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_changingFavouriteRoom{
    .key = "Difficulty.changingFavouriteRoom", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "changingFavouriteRoom"};

/// The custom difficulty's ghost interaction amount, a ValueType number (None 0, Low 1, Medium 2, High 3,
/// VeryHigh 4). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_interactionAmount{
    .key = "Difficulty.interactionAmount", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "interactionAmount"};

/// The custom difficulty's event frequency, a ValueType number (None 0, Low 1, Medium 2, High 3, VeryHigh 4).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_eventFrequency{
    .key = "Difficulty.eventFrequency", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "eventFrequency"};

/// The custom difficulty's hunt duration, a ValueType number (None 0, Low 1, Medium 2, High 3, VeryHigh 4).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_huntDuration{
    .key = "Difficulty.huntDuration", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "huntDuration"};

/// The custom difficulty's grace period, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_gracePeriod{
    .key = "Difficulty.gracePeriod", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "gracePeriod",
    .type = "int"};

/// The custom difficulty's fingerprint chance, a float. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_fingerprintChance{
    .key = "Difficulty.fingerprintChance", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "fingerprintChance", .type = "float"};

/// The custom difficulty's fingerprint duration, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_fingerprintDuration{
    .key = "Difficulty.fingerprintDuration", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "fingerprintDuration", .type = "int"};

/// The custom difficulty's option for kills extending the hunt duration, a ValueType number (None 0, Low 1,
/// Medium 2, High 3, VeryHigh 4). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_killsExtendHuntDuration{
    .key = "Difficulty.killsExtendHuntDuration", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "killsExtendHuntDuration"};

/// The custom difficulty's roaming frequency, a ValueType number (None 0, Low 1, Medium 2, High 3, VeryHigh 4).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_roamingFrequency{
    .key = "Difficulty.roamingFrequency", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "roamingFrequency"};

/// The custom difficulty's ghost speed, a float. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_ghostSpeed{
    .key = "Difficulty.ghostSpeed", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "ghostSpeed",
    .type = "float"};

/// The custom difficulty's setup time, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_setupTime{
    .key = "Difficulty.setupTime", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "setupTime",
    .type = "int"};

/// The custom difficulty's selected weather, a WeatherValueType (the WeatherValueType note above numbers it).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_selectedWeather{
    .key = "Difficulty.selectedWeather", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "selectedWeather"};

/// The custom difficulty's amount of doors open, a ValueType number (None 0, Low 1, Medium 2, High 3, VeryHigh 4).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_amountOfDoorsOpen{
    .key = "Difficulty.amountOfDoorsOpen", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "amountOfDoorsOpen"};

/// The custom difficulty's amount of hiding places, a ValueType number (None 0, Low 1, Medium 2, High 3,
/// VeryHigh 4). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_amountOfHidingPlaces{
    .key = "Difficulty.amountOfHidingPlaces", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "amountOfHidingPlaces"};

/// The custom difficulty's sanity monitor option, a bool. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_sanityMonitor{
    .key = "Difficulty.sanityMonitor", .kind = pm::sdk::Kind::Field, .owner = "Difficulty", .name = "sanityMonitor",
    .type = "bool"};

/// The custom difficulty's activity monitor option, a bool. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_activityMonitor{
    .key = "Difficulty.activityMonitor", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "activityMonitor", .type = "bool"};

/// The custom difficulty's option for the fuse box showing on the map, a bool. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_fuseBoxVisibleOnMap{
    .key = "Difficulty.fuseBoxVisibleOnMap", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "fuseBoxVisibleOnMap", .type = "bool"};

/// The custom difficulty's fuse box starting power, a FuseBoxValueType (the FuseBoxValueType note below numbers
/// it). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_fuseBoxStartPower{
    .key = "Difficulty.fuseBoxStartPower", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "fuseBoxStartPower"};

/// The custom difficulty's number of cursed possessions, an int. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_cursedPossessionsQuantity{
    .key = "Difficulty.cursedPossessionsQuantity", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "cursedPossessionsQuantity", .type = "int"};

/// The custom difficulty's override multiplier, a float. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Difficulty_overrideMultiplier{
    .key = "Difficulty.overrideMultiplier", .kind = pm::sdk::Kind::Field, .owner = "Difficulty",
    .name = "overrideMultiplier", .type = "float"};

/// System.Enum (mscorlib). Inferred: the .NET class.
inline constexpr pm::sdk::EntryDesc SystemEnum{.key = "Enum", .name = "Enum", .assembly = "mscorlib", .ns = "System"};

// Found by shape, since its overloads share the name.
/// Enum.ToString(), the overload without parameters: the value's name, its number when it has none.
/// The menu's summary calls it on a GhostType boxed on the stack. Decompiled.
inline constexpr pm::sdk::EntryDesc SystemEnum_ToString{.key = "Enum.ToString", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "Enum", .hint = "ToString", .type = "string"};

/// Enum.GetHashCode(): for an enum stored as an int, its value. Decompiled.
inline constexpr pm::sdk::EntryDesc SystemEnum_GetHashCode{.key = "Enum.GetHashCode", .kind = pm::sdk::Kind::Method,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "Enum", .name = "GetHashCode"};

inline constexpr const char* WeatherValueType_values[] = {"Random", "LightRain", "HeavyRain", "Snow", "Wind",
                                                          "Clear",  "Fog",       "Sunrise",   "BloodMoon"};

/// The difficulty's weather numbering, an enum nested in Difficulty (real names): Random 0, LightRain 1, HeavyRain 2,
/// Snow 3, Wind 4, Clear 5, Fog 6, Sunrise 7, BloodMoon 8. It is not LevelValues' WeatherType, which numbers the
/// scene's profiles (clear 0 to bloodMoon 7): a mod pairs the two by name. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType{.key = "WeatherValueType", .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "Difficulty", .values = WeatherValueType_values};

/// Light rain. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_LightRain{.key = "WeatherValueType.LightRain", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "LightRain"};
/// Heavy rain. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_HeavyRain{.key = "WeatherValueType.HeavyRain", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "HeavyRain"};
/// Snow. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_Snow{.key = "WeatherValueType.Snow", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "Snow"};
/// Wind. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_Wind{.key = "WeatherValueType.Wind", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "Wind"};
/// Clear. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_Clear{.key = "WeatherValueType.Clear", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "Clear"};
/// Fog. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_Fog{.key = "WeatherValueType.Fog", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "Fog"};
/// Sunrise. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_Sunrise{.key = "WeatherValueType.Sunrise", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "Sunrise"};
/// The Blood Moon: the hunt's Blood Moon branch reads it here. Decompiled.
inline constexpr pm::sdk::EntryDesc WeatherValueType_BloodMoon{.key = "WeatherValueType.BloodMoon", .kind = pm::sdk::Kind::EnumValue,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "WeatherValueType", .name = "BloodMoon"};

/// The contract's weather as it is played, a WeatherValueType (8 is the blood moon). WintersJestManager.RPC_MakeKrampusStronger
/// and GhostAI.speedFlag's readers compare it. Decompiled.
inline constexpr pm::sdk::EntryDesc Difficulty_actualWeather{.key = "Difficulty.actualWeather", .kind = pm::sdk::Kind::Field,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "Difficulty", .name = "actualWeather"};

inline constexpr const char* DifficultyValueType_values[] = {"None", "Low", "Medium", "High", "VeryHigh"};

/// The numbering of the custom difficulty's level options, an enum nested in Difficulty (real name ValueType):
/// None 0, Low 1, Medium 2, High 3, VeryHigh 4. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType{.key = "DifficultyValueType", .owner = "Difficulty",
                                                        .name = "ValueType", .values = DifficultyValueType_values};

/// None. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType_None{
    .key = "DifficultyValueType.None", .kind = pm::sdk::Kind::EnumValue, .owner = "DifficultyValueType",
    .name = "None"};
/// Low. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType_Low{
    .key = "DifficultyValueType.Low", .kind = pm::sdk::Kind::EnumValue, .owner = "DifficultyValueType", .name = "Low"};
/// Medium. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType_Medium{
    .key = "DifficultyValueType.Medium", .kind = pm::sdk::Kind::EnumValue, .owner = "DifficultyValueType",
    .name = "Medium"};
/// High. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType_High{
    .key = "DifficultyValueType.High", .kind = pm::sdk::Kind::EnumValue, .owner = "DifficultyValueType",
    .name = "High"};
/// VeryHigh. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc DifficultyValueType_VeryHigh{
    .key = "DifficultyValueType.VeryHigh", .kind = pm::sdk::Kind::EnumValue, .owner = "DifficultyValueType",
    .name = "VeryHigh"};

inline constexpr const char* FuseBoxValueType_values[] = {"Broken", "Off", "On"};

/// The custom difficulty's fuse box power numbering, an enum nested in Difficulty: Broken 0, Off 1, On 2.
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc FuseBoxValueType{.key = "FuseBoxValueType", .owner = "Difficulty",
                                                     .values = FuseBoxValueType_values};

/// Broken: the fuse box starts broken. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc FuseBoxValueType_Broken{
    .key = "FuseBoxValueType.Broken", .kind = pm::sdk::Kind::EnumValue, .owner = "FuseBoxValueType", .name = "Broken"};
/// Off: the fuse box starts off. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc FuseBoxValueType_Off{
    .key = "FuseBoxValueType.Off", .kind = pm::sdk::Kind::EnumValue, .owner = "FuseBoxValueType", .name = "Off"};
/// On: the fuse box starts on. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc FuseBoxValueType_On{
    .key = "FuseBoxValueType.On", .kind = pm::sdk::Kind::EnumValue, .owner = "FuseBoxValueType", .name = "On"};

/// The contract's hunt settings (Assembly-CSharp, global namespace), a MonoBehaviourPun with one static instance.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultySettings{.key = "DifficultySettings",
                                                       .confidence = pm::sdk::Confidence::Decompiled,
                                                       .name = "DifficultySettings", .assembly = "Assembly-CSharp", .ns = ""};

/// Adds its int to the hunt's duration, once a contract: unless a flag of the settings is set, it sends an RPC to every
/// client, whose body adds the int to an int field of the settings and sets the flag. The music box's break, the
/// voodoo doll's hunt, the Death card and a monkey paw wish call it with 20 just before their forced hunt. That the field
/// is the hunt's length is its name's word; its unit was not traced. Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultySettings_AddHuntDuration{
    .key = "DifficultySettings.AddHuntDuration", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultySettings", .name = "AddHuntDuration"};

/// Puts the hunt's grace period back: copies a private int field of the settings into the public one beside it. The
/// cursed possessions that force a hunt set that public field to 1 before the change and call this after it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc DifficultySettings_ResetHuntGracePeriod{
    .key = "DifficultySettings.ResetHuntGracePeriod", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "DifficultySettings", .name = "ResetHuntGracePeriod"};

}  // namespace pm::map

/// @file
/// @brief The game API for the custom difficulty (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null, false) without touching the
/// game.
namespace phasmo {
/// @addtogroup lobby
/// @{

/// A difficulty. `d` below is the Difficulty.
struct Difficulty {
  /// The Difficulty type. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::Difficulty> type{};
  /// The contract's cursed possessions, a CursedItemType[]: actualCursedItems.get(d), read as a
  /// pm::api::Array<int32_t>. Unresolved: null.
  static constexpr pm::api::Field<pm::map::Difficulty_actualCursedItems, void*> actualCursedItems{};
  /// The weather as it is played, a WeatherValueType number: actualWeather.get(d). The hunt's Blood Moon branch
  /// reads it each time (GhostAI.speedFlag's note). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_actualWeather, int32_t> actualWeather{};
  /// Which preset it is: difficulty.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_difficulty, int32_t> difficulty{};
  /// The forced ghost type, GhostType.None for Random: ghostTypeOverride.get(d), .set(d, type).
  /// Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_ghostTypeOverride, int32_t> ghostTypeOverride{};
  /// How many evidence the ghost gives: evidenceGiven.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_evidenceGiven, int32_t> evidenceGiven{};
  /// Whether its ghost is friendly (hunts lock no doors): friendlyGhost.get(d), .set(d, v). Unresolved:
  /// false, writes nothing.
  static constexpr pm::api::Field<pm::map::Difficulty_friendlyGhost, bool> friendlyGhost{};
  /// Whether players keep what they hold when they die: loseItemsOnDeath.get(d), .set(d, v). Read at
  /// the reward screen. Unresolved: false, writes nothing.
  static constexpr pm::api::Field<pm::map::Difficulty_loseItemsOnDeath, bool> loseItemsOnDeath{};
  /// Whether players may sprint, 0 off, 1 on, 2 infinite: sprinting.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_sprinting, int32_t> sprinting{};
  /// The players' speed factor: playerSpeed.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_playerSpeed, float> playerSpeed{};
  /// The sanity pill restoration amount: sanityPillRestorationAmount.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_sanityPillRestorationAmount, int32_t>
      sanityPillRestorationAmount{};
  /// The players' starting sanity: startingSanity.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_startingSanity, int32_t> startingSanity{};
  /// The sanity drain: sanityDrain.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_sanityDrain, float> sanityDrain{};
  /// The flashlights option: flashlights.get(d), .set(d, v). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Difficulty_flashlights, bool> flashlights{};
  /// The changing favourite room option, a DifficultyValueType number: changingFavouriteRoom.get(d).
  /// Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_changingFavouriteRoom, int32_t> changingFavouriteRoom{};
  /// The ghost interaction amount, a DifficultyValueType number: interactionAmount.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_interactionAmount, int32_t> interactionAmount{};
  /// The event frequency, a DifficultyValueType number: eventFrequency.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_eventFrequency, int32_t> eventFrequency{};
  /// The hunt duration, a DifficultyValueType number: huntDuration.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_huntDuration, int32_t> huntDuration{};
  /// The grace period: gracePeriod.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_gracePeriod, int32_t> gracePeriod{};
  /// The fingerprint chance: fingerprintChance.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_fingerprintChance, float> fingerprintChance{};
  /// The fingerprint duration: fingerprintDuration.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_fingerprintDuration, int32_t> fingerprintDuration{};
  /// The option for kills extending the hunt duration, a DifficultyValueType number:
  /// killsExtendHuntDuration.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_killsExtendHuntDuration, int32_t> killsExtendHuntDuration{};
  /// The roaming frequency, a DifficultyValueType number: roamingFrequency.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_roamingFrequency, int32_t> roamingFrequency{};
  /// The ghost speed: ghostSpeed.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_ghostSpeed, float> ghostSpeed{};
  /// The setup time: setupTime.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_setupTime, int32_t> setupTime{};
  /// The selected weather, a WeatherValueType number: selectedWeather.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_selectedWeather, int32_t> selectedWeather{};
  /// The amount of doors open, a DifficultyValueType number: amountOfDoorsOpen.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_amountOfDoorsOpen, int32_t> amountOfDoorsOpen{};
  /// The amount of hiding places, a DifficultyValueType number: amountOfHidingPlaces.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_amountOfHidingPlaces, int32_t> amountOfHidingPlaces{};
  /// The sanity monitor option: sanityMonitor.get(d), .set(d, v). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Difficulty_sanityMonitor, bool> sanityMonitor{};
  /// The activity monitor option: activityMonitor.get(d), .set(d, v). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Difficulty_activityMonitor, bool> activityMonitor{};
  /// The option for the fuse box showing on the map: fuseBoxVisibleOnMap.get(d), .set(d, v). Unresolved: false.
  static constexpr pm::api::Field<pm::map::Difficulty_fuseBoxVisibleOnMap, bool> fuseBoxVisibleOnMap{};
  /// The fuse box starting power, a FuseBoxValueType number: fuseBoxStartPower.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_fuseBoxStartPower, int32_t> fuseBoxStartPower{};
  /// The number of cursed possessions: cursedPossessionsQuantity.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_cursedPossessionsQuantity, int32_t>
      cursedPossessionsQuantity{};
  /// The override multiplier: overrideMultiplier.get(d). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::Difficulty_overrideMultiplier, float> overrideMultiplier{};
  /// What it multiplies a contract's reward by: GetRewardMultiplier(d). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::Difficulty_GetRewardMultiplier, float(void*)> GetRewardMultiplier{};
};

/// One option of the difficulty menu. `option` below is the DifficultyOption.
struct DifficultyOption {
  /// The DifficultyOption type. Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::DifficultyOption> type{};
  /// The choice shown: index.get(option), index.set(option, i). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::DifficultyOption_index, int32_t> index{};
  /// How many choices: count.get(option), count.set(option, n). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::DifficultyOption_count, int32_t> count{};
  /// Sets the arrows for the index: ButtonCheck(option). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::DifficultyOption_ButtonCheck, void(void*)> ButtonCheck{};
};

/// The lobby's difficulty menu. `menu` below is the DifficultyUIManager.
struct DifficultyUIManager {
  /// The DifficultyUIManager type: type_object() for Object.FindObjectOfType. Unresolved: null.
  static constexpr pm::api::Type<pm::map::DifficultyUIManager> type{};
  /// The Ghost Type DifficultyOption: ghostType.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DifficultyUIManager_ghostType, void*> ghostType{};
  /// The GhostCollection the option offers: allGhostsData.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DifficultyUIManager_allGhostsData, void*> allGhostsData{};
  /// The custom presets, a Difficulty[]: presets.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DifficultyUIManager_presets, void*> presets{};
  /// The lobby's difficulty, or null: current.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DifficultyUIManager_current, void*> current{};
  /// Its settled copy, or null: applied.get(menu). Unresolved: null.
  static constexpr pm::api::Field<pm::map::DifficultyUIManager_applied, void*> applied{};
  /// Sends a difficulty to every player, this one included: ForceSyncDifficulty(menu, d). Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::DifficultyUIManager_ForceSyncDifficulty, void(void*, void*)>
      ForceSyncDifficulty{};
  /// The Ghost Type option's change handler: GhostTypeChosen(menu). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::DifficultyUIManager_GhostTypeChosen, void(void*)> GhostTypeChosen{};
  /// The custom difficulty's summary text of `d`, a System.String: SummaryText(menu, d). Unresolved:
  /// null.
  static constexpr pm::api::Method<pm::map::DifficultyUIManager_SummaryText, void*(void*, void*)> SummaryText{};
};

/// EasySave3's type for Difficulty. `es3type` below is the type object, `writer` an ES3Writer.
struct ES3UserType_Difficulty {
  /// Writes the difficulty `d`: WriteScriptableObject(es3type, d, writer). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::ES3UserType_Difficulty_WriteScriptableObject, void(void*, void*, void*)>
      WriteScriptableObject{};
};

/// The values of the running contract.
struct LevelValues {
  /// The one LevelValues: get_Instance(). Reading it while no LevelValues exists makes one, so call it
  /// while the game plays. Unresolved: null.
  static constexpr pm::api::Method<pm::map::LevelValues_get_Instance, void*()> get_Instance{};
  /// The contract's difficulty: difficulty.get(values). Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelValues_difficulty, void*> difficulty{};
  /// Whether the ghost's ability may not end in an event now: eventsBlocked.get(values). Unresolved:
  /// false.
  static constexpr pm::api::Field<pm::map::LevelValues_eventsBlocked, bool> eventsBlocked{};
  /// The contract's weather, a WeatherProfile (Weather.h): weather.get(values), or null. Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::LevelValues_weather, void*> weather{};
  /// The one LevelValues: instance.get(), or null (or a destroyed one) when there is none; reading it
  /// makes nothing. Reading it may run the class's static constructor, so read it while the game plays.
  /// Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::LevelValues_instance, void*> instance{};
  /// The contract's main objective, an Objective (Rewards.h): mainObjective.get(values), or null.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::LevelValues_mainObjective, void*> mainObjective{};
  /// The money the contract's objectives pay, the main one's and the side ones':
  /// GetInvestigationBonusReward(values). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::LevelValues_GetInvestigationBonusReward, int32_t(void*)>
      GetInvestigationBonusReward{};
  /// Whether the contract ended the game's own way: completed.get(values); a mod's own end of the
  /// contract writes it as ExitLevel.Exit does. Unresolved: false, writes nothing.
  static constexpr pm::api::Field<pm::map::LevelValues_completed, bool> completed{};
};

/// The contract's hunt settings. `settings` below is the DifficultySettings.
struct DifficultySettings {
  /// The DifficultySettings type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::DifficultySettings> type{};
  /// AddHuntDuration(settings, amount): adds `amount` to the hunt's duration on every client, once a contract; it
  /// sends an RPC (sdk/systems/photon.md). A mod hooks it, never calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::DifficultySettings_AddHuntDuration, void(void*, int32_t)> AddHuntDuration{};
  /// ResetHuntGracePeriod(settings): the hunt's grace period goes back to its own value, on this client. Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::DifficultySettings_ResetHuntGracePeriod, void(void*)> ResetHuntGracePeriod{};
};

/// System.Enum. `boxed` below is a boxed enum value.
struct Enum {
  /// The value's name, its number when it has none: ToString(boxed). Unresolved: null.
  static constexpr pm::api::Method<pm::map::SystemEnum_ToString, void*(void*)> ToString{};
  /// For an enum stored as an int, its value: GetHashCode(boxed). Unresolved: 0.
  static constexpr pm::api::Method<pm::map::SystemEnum_GetHashCode, int32_t(void*)> GetHashCode{};
};

/// The cursed possessions a contract can carry: the numbers Difficulty.actualCursedItems holds.
/// The difficulty's weather numbering (Difficulty.actualWeather). Pair it with WeatherType by name.
namespace WeatherValueType {
/// LightRain(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_LightRain> LightRain{};
/// HeavyRain(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_HeavyRain> HeavyRain{};
/// Snow(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_Snow> Snow{};
/// Wind(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_Wind> Wind{};
/// Clear(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_Clear> Clear{};
/// Fog(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_Fog> Fog{};
/// Sunrise(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_Sunrise> Sunrise{};
/// BloodMoon(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::WeatherValueType_BloodMoon> BloodMoon{};
}  // namespace WeatherValueType

/// The custom difficulty's level options' numbering (None 0, Low 1, Medium 2, High 3, VeryHigh 4): the
/// DifficultyValueType of its changingFavouriteRoom, interactionAmount, eventFrequency, huntDuration,
/// killsExtendHuntDuration, roamingFrequency, amountOfDoorsOpen and amountOfHidingPlaces options.
namespace DifficultyValueType {
/// None(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DifficultyValueType_None> None{};
/// Low(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DifficultyValueType_Low> Low{};
/// Medium(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DifficultyValueType_Medium> Medium{};
/// High(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DifficultyValueType_High> High{};
/// VeryHigh(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::DifficultyValueType_VeryHigh> VeryHigh{};
}  // namespace DifficultyValueType

/// The custom difficulty's fuse box power: the FuseBoxValueType of its fuseBoxStartPower option.
namespace FuseBoxValueType {
/// Broken(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::FuseBoxValueType_Broken> Broken{};
/// Off(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::FuseBoxValueType_Off> Off{};
/// On(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::FuseBoxValueType_On> On{};
}  // namespace FuseBoxValueType

namespace CursedItemType {
/// The CursedItemType enum type: klass(), is(obj), type_object() and make_array(n). Unresolved: null and false.
inline constexpr pm::api::Type<pm::map::CursedItemType> type{};
/// tarotCards(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::CursedItemType_tarotCards> tarotCards{};
/// monkeyPaw(). Unresolved: 0.
inline constexpr pm::api::EnumValue<pm::map::CursedItemType_monkeyPaw> monkeyPaw{};
}  // namespace CursedItemType

/// @}
}  // namespace phasmo
