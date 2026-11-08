#pragma once
#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pm::update {
/** What asking the update source found. */
struct Check {
  enum class Kind { UpToDate, Available, Failed } kind = Kind::UpToDate;
  std::string version;  // the version on offer, when Available
  std::wstring error;   // why the check failed, when Failed
};

/**
 * The update source and installer. The Velopack backend implements it; tests fake it.
 * `check` and `Install` call it from worker threads that can outlive them, which is why they co-own it.
 * Any method may throw (the Velopack wrapper does on every native error); the layer catches it
 * and carries on with the installed version.
 */
class Backend {
 public:
  virtual ~Backend() = default;
  /** True when this program was installed by Setup.exe and can update itself. */
  virtual bool installed() = 0;
  /** The installed version. */
  virtual std::string current() = 0;
  virtual Check check() = 0;
  /** Download what the last check found. `progress` hears 0 to 100 as it goes, on the calling thread. */
  virtual bool download(const std::string& version, const std::function<void(int percent)>& progress, std::wstring& error) = 0;
  /** Apply the downloaded version once this program exits, then start the new launcher with `arguments`. */
  virtual bool apply(const std::vector<std::wstring>& arguments, std::wstring& error) = 0;
};

enum class Outcome {
  NotInstalled,  // not installed by Setup.exe: no update was tried
  UpToDate,
  Restarting,    // the new version is applied once this program exits, which it must do now
  CarryOn,       // no update happened: start the installed version
  Available,     // the check found a version to install; nothing is downloaded yet
};

struct Report {
  Outcome outcome = Outcome::CarryOn;
  std::wstring message;  // for the log or the user; empty for NotInstalled, UpToDate and Available
  std::string version;   // the version on offer, when Available
};

/**
 * Ask the update source for another version on a worker thread, waiting at most `limit`. The check
 * only reads the release feed, so a short limit suits every connection. When `limit` passes first,
 * the result is CarryOn and the worker, which shares ownership of `backend`, ends on its own.
 * Available leaves `backend` ready to download that version with `Install::start`.
 */
Report check(std::shared_ptr<Backend> backend, std::chrono::milliseconds limit);

/**
 * A download and apply running on a worker thread. It has no overall time limit, so a slow
 * connection finishes; it gives up only when the download fails or stops making progress.
 * Once the apply has started it is never interrupted: Velopack's apply only starts its updater,
 * which waits for this program to exit, and returns, so it is short.
 */
class Install {
 public:
  struct Status {
    bool finished = false;
    int percent = 0;  // the download's progress, 0 to 100
    Report report;    // when finished: Restarting, or CarryOn with why
  };
  /** Start downloading `version`, which the last `check` on `backend` found, then apply it to restart with `arguments`. */
  static Install start(std::shared_ptr<Backend> backend, std::string version, std::vector<std::wstring> arguments);
  /**
   * Where the install is at `now`. When the download has reported no progress for longer than `stall`
   * (counted from the start until its first report), it is abandoned: the result is CarryOn, and the
   * worker finishes the download in the background but never applies it.
   */
  Status poll(std::chrono::steady_clock::time_point now, std::chrono::milliseconds stall);
  /** Stop waiting, so the download is never applied. False once the apply has started: keep polling for its result. */
  bool cancel();

  struct Job;  // what the handle and its worker share (tools/update.cpp)

 private:
  explicit Install(std::shared_ptr<Job> job) : job_(std::move(job)) {}
  std::shared_ptr<Job> job_;
};

/** Whether to offer `version` to the player: every version but the one they chose to ignore. */
bool should_offer(const std::string& version, const std::string& ignored);
/** The version saved in `file` as ignored; empty when none is saved or it can't be read. */
std::string read_ignored_version(const std::filesystem::path& file);
/** Saves `version` as ignored in `file`, making its folder. False when it could not be saved. */
bool save_ignored_version(const std::filesystem::path& file, const std::string& version);

/** The launcher's own arguments after argv[0], unchanged, for the restarted launcher. */
std::vector<std::wstring> restart_arguments(const std::vector<std::wstring>& launcherArguments);

/** Which of the project's builds an install takes (docs/releasing.md). */
struct Feed {
  std::string channel;  // Velopack's channel
  bool prerelease;      // whether GitHub's pre-releases are read
};

/** The development builds' feed, or the releases'. */
Feed feed_for(bool development);
/** Whether `version` is a development build's, like 0.3.0-dev.12. */
bool is_development_version(const std::string& version);
/** Whether an install takes development builds: as `saved`, and with no choice saved, as what is installed now. */
bool wants_development(std::optional<bool> saved, const std::string& current);
/** The choice saved in `file`; none when it is missing or can't be read. */
std::optional<bool> read_development_choice(const std::filesystem::path& file);
/** Saves the choice in `file`, making its folder. False when it could not be saved. */
bool save_development_choice(const std::filesystem::path& file, bool development);

/**
 * The Velopack backend (tools/update_velopack.cpp) for `feed`. It updates from the folder that the
 * environment variable PM_UPDATE_SOURCE names when that folder exists, and from the project's
 * GitHub releases otherwise.
 */
std::shared_ptr<Backend> velopack_backend(const Feed& feed);
}
