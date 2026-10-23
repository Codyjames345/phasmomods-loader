#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <vector>

namespace pg::sdk {

// A PE32+ image laid out by RVA, as a loaded module is, and the bounds of its functions from the
// exception directory's RUNTIME_FUNCTION table. It refers to the bytes, which must outlive it.
class CodeImage {
 public:
  explicit CodeImage(std::span<const uint8_t> mapped);

  // The headers parsed and the exception directory lies inside the image.
  bool valid() const { return valid_; }

  // The whole function containing rva. A chunk chained to another entry (UNW_FLAG_CHAININFO) belongs
  // to its primary function, and the span runs from the primary's start to the end of the last of
  // its chunks contiguous with it. Empty when rva is in no function, or when its chain is broken
  // (it leaves the image or the table, or loops).
  std::span<const uint8_t> function(uint64_t rva) const;
  // The start of that function, or 0 when there is none.
  uint64_t function_start(uint64_t rva) const;

 private:
  struct Chunk {
    uint32_t begin, end, unwind;
  };
  const Chunk* chunk_at(uint64_t rva) const;
  const Chunk* chunk_starting(uint32_t begin) const;
  // The begin of the primary entry the chunk's chain of unwind info ends at.
  std::optional<uint32_t> primary(Chunk chunk) const;
  // The primary function's own chunk, for rva.
  const Chunk* primary_chunk(uint64_t rva) const;

  std::span<const uint8_t> image_;
  std::vector<Chunk> chunks_;  // sorted by begin
  bool valid_ = false;
};

// Reads a PE32+ file and lays it out by RVA, as the loader would, for the offline check. Empty when
// the file cannot be read or any header or section lies outside the file or the image.
std::vector<uint8_t> map_pe_file(const std::filesystem::path& file);

}  // namespace pg::sdk
