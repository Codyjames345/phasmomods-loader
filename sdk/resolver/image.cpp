#include "image.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace pm::sdk {
namespace {

constexpr uint32_t kChainInfo = 0x4;     // UNW_FLAG_CHAININFO
constexpr int kMaxChain = 32;            // deeper chains are treated as broken
constexpr uint64_t kMaxImage = 1u << 31;  // a larger SizeOfImage is a malformed header

bool in_bounds(std::span<const uint8_t> b, uint64_t offset, uint64_t size) {
  return offset <= b.size() && size <= b.size() - offset;
}

template <typename T>
std::optional<T> read(std::span<const uint8_t> b, uint64_t offset) {
  if (!in_bounds(b, offset, sizeof(T))) return std::nullopt;
  T value;
  std::memcpy(&value, b.data() + offset, sizeof(T));
  return value;
}

// What the image and the file need from the DOS, NT and optional headers. Offsets are those of the
// headers' fields in a PE32+ image.
struct Headers {
  uint32_t sizeOfImage = 0, sizeOfHeaders = 0;
  uint32_t exceptionRva = 0, exceptionSize = 0;
  uint32_t exportRva = 0, exportSize = 0;
  uint64_t sections = 0;  // offset of the section table
  uint16_t sectionCount = 0;
};

std::optional<Headers> parse_headers(std::span<const uint8_t> b) {
  if (read<uint16_t>(b, 0) != 0x5A4D) return std::nullopt;  // MZ
  const auto lfanew = read<uint32_t>(b, 0x3C);
  if (!lfanew || read<uint32_t>(b, *lfanew) != 0x4550u) return std::nullopt;  // PE\0\0
  const uint64_t nt = *lfanew;
  const auto sectionCount = read<uint16_t>(b, nt + 6);
  const auto optionalSize = read<uint16_t>(b, nt + 20);
  const uint64_t opt = nt + 24;
  if (!sectionCount || !optionalSize || read<uint16_t>(b, opt) != 0x20B) return std::nullopt;  // PE32+
  const auto dirCount = read<uint32_t>(b, opt + 108);
  if (!dirCount || *optionalSize < 112 + uint64_t(*dirCount) * 8) return std::nullopt;
  Headers h;
  h.sizeOfImage = read<uint32_t>(b, opt + 56).value_or(0);
  h.sizeOfHeaders = read<uint32_t>(b, opt + 60).value_or(0);
  h.exportRva = read<uint32_t>(b, opt + 112).value_or(0);  // the export directory is entry 0
  h.exportSize = read<uint32_t>(b, opt + 112 + 4).value_or(0);
  if (*dirCount > 3) {  // the exception directory is entry 3
    h.exceptionRva = read<uint32_t>(b, opt + 112 + 3 * 8).value_or(0);
    h.exceptionSize = read<uint32_t>(b, opt + 112 + 3 * 8 + 4).value_or(0);
  }
  h.sections = opt + *optionalSize;
  h.sectionCount = *sectionCount;
  if (!in_bounds(b, h.sections, uint64_t(h.sectionCount) * 40)) return std::nullopt;
  return h;
}

}  // namespace

CodeImage::CodeImage(std::span<const uint8_t> mapped) {
  const auto h = parse_headers(mapped);
  if (!h || !in_bounds(mapped, h->exceptionRva, h->exceptionSize)) return;
  for (uint64_t at = h->exceptionRva; at + 12 <= uint64_t(h->exceptionRva) + h->exceptionSize; at += 12) {
    const Chunk c = *read<Chunk>(mapped, at);
    if (c.begin < c.end && c.end <= mapped.size()) chunks_.push_back(c);
  }
  std::sort(chunks_.begin(), chunks_.end(), [](const Chunk& a, const Chunk& b) { return a.begin < b.begin; });
  // IMAGE_EXPORT_DIRECTORY is 40 bytes.
  if (h->exportSize >= 40 && in_bounds(mapped, h->exportRva, h->exportSize)) {
    exportRva_ = h->exportRva;
    exportSize_ = h->exportSize;
  }
  // IMAGE_SECTION_HEADER: the name in 8 bytes, NUL-padded, then VirtualSize and VirtualAddress.
  for (uint16_t i = 0; i < h->sectionCount; ++i) {
    const uint64_t s = h->sections + uint64_t(i) * 40;
    const char* name = reinterpret_cast<const char*>(mapped.data() + s);
    sections_.push_back({std::string(name, strnlen(name, 8)), *read<uint32_t>(mapped, s + 12),
                         *read<uint32_t>(mapped, s + 8)});
  }
  image_ = mapped;
  valid_ = true;
}

uint64_t CodeImage::exported(std::string_view name) const {
  if (exportSize_ == 0) return 0;
  // IMAGE_EXPORT_DIRECTORY: NumberOfFunctions at 20, NumberOfNames at 24, then the RVAs of the
  // function table, the name table and the name ordinals at 28, 32 and 36.
  const auto functions = read<uint32_t>(image_, exportRva_ + 20);
  const auto names = read<uint32_t>(image_, exportRva_ + 24);
  const auto functionTable = read<uint32_t>(image_, exportRva_ + 28);
  const auto nameTable = read<uint32_t>(image_, exportRva_ + 32);
  const auto ordinalTable = read<uint32_t>(image_, exportRva_ + 36);
  if (!functions || !names || !functionTable || !nameTable || !ordinalTable) return 0;
  uint64_t found = 0;
  int matches = 0;
  for (uint32_t i = 0; i < *names; ++i) {
    const auto text = read<uint32_t>(image_, *nameTable + uint64_t(i) * 4);
    if (!text) return 0;  // the name table leaves the image
    if (!in_bounds(image_, *text, name.size() + 1) || image_[*text + name.size()] != 0 ||
        std::memcmp(image_.data() + *text, name.data(), name.size()) != 0)
      continue;
    const auto ordinal = read<uint16_t>(image_, *ordinalTable + uint64_t(i) * 2);
    if (!ordinal || *ordinal >= *functions) return 0;
    const auto rva = read<uint32_t>(image_, *functionTable + uint64_t(*ordinal) * 4);
    if (!rva) return 0;
    found = *rva;
    ++matches;
  }
  // A forwarder's RVA names its text inside the export directory, not code.
  const bool forwarded = found >= exportRva_ && found < uint64_t(exportRva_) + exportSize_;
  if (matches != 1 || found == 0 || found >= image_.size() || forwarded) return 0;
  return found;
}

std::span<const uint8_t> CodeImage::section(std::string_view name, uint64_t& rva) const {
  const Section* found = nullptr;
  for (const Section& s : sections_) {
    if (s.name != name) continue;
    if (found) return {};
    found = &s;
  }
  if (!found || found->size == 0 || !in_bounds(image_, found->rva, found->size)) return {};
  rva = found->rva;
  return image_.subspan(found->rva, found->size);
}

const CodeImage::Chunk* CodeImage::chunk_at(uint64_t rva) const {
  auto it = std::upper_bound(chunks_.begin(), chunks_.end(), rva,
                             [](uint64_t r, const Chunk& c) { return r < c.begin; });
  if (it == chunks_.begin()) return nullptr;
  --it;
  return rva < it->end ? &*it : nullptr;
}

const CodeImage::Chunk* CodeImage::chunk_starting(uint32_t begin) const {
  auto it = std::lower_bound(chunks_.begin(), chunks_.end(), begin,
                             [](const Chunk& c, uint32_t b) { return c.begin < b; });
  return it != chunks_.end() && it->begin == begin ? &*it : nullptr;
}

std::optional<uint32_t> CodeImage::primary(Chunk chunk) const {
  for (int depth = 0; depth < kMaxChain; ++depth) {
    if (chunk.unwind & 1) {  // the unwind data is itself the parent's RUNTIME_FUNCTION
      const auto parent = read<Chunk>(image_, chunk.unwind & ~1u);
      if (!parent) return std::nullopt;
      chunk = *parent;
      continue;
    }
    // UNWIND_INFO: version and flags, prologue size, count of codes, frame register; then the codes,
    // padded to an even count, and with UNW_FLAG_CHAININFO the parent's RUNTIME_FUNCTION.
    const auto header = read<uint32_t>(image_, chunk.unwind);
    if (!header) return std::nullopt;
    const uint32_t flags = (*header & 0xFF) >> 3;
    const uint32_t codes = (*header >> 16) & 0xFF;
    if (!(flags & kChainInfo)) return chunk.begin;
    const auto parent = read<Chunk>(image_, uint64_t(chunk.unwind) + 4 + ((codes + 1) & ~1u) * 2);
    if (!parent) return std::nullopt;
    chunk = *parent;
  }
  return std::nullopt;
}

const CodeImage::Chunk* CodeImage::primary_chunk(uint64_t rva) const {
  const Chunk* c = chunk_at(rva);
  if (!c) return nullptr;
  const auto begin = primary(*c);
  return begin ? chunk_starting(*begin) : nullptr;
}

std::span<const uint8_t> CodeImage::function(uint64_t rva) const {
  const Chunk* first = primary_chunk(rva);
  if (!first) return {};
  uint32_t end = first->end;
  const Chunk* const last = chunks_.data() + chunks_.size();
  for (const Chunk* next = first + 1; next != last && next->begin == end && primary(*next) == first->begin; ++next)
    end = next->end;
  return image_.subspan(first->begin, end - first->begin);
}

std::span<const uint8_t> CodeImage::leaf(uint64_t rva) const {
  if (rva >= image_.size() || chunk_at(rva)) return {};
  auto next = std::upper_bound(chunks_.begin(), chunks_.end(), rva,
                               [](uint64_t r, const Chunk& c) { return r < c.begin; });
  uint64_t end = std::min<uint64_t>(rva + kMaxLeaf, image_.size());
  if (next != chunks_.end()) end = std::min<uint64_t>(end, next->begin);
  return image_.subspan(rva, end - rva);
}

uint64_t CodeImage::function_start(uint64_t rva) const {
  const Chunk* first = primary_chunk(rva);
  return first ? first->begin : 0;
}

std::vector<uint8_t> map_pe_file(const std::filesystem::path& file) {
  std::ifstream in(file, std::ios::binary | std::ios::ate);
  if (!in) return {};
  const std::streamoff size = in.tellg();
  if (size <= 0) return {};
  std::vector<uint8_t> raw(static_cast<size_t>(size));
  in.seekg(0);
  if (!in.read(reinterpret_cast<char*>(raw.data()), size)) return {};

  const auto h = parse_headers(raw);
  if (!h || h->sizeOfImage == 0 || h->sizeOfImage > kMaxImage || h->sizeOfHeaders > h->sizeOfImage ||
      h->sizeOfHeaders > raw.size())
    return {};
  std::vector<uint8_t> image(h->sizeOfImage);
  std::copy_n(raw.begin(), h->sizeOfHeaders, image.begin());
  for (uint16_t i = 0; i < h->sectionCount; ++i) {
    const uint64_t s = h->sections + uint64_t(i) * 40;
    const uint32_t virtualSize = *read<uint32_t>(raw, s + 8);
    const uint32_t virtualAddress = *read<uint32_t>(raw, s + 12);
    const uint32_t rawSize = *read<uint32_t>(raw, s + 16);
    const uint32_t rawPointer = *read<uint32_t>(raw, s + 20);
    const uint32_t n = virtualSize ? std::min(virtualSize, rawSize) : rawSize;
    if (n == 0) continue;
    if (!in_bounds(raw, rawPointer, n) || !in_bounds(image, virtualAddress, n)) return {};
    std::copy_n(raw.begin() + rawPointer, n, image.begin() + virtualAddress);
  }
  return image;
}

}  // namespace pm::sdk
