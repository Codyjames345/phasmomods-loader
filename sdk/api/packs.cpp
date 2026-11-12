#include "packs.h"

#include "mod_api.h"
#include "mod_glue.h"

namespace pm::api::packs {

namespace {

// Whether the host the mod is bound to has the content pack functions (Contract 2.4 and later).
const PmHostApi* host_with_packs() {
  const PmHostApi* host = mod::bound() ? &mod::host() : nullptr;
  if (!host || !pm_api_compatible(host->version, (PMMOD_API_MAJOR << 16) | 4u)) return nullptr;
  return host;
}

}  // namespace

std::vector<Pack> owned() {
  std::vector<Pack> packs;
  const PmHostApi* host = host_with_packs();
  if (!host) return packs;
  const char* id = mod::info().id;
  const uint32_t count = host->content_packs(id, nullptr, 0, sizeof(PmContentPack));
  std::vector<PmContentPack> list(count);
  const uint32_t filled = count ? host->content_packs(id, list.data(), count, sizeof(PmContentPack)) : 0;
  for (uint32_t i = 0; i < filled && i < count; ++i) {
    const PmContentPack& p = list[i];
    packs.push_back({p.id ? p.id : "", p.name ? p.name : "", p.version ? p.version : "",
                     p.folder ? std::filesystem::path(p.folder) : std::filesystem::path(),
                     p.text ? std::filesystem::path(p.text) : std::filesystem::path()});
  }
  return packs;
}

void failed(std::string_view packId, std::string_view why) {
  if (const PmHostApi* host = host_with_packs()) host->content_pack_failed(std::string(packId).c_str(), std::string(why).c_str());
}

}  // namespace pm::api::packs
