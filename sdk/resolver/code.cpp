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

struct Insn {
  hde64s hs;
  uint64_t at;
  size_t offset;
  size_t len;
  bool ok;  // decoded without error; otherwise len only keeps the decoder moving
};

std::vector<Insn> decode(std::span<const uint8_t> fn, uint64_t rva) {
  std::vector<Insn> code;
  for (size_t offset = 0; offset < fn.size();) {
    // HDE reads past the instruction, so it decodes a zero-padded copy, never past the function.
    uint8_t buffer[64] = {};
    const size_t n = std::min<size_t>(fn.size() - offset, 16);
    std::memcpy(buffer, fn.data() + offset, n);
    Insn in{};
    in.at = rva + offset;
    in.offset = offset;
    hde64_disasm(buffer, &in.hs);
    in.ok = !(in.hs.flags & F_ERROR) && in.hs.len != 0 && in.hs.len <= n;
    in.len = in.ok ? in.hs.len : std::clamp<size_t>(in.hs.len, 1, n);
    code.push_back(in);
    offset += in.len;
  }
  return code;
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
  if (!in.ok || !(h.flags & F_MODRM) || h.modrm_mod == 3) return std::nullopt;
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
  a.at = in.at;
  a.disp = h.modrm_mod == 1 ? static_cast<int8_t>(h.disp.disp8)
         : h.modrm_mod == 2 ? static_cast<int32_t>(h.disp.disp32)
                            : 0;
  a.store = writes(h);
  a.hasImm = a.store && (h.opcode == 0xC6 || h.opcode == 0xC7);
  a.imm = a.hasImm ? imm_value(h) : 0;
  return a;
}

// The X of the method's own init check, `cmp byte ptr [rip+X], 0` followed by a later
// `mov byte ptr [rip+X], 1`. Only the first such cmp among the first instructions counts.
std::optional<uint64_t> init_flag(const std::vector<Insn>& code) {
  const size_t window = std::min(code.size(), kFlagWindow);
  for (size_t i = 0; i < window; ++i) {
    const hde64s& h = code[i].hs;
    if (!code[i].ok || h.opcode != 0x80 || h.modrm_reg != 7 || !rip_relative(h) || h.imm.imm8 != 0) continue;
    const uint64_t x = rip_target(code[i]);
    for (size_t j = i + 1; j < code.size(); ++j) {
      const hde64s& s = code[j].hs;
      if (code[j].ok && s.opcode == 0xC6 && s.modrm_reg == 0 && rip_relative(s) && s.imm.imm8 == 1 &&
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

}  // namespace

CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva) {
  CodeFacts facts;
  const std::vector<Insn> code = decode(fn, rva);
  const std::optional<uint64_t> flag = init_flag(code);
  const uint64_t end = rva + fn.size();
  auto inside = [&](uint64_t target) { return target >= rva && target < end; };
  // An address inside the function as an offset from its start, any other as itself.
  auto place = [&](uint64_t target) {
    return inside(target) ? std::format("+{:x}", target - rva) : std::format("@{:x}", target);
  };

  for (const Insn& in : code) {
    const hde64s& h = in.hs;
    const uint8_t* p = fn.data() + in.offset;
    if (!in.ok) {
      facts.normalized.push_back("?" + hex(p, in.len));
      continue;
    }
    if (const auto a = access(in)) facts.accesses.push_back(*a);
    const size_t imm = imm_size(h);
    if (h.flags & F_RELATIVE) {
      const uint64_t target = branch_target(in);
      if ((h.opcode == 0xE8 || h.opcode == 0xE9) && (h.flags & F_IMM32) && !inside(target))
        facts.callTargets.push_back(target);
      facts.normalized.push_back(hex(p, in.len - imm) + " " + place(target));
    } else if (rip_relative(h)) {
      const uint64_t target = rip_target(in);
      const size_t head = in.len - imm - 4;
      std::string token = hex(p, head) + " " + (target == flag ? std::string("FLAG") : place(target));
      if (imm) token += " " + hex(p + head + 4, imm);
      facts.normalized.push_back(std::move(token));
    } else {
      facts.normalized.push_back(hex(p, in.len));
    }
  }
  return facts;
}

}  // namespace pg::sdk
