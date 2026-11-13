#pragma once

/// @file
/// @brief Content packs: mod folders of data alone made for your mod, which your mod reads in its own format.
///
/// A content pack follows SMAPI's: a folder with a manifest.json whose "contentPackFor" names your mod
/// (`{"id": "<your mod id>", "minimumVersion": "1.2.0"}`), an `assets` folder and an `i18n/default.json`, and no DLL. The
/// loader lists, copies, matches across the lobby and starts packs as it does mods: a pack is enabled in the Mods panel,
/// needs your mod (a required dependency, so it stays off without it), and every player in a lobby has the same packs or
/// is removed. What a pack holds is your mod's business: the Emotes mod reads an `emotes.json` catalogue and a bundle.
///
/// @code
/// bool start() {  // from pmmod_start
///   for (const pm::api::packs::Pack& p : pm::api::packs::owned())
///     if (!load_my_pack(p.folder)) pm::api::packs::failed(p.id, "its catalogue cannot be read");
///   return true;
/// }
/// @endcode
///
/// Main thread. Local only: nothing is sent; every player in the lobby has the same packs.
/// @ingroup runtime
/// @since Contract 2.6

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace pm::api::packs {

/// A content pack made for the calling mod.
struct Pack {
  std::string id;       ///< its mod id, "<author>.<name>"
  std::string name;     ///< its manifest's name, as the Mods panel shows it
  std::string version;  ///< its manifest's version, MAJOR.MINOR.PATCH
  std::filesystem::path folder;  ///< the copy of its `assets` folder for this game process: read only
  std::filesystem::path text;    ///< the copy of its `i18n/default.json` (core/text_table.h's format); may not exist
};

/// The content packs made for the calling mod that start with it, in id order. Ask from pmmod_start: packs start just
/// after the mod they are for, and are listed there already.
/// @return Empty before bind, on a host older than Contract 2.6, and when there are none.
std::vector<Pack> owned();

/// From pmmod_start: the pack `packId` (one of owned()) cannot be used. It does not start, the Mods panel shows `why`, and
/// so it is not in the lobby's list of mods. Logged and ignored outside pmmod_start, before bind, or on an older host.
/// @param packId The pack's id.
/// @param why A sentence for the player, from your string table.
void failed(std::string_view packId, std::string_view why);

}  // namespace pm::api::packs
