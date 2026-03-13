#pragma once

#include "mozuku/core/types.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace MoZuku {
namespace text {

class TextProcessor {
public:
  static std::string sanitizeUTF8(const std::string &input);

  static std::vector<SentenceBoundary>
  splitIntoSentences(const std::string &text);

  static double calculateJapaneseRatio(const std::string &text);

  static bool isJapanesePunctuation(const std::string &text, size_t pos);

  static size_t skipWhitespace(const std::string &text, size_t pos);

private:
  static bool isValidUtf8Sequence(const std::string &input, size_t pos,
                                  size_t seqLen);
  static uint32_t decodeCodepoint(const std::string &text, size_t pos,
                                  size_t seqLen);
  static bool isWhitespaceCodepoint(uint32_t codepoint);
  static bool isJapaneseCodepoint(uint32_t codepoint);
};

} // namespace text
} // namespace MoZuku
