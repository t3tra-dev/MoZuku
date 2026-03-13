#include "utf16.hpp"

#include "encoding_utils.hpp"

namespace {

size_t validatedSequenceLength(const std::string &text, size_t offset) {
  if (offset >= text.size()) {
    return 0;
  }

  size_t seqLen = MoZuku::encoding::utf8SequenceLength(
      static_cast<unsigned char>(text[offset]));
  if (seqLen == 0 || offset + seqLen > text.size()) {
    return 1;
  }

  for (size_t i = 1; i < seqLen; ++i) {
    unsigned char c = static_cast<unsigned char>(text[offset + i]);
    if ((c & 0xC0) != 0x80) {
      return 1;
    }
  }

  return seqLen;
}

unsigned int decodeCodePoint(const std::string &text, size_t offset,
                             size_t seqLen) {
  unsigned char c = static_cast<unsigned char>(text[offset]);
  if (seqLen == 1) {
    return c;
  }
  if (seqLen == 2) {
    return ((c & 0x1F) << 6) |
           (static_cast<unsigned char>(text[offset + 1]) & 0x3F);
  }
  if (seqLen == 3) {
    return ((c & 0x0F) << 12) |
           ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 6) |
           (static_cast<unsigned char>(text[offset + 2]) & 0x3F);
  }
  return ((c & 0x07) << 18) |
         ((static_cast<unsigned char>(text[offset + 1]) & 0x3F) << 12) |
         ((static_cast<unsigned char>(text[offset + 2]) & 0x3F) << 6) |
         (static_cast<unsigned char>(text[offset + 3]) & 0x3F);
}

int utf16UnitsAt(const std::string &text, size_t offset, size_t seqLen) {
  if (seqLen < 4) {
    return 1;
  }

  unsigned int cp = decodeCodePoint(text, offset, seqLen);
  return cp <= 0xFFFF ? 1 : 2;
}

} // namespace

TextOffsetMapper::TextOffsetMapper(const std::string &text)
    : text_(text), line_starts_(computeLineStarts(text)) {}

const std::vector<size_t> &TextOffsetMapper::lineStarts() const {
  return line_starts_;
}

Position TextOffsetMapper::byteOffsetToPosition(size_t offset) const {
  return ::byteOffsetToPosition(text_, line_starts_, offset);
}

size_t TextOffsetMapper::positionToByteOffset(int line, int character) const {
  return ::positionToByteOffset(text_, line_starts_, line, character);
}

size_t TextOffsetMapper::positionToByteOffset(const Position &position) const {
  return positionToByteOffset(position.line, position.character);
}

size_t TextOffsetMapper::tokenStartByteOffset(const TokenData &token) const {
  return positionToByteOffset(token.line, token.startChar);
}

std::vector<size_t> computeLineStarts(const std::string &text) {
  std::vector<size_t> lineStarts;
  lineStarts.reserve(64);
  lineStarts.push_back(0);
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] == '\n') {
      lineStarts.push_back(i + 1);
    }
  }
  return lineStarts;
}

Position byteOffsetToPosition(const std::string &text,
                              const std::vector<size_t> &lineStarts,
                              size_t offset) {
  if (offset > text.size()) {
    offset = text.size();
  }

  size_t lo = 0;
  size_t hi = lineStarts.size();
  while (lo + 1 < hi) {
    size_t mid = (lo + hi) / 2;
    if (lineStarts[mid] <= offset) {
      lo = mid;
    } else {
      hi = mid;
    }
  }

  size_t bytePos = lineStarts[lo];
  int utf16Pos = 0;

  while (bytePos < offset && bytePos < text.size() && text[bytePos] != '\n') {
    size_t seqLen = validatedSequenceLength(text, bytePos);
    utf16Pos += utf16UnitsAt(text, bytePos, seqLen);
    bytePos += seqLen;
  }

  return Position{static_cast<int>(lo), utf16Pos};
}

size_t positionToByteOffset(const std::string &text,
                            const std::vector<size_t> &lineStarts, int line,
                            int character) {
  if (line < 0 || lineStarts.empty()) {
    return 0;
  }
  if (line >= static_cast<int>(lineStarts.size())) {
    return text.size();
  }

  size_t bytePos = lineStarts[line];
  int utf16Pos = 0;

  while (bytePos < text.size() && utf16Pos < character &&
         text[bytePos] != '\n') {
    size_t seqLen = validatedSequenceLength(text, bytePos);
    utf16Pos += utf16UnitsAt(text, bytePos, seqLen);
    bytePos += seqLen;
  }

  return bytePos;
}

size_t positionToByteOffset(const std::string &text, int line, int character) {
  return positionToByteOffset(text, computeLineStarts(text), line, character);
}

size_t utf8ToUtf16Length(const std::string &utf8Str) {
  size_t offset = 0;
  size_t utf16Length = 0;

  while (offset < utf8Str.size()) {
    size_t seqLen = validatedSequenceLength(utf8Str, offset);
    utf16Length += utf16UnitsAt(utf8Str, offset, seqLen);
    offset += seqLen;
  }

  return utf16Length;
}
