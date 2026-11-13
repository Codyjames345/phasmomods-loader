// Offline tests for a mod's own content packs (sdk/api/packs.h) over the fake host: what the fake host's
// content_packs answers, what the wrapper hands the mod, and how the host fills a caller's array of a
// bigger struct. The host's own list of a framework's packs (host/mods.h packs_for and pack_failed) is
// tested in tests/mods_test.cpp. Run: build\content_packs_test.exe
#include <cstddef>
#include <cstring>
#include <cwchar>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

#include "check.h"
#include "fake_game.h"
#include "mod_glue.h"
#include "packs.h"

namespace {

namespace fake = pm::fake;
namespace packs = pm::api::packs;

const PmModInfo kInfo{PMMOD_API_VERSION, "me.mod", "My Mod", 210, 0};

// Two packs made for this mod and one for another: what the fake host's content_packs answers, each pack
// for its framework in the order given.
void give_packs() {
  fake::reset();
  fake::content_packs({
      {"me.mod", "b.pack", "Bee Pack", "1.0.0", L"mods\\b.pack", L"mods\\b.pack\\i18n\\default.json"},
      {"me.mod", "a.pack", "Ay Pack", "0.9.0", L"mods\\a.pack", L"mods\\a.pack\\i18n\\default.json"},
      {"other.mod", "z.pack", "Zed Pack", "2.0.0", L"mods\\z.pack", L"mods\\z.pack\\i18n\\default.json"},
  });
}

// The wrapper gives this mod's packs only, as the host gives them, every field copied.
void owned_lists_only_my_packs_in_order() {
  give_packs();
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  const std::vector<packs::Pack> mine = packs::owned();
  CHECK(mine.size() == 2);
  if (mine.size() != 2) return;
  CHECK(mine[0].id == "b.pack" && mine[1].id == "a.pack");  // the host's order, not sorted again
  CHECK(mine[0].name == "Bee Pack" && mine[0].version == "1.0.0");
  CHECK(mine[1].name == "Ay Pack" && mine[1].version == "0.9.0");
  CHECK(mine[0].folder == std::filesystem::path(L"mods\\b.pack"));
  CHECK(mine[0].text == std::filesystem::path(L"mods\\b.pack\\i18n\\default.json"));
  CHECK(mine[1].folder == std::filesystem::path(L"mods\\a.pack"));
  CHECK(mine[1].text == std::filesystem::path(L"mods\\a.pack\\i18n\\default.json"));
}

// Before the mod is bound there is no host to ask, so it has none; after, the host's answer. Run first:
// nothing in this process has bound a mod before it.
void owned_is_empty_before_bind() {
  give_packs();
  CHECK(packs::owned().empty());
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  CHECK(packs::owned().size() == 2);
}

// A pack the mod cannot use reaches the host with the reason its Mods panel row will show.
void failed_reaches_the_host() {
  give_packs();
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  packs::failed("a.pack", "Broken");
  const std::vector<std::pair<std::string, std::string>>& told = fake::failed_packs();
  CHECK(told.size() == 1);
  if (told.size() != 1) return;
  CHECK(told[0].first == "a.pack" && told[0].second == "Broken");
}

// The host fills nothing when the caller's entry size is smaller than its own struct, nor when no mod
// is named.
void the_struct_size_is_checked() {
  give_packs();
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  PmContentPack out[4]{};
  CHECK(pm::mod::host().content_packs("me.mod", out, 4, sizeof(PmContentPack) - 1) == 0);
  CHECK(pm::mod::host().content_packs(nullptr, out, 4, sizeof(PmContentPack)) == 0);
}

// A mod built for contract 2.6 passes 2.6's entry size; once a later minor grows PmContentPack, the host must still
// fill those entries, up to the size the caller gave (the review, 2026-11-13).
void a_2_6_caller_is_filled_up_to_its_size() {
  give_packs();
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  CHECK(PM_CONTENT_PACK_SIZE_2_6 == offsetof(PmContentPack, text) + sizeof(const wchar_t*));
  PmContentPack out[4]{};
  CHECK(pm::mod::host().content_packs("me.mod", out, 4, PM_CONTENT_PACK_SIZE_2_6) == 2);
  CHECK(out[0].size == PM_CONTENT_PACK_SIZE_2_6);
  CHECK(out[0].id && std::strcmp(out[0].id, "b.pack") == 0);
}

// The caller may hold a bigger struct per entry: the host steps itemSize bytes, writes at most
// sizeof(PmContentPack) bytes into each entry and sets its size, and leaves everything else alone.
void a_larger_caller_struct_is_stepped_by_item_size() {
  struct Bigger {
    PmContentPack p;
    std::uint64_t extra;
  };
  give_packs();
  CHECK(pm::mod::bind(fake::host(), &kInfo));
  Bigger entries[4]{};
  for (Bigger& e : entries) e.extra = 0x5A5A5A5A5A5A5A5Aull;
  const uint32_t count = pm::mod::host().content_packs("me.mod", reinterpret_cast<PmContentPack*>(entries), 4, sizeof(Bigger));
  CHECK(count == 2);
  if (count != 2) return;
  CHECK(entries[0].p.size == sizeof(PmContentPack));
  CHECK(entries[0].p.id && std::strcmp(entries[0].p.id, "b.pack") == 0);
  CHECK(entries[0].p.name && std::strcmp(entries[0].p.name, "Bee Pack") == 0);
  CHECK(entries[1].p.id && std::strcmp(entries[1].p.id, "a.pack") == 0);
  CHECK(entries[0].p.version && std::strcmp(entries[0].p.version, "1.0.0") == 0);
  CHECK(entries[0].p.folder && std::wcscmp(entries[0].p.folder, L"mods\\b.pack") == 0);
  CHECK(entries[0].p.text && std::wcscmp(entries[0].p.text, L"mods\\b.pack\\i18n\\default.json") == 0);
  for (const Bigger& e : entries) CHECK(e.extra == 0x5A5A5A5A5A5A5A5Aull);  // every extra untouched
  CHECK(entries[2].p.id == nullptr);  // the entries past count are left as the caller left them
}

}  // namespace

int main() {
  owned_is_empty_before_bind();  // first: the only unbound state there is in this process
  owned_lists_only_my_packs_in_order();
  failed_reaches_the_host();
  the_struct_size_is_checked();
  a_larger_caller_struct_is_stepped_by_item_size();
  a_2_6_caller_is_filled_up_to_its_size();
  std::printf("%s\n", g_failures ? "content_packs tests FAILED" : "content_packs tests passed");
  return g_failures ? 1 : 0;
}
