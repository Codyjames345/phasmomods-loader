#include <windows.h>

#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
int reads = 0;
bool failSecondRead = false;

/** Inject a disk read error after real file data has reached the hash. */
BOOL WINAPI read_file(HANDLE file, void* buffer, DWORD size, DWORD* count, OVERLAPPED* overlapped) {
  if (failSecondRead && ++reads == 2) {
    *count = 0;
    SetLastError(ERROR_READ_FAULT);
    return FALSE;
  }
  return ReadFile(file, buffer, size, count, overlapped);
}
}  // namespace

// Compile the production hash loop with only the disk-error boundary replaced.
#define ReadFile read_file
#include "../host/build_check.cpp"
#undef ReadFile

namespace pm::log {
/** Discard diagnostics while testing the real BCrypt and file boundaries. */
void write(const char*, ...) {}
}

#define CHECK(condition) do { if (!(condition)) { \
  std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

namespace {
namespace fs = std::filesystem;
using pm::build::Hashes;
using pm::build::Record;
using pm::build::Standing;

const std::string kGame(64, 'a'), kUnity(64, 'b');
constexpr char kMap[] = "0123456789abcdef";

void write(const fs::path& file, const std::string& text) { std::ofstream(file, std::ios::binary) << text; }

/** Check known content, denied access, missing files, and a partial read failure. */
void hashes_a_file() {
  wchar_t directory[MAX_PATH], path[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, directory) > 0);
  CHECK(GetTempFileNameW(directory, L"pgh", 0, path) != 0);
  HANDLE fixture = CreateFileW(path, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, 0, nullptr);
  CHECK(fixture != INVALID_HANDLE_VALUE);
  DWORD written = 0;
  CHECK(WriteFile(fixture, "abc", 3, &written, nullptr) && written == 3);
  char digest[65] = {};
  CHECK(!pm::build::sha256_file(path, digest));  // exclusive writer denies reads
  CloseHandle(fixture);
  CHECK(pm::build::sha256_file(path, digest));
  CHECK(std::strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
  failSecondRead = true;
  reads = 0;
  CHECK(!pm::build::sha256_file(path, digest));
  CHECK(reads == 2);
  failSecondRead = false;
  CHECK(DeleteFileW(path));
  CHECK(!pm::build::sha256_file(path, digest));
}

/** A record reads back as written, is named for its build, and anything short of three good hashes is no record. */
void a_record_round_trips() {
  const Record record{kGame, kUnity, kMap};
  const std::string text = pm::build::write_record(record);
  Record read;
  CHECK(pm::build::parse_record(text, read));
  CHECK(read.gameAssembly == kGame && read.unityPlayer == kUnity && read.map == kMap);
  CHECK(pm::build::record_file_name(record) == std::string(16, 'a') + "-" + std::string(16, 'b') + ".txt");
  CHECK(pm::build::parse_record("later=x\r\n" + text + "another=y\r\n", read));  // a later loader may add lines
  CHECK(!pm::build::parse_record("", read));
  CHECK(!pm::build::parse_record("gameassembly=" + kGame + "\nunityplayer=" + kUnity + "\n", read));  // no map
  CHECK(!pm::build::parse_record("gameassembly=" + kGame + "\nunityplayer=" + kUnity + "\nmap=short\n", read));
  CHECK(!pm::build::parse_record("gameassembly=" + std::string(63, 'a') + "\nunityplayer=" + kUnity + "\nmap=" + kMap + "\n", read));
  CHECK(!pm::build::parse_record("gameassembly=" + std::string(64, 'A') + "\nunityplayer=" + kUnity + "\nmap=" + kMap + "\n", read));
  CHECK(!pm::build::parse_record("gameassembly=" + std::string(64, 'g') + "\nunityplayer=" + kUnity + "\nmap=" + kMap + "\n", read));
}

/** Known only for both hashes and this map. Everything else, however it fails, is unseen and never stops anything. */
void judges_a_build(const fs::path& root) {
  const fs::path records = root / "records";
  fs::create_directories(records);
  const Hashes running{kGame, kUnity};
  // No folder, an empty one, and a folder that is a file.
  CHECK(pm::build::judge(root / "none", running, kMap) == Standing::Unseen);
  CHECK(pm::build::judge(records, running, kMap) == Standing::Unseen);
  write(root / "a-file", "x");
  CHECK(pm::build::judge(root / "a-file", running, kMap) == Standing::Unseen);
  // Another build's record says nothing of this one, whichever library differs.
  write(records / "other.txt", pm::build::write_record({std::string(64, 'c'), kUnity, kMap}));
  write(records / "other2.txt", pm::build::write_record({kGame, std::string(64, 'c'), kMap}));
  CHECK(pm::build::judge(records, running, kMap) == Standing::Unseen);
  // What cannot be read counts for nothing, and hides no good record beside it.
  write(records / "aaa-bad.txt", "gameassembly=" + kGame);
  write(records / "aab-huge.txt", pm::build::write_record({kGame, kUnity, kMap}) + std::string(pm::build::kMaxRecordBytes, '#'));
  fs::create_directories(records / "aac-folder.txt");
  CHECK(pm::build::judge(records, running, kMap) == Standing::Unseen);
  // A record made with another map is stale: the host treats it as no record.
  write(records / "stale.txt", pm::build::write_record({kGame, kUnity, "fedcba9876543210"}));
  CHECK(pm::build::judge(records, running, kMap) == Standing::Stale);
  CHECK(!pm::build::checked(Standing::Stale) && !pm::build::checked(Standing::Unseen));
  // Other files are not records.
  write(records / "known.md", pm::build::write_record({kGame, kUnity, kMap}));
  CHECK(pm::build::judge(records, running, kMap) == Standing::Stale);
  write(records / "known.txt", pm::build::write_record({kGame, kUnity, kMap}));
  CHECK(pm::build::judge(records, running, kMap) == Standing::Known);
  CHECK(pm::build::checked(Standing::Known));
  // A hash that could not be computed is an unseen build, even with a record for the other library's hash.
  CHECK(pm::build::judge(records, {"", kUnity}, kMap) == Standing::Unseen);
  CHECK(pm::build::judge(records, {kGame, ""}, kMap) == Standing::Unseen);
}
}  // namespace

// A game's data folder holding what `engine` and `metadata` give its two headers, as the game's files lay them out.
void game_files(const fs::path& data, const std::string& engine, uint32_t magic, uint32_t metadata, uint32_t format = 22) {
  std::error_code error;
  fs::create_directories(data / L"il2cpp_data" / L"Metadata", error);
  std::string head(48, '\0');
  head[8] = static_cast<char>(format >> 24);
  head[9] = static_cast<char>(format >> 16);
  head[10] = static_cast<char>(format >> 8);
  head[11] = static_cast<char>(format);
  write(data / L"globalgamemanagers", head + engine + std::string(1, '\0') + "more of the file");
  std::string meta(reinterpret_cast<const char*>(&magic), 4);
  meta.append(reinterpret_cast<const char*>(&metadata), 4);
  write(data / L"il2cpp_data" / L"Metadata" / L"global-metadata.dat", meta + "more of the file");
}

// The read itself fails safe: what is missing, cut short or does not read as a version counts as unknown, never as known.
void engine_versions_read_from_the_game_files(const fs::path& root) {
  constexpr uint32_t kMagic = pm::engine::kMetadataMagic;
  const fs::path data = root / L"versions";
  game_files(data, "2022.3.40f1", kMagic, 31);
  pm::engine::Versions v = pm::engine::read(data);
  CHECK(v.engine == "2022.3.40f1" && v.metadata == 31 && pm::engine::known(v));

  game_files(data, "6000.0.23f1", kMagic, 31);
  v = pm::engine::read(data);
  CHECK(v.engine == "6000.0.23f1" && !pm::engine::known(v));
  game_files(data, "2022.3.40f1", kMagic, 32);
  CHECK(!pm::engine::known(pm::engine::read(data)));

  // Another header's format, another file's magic, and a version that does not read as one.
  game_files(data, "2022.3.40f1", kMagic, 31, 21);
  v = pm::engine::read(data);
  CHECK(v.engine.empty() && v.metadata == 31 && !pm::engine::known(v));
  game_files(data, "2022.3.40f1", kMagic + 1, 31);
  v = pm::engine::read(data);
  CHECK(v.engine == "2022.3.40f1" && v.metadata == -1 && !pm::engine::known(v));
  game_files(data, "2022.3 <b>", kMagic, 31);
  CHECK(pm::engine::read(data).engine.empty());
  game_files(data, std::string(40, '2'), kMagic, 31);  // no end within what a version can be
  CHECK(pm::engine::read(data).engine.empty());

  // Cut short, and not there at all.
  write(data / L"globalgamemanagers", std::string(48, '\0'));
  write(data / L"il2cpp_data" / L"Metadata" / L"global-metadata.dat", "abc");
  v = pm::engine::read(data);
  CHECK(v.engine.empty() && v.metadata == -1);
  v = pm::engine::read(root / L"no-such-folder");
  CHECK(v.engine.empty() && v.metadata == -1 && !pm::engine::known(v));
  CHECK(pm::engine::known({"2022.3.40f1", 31}) && !pm::engine::known({}));
  // This test program has no data folder beside it: the host's own read answers unknown and throws nothing.
  CHECK(!pm::engine::known(pm::build::running_versions()));
}

int main() {
  hashes_a_file();
  a_record_round_trips();
  std::error_code error;
  const fs::path root = fs::temp_directory_path(error) / (L"pg-build-check-" + std::to_wstring(GetCurrentProcessId()));
  fs::remove_all(root, error);
  judges_a_build(root);
  engine_versions_read_from_the_game_files(root);
  fs::remove_all(root, error);
  return failures ? 1 : 0;
}
