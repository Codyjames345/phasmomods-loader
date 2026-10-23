#include "code.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <iterator>
#include <optional>

#include "hde64.h"

namespace pg::sdk {
namespace {

constexpr size_t kFlagWindow = 6;  // the init check is among a method's first instructions

// What a decoded item is: an instruction HDE64 read, one it could not (an error, or a form it does
// not know, read only for its length), an entry of a switch's table of case addresses, or a switch's
// byte table of case indices.
enum class Kind { Instruction, Opaque, Entry, Bytes };

struct Insn {
  hde64s hs;
  uint64_t at;
  size_t offset;
  size_t len;
  Kind kind;
  bool ok() const { return kind == Kind::Instruction; }
};

bool legacy_prefix(uint8_t b) {
  switch (b) {
    case 0x66: case 0x67: case 0xF2: case 0xF3: case 0xF0:
    case 0x2E: case 0x36: case 0x3E: case 0x26: case 0x64: case 0x65:
      return true;
  }
  return false;
}

// The length of a ModRM byte with its SIB and displacement.
size_t modrm_length(const uint8_t* p) {
  const uint8_t mod = p[0] >> 6, rm = p[0] & 7;
  size_t n = 1;
  if (mod != 3 && rm == 4) {
    ++n;
    if (mod == 0 && (p[1] & 7) == 5) n += 4;
  }
  if (mod == 0 && rm == 5) n += 4;
  if (mod == 1) n += 1;
  if (mod == 2) n += 4;
  return n;
}

// The length of a form HDE64 marks as an error and then loses its place in: the 0F 38 and 0F 3A
// opcode maps, VEX (C5, C4) and EVEX (62). 0 for any other instruction.
size_t extended_length(const uint8_t* p) {
  size_t i = 0;
  while (i < 14 && legacy_prefix(p[i])) ++i;
  const bool rex = (p[i] & 0xF0) == 0x40;
  if (rex) ++i;
  bool modrm = true, imm8 = false;
  if (p[i] == 0x0F && (p[i + 1] == 0x38 || p[i + 1] == 0x3A)) {
    imm8 = p[i + 1] == 0x3A;
    i += 3;
  } else if (!rex && (p[i] == 0xC5 || p[i] == 0xC4 || p[i] == 0x62)) {
    unsigned map;
    uint8_t op;
    if (p[i] == 0xC5) {
      map = 1, op = p[i + 2], i += 3;
    } else if (p[i] == 0xC4) {
      map = p[i + 1] & 0x1F, op = p[i + 3], i += 4;
    } else {
      map = p[i + 1] & 0x07, op = p[i + 4], i += 5;
    }
    if (map == 3) {
      imm8 = true;
    } else if (map == 1) {
      modrm = op != 0x77;  // vzeroupper, vzeroall
      imm8 = (op >= 0x70 && op <= 0x73) || op == 0xC2 || (op >= 0xC4 && op <= 0xC6);
    } else if (map != 2) {
      return 0;
    }
  } else {
    return 0;
  }
  if (modrm) i += modrm_length(p + i);
  return imm8 ? i + 1 : i;
}

// Decodes the instruction at offset, never reading past the function.
Insn decode_one(std::span<const uint8_t> fn, uint64_t rva, size_t offset) {
  // The decoders read past the instruction, so they read a zero-padded copy.
  uint8_t buffer[64] = {};
  const size_t n = std::min<size_t>(fn.size() - offset, 16);
  std::memcpy(buffer, fn.data() + offset, n);
  Insn in{};
  in.at = rva + offset;
  in.offset = offset;
  if (const size_t len = extended_length(buffer); len != 0 && len <= n) {
    in.kind = Kind::Opaque;
    in.len = len;
    return in;
  }
  hde64_disasm(buffer, &in.hs);
  const bool ok = !(in.hs.flags & F_ERROR) && in.hs.len != 0 && in.hs.len <= n;
  in.kind = ok ? Kind::Instruction : Kind::Opaque;
  in.len = ok ? in.hs.len : std::clamp<size_t>(in.hs.len, 1, n);
  return in;
}

size_t imm_size(const hde64s& h) {
  return (h.flags & F_IMM8 ? 1 : 0) + (h.flags & F_IMM16 ? 2 : 0) + (h.flags & F_IMM32 ? 4 : 0) +
         (h.flags & F_IMM64 ? 8 : 0);
}

// The immediate, sign-extended as the CPU does.
int64_t imm_value(const hde64s& h) {
  if (h.flags & F_IMM64) return static_cast<int64_t>(h.imm.imm64);
  if (h.flags & F_IMM32) return static_cast<int32_t>(h.imm.imm32);
  if (h.flags & F_IMM16) return static_cast<int16_t>(h.imm.imm16);
  if (h.flags & F_IMM8) return static_cast<int8_t>(h.imm.imm8);
  return 0;
}

bool rip_relative(const hde64s& h) {
  return (h.flags & F_MODRM) && h.modrm_mod == 0 && h.modrm_rm == 5 && !(h.flags & F_PREFIX_67);
}

uint64_t rip_target(const Insn& in) {
  return in.at + in.len + static_cast<int32_t>(in.hs.disp.disp32);
}

// The target of a relative branch (jcc, jmp, call, loop): the immediate is its offset.
uint64_t branch_target(const Insn& in) { return in.at + in.len + imm_value(in.hs); }

// Whether the instruction writes its r/m operand, for the encodings compiled code uses.
bool writes(const hde64s& h) {
  const uint8_t op = h.opcode, reg = h.modrm_reg;
  if (op == 0x0F) {
    const uint8_t op2 = h.opcode2;
    if (op2 >= 0x90 && op2 <= 0x9F) return true;  // setcc
    switch (op2) {
      case 0x11: case 0x13: case 0x17: case 0x29: case 0x2B:  // movups/movss/movsd, movlps, movhps, movaps, movntps
      case 0x7F: case 0xD6: case 0xE7: case 0xC3:             // movdqa/movdqu, movq, movntdq, movnti
      case 0xAB: case 0xB3: case 0xBB:                        // bts, btr, btc
      case 0xA4: case 0xA5: case 0xAC: case 0xAD:             // shld, shrd
      case 0xB0: case 0xB1: case 0xC0: case 0xC1:             // cmpxchg, xadd
        return true;
      case 0x7E: return h.p_rep != 0xF3;  // movd/movq r/m, xmm; F3 0F 7E loads
      case 0xBA: return reg >= 5;         // bts, btr, btc with imm8
      case 0xC7: return reg == 1;         // cmpxchg8b/16b
    }
    return false;
  }
  if (op < 0x40 && (op & 7) <= 1) return (op & 0x38) != 0x38;  // add..xor r/m, r; cmp only reads
  switch (op) {
    case 0x80: case 0x81: case 0x83: return reg != 7;  // the group 1 ALU ops with imm; /7 is cmp
    case 0x86: case 0x87: case 0x88: case 0x89: return true;  // xchg, mov r/m, r
    case 0x8F: case 0xC6: case 0xC7: return reg == 0;  // pop r/m, mov r/m, imm
    case 0xC0: case 0xC1: case 0xD0: case 0xD1: case 0xD2: case 0xD3: return true;  // shifts, rotates
    case 0xF6: case 0xF7: return reg == 2 || reg == 3;  // not, neg
    case 0xFE: case 0xFF: return reg <= 1;              // inc, dec
  }
  return false;
}

std::optional<Access> access(const Insn& in) {
  const hde64s& h = in.hs;
  if (!in.ok() || !(h.flags & F_MODRM) || h.modrm_mod == 3) return std::nullopt;
  if (h.modrm_mod == 0 && h.modrm_rm == 5) return std::nullopt;  // rip-relative
  if (h.opcode == 0x8D) return std::nullopt;                      // lea computes an address
  if (h.opcode == 0x0F && (h.opcode2 == 0x1F || h.opcode2 == 0x18 || h.opcode2 == 0x0D))
    return std::nullopt;  // nop and prefetch
  int base;
  if (h.modrm_rm == 4) {
    if ((h.sib_index | h.rex_x << 3) != 4) return std::nullopt;  // an index register: not a field
    if (h.sib_base == 5 && h.modrm_mod == 0) return std::nullopt;  // no base register
    base = h.sib_base | h.rex_b << 3;
  } else {
    base = h.modrm_rm | h.rex_b << 3;
  }
  if (base == 4) return std::nullopt;  // rsp: the stack frame
  Access a{};
  a.base = base;
  a.at = in.at;
  a.disp = h.modrm_mod == 1 ? static_cast<int8_t>(h.disp.disp8)
         : h.modrm_mod == 2 ? static_cast<int32_t>(h.disp.disp32)
                            : 0;
  a.store = writes(h);
  a.hasImm = a.store && (h.opcode == 0xC6 || h.opcode == 0xC7);
  a.imm = a.hasImm ? imm_value(h) : 0;
  return a;
}

constexpr uint16_t kAllRegisters = 0xFFFF;
// rax, rcx, rdx, r8 to r11: what a call may leave changed.
constexpr uint16_t kVolatile = 1 << 0 | 1 << 1 | 1 << 2 | 0x0F00;
constexpr int kRcx = 1, kRsp = 4;

// Whether the instruction's register operands are byte registers.
bool byte_operands(const hde64s& h) {
  const uint8_t op = h.opcode;
  if (op == 0x0F) return (h.opcode2 >= 0x90 && h.opcode2 <= 0x9F) || h.opcode2 == 0xB0 || h.opcode2 == 0xC0;
  if (op < 0x40) return (op & 1) == 0;
  if (op >= 0xB0 && op <= 0xB7) return true;
  switch (op) {
    case 0x80: case 0x86: case 0x88: case 0x8A: case 0xC0: case 0xC6: case 0xD0: case 0xD2: case 0xF6: case 0xFE:
      return true;
  }
  return false;
}

// The general registers an instruction may write, a bit each. A form not listed here writes them all.
uint16_t register_writes(const Insn& in) {
  if (!in.ok()) return kAllRegisters;
  const hde64s& h = in.hs;
  // A byte register without REX numbered 4 to 7 is ah, ch, dh or bh, so both readings are marked.
  const bool highBytes = byte_operands(h) && !(h.flags & F_PREFIX_REX);
  const auto bit = [&](int reg) {
    return static_cast<uint16_t>(1u << reg | (highBytes && reg >= 4 && reg < 8 ? 1u << (reg - 4) : 0u));
  };
  const uint8_t op = h.opcode, slash = h.modrm_reg;
  const bool direct = (h.flags & F_MODRM) && h.modrm_mod == 3;
  const uint16_t toReg = bit(h.modrm_reg | h.rex_r << 3);
  const uint16_t toRm = direct ? bit(h.modrm_rm | h.rex_b << 3) : 0;
  const uint16_t opReg = bit((op & 7) | h.rex_b << 3);  // the register in the opcode's low bits
  if (op == 0x0F) {
    const uint8_t op2 = h.opcode2;
    if (op2 >= 0x80 && op2 <= 0x8F) return 0;     // jcc
    if (op2 >= 0x90 && op2 <= 0x9F) return toRm;  // setcc
    if (op2 >= 0x40 && op2 <= 0x4F) return toReg;  // cmovcc
    if (op2 >= 0xC8 && op2 <= 0xCF) return bit((op2 & 7) | h.rex_b << 3);  // bswap
    switch (op2) {
      case 0x0B: case 0x0D: case 0x18: case 0x1E: case 0x1F: case 0xA3:
        return 0;  // ud2, prefetch, endbr, nop, bt
      case 0xAF: case 0xB6: case 0xB7: case 0xBE: case 0xBF:  // imul, movzx, movsx
      case 0xB8: case 0xBC: case 0xBD:                        // popcnt, bsf/tzcnt, bsr/lzcnt
      case 0x2C: case 0x2D: case 0x50: case 0xD7: case 0xC5:  // cvt to integer, movmsk, pmovmskb, pextrw
        return toReg;
      case 0xAB: case 0xB3: case 0xBB: case 0xA4: case 0xA5: case 0xAC: case 0xAD: return toRm;
      case 0xBA: return slash >= 5 ? toRm : 0;  // bts, btr, btc; bt only reads
      case 0xB0: case 0xB1: return toRm | 1;    // cmpxchg writes rax as well
      case 0xC0: case 0xC1: return toRm | toReg;  // xadd
      case 0x7E: return h.p_rep == 0xF3 ? 0 : toRm;  // movd/movq r/m, xmm; F3 0F 7E is xmm to xmm
      case 0x31: return 1 | 1 << 2;                  // rdtsc
      case 0xA2: return 0x000F;                      // cpuid
    }
    // The SSE moves and arithmetic: their ModRM names xmm registers, and they write no general one.
    if ((op2 >= 0x10 && op2 <= 0x17) || (op2 >= 0x28 && op2 <= 0x2F) || (op2 >= 0x51 && op2 <= 0x7F) ||
        op2 == 0xC2 || op2 == 0xC6 || op2 >= 0xD0)
      return 0;
    return kAllRegisters;
  }
  if (op < 0x40 && (op & 7) <= 3) {  // add..cmp in their ModRM forms
    if ((op & 0x38) == 0x38) return 0;  // cmp
    return (op & 2) ? toReg : toRm;
  }
  if (op < 0x40 && (op & 7) <= 5) return (op & 0x38) == 0x38 ? 0 : 1;  // add..cmp al/eax, imm
  if (op >= 0x50 && op <= 0x57) return 0;                              // push
  if (op >= 0x58 && op <= 0x5F) return opReg;                          // pop
  if (op >= 0x70 && op <= 0x7F) return 0;                              // jcc
  if (op >= 0x90 && op <= 0x97) return op == 0x90 && !h.rex_b ? 0 : opReg | 1;  // nop, xchg rax, r
  if (op >= 0xB0 && op <= 0xBF) return opReg;                          // mov r, imm
  if (op >= 0xD8 && op <= 0xDF) return op == 0xDF && h.modrm == 0xE0 ? 1 : 0;  // x87; fnstsw ax
  switch (op) {
    case 0x63: case 0x69: case 0x6B: case 0x8A: case 0x8B: case 0x8D: return toReg;  // movsxd, imul, mov, lea
    case 0x80: case 0x81: case 0x83: return slash == 7 ? 0 : toRm;
    case 0x68: case 0x6A: case 0x84: case 0x85: case 0x8E: case 0xA8: case 0xA9:
      return 0;  // push imm, test, mov sreg
    case 0x86: case 0x87: return toRm | toReg;
    case 0x88: case 0x89: case 0x8C: return toRm;
    case 0x8F: case 0xC0: case 0xC1: case 0xC6: case 0xC7: return toRm;  // pop r/m, shifts, mov r/m, imm
    case 0xD0: case 0xD1: case 0xD2: case 0xD3: return toRm;
    case 0x98: return 1;       // cbw, cwde, cdqe
    case 0x99: return 1 << 2;  // cwd, cdq, cqo
    case 0x9F: return 1;       // lahf
    case 0x9C: case 0x9D: case 0xC2: case 0xC3: case 0xCC: case 0xE9: case 0xEB: case 0xF4:
    case 0xF5: case 0xF8: case 0xF9: case 0xFA: case 0xFB: case 0xFC: case 0xFD:
      return 0;                // pushf, popf, ret, int3, jmp, hlt, the flag instructions
    case 0xE8: return kVolatile;  // call
    case 0xF6: case 0xF7:
      if (slash <= 1) return 0;          // test
      if (slash <= 3) return toRm;       // not, neg
      return 1 | 1 << 2;                 // mul, imul, div, idiv write rax and rdx
    case 0xFE: case 0xFF:
      if (slash <= 1) return toRm;              // inc, dec
      if (slash == 2 || slash == 3) return kVolatile;  // call
      if (slash <= 6) return 0;                 // jmp, push
      break;
  }
  return kAllRegisters;
}

uint32_t entry_at(std::span<const uint8_t> fn, size_t offset);

// How many instructions run straight from the function's start, before the first branch or return
// and before any place a branch or a switch case lands. Every call to the function runs them all, in
// order.
size_t entry_run(std::span<const uint8_t> fn, uint64_t rva, const std::vector<Insn>& code) {
  std::vector<bool> landed(fn.size() + 1);
  for (const Insn& in : code) {
    const uint64_t target = in.kind == Kind::Entry ? entry_at(fn, in.offset)
                            : in.ok() && (in.hs.flags & F_RELATIVE) ? branch_target(in)
                                                                    : ~0ull;
    if (target >= rva && target < rva + fn.size()) landed[target - rva] = true;
  }
  size_t n = 0;
  for (; n < code.size(); ++n) {
    const Insn& in = code[n];
    const hde64s& h = in.hs;
    if (!in.ok() || landed[in.offset]) break;
    if ((h.flags & F_RELATIVE) && h.opcode != 0xE8) break;                       // jcc, jmp, loop
    if (h.opcode == 0xC2 || h.opcode == 0xC3) break;                             // ret
    if (h.opcode == 0xFF && (h.modrm_reg == 4 || h.modrm_reg == 5)) break;       // jmp r/m
  }
  return n;
}

// Which registers provably hold `this` wherever the function uses them, a bit each. A register
// qualifies when every write to it is a 64-bit register copy from `this` or the pop restoring it
// where the function also pushes it, and at least one such copy is among its writes. `this` is rcx
// at entry: rcx itself with nothing writing it, or a copy of rcx made before anything writes it in
// the instructions every call runs first. Judged over the whole function, not per path.
uint16_t this_registers(std::span<const uint8_t> fn, uint64_t rva, const std::vector<Insn>& code) {
  constexpr int kEntryThis = 16;  // rcx's value at entry, as a copy's source
  uint32_t pushed = 0, otherWrite = 0;
  std::vector<std::pair<int, int>> copies;  // to, from
  for (const Insn& in : code)
    if (in.ok() && in.hs.opcode >= 0x50 && in.hs.opcode <= 0x57)
      pushed |= 1u << ((in.hs.opcode & 7) | in.hs.rex_b << 3);
  const size_t run = entry_run(fn, rva, code);
  bool rcxWritten = false;  // by anything so far, copies included
  for (size_t i = 0; i < code.size(); ++i) {
    const Insn& in = code[i];
    if (in.kind == Kind::Entry || in.kind == Kind::Bytes) continue;
    const hde64s& h = in.hs;
    uint32_t writes = register_writes(in);
    const bool writesRcx = (writes >> kRcx & 1) != 0;
    if (in.ok() && h.rex_w && (h.opcode == 0x89 || h.opcode == 0x8B) && h.modrm_mod == 3) {
      const int reg = h.modrm_reg | h.rex_r << 3, rm = h.modrm_rm | h.rex_b << 3;
      auto [to, from] = h.opcode == 0x8B ? std::pair(reg, rm) : std::pair(rm, reg);
      if (from == kRcx && i < run && !rcxWritten) from = kEntryThis;
      copies.emplace_back(to, from);
      writes &= ~(1u << to);
    } else if (in.ok() && h.opcode >= 0x58 && h.opcode <= 0x5F) {
      writes &= ~(pushed & (1u << ((h.opcode & 7) | h.rex_b << 3)));
    }
    otherWrite |= writes;
    rcxWritten = rcxWritten || writesRcx;
  }
  const uint32_t allowed = ~otherWrite & ~(1u << kRsp) & 0xFFFF;
  auto copied_from = [&](int to, uint32_t from) {
    bool any = false, all = true;
    for (const auto& [t, f] : copies)
      if (t == to) {
        any = true;
        all = all && (from >> f & 1);
      }
    return std::pair(any, all);
  };
  // Grow from `this` along copies whose sources all qualify so far. A register with a copy from one
  // that does not qualify (rcx copied back from such a register, say) is then dropped, and with it
  // whatever was copied from it.
  uint32_t holds = 1u << kEntryThis | (allowed & 1u << kRcx);
  for (bool changed = true; changed;) {
    changed = false;
    for (int r = 0; r < 16; ++r) {
      if (!(allowed >> r & 1) || (holds >> r & 1)) continue;
      const auto [any, all] = copied_from(r, holds);
      if (any && all) {
        holds |= 1u << r;
        changed = true;
      }
    }
  }
  for (bool changed = true; changed;) {
    changed = false;
    for (int r = 0; r < 16; ++r) {
      if (!(holds >> r & 1) || copied_from(r, holds).second) continue;
      holds &= ~(1u << r);
      changed = true;
    }
  }
  return static_cast<uint16_t>(holds);
}

// The X of the method's own init check, `cmp byte ptr [rip+X], 0` followed by a later
// `mov byte ptr [rip+X], 1`. Only the first such cmp among the first instructions counts.
std::optional<uint64_t> init_flag(const std::vector<Insn>& code) {
  const size_t window = std::min(code.size(), kFlagWindow);
  for (size_t i = 0; i < window; ++i) {
    const hde64s& h = code[i].hs;
    if (!code[i].ok() || h.opcode != 0x80 || h.modrm_reg != 7 || !rip_relative(h) || h.imm.imm8 != 0) continue;
    const uint64_t x = rip_target(code[i]);
    for (size_t j = i + 1; j < code.size(); ++j) {
      const hde64s& s = code[j].hs;
      if (code[j].ok() && s.opcode == 0xC6 && s.modrm_reg == 0 && rip_relative(s) && s.imm.imm8 == 1 &&
          rip_target(code[j]) == x)
        return x;
    }
    return std::nullopt;
  }
  return std::nullopt;
}

std::string hex(const uint8_t* p, size_t n) {
  std::string s;
  for (size_t i = 0; i < n; ++i) std::format_to(std::back_inserter(s), "{:02x}", p[i]);
  return s;
}

// A switch's table inside the function, found from the instruction that indexes it. MSVC reads it
// relative to the image base: `lea r, [__ImageBase]`, then `mov r32, [r + index*4 + TABLE]` for the
// case RVAs, sometimes after `movzx r32, byte ptr [r + index + BYTES]` for a byte table of case
// indices. Both lie after the function's code.
struct Table {
  size_t offset;
  bool bytes;
  size_t size = 0;
};

enum class Ref { None, Entries, Bytes, Unknown };

// Whether the instruction names in-function data by its RVA, and in which shape. A disp32 inside
// the function's own RVA range is never a field offset, so whatever the registers it is a table
// read: of case RVAs when the image base (a bit per register in imageBase) is the base of a scale-4
// read, of case indices when it is the base or the index of a scale-1 movzx, and Unknown otherwise.
// target receives the data's RVA.
Ref table_ref(const Insn& in, uint32_t imageBase, uint64_t rva, uint64_t end, uint64_t& target) {
  const hde64s& h = in.hs;
  if (!in.ok() || !(h.flags & F_MODRM) || h.modrm_mod == 3 || !(h.flags & F_DISP32) || rip_relative(h))
    return Ref::None;
  target = h.disp.disp32;
  if (target < rva || target >= end) return Ref::None;
  const bool sib = h.modrm_rm == 4;
  const bool hasBase = !(sib && h.modrm_mod == 0 && h.sib_base == 5);
  const int base = (sib ? h.sib_base : h.modrm_rm) | h.rex_b << 3;
  const int index = sib ? h.sib_index | h.rex_x << 3 : 4;
  const bool baseIsImage = hasBase && (imageBase >> base & 1);
  const bool indexIsImage = index != 4 && (imageBase >> index & 1);
  if (h.modrm_mod != 2 || index == 4) return Ref::Unknown;
  if (h.sib_scale == 2 && h.opcode == 0x8B && baseIsImage) return Ref::Entries;
  if (h.sib_scale == 0 && h.opcode == 0x0F && h.opcode2 == 0xB6 && (baseIsImage || indexIsImage)) return Ref::Bytes;
  return Ref::Unknown;
}

uint32_t entry_at(std::span<const uint8_t> fn, size_t offset) {
  uint32_t v;
  std::memcpy(&v, fn.data() + offset, 4);
  return v;
}

bool by_offset(const Table& a, const Table& b) { return a.offset < b.offset; }

// Sizes each table: case RVAs while they point into the function, then case indices while they
// index one of those. Each ends at the next table at the latest. False when a table is empty.
bool size_tables(std::span<const uint8_t> fn, uint64_t rva, std::vector<Table>& tables) {
  size_t cases = 0;
  for (const bool bytes : {false, true})
    for (size_t i = 0; i < tables.size(); ++i) {
      Table& t = tables[i];
      if (t.bytes != bytes) continue;
      const size_t limit = i + 1 < tables.size() ? tables[i + 1].offset : fn.size();
      if (!t.bytes) {
        while (t.offset + t.size + 4 <= limit && entry_at(fn, t.offset + t.size) - rva < fn.size()) t.size += 4;
        cases = std::max(cases, t.size / 4);
      } else {
        while (t.offset + t.size < limit && fn[t.offset + t.size] < cases) ++t.size;
      }
      if (t.size == 0) return false;
    }
  return true;
}

// Decodes the function, reading the sized tables (sorted by offset) as data. desync is set when an
// instruction runs into a table.
std::vector<Insn> decode(std::span<const uint8_t> fn, uint64_t rva, const std::vector<Table>& tables, bool& desync) {
  std::vector<Insn> code;
  size_t next = 0;
  for (size_t offset = 0; offset < fn.size();) {
    while (next < tables.size() && tables[next].offset < offset) ++next;
    if (next < tables.size() && tables[next].offset == offset) {
      const Table& t = tables[next++];
      const size_t step = t.bytes ? t.size : 4;
      for (size_t at = offset; at < offset + t.size; at += step) {
        Insn d{};
        d.at = rva + at;
        d.offset = at;
        d.len = step;
        d.kind = t.bytes ? Kind::Bytes : Kind::Entry;
        code.push_back(d);
      }
      offset += t.size;
      continue;
    }
    const Insn in = decode_one(fn, rva, offset);
    if (next < tables.size() && tables[next].offset < offset + in.len) desync = true;
    code.push_back(in);
    offset += in.len;
  }
  return code;
}

// A decoding whose tables agree with the instructions that read them.
struct Layout {
  std::vector<Insn> code;
  std::vector<std::pair<size_t, uint64_t>> refs;  // each table-reading instruction and its table's RVA
  bool sure = false;  // settled, in sync, and every in-function read is of a known table shape
};

Layout lay_out(std::span<const uint8_t> fn, uint64_t rva) {
  const uint64_t end = rva + fn.size();
  // Decoding finds the tables and the tables change the decoding, so it repeats until both agree.
  std::vector<Table> tables;
  for (int round = 0; round < 4; ++round) {
    Layout out;
    bool desync = false, unknown = false;
    out.code = decode(fn, rva, tables, desync);
    std::vector<Table> found;
    uint32_t imageBase = 0;
    for (size_t i = 0; i < out.code.size(); ++i) {
      const Insn& in = out.code[i];
      if (in.ok() && in.hs.opcode == 0x8D && rip_relative(in.hs) && rip_target(in) == 0)
        imageBase |= 1u << (in.hs.modrm_reg | in.hs.rex_r << 3);  // lea r, [__ImageBase]
      uint64_t target = 0;
      const Ref ref = table_ref(in, imageBase, rva, end, target);
      if (ref == Ref::None) continue;
      if (ref == Ref::Unknown) {
        unknown = true;
        continue;
      }
      out.refs.emplace_back(i, target);
      const Table t{static_cast<size_t>(target - rva), ref == Ref::Bytes};
      const auto same = std::find_if(found.begin(), found.end(), [&](const Table& f) { return f.offset == t.offset; });
      if (same == found.end()) found.push_back(t);
      else if (same->bytes != t.bytes) unknown = true;
    }
    std::sort(found.begin(), found.end(), by_offset);
    const bool settled = std::equal(found.begin(), found.end(), tables.begin(), tables.end(),
                                    [](const Table& a, const Table& b) {
                                      return a.offset == b.offset && a.bytes == b.bytes;
                                    });
    if (settled) {
      out.sure = !desync && !unknown;
      return out;
    }
    tables = std::move(found);
    if (!size_tables(fn, rva, tables)) {
      out.sure = false;
      return out;
    }
  }
  Layout out;
  bool desync = false;
  out.code = decode(fn, rva, tables, desync);
  return out;
}

}  // namespace

CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva) {
  CodeFacts facts;
  const Layout layout = lay_out(fn, rva);
  const std::vector<Insn>& code = layout.code;
  const std::optional<uint64_t> flag = init_flag(code);
  const uint64_t end = rva + fn.size();
  auto inside = [&](uint64_t target) { return target >= rva && target < end; };
  // An address inside the function as an offset from its start, any other as itself.
  auto place = [&](uint64_t target) {
    return inside(target) ? std::format("+{:x}", target - rva) : std::format("@{:x}", target);
  };
  // Every in-function branch and case must land where an instruction begins.
  std::vector<bool> starts(fn.size());
  for (const Insn& in : code) {
    if (in.kind != Kind::Instruction && in.kind != Kind::Opaque) continue;
    starts[in.offset] = true;
    facts.starts.push_back(in.at);
  }
  bool complete = layout.sure;
  auto lands = [&](uint64_t target) { complete = complete && inside(target) && starts[target - rva]; };

  for (size_t i = 0; i < code.size(); ++i) {
    const Insn& in = code[i];
    const hde64s& h = in.hs;
    const uint8_t* p = fn.data() + in.offset;
    if (in.kind == Kind::Entry) {
      const uint64_t target = entry_at(fn, in.offset);
      lands(target);
      facts.normalized.push_back("dd " + place(target));
      continue;
    }
    if (in.kind == Kind::Bytes) {
      facts.normalized.push_back("db " + hex(p, in.len));
      continue;
    }
    if (in.kind == Kind::Opaque) {
      complete = false;
      facts.normalized.push_back("?" + hex(p, in.len));
      continue;
    }
    if (const auto a = access(in)) facts.accesses.push_back(*a);
    const size_t imm = imm_size(h);
    const auto ref = std::find_if(layout.refs.begin(), layout.refs.end(), [&](const auto& r) { return r.first == i; });
    const bool readsTable = ref != layout.refs.end();
    if (h.flags & F_RELATIVE) {
      const uint64_t target = branch_target(in);
      if ((h.opcode == 0xE8 || h.opcode == 0xE9) && (h.flags & F_IMM32) && !inside(target))
        facts.callTargets.push_back(target);
      if (inside(target)) lands(target);
      facts.normalized.push_back(hex(p, in.len - imm) + " " + place(target));
    } else if (rip_relative(h) || readsTable) {
      // The 32-bit displacement names an address: rip-relative, or a table's RVA.
      const uint64_t target = readsTable ? ref->second : rip_target(in);
      // In-function data read rip-relative is in no table this reader knows.
      if (!readsTable && inside(target) && h.opcode != 0x8D) complete = false;
      const size_t head = in.len - imm - 4;
      std::string token = hex(p, head) + " " + (target == flag ? std::string("FLAG") : place(target));
      if (imm) token += " " + hex(p + head + 4, imm);
      facts.normalized.push_back(std::move(token));
    } else {
      facts.normalized.push_back(hex(p, in.len));
    }
  }
  const uint16_t self = this_registers(fn, rva, code);
  for (Access& a : facts.accesses) a.thisBase = (self >> a.base & 1) != 0;
  facts.complete = complete;
  return facts;
}

}  // namespace pg::sdk
