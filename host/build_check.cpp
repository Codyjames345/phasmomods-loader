#include "build_check.h"

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>
#include <cstring>

#include "log.h"

namespace pg::build {
namespace {
constexpr char kGameAssemblySha256[] =
    "e300eda993f2479922628f1b44f59e60c156c240f5a07c7289c4bd0fe4b790c8";
constexpr char kUnityPlayerSha256[] =
    "cd106e0f8a54c36ee741494ea2c557752890055220a825ce49b0fdf87d50c496";
}  // namespace

/** Read and hash the whole file, refusing unreadable chunks and avoiding heap allocation. */
bool sha256_file(const wchar_t* path, char (&hex)[65]) {
  hex[0] = '\0';
  HANDLE f = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                         OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
  if (f == INVALID_HANDLE_VALUE) return false;
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0 &&
            BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0;
  unsigned char buf[64 * 1024];
  DWORD got = 0;
  while (ok) {
    if (!ReadFile(f, buf, sizeof buf, &got, nullptr)) {
      ok = false;
      break;
    }
    if (!got) break;
    ok = BCryptHashData(hash, buf, got, 0) == 0;
  }
  unsigned char digest[32];
  ok = ok && BCryptFinishHash(hash, digest, sizeof digest, 0) == 0;
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  CloseHandle(f);
  if (!ok) return false;
  for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return true;
}

namespace {
/** Require the loaded module's complete on-disk hash to match its audited binary. */
bool verify(void* module, const char* expected, const char* label) {
  wchar_t path[MAX_PATH];
  DWORD n = GetModuleFileNameW(static_cast<HMODULE>(module), path, MAX_PATH);
  char hex[65] = {};
  if (n == 0 || n >= MAX_PATH || !sha256_file(path, hex)) {
    PG_LOG("build: could not hash %s", label);
    return false;
  }
  bool ok = std::strcmp(hex, expected) == 0;
  PG_LOG("build: %s sha256 %s (%s)", label, hex, ok ? "supported" : "UNSUPPORTED");
  return ok;
}
}  // namespace

/** Check the GameAssembly build before resolving fixed addresses or fields. */
bool verify_game_assembly(void* module) { return verify(module, kGameAssemblySha256, "GameAssembly.dll"); }
/** Check the UnityPlayer build before creating hooks or installing steering. */
bool verify_unity_player(void* module) { return verify(module, kUnityPlayerSha256, "UnityPlayer.dll"); }

}  // namespace pg::build
