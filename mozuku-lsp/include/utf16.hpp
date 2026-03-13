#pragma once

#include "mozuku/core/types.hpp"

#include <string>
#include <vector>

class TextOffsetMapper {
public:
  explicit TextOffsetMapper(const std::string &text);

  const std::vector<size_t> &lineStarts() const;
  Position byteOffsetToPosition(size_t offset) const;
  size_t positionToByteOffset(int line, int character) const;
  size_t positionToByteOffset(const Position &position) const;
  size_t tokenStartByteOffset(const TokenData &token) const;

private:
  const std::string &text_;
  std::vector<size_t> line_starts_;
};

std::vector<size_t> computeLineStarts(const std::string &text);

Position byteOffsetToPosition(const std::string &text,
                              const std::vector<size_t> &lineStarts,
                              size_t offset);

size_t positionToByteOffset(const std::string &text,
                            const std::vector<size_t> &lineStarts, int line,
                            int character);

size_t positionToByteOffset(const std::string &text, int line, int character);

size_t utf8ToUtf16Length(const std::string &utf8Str);
