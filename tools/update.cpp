#include "update.h"
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace pg::update {
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

void work(const std::shared_ptr<Job>& job, Backend* backend, const std::vector<std::wstring>& arguments) {
  const Check check = backend->check();
  if (check.kind == Check::Kind::Failed) {
    finish(*job, Outcome::CarryOn,
           L"Could not check for updates (" + check.error + L"). Starting the installed version.");
    return;
  }
  if (check.kind == Check::Kind::UpToDate) {
    finish(*job, Outcome::UpToDate, L"");
    return;
  }
  {
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->abandoned) return;
  }
  std::wstring error;
  if (!backend->download(check.version, error)) {
    finish(*job, Outcome::CarryOn,
           L"Could not download version " + widen(check.version) + L" (" + error +
             L"). Starting the installed version.");
    return;
  }
  {
    std::lock_guard<std::mutex> lock(job->mutex);
    if (job->abandoned) return;
    job->applying = true;
  }
  if (!backend->apply_and_restart(arguments, error)) {
    finish(*job, Outcome::CarryOn,
           L"Could not install version " + widen(check.version) + L" (" + error +
             L"). Starting the installed version.");
    return;
  }
  finish(*job, Outcome::Restarting, L"Updated to " + widen(check.version) + L". Restarting.");
}
}

Report run(Backend& backend, const std::vector<std::wstring>& arguments, std::chrono::milliseconds limit) {
  if (!backend.installed()) return {Outcome::NotInstalled, L""};
  auto job = std::make_shared<Job>();
  Backend* const target = &backend;
  std::thread([job, target, arguments] { work(job, target, arguments); }).detach();

  std::unique_lock<std::mutex> lock(job->mutex);
  job->finished.wait_for(lock, limit, [&] { return job->done; });
  if (!job->done && !job->applying) {
    job->abandoned = true;
    return {Outcome::CarryOn, L"Checking for updates took too long. Starting the installed version."};
  }
  // An apply that has started is not interrupted: it either restarts the program or reports why not.
  job->finished.wait(lock, [&] { return job->done; });
  return job->report;
}

std::vector<std::wstring> restart_arguments(const std::vector<std::wstring>& launcherArguments) {
  if (launcherArguments.empty()) return {};
  return {launcherArguments.begin() + 1, launcherArguments.end()};
}
}
