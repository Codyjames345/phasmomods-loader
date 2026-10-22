#pragma once

#include <windows.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

// Small file and text helpers shared by the host and the launcher, for the suite's UTF-8 text
// files (core/mod_list.h).
namespace pg::text {

/** A whole file's bytes; empty when it cannot be read. */
inline std::string read_file(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::ostringstream text;
  text << in.rdbuf();
  return text.str();
}

/** UTF-8 as a Windows string. */
inline std::wstring widen(const std::string& utf8) {
  if (utf8.empty()) return {};
  const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  std::wstring out(n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), out.data(), n);
  return out;
}

/** A Windows string as UTF-8. */
inline std::string narrow(const std::wstring& wide) {
  if (wide.empty()) return {};
  const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
  std::string out(n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), out.data(), n, nullptr, nullptr);
  return out;
}

}  // namespace pg::text
