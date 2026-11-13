#pragma once

// GhostHuntState: the ghost's hunt, the state object GhostAI's state machine runs while the ghost
// hunts (on the master, which runs the AI). Its class and every member are obfuscated, so they are
// found by shape and machine-code facts. Notes read from decompiled code are from build 25811160.
// sdk/systems/ghost-ai.md, "Hunt speed", ties them together.

#include <bit>
#include <cstdint>

#include "entry.h"
#include "game_api.h"

namespace pm::map {

// The hunt state's class holds a GhostAI, its NavMeshAgent, the ClosetZone of its closet search, the
// ghost's type and two lists of players (the seen and the killed); no other class has all of them.
inline constexpr const char* GhostHuntState_fieldTypes[] = {"@GhostAI", "NavMeshAgent", "ClosetZone", "@GhostType",
                                                            "List<Player>", "List<Player>"};
// Two void() methods of the class ask for the players' average insanity (the Moroi's speed): the base
// refresh and an uncalled decoy of it that stores other constants. Only the refresh is called, by the
// hunt's tick and its look-alikes.
inline constexpr pm::sdk::Fact GhostHuntState_RefreshBase_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GameController.GetAveragePlayerInsanity"},
    {.is = pm::sdk::Fact::Is::Called}};
// Four void() methods of the class run the Dayan's checks on the players: the speed update and three
// uncalled decoys of it. Only the update is reached, by a tail jump from the hunt's tick.
inline constexpr pm::sdk::Fact GhostHuntState_UpdateSpeed_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.DayanPlayerChecks"}, {.is = pm::sdk::Fact::Is::Called}};
// The base refresh stores one field of this, the base.
inline constexpr pm::sdk::Fact GhostHuntState_baseSpeed_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.RefreshBase"}};
// The speed update stores one field of this, the line-of-sight speed-up.
inline constexpr pm::sdk::Fact GhostHuntState_sightSpeedUp_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.UpdateSpeed"}};

/// The ghost's hunt state (Assembly-CSharp, global namespace, name obfuscated), one of the state
/// objects that implement the ghost's state interface. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState{.key = "GhostHuntState",
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .assembly = "Assembly-CSharp", .ns = "",
                                                   .fieldTypes = GhostHuntState_fieldTypes};

/// The ghost whose hunt this is: the class's one GhostAI field. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_ghost{.key = "GhostHuntState.ghost", .kind = pm::sdk::Kind::Field,
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .owner = "GhostHuntState",
                                                         .type = "@GhostAI"};

/// The hunt's base speed in metres a second, before the line-of-sight speed-up and the multipliers
/// every type shares. RefreshBase writes it every two seconds; UpdateSpeed reads it every frame.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_baseSpeed{.key = "GhostHuntState.baseSpeed",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "GhostHuntState",
                                                             .type = "float", .facts = GhostHuntState_baseSpeed_facts};

/// The line-of-sight speed-up the base is multiplied by: UpdateSpeed raises it 0.05 a second while the hunt has a
/// target (faster for one type), lowers it only once it has neither a target nor a searchTarget, keeps it between 1
/// and 1.65, and reads it afresh every frame (so a mod's write counts from the next one). A smudged ghost runs at 1
/// whatever it holds. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_sightSpeedUp{
    .key = "GhostHuntState.sightSpeedUp", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "float", .facts = GhostHuntState_sightSpeedUp_facts};

/// Writes the base: for the Moroi from the players' average insanity, for the Deogen from a field of
/// its GhostAI, and for every other type (None, a custom ghost's behaviour type, among them) from the
/// GhostAI's base speed, which GhostAI.Awake sets to 1.7. The hunt runs it every two seconds.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_RefreshBase{
    .key = "GhostHuntState.RefreshBase", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_RefreshBase_facts};

/// The hunt's speed, every frame of the hunt on the master: it moves the line-of-sight speed-up
/// (sightSpeedUp), then sets the NavMeshAgent's speed from the base times that speed-up and the
/// multipliers every type shares (the difficulty's ghost speed, Blood Moon), with branches by the
/// ghost's type. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_UpdateSpeed{
    .key = "GhostHuntState.UpdateSpeed", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_UpdateSpeed_facts};

// Four Player fields: one internal, three private. The speed update reads two of them through this,
// the internal one and one private one; its other reads at those offsets are of the GhostAI and the
// LevelController it reaches through its own fields, types unrelated to the hunt state's.
inline constexpr pm::sdk::Fact GhostHuntState_searchTarget_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.UpdateSpeed"}};

/// The player the hunt follows first, a Player, or null: the class's one internal Player field.
/// GhostHuntState.UpdateSpeed tests it alive first in its branches by the ghost's type, and the
/// hunt's tick clears it with searchTarget when a timer runs out. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_target{
    .key = "GhostHuntState.target", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "Player", .access = pm::sdk::Visibility::Internal};

/// The player the hunt falls back on, a Player, or null: the private Player field UpdateSpeed reads,
/// which it tests when target is not alive. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_searchTarget{
    .key = "GhostHuntState.searchTarget", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "Player", .facts = GhostHuntState_searchTarget_facts,
    .access = pm::sdk::Visibility::Private};

// The hunt's senses and its target. Each is one of several same-shape methods of the class (the
// obfuscator's look-alikes), told apart by what its own code compares or calls. Read from decompiled
// code of build 25811160.

inline constexpr const char* GhostHuntState_Player[] = {"Player"};
inline constexpr const char* GhostHuntState_Player_bool[] = {"Player", "bool"};
inline constexpr const char* GhostHuntState_ClosetZone[] = {"ClosetZone"};

// Two bool(Player, bool) methods: the sight test compares the ghost's behaviour type with the Deogen's
// (its second argument skips the Deogen's own sight), its look-alike with 0x4B.
inline constexpr pm::sdk::Fact GhostHuntState_Sees_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Deogen"}};

/// The hunt's sight test: false for a player in its killed list; otherwise true when the ghost's vision
/// sensor detects any of the player's losTargets (the Deogen sees every player unless the second
/// argument is set), and a pass adds the player to its seen list. Every target the hunt sets or keeps
/// passes it, and so does its kill. It throws for a player without losTargets. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Sees{
    .key = "GhostHuntState.Sees", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_Player_bool,
    .facts = GhostHuntState_Sees_facts};

// Of the class's bool(Player) methods only the voice test calls IsTalkingLoudly; its look-alike
// calls an obfuscated copy of it.
inline constexpr pm::sdk::Fact GhostHuntState_Hears_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "PlayerAudio.IsTalkingLoudly"}};

/// The hunt's voice test: whether a player it has not killed, within its hearing, is talking loudly
/// (PlayerAudio.IsTalkingLoudly with 0.1, on the player's Player.audio). Its hearing: always true for the
/// Deogen; otherwise the player's body (Player.body) within 9 m of the ghost's head (GhostAI.head), 2.5 m for
/// the Yokai and 1000 m for a player Player.wished marks, on the floor of the ghost's room
/// (LevelController.currentGhostRoom's LevelRoom.floor against the player's room's) unless wished, and the
/// player's room not RoomKind.outsideMap. Listen sends the hunt to such a player. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Hears{
    .key = "GhostHuntState.Hears", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_Player,
    .facts = GhostHuntState_Hears_facts};

// Two bool(Player) methods compare the behaviour type with the Deogen's, the voice test and the
// electronics test; only the electronics test calls Object.op_Implicit (its two look-alikes compare
// with no Deogen).
inline constexpr pm::sdk::Fact GhostHuntState_SensesGear_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Deogen"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "Object.op_Implicit"}};

/// The hunt's electronics test: whether a player it has not killed, within its range, carries
/// equipment that is switched on. Listen sends the hunt to such a player. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_SensesGear{
    .key = "GhostHuntState.SensesGear", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_Player,
    .facts = GhostHuntState_SensesGear_facts};

// The voice test has one caller, this.
inline constexpr pm::sdk::Fact GhostHuntState_Listen_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.Hears"}};

// Found only for the facts of SetCloset and lastPlayer.
/// The hunt's listening: for each living player outside the truck whom it may search for, when the
/// voice test or the electronics test passes, it makes that player its searchTarget and lastPlayer,
/// gives SetCloset the closet they hide in (null for none) and goes where they stand. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Listen{
    .key = "GhostHuntState.Listen", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .facts = GhostHuntState_Listen_facts};

// Seven void(ClosetZone) methods; six of them differ only in the flags they store on the closets.
// Listen calls this one and no other.
inline constexpr pm::sdk::Fact GhostHuntState_SetCloset_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostHuntState.Listen"}};

/// Sets the closet the hunt's closet search is for (searchCloset; null for none): when it changes, the
/// old closet's flag (a bool of an object the closet holds) is set and the new one's cleared.
/// SetTarget, Listen and a dozen other hunt methods call it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_SetCloset{
    .key = "GhostHuntState.SetCloset", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .params = GhostHuntState_ClosetZone,
    .facts = GhostHuntState_SetCloset_facts};

inline constexpr const char* GhostHuntState_out_Player[] = {"out Player"};
// Five bool(out Player) methods: the look in sight compares the hunt's ghost type with the Banshee's and
// runs the sight test; its three look-alikes are never called.
inline constexpr pm::sdk::Fact GhostHuntState_SightPick_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Banshee"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.Sees"}};

/// The hunt's look for a player in sight: for the Banshee (the hunt's ghostType is GhostType.Banshee) it
/// finds nobody; for any other type, with at least two players in the lobby, the first player whose
/// sight test (Sees) passes, into its out argument. TargetPick runs it after Listen and after its kept
/// target, and sets whoever it finds as the target. So a Banshee's sight goes only for the player it
/// keeps (sdk/internals/ghost-ai.md, "The Banshee's one target"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_SightPick{
    .key = "GhostHuntState.SightPick", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_out_Player,
    .facts = GhostHuntState_SightPick_facts};

// Of the three bool(Player) tests Listen calls, the voice test, the electronics test and this one, only
// this one calls Object.op_Inequality: it compares the ghost's Banshee target with null and with the player.
inline constexpr pm::sdk::Fact GhostHuntState_TargetAllows_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostHuntState.Listen"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "Object.op_Inequality"}};

/// Whether the ghost's Banshee target (GhostAI.bansheeTarget) lets the hunt go for a player: true for
/// anyone while that field is null, or its player's room is outside the map (RoomKind.outsideMap), its
/// player is dead (Player.isDead) or in the hunt's killed list; otherwise true only for that player. It
/// reads the field, not the ghost's type, and throws when that player has no room. Listen asks it for
/// every player before its voice and electronics tests, StartPick for the field's player and
/// KeepNearest for every player (sdk/internals/ghost-ai.md, "The Banshee's one target"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_TargetAllows{
    .key = "GhostHuntState.TargetAllows", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_Player,
    .facts = GhostHuntState_TargetAllows_facts};

// Of the class's bool() methods two ask TargetAllows, Listen and this one; only this one changes the
// ghost's state.
inline constexpr pm::sdk::Fact GhostHuntState_StartPick_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.TargetAllows"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.ChangeState"}};

/// The hunt's start pick, which the hunt's enter runs once: it sets the player the hunt keeps (a Player
/// field of the hunt that TargetPick and the chase test with Sees). The ghost's Banshee target
/// (GhostAI.bansheeTarget) becomes that kept player when TargetAllows passes for them and they are alive
/// and not outside the map; otherwise the start pick takes a player from the network's player list. With
/// nobody to keep it ends the hunt (GhostAI.ChangeState) and answers false. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_StartPick{
    .key = "GhostHuntState.StartPick", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .facts = GhostHuntState_StartPick_facts};

// Of the class's void() methods only the target pick calls SetTarget.
inline constexpr pm::sdk::Fact GhostHuntState_TargetPick_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.SetTarget"}};

// Mapped for the chase's facts.
/// The hunt's target pick, which the chase runs while the hunt has no target: Listen first; then the
/// player the hunt keeps, when Sees passes for them; then whoever SightPick finds. Each becomes the target
/// through SetTarget. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_TargetPick{
    .key = "GhostHuntState.TargetPick", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_TargetPick_facts};

// Four void() methods call TargetPick; only the chase is called (by the hunt's Execute), the other three
// are uncalled look-alikes.
inline constexpr pm::sdk::Fact GhostHuntState_Chase_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.TargetPick"}, {.is = pm::sdk::Fact::Is::Called}};

// Mapped for KeepNearest's facts.
/// The hunt's chase, every frame once the hunt has started: unless the hunt's ghostType is the Banshee's
/// or the Kormos's it first runs KeepNearest; then, with no target, it walks to the closet or player it
/// searches and takes the kept player as its target when Sees passes for them, or runs TargetPick; with a
/// target, it follows them and kills them (Kill). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Chase{
    .key = "GhostHuntState.Chase", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_Chase_facts};

// Two void() methods ask TargetAllows: this one and a door step; the chase calls only this one.
inline constexpr pm::sdk::Fact GhostHuntState_KeepNearest_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.TargetAllows"},
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "GhostHuntState.Chase"}};

/// Moves the player the hunt keeps to the nearest it may go for: each player who is alive, not dead, not
/// in the killed list, not outside the map and whom TargetAllows passes, when nearer to the ghost's head
/// (GhostAI.head) than the one kept. The chase runs it every frame unless the hunt's ghostType is the
/// Banshee's or the Kormos's, so a Banshee keeps the player StartPick chose for the whole hunt
/// (sdk/internals/ghost-ai.md, "The Banshee's one target"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_KeepNearest{
    .key = "GhostHuntState.KeepNearest", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_KeepNearest_facts};

inline constexpr const char* GhostHuntState_Kill_params[] = {"Player", "bool", "float", "float"};
// Three bool(Player, bool, float, float) methods: the kill compares the hunt's ghost type with the
// Deogen's and the Banshee's new target follows each kill; one look-alike compares with -0x57 and
// -0x44, and another (which compares with -0xB and -0x11) calls no SetNewBansheeTarget.
inline constexpr pm::sdk::Fact GhostHuntState_Kill_facts[] = {
    {.is = pm::sdk::Fact::Is::Compares, .method = "GhostType.Deogen"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.SetNewBansheeTarget"}};

/// The hunt's kill: false at once for a player in its killed list or while the ghost's smudge is alive;
/// unless its second argument forces it, the player must be within its reach (the third and fourth
/// arguments, -1 for its own), and for the Deogen in its sight. A kill then stops the ghost, clears the
/// hunt's target, searchTarget, closet and lastPlayer, calls SetNewBansheeTarget with the victim,
/// begins their death and adds them to the killed list. Its callers pass the hunt's target, the closet
/// search's searchTarget or a player they loop over; it has no type test of its own, so it kills
/// whoever it is given (sdk/internals/ghost-ai.md, "The Banshee's one target"). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Kill{
    .key = "GhostHuntState.Kill", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .params = GhostHuntState_Kill_params,
    .facts = GhostHuntState_Kill_facts};

// Five void(Player) methods; of them only the set-target calls SetCloset (its two look-alikes call
// a look-alike of SetCloset).
inline constexpr pm::sdk::Fact GhostHuntState_SetTarget_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.SetCloset"}};

/// Sets the hunt's target: the player becomes its target, searchTarget and lastPlayer, and
/// SetCloset gets the closet they hide in (null for none). Its only caller runs the sight test
/// before it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_SetTarget{
    .key = "GhostHuntState.SetTarget", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .params = GhostHuntState_Player,
    .facts = GhostHuntState_SetTarget_facts};

// Two bool() methods count the start delay down; the gate ends the hunt with GhostAI.ChangeState and
// shows the ghost with GhostAI.Appear, its look-alike calls look-alikes of both.
inline constexpr pm::sdk::Fact GhostHuntState_Started_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.ChangeState"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostAI.Appear"}};

/// The hunt's gate, which its Execute runs first each frame: it ends the hunt when its time is up or it
/// has no player to go for; otherwise it is true once startDelay is below 0, and until then counts
/// startDelay down by the frame's time and shows the ghost the frame it runs out. Execute runs the
/// hunt's tick (chase, speed, kill) only while the gate is true. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Started{
    .key = "GhostHuntState.Started", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "bool", .facts = GhostHuntState_Started_facts};

// The state's overrides and its many void() look-alikes: only the exit calls ResetHuntEffects and ends
// with a jump to SetCloset.
inline constexpr pm::sdk::Fact GhostHuntState_Exit_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GameController.ResetHuntEffects"},
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostHuntState.SetCloset"}};

// Found only for its facts.
/// The hunt's end, an override of the state interface: it stops the hunt's animation and speed, and
/// when the hunt was on ends its effects (GameController.ResetHuntEffects), sends the ghost back to
/// its room, completes the escape objective when `seen` is not empty, and clears the closet search
/// (SetCloset(null)). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_Exit{
    .key = "GhostHuntState.Exit", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "void", .facts = GhostHuntState_Exit_facts};

// Found only for its facts.
/// The hunt state's constructor, (GhostAI, NavMeshAgent, PhotonView, bool): it makes the seen and
/// killed lists and gives the hunt's numbers their starting values. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_ctor{.key = "GhostHuntState.ctor", .kind = pm::sdk::Kind::Method,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "GhostHuntState", .name = ".ctor"};

// Of the private Player fields Listen stores two, searchTarget and this; UpdateSpeed reads searchTarget
// and not this. (SetTarget stores them too, but it reuses the register that holds `this`, so the
// resolver cannot count its stores.)
inline constexpr pm::sdk::Fact GhostHuntState_lastPlayer_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.Listen"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.UpdateSpeed", .negate = true}};

// Of the hunt's four Player fields, the start pick and the keep-nearest both store only the one they keep.
inline constexpr pm::sdk::Fact GhostHuntState_kept_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.KeepNearest"},
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.StartPick"}};

/// The player the hunt keeps, a Player, or null: StartPick sets it once as the hunt starts (the Banshee's target when
/// it may, otherwise a player from the network's list), KeepNearest moves it to the nearest player it may go for each
/// frame (not for the Banshee or the Kormos), and the chase tests it with Sees. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_kept{
    .key = "GhostHuntState.kept", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "Player", .facts = GhostHuntState_kept_facts,
    .access = pm::sdk::Visibility::Private};

/// The player the hunt last went for, a Player, or null: SetTarget and Listen set it with searchTarget,
/// and while it is set the hunt's wander stays near where that player was. Its kill clears it.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_lastPlayer{
    .key = "GhostHuntState.lastPlayer", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "Player", .facts = GhostHuntState_lastPlayer_facts,
    .access = pm::sdk::Visibility::Private};

// Of the two List<Player> fields the sight test reads both, the exit only the seen list.
inline constexpr pm::sdk::Fact GhostHuntState_seen_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.Exit"}};
inline constexpr pm::sdk::Fact GhostHuntState_killed_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.Sees"},
    {.is = pm::sdk::Fact::Is::Loads, .method = "GhostHuntState.Exit", .negate = true}};

/// The players the hunt has seen, a List<Player>: the sight test adds each player it passes, once,
/// and the hunt's end reads it for the escape objective. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_seen{
    .key = "GhostHuntState.seen", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "List<Player>", .facts = GhostHuntState_seen_facts};

/// The players the hunt has killed, a List<Player>: its tests are false for a player in it. The
/// constructor makes it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_killed{
    .key = "GhostHuntState.killed", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "List<Player>", .facts = GhostHuntState_killed_facts};

/// The closet the hunt's closet search is for, a ClosetZone, or null: the class's one field of that
/// type, which SetCloset sets. The search goes to it, checks it (ClosetZone.CheckCloset) and kills
/// whoever hides there. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_searchCloset{
    .key = "GhostHuntState.searchCloset", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "ClosetZone"};

// The gate reads two floats of the hunt and writes one, this.
inline constexpr pm::sdk::Fact GhostHuntState_startDelay_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "GhostHuntState.Started"}};

/// Seconds left of the hunt's start delay. One of the state's interface overrides sets it from a
/// whole-number setting; the gate counts it down, and the hunt's chase, speed and kill run only once
/// it is below 0. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_startDelay{
    .key = "GhostHuntState.startDelay", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "float", .facts = GhostHuntState_startDelay_facts};

// The constructor stores 15 float constants; 1.1 and 1.5 only into these two.
inline constexpr pm::sdk::Fact GhostHuntState_killReach_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostHuntState.ctor", .value = std::bit_cast<int32_t>(1.1f)}};
inline constexpr pm::sdk::Fact GhostHuntState_slowKillReach_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostHuntState.ctor", .value = std::bit_cast<int32_t>(1.5f)}};

/// The hunt's kill reach in metres (1.1 from the constructor): the kill's reach test passes at once
/// for a player closer than it to the ghost. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_killReach{
    .key = "GhostHuntState.killReach", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "float", .facts = GhostHuntState_killReach_facts};

/// The hunt's slower kill reach in metres (1.5 from the constructor): the kill's reach test passes for
/// a player closer than it once they have stayed so for a second (the constructor's 1.0 beside it,
/// doubled for the Deogen). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_slowKillReach{
    .key = "GhostHuntState.slowKillReach", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "GhostHuntState", .type = "float",
    .facts = GhostHuntState_slowKillReach_facts};

// The constructor stores 4.0 only into the throw timer.
inline constexpr pm::sdk::Fact GhostHuntState_throwTimer_facts[] = {
    {.is = pm::sdk::Fact::Is::StoresConst, .method = "GhostHuntState.ctor", .value = std::bit_cast<int32_t>(4.0f)}};

/// The hunt's throw timer in seconds (4.0 from the constructor). The hunt update counts it down by
/// deltaTime and, once it runs out, sets it back up and throws: GhostActivity.GetPropToThrow, a roll
/// that holds back half the throws unless the hunt's ghost type is the Poltergeist
/// (RandomRangeInt(0, 2) == 1 means none), then Prop.TryThrow(true, true). Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_throwTimer{
    .key = "GhostHuntState.throwTimer", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "GhostHuntState", .type = "float", .facts = GhostHuntState_throwTimer_facts};

/// The hunt's ghost type, which its branches test: the class's one GhostType field, which the
/// constructor copies from the ghost's GhostInfo. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostHuntState_ghostType{.key = "GhostHuntState.ghostType",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "GhostHuntState",
                                                             .type = "@GhostType"};

}  // namespace pm::map

/// @file
/// @brief The game API for the hunt state (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game. A mod
/// sets a ghost's hunt speed through content rules (sdk/api/content.h, GhostRules::huntSpeed), which write the base
/// and the speed-up here, never the speed they produce.
namespace phasmo {
/// @addtogroup ghost
/// @{

/// The ghost's hunt state.
struct GhostHuntState {
  /// The type: klass(), is(obj), type_object(), make() and make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostHuntState> type{};
  /// The hunting ghost's GhostAI: ghost.get(hunt). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_ghost, void*> ghost{};
  /// The base speed: baseSpeed.get(hunt), baseSpeed.set(hunt, metresPerSecond). Unresolved: 0, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::GhostHuntState_baseSpeed, float> baseSpeed{};
  /// The line-of-sight speed-up: sightSpeedUp.get(hunt), sightSpeedUp.set(hunt, factor). Unresolved:
  /// 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostHuntState_sightSpeedUp, float> sightSpeedUp{};
  /// RefreshBase(hunt). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_RefreshBase, void(void*)> RefreshBase{};
  /// UpdateSpeed(hunt). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_UpdateSpeed, void(void*)> UpdateSpeed{};
  /// The player the hunt follows first, a Player: target.get(hunt), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_target, void*> target{};
  /// The player it falls back on, a Player: searchTarget.get(hunt), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_searchTarget, void*> searchTarget{};
  /// The player it last went for, a Player: lastPlayer.get(hunt), or null. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_lastPlayer, void*> lastPlayer{};
  /// The player the hunt keeps (StartPick, then KeepNearest): kept.get(state). Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_kept, void*> kept{};
  /// The players it has seen, a List<Player>: seen.get(hunt), read as a pm::api::List. Unresolved: null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_seen, void*> seen{};
  /// The players it has killed, a List<Player>: killed.get(hunt), read as a pm::api::List. Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_killed, void*> killed{};
  /// The closet its closet search is for, a ClosetZone: searchCloset.get(hunt), or null. Unresolved:
  /// null.
  static constexpr pm::api::Field<pm::map::GhostHuntState_searchCloset, void*> searchCloset{};
  /// Seconds left of its start delay: startDelay.get(hunt). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostHuntState_startDelay, float> startDelay{};
  /// Its kill reach in metres: killReach.get(hunt), killReach.set(hunt, metres). Unresolved: 0, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::GhostHuntState_killReach, float> killReach{};
  /// Its slower kill reach in metres: slowKillReach.get(hunt), slowKillReach.set(hunt, metres).
  /// Unresolved: 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostHuntState_slowKillReach, float> slowKillReach{};
  /// Its throw timer in seconds: throwTimer.get(hunt). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostHuntState_throwTimer, float> throwTimer{};
  /// Its ghost type, a GhostType number: ghostType.get(hunt). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::GhostHuntState_ghostType, int32_t> ghostType{};
  /// Sees(hunt, player, skipDeogen): the sight test, which records a player it passes in seen. A mod
  /// usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_Sees, bool(void*, void*, bool)> Sees{};
  /// Hears(hunt, player): the voice test. A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_Hears, bool(void*, void*)> Hears{};
  /// SensesGear(hunt, player): the electronics test. A mod usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_SensesGear, bool(void*, void*)> SensesGear{};
  /// SetTarget(hunt, player): makes the player its target and searches the closet they hide in.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_SetTarget, void(void*, void*)> SetTarget{};
  /// SetCloset(hunt, closet): the closet its closet search is for, null for none. Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_SetCloset, void(void*, void*)> SetCloset{};
  /// Kill(hunt, player, force, reach, slowReach): the hunt's kill of `player`, true when it killed. A mod
  /// usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_Kill, bool(void*, void*, bool, float, float)> Kill{};
  /// SightPick(hunt, &player): the player its sight finds, as the target pick asks. A mod usually hooks
  /// it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_SightPick, bool(void*, void**)> SightPick{};
  /// StartPick(hunt): the start pick, which keeps the ghost's Banshee target as the hunt's player. A mod
  /// usually hooks it. Unresolved: false.
  static constexpr pm::api::Method<pm::map::GhostHuntState_StartPick, bool(void*)> StartPick{};
  /// KeepNearest(hunt): moves the hunt's kept player to the nearest it may go for. A mod usually hooks
  /// it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_KeepNearest, void(void*)> KeepNearest{};
  /// Exit(hunt): the hunt's end. A mod usually hooks it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::GhostHuntState_Exit, void(void*)> Exit{};
};

/// @}
}  // namespace phasmo
