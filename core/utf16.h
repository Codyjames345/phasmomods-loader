#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace pg {

// Converts UTF-16 text (a managed string's characters) to NUL-terminated UTF-8 in `out`, `size`
// bytes including the NUL. A surrogate pair becomes one character and an unpaired surrogate becomes
// U+FFFD. Text that does not fit is cut at a character boundary. Returns the length in bytes of the
// whole text in UTF-8, without the NUL, so a caller can size its buffer; `out` may be null when
// `size` is 0. Any thread.
inline size_t utf16_to_utf8(std::u16string_view text, char* out, size_t size) {
  size_t total = 0;    // bytes of the whole text
  size_t written = 0;  // bytes in `out`
  bool fits = size > 0;
  for (size_t i = 0; i < text.size(); ++i) {
    uint32_t c = text[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < text.size() && text[i + 1] >= 0xDC00 && text[i + 1] < 0xE000) {
      c = 0x10000 + ((c - 0xD800) << 10) + (text[++i] - 0xDC00);
    } else if (c >= 0xD800 && c < 0xE000) {
      c = 0xFFFD;  // unpaired surrogate
    }
    char b[4];
    size_t len;
    if (c < 0x80) {
      b[0] = static_cast<char>(c), len = 1;
    } else if (c < 0x800) {
      b[0] = static_cast<char>(0xC0 | (c >> 6)), b[1] = static_cast<char>(0x80 | (c & 0x3F)), len = 2;
    } else if (c < 0x10000) {
      b[0] = static_cast<char>(0xE0 | (c >> 12)), b[1] = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      b[2] = static_cast<char>(0x80 | (c & 0x3F)), len = 3;
    } else {
      b[0] = static_cast<char>(0xF0 | (c >> 18)), b[1] = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      b[2] = static_cast<char>(0x80 | ((c >> 6) & 0x3F)), b[3] = static_cast<char>(0x80 | (c & 0x3F)), len = 4;
    }
    total += len;
    if (fits && written + len + 1 <= size) {  // never split a character
      std::memcpy(out + written, b, len);
      written += len;
    } else {
      fits = false;
    }
  }
  if (size) out[written] = '\0';
  return total;
}

}  // namespace pg
