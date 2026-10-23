#include <cstdint>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <vector>

#include "check.h"
#include "code.h"
#include "image.h"
#include "sdk_image_fixture.h"

using namespace pg::sdk;

namespace {

// Hand-assembles a function placed at `start`, computing rel32 fields from absolute targets.
struct Asm {
  uint64_t start;
  std::vector<uint8_t> bytes;

  uint64_t here() const { return start + bytes.size(); }
  void raw(std::initializer_list<uint8_t> b) { bytes.insert(bytes.end(), b); }
  // An instruction whose rel32 (a rip-relative disp32 or a branch offset) lies between head and
  // tail: the field is the target less the address of the instruction's end.
  void rel(std::initializer_list<uint8_t> head, uint64_t target, std::initializer_list<uint8_t> tail = {}) {
    const uint64_t end = here() + head.size() + 4 + tail.size();
    const auto v = static_cast<uint32_t>(static_cast<int32_t>(target - end));
    raw(head);
    for (int i = 0; i < 4; ++i) bytes.push_back(static_cast<uint8_t>(v >> (8 * i)));
    raw(tail);
  }
};

std::vector<uint8_t> filled(size_t n, uint8_t b = 0x90) { return std::vector<uint8_t>(n, b); }

CodeFacts facts(std::initializer_list<uint8_t> b) {
  const std::vector<uint8_t> v(b);
  return read_code(v, 0x1000);
}

void function_bounds_from_pdata() {
  ImageBuilder b;
  b.function(0x1000, filled(0x20));
  b.function(0x1040, filled(0x10));
  // Two separate functions that touch stay separate.
  b.function(0x1060, filled(0x10));
  b.function(0x1070, filled(0x10));
  const std::vector<uint8_t> mapped = b.build();
  const CodeImage image(mapped);
  CHECK(image.valid());
  CHECK(image.function(0x1010).size() == 0x20);
  CHECK(image.function(0x1010).data() == mapped.data() + 0x1000);
  CHECK(image.function(0x1000).size() == 0x20);
  CHECK(image.function(0x1030).empty());
  CHECK(image.function(0x1020).empty());
  CHECK(image.function_start(0x1045) == 0x1040);
  CHECK(image.function_start(0x1030) == 0);
  CHECK(image.function(0x1060).size() == 0x10);
  CHECK(image.function_start(0x1070) == 0x1070);
  CHECK(image.function(0x90000).empty());

  const std::vector<uint8_t> junk(0x200, 0xCC);
  CHECK(!CodeImage(junk).valid());
  CHECK(CodeImage(junk).function(0x10).empty());
  CHECK(!CodeImage(std::span<const uint8_t>()).valid());
}

void chained_unwind_entry_maps_to_primary() {
  ImageBuilder b;
  b.function(0x2000, filled(0x30));
  b.chunk(0x2030, filled(0x10), 0x2000);
  // A chunk chained to a chunk, with an odd count of unwind codes before the parent entry.
  b.chunk(0x2040, filled(0x10), 0x2030, 3);
  // A chain to an entry that is not in the table.
  b.code(0x3000, filled(0x10));
  b.entry(0x3000, 0x3010, b.unwind(ImageBuilder::RuntimeFunction{0x7000, 0x7010, b.unwind()}));
  // Unwind info outside the image.
  b.entry(0x3100, 0x3110, 0xFFFFFFF0);
  // A chained entry whose parent RUNTIME_FUNCTION would run past the end of the image.
  b.entry(0x3200, 0x3210, ImageBuilder::kSize - 6);
  b.image[ImageBuilder::kSize - 6] = 1 | (0x4 << 3);
  // A chain that names itself.
  const uint32_t self = b.nextUnwind;
  b.entry(0x3300, 0x3310, b.unwind(ImageBuilder::RuntimeFunction{0x3300, 0x3310, self}));
  const std::vector<uint8_t> mapped = b.build();
  const CodeImage image(mapped);
  CHECK(image.valid());
  CHECK(image.function_start(0x2038) == 0x2000);
  CHECK(image.function_start(0x2048) == 0x2000);
  CHECK(image.function(0x2038).data() == mapped.data() + 0x2000);
  CHECK(image.function(0x2038).size() == 0x50);
  CHECK(image.function(0x2000).size() == 0x50);
  for (const uint64_t rva : {0x3008, 0x3108, 0x3208, 0x3308}) {
    CHECK(image.function(rva).empty());
    CHECK(image.function_start(rva) == 0);
  }
}

void map_pe_file_lays_out_by_rva() {
  ImageBuilder b;
  b.function(0x1000, filled(0x20, 0xC3));
  const std::filesystem::path dir = std::filesystem::temp_directory_path() / "pg_sdk_code_test";
  std::filesystem::create_directories(dir);
  auto write = [&](const char* name, const std::vector<uint8_t>& bytes) {
    const std::filesystem::path path = dir / name;
    std::ofstream(path, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return path;
  };
  const std::vector<uint8_t> file = b.file();
  CHECK(file.size() != b.build().size());
  CHECK(map_pe_file(write("good.dll", file)) == b.build());
  CHECK(CodeImage(map_pe_file(dir / "good.dll")).function(0x1010).size() == 0x20);

  CHECK(map_pe_file(dir / "missing.dll").empty());
  CHECK(map_pe_file(write("truncated.dll", std::vector<uint8_t>(file.begin(), file.begin() + 0x100))).empty());
  CHECK(map_pe_file(write("short.dll", std::vector<uint8_t>(file.begin(), file.end() - 0x10))).empty());
  std::vector<uint8_t> bad = file;
  bad[0] = 'X';
  CHECK(map_pe_file(write("magic.dll", bad)).empty());
  bad = file;
  bad[0x58] = 0x0B;  // PE32, not PE32+
  bad[0x59] = 0x01;
  CHECK(map_pe_file(write("pe32.dll", bad)).empty());
  std::filesystem::remove_all(dir);
}

void call_targets() {
  Asm a{0x1000};
  a.rel({0xE8}, 0x5000);    // call 0x5000
  a.rel({0xE8}, 0x1010);    // call 0x1010, inside
  a.rel({0xE9}, 0x1010);    // jmp 0x1010, inside
  a.raw({0x90, 0x90});
  a.raw({0xC3});            // 0x1010
  a.rel({0xE9}, 0x6000);    // jmp 0x6000, a tail call
  const CodeFacts f = read_code(a.bytes, 0x1000);
  CHECK((f.callTargets == std::vector<uint64_t>{0x5000, 0x6000}));
  CHECK(f.normalized.size() == 7);
  CHECK(f.accesses.empty());
}

void byte_store_with_imm() {
  const CodeFacts f = facts({0xC6, 0x87, 0xD8, 0x00, 0x00, 0x00, 0x01});  // mov byte ptr [rdi+0xD8], 1
  CHECK(f.accesses.size() == 1);
  if (f.accesses.size() != 1) return;
  const Access& a = f.accesses[0];
  CHECK(a.at == 0x1000);
  CHECK(a.disp == 0xD8);
  CHECK(a.store);
  CHECK(a.hasImm);
  CHECK(a.imm == 1);
}

void register_store_has_no_imm() {
  const CodeFacts f = facts({0x40, 0x88, 0xAF, 0xF3, 0x00, 0x00, 0x00});  // mov byte ptr [rdi+0xF3], bpl
  CHECK(f.accesses.size() == 1);
  if (f.accesses.size() != 1) return;
  CHECK(f.accesses[0].disp == 0xF3);
  CHECK(f.accesses[0].store);
  CHECK(!f.accesses[0].hasImm);

  // mov qword ptr [rdi+0x20], rax; mov dword ptr [rbx-0x10], 0x12345678
  const CodeFacts g = facts({0x48, 0x89, 0x47, 0x20, 0xC7, 0x43, 0xF0, 0x78, 0x56, 0x34, 0x12});
  CHECK(g.accesses.size() == 2);
  if (g.accesses.size() != 2) return;
  CHECK(g.accesses[0].disp == 0x20 && g.accesses[0].store && !g.accesses[0].hasImm);
  CHECK(g.accesses[1].at == 0x1004);
  CHECK(g.accesses[1].disp == -0x10 && g.accesses[1].store && g.accesses[1].hasImm);
  CHECK(g.accesses[1].imm == 0x12345678);
}

void load_is_not_store() {
  const CodeFacts f = facts({0x80, 0xB8, 0xD8, 0x00, 0x00, 0x00, 0x00});  // cmp byte ptr [rax+0xD8], 0
  CHECK(f.accesses.size() == 1);
  if (f.accesses.size() != 1) return;
  CHECK(f.accesses[0].disp == 0xD8);
  CHECK(!f.accesses[0].store);
  CHECK(!f.accesses[0].hasImm);

  // mov eax, dword ptr [rdi+0xD8]; movss xmm0, dword ptr [rcx+0x3C]; movss dword ptr [rcx+0x40], xmm0
  const CodeFacts g = facts({0x8B, 0x87, 0xD8, 0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0x41, 0x3C, 0xF3, 0x0F, 0x11,
                             0x41, 0x40});
  CHECK(g.accesses.size() == 3);
  if (g.accesses.size() != 3) return;
  CHECK(!g.accesses[0].store && g.accesses[0].disp == 0xD8);
  CHECK(!g.accesses[1].store && g.accesses[1].disp == 0x3C);
  CHECK(g.accesses[2].store && g.accesses[2].disp == 0x40);
}

void rip_and_rsp_are_not_field_accesses() {
  CHECK(facts({0xC6, 0x05, 0x10, 0x20, 0x30, 0x00, 0x01}).accesses.empty());  // mov byte ptr [rip+X], 1
  CHECK(facts({0xC6, 0x44, 0x24, 0x20, 0x01}).accesses.empty());              // mov byte ptr [rsp+0x20], 1
  CHECK(facts({0xC6, 0x04, 0x08, 0x01}).accesses.empty());                    // mov byte ptr [rax+rcx], 1
  CHECK(facts({0x48, 0x8D, 0x4F, 0x20}).accesses.empty());                    // lea rcx, [rdi+0x20]
  // [r12+0x18] is a field access: r12 shares rsp's register bits only through REX.B.
  const CodeFacts f = facts({0x41, 0xC6, 0x44, 0x24, 0x18, 0x01});  // mov byte ptr [r12+0x18], 1
  CHECK(f.accesses.size() == 1 && f.accesses[0].disp == 0x18);
}

// A method shaped like IL2CPP's: its runtime-metadata init check on `flag`, a load of `data`, and a
// call. With `setFlag` 0 the init check sets some other byte, so the leading cmp is not its flag.
std::vector<std::string> method(uint64_t start, uint64_t flag, uint64_t data, uint64_t setFlag = 0) {
  Asm a{start};
  a.raw({0x48, 0x83, 0xEC, 0x28});            // sub rsp, 0x28
  a.rel({0x80, 0x3D}, flag, {0x00});          // cmp byte ptr [rip+flag], 0
  a.raw({0x75, 0x0E});                        // jne past the init
  a.rel({0x48, 0x8D, 0x0D}, data);            // lea rcx, [rip+data]
  a.rel({0xC6, 0x05}, setFlag ? setFlag : flag, {0x01});  // mov byte ptr [rip+flag], 1
  a.rel({0x48, 0x8B, 0x05}, data);            // mov rax, [rip+data]
  a.rel({0xE8}, 0x9000);                      // call 0x9000
  a.raw({0x48, 0x83, 0xC4, 0x28, 0xC3});      // add rsp, 0x28; ret
  return read_code(a.bytes, start).normalized;
}

void normalized_masks_own_flag() {
  const auto base = method(0x1000, 0x8000, 0x8100);
  CHECK(base.size() == 9);
  CHECK(base == method(0x3000, 0x8000, 0x8100));  // the place does not matter
  CHECK(base == method(0x3000, 0x8010, 0x8100));  // nor the method's own flag
  CHECK(base != method(0x3000, 0x8000, 0x8108));  // another rip-relative target does
  // Without the later store of 1, the leading cmp is not the init flag, and its target counts.
  CHECK(method(0x1000, 0x8000, 0x8100, 0x8200) != method(0x3000, 0x8010, 0x8100, 0x8200));
}

}  // namespace

int main() {
  function_bounds_from_pdata();
  chained_unwind_entry_maps_to_primary();
  map_pe_file_lays_out_by_rva();
  call_targets();
  byte_store_with_imm();
  register_store_has_no_imm();
  load_is_not_store();
  rip_and_rsp_are_not_field_accesses();
  normalized_masks_own_flag();
  return g_failures ? 1 : 0;
}
