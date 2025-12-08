#pragma once

#include "analyzer.hpp"
#include <string>
#include <vector>

namespace MoZuku {
namespace text {

class TextProcessor {
public:
  static std::string sanitizeUTF8(const std::string &input);

  static std::vector<SentenceBoundary>
  splitIntoSentences(const std::string &text);

  static bool isJapanesePunctuation(const std::string &text, size_t pos);

  static size_t skipWhitespace(const std::string &text, size_t pos);

private:
  static bool isValidUtf8Sequence(const std::string &input, size_t pos,
                                  size_t seqLen);
};

} // namespace text
} // namespace MoZuku
