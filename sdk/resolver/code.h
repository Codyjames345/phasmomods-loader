#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace pg::sdk {

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
};

struct CodeFacts {
  // Absolute targets of `call rel32`, and of `jmp rel32` leaving the function, in code order.
  std::vector<uint64_t> callTargets;
  std::vector<Access> accesses;
  // One token per instruction, the same for two bodies wherever they sit: rip-relative operands
  // become their absolute address, in-function branch targets become offsets from the start, and
  // the method's own runtime-metadata init flag becomes FLAG.
  std::vector<std::string> normalized;
  // False when the reader could not account for every byte: an instruction it cannot decode or whose
  // effect it does not know (VEX, 0F 38, 0F 3A), a desync, in-function data of an unknown shape, or a
  // branch or case that lands inside an instruction. The facts above are then partial: callers must
  // not pick a candidate on them, nor compare twins through them.
  bool complete = true;
};

// Decodes the function `fn` placed at `rva`. The init flag is the X of the first
// `cmp byte ptr [rip+X], 0` among the first six instructions, when a later `mov byte ptr [rip+X], 1`
// sets it. A switch's tables inside the function (MSVC's case RVAs and byte case indices, read
// relative to the image base) are read as data: `dd +off` per case and one `db` token per byte
// table, and the instructions that index them name them as `+off`. An instruction that does not
// decode becomes an opaque `?` token, gives no facts, and makes the facts incomplete.
CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva);

}  // namespace pg::sdk
