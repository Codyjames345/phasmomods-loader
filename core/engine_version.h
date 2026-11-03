#pragma once

#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

// Which engine and metadata a game build runs on, read from its files. The host's object layouts (host/il2cpp.h, the
// MethodInfo copy in host/il2cpp.cpp) are compiled in for one Unity branch and one metadata version, and code built for
// another layout can crash the game past any catch. So these two are a fixed list where everything else is read from the
// game: a build on anything else, or whose versions cannot be read, is refused, by the host (it stays inactive and the game
// plays vanilla) and by the offline check (no record is written). A new Unity patch on the same branch and metadata version
// keeps the layouts and passes. Both files are only read, and only their first bytes.
namespace pm::engine {

inline constexpr std::string_view kEngineBranch = "2022.3.";  // a version on this branch starts so
inline constexpr int32_t kMetadataVersion = 31;
inline constexpr uint32_t kMetadataMagic = 0xFAB11BAF;  // pm:not-game-data: IL2CPP's own magic number
inline constexpr uint32_t kFirstLongHeader = 22;        // the serialized-file format from which the header below holds
inline constexpr size_t kLongHeaderBytes = 48;          // where such a file's Unity version begins
inline constexpr size_t kMaxVersionBytes = 32;

struct Versions {
  std::string engine;    // "2022.3.40f1"; empty when it cannot be read
  int32_t metadata = -1;  // -1 when it cannot be read
};

/** Whether the compiled-in layouts were written for these. An unread value is no known one. */
inline bool known(const Versions& v) { return v.engine.starts_with(kEngineBranch) && v.metadata == kMetadataVersion; }

/** Says what was found against what the layouts were written for. */
inline std::string refusal(const Versions& v) {
  return "this loader's layouts were written for Unity " + std::string(kEngineBranch) + "x and metadata version " +
         std::to_string(kMetadataVersion) + ", and this game is on Unity " + (v.engine.empty() ? "(unknown)" : v.engine) +
         " and metadata version " + (v.metadata < 0 ? "(unknown)" : std::to_string(v.metadata)) +
         ", so nothing is hooked and the game plays without mods";
}

/** The Unity version in the header of a serialized file (`globalgamemanagers`); empty when it does not read as one. */
inline std::string engine_in(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::array<char, kLongHeaderBytes + kMaxVersionBytes> head{};
  in.read(head.data(), static_cast<std::streamsize>(head.size()));
  if (static_cast<size_t>(in.gcount()) <= kLongHeaderBytes) return {};
  const auto byte = [&](size_t at) { return static_cast<uint32_t>(static_cast<unsigned char>(head[at])); };
  // The format's number is big-endian, at +8.
  if ((byte(8) << 24 | byte(9) << 16 | byte(10) << 8 | byte(11)) < kFirstLongHeader) return {};
  std::string version;
  for (size_t at = kLongHeaderBytes; at < static_cast<size_t>(in.gcount()); ++at) {
    const char c = head[at];
    if (c == '\0') return version;
    const bool fits = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.';
    if (!fits) return {};
    version += c;
  }
  return {};  // no end within what a version can be
}

/** The version in the header of `global-metadata.dat`; -1 when it does not read as one. */
inline int32_t metadata_in(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary);
  std::array<unsigned char, 8> head{};
  in.read(reinterpret_cast<char*>(head.data()), static_cast<std::streamsize>(head.size()));
  if (static_cast<size_t>(in.gcount()) != head.size()) return -1;
  const auto word = [&](size_t at) {
    return static_cast<uint32_t>(head[at]) | static_cast<uint32_t>(head[at + 1]) << 8 |
           static_cast<uint32_t>(head[at + 2]) << 16 | static_cast<uint32_t>(head[at + 3]) << 24;
  };
  if (word(0) != kMetadataMagic) return -1;
  const uint32_t version = word(4);
  return version <= 0xFFFF ? static_cast<int32_t>(version) : -1;  // pm:not-game-data: a bound on a version number
}

/** Both versions from a game's data folder (`Phasmophobia_Data`). Never throws: what cannot be read counts as unknown. */
inline Versions read(const std::filesystem::path& data) noexcept {
  Versions v;
  try {
    v.engine = engine_in(data / L"globalgamemanagers");
    v.metadata = metadata_in(data / L"il2cpp_data" / L"Metadata" / L"global-metadata.dat");
  } catch (...) {
    return Versions{};
  }
  return v;
}

}  // namespace pm::engine
