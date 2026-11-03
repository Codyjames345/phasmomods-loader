#pragma once

// The custom difficulty side of content (core/mod_api.h, version 2.2), following
// sdk/systems/difficulty.md:
//
// - while content is active, the lobby's difficulty menu offers the custom ghosts in the Ghost Type
//   option of Custom 1 to 3: they join the menu's GhostCollection (host/content_types.h, offer), and
//   the option's count follows the collection, Random and then every ghost in it;
// - the pick each custom preset forces is learnt from the option's change handler (the player's arrows
//   and dice) and kept by ghost id, not by number, in a text file ("<preset>=<ghost id>" lines), so a
//   new mod's ghost that shifts the numbers leaves every pick on its ghost;
// - while content is active, every difficulty the lobby holds at Random for a custom preset with a
//   pick gets that pick back: the presets, the lobby's own difficulty, its settled copy and the
//   difficulty the next contract plays (LevelValues.difficulty), the lobby's own first, since a preset
//   may be that very object; the host then sends the lobby's difficulty to every player, and again
//   once a player who joined is verified (they got Random);
// - a custom type never reaches a save or a lobby sync except the host's own sends while content is
//   active and everyone in the room now is verified: EasySave3 writes every Difficulty through one type
//   (ES3UserType_Difficulty), which writes Random for it otherwise. So the game's own sync and its send to
//   a joiner carry Random during a joiner's grace, and what the host sees does not change: it keeps offering the
//   ghosts, its pick comes back the next frame, and the pick goes out once everyone is verified;
// - while content is not active (off, or pending: the host not heard from yet, or this client's mods not
//   matching it), every custom type in the lobby the menu was found in goes back to
//   Random, the menu offers the game's ghosts only and the host sends Random; each pick comes back once
//   content is active again;
// - a contract without custom ghosts (content_types.h decides it) never forces one;
// - the custom difficulty's summary names a custom ghost where Enum.ToString would print its number.
//
// It reaches the game only through map entries (the game API bound to the host's own contract).
// Main thread.

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "content_registry.h"
#include "content_types.h"
#include "held.h"

namespace pm::host {

/// Which custom ghost each custom preset forces, by ghost id. Pure, so it is tested offline.
class DifficultyPicks {
 public:
  /// The ghost id `preset` forces, or "" for none.
  const std::string& of(int32_t preset) const;
  /// The player chose the ghost `id` ("" for any other choice: Random or a game ghost) for `preset`.
  /// True when what is kept changed.
  bool choose(int32_t preset, const std::string& id);
  /// One "<preset>=<ghost id>" line each, in preset order.
  std::string text() const;
  /// Picks from text(). A line keyed by a number (an older version kept the type's number, which no
  /// longer names one ghost) is left out and counted in `legacy`; any other line that holds no content id is
  /// ignored. A bundled mod's old content id reads as its new one, counted in `migrated`.
  static DifficultyPicks parse(std::string_view text, int& legacy, int* migrated = nullptr);
  bool operator==(const DifficultyPicks&) const = default;

 private:
  std::map<int32_t, std::string> picks_;
};

class ContentDifficulty {
 public:
  /// `roomVerified` says whether everyone in the room now is verified, asked as a send is written: a custom type goes only to
  /// them. Null: never.
  ContentDifficulty(content::Registry& registry, ContentTypes& types, bool (*roomVerified)())
      : registry_(registry), types_(types), roomVerified_(roomVerified) {}

  /// The map entries this needs that did not resolve, ", "-separated; empty when every one did. Until
  /// they all resolve the host withholds custom content. Main thread, after the game API is bound.
  std::string missing() const;
  /// Creates its hooks (not enabled), for this instance; one instance hooks at a time. False when one
  /// could not be made (the game API logs which).
  bool create_hooks();
  /// Reads the picks kept in `file` (custom_difficulty.txt in the host's data folder). The file is
  /// written only when a pick changes; a file of the older form is rewritten then (logged).
  void load(std::wstring file);

  /// Every frame, after the ghost types' tick (which starts and ends the lobby's offer): in a contract,
  /// keeps one without custom ghosts from forcing one; in the lobby, finds the menu (inactive too, once
  /// a second until found), and on `state` offers the custom ghosts and gives picks back (active) or
  /// withdraws them (otherwise) as above. `master`: this client is the room's master, whose lobby
  /// difficulty is the lobby's. `nowMs`: a steady clock.
  void tick(content::State state, bool privateRoom, bool master, uint64_t nowMs);
  /// Withdraws everything from the lobby, as when content turns off, before the mods stop.
  void withdraw(bool master);

  // What each hook does; each may be called on its own.

  /// DifficultyUIManager.GhostTypeChosen, after: the player's choice for the lobby difficulty's preset
  /// is kept (a custom preset only, and only while content has ghosts), not when the host itself
  /// showed a choice.
  void after_choice(void* menu);
  /// ES3UserType_Difficulty.WriteScriptableObject, before: a custom type in `d` becomes Random for the
  /// write unless the host is sending it while content is active; returns the type to put back, or -1.
  int32_t before_write(void* d) const;
  /// After the write: puts back what before_write changed.
  void after_write(void* d, int32_t type) const;
  /// DifficultyUIManager.SummaryText, around: the summary it builds names a custom ghost, whichever
  /// method asked for it (the summary panel's toggle, or the display's refresh).
  void enter_summary() { ++summarising_; }
  void leave_summary() { --summarising_; }
  /// Enum.ToString, instead of the game's: inside the summary, the name of the custom ghost type
  /// `boxed` holds, in the game's language (else English); none otherwise.
  std::optional<std::string> enum_name(void* boxed) const;

  /// The picks as kept (for tests and logs).
  const DifficultyPicks& picks() const { return picks_; }

 private:
  bool ours(int32_t type) const;  // a type this client's content numbers
  bool agreed_room() const;       // active, and everyone in the room now is verified
  bool custom_preset(void* menu, int32_t preset) const;
  void* find_menu(uint64_t nowMs);
  void* menu_list(void* menu) const;
  void fit_option(void* menu);
  bool give_back(void* menu, bool master);
  void set_aside(void* menu, bool master);
  void send(void* menu);
  void show_pick(void* menu);
  void withdraw_menu(void* menu, bool master);
  void keep_contract_vanilla();
  void save();

  content::Registry& registry_;
  ContentTypes& types_;
  DifficultyPicks picks_;
  std::wstring file_;
  int legacy_ = 0;               // lines of the older form read from the file, dropped at the next write
  content::State state_ = content::State::Off;
  pm::api::Held menu_;           // the lobby's DifficultyUIManager
  pm::api::Held offeredTo_;      // the menu that offers the custom ghosts
  uint64_t nextSearch_ = 0;
  bool sentActive_ = false;      // the lobby difficulty was sent since content turned active or everyone was last verified
  bool sending_ = false;         // the host's own send is being written
  bool showing_ = false;         // the host is running the handler to show a choice
  int summarising_ = 0;          // depth of the summary's builder
  bool (*roomVerified_)();       // everyone in the room now is verified
  bool vanillaLogged_ = false;   // that a contract without custom ghosts was kept from one was logged
  bool saveLogged_ = false;      // that the picks could not be kept was logged
};

}  // namespace pm::host
