#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pm::sdk {

// A memory operand [reg + disp] whose base is neither rip nor rsp and which has no index register:
// how a method reaches a field of an object. `store` when the instruction writes the operand, and
// `hasImm` when what it writes is the constant `imm` (mov r/m, imm). lea and nop take no access.
struct Access {
  uint64_t at;  // the instruction's rva
  int32_t disp;
  bool store;
  bool hasImm;
  int64_t imm;
  int base;  // the base register, 0 (rax) to 15 (r15)
  // The base provably holds `this` throughout the function: rcx when nothing in the function writes
  // it (a call writes the volatile registers), or a register whose every write is a 64-bit copy of
  // `this` or the pop restoring it where the function pushed it. A copy of rcx counts when no write to
  // rcx comes before it in the instructions every call runs first, as in a prologue's mov rdi, rcx.
  // Judged over the whole function, not per path, so it errs towards false.
  bool thisBase;
  // The base was last written, in straight-line code with no branch landing between, by a load from a
  // fixed address (mov r64, [rip+X]): a runtime metadata slot (a class, as the class-initialised check
  // `mov rcx, [rip+X]; cmp dword [rcx+0E0h], 0` reads it), never an object of the game's classes.
  bool global = false;
  // When the base is not this: the field loads that gave it its value, in straight-line code with no
  // branch landing between, starting from this ({0x10, 0x60}: the base is [[this+0x10]+0x60]). Empty
  // when it came from anywhere else.
  std::vector<int32_t> chain;
};

struct CodeFacts {
  // Absolute targets of `call rel32`, and of `jmp rel32` leaving the function, in code order.
  std::vector<uint64_t> callTargets;
  std::vector<Access> accesses;
  // Every `cmp` with an immediate, in code order: the immediate as the comparison reads it
  // (sign-extended to the operand's width, then to 64 bits) and the operand's width in bits.
  struct Compare {
    int64_t imm;
    int bits;
    // Whether the comparison is with the number `value`: `value` fits the operand's width, signed or
    // unsigned, and equals the immediate in its low `bits` (a byte compare with 0x80 is a compare with
    // 128 or -128, never with 384; a dword compare with -1 is one with 0xFFFFFFFF).
    bool is(int64_t value) const {
      if (bits >= 64) return imm == value;
      const int64_t half = int64_t{1} << (bits - 1);
      const uint64_t mask = (uint64_t{1} << bits) - 1;
      const bool fits = value >= -half && value <= static_cast<int64_t>(mask);
      return fits && (static_cast<uint64_t>(imm) & mask) == (static_cast<uint64_t>(value) & mask);
    }
  };
  std::vector<Compare> compares;
  // Every `mov` of a 32-bit immediate into a register or a place (B8+r, C7 /0), in code order.
  std::vector<int64_t> moves;
  // One token per instruction, the same for two bodies wherever they sit: rip-relative operands
  // become their absolute address, in-function branch targets become offsets from the start, and
  // the method's own runtime-metadata init flag becomes FLAG.
  std::vector<std::string> normalized;
  // The rva where each instruction begins, in code order; a switch table's data is no instruction.
  std::vector<uint64_t> starts;
  // False when the reader could not account for every byte: an instruction it cannot decode or whose
  // effect it does not know (VEX, 0F 38, 0F 3A), a desync, in-function data of an unknown shape, or a
  // branch or case that lands inside an instruction. The facts above are then partial: callers must
  // not pick a candidate on them, nor compare twins through them.
  bool complete = true;
  // Whether the code gives control back: a ret, a jump leaving the function, or an indirect jump. A function it
  // is false for never returns, as the runtime's stub for a method with no code raises and stops.
  bool returns = false;
};

// Decodes the function `fn` placed at `rva`. The init flag is the X of the first
// `cmp byte ptr [rip+X], 0` among the first six instructions, when a later `mov byte ptr [rip+X], 1`
// sets it. A switch's tables inside the function (MSVC's case RVAs and byte case indices, read
// relative to the image base) are read as data: `dd +off` per case and one `db` token per byte
// table, and the instructions that index them name them as `+off`. An instruction that does not
// decode becomes an opaque `?` token, gives no facts, and makes the facts incomplete.
CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva);

// Decodes a leaf function at `rva` (one the exception table does not list, which calls nothing and
// leaves the stack as it found it), `code` being the bytes from there on: its instructions up to the
// ret or the jmp rel32 that ends it, that jump's target being its one call target. Complete only when
// it reaches that end with every instruction decoded and straight-line: a branch, a call, an indirect
// call or jump, or a breakpoint before the end leaves it incomplete. Only callTargets, starts, complete and
// returns are given. It is stricter than read_code on returns, on purpose: only a ret counts, so a leaf that ends in a
// jump, which it cannot follow, says false, and a caller asking whether code comes back fails closed on it.
CodeFacts read_leaf(std::span<const uint8_t> code, uint64_t rva);

}  // namespace pm::sdk
