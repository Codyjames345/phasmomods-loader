#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <vector>

// Builds a small PE32+ image for the code reader's tests, laid out by RVA as a loaded module is: DOS
// and NT headers, one section spanning the image, unwind info and an exception directory (the
// RUNTIME_FUNCTION table). file() gives the same image in file layout, with the section at another
// file offset than its RVA.
struct ImageBuilder {
  struct RuntimeFunction {
    uint32_t begin, end, unwind;
  };

  static constexpr uint32_t kSize = 0x10000;
  static constexpr uint32_t kHeaders = 0x400;   // SizeOfHeaders, and the section's file offset
  static constexpr uint32_t kSection = 0x1000;  // the section's RVA
  static constexpr uint32_t kUnwind = 0xA000;   // where unwind info is placed
  static constexpr uint32_t kTable = 0xC000;    // where the RUNTIME_FUNCTION table is placed

  std::vector<uint8_t> image = std::vector<uint8_t>(kSize);
  std::vector<RuntimeFunction> table;
  uint32_t nextUnwind = kUnwind;

  template <typename T>
  void put(uint32_t rva, T value) {
    std::memcpy(image.data() + rva, &value, sizeof(T));
  }

  void code(uint32_t rva, std::span<const uint8_t> bytes) {
    std::copy(bytes.begin(), bytes.end(), image.begin() + rva);
  }

  // Unwind info with `codes` unwind codes; with a parent, it is chained (UNW_FLAG_CHAININFO) and
  // ends with the parent's RUNTIME_FUNCTION. Returns its RVA.
  uint32_t unwind(std::optional<RuntimeFunction> parent = {}, uint8_t codes = 0) {
    const uint32_t rva = nextUnwind;
    image[rva] = static_cast<uint8_t>(1 | (parent ? 0x4 << 3 : 0));  // version 1, flags
    image[rva + 2] = codes;
    const uint32_t slots = (codes + 1u) & ~1u;  // the codes array is padded to an even count
    if (parent) put(rva + 4 + slots * 2, *parent);
    nextUnwind = rva + 4 + slots * 2 + 12;
    nextUnwind = (nextUnwind + 3) & ~3u;
    return rva;
  }

  void entry(uint32_t begin, uint32_t end, uint32_t unwindRva) { table.push_back({begin, end, unwindRva}); }

  // A function of its own: code at rva and an unchained entry covering it.
  void function(uint32_t rva, std::span<const uint8_t> bytes) {
    code(rva, bytes);
    entry(rva, rva + static_cast<uint32_t>(bytes.size()), unwind());
  }

  // A chunk chained to the entry that begins at parentBegin.
  void chunk(uint32_t rva, std::span<const uint8_t> bytes, uint32_t parentBegin, uint8_t codes = 0) {
    RuntimeFunction parent{};
    for (const RuntimeFunction& f : table)
      if (f.begin == parentBegin) parent = f;
    code(rva, bytes);
    entry(rva, rva + static_cast<uint32_t>(bytes.size()), unwind(parent, codes));
  }

  // The image laid out by RVA.
  std::vector<uint8_t> build() const {
    std::vector<uint8_t> out = image;
    auto put32 = [&](uint32_t at, uint32_t v) { std::memcpy(out.data() + at, &v, 4); };
    auto put16 = [&](uint32_t at, uint16_t v) { std::memcpy(out.data() + at, &v, 2); };
    put16(0, 0x5A4D);  // MZ
    put32(0x3C, 0x40);  // e_lfanew
    put32(0x40, 0x4550);  // PE\0\0
    put16(0x44, 0x8664);  // Machine: x64
    put16(0x46, 1);       // NumberOfSections
    put16(0x54, 0xF0);    // SizeOfOptionalHeader
    const uint32_t opt = 0x58;
    put16(opt, 0x20B);  // PE32+
    put32(opt + 56, kSize);
    put32(opt + 60, kHeaders);
    put32(opt + 108, 16);  // NumberOfRvaAndSizes
    std::vector<RuntimeFunction> sorted = table;
    std::sort(sorted.begin(), sorted.end(), [](auto& a, auto& b) { return a.begin < b.begin; });
    put32(opt + 112 + 3 * 8, kTable);  // the exception directory
    put32(opt + 112 + 3 * 8 + 4, static_cast<uint32_t>(sorted.size() * 12));
    for (size_t i = 0; i < sorted.size(); ++i)
      std::memcpy(out.data() + kTable + i * 12, &sorted[i], 12);
    const uint32_t sec = opt + 0xF0;
    std::memcpy(out.data() + sec, ".text", 5);
    put32(sec + 8, kSize - kSection);   // VirtualSize
    put32(sec + 12, kSection);          // VirtualAddress
    put32(sec + 16, kSize - kSection);  // SizeOfRawData
    put32(sec + 20, kHeaders);          // PointerToRawData
    return out;
  }

  // The same image as it lies in a file.
  std::vector<uint8_t> file() const {
    const std::vector<uint8_t> mapped = build();
    std::vector<uint8_t> out(kHeaders + kSize - kSection);
    std::copy(mapped.begin(), mapped.begin() + kHeaders, out.begin());
    std::copy(mapped.begin() + kSection, mapped.end(), out.begin() + kHeaders);
    return out;
  }
};
