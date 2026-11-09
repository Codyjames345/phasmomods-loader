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
/** What the caller and the check's worker share, owned by both so neither outlives it. */
struct CheckJob {
  std::mutex mutex;
  std::condition_variable finished;
  bool done = false;
  Report report;
};

/** No update: start the installed version, telling the player `why`. */
Report carry_on(const std::string& why) { return {Outcome::CarryOn, pm::text::widen(why), ""}; }

std::string reason_of(const std::wstring& error) { return pm::text::narrow(error); }

/** The check's verdict. Nothing a backend throws may leave the worker, or the launcher would terminate. */
Report ask(Backend& backend) {
  try {
    const Check check = backend.check();
    if (check.kind == Check::Kind::Failed) return carry_on(pm::i18n::text("updater.check-failed", {{"reason", reason_of(check.error)}}));
    if (check.kind == Check::Kind::UpToDate) return {Outcome::UpToDate, pm::text::widen(pm::i18n::text("updater.latest")), ""};
    return {Outcome::Available, L"", check.version};
  } catch (const std::exception& error) {
    return carry_on(pm::i18n::text("updater.check-failed", {{"reason", error.what()}}));
  } catch (...) {
    return carry_on(pm::i18n::text("updater.check-failed", {{"reason", pm::i18n::text("updater.unknown-error")}}));
  }
}
}

Report check(std::shared_ptr<Backend> backend, std::chrono::milliseconds limit) {
  try {
    if (!backend->installed()) return {Outcome::NotInstalled, L"", ""};
  } catch (...) {
    return {Outcome::NotInstalled, L"", ""};
  }
  auto job = std::make_shared<CheckJob>();
  std::thread([job, backend] {
    Report report = ask(*backend);
    std::lock_guard<std::mutex> lock(job->mutex);
    job->report = std::move(report);
    job->done = true;
    job->finished.notify_all();
  }).detach();
  std::unique_lock<std::mutex> lock(job->mutex);
  if (!job->finished.wait_for(lock, limit, [&] { return job->done; })) return carry_on(pm::i18n::text("updater.checking-updates-took-too"));
  return job->report;
}

/** What an Install handle and its worker share, owned by both so neither outlives it. */
struct Install::Job {
  std::mutex mutex;
  std::string version;
  bool done = false;       // the result is settled: the worker's, a stall's or a cancel's
  bool abandoned = false;  // no one waits any more: never start the apply
  bool applying = false;   // the apply has started: nothing interrupts it
  int percent = 0;
  std::chrono::steady_clock::time_point moved;  // the last progress, or the start
  Report report;

  /** Settle the result, unless a stall or a cancel settled it first. */
  void finish(Report result) {
    std::lock_guard<std::mutex> lock(mutex);
    if (done) return;
    report = std::move(result);
    done = true;
  }
};

namespace {
void install_steps(const std::shared_ptr<Install::Job>& job, Backend& backend, const std::vector<std::wstring>& arguments) {
  const std::string version = job->version;
  std::wstring error;
  const auto progress = [job](int percent) {
    std::lock_guard<std::mutex> lock(job->mutex);
    job->percent = percent < 0 ? 0 : percent > 100 ? 100 : percent;
    job->moved = std::chrono::steady_clock::now();
  };
  if (!backend.download(version, progress, error)) {
    job->finish(carry_on(pm::i18n::text("updater.download-failed", {{"version", version}, {"reason", reason_of(error)}})));
    return;
  }
  {
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->abandoned) return;
    job->applying = true;
  }
  if (!backend.apply(arguments, error)) {
    job->finish(carry_on(pm::i18n::text("updater.install-failed", {{"version", version}, {"reason", reason_of(error)}})));
    return;
  }
  job->finish({Outcome::Restarting, pm::text::widen(pm::i18n::text("updater.updated-restarting", {{"version", version}})), ""});
}
}

Install Install::start(std::shared_ptr<Backend> backend, std::string version, std::vector<std::wstring> arguments) {
  auto job = std::make_shared<Job>();
  job->version = std::move(version);
  job->moved = std::chrono::steady_clock::now();
  std::thread([job, backend, arguments = std::move(arguments)] {
    // Nothing a backend throws may leave this thread, or the launcher would terminate.
    try {
      install_steps(job, *backend, arguments);
    } catch (const std::exception& error) {
      job->finish(carry_on(pm::i18n::text("updater.failed", {{"reason", error.what()}})));
    } catch (...) {
      job->finish(carry_on(pm::i18n::text("updater.failed", {{"reason", pm::i18n::text("updater.unknown-error")}})));
    }
  }).detach();
  return Install(job);
}

Install::Status Install::poll(std::chrono::steady_clock::time_point now, std::chrono::milliseconds stall) {
  std::lock_guard<std::mutex> lock(job_->mutex);
  if (!job_->done && !job_->applying && now - job_->moved > stall) {
    job_->abandoned = job_->done = true;
    job_->report = carry_on(pm::i18n::text("updater.download-stalled", {{"version", job_->version}}));
  }
  return {job_->done, job_->percent, job_->done ? job_->report : Report{}};
}

bool Install::cancel() {
  std::lock_guard<std::mutex> lock(job_->mutex);
  if (job_->applying) return false;
  job_->abandoned = true;
  if (!job_->done) {
    job_->report = {Outcome::CarryOn, L"", ""};
    job_->done = true;
  }
  return true;
}

bool should_offer(const std::string& version, const std::string& ignored) { return version != ignored; }

std::string read_ignored_version(const std::filesystem::path& file) {
  std::ifstream input(file);
  std::string version;
  if (!input || !(input >> version)) return {};
  return version;
}

bool save_ignored_version(const std::filesystem::path& file, const std::string& version) {
  if (file.empty()) return false;
  std::error_code error;
  std::filesystem::create_directories(file.parent_path(), error);
  std::ofstream output(file, std::ios::binary | std::ios::trunc);
  output << version << "\n";
  output.flush();
  return output.good();
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
