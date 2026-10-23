#include <windows.h>
#include <shellapi.h>
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <thread>
#include "check.h"
#include "update.h"
#include "launcher.h"

namespace {
using namespace pg::update;
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

  bool installed() override { return isInstalled; }
  std::string current() override { return version; }
  Check check() override {
    ++checks;
    std::this_thread::sleep_for(checkDelay);
    return result;
  }
  bool download(const std::string& v, std::wstring& error) override {
    ++downloads;
    downloaded = v;
    if (!downloadOk) error = downloadError;
    return downloadOk;
  }
  bool apply_and_restart(const std::vector<std::wstring>& arguments, std::wstring& error) override {
    ++applies;
    applied = arguments;
    if (!applyOk) error = applyError;
    return applyOk;
  }
};

Check available(const char* version) { return {Check::Kind::Available, version, L""}; }
const std::vector<std::wstring> kArguments{L"--steam", L"C:\\Game\\Phasmophobia.exe", L"-arg"};

void not_installed_skips_quietly() {
  auto fake = std::make_shared<FakeBackend>();
  fake->isInstalled = false;
  const auto report = run(fake, kArguments, 1s);
  CHECK(report.outcome == Outcome::NotInstalled);
  CHECK(report.message.empty());
  CHECK(fake->checks == 0);
}

void same_version_is_up_to_date() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = {Check::Kind::UpToDate, "", L""};
  CHECK(run(fake, kArguments, 1s).outcome == Outcome::UpToDate);
  CHECK(fake->downloads == 0);
  CHECK(fake->applies == 0);
}

void newer_version_restarts() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  const auto report = run(fake, kArguments, 1s);
  CHECK(report.outcome == Outcome::Restarting);
  CHECK(report.message.find(L"0.2.0") != std::wstring::npos);
  CHECK(fake->downloads == 1);
  CHECK(fake->downloaded == "0.2.0");
  CHECK(fake->applies == 1);
  CHECK(fake->applied == kArguments);
}

void older_version_also_applies() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.0.9");
  CHECK(run(fake, kArguments, 1s).outcome == Outcome::Restarting);
  CHECK(fake->downloaded == "0.0.9");
  CHECK(fake->applies == 1);
}

void check_failure_carries_on() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = {Check::Kind::Failed, "", L"offline"};
  const auto report = run(fake, kArguments, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"offline") != std::wstring::npos);
  CHECK(fake->downloads == 0);
}

void download_failure_keeps_installed() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  fake->downloadOk = false;
  const auto report = run(fake, kArguments, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"corrupt") != std::wstring::npos);
  CHECK(fake->applies == 0);
}

void apply_refused_carries_on() {
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  fake->applyOk = false;
  const auto report = run(fake, kArguments, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"busy") != std::wstring::npos);
}

void slow_check_is_abandoned() {
  // The abandoned worker still holds the backend after run returns and this scope ends.
  auto fake = std::make_shared<FakeBackend>();
  fake->result = available("0.2.0");
  fake->checkDelay = 300ms;
  const auto start = std::chrono::steady_clock::now();
  const auto report = run(fake, kArguments, 50ms);
  const auto took = std::chrono::steady_clock::now() - start;
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(!report.message.empty());
  CHECK(took < 200ms);
  std::weak_ptr<FakeBackend> watch = fake;
  std::this_thread::sleep_for(600ms);
  CHECK(fake->checks == 1);
  CHECK(fake->downloads == 0);
  CHECK(fake->applies == 0);
  fake.reset();
  std::this_thread::sleep_for(100ms);
  CHECK(watch.expired());  // the worker let go of it once it finished
}

struct ThrowingBackend : FakeBackend {
  bool throwInCheck = false;
  bool throwOther = false;
  Check check() override {
    if (throwInCheck) throw std::runtime_error("native failure");
    return FakeBackend::check();
  }
  bool download(const std::string&, std::wstring&) override {
    if (throwOther) throw 7;
    throw std::runtime_error("disk full");
  }
};

void backend_exceptions_carry_on() {
  auto thrownInCheck = std::make_shared<ThrowingBackend>();
  thrownInCheck->throwInCheck = true;
  auto report = run(thrownInCheck, kArguments, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"native failure") != std::wstring::npos);

  auto thrownInDownload = std::make_shared<ThrowingBackend>();
  thrownInDownload->result = available("0.2.0");
  report = run(thrownInDownload, kArguments, 1s);
  CHECK(report.outcome == Outcome::CarryOn);
  CHECK(report.message.find(L"disk full") != std::wstring::npos);
  CHECK(thrownInDownload->applies == 0);

  auto thrownOther = std::make_shared<ThrowingBackend>();
  thrownOther->result = available("0.2.0");
  thrownOther->throwOther = true;
  CHECK(run(thrownOther, kArguments, 1s).outcome == Outcome::CarryOn);
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

  const std::wstring line = L"\"PhasmoMods Loader.exe\" " + pg::launcher::command_line(restart);
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
  not_installed_skips_quietly();
  same_version_is_up_to_date();
  newer_version_restarts();
  older_version_also_applies();
  check_failure_carries_on();
  download_failure_keeps_installed();
  apply_refused_carries_on();
  slow_check_is_abandoned();
  backend_exceptions_carry_on();
  restart_arguments_round_trip();
  return g_failures ? 1 : 0;
}
