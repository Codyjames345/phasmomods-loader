#include "build_check.h"

#include <windows.h>

#include <fstream>

#include "log.h"
#include "text_files.h"

namespace pm::build {
namespace {
constexpr size_t kMaxRecordFiles = 1024;  // one per game build ever checked: far past what a package ships

/** The record in `file`, false when it cannot be read or is no record. */
bool read_record(const std::filesystem::path& file, Record& out) {
  std::ifstream in(file, std::ios::binary);
  if (!in) return false;
  std::string text(kMaxRecordBytes + 1, '\0');
  in.read(text.data(), static_cast<std::streamsize>(text.size()));
  text.resize(static_cast<size_t>(in.gcount()));
  return text.size() <= kMaxRecordBytes && parse_record(text, out);
}

Standing read_records(const std::filesystem::path& records, const Hashes& running, std::string_view mapDigest) {
  namespace fs = std::filesystem;
  Standing standing = Standing::Unseen;
  std::error_code error;
  size_t files = 0;
  for (fs::directory_iterator it(records, error), end; !error && it != end; it.increment(error)) {
    if (++files > kMaxRecordFiles) {
      PM_LOG("build: more than %zu files among the build records; the rest are not read", kMaxRecordFiles);
      break;
    }
    const fs::path& file = it->path();
    if (_wcsicmp(file.extension().c_str(), L".txt") != 0) continue;
    Record record;
    if (!read_record(file, record)) {
      PM_LOG("build: the build record %s cannot be read, so it counts for nothing", text::narrow(file.filename().wstring()).c_str());
      continue;
    }
    if (record.gameAssembly != running.gameAssembly || record.unityPlayer != running.unityPlayer) continue;
    if (record.map == mapDigest) return Standing::Known;
    standing = Standing::Stale;
  }
  if (error) PM_LOG("build: the folder of build records cannot be read to its end (Windows error %d)", error.value());
  return standing;
}
}  // namespace

std::string hash_module(void* module, const char* label) {
  wchar_t path[MAX_PATH];
  const DWORD n = GetModuleFileNameW(static_cast<HMODULE>(module), path, MAX_PATH);
  char hex[65] = {};
  if (n == 0 || n >= MAX_PATH || !sha256_file(path, hex)) {
    PM_LOG("build: could not hash %s", label);
    return {};
  }
  PM_LOG("build: %s sha256 %s", label, hex);
  return hex;
}

engine::Versions running_versions() {
  wchar_t path[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) return {};
  try {
    const std::filesystem::path exe(path);
    return engine::read(exe.parent_path() / (exe.stem().wstring() + L"_Data"));  // Unity's own name for the folder
  } catch (...) {
    return {};
  }
}

Standing judge(const std::filesystem::path& records, const Hashes& running, std::string_view mapDigest) {
  Standing standing = Standing::Unseen;
  if (running.gameAssembly.empty() || running.unityPlayer.empty()) {
    PM_LOG("build: a game library could not be hashed, so this counts as an unseen build");
    return standing;
  }
  try {
    standing = read_records(records, running, mapDigest);
  } catch (...) {
    PM_LOG("build: the build records could not be read, so this counts as an unseen build");
    return Standing::Unseen;
  }
  if (standing == Standing::Known) PM_LOG("build: the offline check has passed this game build");
  else if (standing == Standing::Stale) PM_LOG("build: this game build's record was made with another map, so it counts as an unseen build");
  else PM_LOG("build: the offline check has not seen this game build");
  return standing;
}

}  // namespace pm::build
