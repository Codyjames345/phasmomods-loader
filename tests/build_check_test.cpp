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

namespace pg::log {
/** Discard diagnostics while testing the real BCrypt and file boundaries. */
void write(const char*, ...) {}
}

#define CHECK(condition) do { if (!(condition)) { \
  std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

/** Check known content, denied access, missing files, and a partial read failure. */
int main() {
  wchar_t directory[MAX_PATH], path[MAX_PATH];
  CHECK(GetTempPathW(MAX_PATH, directory) > 0);
  CHECK(GetTempFileNameW(directory, L"pgh", 0, path) != 0);
  HANDLE fixture = CreateFileW(path, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, 0, nullptr);
  CHECK(fixture != INVALID_HANDLE_VALUE);
  DWORD written = 0;
  CHECK(WriteFile(fixture, "abc", 3, &written, nullptr) && written == 3);
  char digest[65] = {};
  CHECK(!pg::build::sha256_file(path, digest));  // exclusive writer denies reads
  CloseHandle(fixture);
  CHECK(pg::build::sha256_file(path, digest));
  CHECK(std::strcmp(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);
  failSecondRead = true;
  reads = 0;
  CHECK(!pg::build::sha256_file(path, digest));
  CHECK(reads == 2);
  failSecondRead = false;
  CHECK(DeleteFileW(path));
  CHECK(!pg::build::sha256_file(path, digest));
  return failures ? 1 : 0;
}
