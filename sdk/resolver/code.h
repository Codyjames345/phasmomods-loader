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
};

struct CodeFacts {
  // Absolute targets of `call rel32`, and of `jmp rel32` leaving the function, in code order.
  std::vector<uint64_t> callTargets;
  std::vector<Access> accesses;
  // One token per instruction, the same for two bodies wherever they sit: rip-relative operands
  // become their absolute address, in-function branch targets become offsets from the start, and
  // the method's own runtime-metadata init flag becomes FLAG.
  std::vector<std::string> normalized;
};

// Decodes the function `fn` placed at `rva`. The init flag is the X of the first
// `cmp byte ptr [rip+X], 0` among the first six instructions, when a later `mov byte ptr [rip+X], 1`
// sets it. An instruction that does not decode becomes an opaque token and gives no facts.
CodeFacts read_code(std::span<const uint8_t> fn, uint64_t rva);

}  // namespace pg::sdk
