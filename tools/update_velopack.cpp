#include "update.h"
#include "text_table.h"
#include "text_files.h"
#include "Velopack.hpp"
#include <windows.h>
#include <exception>
#include <filesystem>
#include <optional>
#include <utility>

namespace pm::update {
namespace {
constexpr char kRepository[] = "https://github.com/Codyjames345/phasmomods-loader";

/** Why a Velopack call failed: its own message, as Velopack's wrapper throws it. */
std::wstring reason(const std::exception& error) {
  const std::wstring text = text::widen(error.what());
  return text.empty() ? pm::text::widen(pm::i18n::text("updater.no-reason")) : text;
}

/** The folder `PM_UPDATE_SOURCE` names, when it names one that exists; empty otherwise. */
std::filesystem::path local_source() {
  std::wstring value(32768, L'\0');
  const DWORD length = GetEnvironmentVariableW(L"PM_UPDATE_SOURCE", value.data(), static_cast<DWORD>(value.size()));
  if (!length || length >= value.size()) return {};
  value.resize(length);
  std::error_code error;
  return std::filesystem::is_directory(value, error) ? std::filesystem::path(value) : std::filesystem::path();
}

/**
 * The Backend over Velopack's UpdateManager. Every Velopack call throws on failure, and every
 * method catches it here, so nothing thrown leaves this file.
 */
class VelopackBackend : public Backend {
 public:
  explicit VelopackBackend(Feed feed) : feed_(std::move(feed)) {}

  /** Constructing the manager fails when Setup.exe did not install this program (a dev build). */
  bool installed() override {
    try {
      Velopack::UpdateOptions options{};
      options.AllowVersionDowngrade = true;  // a taken-down release moves back, and so does leaving the development builds
      options.MaximumDeltasBeforeFallback = 10;  // Velopack's default
      options.ExplicitChannel = feed_.channel;
      const std::filesystem::path folder = local_source();
      if (!folder.empty())
        manager_.emplace(std::make_unique<Velopack::FileSource>(text::narrow(folder.wstring())), &options);
      else
        manager_.emplace(std::make_unique<Velopack::GithubSource>(kRepository, "", feed_.prerelease), &options);
      return true;
    } catch (...) {
      manager_.reset();
      return false;
    }
  }

  std::string current() override {
    try {
      return manager_ ? manager_->GetCurrentVersion() : std::string();
    } catch (...) {
      return {};
    }
  }

  Check check() override {
    Check result;
    try {
      if (!manager_) return {Check::Kind::Failed, {}, pm::text::widen(pm::i18n::text("updater.not-installed"))};
      update_ = manager_->CheckForUpdates();
      if (!update_) return result;
      result.kind = Check::Kind::Available;
      result.version = update_->TargetFullRelease.Version;
    } catch (const std::exception& error) {
      return {Check::Kind::Failed, {}, reason(error)};
    } catch (...) {
      return {Check::Kind::Failed, {}, pm::text::widen(pm::i18n::text("updater.unknown-error"))};
    }
    return result;
  }

  bool download(const std::string& version, std::wstring& error) override {
    try {
      if (!manager_ || !update_ || update_->TargetFullRelease.Version != version) {
        error = pm::text::widen(pm::i18n::text("updater.not-found"));
        return false;
      }
      manager_->DownloadUpdates(*update_);
      return true;
    } catch (const std::exception& failure) {
      error = reason(failure);
    } catch (...) {
      error = pm::text::widen(pm::i18n::text("updater.unknown-error"));
    }
    return false;
  }

  /** Starts Velopack's updater, which waits for this program to exit, applies, and restarts it when asked. */
  bool apply(const std::vector<std::wstring>& arguments, bool restart, std::wstring& error) override {
    try {
      if (!manager_ || !update_) {
        error = pm::text::widen(pm::i18n::text("updater.not-downloaded"));
        return false;
      }
      std::vector<std::string> restartArguments;
      for (const std::wstring& argument : arguments) restartArguments.push_back(text::narrow(argument));
      manager_->WaitExitThenApplyUpdates(*update_, false, restart, restartArguments);
      return true;
    } catch (const std::exception& failure) {
      error = reason(failure);
    } catch (...) {
      error = pm::text::widen(pm::i18n::text("updater.unknown-error"));
    }
    return false;
  }

 private:
  Feed feed_;
  std::optional<Velopack::UpdateManager> manager_;
  std::optional<Velopack::UpdateInfo> update_;  // what the last check found
};
}

std::shared_ptr<Backend> velopack_backend(const Feed& feed) { return std::make_shared<VelopackBackend>(feed); }
}
