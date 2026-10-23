#pragma once
#include <chrono>
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
 * `run` calls it from a worker thread that can outlive `run`, so a Backend must outlive
 * that worker: give it static or process lifetime, never a scope that `run`'s caller leaves.
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
 * abandoned: it starts neither a download nor an apply afterwards. The worker keeps using `backend`, which
 * therefore must outlive it (see Backend). `arguments` are copied.
 */
Report run(Backend& backend, const std::vector<std::wstring>& arguments, std::chrono::milliseconds limit);

/** The launcher's own arguments after argv[0], unchanged, for the restarted launcher. */
std::vector<std::wstring> restart_arguments(const std::vector<std::wstring>& launcherArguments);
}
