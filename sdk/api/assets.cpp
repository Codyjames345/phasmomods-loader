#include "assets.h"

#include <windows.h>

#include "mod_assets.h"

namespace pm::api {

std::filesystem::path assets_folder() {
  HMODULE self = nullptr;
  wchar_t path[32768];
  // The module holding this function is the mod's own DLL copy: the library is linked into each mod.
  if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                          reinterpret_cast<LPCWSTR>(&assets_folder), &self))
    return {};
  const DWORD length = GetModuleFileNameW(self, path, 32768);
  if (!length || length >= 32768) return {};
  const std::filesystem::path folder = pm::modassets::beside_copy(path);
  std::error_code error;
  return std::filesystem::is_directory(folder, error) ? folder : std::filesystem::path{};
}

}  // namespace pm::api
