#pragma once
#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace pg::update {
/** What asking the update source found. */
struct Check {
  enum class Kind { UpToDate, Available, Failed } kind = Kind::UpToDate;
  std::string version;  // the version on offer, when Available
  std::wstring error;   // why the check failed, when Failed
};

/**
 * The update source and installer. The Velopack backend implements it; tests fake it.
 * `run` calls it from a worker thread that can outlive `run`, which is why `run` co-owns it.
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
  virtual bool download(const std::string& version, std::wstring& error) = 0;
  /** Apply the downloaded version and start the new launcher with `arguments`. */
  virtual bool apply_and_restart(const std::vector<std::wstring>& arguments, std::wstring& error) = 0;
};

enum class Outcome {
  NotInstalled,  // not installed by Setup.exe: no update was tried
  UpToDate,
  Restarting,    // the new version was applied and is starting
  CarryOn,       // no update happened: start the installed version
};

struct Report {
  Outcome outcome = Outcome::CarryOn;
  std::wstring message;  // for the log or the user; empty for NotInstalled
};

/**
 * Check, download and apply on a worker thread, waiting at most `limit` for the check and
 * download. When `limit` passes first, run returns CarryOn and the worker is told it was
 * abandoned: it starts neither a download nor an apply afterwards. The worker shares ownership
 * of `backend`, so it stays valid however long an abandoned call takes. `arguments` are copied.
 * Once an apply has started, run waits for it without a limit. Velopack's apply only starts its
 * updater, which waits for this program to exit, and returns, so that wait is short.
 */
Report run(std::shared_ptr<Backend> backend, const std::vector<std::wstring>& arguments,
           std::chrono::milliseconds limit);

/** The launcher's own arguments after argv[0], unchanged, for the restarted launcher. */
std::vector<std::wstring> restart_arguments(const std::vector<std::wstring>& launcherArguments);

/**
 * The Velopack backend (tools/update_velopack.cpp). It updates from the folder that the
 * environment variable PG_UPDATE_SOURCE names when that folder exists, and from the project's
 * GitHub releases otherwise.
 */
std::shared_ptr<Backend> velopack_backend();
}
