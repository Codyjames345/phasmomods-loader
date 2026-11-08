#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include "check.h"
#include "text_fixture.h"
#include "update.h"
#include "launcher.h"

namespace {
using namespace pm::update;
using namespace std::chrono_literals;

struct FakeBackend : Backend {
  bool isInstalled = true;
  std::string version = "0.1.0";
  Check result;
  bool downloadOk = true, applyOk = true;
  std::wstring downloadError = L"corrupt", applyError = L"busy";
  std::chrono::milliseconds checkDelay{0};
  std::atomic<int> checks{0}, downloads{0}, applies{0};
  std::string downloaded;
  std::vector<std::wstring> applied;
  // Runs inside download and apply, on the worker: a test reports progress or holds the step here.
  std::function<void(const std::function<void(int)>&)> duringDownload;
  std::function<void()> duringApply;

  bool installed() override { return isInstalled; }
  std::string current() override { return version; }
  Check check() override {
    ++checks;
    std::this_thread::sleep_for(checkDelay);
    return result;
  }
  bool download(const std::string& v, const std::function<void(int)>& progress, std::wstring& error) override {
    ++downloads;
    downloaded = v;
    if (duringDownload) duringDownload(progress);
    if (!downloadOk) error = downloadError;
    return downloadOk;
  }
  bool apply(const std::vector<std::wstring>& arguments, std::wstring& error) override {
    if (duringApply) duringApply();
    ++applies;
    applied = arguments;
    if (!applyOk) error = applyError;
    return applyOk;
  }
};

/** A gate a test opens to let the fake's download or apply go on. */
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool open = false;
  void wait() {
    std::unique_lock<std::mutex> lock(mutex);
    changed.wait(lock, [&] { return open; });
  }
  void release() {
    std::lock_guard<std::mutex> lock(mutex);
    open = true;
    changed.notify_all();
  }
};

/** Poll `install` as the launcher's dialog does, until it finishes, with a stall far longer than any test step. */
Install::Status finish(Install& install, std::chrono::milliseconds stall = 10s) {
  for (;;) {
    const Install::Status status = install.poll(std::chrono::steady_clock::now(), stall);
    if (status.finished) return status;
    std::this_thread::sleep_for(5ms);
  }
}

/** Poll until `done` holds, for at most `limit`. */
template <class Done>
bool wait_until(Done done, std::chrono::milliseconds limit = 2s) {
  const auto until = std::chrono::steady_clock::now() + limit;
  while (!done()) {
    if (std::chrono::steady_clock::now() > until) return false;
    std::this_thread::sleep_for(2ms);
  }
  return true;
}

Check available(const char* version) { return {Check::Kind::Available, version, L""}; }
const std::vector<std::wstring> kArguments{L"--steam", L"C:\\Game\\Phasmophobia.exe", L"-arg"};

// The checkbox moves an install between two feeds. The release's is Velopack's default channel with GitHub's pre-releases left out,
// so a player who never ticks it sees nothing different; a development build's is its own channel, and reads pre-releases.
void each_choice_reads_its_own_feed() {
  const Feed release = feed_for(false);
  CHECK(release.channel == "win" && !release.prerelease);
  const Feed development = feed_for(true);
  CHECK(development.channel == "dev" && development.prerelease);
}

// With no choice saved, an install follows what it is: a release stays on releases, and a copy installed from a development
// Setup.exe stays on development. A saved choice wins either way, which is how unticking comes back from a development build.
void the_choice_defaults_to_what_is_installed() {
  CHECK(!wants_development(std::nullopt, "0.2.0"));
  CHECK(wants_development(std::nullopt, "0.3.0-dev.12"));
  CHECK(wants_development(true, "0.2.0"));
  CHECK(!wants_development(false, "0.3.0-dev.12"));
  CHECK(is_development_version("0.3.0-dev.12") && !is_development_version("0.3.0") && !is_development_version(""));
}

void the_choice_is_saved_and_read_back() {
  const std::filesystem::path folder =
      std::filesystem::temp_directory_path() / ("pm-channel-test-" + std::to_string(GetCurrentProcessId()));
  std::error_code ignored;
  std::filesystem::remove_all(folder, ignored);
  const std::filesystem::path file = folder / "nested" / "update_channel.txt";
  CHECK(!read_development_choice(file));  // nothing saved yet
  CHECK(save_development_choice(file, true));  // makes its folder
  CHECK(read_development_choice(file) == true);
  CHECK(save_development_choice(file, false));
  CHECK(read_development_choice(file) == false);
  std::ofstream(file) << "something else";
  CHECK(!read_development_choice(file));  // a file it can't read is no choice
  CHECK(!save_development_choice(std::filesystem::path(), true));  // no folder to save in: the checkbox says so
  std::filesystem::remove_all(folder, ignored);
}

void not_installed_skips_quietly() {
  auto fake = std::make_shared<FakeBackend>();
  fake->isInstalled = false;
  const auto report = check(fake, 1s);
  CHECK(report.outcome == Outcome::NotInstalled);
  CHECK(report.message.empty());
  CHECK(fake->checks == 0);
}

void same_version_is_up_to_date() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = {Check::Kind::UpToDate, "", L""};
  CHECK(check(fake, 1s).outcome == Outcome::UpToDate);
  CHECK(fake->downloads == 0);
}

// The check only says what is on offer: nothing downloads until the player chooses to update.
void another_version_is_offered_without_downloading() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  const auto report = check(fake, 1s);
  CHECK(report.outcome == Outcome::Available);
  CHECK(report.version == "0.2.0");
  CHECK(report.message.empty());
  CHECK(fake->downloads == 0);
  CHECK(fake->applies == 0);

  fake->result = available("0.0.9");  // a taken-down release moves back
  CHECK(check(fake, 1s).version == "0.0.9");
}

void check_failure_carries_on() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = {Check::Kind::Failed, "", L"offline"};
  const auto report = check(fake, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"offline") != std::wstring::npos);
  CHECK(fake->downloads == 0);
}

void slow_check_is_abandoned() {
  // The abandoned worker still holds the backend after check returns and this scope ends.
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  fake->checkDelay = 300ms;
  const auto start = std::chrono::steady_clock::now();
  const auto report = check(fake, 50ms);
  const auto took = std::chrono::steady_clock::now() - start;
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(!report.message.empty());
  CHECK(took < 200ms);
  std::weak_ptr<FakeBackend> watch = fake;
  std::this_thread::sleep_for(600ms);
  CHECK(fake->checks == 1);
  CHECK(fake->downloads == 0);
  fake.reset();
  std::this_thread::sleep_for(100ms);
  CHECK(watch.expired());  // the worker let go of it once it finished
}

void installing_restarts() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  CHECK(check(fake, 1s).outcome == Outcome::Available);
  Install install = Install::start(fake, "0.2.0", kArguments);
  const auto status = finish(install);
  CHECK(status.report.outcome == Outcome::Restarting);
  CHECK(status.report.message.find(L"0.2.0") != std::wstring::npos);
  CHECK(fake->downloads == 1);
  CHECK(fake->downloaded == "0.2.0");
  CHECK(fake->applies == 1);
  CHECK(fake->applied == kArguments);
}

void the_download_reports_its_progress() {
  auto fake = std::make_shared<FakeBackend>();
  Gate halfway;
  std::atomic<bool> atHalf{false};
  fake->duringDownload = [&](const std::function<void(int)>& progress) {
    progress(50);
    atHalf = true;
    halfway.wait();
    progress(100);
  };
  Install install = Install::start(fake, "0.2.0", kArguments);
  CHECK(wait_until([&] { return atHalf.load(); }));
  const auto status = install.poll(std::chrono::steady_clock::now(), 10s);
  CHECK(!status.finished);
  CHECK(status.percent == 50);
  halfway.release();
  CHECK(finish(install).report.outcome == Outcome::Restarting);
}

// A slow connection takes longer than the stall limit in all, but keeps reporting progress, so it finishes.
void a_slow_download_that_keeps_moving_finishes() {
  auto fake = std::make_shared<FakeBackend>();
  fake->duringDownload = [](const std::function<void(int)>& progress) {
    for (int percent = 0; percent <= 100; percent += 10) {
      std::this_thread::sleep_for(30ms);
      progress(percent);
    }
  };
  const auto start = std::chrono::steady_clock::now();
  Install install = Install::start(fake, "0.2.0", kArguments);
  const auto status = finish(install, 150ms);
  CHECK(std::chrono::steady_clock::now() - start > 300ms);  // twice the stall limit in all
  CHECK(status.report.outcome == Outcome::Restarting);
  CHECK(fake->applies == 1);
}

// A download that stops moving gives up, says why, and the worker never applies it when it ends after all.
void a_stalled_download_gives_up() {
  auto fake = std::make_shared<FakeBackend>();
  Gate stuck;
  std::atomic<bool> started{false};
  fake->duringDownload = [&](const std::function<void(int)>& progress) {
    progress(20);
    started = true;
    stuck.wait();
    progress(100);
  };
  Install install = Install::start(fake, "0.2.0", kArguments);
  CHECK(wait_until([&] { return started.load(); }));
  CHECK(!install.poll(std::chrono::steady_clock::now(), 30s).finished);  // moving a moment ago
  const auto status = install.poll(std::chrono::steady_clock::now() + 31s, 30s);
  CHECK(status.finished);
  CHECK(status.report.outcome == Outcome::CarryOn);
  CHECK(status.report.message.find(L"0.2.0") != std::wstring::npos);
  // Asked again, it stays given up.
  CHECK(install.poll(std::chrono::steady_clock::now(), 30s).report.outcome == Outcome::CarryOn);
  stuck.release();
  std::this_thread::sleep_for(100ms);
  CHECK(fake->downloads == 1);
  CHECK(fake->applies == 0);
}

// No progress at all counts from the start: a connection that never answers stalls too.
void a_download_that_never_starts_stalls() {
  auto fake = std::make_shared<FakeBackend>();
  Gate stuck;
  fake->duringDownload = [&](const std::function<void(int)>&) { stuck.wait(); };
  Install install = Install::start(fake, "0.2.0", kArguments);
  CHECK(install.poll(std::chrono::steady_clock::now() + 31s, 30s).report.outcome == Outcome::CarryOn);
  stuck.release();
  std::this_thread::sleep_for(100ms);
  CHECK(fake->applies == 0);
}

void cancelling_the_download_never_applies_it() {
  auto fake = std::make_shared<FakeBackend>();
  Gate stuck;
  std::atomic<bool> started{false};
  fake->duringDownload = [&](const std::function<void(int)>&) {
    started = true;
    stuck.wait();
  };
  Install install = Install::start(fake, "0.2.0", kArguments);
  CHECK(wait_until([&] { return started.load(); }));
  CHECK(install.cancel());
  const auto status = install.poll(std::chrono::steady_clock::now(), 30s);
  CHECK(status.finished);
  CHECK(status.report.outcome == Outcome::CarryOn);
  stuck.release();
  std::this_thread::sleep_for(100ms);
  CHECK(fake->applies == 0);
}

// Once the apply has started it runs to its end, whatever the player clicks or the clock says.
void an_apply_under_way_is_not_interrupted() {
  auto fake = std::make_shared<FakeBackend>();
  Gate applying;
  std::atomic<bool> started{false};
  fake->duringApply = [&] {
    started = true;
    applying.wait();
  };
  Install install = Install::start(fake, "0.2.0", kArguments);
  CHECK(wait_until([&] { return started.load(); }));
  CHECK(!install.cancel());
  CHECK(!install.poll(std::chrono::steady_clock::now() + 1h, 30s).finished);
  applying.release();
  CHECK(finish(install).report.outcome == Outcome::Restarting);
  CHECK(fake->applies == 1);
}

void download_failure_keeps_installed() {
  auto fake = std::make_shared<FakeBackend>();
  fake->downloadOk = false;
  Install install = Install::start(fake, "0.2.0", kArguments);
  const auto status = finish(install);
  CHECK(status.report.outcome == Outcome::CarryOn);
  CHECK(status.report.message.find(L"corrupt") != std::wstring::npos);
  CHECK(fake->applies == 0);
}

void apply_refused_carries_on() {
  auto fake = std::make_shared<FakeBackend>();
  fake->applyOk = false;
  Install install = Install::start(fake, "0.2.0", kArguments);
  const auto status = finish(install);
  CHECK(status.report.outcome == Outcome::CarryOn);
  CHECK(status.report.message.find(L"busy") != std::wstring::npos);
}

struct ThrowingBackend : FakeBackend {
  bool throwInCheck = false;
  bool throwOther = false;
  Check check() override {
    if (throwInCheck) throw std::runtime_error("native failure");
    return FakeBackend::check();
  }
  bool download(const std::string&, const std::function<void(int)>&, std::wstring&) override {
    if (throwOther) throw 7;
    throw std::runtime_error("disk full");
  }
};

void backend_exceptions_carry_on() {
  auto thrownInCheck = std::make_shared<ThrowingBackend>();
  thrownInCheck->throwInCheck = true;
  auto report = check(thrownInCheck, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"native failure") != std::wstring::npos);

  auto thrownInDownload = std::make_shared<ThrowingBackend>();
  Install install = Install::start(thrownInDownload, "0.2.0", kArguments);
  auto status = finish(install);
  CHECK(status.report.outcome == Outcome::CarryOn);
  CHECK(status.report.message.find(L"disk full") != std::wstring::npos);
  CHECK(thrownInDownload->applies == 0);

  auto thrownOther = std::make_shared<ThrowingBackend>();
  thrownOther->throwOther = true;
  Install other = Install::start(thrownOther, "0.2.0", kArguments);
  CHECK(finish(other).report.outcome == Outcome::CarryOn);
}

// Ignore skips that one version: any other, newer or older, is offered again.
void only_the_ignored_version_is_skipped() {
  CHECK(!should_offer("0.3.0", "0.3.0"));
  CHECK(should_offer("0.3.1", "0.3.0"));
  CHECK(should_offer("0.2.9", "0.3.0"));
  CHECK(should_offer("0.3.0", ""));
}

void the_ignored_version_is_saved_and_read_back() {
  const std::filesystem::path folder =
      std::filesystem::temp_directory_path() / ("pm-ignored-test-" + std::to_string(GetCurrentProcessId()));
  std::error_code ignored;
  std::filesystem::remove_all(folder, ignored);
  const std::filesystem::path file = folder / "nested" / "update_ignored.txt";
  CHECK(read_ignored_version(file).empty());  // nothing saved yet
  CHECK(save_ignored_version(file, "0.3.0-dev.30"));  // makes its folder
  CHECK(read_ignored_version(file) == "0.3.0-dev.30");
  CHECK(save_ignored_version(file, "0.3.0"));
  CHECK(read_ignored_version(file) == "0.3.0");
  CHECK(!save_ignored_version(std::filesystem::path(), "0.3.0"));
  std::filesystem::remove_all(folder, ignored);
}

void restart_arguments_round_trip() {
  const std::vector<std::wstring> argv{
    L"PhasmoMods Loader.exe", L"--steam",
    L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\Phasmophobia\\Phasmophobia.exe",
    L"-screen-fullscreen 0"};
  const auto restart = restart_arguments(argv);
  CHECK(restart == std::vector<std::wstring>(argv.begin() + 1, argv.end()));
  CHECK(restart_arguments({L"PhasmoMods Loader.exe"}).empty());
  CHECK(restart_arguments({}).empty());

  const std::wstring line = L"\"PhasmoMods Loader.exe\" " + pm::launcher::command_line(restart);
  int count = 0;
  wchar_t** parsed = CommandLineToArgvW(line.c_str(), &count);
  CHECK(parsed != nullptr);
  if (!parsed) return;
  CHECK(count == 4);
  for (int i = 0; i < count && i < 4; ++i) CHECK(std::wstring(parsed[i]) == argv[i]);
  LocalFree(parsed);
}
}

int main() {
  use_text({"i18n/default.json"});
  each_choice_reads_its_own_feed();
  the_choice_defaults_to_what_is_installed();
  the_choice_is_saved_and_read_back();
  not_installed_skips_quietly();
  same_version_is_up_to_date();
  another_version_is_offered_without_downloading();
  check_failure_carries_on();
  slow_check_is_abandoned();
  installing_restarts();
  the_download_reports_its_progress();
  a_slow_download_that_keeps_moving_finishes();
  a_stalled_download_gives_up();
  a_download_that_never_starts_stalls();
  cancelling_the_download_never_applies_it();
  an_apply_under_way_is_not_interrupted();
  download_failure_keeps_installed();
  apply_refused_carries_on();
  backend_exceptions_carry_on();
  only_the_ignored_version_is_skipped();
  the_ignored_version_is_saved_and_read_back();
  restart_arguments_round_trip();
  return g_failures ? 1 : 0;
}
