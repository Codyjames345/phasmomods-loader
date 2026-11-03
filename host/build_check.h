#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "build_record.h"
#include "engine_version.h"
#include "file_hash.h"

// Which game build this is, and whether the offline check has passed it. The answer never stops the host: a
// mod runs when its required entries resolve on whatever build this is (host/game_map.h). It decides only
// what the consent notice says and how often it asks (host/consent_text.h). Only an engine or metadata version the host's
// layouts were not written for stops it (core/engine_version.h).
namespace pm::build {

enum class Standing {
  Known,   // a record names both hashes and was made with this map
  Stale,   // a record names both hashes, but was made with another map: a new or changed entry was not checked here
  Unseen,  // no record names both hashes, or the hashes or the records could not be read
};

/** The complete on-disk hash of a loaded module, logged under `label`; empty, logged, when it cannot be read. */
std::string hash_module(void* module, const char* label);

/**
 * The running game's engine and metadata versions, from the data folder beside its executable. Touches no runtime, so it
 * can run before anything relies on a layout. What cannot be read counts as unknown, which core/engine_version.h refuses.
 */
engine::Versions running_versions();

/**
 * What the records in the folder `records` (core/build_record.h) say of `running`, with `mapDigest` the digest of
 * this host's map, logged. Whatever cannot be read counts for nothing: a hash not computed, a folder missing, a
 * record malformed or larger than a record can be. Nothing thrown leaves it, and it reads a bounded number of files.
 */
Standing judge(const std::filesystem::path& records, const Hashes& running, std::string_view mapDigest);

/** Whether the notice treats this build as checked. A stale record counts as none. */
inline bool checked(Standing s) { return s == Standing::Known; }
}  // namespace pm::build
