#pragma once

#include <windows.h>
#include <bcrypt.h>

#include <cstdio>

// The SHA-256 of a whole file, for the host's build check and the offline check that writes its records.
// Link bcrypt.
namespace pm::build {

/** Read and hash the whole file, refusing unreadable chunks and avoiding heap allocation. A read error never gives a partial digest. */
inline bool sha256_file(const wchar_t* path, char (&hex)[65]) {
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
  for (int i = 0; i < 32; ++i) std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
  return true;
}

}  // namespace pm::build
