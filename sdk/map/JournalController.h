#pragma once

// The journal: JournalController (one on every player's book, other players' included), its guess
// buttons on the evidence spread's ghost page (GhostButton), the controller that syncs their choices
// (JournalGhostSyncController), the game's Ghost value its lists hold, and the TextLocaliserUI that
// writes each of its texts in the player's language. Types and most members keep their real names;
// the obfuscated ones are found by shape and machine-code facts. Notes read from decompiled code are
// from build 25811160. sdk/systems/journal.md ties them together.

#include "entry.h"
#include "game_api.h"

namespace pm::map {

inline constexpr const char* Journal_int[] = {"int"};
inline constexpr const char* Journal_string[] = {"string"};
inline constexpr const char* Journal_evidence[] = {"@EvidenceType"};
// Awake calls one void() method of the controller that reads GhostData.GetGhostData: the builder of
// the ghost lists (its look-alikes are never called).
inline constexpr pm::sdk::Fact JournalController_RebuildGhosts_facts[] = {
    {.is = pm::sdk::Fact::Is::Calls, .method = "GhostData.GetGhostData"},
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "JournalController.Awake"}};
// Of the controller's two List<Ghost> fields the builder fills this one; SetGhostTypes refills the
// other with the ghosts the evidence leaves.
inline constexpr pm::sdk::Fact JournalController_allGhosts_facts[] = {
    {.is = pm::sdk::Fact::Is::Loads, .method = "JournalController.RebuildGhosts"}};

/// The journal (Assembly-CSharp, global namespace), a MonoBehaviour on the object under each player's
/// book that holds its pages (Journal2.0Canvas). Every player's book is on every client. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController{.key = "JournalController",
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .name = "JournalController", .assembly = "Assembly-CSharp",
                                                      .ns = ""};

/// Wakes the journal: builds its ghost lists (RebuildGhosts), then has the guess buttons' sync
/// controller read the buttons (Initialise). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_Awake{.key = "JournalController.Awake",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "JournalController", .name = "Awake"};

/// The pages, a GameObject[]: a page's number is its index, and GoToPage(int), the page sync between
/// players and the tabs use it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_pages{.key = "JournalController.pages",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "JournalController", .name = "pages"};

/// The book's tabs, a JournalTab[]: each covers the pages from its min to its max. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_journalTabs{.key = "JournalController.journalTabs",
                                                                  .kind = pm::sdk::Kind::Field,
                                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                                  .owner = "JournalController",
                                                                  .name = "journalTabs"};

/// The GhostCollection the journal lists its ghosts from, a serialized reference (the host adds custom
/// ghosts to a journal only when it is the contract's GhostController.allGhostsCollection). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_allGhostsData{.key = "JournalController.allGhostsData",
                                                                    .kind = pm::sdk::Kind::Field,
                                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                                    .owner = "JournalController",
                                                                    .name = "allGhostsData"};

/// The guess buttons on the evidence spread's ghost page, a GhostButton[]. RebuildGhosts pairs each
/// listed ghost with the button of its type, and the sync controller reads them. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_allGhostTypeButtons{
    .key = "JournalController.allGhostTypeButtons", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "allGhostTypeButtons"};

/// The controller that sends and receives the players' guesses: the class's one
/// JournalGhostSyncController field. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_ghostSync{.key = "JournalController.ghostSync",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "JournalController",
                                                                .type = "@JournalGhostSyncController"};

/// Every ghost the journal lists, a List<Ghost> in its collection's order, filled by RebuildGhosts.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_allGhosts{.key = "JournalController.allGhosts",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .confidence = pm::sdk::Confidence::Decompiled,
                                                                .owner = "JournalController",
                                                                .type = "List<@Ghost>",
                                                                .facts = JournalController_allGhosts_facts};

/// Each listed ghost's guess button, a Dictionary<Ghost, GhostButton>, filled by RebuildGhosts (with
/// Add, so a ghost already in it makes the builder throw). SetGhostTypes crosses out the buttons of
/// the ghosts the chosen evidence rules out through it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_ghostButtons{
    .key = "JournalController.ghostButtons", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "Dictionary<@Ghost, @GhostButton>"};

/// Builds the ghost lists: for each GhostData of allGhostsData, adds its Ghost to allGhosts and,
/// with the button of its type in allGhostTypeButtons, to ghostButtons. Awake runs it first. A
/// private void(). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_RebuildGhosts{
    .key = "JournalController.RebuildGhosts", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "void", .facts = JournalController_RebuildGhosts_facts};

/// The evidence buttons on the evidence spread's evidence page, an EvidenceButton[]: the evidence sync
/// controller reads them, and ScratchWrongEvidence and HighlightGhostEvidence walk them, each by its
/// evidence number. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_evidenceButtons{
    .key = "JournalController.evidenceButtons", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "allEvidenceTypeButtons"};

/// The controller that sends and receives the players' evidence marks: the class's one
/// JournalEvidenceSyncController field. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_evidenceSync{.key = "JournalController.evidenceSync",
                                                                   .kind = pm::sdk::Kind::Field,
                                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                                   .owner = "JournalController",
                                                                   .type = "@JournalEvidenceSyncController"};

/// Marks an evidence as found: takes it off the ruled-out list, adds it to the found list (once), then
/// runs SetGhostTypes. An evidence button's override of its state change runs this body, inlined, with
/// the button's evidence number when its state turns to 1. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_AddEvidence{.key = "JournalController.AddEvidence",
                                                                  .kind = pm::sdk::Kind::Method,
                                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                                  .owner = "JournalController", .name = "AddEvidence",
                                                                  .params = Journal_evidence};

/// Marks an evidence as ruled out: takes it off the found list, adds it to the ruled-out list (once),
/// then runs SetGhostTypes. The button's state change runs this body, inlined, at state 2. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_FilterOutEvidence{
    .key = "JournalController.FilterOutEvidence", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "FilterOutEvidence",
    .params = Journal_evidence};

/// Takes an evidence off both lists, then runs SetGhostTypes. The button's state change runs this body,
/// inlined, at state 0. An obfuscated copy has the same body. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_RemoveEvidence{
    .key = "JournalController.RemoveEvidence", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "RemoveEvidence",
    .params = Journal_evidence};

/// The evidence filter: crosses out each guess button whose ghost the chosen evidence rules out, through
/// ghostButtons. A ghost stays when its three evidence numbers hold every found evidence and none of the
/// ruled-out ones; there is no count of evidence in it. Start runs it, and every change of evidence. An
/// obfuscated copy of the same length exists under another name, never called. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_SetGhostTypes{.key = "JournalController.SetGhostTypes",
                                                                    .kind = pm::sdk::Kind::Method,
                                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                                    .owner = "JournalController",
                                                                    .name = "SetGhostTypes"};

/// Turns to the page with that number and tells the other players' copies of this journal (the page
/// sync, by number). The overload taking a GameObject is another method. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_GoToPage{.key = "JournalController.GoToPage",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "JournalController", .name = "GoToPage",
                                                               .params = Journal_int};

/// The page sync (a PunRPC, sent AllBuffered by GoToPage(int) while the journal's view is the local
/// player's; GoToPage runs it on this client alone otherwise, with the view's owner as the sender). It
/// first checks that the sender owns the book: a sender who does not is reported by the game, and the
/// call changes nothing. For the owner it shows or hides the page arrows, writes the page numbers' labels
/// and plays the page-turn sound, stores the number as the current page, deactivates every page, then
/// shows that page, throwing IndexOutOfRangeException for a number not inside this client's pages and
/// leaving the book blank. Its message info is a struct passed by address. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_PageSync{.key = "JournalController.PageSync",
                                                               .kind = pm::sdk::Kind::Method,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "JournalController", .name = "PageSync"};

// The current page: the int the page sync stores.
inline constexpr pm::sdk::Fact JournalController_currentPage_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "JournalController.PageSync"}};

/// The page the book is on, an int: the page sync (JournalController.PageSync) stores it, Start sets it to -1, and
/// GoToPage(int) does nothing for the page it already holds; NextPage and PreviousPage step from it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_currentPage{
    .key = "JournalController.currentPage", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "int", .facts = JournalController_currentPage_facts};

/// The journal's PhotonView, the class's one PhotonView field: GoToPage(int) sends the page sync to
/// every client only while it is the local player's (its IsMine), else runs it on this client alone.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_photonView{
    .key = "JournalController.photonView", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "@PhotonView"};

/// The player's guess, by ghost type (the GhostType of the Ghost SelectGhost copied from allGhosts
/// into the journal by type; None after UnselectGhost). Returns it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_GetGhostType{.key = "JournalController.GetGhostType",
                                                                   .kind = pm::sdk::Kind::Method,
                                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                                   .owner = "JournalController",
                                                                   .name = "GetGhostType"};

/// Clears the player's guess: the guess's type becomes None and the rest of it zero. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_UnselectGhost{.key = "JournalController.UnselectGhost",
                                                                    .kind = pm::sdk::Kind::Method,
                                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                                    .owner = "JournalController",
                                                                    .name = "UnselectGhost"};

/// A tab of the book, a class nested in JournalController. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalTab{.key = "JournalTab", .confidence = pm::sdk::Confidence::Decompiled,
                                               .owner = "JournalController", .name = "JournalTab"};

/// The first page the tab covers, a float holding a page number. The tab of the current page is the
/// first whose min <= page <= max. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalTab_min{.key = "JournalTab.min", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "JournalTab", .name = "min", .type = "float"};

/// The last page the tab covers, a float holding a page number. In the PCPlayer prefab the Ghosts tab's
/// is 55, the page count (pages 0 to 54), one past its last page (assetcheck snapshot). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalTab_max{.key = "JournalTab.max", .kind = pm::sdk::Kind::Field,
                                                   .confidence = pm::sdk::Confidence::Decompiled,
                                                   .owner = "JournalTab", .name = "max", .type = "float"};

/// A guess button on the evidence spread's ghost page (Assembly-CSharp, global namespace), a
/// NetworkThreeStateButton. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostButton{.key = "GhostButton", .confidence = pm::sdk::Confidence::Decompiled,
                                                .name = "GhostButton", .assembly = "Assembly-CSharp", .ns = ""};

/// The ghost type the button guesses: the class's one GhostType field. RebuildGhosts pairs it with the
/// ghost of that type, and the sync controller sends it. Decompiled.
inline constexpr pm::sdk::EntryDesc GhostButton_type{.key = "GhostButton.type", .kind = pm::sdk::Kind::Field,
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .owner = "GhostButton",
                                                     .type = "@GhostType"};

/// A journal button with three states (Assembly-CSharp, global namespace), a MonoBehaviour: the base of
/// the shared buttons. Decompiled.
inline constexpr pm::sdk::EntryDesc ThreeStateButton{.key = "ThreeStateButton",
                                                     .confidence = pm::sdk::Confidence::Decompiled,
                                                     .name = "ThreeStateButton", .assembly = "Assembly-CSharp",
                                                     .ns = ""};

/// The button's own text (a TextMeshProUGUI): on an evidence button the evidence's name, whose
/// TextLocaliserUI holds the key the ghost pages show for that evidence too. Decompiled.
inline constexpr pm::sdk::EntryDesc ThreeStateButton_textLabel{.key = "ThreeStateButton.textLabel",
                                                               .kind = pm::sdk::Kind::Field,
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .owner = "ThreeStateButton", .name = "textLabel"};

/// A journal button whose state the players share (Assembly-CSharp, global namespace). Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkThreeStateButton{.key = "NetworkThreeStateButton",
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .name = "NetworkThreeStateButton",
                                                            .assembly = "Assembly-CSharp", .ns = ""};

/// What the button raises when the player changes it (the event OnUserStateChanged's delegate, an
/// Action<NetworkThreeStateButton>): the class's one field of that type. The sync controller's
/// Initialise subscribes to it on every button it reads. Decompiled.
inline constexpr pm::sdk::EntryDesc NetworkThreeStateButton_userStateChanged{
    .key = "NetworkThreeStateButton.userStateChanged", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "NetworkThreeStateButton",
    .type = "Action<@NetworkThreeStateButton>"};

/// The controller that sends a player's guess to the others and shows theirs (Assembly-CSharp, global
/// namespace), on the evidence spread's ghost page. Its generic base keeps the buttons it reads in
/// Initialise (a public void()), which subscribes to each one's userStateChanged and to the players
/// leaving; the base is generic, so the host runs Initialise through the runtime by name. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalGhostSyncController{.key = "JournalGhostSyncController",
                                                               .confidence = pm::sdk::Confidence::Decompiled,
                                                               .name = "JournalGhostSyncController",
                                                               .assembly = "Assembly-CSharp", .ns = ""};

/// The controller that sends a player's evidence marks to the others and shows theirs (Assembly-CSharp,
/// global namespace), on the evidence spread's evidence page: the guess sync's generic base for
/// evidence buttons. Its getter of the buttons returns the journal's evidenceButtons and its getter of
/// a button's value the button's evidence number, so a mark is sent and matched by that number.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalEvidenceSyncController{.key = "JournalEvidenceSyncController",
                                                                  .confidence = pm::sdk::Confidence::Decompiled,
                                                                  .name = "JournalEvidenceSyncController",
                                                                  .assembly = "Assembly-CSharp", .ns = ""};

/// An evidence button on the evidence spread's evidence page (Assembly-CSharp, global namespace), a
/// NetworkThreeStateButton: a row with the evidence's name and, under it, a line saying how it is
/// found, each a text with a TextLocaliserUI. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceButton{.key = "EvidenceButton", .confidence = pm::sdk::Confidence::Decompiled,
                                                   .name = "EvidenceButton", .assembly = "Assembly-CSharp", .ns = ""};

/// The evidence the button marks: the class's one EvidenceType field. Its state change hands it to the
/// journal's evidence lists, and the sync controller sends it. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceButton_evidence{.key = "EvidenceButton.evidence",
                                                            .kind = pm::sdk::Kind::Field,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "EvidenceButton",
                                                            .type = "@EvidenceType"};

/// The line under the evidence's name (a TextMeshProUGUI), saying how the evidence is found. Its
/// TextLocaliserUI's key is the name's key with a suffix, the same for every evidence of the game's.
/// SetStateColor colours it with the row. Decompiled.
inline constexpr pm::sdk::EntryDesc EvidenceButton_descriptionLabel{
    .key = "EvidenceButton.descriptionLabel", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "EvidenceButton", .name = "descriptionLabel"};

/// A ghost as the journal's lists hold it (Assembly-CSharp, global namespace), a struct: its type, its
/// name and its three evidence, made by GhostData.GetGhostData. Decompiled.
inline constexpr pm::sdk::EntryDesc Ghost{.key = "Ghost", .confidence = pm::sdk::Confidence::Decompiled,
                                          .name = "Ghost", .assembly = "Assembly-CSharp", .ns = ""};

/// Writes a text object's words in the player's language (Assembly-CSharp, global namespace), a
/// MonoBehaviour beside a TextMeshPro text. Its writer puts LocalisationSystem.TryGetLocalisedValue of
/// its key into the text when it wakes, when it is enabled after the language changed, when the language
/// changes (it listens while enabled), and when its key is set; it shows what stands before the text's
/// first @. Decompiled.
inline constexpr pm::sdk::EntryDesc TextLocaliserUI{.key = "TextLocaliserUI",
                                                    .confidence = pm::sdk::Confidence::Decompiled,
                                                    .name = "TextLocaliserUI", .assembly = "Assembly-CSharp",
                                                    .ns = ""};

/// The localisation key it writes, a string. Decompiled.
inline constexpr pm::sdk::EntryDesc TextLocaliserUI_key{.key = "TextLocaliserUI.key", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "TextLocaliserUI", .name = "key",
                                                        .type = "string"};

/// Sets the key and writes its text at once, in the player's language. Decompiled.
inline constexpr pm::sdk::EntryDesc TextLocaliserUI_set_Key{.key = "TextLocaliserUI.set_Key",
                                                            .kind = pm::sdk::Kind::Method,
                                                            .confidence = pm::sdk::Confidence::Decompiled,
                                                            .owner = "TextLocaliserUI", .name = "set_Key",
                                                            .params = Journal_string};

// The journal's media page: the photos, videos and sounds a team has taken. Each player's journal is on
// every client. Read from the class dump and decompiles of build 25811160.

inline constexpr const char* Journal_bool[] = {"bool"};

/// Opens or closes the journal: it flips JournalController.open and shows or hides the book.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_OpenCloseJournal{
    .key = "JournalController.OpenCloseJournal", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "OpenCloseJournal",
    .params = Journal_bool};

inline constexpr pm::sdk::Fact JournalController_open_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "JournalController.OpenCloseJournal"}};

/// Whether the journal is open: the bool OpenCloseJournal flips. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_open{.key = "JournalController.open", .kind = pm::sdk::Kind::Field,
                                                           .confidence = pm::sdk::Confidence::Decompiled,
                                                           .owner = "JournalController",
                                                           .type = "bool", .facts = JournalController_open_facts};

/// The journal's media page (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Journal_PhotoPage{.key = "Journal_PhotoPage", .name = "Journal_PhotoPage",
                                                      .assembly = "Assembly-CSharp", .ns = ""};

/// The journal's media page: its one Journal_PhotoPage field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc JournalController_photoPage{.key = "JournalController.photoPage",
                                                                .kind = pm::sdk::Kind::Field,
                                                                .owner = "JournalController",
                                                                .type = "@Journal_PhotoPage"};

/// The page's photo slots, a JournalMedia[]. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Journal_PhotoPage_photos{.key = "Journal_PhotoPage.photos",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .owner = "Journal_PhotoPage", .name = "journalPhotos"};

/// The page's video slots, a JournalMedia[]. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Journal_PhotoPage_videos{.key = "Journal_PhotoPage.videos",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .owner = "Journal_PhotoPage", .name = "journalVideos"};

/// The page's sound slots, a JournalMedia[]. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Journal_PhotoPage_sounds{.key = "Journal_PhotoPage.sounds",
                                                             .kind = pm::sdk::Kind::Field,
                                                             .owner = "Journal_PhotoPage", .name = "journalSounds"};

/// One media slot of the page (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc JournalMedia{.key = "JournalMedia", .name = "JournalMedia",
                                                 .assembly = "Assembly-CSharp", .ns = ""};

/// A piece of media (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Media{.key = "Media", .name = "Media", .assembly = "Assembly-CSharp", .ns = ""};

/// Puts media in the slot: it stores the Media and sets JournalMedia.filled. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalMedia_AddMedia{.key = "JournalMedia.AddMedia", .kind = pm::sdk::Kind::Method,
                                                          .confidence = pm::sdk::Confidence::Decompiled,
                                                          .owner = "JournalMedia", .name = "AddMedia"};

/// Takes the media out of the slot, the game's own removal: it clears JournalMedia.filled and drops the
/// Media from the team's lists. An override of the slot's base class's method. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalMedia_RemoveMedia{.key = "JournalMedia.RemoveMedia",
                                                             .kind = pm::sdk::Kind::Method,
                                                             .confidence = pm::sdk::Confidence::Decompiled,
                                                             .owner = "JournalMedia", .name = "RemoveMedia",
                                                             .params = Journal_bool};

/// The Media the slot shows: JournalMedia's one Media field. Inferred: from the class dump.
inline constexpr pm::sdk::EntryDesc JournalMedia_media{.key = "JournalMedia.media", .kind = pm::sdk::Kind::Field,
                                                       .owner = "JournalMedia", .type = "@Media"};

inline constexpr pm::sdk::Fact JournalMedia_filled_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "JournalMedia.AddMedia"}};

/// Whether the slot holds media: the bool AddMedia sets and RemoveMedia clears. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalMedia_filled{.key = "JournalMedia.filled", .kind = pm::sdk::Kind::Field,
                                                        .confidence = pm::sdk::Confidence::Decompiled,
                                                        .owner = "JournalMedia", .type = "bool",
                                                        .facts = JournalMedia_filled_facts};

/// What a piece of media shows, a MediaValues. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Media_value{.key = "Media.value", .kind = pm::sdk::Kind::Field, .owner = "Media",
                                                .name = "value"};

/// What a kind of media is (Assembly-CSharp, global namespace). Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MediaValues{.key = "MediaValues", .name = "MediaValues", .assembly = "Assembly-CSharp",
                                                .ns = ""};

/// The media's subject, a MediaSubject number. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc MediaValues_evidenceType{.key = "MediaValues.evidenceType",
                                                             .kind = pm::sdk::Kind::Field, .owner = "MediaValues",
                                                             .name = "evidenceType"};

/// A thing in the world a photo, video or sound can capture (Assembly-CSharp, global namespace).
/// Inferred: from its name.
inline constexpr pm::sdk::EntryDesc Evidence{.key = "Evidence", .name = "Evidence", .assembly = "Assembly-CSharp",
                                             .ns = ""};

inline constexpr const char* MediaSubject_values[] = {"emfSpot", "ouijaBoard", "fingerprint", "emf5"};

// Found by its values.
/// What a piece of media shows, an enum nested in Evidence (name obfuscated, value names real):
/// emfSpot, ouijaBoard, fingerprint, ... emf5; the numbers MediaValues.evidenceType holds. Inferred:
/// from the class dump.
inline constexpr pm::sdk::EntryDesc MediaSubject{.key = "MediaSubject", .owner = "Evidence",
                                                 .values = MediaSubject_values};

/// Media of an EMF reading: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_emfSpot{
    .key = "MediaSubject.emfSpot", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "emfSpot"};

/// Media of a fingerprint: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_fingerprint{
    .key = "MediaSubject.fingerprint", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "fingerprint"};

/// Media of a footstep: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_footstep{
    .key = "MediaSubject.footstep", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "footstep"};

/// Media of the ghost itself: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghost{
    .key = "MediaSubject.ghost", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghost"};

/// Media of dirty water: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_dirtyWater{
    .key = "MediaSubject.dirtyWater", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "dirtyWater"};

/// Media of ghost writing: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostWriting{
    .key = "MediaSubject.ghostWriting", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostWriting"};

/// Media of a used crucifix: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_usedCrucifix{
    .key = "MediaSubject.usedCrucifix", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "usedCrucifix"};

/// Media of a D.O.T.S. ghost: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_dotsGhost{
    .key = "MediaSubject.dotsGhost", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "dotsGhost"};

/// Media of a ghost orb: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostOrb{
    .key = "MediaSubject.ghostOrb", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostOrb"};

/// Media of salt stepped in: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_salt{
    .key = "MediaSubject.salt", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "salt"};

/// Media of a freezing temperature: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_freezingTemperature{
    .key = "MediaSubject.freezingTemperature", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "freezingTemperature"};

/// Media of ghost smoke: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostSmoke{
    .key = "MediaSubject.ghostSmoke", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostSmoke"};

/// Media of a camouflaged ghost: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_camoGhost{
    .key = "MediaSubject.camoGhost", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "camoGhost"};

/// Media of a paranormal sound: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_paranormalSound{
    .key = "MediaSubject.paranormalSound", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "paranormalSound"};

/// Media of a spirit box answer: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_spiritBox{
    .key = "MediaSubject.spiritBox", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "spiritBox"};

/// Media of a hunt: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostHunt{
    .key = "MediaSubject.ghostHunt", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostHunt"};

/// Media of a banshee's wail: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_bansheeWail{
    .key = "MediaSubject.bansheeWail", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "bansheeWail"};

/// Media of a spirit box breath: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_spiritBoxBreath{
    .key = "MediaSubject.spiritBoxBreath", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "spiritBoxBreath"};

/// Media of a tripped motion sensor: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_motionSensor{
    .key = "MediaSubject.motionSensor", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "motionSensor"};

/// Media of a shadow ghost: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_shadowGhost{
    .key = "MediaSubject.shadowGhost", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "shadowGhost"};

/// Media of a ghost's groan: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostGroan{
    .key = "MediaSubject.ghostGroan", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostGroan"};

/// Media of a ghost's laugh: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostLaugh{
    .key = "MediaSubject.ghostLaugh", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostLaugh"};

/// Media of a ghost talking: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostTalk{
    .key = "MediaSubject.ghostTalk", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostTalk"};

/// Media of a ghost's whisper: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_ghostWhisper{
    .key = "MediaSubject.ghostWhisper", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "ghostWhisper"};

/// Media of an Obake's fingerprint: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_obakeFingerprint{
    .key = "MediaSubject.obakeFingerprint", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "obakeFingerprint"};

/// Media of a crucifix burnt in the chapel: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_burningChapelCrucifix{
    .key = "MediaSubject.burningChapelCrucifix", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "burningChapelCrucifix"};

/// Media of an Obake's shapeshift: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_obakeShapeshift{
    .key = "MediaSubject.obakeShapeshift", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "obakeShapeshift"};

/// Media of an EMF 5 reading: one of the subjects the Lethe's fading counts. Inferred: from the value's name.
inline constexpr pm::sdk::EntryDesc MediaSubject_emf5{
    .key = "MediaSubject.emf5", .kind = pm::sdk::Kind::EnumValue, .owner = "MediaSubject", .name = "emf5"};

// The book's opening and closing, as the game runs them for a player's own journal: the controller's
// networked state change, the rig that carries the book in the hands and turns its tabs, and the pause
// menu's player list. Read from the class dump and decompiles of build 25811160.

inline constexpr const char* Journal_StateChangeSync_params[] = {"bool", "bool", "PhotonMessageInfo"};
inline constexpr const char* Journal_bool_bool[] = {"bool", "bool"};
inline constexpr const char* Journal_GameObject[] = {"GameObject"};

/// The pause menu's player list: the class's one PlayersList field. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_playersList{
    .key = "JournalController.playersList", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "@PlayersList"};

/// The rig that carries this book: the class's one JournalRigController field. StateChangeSync runs the
/// rig's OpenCloseJournalNetworked and UpdateJournalTabs through it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_rig{
    .key = "JournalController.rig", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "@JournalRigController"};

/// The pages' GraphicRaycaster, which takes the clicks on the open book. JournalController.ShowCanvas
/// switches it on with the canvas only for this client's own journal. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_canvasRaycaster{
    .key = "JournalController.canvasRaycaster", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "canvasRaycaster"};

/// The AudioSource the book's sounds play from: StateChangeSync stops it and plays openClip or closeClip
/// on it. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_openSource{
    .key = "JournalController.openSource", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .name = "openSource"};

/// The AudioClip played as the book opens. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_openClip{
    .key = "JournalController.openClip", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .name = "openClip"};

/// The AudioClip played as the book closes. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_closeClip{
    .key = "JournalController.closeClip", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .name = "closeClip"};

/// The pause menu's tabs, a JournalTab[]: the first is the tab StateChangeSync selects when the book
/// opens on the pause menu. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_pauseMenuTabs{
    .key = "JournalController.pauseMenuTabs", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "pauseMenuTabs"};

/// The tab's Button: StateChangeSync hands its GameObject to JournalRigController.SelectTab. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalTab_button{.key = "JournalTab.button", .kind = pm::sdk::Kind::Field,
                                                      .confidence = pm::sdk::Confidence::Decompiled,
                                                      .owner = "JournalTab", .name = "button"};

/// Turns the book to the evidence spread, on this client. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc JournalController_OpenEvidencePage{
    .key = "JournalController.OpenEvidencePage", .kind = pm::sdk::Kind::Method, .owner = "JournalController",
    .name = "OpenEvidencePage"};

/// The networked open or close of a player's book, an RPC every client runs: for a book that is not
/// this client's own it runs the rig's OpenCloseJournalNetworked and UpdateJournalTabs, selects the
/// pause menu's first tab when it opens on the pause menu, runs ShowCanvas, and plays the open or close
/// clip. Found for the fact that names ShowCanvas. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_StateChangeSync{
    .key = "JournalController.StateChangeSync", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalController", .name = "StateChangeSync",
    .params = Journal_StateChangeSync_params};

// StateChangeSync calls one void(bool, bool) of the controller: the canvas step (its look-alikes are not
// called from it).
inline constexpr pm::sdk::Fact JournalController_ShowCanvas_facts[] = {
    {.is = pm::sdk::Fact::Is::CalledBy, .method = "JournalController.StateChangeSync"}};

/// The canvas step of an open or close, ShowCanvas(journal, open, pause): the canvas and its contents
/// shown or hidden with the book; the GraphicRaycaster switched with them only for this client's own
/// journal; the journal's pages shown and the pause menu hidden, or the other way round on the pause
/// menu; and the recent players page hidden. A private void(bool, bool). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalController_ShowCanvas{
    .key = "JournalController.ShowCanvas", .kind = pm::sdk::Kind::Method, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalController", .type = "void", .params = Journal_bool_bool,
    .facts = JournalController_ShowCanvas_facts};

/// The rig that carries a player's book (Assembly-CSharp, global namespace), a MonoBehaviourPun beside
/// it: it holds the book in the body's hands, plays its animation and turns its tabs. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController{.key = "JournalRigController",
                                                         .confidence = pm::sdk::Confidence::Decompiled,
                                                         .name = "JournalRigController",
                                                         .assembly = "Assembly-CSharp", .ns = ""};

/// The book's Animator: the class's one Animator field. OpenCloseJournalNetworked sets its open bool.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_animator{
    .key = "JournalRigController.animator", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalRigController", .type = "Animator"};

/// The book's 3D tabs, a GameObject[]: OpenCloseJournalNetworked puts each on the open or closed layer.
/// Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_allTabsObjects{
    .key = "JournalRigController.allTabsObjects", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController", .name = "allTabsObjects"};

/// Turns the book's open state over to the rig, on every client: it copies JournalController.open into
/// JournalRigController.opened, puts the rig and its tabs on the open layer or on closedLayer, and sets
/// the Animator's open bool. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_OpenCloseJournalNetworked{
    .key = "JournalRigController.OpenCloseJournalNetworked", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController",
    .name = "OpenCloseJournalNetworked"};

// Of the rig's two bools, OpenCloseJournalNetworked stores this one (HideJournal stores the other).
inline constexpr pm::sdk::Fact JournalRigController_opened_facts[] = {
    {.is = pm::sdk::Fact::Is::Stores, .method = "JournalRigController.OpenCloseJournalNetworked"}};

/// Whether the rig holds the book open. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_opened{
    .key = "JournalRigController.opened", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalRigController", .type = "bool", .facts = JournalRigController_opened_facts};

/// The layer the rig and its tabs go back to as the book closes: the class's one LayerMask field, read as
/// a layer number (OpenCloseJournalNetworked hands it to GameObject.set_layer). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_closedLayer{
    .key = "JournalRigController.closedLayer", .kind = pm::sdk::Kind::Field,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController",
    .type = "LayerMask"};

/// Shows the tabs of the journal, or of the pause menu (its bool). Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_UpdateJournalTabs{
    .key = "JournalRigController.UpdateJournalTabs", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController", .name = "UpdateJournalTabs",
    .params = Journal_bool};

/// Selects the tab whose GameObject it is given. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_SelectTab{
    .key = "JournalRigController.SelectTab", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController", .name = "SelectTab",
    .params = Journal_GameObject};

/// Hides the book (true) or shows it, with its Animator's culling. Decompiled.
inline constexpr pm::sdk::EntryDesc JournalRigController_HideJournal{
    .key = "JournalRigController.HideJournal", .kind = pm::sdk::Kind::Method,
    .confidence = pm::sdk::Confidence::Decompiled, .owner = "JournalRigController", .name = "HideJournal",
    .params = Journal_bool};

/// The pause menu's list of the room's players (Assembly-CSharp, global namespace). Inferred: from the
/// class dump.
inline constexpr pm::sdk::EntryDesc PlayersList{.key = "PlayersList", .name = "PlayersList",
                                                .assembly = "Assembly-CSharp", .ns = ""};

/// Whether the list is this client's own, which it must be to fill: the class's one bool. Inferred:
/// from the class dump.
inline constexpr pm::sdk::EntryDesc PlayersList_isLocal{.key = "PlayersList.isLocal", .kind = pm::sdk::Kind::Field,
                                                        .owner = "PlayersList",
                                                        .type = "bool"};

/// Fills the list with the room's players. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc PlayersList_RefreshUI{.key = "PlayersList.RefreshUI", .kind = pm::sdk::Kind::Method,
                                                          .owner = "PlayersList", .name = "RefreshUI",
                                                          .params = Journal_bool};

/// The image a ThreeStateButton shows when ticked. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc ThreeStateButton_tickedImage{.key = "ThreeStateButton.tickedImage",
                                                                 .kind = pm::sdk::Kind::Field,
                                                                 .owner = "ThreeStateButton",
                                                                 .name = "tickedImage"};

/// The image a ThreeStateButton shows when crossed out. Inferred: from its name.
inline constexpr pm::sdk::EntryDesc ThreeStateButton_crossedOutImage{.key = "ThreeStateButton.crossedOutImage",
                                                                     .kind = pm::sdk::Kind::Field,
                                                                     .owner = "ThreeStateButton",
                                                                     .name = "crossedOutImage"};

/// The book's mesh object, Journal_geo under the PCPlayer prefab's journal: a sibling of the canvas that holds JournalController and
/// PauseMenuController, so HideJournal leaves those two active and the pause menu's Leave can start its coroutine. The field is
inline constexpr pm::sdk::EntryDesc JournalRigController_journalMeshObject{
    .key = "JournalRigController.journalMeshObject", .kind = pm::sdk::Kind::Field, .confidence = pm::sdk::Confidence::Decompiled,
    .owner = "JournalRigController", .name = "journalMeshObject", .type = "GameObject"};

}  // namespace pm::map

/// @file
/// @brief The game API for the journal (sdk/api/game_api.h).
///
/// Every member is called on Unity's main thread. One that did not resolve in the running game build logs "<key> is
/// not available in this game build" once and returns its type's default (0, null) without touching the game. A
/// reference it returns is valid for the current frame: keep it across frames only in a pm::api::Held.
namespace phasmo {
/// @addtogroup journal
/// @{

/// The sync controller of a journal's guess buttons. Unresolved: null and false.
struct JournalGhostSyncController {
  static constexpr pm::api::Type<pm::map::JournalGhostSyncController> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};

/// The sync controller of a journal's evidence rows. Unresolved: null and false.
struct JournalEvidenceSyncController {
  static constexpr pm::api::Type<pm::map::JournalEvidenceSyncController> type{};  ///< The type: klass(), is(obj), type_object(). Unresolved: null and false.
};

/// The journal. `journal` below is the JournalController.
struct JournalController {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::JournalController> type{};
  /// Awake(journal): hook it to see each journal as it wakes. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_Awake, void(void*)> Awake{};
  /// The pages, a GameObject[]: pages.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_pages, void*> pages{};
  /// The tabs, a JournalTab[]: journalTabs.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_journalTabs, void*> journalTabs{};
  /// The GhostCollection it lists: allGhostsData.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_allGhostsData, void*> allGhostsData{};
  /// The guess buttons, a GhostButton[]: allGhostTypeButtons.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_allGhostTypeButtons, void*> allGhostTypeButtons{};
  /// The guess buttons' sync controller: ghostSync.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_ghostSync, void*> ghostSync{};
  /// Every listed ghost, a List<Ghost>: allGhosts.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_allGhosts, void*> allGhosts{};
  /// Each listed ghost's button, a Dictionary<Ghost, GhostButton>: ghostButtons.get(journal).
  /// Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_ghostButtons, void*> ghostButtons{};
  /// RebuildGhosts(journal): adds to allGhosts and ghostButtons (empty them first). Unresolved: does
  /// nothing.
  static constexpr pm::api::Method<pm::map::JournalController_RebuildGhosts, void(void*)> RebuildGhosts{};
  /// SetGhostTypes(journal): the evidence filter. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_SetGhostTypes, void(void*)> SetGhostTypes{};
  /// The evidence buttons, an EvidenceButton[]: evidenceButtons.get(journal), .set(journal, array).
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::JournalController_evidenceButtons, void*> evidenceButtons{};
  /// The evidence buttons' sync controller: evidenceSync.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_evidenceSync, void*> evidenceSync{};
  /// AddEvidence(journal, evidence): marks it found and filters. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_AddEvidence, void(void*, int32_t)> AddEvidence{};
  /// FilterOutEvidence(journal, evidence): marks it ruled out and filters. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_FilterOutEvidence, void(void*, int32_t)>
      FilterOutEvidence{};
  /// RemoveEvidence(journal, evidence): unmarks it and filters. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_RemoveEvidence, void(void*, int32_t)> RemoveEvidence{};
  /// GoToPage(journal, page). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_GoToPage, void(void*, int32_t)> GoToPage{};
  /// PageSync(journal, page, &info), the page sync as the RPC runs it. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_PageSync, void(void*, int32_t, const void*)> PageSync{};
  /// The page the book is on. Unresolved: 0.
  static constexpr pm::api::Field<pm::map::JournalController_currentPage, int32_t> currentPage{};
  /// The journal's PhotonView. Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_photonView, void*> photonView{};
  /// GetGhostType(journal): the player's guess, a ghost type. Unresolved: 0.
  static constexpr pm::api::Method<pm::map::JournalController_GetGhostType, int32_t(void*)> GetGhostType{};
  /// UnselectGhost(journal): clears the player's guess. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_UnselectGhost, void(void*)> UnselectGhost{};
  /// Whether the journal is open: open.get(journal). Unresolved: false.
  static constexpr pm::api::Field<pm::map::JournalController_open, bool> open{};
  /// Its media page, a Journal_PhotoPage: photoPage.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_photoPage, void*> photoPage{};
  /// The pause menu's PlayersList: playersList.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_playersList, void*> playersList{};
  /// The JournalRigController that carries the book: rig.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_rig, void*> rig{};
  /// The pages' GraphicRaycaster, a Behaviour: canvasRaycaster.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_canvasRaycaster, void*> canvasRaycaster{};
  /// The book's AudioSource: openSource.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_openSource, void*> openSource{};
  /// The opening sound, an AudioClip: openClip.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_openClip, void*> openClip{};
  /// The closing sound, an AudioClip: closeClip.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_closeClip, void*> closeClip{};
  /// The pause menu's tabs, a JournalTab[]: pauseMenuTabs.get(journal). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalController_pauseMenuTabs, void*> pauseMenuTabs{};
  /// OpenCloseJournal(journal, pause): the player's own open or close of the book. A mod hooks it.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_OpenCloseJournal, void(void*, bool)> OpenCloseJournal{};
  /// OpenEvidencePage(journal): the book turns to the evidence spread. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_OpenEvidencePage, void(void*)> OpenEvidencePage{};
  /// ShowCanvas(journal, open, pause): the canvas step of an open or close (see its entry). Unresolved:
  /// does nothing.
  static constexpr pm::api::Method<pm::map::JournalController_ShowCanvas, void(void*, bool, bool)> ShowCanvas{};
};

/// The rig that carries a player's book. `rig` below is the JournalRigController.
struct JournalRigController {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::JournalRigController> type{};
  /// The book's Animator: animator.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalRigController_animator, void*> animator{};
  /// The book's 3D tabs, a GameObject[]: allTabsObjects.get(rig). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalRigController_allTabsObjects, void*> allTabsObjects{};
  /// Whether it holds the book open: opened.get(rig), .set(rig, open). Unresolved: false, writes nothing.
  static constexpr pm::api::Field<pm::map::JournalRigController_opened, bool> opened{};
  /// The layer number the tabs go back to as the book closes: closedLayer.get(rig). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::JournalRigController_closedLayer, int32_t> closedLayer{};
  /// UpdateJournalTabs(rig, pause): the pause menu's tabs or the journal's. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalRigController_UpdateJournalTabs, void(void*, bool)>
      UpdateJournalTabs{};
  /// SelectTab(rig, tabGameObject). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalRigController_SelectTab, void(void*, void*)> SelectTab{};
  /// HideJournal(rig, hidden). Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalRigController_HideJournal, void(void*, bool)> HideJournal{};
};

/// The pause menu's player list. `list` below is the PlayersList.
struct PlayersList {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::PlayersList> type{};
  /// Whether it is this client's own: isLocal.get(list), .set(list, own). Unresolved: false, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::PlayersList_isLocal, bool> isLocal{};
  /// RefreshUI(list, flag): fills it with the room's players. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::PlayersList_RefreshUI, void(void*, bool)> RefreshUI{};
};

/// The journal's media page. `page` below is one.
struct Journal_PhotoPage {
  /// The photo slots, a JournalMedia[]: photos.get(page). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Journal_PhotoPage_photos, void*> photos{};
  /// The video slots, a JournalMedia[]: videos.get(page). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Journal_PhotoPage_videos, void*> videos{};
  /// The sound slots, a JournalMedia[]: sounds.get(page). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Journal_PhotoPage_sounds, void*> sounds{};
};

/// A media slot. `slot` below is one.
struct JournalMedia {
  /// The Media it shows: media.get(slot). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalMedia_media, void*> media{};
  /// Whether it holds media: filled.get(slot). Unresolved: false.
  static constexpr pm::api::Field<pm::map::JournalMedia_filled, bool> filled{};
  /// RemoveMedia(slot, flag): the game's removal; call it with virtual_call, as the game does.
  /// Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::JournalMedia_RemoveMedia, void(void*, bool)> RemoveMedia{};
};

/// A piece of media. `media` below is one.
struct Media {
  /// What it shows, a MediaValues: value.get(media). Unresolved: null.
  static constexpr pm::api::Field<pm::map::Media_value, void*> value{};
};

/// A kind of media. `values` below is one.
struct MediaValues {
  /// Its subject, a MediaSubject number: evidenceType.get(values). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::MediaValues_evidenceType, int32_t> evidenceType{};
};

/// What a piece of media shows (MediaValues.evidenceType). Unresolved values are 0, which is emfSpot.
namespace MediaSubject {
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_emfSpot> emfSpot{};  ///< emfSpot(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_fingerprint> fingerprint{};  ///< fingerprint(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_footstep> footstep{};  ///< footstep(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghost> ghost{};  ///< ghost(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_dirtyWater> dirtyWater{};  ///< dirtyWater(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostWriting> ghostWriting{};  ///< ghostWriting(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_usedCrucifix> usedCrucifix{};  ///< usedCrucifix(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_dotsGhost> dotsGhost{};  ///< dotsGhost(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostOrb> ghostOrb{};  ///< ghostOrb(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_salt> salt{};  ///< salt(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_freezingTemperature> freezingTemperature{};  ///< freezingTemperature(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostSmoke> ghostSmoke{};  ///< ghostSmoke(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_camoGhost> camoGhost{};  ///< camoGhost(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_paranormalSound> paranormalSound{};  ///< paranormalSound(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_spiritBox> spiritBox{};  ///< spiritBox(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostHunt> ghostHunt{};  ///< ghostHunt(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_bansheeWail> bansheeWail{};  ///< bansheeWail(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_spiritBoxBreath> spiritBoxBreath{};  ///< spiritBoxBreath(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_motionSensor> motionSensor{};  ///< motionSensor(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_shadowGhost> shadowGhost{};  ///< shadowGhost(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostGroan> ghostGroan{};  ///< ghostGroan(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostLaugh> ghostLaugh{};  ///< ghostLaugh(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostTalk> ghostTalk{};  ///< ghostTalk(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_ghostWhisper> ghostWhisper{};  ///< ghostWhisper(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_obakeFingerprint> obakeFingerprint{};  ///< obakeFingerprint(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_burningChapelCrucifix> burningChapelCrucifix{};  ///< burningChapelCrucifix(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_obakeShapeshift> obakeShapeshift{};  ///< obakeShapeshift(): the value's number in this game build
inline constexpr pm::api::EnumValue<pm::map::MediaSubject_emf5> emf5{};  ///< emf5(): the value's number in this game build
}  // namespace MediaSubject

/// A tab of the book. `tab` below is the JournalTab.
struct JournalTab {
  /// Its first page: min.get(tab). Unresolved: 0.
  static constexpr pm::api::Field<pm::map::JournalTab_min, float> min{};
  /// Its last page: max.get(tab), max.set(tab, page). Unresolved: 0, writes nothing.
  static constexpr pm::api::Field<pm::map::JournalTab_max, float> max{};
  /// Its Button: button.get(tab). Unresolved: null.
  static constexpr pm::api::Field<pm::map::JournalTab_button, void*> button{};
};

/// A guess button. `button` below is the GhostButton.
struct GhostButton {
  /// The type: klass(), is(obj), type_object(), make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::GhostButton> type{};
  /// Its ghost type: type.get(button), type.set(button, ghostType). Unresolved: 0, writes nothing.
  static constexpr pm::api::Field<pm::map::GhostButton_type, int32_t> ghostType{};
};

/// An evidence button. `button` below is the EvidenceButton.
struct EvidenceButton {
  /// The type: klass(), is(obj), type_object(), make_array(n). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::EvidenceButton> type{};
  /// Its evidence number: evidence.get(button), evidence.set(button, value). Unresolved: 0, writes
  /// nothing.
  static constexpr pm::api::Field<pm::map::EvidenceButton_evidence, int32_t> evidence{};
  /// The line under its name, a TextMeshProUGUI: descriptionLabel.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::EvidenceButton_descriptionLabel, void*> descriptionLabel{};
};

/// A three-state journal button. `button` below is a ThreeStateButton (an EvidenceButton included).
struct ThreeStateButton {
  /// Its own text, a TextMeshProUGUI: textLabel.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ThreeStateButton_textLabel, void*> textLabel{};
  /// The image shown when ticked, a Behaviour: tickedImage.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ThreeStateButton_tickedImage, void*> tickedImage{};
  /// The image shown when crossed out, a Behaviour: crossedOutImage.get(button). Unresolved: null.
  static constexpr pm::api::Field<pm::map::ThreeStateButton_crossedOutImage, void*> crossedOutImage{};
};

/// A shared journal button. `button` below is the NetworkThreeStateButton.
struct NetworkThreeStateButton {
  /// The delegate raised on the player's change: userStateChanged.get(button), .set(button, delegate).
  /// Unresolved: null, writes nothing.
  static constexpr pm::api::Field<pm::map::NetworkThreeStateButton_userStateChanged, void*> userStateChanged{};
};

/// A text object's localiser. `localiser` below is the TextLocaliserUI.
struct TextLocaliserUI {
  /// The type: klass(), is(obj), type_object(). Unresolved: null and false.
  static constexpr pm::api::Type<pm::map::TextLocaliserUI> type{};
  /// Its key, a System.String: key.get(localiser). Unresolved: null.
  static constexpr pm::api::Field<pm::map::TextLocaliserUI_key, void*> key{};
  /// set_Key(localiser, string): sets the key and writes its text. Unresolved: does nothing.
  static constexpr pm::api::Method<pm::map::TextLocaliserUI_set_Key, void(void*, void*)> set_Key{};
};

/// @}
}  // namespace phasmo
