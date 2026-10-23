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
  void put32(size_t at, uint64_t v) {
    for (int i = 0; i < 4; ++i) bytes[at + i] = static_cast<uint8_t>(v >> (8 * i));
  }
  void dd(uint64_t v) {
    bytes.resize(bytes.size() + 4);
    put32(bytes.size() - 4, v);
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
  CHECK(f.complete);
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
  CHECK(f.complete);
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

// Whether each access's base provably holds `this` (rcx at entry), flow-insensitively.
std::vector<bool> this_bases(std::initializer_list<uint8_t> b) {
  std::vector<bool> out;
  for (const Access& a : facts(b).accesses) out.push_back(a.thisBase);
  return out;
}

void this_registers() {
  // mov byte ptr [rcx+0xD8], 1
  const CodeFacts f = facts({0xC6, 0x81, 0xD8, 0x00, 0x00, 0x00, 0x01});
  CHECK(f.accesses.size() == 1 && f.accesses[0].base == 1 && f.accesses[0].thisBase);

  // push rdi; mov rdi, rcx; mov byte ptr [rdi+0xD8], 1; mov byte ptr [rdi+0xF3], bpl;
  // movss xmm7, xmm6; pop rdi; ret
  CHECK((this_bases({0x57, 0x48, 0x8B, 0xF9, 0xC6, 0x87, 0xD8, 0x00, 0x00, 0x00, 0x01, 0x40, 0x88, 0xAF, 0xF3,
                     0x00, 0x00, 0x00, 0xF3, 0x0F, 0x10, 0xFE, 0x5F, 0xC3}) == std::vector<bool>{true, true}));
  // The copy written the other way: mov rdi, rcx as 89 /r.
  CHECK((this_bases({0x48, 0x89, 0xCF, 0xC6, 0x47, 0x08, 0x01}) == std::vector<bool>{true}));

  // push rdi; mov rdi, rcx; mov rax, [rdi+0x40]; mov byte ptr [rax+0xF3], 1; pop rdi; ret: rax holds
  // another object.
  const CodeFacts other = facts({0x57, 0x48, 0x8B, 0xF9, 0x48, 0x8B, 0x47, 0x40, 0xC6, 0x80, 0xF3, 0x00, 0x00,
                                 0x00, 0x01, 0x5F, 0xC3});
  CHECK(other.accesses.size() == 2);
  if (other.accesses.size() == 2) {
    CHECK(other.accesses[0].base == 7 && other.accesses[0].thisBase);
    CHECK(other.accesses[1].base == 0 && !other.accesses[1].thisBase);
  }

  // A register written by anything but a copy of this is not this anywhere in the function.
  // mov rdi, rcx; mov byte ptr [rdi+8], 1; mov rdi, [rdi+0x10]
  CHECK((this_bases({0x48, 0x8B, 0xF9, 0xC6, 0x47, 0x08, 0x01, 0x48, 0x8B, 0x7F, 0x10}) ==
         std::vector<bool>{false, false}));
  // mov rdi, rcx; xor edi, edi; mov byte ptr [rdi+8], 1
  CHECK((this_bases({0x48, 0x8B, 0xF9, 0x31, 0xFF, 0xC6, 0x47, 0x08, 0x01}) == std::vector<bool>{false}));
  // mov byte ptr [rcx+8], 1; call: rcx is volatile across it.
  CHECK((this_bases({0xC6, 0x41, 0x08, 0x01, 0xE8, 0xFB, 0x0F, 0x00, 0x00}) == std::vector<bool>{false}));
  // A copy of rcx before anything writes it still holds this after a call:
  // mov rdi, rcx; call; mov byte ptr [rdi+8], 1
  CHECK((this_bases({0x48, 0x8B, 0xF9, 0xE8, 0xF8, 0x0F, 0x00, 0x00, 0xC6, 0x47, 0x08, 0x01}) ==
         std::vector<bool>{true}));
  // ... but not one made after the call: call; mov rdi, rcx; mov byte ptr [rdi+8], 1
  CHECK((this_bases({0xE8, 0xFB, 0x0F, 0x00, 0x00, 0x48, 0x8B, 0xF9, 0xC6, 0x47, 0x08, 0x01}) ==
         std::vector<bool>{false}));
  // ... nor one some calls skip: test edx, edx; je +3; mov rdi, rcx; call; mov byte ptr [rdi+8], 1
  CHECK((this_bases({0x85, 0xD2, 0x74, 0x03, 0x48, 0x8B, 0xF9, 0xE8, 0xF4, 0x0F, 0x00, 0x00, 0xC6, 0x47, 0x08,
                     0x01}) == std::vector<bool>{false}));
  // mov edi, ecx: a 32-bit copy drops the upper half.
  CHECK((this_bases({0x8B, 0xF9, 0xC6, 0x47, 0x08, 0x01}) == std::vector<bool>{false}));
  // mov rdi, rcx; mov byte ptr [rdi+8], 1; pop rdi with no push of rdi.
  CHECK((this_bases({0x48, 0x8B, 0xF9, 0xC6, 0x47, 0x08, 0x01, 0x5F}) == std::vector<bool>{false}));
  // An argument register never written holds no this: mov byte ptr [rdx+8], 1
  CHECK((this_bases({0xC6, 0x42, 0x08, 0x01}) == std::vector<bool>{false}));
  // Copies among themselves, never from rcx: mov rdi, rsi; mov rsi, rdi; mov byte ptr [rdi+8], 1
  CHECK((this_bases({0x48, 0x8B, 0xFE, 0x48, 0x8B, 0xF7, 0xC6, 0x47, 0x08, 0x01}) == std::vector<bool>{false}));
  // A byte register without REX names the high byte: mov rbx, rcx; mov bh, 1 writes rbx.
  CHECK((this_bases({0x48, 0x8B, 0xD9, 0xB7, 0x01, 0xC6, 0x43, 0x08, 0x01}) == std::vector<bool>{false}));
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

// A switch as MSVC compiles it, shaped like the real one at 0x4E3230: the case number indexes the
// case RVAs after the code, read relative to the image base, and jumps there. With `bytes` it first
// indexes a byte table of case indices that follows them. `sib` 0x80 is [r8+rax*4+disp].
// `byIndex` reads the byte table with the image base as the index, [rax+r8+BYTES], and without
// `imageBase` r8 is copied from rcx rather than set to the image base.
std::vector<uint8_t> switch_method(uint64_t start, bool bytes, uint8_t sib = 0x80, bool byIndex = false,
                                   bool imageBase = true) {
  Asm a{start};
  a.raw({0x48, 0x83, 0xEC, 0x28});                         // sub rsp, 0x28
  a.raw({0x83, 0xFA, static_cast<uint8_t>(bytes ? 3 : 2)});  // cmp edx, n
  a.raw({0x77, 0x00});                                     // ja default
  const size_t ja = a.bytes.size() - 1;
  a.raw({0x48, 0x63, 0xC2});           // movsxd rax, edx
  if (imageBase)
    a.rel({0x4C, 0x8D, 0x05}, 0);  // lea r8, [__ImageBase]
  else
    a.raw({0x49, 0x89, 0xC8});     // mov r8, rcx
  size_t byteRef = 0;
  if (bytes) {
    // movzx eax, byte ptr [r8+rax+BYTES], or [rax+r8+BYTES]
    a.raw({static_cast<uint8_t>(byIndex ? 0x42 : 0x41), 0x0F, 0xB6, 0x84, 0x00, 0, 0, 0, 0});
    byteRef = a.bytes.size() - 4;
  }
  a.raw({0x41, 0x8B, 0x94, sib, 0, 0, 0, 0});  // mov edx, [r8+rax*4+TABLE]
  const size_t tableRef = a.bytes.size() - 4;
  a.raw({0x49, 0x03, 0xD0, 0xFF, 0xE2});  // add rdx, r8; jmp rdx
  std::vector<uint64_t> cases;
  for (const uint8_t disp : std::initializer_list<uint8_t>{0x20, 0x28, 0x30}) {
    cases.push_back(a.here());
    a.raw({0x48, 0x8B, 0x41, disp, 0x48, 0x83, 0xC4, 0x28, 0xC3});  // mov rax, [rcx+disp]; add rsp, 0x28; ret
  }
  const uint64_t fallback = a.here();
  a.raw({0x33, 0xC0, 0x48, 0x83, 0xC4, 0x28, 0xC3});  // xor eax, eax; add rsp, 0x28; ret
  a.bytes[ja] = static_cast<uint8_t>(fallback - (start + ja + 1));
  while (a.bytes.size() % 4) a.raw({0xCC});
  a.put32(tableRef, a.here());
  for (const uint64_t c : cases) a.dd(c);
  if (bytes) {
    a.put32(byteRef, a.here());
    a.raw({0x00, 0x01, 0x01, 0x02});
  }
  return a.bytes;
}

void switch_tables_are_data() {
  for (const bool bytes : {false, true}) {
    const std::vector<uint8_t> fn = switch_method(0x4000, bytes);
    const CodeFacts f = read_code(fn, 0x4000);
    CHECK(f.complete);
    // Only the three cases' loads: nothing read from the tables as code.
    CHECK(f.accesses.size() == 3);
    for (const Access& a : f.accesses) CHECK(!a.store && (a.disp == 0x20 || a.disp == 0x28 || a.disp == 0x30));
    CHECK(f.callTargets.empty());
    size_t cases = 0, byteTables = 0;
    for (const std::string& t : f.normalized) {
      cases += t.starts_with("dd +");
      byteTables += t.starts_with("db ");
    }
    CHECK(cases == 3);
    CHECK(byteTables == (bytes ? 1u : 0u));
    // Two copies at other places normalise equal, the tables' RVAs included.
    const CodeFacts g = read_code(switch_method(0x7340, bytes), 0x7340);
    CHECK(g.complete);
    CHECK(f.normalized == g.normalized);
  }
}

void byte_table_indexed_by_the_image_base() {
  const std::vector<uint8_t> fn = switch_method(0x4000, true, 0x80, true);
  const CodeFacts f = read_code(fn, 0x4000);
  CHECK(f.complete);
  CHECK(f.accesses.size() == 3);
  size_t byteTables = 0;
  for (const std::string& t : f.normalized) byteTables += t.starts_with("db ");
  CHECK(byteTables == 1);
  CHECK(f.normalized == read_code(switch_method(0x7340, true, 0x80, true), 0x7340).normalized);
}

void unknown_in_function_data_is_incomplete() {
  // In-function RVAs read through registers not known to hold the image base.
  for (const bool bytes : {false, true}) {
    const CodeFacts f = read_code(switch_method(0x4000, bytes, 0x80, false, false), 0x4000);
    CHECK(!f.complete);
  }
  // A byte table with no case table to index.
  Asm c{0x1000};
  c.rel({0x4C, 0x8D, 0x05}, 0);                          // lea r8, [__ImageBase]
  c.raw({0x41, 0x0F, 0xB6, 0x84, 0x00});                 // movzx eax, byte ptr [r8+rax+BYTES]
  c.dd(0x1000 + c.bytes.size() + 5);
  c.raw({0xC3, 0x00, 0x01});
  CHECK(!read_code(c.bytes, 0x1000).complete);
  // A case table whose first entry is not in the function.
  Asm d{0x1000};
  d.rel({0x4C, 0x8D, 0x05}, 0);                          // lea r8, [__ImageBase]
  d.raw({0x41, 0x8B, 0x94, 0x80});                       // mov edx, [r8+rax*4+TABLE]
  d.dd(0x1000 + d.bytes.size() + 4 + 3);
  d.raw({0x41, 0xFF, 0xE0});                             // jmp r8
  d.dd(0xFFFFFFFF);
  CHECK(!read_code(d.bytes, 0x1000).complete);
  // An in-function disp32 with no base register.
  Asm e{0x1000};
  e.raw({0x8B, 0x04, 0x85});  // mov eax, [rax*4+TABLE]
  e.dd(0x1008);
  e.raw({0xC3, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  CHECK(!read_code(e.bytes, 0x1000).complete);

  // A table read at scale 8: not a shape the reader knows.
  CHECK(!read_code(switch_method(0x4000, false, 0xC0), 0x4000).complete);
  // A case that lands inside an instruction.
  std::vector<uint8_t> fn = switch_method(0x4000, false);
  fn[fn.size() - 4] += 1;
  CHECK(!read_code(fn, 0x4000).complete);
  // A rip-relative read of data inside the function.
  Asm a{0x1000};
  a.rel({0x8B, 0x05}, 0x1008);  // mov eax, [rip+0x1008]
  a.raw({0xC3, 0xCC, 0x00, 0x00, 0x00, 0x00});
  CHECK(!read_code(a.bytes, 0x1000).complete);
  // A lea of an in-function address is not a read.
  Asm b{0x1000};
  b.rel({0x48, 0x8D, 0x05}, 0x1007);  // lea rax, [rip+0x1007]
  b.raw({0xC3});
  CHECK(read_code(b.bytes, 0x1000).complete);
}

void undecodable_instructions_fail_closed() {
  // vmovss xmm0, [rdi+0x20]: HDE64 would read C5 as an error, then FA as cli and the rest as a fake
  // adc store to [rdi+0x20].
  const CodeFacts f = facts({0xC5, 0xFA, 0x10, 0x47, 0x20, 0xC6, 0x87, 0xD8, 0x00, 0x00, 0x00, 0x01, 0xC3});
  CHECK(!f.complete);
  CHECK(f.normalized.size() == 3);
  if (f.normalized.size() == 3) {
    CHECK(f.normalized[0] == "?c5fa104720");
    CHECK(f.normalized[1] == "c687d800000001");
  }
  CHECK(f.accesses.size() == 1);
  if (f.accesses.size() == 1) CHECK(f.accesses[0].disp == 0xD8 && f.accesses[0].store);

  // Each of these decodes to its own length, so the ret after it is read as a ret.
  for (const std::vector<uint8_t>& insn : std::vector<std::vector<uint8_t>>{
           {0x66, 0x0F, 0x38, 0x00, 0xC1},              // pshufb xmm0, xmm1
           {0x66, 0x0F, 0x3A, 0x0F, 0xC1, 0x08},        // palignr xmm0, xmm1, 8
           {0xC4, 0xE2, 0x79, 0x00, 0x44, 0x24, 0x10},  // vpshufb xmm0, xmm0, [rsp+0x10]
           {0xC4, 0xE3, 0x79, 0x0F, 0xC1, 0x08},        // vpalignr xmm0, xmm0, xmm1, 8
           {0xC5, 0xF9, 0x70, 0xC1, 0x1B},              // vpshufd xmm0, xmm1, 0x1B
           {0xC5, 0xF8, 0x77},                          // vzeroupper
           {0x62, 0xF1, 0x7C, 0x48, 0x10, 0x47, 0x01}}) {  // vmovups zmm0, [rdi+0x40]
    std::vector<uint8_t> fn = insn;
    fn.push_back(0xC3);
    const CodeFacts g = read_code(fn, 0x1000);
    CHECK(!g.complete);
    CHECK(g.normalized.size() == 2);
    CHECK(!g.normalized.empty() && g.normalized.back() == "c3");
  }

  // An opcode HDE64 rejects outright (push es is invalid in 64-bit mode).
  CHECK(!facts({0x06, 0xC3}).complete);
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
  this_registers();
  normalized_masks_own_flag();
  switch_tables_are_data();
  byte_table_indexed_by_the_image_base();
  unknown_in_function_data_is_incomplete();
  undecodable_instructions_fail_closed();
  return g_failures ? 1 : 0;
}
