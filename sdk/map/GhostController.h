#pragma once

// GhostController: the contract's ghost roll and type-specific timers, one per contract, and the
// Mimic's copying. The type and its real-named members are found by name; the Mimic's fields and
// methods are obfuscated, so they are found by shape and, where shape is not enough, by machine-code
// facts. Notes read from decompiled code are from build 25811160. sdk/systems/ghost-types.md ties
// them together.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

// AttemptGhostTypeStateChange clears the flag through this; of the class's three bools it stores no
// other.
inline constexpr pm::sdk::Fact GhostController_typeChangeDue_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostController.AttemptGhostTypeStateChange", .value = 0}};
// AttemptGhostTypeStateChange compares the identity with the Mimic, the Thaye and the Obambo through
// this, and never reads the behaviour type.
inline constexpr pm::sdk::Fact GhostController_ghostType_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostController.AttemptGhostTypeStateChange"}};
// AttemptGhostTypeStateChange jumps to two bodies of this shape, the Mimic's and the Thaye's; only the
// Mimic's counts the copy in LevelStats.
inline constexpr pm::sdk::Fact GhostController_MimicCopy_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostController.AttemptGhostTypeStateChange"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "LevelStats.get_Instance"}};
// The Mimic's copy calls one method of this shape: the builder of its list.
inline constexpr pm::sdk::Fact GhostController_BuildMimicCopies_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostController.MimicCopy"}};
// The evidence pick and three obfuscated copies of it all call GetEvidenceList; only the pick reads
// LevelValues through its real getter (each copy calls an obfuscated getter of its own).
inline constexpr pm::sdk::Fact GhostController_PickEvidence_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostData.GetEvidenceList"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "LevelValues.get_Instance"}};
// The pick stores the given evidence through this; of the traits' two evidence lists it stores no
// other.
inline constexpr pm::sdk::Fact GhostController_givenEvidence_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostController.PickEvidence"}};

/// The ghost controller (Assembly-CSharp, global namespace), a MonoBehaviour. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController{.key = "GhostController",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "GhostController", .assembly = "Assembly-CSharp", .ns = ""};

/// The current contract's controller: the class's one static field of its own type, set by Awake.
/// Check it with pm::api::alive after a contract. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_instance{
    .key = "GhostController.instance", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "GhostController", .type = "@GhostController"};

/// The host's roll of the ghost: the class's one GhostTraits field. The roll (on the master, from
/// Start) picks the behaviour type by weight, replaces it with a Custom difficulty's type unless that
/// is None, copies it to the identity, then picks the given evidence. CreateGhost passes these traits
/// to GhostAI.Init. The Mimic's copy never changes them. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_traits{.key = "GhostController.traits",
                                                           .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "GhostController",
                                                           .type = "@GhostTraits"};

/// The rolled ghost's identity, a GhostType: the first GhostType of traits, placed in GhostController.
/// The roll copies the behaviour type into it; AttemptGhostTypeStateChange branches on it. CreateGhost
/// passes it to every client inside the traits. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_ghostType{.key = "GhostController.ghostType",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostController",
                                                              .type = "@GhostType",
                                                              .facts = GhostController_ghostType_facts,
                                                              .within = "GhostController.traits"};

/// The rolled ghost's behaviour type, a GhostType: the second GhostType of traits, placed in
/// GhostController, the very struct field GhostInfo.behaviourType is. The roll picks it; the evidence pick
/// (GhostController.PickEvidence) reads it twice: once for GetGhostOfType, then in each removal for the
/// Hantu, Goryo, Obake, Moroi and Deogen, whose removals skip the last evidence. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_behaviourType{
    .key = "GhostController.behaviourType", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController",
    .type = "@GhostType", .within = "GhostController.traits", .sameFieldAs = "GhostInfo.behaviourType"};

/// The evidence the rolled ghost gives in this contract, as the difficulty left it: a
/// List<EvidenceType>, the first of the traits' two evidence lists, placed in GhostController.
/// PickEvidence makes it. CreateGhost passes it to every client inside the traits, and
/// GhostInfo.SyncValues sends it again. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_givenEvidence{
    .key = "GhostController.givenEvidence", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "List<@EvidenceType>",
    .facts = GhostController_givenEvidence_facts, .within = "GhostController.traits"};

/// The evidence pick, on the master, from the type roll after the type is settled: givenEvidence
/// becomes a new empty list, and when LevelValues.difficulty's evidenceGiven is 1 or more, the
/// GetEvidenceList of the behaviour type's GhostData in allGhostsCollection (its three evidence in
/// order, an empty slot included), less 3 minus evidenceGiven of them, each at a random index. For the
/// Hantu, Goryo, Obake, Moroi and Deogen the index never reaches the last one, so their third evidence
/// stays. It reads the evidence as numbers only: a custom evidence in a custom ghost's GhostData is
/// given or withheld as the game's own. A private void(). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_PickEvidence{
    .key = "GhostController.PickEvidence", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "void", .facts = GhostController_PickEvidence_facts};

/// The contract's GhostCollection: the roll's weights, GetFullEvidence and the evidence pick look a
/// type's GhostData up in it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_allGhostsCollection{
    .key = "GhostController.allGhostsCollection", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .name = "allGhostsCollection"};

/// The types the Mimic may copy, a List<GhostType>: null until the first copy without a pending
/// choice, when BuildMimicCopies makes it, and never rebuilt for the contract. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_mimicCopies{.key = "GhostController.mimicCopies",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "GhostController",
                                                                .type = "List<@GhostType>"};

/// The Mimic's next copy, chosen ahead: a Nullable<GhostType>. While it has a value the copy takes
/// it instead of a random pick from mimicCopies, then clears it. Nothing in the game sets it; Playable
/// Ghost does. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_mimicNext{.key = "GhostController.mimicNext",
                                                              .kind = pm::sdk::Kind::Field,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostController",
                                                              .type = "Nullable<@GhostType>"};

/// Set when a type's own timer asks for a type change: Update sets it for the Mimic and the Thaye when
/// their timers run out (while a flag of the SetupPhaseController is set), and for the Obambo before it
/// calls AttemptGhostTypeStateChange, which clears it as it acts. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_typeChangeDue{
    .key = "GhostController.typeChangeDue", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "bool", .facts = GhostController_typeChangeDue_facts};

/// The game's moment to change a ghost's type, called by GhostAI.ChangeState at every state change
/// it makes (ChangeState acts only while a flag of the ghost's PhotonView is set, on the client that
/// runs the AI) and by Update for the Obambo. It returns at once unless
/// typeChangeDue is set or mimicNext has a value; it then clears typeChangeDue and, by the identity,
/// runs the Mimic's copy (MimicCopy), the Thaye's ageing or the Obambo's switch. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_AttemptGhostTypeStateChange{
    .key = "GhostController.AttemptGhostTypeStateChange", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .name = "AttemptGhostTypeStateChange"};

/// The Mimic's copy, which AttemptGhostTypeStateChange jumps to when the identity is the Mimic. It
/// takes mimicNext when it has a value (and clears it), otherwise a random type from mimicCopies
/// (building it first when null): a plain random element of the whole list, with nothing skipped and
/// nothing re-rolled, so it can be the type being copied now. It writes it to the behaviour type of
/// the ghost's GhostInfo, on this client only. It then counts the copy in LevelStats and starts the
/// Thaye's or the Obambo's state when it copied one of them. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_MimicCopy{.key = "GhostController.MimicCopy",
                                                              .kind = pm::sdk::Kind::Method,
                                                              .confidence = pm::sdk::Confidence::Decompiled,
                                                              .owner = "GhostController",
                                                              .type = "void", .facts = GhostController_MimicCopy_facts};

/// Builds mimicCopies: a new list of every GhostType number below the count of the enum's names but
/// None, skipping, for a ghost whose traits flag says so, the types a dictionary of the controller
/// marks as not allowed (Awake fills it with the Banshee and the Dayan). The Mimic itself is in the
/// list. A custom type is never in it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_BuildMimicCopies{
    .key = "GhostController.BuildMimicCopies", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .type = "void",
    .facts = GhostController_BuildMimicCopies_facts};

/// Sets the controller up as the contract loads; among its stores the Thaye's age (step 0, hunt
/// threshold 75), copied whole. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_Awake{.key = "GhostController.Awake", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "GhostController", .name = "Awake"};

inline constexpr const char* GalluState_values[] = {"Normal", "Enraged", "Exhausted"};
inline constexpr const char* DayanState_values[] = {"Normal", "Empowered", "Weakened"};
inline constexpr const char* ThayeAge_fieldTypes[] = {"int", "int", "float", "float", "float"};

// Found by its values.
/// The Gallu's rage, an enum nested in GhostController (name obfuscated, value names real): Normal,
/// Enraged, Exhausted. Decompiled.
inline constexpr pm::sdk::EntryDesc GalluState{.key = "GalluState", .confidence = pm::sdk::Confidence::Decompiled,
                                               .owner = "GhostController",
                                               .values = GalluState_values};

// Found by its values.
/// The Dayan's strength, an enum nested in GhostController (name obfuscated, value names real): Normal,
/// Empowered, Weakened. Decompiled.
inline constexpr pm::sdk::EntryDesc DayanState{.key = "DayanState", .confidence = pm::sdk::Confidence::Decompiled,
                                               .owner = "GhostController",
                                               .values = DayanState_values};

/// The Thaye's age, a struct nested in GhostController: two ints (its age step and its hunt threshold)
/// and three floats, exactly. Decompiled.
inline constexpr pm::sdk::EntryDesc ThayeAge{.key = "ThayeAge", .confidence = pm::sdk::Confidence::Decompiled,
                                             .flags = pm::sdk::kExactFields, .owner = "GhostController",
                                             .fieldTypes = ThayeAge_fieldTypes};

/// The Gallu's rage now: the controller's one GalluState field. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_galluState{
    .key = "GhostController.galluState", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "@GalluState"};

/// The Dayan's strength now: the controller's one DayanState field. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_dayanState{
    .key = "GhostController.dayanState", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "@DayanState"};

/// The Thaye's age: the controller's one ThayeAge field, which ThayeAgeChangeNetworked moves on.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_thaye{.key = "GhostController.thaye", .kind = pm::sdk::Kind::Field,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "GhostController",
                                                          .type = "@ThayeAge"};

// Of the struct's two ints, GhostAI's event pick reads the step only (it lets the Thaye's events grow
// with its age) and the idle roll the threshold only (its Thaye hunt check). Awake copies the whole
// struct with one 16-byte store, so it tells them apart by no constant.
inline constexpr pm::sdk::Fact GhostController_thayeStep_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostAI.EventPick"}};
inline constexpr pm::sdk::Fact GhostController_thayeThreshold_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostIdleState.Execute"}};

/// The Thaye's age step, 0 to 10 (ThayeAgeChangeNetworked adds 1, or 4 at once): an int of thaye.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_thayeStep{
    .key = "GhostController.thayeStep", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "int", .facts = GhostController_thayeStep_facts,
    .within = "GhostController.thaye"};

/// The Thaye's hunt line, a sanity, from 75 down 6 per step: an int of thaye. GhostIdleState.Execute's
/// hunt branch hunts for the Thaye only while its insanity is above 100 less this, so at a sanity below
/// it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_thayeThreshold{
    .key = "GhostController.thayeThreshold", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .type = "int",
    .facts = GhostController_thayeThreshold_facts, .within = "GhostController.thaye"};

inline constexpr const char* GhostController_ChangeFavouriteRoomTo_params[] = {"LevelRoom"};

/// Moves the ghost's favourite room to the room given: it finds the room's index among the level's
/// rooms, stores it, counts the move in LevelStats, sends it to every client by RPC and respawns the
/// ghost orb. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_ChangeFavouriteRoomTo{
    .key = "GhostController.ChangeFavouriteRoomTo", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .name = "ChangeFavouriteRoomTo",
    .params = GhostController_ChangeFavouriteRoomTo_params};

/// The level's LevelController: the class's one field of that type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_levelController{
    .key = "GhostController.levelController", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .name = "levelController"};

inline constexpr const char* GhostController_PickType_params[] = {"Dictionary<@GhostType, int>"};
// Five static methods share its shape; the spawn calls this one.
inline constexpr pm::sdk::Fact GhostController_PickType_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostController.Spawn"}};

/// The weighted roll of the ghost's type, from a Dictionary<GhostType, int> of weights, with a fallback to
/// every key when no weight is positive. Static. The spawn's roll calls it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_PickType{
    .key = "GhostController.PickType", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .flags = pm::sdk::kStatic, .owner = "GhostController", .type = "@GhostType",
    .params = GhostController_PickType_params, .facts = GhostController_PickType_facts};

inline constexpr pm::sdk::Fact GhostController_SpawnGate_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostController.Start"}};

/// The class's private Start. Inferred: Unity's message name.
inline constexpr pm::sdk::EntryDesc GhostController_Start{.key = "GhostController.Start", .kind = pm::sdk::Kind::Method,
                                                          .owner = "GhostController", .name = "Start"};

/// What runs the spawn once every player has loaded: GhostController.Start and the room callbacks call it,
/// and it ends in a jump to GhostController.Spawn. Found for the fact that names the spawn. A private void().
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_SpawnGate{
    .key = "GhostController.SpawnGate", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "void", .facts = GhostController_SpawnGate_facts};

// The roll's look-alikes call the same methods, but only the real roll is entered from the gate.
inline constexpr pm::sdk::Fact GhostController_Spawn_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostController.PickEvidence"},
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostController.SpawnGate"}};

/// The host's ghost setup, on the master, after every player has loaded: it rolls the ghost's type
/// (GhostController.PickType) and evidence (GhostController.PickEvidence), then its favourite room among
/// LevelController.rooms (into GhostController.favouriteRoomIndex) and its model, and sends the result by the
/// GhostController.CreateGhost RPC to every client, this one at once. A private void(). Tracked by the host
/// (pm::api::inside). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_Spawn{
    .key = "GhostController.Spawn", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .type = "void", .facts = GhostController_Spawn_facts};

/// Puts the ghost in its favourite room: the truck's first use calls it, so an unplayed ghost stays at the
/// level's origin until then. GhostController.favouriteRoomIndex is found through it. Inferred: from its
/// name, and its callers in decompiled code.
inline constexpr pm::sdk::EntryDesc GhostController_WarpGhostToFavouriteRoom{
    .key = "GhostController.WarpGhostToFavouriteRoom", .kind = pm::sdk::Kind::Method,
    .owner = "GhostController", .name = "WarpGhostToFavouriteRoom"};

// The warp reads the room's index through this to look the room up; of the traits' ints it reads no other.
// (ChangeFavouriteRoomTo stores it too, but through a register it later reuses, so that store proves nothing.)
inline constexpr pm::sdk::Fact GhostController_favouriteRoomIndex_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostController.WarpGhostToFavouriteRoom"}};

/// The favourite room's index among LevelController.rooms: an int of traits. GhostController.Spawn rolls it
/// before the CreateGhost RPC and reads it nowhere after; ChangeFavouriteRoomTo stores each new one.
/// GhostController.CreateGhost passes it inside the traits to GhostAI.Init. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_favouriteRoomIndex{
    .key = "GhostController.favouriteRoomIndex", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostController", .type = "int",
    .facts = GhostController_favouriteRoomIndex_facts, .within = "GhostController.traits"};

inline constexpr const char* GhostController_CreateGhost_params[] = {"bool", "int", "PhotonMessageInfo"};

/// The ghost's creation, a Photon RPC every client runs from GhostController.Spawn's AllBuffered call, the
/// master at once, inside the spawn (PhotonNetwork's RPC runs the local call before it returns). It checks the
/// sender, then once per contract sets up the ghost's model and runs GhostAI.Init with a copy of the traits
/// as they are then. A private void(bool, int, PhotonMessageInfo). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostController_CreateGhost{
    .key = "GhostController.CreateGhost", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostController", .name = "CreateGhost", .params = GhostController_CreateGhost_params};

}  // namespace pm::map

/// @file
/// @brief The game API for GhostController (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, false, null) without touching the
/// game. The traits field has no wrapper: it is a struct holding managed references.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// A Nullable<GhostType> as the runtime lays it out: the flag, then the value.
struct NullableGhostType {
  bool hasValue;  ///< whether it holds a type
  int32_t value;  ///< the GhostType number, when hasValue is set
};

/// The contract's ghost roll and the Mimic's copying. `controller` below is the GhostController.
struct GhostController {
  /// The GhostController type: klass(), is(obj), type_object(), make() and make_array(n).
  /// Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostController> type{};
  /// The current contract's controller: instance.get(), or null. A destroyed one remains after a
  /// contract, so check it with pm::api::alive. Reading it may run the class's static constructor, so
  /// read it while the game plays, never at startup. Unresolved: null.
  static constexpr pm::api::StaticField<pm::map::GhostController_instance, void*> instance{};
  /// The rolled ghost's identity, a GhostType number: ghostType.get(controller). Unresolved: reads 0
  /// (Spirit, so check use::resolved()), writes nothing.
  static constexpr pm::api::Field<pm::map::GhostController_ghostType, int32_t> ghostType{};
  /// The rolled ghost's behaviour type, a GhostType number: behaviourType.get(controller),
  /// behaviourType.set(controller, n). Unresolved: reads 0 (Spirit, so check use::resolved()), writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::GhostController_behaviourType, int32_t> behaviourType{};
  /// The given evidence, a List<EvidenceType>, on the master: givenEvidence.get(controller). Read it as
  /// pm::api::Array<int32_t>{pm::api::List{list}.items()}, indexes below the List's size() only.
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostController_givenEvidence, void*> givenEvidence{};
  /// The evidence pick: PickEvidence(controller). A mod hooks it rather than calls it. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::GhostController_PickEvidence, void(void*)> PickEvidence{};
  /// The contract's GhostCollection, a managed reference: allGhostsCollection.get(controller).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostController_allGhostsCollection, void*> allGhostsCollection{};
  /// The List<GhostType> the Mimic copies from, or null before its first copy: mimicCopies.get(
  /// controller). Read it as pm::api::Array<int32_t>{pm::api::List{list}.items()}, indexes below the
  /// List's size() only. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostController_mimicCopies, void*> mimicCopies{};
  /// The Mimic's next copy, chosen ahead: mimicNext.get(controller), mimicNext.set(controller, v).
  /// Unresolved: reads no value, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostController_mimicNext, NullableGhostType> mimicNext{};
  /// Whether a type's timer asks for a change: typeChangeDue.get(controller). Unresolved: false.
  static constexpr pm::api::Field<pm::map::GhostController_typeChangeDue, bool> typeChangeDue{};
  /// The Gallu's rage, a GalluState number: galluState.get(controller). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostController_galluState, int32_t> galluState{};
  /// The Dayan's strength, a DayanState number: dayanState.get(controller). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostController_dayanState, int32_t> dayanState{};
  /// The Thaye's age step: thayeStep.get(controller). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostController_thayeStep, int32_t> thayeStep{};
  /// The Thaye's hunt line, a sanity it hunts below: thayeThreshold.get(controller). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostController_thayeThreshold, int32_t> thayeThreshold{};
  /// The game's moment to change the ghost's type: AttemptGhostTypeStateChange(controller). A mod
  /// usually hooks it rather than calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_AttemptGhostTypeStateChange, void(void*)>
      AttemptGhostTypeStateChange{};
  /// The Mimic's copy: MimicCopy(controller). Hook it to see each copy; calling it copies at once.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_MimicCopy, void(void*)> MimicCopy{};
  /// Builds mimicCopies: BuildMimicCopies(controller). Hook it to add types after the game's own.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_BuildMimicCopies, void(void*)> BuildMimicCopies{};
  /// Moves the ghost's favourite room: ChangeFavouriteRoomTo(controller, room), on the master. A mod
  /// usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_ChangeFavouriteRoomTo, void(void*, void*)>
      ChangeFavouriteRoomTo{};
  /// The level's LevelController: levelController.get(controller). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostController_levelController, void*> levelController{};
  /// PickType(weights): the weighted roll of the ghost's type, on a Dictionary<GhostType, int>. A mod hooks
  /// it. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::GhostController_PickType, int32_t(void*)> PickType{};
  /// Spawn(controller): the ghost's setup on the master. A mod hooks it, and pm::api::inside says whether it
  /// is running. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_Spawn, void(void*)> Spawn{};
  /// WarpGhostToFavouriteRoom(controller). Optional. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_WarpGhostToFavouriteRoom, void(void*)>
      WarpGhostToFavouriteRoom{};
  /// The favourite room's index among LevelController.rooms: favouriteRoomIndex.get(controller),
  /// favouriteRoomIndex.set(controller, i). Unresolved: reads 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostController_favouriteRoomIndex, int32_t> favouriteRoomIndex{};
  /// CreateGhost(controller, flag, model, info): the ghost's creation RPC, info a PhotonMessageInfo passed by
  /// address. A mod hooks it rather than calls it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostController_CreateGhost, void(void*, bool, int32_t, void*)>
      CreateGhost{};
  /// The effect that unwinds the running GhostController.Spawn: abandon_spawn().
  static constexpr pm::api::GhostSpawnAbandon abandon_spawn{};
};

/// @}
}  // namespace phasmo
