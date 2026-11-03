#include "update.h"
#include "text_files.h"
#include "text_table.h"
#include <condition_variable>
#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <thread>

namespace pm::update {
namespace {
/** What the caller and the worker share, owned by both so neither outlives it. */
struct Job {
  std::mutex mutex;
  std::condition_variable finished;
  bool done = false;
  bool abandoned = false;  // the caller stopped waiting: never start an apply
  bool applying = false;   // an apply has started: the caller waits for it
  Report report;
};

std::wstring widen(const std::string& text) { return std::wstring(text.begin(), text.end()); }

void finish(Job& job, Outcome outcome, std::wstring message) {
  std::lock_guard<std::mutex> lock(job.mutex);
  job.report = {outcome, std::move(message)};
  job.done = true;
  job.finished.notify_all();
}

void work_steps(const std::shared_ptr<Job>& job, Backend* backend, const std::vector<std::wstring>& arguments,
                const Progress& progress) {
  const Check check = backend->check();
  if (check.kind == Check::Kind::Failed) {
    finish(*job, Outcome::CarryOn,
           pm::text::widen(pm::i18n::text("updater.check-failed", {{"reason", pm::text::narrow(check.error)}})));
    return;
  }
  if (check.kind == Check::Kind::UpToDate) {
    finish(*job, Outcome::UpToDate, L"");
    return;
  }
  {
    // Under the lock, so the caller hears of the update only while it still waits for it.
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->abandoned) return;
    if (progress.found) progress.found(check.version);
  }
  std::wstring error;
  if (!backend->download(check.version, error)) {
    finish(*job, Outcome::CarryOn,
           pm::text::widen(pm::i18n::text("updater.download-failed", {{"version", check.version}, {"reason", pm::text::narrow(error)}})));
    return;
  }
  {
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->abandoned) return;
    job->applying = true;
  }
  const bool restart = progress.restart ? progress.restart() : true;
  if (!backend->apply(arguments, restart, error)) {
    finish(*job, Outcome::CarryOn,
           pm::text::widen(pm::i18n::text("updater.install-failed", {{"version", check.version}, {"reason", pm::text::narrow(error)}})));
    return;
  }
  finish(*job, Outcome::Restarting,
         restart ? pm::text::widen(pm::i18n::text("updater.updated-restarting", {{"version", check.version}}))
                 : pm::text::widen(pm::i18n::text("updater.updated-on-close", {{"version", check.version}})));
}

/** The worker body: nothing a backend throws may leave this thread, or the launcher would terminate. */
void work(const std::shared_ptr<Job>& job, Backend& backend, const std::vector<std::wstring>& arguments,
          const Progress& progress) {
  try {
    work_steps(job, &backend, arguments, progress);
  } catch (const std::exception& error) {
    finish(*job, Outcome::CarryOn, pm::text::widen(pm::i18n::text("updater.failed", {{"reason", error.what()}})));
  } catch (...) {
    finish(*job, Outcome::CarryOn, pm::text::widen(pm::i18n::text("updater.failed", {{"reason", pm::i18n::text("updater.unknown-error")}})));
  }
}
}

Report run(std::shared_ptr<Backend> backend, const std::vector<std::wstring>& arguments,
           std::chrono::milliseconds limit, Progress progress) {
  try {
    if (!backend->installed()) return {Outcome::NotInstalled, L""};
  } catch (...) {
    return {Outcome::NotInstalled, L""};
  }
  auto job = std::make_shared<Job>();
  std::thread([job, backend, arguments, progress = std::move(progress)] {
    work(job, *backend, arguments, progress);
  }).detach();

  std::unique_lock<std::mutex> lock(job->mutex);
  job->finished.wait_for(lock, limit, [&] { return job->done; });
  if (!job->done && !job->applying) {
    job->abandoned = true;
    return {Outcome::CarryOn, pm::text::widen(pm::i18n::text("updater.checking-updates-took-too"))};
  }
  // An apply that has started is not interrupted: it either restarts the program or reports why not.
  job->finished.wait(lock, [&] { return job->done; });
  return job->report;
}

Feed feed_for(bool development) {
  // "win" is Velopack's default channel on Windows, which every release is packed for.
  return development ? Feed{"dev", true} : Feed{"win", false};
}

bool is_development_version(const std::string& version) { return version.find("-dev.") != std::string::npos; }

bool wants_development(std::optional<bool> saved, const std::string& current) {
  return saved ? *saved : is_development_version(current);
}

std::optional<bool> read_development_choice(const std::filesystem::path& file) {
  std::ifstream input(file);
  std::string word;
  if (!input || !(input >> word)) return std::nullopt;
  if (word == "development") return true;
  if (word == "release") return false;
  return std::nullopt;
}

bool save_development_choice(const std::filesystem::path& file, bool development) {
  if (file.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  output << (development ? "development" : "release") << "\n";
  output.flush();
  return output.good();
}

std::vector<std::wstring> restart_arguments(const std::vector<std::wstring>& launcherArguments) {
  if (launcherArguments.empty()) return {};
  return {launcherArguments.begin() + 1, launcherArguments.end()};
}
}
