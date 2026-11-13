#pragma once

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include "log.h"
#include "text_files.h"

// The one-time move into a mod's data folder (pm::mod::data_folder, core/mod_glue.h): the files a mod kept in the folder
// it built for itself before, %LOCALAPPDATA%\PhasmoMods\<mod id>\, go into %LOCALAPPDATA%\PhasmoMods\data\<mod id>\.
// Plain file logic, tested offline (tests/mod_glue_test.cpp). Main thread.
namespace pm::datafolder {

// What is left of an old folder whose files the data folder already had: renamed to this, so the move runs once and a
// file kept there never comes back in.
inline constexpr wchar_t kMovedSuffix[] = L".moved";

namespace detail {
// A path within a folder, as the log shows it.
inline std::string shown(const std::filesystem::path& relative) { return text::narrow(relative.generic_wstring()); }

// Whether `inner` is `outer` or lies inside it, comparing the paths as written, made absolute and normal.
inline bool within(const std::filesystem::path& inner, const std::filesystem::path& outer) {
  std::error_code ec;
  const std::filesystem::path a = std::filesystem::absolute(inner, ec).lexically_normal();
  const std::filesystem::path b = std::filesystem::absolute(outer, ec).lexically_normal();
  const std::filesystem::path rest = a.lexically_relative(b);
  return !rest.empty() && *rest.begin() != "..";
}
}  // namespace detail

// Moves into `folder` each file of `old` that `folder` lacks, keeping sub-folders: renamed where it can be, else copied
// and the original removed only once the copy is whole. Nothing is ever deleted that was not copied, and nothing in
// `folder` is replaced. A file `folder` already has stays in `old` (logged at debug, with one line for them all). One
// that can be neither moved nor copied stays too, logged as a warning, and is tried again on the next run. Then the
// folders of `old` left empty go, `old` itself included; an `old` that keeps only files `folder` already has is renamed
// `<old>.moved`, so the move runs once. Does nothing, logged, when either folder holds the other: the loader's own
// folders are never moved. Logs through PM_LOG, so a mod's lines carry its name.
inline void move_old_files(const std::filesystem::path& old, const std::filesystem::path& folder) {
  namespace fs = std::filesystem;
  using detail::shown;
  if (detail::within(folder, old) || detail::within(old, folder)) {
    PM_LOG_AT(log::Level::Warn, "data folder: the old folder and the data folder hold each other; nothing moves");
    return;
  }
  std::error_code ec;
  std::vector<fs::path> files, folders;
  bool failed = false;  // a file stays in `old` that the data folder lacks: try again next run
  fs::recursive_directory_iterator it(old, ec), end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code kind;
    if (it->is_directory(kind)) folders.push_back(it->path());
    else if (it->is_regular_file(kind)) files.push_back(it->path());
  }
  if (ec) {
    PM_LOG_AT(log::Level::Warn, "data folder: could not read the whole old folder; what was not read stays there");
    failed = true;
  }
  int kept = 0;
  for (const fs::path& from : files) {
    const fs::path relative = from.lexically_relative(old);
    const fs::path to = folder / relative;
    std::error_code looked;
    const bool there = fs::exists(to, looked);
    if (looked) {
      PM_LOG_AT(log::Level::Warn, "data folder: could not tell whether the data folder has %s; it stays in the old folder",
                shown(relative).c_str());
      failed = true;
      continue;
    }
    if (there) {
      PM_LOG_AT(log::Level::Debug, "data folder: kept %s in the old folder: the data folder has its own", shown(relative).c_str());
      ++kept;
      continue;
    }
    fs::create_directories(to.parent_path(), ec);
    std::error_code moved;
    fs::rename(from, to, moved);
    if (moved) {
      std::error_code copied;
      fs::copy_file(from, to, copied);
      if (copied) {
        // A part of a copy goes; a file that was there already (the copy's own refusal) is the data folder's and stays.
        if (copied != std::errc::file_exists) fs::remove(to, ec);
        PM_LOG_AT(log::Level::Warn, "data folder: could not move %s from the old folder; it stays there", shown(relative).c_str());
        failed = true;
        continue;
      }
      if (!fs::remove(from, ec)) {
        PM_LOG("data folder: copied %s from the old folder, which keeps it too", shown(relative).c_str());
        ++kept;
        continue;
      }
    }
    PM_LOG("data folder: moved %s from the old folder", shown(relative).c_str());
  }
  // The deepest first, so a folder's sub-folders go before it; a folder that still holds anything stays.
  for (auto f = folders.rbegin(); f != folders.rend(); ++f)
    if (fs::is_empty(*f, ec)) fs::remove(*f, ec);
  if (fs::is_empty(old, ec)) {
    fs::remove(old, ec);
    return;
  }
  if (failed || !kept) return;
  fs::path aside = old;
  aside += kMovedSuffix;
  std::error_code renamed;
  if (fs::exists(aside, ec)) renamed = std::make_error_code(std::errc::file_exists);
  else fs::rename(old, aside, renamed);
  if (renamed) {
    PM_LOG_AT(log::Level::Debug, "data folder: the old folder keeps %d file(s) the data folder already has", kept);
    return;
  }
  PM_LOG("data folder: kept %d file(s) the data folder already has in %s", kept, shown(aside.filename()).c_str());
}

}  // namespace pm::datafolder
